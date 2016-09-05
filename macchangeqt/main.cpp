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
#include "../shared/shared.h"

int main(int argc, char **argv)
{
    // useDebugLibs should always be false because even if set all Qt
    // libraries inside a binary to point to debug versions, as soon as
    // one of them loads a Qt plugin, the plugin itself will load the
    // release version of Qt, and as such, the app will crash.
    bool useDebugLibs = false;

    int optionsSpecified = 0;
    for (int i = 2; i < argc; ++i) {
        QByteArray argument = QByteArray(argv[i]);
        if (argument.startsWith(QByteArray("-verbose="))) {
            LogDebug() << "Argument found:" << argument;
            optionsSpecified++;
            int index = argument.indexOf("=");
            bool ok = false;
            int number = argument.mid(index+1).toInt(&ok);
            if (!ok)
                LogError() << "Could not parse verbose level";
            else
                logLevel = number;
        }
    }

    if (argc != (3 + optionsSpecified)) {
        qDebug() << "Changeqt: changes which Qt frameworks an application links against.";
        qDebug() << "Usage: changeqt app-bundle qt-dir <-verbose=[0-3]>";
        return 0;
    }

    const QString appPath = QString::fromLocal8Bit(argv[1]);
    const QString qtPath = QString::fromLocal8Bit(argv[2]);
    changeQtFrameworks(appPath, qtPath, useDebugLibs);
}
