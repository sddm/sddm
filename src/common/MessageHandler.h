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
#include <QStandardPaths>
#include <QFile>
#include <QDir>

#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_JOURNALD
#include <systemd/sd-journal.h>
#endif

namespace SDDM {
#ifdef HAVE_JOURNALD
    static void journaldLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        int priority = LOG_INFO;
        switch (type) {
            case QtDebugMsg:
                priority = LOG_DEBUG;
            break;
            case QtInfoMsg:
                priority = LOG_INFO;
            break;
            case QtWarningMsg:
                priority = LOG_WARNING;
            break;
            case QtCriticalMsg:
                priority = LOG_CRIT;
            break;
            case QtFatalMsg:
                priority = LOG_ALERT;
            break;
        }

        char fileBuffer[PATH_MAX + sizeof("CODE_FILE=")];
        snprintf(fileBuffer, sizeof(fileBuffer), "CODE_FILE=%s", context.file ? context.file : "unknown");

        char lineBuffer[32];
        snprintf(lineBuffer, sizeof(lineBuffer), "CODE_LINE=%d", context.line);

        sd_journal_print_with_location(priority, fileBuffer, lineBuffer,
                                       context.function ? context.function : "unknown",
                                       "%s", qPrintable(msg));
    }
#endif

    static void standardLogger(QtMsgType type, const QString &msg) {
        static QFile file(QStringLiteral(LOG_FILE));

        // Try to open the log file if we're not outputting to a terminal
        if (!file.isOpen() && !isatty(STDERR_FILENO)) {
            if (!file.open(QFile::Append | QFile::WriteOnly))
                file.open(QFile::Truncate | QFile::WriteOnly);

            // If we can't open the file, create it in a writable location
            // It will look spmething like ~/.local/share/$appname/sddm.log
            // or for the sddm user /var/lib/sddm/.local/share/$appname/sddm.log
            if (!file.isOpen()) {
                QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
                file.setFileName(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/sddm.log"));
                if (!file.open(QFile::Append | QFile::WriteOnly))
                    file.open(QFile::Truncate | QFile::WriteOnly);
            }
        }

        // create timestamp
        QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));

        // set log priority
        QString logPriority = QStringLiteral("(II)");
        switch (type) {
            case QtDebugMsg:
            break;
            case QtWarningMsg:
                logPriority = QStringLiteral("(WW)");
            break;
            case QtCriticalMsg:
            case QtFatalMsg:
                logPriority = QStringLiteral("(EE)");
            break;
	    default:
	    break;
        }

        // prepare log message
        QString logMessage = QStringLiteral("[%1] %2 %3\n").arg(timestamp).arg(logPriority).arg(msg);

        // log message
        if (file.isOpen()) {
            file.write(logMessage.toLocal8Bit());
            file.flush();
        } else {
            fputs(qPrintable(logMessage), stderr);
            fflush(stderr);
        }
    }

    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &prefix, const QString &msg) {
#ifdef HAVE_JOURNALD
        // don't log to journald if running interactively, this is likely
        // the case when running sddm in test mode
        static bool isInteractive = isatty(STDERR_FILENO) && qgetenv("USER") != "sddm";
        if (!isInteractive) {
            // log to journald
            journaldLogger(type, context, msg);
            return;
        }
#endif
        // prepend program name
        QString logMessage = prefix + msg;

        // log to file or stderr
        standardLogger(type, logMessage);
    }

    void DaemonMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        messageHandler(type, context, QStringLiteral("DAEMON: "), msg);
    }

    void HelperMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        messageHandler(type, context, QStringLiteral("HELPER: "), msg);
    }

    void GreeterMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        messageHandler(type, context, QStringLiteral("GREETER: "), msg);
    }
}

#endif // SDDM_MESSAGEHANDLER_H
