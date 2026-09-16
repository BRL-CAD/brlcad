/*                       F B T E X T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file fbtext.h
 *
 * Small wrapper around bu/vfont for drawing anti-aliased text into a libfb
 * framebuffer.  Coordinates use the libfb convention (origin lower-left).
 *
 */

#ifndef LAUNCHER_FBTEXT_H
#define LAUNCHER_FBTEXT_H

#include "common.h"

#include "bu/vfont.h"
#include "dm.h"

__BEGIN_DECLS

struct fbtext {
    struct vfont *vfp;
};

/* Load a font by name (NULL for the default).  Returns 0 on success. */
extern int fbtext_open(struct fbtext *t, const char *fontname);
extern void fbtext_close(struct fbtext *t);

/* Nominal line height, in pixels, for layout. */
extern int fbtext_line_height(struct fbtext *t);

/* Rendered pixel width of a string. */
extern int fbtext_string_width(struct fbtext *t, const char *s);

/* Draw string with lower-left baseline near (x,y) in the given color. */
extern void fbtext_draw(struct fb *fbp, struct fbtext *t, int x, int y, const char *s, const RGBpixel color);

__END_DECLS

#endif /* LAUNCHER_FBTEXT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
