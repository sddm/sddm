#ifndef SDE_POWERMANAGER_H
#define SDE_POWERMANAGER_H

#include <QObject>

namespace SDE {
    class PowerManagerPrivate;

    class PowerManager : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(PowerManager)
    public:
        PowerManager();
        ~PowerManager();

    public slots:
        bool canPowerOff() const;
        bool canReboot() const;
        bool canSuspend() const;
        bool canHibernate() const;
        bool canHybridSleep() const;

        void powerOff();
        void reboot();
        void suspend();
        void hibernate();
        void hybridSleep();

    private:
        PowerManagerPrivate *d;
    };
}

#endif // SDE_POWERMANAGER_H
