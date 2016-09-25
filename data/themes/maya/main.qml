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

import "./components"

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

  //
  // Symmetric (equal) padding on all sides
  //
  readonly property int padSym : (spUnit / 8)

  //
  // Asymmetric padding in horizontal & vertical directions
  //
  readonly property int padAsymH : (spUnit / 2)
  readonly property int padAsymV : (spUnit / 8)

  //
  // Font sizes
  //
  readonly property int spFontNormal  : 24
  readonly property int spFontSmall   : 16


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

    Row {
      x       : (parent.width  / 2) + padAsymH
      y       : padAsymV
      width   : ((parent.width  / 2) - (padAsymH * 2))
      height  : (parent.height - (padAsymV * 2))

      spacing : padAsymH

      layoutDirection : Qt.RightToLeft

      //
      // Current date & time
      //
      SpClock {
        height  : parent.height

        tColor  : normalText

        tFont.family    : opensans_cond_light.name
        tFont.pixelSize : spFontNormal
      }
    }
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

    Row {
      x       : padAsymH
      y       : padAsymV
      width   : (parent.width  - (padAsymH * 2))
      height  : (parent.height - (padAsymV * 2))

      //
      // Welcome Text
      //
      Text {
        id      : maya_welcome

        width   : parent.width
        height  : parent.height

        text    : textConstants.welcomeText.arg(sddm.hostName)
        color   : normalText

        font.family         : opensans_cond_light.name
        font.pixelSize      : spFontNormal

        fontSizeMode        : Text.VerticalFit
        horizontalAlignment : Text.AlignLeft
        verticalAlignment   : Text.AlignVCenter
      }
    }
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

    Row {
      x : padAsymH;
      y : padAsymV;

      width   : (parent.width  - (padAsymH * 2))
      height  : (parent.height - (padAsymV * 2))

      spacing: padAsymH

      layoutDirection : Qt.RightToLeft

      //
      // Shutdown button
      //
      SpButton {
        id      : maya_shutdown

        height  : parent.height
        width   : (spUnit * 4)

        font.family : opensans_cond_light.name

        label       : textConstants.shutdown
        labelColor  : normalText

        icon        : "images/ic_power_settings_new_white_24px.svg"
        iconColor   : accentShade

        hoverIconColor  : powerColor
        hoverLabelColor : accentShade
      }

      //
      // Reboot button
      //
      SpButton {
        id      : maya_reboot

        height  : parent.height
        width   : (spUnit * 4)

        font.family : opensans_cond_light.name

        label       : textConstants.reboot
        labelColor  : normalText

        icon        : "images/ic_refresh_white_24px.svg"
        iconColor   : accentLight

        hoverIconColor  : rebootColor
        hoverLabelColor : accentShade
      }
    }
  }

  //
  // Login container
  //
  Rectangle {
    x       : (parent.width  - (6 * spUnit)) / 2
    y       : (parent.height - (5 * spUnit)) / 2
    width   : (6 * spUnit)
    height  : (5 * spUnit)

    color   : primaryHue3

    Row {
      x       : padSym
      y       : padSym
      width   : (parent.width - (padSym * 2))
      height  : (spUnit - (padSym * 2))

      Text {
        width   : parent.width
        height  : parent.height

        text    : textConstants.userName
        color   : accentLight

        font.family     : opensans_cond_light.name
        font.pixelSize  : spFontSmall

        horizontalAlignment : Text.AlignLeft
        verticalAlignment   : Text.AlignBottom
      }
    }

    Row {
      x       : padSym
      y       : spUnit + padSym
      width   : (parent.width - (padSym * 2))
      height  : (spUnit - (padSym * 2))

      TextBox {
        id      : maya_username

        width   : parent.width
        height  : parent.height

        color       : primaryHue1
        borderColor : primaryDark
        focusColor  : accentShade
        hoverColor  : accentLight
        textColor   : normalText

        font.family     : opensans_cond_light.name
        font.pixelSize  : spFontSmall
      }
    }

    Row {
      x       : padSym
      y       : (2 * spUnit) + padSym
      width   : (parent.width - (padSym * 2))
      height  : (spUnit - (padSym * 2))

      Text {
        width   : parent.width
        height  : parent.height

        text    : textConstants.password
        color   : accentLight

        font.family     : opensans_cond_light.name
        font.pixelSize  : spFontSmall

        horizontalAlignment : Text.AlignLeft
        verticalAlignment   : Text.AlignBottom
      }
    }

    Row {
      x       : padSym
      y       : (3 * spUnit) + padSym
      width   : (parent.width - (padSym * 2))
      height  : (spUnit - (padSym * 2))

      PasswordBox {
        id      : maya_password

        width   : parent.width
        height  : parent.height

        color       : primaryHue1
        borderColor : primaryDark
        focusColor  : accentShade
        hoverColor  : accentLight
        textColor   : normalText

        image       : "images/ic_warning_white_24px.svg"

        tooltipEnabled  : true
        tooltipText     : textConstants.capslockWarning
        tooltipFG       : normalText
        tooltipBG       : primaryHue3

        font.family     : opensans_cond_light.name
        font.pixelSize  : spFontNormal
      }
    }

    Row {
      x       : padSym
      y       : (4 * spUnit) + padSym
      width   : (parent.width - (padSym * 2))
      height  : (spUnit - (padSym * 2))

      Button {
        id      : maya_login

        width   : parent.width
        height  : parent.height

        text    : textConstants.login

        color         : primaryDark
        textColor     : normalText

        borderColor   : primaryHue1

        pressedColor  : accentLight
        activeColor   : accentShade

        font.family     : opensans_cond_light.name
        font.pixelSize  : spFontNormal
        font.weight     : Font.DemiBold
      }
    }
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


  Component.onCompleted: {
    if (maya_username.text === "")
      maya_username.focus = true
    else
      maya_password.focus = true
  }
}
