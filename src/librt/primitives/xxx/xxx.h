/*                           X X X . H
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file xxx.h
 *
 * Intersect a ray with an 'xxx' primitive object.  Nearly everything
 * in this file (except for the xxx_specific struct) belongs somewhere
 * else.  See comments in xxx.c for details.
 *
 */

#ifndef LIBRT_PRIMITIVES_XXX_XXX_H
#define LIBRT_PRIMITIVES_XXX_XXX_H

#include "common.h"

#include "bu/parse.h"
#include "bn.h"


/* EXAMPLE_INTERNAL shows how one would store the values that describe
 * or implement this primitive.  The internal structure should go into
 * rtgeom.h, the magic number should go into bu/magic.h, and of course
 * the #if wrapper should go away.
 */
#if defined(EXAMPLE_INTERNAL) || 1

/* parameters for solid, internal representation */
struct rt_xxx_internal {
    uint32_t magic;
    vect_t v;
};

#  define RT_XXX_INTERNAL_MAGIC 0x78787878 /* 'xxxx' */
#  define RT_XXX_CK_MAGIC(_p) BU_CKMAG(_p, RT_XXX_INTERNAL_MAGIC, "rt_xxx_internal")

/* should set in raytrace.h to ID_MAX_SOLID and increment the max */
#  define ID_XXX 0

#endif

/* ray tracing form of solid, including precomputed terms */
struct xxx_specific {
    vect_t xxx_V;
};

#endif /* LIBRT_PRIMITIVES_XXX_XXX_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
