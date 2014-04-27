/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#ifndef SDDM_MESSAGEHANDLER_H
#define SDDM_MESSAGEHANDLER_H

#include "Constants.h"

#include <QDateTime>
#include <QFile>

#include <iostream>

namespace SDDM {
    void MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        Q_UNUSED(context)

        static QFile file(LOG_FILE);

        // try to open file only if it's not already open
        if (!file.isOpen()) {
            if (!file.open(QFile::Append | QFile::WriteOnly))
                file.open(QFile::Truncate | QFile::WriteOnly);
        }

        // create timestamp
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

        // prepare log message
        QString logMessage = msg;
        switch (type) {
            case QtDebugMsg:
                logMessage = QString("[%1] (II) %2\n").arg(timestamp).arg(msg);
            break;
            case QtWarningMsg:
                logMessage = QString("[%1] (WW) %2\n").arg(timestamp).arg(msg);
            break;
            case QtCriticalMsg:
            case QtFatalMsg:
                logMessage = QString("[%1] (EE) %2\n").arg(timestamp).arg(msg);
            break;
        }

        // log message
        if (file.isOpen())
            file.write(logMessage.toLocal8Bit());
        else
            std::cout << qPrintable(logMessage);
    }
}

#endif // SDDM_MESSAGEHANDLER_H
