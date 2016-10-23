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
#include <QDir>
#include <QProcessEnvironment>
#include "../shared/shared.h"
#include <QRegularExpression>
#include <stdlib.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QString appBinaryPath;

    if (argc > 1)
        appBinaryPath = QString::fromLocal8Bit(argv[1]);
        appBinaryPath = QDir::cleanPath(appBinaryPath).trimmed();

    if (argc < 2 || appBinaryPath.startsWith("-")) {
        qDebug() << "Usage: linuxdeployqt app-binary [options]";
        qDebug() << "";
        qDebug() << "Options:";
        qDebug() << "   -verbose=<0-3>      : 0 = no output, 1 = error/warning (default), 2 = normal, 3 = debug";
        qDebug() << "   -no-plugins         : Skip plugin deployment";
        qDebug() << "   -appimage           : Create an AppImage (implies -bundle-non-qt-libs)";
        qDebug() << "   -no-strip           : Don't run 'strip' on the binaries";
        qDebug() << "   -bundle-non-qt-libs : Also bundle non-core, non-Qt libraries";
        qDebug() << "   -executable=<path>  : Let the given executable use the deployed libraries too";
        qDebug() << "   -qmldir=<path>      : Scan for QML imports in the given path";
        qDebug() << "   -always-overwrite   : Copy files even if the target file exists";
        qDebug() << "   -libpath=<path>     : Add the given path to the library search path";
        qDebug() << "";
        qDebug() << "linuxdeployqt takes an application as input and makes it";
        qDebug() << "self-contained by copying in the Qt libraries and plugins that";
        qDebug() << "the application uses.";
        qDebug() << "";
        qDebug() << "It deploys the Qt instance that qmake on the $PATH points to,";
        qDebug() << "so make sure that it is the correct one.";
        qDebug() << "";
        qDebug() << "Plugins related to a Qt library are copied in with the";
        qDebug() << "library. The accessibility, image formats, and text codec";
        qDebug() << "plugins are always copied, unless \"-no-plugins\" is specified.";
        qDebug() << "";
        qDebug() << "See the \"Deploying Applications on Linux\" topic in the";
        qDebug() << "documentation for more information about deployment on Linux.";

        return 1;
    }

    // Allow binaries in the usr/bin subdirectory to be found; this is useful for bundling
    // this application itself together with helper binaries such as patchelf
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString oldPath = env.value("PATH");
    QString newPath = QCoreApplication::applicationDirPath()+ "/usr/bin" + ":" + oldPath;
    qDebug() << newPath;
    setenv("PATH",newPath.toUtf8().constData(),1);

    QString appName = QDir::cleanPath(QFileInfo(appBinaryPath).completeBaseName());

    if (QDir().exists(appBinaryPath)) {
        qDebug() << "app-binary:" << appBinaryPath;
    } else {
        qDebug() << "Error: Could not find app-binary" << appBinaryPath;
        return 1;
    }

    QDir dir;
    // QString appDir = QDir::cleanPath(appFile + "/../" + appName + ".AppDir");
    QString appDir = QDir::cleanPath(appBinaryPath + "/../");

    if (QDir().exists(appDir) == false) {
        qDebug() << "Error: Could not find AppDir" << appDir;
        return 1;
    }

    QString appDirPath = appDir;

    QFile appRun(appDir + "/AppRun");

    if(appRun.exists()){
        appRun.remove();
    }
    QFile::link(appName, appDir + "/AppRun");

    bool plugins = true;
    bool appimage = false;
    extern bool runStripEnabled;
    extern bool bundleAllButCoreLibs;
    extern bool alwaysOwerwriteEnabled;
    extern QStringList librarySearchPath;
    QStringList additionalExecutables;
    bool qmldirArgumentUsed = false;
    QStringList qmlDirs;

    for (int i = 2; i < argc; ++i) {
        QByteArray argument = QByteArray(argv[i]);
        if (argument == QByteArray("-no-plugins")) {
            LogDebug() << "Argument found:" << argument;
            plugins = false;
        } else if (argument == QByteArray("-appimage")) {
            LogDebug() << "Argument found:" << argument;
            appimage = true;
            bundleAllButCoreLibs = true;
        } else if (argument == QByteArray("-no-strip")) {
            LogDebug() << "Argument found:" << argument;
            runStripEnabled = false;
        } else if (argument == QByteArray("-bundle-non-qt-libs")) {
            LogDebug() << "Argument found:" << argument;
            bundleAllButCoreLibs = true;
        } else if (argument.startsWith(QByteArray("-verbose"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            bool ok = false;
            int number = argument.mid(index+1).toInt(&ok);
            if (!ok)
                LogError() << "Could not parse verbose level";
            else
                logLevel = number;
        } else if (argument.startsWith(QByteArray("-executable"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing executable path";
            else
                additionalExecutables << argument.mid(index+1);
        } else if (argument.startsWith(QByteArray("-qmldir"))) {
            LogDebug() << "Argument found:" << argument;
            qmldirArgumentUsed = true;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing qml directory path";
            else
                qmlDirs << argument.mid(index+1);
        } else if (argument.startsWith(QByteArray("-libpath"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing library search path";
            else
                librarySearchPath << argument.mid(index+1);
        } else if (argument == QByteArray("-always-overwrite")) {
            LogDebug() << "Argument found:" << argument;
            alwaysOwerwriteEnabled = true;
        } else if (argument.startsWith("-")) {
            LogError() << "Unknown argument" << argument << "\n";
            return 1;
        }
     }

    if (appimage) {
        if(checkAppImagePrerequisites(appDirPath) == false)
            return 1;
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
        deploymentInfo.pluginPath = QDir::cleanPath(deploymentInfo.qtPath + "/../plugins");
        deployPlugins(appDirPath, deploymentInfo);
        createQtConf(appDirPath);
    }

    if (runStripEnabled)
        stripAppBinary(appDirPath);

    if (appimage) {
        int result = createAppImage(appDirPath);
        LogDebug() << "result:" << result;
        if(result =! 0){
            LogDebug() << "result != 0:" << result;
            exit(result);
        }
    }
    LogDebug() << "exit(0);";
    exit(0);
}
