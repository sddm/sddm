/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QDebug>
#include <QString>

#include "VirtualTerminal.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#ifdef __FreeBSD__
#include <sys/consio.h>
#else
#include <linux/vt.h>
#include <linux/kd.h>
#endif
#include <sys/ioctl.h>
#include <qscopeguard.h>
#include <QFileInfo>

#define RELEASE_DISPLAY_SIGNAL (SIGRTMAX)
#define ACQUIRE_DISPLAY_SIGNAL (SIGRTMAX - 1)

namespace SDDM {
    namespace VirtualTerminal {
#ifdef __FreeBSD__
        static const char *defaultVtPath = "/dev/ttyv0";

        QString path(int vt) {
            char c = (vt <= 10 ? '0' : 'a') + (vt - 1);
            return QStringLiteral("/dev/ttyv%1").arg(c);
        }

        int getVtActive(int fd) {
            int vtActive = 0;
            if (ioctl(fd, VT_GETACTIVE, &vtActive) < 0) {
                qCritical() << "Failed to get current VT:" << strerror(errno);
                return -1;
            }
            return vtActive;
        }
#else
        static const char *defaultVtPath = "/dev/tty0";

        QString path(int vt) {
            return QStringLiteral("/dev/tty%1").arg(vt);
        }

        int getVtActive(int fd) {
            vt_stat vtState { };
            if (ioctl(fd, VT_GETSTATE, &vtState) < 0) {
                qCritical() << "Failed to get current VT:" << strerror(errno);
                return -1;
            }
            return vtState.v_active;
        }
#endif

        static void onAcquireDisplay([[maybe_unused]] int signal) {
            int fd = open(defaultVtPath, O_RDWR | O_NOCTTY);
            ioctl(fd, VT_RELDISP, VT_ACKACQ);
            close(fd);
        }

        static void onReleaseDisplay([[maybe_unused]] int signal) {
            int fd = open(defaultVtPath, O_RDWR | O_NOCTTY);
            ioctl(fd, VT_RELDISP, 1);
            close(fd);
        }

        static bool handleVtSwitches(int fd) {
            vt_mode setModeRequest { };
            bool ok = true;

            setModeRequest.mode = VT_PROCESS;
            setModeRequest.relsig = RELEASE_DISPLAY_SIGNAL;
            setModeRequest.acqsig = ACQUIRE_DISPLAY_SIGNAL;

            if (ioctl(fd, VT_SETMODE, &setModeRequest) < 0) {
                qDebug() << "Failed to manage VT manually:" << strerror(errno);
                ok = false;
            }

            signal(RELEASE_DISPLAY_SIGNAL, onReleaseDisplay);
            signal(ACQUIRE_DISPLAY_SIGNAL, onAcquireDisplay);

            return ok;
        }

        static void fixVtMode(int fd, bool vt_auto) {
            vt_mode getmodeReply { };
            int kernelDisplayMode = 0;
            bool modeFixed = false;
            bool ok = true;

            if (ioctl(fd, VT_GETMODE, &getmodeReply) < 0) {
                qWarning() << "Failed to query VT mode:" << strerror(errno);
                ok = false;
            }

            if (getmodeReply.mode != VT_AUTO)
                goto out;

            if (ioctl(fd, KDGETMODE, &kernelDisplayMode) < 0) {
                qWarning() << "Failed to query kernel display mode:" << strerror(errno);
                ok = false;
            }

            if (kernelDisplayMode == KD_TEXT)
                goto out;

            // VT is in the VT_AUTO + KD_GRAPHICS state, fix it
            if (vt_auto) {
                // If vt_auto is true, the controlling process is already gone, so there is no
                // process which could send the VT_RELDISP 1 ioctl to release the vt.
                // Switch to KD_TEXT and let the kernel switch vts automatically
                if (ioctl(fd, KDSETMODE, KD_TEXT) < 0) {
                    qWarning("Failed to set text mode for current VT: %s", strerror(errno));
                    ok = false;
                }
            }
            else {
                ok = handleVtSwitches(fd);
                modeFixed = true;
            }
out:
            if (!ok) {
                qCritical() << "Failed to set up VT mode";
                return;
            }

            if (modeFixed)
                qDebug() << "VT mode fixed";
            else
                qDebug() << "VT mode didn't need to be fixed";
        }

        int currentVt()
        {
            int fd = open(defaultVtPath, O_RDWR | O_NOCTTY);
            if (fd < 0) {
                qCritical() << "Failed to open VT master:" << strerror(errno);
                return -1;
            }
            auto closeFd = qScopeGuard([fd] {
                close(fd);
            });

            return getVtActive(fd);
        }


        int setUpNewVt() {
            // open VT master
            int fd = open(defaultVtPath, O_RDWR | O_NOCTTY);
            if (fd < 0) {
                qCritical() << "Failed to open VT master:" << strerror(errno);
                return -1;
            }
            auto closeFd = qScopeGuard([fd] {
                close(fd);
            });

            int vt = 0;
            if (ioctl(fd, VT_OPENQRY, &vt) < 0) {
                qCritical() << "Failed to open new VT:" << strerror(errno);
                return -1;
            }

            // fallback to active VT
            if (vt <= 0) {
                int vtActive = getVtActive(fd);
                qWarning() << "New VT" << vt << "is not valid, fall back to" << vtActive;
                return vtActive;
            }

            return vt;
        }

        void jumpToVt(int vt, bool vt_auto) {
            qDebug() << "Jumping to VT" << vt;

            int fd;

            int activeVtFd = open(defaultVtPath, O_RDWR | O_NOCTTY);

            QString ttyString = path(vt);
            int vtFd = open(qPrintable(ttyString), O_RDWR | O_NOCTTY);
            if (vtFd != -1) {
                fd = vtFd;

                // Clear VT
                static const char *clearEscapeSequence = "\33[H\33[2J";
                if (write(vtFd, clearEscapeSequence, sizeof(clearEscapeSequence)) == -1) {
                    qWarning("Failed to clear VT %d: %s", vt, strerror(errno));
                }

                // set graphics mode to prevent flickering
                if (ioctl(fd, KDSETMODE, KD_GRAPHICS) < 0)
                    qWarning("Failed to set graphics mode for VT %d: %s", vt, strerror(errno));

                // it's possible that the current VT was left in a broken
                // combination of states (KD_GRAPHICS with VT_AUTO) that we
                // cannot switch from, so make sure things are in a way that
                // will make VT_ACTIVATE work without hanging VT_WAITACTIVE
                fixVtMode(activeVtFd, vt_auto);
            } else {
                qWarning("Failed to open %s: %s", qPrintable(ttyString), strerror(errno));
                qDebug("Using %s instead of %s!", defaultVtPath, qPrintable(ttyString));
                fd = activeVtFd;
            }

            // If vt_auto is true, the controlling process is already gone, so there is no
            // process which could send the VT_RELDISP 1 ioctl to release the vt.
            // Let the kernel switch vts automatically
            if (!vt_auto)
                handleVtSwitches(fd);

            do {
                errno = 0;

                if (ioctl(fd, VT_ACTIVATE, vt) < 0) {
                    if (errno == EINTR)
                        continue;

                    qWarning("Couldn't initiate jump to VT %d: %s", vt, strerror(errno));
                    break;
                }

                if (ioctl(fd, VT_WAITACTIVE, vt) < 0 && errno != EINTR)
                    qWarning("Couldn't finalize jump to VT %d: %s", vt, strerror(errno));

            } while (errno == EINTR);
            close(activeVtFd);
            if (vtFd != -1)
                close(vtFd);
        }
    }
}
