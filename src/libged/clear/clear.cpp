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

extern "C" int
ged_clear_core(struct ged *gedp, int UNUSED(argc), const char **UNUSED(argv))
{
    // Without a callback, this is just a no-op
    if (!gedp || !gedp->ged_screen_clear_callback)
	return BRLCAD_OK;

    // Fire the screen clear callback if we've got one
    (*gedp->ged_screen_clear_callback)(gedp, gedp->ged_screen_clear_callback_udata);

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

