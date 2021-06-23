/***************************************************************************
* Copyright (c) 2017 Thomas Hoehn <thomas_hoehn@gmx.net>
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
* Note: simple password change dialog for user with expired password (pam conversation)
*
***************************************************************************/

import QtQuick 2.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

// PasswordBox, Button
import SddmComponents 2.0

FocusScope {

    id: container
    height: infosHeight+200
    width: infosWidth+120

    // access to password input
    readonly property string password: pwdInput.text

    // prompt messages from pam
    property alias prompt: promptTxt.text

    // if needed for tab navigation:
    // item outside dialog (e.g. virtual keyboard, shutdown button etc.) which gets focus with tab
    property var outsideFocusItem

    // infos text area with pam messages
    property int infosWidth: 480
    property int infosHeight: 30

    property alias radius: dialog.radius
    property alias color: dialog.color
    property int margins: 8

    property color titleColor: "transparent" // "white"
    property color titleTextColor: "white"
    property color promptColor: "black"
    property color infosColor: "red"

    // flag: warning about empty password already shown
    property bool emptyWarningShown: false

    signal ok // password input completed
    signal cancel // dialog canceled
    signal error(string msg) // invalid prompts

    TextConstants { id: textConstants }

    // when virtual keyboard is switched on and focus is set back
    // to this dialog then switch to password input field
    onFocusChanged: if(focus) pwdInput.forceActiveFocus()

    function show() {
        visible = true
        enabled = true
        pwdInput.text = ""
        pwdInput.enabled = true
        pwdInput.forceActiveFocus()
    }

    function close() {
        visible = false
        // remove focus
        enabled = false
        resetEmptyWarning()
        // clear pam infos and password
        clearAll()
    }

    // set prompt (from pam_chauthtok) and show dialog
    function newPrompt(msg) {
        if(msg != "")
        {
            promptTxt.text = msg
            show()
        } else // unlikely
            error(textConstants.emptyPrompt)
    }

    // clear all text
    function clearAll() {
       infoarea.text = ""
       pwdInput.text = ""
    }

    // append new pam error/info
    function append(msg) {
        infoarea.append(msg)
    }

    // internal

    function resetEmptyWarning() {
        emptyWarningShown = false
    }

    function validatePwd() {
        // unlikely, because PasswordBox forbids empty password
        if(pwdInput.text.length == 0)
        {
            if(emptyWarningShown==false)
            {
                infoarea.append(textConstants.emptyPassword)
                emptyWarningShown = true
            }
            return
        }

        if(pwdInput.text.length>0)
        {
            resetEmptyWarning()
            pwdInput.enabled = false
            // new password ok
            ok()
        }
    }


    Rectangle {

        id: dialog
        anchors.fill: parent

        ColumnLayout {

            anchors.fill: parent

            Rectangle {

                id: titleBar
                height: 40
                color: titleColor

                Layout.fillWidth: true
                Layout.fillHeight: false

                Label {
                    anchors.centerIn: parent
                    text: textConstants.passwordChange
                    color: titleTextColor
                    elide: Text.ElideNone
                    wrapMode: Text.NoWrap
                    font.pixelSize: 20
                }
            }

            Item {
                id: infoItem
                Layout.minimumWidth: infosWidth
                Layout.minimumHeight: infosHeight
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: margins

                // show info/error from pam_conv
                TextArea {
                    id: infoarea
                    anchors.fill: parent
                    viewport.implicitHeight: infosHeight
                    backgroundVisible: false
                    activeFocusOnPress: false
                    frameVisible: false // true
                    verticalScrollBarPolicy: Qt.ScrollBarAlwaysOff
                    textColor: infosColor
                    readOnly: true
                }
            }

            RowLayout {

                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignCenter

                // shows message (prompt) from pam_conv
                Text {
                    id: promptTxt
                    color: promptColor
                    text: qsTr("Password:")
                    elide: Text.ElideNone
                    wrapMode: Text.NoWrap
                }

                PasswordBox {
                    id: pwdInput
                    Layout.minimumWidth: 150
                    maximumLength: 64
                    KeyNavigation.down: okBtn
                    KeyNavigation.tab: okBtn
                    KeyNavigation.right: typeof outsideFocusItem !== "undefined" ? outsideFocusItem : pwdInput
                    onAccepted: validatePwd()
                }
            }

            RowLayout {
                id: btnRow
                Layout.topMargin: margins
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignCenter
                Layout.bottomMargin: 2*margins
                spacing: 2*margins

                Button {
                    id: okBtn
                    text: qsTr("Ok")
                    // disabled because in Qt 5.9 property gives error:
                    // "Button.activeFocusOnTab" is not available due to component versioning.
                    //activeFocusOnTab: true
                    enabled: pwdInput.text != ""
                    Keys.onEnterPressed: clicked()
                    Keys.onReturnPressed: clicked()
                    KeyNavigation.left: pwdInput
                    KeyNavigation.right: clearBtn
                    KeyNavigation.tab: clearBtn
                    KeyNavigation.up: pwdInput
                    onClicked: {
                        validatePwd()
                        pwdInput.forceActiveFocus()
                    }
                }

                Button {
                    id: clearBtn
                    text: qsTr("Clear")
                    //activeFocusOnTab: true
                    enabled: pwdInput.text != ""
                    Keys.onEnterPressed: clicked()
                    Keys.onReturnPressed: clicked()
                    KeyNavigation.right: cancelBtn
                    KeyNavigation.tab: cancelBtn
                    KeyNavigation.left: okBtn
                    KeyNavigation.up: pwdInput
                    onClicked: {
                        pwdInput.text = ""
                        resetEmptyWarning()
                        pwdInput.forceActiveFocus()
                    }
                }

                Button {
                    id: cancelBtn
                    text: qsTr("Cancel")
                    //activeFocusOnTab: true
                    Keys.onEnterPressed: clicked()
                    Keys.onReturnPressed: clicked()
                    KeyNavigation.left: clearBtn
                    KeyNavigation.up: pwdInput
                    KeyNavigation.tab: typeof outsideFocusItem !== "undefined" ? outsideFocusItem : pwdInput
                    onClicked: {
                        close()
                        cancel()
                    }
                }
            } // RowLayout
        } // ColumnLayout
    } // dialog
} // FocusScope
