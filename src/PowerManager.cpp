#include "PowerManager.h"

#include "Configuration.h"

#include <QDBusInterface>
#include <QDBusReply>

#if !USE_SYSTEMD
#include <QProcess>
#endif

namespace SDE {
    class PowerManagerPrivate {
    public:
        PowerManagerPrivate() {
#if USE_SYSTEMD
            powerInterface = new QDBusInterface("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
#elif USE_UPOWER
            powerInterface = new QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus());
#endif
        }

        ~PowerManagerPrivate() {
            delete powerInterface;
        }

        QDBusInterface *powerInterface { nullptr };
    };

    PowerManager::PowerManager() : d(new PowerManagerPrivate()) {
    }

    PowerManager::~PowerManager() {
        delete d;
    }

    bool PowerManager::canPowerOff() const {
#if USE_SYSTEMD
        QDBusReply<QString> reply = d->powerInterface->call("CanPowerOff");
        return reply.isValid() && (reply.value() == "yes");
#else
        return true;
#endif
    }

    bool PowerManager::canReboot() const {
#if USE_SYSTEMD
        QDBusReply<QString> reply = d->powerInterface->call("CanReboot");
        return reply.isValid() && (reply.value() == "yes");
#else
        return true;
#endif
    }

    bool PowerManager::canSuspend() const {
#if USE_SYSTEMD
        QDBusReply<QString> reply = d->powerInterface->call("CanSuspend");
        return reply.isValid() && (reply.value() == "yes");
#elif USE_UPOWER
        QDBusReply<bool> reply = d->powerInterface->call("SuspendAllowed");
        return reply.isValid() && reply.value();
#else
        return false;
#endif
    }

    bool PowerManager::canHibernate() const {
#if USE_SYSTEMD
        QDBusReply<QString> reply = d->powerInterface->call("CanHibernate");
        return reply.isValid() && (reply.value() == "yes");
#elif USE_UPOWER
        QDBusReply<bool> reply = d->powerInterface->call("HibernateAllowed");
        return reply.isValid() && reply.value();
#else
        return false;
#endif
    }


    bool PowerManager::canHybridSleep() const {
#if USE_SYSTEMD
        QDBusReply<QString> reply = d->powerInterface->call("CanHybridSleep");
        return reply.isValid() && (reply.value() == "yes");
#else
        return false;
#endif
    }

    void PowerManager::powerOff() {
#if USE_SYSTEMD
        d->powerInterface->call("PowerOff", true);
#else
        QProcess::execute(Configuration::instance()->haltCommand());
#endif
    }

    void PowerManager::reboot() {
#if USE_SYSTEMD
        d->powerInterface->call("Reboot", true);
#else
        QProcess::execute(Configuration::instance()->rebootCommand());
#endif
    }

    void PowerManager::suspend() {
#if USE_SYSTEMD
        d->powerInterface->call("Suspend", true);
#elif USE_UPOWER
        d->powerInterface->call("Suspend");
#endif
    }

    void PowerManager::hibernate() {
#if USE_SYSTEMD
        d->powerInterface->call("Hibernate", true);
#elif USE_UPOWER
        d->powerInterface->call("Hibernate");
#endif
    }

    void PowerManager::hybridSleep() {
#if USE_SYSTEMD
        d->powerInterface->call("HybridSleep", true);
#endif
    }
}
