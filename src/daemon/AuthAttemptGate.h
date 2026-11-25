#ifndef SDDM_AUTH_ATTEMPT_GATE_H
#define SDDM_AUTH_ATTEMPT_GATE_H

#include "Auth.h"

#include <QString>

namespace SDDM {
    // One display's authentication decision. Helper processes and PAM handles remain
    // owned by Display/Auth; this gate only decides which results may authorize.
    class AuthAttemptGate {
    public:
        void begin(const QString &requestId, const QString &user) {
            m_requestId = requestId;
            m_user = user;
            m_active = true;
            m_passwordCancelled = false;
            m_fingerprintCancelled = false;
            m_winner = -1;
            m_waitingFor = -1;
            m_authorized = false;
        }

        void invalidate() { m_active = false; }

        void cancel(AuthMethod method) {
            if (method == AuthMethod::Password)
                m_passwordCancelled = true;
            else
                m_fingerprintCancelled = true;
        }
        void resume(AuthMethod method) {
            if (method == AuthMethod::Password)
                m_passwordCancelled = false;
            else
                m_fingerprintCancelled = false;
        }

        bool select(const QString &requestId, const QString &user, AuthMethod method) {
            if (!m_active || requestId != m_requestId || user != m_user
                || cancelled(method) || m_winner != -1)
                return false;
            m_winner = int(method);
            return true;
        }

        void waitFor(AuthMethod method) { m_waitingFor = int(method); }

        // True exactly once, after the losing helper has exited (if one existed).
        bool authorize(AuthMethod method) {
            if (!m_active || m_winner != int(method) || m_waitingFor != -1 || m_authorized)
                return false;
            m_authorized = true;
            return true;
        }

        void exited(AuthMethod method) {
            if (m_waitingFor == int(method))
                m_waitingFor = -1;
        }

        bool cancelled(AuthMethod method) const {
            return method == AuthMethod::Password ? m_passwordCancelled : m_fingerprintCancelled;
        }

    private:
        QString m_requestId;
        QString m_user;
        bool m_active { false };
        bool m_passwordCancelled { false };
        bool m_fingerprintCancelled { false };
        int m_winner { -1 };
        int m_waitingFor { -1 };
        bool m_authorized { false };
    };
}

#endif // SDDM_AUTH_ATTEMPT_GATE_H
