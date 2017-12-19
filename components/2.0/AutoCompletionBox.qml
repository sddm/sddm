/***************************************************************************
* Copyright (c) 2017 Marco Clocchiatti <ziapannocchia@gmail.com>
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

FocusScope {
    id: root

    property bool indexIsNew: false
    property bool unameSelected: false
    property int vMargin: 35
    property int hMargin: 30
    property int sbHeight: 8
    z: 200

    width: parent.width
    height: parent.height

    Connections {
        target: sddm

        onLoginSucceeded: {
            messageColor = "steelblue"
            messageText = textConstants.loginSucceeded
        }

        onLoginFailed: {
            messageColor = "red"
            messageText = textConstants.loginFailed
        }
    }

    Component {
        id: listViewItem

        Rectangle {
            id: wrapper
            width: listV_user.height - 8
            height: listV_user.height/5
            property string tmpUsername: ListView.isCurrentItem ? contactInfo.text : ""
            property bool mouse_click
            color: (index % 2)? "#f5f5f5" : "white"
            clip: true
            focus: false

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listV_user.currentIndex = index
                    item.DelegateModel.inSelected = !item.DelegateModel.inSelected
                    contactInfo.focus = true
                    scope.focus = true
                    wrapper.mouse_click = true
                    unameSelected = true
                    username.text = completion.text
                }
            }
            Row {
                anchors {
                    left: parent.left; leftMargin: 8
                    right: parent.right; rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                Image {
                    id: userIcon
                    fillMode: Image.PreserveAspectCrop
                    width: 48
                    height: 48
                    source: loadIcons? icon : ""
                }
                Column {
                    Text {
                        id: contactInfo
                        focus: true
                        text: name
                        font { pixelSize: 16; bold: true; italic: false}
                        color: wrapper.ListView.isCurrentItem ? "black" : "gray"
                    }
                    Text {
                        width: 200
                        id: realNameField
                        text: realName
                        color: wrapper.ListView.isCurrentItem ? "gray" : "black"
                    }
                }
            }
        }
    }
    Row {
        id: listRow
        anchors {
            top: parent.top; topMargin: vMargin
            left: parent.left; leftMargin: hMargin
        }
        width:parent.width
        spacing: 0

        ListView {
            id: listV_user
            property bool scrollIndexStatus: false
            width: root.width / 3;
            height: image.height - 2 * vMargin - cbHeight
            maximumFlickVelocity: 1500
            clip: true

            model: userSortModel
            delegate: listViewItem
            spacing: 0
            onCurrentItemChanged: {
                if (currentItem) {
                    completion.text = currentItem.tmpUsername
                }
            }
            onCurrentIndexChanged: {
                scrollIndexStatus = true
                scrollIndexStatus = false
                if (currentIndex == -1) {
                    completion.text = ""
                }
                else {
                    if(username.changedText) {
                        username.changedText = false
                        currentIndex = 0
                    }
                }
                if (indexIsNew) {
                    indexIsNew = false
                }
            }
        }
        ScrollBar {
            id: scroll_listV_user
            property string themePath: localPath
            orientation: Qt.Vertical
            height: listV_user.height;
            width: sbHeight
            scrollArea: listV_user;
        }

        Column {
            id: inputFields
            width: parent.width - listV_user.width - 2 * hMargin - sbHeight
            height: listV_user.height

            Rectangle {
                id: iconRectangle
                width:parent.width
                height: width / 2
                color: "transparent"
                Image {
                    id: qtIcon
                    width:iconRectangle.width
                    height: iconRectangle.height
                    source: localPath + "images/qt.gif"
                }
            }
            Rectangle {
                id: userNameLabel
                width: parent.width;
                height: listV_user.height/10
                color: "transparent"
                Text {
                    id: userNameText
                    anchors.left: parent.left
                    anchors.leftMargin:sbHeight
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    clip: true
                    font.bold: true
                    text: "Username"
                }
            }
            Rectangle {
                id: userNameInput
                width: parent.width - hMargin
                height: listV_user.height/10
                Text {
                    id: completion
                    anchors.left: parent.left
                    anchors.leftMargin:sbHeight
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#c7c1c1"
                    clip: true
                }
                TextInput {
                    id: username
                    width: parent.width
                    anchors.left: parent.left
                    anchors.leftMargin:sbHeight
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    property bool changedText: false
                    focus: true

                    KeyNavigation.tab: password

                    onTextChanged: {
                        if(!unameSelected) {
                            userSortModel.setFilterRegExp("^"+text)
                            changedText = true
                        }
                        else {
                            unameSelected = false
                        }
                        textUserValue = text
                    }
                    Keys.onPressed: {
                        changedText = false
                        if(event.key == Qt.Key_Tab) {
                            username.text=completion.text
                        }
                        if(event.key == Qt.Key_Right) {
                            if ((listV_user.currentIndex != -1) &&
                                    (((text != completion.text) && (text != "")) ||
                                     (text == "")
                                     )
                                    ) {
                                event.accepted = true;
                                text = completion.text
                            }
                        }
                        if(event.key == Qt.Key_Up) {
                            event.accepted = true;
                            listV_user.decrementCurrentIndex()
                        }
                        if(event.key == Qt.Key_Down) {
                            event.accepted = true;
                            listV_user.incrementCurrentIndex()
                        }
                    }
                }
            }
            Rectangle {
                id: passwordLabel
                width: parent.width - hMargin
                height: listV_user.height/10
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin:sbHeight
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    font.bold: true
                    text: textConstants.password
                }
            }
            PasswordBox {
                id: password
                width: parent.width - hMargin
                height: listV_user.height/10
                textColor: focus? "black" : "grey"

                KeyNavigation.backtab: username; KeyNavigation.tab: loginButton
                Keys.onPressed: {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        sddm.login(textUserValue, password.text, session.index)
                        event.accepted = true
                        text = ""
                    }
                }
            }
            Rectangle {
                id: prompt_bg
                color: "transparent"
                width: parent.width - hMargin
                height: listV_user.height/10
                Text {
                    id      : prompt_txt
                    color   : messageColor
                    text    : messageText
                    font.pixelSize  : spFontNormal
                    horizontalAlignment : Text.AlignHCenter
                    verticalAlignment   : Text.AlignVCenter
                }
            }
        }
    }

    Row {
        id: leftBar
        anchors.top: listRow.bottom
        anchors.topMargin: cbHeight / 4
        anchors.left: listRow.left
        spacing: 4
        Column {
            id: sessionCombo
            Text {
                id: lblSession
                text: textConstants.session
                wrapMode: TextEdit.WordWrap
                font.bold: true
                font.pixelSize: 12
            }
            ComboBox {
                id: session
                property string arroColor: "transparent"
                model: sessionModel
                index: sessionModel.lastIndex
                width: 200
                color: "transparent"
                borderColor: "#ffd700"
                arrowIcon: localPath + "images/angle-down.png"
                KeyNavigation.backtab: password; KeyNavigation.tab: layoutBox
            }
        }
        Column {
            id: layoutCombo
            Text {
                id: lblLayout
                width: parent.width
                text: textConstants.layout
                wrapMode: TextEdit.WordWrap
                font.bold: true
                font.pixelSize: 12
            }
            LayoutBox {
                id: layoutBox
                color: "transparent"
                borderColor: "#ffd700"
                arrowIcon: localPath + "images/angle-down.png"

                KeyNavigation.backtab: session; KeyNavigation.tab: loginButton
            }
        }
    }

    Row { id: rightBar
        anchors.top: listRow.bottom
        anchors.topMargin: 43
        anchors.right: listRow.right
        anchors.rightMargin: 2 * (hMargin +sbHeight)
        spacing: 4
        property int btnWidth: Math.max(loginButton.implicitWidth,
                                        shutdownButton.implicitWidth,
                                        rebootButton.implicitWidth, 80) + 8
        Button {
            id: loginButton
            text: textConstants.login
            width: parent.btnWidth
            color: "#a0a0a0"
            onClicked: sddm.login(textUserValue, password.text, session.index)

            KeyNavigation.backtab: username; KeyNavigation.tab: shutdownButton
        }
        Button {
            id: shutdownButton
            text: textConstants.shutdown
            width: parent.btnWidth
            color: "#a0a0a0"
            onClicked: sddm.powerOff()

            KeyNavigation.backtab: loginButton; KeyNavigation.tab: rebootButton
        }
        Button {
            id: rebootButton
            text: textConstants.reboot
            width: parent.btnWidth
            color: "#a0a0a0"
            onClicked: sddm.reboot()

            KeyNavigation.backtab: shutdownButton; KeyNavigation.tab: username
        }
    }
}
