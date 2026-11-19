#!/bin/bash
# Isolated D-Bus; no host login manager or live greeter.
export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
exec "$@"
