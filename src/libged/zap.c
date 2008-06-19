/*                         Z A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file zap.c
 *
 * The zap command.
 *
 */

#include "ged_private.h"
#include "solid.h"


/*
 * Erase all currently displayed geometry
 *
 * Usage:
 *        zap
 *
 */
int
ged_zap(struct ged *gedp, int argc, const char *argv[])
{
    register struct solid *sp;
    register struct solid *nsp;
    struct directory *dp;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    sp = BU_LIST_NEXT(solid, &gedp->ged_gdp->gd_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gedp->ged_gdp->gd_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    if (db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
		bu_vls_printf(&gedp->ged_result_str, "ged_zap: db_dirdelete failed\n");
	    }
	}

	nsp = BU_LIST_PNEXT(solid, sp);
	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &FreeSolid.l);
	sp = nsp;
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
