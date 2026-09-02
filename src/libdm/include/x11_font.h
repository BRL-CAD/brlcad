/*                      X 1 1 _ F O N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef DM_X11_FONT_H
#define DM_X11_FONT_H

/* Xlib permits per_char to be NULL when all glyphs share the bounds. */
static inline int
dm_x11_font_width(const XFontStruct *font)
{
    if (!font)
	return 0;
    if (font->per_char)
	return font->per_char->width;
    return font->min_bounds.width;
}

#endif /* DM_X11_FONT_H */
