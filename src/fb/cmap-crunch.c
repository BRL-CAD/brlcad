/*                   C M A P - C R U N C H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *
 */
/** @file cmap-crunch.c
 *
 *  Utility subroutine to apply a colormap to a buffer of pixels
 *
 *  Author -
 *	Gary S. Moss
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "fb.h"

void
cmap_crunch(RGBpixel (*scan_buf), int pixel_ct, ColorMap *cmap)
{
    unsigned short	*rp = cmap->cm_red;
    unsigned short	*gp = cmap->cm_green;
    unsigned short	*bp = cmap->cm_blue;

    /* noalias ? */
    for (; pixel_ct > 0; pixel_ct--, scan_buf++ )  {
	(*scan_buf)[RED] = rp[(*scan_buf)[RED]] >> 8;
	(*scan_buf)[GRN] = gp[(*scan_buf)[GRN]] >> 8;
	(*scan_buf)[BLU] = bp[(*scan_buf)[BLU]] >> 8;
    }
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
