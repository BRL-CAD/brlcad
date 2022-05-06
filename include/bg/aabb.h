/*                         A A B B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file aabb.h */
/** @addtogroup bg_aabb */
/** @{ */

/**
 * @brief
 *
 * Routines for calculating axis-aligned bounding boxes
 *
 */

#ifndef BG_AABB_H
#define BG_AABB_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"
#include "bg/geom.h"

__BEGIN_DECLS

BG_EXPORT extern int
bg_tor_bbox(point_t *min, point_t *max, struct bg_torus *t);

__END_DECLS

#endif  /* BG_AABB_RAY_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
