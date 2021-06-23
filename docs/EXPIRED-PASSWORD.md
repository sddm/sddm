## Introduction

SDDM can handle expired passwords during login.
A basic password dialog is provided to change the expired password.
The greeter frontend will talk with the backend to handle the pam conversation.
Support for password change is provided with two components used in greeter themes:

* ``PasswordChange.qml`` basic dialog for password change via pam conversation
* ``PamConvHelper.qml`` helper for password change dialog, handle pam conversation for themes

## Usage

For new themes the two components have to be included in the ``Main.qml``.
See the built-in greeter theme ``Main.qml`` for an example,
and add this to  ``Main.qml``:

```
PamConvHelper { dialog: passwordChange }

...

        // password change dialog
        PasswordChange {
            id: passwordChange
            // customize here e.g.:
            //anchors.top: parent.top
            //visible: false
            //color: "#22888888"
            //promptColor: "white"
            //infosColor: "lightcoral"
        }

...

        Item {
            id: usersContainer
            // block user selection during password change
            enabled: !passwordChange.visible
```

## Some more details

The following qml objects are available in ``Main.qml``,
they are used for password change and pam conversation:

### Signals

Signals coming from daemon backend:

* ``pamConvMsg(pam_msg, result)``
Provides infos/errors from (pam) backend conversation to present to user.
Results from the pam function e.g. pam_authentication are provided for evaluation.

* ``pamRequest()``
New request from (pam) backend, user response is required.

### Property

* ``request`` (type AuthRequest)
Prompts from (pam) backend with messages from pam_conv.

### Response

Responses from greeter frontend to the backend:

* ``sddm.enablePwdChange()``
The theme tells the greeter it can handle password change (has a dialog and logic as described above).
For themes which do not have a password change capabiliy, this method is not called.
In that case the pam conversation is just canceled (otherwise pam_conv user session sits there waiting for password response).
This will keep compatibility to (older) themes which do not support expired passwords yet.

* ``sddm.pamResponse(password)``
Send password response to (pam) backend, i.e. pam_conv.

* ``sddm.cancelPamConv()``
Cancel pam conversation with pam_conv.

## New sddm.conf entries

* ``LocaleFile``
File path for distro-specific locale.conf file (for pam backend) to set locale for PAM conversation
and user session.

* ``RetryLoop``
PAM retry setting for password change with pam_pwquality module is usually very low and aborts
password change dialog in greeter too early if new password is refused several times. Set entry
true to ignore PAM_MAXTRIES error from pam_chauthtok and loop password conversation.
