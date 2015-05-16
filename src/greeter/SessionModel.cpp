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

#include "SessionModel.h"

#include "Configuration.h"

#include <QList>
#include <QProcessEnvironment>

namespace SDDM {
    class SessionModelPrivate {
    public:
        ~SessionModelPrivate() {
            while (!sessions.isEmpty())
                delete sessions.takeFirst();
        }

        int lastIndex { 0 };
        QList<Session *> sessions;
    };

    SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent), d(new SessionModelPrivate()) {
        populate(Session::X11Session, mainConfig.XDisplay.SessionDir.get());
        populate(Session::WaylandSession, mainConfig.WaylandDisplay.SessionDir.get());
    }

    SessionModel::~SessionModel() {
        delete d;
    }

    QHash<int, QByteArray> SessionModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[DirectoryRole] = "directory";
        roleNames[FileRole] = "file";
        roleNames[TypeRole] = "type";
        roleNames[NameRole] = "name";
        roleNames[ExecRole] = "exec";
        roleNames[CommentRole] = "comment";

        return roleNames;
    }

    const int SessionModel::lastIndex() const {
        return d->lastIndex;
    }

    int SessionModel::rowCount(const QModelIndex &parent) const {
        return d->sessions.length();
    }

    QVariant SessionModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() >= d->sessions.count())
            return QVariant();

        // get session
        Session *session = d->sessions[index.row()];

        // return correct value
        switch (role) {
        case DirectoryRole:
            return session->directory().absolutePath();
        case FileRole:
            return session->fileName();
        case TypeRole:
            return session->type();
        case NameRole:
            return session->displayName();
        case ExecRole:
            return session->exec();
        case CommentRole:
            return session->comment();
        default:
            break;
        }

        // return empty value
        return QVariant();
    }

    void SessionModel::populate(Session::Type type, const QString &path) {
        // read session files
        QDir dir(path);
        dir.setNameFilters(QStringList() << "*.desktop");
        dir.setFilter(QDir::Files);
        // read session
        foreach(const QString &session, dir.entryList()) {
            if (!dir.exists(session))
                continue;

            Session *si = new Session(type, session);
            bool execAllowed = true;
            QFileInfo fi(si->tryExec());
            if (fi.isAbsolute()) {
                if (!fi.exists() || !fi.isExecutable())
                    execAllowed = false;
            } else {
                execAllowed = false;
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                QString envPath = env.value("PATH");
                QStringList pathList = envPath.split(':');
                foreach(const QString &path, pathList) {
                    QDir pathDir(path);
                    fi.setFile(pathDir, si->tryExec());
                    if (fi.exists() && fi.isExecutable()) {
                        execAllowed = true;
                        break;
                    }
                }
            }
            // add to sessions list
            if (execAllowed)
                d->sessions.push_back(si);
        }
        // add failsafe session
        if (type == Session::X11Session) {
            Session *si = new Session(type, "failsafe");
            si->m_displayName = QStringLiteral("Failsafe");
            si->m_comment = QStringLiteral("Failsafe Session");
            si->m_exec = QStringLiteral("failsafe");
            d->sessions << si;
        }
        // find out index of the last session
        for (int i = 0; i < d->sessions.size(); ++i) {
            if (d->sessions.at(i)->fileName() == stateConfig.Last.Session.get()) {
                d->lastIndex = i;
                break;
            }
        }
    }
}
