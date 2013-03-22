## Dependencies

SDDM depends on PAM for authorization and XCB to communicate with the X server. Also it depends on Qt for user interface and event loop management, apart from other things. SDDM can optionally make use of systemd login manager or upower to enable support for suspend, hibernate etc.

## Compilation

SDDM uses CMake to configure the build and it can be compiled with the typical cmake build process:

`mkdir build`

`cd build`

`cmake .. -DCMAKE_INSTALL_PREFIX=/usr`

`make`

After successfully building the software, type following to install.

`sudo make install`

When found, systemd will be used for power management. When systemd is not available build will fallback to using upower. If upower can not be found either, shutdown/reboot/suspend/hibernate actions won't be available. Note that, when systemd found, HaltCommand and RebootCommand config file entries has no effect.

SDDM by default uses Qt4. If you want to use Qt5 instead, simply add `-DUSE_QT5=true` at the end of the cmake line. Qt5 unlocks some additional capabilities for the themes like hardware acceleration, mouse cursors etc. 

Note that SDDM uses certain C++11 features for a modern and clean codebase, therefore needs `gcc >= 4.7` to compile.

## Configuration

After installation you can use `sddm.conf` to configure sddm. Options in the config file are mostly self explanatory, but you can also consult the sample config file, by default named as `sddm.conf.sample`, which contains comments for each invidiual option.
