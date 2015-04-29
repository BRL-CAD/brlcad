/*                          X R A Y . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file xray.h
 *
 * All necessary information about a ray.
 */

#ifndef RT_XRAY_H
#define RT_XRAY_H

#include "common.h"
#include "vmath.h"

__BEGIN_DECLS

/**
 * @brief Primary ray data structure
 *
 * Not called just "ray" to prevent conflicts with VLD stuff.
 */
struct xray {
    uint32_t            magic;
    int                 index;          /**< @brief Which ray of a bundle */
    point_t             r_pt;           /**< @brief Point at which ray starts */
    vect_t              r_dir;          /**< @brief Direction of ray (UNIT Length) */
    fastf_t             r_min;          /**< @brief entry dist to bounding sphere */
    fastf_t             r_max;          /**< @brief exit dist from bounding sphere */
};
#define RAY_NULL        ((struct xray *)0)
#define RT_CK_RAY(_p) BU_CKMAG(_p, RT_RAY_MAGIC, "struct xray");

/**
 * This plural xrays structure is a bu_list based container designed
 * to hold a list or bundle of xray(s). This bundle is utilized by
 * rt_shootrays() through its application bundle input.
 */
struct xrays
{
    struct bu_list l;
    struct xray ray;
};


__END_DECLS

#endif /* RT_XRAY_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
