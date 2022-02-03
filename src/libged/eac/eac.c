/*                         E A C . C
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
/** @file libged/eac.c
 *
 * The eac command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_eac_core(struct ged *gedp, int argc, const char *argv[])
{
    int j;
    int item;
    struct directory *dp;
    struct bu_vls v = BU_VLS_INIT_ZERO;
    int new_argc;
    int lim;
    static const char *usage = "air_code(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    bu_vls_strcat(&v, "draw");
    lim = 1;

    for (j = 1; j < argc; j++) {
	item = atoi(argv[j]);
	if (item < 1)
	    continue;

	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;

	    if (!(dp->d_flags & RT_DIR_REGION))
		continue;

	    bu_vls_printf(gedp->ged_result_str, "%s: looking at %s\n", argv[0], dp->d_namep);

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting\n", argv[0]);
		return BRLCAD_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    if (comb->region_id != 0
		|| comb->aircode != item)
	    {
		intern.idb_meth->ft_ifree(&intern);
		continue;
	    }
	    intern.idb_meth->ft_ifree(&intern);

	    bu_vls_strcat(&v, " ");
	    bu_vls_strcat(&v, dp->d_namep);
	    lim++;
	} FOR_ALL_DIRECTORY_END;
    }

    if (lim > 1) {
	int retval;
	char **new_argv;

	new_argv = (char **)bu_calloc(lim+1, sizeof(char *), "ged_eac_core: new_argv");
	new_argc = bu_argv_from_string(new_argv, lim, bu_vls_addr(&v));
	retval = ged_draw(gedp, new_argc, (const char **)new_argv);
	bu_free((void *)new_argv, "ged_eac_core: new_argv");
	bu_vls_free(&v);
	return retval;
    }

    bu_vls_free(&v);
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl eac_cmd_impl = {
    "eac",
    ged_eac_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd eac_cmd = { &eac_cmd_impl };
const struct ged_cmd *eac_cmds[] = { &eac_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  eac_cmds, 1 };

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
