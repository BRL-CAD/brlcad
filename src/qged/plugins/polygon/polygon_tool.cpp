/*               P O L Y G O N  _ T O O L . C P P
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
/** @file polygon_tool.cpp
 *
 */

#include "polygon_control.h"
#include "qtcad/QToolPalette.h"
#include "../plugin.h"

void *
polygon_tool_create()
{
    QIcon *obj_icon = new QIcon(QPixmap(":polygon.svg"));

    QPolyControl *poly_control = new QPolyControl();
    poly_control->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, poly_control);

    // These controls may change the view - connect the internal widget signal
    // to the QToolPaletteElement slot so the application can get the word when
    // that happens.
    QObject::connect(poly_control, &QPolyControl::view_updated, el, &QToolPaletteElement::do_gui_changed_view);

    // Let the element (and hence the application) know that this tool has a
    // locally customized event filter to use with the view widget.
    el->use_event_filter = true;

    return el;
}

void *
polygon_tool_modify()
{
    QIcon *obj_icon = new QIcon(QPixmap(":polygon.svg"));

    QPolyControl *poly_control = new QPolyControl();
    poly_control->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, poly_control);

    // These controls may change the view - connect the internal widget signal
    // to the QToolPaletteElement slot so the application can get the word when
    // that happens.
    QObject::connect(poly_control, &QPolyControl::view_updated, el, &QToolPaletteElement::do_gui_changed_view);

    // Let the element (and hence the application) know that this tool has a
    // locally customized event filter to use with the view widget.
    el->use_event_filter = true;

    return el;
}

extern "C" {
    struct qged_tool_impl polygon_tool_create_impl = {
	polygon_tool_create
    };

    struct qged_tool_impl polygon_tool_modify_impl = {
	polygon_tool_modify
    };


    const struct qged_tool polygon_tool_create_s = { &polygon_tool_create_impl, 100 };
    const struct qged_tool polygon_tool_modify_s = { &polygon_tool_modify_impl, 101 };
    const struct qged_tool *polygon_tools[] = { &polygon_tool_create_s, &polygon_tool_modify_s, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, polygon_tools, 2 };

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
