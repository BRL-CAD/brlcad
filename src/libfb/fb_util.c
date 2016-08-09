/*                       F B _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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
/** @addtogroup libfb */
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

#include "fb_private.h"
#include "fb.h"


/*
 * A routine to simulate the effect of fb_view() by simply
 * storing this information into the fb structure.
 */
int
fb_sim_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    FB_CK_FB(ifp);

    ifp->if_xcenter = xcenter;
    ifp->if_ycenter = ycenter;
    ifp->if_xzoom = xzoom;
    ifp->if_yzoom = yzoom;

    return 0;
}


/*
 * A routine to simulate the effect of fb_getview() by simply
 * reading this information from the fb structure.
 */
int
fb_sim_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FB(ifp);

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


/*
 * A routine to simulate the effect of fb_cursor() by simply
 * storing this information into the fb structure.
 */
int
fb_sim_cursor(fb *ifp, int mode, int x, int y)
{
    FB_CK_FB(ifp);

    ifp->if_cursmode = mode;
    ifp->if_xcurs = x;
    ifp->if_ycurs = y;

    return 0;
}


/*
 * A routine to simulate the effect of fb_getcursor() by simply
 * reading this information from the fb structure.
 */
int
fb_sim_getcursor(fb *ifp, int *mode, int *x, int *y)
{
    FB_CK_FB(ifp);

    *mode = ifp->if_cursmode;
    *x = ifp->if_xcurs;
    *y = ifp->if_ycurs;

    return 0;
}


/* Backward Compatibility Routines */

int
fb_reset(fb *ifp)
{
    if (ifp) {
	FB_CK_FB(ifp);
    }

    return 0;
}


int
fb_viewport(fb *ifp, int UNUSED(left), int UNUSED(top), int UNUSED(right), int UNUSED(bottom))
{
    if (ifp) {
	FB_CK_FB(ifp);
    }

    return 0;
}


int
fb_window(fb *ifp, int x, int y)
{
    int xcenter, ycenter;
    int xzoom, yzoom;

    if (ifp) {
      FB_CK_FB(ifp);
      fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
      xcenter = x;
      ycenter = y;
      return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
    } else {
      return 0;
    }
}


int
fb_zoom(fb *ifp, int x, int y)
{
    int xcenter, ycenter;
    int xzoom, yzoom;

    if (ifp) {
      FB_CK_FB(ifp);

      fb_getview(ifp, &xcenter, &ycenter, &xzoom, &yzoom);
      xzoom = x;
      yzoom = y;
      return fb_view(ifp, xcenter, ycenter, xzoom, yzoom);
    } else {
      return 0;
    }
}


int
fb_scursor(fb *ifp, int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    if (ifp) {
	FB_CK_FB(ifp);
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
