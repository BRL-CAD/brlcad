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
 * Intersect a ray with an 'xxx' primitive object.
 *
 * Adding a new solid type:
 *
 * Design disk record
 *
 * define rt_xxx_internal --- parameters for solid
 * define xxx_specific --- raytracing form, possibly w/precomputed terms
 * define rt_xxx_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 * code import/export4/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 * edit db.h add solidrec s_type define
 * edit rtgeom.h to add rt_xxx_internal
 * edit magic.h to add RT_XXX_INTERNAL_MAGIC
 * edit table.c:
 *	RT_DECLARE_INTERFACE()
 *	struct rt_functab entry
 *	rt_id_solid()
 * edit raytrace.h to make ID_XXX, increment ID_MAXIMUM
 * edit db_scan.c to add the new solid to db_scan()
 * edit CMakeLists.txt to add g_xxx.c to compile
 *
 * go to src/libwdb and create mk_xxx() routine
 * go to src/conv and edit g2asc.c and asc2g.c to support the new solid
 * go to src/librt and edit tcl.c to add the new solid to
 *	rt_solid_type_lookup[]
 *	also add the interface table and to rt_id_solid() in table.c
 * go to src/mged and create the edit support
 *
 */

#ifndef LIBRT_PRIMITIVES_XXX_XXX_H
#define LIBRT_PRIMITIVES_XXX_XXX_H

#include "common.h"

#include "bu.h"
#include "bn.h"


/* EXAMPLE_INTERNAL shows how one would store the values that describe
 * or implement this primitive.  The internal structure should go into
 * rtgeom.h, the magic number should go into magic.h, and of course
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
