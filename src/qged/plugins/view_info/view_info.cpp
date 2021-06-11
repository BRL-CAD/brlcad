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

#include "qtcad/QToolPalette.h"
#include "../plugin.h"
#include "view_model.h"

void *
view_info_tool_create()
{
    CADViewModel *vmodel = new CADViewModel();
    QIcon *obj_icon = new QIcon(QPixmap(":info.svg"));
    QKeyValView *vview = new QKeyValView(NULL, 0);
    vview->setModel(vmodel);
    vview->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, vview);
    QObject::connect(el, &QToolPaletteElement::app_view_update, vmodel, &CADViewModel::refresh);

    return el;
}

extern "C" {
    struct qged_tool_impl view_info_tool_impl = {
	view_info_tool_create
    };

    const struct qged_tool view_info_tool = { &view_info_tool_impl };
    const struct qged_tool *view_info_tools[] = { &view_info_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, view_info_tools, 1 };

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
