/*                       C L E A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/clear.cpp
 *
 * The clear command.
 *
 */

#include "common.h"

#include "../ged_private.h"

// clear is a no-op at the libged level - it's purpose is to support ged_exec
// callback hooks that call any application callback functions assigned to this
// command.
extern "C" int
ged_clear_core(struct ged *UNUSED(gedp), int UNUSED(argc), const char **UNUSED(argv))
{
    return BRLCAD_OK;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl clear_cmd_impl = {"clear", ged_clear_core, GED_CMD_DEFAULT};
const struct ged_cmd clear_cmd = { &clear_cmd_impl };

const struct ged_cmd *clear_cmds[] = { &clear_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  clear_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

