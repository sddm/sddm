/*
 * Base backend class to be inherited further
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "Backend.h"
#include "HelperApp.h"

#include "backend/PamBackend.h"
#include "backend/PasswdBackend.h"
#include "Configuration.h"
#include "UserSession.h"

#include <QtCore/QProcessEnvironment>

#include <pwd.h>

#if defined(Q_OS_FREEBSD)
#include <sys/types.h>
#include <login_cap.h>
#endif

namespace SDDM {
    Backend::Backend(HelperApp* parent)
            : QObject(parent)
            , m_app(parent) {
    }

    Backend *Backend::get(HelperApp* parent)
    {
    #ifdef USE_PAM
        return new PamBackend(parent);
    #else
        return new PasswdBackend(parent);
    #endif
    }

    void Backend::setAutologin(bool on) {
        m_autologin = on;
    }

    void Backend::setDisplayServer(bool on)
    {
        m_displayServer = on;
    }

    void Backend::setGreeter(bool on) {
        m_greeter = on;
    }

    // Dummy, implemented and used in PamBackend
    void Backend::setRetryLoop(bool loop) {
        Q_UNUSED(loop);
    }

    bool Backend::openSession() {
        struct passwd *pw;
        pw = getpwnam(qPrintable(qobject_cast<HelperApp*>(parent())->user()));
        if (pw) {
            QProcessEnvironment env = m_app->session()->processEnvironment();
            env.insert(QStringLiteral("HOME"), QString::fromLocal8Bit(pw->pw_dir));
            env.insert(QStringLiteral("PWD"), QString::fromLocal8Bit(pw->pw_dir));
            env.insert(QStringLiteral("SHELL"), QString::fromLocal8Bit(pw->pw_shell));
            env.insert(QStringLiteral("USER"), QString::fromLocal8Bit(pw->pw_name));
            env.insert(QStringLiteral("LOGNAME"), QString::fromLocal8Bit(pw->pw_name));
            if (env.contains(QStringLiteral("DISPLAY")) && !env.contains(QStringLiteral("XAUTHORITY"))) {
                // determine Xauthority path
                QString value = QStringLiteral("%1/%2")
                        .arg(QString::fromLocal8Bit(pw->pw_dir))
                        .arg(mainConfig.X11.UserAuthFile.get());
                env.insert(QStringLiteral("XAUTHORITY"), value);
            }
#if defined(Q_OS_FREEBSD)
        /* get additional environment variables via setclassenvironment();
            this needs to be done here instead of in UserSession::setupChildProcess
            as the environment for execve() is prepared here
        */
        login_cap_t *lc;

        if (lc = login_getpwclass(pw)) {
            // save, clear and later restore SDDM's environment because
            // setclassenvironment() mangles it
            QProcessEnvironment savedEnv = QProcessEnvironment::systemEnvironment();
            QProcessEnvironment::systemEnvironment().clear();
            QString savedLang = env.value(QStringLiteral("LANG"));

            // setclassenvironment() is the implementation inside setusercontext()
            // so use lowest-level function there
            setclassenvironment(lc, pw, 1);     /* path variables */
            setclassenvironment(lc, pw, 0);     /* non-path variables */
            login_close(lc);
            if (lc = login_getuserclass(pw)) {
                setclassenvironment(lc, pw, 1);
                setclassenvironment(lc, pw, 0);
                login_close(lc);
            }

            // copy all environment variables that are now set
            env.insert(QProcessEnvironment::systemEnvironment());
            // for sddm itself, we don't want to set LANG from capabilities.
            // instead, honour sddm_lang variable from rc script
            if (qobject_cast<HelperApp*>(parent())->user() == QStringLiteral("sddm"))
                env.insert(QStringLiteral("LANG"), savedLang);
            // finally, restore original helper environment
            QProcessEnvironment::systemEnvironment().clear();
            QProcessEnvironment::systemEnvironment().insert(savedEnv);
        }
#endif
            // TODO: I'm fairly sure this shouldn't be done for PAM sessions, investigate!
            m_app->session()->setProcessEnvironment(env);
        }
        return m_app->session()->start();
    }

    bool Backend::closeSession() {
        return true;
    }
}
