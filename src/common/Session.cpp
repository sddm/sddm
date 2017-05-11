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

#include "Configuration.h"
#include "Session.h"

const QString s_entryExtention = QStringLiteral(".desktop");

namespace SDDM {
    Session::Session()
        : m_valid(false)
        , m_type(UnknownSession)
        , m_isHidden(false)
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
        return fileName().replace(s_entryExtention, QString());
    }

    QString Session::desktopNames() const
    {
        return m_desktopNames;
    }

    bool Session::isHidden() const
    {
        return m_isHidden;
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
        case X11Session:
            m_dir = QDir(mainConfig.X11.SessionDir.get());
            m_xdgSessionType = QStringLiteral("x11");
            break;
        case WaylandSession:
            m_dir = QDir(mainConfig.Wayland.SessionDir.get());
            m_xdgSessionType = QStringLiteral("wayland");
            break;
        default:
            m_xdgSessionType.clear();
            break;
        }

        m_fileName = m_dir.absoluteFilePath(fileName);

        qDebug() << "Reading from" << m_fileName;

        QFile file(m_fileName);
        if (!file.open(QIODevice::ReadOnly))
            return;

        QString current_section;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();

            if (line.startsWith(QLatin1String("["))) {
                // The section name ends before the last ] before the start of a comment
                int end = line.lastIndexOf(QLatin1Char(']'), line.indexOf(QLatin1Char('#')));
                if (end != -1)
                    current_section = line.mid(1, end - 1);
            }

            if (current_section != QLatin1String("Desktop Entry"))
                continue; // We are only interested in the "Desktop Entry" section

            if (line.startsWith(QLatin1String("Name="))) {
                if (type == WaylandSession)
                    m_displayName = QObject::tr("%1 (Wayland)").arg(line.mid(5));
                else
                    m_displayName = line.mid(5);
            }
            if (line.startsWith(QLatin1String("Comment=")))
                m_comment = line.mid(8);
            if (line.startsWith(QLatin1String("Exec=")))
                m_exec = line.mid(5);
            if (line.startsWith(QStringLiteral("TryExec=")))
                m_tryExec = line.mid(8);
            if (line.startsWith(QLatin1String("DesktopNames=")))
                m_desktopNames = line.mid(13).replace(QLatin1Char(';'), QLatin1Char(':'));
            if (line.startsWith(QLatin1String("Hidden=")))
                m_isHidden = line.mid(7).toLower() == QLatin1String("true");
        }

        file.close();

        m_type = type;
        m_valid = true;
    }

    Session &Session::operator=(const Session &other)
    {
        setTo(other.type(), other.fileName());
        return *this;
    }
}
