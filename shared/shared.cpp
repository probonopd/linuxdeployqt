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

#ifdef Q_OS_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#endif

bool runStripEnabled = true;
bool alwaysOwerwriteEnabled = false;
bool runCodesign = false;
QStringList librarySearchPath;
QString codesignIdentiy;
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

const QString bundleFrameworkDirectory = "Contents/Frameworks";

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

    LogDebug() << "Using otool:";
    LogDebug() << " inspecting" << binaryPath;
    QProcess otool;
    otool.start("otool", QStringList() << "-L" << binaryPath);
    otool.waitForFinished();

    if (otool.exitStatus() != QProcess::NormalExit || otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
        return info;
    }

    static const QRegularExpression regexp(QStringLiteral(
        "^\\t(.+) \\(compatibility version (\\d+\\.\\d+\\.\\d+), "
        "current version (\\d+\\.\\d+\\.\\d+)\\)$"));

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n", QString::SkipEmptyParts);
    if (outputLines.size() < 2) {
        LogError() << "Could not parse otool output:" << output;
        return info;
    }

    outputLines.removeFirst(); // remove line containing the binary path
    if (binaryPath.contains(".framework/") || binaryPath.endsWith(".dylib")) {
        const auto match = regexp.match(outputLines.first());
        if (match.hasMatch()) {
            info.installName = match.captured(1);
            info.compatibilityVersion = QVersionNumber::fromString(match.captured(2));
            info.currentVersion = QVersionNumber::fromString(match.captured(3));
        } else {
            LogError() << "Could not parse otool output line:" << outputLines.first();
        }
        outputLines.removeFirst();
    }

    for (const QString &outputLine : outputLines) {
        const auto match = regexp.match(outputLine);
        if (match.hasMatch()) {
            DylibInfo dylib;
            dylib.binaryPath = match.captured(1);
            dylib.compatibilityVersion = QVersionNumber::fromString(match.captured(2));
            dylib.currentVersion = QVersionNumber::fromString(match.captured(3));
            info.dependencies << dylib;
        } else {
            LogError() << "Could not parse otool output line:" << outputLine;
        }
    }

    return info;
}

FrameworkInfo parseOtoolLibraryLine(const QString &line, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs)
{
    FrameworkInfo info;
    QString trimmed = line.trimmed();

    if (trimmed.isEmpty())
        return info;

    // Don't deploy system libraries.
    if (trimmed.startsWith("/System/Library/") ||
        (trimmed.startsWith("/usr/lib/") && trimmed.contains("libQt") == false) // exception for libQtuitools and libQtlucene
        || trimmed.startsWith("@executable_path") || trimmed.startsWith("@loader_path"))
        return info;

    // Resolve rpath relative libraries.
    if (trimmed.startsWith("@rpath/")) {
        trimmed = trimmed.mid(QStringLiteral("@rpath/").length());
        bool foundInsideBundle = false;
        foreach (const QString &rpath, rpaths) {
            QString path = QDir::cleanPath(rpath + "/" + trimmed);
            // Skip paths already inside the bundle.
            if (!appBundlePath.isEmpty()) {
                if (QDir::isAbsolutePath(appBundlePath)) {
                    if (path.startsWith(QDir::cleanPath(appBundlePath) + "/")) {
                        foundInsideBundle = true;
                        continue;
                    }
                } else {
                    if (path.startsWith(QDir::cleanPath(QDir::currentPath() + "/" + appBundlePath) + "/")) {
                        foundInsideBundle = true;
                        continue;
                    }
                }
            }
            // Try again with substituted rpath.
            FrameworkInfo resolvedInfo = parseOtoolLibraryLine(path, appBundlePath, rpaths, useDebugLibs);
            if (!resolvedInfo.frameworkName.isEmpty() && QFile::exists(resolvedInfo.frameworkPath)) {
                resolvedInfo.rpathUsed = rpath;
                return resolvedInfo;
            }
        }
        if (!rpaths.isEmpty() && !foundInsideBundle) {
            LogError() << "Cannot resolve rpath" << trimmed;
            LogError() << " using" << rpaths;
        }
        return info;
    }

    enum State {QtPath, FrameworkName, DylibName, Version, End};
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
            if (part < parts.count() && parts.at(part).contains(".dylib")) {
                info.frameworkDirectory += "/" + (qtPath + currentPart + "/").simplified();
                state = DylibName;
                continue;
            } else if (part < parts.count() && parts.at(part).endsWith(".framework")) {
                info.frameworkDirectory += "/" + (qtPath + "lib/").simplified();
                state = FrameworkName;
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
                if (currentPart.contains(".framework")) {
                    if (info.frameworkDirectory.isEmpty())
                        info.frameworkDirectory = "/Library/Frameworks/" + partsCopy.join("/");
                    if (!info.frameworkDirectory.endsWith("/"))
                        info.frameworkDirectory += "/";
                    state = FrameworkName;
                    --part;
                    continue;
                } else if (currentPart.contains(".dylib")) {
                    if (info.frameworkDirectory.isEmpty())
                        info.frameworkDirectory = "/usr/lib/" + partsCopy.join("/");
                    if (!info.frameworkDirectory.endsWith("/"))
                        info.frameworkDirectory += "/";
                    state = DylibName;
                    --part;
                    continue;
                }
            }
            qtPath += (currentPart + "/");

        } if (state == FrameworkName) {
            // remove ".framework"
            name = currentPart;
            name.chop(QString(".framework").length());
            info.isDylib = false;
            info.frameworkName = currentPart;
            state = Version;
            ++part;
            continue;
        } if (state == DylibName) {
            name = currentPart;
            info.isDylib = true;
            info.frameworkName = name;
            info.binaryName = name.left(name.indexOf('.')) + suffix + name.mid(name.indexOf('.'));
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.binaryName;
            info.frameworkPath = info.frameworkDirectory + info.binaryName;
            info.sourceFilePath = info.frameworkPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/";
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory;
            info.binaryDirectory = info.frameworkDirectory;
            info.binaryPath = info.frameworkPath;
            state = End;
            ++part;
            continue;
        } else if (state == Version) {
            info.version = currentPart;
            info.binaryDirectory = "Versions/" + info.version;
            info.binaryName = name + suffix;
            info.binaryPath = "/" + info.binaryDirectory + "/" + info.binaryName;
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.frameworkName + info.binaryPath;
            info.frameworkPath = info.frameworkDirectory + info.frameworkName;
            info.sourceFilePath = info.frameworkPath + info.binaryPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/" + info.frameworkName;
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory + "/" + info.binaryDirectory;
            state = End;
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

#ifdef Q_OS_DARWIN
    CFStringRef bundlePath = appBundlePath.toCFString();
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundlePath,
                                                       kCFURLPOSIXPathStyle, true);
    CFRelease(bundlePath);
    CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
    if (bundle) {
        CFURLRef executableURL = CFBundleCopyExecutableURL(bundle);
        if (executableURL) {
            CFURLRef absoluteExecutableURL = CFURLCopyAbsoluteURL(executableURL);
            if (absoluteExecutableURL) {
                CFStringRef executablePath = CFURLCopyFileSystemPath(absoluteExecutableURL,
                                                                     kCFURLPOSIXPathStyle);
                if (executablePath) {
                    binaryPath = QString::fromCFString(executablePath);
                    CFRelease(executablePath);
                }
                CFRelease(absoluteExecutableURL);
            }
            CFRelease(executableURL);
        }
        CFRelease(bundle);
    }
    CFRelease(bundleURL);
#endif

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
    QString searchPath = appBundlePath + "/Contents/Frameworks/";
    QDirIterator iter(searchPath, QStringList() << QString::fromLatin1("*.framework"), QDir::Dirs);
    while (iter.hasNext()) {
        iter.next();
        frameworks << iter.fileInfo().fileName();
    }

    return frameworks;
}

QStringList findAppFrameworkPaths(const QString &appBundlePath)
{
    QStringList frameworks;
    QString searchPath = appBundlePath + "/Contents/Frameworks/";
    QDirIterator iter(searchPath, QStringList() << QString::fromLatin1("*.framework"), QDir::Dirs);
    while (iter.hasNext()) {
        iter.next();
        frameworks << iter.fileInfo().filePath();
    }

    return frameworks;
}

QStringList findAppLibraries(const QString &appBundlePath)
{
    QStringList result;
    // dylibs
    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*.dylib"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        result << iter.fileInfo().filePath();
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

QString resolveDyldPrefix(const QString &path, const QString &loaderPath, const QString &executablePath)
{
    if (path.startsWith("@")) {
        if (path.startsWith(QStringLiteral("@executable_path/"))) {
            // path relative to bundle executable dir
            if (QDir::isAbsolutePath(executablePath)) {
                return QDir::cleanPath(QFileInfo(executablePath).path() + path.mid(QStringLiteral("@executable_path").length()));
            } else {
                return QDir::cleanPath(QDir::currentPath() + "/" +
                                       QFileInfo(executablePath).path() + path.mid(QStringLiteral("@executable_path").length()));
            }
        } else if (path.startsWith(QStringLiteral("@loader_path/"))) {
            // path relative to loader dir
            if (QDir::isAbsolutePath(loaderPath)) {
                return QDir::cleanPath(QFileInfo(loaderPath).path() + path.mid(QStringLiteral("@loader_path").length()));
            } else {
                return QDir::cleanPath(QDir::currentPath() + "/" +
                                       QFileInfo(loaderPath).path() + path.mid(QStringLiteral("@loader_path").length()));
            }
        } else {
          LogError() << "Unexpected prefix" << path;
        }
    }
    return path;
}

QSet<QString> getBinaryRPaths(const QString &path, bool resolve = true, QString executablePath = QString())
{
    QSet<QString> rpaths;

    QProcess otool;
    otool.start("otool", QStringList() << "-l" << path);
    otool.waitForFinished();

    if (otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
    }

    if (resolve && executablePath.isEmpty()) {
      executablePath = path;
    }

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n");
    QStringListIterator i(outputLines);

    while (i.hasNext()) {
        if (i.next().contains("cmd LC_RPATH") && i.hasNext() &&
        i.next().contains("cmdsize") && i.hasNext()) {
            const QString &rpathCmd = i.next();
            int pathStart = rpathCmd.indexOf("path ");
            int pathEnd = rpathCmd.indexOf(" (");
            if (pathStart >= 0 && pathEnd >= 0 && pathStart < pathEnd) {
                QString rpath = rpathCmd.mid(pathStart + 5, pathEnd - pathStart - 5);
                if (resolve) {
                    rpaths << resolveDyldPrefix(rpath, path, executablePath);
                } else {
                    rpaths << rpath;
                }
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
        } else if (file.endsWith(QStringLiteral(".dylib"))) {
            // App store code signing rules forbids code binaries in Contents/Resources/,
            // which poses a problem for deploying mixed .qml/.dylib Qt Quick imports.
            // Solve this by placing the dylibs in Contents/PlugIns/quick, and then
            // creting a symlink to there from the Qt Quick import in Contents/Resources/.
            //
            // Example:
            // MyApp.app/Contents/Resources/qml/QtQuick/Controls/libqtquickcontrolsplugin.dylib ->
            // ../../../../PlugIns/quick/libqtquickcontrolsplugin.dylib
            //

            // The .dylib destination path:
            QString fileDestinationDir = appBundlePath + QStringLiteral("/Contents/PlugIns/quick/");
            QDir().mkpath(fileDestinationDir);
            QString fileDestinationPath = fileDestinationDir + file;

            // The .dylib symlink destination path:
            QString linkDestinationPath = destinationPath + QLatin1Char('/') + file;

            // The (relative) link; with a correct number of "../"'s.
            QString linkPath = QStringLiteral("PlugIns/quick/") + file;
            int cdupCount = linkDestinationPath.count(QStringLiteral("/")) - appBundlePath.count(QStringLiteral("/"));
            for (int i = 0; i < cdupCount - 2; ++i)
                linkPath.prepend("../");

            if (copyFilePrintStatus(fileSourcePath, fileDestinationPath)) {
                linkFilePrintStatus(linkPath, linkDestinationPath);

                runStrip(fileDestinationPath);
                bool useDebugLibs = false;
                bool useLoaderPath = false;
                QList<FrameworkInfo> frameworks = getQtFrameworks(fileDestinationPath, appBundlePath, rpaths, useDebugLibs);
                deployQtFrameworks(frameworks, appBundlePath, QStringList(fileDestinationPath), useDebugLibs, useLoaderPath);
            }
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

QString copyFramework(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/Foo.framework/Versions/5/QtFoo
    QString frameworkDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString frameworkBinaryDestinationDirectory = frameworkDestinationDirectory + QLatin1Char('/') + framework.binaryDirectory;
    QString frameworkDestinationBinaryPath = frameworkBinaryDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Return if the framework has aleardy been deployed
    if (QDir(frameworkDestinationDirectory).exists() && !alwaysOwerwriteEnabled)
        return QString();

    // Create destination directory
    if (!QDir().mkpath(frameworkBinaryDestinationDirectory)) {
        LogError() << "could not create destination directory" << frameworkBinaryDestinationDirectory;
        return QString();
    }

    // Now copy the framework. Some parts should be left out (headers/, .prl files).
    // Some parts should be included (Resources/, symlink structure). We want this
    // function to make as few assumtions about the framework as possible while at
    // the same time producing a codesign-compatible framework.

    // Copy framework binary
    copyFilePrintStatus(framework.sourceFilePath, frameworkDestinationBinaryPath);

    // Copy Resouces/, Libraries/ and Helpers/
    const QString resourcesSourcePath = framework.frameworkPath + "/Resources";
    const QString resourcesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Resources";
    recursiveCopy(resourcesSourcePath, resourcesDestianationPath);
    const QString librariesSourcePath = framework.frameworkPath + "/Libraries";
    const QString librariesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Libraries";
    bool createdLibraries = recursiveCopy(librariesSourcePath, librariesDestianationPath);
    const QString helpersSourcePath = framework.frameworkPath + "/Helpers";
    const QString helpersDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Helpers";
    bool createdHelpers = recursiveCopy(helpersSourcePath, helpersDestianationPath);

    // Create symlink structure. Links at the framework root point to Versions/Current/
    // which again points to the actual version:
    // QtFoo.framework/QtFoo -> Versions/Current/QtFoo
    // QtFoo.framework/Resources -> Versions/Current/Resources
    // QtFoo.framework/Versions/Current -> 5
    linkFilePrintStatus("Versions/Current/" + framework.binaryName, frameworkDestinationDirectory + "/" + framework.binaryName);
    linkFilePrintStatus("Versions/Current/Resources", frameworkDestinationDirectory + "/Resources");
    if (createdLibraries)
        linkFilePrintStatus("Versions/Current/Libraries", frameworkDestinationDirectory + "/Libraries");
    if (createdHelpers)
        linkFilePrintStatus("Versions/Current/Helpers", frameworkDestinationDirectory + "/Helpers");
    linkFilePrintStatus(framework.version, frameworkDestinationDirectory + "/Versions/Current");

    // Correct Info.plist location for frameworks produced by older versions of qmake
    // Contents/Info.plist should be Versions/5/Resources/Info.plist
    const QString legacyInfoPlistPath = framework.frameworkPath + "/Contents/Info.plist";
    const QString correctInfoPlistPath = frameworkDestinationDirectory + "/Resources/Info.plist";
    if (QFile(legacyInfoPlistPath).exists()) {
        copyFilePrintStatus(legacyInfoPlistPath, correctInfoPlistPath);
        patch_debugInInfoPlist(correctInfoPlistPath);
    }
    return frameworkDestinationBinaryPath;
}

void runInstallNameTool(QStringList options)
{
    QProcess installNametool;
    installNametool.start("install_name_tool", options);
    installNametool.waitForFinished();
    if (installNametool.exitCode() != 0) {
        LogError() << installNametool.readAllStandardError();
        LogError() << installNametool.readAllStandardOutput();
    }
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " change identification in" << binaryPath;
    LogDebug() << " to" << id;
    runInstallNameTool(QStringList() << "-id" << id << binaryPath);
}

void changeInstallName(const QString &bundlePath, const FrameworkInfo &framework, const QStringList &binaryPaths, bool useLoaderPath)
{
    const QString absBundlePath = QFileInfo(bundlePath).absoluteFilePath();
    foreach (const QString &binary, binaryPaths) {
        QString deployedInstallName;
        if (useLoaderPath) {
            deployedInstallName = QLatin1String("@loader_path/")
                    + QFileInfo(binary).absoluteDir().relativeFilePath(absBundlePath + QLatin1Char('/') + framework.binaryDestinationDirectory + QLatin1Char('/') + framework.binaryName);
        } else {
            deployedInstallName = framework.deployedInstallName;
        }
        changeInstallName(framework.installName, deployedInstallName, binary);
    }
}

void addRPath(const QString &rpath, const QString &binaryPath)
{
    runInstallNameTool(QStringList() << "-add_rpath" << rpath << binaryPath);
}

void deployRPaths(const QString &bundlePath, const QSet<QString> &rpaths, const QString &binaryPath, bool useLoaderPath)
{
    const QString absFrameworksPath = QFileInfo(bundlePath).absoluteFilePath()
            + QLatin1String("/Contents/Frameworks");
    const QString relativeFrameworkPath = QFileInfo(binaryPath).absoluteDir().relativeFilePath(absFrameworksPath);
    const QString loaderPathToFrameworks = QLatin1String("@loader_path/") + relativeFrameworkPath;
    bool rpathToFrameworksFound = false;
    QStringList args;
    foreach (const QString &rpath, getBinaryRPaths(binaryPath, false)) {
        if (rpath == "@executable_path/../Frameworks" ||
                rpath == loaderPathToFrameworks) {
            rpathToFrameworksFound = true;
            continue;
        }
        if (rpaths.contains(resolveDyldPrefix(rpath, binaryPath, binaryPath))) {
            args << "-delete_rpath" << rpath;
        }
    }
    if (!args.length()) {
        return;
    }
    if (!rpathToFrameworksFound) {
        if (!useLoaderPath) {
            args << "-add_rpath" << "@executable_path/../Frameworks";
        } else {
            args << "-add_rpath" << loaderPathToFrameworks;
        }
    }
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " change rpaths in" << binaryPath;
    LogDebug() << " using" << args;
    runInstallNameTool(QStringList() << args << binaryPath);
}

void deployRPaths(const QString &bundlePath, const QSet<QString> &rpaths, const QStringList &binaryPaths, bool useLoaderPath)
{
    foreach (const QString &binary, binaryPaths) {
        deployRPaths(bundlePath, rpaths, binary, useLoaderPath);
    }
}

void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " in" << binaryPath;
    LogDebug() << " change reference" << oldName;
    LogDebug() << " to" << newName;
    runInstallNameTool(QStringList() << "-change" << oldName << newName << binaryPath);
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
    if (strip.exitCode() != 0) {
        LogError() << strip.readAllStandardError();
        LogError() << strip.readAllStandardOutput();
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
    LogNormal() << "Deploying Qt frameworks found inside:" << binaryPaths;
    QStringList copiedFrameworks;
    DeploymentInfo deploymentInfo;
    deploymentInfo.useLoaderPath = useLoaderPath;
    deploymentInfo.isFramework = bundlePath.contains(".framework");
    QSet<QString> rpathsUsed;

    while (frameworks.isEmpty() == false) {
        const FrameworkInfo framework = frameworks.takeFirst();
        copiedFrameworks.append(framework.frameworkName);

        if (deploymentInfo.qtPath.isNull())
            deploymentInfo.qtPath = QLibraryInfo::location(QLibraryInfo::PrefixPath);

        if (framework.frameworkDirectory.startsWith(bundlePath)) {
            LogError()  << framework.frameworkName << "already deployed, skipping.";
            continue;
        }

        // Install_name_tool the new id into the binaries
        if (framework.rpathUsed.isEmpty()) {
            changeInstallName(bundlePath, framework, binaryPaths, useLoaderPath);
        } else {
            rpathsUsed << framework.rpathUsed;
        }

        // Copy the framework/dylib to the app bundle.
        const QString deployedBinaryPath = framework.isDylib ? copyDylib(framework, bundlePath)
                                                             : copyFramework(framework, bundlePath);
        // Skip the rest if already was deployed.
        if (deployedBinaryPath.isNull())
            continue;

        runStrip(deployedBinaryPath);

        // Install_name_tool it a new id.
        if (!framework.rpathUsed.length()) {
            changeIdentification(framework.deployedInstallName, deployedBinaryPath);
        }

        // Check for framework dependencies
        QList<FrameworkInfo> dependencies = getQtFrameworks(deployedBinaryPath, bundlePath, rpathsUsed, useDebugLibs);

        foreach (FrameworkInfo dependency, dependencies) {
            if (dependency.rpathUsed.isEmpty()) {
                changeInstallName(bundlePath, dependency, QStringList() << deployedBinaryPath, useLoaderPath);
            } else {
                rpathsUsed << dependency.rpathUsed;
            }

            // Deploy framework if necessary.
            if (copiedFrameworks.contains(dependency.frameworkName) == false && frameworks.contains(dependency) == false) {
                frameworks.append(dependency);
            }
        }
    }
    deploymentInfo.deployedFrameworks = copiedFrameworks;
    deployRPaths(bundlePath, rpathsUsed, binaryPaths, useLoaderPath);
    deploymentInfo.rpathsUsed += rpathsUsed;
    return deploymentInfo;
}

DeploymentInfo deployQtFrameworks(const QString &appBundlePath, const QStringList &additionalExecutables, bool useDebugLibs)
{
   ApplicationBundleInfo applicationBundle;
   applicationBundle.path = appBundlePath;
   applicationBundle.binaryPath = findAppBinary(appBundlePath);
   applicationBundle.libraryPaths = findAppLibraries(appBundlePath);
   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   QSet<QString> allLibraryPaths = getBinaryRPaths(applicationBundle.binaryPath, true);
   allLibraryPaths.insert(QLibraryInfo::location(QLibraryInfo::LibrariesPath));
   QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(allBinaryPaths, appBundlePath, allLibraryPaths, useDebugLibs);
   if (frameworks.isEmpty() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << "Could not find any external Qt frameworks to deploy in" << appBundlePath;
        LogWarning() << "Perhaps macdeployqt was already used on" << appBundlePath << "?";
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
    pluginList.append("platforms/libqcocoa.dylib");

    // Cocoa print support
    pluginList.append("printsupport/libcocoaprintersupport.dylib");

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
                          "Plugins = PlugIns\n"
                          "Imports = Resources/qml\n"
                          "Qml2Imports = Resources/qml\n";

    QString filePath = appBundlePath + "/Contents/Resources/";
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << fileName << "already exists, will not overwrite.";
        LogWarning() << "To make sure the plugins are loaded from the correct location,";
        LogWarning() << "please make sure qt.conf contains the following lines:";
        LogWarning() << "[Paths]";
        LogWarning() << "  Plugins = PlugIns";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file:" << fileName;
        LogNormal() << "This file sets the plugin search path to" << appBundlePath + "/Contents/PlugIns";
    }
}

void deployPlugins(const QString &appBundlePath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    ApplicationBundleInfo applicationBundle;
    applicationBundle.path = appBundlePath;
    applicationBundle.binaryPath = findAppBinary(appBundlePath);

    const QString pluginDestinationPath = appBundlePath + "/" + "Contents/PlugIns";
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo, useDebugLibs);
}

void deployQmlImport(const QString &appBundlePath, const QSet<QString> &rpaths, const QString &importSourcePath, const QString &importName)
{
    QString importDestinationPath = appBundlePath + "/Contents/Resources/qml/" + importName;

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    if (QDir().exists(importDestinationPath))
        return;

    recursiveCopyAndDeploy(appBundlePath, rpaths, importSourcePath, importDestinationPath);
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to Contents/Resources/qml.
bool deployQmlImports(const QString &appBundlePath, DeploymentInfo deploymentInfo, QStringList &qmlDirs)
{
    LogNormal() << "";
    LogNormal() << "Deploying QML imports ";
    LogNormal() << "Application QML file search path(s) is" << qmlDirs;

    // Use qmlimportscanner from QLibraryInfo::BinariesPath
    QString qmlImportScannerPath = QDir::cleanPath(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlimportscanner");

    // Fallback: Look relative to the macdeployqt binary
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
        foreach (const QString &binary, binaryPaths)
            changeInstallName(oldBinaryId, newBinaryId, binary);
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

void codesignFile(const QString &identity, const QString &filePath)
{
    if (!runCodesign)
        return;

    LogNormal() << "codesign" << filePath;

    QProcess codesign;
    codesign.start("codesign", QStringList() << "--preserve-metadata=identifier,entitlements"
                                             << "--force" << "-s" << identity << filePath);
    codesign.waitForFinished(-1);

    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "Codesign signing error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }
}

QSet<QString> codesignBundle(const QString &identity,
                             const QString &appBundlePath,
                             QList<QString> additionalBinariesContainingRpaths)
{
    // Code sign all binaries in the app bundle. This needs to
    // be done inside-out, e.g sign framework dependencies
    // before the main app binary. The codesign tool itself has
    // a "--deep" option to do this, but usage when signing is
    // not recommended: "Signing with --deep is for emergency
    // repairs and temporary adjustments only."

    LogNormal() << "";
    LogNormal() << "Signing" << appBundlePath << "with identity" << identity;

    QStack<QString> pendingBinaries;
    QSet<QString> pendingBinariesSet;
    QSet<QString> signedBinaries;

    // Create the root code-binary set. This set consists of the application
    // executable(s) and the plugins.
    QString appBundleAbsolutePath = QFileInfo(appBundlePath).absoluteFilePath();
    QString rootBinariesPath = appBundleAbsolutePath + "/Contents/MacOS/";
    QStringList foundRootBinaries = QDir(rootBinariesPath).entryList(QStringList() << "*", QDir::Files);
    foreach (const QString &binary, foundRootBinaries) {
        QString binaryPath = rootBinariesPath + binary;
        pendingBinaries.push(binaryPath);
        pendingBinariesSet.insert(binaryPath);
        additionalBinariesContainingRpaths.append(binaryPath);
    }

    bool getAbsoltuePath = true;
    QStringList foundPluginBinaries = findAppBundleFiles(appBundlePath + "/Contents/PlugIns/", getAbsoltuePath);
    foreach (const QString &binary, foundPluginBinaries) {
         pendingBinaries.push(binary);
         pendingBinariesSet.insert(binary);
    }

    // Add frameworks for processing.
    QStringList frameworkPaths = findAppFrameworkPaths(appBundlePath);
    foreach (const QString &frameworkPath, frameworkPaths) {

        // Add all files for a framework as a catch all.
        QStringList bundleFiles = findAppBundleFiles(frameworkPath, getAbsoltuePath);
        foreach (const QString &binary, bundleFiles) {
            pendingBinaries.push(binary);
            pendingBinariesSet.insert(binary);
        }

        // Prioritise first to sign any additional inner bundles found in the Helpers folder (e.g
        // used by QtWebEngine).
        QDirIterator helpersIterator(frameworkPath, QStringList() << QString::fromLatin1("Helpers"), QDir::Dirs | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (helpersIterator.hasNext()) {
            helpersIterator.next();
            QString helpersPath = helpersIterator.filePath();
            QStringList innerBundleNames = QDir(helpersPath).entryList(QStringList() << "*.app", QDir::Dirs);
            foreach (const QString &innerBundleName, innerBundleNames)
                signedBinaries += codesignBundle(identity,
                                                 helpersPath + "/" + innerBundleName,
                                                 additionalBinariesContainingRpaths);
        }

        // Also make sure to sign any libraries that will not be found by otool because they
        // are not linked and won't be seen as a dependency.
        QDirIterator librariesIterator(frameworkPath, QStringList() << QString::fromLatin1("Libraries"), QDir::Dirs | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (librariesIterator.hasNext()) {
            librariesIterator.next();
            QString librariesPath = librariesIterator.filePath();
            bundleFiles = findAppBundleFiles(librariesPath, getAbsoltuePath);
            foreach (const QString &binary, bundleFiles) {
                pendingBinaries.push(binary);
                pendingBinariesSet.insert(binary);
            }
        }
    }

    // Sign all binaries; use otool to find and sign dependencies first.
    while (!pendingBinaries.isEmpty()) {
        QString binary = pendingBinaries.pop();
        if (signedBinaries.contains(binary))
            continue;

        // Check if there are unsigned dependencies, sign these first.
        QStringList dependencies =
                getBinaryDependencies(rootBinariesPath, binary, additionalBinariesContainingRpaths).toSet()
                .subtract(signedBinaries)
                .subtract(pendingBinariesSet)
                .toList();

        if (!dependencies.isEmpty()) {
            pendingBinaries.push(binary);
            pendingBinariesSet.insert(binary);
            int dependenciesSkipped = 0;
            foreach (const QString &dependency, dependencies) {
                // Skip dependencies that are outside the current app bundle, because this might
                // cause a codesign error if the current bundle is part of the dependency (e.g.
                // a bundle is part of a framework helper, and depends on that framework).
                // The dependencies will be taken care of after the current bundle is signed.
                if (!dependency.startsWith(appBundleAbsolutePath)) {
                    ++dependenciesSkipped;
                    LogNormal() << "Skipping outside dependency: " << dependency;
                    continue;
                }
                pendingBinaries.push(dependency);
                pendingBinariesSet.insert(dependency);
            }

            // If all dependencies were skipped, make sure the binary is actually signed, instead
            // of going into an infinite loop.
            if (dependenciesSkipped == dependencies.size()) {
                pendingBinaries.pop();
            } else {
                continue;
            }
        }

        // All dependencies are signed, now sign this binary.
        codesignFile(identity, binary);
        signedBinaries.insert(binary);
        pendingBinariesSet.remove(binary);
    }

    LogNormal() << "Finished codesigning " << appBundlePath << "with identity" << identity;

    // Verify code signature
    QProcess codesign;
    codesign.start("codesign", QStringList() << "--deep" << "-v" << appBundlePath);
    codesign.waitForFinished(-1);
    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "codesign verification error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }

    return signedBinaries;
}

void codesign(const QString &identity, const QString &appBundlePath) {
    codesignBundle(identity, appBundlePath, QList<QString>());
}

void createDiskImage(const QString &appBundlePath)
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

void fixupFramework(const QString &frameworkName)
{
    // Expected framework name looks like "Foo.framework"
    QStringList parts = frameworkName.split(".");
    if (parts.count() < 2) {
        LogError() << "fixupFramework: Unexpected framework name" << frameworkName;
        return;
    }

    // Assume framework binary path is Foo.framework/Foo
    QString frameworkBinary = frameworkName + QStringLiteral("/") + parts[0];

    // Xcode expects to find Foo.framework/Versions/A when code
    // signing, while qmake typically generates numeric versions.
    // Create symlink to the actual version in the framework.
    linkFilePrintStatus("Current", frameworkName + "/Versions/A");

    // Set up @rpath structure.
    changeIdentification("@rpath/" + frameworkBinary, frameworkBinary);
    addRPath("@loader_path/../../Contents/Frameworks/", frameworkBinary);
}
