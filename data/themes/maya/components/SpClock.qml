//
// [maya] components/SpClock.qml
//
// Implements custom clock component
//
// (c) 2016 Sanjeev Premi (spremi@ymail.com)
//
// SPDX-License-Identifier: MIT
//                          (https://spdx.org/licenses/MIT.html)
//


import QtQuick 2.0


Item {
  id  : sp_clock

  property date value   : new Date()

  property color tColor : "white"
  property alias tFont  : sp_clock_text.font

  implicitWidth   : sp_clock_text.implicitWidth
  implicitHeight  : sp_clock_text.implicitHeight


  Timer {
    interval    : 100
    running     : true
    repeat      : true;
    onTriggered : sp_clock.value = new Date()
  }

  Text {
    id    : sp_clock_text

    text  : Qt.formatDateTime(sp_clock.value, "dddd, dd MMMM yyyy  HH:mm AP")

    color : sp_clock.tColor

    font.pixelSize      : 24

    fontSizeMode        : Text.VerticalFit

    horizontalAlignment : Text.AlignHCenter
    verticalAlignment   : Text.AlignVCenter
  }
}
