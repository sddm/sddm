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

#include "Constants.h"

#include <QDBusInterface>
#include <QDBusReply>

#if !LOGIN1_FOUND
#include "Configuration.h"

#include <QProcess>
#endif

namespace SDE {
    class PowerManagerPrivate {
    public:
        QDBusInterface *interface { nullptr };
    };

#if TEST || (!LOGIN1_FOUND && !UPOWER_FOUND)

    PowerManager::PowerManager(QObject *parent) : QObject(parent) {
    }

    PowerManager::~PowerManager() {
    }

    bool PowerManager::canPowerOff() const {
        return true;
    }

    bool PowerManager::canReboot() const {
        return true;
    }

    bool PowerManager::canSuspend() const {
        return true;
    }

    bool PowerManager::canHibernate() const {
        return true;
    }

    bool PowerManager::canHybridSleep() const {
        return true;
    }

    void PowerManager::powerOff() {
    }

    void PowerManager::reboot() {
    }

    void PowerManager::suspend() {
    }

    void PowerManager::hibernate() {
    }

    void PowerManager::hybridSleep() {
        d->interface->call("HybridSleep", true);
    }

#elif LOGIN1_FOUND

    PowerManager::PowerManager(QObject *parent) : QObject(parent), d(new PowerManagerPrivate()) {
        d->interface = new QDBusInterface("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
    }

    PowerManager::~PowerManager() {
        delete d;
    }

    bool PowerManager::canPowerOff() const {
        QDBusReply<QString> reply = d->interface->call("CanPowerOff");
        return reply.isValid() && (reply.value() == "yes");
    }

    bool PowerManager::canReboot() const {
        QDBusReply<QString> reply = d->interface->call("CanReboot");
        return reply.isValid() && (reply.value() == "yes");
    }

    bool PowerManager::canSuspend() const {
        QDBusReply<QString> reply = d->interface->call("CanSuspend");
        return reply.isValid() && (reply.value() == "yes");
    }

    bool PowerManager::canHibernate() const {
        QDBusReply<QString> reply = d->interface->call("CanHibernate");
        return reply.isValid() && (reply.value() == "yes");
    }


    bool PowerManager::canHybridSleep() const {
        QDBusReply<QString> reply = d->interface->call("CanHybridSleep");
        return reply.isValid() && (reply.value() == "yes");
    }

    void PowerManager::powerOff() {
        d->interface->call("PowerOff", true);
    }

    void PowerManager::reboot() {
        d->interface->call("Reboot", true);
    }

    void PowerManager::suspend() {
        d->interface->call("Suspend", true);
    }

    void PowerManager::hibernate() {
        d->interface->call("Hibernate", true);
    }

    void PowerManager::hybridSleep() {
        d->interface->call("HybridSleep", true);
    }

#elif UPOWER_FOUND

    PowerManager::PowerManager(QObject *parent) : QObject(parent), d(new PowerManagerPrivate()) {
        d->interface = new QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus());
    }

    PowerManager::~PowerManager() {
        delete d;
    }

    bool PowerManager::canPowerOff() const {
        return true;
    }

    bool PowerManager::canReboot() const {
        return true;
    }

    bool PowerManager::canSuspend() const {
        QDBusReply<bool> reply = d->interface->call("SuspendAllowed");
        return reply.isValid() && reply.value();
    }

    bool PowerManager::canHibernate() const {
        QDBusReply<bool> reply = d->interface->call("HibernateAllowed");
        return reply.isValid() && reply.value();
    }

    bool PowerManager::canHybridSleep() const {
        return false;
    }

    void PowerManager::powerOff() {
        QProcess::execute(Configuration::instance()->haltCommand());
    }

    void PowerManager::reboot() {
        QProcess::execute(Configuration::instance()->rebootCommand());
    }

    void PowerManager::suspend() {
        d->interface->call("Suspend");
    }

    void PowerManager::hibernate() {
        d->interface->call("Hibernate");
    }

    void PowerManager::hybridSleep() {
    }

#endif
}
