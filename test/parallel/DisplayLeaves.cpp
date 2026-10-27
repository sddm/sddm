// SPDX-License-Identifier: GPL-2.0-or-later

// Process/seat/application boundaries only. Display, Auth, arbitration and sockets are production code.
#include "DaemonApp.h"
#include "Display.h"
#include "Greeter.h"
#include "Seat.h"
#include "PowerManager.h"
namespace SDDM {
    DaemonApp *DaemonApp::self = nullptr;
    DaemonApp::DaemonApp(int &argc, char **argv) : QCoreApplication(argc, argv) {
        self = this;
        m_testing = true;
        m_powerManager = new PowerManager(this);
    }
    bool DaemonApp::testing() const {
        return true;
    }
    QString DaemonApp::hostName() const {
        return QStringLiteral("fixture");
    }
    DisplayManager *DaemonApp::displayManager() const {
        return m_displayManager;
    }
    PowerManager *DaemonApp::powerManager() const {
        return m_powerManager;
    }
    SeatManager *DaemonApp::seatManager() const {
        return m_seatManager;
    }
    SignalHandler *DaemonApp::signalHandler() const {
        return m_signalHandler;
    }
    int DaemonApp::newSessionId() {
        return m_lastSessionId++;
    }
    Seat::Seat(const QString &name, QObject *parent) : QObject(parent), m_name(name) {}
    const QString &Seat::name() const {
        return m_name;
    }
    bool Seat::canTTY() {
        return false;
    }
    void Seat::createDisplay(Display::DisplayServerType) {
        qFatal("fixture must not create a real display");
    }
    void Seat::removeDisplay(Display *) {}
    void Seat::displayStopped() {}
    void Seat::startDisplay(Display *, int) {}
    Greeter::Greeter(Display *parent) : QObject(parent), m_display(parent) {}
    Greeter::~Greeter() {}
    void Greeter::setSocket(const QString &s) {
        m_socket = s;
    }
    void Greeter::setTheme(const QString &s) {
        m_themePath = s;
    }
    QString Greeter::displayServerCommand() const {
        return m_displayServerCmd;
    }
    void Greeter::setDisplayServerCommand(const QString &s) {
        m_displayServerCmd = s;
    }
    bool Greeter::isRunning() const {
        return m_started;
    }
    bool Greeter::start() {
        m_started = true;
        return true;
    }
    void Greeter::stop() {
        m_started = false;
    }
    void Greeter::finished() {
        m_started = false;
    }
    void Greeter::onRequestChanged() {}
    void Greeter::onSessionStarted(bool) {}
    void Greeter::onDisplayServerReady(const QString &) {}
    void Greeter::onHelperFinished(Auth::HelperExitStatus) {}
    void Greeter::onReadyReadStandardOutput() {}
    void Greeter::onReadyReadStandardError() {}
    void Greeter::authInfo(const QString &, Auth::Info) {}
    void Greeter::authError(const QString &, Auth::Error) {}
} // namespace SDDM
