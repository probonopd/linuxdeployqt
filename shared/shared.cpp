/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd. and Simon Peter
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <iostream>
#include <QProcess>
#include <QDir>
#include <QRegExp>
#include <QSet>
#include <QStack>
#include <QDirIterator>
#include <QLibraryInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include "shared.h"


bool runStripEnabled = true;
bool bundleAllButCoreLibs = false;
bool alwaysOwerwriteEnabled = false;
QStringList librarySearchPath;
bool appstoreCompliant = false;
int logLevel = 1;
bool deployLibrary = false;

using std::cout;
using std::endl;

QMap<QString,QString> qtToBeBundledInfo;

bool operator==(const LibraryInfo &a, const LibraryInfo &b)
{
    return ((a.libraryPath == b.libraryPath) && (a.binaryPath == b.binaryPath));
}

QDebug operator<<(QDebug debug, const LibraryInfo &info)
{
    debug << "Library name" << info.libraryName << "\n";
    debug << "Library directory" << info.libraryDirectory << "\n";
    debug << "Library path" << info.libraryPath << "\n";
    debug << "Binary directory" << info.binaryDirectory << "\n";
    debug << "Binary name" << info.binaryName << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Version" << info.version << "\n";
    debug << "Install name" << info.installName << "\n";
    debug << "Deployed install name" << info.deployedInstallName << "\n";
    debug << "Source file Path" << info.sourceFilePath << "\n";
    debug << "Library Destination Directory (relative to bundle)" << info.libraryDestinationDirectory << "\n";
    debug << "Binary Destination Directory (relative to bundle)" << info.binaryDestinationDirectory << "\n";

    return debug;
}

const QString bundleLibraryDirectory = "lib"; // the same directory as the main executable; could define a relative subdirectory here

inline QDebug operator<<(QDebug debug, const AppDirInfo &info)
{
    debug << "Application bundle path" << info.path << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Additional libraries" << info.libraryPaths << "\n";
    return debug;
}

// Determine whether the given 'ldd' output contains a Linux VDSO
// shared object. The name of the VDSO object differs depending
// on architecture. See "vDSO names" in the notes section of vdso(7)
// for more information.
static bool lddOutputContainsLinuxVDSO(const QString &lddOutput) {
	// aarch64, arm, mips, x86_64, x86/x32
	if (lddOutput.contains(QStringLiteral("linux-vdso.so.1"))) {
		return true;
	// ppc32, s390
	} else if (lddOutput.contains(QStringLiteral("linux-vdso32.so.1"))) {
		return true;
	// ppc64, s390x
	} else if (lddOutput.contains(QStringLiteral("linux-vdso64.so.1"))) {
		return true;
	// ia64, sh, i386
	} else if (lddOutput.contains(QStringLiteral("linux-gate.so.1"))) {
		return true;
	}
	return false;
}

bool copyFilePrintStatus(const QString &from, const QString &to)
{
    if (QFile(to).exists()) {
        if (alwaysOwerwriteEnabled) {
            QFile(to).remove();
        } else {
            LogNormal() << "File exists, skip copy:" << to;
            return false;
        }
    }

    if (QFile::copy(from, to)) {
        QFile dest(to);
        dest.setPermissions(dest.permissions() | QFile::WriteOwner | QFile::WriteUser);
        LogNormal() << " copied:" << from;
        LogNormal() << " to" << to;

        // The source file might not have write permissions set. Set the
        // write permission on the target file to make sure we can use
        // install_name_tool on it later.
        QFile toFile(to);
        if (toFile.permissions() & QFile::WriteOwner)
            return true;

        if (!toFile.setPermissions(toFile.permissions() | QFile::WriteOwner)) {
            LogError() << "Failed to set u+w permissions on target file: " << to;
            return false;
        }

        return true;
    } else {
        LogError() << "file copy failed from" << from;
        LogError() << " to" << to;
        return false;
    }
}

LddInfo findDependencyInfo(const QString &binaryPath)
{
    LddInfo info;
    info.binaryPath = binaryPath;

    LogDebug() << "Using ldd:";
    LogDebug() << " inspecting" << binaryPath;
    QProcess ldd;
    ldd.start("ldd", QStringList() << binaryPath);
    ldd.waitForFinished();

    if (ldd.exitStatus() != QProcess::NormalExit || ldd.exitCode() != 0) {
        LogError() << "findDependencyInfo:" << ldd.readAllStandardError();
        return info;
    }

    static const QRegularExpression regexp(QStringLiteral("^.+ => (.+) \\("));

    QString output = ldd.readAllStandardOutput();
    QStringList outputLines = output.split("\n", QString::SkipEmptyParts);
    if (outputLines.size() < 2) {
        if ((output.contains("statically linked") == false)){
            LogError() << "Could not parse ldd output under 2 lines:" << output;
        }
        return info;
    }

    foreach (QString outputLine, outputLines) {
        LogDebug() << "ldd outputLine:" << outputLine;
        if (outputLine.contains("not found")){
            LogError() << "ldd outputLine:" << outputLine;
        }
    }

    if ((binaryPath.contains(".so.") || binaryPath.endsWith(".so")) && (!lddOutputContainsLinuxVDSO(output))) {
        const QRegularExpressionMatch match = regexp.match(outputLines.first());
        if (match.hasMatch())  {
            info.installName = match.captured(1);
        } else {
            LogError() << "Could not parse ldd output line:" << outputLines.first();
        }
        outputLines.removeFirst();
    }

    foreach (const QString &outputLine, outputLines) {
        const QRegularExpressionMatch match = regexp.match(outputLine);
        if (match.hasMatch()) {
            DylibInfo dylib;
            dylib.binaryPath = match.captured(1).trimmed();
            LogDebug() << " dylib.binaryPath" << dylib.binaryPath;
            /*
            dylib.compatibilityVersion = 0;
            dylib.currentVersion = 0;
            */
            info.dependencies << dylib;
        }
    }

    return info;
}

int containsHowOften(QStringList haystack, QString needle) {
    int result = haystack.filter(needle).length();
    return result;
}

LibraryInfo parseLddLibraryLine(const QString &line, const QString &appDirPath, const QSet<QString> &rpaths)
{
    LibraryInfo info;
    QString trimmed = line.trimmed();

    LogDebug() << "parsing" << trimmed;

    if (trimmed.isEmpty())
        return info;


    if(bundleAllButCoreLibs) {
        /*
        Bundle every lib including the low-level ones except those that are explicitly blacklisted.
        This is more suitable for bundling in a way that is portable between different distributions and target systems.
        Along the way, this also takes care of non-Qt libraries.

        The excludelist can be updated by running
        #/bin/bash
        blacklisted=$(wget https://raw.githubusercontent.com/probonopd/AppImages/master/excludelist -O - | sort | uniq | grep -v "^#.*" | grep "[^-\s]")
        for item in $blacklisted; do
          echo -ne '"'$item'" << '
        done
        */

        QStringList excludelist;
        excludelist << "libasound.so.2" << "libcom_err.so.2" << "libcrypt.so.1" << "libc.so.6" << "libdl.so.2" << "libdrm.so.2" << "libexpat.so.1" << "libfontconfig.so.1" << "libgcc_s.so.1" << "libgdk_pixbuf-2.0.so.0" << "libgdk-x11-2.0.so.0" << "libgio-2.0.so.0" << "libglib-2.0.so.0" << "libGL.so.1" << "libgobject-2.0.so.0" << "libgpg-error.so.0" << "libgssapi_krb5.so.2" << "libgtk-x11-2.0.so.0" << "libhcrypto.so.4" << "libhx509.so.5" << "libICE.so.6" << "libidn.so.11" << "libk5crypto.so.3" << "libkeyutils.so.1" << "libkrb5.so.26" << "libkrb5.so.3" << "libkrb5support.so.0" << "libm.so.6" << "libnss3.so" << "libp11-kit.so.0" << "libpcre.so.3" << "libpthread.so.0" << "libresolv.so.2" << "libroken.so.18" << "librt.so.1" << "libselinux.so.1" << "libSM.so.6" << "libstdc++.so.6" << "libusb-1.0.so.0" << "libuuid.so.1" << "libwind.so.0" << "libX11.so.6" << "libxcb.so.1" << "libz.so.1";
        LogDebug() << "excludelist:" << excludelist;
        if (! trimmed.contains("libicu")) {
            if (containsHowOften(excludelist, QFileInfo(trimmed).completeBaseName())) {
                LogDebug() << "Skipping blacklisted" << trimmed;
                return info;
            }
        }
    } else {
        /*
        Don't deploy low-level libraries in /usr or /lib because these tend to break if moved to a system with a different glibc.
        TODO: Could make bundling these low-level libraries optional but then the bundles might need to
        use something like patchelf --set-interpreter or http://bitwagon.com/rtldi/rtldi.html
        With the Qt provided by qt.io the libicu libraries come bundled, but that is not the case with e.g.,
        Qt from ppas. Hence we make sure libicu is always bundled since it cannot be assumed to be on target sytems
        */
        // Manual make of Qt deploys it to /usr/local/Qt-x.x.x so we cannot remove this path just like that, so let's allow known libs of Qt.
        if (!trimmed.contains("libicu") && !trimmed.contains("lib/libQt") && !trimmed.contains("lib/libqgsttools")) {
            if ((trimmed.startsWith("/usr") or (trimmed.startsWith("/lib")))) {
                return info;
            }
        }
    }

    enum State {QtPath, LibraryName, Version, End};
    State state = QtPath;
    int part = 0;
    QString name;
    QString qtPath;
    QString suffix = "";

    // Split the line into [Qt-path]/lib/qt[Module].library/Versions/[Version]/
    QStringList parts = trimmed.split("/");
    while (part < parts.count()) {
        const QString currentPart = parts.at(part).simplified() ;
        ++part;
        if (currentPart == "")
            continue;

        if (state == QtPath) {
            // Check for library name part
            if (part < parts.count() && parts.at(part).contains(".so")) {
                info.libraryDirectory += "/" + (qtPath + currentPart + "/").simplified();
                LogDebug() << "info.libraryDirectory:" << info.libraryDirectory;
                state = LibraryName;
                continue;
            } else if (trimmed.startsWith("/") == false) {      // If the line does not contain a full path, the app is using a binary Qt package.
                QStringList partsCopy = parts;
                partsCopy.removeLast();
                foreach (QString path, librarySearchPath) {
                    if (!path.endsWith("/"))
                        path += '/';
                    QString nameInPath = path + parts.join("/");
                    if (QFile::exists(nameInPath)) {
                        info.libraryDirectory = path + partsCopy.join("/");
                        break;
                    }
                }
                if (info.libraryDirectory.isEmpty())
                    info.libraryDirectory = "/usr/lib/" + partsCopy.join("/");
                if (!info.libraryDirectory.endsWith("/"))
                    info.libraryDirectory += "/";
                state = LibraryName;
                --part;
                continue;
            }
            qtPath += (currentPart + "/");

        } if (state == LibraryName) {
            name = currentPart;
            info.isDylib = true;
            info.libraryName = name;
            info.binaryName = name.left(name.indexOf('.')) + suffix + name.mid(name.indexOf('.'));
            info.deployedInstallName = "$ORIGIN"; // + info.binaryName;
            info.libraryPath = info.libraryDirectory + info.binaryName;
            info.sourceFilePath = info.libraryPath;
            info.libraryDestinationDirectory = bundleLibraryDirectory + "/";
            info.binaryDestinationDirectory = info.libraryDestinationDirectory;
            info.binaryDirectory = info.libraryDirectory;
            info.binaryPath = info.libraryPath;
            state = End;
            ++part;
            continue;
        } else if (state == End) {
            break;
        }
    }

    if (!info.sourceFilePath.isEmpty() && QFile::exists(info.sourceFilePath)) {
        info.installName = findDependencyInfo(info.sourceFilePath).installName;
        if (info.installName.startsWith("@rpath/"))
            info.deployedInstallName = info.installName;
    }

    return info;
}

QString findAppBinary(const QString &appDirPath)
{
    QString binaryPath;

    // FIXME: Do without the need for an AppRun symlink
    // by passing appBinaryPath from main.cpp here
    binaryPath = appDirPath + "/" + "AppRun";

    if (QFile::exists(binaryPath))
        return binaryPath;

    LogError() << "Could not find bundle binary for" << appDirPath << "at" << binaryPath;
    exit(1);
}

QStringList findAppLibraries(const QString &appDirPath)
{
    QStringList result;
    // .so
    QDirIterator iter(appDirPath, QStringList() << QString::fromLatin1("*.so"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        result << iter.fileInfo().filePath();
    }
    // .so.*, FIXME: Is the above really needed or is it covered by the below too?
    QDirIterator iter2(appDirPath, QStringList() << QString::fromLatin1("*.so*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter2.hasNext()) {
        iter2.next();
        result << iter2.fileInfo().filePath();
    }
    return result;
}

QList<LibraryInfo> getQtLibraries(const QList<DylibInfo> &dependencies, const QString &appDirPath, const QSet<QString> &rpaths)
{
    QList<LibraryInfo> libraries;
    foreach (const DylibInfo &dylibInfo, dependencies) {
        LibraryInfo info = parseLddLibraryLine(dylibInfo.binaryPath, appDirPath, rpaths);
        if (info.libraryName.isEmpty() == false) {
            LogDebug() << "Adding library:";
            LogDebug() << info;
            libraries.append(info);
        }
    }
    return libraries;
}

// TODO: Switch the following to using patchelf
QSet<QString> getBinaryRPaths(const QString &path, bool resolve = true, QString executablePath = QString())
{
    QSet<QString> rpaths;

    QProcess objdump;
    objdump.start("objdump", QStringList() << "-x" << path);

    if (!objdump.waitForStarted()) {
        if(objdump.errorString().contains("execvp: No such file or directory")){
            LogError() << "Could not start objdump.";
            LogError() << "Make sure it is installed on your $PATH.";
        } else {
            LogError() << "Could not start objdump. Process error is" << objdump.errorString();
        }
        exit(1);
    }

    objdump.waitForFinished();

    if (objdump.exitCode() != 0) {
        LogError() << "getBinaryRPaths:" << objdump.readAllStandardError();
    }

    if (resolve && executablePath.isEmpty()) {
      executablePath = path;
    }

    QString output = objdump.readAllStandardOutput();
    QStringList outputLines = output.split("\n");
    QStringListIterator i(outputLines);

    while (i.hasNext()) {
        if (i.next().contains("RUNPATH") && i.hasNext()) {
            i.previous();
            const QString &rpathCmd = i.next();
            int pathStart = rpathCmd.indexOf("RUNPATH");
            if (pathStart >= 0) {
                QString rpath = rpathCmd.mid(pathStart+8).trimmed();
                LogDebug() << "rpath:" << rpath;
                rpaths << rpath;
            }
        }
    }

    return rpaths;
}

QList<LibraryInfo> getQtLibraries(const QString &path, const QString &appDirPath, const QSet<QString> &rpaths)
{
    const LddInfo info = findDependencyInfo(path);
    return getQtLibraries(info.dependencies, appDirPath, rpaths + getBinaryRPaths(path));
}

QList<LibraryInfo> getQtLibrariesForPaths(const QStringList &paths, const QString &appDirPath, const QSet<QString> &rpaths)
{
    QList<LibraryInfo> result;
    QSet<QString> existing;

    foreach (const QString &path, paths) {
        foreach (const LibraryInfo &info, getQtLibraries(path, appDirPath, rpaths)) {
            if (!existing.contains(info.libraryPath)) { // avoid duplicates
                existing.insert(info.libraryPath);
                result << info;
            }
        }
    }
    return result;
}

QStringList getBinaryDependencies(const QString executablePath,
                                  const QString &path,
                                  const QList<QString> &additionalBinariesContainingRpaths)
{
    QStringList binaries;

    const QList<DylibInfo> dependencies = findDependencyInfo(path).dependencies;

    bool rpathsLoaded = false;
    QSet<QString> rpaths;

    // return bundle-local dependencies. (those starting with @executable_path)
    foreach (const DylibInfo &info, dependencies) {
        QString trimmedLine = info.binaryPath;
        if (trimmedLine.startsWith("@executable_path/")) {
            QString binary = QDir::cleanPath(executablePath + trimmedLine.mid(QStringLiteral("@executable_path/").length()));
            if (binary != path)
                binaries.append(binary);
        } else if (trimmedLine.startsWith("@rpath/")) {
            if (!rpathsLoaded) {
                rpaths = getBinaryRPaths(path, true, executablePath);
                foreach (const QString &binaryPath, additionalBinariesContainingRpaths) {
                    QSet<QString> binaryRpaths = getBinaryRPaths(binaryPath, true);
                    rpaths += binaryRpaths;
                }
                rpathsLoaded = true;
            }
            bool resolved = false;
            foreach (const QString &rpath, rpaths) {
                QString binary = QDir::cleanPath(rpath + "/" + trimmedLine.mid(QStringLiteral("@rpath/").length()));
                LogDebug() << "Checking for" << binary;
                if (QFile::exists(binary)) {
                    binaries.append(binary);
                    resolved = true;
                    break;
                }
            }
            if (!resolved && !rpaths.isEmpty()) {
                LogError() << "Cannot resolve rpath" << trimmedLine;
                LogError() << " using" << rpaths;
            }
        }
    }

    return binaries;
}

// copies everything _inside_ sourcePath to destinationPath
bool recursiveCopy(const QString &sourcePath, const QString &destinationPath)
{
    if (!QDir(sourcePath).exists())
        return false;
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;

    QStringList files = QDir(sourcePath).entryList(QStringList() << "*", QDir::Files | QDir::NoDotAndDotDot);
    foreach (QString file, files) {
        const QString fileSourcePath = sourcePath + "/" + file;
        const QString fileDestinationPath = destinationPath + "/" + file;
        copyFilePrintStatus(fileSourcePath, fileDestinationPath);
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << "*", QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QString dir, subdirs) {
        recursiveCopy(sourcePath + "/" + dir, destinationPath + "/" + dir);
    }
    return true;
}

void recursiveCopyAndDeploy(const QString &appDirPath, const QSet<QString> &rpaths, const QString &sourcePath, const QString &destinationPath)
{
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;

    QStringList files = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Files | QDir::NoDotAndDotDot);
    foreach (QString file, files) {
        const QString fileSourcePath = sourcePath + QLatin1Char('/') + file;

        if (file.endsWith("_debug.dylib")) {
            continue; // Skip debug versions
        } else {
            QString fileDestinationPath = destinationPath + QLatin1Char('/') + file;
            copyFilePrintStatus(fileSourcePath, fileDestinationPath);
        }
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QString dir, subdirs) {
        recursiveCopyAndDeploy(appDirPath, rpaths, sourcePath + QLatin1Char('/') + dir, destinationPath + QLatin1Char('/') + dir);
    }
}

QString copyDylib(const LibraryInfo &library, const QString path)
{
    if (!QFile::exists(library.sourceFilePath)) {
        LogError() << "no file at" << library.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Libraries/libfoo.dylib
    QString dylibDestinationDirectory = path + QLatin1Char('/') + library.libraryDestinationDirectory;
    QString dylibDestinationBinaryPath = dylibDestinationDirectory + QLatin1Char('/') + library.binaryName;

    // Create destination directory
    if (!QDir().mkpath(dylibDestinationDirectory)) {
        LogError() << "could not create destination directory" << dylibDestinationDirectory;
        return QString();
    }

    // Retrun if the dylib has aleardy been deployed
    if (QFileInfo(dylibDestinationBinaryPath).exists() && !alwaysOwerwriteEnabled)
        return dylibDestinationBinaryPath;

    // Copy dylib binary
    copyFilePrintStatus(library.sourceFilePath, dylibDestinationBinaryPath);
    return dylibDestinationBinaryPath;
}

void runPatchelf(QStringList options)
{
    QProcess patchelftool;
    patchelftool.start("patchelf", options);
    if (!patchelftool.waitForStarted()) {
        if(patchelftool.errorString().contains("execvp: No such file or directory")){
            LogError() << "Could not start patchelf.";
            LogError() << "Make sure it is installed on your $PATH, e.g., in /usr/local/bin.";
            LogError() << "You can get it from https://nixos.org/patchelf.html.";
        } else {
            LogError() << "Could not start patchelftool. Process error is" << patchelftool.errorString();
        }
        exit(1);
    }
    patchelftool.waitForFinished();
    if (patchelftool.exitCode() != 0) {
        LogError() << "runPatchelf:" << patchelftool.readAllStandardError();
        LogError() << "runPatchelf:" << patchelftool.readAllStandardOutput();
        exit(1);
    }
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogDebug() << "Using patchelf:";
    LogDebug() << " change rpath in" << binaryPath;
    LogDebug() << " to" << id;
    runPatchelf(QStringList() << "--set-rpath" << id << binaryPath);
}

void runStrip(const QString &binaryPath)
{
    if (runStripEnabled == false)
        return;

    // Since we might have a symlink, we need to find its target first
    QString resolvedPath = QFileInfo(binaryPath).canonicalFilePath();

    LogDebug() << "Determining whether to run strip:";
    LogDebug() << " checking whether" << resolvedPath << "has an rpath set";
    LogDebug() << "patchelf" << "--print-rpath" << resolvedPath;
    QProcess patchelfread;
    patchelfread.start("patchelf", QStringList() << "--print-rpath" << resolvedPath);
    if (!patchelfread.waitForStarted()) {
        if(patchelfread.errorString().contains("execvp: No such file or directory")){
            LogError() << "Could not start patchelf.";
            LogError() << "Make sure it is installed on your $PATH.";
        } else {
            LogError() << "Could not start patchelf. Process error is" << patchelfread.errorString();
        }
        exit(1);
    }
    patchelfread.waitForFinished();

    if (patchelfread.exitCode() != 0){
        LogError() << "Error reading rpath with patchelf" << QFileInfo(resolvedPath).completeBaseName() << ":" << patchelfread.readAllStandardError();
        LogError() << "Error reading rpath with patchelf" << QFileInfo(resolvedPath).completeBaseName() << ":" << patchelfread.readAllStandardOutput();
        exit(1);
    }

    QString rpath = patchelfread.readAllStandardOutput();

    if (rpath.startsWith("$")){
        LogDebug() << "Already contains rpath starting with $, hence not stripping";
        LogDebug() << patchelfread.readAllStandardOutput();
        return;
    }

    LogDebug() << "Using strip:";
    LogDebug() << " stripping" << resolvedPath;
    QProcess strip;
    strip.start("strip", QStringList() << "-x" << resolvedPath);
    if (!strip.waitForStarted()) {
        if(strip.errorString().contains("execvp: No such file or directory")){
            LogError() << "Could not start strip.";
            LogError() << "Make sure it is installed on your $PATH.";
        } else {
            LogError() << "Could not start strip. Process error is" << strip.errorString();
        }
        exit(1);
    }
    strip.waitForFinished();

    if (strip.exitCode() == 0)
        return;

    if (strip.readAllStandardError().contains("Not enough room for program headers")) {
        LogNormal() << QFileInfo(resolvedPath).completeBaseName() << "already stripped.";
    } else {
        LogError() << "Error stripping" << QFileInfo(resolvedPath).completeBaseName() << ":" << strip.readAllStandardError();
        LogError() << "Error stripping" << QFileInfo(resolvedPath).completeBaseName() << ":" << strip.readAllStandardOutput();
        exit(1);
    }

}

void stripAppBinary(const QString &bundlePath)
{
    runStrip(findAppBinary(bundlePath));
}

/*
    Deploys the the libraries listed into an app bundle.
    The libraries are searched for dependencies, which are also deployed.
    (deploying Qt3Support will also deploy QtNetwork and QtSql for example.)
    Returns a DeploymentInfo structure containing the Qt path used and a
    a list of actually deployed libraries.
*/
DeploymentInfo deployQtLibraries(QList<LibraryInfo> libraries,
        const QString &bundlePath, const QStringList &binaryPaths,
                                  bool useLoaderPath)
{
    LogNormal() << "Deploying libraries found inside:" << binaryPaths;
    QStringList copiedLibraries;
    DeploymentInfo deploymentInfo;
    deploymentInfo.useLoaderPath = useLoaderPath;
    deploymentInfo.pluginPath = qtToBeBundledInfo.value("QT_INSTALL_PLUGINS");
    QSet<QString> rpathsUsed;

    while (libraries.isEmpty() == false) {
        const LibraryInfo library = libraries.takeFirst();
        copiedLibraries.append(library.libraryName);

        if(library.libraryName.contains("libQt") and library.libraryName.contains("Core.so")) {
            LogNormal() << "Setting deploymentInfo.qtPath to:" << library.libraryDirectory;
            deploymentInfo.qtPath = library.libraryDirectory;
        }


        if (library.libraryDirectory.startsWith(bundlePath)) {
            LogNormal()  << library.libraryName << "already deployed, skipping.";
            continue;
        }

        if (library.rpathUsed.isEmpty() != true) {
            rpathsUsed << library.rpathUsed;
        }

        // Copy the library to the app bundle.
        const QString deployedBinaryPath = copyDylib(library, bundlePath);
        // Skip the rest if already was deployed.
        if (deployedBinaryPath.isNull())
            continue;

        runStrip(deployedBinaryPath);

        if (!library.rpathUsed.length()) {
            changeIdentification(library.deployedInstallName, deployedBinaryPath);
        }

        // Check for library dependencies
        QList<LibraryInfo> dependencies = getQtLibraries(deployedBinaryPath, bundlePath, rpathsUsed);

        foreach (LibraryInfo dependency, dependencies) {
            if (dependency.rpathUsed.isEmpty() != true) {
                rpathsUsed << dependency.rpathUsed;
            }

            // Deploy library if necessary.
            if (copiedLibraries.contains(dependency.libraryName) == false && libraries.contains(dependency) == false) {
                libraries.append(dependency);
            }
        }
    }
    deploymentInfo.deployedLibraries = copiedLibraries;

    deploymentInfo.rpathsUsed += rpathsUsed;

    return deploymentInfo;
}

static QString captureOutput(const QString &command)
{
    QProcess process;
    process.start(command, QIODevice::ReadOnly);
    process.waitForFinished();

    if (process.exitStatus() != QProcess::NormalExit) {
        LogError() << command << "crashed:" << process.readAllStandardError();
    } else if (process.exitCode() != 0) {
        LogError() << command << "exited with" << process.exitCode() << ":" << process.readAllStandardError();
    }

    return process.readAllStandardOutput();
}

DeploymentInfo deployQtLibraries(const QString &appDirPath, const QStringList &additionalExecutables)
{
   AppDirInfo applicationBundle;

   applicationBundle.path = appDirPath;
   LogDebug() << "applicationBundle.path:" << applicationBundle.path;
   applicationBundle.binaryPath = findAppBinary(appDirPath);
   LogDebug() << "applicationBundle.binaryPath:" << applicationBundle.binaryPath;

   // Determine the location of the Qt to be bundled
   LogDebug() << "Using qmake to determine the location of the Qt to be bundled";
   QString output = captureOutput("qmake -query");
   QStringList outputLines = output.split("\n", QString::SkipEmptyParts);
   foreach (const QString &outputLine, outputLines) {
       int colonIndex = outputLine.indexOf(QLatin1Char(':'));
       if (colonIndex != -1) {
           QString name = outputLine.left(colonIndex);
           QString value = outputLine.mid(colonIndex + 1);
           qtToBeBundledInfo.insert(name, value);
       }
   }

   QString qtLibsPath = qtToBeBundledInfo.value("QT_INSTALL_LIBS");

   if (qtLibsPath.isEmpty() || !QFile::exists(qtLibsPath)) {
       LogError() << "Qt path could not be determined from qmake on the $PATH";
       LogError() << "Make sure you have the correct Qt on your $PATH";
       LogError() << "You can check this with qmake -v";
   } else {
       LogDebug() << "Qt libs path determined from qmake:" << qtLibsPath;
       QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
       QString oldPath = env.value("LD_LIBRARY_PATH");
       QString newPath = qtLibsPath + ":" + oldPath; // FIXME: If we use a ldd replacement, we still need to observe this path
       // FIXME: Directory layout might be different for system Qt; cannot assume lib/ to always be inside the Qt directory
       LogDebug() << "Changed LD_LIBRARY_PATH:" << newPath;
       setenv("LD_LIBRARY_PATH",newPath.toUtf8().constData(),1);
   }

   changeIdentification("$ORIGIN/" + bundleLibraryDirectory, applicationBundle.binaryPath);
   applicationBundle.libraryPaths = findAppLibraries(appDirPath);
   LogDebug() << "applicationBundle.libraryPaths:" << applicationBundle.libraryPaths;

   LogDebug() << "additionalExecutables:" << additionalExecutables;

   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   LogDebug() << "allBinaryPaths:" << allBinaryPaths;

   QSet<QString> allRPaths = getBinaryRPaths(applicationBundle.binaryPath, true);
   allRPaths.insert(QLibraryInfo::location(QLibraryInfo::LibrariesPath));
   LogDebug() << "allRPaths:" << allRPaths;

   QList<LibraryInfo> libraries = getQtLibrariesForPaths(allBinaryPaths, appDirPath, allRPaths);
   if (libraries.isEmpty() && !alwaysOwerwriteEnabled) {
        LogWarning() << "Could not find any external Qt libraries to deploy in" << appDirPath;
        LogWarning() << "Perhaps linuxdeployqt was already used on" << appDirPath << "?";
        LogWarning() << "If so, you will need to rebuild" << appDirPath << "before trying again.";
        LogWarning() << "Or ldd does not find the external Qt libraries but sees the system ones.";
        LogWarning() << "If so, you will need to set LD_LIBRARY_PATH to the directory containing the external Qt libraries before trying again.";
        LogWarning() << "FIXME: https://github.com/probonopd/linuxdeployqt/issues/2";
        return DeploymentInfo();
   } else {
       return deployQtLibraries(libraries, applicationBundle.path, allBinaryPaths, !additionalExecutables.isEmpty());
   }
}

void deployPlugins(const AppDirInfo &appDirInfo, const QString &pluginSourcePath,
        const QString pluginDestinationPath, DeploymentInfo deploymentInfo)
{
    LogNormal() << "Deploying plugins from" << pluginSourcePath;

    if (!pluginSourcePath.contains(deploymentInfo.pluginPath))
        return;

    // Plugin white list:
    QStringList pluginList;

    LogDebug() << "deploymentInfo.deployedLibraries before attempting to bundle required plugins:" << deploymentInfo.deployedLibraries;

    // Platform plugin:
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Gui")) {
        LogDebug() << "libQt5Gui detected";
        pluginList.append("platforms/libqxcb.so");
        // All image formats (svg if QtSvg library is used)
        QStringList imagePlugins = QDir(pluginSourcePath +  QStringLiteral("/imageformats")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, imagePlugins) {
            if (plugin.contains(QStringLiteral("qsvg"))) {
                if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Svg")) {
                    pluginList.append(QStringLiteral("imageformats/") + plugin);
                }
            } else {
                pluginList.append(QStringLiteral("imageformats/") + plugin);
	    }
        }
    }

    // Platform OpenGL context
    if ((containsHowOften(deploymentInfo.deployedLibraries, "libQt5OpenGL")) or (containsHowOften(deploymentInfo.deployedLibraries, "libQt5XcbQpa"))) {
        QStringList xcbglintegrationPlugins = QDir(pluginSourcePath +  QStringLiteral("/xcbglintegrations")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, xcbglintegrationPlugins) {
            pluginList.append(QStringLiteral("xcbglintegrations/") + plugin);
        }
    }    
    
    // Also deploy plugins/iconengines/libqsvgicon.so whenever libQt5Svg.so.* is about to be deployed,
    // https://github.com/probonopd/linuxdeployqt/issues/36
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Svg")) {
        pluginList.append(QStringLiteral("iconengines/libqsvgicon.so"));
    }

    // CUPS print support
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5PrintSupport")) {
        pluginList.append("printsupport/libcupsprintersupport.so");
    }

    // Network
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Network")) {
        QStringList bearerPlugins = QDir(pluginSourcePath +  QStringLiteral("/bearer")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, bearerPlugins) {
            pluginList.append(QStringLiteral("bearer/") + plugin);
        }
    }

    // Sql plugins if QtSql library is in use
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Sql")) {
        QStringList sqlPlugins = QDir(pluginSourcePath +  QStringLiteral("/sqldrivers")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, sqlPlugins) {
            pluginList.append(QStringLiteral("sqldrivers/") + plugin);
        }
    }

    // multimedia plugins if QtMultimedia library is in use
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Multimedia")) {
        QStringList plugins = QDir(pluginSourcePath + QStringLiteral("/mediaservice")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, plugins) {
            pluginList.append(QStringLiteral("mediaservice/") + plugin);
        }
        plugins = QDir(pluginSourcePath + QStringLiteral("/audio")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, plugins) {
            pluginList.append(QStringLiteral("audio/") + plugin);
        }
    }

    QString sourcePath;
    QString destinationPath;
    
    // Qt WebEngine if libQt5WebEngineCore is in use
    // https://doc-snapshots.qt.io/qt5-5.7/qtwebengine-deploying.html
    // TODO: Rather than hardcode the source paths, somehow get them dynamically
    // from the Qt instance that is to be bundled (pull requests welcome!)
    // especially since stuff that is supposed to come from resources actually
    // seems to come in libexec in the upstream Qt binary distribution
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5WebEngineCore")) {
        // Find directories with needed files:
        QString qtLibexecPath = qtToBeBundledInfo.value("QT_INSTALL_LIBEXECS");
        QString qtDataPath = qtToBeBundledInfo.value("QT_INSTALL_DATA");
        QString qtTranslationsPath = qtToBeBundledInfo.value("QT_INSTALL_TRANSLATIONS");
        // create destination directories:
        QString dstLibexec = appDirInfo.path + "/libexec";
        QString dstResources = appDirInfo.path + "/resources";
        QString dstTranslations = appDirInfo.path + "/translations";
        QDir().mkpath(dstLibexec);
        QDir().mkpath(dstResources);
        QDir().mkpath(dstTranslations);
        // WebEngine executable:
        sourcePath = QDir::cleanPath(qtLibexecPath + "/QtWebEngineProcess");
        destinationPath = QDir::cleanPath(dstLibexec + "/QtWebEngineProcess");
        copyFilePrintStatus(sourcePath, destinationPath);
        // put qt.conf file next to browser process so it can also make use of our local Qt resources
        createQtConfForQtWebEngineProcess(dstLibexec);
        // Resources:
        sourcePath = QDir::cleanPath(qtDataPath + "/resources/qtwebengine_resources.pak");
        destinationPath = QDir::cleanPath(dstResources + "/qtwebengine_resources.pak");
        copyFilePrintStatus(sourcePath, destinationPath);
        sourcePath = QDir::cleanPath(qtDataPath + "/resources/qtwebengine_devtools_resources.pak");
        destinationPath = QDir::cleanPath(dstResources + "/qtwebengine_devtools_resources.pak");
        copyFilePrintStatus(sourcePath, destinationPath);
        sourcePath = QDir::cleanPath(qtDataPath + "/resources/qtwebengine_resources_100p.pak");
        destinationPath = QDir::cleanPath(dstResources + "/qtwebengine_resources_100p.pak");
        copyFilePrintStatus(sourcePath, destinationPath);
        sourcePath = QDir::cleanPath(qtDataPath + "/resources/qtwebengine_resources_200p.pak");
        destinationPath = QDir::cleanPath(dstResources + "/qtwebengine_resources_200p.pak");
        copyFilePrintStatus(sourcePath, destinationPath);
        sourcePath = QDir::cleanPath(qtDataPath + "/resources/icudtl.dat");
        destinationPath = QDir::cleanPath(dstResources + "/icudtl.dat");
        copyFilePrintStatus(sourcePath, destinationPath);
        // Translations:
        sourcePath = QDir::cleanPath(qtTranslationsPath + "/qtwebengine_locales");
        destinationPath = QDir::cleanPath(dstTranslations + "/qtwebengine_locales");
        recursiveCopy(sourcePath, destinationPath);
    }
    
    LogDebug() << "pluginList after having detected hopefully all required plugins:" << pluginList;
    foreach (const QString &plugin, pluginList) {
        sourcePath = pluginSourcePath + "/" + plugin;
        destinationPath = pluginDestinationPath + "/" + plugin;
        QDir dir;
        dir.mkpath(QFileInfo(destinationPath).path());
        deploymentInfo.deployedLibraries += findAppLibraries(destinationPath);
        deploymentInfo.deployedLibraries = deploymentInfo.deployedLibraries.toSet().toList();

        if (copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);
            QList<LibraryInfo> libraries = getQtLibraries(sourcePath, appDirInfo.path, deploymentInfo.rpathsUsed);
            deployQtLibraries(libraries, appDirInfo.path, QStringList() << destinationPath, deploymentInfo.useLoaderPath);
        }
    }
}

void createQtConf(const QString &appDirPath)
{
    // Set Plugins and imports paths. These are relative to App.app/Contents.
    QByteArray contents = "# Generated by linuxdeployqt\n"
                          "# https://github.com/probonopd/linuxdeployqt/\n"
                          "[Paths]\n"
                          "Plugins = plugins\n"
                          "Imports = qml\n"
                          "Qml2Imports = qml\n";

    QString filePath = appDirPath + "/"; // Is picked up when placed next to the main executable
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {

        LogWarning() << fileName << "already exists, will not overwrite.";
        LogWarning() << "To make sure the plugins are loaded from the correct location,";
        LogWarning() << "please make sure qt.conf contains the following lines:";
        LogWarning() << "[Paths]";
        LogWarning() << "  Plugins = plugins";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file:" << fileName;
        LogNormal() << "This file sets the plugin search path to" << appDirPath + "/plugins";
    }
}

void createQtConfForQtWebEngineProcess(const QString &appDirPath)
{
    QByteArray contents = "# Generated by linuxdeployqt\n"
                          "# https://github.com/probonopd/linuxdeployqt/\n"
                          "[Paths]\n"
                          "Prefix = ../\n";
    QString filePath = appDirPath + "/";
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {
        LogWarning() << fileName << "already exists, will not overwrite.";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file for Qt WebEngine process:" << fileName;
        LogNormal() << "This file sets the prefix option to parent directory of browser process executable";
    }
}

void deployPlugins(const QString &appDirPath, DeploymentInfo deploymentInfo)
{
    AppDirInfo applicationBundle;
    applicationBundle.path = appDirPath;
    applicationBundle.binaryPath = findAppBinary(appDirPath);

    const QString pluginDestinationPath = appDirPath + "/" + "plugins";
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo);
}

void deployQmlImport(const QString &appDirPath, const QSet<QString> &rpaths, const QString &importSourcePath, const QString &importName)
{
    QString importDestinationPath = appDirPath + "/qml/" + importName;

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    if (QDir().exists(importDestinationPath))
        return;

    recursiveCopyAndDeploy(appDirPath, rpaths, importSourcePath, importDestinationPath);
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to ./qml.
bool deployQmlImports(const QString &appDirPath, DeploymentInfo deploymentInfo, QStringList &qmlDirs)
{
    LogNormal() << "";
    LogNormal() << "Deploying QML imports ";
    LogNormal() << "Application QML file search path(s) is" << qmlDirs;

    // Use qmlimportscanner from QLibraryInfo::BinariesPath
    QString qmlImportScannerPath = QDir::cleanPath(qtToBeBundledInfo.value("QT_INSTALL_BINS")) + "/qmlimportscanner";
    LogDebug() << "Looking for qmlimportscanner at" << qmlImportScannerPath;

    // Fallback: Look relative to the linuxdeployqt binary
    if (!QFile(qmlImportScannerPath).exists()){
        qmlImportScannerPath = QCoreApplication::applicationDirPath() + "/qmlimportscanner";
        LogDebug() << "Fallback, looking for qmlimportscanner at" << qmlImportScannerPath;
    }

    // Verify that we found a qmlimportscanner binary
    if (!QFile(qmlImportScannerPath).exists()) {
        LogError() << "qmlimportscanner not found at" << qmlImportScannerPath;
        LogError() << "Rebuild qtdeclarative/tools/qmlimportscanner";
        return false;
    }

    // build argument list for qmlimportsanner: "-rootPath foo/ -rootPath bar/ -importPath path/to/qt/qml"
    // ("rootPath" points to a directory containing app qml, "importPath" is where the Qt imports are installed)
    QStringList argumentList;
    foreach (const QString &qmlDir, qmlDirs) {
        argumentList.append("-rootPath");
        argumentList.append(qmlDir);
    }
    QString qmlImportsPath = qtToBeBundledInfo.value("QT_INSTALL_QML");
    LogDebug() << "qmlImportsPath candidate:" << qmlImportsPath;

    // Verify that we found a valid qmlImportsPath
    if (!QFile(qmlImportsPath + "/QtQml").exists()) {
        LogError() << "Valid qmlImportsPath not found at" << qmlImportsPath;
        LogError() << "Possibly your Qt library has the wrong information in qt_prfxpath, e.g., because it was moved since it was compiled";
        return false;
    }

    LogDebug() << "qmlImportsPath:" << qmlImportsPath;
    argumentList.append( "-importPath");
    argumentList.append(qmlImportsPath);

    // run qmlimportscanner
    QProcess qmlImportScanner;
    LogDebug() << qmlImportScannerPath << argumentList;
    qmlImportScanner.start(qmlImportScannerPath, argumentList);
    if (!qmlImportScanner.waitForStarted()) {
        LogError() << "Could not start qmlimpoortscanner. Process error is" << qmlImportScanner.errorString();
        return false;
    }
    qmlImportScanner.waitForFinished();

    // log qmlimportscanner errors
    qmlImportScanner.setReadChannel(QProcess::StandardError);
    QByteArray errors = qmlImportScanner.readAll();
    if (!errors.isEmpty()) {
        LogWarning() << "QML file parse error (deployment will continue):";
        LogWarning() << errors;
    }

    // parse qmlimportscanner json
    qmlImportScanner.setReadChannel(QProcess::StandardOutput);
    QByteArray json = qmlImportScanner.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        LogError() << "qmlimportscanner output error. Expected json array, got:";
        LogError() << json;
        return false;
    }

    bool qtQuickContolsInUse = false; // condition for QtQuick.PrivateWidgets below

    // deploy each import
    foreach (const QJsonValue &importValue, doc.array()) {
        if (!importValue.isObject())
            continue;

        QJsonObject import = importValue.toObject();
        QString name = import["name"].toString();
        QString path = import["path"].toString();
        QString type = import["type"].toString();

        if (import["name"].toString() == "QtQuick.Controls")
            qtQuickContolsInUse = true;

        LogNormal() << "Deploying QML import" << name;
        LogDebug() << "path:" << path;
        LogDebug() << "type:" << type;

        // Skip imports with missing info - path will be empty if the import is not found.
        if (name.isEmpty() || path.isEmpty()) {
            LogNormal() << "  Skip import: name or path is empty";
            LogNormal() << "";
            continue;
        }

        // Deploy module imports only, skip directory (local/remote) and js imports. These
        // should be deployed as a part of the application build.
        if (type != QStringLiteral("module")) {
            LogNormal() << "  Skip non-module import";
            LogNormal() << "";
            continue;
        }

        // Create the destination path from the name
        // and version (grabbed from the source path)
        // ### let qmlimportscanner provide this.
        name.replace(QLatin1Char('.'), QLatin1Char('/'));
        int secondTolast = path.length() - 2;
        QString version = path.mid(secondTolast);
        if (version.startsWith(QLatin1Char('.')))
            name.append(version);

        deployQmlImport(appDirPath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }

    // Special case:
    // Use of QtQuick/PrivateWidgets is not discoverable at deploy-time.
    // Recreate the run-time logic here as best as we can - deploy it iff
    //      1) QtWidgets library is used
    //      2) QtQuick.Controls is used
    // The intended failure mode is that libwidgetsplugin.dylib will be present
    // in the app bundle but not used at run-time.
    if (deploymentInfo.deployedLibraries.contains("QtWidgets") && qtQuickContolsInUse) {
        LogNormal() << "Deploying QML import QtQuick/PrivateWidgets";
        QString name = "QtQuick/PrivateWidgets";
        QString path = qmlImportsPath + QLatin1Char('/') + name;
        deployQmlImport(appDirPath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }
    return true;
}

void changeQtLibraries(const QList<LibraryInfo> libraries, const QStringList &binaryPaths, const QString &absoluteQtPath)
{
    LogNormal() << "Changing" << binaryPaths << "to link against";
    LogNormal() << "Qt in" << absoluteQtPath;
    QString finalQtPath = absoluteQtPath;

    if (!absoluteQtPath.startsWith("/Library/Libraries"))
        finalQtPath += "/lib/";

    foreach (LibraryInfo library, libraries) {
        const QString oldBinaryId = library.installName;
        const QString newBinaryId = finalQtPath + library.libraryName +  library.binaryPath;
    }
}

void changeQtLibraries(const QString appPath, const QString &qtPath)
{
    const QString appBinaryPath = findAppBinary(appPath);
    const QStringList libraryPaths = findAppLibraries(appPath);
    const QList<LibraryInfo> libraries = getQtLibrariesForPaths(QStringList() << appBinaryPath << libraryPaths, appPath, getBinaryRPaths(appBinaryPath, true));
    if (libraries.isEmpty()) {

        LogWarning() << "Could not find any _external_ Qt libraries to change in" << appPath;
        return;
    } else {
        const QString absoluteQtPath = QDir(qtPath).absolutePath();
        changeQtLibraries(libraries, QStringList() << appBinaryPath << libraryPaths, absoluteQtPath);
    }
}

bool checkAppImagePrerequisites(const QString &appDirPath)
{
    QDirIterator iter(appDirPath, QStringList() << QString::fromLatin1("*.desktop"),
            QDir::Files, QDirIterator::Subdirectories);
    if (!iter.hasNext()) {
        LogError() << "Desktop file missing, creating a default one (you will probably want to edit it)";
        QFile file(appDirPath + "/default.desktop");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=Application\n";
        out << "Exec=AppRun %F\n";
        out << "Icon=default\n";
        out << "Comment=Edit this default file\n";
        out << "Terminal=true\n";
        file.close();
    }

    // TODO: Compare whether the icon filename matches the Icon= entry without ending in the *.desktop file above
    QDirIterator iter2(appDirPath, QStringList() << QString::fromLatin1("*.png"),
            QDir::Files, QDirIterator::Subdirectories);
    if (!iter2.hasNext()) {
        LogError() << "Icon file missing, creating a default one (you will probably want to edit it)";
        QFile file2(appDirPath + "/default.png");
        file2.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out2(&file2);
        out2 << "";
        QTextStream out(&file2);
        file2.close();
    }
    return true;
}

int createAppImage(const QString &appDirPath)
{
    QString appImageCommand = "appimagetool '" + appDirPath + "' --verbose"; // +"' '" + appImagePath + "'";
    int ret = system(appImageCommand.toUtf8().constData());
    LogNormal() << "ret" << ret;
    LogNormal() << "WEXITSTATUS(ret)" << WEXITSTATUS(ret);
    return WEXITSTATUS(ret);
}
