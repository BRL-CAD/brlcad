/*                       F B _ R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2007 United States Government as represented by
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
/** @addtogroup fb */
/** @{ */
/** @file fb_rect.c
 *
 *  Subroutines to simulate the fb_readrect() and fb_writerect()
 *  capabilities for displays that do not presently handle it.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

/*
 *			F B _ S I M _ R E A D R E C T
 *
 *  A routine to simulate the effect of fb_readrect() when a
 *  particular display does not handle it.
 */
int
fb_sim_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	register int	y;
	register int	tot;
	int		got;

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		got = fb_read( ifp, xmin, y, pp, width );
		if( got < 0 )  {
			fb_log("fb_sim_readrect() y=%d unexpected EOF\n", y);
			break;
		}
		tot += got;
		if( got != width )  {
			fb_log("fb_sim_readrect() y=%d, read of %d got %d pixels, aborting\n",
				y, width, got );
			break;
		}
		pp += width * sizeof(RGBpixel);
	}
	return(tot);
}

/*
 *			F B _ S I M _ W R I T E R E C T
 *
 *  A routine to simulate the effect of fb_writerect() when a
 *  particular display does not handle it.
 *
 *  Returns number of pixels actually written.
 *  Clipping to the screen may reduce the total if caller was sloppy.
 */
int
fb_sim_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	register int	y;
	register int	tot;
	int		got;
	int		xlen;

	xlen = width;
	if( xmin + width > fb_getwidth(ifp) )
		xlen = fb_getwidth(ifp) - xmin;

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		got = fb_write( ifp, xmin, y, pp, xlen );
		tot += got;
		if( got != xlen )  break;
		pp += width * sizeof(RGBpixel);
	}
	return(tot);
}

/*
 *			F B _ S I M _ B W R E A D R E C T
 */
#define SIMBUF_SIZE	(24*1024)
int
fb_sim_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	register int	y;
	register int	tot;
	int		got;
	unsigned char	buf[SIMBUF_SIZE*3];

	if( width > SIMBUF_SIZE )  {
		fb_log("fb_sim_bwreadrect() width of %d exceeds internal buffer, aborting\n", width);
		return -SIMBUF_SIZE;	/* FAIL */
	}

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		register int	x;

		got = fb_read( ifp, xmin, y, buf, width );

		/* Extract green chan */
		for( x=0; x < width; x++ )
			*pp++ = buf[x*3+GRN];

		tot += got;
		if( got != width )  break;
	}
	return tot;
}

/*
 *			F B _ S I M _ B W W R I T E R E C T
 */
int
fb_sim_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	register int	y;
	register int	tot;
	int		got;
	int		xlen;
	unsigned char	buf[SIMBUF_SIZE];

	if( width > SIMBUF_SIZE )  {
		fb_log("fb_sim_bwwriterect() width of %d exceeds internal buffer, aborting\n", width);
		return -SIMBUF_SIZE;	/* FAIL */
	}

	xlen = width;
	if( xmin + width > fb_getwidth(ifp) )
		xlen = fb_getwidth(ifp) - xmin;

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		register int	x;
		register unsigned char	*bp;

		/* Copy monochrome (b&w) intensity into all three chans */
		bp = buf;
		for( x=0; x < width; x++ )  {
			register unsigned char	c = *pp++;
			bp[0] = c;
			bp[1] = c;
			bp[2] = c;
			bp += 3;
		}

		got = fb_write( ifp, xmin, y, buf, xlen );
		tot += got;
		if( got != xlen )  break;
	}
	return tot;
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
