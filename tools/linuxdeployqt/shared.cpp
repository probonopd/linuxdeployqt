/****************************************************************************
**
** Copyright (C) 2016-18 The Qt Company Ltd. and Simon Peter
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
#include <QStandardPaths>
#include "shared.h"
#include "excludelist.h"

QString appBinaryPath;
bool runStripEnabled = true;
bool bundleAllButCoreLibs = false;
bool fhsLikeMode = false;
QString fhsPrefix;
bool alwaysOwerwriteEnabled = false;
QStringList librarySearchPath;
bool appstoreCompliant = false;
int logLevel = 1;
int qtDetected = 0;
bool qtDetectionComplete = 0; // As long as Qt is not detected yet, ldd may encounter "not found" messages, continue anyway
bool deployLibrary = false;
QStringList extraQtPlugins;
QStringList excludeLibs;
QStringList ignoreGlob;
bool copyCopyrightFiles = true;

using std::cout;
using std::endl;

static QMap<QString,QString> qtToBeBundledInfo;

enum QtModule
#if defined(Q_COMPILER_CLASS_ENUM) || defined(Q_CC_MSVC)
    : quint64
#endif
{
    QtBluetoothModule         = 0x0000000000000001,
    QtCLuceneModule           = 0x0000000000000002,
    QtConcurrentModule        = 0x0000000000000004,
    QtCoreModule              = 0x0000000000000008,
    QtDeclarativeModule       = 0x0000000000000010,
    QtDesignerComponents      = 0x0000000000000020,
    QtDesignerModule          = 0x0000000000000040,
    QtGuiModule               = 0x0000000000000080,
    QtCluceneModule           = 0x0000000000000100,
    QtHelpModule              = 0x0000000000000200,
    QtMultimediaModule        = 0x0000000000000400,
    QtMultimediaWidgetsModule = 0x0000000000000800,
    QtMultimediaQuickModule   = 0x0000000000001000,
    QtNetworkModule           = 0x0000000000002000,
    QtNfcModule               = 0x0000000000004000,
    QtOpenGLModule            = 0x0000000000008000,
    QtPositioningModule       = 0x0000000000010000,
    QtPrintSupportModule      = 0x0000000000020000,
    QtQmlModule               = 0x0000000000040000,
    QtQuickModule             = 0x0000000000080000,
    QtQuickParticlesModule    = 0x0000000000100000,
    QtScriptModule            = 0x0000000000200000,
    QtScriptToolsModule       = 0x0000000000400000,
    QtSensorsModule           = 0x0000000000800000,
    QtSerialPortModule        = 0x0000000001000000,
    QtSqlModule               = 0x0000000002000000,
    QtSvgModule               = 0x0000000004000000,
    QtTestModule              = 0x0000000008000000,
    QtWidgetsModule           = 0x0000000010000000,
    QtWinExtrasModule         = 0x0000000020000000,
    QtXmlModule               = 0x0000000040000000,
    QtXmlPatternsModule       = 0x0000000080000000,
    QtWebKitModule            = 0x0000000100000000,
    QtWebKitWidgetsModule     = 0x0000000200000000,
    QtQuickWidgetsModule      = 0x0000000400000000,
    QtWebSocketsModule        = 0x0000000800000000,
    QtEnginioModule           = 0x0000001000000000,
    QtWebEngineCoreModule     = 0x0000002000000000,
    QtWebEngineModule         = 0x0000004000000000,
    QtWebEngineWidgetsModule  = 0x0000008000000000,
    QtQmlToolingModule        = 0x0000010000000000,
    Qt3DCoreModule            = 0x0000020000000000,
    Qt3DRendererModule        = 0x0000040000000000,
    Qt3DQuickModule           = 0x0000080000000000,
    Qt3DQuickRendererModule   = 0x0000100000000000,
    Qt3DInputModule           = 0x0000200000000000,
    QtLocationModule          = 0x0000400000000000,
    QtWebChannelModule        = 0x0000800000000000,
    QtTextToSpeechModule      = 0x0001000000000000,
    QtSerialBusModule         = 0x0002000000000000
};

struct QtModuleEntry {
    quint64 module;
    const char *option;
    const char *libraryName;
    const char *translation;
};

static QtModuleEntry qtModuleEntries[] = {
    { QtBluetoothModule, "bluetooth", "Qt5Bluetooth", 0 },
    { QtCLuceneModule, "clucene", "Qt5CLucene", "qt_help" },
    { QtConcurrentModule, "concurrent", "Qt5Concurrent", "qtbase" },
    { QtCoreModule, "core", "Qt5Core", "qtbase" },
    { QtDeclarativeModule, "declarative", "Qt5Declarative", "qtquick1" },
    { QtDesignerModule, "designer", "Qt5Designer", 0 },
    { QtDesignerComponents, "designercomponents", "Qt5DesignerComponents", 0 },
    { QtEnginioModule, "enginio", "Enginio", 0 },
    { QtGuiModule, "gui", "Qt5Gui", "qtbase" },
    { QtHelpModule, "qthelp", "Qt5Help", "qt_help" },
    { QtMultimediaModule, "multimedia", "Qt5Multimedia", "qtmultimedia" },
    { QtMultimediaWidgetsModule, "multimediawidgets", "Qt5MultimediaWidgets", "qtmultimedia" },
    { QtMultimediaQuickModule, "multimediaquick", "Qt5MultimediaQuick_p", "qtmultimedia" },
    { QtNetworkModule, "network", "Qt5Network", "qtbase" },
    { QtNfcModule, "nfc", "Qt5Nfc", 0 },
    { QtOpenGLModule, "opengl", "Qt5OpenGL", 0 },
    { QtPositioningModule, "positioning", "Qt5Positioning", 0 },
    { QtPrintSupportModule, "printsupport", "Qt5PrintSupport", 0 },
    { QtQmlModule, "qml", "Qt5Qml", "qtdeclarative" },
    { QtQmlToolingModule, "qmltooling", "qmltooling", 0 },
    { QtQuickModule, "quick", "Qt5Quick", "qtdeclarative" },
    { QtQuickParticlesModule, "quickparticles", "Qt5QuickParticles", 0 },
    { QtQuickWidgetsModule, "quickwidgets", "Qt5QuickWidgets", 0 },
    { QtScriptModule, "script", "Qt5Script", "qtscript" },
    { QtScriptToolsModule, "scripttools", "Qt5ScriptTools", "qtscript" },
    { QtSensorsModule, "sensors", "Qt5Sensors", 0 },
    { QtSerialPortModule, "serialport", "Qt5SerialPort", "qtserialport" },
    { QtSqlModule, "sql", "Qt5Sql", "qtbase" },
    { QtSvgModule, "svg", "Qt5Svg", 0 },
    { QtTestModule, "test", "Qt5Test", "qtbase" },
    { QtWebKitModule, "webkit", "Qt5WebKit", 0 },
    { QtWebKitWidgetsModule, "webkitwidgets", "Qt5WebKitWidgets", 0 },
    { QtWebSocketsModule, "websockets", "Qt5WebSockets", "qtwebsockets" },
    { QtWidgetsModule, "widgets", "Qt5Widgets", "qtbase" },
    { QtWinExtrasModule, "winextras", "Qt5WinExtras", 0 },
    { QtXmlModule, "xml", "Qt5Xml", "qtbase" },
    { QtXmlPatternsModule, "xmlpatterns", "Qt5XmlPatterns", "qtxmlpatterns" },
    { QtWebEngineCoreModule, "webenginecore", "Qt5WebEngineCore", 0 },
    { QtWebEngineModule, "webengine", "Qt5WebEngine", "qtwebengine" },
    { QtWebEngineWidgetsModule, "webenginewidgets", "Qt5WebEngineWidgets", 0 },
    { Qt3DCoreModule, "3dcore", "Qt53DCore", 0 },
    { Qt3DRendererModule, "3drenderer", "Qt53DRenderer", 0 },
    { Qt3DQuickModule, "3dquick", "Qt53DQuick", 0 },
    { Qt3DQuickRendererModule, "3dquickrenderer", "Qt53DQuickRenderer", 0 },
    { Qt3DInputModule, "3dinput", "Qt53DInput", 0 },
    { QtLocationModule, "geoservices", "Qt5Location", 0 },
    { QtWebChannelModule, "webchannel", "Qt5WebChannel", 0 },
    { QtTextToSpeechModule, "texttospeech", "Qt5TextToSpeech", 0 },
    { QtSerialBusModule, "serialbus", "Qt5SerialBus", 0 }
};

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

static QString bundleLibraryDirectory;

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
        if (alwaysOwerwriteEnabled && QFileInfo(to) != QFileInfo(from)) {
            QFile(to).remove();
        } else {
            LogDebug() << QFileInfo(to).fileName() << "already exists at target location";
            return true;
        }
    }

    QDir dir(QDir::cleanPath(to + "/../"));
    if (!dir.exists()) {
        dir.mkpath(".");
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

bool copyCopyrightFile(QString libPath){

    /* When deploying files (e.g., libraries) from the
     * system, then try to also deploy their copyright file.
     * This is currently only implemented for dpkg-based,
     * Debian-like systems. Pull requests welcome for other
     * systems. */

    if (!copyCopyrightFiles) {
        LogNormal() << "Skipping copyright files deployment as requested by the user";
        return false;
    }

    QString dpkgPath;
    dpkgPath = QStandardPaths::findExecutable("dpkg");
    if(dpkgPath == ""){
        LogNormal() << "dpkg not found, hence not deploying copyright files";
        return false;
    }

    QString dpkgQueryPath;
    dpkgQueryPath = QStandardPaths::findExecutable("dpkg-query");
    if(dpkgQueryPath == ""){
        LogNormal() << "dpkg-query not found, hence not deploying copyright files";
        return false;
    }
    
    QString copyrightFilePath;

    /* Find out which package the file being deployed belongs to */

    QStringList arguments;
    arguments << "-S" << libPath;
    QProcess *myProcess = new QProcess();
    myProcess->start(dpkgPath, arguments);
    myProcess->waitForFinished();
    QString strOut = myProcess->readAllStandardOutput().split(':')[0];
    if(strOut == "") return false;

    /* Find out the copyright file in that package */
    arguments << "-L" << strOut;
    myProcess->start(dpkgQueryPath, arguments);
    myProcess->waitForFinished();
    strOut = myProcess->readAllStandardOutput();

     QStringList outputLines = strOut.split("\n", QString::SkipEmptyParts);

     foreach (QString outputLine, outputLines) {
        if((outputLine.contains("usr/share/doc")) && (outputLine.contains("/copyright")) && (outputLine.contains(" "))){
            // copyrightFilePath = outputLine.split(' ')[1]; // This is not working on multiarch systems; see https://github.com/probonopd/linuxdeployqt/issues/184#issuecomment-345293540
            QStringList parts = outputLine.split(' ');
            copyrightFilePath = parts[parts.size() - 1]; // Grab last element
            break;
        }
     }

     if(copyrightFilePath == "") return false;

     LogDebug() << "copyrightFilePath:" << copyrightFilePath;

     /* Where should we copy this file to? We are assuming the Debian-like path contains
      * the name of the package like so: copyrightFilePath: "/usr/share/doc/libpcre3/copyright"
      * this assumption is most likely only true for Debian-like systems */
     QString packageName = copyrightFilePath.split("/")[copyrightFilePath.split("/").length()-2];
     QString copyrightFileTargetPath;
     if(fhsLikeMode){
         copyrightFileTargetPath = QDir::cleanPath(appBinaryPath + "/../../share/doc/" + packageName + "/copyright");
     } else {
         copyrightFileTargetPath = QDir::cleanPath(appBinaryPath + "/../doc/" + packageName + "/copyright");
     }

     /* Do the actual copying */
     return(copyFilePrintStatus(copyrightFilePath, copyrightFileTargetPath));
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

       if(outputLine.contains("libQt5")){
               qtDetected = 5;
       }
       if(outputLine.contains("libQtCore.so.4")){
               qtDetected = 4;
       }

        // LogDebug() << "ldd outputLine:" << outputLine;
        if ((outputLine.contains("not found")) && (qtDetectionComplete == 1)){
            LogError() << "ldd outputLine:" << outputLine.replace("\t", "");
            LogError() << "for binary:" << binaryPath;
            LogError() << "Please ensure that all libraries can be found by ldd. Aborting.";
            exit(1);
        }
    }

/*
    FIXME: For unknown reasons, this segfaults; see https://travis-ci.org/probonopd/Labrador/builds/339803886#L1320
    if (binaryPath.contains("platformthemes")) {
        LogDebug() << "Not adding dependencies of" << binaryPath << "because we do not bundle dependencies of platformthemes";
        return info;
    }
*/

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
    (void)rpaths;

    if(fhsLikeMode == false){
        bundleLibraryDirectory= "lib"; // relative to bundle
    } else {
        QString relativePrefix = fhsPrefix.replace(appDirPath+"/", "");
        bundleLibraryDirectory = relativePrefix + "/lib/";
    }
    LogDebug() << "bundleLibraryDirectory:" << bundleLibraryDirectory;

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

        The excludelist can be updated by running the bundled script generate-excludelist.sh
        */

        // copy generated excludelist
        QStringList excludelist = generatedExcludelist;

        // append exclude libs
        excludelist += excludeLibs;

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

QStringList findAppLibraries(const QString &appDirPath)
{
	QStringList ignoreGlobAbs;
	QDir appDir(appDirPath);
	foreach (const QString &glob, ignoreGlob) {
		QString globAbs = appDir.filePath(glob);
		LogDebug() << "Ignoring libraries matching" << globAbs;
		ignoreGlobAbs += globAbs;
	}

    QStringList result;
    // .so, .so.*
    QDirIterator iter(appDirPath, QStringList() << QString::fromLatin1("*.so") << QString::fromLatin1("*.so.*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
		if (QDir::match(ignoreGlobAbs, iter.fileInfo().absoluteFilePath())) {
			continue;
		}
        result << iter.fileInfo().filePath();
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
        if (!excludeLibs.contains(QFileInfo(path).baseName()))
        {
            foreach (const LibraryInfo &info, getQtLibraries(path, appDirPath, rpaths)) {
                if (!existing.contains(info.libraryPath)) { // avoid duplicates
                    existing.insert(info.libraryPath);
                    result << info;
                }
            }
        }
    }
    return result;
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
        LogDebug() << "copyCopyrightFile:" << fileSourcePath;
        copyCopyrightFile(fileSourcePath);
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

        QString fileDestinationPath = destinationPath + QLatin1Char('/') + file;
        copyFilePrintStatus(fileSourcePath, fileDestinationPath);
        LogDebug() << "copyCopyrightFile:" << fileSourcePath;
        copyCopyrightFile(fileSourcePath);

        if(fileDestinationPath.endsWith(".so")){

            LogDebug() << "Deploying .so in QML import" << fileSourcePath;
            runStrip(fileDestinationPath);

            // Find out the relative path to the lib/ directory and set it as the rpath
            // FIXME: remove code duplication - the next few lines exist elsewhere already
            if(fhsLikeMode == false){
                bundleLibraryDirectory= "lib"; // relative to bundle
            } else {
                QString relativePrefix = fhsPrefix.replace(appDirPath+"/", "");
                bundleLibraryDirectory = relativePrefix + "/lib/";
            }

            QDir dir(QFileInfo(fileDestinationPath).canonicalFilePath());
            QString relativePath = dir.relativeFilePath(appDirPath + "/" + bundleLibraryDirectory);
            relativePath.remove(0, 3); // remove initial '../'
            changeIdentification("$ORIGIN:$ORIGIN/" + relativePath, QFileInfo(fileDestinationPath).canonicalFilePath());

            QList<LibraryInfo> libraries = getQtLibraries(fileSourcePath, appDirPath, QSet<QString>());
            deployQtLibraries(libraries, appDirPath, QStringList() << destinationPath, false);
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
    LogDebug() << "copyCopyrightFile:" << library.sourceFilePath;
    copyCopyrightFile(library.sourceFilePath);
    return dylibDestinationBinaryPath;
}

QString runPatchelf(QStringList options)
{
    QProcess patchelftool;
    LogDebug() << "options:" << options;
    patchelftool.start("patchelf", options);
    if (!patchelftool.waitForStarted()) {
        if(patchelftool.errorString().contains("No such file or directory")){
            LogError() << "Could not start patchelf.";
            LogError() << "Make sure it is installed on your $PATH, e.g., in /usr/local/bin.";
            LogError() << "You can get it from https://nixos.org/patchelf.html.";
        } else {
            LogError() << "Could not start patchelf tool. Process error is" << patchelftool.errorString();
        }
        exit(1);
    }
    patchelftool.waitForFinished();
    if (patchelftool.exitCode() != 0) {
        LogError() << "runPatchelf:" << patchelftool.readAllStandardError();
        // LogError() << "runPatchelf:" << patchelftool.readAllStandardOutput();
        // exit(1); // Do not exit because this could be a script that patchelf can't work on
    }
    return(patchelftool.readAllStandardOutput().trimmed());
}

bool patchQtCore(const QString &path, const QString &variable, const QString &value)
{
    return true; // ################################### Disabling for now since using qt.conf
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) {
        LogWarning() << QString::fromLatin1("Unable to patch %1: %2").arg(
                    QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    QByteArray content = file.readAll();

    if (content.isEmpty()) {
        LogWarning() << QString::fromLatin1("Unable to patch %1: Could not read file content").arg(
                    QDir::toNativeSeparators(path));
        return false;
    }

    QString searchString = QString::fromLatin1("%1=").arg(variable);
    QByteArray searchStringQByteArray = searchString.toLatin1().data();

    int startPos = content.indexOf(searchStringQByteArray);
    if (startPos != -1) {
        LogNormal() << QString::fromLatin1(
                    "Patching value of %2 in %1 to '%3'").arg(QDir::toNativeSeparators(path), variable, value);
    }
    startPos += searchStringQByteArray.length();
    int endPos = content.indexOf(char(0), startPos);
    if (endPos == -1) {
        LogWarning() << QString::fromLatin1("Unable to patch %1: Internal error").arg(
                    QDir::toNativeSeparators(path));
        return false;
    }

    QByteArray replacement = QByteArray(endPos - startPos, char(0));
    QByteArray replacementBegin = value.toLatin1().data();
    replacement.prepend(replacementBegin);
    replacement.truncate(endPos - startPos);

    content.replace(startPos, endPos - startPos, replacement);

    if (!file.seek(0) || (file.write(content) != content.size())) {
        LogWarning() << QString::fromLatin1("Unable to patch %1: Could not write to file").arg(
                    QDir::toNativeSeparators(path));
        return false;
    }
    return true;
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogNormal() << "Checking rpath in" << binaryPath;
    QString oldRpath = runPatchelf(QStringList() << "--print-rpath" << binaryPath);
    LogDebug() << "oldRpath:" << oldRpath;
    if (oldRpath.startsWith("/")){
        LogDebug() << "Old rpath in" << binaryPath << "starts with /, hence adding it to LD_LIBRARY_PATH";
        // FIXME: Split along ":" characters, check each one, only append to LD_LIBRARY_PATH if not already there
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString oldPath = env.value("LD_LIBRARY_PATH");
        if (not oldPath.contains(oldRpath)){
            QString newPath = oldRpath + ":" + oldPath; // FIXME: If we use a ldd replacement, we still need to observe this path
            // FIXME: Directory layout might be different for system Qt; cannot assume lib/ to always be inside the Qt directory
            LogDebug() << "Added to LD_LIBRARY_PATH:" << newPath;
            setenv("LD_LIBRARY_PATH",newPath.toUtf8().constData(),1);
        }
    }
    LogNormal() << "Changing rpath in" << binaryPath << "to" << id;
    runPatchelf(QStringList() << "--set-rpath" << id << binaryPath);

    // qt_prfxpath:
    if (binaryPath.contains("libQt5Core")) {
        LogDebug() << "libQt5Core detected, patching its hardcoded strings";

        /* https://codereview.qt-project.org/gitweb?p=qt/qttools.git;a=blob_plain;f=src/windeployqt/utils.cpp;h=e89496ea1f371ed86f6937284c1c801daf576572;hb=7be81b804da102b374c2089aac38353a0383c254
         * Search for "qt_prfxpath=<xxx>" in a path, and replace it with "qt_prfxpath=." or "qt_prfxpath=.." */

        if(fhsLikeMode == true){
            patchQtCore(binaryPath, "qt_prfxpath", "..");
        } else {
            patchQtCore(binaryPath, "qt_prfxpath", ".");
        }

        patchQtCore(binaryPath, "qt_adatpath", ".");
        patchQtCore(binaryPath, "qt_docspath", "doc");
        patchQtCore(binaryPath, "qt_hdrspath", "include");
        patchQtCore(binaryPath, "qt_libspath", "lib");
        patchQtCore(binaryPath, "qt_lbexpath", "libexec");
        patchQtCore(binaryPath, "qt_binspath", "bin");
        patchQtCore(binaryPath, "qt_plugpath", "plugins");
        patchQtCore(binaryPath, "qt_impspath", "imports");
        patchQtCore(binaryPath, "qt_qml2path", "qml");
        patchQtCore(binaryPath, "qt_datapath", ".");
        patchQtCore(binaryPath, "qt_trnspath", "translations");
        patchQtCore(binaryPath, "qt_xmplpath", "examples");
        patchQtCore(binaryPath, "qt_demopath", "demos");
        patchQtCore(binaryPath, "qt_tstspath", "tests");
        patchQtCore(binaryPath, "qt_hpfxpath", ".");
        patchQtCore(binaryPath, "qt_hbinpath", "bin");
        patchQtCore(binaryPath, "qt_hdatpath", ".");
        patchQtCore(binaryPath, "qt_stngpath", "."); // e.g., /opt/qt53/etc/xdg; does it load Trolltech.conf from there?

        /* Qt on Arch Linux comes with more hardcoded paths
         * https://github.com/probonopd/linuxdeployqt/issues/98
        patchString(binaryPath, "lib/qt/libexec", "libexec");
        patchString(binaryPath, "lib/qt/plugins", "plugins");
        patchString(binaryPath, "lib/qt/imports", "imports");
        patchString(binaryPath, "lib/qt/qml", "qml");
        patchString(binaryPath, "lib/qt", "");
        patchString(binaryPath, "share/doc/qt", "doc");
        patchString(binaryPath, "include/qt", "include");
        patchString(binaryPath, "share/qt", "");
        patchString(binaryPath, "share/qt/translations", "translations");
        patchString(binaryPath, "share/doc/qt/examples", "examples");
        */
    }

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
        // exit(1); // Do not exit because this could be a script that patchelf can't work on
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
    strip.start("strip", QStringList() << resolvedPath);
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
    (void)bundlePath;

    runStrip(appBinaryPath);
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
    LogNormal() << "Deploying the following libraries:" << binaryPaths;
    QStringList copiedLibraries;
    DeploymentInfo deploymentInfo;
    deploymentInfo.requiresQtWidgetsLibrary = false;
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

        if(library.libraryName.contains("libQt") and library.libraryName.contains("Widgets.so")) {
            deploymentInfo.requiresQtWidgetsLibrary = true;
        }

        if (library.libraryDirectory.startsWith(bundlePath)) {
            LogNormal()  << library.libraryName << "already at target location";
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
            changeIdentification(library.deployedInstallName, QFileInfo(deployedBinaryPath).canonicalFilePath());
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

DeploymentInfo deployQtLibraries(const QString &appDirPath, const QStringList &additionalExecutables, const QString& qmake)
{
   AppDirInfo applicationBundle;

   applicationBundle.path = appDirPath;
   LogDebug() << "applicationBundle.path:" << applicationBundle.path;
   applicationBundle.binaryPath = appBinaryPath;
   LogDebug() << "applicationBundle.binaryPath:" << applicationBundle.binaryPath;

   // Find out whether Qt is a dependency of the application to be bundled
   LddInfo lddInfo = findDependencyInfo(appBinaryPath);

   if(qtDetected != 0){

       // Determine the location of the Qt to be bundled
       LogDebug() << "Using qmake to determine the location of the Qt to be bundled";

       // Use the qmake executable passed in by the user:
       QString qmakePath = qmake;

       if (qmakePath.isEmpty())   {
          // Try to find qmake in the same path as the executable
          QString path = QCoreApplication::applicationDirPath();
          qmakePath = QStandardPaths::findExecutable("qmake", QStringList(path));
       }
       if (qmakePath.isEmpty()) {
           // Try to find a version specific qmake first
           // openSUSE has qmake for Qt 4 and qmake-qt5 for Qt 5
           // Qt 4 on Fedora comes with suffix -qt4
           // http://www.geopsy.org/wiki/index.php/Installing_Qt_binary_packages
           if(qtDetected == 5){
               qmakePath = QStandardPaths::findExecutable("qmake-qt5");
               LogDebug() << "qmake 5";
           } else if(qtDetected == 4){
               qmakePath = QStandardPaths::findExecutable("qmake-qt4");
               LogDebug() << "qmake 4";
           }

           if(qmakePath.isEmpty()){
             // The upstream name of the binary is "qmake", for Qt 4 and Qt 5
             qmakePath = QStandardPaths::findExecutable("qmake");
           }
       }

       if(qmakePath.isEmpty()){
           LogError() << "qmake not found on the $PATH";
           exit(1);
       }

       LogNormal() << "Using qmake: " << qmakePath;
       QString output = captureOutput(qmakePath + " -query");
       LogDebug() << "-query output from qmake:" << output;

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
           exit(1);
       } else {
           LogDebug() << "Qt libs path determined from qmake:" << qtLibsPath;
           QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
           QString oldPath = env.value("LD_LIBRARY_PATH");
           QString newPath = qtLibsPath + ":" + oldPath; // FIXME: If we use a ldd replacement, we still need to observe this path
           // FIXME: Directory layout might be different for system Qt; cannot assume lib/ to always be inside the Qt directory
           LogDebug() << "Changed LD_LIBRARY_PATH:" << newPath;
           setenv("LD_LIBRARY_PATH",newPath.toUtf8().constData(),1);
       }
   }

   /* From now on let ldd exit if it doesn't find something */
   qtDetectionComplete = 1;

   QString libraryPath;
   if(fhsLikeMode == false){
       libraryPath = QFileInfo(applicationBundle.binaryPath).dir().filePath("lib/" + bundleLibraryDirectory);
   } else {
       libraryPath = QFileInfo(applicationBundle.binaryPath).dir().filePath("../lib/" + bundleLibraryDirectory);
   }

   /* Make ldd detect pre-existing libraries in the AppDir.
    * TODO: Consider searching the AppDir for .so* files outside of libraryPath
    * and warning about them not being taken into consideration */
   QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
   QString oldPath = env.value("LD_LIBRARY_PATH");
   QString newPath = libraryPath + ":" + oldPath; // FIXME: If we use a ldd replacement, we still need to observe this path
   LogDebug() << "Changed LD_LIBRARY_PATH:" << newPath;
   setenv("LD_LIBRARY_PATH",newPath.toUtf8().constData(),1);

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

   DeploymentInfo depInfo;
   if (libraries.isEmpty() && !alwaysOwerwriteEnabled) {
      LogWarning() << "Could not find any external Qt libraries to deploy in" << appDirPath;
      LogWarning() << "Perhaps linuxdeployqt was already used on" << appDirPath << "?";
      LogWarning() << "If so, you will need to rebuild" << appDirPath << "before trying again.";
      LogWarning() << "Or ldd does not find the external Qt libraries but sees the system ones.";
      LogWarning() << "If so, you will need to set LD_LIBRARY_PATH to the directory containing the external Qt libraries before trying again.";
      LogWarning() << "FIXME: https://github.com/probonopd/linuxdeployqt/issues/2";
   } else {
      depInfo = deployQtLibraries(libraries, applicationBundle.path, allBinaryPaths, !additionalExecutables.isEmpty());
   }

   foreach (const QString &executable, QStringList() << applicationBundle.binaryPath << additionalExecutables) {
      changeIdentification("$ORIGIN/" + QFileInfo(executable).dir().relativeFilePath(libraryPath), QFileInfo(executable).canonicalFilePath());
   }

   return depInfo;
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
	// Platform plugin contexts - apparently needed to enter special characters
        QStringList platformPluginContexts = QDir(pluginSourcePath +  QStringLiteral("/platforminputcontexts")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, platformPluginContexts) {
            pluginList.append(QStringLiteral("platforminputcontexts/") + plugin);
        }
	// Platform themes - make Qt look more native e.g., on Gtk+ 3 (if available in Qt installation)
        // FIXME: Do not do this until we find a good way to do this without also deploying their dependencies
        // See https://github.com/probonopd/linuxdeployqt/issues/236
	/*
        QStringList platformThemes = QDir(pluginSourcePath +  QStringLiteral("/platformthemes")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, platformThemes) {
            pluginList.append(QStringLiteral("platformthemes/") + plugin);
        }
	*/
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
    if ((containsHowOften(deploymentInfo.deployedLibraries, "libQt5OpenGL"))
		    or (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Gui"))
		    or (containsHowOften(deploymentInfo.deployedLibraries, "libQt5XcbQpa"))
		    or (containsHowOften(deploymentInfo.deployedLibraries, "libxcb-glx"))) {
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

    // Positioning plugins if QtPositioning library is in use
    if (containsHowOften(deploymentInfo.deployedLibraries, "libQt5Positioning")) {
        QStringList posPlugins = QDir(pluginSourcePath +  QStringLiteral("/position")).entryList(QStringList() << QStringLiteral("*.so"));
        foreach (const QString &plugin, posPlugins) {
            pluginList.append(QStringLiteral("position/") + plugin);
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
        QString dstLibexec;
        QString dstResources;
        QString dstTranslations;
        if(fhsLikeMode){
            QFileInfo qfi(appDirInfo.binaryPath);
            QString qtTargetDir = qfi.absoluteDir().absolutePath() + "/../";
            dstLibexec = qtTargetDir + "/libexec";
            dstResources = qtTargetDir + "/resources";
            dstTranslations = qtTargetDir + "/translations";
        } else {
            dstLibexec = appDirInfo.path + "/libexec";
            dstResources = appDirInfo.path + "/resources";
            dstTranslations = appDirInfo.path + "/translations";
        }
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

    if (!extraQtPlugins.isEmpty()) {
        LogNormal() << "Deploying extra plugins.";
        foreach (const QString &plugin, extraQtPlugins) {
            QDir pluginDirectory(pluginSourcePath + "/" + plugin);
            if (pluginDirectory.exists()) {
                //If it is a plugin directory we will deploy the entire directory
                QStringList plugins = pluginDirectory.entryList(QStringList() << QStringLiteral("*.so"));
                foreach (const QString &pluginFile, plugins) {
                    pluginList.append(plugin + "/" + pluginFile);
                    LogDebug() << plugin + "/" + pluginFile << "appended";
                }
            }
            else {
                //If it isn't a directory we asume it is an explicit plugin and we will try to deploy that
                if (!pluginList.contains(plugin)) {
                    if (QFile::exists(pluginSourcePath + "/" + plugin)) {
                        pluginList.append(plugin);
                        LogDebug() << plugin << "appended";
                    }
                    else {
                        LogWarning() <<"The plugin" << pluginSourcePath + "/" + plugin << "could not be found. Please check spelling and try again!";
                    }
                }
                else {
                    LogDebug() <<  "The plugin" << plugin << "was already deployed." ;
                }
            }
        }
    }

    LogNormal() << "pluginList after having detected hopefully all required plugins:" << pluginList;

    foreach (const QString &plugin, pluginList) {
        sourcePath = pluginSourcePath + "/" + plugin;
        destinationPath = pluginDestinationPath + "/" + plugin;
        if(!excludeLibs.contains(QFileInfo(sourcePath).baseName()))
        {
            QDir dir;
            dir.mkpath(QFileInfo(destinationPath).path());
            QList<LibraryInfo> libraries = getQtLibraries(sourcePath, appDirInfo.path, deploymentInfo.rpathsUsed);
            LogDebug() << "Deploying plugin" << sourcePath;
            if (copyFilePrintStatus(sourcePath, destinationPath)) {
                runStrip(destinationPath);
                deployQtLibraries(libraries, appDirInfo.path, QStringList() << destinationPath, deploymentInfo.useLoaderPath);
                /* See whether this makes any difference */
                // Find out the relative path to the lib/ directory and set it as the rpath
                QDir dir(destinationPath);
                QString relativePath = dir.relativeFilePath(appDirInfo.path + "/" + libraries[0].libraryDestinationDirectory);
                relativePath.remove(0, 3); // remove initial '../'
                changeIdentification("$ORIGIN/" + relativePath, QFileInfo(destinationPath).canonicalFilePath());

            }
            LogDebug() << "copyCopyrightFile:" << sourcePath;
            copyCopyrightFile(sourcePath);
        }
    }
}

void createQtConf(const QString &appDirPath)
{
    // Set Plugins and imports paths. These are relative to QCoreApplication::applicationDirPath()
    // which is where the main executable resides; see http://doc.qt.io/qt-5/qt-conf.html
    // See https://github.com/probonopd/linuxdeployqt/issues/ 75, 98, 99
    QByteArray contents;
    if(fhsLikeMode){
        contents = "# Generated by linuxdeployqt\n"
                  "# https://github.com/probonopd/linuxdeployqt/\n"
                  "[Paths]\n"
                  "Prefix = ../\n"
                  "Plugins = plugins\n"
                  "Imports = qml\n"
                  "Qml2Imports = qml\n";
    } else {
        contents = "# Generated by linuxdeployqt\n"
                  "# https://github.com/probonopd/linuxdeployqt/\n"
                  "[Paths]\n"
                      "Prefix = ./\n"
                  "Plugins = plugins\n"
                  "Imports = qml\n"
                  "Qml2Imports = qml\n";
    }

    QString filePath = appDirPath + "/"; // Is picked up when placed next to the main executable
    QString fileName = QDir::cleanPath(appBinaryPath + "/../qt.conf");

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {

        LogWarning() << fileName << "already exists, will not overwrite.";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file:" << fileName;
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
    applicationBundle.binaryPath = appBinaryPath;

   QString pluginDestinationPath;
    if(fhsLikeMode){
        QFileInfo qfi(applicationBundle.binaryPath);
        QString qtTargetDir = qfi.absoluteDir().absolutePath() + "/../";
        pluginDestinationPath = qtTargetDir + "/plugins";
    } else {
        pluginDestinationPath = appDirPath + "/" + "plugins";
    }
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo);
}

void deployQmlImport(const QString &appDirPath, const QSet<QString> &rpaths, const QString &importSourcePath, const QString &importName)
{
    AppDirInfo applicationBundle;
    applicationBundle.path = appDirPath;
    applicationBundle.binaryPath = appBinaryPath;

    QString importDestinationPath;
    if(fhsLikeMode){
        QFileInfo qfi(applicationBundle.binaryPath);
        QString qtTargetDir = qfi.absoluteDir().absolutePath() + "/../";
        importDestinationPath = qtTargetDir + "/qml/" + importName;
    } else {
        importDestinationPath = appDirPath + "/qml/" + importName;
    }

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    // NOTE: this stops the deployment of certain imports if there is a folder in them, for example
    //       - if QtQuick.Controls.Styles is already there QtQuick.Controls is skipped
    //       - if QtQuick.Extras.Private is there QtQuick.Extras is skipped
    QDir destDir(importDestinationPath);
    if (destDir.exists()) {
        destDir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        if(destDir.entryInfoList().length() > 0)
            return;
    }

    recursiveCopyAndDeploy(appDirPath, rpaths, importSourcePath, importDestinationPath);
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to ./qml.
bool deployQmlImports(const QString &appDirPath, DeploymentInfo deploymentInfo, QStringList &qmlDirs)
{
    if(!qtDetected){
        LogDebug() << "Skipping QML imports since no Qt detected";
        return false;
    }

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
        LogError() << "Please install it if you want to bundle QML based applications.";
        return true;
    }

    // build argument list for qmlimportsanner: "-rootPath foo/ -rootPath bar/ -importPath path/to/qt/qml"
    // ("rootPath" points to a directory containing app qml, "importPath" is where the Qt imports are installed)
    QStringList argumentList;
    foreach (const QString &qmlDir, qmlDirs) {
        argumentList.append("-rootPath");
        argumentList.append(qmlDir);
    }

    argumentList.append( "-importPath");
    argumentList.append(qtToBeBundledInfo.value("QT_INSTALL_QML"));

    LogDebug() << "qmlImportsPath (QT_INSTALL_QML):" << qtToBeBundledInfo.value("QT_INSTALL_QML");

    // run qmlimportscanner
    QProcess qmlImportScanner;
    LogDebug() << qmlImportScannerPath << argumentList;
    qmlImportScanner.start(qmlImportScannerPath, argumentList);
    if (!qmlImportScanner.waitForStarted()) {
        LogError() << "Could not start qmlimportscanner. Process error is" << qmlImportScanner.errorString();
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
    if (deploymentInfo.requiresQtWidgetsLibrary && qtQuickContolsInUse) {
        LogNormal() << "Deploying QML import QtQuick/PrivateWidgets";
        QString name = "QtQuick/PrivateWidgets";
        QString path = qtToBeBundledInfo.value("QT_INSTALL_QML") + QLatin1Char('/') + name;
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

    finalQtPath += "/lib/";

    foreach (LibraryInfo library, libraries) {
        const QString oldBinaryId = library.installName;
        const QString newBinaryId = finalQtPath + library.libraryName +  library.binaryPath;
    }
}

void changeQtLibraries(const QString appPath, const QString &qtPath)
{
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
    if(fhsLikeMode == true){
        /* In FHS-like mode, we assume that there will be a desktop file
         * and icon file that appimagetool will be able to pick up */
        return true;
    }

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
    QString appImageCommand = "appimagetool '" + appDirPath + "' --verbose -n -g"; // +"' '" + appImagePath + "'";
    int ret = system(appImageCommand.toUtf8().constData());
    LogNormal() << "ret" << ret;
    LogNormal() << "WEXITSTATUS(ret)" << WEXITSTATUS(ret);
    return WEXITSTATUS(ret);
}

void findUsedModules(DeploymentInfo &info)
{
    LogDebug() << "Creating mask of used modules";

    const QStringList &libraries = info.deployedLibraries;

    const size_t qtModulesCount = sizeof(qtModuleEntries)/sizeof(QtModuleEntry);
    for (size_t i = 0; i < qtModulesCount; ++i) {
        QtModuleEntry &entry = qtModuleEntries[i];
        const QString name = QLatin1String(qtModuleEntries[i].libraryName);

        bool found = false;
        foreach (const QString &library, libraries) {
            if (library.contains(name, Qt::CaseInsensitive)) {
                LogDebug() << "Found dependency:" << name;
                found = true;
                break;
            }
        }

        if (found) {
            info.usedModulesMask |= entry.module;
        }
    }
}

void deployTranslations(const QString &appDirPath, quint64 usedQtModules)
{
    LogDebug() << "Deploying translations...";
    QString qtTranslationsPath = qtToBeBundledInfo.value("QT_INSTALL_TRANSLATIONS");
    if (qtTranslationsPath.isEmpty() || !QFile::exists(qtTranslationsPath)) {
        LogDebug() << "Qt translations path could not be determined, maybe there are no translations?";
        return;
    }

    QString translationsDirPath;
    if (!fhsLikeMode) {
        translationsDirPath = appDirPath + QStringLiteral("/translations");
    } else {
        // TODO: refactor this global variables hack
        QFileInfo appBinaryFI(appBinaryPath);
        QString appRoot = appBinaryFI.absoluteDir().absolutePath() + "/../";
        translationsDirPath = appRoot + QStringLiteral("/translations");
    }

    LogNormal() << "Using" << translationsDirPath << "as translations directory for App";
    LogNormal() << "Using" << qtTranslationsPath << " to search for Qt translations";

    QFileInfo fi(translationsDirPath);
    if (!fi.isDir()) {
        if (!QDir().mkpath(translationsDirPath)) {
            LogError() << "Failed to create translations directory";
        }
    } else {
        LogDebug() << "Translations directory already exists";
    }

    if (!deployTranslations(qtTranslationsPath, translationsDirPath, usedQtModules)) {
        LogError() << "Failed to copy translations";
    }
}

QStringList translationNameFilters(quint64 modules, const QString &prefix)
{
    QStringList result;
    const size_t qtModulesCount = sizeof(qtModuleEntries)/sizeof(QtModuleEntry);
    for (size_t i = 0; i < qtModulesCount; ++i) {
        if ((qtModuleEntries[i].module & modules) && qtModuleEntries[i].translation) {
            const QString name = QLatin1String(qtModuleEntries[i].translation) +
                                 QLatin1Char('_') +  prefix + QStringLiteral(".qm");
            if (!result.contains(name))
                result.push_back(name);
        }
    }
    LogDebug() << "Translation name filters:" << result;
    return result;
}

bool deployTranslations(const QString &sourcePath, const QString &target, quint64 usedQtModules)
{
    LogDebug() << "Translations target is" << target;

    // Find available languages prefixes by checking on qtbase.
    QStringList prefixes;
    QDir sourceDir(sourcePath);
    const QStringList qmFilter = QStringList(QStringLiteral("qtbase_*.qm"));
    foreach (QString qmFile, sourceDir.entryList(qmFilter)) {
        qmFile.chop(3);
        qmFile.remove(0, 7);
        prefixes.push_back(qmFile);
    }
    if (prefixes.isEmpty()) {
        LogError() << "Could not find any translations in "
                   << sourcePath << " (developer build?)";
        return true;
    }
    // Run lconvert to concatenate all files into a single named "qt_<prefix>.qm" in the application folder
    // Use QT_INSTALL_TRANSLATIONS as working directory to keep the command line short.
    const QString absoluteTarget = QFileInfo(target).absoluteFilePath();

    QString lconvertPath = QDir::cleanPath(qtToBeBundledInfo.value("QT_INSTALL_BINS")) + "/lconvert";
    LogDebug() << "Looking for lconvert at" << lconvertPath;

    // Fallback: Look relative to the linuxdeployqt binary
    if (!QFile(lconvertPath).exists()){
        lconvertPath = QCoreApplication::applicationDirPath() + "/lconvert";
        LogDebug() << "Fallback, looking for lconvert at" << lconvertPath;
    }

    // Verify that we found a lconvert binary
    if (!QFile(lconvertPath).exists()) {
        LogError() << "lconvert not found at" << lconvertPath;
        return false;
    }

    LogNormal() << "Found lconvert at" << lconvertPath;

    QStringList arguments;
    foreach (const QString &prefix, prefixes) {
        arguments.clear();
        const QString targetFile = QStringLiteral("qt_") + prefix + QStringLiteral(".qm");
        arguments.append(QStringLiteral("-o"));
        const QString currentTargetFile = absoluteTarget + QLatin1Char('/') + targetFile;
        arguments.append(currentTargetFile);

        foreach (const QFileInfo &qmFileInfo, sourceDir.entryInfoList(translationNameFilters(usedQtModules, prefix)))
            arguments.append(qmFileInfo.absoluteFilePath());

        LogNormal() << "Creating " << currentTargetFile << "...";
        LogDebug() << "lconvert arguments:" << arguments;

        QProcess lconvert;
        lconvert.start(lconvertPath, arguments);
        lconvert.waitForFinished();

        if (lconvert.exitStatus() != QProcess::NormalExit) {
            LogError() << "Fail in lconvert on file" << currentTargetFile;
        }
    } // for prefixes.
    return true;
}
