/*                       Q U I T . C P P
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
/** @file libged/quit.cpp
 *
 * The quit command.
 *
 */

#include "common.h"

#include "../ged_private.h"

// quit is a no-op at the libged level - it's purpose is to support ged_exec
// callback hooks that call any application callback functions assigned to this
// command and return GED_EXIT for the application to handle.
extern "C" int
ged_quit_core(struct ged *UNUSED(gedp), int UNUSED(argc), const char **UNUSED(argv))
{
    return GED_EXIT;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl exit_cmd_impl = {"exit", ged_quit_core, GED_CMD_DEFAULT};
const struct ged_cmd exit_cmd = { &exit_cmd_impl };

struct ged_cmd_impl q_cmd_impl = {"q", ged_quit_core, GED_CMD_DEFAULT};
const struct ged_cmd q_cmd = { &q_cmd_impl };

struct ged_cmd_impl quit_cmd_impl = {"quit", ged_quit_core, GED_CMD_DEFAULT};
const struct ged_cmd quit_cmd = { &quit_cmd_impl };

const struct ged_cmd *quit_cmds[] = { &exit_cmd, &q_cmd, &quit_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  quit_cmds, 3 };

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

