/*                         P M A T . C
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
/** @file libged/pmat.c
 *
 * The pmat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_pmat(struct ged *gedp, int argc, const char *argv[])
{
    mat_t pmat;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the perspective matrix */
    if (argc == 1) {
	bn_encode_mat(gedp->ged_result_str, gedp->ged_gvp->gv_pmat);
	return GED_OK;
    } else if (argc == 2) {
	/* set perspective matrix */
	if (bn_decode_mat(pmat, argv[1]) != 16)
	    return TCL_ERROR;

	MAT_COPY(gedp->ged_gvp->gv_pmat, pmat);
	ged_view_update(gedp->ged_gvp);

	return TCL_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
    return GED_ERROR;
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
