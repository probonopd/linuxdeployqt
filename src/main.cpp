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
#include <QProcessEnvironment>
#include <QSettings>
#include <QDirIterator>
#include <QCommandLineParser>

#include "shared.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser cliParser;

    // Even with ParseAsLongOptions set, -h option still show the options as
    // double dash, but the parser interpret it correctly. It seems to be a bug
    // related to Qt.
    cliParser.addHelpOption();
    cliParser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    cliParser.setApplicationDescription(QObject::tr(
        "linuxdeployqt takes an application as input and makes it "
        "self-contained by copying in the Qt libraries and plugins that "
        "the application uses.\n"
        "\n"
        "It deploys the Qt instance that qmake on the $PATH points to, "
        "so make sure that it is the correct one.\n"
        "\n"
        "Plugins related to a Qt library are copied in with the library.\n"
        "\n"
        "See the \"Deploying Applications on Linux\" topic in the "
        "documentation for more information about deployment on Linux."
    ));

    cliParser.addPositionalArgument("file",
                                    "Deploy libraries for this file",
                                    "<app-binary|desktop file>");

    QCommandLineOption verboseOpt(
        "verbose",
        QObject::tr("0 = no output, 1 = error/warning (default), 2 = normal, 3 = debug"),
        "0-3", "1"
    );
    cliParser.addOption(verboseOpt);

    QCommandLineOption noPluginsOpt(
        "no-plugins",
        QObject::tr("Skip plugin deployment"));
    cliParser.addOption(noPluginsOpt);

    QCommandLineOption appimageOpt(
        "appimage",
        QObject::tr("Create an AppImage (implies -bundle-non-qt-libs)"));
    cliParser.addOption(appimageOpt);

    QCommandLineOption noStripOpt(
        "no-strip",
        QObject::tr("Don't run 'strip' on the binaries"));
    cliParser.addOption(noStripOpt);

    QCommandLineOption bundleNonQtLibsOpt(
        "bundle-non-qt-libs",
        QObject::tr("Also bundle non-core, non-Qt libraries"));
    cliParser.addOption(bundleNonQtLibsOpt);

    QCommandLineOption executableOpt(
        "executable",
        QObject::tr("Let the given executable use the deployed libraries too"),
        "path", ""
    );
    cliParser.addOption(executableOpt);

    QCommandLineOption qmldirOpt(
        "qmldir",
        QObject::tr("Scan for QML imports in the given path"),
        "path", ""
    );
    cliParser.addOption(qmldirOpt);

    QCommandLineOption alwaysOverwriteOpt(
        "always-overwrite",
        QObject::tr("Copy files even if the target file exists"));
    cliParser.addOption(alwaysOverwriteOpt);
    /*
     * TODO: Proposed option set. -scan-bin-paths and -scan-qml-paths
     * options may subtitute -executable and -qmldir.
     *
     * -library-blacklist: When deploying required libraries, avoid including
     *                     libraries listed here.
     * -extra-plugins    : Also deploy this plugins.
     * -extra-files      : Also copy these files to the deploy folder (useful for
     *                     including extra required utilities).
     * -qmake            : Use this alternative binary as qmake.
     * -scan-bin-paths   : Scan this paths for binaries and dynamic libraries.
     * -scan-qml-paths   : Scan this directories for Qml imports.
     * -scan-recursive   : Scan directories recursively.
     * -bins-dest        : Destination path for executables.
     * -libs-dest        : Destination path for libraries.
     */
    cliParser.process(app);

    extern QString appBinaryPath;
    appBinaryPath = ""; // Cannot do it in one go due to "extern"

    QString desktopFile = "";
    QString desktopExecEntry = "";
    QString desktopIconEntry = "";

    QString firstArgument = cliParser.positionalArguments().value(0, "");

    if (!firstArgument.isEmpty()) {
        /* If we got a desktop file as the argument, try to figure out the application binary from it.
         * This has the advantage that we can also figure out the icon file this way, and have less work
         * to do when using linuxdeployqt. */
        if (firstArgument.endsWith(".desktop")){
            qDebug() << "Desktop file as first argument:" << firstArgument;
            QSettings * settings = 0;
            settings = new QSettings(firstArgument, QSettings::IniFormat);
            desktopExecEntry = settings->value("Desktop Entry/Exec", "r").toString().split(' ').first().split('/').last().trimmed();
            qDebug() << "desktopExecEntry:" << desktopExecEntry;
            desktopFile = firstArgument;
            desktopIconEntry = settings->value("Desktop Entry/Icon", "r").toString().split(' ').first().split('.').first().trimmed();
            qDebug() << "desktopIconEntry:" << desktopIconEntry;

            QString candidateBin = QDir::cleanPath(QFileInfo(firstArgument).absolutePath() + desktopExecEntry); // Not FHS-like

            /* Search directory for an executable with the name in the Exec= key */
            QString directoryToBeSearched;
            directoryToBeSearched = QDir::cleanPath(QFileInfo(firstArgument).absolutePath());

            QDirIterator it(directoryToBeSearched, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                if((it.fileName() == desktopExecEntry) && (it.fileInfo().isFile()) && (it.fileInfo().isExecutable())){
                    qDebug() << "Found binary from desktop file:" << it.fileInfo().canonicalFilePath();
                    appBinaryPath = it.fileInfo().absoluteFilePath();
                    break;
                }
            }

            /* Only if we could not find it below the directory in which the desktop file resides, search above */
            if(appBinaryPath == ""){
                if(QFileInfo(QDir::cleanPath(QFileInfo(firstArgument).absolutePath() + "/../../bin/" + desktopExecEntry)).exists()){
                    directoryToBeSearched = QDir::cleanPath(QFileInfo(firstArgument).absolutePath() + "/../../");
                } else {
                    directoryToBeSearched = QDir::cleanPath(QFileInfo(firstArgument).absolutePath() + "/../");
                }
                QDirIterator it2(directoryToBeSearched, QDirIterator::Subdirectories);
                while (it2.hasNext()) {
                    it2.next();
                    if((it2.fileName() == desktopExecEntry) && (it2.fileInfo().isFile()) && (it2.fileInfo().isExecutable())){
                        qDebug() << "Found binary from desktop file:" << it2.fileInfo().canonicalFilePath();
                        appBinaryPath = it2.fileInfo().absoluteFilePath();
                        break;
                    }
                }
            }

            if(appBinaryPath == ""){
                if((QFileInfo(candidateBin).isFile()) && (QFileInfo(candidateBin).isExecutable())) {
                    appBinaryPath = QFileInfo(candidateBin).absoluteFilePath();
                } else {
                    LogError() << "Could not determine the path to the executable based on the desktop file\n";
                    return 1;
                }
            }

        } else {
            appBinaryPath = firstArgument;
            appBinaryPath = QFileInfo(QDir::cleanPath(appBinaryPath)).absoluteFilePath();
        }
    }

    // Allow binaries next to linuxdeployqt to be found; this is useful for bundling
    // this application itself together with helper binaries such as patchelf
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString oldPath = env.value("PATH");
    QString newPath = QCoreApplication::applicationDirPath() + ":" + oldPath;
    LogDebug() << newPath;
    setenv("PATH",newPath.toUtf8().constData(),1);

    QString appName = QDir::cleanPath(QFileInfo(appBinaryPath).completeBaseName());

    QString appDir = QDir::cleanPath(appBinaryPath + "/../");
    if (QDir().exists(appDir) == false) {
        qDebug() << "Error: Could not find AppDir" << appDir;
        return 1;
    }

    bool plugins = true;
    bool appimage = false;
    extern bool runStripEnabled;
    extern bool bundleAllButCoreLibs;
    extern bool fhsLikeMode;
    extern QString fhsPrefix;
    extern bool alwaysOwerwriteEnabled;
    QStringList additionalExecutables;
    bool qmldirArgumentUsed = false;
    QStringList qmlDirs;

    /* FHS-like mode is for an application that has been installed to a $PREFIX which is otherwise empty, e.g., /path/to/usr.
     * In this case, we want to construct an AppDir in /path/to. */
    if (QDir().exists((QDir::cleanPath(appBinaryPath + "/../../bin"))) == true) {
        fhsPrefix = QDir::cleanPath(appBinaryPath + "/../../");
        qDebug() << "FHS-like mode with PREFIX, fhsPrefix:" << fhsPrefix;
        fhsLikeMode = true;
    } else {
        qDebug() << "Not using FHS-like mode";
    }

    if (QDir().exists(appBinaryPath)) {
        qDebug() << "app-binary:" << appBinaryPath;
    } else {
        qDebug() << "Error: Could not find app-binary" << appBinaryPath;
        return 1;
    }

    QString appDirPath;
    QString relativeBinPath;
    if(fhsLikeMode == false){
        appDirPath = appDir;
        relativeBinPath = appName;
    } else {
        appDirPath = QDir::cleanPath(fhsPrefix + "/../");
        QString relativePrefix = fhsPrefix.replace(appDirPath+"/", "");
        relativeBinPath = relativePrefix + "/bin/" + appName;
    }
    qDebug() << "appDirPath:" << appDirPath;
    qDebug() << "relativeBinPath:" << relativeBinPath;

    QFile appRun(appDirPath + "/AppRun");
    if(appRun.exists()){
        appRun.remove();
    }

    QFile::link(relativeBinPath, appDirPath + "/AppRun");

    /* Copy the desktop file in place, into the top level of the AppDir */
    if(desktopFile != ""){
        QString destination = QDir::cleanPath(appDirPath + "/" + QFileInfo(desktopFile).fileName());
        if(QFileInfo(destination).exists() == false){
            if (QFile::copy(desktopFile, destination)){
                qDebug() << "Copied" << desktopFile << "to" << destination;
            }
        }
        if(QFileInfo(destination).isFile() == false){
            LogError() << destination << "does not exist and could not be copied there\n";
            return 1;
        }
    }

    /* To make an AppDir, we need to find the icon and copy it in place */
    QStringList candidates;
    QString iconToBeUsed = "";
    if(desktopIconEntry != ""){
        QDirIterator it3(appDirPath, QDirIterator::Subdirectories);
        while (it3.hasNext()) {
            it3.next();
            if((it3.fileName().startsWith(desktopIconEntry)) && ((it3.fileName().endsWith(".png")) || (it3.fileName().endsWith(".svg")) || (it3.fileName().endsWith(".svgz")) || (it3.fileName().endsWith(".xpm")))){
                candidates.append(it3.filePath());
            }
        }
        qDebug() << "Found icons from desktop file:" << candidates;

        /* Select the main icon from the candidates */
        if(candidates.length() == 1){
            iconToBeUsed = candidates.at(0); // The only choice
        } else if(candidates.length() > 1){
            foreach(QString current, candidates) {
                if(current.contains("256")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("128")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("svg")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("svgz")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("512")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("1024")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("64")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("48")){
                    iconToBeUsed = current;
                    continue;
                }
                if(current.contains("xpm")){
                    iconToBeUsed = current;
                    continue;
                }
            }
        }

        /* Copy in place */
        if(iconToBeUsed != ""){
            /* Check if there is already an icon and only if there is not, copy it to the AppDir.
             * As per the ROX AppDir spec, also copying to .DirIcon. */
            QString preExistingToplevelIcon = "";
            if(QFileInfo(appDirPath + "/" + desktopIconEntry + ".xpm").exists() == true){
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".xpm";
                if(QFileInfo(appDirPath + "/.DirIcon").exists() == false) QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if(QFileInfo(appDirPath + "/" + desktopIconEntry + ".svgz").exists() == true){
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".svgz";
                if(QFileInfo(appDirPath + "/.DirIcon").exists() == false) if(QFileInfo(appDirPath + "/.DirIcon").exists() == false) QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if(QFileInfo(appDirPath + "/" + desktopIconEntry + ".svg").exists() == true){
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".svg";
                if(QFileInfo(appDirPath + "/.DirIcon").exists() == false) QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if(QFileInfo(appDirPath + "/" + desktopIconEntry + ".png").exists() == true){
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".png";
                if(QFileInfo(appDirPath + "/.DirIcon").exists() == false) QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }

            if(preExistingToplevelIcon != ""){
                qDebug() << "preExistingToplevelIcon:" << preExistingToplevelIcon;
            } else {
                qDebug() << "iconToBeUsed:" << iconToBeUsed;
                QString targetIconPath = appDirPath + "/" + QFileInfo(iconToBeUsed).fileName();
                if (QFile::copy(iconToBeUsed, targetIconPath)){
                    qDebug() << "Copied" << iconToBeUsed << "to" << targetIconPath;
                    QFile::copy(targetIconPath, appDirPath + "/.DirIcon");
                } else {
                    LogError() << "Could not copy" << iconToBeUsed << "to" << targetIconPath << "\n";
                    exit(1);
                }
            }
        }
    }

    // Set options from command line
    if (cliParser.isSet(noPluginsOpt)) {
        LogDebug() << "Argument found:" << noPluginsOpt.valueName();
        plugins = false;
    }

    if (cliParser.isSet(appimageOpt)) {
        LogDebug() << "Argument found:" << appimageOpt.valueName();
        appimage = true;
        bundleAllButCoreLibs = true;
    }

    if (cliParser.isSet(noStripOpt)) {
        LogDebug() << "Argument found:" << noStripOpt.valueName();
        runStripEnabled = false;
    }

    if (cliParser.isSet(bundleNonQtLibsOpt)) {
        LogDebug() << "Argument found:" << bundleNonQtLibsOpt.valueName();
        bundleAllButCoreLibs = true;
    }

    if (cliParser.isSet(verboseOpt)) {
        LogDebug() << "Argument found:" << verboseOpt.valueName();
        bool ok = false;
        int number = cliParser.value(verboseOpt).toInt(&ok);

        if (ok)
            logLevel = number;
        else
            LogError() << "Could not parse verbose level";
    }

    if (cliParser.isSet(executableOpt)) {
        LogDebug() << "Argument found:" << executableOpt.valueName();
        QString executables = cliParser.value(executableOpt).trimmed();

        if (!executables.isEmpty())
            additionalExecutables << executables;
        else
            LogError() << "Missing executable path";
    }

    if (cliParser.isSet(qmldirOpt)) {
        LogDebug() << "Argument found:" << qmldirOpt.valueName();
        QString dirs = cliParser.value(qmldirOpt).trimmed();
        qmldirArgumentUsed = true;

        if (!dirs.isEmpty())
            qmlDirs << dirs;
        else
            LogError() << "Missing qml directory path";
    }

    if (cliParser.isSet(alwaysOverwriteOpt)) {
        LogDebug() << "Argument found:" << alwaysOverwriteOpt.valueName();
        alwaysOwerwriteEnabled = true;
    }

    if (appimage) {
        if(checkAppImagePrerequisites(appDirPath) == false){
            LogError() << "checkAppImagePrerequisites failed\n";
            return 1;
        }
    }

    DeploymentInfo deploymentInfo = deployQtLibraries(appDirPath, additionalExecutables);

    // Convenience: Look for .qml files in the current directoty if no -qmldir specified.
    if (qmlDirs.isEmpty()) {
        QDir dir;
        if (!dir.entryList(QStringList() << QStringLiteral("*.qml")).isEmpty()) {
            qmlDirs += QStringLiteral(".");
        }
    }

    if (!qmlDirs.isEmpty()) {
        bool ok = deployQmlImports(appDirPath, deploymentInfo, qmlDirs);
        if (!ok && qmldirArgumentUsed)
            return 1; // exit if the user explicitly asked for qml import deployment
        // Update deploymentInfo.deployedLibraries - the QML imports
        // may have brought in extra libraries as dependencies.
        deploymentInfo.deployedLibraries += findAppLibraries(appDirPath);
        deploymentInfo.deployedLibraries = deploymentInfo.deployedLibraries.toSet().toList();
    }

    if (plugins && !deploymentInfo.qtPath.isEmpty()) {
        if (deploymentInfo.pluginPath.isEmpty())
            deploymentInfo.pluginPath = QDir::cleanPath(deploymentInfo.qtPath + "/../plugins");
        deployPlugins(appDirPath, deploymentInfo);
    }

    if (runStripEnabled)
        stripAppBinary(appDirPath);

    if (appimage) {
        int result = createAppImage(appDirPath);
        LogDebug() << "result:" << result;
        exit(result);
    }

    return 0;
}
