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

Rectangle {
    id: container
    width: 1024
    height: 768

    LayoutMirroring.enabled: Qt.locale().textDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property int sessionIndex: session.index
    property bool pamConversationActive: false
    property string pamConversationUser: ""
    property bool pamResponsePending: false
    property string pamPendingResponse: ""

    function beginPamAuthentication(username) {
        if (!username)
            return

        if (pamConversationActive && pamConversationUser === username)
            return

        if (pamConversationActive)
            sddm.cancelAuthentication()

        pamConversationActive = true
        pamConversationUser = username
        pamResponsePending = false
        pamPendingResponse = ""
        txtMessage.text = ""
        if (listView.currentItem)
            listView.currentItem.password = ""
        sddm.beginAuthentication(username, sessionIndex)
    }

    function submitPamResponse(username, response) {
        if (!username)
            return

        if (!pamConversationActive || pamConversationUser !== username) {
            beginPamAuthentication(username)
            pamPendingResponse = response
            pamResponsePending = true
            return
        }

        sddm.respond(response)
        if (listView.currentItem)
            listView.currentItem.password = ""
    }

    TextConstants { id: textConstants }

    Connections {
        target: sddm
        function onLoginSucceeded() {
        }

        function onLoginFailed() {
            txtMessage.text = textConstants.loginFailed
            if (listView.currentItem)
                listView.currentItem.password = ""
            pamConversationActive = false
            pamConversationUser = ""
            pamResponsePending = false
            pamPendingResponse = ""
        }

        function onAuthenticationPrompt(message, promptVisible) {
            txtMessage.text = message
            if (listView.currentItem) {
                listView.currentItem.showPassword = true
                listView.currentItem.password = ""
                listView.currentItem.forceActiveFocus()
            }

            if (pamResponsePending) {
                var response = pamPendingResponse
                pamResponsePending = false
                pamPendingResponse = ""
                sddm.respond(response)
            }
        }

        function onInformationMessage(message) {
            txtMessage.text = message
        }
    }

    Background {
        anchors.fill: parent
        source: Qt.resolvedUrl(config.background)
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            var defaultBackground = Qt.resolvedUrl(config.defaultBackground)
            if (status == Image.Error && source != defaultBackground) {
                source = defaultBackground
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
                property string userName: model.name
                name: (model.realName === "") ? model.name : model.realName
                icon: model.icon
                showPassword: model.needsPassword

                focus: (listView.currentIndex === index) ? true : false
                state: (listView.currentIndex === index) ? "active" : ""

                onLogin: container.submitPamResponse(userName, password);

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        listView.currentIndex = index;
                        listView.focus = true;
                        container.beginPamAuthentication(model.name);
                    }
                }
            }
        }

        Row {
            anchors.fill: parent
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
                        source: "qrc:///theme/angle-left.png"
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

                        onCurrentItemChanged: {
                            if (currentItem)
                                container.beginPamAuthentication(currentItem.userName)
                        }

                        KeyNavigation.backtab: prevUser; KeyNavigation.tab: nextUser
                    }

                    ImageButton {
                        id: nextUser
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 10
                        source: "qrc:///theme/angle-right.png"
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

                    arrowIcon: "qrc:///theme/angle-down.png"

                    model: sessionModel
                    index: sessionModel.lastIndex
                    onIndexChanged: {
                        if (pamConversationActive)
                            sddm.setSession(index)
                    }

                    font.pixelSize: 14

                    KeyNavigation.backtab: nextUser; KeyNavigation.tab: layoutBox
                }

                Text {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter

                    visible: layoutBox.visible

                    text: textConstants.layout
                    font.pixelSize: 16
                    verticalAlignment: Text.AlignVCenter
                }

                LayoutBox {
                    id: layoutBox
                    width: 90
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 14

                    visible: keyboard.enabled && keyboard.layouts.length > 0

                    arrowIcon: "qrc:///theme/angle-down.png"

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
                    source: "qrc:///theme/reboot.png"

                    visible: sddm.canReboot

                    onClicked: sddm.reboot()

                    KeyNavigation.backtab: layoutBox; KeyNavigation.tab: btnShutdown
                }

                ImageButton {
                    id: btnShutdown
                    height: parent.height
                    source: "qrc:///theme/shutdown.png"

                    visible: sddm.canPowerOff

                    onClicked: sddm.powerOff()

                    KeyNavigation.backtab: btnReboot; KeyNavigation.tab: prevUser
                }
            }
        }
    }
}
