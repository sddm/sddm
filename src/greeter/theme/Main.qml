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

    readonly property bool conversationAvailable: typeof sddm.authenticationConversationVersion !== "undefined" && sddm.authenticationConversationVersion === 1
    property var activeAttempt: 0
    property var activePrompt: 0
    property bool awaitingResponse: false
    property bool responseVisible: false
    property bool cancelling: false
    property bool authorized: false
    property string promptText: ""
    property var notices: []
    property string conversationStatus: ""

    function clearResponse() {
        response.text = ""
        awaitingResponse = false
        activePrompt = 0
        promptText = ""
    }

    function startAuthentication() {
        if (!conversationAvailable || activeAttempt || authorized) return
        clearResponse()
        notices = []
        cancelling = false
        conversationStatus = ""
        activeAttempt = sddm.beginAuthentication(username.text.trim(), sessionIndex)
        if (!activeAttempt) conversationStatus = qsTr("Unable to start authentication.")
    }

    function sendResponse() {
        if (!activeAttempt || !activePrompt || !awaitingResponse || cancelling) return
        var reply = response.text
        var prompt = activePrompt
        clearResponse()
        sddm.respond(activeAttempt, prompt, reply)
        reply = ""
    }

    function cancelAuthentication() {
        if (!activeAttempt || authorized || cancelling) return
        cancelling = true
        clearResponse()
        notices = []
        conversationStatus = qsTr("Cancelling authentication…")
        sddm.cancelAuthentication(activeAttempt)
    }

    function receivePrompt(attempt, prompt, message, visible) {
        if (attempt !== activeAttempt || !activeAttempt || cancelling) return
        clearResponse()
        activePrompt = prompt
        promptText = message
        responseVisible = visible
        awaitingResponse = true
        response.forceActiveFocus()
    }

    function receiveMessage(attempt, message, error) {
        if (attempt !== activeAttempt || !activeAttempt || cancelling) return
        var next = notices.slice()
        if (next.length && next[next.length - 1] === message) return
        // Keep initial instructions (including number matching) and latest status.
        if (next.length < 16) next.push(message)
        else next[15] = message
        notices = next
    }

    function authenticationFinished(attempt, result) {
        if (attempt !== activeAttempt || !activeAttempt) return
        clearResponse()
        notices = []
        activeAttempt = 0
        cancelling = false
        authorized = result === 2
        conversationStatus = result === 2 ? qsTr("Starting session…")
            : result === 1 ? qsTr("Authentication cancelled.") : textConstants.loginFailed
    }

    TextConstants { id: textConstants }

    Connections {
        target: sddm
        ignoreUnknownSignals: true
        function onAuthenticationPrompt(attempt, prompt, message, visible) { container.receivePrompt(attempt, prompt, message, visible) }
        function onAuthenticationMessage(attempt, message, error) { container.receiveMessage(attempt, message, error) }
        function onAuthenticationFinished(attempt, result) { container.authenticationFinished(attempt, result) }
        function onAuthenticationAuthorized(attempt) {
            if (attempt === container.activeAttempt) {
                container.authorized = true
                container.cancelling = false
            }
        }
        function onSocketDisconnected() {
            container.clearResponse()
            container.notices = []
            container.activeAttempt = 0
            container.authorized = false
            container.conversationStatus = qsTr("Disconnected from the display manager.")
        }
        function onLoginSucceeded() {
        }

        function onLoginFailed() {
            txtMessage.text = textConstants.loginFailed
            if (listView.currentItem) listView.currentItem.password = ""
        }

        function onInformationMessage(message) {
            txtMessage.text = message
        }
    }

    Background {
        anchors.fill: parent
        source: "qrc:///theme/background.png"
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
                    visible: !container.conversationAvailable
                    enabled: visible
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

                Column {
                    id: conversationPanel
                    visible: container.conversationAvailable
                    anchors.centerIn: parent
                    width: parent.width - 60
                    spacing: 10

                    Text { text: qsTr("Username"); color: "white"; textFormat: Text.PlainText }
                    Rectangle {
                        width: parent.width; height: 34; color: "white"; border.color: "#ababab"
                        TextInput {
                            id: username
                            anchors.fill: parent; anchors.margins: 6
                            text: userModel.lastUser
                            enabled: !container.activeAttempt && !container.authorized
                            maximumLength: 256
                            selectByMouse: true
                            font.pixelSize: 16
                            Keys.onReturnPressed: container.startAuthentication()
                            Keys.onEnterPressed: container.startAuthentication()
                        }
                    }
                    Flickable {
                        width: parent.width
                        height: Math.min(110, noticeText.implicitHeight)
                        contentHeight: noticeText.implicitHeight
                        clip: true
                        Text {
                            id: noticeText
                            width: parent.width
                            text: container.notices.join("\n")
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: "white"
                            font.pixelSize: 18
                        }
                    }
                    Flickable {
                        width: parent.width
                        height: Math.min(90, promptLabel.implicitHeight)
                        contentHeight: promptLabel.implicitHeight
                        clip: true
                        Text {
                            id: promptLabel
                            width: parent.width
                            text: container.promptText
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: "white"
                            font.pixelSize: 18
                        }
                    }
                    Rectangle {
                        width: parent.width; height: 34; color: "white"; border.color: "#ababab"
                        visible: container.awaitingResponse
                        TextInput {
                            id: response
                            anchors.fill: parent; anchors.margins: 6
                            enabled: container.awaitingResponse && !container.cancelling
                            maximumLength: 4096
                            echoMode: container.responseVisible ? TextInput.Normal : TextInput.Password
                            selectByMouse: true
                            font.pixelSize: 16
                            Keys.onReturnPressed: container.sendResponse()
                            Keys.onEnterPressed: container.sendResponse()
                            Keys.onEscapePressed: container.cancelAuthentication()
                        }
                    }
                    Text {
                        width: parent.width
                        text: container.conversationStatus
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: "white"
                    }
                    Row {
                        spacing: 10
                        Button {
                            width: 110; height: 32
                            text: container.activeAttempt ? qsTr("Continue") : qsTr("Sign in")
                            enabled: !container.cancelling && (container.activeAttempt ? container.awaitingResponse : !container.authorized)
                            onClicked: container.activeAttempt ? container.sendResponse() : container.startAuthentication()
                        }
                        Button {
                            width: 110; height: 32
                            text: qsTr("Cancel")
                            enabled: container.activeAttempt && !container.cancelling && !container.authorized
                            onClicked: container.cancelAuthentication()
                        }
                    }
                }

                Text {
                    id: txtMessage
                    visible: !container.conversationAvailable
                    textFormat: Text.PlainText
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
                    textFormat: Text.PlainText
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
                    enabled: !container.activeAttempt && !container.authorized
                    width: 245
                    anchors.verticalCenter: parent.verticalCenter

                    arrowIcon: "qrc:///theme/angle-down.png"

                    model: sessionModel
                    index: sessionModel.lastIndex

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
