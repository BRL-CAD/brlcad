/*                       T I E P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file tieprivate.h
 *
 * private stuff for libtie/librender
 *
 */

#ifndef LIBRT_PRIMITIVES_BOT_TIEPRIVATE_H
#define LIBRT_PRIMITIVES_BOT_TIEPRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The last three bits of the 'b' field in the kdtree are used to
 * store data about the object.  0x4L marks if the node has children
 * (is set to 0 for a leaf), the last two are the encoding for the
 * splitting plane.
 */
#define TIE_HAS_CHILDREN(bits) (bits & (uint32_t)0x4L)
#define TIE_SET_HAS_CHILDREN(bits) (bits | (uint32_t)0x4L)

struct tie_geom_s {
    struct tie_tri_s **tri_list; /* 4-bytes or 8-bytes */
    uint32_t tri_num; /* 4-bytes */
};

#ifdef _WIN32
# undef near
# undef far
#endif

struct tie_stack_s {
    struct tie_kdtree_s *node; /* 4-bytes or 8-bytes */
    TFLOAT near; /* 4-bytes or 8-bytes */
    TFLOAT far; /* 4-bytes or 8-bytes */
};

#ifdef __cplusplus
}
#endif

#endif /* LIBRT_PRIMITIVES_BOT_TIEPRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
