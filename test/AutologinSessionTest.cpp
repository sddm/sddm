/***************************************************************************
* Copyright (c) 2026 Simon Quigley
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

#include "Configuration.h"
#include "Session.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTest>

class AutologinSessionTest : public QObject {
    Q_OBJECT
private slots:
    void testPrefersWaylandByDefault()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString waylandDir = tempDir.path() + QStringLiteral("/wayland-sessions");
        const QString x11Dir = tempDir.path() + QStringLiteral("/xsessions");
        QVERIFY(QDir().mkpath(waylandDir));
        QVERIFY(QDir().mkpath(x11Dir));

        const QString desktopContents = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Test Session\n"
            "Exec=/bin/true\n"
        );

        QFile waylandDesktop(waylandDir + QStringLiteral("/plasma.desktop"));
        QVERIFY(waylandDesktop.open(QIODevice::WriteOnly));
        waylandDesktop.write(desktopContents.toUtf8());
        waylandDesktop.close();

        QFile x11Desktop(x11Dir + QStringLiteral("/plasma.desktop"));
        QVERIFY(x11Desktop.open(QIODevice::WriteOnly));
        x11Desktop.write(desktopContents.toUtf8());
        x11Desktop.close();

        SDDM::mainConfig.Wayland.SessionDir.set({waylandDir});
        SDDM::mainConfig.X11.SessionDir.set({x11Dir});

        const SDDM::Session waylandSession = SDDM::Session::findAutologinSession(
            QStringLiteral("plasma"), SDDM::Session::WaylandSession);
        QVERIFY(waylandSession.isValid());
        QCOMPARE(waylandSession.type(), SDDM::Session::WaylandSession);
        QCOMPARE(waylandSession.xdgSessionType(), QStringLiteral("wayland"));

        const SDDM::Session x11Session = SDDM::Session::findAutologinSession(
            QStringLiteral("plasma"), SDDM::Session::X11Session);
        QVERIFY(x11Session.isValid());
        QCOMPARE(x11Session.type(), SDDM::Session::X11Session);
        QCOMPARE(x11Session.xdgSessionType(), QStringLiteral("x11"));
    }

    void testFallsBackToOtherType()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString x11Dir = tempDir.path() + QStringLiteral("/xsessions");
        QVERIFY(QDir().mkpath(x11Dir));

        const QString desktopContents = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Test Session\n"
            "Exec=/bin/true\n"
        );

        QFile x11Desktop(x11Dir + QStringLiteral("/plasma.desktop"));
        QVERIFY(x11Desktop.open(QIODevice::WriteOnly));
        x11Desktop.write(desktopContents.toUtf8());
        x11Desktop.close();

        SDDM::mainConfig.Wayland.SessionDir.set({tempDir.path() + QStringLiteral("/wayland-sessions")});
        SDDM::mainConfig.X11.SessionDir.set({x11Dir});

        const SDDM::Session session = SDDM::Session::findAutologinSession(
            QStringLiteral("plasma"), SDDM::Session::WaylandSession);
        QVERIFY(session.isValid());
        QCOMPARE(session.type(), SDDM::Session::X11Session);
    }

    void testSessionTypeParsing()
    {
        SDDM::MainConfig::AutologinSessionType type = SDDM::MainConfig::AUTOLOGIN_WAYLAND;

        QString waylandValue = QStringLiteral("wayland");
        QTextStream waylandIn(&waylandValue);
        waylandIn >> type;
        QCOMPARE(type, SDDM::MainConfig::AUTOLOGIN_WAYLAND);

        QString x11Value = QStringLiteral("x11");
        QTextStream x11In(&x11Value);
        x11In >> type;
        QCOMPARE(type, SDDM::MainConfig::AUTOLOGIN_X11);

        QString str;
        QTextStream out(&str);
        out << SDDM::MainConfig::AUTOLOGIN_X11;
        QCOMPARE(str, QStringLiteral("x11"));
    }
};

QTEST_MAIN(AutologinSessionTest);

#include "AutologinSessionTest.moc"