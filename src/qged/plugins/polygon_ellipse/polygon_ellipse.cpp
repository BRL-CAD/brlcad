/*             P O L Y G O N _ E L L I P S E _ T O O L . C P P
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
/** @file polygon_ellipse_tool.cpp
 *
 */

#include "qtcad/QToolPalette.h"
#include "../plugin.h"

void *
polygon_ellipse_tool_create()
{
    QIcon *obj_icon = new QIcon(QPixmap(":ellipse.svg"));

    QString polygon_ctrls("polygon ellipse controls ");
    QPushButton *poly_control = new QPushButton(polygon_ctrls);
    poly_control->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, poly_control);

    return el;
}

extern "C" {
    struct qged_tool_impl polygon_ellipse_tool_impl = {
	polygon_ellipse_tool_create
    };

    const struct qged_tool polygon_ellipse_tool = { &polygon_ellipse_tool_impl, 101 };
    const struct qged_tool *polygon_ellipse_tools[] = { &polygon_ellipse_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, polygon_ellipse_tools, 1 };

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
