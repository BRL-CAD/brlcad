/*                   C M A P - C R U N C H . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
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
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"

void
cmap_crunch(register RGBpixel (*scan_buf), register int pixel_ct, ColorMap *cmap)
{
	register unsigned short	*rp = cmap->cm_red;
	register unsigned short	*gp = cmap->cm_green;
	register unsigned short	*bp = cmap->cm_blue;

	/* noalias ? */
	for( ; pixel_ct > 0; pixel_ct--, scan_buf++ )  {
		(*scan_buf)[RED] = rp[(*scan_buf)[RED]] >> 8;
		(*scan_buf)[GRN] = gp[(*scan_buf)[GRN]] >> 8;
		(*scan_buf)[BLU] = bp[(*scan_buf)[BLU]] >> 8;
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
