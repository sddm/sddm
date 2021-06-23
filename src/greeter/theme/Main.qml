/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

import QtQuick 2.0
import SddmComponents 2.0
import PamTypes 1.0

Rectangle {
    id: container
    width: 1024
    height: 768

    LayoutMirroring.enabled: Qt.locale().textDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property int sessionIndex: session.index

    TextConstants { id: textConstants }

    Component {
        id: pwdChangeComp
        // dialog to change expired passwords
        PasswordChange {
            color: "#22888888"
            promptColor: "white"
            infosColor: "lightcoral"
            titleColor: "white"
            titleTextColor: "black"
            //infosHeight: 10
        }
    }

    PamConvHelper {
        loader: pwdChangeLoader
        component: pwdChangeComp
    }

    function resetTxtMsg() {
        txtMessage.text = textConstants.promptSelectUser
        txtMessage.color = "white"
    }

    Connections {
        target: sddm
        onLoginFailed: {
            listView.currentItem.password = ""
            listView.currentItem.forceActiveFocus()
            txtMessage.color = "red"
            // explain why password change dialog suddenly disappears,
            // cause pam_chauthtok failed with PAM_MAXTRIES
            if(result == PamTypes.RESULT_PAM_MAXTRIES)
                txtMessage.text = textConstants.pamMaxtriesError
            else // filter out login failure details
                txtMessage.text = textConstants.loginFailed
        }
    }

    Background {
        anchors.fill: parent
        source: "qrc:/theme/background.png"
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            if (status == Image.Error && source != config.defaultBackground) {
                source = config.defaultBackground
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                listView.focus = true;
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        //visible: primaryScreen

        Component {
            id: userDelegate

            PictureBox {
                anchors.verticalCenter: parent.verticalCenter
                name: (model.realName === "") ? model.name : model.realName
                icon: model.icon
                showPassword: model.needsPassword

                focus: (listView.currentIndex === index) ? true : false
                state: (listView.currentIndex === index) ? "active" : ""

                onLogin: sddm.login(model.name, password, sessionIndex);

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        listView.currentIndex = index;
                        listView.focus = true;
                    }
                }
            }
        }

        Row {
            anchors.fill: parent
            // hide main input for small displays when password change dialog is active
            visible: !(pwdChangeLoader.status == Loader.Ready && container.height < 768)
            //visible: primaryScreen

            Rectangle {
                width: parent.width / 2; height: parent.height
                color: "#00000000"

                Clock {
                    id: clock
                    anchors.centerIn: parent
                    color: "white"
                    timeFont.family: "Oxygen"
                }
            }

            Rectangle {
                id: rightBox
                width: parent.width / 2; height: parent.height
                color: "#22000000"
                clip: true

                Item {
                    id: usersContainer
                    width: parent.width; height: 300
                    anchors.verticalCenter: parent.verticalCenter
                    // block user selection during password change
                    enabled: pwdChangeLoader.status != Loader.Ready

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
                        onCurrentIndexChanged: resetTxtMsg()
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
                    text: textConstants.promptSelectUser
                    wrapMode: Text.WordWrap
                    width:parent.width - 60
                    font.pixelSize: 20
                }

                Text {
                    id: errMessage
                    anchors.top: txtMessage.bottom
                    anchors.margins: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "red"
                    text: "The current theme cannot be loaded due to the errors below, please select another theme.\n" + __sddm_errors
                    wrapMode: Text.WordWrap
                    width: parent.width - 60
                    font.pixelSize: 20
                    visible: __sddm_errors !== ""
                }

            }

        }

        Loader {
            id: pwdChangeLoader
            // if screen has enough space show dialog below picture box
            anchors.top: container.height<768 ? undefined : actionBar.bottom
            anchors.topMargin: container.height<768 ? undefined : 32
            anchors.horizontalCenter: container.height<768 ? undefined : parent.horizontalCenter
            // otherwise on small displays center on screen and hide picture box
            anchors.centerIn: container.height<768 ? parent : undefined
        }

        Rectangle {
            id: actionBar
            anchors.top: parent.top;
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width; height: 40
            //visible: primaryScreen

            Row {
                anchors.left: parent.left
                anchors.margins: 5
                height: parent.height
                spacing: 5

                Text {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    text: textConstants.session
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

                    KeyNavigation.backtab: nextUser; KeyNavigation.tab: layoutBox
                }

                Text {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    text: textConstants.layout
                    font.pixelSize: 16
                    verticalAlignment: Text.AlignVCenter
                }

                LayoutBox {
                    id: layoutBox
                    width: 90
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 14

                    arrowIcon: "angle-down.png"

                    KeyNavigation.backtab: session; KeyNavigation.tab: btnShutdown
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

                    KeyNavigation.backtab: layoutBox; KeyNavigation.tab: btnShutdown
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
