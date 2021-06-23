/***************************************************************************
* Copyright (c) 2018 Thomas Hoehn <thomas_hoehn@gmx.net>
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
* Note: helper for password change dialog, hide signal handling for pam conversation from theme
*
***************************************************************************/

import QtQuick 2.0
import SddmComponents 2.0
//import PamTypes 1.0

Item {

    property Loader loader
    property Component component

    property string msgBuffer: ""

    // tell greeter we handle password change (password expired),
    // default  in old themes is no password change
    Component.onCompleted: sddm.enablePwdChange()

    // handles password change events
    Connections {

        target: loader.item

        onCancel: sddm.cancelPamConv()

        onOk: sddm.pamResponse(loader.item.password)

    }

    Connections {

        target: sddm

        // unload dialog
        onLoginFailed: loader.sourceComponent = undefined

        // show messages from pam conversation
        onPamConvMsg:
            // collect pam messages arriving
            // before a pam request
            if(loader.status != Loader.Ready) {
                if(msgBuffer === "")
                    msgBuffer += pam_msg
                else
                    msgBuffer += '\n' + pam_msg
            } else {
                // from signal pamConvMsg(pam_msg, result)
                loader.item.append(pam_msg)
                /*
                // hint for user why current password is asked again
                if(result == PamTypes.RESULT_PAM_MAXTRIES)
                    loader.item.append(textConstants.pamMaxtriesInfo)
                */
            }

        // new prompt arrived from pam_chauthtok,
        // e.g. for password change, prompt for current or new password
        onPamRequest: {
            loader.sourceComponent = component

            // show pam errors and infos which arrived before request
            if(msgBuffer !== "")
                loader.item.append(msgBuffer)
            msgBuffer = ""

            // open password change dialog and block other GUI
            // NOTE: only one prompt per request supported!
            loader.item.newPrompt(request.findChangePwdMessage() /* prompt message */)
        }
    }
}
