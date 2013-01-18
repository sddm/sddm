import QtQuick 1.1

Rectangle {
    width: 640
    height: 480

    Connections {
        target: sessionManager

        onSuccess: {
            errorMessage.color = "steelblue"
            errorMessage.text = qsTr("Login succeeded.")
        }

        onFail: {
            errorMessage.color = "red"
            errorMessage.text = qsTr("Login failed.")
        }
    }
    
    Image {
        anchors.centerIn: parent
        source: "background.png"
        fillMode: Image.Tile
    }
    
    Rectangle {
        anchors.centerIn: parent
        width: 320; height: 320
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#c3c3c3" }
            GradientStop { position: 0.5; color: "#e5e5e5" }
            GradientStop { position: 1.0; color: "#c3c3c3" }
        }
        border.color: "#ababab"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 12
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: "black"
                text: qsTr("Welcome to ") + sessionManager.hostName
                font.pixelSize: 24
            }

            Column {
                width: parent.width
                spacing: 4
                Text {
                    id: lblName
                    width: 60
                    text: qsTr("User name")
                    font.bold: true
                    font.pixelSize: 12
                }

                TextBox {
                    id: name
                    width: parent.width; height: 30
                    text: sessionManager.lastUser
                    font.pixelSize: 14

                    KeyNavigation.backtab: rebootButton; KeyNavigation.tab: password

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return) {
                            sessionManager.login(name.text, password.text, session.index)
                            event.accepted = true
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing : 4
                Text {
                    id: lblPassword
                    width: 60
                    text: qsTr("Password")
                    font.bold: true
                    font.pixelSize: 12
                }

                TextBox {
                    id: password
                    width: parent.width; height: 30
                    font.pixelSize: 14

                    echoMode: TextInput.Password

                    KeyNavigation.backtab: name; KeyNavigation.tab: session

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return) {
                            sessionManager.login(name.text, password.text, session.index)
                            event.accepted = true
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing : 4
                Text {
                    id: lblSession
                    width: 60
                    text: qsTr("Session")
                    font.bold: true
                    font.pixelSize: 12
                }

                SpinBox {
                    id: session
                    width: parent.width; height: 30
                    font.pixelSize: 14

                    items: sessionManager.sessions
                    index: sessionManager.lastSession

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return) {
                            sessionManager.login(name.text, password.text, session.index)
                            event.accepted = true
                        }
                    }

                    KeyNavigation.backtab: password; KeyNavigation.tab: loginButton
                }
            }

            Column {
                width: parent.width
                Text {
                    id: errorMessage
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Enter your user name password.")
                    font.pixelSize: 10
                }
            }

            Row {
                spacing: 4
                anchors.horizontalCenter: parent.horizontalCenter
                Button {
                    id: loginButton
                    text: qsTr("Login")

                    onClicked: sessionManager.login(name.text, password.text, session.index)

                    KeyNavigation.backtab: session; KeyNavigation.tab: shutdownButton
                }

                Button {
                    id: shutdownButton
                    text: qsTr("Shutdown")

                    onClicked: sessionManager.shutdown()

                    KeyNavigation.backtab: loginButton; KeyNavigation.tab: rebootButton
                }

                Button {
                    id: rebootButton
                    text: qsTr("Reboot")

                    onClicked: sessionManager.reboot()

                    KeyNavigation.backtab: shutdownButton; KeyNavigation.tab: name
                }
            }
        }
    }


    Component.onCompleted: {
        name.focus = true
    }
}
