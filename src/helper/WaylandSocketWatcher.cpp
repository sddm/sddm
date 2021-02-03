/*
 * Session process wrapper
 * Copyright (C) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
 * Copyright (C) 2021 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#include "WaylandSocketWatcher.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>

namespace SDDM {

WaylandSocketWatcher::WaylandSocketWatcher(QObject* parent, const QProcessEnvironment& env)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_runtimeDir(env.value(QLatin1String("XDG_RUNTIME_DIR")))
    , m_socketFileName(QDir(m_runtimeDir).absoluteFilePath(QLatin1String("wayland-0")))
{
}


void WaylandSocketWatcher::start()
{
    // give the compositor some time to start, if after that there is
    // no socket the session is considered failed
    m_timer.setSingleShot(true);
    m_timer.setInterval(15000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_watcher.isNull())
            m_watcher->deleteLater();
        qWarning() << "Wayland socket watcher timed out, checking for" << m_socketFileName << QFile::exists(m_socketFileName);
        emit sessionStarted(QFile::exists(m_socketFileName));
    });

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &path) {
        qDebug() << "Directory" << path << "has changed, checking for" << m_socketFileName;

        if (QFile::exists(m_socketFileName)) {
            // kill the timer
            m_timer.stop();

            // tell HelperApp we have the socket and delete the watcher
            qInfo() << "Wayland socket ready";
            emit sessionStarted(true);

            // kill the watcher
            if (!m_watcher.isNull())
                m_watcher->deleteLater();
        }
    });

    // if can't watch the runtime directory for any reason delete the
    // watcher and continue
    if (m_runtimeDir.isEmpty() || !m_watcher->addPath(m_runtimeDir)) {
        qWarning("Cannot watch \"%s\" for Wayland socket", qPrintable(m_runtimeDir));
        m_watcher->deleteLater();
    }

    // start the timer
    m_timer.start();
}

void WaylandSocketWatcher::stop()
{
    m_timer.stop();
    if (!m_watcher.isNull())
        m_watcher->deleteLater();
}

}

