/*                     M E A S U R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file measure.cpp
 *
 */

#include <QGroupBox>
#include <QCheckBox>
#include "qtcad/QToolPalette.h"
#include "../../plugin.h"
#include "./widget.h"

void *
view_measure_tool_create()
{
    QIcon *obj_icon = new QIcon(QPixmap(":measure.svg"));

    CADViewMeasure *er = new CADViewMeasure();

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, er);

    QObject::connect(er, &CADViewMeasure::view_updated, el, &QToolPaletteElement::do_view_changed);

    // If units change, need to update the reported dimensions
    QObject::connect(el, &QToolPaletteElement::app_db_sync, er, &CADViewMeasure::adjust_text_db);

    // Let the element (and hence the application) know that this tool has a
    // locally customized event filter to use with the view widget.
    el->use_event_filter = true;

    return el;
}

extern "C" {
    struct qged_tool_impl view_measure_tool_impl = {
	view_measure_tool_create
    };

    const struct qged_tool view_measure_tool = { &view_measure_tool_impl, 3 };
    const struct qged_tool *view_measure_tools[] = { &view_measure_tool, NULL };

    static const struct qged_plugin pinfo = { QGED_VC_TOOL_PLUGIN, view_measure_tools, 1 };

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
