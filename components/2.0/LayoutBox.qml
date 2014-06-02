/***************************************************************************
* Copyright (c) 2013 Nikita Mikhaylov <nslqqq@gmail.com>
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

ComboBox {
    id: combo

    model: keyboard.layouts
    index: keyboard.currentLayout

    onValueChanged: keyboard.currentLayout = id

    Connections {
        target: keyboard

        onCurrentLayoutChanged: combo.index = keyboard.currentLayout
    }

    rowDelegate: Rectangle {
        color: "transparent"

        Image {
            id: img
            source: "${DATA_INSTALL_DIR}/flags/%1.png".arg(modelItem ? modelItem.modelData.shortName : "zz")

            anchors.margins: 4
            fillMode: Image.PreserveAspectFit

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }

        Text {
            anchors.margins: 4
            anchors.left: img.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            verticalAlignment: Text.AlignVCenter

            text: modelItem ? modelItem.modelData.shortName : "zz"
            font.pixelSize: 14
        }
    }
}

