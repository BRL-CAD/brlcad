/*                         K I L L R E F S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/killrefs.c
 *
 * The killrefs command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_killrefs(struct ged *gedp, int argc, const char *argv[])
{
    int k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int nflag;
    int ret;
    static const char *usage = "[-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (!gedp->ged_internal_call) {
	/* initialize result */
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Process the -n option */
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
	nflag = 1;
	--argc;
	++argv;
    } else
	nflag = 0;

    if (!nflag && !gedp->ged_internal_call) {
	for (k = 1; k < argc; k++)
	    _ged_eraseAllNamesFromDisplay(gedp, argv[k], 1);
    }

    ret = GED_OK;

    FOR_ALL_DIRECTORY_START(dp, gedp->ged_wdbp->dbip) {
	if (!(dp->d_flags & RT_DIR_COMB))
	    continue;

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
	    ret = GED_ERROR;
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	for (k=1; k<argc; k++) {
	    int code;

	    code = db_tree_del_dbleaf(&(comb->tree), argv[k], &rt_uniresource, nflag);
	    if (code == -1)
		continue;	/* not found */
	    if (code == -2)
		continue;	/* empty tree */
	    if (code < 0) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, argv[k]);
		ret = GED_ERROR;
	    } else {
		if (nflag)
		    bu_vls_printf(gedp->ged_result_str, "%s ", dp->d_namep);
		else
		    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, argv[k]);
	    }
	}

	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Unable to write new combination into database.\n");
	    ret = GED_ERROR;
	    continue;
	}
    } FOR_ALL_DIRECTORY_END;

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
