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

#ifndef SDE_SIGNALHANDLER_H
#define SDE_SIGNALHANDLER_H

#include <QObject>

class QSocketNotifier;

namespace SDE {
    class SignalHandler : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(SignalHandler)
    public:
        SignalHandler(QObject *parent = 0);

        static void initialize();
        static void hupSignalHandler(int unused);
        static void intSignalHandler(int unused);

    signals:
        void sighupReceived();
        void sigintReceived();

    private slots:
        void handleSighup();
        void handleSigint();

    private:
        QSocketNotifier *snhup { nullptr };
        QSocketNotifier *snint { nullptr };
    };
}
#endif // SDE_SIGNALHANDLER_H
