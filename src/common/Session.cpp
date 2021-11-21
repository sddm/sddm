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

        switch (type) {
        case WaylandSession:
            m_dir = QDir(mainConfig.Wayland.SessionDir.get());
            m_xdgSessionType = QStringLiteral("wayland");
            break;
        case X11Session:
            m_dir = QDir(mainConfig.X11.SessionDir.get());
            m_xdgSessionType = QStringLiteral("x11");
            break;
        default:
            m_xdgSessionType.clear();
            break;
        }

        m_fileName = m_dir.absoluteFilePath(fileName);

        qDebug() << "Reading from" << m_fileName;

        QSettings Settings(m_fileName, QSettings::IniFormat);
        QStringList Locales;
	Locales << QLocale().name() << QLocale().name().remove(QRegExp(QLatin1String("_.*")));

        if (Settings.status() != QSettings::NoError)
        	return;
 
        Settings.beginGroup(QLatin1String("Desktop Entry"));

        QString Name = Settings.value(QLatin1String("Name"), QLatin1String("")).toString();
        QString LocalizedName;

        for (QString locale : qAsConst(Locales)) {
                LocalizedName = Settings.value(QLatin1String("Name") + QLatin1String("[") + locale + QLatin1String("]"), Name).toString();

                if (LocalizedName.compare(Name) == 0)
                        continue;
                else
                        break;
        }

        if (type == WaylandSession)
                if ((Name.endsWith(QLatin1String(" (Wayland)"))) ||
                        (Name.compare(QLatin1String("Last used session")) == 0))
                        m_displayName = QObject::tr("%1").arg(LocalizedName);
                else
                        m_displayName = QObject::tr("%1 (Wayland)").arg(LocalizedName);
        else
                m_displayName = LocalizedName;

        QString Comment = Settings.value(QLatin1String("Comment"), QLatin1String("")).toString();
        QString LocalizedComment;

        for (QString locale : qAsConst(Locales)) {
                LocalizedComment = Settings.value(QLatin1String("Comment") + QLatin1String("[") + locale + QLatin1String("]"), Comment).toString();
 
                if (LocalizedComment.compare(Comment) == 0)
                        continue;
                 else
                        break;
        }
 
        m_comment = QObject::tr("%1").arg(LocalizedComment);
        m_exec = Settings.value(QLatin1String("Exec"), QLatin1String("")).toString();
        m_tryExec = Settings.value(QLatin1String("TryExec"), QLatin1String("")).toString();
        m_desktopNames = Settings.value(QLatin1String("DesktopNames"), QLatin1String("")).toString().replace(QLatin1Char(';'), QLatin1Char(':'));
        QString Hidden = Settings.value(QLatin1String("Hidden"), QLatin1String("")).toString();
        m_isHidden = Hidden.toLower() == QLatin1String("true");
        QString NoDisplay = Settings.value(QLatin1String("NoDisplay"), QLatin1String("")).toString();
        m_isNoDisplay = NoDisplay.toLower() == QLatin1String("true");
        QString AdditionalEnv = Settings.value(QLatin1String("X-SDDM-Env="), QLatin1String(""));
        m_additionalEnv = parseEnv(AdditionalEnv);
        Settings.endGroup();

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

        const QVector<QStringRef> entryList = list.splitRef(QLatin1Char(','));
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
