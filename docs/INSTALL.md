## Dependencies

SDDM depends on PAM for authorization and XCB to communicate with the X server. Apart from other things, it also depends on Qt for the user interface and event loop management. SDDM can optionally make use of systemd's login manager (logind) or upower to enable support for suspend, hibernate etc.

## Compilation

SDDM uses CMake to configure the build and it can be compiled with the typical cmake build process:

`mkdir build`

`cd build`

`cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local`

`make`

After successfully building the software, install it:

`sudo make install`

If systemd is found during the build, it will be used for power management. Otherwise, the build will fallback to upower. If upower can't be found either, shutdown/reboot/suspend/hibernate actions won't be available.
Note that, when systemd is used, the HaltCommand and RebootCommand config file entries have no effect.

SDDM by default uses Qt4. If you want to use Qt5 instead, simply add `-DUSE_QT5=true` at the end of the cmake line. Qt5 unlocks some additional capabilities for the themes like hardware acceleration, mouse cursors etc.

Note that SDDM uses certain C++11 features for a modern and clean codebase, therefore it needs `gcc >= 4.7` to compile.

## Configuration

After installation, the file `sddm.conf` can be used to configure sddm. Options in the config file are mostly self explanatory, but the sample config file, by default named `sddm.conf.sample` can also be used as a reference as it contains comments for each individual option.
