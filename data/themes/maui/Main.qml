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

    property int sessionIndex: session.index

    Connections {
        target: sddm
        onLoginSucceeded: {
        }

        onLoginFailed: {
            txtMessage.text = qsTr("Login failed. Please try again.")
            listView.currentItem.password.text = ""
        }
    }

    FontLoader { id: clockFont; source: "GeosansLight.ttf" }

    Repeater {
        model: screenModel
        Background {
            x: geometry.x; y: geometry.y; width: geometry.width; height:geometry.height
            source: config.background
            fillMode: Image.PreserveAspectCrop
        }
    }

    Rectangle {
        property variant geometry: screenModel.geometry(screenModel.primary)
        x: geometry.x; y: geometry.y; width: geometry.width; height: geometry.height
        color: "transparent"

        Component {
            id: userDelegate

            PictureBox {
                anchors.verticalCenter: parent.verticalCenter
                name: (model.realName === "") ? model.name : model.realName
                icon: model.icon

                focus: (listView.currentIndex === index) ? true : false
                state: {
                    if (listView.currentIndex == index) {
                        if (keyboard.enabled && keyboard.capsLock)
                            state:  "activeWarning"
                        else
                            state: "active"
                    } else
                        state: ""
                }

                onLogin: sddm.login(model.name, password, sessionIndex);

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
                    timeFont.family: clockFont.name
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

                        KeyNavigation.backtab: btnShutdown; KeyNavigation.tab: listView
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
                        currentIndex: userModel.lastIndex

                        KeyNavigation.backtab: prevUser; KeyNavigation.tab: nextUser
                    }

                    ImageButton {
                        id: nextUser
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 10
                        source: "angle-right.png"
                        onClicked: listView.incrementCurrentIndex()
                        KeyNavigation.backtab: listView; KeyNavigation.tab: session
                    }
                }

                Text {
                    id: txtMessage
                    anchors.top: usersContainer.bottom;
                    anchors.margins: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "white"
                    text: qsTr("Select your user and enter password.")

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

                    KeyNavigation.backtab: nextUser; KeyNavigation.tab: btnReboot
                }

                Text {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    text: qsTr("Layout:")
                    font.pixelSize: 16
                    verticalAlignment: Text.AlignVCenter
                    visible: keyboard.enabled
                }

                ComboBox {
                    id: layoutsBox
                    width: 80
                    visible: keyboard.enabled
                    anchors.verticalCenter: parent.verticalCenter

                    arrowIcon: "angle-down.png"

                    model: keyboard.layouts
                    index: keyboard.currentLayout
                    textDelegate: Text {
                        anchors.fill: parent
                        anchors.margins: 4

                        text: modelItem.modelData.shortName

                        font: layoutsBox.font
                    }
                    onValueChanged: keyboard.currentLayout = id

                    font.pixelSize: 14

                    KeyNavigation.backtab: nextUser; KeyNavigation.tab: btnReboot
                }
            }

            Row {
                height: parent.height
                anchors.right: parent.right
                anchors.margins: 5
                spacing: 5

                ImageButton {
                    id: btnReboot
                    height: parent.height
                    source: "reboot.png"

                    visible: sddm.canReboot

                    onClicked: sddm.reboot()

                    KeyNavigation.backtab: session; KeyNavigation.tab: btnShutdown
                }

                ImageButton {
                    id: btnShutdown
                    height: parent.height
                    source: "shutdown.png"

                    visible: sddm.canPowerOff

                    onClicked: sddm.powerOff()

                    KeyNavigation.backtab: btnReboot; KeyNavigation.tab: prevUser
                }
            }
        }
    }
}
