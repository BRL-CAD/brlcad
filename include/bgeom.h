/*                         B G E O M . H
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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

/** @addtogroup libbgeom */
/** @{ */
/** @file bgeom.h
 *
 * BRL-CAD geometry library. This library is intended for generic
 * geometry algorithms, such as point-in-polygon, does a line
 * intersect a sphere, ear clipping triangulation of a polygon, etc.
 *
 * Algorithms in this library should not require solid raytracing
 * of the BRL-CAD CSG boolean hierarchy - routines using those high
 * level constructs should be in libanalyze, which uses librt to
 * do the necessary raytracing.  libbgeom is lower level, and should
 * depend only on the numerics library (libbn) and the libbu utility
 * (libbu)
 *
 * Strictly numerical algorithms, which do not involve 3D geometry
 * concepts, belong in libbn.  An example of something that would belong
 * in libbn would be sparse matrix solving.
 */

#ifndef BGEOM_H
#define BGEOM_H

#include "common.h"

#include "bgeom/defines.h"
#include "bgeom/chull.h"
#include "bgeom/obr.h"
#include "bgeom/polygon.h"
#include "bgeom/tri_ray.h"
#include "bgeom/tri_tri.h"

#endif /* BGEOM_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
