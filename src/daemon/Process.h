/***************************************************************************
* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#ifndef PROCESS_H
#define PROCESS_H

#include <QObject>
#include <QStringList>

class Process : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Process)
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(QStringList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(QString dir READ dir WRITE setDir NOTIFY dirChanged)

public:
    Process(QObject *parent = 0);
    ~Process();

    const QString &program() const;
    const QStringList &arguments() const;
    const QString &dir() const;

    Q_INVOKABLE QByteArray readAllStandardError();
    Q_INVOKABLE QByteArray readAllStandardOutput();

public slots:
    void setProgram(const QString &program);
    void setArguments(const QStringList &arguments);
    void setDir(const QString &dir);

    void start();
    void terminate();
    void kill();

signals:
    void programChanged(const QString &program);
    void argumentsChanged(const QStringList &arguments);
    void dirChanged();

    void finished(int exitCode);
    void readyReadStandardError();
    void readyReadStandardOutput();
    void started();

private:
    class Private;
    Private *d;
};

#endif // PROCESS_H
