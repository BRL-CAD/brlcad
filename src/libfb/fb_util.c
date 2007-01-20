/*                       F B _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup fb */
/*@{*/
/** @file fb_util.c
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
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



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
fb_sim_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
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
fb_sim_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
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
fb_sim_cursor(FBIO *ifp, int mode, int x, int y)
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
fb_sim_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	*mode = ifp->if_cursmode;
	*x = ifp->if_xcurs;
	*y = ifp->if_ycurs;

	return	0;
}

/* Backward Compatibility Routines */

int
fb_reset(FBIO *ifp)
{
	return	0;
}

int
fb_viewport(FBIO *ifp, int left, int top, int right, int bottom)
{
	return	0;
}

int
fb_window(FBIO *ifp, int x, int y)
{
	int	xcenter, ycenter;
	int	xzoom, yzoom;

	fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
	xcenter = x;
	ycenter = y;
	return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
}

int
fb_zoom(FBIO *ifp, int x, int y)
{
	int	xcenter, ycenter;
	int	xzoom, yzoom;

	fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
	xzoom = x;
	yzoom = y;
	return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
}

int
fb_scursor(FBIO *ifp, int mode, int x, int y)
{
	/* We could actually implement this but it
	 * is probably of no value.
	 */
	return	0;
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
