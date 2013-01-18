import QtQuick 1.1

Item {
    id: container
    width: 80; height: 30

    property color color: "white"
    property color borderColor: "#ababab"
    property color focusColor: "#266294"
    property alias font: textInput.font
    property alias textColor: textInput.color
    property alias echoMode: textInput.echoMode
    property alias text: textInput.text

    onFocusChanged: textInput.focus = focus

    Rectangle {
        id: border
        anchors.fill: parent

        color: container.color
        border.color: textInput.focus ? container.focusColor : container.borderColor
        border.width: 1
    }

    TextInput {
        id: textInput
        width: parent.width - 16
        anchors.centerIn: parent

        color: "black"

        passwordCharacter: "\u25cf"
    }
}
