/*                          G L O B . C
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
/** @file fbed/glob.c
 *
 */

#include "common.h"

#include <stdio.h>

#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./try.h"
#include "./extern.h"

RGBpixel *menu_addr;
RGBpixel paint;
Point cursor_pos;   /* Current location in image space. */
Point image_center; /* Center of image space. */
Point windo_center; /* Center of screen, image coords. */
Point windo_anchor; /* Saved "windo_center". */
Try		*try_rootp = (Try *) NULL;
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
