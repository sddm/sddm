#include "PowerModel.h"
#include <QDebug>
#include <QDBusInterface>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace SDDM {
    class PowerDevice : public QObject {
        Q_OBJECT

    public:
        enum Type {
            Unknown = 0,
            LinePower = 1,
            Battery = 2
        };

    private:
        Type m_type { Type::Unknown };
        bool m_isPowerSupply { false };
        bool m_isOnline { false };
        double m_energy { 0.0 };
        double m_energyEmpty { 0.0 };
        double m_energyFull { 0.0 };
        double m_energyFullDesign { 0.0 };
        double m_energyRate { 0.0 };
        bool m_isPresent { false };

        template<typename T> bool updateProperty(const QVariantMap& changed, const QString& propertyName, T * value) {
            QVariant v = changed[propertyName];
            if (v.canConvert<T>()) {
                *value = v.value<T>();
                return true;
            }

            return false;
        }

    public:

        explicit PowerDevice(const QDBusObjectPath& path, QObject* parent = 0) : QObject(parent) {
            setObjectName(path.path());
            QDBusConnection::systemBus().connect("org.freedesktop.UPower", path.path(), "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

            QDBusInterface * interface = new QDBusInterface("org.freedesktop.UPower", path.path(), "org.freedesktop.DBus.Properties", QDBusConnection::systemBus(), this);
            QDBusPendingCallWatcher * watcher = new QDBusPendingCallWatcher(interface->asyncCall("GetAll", "org.freedesktop.UPower.Device"), this);
            connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(onGetAllFinished(QDBusPendingCallWatcher*)));
        }

        virtual ~PowerDevice() {}

        Type type() const { return m_type; }
        bool isPowerSupply() const { return m_isPowerSupply; }

        // only valid if type == LinePower
        bool isOnline() const { return m_isOnline; }

        // only valid if type == Battery
        double energy() const { return m_energy; }
        double energyEmpty() const { return m_energyEmpty; }
        double energyFull() const { return m_energyFull; }
        double energyFullDesign() const { return m_energyFullDesign; }
        double energyRate() const { return m_energyRate; }
        bool isPresent() const { return m_isPresent; }

    signals:
        void propertiesChanged();

    private slots:
        void onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& /*invalidated*/) {
            if (interface != "org.freedesktop.UPower.Device")
                return;

            bool emitSignal = false;

            if (updateProperty(changed, "Type", (uint*)&m_type)) emitSignal = true;
            if (updateProperty(changed, "PowerSupply", &m_isPowerSupply)) emitSignal = true;
            if (updateProperty(changed, "Online", &m_isOnline)) emitSignal = true;
            if (updateProperty(changed, "IsPresent", &m_isPresent)) emitSignal = true;
            if (updateProperty(changed, "Energy", &m_energy)) emitSignal = true;
            if (updateProperty(changed, "EnergyEmpty", &m_energyEmpty)) emitSignal = true;
            if (updateProperty(changed, "EnergyFull", &m_energyFull)) emitSignal = true;
            if (updateProperty(changed, "EnergyFullDesign", &m_energyFullDesign)) emitSignal = true;
            if (updateProperty(changed, "EnergyRate", &m_energyRate)) emitSignal = true;

            if (emitSignal)
                emit propertiesChanged();
        }

        void onGetAllFinished(QDBusPendingCallWatcher* call) {
            QDBusPendingReply<QVariantMap> reply = *call;
            if (reply.isError()) {
                qDebug() << "Cannot enumerate properties:" << reply.error().message();
            } else {
                QVariantMap props = reply.value();
                onPropertiesChanged("org.freedesktop.UPower.Device", reply.value(), QStringList());
            }
        }
    };


    PowerModel::PowerModel(QObject* parent): QObject(parent), m_batteryLevel(0), m_batteryPresent(false), m_linePowerPresent(true) {
        QDBusConnection::systemBus().connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", this, SLOT(onNameOwnerChanged(QString,QString,QString)));

        if (QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.UPower"))
            start();
    }

    PowerModel::~PowerModel() {
    }

    void PowerModel::start() {
        m_interface = new QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus(), this);
        connect(m_interface, SIGNAL(DeviceAdded(QDBusObjectPath)), this, SLOT(onDeviceAdded(QDBusObjectPath)));
        connect(m_interface, SIGNAL(DeviceRemoved(QDBusObjectPath)), this, SLOT(onDeviceRemoved(QDBusObjectPath)));
        QDBusConnection::systemBus().connect("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

        QDBusPendingCallWatcher * watcher = new QDBusPendingCallWatcher(m_interface->asyncCall("EnumerateDevices"), this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(onEnumeratedDevices(QDBusPendingCallWatcher*)));
    }

    void PowerModel::onEnumeratedDevices(QDBusPendingCallWatcher* call) {
        QDBusPendingReply<QList<QDBusObjectPath>> reply = *call;

        if (reply.isError()) {
            qDebug() << "Cannot enumerate devices:" << reply.error().message();
        } else {
            for(QDBusObjectPath& path: reply.value()) {
                onDeviceAdded(path);
            }
        }

        call->deleteLater();
    }

    void PowerModel::onNameOwnerChanged(const QString& name, const QString& /*oldOwner*/, const QString& newOwner) {
        if (name == "org.freedesktop.UPower") {
            if (m_interface) {
                delete m_interface;
                m_interface = nullptr;
            }

            if (newOwner != "") {
                start();
            }
        }
    }

    void PowerModel::onDeviceAdded(const QDBusObjectPath& path) {
        PowerDevice * device = new PowerDevice(path, this);
        connect(device, SIGNAL(propertiesChanged()), this, SLOT(onDeviceChanged()));
        m_devices.insert(path.path(), device);
    }

    void PowerModel::onDeviceRemoved(const QDBusObjectPath& path) {
        PowerDevice * device = m_devices[path.path()];
        m_devices.remove(path.path());
        device->deleteLater();
    }

    void PowerModel::onDeviceChanged() {
        double energy = 0;
        double energyFull = 0;
        bool batteryPresent = false;
        bool linePowerPresent = false;

        for(const PowerDevice* device: m_devices) {
            if (!device->isPowerSupply()) {
                continue;
            }

            switch(device->type()) {
                case PowerDevice::Type::LinePower:
                    if (device->isOnline()) {
                        linePowerPresent = true;
                    }
                    break;

                case PowerDevice::Type::Battery:
                    if (device->isPresent()) {
                        batteryPresent = true;
                        energy += device->energy() - device->energyEmpty();
                        energyFull += device->energyFull() - device->energyEmpty();
                    }
                    break;

                default:
                    break;
            }
        }

        if (batteryPresent && energyFull > 0) {
            m_batteryLevel = 100.0 * energy / energyFull;
            emit batteryLevelChanged(m_batteryLevel);
        }

        if (!batteryPresent && !linePowerPresent) {
            linePowerPresent = true;
        }

        if (batteryPresent != m_batteryPresent) {
            m_batteryPresent = batteryPresent;
            emit batteryPresentChanged(m_batteryPresent);
        }

        if (linePowerPresent != m_linePowerPresent) {
            m_linePowerPresent = linePowerPresent;
            emit linePowerPresentChanged(m_linePowerPresent);
        }
    }
}

#include "PowerModel.moc"
