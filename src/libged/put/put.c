/*                         P U T . C
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
/** @file libged/put.c
 *
 * The put command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/cmd.h"
#include "ged.h"


int
ged_put_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "object type attrs";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* The Tcl wdbp doesn't seem to work reliably for this purpose (??) so
     * temporarily switch the dbip->dbi_wdbp pointer to the GED version. */
    struct rt_wdb *tmp_wdbp = gedp->dbip->dbi_wdbp;
    gedp->dbip->dbi_wdbp = gedp->ged_wdbp;

    int ret = rt_cmd_put(gedp->ged_result_str, gedp->dbip, argc, argv);

    /* Restore default dbip->dbi_wdbp */
    gedp->dbip->dbi_wdbp = tmp_wdbp;

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl put_cmd_impl = {
    "put",
    ged_put_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd put_cmd = { &put_cmd_impl };
const struct ged_cmd *put_cmds[] = { &put_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  put_cmds, 1 };

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
