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

import QtQuick 2.0

Rectangle {
    id: container
    width: 80; height: 30

    property alias borderColor: main.color
    property alias textColor: textArea.color
    property alias font: textArea.font
    property alias text: textArea.text
    property alias implicitWidth: textArea.implicitWidth
    property alias implicitHeight: textArea.implicitHeight

    color: "#4682b4"
    property color disabledColor: "#888888"
    property color activeColor: "#266294"
    property color pressedColor: "#064264"

    property bool enabled: true
    property bool spaceDown: false
    property bool isFocused: activeFocus || mouseArea.containsMouse
    property bool isPressed: spaceDown || mouseArea.pressed

    signal pressed()
    signal released()
    signal clicked()

    states: [
        State {
            name: "disabled"; when: (container.enabled === false)
            PropertyChanges { target: container; color: disabledColor }
            PropertyChanges { target: main; color: disabledColor }
        },
        State {
            name: "active"; when: container.enabled && container.isFocused && !container.isPressed
            PropertyChanges { target: container; color: activeColor }
            PropertyChanges { target: main; color: activeColor }
        },
        State {
            name: "pressed"; when: container.enabled && container.isPressed
            PropertyChanges { target: container; color: pressedColor }
            PropertyChanges { target: main; color: pressedColor }
        }
    ]

    Behavior on color { NumberAnimation { duration: 200 } }

    clip: true
    smooth: true

    Rectangle {
        id: main
        width: parent.width - 2; height: parent.height - 2
        anchors.centerIn: parent

        color: parent.color
        border.color: "white"
        border.width: 1

        visible: container.isFocused
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

        anchors.fill: parent

        cursorShape: Qt.PointingHandCursor

        hoverEnabled: container.enabled
		enabled: container.enabled

        acceptedButtons: Qt.LeftButton

        onPressed: { container.focus = true; container.pressed() }
        onClicked: { container.focus = true; container.clicked() }
        onReleased: { container.focus = true; container.released() }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Space) {
            container.spaceDown = true;
            container.pressed()
            event.accepted = true
        } else if (event.key === Qt.Key_Return) {
            container.clicked()
            event.accepted = true
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Space) {
            container.spaceDown = false;
            container.released()
            container.clicked()
            event.accepted = true
        }
    }
}
