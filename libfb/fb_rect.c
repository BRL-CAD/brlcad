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
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

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
fb_sim_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
unsigned char	*pp;
{
	register int	y;
	register int	tot;
	int		got;

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		got = fb_read( ifp, xmin, y, pp, width );
		tot += got;
		if( got != width )  break;
		pp += width * sizeof(RGBpixel);
	}
	return(tot);
}

/*
 *			F B _ S I M _ W R I T E R E C T
 *
 *  A routine to simulate the effect of fb_writerect() when a
 *  particular display does not handle it.
 */
int
fb_sim_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
CONST unsigned char	*pp;
{
	register int	y;
	register int	tot;
	int		got;

	tot = 0;
	for( y=ymin; y < ymin+height; y++ )  {
		got = fb_write( ifp, xmin, y, pp, width );
		tot += got;
		if( got != width )  break;
		pp += width * sizeof(RGBpixel);
	}
	return(tot);
}
