#ifndef PowerModel_H
#define PowerModel_H

#include <QObject>
#include <QMap>

class QDBusInterface;
class QDBusObjectPath;
class QDBusPendingCallWatcher;


namespace SDDM {
    class PowerDevice;

    class PowerModel : public QObject {
        Q_OBJECT

        Q_PROPERTY(double batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
        Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY batteryPresentChanged)
        Q_PROPERTY(bool linePowerPresent READ linePowerPresent NOTIFY linePowerPresentChanged)

        QDBusInterface * m_interface { nullptr };
        QMap<QString, PowerDevice *> m_devices;

        double m_batteryLevel;
        bool m_batteryPresent;
        bool m_linePowerPresent;

    public:
        explicit PowerModel(QObject* parent = 0);
        virtual ~PowerModel();

        double batteryLevel() const { return m_batteryLevel; }
        bool batteryPresent() const { return m_batteryPresent; }
        bool linePowerPresent() const { return m_linePowerPresent; }

    private:
        void start();

    private slots:
        void onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
        void onEnumeratedDevices(QDBusPendingCallWatcher * call);

        void onDeviceAdded(const QDBusObjectPath& path);
        void onDeviceRemoved(const QDBusObjectPath& path);
        void onDeviceChanged();

    signals:
        void batteryLevelChanged(double batteryLevel);
        void batteryPresentChanged(bool batteryPresent);
        void linePowerPresentChanged(bool linePowerPresent);
    };
}

#endif // PowerModel_H
