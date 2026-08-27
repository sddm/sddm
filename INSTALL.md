## Installation instructions

SDDM uses CMake to configure and build the project.

  1. From the project root, create a build directory: `mkdir build`
  2. From the build directory, run cmake to the project root. Eg: `cd build && cmake ..`
  3. To build, run `make`.
  4. To install, run `make install`.

CMake accepts a number of standard and extra arguments:

  - BUILD_MAN_PAGES, pass -DBUILD_MAN_PAGES=ON to build man pages
  - ENABLE_JOURNALD, pass -DENABLE_JOURNALD=OFF
    to disable logging to the journal

By default, a debug build is created. To build for production, use
`cmake -DCMAKE_BUILD_TYPE=Release`.

To see all the possible arguments, run `cmake -L ..`.
For documentation on standard CMake variables, see:
  http://www.cmake.org/cmake/help/v3.0/manual/cmake-variables.7.html

### Post-installation steps

By default, SDDM runs as its own user. An `sddm` user needs to be created, with
its home set to `/var/lib/sddm` by default.

Several PAM services need to be created to set up authentication. As distributions
have different methods for PAM service configuration, SDDM does not provide them
itself anymore.

* `sddm`: Used for user sessions with password-based authentication.
  Authentication must be password-based (e.g. `pam_unix`), the session needs
  to be set up and managed if applicable (e.g. `pam_systemd`).
* `sddm-autologin`: Used for user sessions without authentication. Like `sddm`,
  without modules asking for passwords.
* `sddm-greeter`: Used for the greeter session. Authentication must always
  succeed (`pam_permit`). Session should use modules appropriate for the
  greeter environment, which can be identical to user sessions.


### Dependencies

SDDM depends on PAM for authorization and XCB to communicate with the X server.
Apart from other things, it also depends on Qt for the user interface and event
loop management.
SDDM can optionally make use of logind (the systemd login manager API), or
ConsoleKit2, or upower to enable support for suspend, hibernate etc.
In order to build the man pages, you will need `rst2man` installed. It is
provided by the python `docutils` package

Note that SDDM makes use of C++11 features for a modern and clean codebase,
therefore it needs a recent version of GCC to compile (4.7 at least).
