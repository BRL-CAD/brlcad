/*
 *			F B _ R E C T
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
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
