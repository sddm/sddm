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

#include "Application.h"

int main(int argc, char **argv) {
    SDE::Application app(argc, argv);

    // get arguments
    const QStringList &arguments = app.arguments();

    // check for config argument
    QString configPath = "/etc/sddm.conf";
    int configIndex = arguments.indexOf("-c");
    if (configIndex >= 0) {
        if ((configIndex < arguments.size() - 1) && !arguments.at(configIndex + 1).startsWith("-"))
            configPath = arguments.at(configIndex + 1);
    }

    // check for test argument
    bool testing = false;
    QString testTheme = "";
    int testIndex = arguments.indexOf("-t");
    if (testIndex != -1) {
        testing = true;
        if ((testIndex < arguments.size() - 1) && !arguments.at(testIndex + 1).startsWith("-"))
            testTheme = arguments.at(testIndex + 1);
    }

    // init
    app.init(configPath);

    // run app
    if (testing)
        app.test(testTheme);
    else
        app.run();

    // return success
    return 0;
}
