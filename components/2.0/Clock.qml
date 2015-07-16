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

Column {
    id: container

    property date dateTime: new Date()
    property color color: "white"
    property alias timeFont: time.font
    property alias dateFont: date.font

    Timer {
        interval: 100; running: true; repeat: true;
        onTriggered: container.dateTime = new Date()
    }

    Text {
        id: time
        anchors.horizontalCenter: parent.horizontalCenter

        color: container.color

        text : Qt.formatTime(container.dateTime, "hh:mm")

        font.pointSize: 72
    }

    Text {
        id: date
        anchors.horizontalCenter: parent.horizontalCenter

        color: container.color

        text : Qt.formatDate(container.dateTime, Qt.DefaultLocaleLongDate)

        font.pointSize: 24
    }
}
