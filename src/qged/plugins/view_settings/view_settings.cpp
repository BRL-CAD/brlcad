/*                 V I E W _ I N F O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file view_info.cpp
 *
 */

#include <QGroupBox>
#include <QCheckBox>
#include "qtcad/QToolPalette.h"
#include "../plugin.h"

void *
view_settings_tool_create()
{
    QIcon *obj_icon = new QIcon(QPixmap(":settings.svg"));

    QWidget *w = new QWidget();
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    QCheckBox *a_ckbx = new QCheckBox("Adaptive Plotting");
    QCheckBox *adc_ckbx = new QCheckBox("Angle/Dist. Cursor");
    QCheckBox *cdot_ckbx = new QCheckBox("Center Dot");
    QCheckBox *fb_ckbx = new QCheckBox("Framebuffer");
    QCheckBox *fbo_ckbx = new QCheckBox("FB Overlay");
    QCheckBox *fps_ckbx = new QCheckBox("FPS");
    QCheckBox *grid_ckbx = new QCheckBox("Grid");
    QCheckBox *i_ckbx = new QCheckBox("Independent View");
    QCheckBox *mdlaxes_ckbx = new QCheckBox("Model Axes");
    QCheckBox *params_ckbx = new QCheckBox("Parameters");
    QCheckBox *scale_ckbx = new QCheckBox("Scale");
    QCheckBox *viewaxes_ckbx = new QCheckBox("View Axes");
    wl->addWidget(a_ckbx);
    wl->addWidget(adc_ckbx);
    wl->addWidget(cdot_ckbx);
    wl->addWidget(fb_ckbx);
    wl->addWidget(fbo_ckbx);
    wl->addWidget(fps_ckbx);
    wl->addWidget(grid_ckbx);
    wl->addWidget(i_ckbx);
    wl->addWidget(mdlaxes_ckbx);
    wl->addWidget(params_ckbx);
    wl->addWidget(scale_ckbx);
    wl->addWidget(viewaxes_ckbx);
    w->setLayout(wl);

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, w);

    return el;
}

extern "C" {
    struct qged_tool_impl view_settings_tool_impl = {
	view_settings_tool_create
    };

    const struct qged_tool view_settings_tool = { &view_settings_tool_impl, 1 };
    const struct qged_tool *view_settings_tools[] = { &view_settings_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, view_settings_tools, 1 };

    COMPILER_DLLEXPORT const struct qged_plugin *qged_plugin_info()
    {
	return &pinfo;
    }
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
