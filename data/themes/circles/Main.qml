/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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
import SddmComponents ${COMPONENTS_VERSION}

Rectangle {
    width: 640
    height: 480

    Connections {
        target: sessionManager

        onSuccess: {
            errorMessage.color = "steelblue"
            errorMessage.text = qsTr("Login succeeded.")
        }

        onFail: {
            errorMessage.color = "red"
            errorMessage.text = qsTr("Login failed.")
        }
    }

    Image {
        anchors.centerIn: parent
        source: "background.png"
        fillMode: Image.Tile
    }

    Rectangle {
        anchors.centerIn: parent
        width: 320; height: 320
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#c3c3c3" }
            GradientStop { position: 0.5; color: "#e5e5e5" }
            GradientStop { position: 1.0; color: "#c3c3c3" }
        }

        border.color: "#ababab"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 12
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                color: "black"
                text: qsTr("Welcome to ") + sessionManager.hostName
                font.pixelSize: 24
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }

            Column {
                width: parent.width
                spacing: 4
                Text {
                    id: lblName
                    width: 60
                    text: qsTr("User name")
                    font.bold: true
                    font.pixelSize: 12
                }

                TextBox {
                    id: name
                    width: parent.width; height: 30
                    text: userModel.lastUser
                    font.pixelSize: 14

                    KeyNavigation.backtab: rebootButton; KeyNavigation.tab: password

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return) {
                            sessionManager.login(name.text, password.text, session.index)
                            event.accepted = true
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing : 4
                Text {
                    id: lblPassword
                    width: 60
                    text: qsTr("Password")
                    font.bold: true
                    font.pixelSize: 12
                }

                TextBox {
                    id: password
                    width: parent.width; height: 30
                    font.pixelSize: 14

                    echoMode: TextInput.Password

                    KeyNavigation.backtab: name; KeyNavigation.tab: session

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return) {
                            sessionManager.login(name.text, password.text, session.index)
                            event.accepted = true
                        }
                    }
                }
            }

            Column {
                z: 100
                width: parent.width
                spacing : 4
                Text {
                    id: lblSession
                    width: 60
                    text: qsTr("Session")
                    font.bold: true
                    font.pixelSize: 12
                }

                ComboBox {
                    id: session
                    width: parent.width; height: 30

                    arrowIcon: "angle-down.png"

                    model: sessionModel
                    index: sessionModel.lastIndex

                    font.pixelSize: 14

                    KeyNavigation.backtab: password; KeyNavigation.tab: loginButton
                }
            }

            Column {
                width: parent.width
                Text {
                    id: errorMessage
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Enter your user name and password.")
                    font.pixelSize: 10
                }
            }

            Row {
                spacing: 4
                anchors.horizontalCenter: parent.horizontalCenter
                Button {
                    id: loginButton
                    text: qsTr("Login")

                    onClicked: sessionManager.login(name.text, password.text, session.index)

                    KeyNavigation.backtab: session; KeyNavigation.tab: shutdownButton
                }

                Button {
                    id: shutdownButton
                    text: qsTr("Shutdown")

                    onClicked: sessionManager.shutdown()

                    KeyNavigation.backtab: loginButton; KeyNavigation.tab: rebootButton
                }

                Button {
                    id: rebootButton
                    text: qsTr("Reboot")

                    onClicked: sessionManager.reboot()

                    KeyNavigation.backtab: shutdownButton; KeyNavigation.tab: name
                }
            }
        }
    }

    Component.onCompleted: {
        if (name.text === "")
            name.focus = true
        else
            password.focus = true
    }
}
