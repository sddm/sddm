// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SDDM_XAUTHUTILS_H
#define SDDM_XAUTHUTILS_H

class QString;
class QByteArray;

namespace SDDM {
    namespace Xauth {
        QByteArray generateCookie();
        bool writeCookieToFile(const QString &filename, const QString &display, QByteArray cookie);
    }
}

#endif // SDDM_XAUTHUTILS_H
