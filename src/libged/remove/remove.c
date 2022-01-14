/*                         R E M O V E . C
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
/** @file libged/remove.c
 *
 * The remove command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_remove_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int num_deleted;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret;
    static const char *usage = "comb object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if ((dp->d_flags & RT_DIR_COMB) == 0) {
	bu_vls_printf(gedp->ged_result_str, "rm: %s is not a combination", dp->d_namep);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* Process each argument */
    num_deleted = 0;
    ret = GED_OK;
    for (i = 2; i < argc; i++) {
	if (db_tree_del_dbleaf(&(comb->tree), argv[i], &rt_uniresource, 0) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, argv[i]);
	    ret = GED_ERROR;
	} else {
	    struct bu_vls path = BU_VLS_INIT_ZERO;

	    bu_vls_printf(&path, "%s/%s", dp->d_namep, argv[i]);
	    _dl_eraseAllPathsFromDisplay(gedp, bu_vls_addr(&path), 0);
	    bu_vls_free(&path);
	    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, argv[i]);
	    num_deleted++;
	}
    }

    if (rt_db_put_internal(dp, gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return GED_ERROR;
    }

    // Comb changed, need to update nref count
    db_update_nref(gedp->dbip, &rt_uniresource);

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl remove_cmd_impl = {"remove", ged_remove_core, GED_CMD_DEFAULT};
const struct ged_cmd remove_cmd = { &remove_cmd_impl };

struct ged_cmd_impl rm_cmd_impl = {"rm", ged_remove_core, GED_CMD_DEFAULT};
const struct ged_cmd rm_cmd = { &rm_cmd_impl };

const struct ged_cmd *remove_cmds[] = { &remove_cmd, &rm_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  remove_cmds, 2 };

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
