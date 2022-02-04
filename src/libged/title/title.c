/*                         T I T L E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/title.c
 *
 * The title command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_title_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* The Tcl wdbp doesn't seem to work reliably for this purpose (the put
     * command has issues??) so temporarily switch the dbip->dbi_wdbp pointer
     * to the GED version. */
    struct rt_wdb *tmp_wdbp = gedp->dbip->dbi_wdbp;
    gedp->dbip->dbi_wdbp = gedp->ged_wdbp;

    int ret = rt_cmd_title(gedp->ged_result_str, gedp->dbip, argc, argv);

    /* Restore default dbip->dbi_wdbp */
    gedp->dbip->dbi_wdbp = tmp_wdbp;

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl title_cmd_impl = {
    "title",
    ged_title_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd title_cmd = { &title_cmd_impl };
const struct ged_cmd *title_cmds[] = { &title_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  title_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
