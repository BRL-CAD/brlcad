/*                      S C A N L I N E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file rt/scanline.h
 *
 * Header of scanline functions, common to viewmlt.c and view.c
 *
 */

#ifndef RT_SCANLINE_H
#define RT_SCANLINE_H

#include "bu.h"

struct scanline {
    int	sl_left;		/* # pixels left on this scanline */
    unsigned char *sl_buf;	/* ptr to buffer for scanline */
};

void free_scanlines(int, struct scanline*);
struct scanline* alloc_scanlines(int);

#endif /* RT_SCANLINE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
