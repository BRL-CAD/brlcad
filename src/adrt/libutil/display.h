/*                     D I S P L A Y . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2008 United States Government as represented by
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
/** @file display.h
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

#ifdef HAVE_CONFIG_H
# include "brlcad_config.h"
#endif

#ifdef HAVE_SDL
#include <SDL.h>

extern SDL_Surface *util_display_screen;
extern SDL_Surface *util_display_buffer;
#endif

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

extern void util_display_init(int w, int h);
extern void util_display_free(void);
extern void util_display_draw(void *frame);
extern void util_display_text(const char *text, int x, int y, int jh, int jv);
extern void util_display_flush(void);
extern void util_display_cross(void);
extern void util_display_flip(void);
extern void util_display_editor(char **content_buffer, int *content_lines, char **console_buffer, int *console_lines, void (*fcb_process)(char *content, char *response));

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
