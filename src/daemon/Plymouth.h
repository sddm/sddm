/***************************************************************************
* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#ifndef __PLYMOUTH_H__
#define __PLYMOUTH_H__

#include <QtCore/QObject>

class Plymouth : public QObject
{
    Q_OBJECT

public:
    explicit Plymouth(QObject *parent = nullptr);
    ~Plymouth();

    static bool isRunning();
    static bool hasActiveVt();
    static void prepareForTransition();
    static void quitWithoutTransition();
    static void quitWithTransition();
    static void log(QString);
};

#endif // __PLYMOUTH_H__
