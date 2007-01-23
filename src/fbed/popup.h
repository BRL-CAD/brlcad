/*                         P O P U P . H
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
/** @file popup.h
	SCCS id:	@(#) popup.h	2.1
	Modified: 	12/9/86 at 15:55:41
	Retrieved: 	12/26/86 at 21:53:44
	SCCS archive:	/vld/moss/src/fbed/s.popup.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#define INCL_POPUP
#define MENU_FONT	"/usr/lib/vfont/fix.6"

typedef struct
	{
	int p_x;
	int p_y;
	}
Point;

typedef struct
	{
	Point r_origin;
	Point r_corner;
	}
Rectangle;

typedef struct
	{
	RGBpixel color;
	void (*func)();
	char *label;
	} Seg;

typedef struct
	{
	int wid, hgt;
	int n_segs, seg_hgt;
	int max_chars, char_base;
	int on_flag, cmap_base;
	int last_pick;
	Rectangle rect;
	RGBpixel *outlines, *touching, *selected;
	RGBpixel *under, *image;
	char *title, *font;
	Seg		*segs;
	ColorMap	cmap;
	}
Menu;

typedef struct
	{
	RGBpixel  *n_buf;
	int n_wid;
	int n_hgt;
	}
Panel;

#define RESERVED_CMAP  ((pallet.cmap_base+pallet.n_segs+1)*2)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
