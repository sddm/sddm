## Dependencies

SDDM depends on PAM for authoriation and Xlib to communicate with the X11 server. Also it depends on Qt for user interface and event loop management, apart from other things. SDDM can optionally make use of systemd login manager or upower to enable support for suspend, hibernate etc.

## Compilation

SDDM uses CMake to configure the build and it can be compiled with the typical cmake build process:

`mkdir build`

`cd build`

`cmake .. -DCMAKE_INSTALL_PREFIX=/usr`

`make`

After successfully building the software, type following to install.

`sudo make install`

By default configure script searches for systemd. When found, it will be used for power management. If not, the script tries to use upower. If that can not be found either, power management actions will not be available. Note that, if systemd is used, HaltCommand and RebootCommand config file entries will have no effect.

SDDM by default uses Qt4. If you want to use Qt5 instead, simply add `-DUSE_QT5=true` at the end of the cmake line. Qt5 unlocks some additional capabilities for the themes like hardware acceleration, mouse cursors etc.

## Configuration

After installation you can use `sddm.conf` to configure sddm. Options in the config file are mostly self explanatory, but you can also consult the sample config file, by default named as `sddm.conf.sample`, which contains comments for each invidiual option.
