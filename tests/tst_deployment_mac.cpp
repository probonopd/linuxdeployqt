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
#include <QtCore>
#include <QtTest/QTest>
#include <shared.h>

class tst_deployment_mac : public QObject
{
Q_OBJECT
private slots:
    void testParseOtoolLibraryLine();
    void testgetQtFrameworks();
    void testFindAppBinarty();
};

void tst_deployment_mac::testParseOtoolLibraryLine()
{
{
    QString line = "   /Users/foo/build/qt-4.4/lib/QtGui.framework/Versions/4/QtGui (compatibility version 4.4.0, current version 4.4.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/Users/foo/build/qt-4.4/lib/"));
    QCOMPARE(info.frameworkName, QLatin1String("QtGui.framework"));
    QCOMPARE(info.frameworkPath, QLatin1String("/Users/foo/build/qt-4.4/lib/QtGui.framework"));
    QCOMPARE(info.binaryDirectory, QLatin1String("Versions/4"));
    QCOMPARE(info.binaryName, QLatin1String("QtGui"));
    QCOMPARE(info.binaryPath, QLatin1String("/Versions/4/QtGui"));
    QCOMPARE(info.version, QLatin1String("4"));
    QCOMPARE(info.installName, QLatin1String("/Users/foo/build/qt-4.4/lib/QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/Users/foo/build/qt-4.4/lib/QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/QtGui.framework/Versions/4"));
}
{
    QString line = "	/Users/foo/build/qt-4.4/lib/phonon.framework/Versions/4/phonon (compatibility version 4.1.0, current version 4.1.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/Users/foo/build/qt-4.4/lib/"));
    QCOMPARE(info.frameworkName, QLatin1String("phonon.framework"));
    QCOMPARE(info.frameworkPath, QLatin1String("/Users/foo/build/qt-4.4/lib/phonon.framework"));
    QCOMPARE(info.binaryDirectory, QLatin1String("Versions/4"));
    QCOMPARE(info.binaryName, QLatin1String("phonon"));
    QCOMPARE(info.binaryPath, QLatin1String("/Versions/4/phonon"));
    QCOMPARE(info.version, QLatin1String("4"));
    QCOMPARE(info.installName, QLatin1String("/Users/foo/build/qt-4.4/lib/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/Users/foo/build/qt-4.4/lib/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/phonon.framework/Versions/4"));
}

{
    QString line = "	/usr/local/Qt-4.4.0/lib/phonon.framework/Versions/4/phonon (compatibility version 4.1.0, current version 4.1.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/usr/local/Qt-4.4.0/lib/"));
    QCOMPARE(info.frameworkName, QLatin1String("phonon.framework"));
    QCOMPARE(info.frameworkPath, QLatin1String("/usr/local/Qt-4.4.0/lib/phonon.framework"));
    QCOMPARE(info.binaryDirectory, QLatin1String("Versions/4"));
    QCOMPARE(info.binaryName, QLatin1String("phonon"));
    QCOMPARE(info.binaryPath, QLatin1String("/Versions/4/phonon"));
    QCOMPARE(info.version, QLatin1String("4"));
    QCOMPARE(info.installName, QLatin1String("/usr/local/Qt-4.4.0/lib/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/usr/local/Qt-4.4.0/lib/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/phonon.framework/Versions/4"));
}

{
    QString line = "	QtGui.framework/Versions/4/QtGui (compatibility version 4.1.0, current version 4.1.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/Library/Frameworks/"));
    QCOMPARE(info.frameworkName, QLatin1String("QtGui.framework"));
    QCOMPARE(info.frameworkPath, QLatin1String("/Library/Frameworks/QtGui.framework"));
    QCOMPARE(info.binaryDirectory, QLatin1String("Versions/4"));
    QCOMPARE(info.binaryName, QLatin1String("QtGui"));
    QCOMPARE(info.binaryPath, QLatin1String("/Versions/4/QtGui"));
    QCOMPARE(info.version, QLatin1String("4"));
    QCOMPARE(info.installName, QLatin1String("QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/Library/Frameworks/QtGui.framework/Versions/4/QtGui"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/QtGui.framework/Versions/4"));
}

{
    QString line = "	phonon.framework/Versions/4/QtGui (compatibility version 4.1.0, current version 4.1.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/Library/Frameworks/"));
    QCOMPARE(info.frameworkName, QLatin1String("phonon.framework"));
    QCOMPARE(info.frameworkPath, QLatin1String("/Library/Frameworks/phonon.framework"));
    QCOMPARE(info.binaryDirectory, QLatin1String("Versions/4"));
    QCOMPARE(info.binaryName, QLatin1String("phonon"));
    QCOMPARE(info.binaryPath, QLatin1String("/Versions/4/phonon"));
    QCOMPARE(info.version, QLatin1String("4"));
    QCOMPARE(info.installName, QLatin1String("phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/Library/Frameworks/phonon.framework/Versions/4/phonon"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/phonon.framework/Versions/4"));
}

{
    QString line = "	/Users/foo/build/qt-4.4/lib/libQtCLucene.4.dylib (compatibility version 4.4.0, current version 4.4.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/Users/foo/build/qt-4.4/lib/"));
    QCOMPARE(info.binaryName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(info.frameworkName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(info.frameworkPath, QLatin1String("/Users/foo/build/qt-4.4/lib/libQtCLucene.4.dylib"));
    QCOMPARE(info.installName, QLatin1String("/Users/foo/build/qt-4.4/lib/libQtCLucene.4.dylib"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/libQtCLucene.4.dylib"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/Users/foo/build/qt-4.4/lib/libQtCLucene.4.dylib"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/"));
}
{
    QString line = "libQtCLucene.4.dylib (compatibility version 4.4.0, current version 4.4.0)";
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
//    qDebug() << info;
    QCOMPARE(info.frameworkDirectory, QLatin1String("/usr/lib/"));
    QCOMPARE(info.binaryName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(info.frameworkName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(info.frameworkPath, QLatin1String("/usr/lib/libQtCLucene.4.dylib"));
    QCOMPARE(info.installName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(info.deployedInstallName, QLatin1String("@executable_path/../Frameworks/libQtCLucene.4.dylib"));
    QCOMPARE(info.sourceFilePath, QLatin1String("/usr/lib/libQtCLucene.4.dylib"));
    QCOMPARE(info.destinationDirectory, QLatin1String("Contents/Frameworks/"));
}
{
    QString line = "/foo"; //invalid
    FrameworkInfo info = parseOtoolLibraryLine(line, false);
    QCOMPARE(info.frameworkName, QString());
}

}

void tst_deployment_mac::testgetQtFrameworks()
{
{
    QStringList otool = QStringList()
    << "/Users/foo/build/qt-4.4/lib/phonon.framework/Versions/4/phonon (compatibility version 4.1.0, current version 4.1.0)"
    << "/Users/foo/build/qt-4.4/lib/QtGui.framework/Versions/4/QtGui (compatibility version 4.4.0, current version 4.4.0)"
    << "/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon (compatibility version 2.0.0, current version 136.0.0)"
    << "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit (compatibility version 45.0.0, current version 949.27.0)"
    << "/Users/foo/build/qt-4.4/lib/QtCore.framework/Versions/4/QtCore (compatibility version 4.4.0, current version 4.4.0)"
    << "/usr/lib/libz.1.dylib (compatibility version 1.0.0, current version 1.2.3)"
    << "/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 111.0.0)"
    << "/usr/lib/libstdc++.6.dylib (compatibility version 7.0.0, current version 7.4.0)"
    << "/usr/lib/libgcc_s.1.dylib (compatibility version 1.0.0, current version 1.0.0)"
    << " "
    ;

    QList<FrameworkInfo> frameworks = getQtFrameworks(otool, false);
    QCOMPARE(frameworks.count(), 3);
    QCOMPARE(frameworks.at(0).binaryName, QLatin1String("phonon"));
    QCOMPARE(frameworks.at(1).binaryName, QLatin1String("QtGui"));
    QCOMPARE(frameworks.at(2).binaryName, QLatin1String("QtCore"));
}
{
    QStringList otool = QStringList()
    << "QtHelp.framework/Versions/4/QtHelp (compatibility version 4.4.0, current version 4.4.0)"
    << "libQtCLucene.4.dylib (compatibility version 4.4.0, current version 4.4.0)"
    << "QtSql.framework/Versions/4/QtSql (compatibility version 4.4.0, current version 4.4.0)"
    << "QtXml.framework/Versions/4/QtXml (compatibility version 4.4.0, current version 4.4.0)"
    << "QtGui.framework/Versions/4/QtGui (compatibility version 4.4.0, current version 4.4.0)"
    << "/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon (compatibility version 2.0.0, current version 128.0.0)"
    << "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit (compatibility version 45.0.0, current version 824.42.0)"
    << "QtNetwork.framework/Versions/4/QtNetwork (compatibility version 4.4.0, current version 4.4.0)"
    << "QtCore.framework/Versions/4/QtCore (compatibility version 4.4.0, current version 4.4.0)"
    << "/usr/lib/libz.1.dylib (compatibility version 1.0.0, current version 1.2.3)"
    << "/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 88.3.6)"
    << "/usr/lib/libstdc++.6.dylib (compatibility version 7.0.0, current version 7.4.0)"
    << "/usr/lib/libgcc_s.1.dylib (compatibility version 1.0.0, current version 1.0.0)"
    ;

    QList<FrameworkInfo> frameworks = getQtFrameworks(otool, false);
    QCOMPARE(frameworks.count(), 7);
    QCOMPARE(frameworks.at(0).binaryName, QLatin1String("QtHelp"));
    QCOMPARE(frameworks.at(1).binaryName, QLatin1String("libQtCLucene.4.dylib"));
    QCOMPARE(frameworks.at(2).binaryName, QLatin1String("QtSql"));
    QCOMPARE(frameworks.at(3).binaryName, QLatin1String("QtXml"));
    QCOMPARE(frameworks.at(4).binaryName, QLatin1String("QtGui"));
    QCOMPARE(frameworks.at(5).binaryName, QLatin1String("QtNetwork"));
    QCOMPARE(frameworks.at(6).binaryName, QLatin1String("QtCore"));
}

}

void tst_deployment_mac::testFindAppBinarty()
{
    QCOMPARE(findAppBinary("tst_deployment_mac.app"), QLatin1String("tst_deployment_mac.app/Contents/MacOS/tst_deployment_mac"));
}

QTEST_MAIN(tst_deployment_mac)

#include "tst_deployment_mac.moc"
