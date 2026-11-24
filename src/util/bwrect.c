/*                        B W R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/bwrect.c
 *
 * Remove a portion of a potentially huge .bw file.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/mime.h"

#include "rect_util.h"

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    return rect_main(argc, argv, "bwrect",
	"Extract a rectangle from a BW image", BU_MIME_IMAGE_BW);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
