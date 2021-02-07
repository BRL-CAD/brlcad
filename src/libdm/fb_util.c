/*                       F B _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2021 United States Government as represented by
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
/** @addtogroup libstruct fb */
/** @{ */
/** @file fb_util.c
 *
 * Subroutines to simulate device specific functions for simple
 * device interfaces to use, and backward compatibility routines.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>

#include "./include/private.h"
#include "dm.h"


/*
 * A routine to simulate the effect of fb_view() by simply
 * storing this information into the fb structure.
 */
int
fb_sim_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    FB_CK_FB(ifp->i);

    ifp->i->if_xcenter = xcenter;
    ifp->i->if_ycenter = ycenter;
    ifp->i->if_xzoom = xzoom;
    ifp->i->if_yzoom = yzoom;

    return 0;
}


/*
 * A routine to simulate the effect of fb_getview() by simply
 * reading this information from the fb structure.
 */
int
fb_sim_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FB(ifp->i);

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


/*
 * A routine to simulate the effect of fb_cursor() by simply
 * storing this information into the fb structure.
 */
int
fb_sim_cursor(struct fb *ifp, int mode, int x, int y)
{
    FB_CK_FB(ifp->i);

    ifp->i->if_cursmode = mode;
    ifp->i->if_xcurs = x;
    ifp->i->if_ycurs = y;

    return 0;
}


/*
 * A routine to simulate the effect of fb_getcursor() by simply
 * reading this information from the fb structure.
 */
int
fb_sim_getcursor(struct fb *ifp, int *mode, int *x, int *y)
{
    FB_CK_FB(ifp->i);

    *mode = ifp->i->if_cursmode;
    *x = ifp->i->if_xcurs;
    *y = ifp->i->if_ycurs;

    return 0;
}


/* Backward Compatibility Routines */

int
fb_reset(struct fb *ifp)
{
    if (ifp) {
	FB_CK_FB(ifp->i);
    }

    return 0;
}


int
fb_viewport(struct fb *ifp, int UNUSED(left), int UNUSED(top), int UNUSED(right), int UNUSED(bottom))
{
    if (ifp) {
	FB_CK_FB(ifp->i);
    }

    return 0;
}


int
fb_window(struct fb *ifp, int x, int y)
{
    int xcenter, ycenter;
    int xzoom, yzoom;

    if (ifp) {
      FB_CK_FB(ifp->i);
      fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
      xcenter = x;
      ycenter = y;
      return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
    } else {
      return 0;
    }
}


int
fb_zoom(struct fb *ifp, int x, int y)
{
    int xcenter, ycenter;
    int xzoom, yzoom;

    if (ifp) {
      FB_CK_FB(ifp->i);

      fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
      xzoom = x;
      yzoom = y;
      return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
    } else {
      return 0;
    }
}


int
fb_scursor(struct fb *ifp, int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    if (ifp) {
	FB_CK_FB(ifp->i);
    }

    /* We could actually implement this but it
     * is probably of no value.
     */
    return 0;
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
