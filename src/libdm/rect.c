/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rect.c
 *
 * Rubber band rectangle.
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

void
dm_draw_rect(struct dm *dmp, struct ged_rect_state *grsp, struct ged_view *gvp)
{
    if (NEAR_ZERO(grsp->grs_width, (fastf_t)SMALL_FASTF) &&
	NEAR_ZERO(grsp->grs_height, (fastf_t)SMALL_FASTF))
	return;

#if 0
    if (grsp->grs_active && mged_variables->mv_mouse_behavior == 'z')
	ged_adjust_rect_for_zoom();
#endif

    /* draw rectangle */
    DM_SET_FGCOLOR(dmp,
		   grsp->grs_color[0],
		   grsp->grs_color[1],
		   grsp->grs_color[2], 1, 1.0);
    DM_SET_LINE_ATTR(dmp, grsp->grs_line_width, grsp->grs_line_style);

    DM_DRAW_LINE_2D(dmp,
		    grsp->grs_x,
		    grsp->grs_y * dmp->dm_aspect,
		    grsp->grs_x,
		    (grsp->grs_y + grsp->grs_height) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    grsp->grs_x,
		    (grsp->grs_y + grsp->grs_height) * dmp->dm_aspect,
		    grsp->grs_x + grsp->grs_width,
		    (grsp->grs_y + grsp->grs_height) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    grsp->grs_x + grsp->grs_width,
		    (grsp->grs_y + grsp->grs_height) * dmp->dm_aspect,
		    grsp->grs_x + grsp->grs_width,
		    grsp->grs_y * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    grsp->grs_x + grsp->grs_width,
		    grsp->grs_y * dmp->dm_aspect,
		    grsp->grs_x,
		    grsp->grs_y * dmp->dm_aspect);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
