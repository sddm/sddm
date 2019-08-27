/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_XORGUSERDISPLAYSERVER_H
#define SDDM_XORGUSERDISPLAYSERVER_H

#include "DisplayServer.h"
#include "XAuth.h"

class QProcess;

namespace SDDM {

class XorgUserDisplayServer : public DisplayServer
{
    Q_OBJECT
    Q_DISABLE_COPY(XorgUserDisplayServer)
public:
    explicit XorgUserDisplayServer(Display *parent);
    ~XorgUserDisplayServer();

    QString sessionType() const;

    void setDisplayName(const QString &displayName);

    static QString command(Display *display);

public Q_SLOTS:
    bool start();
    void stop();
    void finished();
    void setupDisplay();
};

} // namespace SDDM

#endif // SDDM_XORGUSERDISPLAYSERVER_H
