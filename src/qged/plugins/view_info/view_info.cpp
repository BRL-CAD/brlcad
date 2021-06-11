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

#include "../../app.h"
#include "../../palettes.h"
#include "view_model.h"

int
view_info_tool_create(CADApp *ap)
{
    if (!ap || !ap->w || !ap->w->vc)
	return -1;

    CADPalette *vc = ap->w->vc;
    CADViewModel *vmodel = new CADViewModel();
    QIcon *obj_icon = new QIcon(QPixmap(":info.svg"));
    QKeyValView *vview = new QKeyValView(ap->w, 0);
    vview->setModel(vmodel);
    vview->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QObject::connect(((CADApp *)qApp), &CADApp::view_change, vmodel, &CADViewModel::refresh);
    QObject::connect(ap->w, &BRLCAD_MainWindow::view_change, vmodel, &CADViewModel::refresh);
    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, vview);
    vc->addTool(el);

    // If the view changes according to either the view or the app, we need to
    // update our view info in the tool.
    QObject::connect(ap, &CADApp::view_change, vmodel, &CADViewModel::update);
    if (ap->w->canvas) {
	QObject::connect(ap->w->canvas, &QtCADView::changed, vmodel, &CADViewModel::update);
    } else if (ap->w->c4) {
	QObject::connect(ap->w->c4, &QtCADQuad::changed, vmodel, &CADViewModel::update);
    }

    return 0;
}

extern "C" {
    struct qged_tool_impl view_info_tool_impl = {
	view_info_tool_create
    };

    const struct qged_tool view_info_tool = { &view_info_tool_impl };
    const struct qged_tool *view_info_tools[] = { &view_info_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_TOOL_PLUGIN, view_info_tools, 1 };

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
