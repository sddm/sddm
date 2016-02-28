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

GreenIsland.WaylandCompositor {
    property variant outputs: []

    id: sddmCompositor
    onSurfaceRequested: {
        var surface = surfaceComponent.createObject(sddmCompositor, {});
        surface.initialize(sddmCompositor, client, id, version);
    }

    GreenIsland.ScreenManager {
        id: screenManager
        onScreenAdded: {
            var output = outputComponent.createObject(sddmCompositor, {
                                                          "compositor": sddmCompositor,
                                                          "nativeScreen": screen
                                                      });
            outputs.push(output);
        }
        onScreenRemoved: {
            var index = screenManager.indexOf(screen);
            if (index < outputs.length) {
                var output = outputs[index];
                outputs.splice(index, 1);
                output.destroy();
            }
        }
        onPrimaryScreenChanged: {
            var index = screenManager.indexOf(screen);
            if (index < outputs.length)
                sddmCompositor.defaultOutput = outputs[index];
        }
    }

    GreenIsland.QtWindowManager {
        showIsFullScreen: true
    }

    GreenIsland.WlShell {
        id: wlShell
        onWlShellSurfaceCreated: {
            for (var i = 0; i < outputs.length; i++) {
                var view = chromeComponent.createObject(outputs[i].surfacesArea, {
                    "shellSurface": shellSurface
                });
            }
        }
    }

    Component {
        id: surfaceComponent

        GreenIsland.WaylandSurface {}
    }

    Component {
        id: outputComponent

        Output {}
    }

    Component {
        id: chromeComponent

        Chrome {}
    }
}
