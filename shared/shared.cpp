/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
bool alwaysOwerwriteEnabled = false;
QStringList librarySearchPath;
bool appstoreCompliant = false;
int logLevel = 1;
bool deployFramework = false;

using std::cout;
using std::endl;

bool operator==(const FrameworkInfo &a, const FrameworkInfo &b)
{
    return ((a.frameworkPath == b.frameworkPath) && (a.binaryPath == b.binaryPath));
}

QDebug operator<<(QDebug debug, const FrameworkInfo &info)
{
    debug << "Framework name" << info.frameworkName << "\n";
    debug << "Framework directory" << info.frameworkDirectory << "\n";
    debug << "Framework path" << info.frameworkPath << "\n";
    debug << "Binary directory" << info.binaryDirectory << "\n";
    debug << "Binary name" << info.binaryName << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Version" << info.version << "\n";
    debug << "Install name" << info.installName << "\n";
    debug << "Deployed install name" << info.deployedInstallName << "\n";
    debug << "Source file Path" << info.sourceFilePath << "\n";
    debug << "Framework Destination Directory (relative to bundle)" << info.frameworkDestinationDirectory << "\n";
    debug << "Binary Destination Directory (relative to bundle)" << info.binaryDestinationDirectory << "\n";

    return debug;
}

const QString bundleFrameworkDirectory = "lib"; // the same directory as the main executable; could define a relative subdirectory here

inline QDebug operator<<(QDebug debug, const ApplicationBundleInfo &info)
{
    debug << "Application bundle path" << info.path << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Additional libraries" << info.libraryPaths << "\n";
    return debug;
}

bool copyFilePrintStatus(const QString &from, const QString &to)
{
    if (QFile(to).exists()) {
        if (alwaysOwerwriteEnabled) {
            QFile(to).remove();
        } else {
            qDebug() << "File exists, skip copy:" << to;
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

bool linkFilePrintStatus(const QString &file, const QString &link)
{
    if (QFile(link).exists()) {
        if (QFile(link).symLinkTarget().isEmpty())
            LogError() << link << "exists but it's a file.";
        else
            LogNormal() << "Symlink exists, skipping:" << link;
        return false;
    } else if (QFile::link(file, link)) {
        LogNormal() << " symlink" << link;
        LogNormal() << " points to" << file;
        return true;
    } else {
        LogError() << "failed to symlink" << link;
        LogError() << " to" << file;
        return false;
    }
}

void patch_debugInInfoPlist(const QString &infoPlistPath)
{
    // Older versions of qmake may have the "_debug" binary as
    // the value for CFBundleExecutable. Remove it.
    QFile infoPlist(infoPlistPath);
    infoPlist.open(QIODevice::ReadOnly);
    QByteArray contents = infoPlist.readAll();
    infoPlist.close();
    infoPlist.open(QIODevice::WriteOnly | QIODevice::Truncate);
    contents.replace("_debug", ""); // surely there are no legit uses of "_debug" in an Info.plist
    infoPlist.write(contents);
}

OtoolInfo findDependencyInfo(const QString &binaryPath)
{
    OtoolInfo info;
    info.binaryPath = binaryPath;

    LogDebug() << "Using ldd:";
    LogDebug() << " inspecting" << binaryPath;
    QProcess otool;
    otool.start("ldd", QStringList() << binaryPath);
    otool.waitForFinished();

    if (otool.exitStatus() != QProcess::NormalExit || otool.exitCode() != 0) {
        LogError() << "findDependencyInfo:" << otool.readAllStandardError();
        return info;
    }

    static const QRegularExpression regexp(QStringLiteral(
        "^.+ => (.+) \\("));

    /*
    static const QRegularExpression regexp(QStringLiteral(
        "^\\t(.+) \\(compatibility version (\\d+\\.\\d+\\.\\d+), "
        "current version (\\d+\\.\\d+\\.\\d+)\\)$"));
    */


    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n", QString::SkipEmptyParts);
    if (outputLines.size() < 2) {
        if (output.contains("statically linked") == false){
            LogError() << "Could not parse objdump output:" << output;
        }
        return info;
    }

    outputLines.removeFirst(); // remove line containing the binary path
    if (binaryPath.contains(".so.") || binaryPath.endsWith(".so")) {
        const auto match = regexp.match(outputLines.first());
        if (match.hasMatch()) {
            info.installName = match.captured(1);
            info.compatibilityVersion = QVersionNumber::fromString(match.captured(2));
            info.currentVersion = QVersionNumber::fromString(match.captured(3));
        } else {
            LogError() << "Could not parse objdump output line:" << outputLines.first();
        }
        outputLines.removeFirst();
    }

    for (const QString &outputLine : outputLines) {
        const auto match = regexp.match(outputLine);
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

FrameworkInfo parseOtoolLibraryLine(const QString &line, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs)
{
    FrameworkInfo info;
    QString trimmed = line.trimmed();

    LogDebug() << "parsing" << trimmed;

    if (trimmed.isEmpty())
        return info;

    // Don't deploy low-level libraries in /lib because these tend to break if moved to a system with a different glibc.
    // TODO: Could make bundling these low-level libraries optional but then the bundles might need to
    // use something like patchelf --set-interpreter or http://bitwagon.com/rtldi/rtldi.html
    if (trimmed.startsWith("/lib"))
        return info;

    enum State {QtPath, FrameworkName, LibraryName, Version, End};
    State state = QtPath;
    int part = 0;
    QString name;
    QString qtPath;
    QString suffix = useDebugLibs ? "_debug" : "";

    // Split the line into [Qt-path]/lib/qt[Module].framework/Versions/[Version]/
    QStringList parts = trimmed.split("/");
    while (part < parts.count()) {
        const QString currentPart = parts.at(part).simplified() ;
        ++part;
        if (currentPart == "")
            continue;

        if (state == QtPath) {
            // Check for library name part
            if (part < parts.count() && parts.at(part).contains(".so")) {
                info.frameworkDirectory += "/" + (qtPath + currentPart + "/").simplified();
                LogDebug() << "info.frameworkDirectory:" << info.frameworkDirectory;
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
                        info.frameworkDirectory = path + partsCopy.join("/");
                        break;
                    }
                }
                if (info.frameworkDirectory.isEmpty())
                    info.frameworkDirectory = "/usr/lib/" + partsCopy.join("/");
                if (!info.frameworkDirectory.endsWith("/"))
                    info.frameworkDirectory += "/";
                state = LibraryName;
                --part;
                continue;
            }
            qtPath += (currentPart + "/");

        } if (state == LibraryName) {
            name = currentPart;
            info.isDylib = true;
            info.frameworkName = name;
            info.binaryName = name.left(name.indexOf('.')) + suffix + name.mid(name.indexOf('.'));
            info.deployedInstallName = "$ORIGIN"; // + info.binaryName;
            info.frameworkPath = info.frameworkDirectory + info.binaryName;
            info.sourceFilePath = info.frameworkPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/";
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory;
            info.binaryDirectory = info.frameworkDirectory;
            info.binaryPath = info.frameworkPath;
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

QString findAppBinary(const QString &appBundlePath)
{
    QString binaryPath;

    // FIXME: Do without the need for an AppRun symlink
    // by passing appBinaryPath from main.cpp here
    QString parentDir =  QDir::cleanPath(QFileInfo(appBundlePath).path());
    QString appDir =   QDir::cleanPath(QFileInfo(appBundlePath).baseName());
    binaryPath = parentDir + "/" + appDir + "/AppRun";

    if (QFile::exists(binaryPath))
        return binaryPath;
    LogError() << "Could not find bundle binary for" << appBundlePath;

    return QString();
}

QStringList findAppFrameworkNames(const QString &appBundlePath)
{
    QStringList frameworks;

    // populate the frameworks list with QtFoo.framework etc,
    // as found in /Contents/Frameworks/
    QString searchPath = appBundlePath + bundleFrameworkDirectory;
    QDirIterator iter(searchPath, QStringList() << QString::fromLatin1("*.framework"), QDir::Dirs);
    while (iter.hasNext()) {
        iter.next();
        frameworks << iter.fileInfo().fileName();
    }

    return frameworks;
}



QStringList findAppLibraries(const QString &appBundlePath)
{
    QStringList result;
    // .so
    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*.so"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        result << iter.fileInfo().filePath();
    }
    // .so.*
    QDirIterator iter2(appBundlePath, QStringList() << QString::fromLatin1("*.so.*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter2.hasNext()) {
        iter2.next();
        result << iter2.fileInfo().filePath();
    }
    return result;
}

QStringList findAppBundleFiles(const QString &appBundlePath, bool absolutePath = false)
{
    QStringList result;

    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        if (iter.fileInfo().isSymLink())
            continue;
        result << (absolutePath ? iter.fileInfo().absoluteFilePath() : iter.fileInfo().filePath());
    }

    return result;
}

QList<FrameworkInfo> getQtFrameworks(const QList<DylibInfo> &dependencies, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs)
{
    QList<FrameworkInfo> libraries;
    for (const DylibInfo &dylibInfo : dependencies) {
        FrameworkInfo info = parseOtoolLibraryLine(dylibInfo.binaryPath, appBundlePath, rpaths, useDebugLibs);
        if (info.frameworkName.isEmpty() == false) {
            LogDebug() << "Adding framework:";
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

    QProcess otool;
    otool.start("objdump", QStringList() << "-x" << path);
    otool.waitForFinished();

    if (otool.exitCode() != 0) {
        LogError() << "getBinaryRPaths:" << otool.readAllStandardError();
    }

    if (resolve && executablePath.isEmpty()) {
      executablePath = path;
    }

    QString output = otool.readAllStandardOutput();
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

QList<FrameworkInfo> getQtFrameworks(const QString &path, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs)
{
    const OtoolInfo info = findDependencyInfo(path);
    return getQtFrameworks(info.dependencies, appBundlePath, rpaths + getBinaryRPaths(path), useDebugLibs);
}

QList<FrameworkInfo> getQtFrameworksForPaths(const QStringList &paths, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs)
{
    QList<FrameworkInfo> result;
    QSet<QString> existing;

    foreach (const QString &path, paths) {
        foreach (const FrameworkInfo &info, getQtFrameworks(path, appBundlePath, rpaths, useDebugLibs)) {
            if (!existing.contains(info.frameworkPath)) { // avoid duplicates
                existing.insert(info.frameworkPath);
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

    const auto dependencies = findDependencyInfo(path).dependencies;

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

void recursiveCopyAndDeploy(const QString &appBundlePath, const QSet<QString> &rpaths, const QString &sourcePath, const QString &destinationPath)
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
        recursiveCopyAndDeploy(appBundlePath, rpaths, sourcePath + QLatin1Char('/') + dir, destinationPath + QLatin1Char('/') + dir);
    }
}

QString copyDylib(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/libfoo.dylib
    QString dylibDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString dylibDestinationBinaryPath = dylibDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Create destination directory
    if (!QDir().mkpath(dylibDestinationDirectory)) {
        LogError() << "could not create destination directory" << dylibDestinationDirectory;
        return QString();
    }

    // Retrun if the dylib has aleardy been deployed
    if (QFileInfo(dylibDestinationBinaryPath).exists() && !alwaysOwerwriteEnabled)
        return dylibDestinationBinaryPath;

    // Copy dylib binary
    copyFilePrintStatus(framework.sourceFilePath, dylibDestinationBinaryPath);
    return dylibDestinationBinaryPath;
}

void runPatchelf(QStringList options)
{
    QProcess patchelftool;
    patchelftool.start("patchelf", options);
    patchelftool.waitForFinished();
    if (patchelftool.exitCode() != 0) {
        LogError() << "FIXME: Check whether patchelf is on the $PATH and otherwise inform the user where to get it from";
        LogError() << "runPatchelf:" << patchelftool.readAllStandardError();
        LogError() << "runPatchelf:" << patchelftool.readAllStandardOutput();
    }
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogDebug() << "Using patchelf:";
    LogDebug() << " change identification in" << binaryPath;
    LogDebug() << " to" << id;
    runPatchelf(QStringList() << "--set-rpath" << id << binaryPath);
}

void addRPath(const QString &rpath, const QString &binaryPath)
{
    runPatchelf(QStringList() << "-add_rpath" << rpath << binaryPath);
}

void runStrip(const QString &binaryPath)
{
    if (runStripEnabled == false)
        return;

    LogDebug() << "Using strip:";
    LogDebug() << " stripped" << binaryPath;
    QProcess strip;
    strip.start("strip", QStringList() << "-x" << binaryPath);
    strip.waitForFinished();

    if (strip.exitCode() == 0)
        return;

    if (strip.readAllStandardError().contains("Not enough room for program headers")) {
        LogError() << QFileInfo(binaryPath).completeBaseName() << "already stripped.";
    } else {
        LogError() << "Error stripping" << QFileInfo(binaryPath).completeBaseName() << ":" << strip.readAllStandardError();
        LogError() << "Error stripping" << QFileInfo(binaryPath).completeBaseName() << ":" << strip.readAllStandardOutput();
    }

}

void stripAppBinary(const QString &bundlePath)
{
    runStrip(findAppBinary(bundlePath));
}

/*
    Deploys the the listed frameworks listed into an app bundle.
    The frameworks are searched for dependencies, which are also deployed.
    (deploying Qt3Support will also deploy QtNetwork and QtSql for example.)
    Returns a DeploymentInfo structure containing the Qt path used and a
    a list of actually deployed frameworks.
*/
DeploymentInfo deployQtFrameworks(QList<FrameworkInfo> frameworks,
        const QString &bundlePath, const QStringList &binaryPaths, bool useDebugLibs,
                                  bool useLoaderPath)
{
    LogNormal();
    LogNormal() << "Deploying libraries found inside:" << binaryPaths;
    QStringList copiedFrameworks;
    DeploymentInfo deploymentInfo;
    deploymentInfo.useLoaderPath = useLoaderPath;
    QSet<QString> rpathsUsed;

    while (frameworks.isEmpty() == false) {
        const FrameworkInfo framework = frameworks.takeFirst();
        copiedFrameworks.append(framework.frameworkName);

        if(framework.frameworkName.contains("libQt") and framework.frameworkName.contains("Core.so")) {
            LogNormal() << "Setting deploymentInfo.qtPath to:" << framework.frameworkDirectory;
            deploymentInfo.qtPath = framework.frameworkDirectory;
        }


        if (framework.frameworkDirectory.startsWith(bundlePath)) {
            LogError()  << framework.frameworkName << "already deployed, skipping.";
            continue;
        }

        if (framework.rpathUsed.isEmpty() != true) {
            rpathsUsed << framework.rpathUsed;
        }

        // Copy the framework/dylib to the app bundle.
        const QString deployedBinaryPath = copyDylib(framework, bundlePath);
        // Skip the rest if already was deployed.
        if (deployedBinaryPath.isNull())
            continue;

        runStrip(deployedBinaryPath);

        if (!framework.rpathUsed.length()) {
            changeIdentification(framework.deployedInstallName, deployedBinaryPath);
        }

        // Check for framework dependencies
        QList<FrameworkInfo> dependencies = getQtFrameworks(deployedBinaryPath, bundlePath, rpathsUsed, useDebugLibs);

        foreach (FrameworkInfo dependency, dependencies) {
            if (dependency.rpathUsed.isEmpty() != true) {
                rpathsUsed << dependency.rpathUsed;
            }

            // Deploy framework if necessary.
            if (copiedFrameworks.contains(dependency.frameworkName) == false && frameworks.contains(dependency) == false) {
                frameworks.append(dependency);
            }
        }
    }
    deploymentInfo.deployedFrameworks = copiedFrameworks;

    deploymentInfo.rpathsUsed += rpathsUsed;

    return deploymentInfo;
}

DeploymentInfo deployQtFrameworks(const QString &appBundlePath, const QStringList &additionalExecutables, bool useDebugLibs)
{
   ApplicationBundleInfo applicationBundle;
   applicationBundle.path = appBundlePath;
   LogDebug() << "applicationBundle.path:" << applicationBundle.path;
   applicationBundle.binaryPath = findAppBinary(appBundlePath);
   LogDebug() << "applicationBundle.binaryPath:" << applicationBundle.binaryPath;
   changeIdentification("$ORIGIN/" + bundleFrameworkDirectory, applicationBundle.binaryPath);
   applicationBundle.libraryPaths = findAppLibraries(appBundlePath);
   LogDebug() << "applicationBundle.libraryPaths:" << applicationBundle.libraryPaths;

   LogDebug() << "additionalExecutables:" << additionalExecutables;

   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   LogDebug() << "allBinaryPaths:" << allBinaryPaths;

   QSet<QString> allLibraryPaths = getBinaryRPaths(applicationBundle.binaryPath, true);
   allLibraryPaths.insert(QLibraryInfo::location(QLibraryInfo::LibrariesPath));

   LogDebug() << "allLibraryPaths:" << allLibraryPaths;

   QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(allBinaryPaths, appBundlePath, allLibraryPaths, useDebugLibs);
   if (frameworks.isEmpty() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << "Could not find any external Qt frameworks to deploy in" << appBundlePath;
        LogWarning() << "Perhaps linuxdeployqt was already used on" << appBundlePath << "?";
        LogWarning() << "If so, you will need to rebuild" << appBundlePath << "before trying again.";
        return DeploymentInfo();
   } else {
       return deployQtFrameworks(frameworks, applicationBundle.path, allBinaryPaths, useDebugLibs, !additionalExecutables.isEmpty());
   }
}

void deployPlugins(const ApplicationBundleInfo &appBundleInfo, const QString &pluginSourcePath,
        const QString pluginDestinationPath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    LogNormal() << "Deploying plugins from" << pluginSourcePath;

    if (!pluginSourcePath.contains(deploymentInfo.pluginPath))
        return;

    // Plugin white list:
    QStringList pluginList;

    // Platform plugin:
    pluginList.append("platforms/libqxcb.so");

    // CUPS print support
    pluginList.append("printsupport/libcupsprintersupport.so");

    // Network
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtNetwork.framework"))) {
        QStringList bearerPlugins = QDir(pluginSourcePath +  QStringLiteral("/bearer")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, bearerPlugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("bearer/") + plugin);
        }
    }

    // All image formats (svg if QtSvg.framework is used)
    QStringList imagePlugins = QDir(pluginSourcePath +  QStringLiteral("/imageformats")).entryList(QStringList() << QStringLiteral("*.dylib"));
    foreach (const QString &plugin, imagePlugins) {
        if (plugin.contains(QStringLiteral("qsvg"))) {
            if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtSvg.framework")))
                pluginList.append(QStringLiteral("imageformats/") + plugin);
        } else if (!plugin.endsWith(QStringLiteral("_debug.dylib"))) {
            pluginList.append(QStringLiteral("imageformats/") + plugin);
        }
    }

    // Sql plugins if QtSql.framework is in use
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtSql.framework"))) {
        QStringList sqlPlugins = QDir(pluginSourcePath +  QStringLiteral("/sqldrivers")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, sqlPlugins) {
            if (plugin.endsWith(QStringLiteral("_debug.dylib")))
                continue;

            // Some sql plugins are known to cause app store rejections. Skip or warn for these plugins.
            if (plugin.startsWith(QStringLiteral("libqsqlodbc")) || plugin.startsWith(QStringLiteral("libqsqlpsql"))) {
                LogWarning() << "Plugin" << plugin << "uses private API and is not Mac App store compliant.";
                if (appstoreCompliant) {
                    LogWarning() << "Skip plugin" << plugin;
                    continue;
                }
            }

            pluginList.append(QStringLiteral("sqldrivers/") + plugin);
        }
    }

    // multimedia plugins if QtMultimedia.framework is in use
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtMultimedia.framework"))) {
        QStringList plugins = QDir(pluginSourcePath + QStringLiteral("/mediaservice")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, plugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("mediaservice/") + plugin);
        }
        plugins = QDir(pluginSourcePath + QStringLiteral("/audio")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, plugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("audio/") + plugin);
        }
    }

    foreach (const QString &plugin, pluginList) {
        QString sourcePath = pluginSourcePath + "/" + plugin;
        if (useDebugLibs) {
            // Use debug plugins if found.
            QString debugSourcePath = sourcePath.replace(".dylib", "_debug.dylib");
            if (QFile::exists(debugSourcePath))
                sourcePath = debugSourcePath;
        }

        const QString destinationPath = pluginDestinationPath + "/" + plugin;
        QDir dir;
        dir.mkpath(QFileInfo(destinationPath).path());

        if (copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);
            QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, appBundleInfo.path, deploymentInfo.rpathsUsed, useDebugLibs);
            deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);
        }
    }
}

void createQtConf(const QString &appBundlePath)
{
    // Set Plugins and imports paths. These are relative to App.app/Contents.
    QByteArray contents = "[Paths]\n"
                          "Plugins = plugins\n"
                          "Imports = qml\n"
                          "Qml2Imports = qml\n";

    QString filePath = appBundlePath + "/"; // Is picked up when placed next to the main executable
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {
        LogWarning();
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
        LogNormal() << "This file sets the plugin search path to" << appBundlePath + "/plugins";
    }
}

void deployPlugins(const QString &appBundlePath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    ApplicationBundleInfo applicationBundle;
    applicationBundle.path = appBundlePath;
    applicationBundle.binaryPath = findAppBinary(appBundlePath);

    const QString pluginDestinationPath = appBundlePath + "/" + "plugins";
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo, useDebugLibs);
}

void deployQmlImport(const QString &appBundlePath, const QSet<QString> &rpaths, const QString &importSourcePath, const QString &importName)
{
    QString importDestinationPath = appBundlePath + "/qml/" + importName;

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    if (QDir().exists(importDestinationPath))
        return;

    recursiveCopyAndDeploy(appBundlePath, rpaths, importSourcePath, importDestinationPath);
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to ./qml.
bool deployQmlImports(const QString &appBundlePath, DeploymentInfo deploymentInfo, QStringList &qmlDirs)
{
    LogNormal() << "";
    LogNormal() << "Deploying QML imports ";
    LogNormal() << "Application QML file search path(s) is" << qmlDirs;

    // Use qmlimportscanner from QLibraryInfo::BinariesPath
    QString qmlImportScannerPath = QDir::cleanPath(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlimportscanner");

    // Fallback: Look relative to the linuxdeployqt binary
    if (!QFile(qmlImportScannerPath).exists())
        qmlImportScannerPath = QCoreApplication::applicationDirPath() + "/qmlimportscanner";

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
    QString qmlImportsPath = QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath);
    argumentList.append( "-importPath");
    argumentList.append(qmlImportsPath);

    // run qmlimportscanner
    QProcess qmlImportScanner;
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

        if (import["name"] == "QtQuick.Controls")
            qtQuickContolsInUse = true;

        LogNormal() << "Deploying QML import" << name;

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

        deployQmlImport(appBundlePath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }

    // Special case:
    // Use of QtQuick.PrivateWidgets is not discoverable at deploy-time.
    // Recreate the run-time logic here as best as we can - deploy it iff
    //      1) QtWidgets.framework is used
    //      2) QtQuick.Controls is used
    // The intended failure mode is that libwidgetsplugin.dylib will be present
    // in the app bundle but not used at run-time.
    if (deploymentInfo.deployedFrameworks.contains("QtWidgets.framework") && qtQuickContolsInUse) {
        LogNormal() << "Deploying QML import QtQuick.PrivateWidgets";
        QString name = "QtQuick/PrivateWidgets";
        QString path = qmlImportsPath + QLatin1Char('/') + name;
        deployQmlImport(appBundlePath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }
    return true;
}

void changeQtFrameworks(const QList<FrameworkInfo> frameworks, const QStringList &binaryPaths, const QString &absoluteQtPath)
{
    LogNormal() << "Changing" << binaryPaths << "to link against";
    LogNormal() << "Qt in" << absoluteQtPath;
    QString finalQtPath = absoluteQtPath;

    if (!absoluteQtPath.startsWith("/Library/Frameworks"))
        finalQtPath += "/lib/";

    foreach (FrameworkInfo framework, frameworks) {
        const QString oldBinaryId = framework.installName;
        const QString newBinaryId = finalQtPath + framework.frameworkName +  framework.binaryPath;
    }
}

void changeQtFrameworks(const QString appPath, const QString &qtPath, bool useDebugLibs)
{
    const QString appBinaryPath = findAppBinary(appPath);
    const QStringList libraryPaths = findAppLibraries(appPath);
    const QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(QStringList() << appBinaryPath << libraryPaths, appPath, getBinaryRPaths(appBinaryPath, true), useDebugLibs);
    if (frameworks.isEmpty()) {
        LogWarning();
        LogWarning() << "Could not find any _external_ Qt frameworks to change in" << appPath;
        return;
    } else {
        const QString absoluteQtPath = QDir(qtPath).absolutePath();
        changeQtFrameworks(frameworks, QStringList() << appBinaryPath << libraryPaths, absoluteQtPath);
    }
}

void createAppImage(const QString &appBundlePath)
{
    QString appBaseName = appBundlePath;
    appBaseName.chop(4); // remove ".app" from end

    QString dmgName = appBaseName + ".dmg";

    QFile dmg(dmgName);

    if (dmg.exists() && alwaysOwerwriteEnabled)
        dmg.remove();

    if (dmg.exists()) {
        LogNormal() << "Disk image already exists, skipping .dmg creation for" << dmg.fileName();
    } else {
        LogNormal() << "Creating disk image (.dmg) for" << appBundlePath;
    }

    // More dmg options can be found in the hdiutil man page.
    QStringList options = QStringList()
            << "create" << dmgName
            << "-srcfolder" << appBundlePath
            << "-format" << "UDZO"
            << "-volname" << appBaseName;

    QProcess hdutil;
    hdutil.start("hdiutil", options);
    hdutil.waitForFinished(-1);
}
