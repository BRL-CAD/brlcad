/*
 *			C M A P - C R U N C H . C
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
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
