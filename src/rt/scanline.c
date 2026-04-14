/*                      S C A N L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file rt/scanline.c
*
 * Source and implementation of scanline functions.
 *
 */

#include "scanline.h"

#include "bu/malloc.h"

void
free_scanlines(int nlines, struct scanline* sl)
{
    register int y;

    for (y = 0; y < nlines; y++)  {
	if (sl[y].sl_buf)  {
	    bu_free(sl[y].sl_buf, "sl_buf scanline buffer");
		sl[y].sl_buf = (unsigned char *) 0;
	}
    }
    bu_free((char*) sl, "struct scanline[height]");
    sl = (struct scanline*) 0;
}

struct scanline *
alloc_scanlines(int nlines)
{
    struct scanline *temp;

	temp = (struct scanline *)bu_calloc(
	    nlines, sizeof(struct scanline),
	    "struct scanline[height]" );
    return temp;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
