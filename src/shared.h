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

#ifndef LINUX_DEPLOMYMENT_SHARED_H
#define LINUX_DEPLOMYMENT_SHARED_H

#include <QDebug>

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

class Deploy
{
    public:
        QString appBinaryPath;
        QString fhsPrefix;
        bool fhsLikeMode;
        bool bundleAllButCoreLibs;
        bool runStripEnabled;
        bool alwaysOwerwriteEnabled;
        int logLevel;

        explicit Deploy();
        ~Deploy();

        void changeQtLibraries(const QString appPath,
                               const QString &qtPath);
        void changeQtLibraries(const QList<LibraryInfo> libraries,
                               const QStringList &binaryPaths,
                               const QString &qtPath);
        LddInfo findDependencyInfo(const QString &binaryPath);
        LibraryInfo parseLddLibraryLine(const QString &line,
                                        const QString &appDirPath,
                                        const QSet<QString> &rpaths);
        QList<LibraryInfo> getQtLibraries(const QString &path,
                                          const QString &appDirPath,
                                          const QSet<QString> &rpaths);
        QList<LibraryInfo> getQtLibraries(const QList<DylibInfo> &dependencies,
                                          const QString &appDirPath,
                                          const QSet<QString> &rpaths);
        DeploymentInfo deployQtLibraries(const QString &appDirPath,
                                         const QStringList &additionalExecutables);
        DeploymentInfo deployQtLibraries(QList<LibraryInfo> libraries,
                                         const QString &bundlePath,
                                         const QStringList &binaryPaths,
                                         bool useLoaderPath);
        void createQtConf(const QString &appDirPath);
        void createQtConfForQtWebEngineProcess(const QString &appDirPath);
        void deployPlugins(const QString &appDirPath,
                           DeploymentInfo deploymentInfo);
        void deployPlugins(const AppDirInfo &appDirInfo,
                           const QString &pluginSourcePath,
                           const QString pluginDestinationPath,
                           DeploymentInfo deploymentInfo);
        bool deployQmlImports(const QString &appDirPath,
                              DeploymentInfo deploymentInfo,
                              QStringList &qmlDirs);
        void changeIdentification(const QString &id, const QString &binaryPath);
        void runStrip(const QString &binaryPath);
        void stripAppBinary(const QString &bundlePath);
        QStringList findAppLibraries(const QString &appDirPath);
        bool patchQtCore(const QString &path, const QString &variable,
                         const QString &value);
        int createAppImage(const QString &appBundlePath);
        bool checkAppImagePrerequisites(const QString &appBundlePath);
        void findUsedModules(DeploymentInfo &info);
        void deployTranslations(const QString &appDirPath,
                                quint64 usedQtModules);
        bool deployTranslations(const QString &sourcePath,
                                const QString &target,
                                quint64 usedQtModules);

        QDebug LogError();
        QDebug LogWarning();
        QDebug LogNormal();
        QDebug LogDebug();

    private:
        QString m_bundleLibraryDirectory;
        QStringList m_librarySearchPath;
        QMap<QString,QString> m_qtToBeBundledInfo;
        QString m_log;
        bool m_appstoreCompliant;
        int m_qtDetected;
        bool m_deployLibrary;
        QStringList m_excludeList;

        bool lddOutputContainsLinuxVDSO(const QString &lddOutput);
        bool copyFilePrintStatus(const QString &from, const QString &to);
        int containsHowOften(QStringList haystack, QString needle);
        QSet<QString> getBinaryRPaths(const QString &path,
                                      bool resolve = true,
                                      QString executablePath = QString());
        QList<LibraryInfo> getQtLibrariesForPaths(const QStringList &paths,
                                                  const QString &appDirPath,
                                                  const QSet<QString> &rpaths);
        bool recursiveCopy(const QString &sourcePath, const QString &destinationPath);
        void recursiveCopyAndDeploy(const QString &appDirPath,
                                    const QSet<QString> &rpaths,
                                    const QString &sourcePath,
                                    const QString &destinationPath);
        QString copyDylib(const LibraryInfo &library, const QString path);
        void runPatchelf(QStringList options);
        QString captureOutput(const QString &command);
        void deployQmlImport(const QString &appDirPath,
                             const QSet<QString> &rpaths,
                             const QString &importSourcePath,
                             const QString &importName);
        QStringList translationNameFilters(quint64 modules,
                                           const QString &prefix);
        QStringList readExcludeList() const;
};

#endif
