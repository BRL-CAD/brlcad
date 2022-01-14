/*                      P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
 *
 * TODO - need to support more than just QToolPaletteElement types
 * being created by plugins.  The four logical candidates so far:
 *
 * 1.  ged-style command line commands  (maybe should expand libged
 *     plugin setup to support hooking them in at that level, but may
 *     also want awareness of app level info in commands...)
 * 2.  QToolPaletteLElement (currently what we're using)
 * 3.  QDockWidget items, equivalent to (say) the standard attributes
 *     dialog and able to be docked in the main GUI.
 * 4.  Full-fledged dialogs that are their own windows and launched
 *     from the menu.  Candidates might include complex procedural geometry
 *     generation tools, tabular report generators, or other large
 *     grpahical layout scenarios that won't fit well as a widget in
 *     the main GUI.
 */

#ifndef QGED_PLUGIN_H
#define QGED_PLUGIN_H

#include "common.h"
#include "brlcad_version.h"

struct qged_tool_impl;
struct qged_tool {
    struct qged_tool_impl *i;
    int palette_priority;
};

struct qged_plugin {
    uint32_t api_version; /* must be first in struct */
    const struct qged_tool ** const cmds;
    int cmd_cnt;
};

#define QGED_CMD_PLUGIN  (3*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_VC_TOOL_PLUGIN (4*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_IC_TOOL_PLUGIN (5*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_OC_TOOL_PLUGIN (6*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

typedef void * (*qged_func_ptr)();
struct qged_tool_impl {
    qged_func_ptr tool_create;
};

#endif  /* QGED_PLUGIN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
