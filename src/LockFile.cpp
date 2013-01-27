/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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

#include "LockFile.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>

#include <fstream>

#include <signal.h>

namespace SDE {
    class LockFilePrivate {
    public:
        QString path { "" };
        bool testing { false };

        bool success { true };
        QString errorMessage { "" };
    };

    LockFile::LockFile(const QString &path, bool testing) : d(new LockFilePrivate) {
        d->path = path;
        d->testing = testing;
        // if testing, return
        if (d->testing)
            return;
        // check if lock file exists
        if (QFile::exists(path)) {
            // if exists, read pid from it
            std::ifstream test(path.toStdString());
            pid_t pid = 0;
            test >> pid;
            test.close();
            // check if another process with the pid is running
            if (pid > 0) {
                int ret = kill(pid, 0);
                if (ret == 0 || errno == EPERM) {
                    qCritical() << "error: another instance of the program is running with PID" << pid << ".";
                    d->success = false;
                    return;
                }
            }
            // remove lock file
            QFile::remove(path);
        }
        // create lock
        std::ofstream lock(path.toStdString(), std::ios_base::out);
        // if can not create lock, return
        if (!lock) {
            qCritical() << "error: could not create lock file.";
            d->success = false;
            return;
        }
        // write pid into the file
        lock << qApp->applicationPid() << std::endl;
        // close file
        lock.close();
    }

    LockFile::~LockFile() {
        if (!d->testing)
            QFile::remove(d->path);
        // clean up
        delete d;
    }

    bool LockFile::success() const {
        return d->success;
    }
}
