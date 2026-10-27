// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
namespace SDDM {
class Auth;
// One daemon-side decision per greeter selection, after PAM authentication AND account checks.
class AuthenticationSelection {
public:
    void selectUser(const QString &user) { m_user = user; }
    const QString &user() const { return m_user; }
    Auth *winner() const { return m_winner; }
    void close() { m_closed = true; }
    bool accept(Auth *candidate, const QString &authenticatedUser) {
        if (m_closed || m_winner || !candidate || m_user.isEmpty() || authenticatedUser != m_user)
            return false;
        m_winner = candidate;
        return true;
    }
private:
    QString m_user;
    Auth *m_winner = nullptr;
    bool m_closed = false;
};
}
