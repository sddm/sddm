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

#include "SessionModel.h"

#include "Configuration.h"

#include <QDir>
#include <QFile>
#include <QList>
#include <QTextStream>

#include <memory>

namespace SDDM {
    class Session {
    public:
        QString file;
        QString name;
        QString exec;
        QString comment;
    };

    typedef std::shared_ptr<Session> SessionPtr;

    class SessionModelPrivate {
    public:
        int lastIndex { 0 };
        QList<SessionPtr> sessions;
    };

    SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent), d(new SessionModelPrivate()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[FileRole] = "file";
        roleNames[NameRole] = "name";
        roleNames[ExecRole] = "exec";
        roleNames[CommentRole] = "comment";
        // set role names
        setRoleNames(roleNames);
#endif
        // read session files
        QDir dir(mainConfig.XDisplay.SessionDir.get());
        dir.setNameFilters(QStringList() << "*.desktop");
        dir.setFilter(QDir::Files);
        // read session
        foreach(const QString &session, dir.entryList()) {
            QFile inputFile(dir.absoluteFilePath(session));
            if (!inputFile.open(QIODevice::ReadOnly))
                continue;
            SessionPtr si { new Session { session, "", "", "" } };
            QTextStream in(&inputFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("Name="))
                    si->name = line.mid(5);
                if (line.startsWith("Exec="))
                    si->exec = line.mid(5);
                if (line.startsWith("Comment="))
                    si->comment = line.mid(8);
            }
            // add to sessions list
            d->sessions.push_back(si);
            // close file
            inputFile.close();
        }
        // add failsafe session
        d->sessions << SessionPtr { new Session {"failsafe", "Failsafe", "failsafe", "Failsafe Session"} };
        // find out index of the last session
        for (int i = 0; i < d->sessions.size(); ++i) {
            if (d->sessions.at(i)->file == stateConfig.Last.Session.get()) {
                d->lastIndex = i;
                break;
            }
        }
    }

    SessionModel::~SessionModel() {
        delete d;
    }

    QHash<int, QByteArray> SessionModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[FileRole] = "file";
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
        if (index.row() < 0 || index.row() > d->sessions.count())
            return QVariant();

        // get session
        SessionPtr session = d->sessions[index.row()];

        // return correct value
        if (role == FileRole)
            return session->file;
        else if (role == NameRole)
            return session->name;
        else if (role == ExecRole)
            return session->exec;
        else if (role == CommentRole)
            return session->comment;

        // return empty value
        return QVariant();
    }
}
