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
import QtQuick 1.1

Rectangle {
    id: root

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property color hoverColor: "#5692c4"
    property alias font: textInput.font
    property alias textColor: textInput.color
    property alias model: comboboxContent.model
    property int index

    signal itemClicked(int index)

    width: 80;
    height: 25


    Rectangle {
        id: main

        property bool hover: false

        anchors.fill: parent

        color: root.color
        border.color: hover ? root.hoverColor : (root.activeFocus ? root.focusColor : root.borderColor)
        border.width: 1

        Behavior on border.color { ColorAnimation { duration: 200 } }

        MouseArea {
            hoverEnabled: true
            anchors.fill: parent

            onEntered: parent.hover = true
            onExited: parent.hover = false
        }
    }

    TextInput {
        id: textInput
        width: parent.width - downArrow.width - 16
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter

        clip: true
        color: "black"
        focus: true
        readOnly: true

        selectByMouse: false
        cursorVisible: false
        autoScroll: false
    }

    Rectangle {
        id: downArrow
        width: 20; height: parent.height
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        color: downArea.containsMouse ? (downArea.pressed ? "#ababab" : "#c3c3c3") :"#dddddd"
        border.color: main.border.color
        border.width: 1

        Image {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 2
            source: "chevron-down.png"
            scale: 0.5
        }

        MouseArea {
            id: downArea
            anchors.fill: parent
            hoverEnabled: true
        }
    }

    ComboBoxContentView {
        id: comboboxContent
        contentWidth: main.width
        index: root.index

        Component.onCompleted: {
            textInput.text= root.model.get(index).name
        }

        onItemClicked: {
            root.index = index
            root.itemClicked(index)
        }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Down) {
            root.nextItem()
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            root.prevItem()
            event.accepted = true
        }
    }

    function prevItem() {
        if (index > 0)
            index--

        textInput.text= root.model.get(index).name
        comboboxContent.index=index
    }

    function nextItem() {
        if (index < root.model.count - 1)
            index++

        textInput.text= root.model.get(index).name
        comboboxContent.index=index

    }
}
