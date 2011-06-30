/*                         P A T H S U M . C
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
/** @file libged/pathsum.c
 *
 * The paths command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_pathsum(struct ged *gedp, int argc, const char *argv[])
{
    int i, pos_in;
    struct _ged_trace_data gtd;
    static const char *usage = "pattern";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    pos_in = 1;

    /* find out which command was entered */
    if (BU_STR_EQUAL(argv[0], "paths")) {
	/* want to list all matching paths */
	gtd.gtd_flag = _GED_LISTPATH;
    }
    if (BU_STR_EQUAL(argv[0], "listeval")) {
	/* want to list evaluated solid[s] */
	gtd.gtd_flag = _GED_LISTEVAL;
    }

    if (argc == 2 && strchr(argv[1], '/')) {
	char *tok;
	gtd.gtd_objpos = 0;

	tok = strtok((char *)argv[1], "/");
	while (tok) {
	    if ((gtd.gtd_obj[gtd.gtd_objpos++] = db_lookup(gedp->ged_wdbp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL)
		return GED_ERROR;
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	gtd.gtd_objpos = argc-1;

	/* build directory pointer array for desired path */
	for (i=0; i<gtd.gtd_objpos; i++) {
	    if ((gtd.gtd_obj[i] = db_lookup(gedp->ged_wdbp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL)
		return GED_ERROR;
	}
    }

    MAT_IDN(gtd.gtd_xform);

    _ged_trace(gtd.gtd_obj[0], 0, bn_mat_identity, &gtd);

    if (gtd.gtd_prflag == 0) {
	/* path not found */
	bu_vls_printf(gedp->ged_result_str, "PATH:  ");
	for (i=0; i<gtd.gtd_objpos; i++)
	    bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);

	bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
    }

    return GED_OK;
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
