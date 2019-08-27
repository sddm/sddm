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

#include <QDebug>
#include <QDir>
#include <QUuid>

#include "Configuration.h"
#include "Constants.h"
#include "XAuth.h"

#include <random>

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

QString XAuth::cookie() const
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
    std::uniform_int_distribution<> dis(0, 15);

    // Reseve 32 bytes
    m_cookie.reserve(32);

    // Create a random hexadecimal number
    const char *digits = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
        m_cookie[i] = QLatin1Char(digits[dis(gen)]);
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
                            const QString &cookie)
{
    qDebug() << "Adding cookie to" << fileName;

    // Touch file
    QFile file_handler(fileName);
    file_handler.open(QIODevice::Append);
    file_handler.close();

    QString cmd = QStringLiteral("%1 -f %2 -q").arg(mainConfig.X11.XauthPath.get()).arg(fileName);

    // Execute xauth
    FILE *fp = ::popen(qPrintable(cmd), "w");

    // Check file
    if (!fp)
        return false;
    fprintf(fp, "remove %s\n", qPrintable(display));
    fprintf(fp, "add %s . %s\n", qPrintable(display), qPrintable(cookie));
    fprintf(fp, "exit\n");

    // Close pipe
    return pclose(fp) == 0;
}

} // namespace SDDM
