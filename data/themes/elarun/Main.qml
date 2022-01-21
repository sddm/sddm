/***************************************************************************
* Copyright (c) 2013 Reza Fatahilah Shah <rshah0385@kireihana.com>
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

import QtQuick 2.0
import SddmComponents 2.0

Rectangle {
    width: 640
    height: 480

    LayoutMirroring.enabled: Qt.locale().textDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property int sessionIndex: session.index

    TextConstants { id: textConstants }

    Connections {
        target: sddm
        onLoginSucceeded: {
        }
        onInformationMessage: {
        }
        onLoginFailed: {
            pw_entry.text = ""
        }
    }

    Background {
        anchors.fill: parent
        source: config.background
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            if (status == Image.Error && source != config.defaultBackground) {
                source = config.defaultBackground
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        //visible: primaryScreen

        Rectangle {
            width: 416; height: 262
            color: "#00000000"

            anchors.centerIn: parent

            Image {
                anchors.fill: parent
                source: "images/rectangle.png"
            }

            Image {
                anchors.fill: parent
                source: "images/rectangle_overlay.png"
                opacity: 0.1
            }

            Item {
                anchors.margins: 20
                anchors.fill: parent

                Text {
                    height: 50
                    anchors.top: parent.top
                    anchors.left: parent.left; anchors.right: parent.right

                    color: "#0b678c"
                    opacity: 0.75

                    text: sddm.hostName

                    font.bold: true
                    font.pixelSize: 18
                }

                Column {
                    anchors.centerIn: parent

                    Row {
                        Image { source: "images/user_icon.png" }

                        TextBox {
                            id: user_entry

                            width: 150; height: 30
                            anchors.verticalCenter: parent.verticalCenter;

                            text: userModel.lastUser

                            font.pixelSize: 14

                            KeyNavigation.backtab: layoutBox; KeyNavigation.tab: pw_entry
                        }
                    }

                    Row {

                        Image { source: "images/lock.png" }

                        PasswordBox {
                            id: pw_entry
                            width: 150; height: 30
                            anchors.verticalCenter: parent.verticalCenter;

                            tooltipBG: "CornflowerBlue"

                            font.pixelSize: 14

                            KeyNavigation.backtab: user_entry; KeyNavigation.tab: login_button

                            Keys.onPressed: {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    sddm.login(user_entry.text, pw_entry.text, sessionIndex)
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }

                ImageButton {
                    id: login_button
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 20

                    source: "images/login_normal.png"

                    onClicked: sddm.login(user_entry.text, pw_entry.text, sessionIndex)

		    KeyNavigation.backtab: pw_entry; KeyNavigation.tab: session
                }

                Item {
                    height: 20
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right

                    Row {
                        id: buttonRow
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter

                        spacing: 8

                        ImageButton {
                            id: system_button
                            source: "images/system_shutdown.png"
                            onClicked: sddm.powerOff()

			    KeyNavigation.backtab: session; KeyNavigation.tab: reboot_button
                        }

                        ImageButton {
                            id: reboot_button
                            source: "images/system_reboot.png"
                            onClicked: sddm.reboot()

                            KeyNavigation.backtab: system_button; KeyNavigation.tab: suspend_button
                        }

                        ImageButton {
                            id: suspend_button
                            source: "images/system_suspend.png"
                            visible: sddm.canSuspend
                            onClicked: sddm.suspend()

                            KeyNavigation.backtab: reboot_button; KeyNavigation.tab: hibernate_button
                        }

                        ImageButton {
                            id: hibernate_button
                            source: "images/system_hibernate.png"
                            visible: sddm.canHibernate
                            onClicked: sddm.hibernate()

                            KeyNavigation.backtab: suspend_button; KeyNavigation.tab: session
                        }
                    }

                    Timer {
                        id: time
                        interval: 100
                        running: true
                        repeat: true

                        onTriggered: {
                            dateTime.text = Qt.formatDateTime(new Date(), "dddd, dd MMMM yyyy HH:mm AP")
                        }
                    }

                    Text {
                        id: dateTime
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        horizontalAlignment: Text.AlignRight

                        color: "#0b678c"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
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

                text: textConstants.session
                font.pixelSize: 14
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

                KeyNavigation.backtab: hibernate_button; KeyNavigation.tab: layoutBox
            }

            Text {
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter

                text: textConstants.layout
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
            }

            LayoutBox {
                id: layoutBox
                width: 90
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 14

                arrowIcon: "angle-down.png"

                KeyNavigation.backtab: session; KeyNavigation.tab: user_entry
            }
        }
    }

    Component.onCompleted: {
        if (user_entry.text === "")
            user_entry.focus = true
        else
            pw_entry.focus = true
    }
}
