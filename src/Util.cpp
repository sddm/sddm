/***************************************************************************
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

#include "Util.h"

#include <QDebug>

#include <signal.h>
#include <unistd.h>
#include <wait.h>

namespace SDE {
    char **toArray(const QStringList &list) {
        char **array = new char*[list.size() + 1];
        // copy elements
        for (int i = 0; i < list.size(); ++i)
            array[i] = strdup(list.at(i).toStdString().c_str());
        // end with null
        array[list.size()] = nullptr;
        // return array
        return array;
    }

    pid_t Util::execute(std::function<void ()> function) {
        pid_t pid = fork();

        // child process
        if (pid == 0)
            function();

        // main process
        return pid;
    }

    pid_t Util::execute(const QString &program, const QStringList &arguments, std::function<void ()> function) {
        pid_t pid = fork();

        // child process
        if (pid == 0) {
            char *prog = strdup(program.toStdString().c_str());
            char **args = toArray(QStringList() << program << arguments);
            // call function
            function();
            // execute program
            execv(prog, args);
            // print error
            qCritical() << "error: can not execute program:" << program;
            // exit with error
            _exit(1);
        }

        // main process
        return pid;
    }

    int Util::wait(pid_t pid) {
        // wait process to exit
        int status;
        waitpid(pid, &status, 0);
        // return status code
        return status;
    }

    int Util::terminate(pid_t pid) {
        // kill process
        killpg(pid, SIGTERM);
        // wait process to exit
        return Util::wait(pid);
    }
}
