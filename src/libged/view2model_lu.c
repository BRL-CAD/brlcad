/*                         V I E W 2 M O D E L _ L U . C
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
/** @file libged/view2model_lu.c
 *
 * The view2model_lu command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_view2model_lu(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t sf;
    point_t view_pt;
    point_t model_pt;
    static const char *usage = "x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 4)
	goto bad;

    if (sscanf(argv[1], "%lf", &view_pt[X]) != 1 ||
	sscanf(argv[2], "%lf", &view_pt[Y]) != 1 ||
	sscanf(argv[3], "%lf", &view_pt[Z]) != 1)
	goto bad;

    sf = 1.0 / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
    VSCALE(view_pt, view_pt, sf);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    VSCALE(model_pt, model_pt, gedp->ged_wdbp->dbip->dbi_base2local);

    bn_encode_vect(gedp->ged_result_str, model_pt);

    return GED_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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
