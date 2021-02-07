/*                           X X X . H
 * BRL-CAD
 *
 * Copyright (c) 2012-2021 United States Government as represented by
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

#include "bn.h"


/* ray tracing form of solid, including any precomputed terms.  not
 * strictly necessary in addition to the rt_xxx_internal structure
 * (see below), but often useful for performance.
 */
struct xxx_specific {
    vect_t xxx_V;
};


/*
 * BEGIN EXAMPLE CODE THAT BELONGS ELSEWHERE
 *
 * The remainder of this header has example code that belongs in other
 * files, but that are included here for completeness.  Remove the
 * logic from this header when you've added them to their proper
 * location.
 */

/* For rt/magic.h: magic number registration for this type */
#define RT_XXX_INTERNAL_MAGIC 0x78787878 /* 'xxxx' */

/* For rt/geom.h: parameters for solid, internal representation */
struct rt_xxx_internal {
    uint32_t magic;
    vect_t v;
};

/* For rt/geom.h: validation routines for the internal struct */
#define RT_XXX_CK_MAGIC(_p) BU_CKMAG(_p, RT_XXX_INTERNAL_MAGIC, "rt_xxx_internal")

/* For rt/db5.h: Gets ID_MAX_SOLID value, then ID_MAX_SOLID and
 * ID_MAXIMUM are incremented as needed.
 */
#  define ID_XXX 0

/*
 * END EXAMPLE CODE THAT BELONGS ELSEWHERE
 */

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
