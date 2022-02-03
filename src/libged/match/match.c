/*                         M A T C H . C
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
/** @file libged/match.c
 *
 * The match command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/path.h"

#include "../ged_private.h"


int
ged_match_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "expression";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    for (++argv; *argv != NULL; ++argv) {
	register int i, num;
	register struct directory *dp;
	for (i = num = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (bu_path_match(*argv, dp->d_namep, 0) != 0)
		    continue;
		if (num == 0)
		    bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
		else {
		    bu_vls_strcat(gedp->ged_result_str, " ");
		    bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
		}
		++num;
	    }
	}

	if (num > 0)
	    bu_vls_strcat(gedp->ged_result_str, " ");
    }
    bu_vls_trimspace(gedp->ged_result_str);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl match_cmd_impl = {
    "match",
    ged_match_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd match_cmd = { &match_cmd_impl };
const struct ged_cmd *match_cmds[] = { &match_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  match_cmds, 1 };

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
