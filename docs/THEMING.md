## Themes

SDDM themes are created using the QtQuick framework, a declarative framework to develop next-generation, hardware-accelerated user interfaces with fluid animations. QtQuick offers some basic components.

On top of QtQuick, we provide some custom components to make theme development even easier. For example a picturebox which can show user avatars. Most of the components can be used as views in a model-view sense.

We also provide models containing information about the screens, available sessions and users. Connect these with the provided components and to have a fully working solution. For example, below is the whole _code_ needed to create a session selection combobox:

    ComboBox {
    	id: session
    	arrowIcon: Qt.resolvedPath("angle-down.png")
    	model: sessionModel
    	index: sessionModel.lastIndex
    }

Themes are run by a Qt 5 built sddm-greeter, unless the `QtVersion` property in metadata.desktop specifies a different version, such as `QtVersion=6` for using `sddm-greeter-qt6`.

## Proxy Object

We provide a proxy object, called as `sddm` to the themes as a context property. This object holds some useful properties about the host system. It also acts as a proxy between the greeter and the daemon. All of the methods called on this object will be transferred to the daemon through a local socket to be executed there.

### Properties

**hostName:** Holds the name of the host computer.

**canPowerOff:** true, if we can power off the machine; false, otherwise

**canReboot:** true, if we can reboot the machine; false, otherwise

**canSuspend:** true, if the machine supports suspending to the memory; false, otherwise

**canHibernate:** true, if the machine supports hibernating, e.g suspending to the disk; false, otherwise

**canHybridSleep:** true, if the machine supports hybrid sleep, e.g suspending to both memory and disk; false, otherwise

### Methods

**powerOff():** Powers of the machine.

**reboot():** Reboots the machine.

**suspend():** Suspends the machine to the memory.

**hibernate():** Suspends the machine to the disk.

**hybridSleep():** Suspends the machine both to the memory and the disk.

**login(user, password, sessionIndex):** Attempts to login as the `user`, using the `password` into the session pointed by the `sessionIndex`. Either the `loginFailed` or the `loginSucceeded` signal will be emitted depending on whether the operation is successful or not.

### Signals

**loginFailed():** Emitted when a requested login operation fails.

**loginSucceeded():** Emitted when a requested login operation succeeds.

## Data Models
Besides the proxy object we offer a few models that can be hooked to the views to handle multiple screens or enable selection of users or sessions.

**screenModel:** This is a list model containing geometry information of the screens available. This model only provides logical screen numbers and geometries. If you have two physical monitors, but configured to be duplicates we only report one screen.

For each screen the model provides `name` and `geometry` properties.
The model also provides, a `primary` property pointing to the index of the primary monitor and a `geometry` method which takes a monitor index and returns the geometry of it. If you pass `-1` to the `geometry` method it will return the united geometry of all the screens available.

**sessionModel:** This is a list model which contains information about the desktop sessions installed on the system. This information is gathered by parsing the desktop files in the `/usr/share/xsessions` directory. These desktop files are generally installed when you install a desktop environment or a window manager.

For each session, the model provides `file`, `name`, `exec` and `comment` properties.
Also there is a `lastIndex` property, pointing to the last session the user successfully logged in.

**userModel:** This is list model. Contains information about the users available on the system. This information is gathered by reading the user database provided by `getpwent()`. To prevent system users polluting the user model we only show users with user ids greater than a certain threshold. This threshold is adjustable through the config file and called `MinimumUid`.

For each user the model provides `name`, `realName`, `homeDir` and `icon` properties.
This model also has a `lastIndex` property holding the index of the last user successfully logged in, and a `lastUser` property containing the name of the last user successfully logged in.

## Testing

You can test your themes using `sddm-greeter`. Note that in this mode, actions like shutdown, suspend or login will have no effect.

    sddm-greeter --test --theme /path/to/your/theme

If you have compiled SDDM with Qt4, you can also use it in a nested X session through Xephyr. To accomplish this use:

    sddm --test-mode

When using Qt5, test-mode requires [at least xorg-server 1.15.0](https://bugs.freedesktop.org/show_bug.cgi?id=62346#c8), as older releases don't support GLX in Xephyr which is required by QtQuick2.

### Interactive PAM authentication

Themes can opt into interactive authentication when
`sddm.authenticationConversationVersion === 1`. The property is negotiated with
the daemon; it is zero before negotiation and with older daemons. Do not start
an interactive attempt until version 1 is available. The bundled theme retains
its legacy password interface when this capability is unavailable.

`beginAuthentication(user, sessionIndex)` returns a nonzero attempt identifier,
or zero if local validation or capability checks prevent the request. The
daemon rejects a busy request asynchronously with `authenticationFinished` and
result 0. Retain the current attempt until its final result rather than starting
another one. Bind the selected user and session to that attempt. Trusted PAM modules may canonicalize
the supplied username during normal authentication.

The proxy emits these signals:

* `authenticationPrompt(attempt, prompt, message, responseVisible)` requests one
  fresh response. Render the literal message using `Text.PlainText`; use visible
  input only when `responseVisible` is true.
* `authenticationMessage(attempt, message, error)` carries a literal informational
  or error notice. An error notice is not necessarily a terminal failure: PAM
  may continue with another prompt or retry. Keep relevant instructions visible
  while waiting, including authenticator number-matching information.
* `authenticationAuthorized(attempt)` marks the session-authorization cutoff.
  Cancellation can no longer prevent a session from starting. PAM session setup
  may still request further responses.
* `authenticationFinished(attempt, result)` ends the attempt. Results are 0 for
  failure, 1 for cancellation and 2 for successful session login.

Call `respond(attempt, prompt, response)` once for the current prompt. Clear the
input field before sending and on every new prompt, cancellation and completion.
Never reuse a password or other response for a subsequent prompt. Ignore signals
whose attempt identifier does not match the current attempt. Do not log response
or message contents, interpret PAM text as rich text, or launch links from it.

Call `cancelAuthentication(attempt)` before authorization to cancel. Keep the
attempt active in the UI until its final result arrives; the daemon invalidates
it immediately and asynchronously terminates the helper, forcibly after five
seconds if necessary. A new attempt is refused until the old helper exits.
Socket disconnection clears the UI's outstanding response and attempt.

The existing `login(user, password, sessionIndex)` API remains available for
legacy themes. Its supplied password can answer one hidden prompt only. A
subsequent prompt fails authentication instead of reusing that password. Normal
PAM account checks and session setup remain required. Interactive authentication
does not add an explicit password/PIN reset API.
