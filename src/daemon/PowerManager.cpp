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

#include <QFile>
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

    /************************************************/
    /* PM-UTILS Backend (as failsafe only)          */
    /************************************************/

    class PMUtilsBackend : public PowerManagerBackend {
    public:
    PMUtilsBackend() {
        
    }

    ~PMUtilsBackend() {

    }

    Capabilities capabilities() const {
        Capabilities caps = Capability::PowerOff | Capability::Reboot;

        if(QProcess::execute("pm-is-supported --suspend") == 0)
            caps |= Capability::Suspend;

        if(QProcess::execute("pm-is-supported --hibernate") == 0)
            caps |= Capability::Hibernate;

        if(QProcess::execute("pm-is-supported --suspend-hybrid") == 0 )
            caps |= Capability::HybridSleep;


        return caps;
    }

            void powerOff() const {
            QProcess::execute(daemonApp->configuration()->haltCommand());
        }

        void reboot() const {
            QProcess::execute(daemonApp->configuration()->rebootCommand());
        }
        
        void suspend() const {
            QProcess::execute("pm-suspend");
        }
        
        void hibernate() const {
            QProcess::execute("pm-hibernate");
        }
        
        void hybridSleep() const {
            QProcess::execute("pm-suspend-hybrid");
        }
    };


    /**********************************************/
    /* UPOWER BACKEND                             */
    /**********************************************/

#define UPOWER_SERVICE  QLatin1String("org.freedesktop.UPower")
#define UPOWER_PATH     QLatin1String("/org/freedesktop/UPower")
#define UPOWER_OBJECT   QLatin1String("org.freedesktop.UPower")

    class UPowerBackend : public PowerManagerBackend {
    public:
        UPowerBackend() {
            m_interface = new QDBusInterface(UPOWER_SERVICE, UPOWER_PATH, UPOWER_OBJECT, QDBusConnection::systemBus());
        }

        ~UPowerBackend() {
            delete m_interface;
        }

        Capabilities capabilities() const {
            Capabilities caps = Capability::PowerOff | Capability::Reboot;

            QDBusReply<bool> reply;

            // suspend
            reply = m_interface->call("SuspendAllowed");
            if (reply.isValid() && reply.value())
                caps |= Capability::Suspend;

            // hibernate
            reply = m_interface->call("HibernateAllowed");
            if (reply.isValid() && reply.value())
                caps |= Capability::Hibernate;

            // return capabilities
            return caps;
        }

        void powerOff() const {
            QProcess::execute(daemonApp->configuration()->haltCommand());
        }

        void reboot() const {
            QProcess::execute(daemonApp->configuration()->rebootCommand());
        }

        void suspend() const {
            m_interface->call("Suspend");
        }

        void hibernate() const {
            m_interface->call("Hibernate");
        }

        void hybridSleep() const {
        }

    private:
        QDBusInterface *m_interface { nullptr };
    };

    /**********************************************/
    /* LOGIN1 BACKEND                             */
    /**********************************************/

#define LOGIN1_SERVICE  QLatin1String("org.freedesktop.login1")
#define LOGIN1_PATH     QLatin1String("/org/freedesktop/login1")
#define LOGIN1_OBJECT   QLatin1String("org.freedesktop.login1.Manager")

    class Login1Backend : public PowerManagerBackend {
    public:
        Login1Backend() {
            m_interface = new QDBusInterface(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_OBJECT, QDBusConnection::systemBus());
        }

        ~Login1Backend() {
            delete m_interface;
        }

        Capabilities capabilities() const {
            Capabilities caps = Capability::None;

            QDBusReply<QString> reply;

            // power off
            reply = m_interface->call("CanPowerOff");
            if (reply.isValid() && (reply.value() == "yes"))
                caps |= Capability::PowerOff;

            // reboot
            reply = m_interface->call("CanReboot");
            if (reply.isValid() && (reply.value() == "yes"))
                caps |= Capability::Reboot;

            // suspend
            reply = m_interface->call("CanSuspend");
            if (reply.isValid() && (reply.value() == "yes"))
                caps |= Capability::Suspend;

            // hibernate
            reply = m_interface->call("CanHibernate");
            if (reply.isValid() && (reply.value() == "yes"))
                caps |= Capability::Hibernate;

            // hybrid sleep
            reply = m_interface->call("CanHybridSleep");
            if (reply.isValid() && (reply.value() == "yes"))
                caps |= Capability::HybridSleep;

            // return capabilities
            return caps;
        }

        void powerOff() const {
            m_interface->call("PowerOff", true);
        }

        void reboot() const {
            if (!daemonApp->configuration()->testing)
                m_interface->call("Reboot", true);
        }

        void suspend() const {
            m_interface->call("Suspend", true);
        }

        void hibernate() const {
            m_interface->call("Hibernate", true);
        }

        void hybridSleep() const {
            m_interface->call("HybridSleep", true);
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
            m_backends << new Login1Backend();

        // check if upower interface exists
        if (interface->isServiceRegistered(UPOWER_SERVICE))
            m_backends << new UPowerBackend();

        // check if pm-utils is installed
        QFile PMInstalled("/usr/bin/pm-is-supported");

        //Only add pm-utils if no other option exists
        if(PMInstalled.exists() && m_backends.empty())
        {
            m_backends << new PMUtilsBackend();
        }
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
        if (daemonApp->configuration()->testing)
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::PowerOff) {
                backend->powerOff();
                break;
            }
        }
    }

    void PowerManager::reboot() const {
        if (daemonApp->configuration()->testing)
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Reboot) {
                backend->reboot();
                break;
            }
        }
    }

    void PowerManager::suspend() const {
        if (daemonApp->configuration()->testing)
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Suspend) {
                backend->suspend();
                break;
            }
        }
    }

    void PowerManager::hibernate() const {
        if (daemonApp->configuration()->testing)
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::Hibernate) {
                backend->hibernate();
                break;
            }
        }
    }

    void PowerManager::hybridSleep() const {
        if (daemonApp->configuration()->testing)
            return;

        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::HybridSleep) {
                backend->hybridSleep();
                break;
            }
        }
    }
}
