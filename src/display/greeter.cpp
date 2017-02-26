/*
 * This file is part of SDDM.
 *
 * Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
 * Copyright (C) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#include <QtCore/QDir>

#include "Configuration.h"
#include "ThemeConfig.h"
#include "ThemeMetadata.h"

#include "greeter.h"
#include "launcher.h"

Greeter::Greeter(Launcher *parent)
    : Process(parent)
    , m_themeConfig(new SDDM::ThemeConfig(QString()))
    , m_themeMetadata(new SDDM::ThemeMetadata(QString()))
    , m_process(new QProcess(this))
{
    // Load theme configuration and metadata
    m_themePath = findGreeterTheme();
    if (!m_themePath.isEmpty()) {
        const QString path = QStringLiteral("%1/metadata.desktop").arg(m_themePath);
        m_themeMetadata->setTo(path);

        QString configFile = QStringLiteral("%1/%2").arg(m_themePath).arg(m_themeMetadata->configFile());
        m_themeConfig->setTo(configFile);
    }

    // Relay process messages
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        qDebug() << "Greeter output:" << m_process->readAllStandardOutput().constData();
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        qDebug() << "Greeter errors:" << m_process->readAllStandardError().constData();
    });

    // Stop when the process has finished
    connect(m_process, SIGNAL(finished(int)), this, SLOT(finished()));
}

Greeter::~Greeter()
{
    delete m_themeConfig;
    delete m_themeMetadata;
}

bool Greeter::start()
{
    Launcher *session = qobject_cast<Launcher *>(parent());

    // Themes
    QString xcursorTheme = SDDM::mainConfig.Theme.CursorTheme.get();
    if (m_themeConfig->contains(QLatin1String("cursorTheme")))
        xcursorTheme = m_themeConfig->value(QLatin1String("cursorTheme")).toString();
    QString platformTheme;
    if (m_themeConfig->contains(QLatin1String("platformTheme")))
        platformTheme = m_themeConfig->value(QLatin1String("platformTheme")).toString();
    QString style;
    if (m_themeConfig->contains(QLatin1String("style")))
        style = m_themeConfig->value(QLatin1String("style")).toString();

    // Process environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QLatin1String("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
    env.insert(QLatin1String("XCURSOR_THEME"), xcursorTheme);
    env.insert(QLatin1String("QT_IM_MODULE"), SDDM::mainConfig.InputMethod.get());
    for (const QString &key : m_env.keys())
        env.insert(key, m_env[key]);
    // Some themes may use KDE components and that will automatically load KDE's crash handler which
    // counterintuitively setting this env disables that handler
    env.insert(QLatin1String("KDE_DEBUG"), QString::number(1));
    m_process->setProcessEnvironment(env);

    // Run greeter
    const QString cmd = QStringLiteral("%1/sddm-greeter").arg(QStringLiteral(BIN_INSTALL_DIR));
    QStringList args;
    args << QLatin1String("--socket") << session->socket()
         << QLatin1String("--theme") << m_themePath;
    if (!platformTheme.isEmpty())
        args << QLatin1String("-platformtheme") << platformTheme;
    if (!style.isEmpty())
        args << QLatin1String("-style") << style;
    if (session->isTestModeEnabled())
        args << QLatin1String("--test-mode");
    m_process->start(cmd, args);
    qDebug() << "Running:"
             << qPrintable(cmd)
             << qPrintable(args.join(QLatin1Char(' ')));

    if (m_process->state() == QProcess::NotRunning) {
        qCritical("Failed to run greeter");
        return false;
    }

    if (!m_process->waitForStarted()) {
        qCritical("Failed to start greeter");
        return false;
    }

    qInfo("Greeter started");
    emit started();
    m_started = true;

    return true;
}

void Greeter::stop()
{
    // Terminate the process when stop is requested
    m_process->terminate();
    if (!m_process->waitForFinished())
        m_process->kill();
}

QString Greeter::findGreeterTheme() const
{
    QString themeName = SDDM::mainConfig.Theme.Current.get();

    // An unconfigured theme means the user wants to load the
    // default theme from the resources
    if (themeName.isEmpty())
        return QString();

    QDir dir(SDDM::mainConfig.Theme.ThemeDir.get());

    // Return the default theme if it exists
    if (dir.exists(themeName))
        return dir.absoluteFilePath(themeName);

    // Otherwise use the embedded theme
    qWarning() << "The configured theme" << themeName << "doesn't exist, using the embedded theme instead";
    return QString();
}

void Greeter::finished()
{
    qInfo("Greeter stopped");
    emit stopped();
    m_started = false;
}
