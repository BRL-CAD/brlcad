/*                     D I S P L A Y . H
 *
 * @file display.h
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
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
 *
 *  Comments -
 *      Utilities Library - Display Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _UTIL_DISPLAY_H
#define _UTIL_DISPLAY_H

#include <SDL.h>

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

void util_display_init(int w, int h);
void util_display_free(void);
void util_display_draw(void *frame);
void util_display_text(char *text, int x, int y, int jh, int jv);
void util_display_flush(void);
void util_display_cross(void);
void util_display_flip(void);
void util_display_editor(char **content_buffer, int *content_lines, char **console_buffer, int *console_lines, void (*fcb_process)(char *content, char *response));

extern SDL_Surface *util_display_screen;
extern SDL_Surface *util_display_buffer;

#endif
