/*                          S C A L E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file libdm/scale.c
 *
 * Functions -
 *	dm_draw_scale	Draws the view scale.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"

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
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
