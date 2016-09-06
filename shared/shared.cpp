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
bool alwaysOwerwriteEnabled = false;
QStringList librarySearchPath;
bool appstoreCompliant = false;
int logLevel = 1;
bool deployLibrary = false;

using std::cout;
using std::endl;

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

    static const QRegularExpression regexp(QStringLiteral(
        "^.+ => (.+) \\("));

    QString output = ldd.readAllStandardOutput();
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

LibraryInfo parseLddLibraryLine(const QString &line, const QString &appDirPath, const QSet<QString> &rpaths)
{
    LibraryInfo info;
    QString trimmed = line.trimmed();

    LogDebug() << "parsing" << trimmed;

    if (trimmed.isEmpty())
        return info;

    // Don't deploy low-level libraries in /lib because these tend to break if moved to a system with a different glibc.
    // TODO: Could make bundling these low-level libraries optional but then the bundles might need to
    // use something like patchelf --set-interpreter or http://bitwagon.com/rtldi/rtldi.html
    if (trimmed.startsWith("/lib"))
        return info;

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
    QString parentDir =  QDir::cleanPath(QFileInfo(appDirPath).path());
    QString appDir =   QDir::cleanPath(QFileInfo(appDirPath).baseName());
    binaryPath = parentDir + "/" + appDir + "/AppRun";

    if (QFile::exists(binaryPath))
        return binaryPath;
    LogError() << "Could not find bundle binary for" << appDirPath;

    return QString();
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
    for (const DylibInfo &dylibInfo : dependencies) {
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

    QProcess ldd;
    ldd.start("objdump", QStringList() << "-x" << path);
    ldd.waitForFinished();

    if (ldd.exitCode() != 0) {
        LogError() << "getBinaryRPaths:" << ldd.readAllStandardError();
    }

    if (resolve && executablePath.isEmpty()) {
      executablePath = path;
    }

    QString output = ldd.readAllStandardOutput();
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
        LogNormal() << QFileInfo(binaryPath).completeBaseName() << "already stripped.";
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

        // Copy the library/dylib to the app bundle.
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

DeploymentInfo deployQtLibraries(const QString &appDirPath, const QStringList &additionalExecutables)
{
   AppDirInfo applicationBundle;
   applicationBundle.path = appDirPath;
   LogDebug() << "applicationBundle.path:" << applicationBundle.path;
   applicationBundle.binaryPath = findAppBinary(appDirPath);
   LogDebug() << "applicationBundle.binaryPath:" << applicationBundle.binaryPath;
   changeIdentification("$ORIGIN/" + bundleLibraryDirectory, applicationBundle.binaryPath);
   applicationBundle.libraryPaths = findAppLibraries(appDirPath);
   LogDebug() << "applicationBundle.libraryPaths:" << applicationBundle.libraryPaths;

   LogDebug() << "additionalExecutables:" << additionalExecutables;

   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   LogDebug() << "allBinaryPaths:" << allBinaryPaths;

   QSet<QString> allLibraryPaths = getBinaryRPaths(applicationBundle.binaryPath, true);
   allLibraryPaths.insert(QLibraryInfo::location(QLibraryInfo::LibrariesPath));

   LogDebug() << "allLibraryPaths:" << allLibraryPaths;

   QList<LibraryInfo> libraries = getQtLibrariesForPaths(allBinaryPaths, appDirPath, allLibraryPaths);
   if (libraries.isEmpty() && !alwaysOwerwriteEnabled) {

        LogWarning() << "Could not find any external Qt libraries to deploy in" << appDirPath;
        LogWarning() << "Perhaps linuxdeployqt was already used on" << appDirPath << "?";
        LogWarning() << "If so, you will need to rebuild" << appDirPath << "before trying again.";
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

    // Platform plugin:
    pluginList.append("platforms/libqxcb.so");

    // CUPS print support
    // TODO: Only install if libQt5PrintSupport.so* is about to be bundled?
    pluginList.append("printsupport/libcupsprintersupport.so");

    // Network
    if (deploymentInfo.deployedLibraries.contains(QStringLiteral("QtNetwork"))) {
        QStringList bearerPlugins = QDir(pluginSourcePath +  QStringLiteral("/bearer")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, bearerPlugins) {
            pluginList.append(QStringLiteral("bearer/") + plugin);
        }
    }

    // All image formats (svg if QtSvg.library is used)
    QStringList imagePlugins = QDir(pluginSourcePath +  QStringLiteral("/imageformats")).entryList(QStringList() << QStringLiteral("*.so"));
    foreach (const QString &plugin, imagePlugins) {
        if (plugin.contains(QStringLiteral("qsvg"))) {
            if (deploymentInfo.deployedLibraries.contains(QStringLiteral("QtSvg")))
                pluginList.append(QStringLiteral("imageformats/") + plugin);
            pluginList.append(QStringLiteral("imageformats/") + plugin);
        }
    }

    // Sql plugins if QtSql.library is in use
    if (deploymentInfo.deployedLibraries.contains(QStringLiteral("QtSql"))) {
        QStringList sqlPlugins = QDir(pluginSourcePath +  QStringLiteral("/sqldrivers")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, sqlPlugins) {
            pluginList.append(QStringLiteral("sqldrivers/") + plugin);
        }
    }

    // multimedia plugins if QtMultimedia.library is in use
    if (deploymentInfo.deployedLibraries.contains(QStringLiteral("QtMultimedia"))) {
        QStringList plugins = QDir(pluginSourcePath + QStringLiteral("/mediaservice")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, plugins) {
            pluginList.append(QStringLiteral("mediaservice/") + plugin);
        }
        plugins = QDir(pluginSourcePath + QStringLiteral("/audio")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, plugins) {
            pluginList.append(QStringLiteral("audio/") + plugin);
        }
    }

    foreach (const QString &plugin, pluginList) {
        QString sourcePath = pluginSourcePath + "/" + plugin;

        const QString destinationPath = pluginDestinationPath + "/" + plugin;
        QDir dir;
        dir.mkpath(QFileInfo(destinationPath).path());

        if (copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);
            QList<LibraryInfo> libraries = getQtLibraries(destinationPath, appDirInfo.path, deploymentInfo.rpathsUsed);
            deployQtLibraries(libraries, appDirInfo.path, QStringList() << destinationPath, deploymentInfo.useLoaderPath);
        }
    }
}

void createQtConf(const QString &appDirPath)
{
    // Set Plugins and imports paths. These are relative to App.app/Contents.
    QByteArray contents = "[Paths]\n"
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

        deployQmlImport(appDirPath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }

    // Special case:
    // Use of QtQuick.PrivateWidgets is not discoverable at deploy-time.
    // Recreate the run-time logic here as best as we can - deploy it iff
    //      1) QtWidgets.library is used
    //      2) QtQuick.Controls is used
    // The intended failure mode is that libwidgetsplugin.dylib will be present
    // in the app bundle but not used at run-time.
    if (deploymentInfo.deployedLibraries.contains("QtWidgets.library") && qtQuickContolsInUse) {
        LogNormal() << "Deploying QML import QtQuick.PrivateWidgets";
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
        LogError() << "Desktop file missing, cannot create AppImage";
        // TODO: Create a generic desktop file (never overwrite an existing one though!)
        return false;
    }

    // TODO: Compare whether the icon filename matches the Icon= entry without ending in the *.desktop file above
    QDirIterator iter2(appDirPath, QStringList() << QString::fromLatin1("*.png"),
            QDir::Files, QDirIterator::Subdirectories);
    if (!iter2.hasNext()) {
        LogError() << "Icon file missing, cannot create AppImage";
        // TODO: Create a generic icon (never overwrite an existing one though!)
        return false;
    }
    return true;
}

void createAppImage(const QString &appDirPath)
{
    QString appImagePath = appDirPath + ".AppImage";

    QFile appImage(appImagePath);
    LogDebug() << "appImageName:" << appImagePath;

    if (appImage.exists() && alwaysOwerwriteEnabled)
        appImage.remove();

    if (appImage.exists()) {
        LogNormal() << "AppImage already exists, skipping .AppImage creation for" << appImage.fileName();
        LogNormal() << "use -always-overwrite to overwrite";
    } else {
        LogNormal() << "Creating AppImage for" << appDirPath;
    }

    QStringList options = QStringList()  << appDirPath << appImagePath;

    QProcess appImageAssistant;
    appImageAssistant.start("AppImageAssistant", options);
    appImageAssistant.waitForFinished(-1);

    // FIXME: How to get the output to appear on the console as it happens rather than after the fact?
    QString output = appImageAssistant.readAllStandardError();
    QStringList outputLines = output.split("\n", QString::SkipEmptyParts);

    for (const QString &outputLine : outputLines) {
        // xorriso spits out a lot of WARNINGs which in the context of AppImage can be safely ignored
        if(!outputLine.contains("WARNING")) {
            LogNormal() << outputLine;
        }
    }

    // AppImageAssistant doesn't always give nonzero error codes, so we check for the presence of the AppImage file
    // This should eventually be fixed in AppImageAssistant
    if (!QFile(appDirPath).exists()) {
        LogError() << "FIXME: TODO: Check for the presence of AppImageAssistant on the $PATH";
        if(appImageAssistant.readAllStandardOutput().isEmpty() == false)
            LogError() << "AppImageAssistant:" << appImageAssistant.readAllStandardOutput();
        if(appImageAssistant.readAllStandardError().isEmpty() == false)
            LogError() << "AppImageAssistant:" << appImageAssistant.readAllStandardError();
    } else {
        LogNormal() << "Created AppImage at" << appImagePath;
    }
}
