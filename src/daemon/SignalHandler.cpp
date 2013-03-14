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

#include "SignalHandler.h"

#include <QDebug>
#include <QSocketNotifier>

#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>

namespace SDDM {
    int sighupFd[2];
    int sigintFd[2];

    SignalHandler::SignalHandler(QObject *parent) : QObject(parent) {
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd))
            qCritical() << "Failed to create socket pair for SIGHUP handling.";

        snhup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
        connect(snhup, SIGNAL(activated(int)), this, SLOT(handleSighup()));

        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd))
            qCritical() << "Failed to create socket pair for SIGINT handling.";

        snint = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
        connect(snint, SIGNAL(activated(int)), this, SLOT(handleSigint()));
    }

    void SignalHandler::initialize() {
        struct sigaction sighup;
        sighup.sa_handler = SignalHandler::hupSignalHandler;
        sigemptyset(&sighup.sa_mask);
        sighup.sa_flags = 0;
        sighup.sa_flags |= SA_RESTART;

        if (sigaction(SIGHUP, &sighup, 0) > 0) {
            qCritical() << "Failed to setup SIGHUP handler.";
            return;
        }

        struct sigaction sigint;
        sigint.sa_handler = SignalHandler::intSignalHandler;
        sigemptyset(&sigint.sa_mask);
        sigint.sa_flags |= SA_RESTART;

        if (sigaction(SIGINT, &sigint, 0) > 0) {
            qCritical() << "Failed to set up SIGINT handler.";
            return;
        }
    }

    void SignalHandler::hupSignalHandler(int) {
        char a = 1;
        ::write(sighupFd[0], &a, sizeof(a));
    }

    void SignalHandler::intSignalHandler(int) {
        char a = 1;
        ::write(sigintFd[0], &a, sizeof(a));
    }

    void SignalHandler::handleSighup() {
        // disable notifier
        snhup->setEnabled(false);

        // read from socket
        char a;
        ::read(sighupFd[1], &a, sizeof(a));

        // log event
        qWarning() << "Signal received: SIGHUP";

        // emit signal
        emit sighupReceived();

        // enable notifier
        snhup->setEnabled(true);
    }

    void SignalHandler::handleSigint() {
        // disable notifier
        snint->setEnabled(false);

        // read from socket
        char a;
        ::read(sigintFd[1], &a, sizeof(a));

        // log event
        qWarning() << "Signal received: SIGINT";

        // emit signal
        emit sigintReceived();

        // enable notifier
        snint->setEnabled(true);
    }
}
