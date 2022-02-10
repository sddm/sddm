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

#ifndef SDDM_SIGNALHANDLER_H
#define SDDM_SIGNALHANDLER_H

#include <QObject>

class QSocketNotifier;

namespace SDDM {
    class SignalHandler : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(SignalHandler)
    public:
        SignalHandler(QObject *parent = 0);

        void addCustomSignal(int signal);

    signals:
        void sighupReceived();
        void sigintReceived();
        void sigtermReceived();
        void customSignalReceived(int signal);

    private slots:
        void handleSigint();
        void handleSigterm();
        void handleSigCustom();

    private:
        static void initialize();
        static void intSignalHandler(int unused);
        static void termSignalHandler(int unused);
        static void customSignalHandler(int unused);

        QSocketNotifier *snint { nullptr };
        QSocketNotifier *snterm { nullptr };
        QSocketNotifier *sncustom { nullptr };
    };
}
#endif // SDDM_SIGNALHANDLER_H
