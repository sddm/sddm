import QtQuick 2.0
import QtMultimedia 5.0

Item {
    id: container
    property string image: ""
    property string video: ""

    Image {
        anchors.fill: parent
        source: container.image
        fillMode: Image.PreserveAspectCrop
        visible: parent.video == ""
    }

    VideoOutput {
        anchors.fill: parent
        source: mediaplayer
        visible: parent.video != ""
    }

    MediaPlayer {
        id: mediaplayer
        source: container.video
        autoLoad: true
        autoPlay: true
        loops: -1
    }
}
