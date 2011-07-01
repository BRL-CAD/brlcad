/*                         R R T . C
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
/** @file libged/rrt.c
 *
 * The rrt command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "cmd.h"
#include "solid.h"

#include "./ged_private.h"


int
ged_rrt(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    size_t args;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    args = argc + 2 + ged_count_tops(gedp);
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    for (i=1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    _ged_current_gedp = gedp;
    _ged_current_gedp->ged_gdp->gd_rt_cmd_len = ged_build_tops(gedp, vp, &_ged_current_gedp->ged_gdp->gd_rt_cmd[args]);

    (void)_ged_run_rt(gedp);

    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");

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
