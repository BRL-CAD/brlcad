/*
 *			F B _ U T I L
 *
 *  Subroutines to simulate device specific functions for simple
 *  device interfaces to use, and backward compatibility routines.
 *
 *  Author -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

/*
 *			F B _ S I M _ V I E W
 *
 *  A routine to simulate the effect of fb_view() by simply
 *  storing this information into the FBIO structure.
 */
int
fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = ycenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;

	return	0;
}

/*
 *			F B _ S I M _ G E T V I E W
 *
 *  A routine to simulate the effect of fb_getview() by simply
 *  reading this information from the FBIO structure.
 */
int
fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	*xcenter = ifp->if_xcenter;
	*ycenter = ifp->if_ycenter;
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	return	0;
}

/*
 *			F B _ S I M _ C U R S O R
 *
 *  A routine to simulate the effect of fb_cursor() by simply
 *  storing this information into the FBIO structure.
 */
int
fb_sim_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	ifp->if_cursmode = mode;
	ifp->if_xcurs = x;
	ifp->if_ycurs = y;

	return	0;
}

/*
 *			F B _ S I M _ G E T C U R S O R
 *
 *  A routine to simulate the effect of fb_getcursor() by simply
 *  reading this information from the FBIO structure.
 */
int
fb_sim_getcursor( ifp, mode, x, y )
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	*mode = ifp->if_cursmode;
	*x = ifp->if_xcurs;
	*y = ifp->if_ycurs;

	return	0;
}

/* Backward Compatibility Routines */

int
fb_reset(ifp)
FBIO	*ifp;
{
	return	0;
}

int
fb_viewport(ifp, left, top, right, bottom)
FBIO	*ifp;
int	left, top, right, bottom;
{
	return	0;
}

int
fb_window(ifp, x, y)
FBIO	*ifp;
int	x, y;
{
	int	xcenter, ycenter;
	int	xzoom, yzoom;

	fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
	xcenter = x;
	ycenter = y;
	return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
}

int
fb_zoom(ifp, x, y)
FBIO	*ifp;
int	x, y;
{
	int	xcenter, ycenter;
	int	xzoom, yzoom;

	fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
	xzoom = x;
	yzoom = y;
	return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
}

int
fb_scursor(ifp, mode, x, y)
FBIO	*ifp;
int	mode;
int	x, y;
{
	/* We could actually implement this but it
	 * is probably of no value.
	 */
	return	0;
}
