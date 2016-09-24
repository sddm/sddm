//
// [maya] main.qml
//
// Main script for the SDDM theme
//
// (c) 2016 Sanjeev Premi (spremi@ymail.com)
//
// SPDX-License-Identifier: MIT
//                          (https://spdx.org/licenses/MIT.html)
//


import QtQuick 2.0
import SddmComponents 2.0


Rectangle {

  property color primaryShade : config.primaryShade ? config.primaryShade : "#303F9F"
  property color primaryLight : config.primaryLight ? config.primaryLight : "#7986CB"
  property color primaryDark  : config.primaryDark  ? config.primaryDark  : "#1A237E"

  property color primaryHue1  : config.primaryHue1  ? config.primaryHue1  : "#5965B2"
  property color primaryHue2  : config.primaryHue2  ? config.primaryHue2  : "#303F9F"
  property color primaryHue3  : config.primaryHue3  ? config.primaryHue3  : "#2C3998"

  property color accentShade  : config.accentShade  ? config.accentShade  : "#FF4081"
  property color accentLight  : config.accentLight  ? config.accentLight  : "#FF80AB"

  property color accentHue1   : config.accentHue1   ? config.accentHue1   : "#FF669A"
  property color accentHue2   : config.accentHue2   ? config.accentHue2   : "#FF4081"
  property color accentHue3   : config.accentHue3   ? config.accentHue3   : "#E73677"

  property color normalText   : config.normalText   ? config.normalText   : "#ffffff"

  property color successText  : config.successText  ? config.successText  : "#43a047"
  property color failureText  : config.failureText  ? config.failureText  : "#e53935"
  property color warningText  : config.warningText  ? config.warningText  : "#ff8f00"

  property color rebootColor  : config.rebootColor  ? config.rebootColor  : "#fb8c00"
  property color powerColor   : config.powerColor   ? config.powerColor   : "#ff1744"

  readonly property color defaultBg : primaryShade ? primaryShade : "#1e88e5"

  //
  // Indicates one unit of measure (in pixels)
  //
  readonly property int spUnit: 64

  LayoutMirroring.enabled: Qt.locale().textDirection == Qt.RightToLeft
  LayoutMirroring.childrenInherit: true

  TextConstants { id: textConstants }

  Connections {
    target: sddm
  }

  FontLoader {
    id: opensans_cond_light
    source: "fonts/OpenSans_CondLight.ttf"
  }

  Repeater {
    model: screenModel

    Item {
      Rectangle {
        x       : geometry.x
        y       : geometry.y
        width   : geometry.width
        height  : geometry.height
        color   : defaultBg
      }
    }
  }

  //
  // Status bar on top
  //
  Rectangle {
    x       : 0
    y       : 0
    width   : parent.width
    height  : spUnit

    color   : primaryDark
  }


  //
  // Header
  //
  Rectangle {
    x       : 0
    y       : spUnit
    width   : parent.width
    height  : spUnit

    color   : primaryShade
  }

  //
  // Toolbar
  //
  Rectangle {
    x       : 0
    y       : (spUnit * 2)
    width   : parent.width
    height  : spUnit

    color   : primaryShade
  }

  //
  // Footer
  //
  Rectangle {
    x       : 0
    y       : (parent.height - spUnit)
    width   : parent.width
    height  : spUnit

    color   : primaryHue3
  }

  //
  // Login container
  //
  Rectangle {
    x       : ((parent.width / 2) - (3 * spUnit))
    y       : ((parent.height /2) - (3 * spUnit))
    width   : (6 * spUnit)
    height  : (6 * spUnit)

    color   : primaryHue3
  }

  //
  // Prompt container
  //
  Rectangle {
    x       : (parent.width / 4)
    y       : (parent.height - (3 * spUnit))
    width   : (parent.width / 2)
    height  : spUnit

    color   : "transparent"
  }
}
