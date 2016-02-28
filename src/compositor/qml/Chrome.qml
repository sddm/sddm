/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

import QtQuick 2.0
import GreenIsland 1.0 as GreenIsland

GreenIsland.ShellSurfaceItem {
    id: chrome
    x: 0
    y: 0
    //sizeFollowsSurface: false
    onSurfaceDestroyed: {
        view.bufferLocked = true;
        destroyAnimation.start();
    }

    SequentialAnimation {
        id: destroyAnimation

        NumberAnimation { target: chrome; property: "opacity"; to: 0.0; duration: 2500 }
        ScriptAction { script: chrome.destroy() }
    }
}
