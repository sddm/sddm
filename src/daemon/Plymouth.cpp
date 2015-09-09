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

#include "Plymouth.h"
#include "Process.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDate>

static bool m_havePinged = false;
static bool m_haveCheckedActiveVt = false;
static bool m_isRunning = false;
static bool m_isActive = false;
static bool m_hasActiveVt = false;
static const QString plymouthBin = QString::fromUtf8("plymouth");

Plymouth::Plymouth(QObject *parent)
  : QObject(parent) 
{
}

Plymouth::~Plymouth() 
{
}

void Plymouth::log(QString str)
{
    QFile file(QString::fromUtf8("/var/log/sddm-plymouth.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) + 
            QString::fromUtf8(" ") + str << "\n";
        file.close();
    }
}

bool Plymouth::isRunning() 
{
    if (!m_havePinged) {
        m_havePinged = true;
        Process p;
        p.setProgram(plymouthBin);
        p.setArguments(QStringList() << QString::fromUtf8("--ping"));
        connect(&p, &Process::finished, [=](int exitCode) {
                log(__PRETTY_FUNCTION__);
				m_isRunning = WIFEXITED (exitCode) && WEXITSTATUS (exitCode) == 0;
                m_isActive = m_isRunning;
            });
        p.start();
    }

    return m_isRunning;
}

bool Plymouth::isActive() 
{
    return isRunning() && m_isActive;
}

bool Plymouth::hasActiveVt()
{
    if (!m_haveCheckedActiveVt) {
        m_haveCheckedActiveVt = true;
        Process p;
        p.setProgram(plymouthBin);
        p.setArguments(QStringList() << QString::fromUtf8("--has-active-vt"));
        connect(&p, &Process::finished, [=](int exitCode) {
                log(__PRETTY_FUNCTION__);
				m_hasActiveVt = WIFEXITED (exitCode) && WEXITSTATUS (exitCode) == 0;
            });
        p.start();
    }

    return m_hasActiveVt;
}

void Plymouth::prepareForTransition() 
{
    log(QString::fromUtf8(__PRETTY_FUNCTION__));
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("deactivate"));
    p.start();
}

void Plymouth::quitWithoutTransition() 
{
    log(QString::fromUtf8(__PRETTY_FUNCTION__));
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("quit"));
    p.start();
}

void Plymouth::quitWithTransition() 
{
    log(QString::fromUtf8(__PRETTY_FUNCTION__));
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("quit") << QString("--retain-splash"));
    p.start();
}
