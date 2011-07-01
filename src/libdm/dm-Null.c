/*                       D M - N U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file libdm/dm-Null.c
 *
 */

#include "common.h"
#include "bio.h"

#define PLOTBOUND 1000.0  /* Max magnification in Rot matrix */
#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "dm-Null.h"


int
Nu_int0(void)
{
    return TCL_OK;
}


void
Nu_void(void)
{
    return;
}


unsigned int
Nu_unsign(void)
{
    return TCL_OK;
}


HIDDEN int
Nu_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *UNUSED(data))
{
    if (!dmp || !callback_function) {
	bu_log("WARNING: dmp or callback_function is NULL\n");
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
Nu_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
Nu_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b==%d/%d/%d)\n", r, g, b);
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
Nu_getDisplayImage(struct dm *UNUSED(dmp), unsigned char **UNUSED(image))
{
    return 0;
}

struct dm dm_Null = {
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_draw,
    Nu_fg,
    Nu_bg,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_getDisplayImage, /* display to image function */
    Nu_void,
    0,
    0,				/* no displaylist */
    0,				/* no stereo */
    PLOTBOUND,			/* zoom-in limit */
    1,				/* bound flag */
    "nu",
    "Null Display",
    DM_TYPE_NULL,
    0,/* top */
    0,/* width */
    0,/* height */
    0,/* bytes per pixel */
    0,/* bits per channel */
    0,
    0,
    0,
    0,
    {0, 0},
    {0, 0, 0, 0, 0},		/* bu_vls path name*/
    {0, 0, 0, 0, 0},		/* bu_vls full name drawing window */
    {0, 0, 0, 0, 0},		/* bu_vls short name drawing window */
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {0.0, 0.0, 0.0},		/* clipmin */
    {0.0, 0.0, 0.0},		/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    0				/* Tcl interpreter */
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
