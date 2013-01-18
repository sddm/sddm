import QtQuick 1.1

Item {
    id: container
    width: 80; height: 30

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property alias font: textInput.font
    property alias textColor: textInput.color
    property variant items: [ "" ]
    property int index: 0

    onFocusChanged: textInput.focus = focus

    function prevItem() {
        if (index > 0)
            index--
    }

    function nextItem() {
        if (index < items.length - 1)
            index++
    }

    Rectangle {
        id: main
        anchors.fill: parent

        color: container.color
        border.color: textInput.focus ? container.focusColor : container.borderColor
        border.width: 1
    }

    TextInput {
        id: textInput
        width: parent.width - buttons.width - 16
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter

        color: "black"

        text: container.items[container.index]

        selectByMouse: false
        cursorVisible: false
        autoScroll: false
    }

    Rectangle {
        id: buttons
        width: 20; height: parent.height
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Rectangle {
            id: upArrow
            width: parent.width; height: parent.height / 2
            anchors.top: parent.top
            color: upArea.containsMouse ? (upArea.pressed ? "#ababab" : "#c3c3c3") :"#dddddd"
            border.color: main.border.color
            border.width: 1

            Image {
                anchors.fill: parent
                source: "caret-up.png"
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: upArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: container.prevItem()
            }
        }

        Rectangle {
            id: downArrow
            width: parent.width; height: parent.height / 2
            anchors.bottom: parent.bottom
            color: downArea.containsMouse ? (downArea.pressed ? "#ababab" : "#c3c3c3") :"#dddddd"
            border.color: main.border.color
            border.width: 1

            Image {
                anchors.fill: parent
                source: "caret-down.png"
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: downArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: container.nextItem()
            }
        }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Down) {
            container.nextItem()
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            container.prevItem()
            event.accepted = true
        }
    }
}
