/*                         V I E W D I R . C
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
/** @file libged/viewdir.c
 *
 * The viewdir command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_viewdir(struct ged *gedp, int argc, const char *argv[])
{
    vect_t view;
    vect_t dir;
    mat_t invRot;
    int iflag;
    static const char *usage = "[-i]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'i' && argv[1][2] == '\0') {
	iflag = 1;
	--argc;
	++argv;
    } else
	iflag = 0;

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (iflag) {
	VSET(view, 0.0, 0.0, -1.0);
    } else {
	VSET(view, 0.0, 0.0, 1.0);
    }

    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(dir, invRot, view);
    bn_encode_vect(gedp->ged_result_str, dir);

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
