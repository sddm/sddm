/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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
    id: container
    width: 1024
    height: 768

    Connections {
        target: sessionManager
        onSuccess: {
        }

        onFail: {
            txtMessage.text = qsTr("Login failed. Please try again.")
        }
    }

    FontLoader { id: clockFont; source: "GeosansLight.ttf" }

    Image {
        anchors.fill: parent
        source: "background.png"
        fillMode: Image.PreserveAspectCrop
    }

    Component {
        id: userDelegate

        PictureBox {
            anchors.verticalCenter: parent.verticalCenter
            name: (model.realName === "") ? model.userName : model.realName
            icon: model.icon

            focus: (listView.currentIndex === index) ? true : false
            state: (listView.currentIndex === index) ? "active" : ""

            onLogin: { sessionManager.login(model.userName, password, session.index); password = "" }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: listView.currentIndex = index
                onClicked: listView.focus = true
            }
        }
    }

    Row {
        anchors.fill: parent

        Rectangle {
            width: parent.width / 2; height: parent.height
            color: "#00000000"

            Clock {
                id: clock
                anchors.centerIn: parent
                color: "white"
                font: clockFont.name
            }
        }

        Rectangle {
            width: parent.width / 2; height: parent.height
            color: "#22000000"
            clip: true

            Item {
                id: usersContainer
                width: parent.width; height: 300
                anchors.verticalCenter: parent.verticalCenter

                ImageButton {
                    id: prevUser
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    source: "angle-left.png"
                    onClicked: listView.decrementCurrentIndex()
                }

                ListView {
                    id: listView
                    height: parent.height
                    anchors.left: prevUser.right; anchors.right: nextUser.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10

                    clip: true
                    focus: true

                    spacing: 5

                    model: userModel
                    delegate: userDelegate
                    orientation: ListView.Horizontal
                    currentIndex: 0
                }

                ImageButton {
                    id: nextUser
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    source: "angle-right.png"
                    onClicked: listView.incrementCurrentIndex()
                }
            }

            Text {
                id: txtMessage
                anchors.top: usersContainer.bottom;
                anchors.margins: 20
                anchors.horizontalCenter: parent.horizontalCenter
                color: "white"
                text: qsTr("Enter your user name and password.")

                font.pixelSize: 20
            }
        }
    }

    Rectangle {
        id: actionBar
        anchors.top: parent.top;
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width; height: 40

        Row {
            anchors.left: parent.left
            anchors.margins: 5
            height: parent.height
            spacing: 5

            Text {
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter

                text: qsTr("Session:")
                font.pixelSize: 16
                verticalAlignment: Text.AlignVCenter
            }

            ComboBox {
                id: session
                width: 245
                anchors.verticalCenter: parent.verticalCenter

                arrowIcon: "angle-down.png"

                model: sessionModel
                index: sessionModel.lastIndex

                font.pixelSize: 14
            }
        }

        Row {
            height: parent.height
            anchors.right: parent.right
            anchors.margins: 5
            spacing: 5

            ImageButton {
                height: parent.height
                source: "reboot.png"
                onClicked: sessionManager.reboot()
            }

            ImageButton {
                height: parent.height
                source: "shutdown.png"
                onClicked: sessionManager.shutdown()
            }
        }

    }
}
