/*                            F B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt/fb.c
 *	Author:		Gary S. Moss
 */

#include "common.h"

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./screen.h"
int		zoom;	/* Current zoom factor of frame buffer.		*/
int		fb_Setup(char *file, int size);
void		fb_Zoom_Window(void);

/*	f b _ S e t u p ( )						*/
int
fb_Setup(char *file, int size)
{
    if ( BU_STR_EQUAL( file, "/dev/remote" ) )
	file = "/dev/debug";
    prnt_Event( "Opening device..." );

    if (((fbiop = fb_open(file[0] == '\0' ? NULL : file, size, size)) == FBIO_NULL) ||
	(fb_ioinit( fbiop ) == -1) ||
	(fb_wmap( fbiop, COLORMAP_NULL ) == -1))
	return	0;

    (void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
    (void) fb_cursor( fbiop, 1, size/2, size/2 );

    prnt_Event( (char *) NULL );
    return	1;
}

/*	f b _ Z o o m _ W i n d o w ( )					*/
void
fb_Zoom_Window(void)
{
    int	xpos, ypos;
    zoom = fb_getwidth( fbiop ) / grid_sz;
    xpos = ypos = grid_sz / 2;
    if ( tty )
	prnt_Event( "Zooming..." );
    if ( fb_zoom( fbiop, zoom, zoom ) == -1 )
	bu_log( "Can not set zoom <%d,%d>.\n", zoom, zoom );
    if ( x_fb_origin >= grid_sz )
	xpos += x_fb_origin;
    if ( y_fb_origin >= grid_sz )
	ypos += y_fb_origin;
    if ( tty )
	prnt_Event( "Windowing..." );
    if ( fb_viewport( fbiop, 0, 0, grid_sz, grid_sz ) == -1 )
	bu_log( "Can not set viewport {<%d,%d>,<%d,%d>}.\n",
		0, 0, grid_sz, grid_sz
	    );
    if ( fb_window( fbiop, xpos, ypos ) == -1 )
	bu_log( "Can not set window <%d,%d>.\n", xpos, ypos );
    if ( tty )
	prnt_Event( (char *) NULL );
    return;
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
