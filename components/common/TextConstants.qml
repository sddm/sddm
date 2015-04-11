/***************************************************************************
* Copyright (c) 2013 Nikita Mikhailov <nslqqq@gmail.com>
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
* OR OTHER DEALINGS IN THE SOFTWARE.
*
***************************************************************************/

import QtQuick 2.0

QtObject {
    readonly property string capslockWarning:   qsTr("Warning, Caps Lock is ON!")
    readonly property string layout:            qsTr("Layout")
    readonly property string login:             qsTr("Login")
    readonly property string loginFailed:       qsTr("Login failed")
    readonly property string loginSucceeded:    qsTr("Login succeeded")
    readonly property string password:          qsTr("Password")
    readonly property string prompt:            qsTr("Enter your username and password")
    readonly property string promptSelectUser:  qsTr("Select your user and enter password")
    readonly property string reboot:            qsTr("Reboot")
    readonly property string session:           qsTr("Session")
    readonly property string shutdown:          qsTr("Shutdown")
    readonly property string userName:          qsTr("User name")
    readonly property string welcomeText:       qsTr("Welcome to %1")
}

