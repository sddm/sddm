/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_DISPLAYSERVER_H
#define SDDM_DISPLAYSERVER_H

#include <QObject>

class QProcess;

namespace SDDM {
    class Display;

    class DisplayServer : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(DisplayServer)
    public:
        explicit DisplayServer(Display *parent);

        Display *displayPtr() const;

        const QString &display() const;

        virtual QString sessionType() const = 0;

    public slots:
        virtual bool start() = 0;
        virtual void stop() = 0;
        virtual void finished() = 0;
        virtual void setupDisplay() = 0;

    signals:
        void started();
        void stopped();

    protected:
        bool m_started { false };

        QString m_display;

    private:
        Display *m_displayPtr { nullptr };
    };
}

#endif // SDDM_DISPLAYSERVER_H
