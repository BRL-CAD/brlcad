/*                          G L O B . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file glob.c
	SCCS id:	@(#) glob.c	2.2
	Modified: 	1/5/87 at 16:57:29
	Retrieved: 	1/5/87 at 16:58:15
	SCCS archive:	/vld/moss/src/fbed/s.glob.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) glob.c 2.2, modified 1/5/87 at 16:57:29, archive /vld/moss/src/fbed/s.glob.c";
#endif

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

RGBpixel *menu_addr;
RGBpixel paint;
Point cursor_pos;   /* Current location in image space. */
Point image_center; /* Center of image space. */
Point windo_center; /* Center of screen, image coords. */
Point windo_anchor; /* Saved "windo_center". */
Try		*try_rootp = (Try *) NULL;
bool isSGI = false; /* Are we running on an SGI with graphics. */
char cread_buf[MACROBUFSZ] = { 0 }, *cptr = cread_buf;
char macro_buf[MACROBUFSZ] = { 0 }, *macro_ptr = macro_buf;
int brush_sz = 0;
int gain = 1;
int pad_flag = false;
int remembering = false;
int report_status = true;
int reposition_cursor = true;
int tty = true;
int tty_fd = 0;	/* Keyboard file descriptor. */
int zoom_factor = 1;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
