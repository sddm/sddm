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

Rectangle {
    id: container
    width: 80; height: 30

    property alias borderColor: border.color
    property alias textColor: textArea.color
    property alias font: textArea.font
    property alias text: textArea.text

    property color normalColor: "#4682b4"
    property color downColor: "#266294"

    property bool mouseOver: false
    property bool mouseDown: false

    signal clicked()

    color: (mouseDown | mouseOver | focus) ? downColor : normalColor

    Rectangle {
        id: border
        width: parent.width - 3; height: parent.height - 3
        anchors.centerIn: parent

        color: parent.color
        border.color: "white"
        border.width: 1

        visible: container.focus
    }

    Text {
        id: textArea
        anchors.centerIn: parent
        color: "white"
        text: "Button"
        font.bold: true
    }

    MouseArea {
        id: mouseArea
        hoverEnabled: true
        anchors.fill: parent

        onEntered: container.mouseOver = true
        onExited: container.mouseOver = false
        onPressed: { container.mouseDown = true; container.focus = true }
        onReleased: { container.mouseDown = false }
        onClicked: { if (mouse.button === Qt.LeftButton) container.clicked() }
    }

    Keys.onPressed: {
        if ((event.key === Qt.Key_Space) || (event.key === Qt.Key_Return))  {
            container.mouseDown = true
        }
    }

    Keys.onReleased: {
        if ((event.key === Qt.Key_Space) || (event.key === Qt.Key_Return)) {
            container.mouseDown = false
            container.clicked()
        }
    }
}
