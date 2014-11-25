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
#include <QTimer>

namespace SDDM {
    /************************************************/
    /* POWER MANAGER BACKEND                        */
    /************************************************/
    class PowerManagerBackend : public QObject {
        Q_OBJECT
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
        virtual float batteryLevel() const = 0;
        virtual bool batteryPresent() const = 0;
        virtual bool linePowerPresent() const = 0;

    signals:
        void batteryStatusChanged();
    };

    /**********************************************/
    /* UPOWER BACKEND                             */
    /**********************************************/

#define UPOWER_SERVICE  QStringLiteral("org.freedesktop.UPower")
#define UPOWER_PATH     QStringLiteral("/org/freedesktop/UPower")
#define UPOWER_OBJECT   QStringLiteral("org.freedesktop.UPower")
#define UPOWER_DEVICE   QStringLiteral("org.freedesktop.UPower.Device")
#define UPOWER_PROPS    QStringLiteral("org.freedesktop.DBus.Properties")

    class UPowerBackend : public PowerManagerBackend {
        Q_OBJECT
        bool m_batteryPresent { false };
        bool m_linePowerPresent { true };
        float m_batteryLevel { 1 };
        QTimer m_timer;

    public:
        UPowerBackend() {
            m_interface = new QDBusInterface(UPOWER_SERVICE, UPOWER_PATH, UPOWER_OBJECT, QDBusConnection::systemBus());

            connect(m_interface, SIGNAL(DeviceAdded(QDBusObjectPath)), this, SLOT(deviceAdded(QDBusObjectPath)));
            connect(m_interface, SIGNAL(DeviceRemoved(QDBusObjectPath)), this, SLOT(deviceRemoved(QDBusObjectPath)));
            QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_PROPS, "PropertiesChanged", this, SLOT(upowerChanged(QString,QVariantMap,QStringList)));

            QDBusReply<QList<QDBusObjectPath>> devices = m_interface->call("EnumerateDevices");
            if (devices.isValid()) {
                for(const QDBusObjectPath& i: devices.value()) {
                    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_PROPS, "PropertiesChanged", this, SLOT(deviceChanged(QString,QVariantMap,QStringList)));
                }

                updateBatteryStatus();
            }

            // poll the battery status every minute in case no update is sent from UPower
            m_timer.setTimerType(Qt::VeryCoarseTimer);
            m_timer.start(60000);
            connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateBatteryStatus()));
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

            QDBusReply<QList<QDBusObjectPath>> devices = m_interface->call("EnumerateDevices");
            if (devices.isValid()) {
                for(const QDBusObjectPath& i: devices.value()) {
                    QDBusInterface device(UPOWER_SERVICE, i.path(), UPOWER_DEVICE, QDBusConnection::systemBus());
                    if (device.property("Type") == 2) { // type 2: battery
                        caps |= Capability::BatteryStatus;
                    }
                }
            }
            // return capabilities
            return caps;
        }

        void powerOff() const {
            QProcess::execute(mainConfig.HaltCommand.get());
        }

        void reboot() const {
            QProcess::execute(mainConfig.RebootCommand.get());
        }

        void suspend() const {
            m_interface->call("Suspend");
        }

        void hibernate() const {
            m_interface->call("Hibernate");
        }

        void hybridSleep() const {
        }

        float batteryLevel() const {
            return m_batteryLevel;
        }

        bool batteryPresent() const {
            return m_batteryPresent;
        }

        bool linePowerPresent() const {
            return m_linePowerPresent;
        }

    private slots:
        void updateBatteryStatus() {
            QDBusReply<QList<QDBusObjectPath>> devices = m_interface->call("EnumerateDevices");

            double sumEnergy = 0;
            double sumEnergyFull = 0;
            m_linePowerPresent = false;
            m_batteryPresent = false;

            if (devices.isValid()) {
                for(const QDBusObjectPath& i: devices.value()) {
                    QDBusInterface device(UPOWER_SERVICE, i.path(), UPOWER_DEVICE, QDBusConnection::systemBus());
                    QVariant powerSupply = device.property("PowerSupply");

                    if (powerSupply.canConvert<bool>() && powerSupply.toBool()) {
                        QVariant type = device.property("Type");
                        if (type == 1) { // Line power
                            QVariant online = device.property("Online");

                            if (online.canConvert<bool>() && online.toBool()) {
                                m_linePowerPresent = true;
                            }
                        } else if (type == 2) { // Battery
                            QVariant energy = device.property("Energy");
                            QVariant energyFull = device.property("EnergyFull");

                            m_batteryPresent = true;

                            if (energy.canConvert<double>() && energyFull.canConvert<double>()) {
                                sumEnergy += energy.toDouble();
                                sumEnergyFull += energyFull.toDouble();
                            }
                        }
                    }
                }
            }

            if (sumEnergyFull > 0) {
                m_batteryLevel = sumEnergy / sumEnergyFull;
                if (m_batteryLevel > 1.0)
                    m_batteryLevel = 1.0;
                else if (m_batteryLevel < 0.0)
                    m_batteryLevel = 0.0;
            } else {
                m_batteryLevel = 1.0;
            }

            if (!m_batteryPresent && !m_linePowerPresent) {
                m_linePowerPresent = true;
            }

            emit batteryStatusChanged();
        }

    private slots:
        void deviceAdded(const QDBusObjectPath& path) {
            QDBusInterface device(UPOWER_SERVICE, path.path(), UPOWER_DEVICE, QDBusConnection::systemBus());
            QVariant type = device.property("Type");

            if (type == 1 || type == 2) { // line power or battery
                QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_PROPS, "PropertiesChanged", this, SLOT(deviceChanged(QString,QVariantMap,QStringList)));
                updateBatteryStatus();
            }
        }

        void deviceRemoved(const QDBusObjectPath& path) {
            QDBusInterface device(UPOWER_SERVICE, path.path(), UPOWER_DEVICE, QDBusConnection::systemBus());
            QVariant type = device.property("Type");

            if (type == 1 || type == 2) { // line power or battery
                updateBatteryStatus();
            }
        }

        void upowerChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated) {
            updateBatteryStatus();
        }

        void deviceChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated) {
            updateBatteryStatus();
        }

    private:
        QDBusInterface *m_interface { nullptr };
    };

    /**********************************************/
    /* LOGIN1 BACKEND                             */
    /**********************************************/

#define LOGIN1_SERVICE  QStringLiteral("org.freedesktop.login1")
#define LOGIN1_PATH     QStringLiteral("/org/freedesktop/login1")
#define LOGIN1_OBJECT   QStringLiteral("org.freedesktop.login1.Manager")

    class Login1Backend : public PowerManagerBackend {
        Q_OBJECT
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
            if (!daemonApp->testing())
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

        float batteryLevel() const {
            return 1;
        }

        bool batteryPresent() const {
            return false;
        }

        bool linePowerPresent() const {
            return true;
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
            addBackend(new Login1Backend());

        // check if upower interface exists
        if (interface->isServiceRegistered(UPOWER_SERVICE))
            addBackend(new UPowerBackend());

        // check if new interfaces are created at runtime
        QDBusConnection::systemBus().connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", this, SLOT(nameOwnerChanged(QString,QString,QString)));
    }

    PowerManager::~PowerManager() {
        while (!m_backends.empty())
            delete m_backends.takeFirst();
    }

    void PowerManager::addBackend(PowerManagerBackend* backend)
    {
        m_backends << backend;
        connect(backend, SIGNAL(batteryStatusChanged()), this, SLOT(onBatteryStatusChanged()));
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

    float PowerManager::batteryLevel() const {
        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::BatteryStatus) {
              return backend->batteryLevel();
            }
        }

        return 0;
    }

    bool PowerManager::batteryPresent() const {
        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::BatteryStatus) {
              return backend->batteryPresent();
            }
        }

        return false;
    }

    bool PowerManager::linePowerPresent() const {
        for (PowerManagerBackend *backend: m_backends) {
            if (backend->capabilities() & Capability::BatteryStatus) {
              return backend->linePowerPresent();
            }
        }

        return true;
    }

    void PowerManager::onBatteryStatusChanged() {
        emit batteryStatusChanged();
    }

    void PowerManager::nameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner) {
        bool changed = false;

        if (name == UPOWER_SERVICE) {
            for(int i = 0, count = m_backends.count(); i < count; i++) {
                if (m_backends[i]->metaObject()->className() == QStringLiteral("SDDM::UPowerBackend")) {
                    m_backends.removeAt(i);
                    changed = true;
                    qDebug() << "Removed power backend #" << i;
                }
            }

            if (newOwner != "") {
                addBackend(new UPowerBackend());
                changed = true;
                qDebug() << "Added UPower backend";
            }
        }

        if (name == LOGIN1_SERVICE) {
            for(int i = 0, count = m_backends.count(); i < count; i++) {
                if (m_backends[i]->metaObject()->className() == QStringLiteral("SDDM::Login1Backend")) {
                    m_backends.removeAt(i);
                    changed = true;
                    qDebug() << "Removed power backend #" << i;
                }
            }

            if (newOwner != "") {
                addBackend(new Login1Backend());
                changed = true;
                qDebug() << "Added Login1 backend";
            }
        }

        if (changed) {
            emit batteryStatusChanged();
        }
    }
}

#include "PowerManager.moc"
