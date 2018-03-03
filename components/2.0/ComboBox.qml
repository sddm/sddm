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

FocusScope {
    id: container
    width: 80; height: 30

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property color hoverColor: "#5692c4"
    property color menuColor: "white"
    property color textColor: "black"

    property int borderWidth: 1
    property font font
    property alias model: listView.model
    property int index: 0
    property alias arrowColor: arrow.color
    property alias arrowIcon: arrowIcon.source

    property Component rowDelegate: defaultRowDelegate

    signal valueChanged(int id)

    Component {
        id: defaultRowDelegate
        Text {
            anchors.fill: parent
            anchors.margins: 3 + container.borderWidth + (LayoutMirroring.enabled ? arrow.width : 0)
            verticalAlignment: Text.AlignVCenter
            color: container.textColor
            font: container.font

            text: parent.modelItem.name
        }
    }

    onFocusChanged: if (!container.activeFocus) close(false)

    Rectangle {
        id: main

        anchors.fill: parent

        color: container.color
        border.color: container.borderColor
        border.width: container.borderWidth

        Behavior on border.color { ColorAnimation { duration: 100 } }

        states: [
            State {
                name: "hover"; when: mouseArea.containsMouse
                PropertyChanges { target: main; border.width: container.borderWidth; border.color: container.hoverColor }
            },
            State {
                name: "focus"; when: container.activeFocus && !mouseArea.containsMouse
                PropertyChanges { target: main; border.width: container.borderWidth; border.color: container.focusColor }
            }
        ]
    }

    Loader {
        id: topRow
        anchors.fill: parent
        focus: true
        clip: true

        sourceComponent: rowDelegate
        property variant modelItem
    }

    Rectangle {
        id: arrow
        anchors.right: parent.right
        width: 20 + 2*border.width; height: parent.height

        border.color: main.border.color
        border.width: main.border.width

        Image {
            id: arrowIcon
            anchors.fill: parent
            anchors.leftMargin: parent.border.width
            anchors.rightMargin: parent.border.width
            clip: true
            smooth: true
            fillMode: Image.PreserveAspectFit
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: container

        cursorShape: Qt.PointingHandCursor

        hoverEnabled: true

        onEntered: if (main.state == "") main.state = "hover";
        onExited: if (main.state == "hover") main.state = "";
        onClicked: { container.focus = true; toggle() }
        onWheel: {
            if (wheel.angleDelta.y > 0)
                listView.decrementCurrentIndex()
            else
                listView.incrementCurrentIndex()
        }
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
        anchors.topMargin: 0

        color: container.menuColor

        clip: true

        Behavior on height { NumberAnimation { duration: 100 } }

        Component {
            id: myDelegate

            Rectangle {
                width: dropDown.width; height: container.height - 2*container.borderWidth
                color: "transparent"

                Loader {
                    id: loader
                    anchors.fill: parent
                    sourceComponent: rowDelegate

                    property variant modelItem: model
                }

                property variant modelItem: model

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent

                    cursorShape: Qt.PointingHandCursor

                    hoverEnabled: true
                    onEntered: listView.currentIndex = index
                    onClicked: close(true)
                }
            }
        }

        ListView {
            id: listView
            width: container.width; height: (container.height - 2*container.borderWidth) * count + container.borderWidth
            delegate: myDelegate
            highlight: Rectangle {
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined;
                color: container.hoverColor
            }
        }

        Rectangle {
            anchors.fill: listView
            anchors.topMargin: -container.borderWidth
            color: "transparent"
            clip: false
            border.color: main.border.color
            border.width: main.border.width
        }

        states: [
            State {
                name: "visible";
                PropertyChanges { target: dropDown; height: (container.height - 2*container.borderWidth) * listView.count + container.borderWidth}
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
            topRow.modelItem = listView.currentItem.modelItem
            valueChanged(listView.currentIndex)
        }
    }

    Component.onCompleted: {
        listView.currentIndex = container.index
        if (listView.currentItem)
            topRow.modelItem = listView.currentItem.modelItem
    }

    onIndexChanged: {
        listView.currentIndex = container.index
        if (listView.currentItem)
            topRow.modelItem = listView.currentItem.modelItem
    }

    onModelChanged: {
        listView.currentIndex = container.index
        if (listView.currentItem)
            topRow.modelItem = listView.currentItem.modelItem
    }
}
