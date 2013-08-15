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

import QtQuick ${COMPONENTS_VERSION}

QtObject {
    ${READONLY_KEYWORD} property string capslockWarning:   qsTr("Warning, Caps Lock is ON!")
    ${READONLY_KEYWORD} property string layout:            qsTr("Layout")
    ${READONLY_KEYWORD} property string login:             qsTr("Login")
    ${READONLY_KEYWORD} property string loginFailed:       qsTr("Login failed")
    ${READONLY_KEYWORD} property string loginSucceded:     qsTr("Login succeeded")
    ${READONLY_KEYWORD} property string password:          qsTr("Password")
    ${READONLY_KEYWORD} property string prompt:            qsTr("Enter your username and password")
    ${READONLY_KEYWORD} property string promptSelectUser:  qsTr("Select your user and enter password")
    ${READONLY_KEYWORD} property string reboot:            qsTr("Reboot")
    ${READONLY_KEYWORD} property string session:           qsTr("Session")
    ${READONLY_KEYWORD} property string shutdown:          qsTr("Shutdown")
    ${READONLY_KEYWORD} property string userName:          qsTr("User name")
    ${READONLY_KEYWORD} property string welcomeText:       qsTr("Welcome to %1")
}

