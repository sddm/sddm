import QtQuick 1.1

Rectangle {
    id: container
    width: 80; height: 30

    property alias borderColor: border.color
    property alias textColor: textArea.color
    property alias font: textArea.font
    property alias text: textArea.text

    property color normalColor: "#4682b4"
    property color downColor: "#266294"

    property bool mouseOver: false
    property bool mouseDown: false

    signal clicked()

    color: (mouseDown | mouseOver) ? downColor : normalColor

    Rectangle {
        id: border
        width: parent.width - 3; height: parent.height - 3
        anchors.centerIn: parent

        color: parent.color
        border.color: "white"
        border.width: 1

        visible: container.focus
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
        hoverEnabled: true
        anchors.fill: parent

        onEntered: container.mouseOver = true
        onExited: container.mouseOver = false
        onPressed: { container.mouseDown = true; container.focus = true }
        onReleased: { container.mouseDown = false }
        onClicked: { if (mouse.button === Qt.LeftButton) container.clicked() }
    }

    Keys.onPressed: {
        if ((event.key === Qt.Key_Space) || (event.key === Qt.Key_Return))  {
            container.mouseDown = true
        }
    }

    Keys.onReleased: {
        if ((event.key === Qt.Key_Space) || (event.key === Qt.Key_Return)) {
            container.mouseDown = false
            container.clicked()
        }
    }
}
