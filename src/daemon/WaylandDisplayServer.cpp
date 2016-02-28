/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "WaylandDisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "DisplayManager.h"
#include "Seat.h"
#include "SignalHandler.h"
#include "VirtualTerminal.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QUuid>

#include <pwd.h>
#include <unistd.h>

namespace SDDM
{
WaylandDisplayServer::WaylandDisplayServer(Display *parent)
    : DisplayServer(parent)
{
}

WaylandDisplayServer::~WaylandDisplayServer()
{
    stop();
}

const QString &WaylandDisplayServer::display() const
{
    return m_display;
}

QString WaylandDisplayServer::sessionType() const
{
    return QStringLiteral("wayland");
}

bool WaylandDisplayServer::start()
{
    // check flag
    if (m_started)
        return false;

    // log message
    qDebug() << "Compositor starting...";

    const QString exe = QStringLiteral("%1/sddm-wayland-compositor").arg(QLatin1String(BIN_INSTALL_DIR));
    const QStringList args = QStringList() << QLatin1String("--wayland-socket-name") << QLatin1String("sddm-wayland");

    if (daemonApp->testing()) {
        // create process
        m_process = new QProcess(this);

        // delete process on finish
        connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        connect(m_process, SIGNAL(readyReadStandardOutput()), SLOT(onReadyReadStandardOutput()));
        connect(m_process, SIGNAL(readyReadStandardError()), SLOT(onReadyReadStandardError()));

        qDebug() << "Running:" << qPrintable(exe) << qPrintable(args.join(QLatin1Char(' ')));
        m_process->start(exe, args);

        // don't block in waitForStarted if we fail to start
        if (m_process->state() == QProcess::NotRunning) {
            qCritical() << "Compositor failed to launch.";
            return false;
        }

        // wait for the compositor to start
        if (!m_process->waitForStarted()) {
            qCritical() << "Failed to start compositor.";
            return false;
        }

        qDebug() << "Compositor process started.";
        m_started = true;
        emit started();
    } else {
        // setup vt
        VirtualTerminal::jumpToVt(displayPtr()->terminalId());

        // authentication
        m_auth = new Auth(this);
        m_auth->setVerbose(true);
        connect(m_auth, SIGNAL(requestChanged()), this, SLOT(onRequestChanged()));
        connect(m_auth, SIGNAL(session(bool)), this, SLOT(onSessionStarted(bool)));
        connect(m_auth, SIGNAL(finished(Auth::HelperExitStatus)), this, SLOT(onHelperFinished(Auth::HelperExitStatus)));
        connect(m_auth, SIGNAL(info(QString,Auth::Info)), this, SLOT(authInfo(QString,Auth::Info)));
        connect(m_auth, SIGNAL(error(QString,Auth::Error)), this, SLOT(authError(QString,Auth::Error)));

        // environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        env.insert(QLatin1String("PATH"), mainConfig.Users.DefaultPath.get());
        env.insert(QLatin1String("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
        env.insert(QLatin1String("XDG_SEAT"), displayPtr()->seat()->name());
        env.insert(QLatin1String("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(displayPtr()->seat()->name()));
        env.insert(QLatin1String("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
        env.insert(QLatin1String("XDG_VTNR"), QString::number(displayPtr()->terminalId()));
        env.insert(QLatin1String("XDG_SESSION_CLASS"), QLatin1String("greeter"));
        env.insert(QLatin1String("XDG_SESSION_TYPE"), QLatin1String("wayland"));
        env.insert(QLatin1String("QT_QPA_PLATFORM"), QStringLiteral("greenisland"));

        m_auth->insertEnvironment(env);

        // start compositor
        m_auth->setUser(QLatin1String("sddm"));
        m_auth->setDisplayServer(true);
        m_auth->setSession(QStringLiteral("%1 %2").arg(exe).arg(args.join(QLatin1Char(' '))));
        m_auth->start();
    }

    // return success
    return true;
}

void WaylandDisplayServer::stop()
{
    // check flag
    if (!m_started)
        return;

    // log message
    qDebug() << "Stopping compositor...";

    // terminate process
    m_process->terminate();

    // wait for finished
    if (!m_process->waitForFinished(5000))
        m_process->kill();
}

void WaylandDisplayServer::finished()
{
    // check flag
    if (!m_started)
        return;

    // reset flag
    m_started = false;

    // log message
    qDebug() << "Compositor stopped.";

    // clean up
    m_process->deleteLater();
    m_process = nullptr;

    // emit signal
    emit stopped();
}

void WaylandDisplayServer::setupDisplay()
{
}

void WaylandDisplayServer::onRequestChanged() {
    m_auth->request()->setFinishAutomatically(true);
}

void WaylandDisplayServer::onSessionStarted(bool success) {
    // set flag
    m_started = success;

    // log message
    if (success)
        qDebug() << "Compositor successfully started";
    else
        qDebug() << "Compositor failed to start";

    if (m_started)
        emit started();
}

void WaylandDisplayServer::onHelperFinished(Auth::HelperExitStatus status) {
    // reset flag
    m_started = false;

    // log message
    qDebug() << "Compositor stopped.";

    // clean up
    m_auth->deleteLater();
    m_auth = nullptr;
}

void WaylandDisplayServer::onReadyReadStandardError()
{
    if (m_process) {
        qDebug() << "Compositor errors:" << qPrintable(QString::fromLocal8Bit(m_process->readAllStandardError()));
    }
}

void WaylandDisplayServer::onReadyReadStandardOutput()
{
    if (m_process) {
        qDebug() << "Compositor output:" << qPrintable(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    }
}

void WaylandDisplayServer::authInfo(const QString &message, Auth::Info info) {
    Q_UNUSED(info);
    qDebug() << "Information from compositor session:" << message;
}

void WaylandDisplayServer::authError(const QString &message, Auth::Error error) {
    Q_UNUSED(error);
    qWarning() << "Error from compositor session:" << message;
}
}
