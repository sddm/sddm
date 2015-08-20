## INTRODUCTION

[![IRC Network](https://img.shields.io/badge/irc-freenode-blue.svg "IRC Freenode")](https://webchat.freenode.net/?channels=sddm)
[![GitHub release](https://img.shields.io/github/release/sddm/sddm.svg)](https://github.com/sddm/sddm)
[![GitHub issues](https://img.shields.io/github/issues/sddm/sddm.svg)](https://github.com/sddm/sddm/issues)
[![Build Status](https://travis-ci.org/sddm/sddm.svg?branch=master)](https://travis-ci.org/sddm/sddm)

SDDM is a modern display manager for X11 and Wayland aiming to be fast, simple and beautiful.
It uses modern technologies like QtQuick, which in turn gives the designer the ability to
create smooth, animated user interfaces.

SDDM is extremely themeable. We put no restrictions on the user interface design,
it is completely up to the designer. We simply provide a few callbacks to the user interface
which can be used for authentication, suspend etc.

To further ease theme creation we provide some premade components like a textbox,
a combox etc.

There are a few sample themes distributed with SDDM.
They can be used as a starting point for new themes.

## LICENSING

Source code of SDDM is licensed under GNU GPL version 2 or later (at your opinion).

QML files are MIT licensed and images are CC BY 3.0.

Most of the files include a license header.

## RESOURCES

https://github.com/sddm/sddm

https://github.com/sddm/sddm/issues

https://github.com/sddm/sddm/wiki

https://groups.google.com/group/sddm-devel

**sddm** on **irc.freenode.net**

## SCREENSHOTS

![sample screenshot](https://raw.github.com/sddm/sddm/master/data/themes/maui/maui.jpg)

## VIDEOS

[Video background](https://www.youtube.com/watch?v=kKwz2FQcE3c)

[Maui theme 1](https://www.youtube.com/watch?v=-0d1wkcU9DU)

[Maui theme 2](https://www.youtube.com/watch?v=dJ28mrOeuNA)

## INSTALLATION

Qt >= 5.3.0 is required to use SDDM, although Qt >= 5.4.0 is recommended.

SDDM runs the greeter as a system user named "sddm" whose home directory needs
to be set to ```/var/lib/sddm```.

If pam and systemd are available, the greeter will go through logind
which will give it access to drm devices.

Distributions without pam and systemd will need to put the "sddm" user
into the "video" group, otherwise errors regarding GL and drm devices
might be experienced.
