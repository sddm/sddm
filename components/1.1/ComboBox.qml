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

import QtQuick 1.1

FocusScope {
    id: container
    width: 80; height: 30

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property color hoverColor: "#5692c4"
    property alias font: txtMain.font
    property alias textColor: txtMain.color
    property alias model: listView.model
    property int index: 0
    property alias arrowIcon: arrowIcon.source

    onFocusChanged: if (!container.activeFocus) close(false)

    Rectangle {
        id: main

        anchors.fill: parent

        color: container.color
        border.color: container.borderColor
        border.width: 1

        Behavior on border.color { ColorAnimation { duration: 100 } }

        states: [
            State {
                name: "hover"; when: mouseArea.containsMouse
                PropertyChanges { target: main; border.width: 1; border.color: container.hoverColor }
            },
            State {
                name: "focus"; when: container.activeFocus && !mouseArea.containsMouse
                PropertyChanges { target: main; border.width: 1; border.color: container.focusColor }
            }
        ]
    }

    Text {
        id: txtMain
        anchors.left: parent.left; anchors.right: arrow.left
        anchors.margins: 4
        height: parent.height

        clip: true
        color: "black"
        focus: true
        elide: Text.ElideRight
        text: ""

        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        id: arrow
        anchors.right: parent.right
        width: 20; height: parent.height

        border.color: main.border.color
        border.width: main.border.width

        Image {
            id: arrowIcon
            anchors.fill: parent
            clip: true
            smooth: true
            fillMode: Image.PreserveAspectFit
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: container

        hoverEnabled: true

        onEntered: if (main.state == "") main.state = "hover";
        onExited: if (main.state == "hover") main.state = "";
        onClicked: { container.focus = true; toggle() }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Up) {
            listView.decrementCurrentIndex()
        } else if (event.key === Qt.Key_Down) {
            if (event.modifiers !== Qt.AltModifier)
                listView.incrementCurrentIndex()
            else
                toggle()
        } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            close(true)
        } else if (event.key === Qt.Key_Escape) {
            close(false)
        }
    }

    Rectangle {
        id: dropDown
        width: container.width; height: 0
        anchors.top: container.bottom
        anchors.topMargin: -1

        clip: true

        Behavior on height { NumberAnimation { duration: 100 } }

        Component {
            id: myDelegate

            Rectangle {
                width: dropDown.width; height: container.height
                color: "transparent"

                property string text: model.name

                Text {
                    id: textDelegate
                    anchors.fill: parent
                    anchors.margins: 4

                    text: model.name

                    elide: Text.ElideRight
                    font: txtMain.font
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent

                    hoverEnabled: true
                    onEntered: listView.currentIndex = index
                    onClicked: close(true)
                }
            }
        }

        ListView {
            id: listView
            width: container.width; height: container.height * count
            delegate: myDelegate
            highlight: Rectangle { anchors.horizontalCenter: parent.horizontalCenter; color: container.hoverColor }
        }

        Rectangle {
            anchors.fill: listView
            color: "transparent"
            clip: false
            border.color: main.border.color
            border.width: main.border.width
        }

        states: [
            State {
                name: "visible";
                PropertyChanges { target: dropDown; height: container.height * listView.count }
            }
        ]
    }

    function toggle() {
        if (dropDown.state === "visible")
            close(false)
        else
            open()
    }

    function open() {
        dropDown.state = "visible"
        listView.currentIndex = container.index
    }

    function close(update) {
        dropDown.state = ""

        if (update) {
            container.index = listView.currentIndex
            txtMain.text = listView.currentItem.text
        }
    }

    Component.onCompleted: {
        listView.currentIndex = container.index
        txtMain.text = listView.currentItem.text
    }
}
