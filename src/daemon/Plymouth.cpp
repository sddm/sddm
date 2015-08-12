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

static const QString plymouthBin = "/bin/plymouth";

Plymouth::Plymouth(QObject *parent)
  : QObject(parent) 
{
}

Plymouth::~Plymouth() 
{
}

void Plymouth::log(QString str)
{
    QFile file("/tmp/sddm-plymouth.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << str << "\n";
        file.close();
    }
}

bool Plymouth::isRunning() 
{
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("--ping"));
    connect(&p, &Process::finished, [=](int exitCode) {
                log(__PRETTY_FUNCTION__);
                return WIFEXITED (exitCode) && WEXITSTATUS (exitCode) == 0;
            });
    p.start();

    return false;
}

bool Plymouth::hasActiveVt()
{
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("--has-active-vt"));
    connect(&p, &Process::finished, [=](int exitCode) {
                log(__PRETTY_FUNCTION__);
                return WIFEXITED (exitCode) && WEXITSTATUS (exitCode) == 0;
            });
    p.start();

    return false;
}

void Plymouth::prepareForTransition() 
{
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("deactivate"));
    p.start();
}

void Plymouth::quitWithoutTransition() 
{
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("quit"));
    p.start();
}

void Plymouth::quitWithTransition() 
{
    Process p;
    p.setProgram(plymouthBin);
    p.setArguments(QStringList() << QString("quit") << QString("--retain-splash"));
    p.start();
}
