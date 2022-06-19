/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <limits.h>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QUuid>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xauth.h>

#include "Configuration.h"
#include "Constants.h"
#include "XAuth.h"

namespace SDDM {

XAuth::XAuth()
{
    m_authDir = QStringLiteral(RUNTIME_DIR);
}

QString XAuth::authDirectory() const
{
    return m_authDir;
}

void XAuth::setAuthDirectory(const QString &path)
{
    if (m_setup) {
        qWarning("Unable to set xauth directory after setup");
        return;
    }

    m_authDir = path;
}

QString XAuth::authPath() const
{
    return m_authPath;
}

QByteArray XAuth::cookie() const
{
    return m_cookie;
}

void XAuth::setup()
{
    if (m_setup)
        return;

    m_setup = true;

    // Create directory if not existing
    QDir().mkpath(m_authDir);

    // Set path
    m_authPath = QStringLiteral("%1/%2").arg(m_authDir).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    qDebug() << "Xauthority path:" << m_authPath;

    // Generate cookie
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFF);

    QByteArray m_cookie;
    m_cookie.reserve(16);

    // Create a random hexadecimal number
    for(int i = 0; i < 16; i++)
        m_cookie[i] = dis(gen);
}

bool XAuth::addCookie(const QString &display)
{
    if (!m_setup) {
        qWarning("Please setup xauth before adding a cookie");
        return false;
    }

    return XAuth::addCookieToFile(display, m_authPath, m_cookie);
}

bool XAuth::addCookieToFile(const QString &display, const QString &fileName,
                            const QByteArray &m_cookie)
{

    qDebug() << "Adding cookie to" << fileName;

    if(display.size() < 2 || display[0] != QLatin1Char(':') || m_cookie.size() != 16)
        return false;

    // The file needs 0600 permissions
    int oldumask = umask(077);

    // Truncate the file. We don't support merging like the xauth tool does.
    FILE * const authFp = fopen(qPrintable(fileName), "wb");
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
    auth.data = strdup(m_cookie.data());
    auth.data_length = m_cookie.size();

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

} // namespace SDDM
