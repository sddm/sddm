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

Rectangle {
    id: menu

    width: 200; height: 0

    property int itemHeight: 30
    property alias model: menuList.model
    property alias index: menuList.currentIndex

    Behavior on height { NumberAnimation { duration: 100 } }

    states: [
        State {
            name: "visible";
            PropertyChanges { target: menu; height: itemHeight * menuList.count }
        }
    ]

    Component {
        id: listViewItem

        Rectangle {
            color: mouseArea.containsMouse ? "steelblue" : "transparent"
            width: parent.width; height: menu.itemHeight

            Text {
                anchors.fill: parent
                anchors.margins: 4

                text: model.name
                elide: Text.ElideRight

                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent

                cursorShape: Qt.PointingHandCursor

                hoverEnabled: true

                onClicked: { menuList.currentIndex = index; menu.state = "" }
            }

            Keys.onReturnPressed: { itemClicked(index); menu.state = "" }
        }
    }

    ListView {
        id: menuList

        anchors.fill: parent

        clip: true

        delegate: listViewItem
        highlight: Rectangle { color: "lightsteelblue" }
    }
}

