/*
 * This file is part of SDDM.
 *
 * Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef GREETER_H
#define GREETER_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

#include "process.h"

namespace SDDM {
class ThemeConfig;
class ThemeMetadata;
}

class Greeter : public Process
{
    Q_OBJECT
public:
    explicit Greeter(Launcher *parent);
    ~Greeter();

    virtual bool start();

public Q_SLOTS:
    virtual void stop();

private:
    QString m_themePath;
    SDDM::ThemeConfig *m_themeConfig;
    SDDM::ThemeMetadata *m_themeMetadata;
    QString m_display;
    QString m_authPath;
    QProcess *m_process;

    QString findGreeterTheme() const;

private Q_SLOTS:
    void finished();
};

#endif // GREETER_H
