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
    int sigtermFd[2];
    int sigusr1Fd[2];

    SignalHandler::SignalHandler(QObject *parent) : QObject(parent) {
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sighupFd))
            qCritical() << "Failed to create socket pair for SIGHUP handling.";

        snhup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
        connect(snhup, &QSocketNotifier::activated, this, &SignalHandler::handleSighup);

        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigintFd))
            qCritical() << "Failed to create socket pair for SIGINT handling.";

        snint = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
        connect(snint, &QSocketNotifier::activated, this, &SignalHandler::handleSigint);

        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigtermFd))
            qCritical() << "Failed to create socket pair for SIGTERM handling.";

        snterm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
        connect(snterm, &QSocketNotifier::activated, this, &SignalHandler::handleSigterm);

        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigusr1Fd))
            qCritical() << "Failed to create socket pair for SIGUSR1 handling.";

        snusr1 = new QSocketNotifier(sigusr1Fd[1], QSocketNotifier::Read, this);
        connect(snusr1, &QSocketNotifier::activated, this, &SignalHandler::handleSigusr1);
    }

    void SignalHandler::initialize() {
        struct sigaction sighup = { };
        sighup.sa_handler = SignalHandler::hupSignalHandler;
        sigemptyset(&sighup.sa_mask);
        sighup.sa_flags = SA_RESTART;

        if (sigaction(SIGHUP, &sighup, 0) > 0) {
            qCritical() << "Failed to setup SIGHUP handler.";
            return;
        }

        struct sigaction sigint = { };
        sigint.sa_handler = SignalHandler::intSignalHandler;
        sigemptyset(&sigint.sa_mask);
        sigint.sa_flags = SA_RESTART;

        if (sigaction(SIGINT, &sigint, 0) > 0) {
            qCritical() << "Failed to set up SIGINT handler.";
            return;
        }

        struct sigaction sigterm = { };
        sigterm.sa_handler = SignalHandler::termSignalHandler;
        sigemptyset(&sigterm.sa_mask);
        sigterm.sa_flags = SA_RESTART;

        if (sigaction(SIGTERM, &sigterm, 0) > 0) {
            qCritical() << "Failed to set up SIGTERM handler.";
            return;
        }
    }

    void SignalHandler::initializeSigusr1() {
        struct sigaction sigusr1 = { };
        sigusr1.sa_handler = SignalHandler::usr1SignalHandler;
        sigemptyset(&sigusr1.sa_mask);
        sigusr1.sa_flags = SA_RESTART;

        if (sigaction(SIGUSR1, &sigusr1, 0) > 0) {
            qCritical() << "Failed to set up SIGUSR1 handler.";
            return;
        }
    }

    void SignalHandler::ignoreSigusr1() {
        struct sigaction sigusr1 = { };
        sigusr1.sa_handler = SIG_IGN;
        sigemptyset(&sigusr1.sa_mask);
        sigusr1.sa_flags = SA_RESTART;

        if (sigaction(SIGUSR1, &sigusr1, 0) > 0) {
            qCritical() << "Failed to set up SIGUSR1 handler.";
            return;
        }
    }

    void SignalHandler::hupSignalHandler(int) {
        char a = 1;
        if (::write(sighupFd[0], &a, sizeof(a)) == -1) {
            qCritical() << "Error writing to the SIGHUP handler";
            return;
        }
    }

    void SignalHandler::intSignalHandler(int) {
        char a = 1;
        if (::write(sigintFd[0], &a, sizeof(a)) == -1) {
            qCritical() << "Error writing to the SIGINT handler";
            return;
        }
    }

    void SignalHandler::termSignalHandler(int) {
        char a = 1;
        if (::write(sigtermFd[0], &a, sizeof(a)) == -1) {
            qCritical() << "Error writing to the SIGTERM handler";
            return;
        }
    }

    void SignalHandler::usr1SignalHandler(int) {
        char a = 1;
        if (::write(sigusr1Fd[0], &a, sizeof(a)) == -1) {
            qCritical() << "Error writing to the SIGUSR1 handler";
            return;
        }
    }

    void SignalHandler::handleSighup() {
        // disable notifier
        snhup->setEnabled(false);

        // read from socket
        char a;
        if (::read(sighupFd[1], &a, sizeof(a)) == -1) {
            // something went wrong!
            qCritical() << "Error reading from the socket";
            return;
        }

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
        if (::read(sigintFd[1], &a, sizeof(a)) == -1) {
            // something went wrong!
            qCritical() << "Error reading from the socket";
            return;
        }

        // log event
        qWarning() << "Signal received: SIGINT";

        // emit signal
        emit sigintReceived();

        // enable notifier
        snint->setEnabled(true);
    }

    void SignalHandler::handleSigterm() {
        // disable notifier
        snterm->setEnabled(false);

        // read from socket
        char a;
        if (::read(sigtermFd[1], &a, sizeof(a)) == -1) {
            // something went wrong!
            qCritical() << "Error reading from the socket";
            return;
        }

        // log event
        qWarning() << "Signal received: SIGTERM";

        // emit signal
        emit sigtermReceived();

        // enable notifier
        snterm->setEnabled(true);
    }

    void SignalHandler::handleSigusr1() {
        // disable notifier
        snusr1->setEnabled(false);

        // read from socket
        char a;
        if (::read(sigusr1Fd[1], &a, sizeof(a)) == -1) {
            // something went wrong!
            qCritical() << "Error reading from the socket";
            return;
        }

        // log event
        qWarning() << "Signal received: SIGUSR1";

        // emit signal
        emit sigusr1Received();

        // enable notifier
        snusr1->setEnabled(true);
    }
}
