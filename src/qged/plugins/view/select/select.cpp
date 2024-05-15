/*                     S E L E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
/** @file select.cpp
 *
 */

#include <QGroupBox>
#include <QCheckBox>
#include <QtGlobal>
#include "qtcad/QgToolPalette.h"
#include "../../plugin.h"
#include "./CADViewSelector.h"

void *
view_select_tool_create()
{
    QIcon *obj_icon = new QIcon(QPixmap(":selective-tool.svg"));

    CADViewSelector *er = new CADViewSelector();

    QgToolPaletteElement *el = new QgToolPaletteElement(obj_icon, er);

    QObject::connect(er, &CADViewSelector::view_changed, el, &QgToolPaletteElement::element_view_changed);
    QObject::connect(el, &QgToolPaletteElement::element_view_update, er, &CADViewSelector::do_view_update);

    // Let the element (and hence the application) know that this tool has a
    // locally customized event filter to use with the view widget.
    el->use_event_filter = true;

    return el;
}

extern "C" {
    struct qged_tool_impl view_select_tool_impl = {
	view_select_tool_create
    };

    const struct qged_tool view_select_tool = { &view_select_tool_impl, 2 };
    const struct qged_tool *view_select_tools[] = { &view_select_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, view_select_tools, 1 };

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
