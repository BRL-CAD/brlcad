/*                         S C A L E . C
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
/** @file scale.c
 *
 * The scale command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"
#include "dm.h"


int
ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3)
{
    static const char *usage = "sf (or) sfx sfy sfz";
    int ret = GED_OK;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
        if (sscanf(argv[1], "%lf", sf1) != 1) {
            bu_vls_printf(&gedp->ged_result_str, "\nbad scale factor '%s'", argv[1]);
	    return GED_ERROR;
        }
    } else {
        if (sscanf(argv[1], "%lf", sf1) != 1) {
            bu_vls_printf(&gedp->ged_result_str, "\nbad x scale factor '%s'", argv[1]);
	    ret = GED_ERROR;
        }
        if (sscanf(argv[2], "%lf", sf2) != 1) {
            bu_vls_printf(&gedp->ged_result_str, "\nbad y scale factor '%s'", argv[2]);
	    ret = GED_ERROR;
        }
        if (sscanf(argv[3], "%lf", sf3) != 1) {
            bu_vls_printf(&gedp->ged_result_str, "\nbad z scale factor '%s'", argv[3]);
	    ret = GED_ERROR;
        }
    }
    return ret;
}

int
ged_scale(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    fastf_t sf1;
    fastf_t sf2;
    fastf_t sf3;

    if ((ret = ged_scale_args(gedp, argc, argv, &sf1, &sf2, &sf3)) != GED_OK)
	return ret;

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Can not scale xyz independently on a view.");
	return GED_ERROR;
    }

    if (sf1 <= SMALL_FASTF || INFINITY < sf1)
	return GED_OK;

    /* scale the view */
    gedp->ged_gvp->gv_scale *= sf1;

    if (gedp->ged_gvp->gv_scale < RT_MINVIEWSIZE)
	gedp->ged_gvp->gv_scale = RT_MINVIEWSIZE;
    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


void
dm_draw_scale(struct dm *dmp,
	      fastf_t   viewSize,
	      int       *lineColor,
	      int       *textColor)
{
    int soffset;
    fastf_t xpos1, xpos2;
    fastf_t ypos1, ypos2;
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%g", viewSize*0.5);
    soffset = (int)(strlen(bu_vls_addr(&vls)) * 0.5);

    xpos1 = -0.5;
    xpos2 = 0.5;
    ypos1 = -0.8;
    ypos2 = -0.8;

    DM_SET_FGCOLOR(dmp,
		   (unsigned char)lineColor[0],
		   (unsigned char)lineColor[1],
		   (unsigned char)lineColor[2], 1, 1.0);
    DM_DRAW_LINE_2D(dmp, xpos1, ypos1, xpos2, ypos2);
    DM_DRAW_LINE_2D(dmp, xpos1, ypos1+0.01, xpos1, ypos1-0.01);
    DM_DRAW_LINE_2D(dmp, xpos2, ypos1+0.01, xpos2, ypos1-0.01);

    DM_SET_FGCOLOR(dmp,
		   (unsigned char)textColor[0],
		   (unsigned char)textColor[1],
		   (unsigned char)textColor[2], 1, 1.0);
    DM_DRAW_STRING_2D(dmp, "0", xpos1-0.005, ypos1 + 0.02, 1, 0);
    DM_DRAW_STRING_2D(dmp, bu_vls_addr(&vls),
		      xpos2-(soffset * 0.015),
		      ypos1 + 0.02, 1, 0);

    bu_vls_free(&vls);
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

