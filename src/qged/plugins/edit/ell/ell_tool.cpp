/*                     E L L  _ T O O L . C P P
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
/** @file ell_tool.cpp
 *
 */

#include "QEll.h"
#include "qtcad/QToolPalette.h"
#include "../../plugin.h"

void *
ell_tool()
{
    QIcon *obj_icon = new QIcon(QPixmap(":ell.svg"));

    QEll *ell = new QEll();
    ell->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QToolPaletteElement *el = new QToolPaletteElement(obj_icon, ell);

    // Edit operations will usually change view objects - connect the internal
    // widget signal to the QToolPaletteElement slot so the application can get
    // the word when that happens.
    QObject::connect(ell, &QEll::view_updated, el, &QToolPaletteElement::do_gui_changed_view);
    QObject::connect(ell, &QEll::db_updated, el, &QToolPaletteElement::do_gui_changed_db);

    // Let the element (and hence the application) know that this tool has a
    // locally customized event filter to use with the view widget.
    el->use_event_filter = true;

    return el;
}

extern "C" {
    struct qged_tool_impl ell_tool_impl = {
	ell_tool
    };

    const struct qged_tool ell_tool_s = { &ell_tool_impl, 1000 };
    const struct qged_tool *ell_tools[] = { &ell_tool_s, NULL };

    static const struct qged_plugin pinfo = { QGED_OC_TOOL_PLUGIN, ell_tools, 1 };

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
