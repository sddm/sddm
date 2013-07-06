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
    property alias echoMode: txtMain.echoMode
    property alias text: txtMain.text
    property alias imagePath: img.source

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

    MouseArea {
        id: mouseArea
        anchors.fill: container

        hoverEnabled: true

        onEntered: if (main.state == "") main.state = "hover";
        onExited: if (main.state == "hover") main.state = "";
        onClicked: container.focus = true;
    }

    TextInput {
        id: txtMain
        width: parent.width - 16
        anchors.centerIn: parent

        color: "black"

        clip: true
        focus: true

        passwordCharacter: "\u25cf"
    }

    Image {
        id: img
        visible: keyboard.enabled && keyboard.capsLock
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        fillMode: Image.PreserveAspectFit

        smooth: true
        height: parent.height * 0.8

        sourceSize.width: width
        sourceSize.height: height

        anchors.rightMargin: 0.3 * width

        MouseArea {
            id: hoverArea

            anchors.fill: parent
            hoverEnabled: true

            onEntered: {
                rect.visible = true
            }

            onExited: {
                rect.visible = false
            }

            onPositionChanged: {
                rect.x = mouseX + rect.width
                rect.y = mouseY - rect.height
            }
        }
    }

    Rectangle {
        id: rect
        color: "lightgrey"
        anchors.centerIn: parent

        width: 100
        height: 20
        radius: 4
        visible: false

        Text {
            anchors.centerIn: parent;
            text: "Capslock is ON"
        }
    }
}
