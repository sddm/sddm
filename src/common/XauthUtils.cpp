/****************************************************************************
 * SPDX-FileCopyrightText: 2020 Fabian Vogt <fvogt@suse.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <X11/Xauth.h>

#include <random>

#include <QString>

#include "XauthUtils.h"

namespace SDDM { namespace Xauth {
    QByteArray generateCookie()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 0xFF);

        QByteArray cookie;
        cookie.reserve(16);

        for(int i = 0; i < 16; i++)
            cookie[i] = dis(gen);

        return cookie;
    }

    bool writeCookieToFile(const QString &filename, const QString &display, QByteArray cookie)
    {
        if(display.size() < 2 || display[0] != QLatin1Char(':') || cookie.count() != 16)
            return false;

        // The file needs 0600 permissions
        int oldumask = umask(077);

        // Truncate the file. We don't support merging like the xauth tool does.
        FILE * const authFp = fopen(qPrintable(filename), "wb");
        umask(oldumask);
        if (authFp == nullptr)
            return false;

        char localhost[HOST_NAME_MAX + 1] = "";
        if (gethostname(localhost, HOST_NAME_MAX) < 0)
            strcpy(localhost, "localhost");

        ::Xauth auth = {};
        char cookieName[] = "MIT-MAGIC-COOKIE-1";

        // Skip the ':'
        QByteArray displayNumberUtf8 = display.midRef(1).toUtf8();

        auth.family = FamilyLocal;
        auth.address = localhost;
        auth.address_length = strlen(auth.address);
        auth.number = displayNumberUtf8.data();
        auth.number_length = displayNumberUtf8.size();
        auth.name = cookieName;
        auth.name_length = sizeof(cookieName) - 1;
        auth.data = cookie.data();
        auth.data_length = cookie.count();

        if (XauWriteAuth(authFp, &auth) == 0) {
            fclose(authFp);
            return false;
        }

        // Write the same entry again, just with FamilyWild
        auth.family = FamilyWild;
        auth.address_length = 0;
        if (XauWriteAuth(authFp, &auth) == 0) {
            fclose(authFp);
            return false;
        }

        bool success = fflush(authFp) != EOF;

        fclose(authFp);

        return success;
    }
}}
