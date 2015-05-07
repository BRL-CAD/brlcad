/*                         G M . H
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

/** @addtogroup libgm
 *
 * BRL-CAD geometry library. This library is intended for generic
 * geometry algorithms, such as point-in-polygon, does a line
 * intersect a sphere, ear clipping triangulation of a polygon, etc.
 *
 * Algorithms in this library should not require solid raytracing
 * of the BRL-CAD CSG boolean hierarchy - routines using those high
 * level constructs should be in libanalyze, which uses librt to
 * do the necessary raytracing.  libgm is lower level, and should
 * depend only on the numerics library (libbn) and the libbu utility
 * (libbu)
 *
 * Strictly numerical algorithms, which do not involve 3D geometry
 * concepts, belong in libbn.  An example of something that would belong
 * in libbn would be sparse matrix solving.
 *
 * The functionality provided by this library is specified in the gm.h
 * header or appropriate included files from the ./gm subdirectory.
 */
/** @{ */
/** @brief Header file for the BRL-CAD Geometry Library, LIBGM.*/
/** @file gm.h */
/** @} */

#ifndef GM_H
#define GM_H

#include "common.h"

#include "gm/defines.h"
#include "gm/chull.h"
#include "gm/obr.h"
#include "gm/polygon.h"
#include "gm/tri_ray.h"
#include "gm/tri_tri.h"

#endif /* GM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
