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

#include "PowerManager.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Messages.h"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QProcess>

namespace SDDM {
    /************************************************/
    /* POWER MANAGER BACKEND                        */
    /************************************************/
    class PowerManagerBackend {
    public:
        PowerManagerBackend() {
        }

        virtual ~PowerManagerBackend() {
        }

        virtual Capabilities capabilities() const = 0;

        virtual void powerOff() const = 0;
        virtual void reboot() const = 0;
        virtual void suspend() const = 0;
        virtual void hibernate() const = 0;
        virtual void hybridSleep() const = 0;
    };

    /**********************************************/
    /* UPOWER BACKEND                             */
    /**********************************************/

const QString UPOWER_PATH = QStringLiteral("/org/freedesktop/UPower");
const QString UPOWER_SERVICE = QStringLiteral("org.freedesktop.UPower");
const QString UPOWER_OBJECT = QStringLiteral("org.freedesktop.UPower");

    class UPowerBackend : public PowerManagerBackend {
    public:
        UPowerBackend(const QString & service, const QString & path, const QString & interface) {
            m_interface = new QDBusInterface(service, path, interface, QDBusConnection::systemBus());
        }

        ~UPowerBackend() {
            delete m_interface;
        }

        Capabilities capabilities() const {
            Capabilities caps = Capability::PowerOff | Capability::Reboot;

            QDBusReply<bool> reply;

            // suspend
            reply = m_interface->call(QStringLiteral("SuspendAllowed"));
            if (reply.isValid() && reply.value())
                caps |= Capability::Suspend;

            // hibernate
            reply = m_interface->call(QStringLiteral("HibernateAllowed"));
            if (reply.isValid() && reply.value())
                caps |= Capability::Hibernate;

            // return capabilities
            return caps;
        }

        void powerOff() const {
            auto command = QProcess::splitCommand(mainConfig.HaltCommand.get());
            const QString program = command.takeFirst();
            QProcess::execute(program, command);
        }

        void reboot() const {
            auto command = QProcess::splitCommand(mainConfig.RebootCommand.get());
            const QString program = command.takeFirst();
            QProcess::execute(program, command);
        }

        void suspend() const {
            m_interface->call(QStringLiteral("Suspend"));
        }

        void hibernate() const {
            m_interface->call(QStringLiteral("Hibernate"));
        }

        void hybridSleep() const {
        }

    private:
        QDBusInterface *m_interface { nullptr };
    };

    /**********************************************/
    /* LOGIN1 && ConsoleKit2 BACKEND              */
    /**********************************************/

const QString LOGIN1_SERVICE = QStringLiteral("org.freedesktop.login1");
const QString LOGIN1_PATH = QStringLiteral("/org/freedesktop/login1");
const QString LOGIN1_OBJECT = QStringLiteral("org.freedesktop.login1.Manager");

const QString CK2_SERVICE = QStringLiteral("org.freedesktop.ConsoleKit");
const QString CK2_PATH = QStringLiteral("/org/freedesktop/ConsoleKit/Manager");
const QString CK2_OBJECT = QStringLiteral("org.freedesktop.ConsoleKit.Manager");

    class SeatManagerBackend : public PowerManagerBackend {
    public:
        SeatManagerBackend(const QString & service, const QString & path, const QString & interface) {
            m_interface = new QDBusInterface(service, path, interface, QDBusConnection::systemBus());
        }

        ~SeatManagerBackend() {
            delete m_interface;
        }

        Capabilities capabilities() const {
            Capabilities caps = Capability::None;

            QDBusReply<QString> reply;

            // power off
            reply = m_interface->call(QStringLiteral("CanPowerOff"));
            if (reply.isValid() && (reply.value() == QLatin1String("yes")))
                caps |= Capability::PowerOff;

            // reboot
            reply = m_interface->call(QStringLiteral("CanReboot"));
            if (reply.isValid() && (reply.value() == QLatin1String("yes")))
                caps |= Capability::Reboot;

            // suspend
            reply = m_interface->call(QStringLiteral("CanSuspend"));
            if (reply.isValid() && (reply.value() == QLatin1String("yes")))
                caps |= Capability::Suspend;

            // hibernate
            reply = m_interface->call(QStringLiteral("CanHibernate"));
            if (reply.isValid() && (reply.value() == QLatin1String("yes")))
                caps |= Capability::Hibernate;

            // hybrid sleep
            reply = m_interface->call(QStringLiteral("CanHybridSleep"));
            if (reply.isValid() && (reply.value() == QLatin1String("yes")))
                caps |= Capability::HybridSleep;

            // return capabilities
            return caps;
        }

        void powerOff() const {
            m_interface->call(QStringLiteral("PowerOff"), true);
        }

        void reboot() const {
            if (!daemonApp->testing())
                m_interface->call(QStringLiteral("Reboot"), true);
        }

        void suspend() const {
            m_interface->call(QStringLiteral("Suspend"), true);
        }

        void hibernate() const {
            m_interface->call(QStringLiteral("Hibernate"), true);
        }

        void hybridSleep() const {
            m_interface->call(QStringLiteral("HybridSleep"), true);
        }

    private:
        QDBusInterface *m_interface { nullptr };
    };

    /**********************************************/
    /* POWER MANAGER                              */
    /**********************************************/
    PowerManager::PowerManager(QObject *parent) : QObject(parent) {
        QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();

        // check if login1 interface exists
        if (interface->isServiceRegistered(LOGIN1_SERVICE))
            m_backends << new SeatManagerBackend(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_OBJECT);

        // check if ConsoleKit2 interface exists
        if (interface->isServiceRegistered(CK2_SERVICE))
            m_backends << new SeatManagerBackend(CK2_SERVICE, CK2_PATH, CK2_OBJECT);

        // check if upower interface exists
        if (interface->isServiceRegistered(UPOWER_SERVICE))
            m_backends << new UPowerBackend(UPOWER_SERVICE, UPOWER_PATH, UPOWER_OBJECT);
    }

    PowerManager::~PowerManager() {
        while (!m_backends.empty())
            delete m_backends.takeFirst();
    }

    Capabilities PowerManager::capabilities() const {
        Capabilities caps = Capability::None;

        for (PowerManagerBackend *backend: m_backends)
            caps |= backend->capabilities();

        return caps;
    }

    void PowerManager::powerOff() const {
        if (daemonApp->testing())
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::PowerOff) {
                backend->powerOff();
                break;
            }
        }
    }

    void PowerManager::reboot() const {
        if (daemonApp->testing())
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Reboot) {
                backend->reboot();
                break;
            }
        }
    }

    void PowerManager::suspend() const {
        if (daemonApp->testing())
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Suspend) {
                backend->suspend();
                break;
            }
        }
    }

    void PowerManager::hibernate() const {
        if (daemonApp->testing())
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Hibernate) {
                backend->hibernate();
                break;
            }
        }
    }

    void PowerManager::hybridSleep() const {
        if (daemonApp->testing())
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::HybridSleep) {
                backend->hybridSleep();
                break;
            }
        }
    }
}
