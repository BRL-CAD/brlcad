/*                         R C O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/rcodes.c
 *
 * The rcodes command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_rcodes_core(struct ged *gedp, int argc, const char *argv[])
{
    int item, air, mat, los;
    size_t g_changed = 0;
    int found_a_match = 0;
    char name[RT_MAXLINE];
    char line[RT_MAXLINE];
    char *cp;
    FILE *fp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s filename", argv[0]);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s filename", argv[0]);
	return GED_ERROR;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Failed to read file - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    while (bu_fgets(line, RT_MAXLINE, fp) != NULL) {
	int changed;

	/* character and/or whitespace delimited numbers */
	if (sscanf(line, "%d%*c%d%*c%d%*c%d%s", &item, &air, &mat, &los, name) != 5)
	    continue; /* not useful */

	/* skip over the path */
	if ((cp = strrchr(name, (int)'/')) == NULL)
	    cp = name;
	else
	    ++cp;

	if (*cp == '\0')
	    continue;

	if ((dp = db_lookup(gedp->ged_wdbp->dbip, cp, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[1], cp);
	    continue;
	}

	if (!(dp->d_flags & RT_DIR_REGION)) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a region\n", argv[1], cp);
	    continue;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) != ID_COMBINATION) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a region\n", argv[1], cp);
	    continue;
	}

	/* By the time we make it here, we've got something */
	found_a_match = 1;

	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* make the changes */
	changed = 0;
	if (comb->region_id != item) {
	    comb->region_id = item;
	    changed = 1;
	}
	if (comb->aircode != air) {
	    comb->aircode = air;
	    changed = 1;
	}
	if (comb->GIFTmater != mat) {
	    comb->GIFTmater = mat;
	    changed = 1;
	}
	if (comb->los != los) {
	    comb->los = los;
	    changed = 1;
	}

	if (changed) {
	    /* write out all changes */
	    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource)) {
		bu_vls_printf(gedp->ged_result_str, "Database write error, aborting.\n");
		bu_vls_printf(gedp->ged_result_str,
			      "The in-memory table of contents may not match the status of the on-disk\ndatabase.  The on-disk database should still be intact.  For safety, \nyou should exit now, and resolve the I/O problem, before continuing.\n");

		rt_db_free_internal(&intern);
		fclose(fp);
		return GED_ERROR;
	    }
	}
	g_changed += (size_t)changed;

    }
    fclose(fp);

    if (!found_a_match) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: rcodes file \"%s\" contained no matching lines.  Geometry unchanged.\n", argv[1]);
    } else {
	if (g_changed) {
	    bu_vls_printf(gedp->ged_result_str, "NOTE: rcodes file \"%s\" applied.  %zu region%supdated.\n", argv[1], g_changed, (g_changed==1)?" ":"s ");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "NOTE: 0 regions updated, geometry unchanged.\n");
	}
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rcodes_cmd_impl = {
    "rcodes",
    ged_rcodes_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rcodes_cmd = { &rcodes_cmd_impl };
const struct ged_cmd *rcodes_cmds[] = { &rcodes_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rcodes_cmds, 1 };

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
