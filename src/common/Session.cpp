/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>
#include <QLocale>
#include <QRegularExpression>
#include <QtGlobal>
#include <QtCore/QtGlobal>
#include <QtCore/QStringView>

#include "Configuration.h"
#include "Session.h"

const QString s_entryExtention = QStringLiteral(".desktop");

namespace SDDM {
    Session::Session()
        : m_valid(false)
        , m_type(UnknownSession)
        , m_isHidden(false)
        , m_isNoDisplay(false)
    {
    }

    Session::Session(Type type, const QString &fileName)
        : Session()
    {
        setTo(type, fileName);
    }

    bool Session::isValid() const
    {
        return m_valid;
    }

    Session::Type Session::type() const
    {
        return m_type;
    }

    int Session::vt() const
    {
        return m_vt;
    }

    void Session::setVt(int vt)
    {
        m_vt = vt;
    }

    QString Session::xdgSessionType() const
    {
        return m_xdgSessionType;
    }

    QDir Session::directory() const
    {
        return m_dir;
    }

    QString Session::fileName() const
    {
        return m_fileName;
    }

    QString Session::displayName() const
    {
        return m_displayName;
    }

    QString Session::comment() const
    {
        return m_comment;
    }

    QString Session::exec() const
    {
        return m_exec;
    }

    QString Session::tryExec() const
    {
        return m_tryExec;
    }

    QString Session::desktopSession() const
    {
        return QFileInfo(m_fileName).completeBaseName();
    }

    QString Session::desktopNames() const
    {
        return m_desktopNames;
    }

    bool Session::isHidden() const
    {
        return m_isHidden;
    }

    bool Session::isNoDisplay() const
    {
        return m_isNoDisplay;
    }

    QProcessEnvironment Session::additionalEnv() const {
        return m_additionalEnv;
    }

    void Session::setTo(Type type, const QString &_fileName)
    {
        QString fileName(_fileName);
        if (!fileName.endsWith(s_entryExtention))
            fileName += s_entryExtention;

        QFileInfo info(fileName);

        m_type = UnknownSession;
        m_valid = false;
        m_desktopNames.clear();

        QStringList sessionDirs;

        switch (type) {
        case WaylandSession:
            sessionDirs = mainConfig.Wayland.SessionDir.get();
            m_xdgSessionType = QStringLiteral("wayland");
            break;
        case X11Session:
            sessionDirs = mainConfig.X11.SessionDir.get();
            m_xdgSessionType = QStringLiteral("x11");
            break;
        default:
            m_xdgSessionType.clear();
            break;
        }

        QFile file;
        for (const auto &path: qAsConst(sessionDirs)) {
            m_dir.setPath(path);
            m_fileName = m_dir.absoluteFilePath(fileName);

            qDebug() << "Reading from" << m_fileName;

            file.setFileName(m_fileName);
            if (file.open(QIODevice::ReadOnly))
                break;
        }
        if (!file.isOpen())
            return;

        QSettings settings(m_fileName, QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        QStringList locales = { QLocale().name() };
        if (auto clean = QLocale().name().remove(QRegularExpression(QLatin1String("_.*"))); clean != locales.constFirst()) {
            locales << clean;
        }

        if (settings.status() != QSettings::NoError)
                return;

        settings.beginGroup(QLatin1String("Desktop Entry"));

        auto localizedValue = [&] (const QLatin1String &key) {
            for (QString locale : qAsConst(locales)) {
                QString localizedValue = settings.value(key + QLatin1Char('[') + locale + QLatin1Char(']'), QString()).toString();
                if (!localizedValue.isEmpty()) {
                    return localizedValue;
                }
            }
            return settings.value(key).toString();
        };

        m_displayName = localizedValue(QLatin1String("Name"));
        m_comment = localizedValue(QLatin1String("Comment"));
        m_exec = settings.value(QLatin1String("Exec"), QString()).toString();
        m_tryExec = settings.value(QLatin1String("TryExec"), QString()).toString();
        m_desktopNames = settings.value(QLatin1String("DesktopNames"), QString()).toString().replace(QLatin1Char(';'), QLatin1Char(':'));
        QString hidden = settings.value(QLatin1String("Hidden"), QString()).toString();
        m_isHidden = hidden.toLower() == QLatin1String("true");
        QString noDisplay = settings.value(QLatin1String("NoDisplay"), QString()).toString();
        m_isNoDisplay = noDisplay.toLower() == QLatin1String("true");
        QString additionalEnv = settings.value(QLatin1String("X-SDDM-Env"), QString()).toString();
        m_additionalEnv = parseEnv(additionalEnv);
        settings.endGroup();

        m_type = type;
        m_valid = true;
    }

    Session &Session::operator=(const Session &other)
    {
        setTo(other.type(), other.fileName());
        return *this;
    }

    QProcessEnvironment SDDM::Session::parseEnv(const QString &list)
    {
        QProcessEnvironment env;
        const auto entryList = QStringView{list}.split(u',');
        for (const auto &entry: entryList) {
            int midPoint = entry.indexOf(QLatin1Char('='));
            if (midPoint < 0) {
                qWarning() << "Malformed entry in" << fileName() << ":" << entry;
                continue;
            }
            env.insert(entry.left(midPoint).toString(), entry.mid(midPoint+1).toString());
        }
        return env;
    }


}
