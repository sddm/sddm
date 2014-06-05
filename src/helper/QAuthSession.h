/*
 * Session process wrapper
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef QAUTH_SESSION_H
#define QAUTH_SESSION_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QProcess>

class QAuthApp;
class QAuthSession : public QProcess
{
    Q_OBJECT
public:
    explicit QAuthSession(QAuthApp *parent);
    virtual ~QAuthSession();

    bool start();

    void setPath(const QString &path);
    QString path() const;

protected:
    void bail(int status);
    void setupChildProcess();

private:
    QString m_path { };
};

#endif // QAUTH_SESSION_H
