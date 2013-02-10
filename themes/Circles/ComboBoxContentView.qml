/***************************************************************************
* Copyright (c) 2013 Reza Fatahilah Shah <rshah0385@kireihana.com
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
import "ComboBox.js" as ComboBoxImpl

Rectangle {
    id: comboRoot
    opacity: 0
    anchors.fill: parent
    color: "#00000000"

    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property color hoverColor: "#5692c4"
    property ListModel model
    property alias contentWidth: comboBorder.width
    property int index

    signal itemClicked(int index)

    MouseArea {
        id: comboMouseArea
        anchors.fill: parent
        onClicked: {
            ComboBoxImpl.showMenu(mouse)
        }
    }

    Component.onCompleted: ComboBoxImpl.initializeMenu()

    states: State {
        name: "visible"
        PropertyChanges {
            target: comboRoot
            opacity: 1
        }
    }

    transitions: Transition {
        NumberAnimation {
            target:  comboRoot
            properties: "opacity"
            duration: 250
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: ComboBoxImpl.hideMenu()
    }

    Rectangle {
        id: comboBorder

        height: (model.count*20) + 5

        border.width: 1
        border.color: root.borderColor

        ListView {
            id: comboListView

            anchors {
                fill: parent
                margins: 2
            }

            delegate: contentDelegate
            focus: true
            clip: true
            model: comboRoot.model

            highlight: highlightBar
            highlightFollowsCurrentItem: false

            currentIndex: comboRoot.index

            Keys.onReturnPressed: {
                textInput.text = comboRoot.model.get(comboListView.currentIndex).name
                itemClicked(comboListView.currentIndex);
                ComboBoxImpl.hideMenu()
            }
        }

        Component {
            id: contentDelegate
            Item {
                id: contentContainer
                width: parent.width;
                height: 20

                Rectangle {
                    id:hoverDelegate
                    width: parent.width
                    height: 20
                    anchors.centerIn: contentContainer
                    color: hoverColor
                    visible: false
                }

                Text {
                    id: contentText
                    width: parent.width - 10
                    anchors.centerIn: contentContainer

                    text: name
                    elide: Text.ElideRight
                }

                transitions: Transition {
                    NumberAnimation {
                        properties: "x";
                        duration: 200
                    }
                }

                MouseArea {
                    hoverEnabled: true
                    anchors.fill: parent
                    onEntered: {
                        hoverDelegate.visible = true
                    }
                    onExited: {
                       hoverDelegate.visible = false
                    }
                    onClicked: {
                        comboListView.currentIndex=index
                        textInput.text = name
                        itemClicked(index);
                        ComboBoxImpl.hideMenu()
                    }
                }
            }
        }

        Component {
            id: highlightBar

            Rectangle {
                width: parent.width
                height: 20
                color: focusColor
                y: comboListView.currentItem.y;
                Behavior on y {
                    NumberAnimation {
                        duration: 200
                    }
                }
            }
        }

    }

}
