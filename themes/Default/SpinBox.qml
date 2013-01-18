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

import QtQuick 1.1

Item {
    id: container
    width: 80; height: 30

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property alias font: textInput.font
    property alias textColor: textInput.color
    property variant items: [ "" ]
    property int index: 0

    onFocusChanged: textInput.focus = focus

    function prevItem() {
        if (index > 0)
            index--
    }

    function nextItem() {
        if (index < items.length - 1)
            index++
    }

    Rectangle {
        id: main
        anchors.fill: parent

        color: container.color
        border.color: textInput.focus ? container.focusColor : container.borderColor
        border.width: 1
    }

    TextInput {
        id: textInput
        width: parent.width - buttons.width - 16
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter

        color: "black"

        text: container.items[container.index]

        selectByMouse: false
        cursorVisible: false
        autoScroll: false
    }

    Rectangle {
        id: buttons
        width: 20; height: parent.height
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Rectangle {
            id: upArrow
            width: parent.width; height: parent.height / 2
            anchors.top: parent.top
            color: upArea.containsMouse ? (upArea.pressed ? "#ababab" : "#c3c3c3") :"#dddddd"
            border.color: main.border.color
            border.width: 1

            Image {
                anchors.fill: parent
                source: "caret-up.png"
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: upArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: container.prevItem()
            }
        }

        Rectangle {
            id: downArrow
            width: parent.width; height: parent.height / 2
            anchors.bottom: parent.bottom
            color: downArea.containsMouse ? (downArea.pressed ? "#ababab" : "#c3c3c3") :"#dddddd"
            border.color: main.border.color
            border.width: 1

            Image {
                anchors.fill: parent
                source: "caret-down.png"
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: downArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: container.nextItem()
            }
        }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Down) {
            container.nextItem()
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            container.prevItem()
            event.accepted = true
        }
    }
}
