/*
 * Configuration parser tests
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#ifndef CONFIGURATIONTEST_H
#define CONFIGURATIONTEST_H

#include <QObject>
#include <QStringList>

#include "ConfigReader.h"

#define CONF_FILE QStringLiteral("test.conf")
#define CONF_DIR QStringLiteral("testconfdir")
#define SYS_CONF_DIR QStringLiteral("testconfdir2")
#define CONF_FILE_COPY QStringLiteral("test_copy.conf")

#define TEST_STRING_1_PLAIN "Test Variable Initial String"
#define TEST_STRING_1 QStringLiteral(TEST_STRING_1_PLAIN)
#define TEST_INT_1 12345
#define TEST_STRINGLIST_1 {QStringLiteral("String1"), QStringLiteral("String2")}
#define TEST_BOOL_1 true

Config (TestConfig, CONF_FILE, CONF_DIR, SYS_CONF_DIR,
    enum CustomType {
        FOO,
        BAR,
        BAZ
    };
    Entry(    String,         QString,         _S(TEST_STRING_1_PLAIN), _S("Test String Description"));
    Entry(       Int,             int,                      TEST_INT_1, _S("Test Integer Description"));
    Entry(StringList,     QStringList,  QStringList(TEST_STRINGLIST_1), _S("Test StringList Description"));
    Entry(   Boolean,            bool,                     TEST_BOOL_1, _S("Test Boolean Description"));
    Entry(    Custom,      CustomType,                             FOO, _S("Custom type imitating NumState"));
    Section(Section,
        Entry(    String,         QString,         _S(TEST_STRING_1_PLAIN), _S("Test String Description"));
        Entry(       Int,             int,                      TEST_INT_1, _S("Test Integer Description"));
        Entry(StringList,     QStringList,  QStringList(TEST_STRINGLIST_1), _S("Test StringList Description"));
        Entry(   Boolean,            bool,                     TEST_BOOL_1, _S("Test Boolean Description"));
    );
);

inline QTextStream& operator>>(QTextStream &str, TestConfig::CustomType &state) {
    QString text = str.readLine().trimmed();
    if (text.compare(QLatin1String("foo"), Qt::CaseInsensitive) == 0)
        state = TestConfig::FOO;
    else if (text.compare(QLatin1String("bar"), Qt::CaseInsensitive) == 0)
        state = TestConfig::BAR;
    else
        state = TestConfig::BAZ;
    return str;
}

inline QTextStream& operator<<(QTextStream &str, const TestConfig::CustomType &state) {
    if (state == TestConfig::FOO)
        str << "foo";
    else if (state == TestConfig::BAR)
        str << "bar";
    else
        str << "baz";
    return str;
}

class ConfigurationTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void Basic();
    void Sections();
    void Unused();
    void LineChanges();
    void CustomEnum();
    void RightOnInit();
    void RightOnInitDir();
    void FileChanged();

private:
    TestConfig *config;
};

#endif // CONFIGURATIONTEST_H
