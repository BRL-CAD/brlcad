/*                       S G I _ D E P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file sgi_dep.c
	SCCS id:	@(#) sgi_dep.c	2.3
	Modified: 	1/5/87 at 16:57:35
	Retrieved: 	1/5/87 at 16:58:20
	SCCS archive:	/vld/moss/src/fbed/s.sgi_dep.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) sgi_dep.c 2.3, modified 1/5/87 at 16:57:35, archive /vld/moss/src/fbed/s.sgi_dep.c";
#endif

#include "common.h"

#if HAS_SGIGL

#include <stdio.h>
#include <device.h>

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

int
sgi_Empty()
	{	short	val;
		long	dev;
	winattach();
	if( qtest() )
		{
		if( (dev = qread( &val )) == KEYBD )
			{
			qenter( (short) dev, val );
			return 0;
			}
		else
			qenter( (short) dev, val );
		}
	return 1;
	}

void
sgi_Init()
	{
	qdevice( KEYBD );
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	}

int
sgi_Getchar()
	{	short	val;
	winattach();
	for( ; ; )
		{	long	dev = qread( &val );
		switch( dev )
			{
		case KEYBD :
			return (int) val;
			}
		}
	}

int
sgi_Mouse_Pos( pointp )
register Point *pointp;
	{	static Point mouse;
		Point image;
		long	ox, oy;
		int change_flag = false;
	if( getbutton( MIDDLEMOUSE ) )
		{
		while( getbutton( MIDDLEMOUSE ) )
			; /* Wait for user to let go. */
		return 1;
		}
	image.p_x = getvaluator( MOUSEX );
	image.p_y = getvaluator( MOUSEY );
	if( image.p_x != mouse.p_x || image.p_y != mouse.p_y )
		mouse = image; /* Only use mouse if position changes. */
	else
		return -1;

	/* Transform screen to image coords. */
	getorigin( &ox, &oy );
	image.p_x -= ox;
	image.p_y -= oy;

	/* Ignore input if mouse is off of image. */
	if(	image.p_x < 0 || image.p_x >= fb_getwidth( fbp )
	   ||	image.p_y < 0 || image.p_y >= fb_getheight( fbp )
		)
		return -1;

	/* Adjust coordinates per windowing and zoom factor. */
	image.p_x = windo_anchor.p_x +
			(image.p_x-image_center.p_x)/zoom_factor;
	image.p_y = windo_anchor.p_y +
			(image.p_y-image_center.p_y)/zoom_factor;

	/* If position indicated by mouse is different than the current
		point position, then update current point. */
	if( image.p_x != pointp->p_x )
		{
		pointp->p_x = image.p_x;
		change_flag = true;
		}
	if( image.p_y != pointp->p_y )
		{
		pointp->p_y = image.p_y;
		change_flag = true;
		}
	return change_flag ? 2 : -1;
	}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
