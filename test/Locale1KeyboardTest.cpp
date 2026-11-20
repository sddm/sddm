/***************************************************************************
* Copyright (c) 2026 Simon Quigley
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
***************************************************************************/

#include "Locale1Keyboard.h"

#include <QTest>
#include <QVariantMap>

class Locale1KeyboardTest : public QObject {
    Q_OBJECT
private slots:
    void emptyProperties()
    {
        const SDDM::Locale1Keyboard kb = SDDM::parseLocale1Keyboard(QVariantMap());
        QVERIFY(kb.layouts.isEmpty());
        QVERIFY(kb.variants.isEmpty());
    }

    void singleLayout()
    {
        QVariantMap props;
        props.insert(QStringLiteral("X11Layout"), QStringLiteral("gb"));
        props.insert(QStringLiteral("X11Model"), QStringLiteral("pc105"));
        const SDDM::Locale1Keyboard kb = SDDM::parseLocale1Keyboard(props);
        QCOMPARE(kb.layouts, QStringList() << QStringLiteral("gb"));
        QCOMPARE(kb.model, QStringLiteral("pc105"));
    }

    void multipleLayouts()
    {
        QVariantMap props;
        props.insert(QStringLiteral("X11Layout"), QStringLiteral("gb,us"));
        props.insert(QStringLiteral("X11Variant"), QStringLiteral(",dvorak"));
        const SDDM::Locale1Keyboard kb = SDDM::parseLocale1Keyboard(props);
        QCOMPARE(kb.layouts.size(), 2);
        QCOMPARE(kb.layouts.at(0), QStringLiteral("gb"));
        QCOMPARE(kb.layouts.at(1), QStringLiteral("us"));
        QCOMPARE(kb.variants.size(), 2);
        QCOMPARE(kb.variants.at(1), QStringLiteral("dvorak"));
    }
};

QTEST_MAIN(Locale1KeyboardTest)
#include "Locale1KeyboardTest.moc"
