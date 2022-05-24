/*                      Q V I E W C T R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file QViewCtrl.cpp
 *
 * Qt BRL-CAD View Control button panel implementation
 *
 */

#include "common.h"

#include "qtcad/QViewCtrl.h"


QViewCtrl::QViewCtrl(QWidget *pparent) : QToolBar(pparent)
{
    this->setStyleSheet("QToolButton{margin:0px;}");

    sca = addAction(QIcon(QPixmap(":images/view/view_scale.png")), "Scale");
    rot = addAction(QIcon(QPixmap(":images/view/view_rotate.png")), "Rotate");
    tra = addAction(QIcon(QPixmap(":images/view/view_translate.png")), "Translate");
    center = addAction(QIcon(QPixmap(":images/view/view_center.png")), "Center");

    addSeparator();

    raytrace = addAction(QIcon(QPixmap(":images/view/raytrace.png")), "Raytrace");
    fb_on = addAction(QIcon(QPixmap(":images/view/framebuffer.png")), "Framebuffer On/Off");
    fb_overlay = addAction(QIcon(QPixmap(":images/view/framebuffer_overlay.png")), "Framebuffer Overlay/Underlay");
    fb_clear = addAction(QIcon(QPixmap(":images/view/framebuffer_clear.png")), "Clear Framebuffer");

}

QViewCtrl::~QViewCtrl()
{
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


