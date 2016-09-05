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

#ifndef LINUX_DEPLOMYMENT_SHARED_H
#define LINUX_DEPLOMYMENT_SHARED_H

#include <QString>
#include <QStringList>
#include <QDebug>
#include <QSet>
#include <QVersionNumber>

extern int logLevel;
#define LogError()      if (logLevel < 0) {} else qDebug() << "ERROR:"
#define LogWarning()    if (logLevel < 1) {} else qDebug() << "WARNING:"
#define LogNormal()     if (logLevel < 2) {} else qDebug() << "Log:"
#define LogDebug()      if (logLevel < 3) {} else qDebug() << "Log:"

extern bool runStripEnabled;

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
    QVersionNumber currentVersion;
    QVersionNumber compatibilityVersion;
};

class LddInfo
{
public:
    QString installName;
    QString binaryPath;
    QVersionNumber currentVersion;
    QVersionNumber compatibilityVersion;
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
    QSet<QString> rpathsUsed;
    bool useLoaderPath;
    bool isLibrary;
};

inline QDebug operator<<(QDebug debug, const AppDirInfo &info);

void changeQtLibraries(const QString appPath, const QString &qtPath, bool useDebugLibs);
void changeQtLibraries(const QList<LibraryInfo> libraries, const QStringList &binaryPaths, const QString &qtPath);

LddInfo findDependencyInfo(const QString &binaryPath);
LibraryInfo parseLddLibraryLine(const QString &line, const QString &appDirPath, const QSet<QString> &rpaths, bool useDebugLibs);
QString findAppBinary(const QString &appDirPath);
QList<LibraryInfo> getQtLibraries(const QString &path, const QString &appDirPath, const QSet<QString> &rpaths, bool useDebugLibs);
QList<LibraryInfo> getQtLibraries(const QStringList &lddLines, const QString &appDirPath, const QSet<QString> &rpaths, bool useDebugLibs);
QString copyLibrary(const LibraryInfo &library, const QString path);
DeploymentInfo deployQtLibraries(const QString &appDirPath, const QStringList &additionalExecutables, bool useDebugLibs);
DeploymentInfo deployQtLibraries(QList<LibraryInfo> libraries,const QString &bundlePath, const QStringList &binaryPaths, bool useDebugLibs, bool useLoaderPath);
void createQtConf(const QString &appDirPath);
void deployPlugins(const QString &appDirPath, DeploymentInfo deploymentInfo, bool useDebugLibs);
bool deployQmlImports(const QString &appDirPath, DeploymentInfo deploymentInfo, QStringList &qmlDirs);
void changeIdentification(const QString &id, const QString &binaryPath);
void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath);
void runStrip(const QString &binaryPath);
void stripAppBinary(const QString &bundlePath);
QString findAppBinary(const QString &appDirPath);
QStringList findAppLibraries(const QString &appDirPath);
void createAppImage(const QString &appBundlePath);


#endif
