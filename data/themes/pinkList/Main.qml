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
import SddmComponents 2.0

Rectangle {
    id: container
    //color: "transparent"
    color: "Violet"
    //width: 640
    //height: 480

    LayoutMirroring.enabled: Qt.locale().textDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    TextConstants { id: textConstants }

/*
    Background {
        anchors.fill: parent
        source: "images/" + config.background
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            if (status == Image.Error && source != config.defaultBackground) {
                source = config.defaultBackground
            }
        }
    }
*/

    Column {
        anchors.centerIn: parent
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "black"
            verticalAlignment: Text.AlignVCenter
            text: textConstants.welcomeText.arg(sddm.hostName)
            wrapMode: Text.WordWrap
            font.pixelSize: 24
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            id: listBox
            //visible: primaryScreen
            property int cbHeight: 100
            width: Math.max(650, container.implicitWidth + 50)
            height: 2 * cbHeight + width * 2 / 3
            color: "transparent"
            AutoCompletionBox {
                id: auBox
                property string messageColor
                property string messageText:textConstants.prompt
                property string textUserValue
                property string textUserPassword
                property int cbHeight: parent.cbHeight
                property bool loadIcons:true
                property string localPath: loadIcons ? Qt.resolvedUrl("./") : ""
            }
            Image {
                id: image
                anchors.top: parent.top
                width: parent.width
                height: parent.height
                source: "images/rectangle.png"
            }
            Rectangle {
                id: clockRectangle
                anchors.left: image.right
                color: "transparent"

                Clock {
                    id: clock
                    color: "white"
                    timeFont.family: "Oxygen"
                    timeFont.pixelSize: 24
                    dateFont.pixelSize: 24
                }

            }
        }
    }

    Component.onCompleted: {
        if (auBox.textUserValue == "")
            auBox.focus = true
        else
            password.focus = true
    }
}
