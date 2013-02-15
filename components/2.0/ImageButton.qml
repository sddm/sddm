/***************************************************************************
* Copyright (c) 2013 Reza Fatahilah Shah <rshah0385@kireihana.com>
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

Image {
    id: container

    opacity: 0.6

    signal clicked()

    states: [
        State {
            name: "hovered"; when: mouseArea.containsMouse && !mouseArea.pressed
            PropertyChanges { target: container; opacity: 1.0 }
        },
        State {
            name: "pressed"; when: mouseArea.pressed
            PropertyChanges { target: container; opacity: 1.0 }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation { target: container; properties: "opacity"; duration: 200 }
        }
    ]

    clip: true
    smooth: true
    fillMode: Image.PreserveAspectFit

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        cursorShape: Qt.PointingHandCursor

        hoverEnabled: true

        acceptedButtons: Qt.LeftButton

        onClicked: container.clicked()
    }
}
