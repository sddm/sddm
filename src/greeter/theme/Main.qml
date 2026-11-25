/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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
***************************************************************************/

import QtQuick 2.15
import QtQuick.Window 2.15

Rectangle {
    id: root
    width: 1024
    height: 768
    color: "#182330"

    property bool submitted: false
    property var savedSession: ({})
    property int userIndex: userModel.lastIndex >= 0 && userModel.lastIndex < userModel.count
        ? userModel.lastIndex : (userModel.count > 0 ? 0 : -1)
    property int sessionIndex: sessionModel.lastIndex >= 0 && sessionModel.lastIndex < sessionModel.count
        ? sessionModel.lastIndex : (sessionModel.count > 0 ? 0 : -1)
    property string selectedUser: userModel.count > 0 && userIndex >= 0
        ? String(userList.currentItem ? userList.currentItem.userName : "")
        : manualUser.text.trim()
    readonly property bool controlling: authenticationController
    readonly property real keyboardInset: controlling && Qt.inputMethod.visible &&
            Qt.inputMethod.keyboardRectangle.height > 0
        ? Math.max(0, root.height - Qt.inputMethod.keyboardRectangle.y) : 0
    readonly property bool canSubmit: controlling && sddm.authenticationSubmissionAllowed
                                     && selectedUser.length > 0 && sessionIndex >= 0
                                     && sessionIndex < sessionModel.count

    onControllingChanged: if (controlling) password.forceActiveFocus()

    function submit() {
        if (!canSubmit || submitted) return
        submitted = true
        sddm.login(selectedUser, password.text, sessionIndex)
    }

    function selectionChanged() {
        password.text = ""
        submitted = false
        message.text = ""
    }

    function cycleUser(delta) {
        if (userModel.count <= 0) return
        userIndex = (userIndex + delta + userModel.count) % userModel.count
        selectionChanged()
    }

    function cycleSession(delta) {
        if (sessionModel.count <= 0) return
        sessionIndex = (sessionIndex + delta + sessionModel.count) % sessionModel.count
        selectionChanged()
    }

    function keepFocusVisible() {
        if (!controlling)
            return
        Qt.callLater(function() {
            var window = root.Window.window
            var item = window ? window.activeFocusItem : null
            if (!item)
                return
            var point = item.mapToItem(scroll.contentItem, 0, 0)
            var maximum = Math.max(0, scroll.contentHeight - scroll.height)
            if (point.y < scroll.contentY)
                scroll.contentY = Math.max(0, point.y - 8)
            else if (point.y + item.height > scroll.contentY + scroll.height)
                scroll.contentY = Math.min(maximum, point.y + item.height - scroll.height + 8)
        })
    }

    onWidthChanged: keepFocusVisible()
    onHeightChanged: keepFocusVisible()
    onKeyboardInsetChanged: keepFocusVisible()

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.keepFocusVisible() }
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            if (!root.submitted) return
            root.submitted = false
            password.text = ""
            message.text = qsTr("Login failed. Please try again.")
            password.forceActiveFocus()
        }
        function onInformationMessage(text) {
            if (root.controlling && sddm.authenticationSubmissionAllowed)
                message.text = text
        }
        function onAuthenticationControllerChanged() {
            if (root.controlling) password.forceActiveFocus()
        }
    }

    Connections {
        target: sessionModel
        function onModelAboutToBeReset() {
            root.savedSession = sessionModel.sessionIdentity(root.sessionIndex)
        }
        function onModelReset() {
            root.sessionIndex = root.savedSession.file !== undefined
                ? sessionModel.indexOfSession(root.savedSession.type, root.savedSession.file) : -1
        }
    }

    // Hidden ListViews resolve model roles without requiring QtQuick.Controls.
    ListView {
        id: userList
        visible: false
        width: 1
        height: 1
        model: userModel
        currentIndex: root.userIndex
        delegate: Item {
            readonly property string userName: name
        }
    }

    ListView {
        id: sessionList
        visible: false
        width: 1
        height: 1
        model: sessionModel
        currentIndex: root.sessionIndex
        delegate: Item {
            readonly property string sessionName: name
        }
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        anchors.margins: 24
        anchors.bottomMargin: anchors.margins + root.keyboardInset
        contentWidth: width
        contentHeight: Math.max(form.implicitHeight, height)
        clip: true

        Column {
            id: form
            anchors.centerIn: parent
            width: Math.min(480, scroll.width)
            spacing: 16

            Text {
                text: qsTr("Password login")
                color: "white"
                font.pixelSize: 28
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                visible: !root.controlling || sddm.authenticationBusy
                text: sddm.authenticationUser.length > 0
                    ? qsTr("Completing login for %1").arg(sddm.authenticationUser)
                    : qsTr("Use the controlling display to sign in.")
                color: "white"
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Column {
                visible: root.controlling && !sddm.authenticationBusy
                width: parent.width
                spacing: 12

                Text { text: qsTr("User"); color: "white" }

                Rectangle {
                    id: userSelector
                    visible: userModel.count > 0
                    width: parent.width
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: activeFocus ? "#6cb4ee" : "#3a506b"
                    activeFocusOnTab: true
                    KeyNavigation.tab: manualUser.visible ? manualUser : sessionSelector
                    Keys.onLeftPressed: root.cycleUser(-1)
                    Keys.onRightPressed: root.cycleUser(1)
                    Keys.onUpPressed: root.cycleUser(-1)
                    Keys.onDownPressed: root.cycleUser(1)

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 10
                        color: "white"
                        elide: Text.ElideRight
                        text: userList.currentItem ? userList.currentItem.userName : ""
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            userSelector.forceActiveFocus()
                            root.cycleUser(1)
                        }
                    }
                }

                Rectangle {
                    visible: userModel.count === 0
                    width: parent.width
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: manualUser.activeFocus ? "#6cb4ee" : "#3a506b"

                    TextInput {
                        id: manualUser
                        anchors.fill: parent
                        anchors.margins: 8
                        verticalAlignment: TextInput.AlignVCenter
                        color: "white"
                        clip: true
                        onTextEdited: root.selectionChanged()
                        KeyNavigation.tab: sessionSelector
                    }
                }

                Text { text: qsTr("Session"); color: "white" }

                Rectangle {
                    id: sessionSelector
                    width: parent.width
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: activeFocus ? "#6cb4ee" : "#3a506b"
                    activeFocusOnTab: true
                    KeyNavigation.tab: password
                    Keys.onLeftPressed: root.cycleSession(-1)
                    Keys.onRightPressed: root.cycleSession(1)
                    Keys.onUpPressed: root.cycleSession(-1)
                    Keys.onDownPressed: root.cycleSession(1)

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 10
                        color: "white"
                        elide: Text.ElideRight
                        text: sessionList.currentItem ? sessionList.currentItem.sessionName : ""
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            sessionSelector.forceActiveFocus()
                            root.cycleSession(1)
                        }
                    }
                }

                Text { text: qsTr("Password"); color: "white" }

                Rectangle {
                    width: parent.width
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: password.activeFocus ? "#6cb4ee" : "#3a506b"

                    TextInput {
                        id: password
                        anchors.fill: parent
                        anchors.margins: 8
                        verticalAlignment: TextInput.AlignVCenter
                        color: "white"
                        echoMode: TextInput.Password
                        clip: true
                        onAccepted: root.submit()
                        KeyNavigation.tab: login
                        Component.onCompleted: if (root.controlling) forceActiveFocus()
                    }
                }

                Rectangle {
                    id: login
                    width: parent.width
                    height: 40
                    radius: 4
                    readonly property bool buttonEnabled: root.canSubmit && !root.submitted
                    color: buttonEnabled ? "#2f6fed" : "#364659"
                    border.color: activeFocus ? "#9ac5ff" : "transparent"
                    activeFocusOnTab: buttonEnabled
                    KeyNavigation.tab: reboot.visible ? reboot : (powerOff.visible ? powerOff : userSelector)
                    Keys.onReturnPressed: if (buttonEnabled) root.submit()
                    Keys.onEnterPressed: if (buttonEnabled) root.submit()
                    Keys.onSpacePressed: if (buttonEnabled) root.submit()

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Log in")
                        color: login.buttonEnabled ? "white" : "#9aa8b8"
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: login.buttonEnabled
                        onClicked: root.submit()
                    }
                }
            }

            Text {
                id: message
                color: "white"
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                visible: root.controlling && !sddm.authenticationSubmissionAllowed && !sddm.authenticationBusy
                text: sddm.fingerprintDetailCode === "daemon_upgrade_required"
                    ? qsTr("The daemon must be upgraded before this greeter can log in.")
                    : qsTr("Waiting for the previous authentication to stop.")
                color: "white"
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                visible: __sddm_errors.length > 0
                text: qsTr("The configured theme could not be loaded. Password login is available after authentication cleanup.")
                color: "#ffd580"
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Row {
                visible: root.controlling
                spacing: 12

                Rectangle {
                    id: reboot
                    visible: sddm.canReboot
                    width: 110
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: activeFocus ? "#6cb4ee" : "#3a506b"
                    activeFocusOnTab: true
                    KeyNavigation.tab: powerOff.visible ? powerOff : userSelector
                    Keys.onReturnPressed: sddm.reboot()
                    Keys.onSpacePressed: sddm.reboot()
                    Text { anchors.centerIn: parent; text: qsTr("Restart"); color: "white" }
                    MouseArea { anchors.fill: parent; onClicked: sddm.reboot() }
                }

                Rectangle {
                    id: powerOff
                    visible: sddm.canPowerOff
                    width: 110
                    height: 36
                    radius: 4
                    color: "#243447"
                    border.color: activeFocus ? "#6cb4ee" : "#3a506b"
                    activeFocusOnTab: true
                    KeyNavigation.tab: userSelector.visible ? userSelector : manualUser
                    Keys.onReturnPressed: sddm.powerOff()
                    Keys.onSpacePressed: sddm.powerOff()
                    Text { anchors.centerIn: parent; text: qsTr("Power off"); color: "white" }
                    MouseArea { anchors.fill: parent; onClicked: sddm.powerOff() }
                }
            }
        }
    }
}
