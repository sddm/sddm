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

#ifndef SDDM_MESSAGES_H
#define SDDM_MESSAGES_H

#include <QFlags>

namespace SDDM {
    enum class GreeterMessages : quint32 {
        Connect = 0,
        Login = 1,
        PowerOff = 2,
        Reboot = 3,
        Suspend = 4,
        Hibernate = 5,
        HybridSleep = 6,
        QueryAuthenticationCapabilities = 9,
        SetAuthenticationContext = 10,
        StartAuthenticationMethod = 11,
        CancelAuthenticationMethod = 12
    };

    enum class DaemonMessages : quint32 {
        HostName = 0,
        Capabilities = 1,
        LoginSucceeded = 2,
        LoginFailed = 3,
        InformationMessage = 4,
        AuthenticationCapabilities = 5,
        AuthenticationContext = 8,
        AuthenticationMethodState = 9,
        AuthenticationAttemptFinished = 10
    };

    enum class AuthenticationMethod : quint32 { Password = 0, Fingerprint = 1, NoWinner = 2 };
    enum class AuthenticationAvailability : quint32 { Unknown = 0, Available = 1, Unavailable = 2 };
    enum class AuthenticationResult : quint32 { Failed = 0, Succeeded = 1, Cancelled = 2 };
    enum class AuthenticationPhase : quint32 {
        Idle = 0, Active = 1, WinnerSelected = 2, LoserStopping = 3,
        SessionAuthorized = 4, Finished = 5
    };
    enum class FingerprintState : quint32 {
        Unavailable = 0, Ready = 1, Scanning = 2, Retry = 3,
        Matched = 4, Failed = 5, Cancelled = 6, TimedOut = 7
    };

    enum Capability {
        None = 0x0000,
        PowerOff = 0x0001,
        Reboot = 0x0002,
        Suspend = 0x0004,
        Hibernate = 0x0008,
        HybridSleep = 0x0010
    };

    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Capabilities)
}

#endif // SDDM_MESSAGES_H
