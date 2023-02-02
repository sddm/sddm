/***************************************************************************
* Copyright (c) 2023 Fabian Vogt <fabian@ritter-vogt.de>
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

import QtQuick 2.3
import QtTest 1.0

TestCase {
    name: "QMLThemeConfigTest"

    function test_keys() {
        let keys = Object.keys(config);
        compare(keys.indexOf("doesnotexist"), -1);
        verify(keys.indexOf("someInteger") >= 0);
        keys = config.keys();
        compare(keys.indexOf("doesnotexist"), -1);
        verify(keys.indexOf("someInteger") >= 0);
    }

    // Everything is a string
    function test_propertyAPI() {
        compare(config.doesnotexist, undefined);
        compare(config.someTrueBool, "yes");
        compare(!!config.someTrueBool, true);
        compare(config.someFalseBool, "false");
        // "false" as a string is truthy!
        compare(!!config.someFalseBool, true);
        compare(config.someInteger, "042");
        compare(+config.someInteger, 42);
        compare(config.someRealNumber, "01.5");
        compare(+config.someRealNumber, 1.5);
        compare(config.someString, "Pie/180");
    }

    // Strings are converted to specific types
    function test_typedAPI() {
        compare(config.stringValue("doesnotexist"), "");
        compare(config.boolValue("someTrueBool"), true);
        compare(config.boolValue("someFalseBool"), false);
        // "false" as a string is truthy!
        compare(!!config.someFalseBool, true);
        compare(config.stringValue("someInteger"), "042");
        compare(config.intValue("someInteger"), 42);
        compare(config.realValue("someRealNumber"), 1.5);
        // conversion fails -> 0
        compare(config.intValue("someRealNumber"), 0);
        compare(config.stringValue("someString"), "Pie/180");
        // conversion fails -> 0
        compare(config.intValue("someString"), 0);
        compare(config.realValue("someString"), 0);
    }
}
