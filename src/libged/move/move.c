/*                         M O V E . C
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
/** @file libged/move.c
 *
 * The move command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"

#include "../ged_private.h"


int
ged_move_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct directory *dp;
    struct rt_db_internal intern;
    static const char *usage = "from to";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (db_lookup(gedp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists", argv[2]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    /* Change object name in the in-memory directory. */
    if (db_rename(gedp->dbip, dp, argv[2]) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "error in db_rename to %s, aborting", argv[2]);
	return GED_ERROR;
    }

    /* Re-write to the database.  New name is applied on the way out. */
    if (rt_db_put_internal(dp, gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return GED_ERROR;
    }

    /* Change object name if it matches the first element in the display list path. */
    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	int first = 1;
	int found = 0;
	struct bu_vls new_path = BU_VLS_INIT_ZERO;
	char *dupstr = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	char *tok = strtok(dupstr, "/");

	while (tok) {
	    if (first) {
		first = 0;

		if (BU_STR_EQUAL(tok, argv[1])) {
		    found = 1;
		    bu_vls_printf(&new_path, "%s", argv[2]);
		} else
		    break; /* no need to go further */
	    } else
		bu_vls_printf(&new_path, "/%s", tok);

	    tok = strtok((char *)NULL, "/");
	}

	if (found) {
	    bu_vls_free(&gdlp->dl_path);
	    bu_vls_printf(&gdlp->dl_path, "%s", bu_vls_addr(&new_path));
	}

	free((void *)dupstr);
	bu_vls_free(&new_path);
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl move_cmd_impl = {"move", ged_move_core, GED_CMD_DEFAULT};
const struct ged_cmd move_cmd = { &move_cmd_impl };

struct ged_cmd_impl mv_cmd_impl = {"mv", ged_move_core, GED_CMD_DEFAULT};
const struct ged_cmd mv_cmd = { &mv_cmd_impl };

const struct ged_cmd *move_cmds[] = { &move_cmd, &mv_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  move_cmds, 2 };

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
