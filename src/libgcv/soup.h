/*                       S O U P . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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

/** @file libgcv/soup.h
 *
 * BoT soup stuff
 *
 */

/* hijack the top four bits of mode. For these operations, the mode should
 * necessarily be 0x02 */
#define INSIDE		0x01
#define OUTSIDE		0x02
#define SAME		0x04
#define OPPOSITE	0x08
#define INVERTED	0x10

#define SOUP_MAGIC	0x534F5550	/* SOUP, 32b */
#define SOUP_CKMAG(_ptr) BU_CKMAG(_ptr, SOUP_MAGIC, "soup")

struct face_s {
    point_t vert[3], min, max;
    plane_t plane;
    uint32_t foo;
};


struct soup_s {
    uint32_t magic;
    struct face_s *faces;
    unsigned long int nfaces, maxfaces;
};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
