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

#ifndef LINUX_DEPLOMYMENT_SHARED_H
#define LINUX_DEPLOMYMENT_SHARED_H

#include <QString>
#include <QStringList>
#include <QDebug>
#include <QSet>

extern int logLevel;
#define LogError()      if (logLevel < 0) {} else qDebug() << "ERROR:"
#define LogWarning()    if (logLevel < 1) {} else qDebug() << "WARNING:"
#define LogNormal()     if (logLevel < 2) {} else qDebug() << "Log:"
#define LogDebug()      if (logLevel < 3) {} else qDebug() << "Log:"

extern QString appBinaryPath;
extern bool runStripEnabled;
extern bool bundleAllButCoreLibs;
extern bool bundleEverything;
extern bool fhsLikeMode;
extern QString fhsPrefix;
extern QStringList extraQtPlugins;
extern QStringList excludeLibs;

class LibraryInfo
{
public:
    bool isDylib;
    QString libraryDirectory;
    QString libraryName;
    QString libraryPath;
    QString binaryDirectory;
    QString binaryName;
    QString binaryPath;
    QString rpathUsed;
    QString version;
    QString installName;
    QString deployedInstallName;
    QString sourceFilePath;
    QString libraryDestinationDirectory;
    QString binaryDestinationDirectory;
};

class DylibInfo
{
public:
    QString binaryPath;
};

class LddInfo
{
public:
    QString installName;
    QString binaryPath;
    QList<DylibInfo> dependencies;
};

bool operator==(const LibraryInfo &a, const LibraryInfo &b);
QDebug operator<<(QDebug debug, const LibraryInfo &info);

class AppDirInfo
{
    public:
    QString path;
    QString binaryPath;
    QStringList libraryPaths;
};

class DeploymentInfo
{
public:
    QString qtPath;
    QString pluginPath;
    QStringList deployedLibraries;
    quint64 usedModulesMask;
    QSet<QString> rpathsUsed;
    bool useLoaderPath;
    bool isLibrary;
    bool requiresQtWidgetsLibrary;
};

inline QDebug operator<<(QDebug debug, const AppDirInfo &info);

void changeQtLibraries(const QString appPath, const QString &qtPath);
void changeQtLibraries(const QList<LibraryInfo> libraries, const QStringList &binaryPaths, const QString &qtPath);

LddInfo findDependencyInfo(const QString &binaryPath);
LibraryInfo parseLddLibraryLine(const QString &line, const QString &appDirPath, const QSet<QString> &rpaths);
QString findAppBinary(const QString &appDirPath);
QList<LibraryInfo> getQtLibraries(const QString &path, const QString &appDirPath, const QSet<QString> &rpaths);
QList<LibraryInfo> getQtLibraries(const QStringList &lddLines, const QString &appDirPath, const QSet<QString> &rpaths);
QString copyLibrary(const LibraryInfo &library, const QString path);
DeploymentInfo deployQtLibraries(const QString &appDirPath,
                                 const QStringList &additionalExecutables,
                                 const QString &qmake);
DeploymentInfo deployQtLibraries(QList<LibraryInfo> libraries,const QString &bundlePath, const QStringList &binaryPaths, bool useLoaderPath);
void createQtConf(const QString &appDirPath);
void createQtConfForQtWebEngineProcess(const QString &appDirPath);
void deployPlugins(const QString &appDirPath, DeploymentInfo deploymentInfo);
bool deployQmlImports(const QString &appDirPath, DeploymentInfo deploymentInfo, QStringList &qmlDirs, QStringList &qmlImportPaths);
void changeIdentification(const QString &id, const QString &binaryPath);
void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath);
void runStrip(const QString &binaryPath);
void stripAppBinary(const QString &bundlePath);
QString findAppBinary(const QString &appDirPath);
QStringList findAppLibraries(const QString &appDirPath);
bool patchQtCore(const QString &path, const QString &variable, const QString &value);
int createAppImage(const QString &appBundlePath);
bool checkAppImagePrerequisites(const QString &appBundlePath);
void findUsedModules(DeploymentInfo &info);
void deployTranslations(const QString &appDirPath, quint64 usedQtModules);
bool deployTranslations(const QString &sourcePath, const QString &target, quint64 usedQtModules);

#endif
