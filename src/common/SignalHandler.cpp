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

#include <QCoreApplication>
#include <QDebug>
#include <QSocketNotifier>

#include <signal.h>
#include <unistd.h>
#include <mutex>

#include <sys/socket.h>

namespace SDDM {
    std::once_flag signalsInitialized;

    int sigintFd[2];
    int sigtermFd[2];
    int sigcustomFd[2];

    SignalHandler::SignalHandler(QObject *parent) : QObject(parent) {
        std::call_once(signalsInitialized, &initialize);

        // If it's done before creating the QCoreApplication, it just will not work
        Q_ASSERT(QCoreApplication::instance());

        snint = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
        connect(snint, &QSocketNotifier::activated, this, &SignalHandler::handleSigint);

        snterm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
        connect(snterm, &QSocketNotifier::activated, this, &SignalHandler::handleSigterm);

        sncustom = new QSocketNotifier(sigcustomFd[1], QSocketNotifier::Read, this);
        connect(sncustom, &QSocketNotifier::activated, this, &SignalHandler::handleSigCustom);
    }

    void SignalHandler::initialize() {
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigintFd))
            qCritical() << "Failed to create socket pair for SIGINT handling.";

        struct sigaction sigint = { };
        sigint.sa_handler = SignalHandler::intSignalHandler;
        sigemptyset(&sigint.sa_mask);
        sigint.sa_flags = SA_RESTART;

        if (sigaction(SIGINT, &sigint, 0) > 0) {
            qCritical() << "Failed to set up SIGINT handler.";
            return;
        }

        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigtermFd))
            qCritical() << "Failed to create socket pair for SIGTERM handling.";

        struct sigaction sigterm = { };
        sigterm.sa_handler = SignalHandler::termSignalHandler;
        sigemptyset(&sigterm.sa_mask);
        sigterm.sa_flags = SA_RESTART;

        if (sigaction(SIGTERM, &sigterm, 0) > 0) {
            qCritical() << "Failed to set up SIGTERM handler.";
            return;
        }

        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sigcustomFd))
            qCritical() << "Failed to create socket pair for custom signals handling.";
    }

    void SignalHandler::addCustomSignal(int signal)
    {
        struct sigaction sigcustom = { };
        sigcustom.sa_handler = SignalHandler::customSignalHandler;
        sigemptyset(&sigcustom.sa_mask);
        sigcustom.sa_flags = SA_RESTART;

        if (sigaction(signal, &sigcustom, 0) > 0) {
            qCritical() << "Failed to set up " << strsignal(signal) << " handler.";
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

    void SignalHandler::customSignalHandler(int signal) {
        if (::write(sigcustomFd[0], &signal, sizeof(signal)) == -1) {
            qCritical() << "Error writing to the " << strsignal(signal) << " handler";
            return;
        }
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

    void SignalHandler::handleSigCustom() {
        // disable notifier
        sncustom->setEnabled(false);

        // read from socket
        int signal;
        if (::read(sigcustomFd[1], &signal, sizeof(signal)) == -1) {
            // something went wrong!
            qCritical() << "Error reading from the socket";
            return;
        }

        // log event
        qWarning() << "Signal received: " << strsignal(signal);

        // emit signal
        emit customSignalReceived(signal);

        // enable notifier
        sncustom->setEnabled(true);
    }


}
