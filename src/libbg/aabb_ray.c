/*                      A A B B _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file aabb_ray.c
 *
 * Intersect a ray with an axis aligned bounding box
 *
 */

#include "common.h"
#include <math.h>
#include "vmath.h"
#include "bg/aabb_ray.h"

int bg_isect_aabb_ray(fastf_t *r_min, fastf_t *r_max,
        point_t opt,
        const fastf_t *invdir, /* inverses of dir[] */
        const fastf_t *aabb_min,
        const fastf_t *aabb_max)
{
    register const fastf_t *pt = &opt[0];
    register fastf_t sv;
#define st sv                   /* reuse the register */
    register fastf_t rmin = -MAX_FASTF;
    register fastf_t rmax =  MAX_FASTF;

    /* Start with infinite ray, and trim it down */

    /* X axis */
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
        /* Heading towards smaller numbers */
        /* if (*aabb_min > *pt) miss */
        if (rmax > (sv = (*aabb_min - *pt) * *invdir))
            rmax = sv;
        if (rmin < (st = (*aabb_max - *pt) * *invdir))
            rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
        /* Heading towards larger numbers */
        /* if (*max < *pt) miss */
        if (rmax > (st = (*aabb_max - *pt) * *invdir))
            rmax = st;
        if (rmin < ((sv = (*aabb_min - *pt) * *invdir)))
            rmin = sv;
    } else {
        /*
         * Direction cosines along this axis is NEAR 0,
         * which implies that the ray is perpendicular to the axis,
         * so merely check position against the boundaries.
         */
        if ((*aabb_min > *pt) || (*aabb_max < *pt))
            return 0;   /* MISS */
    }

    /* Y axis */
    pt++; invdir++; aabb_max++; aabb_min++;
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
        if (rmax > (sv = (*aabb_min - *pt) * *invdir))
            rmax = sv;
        if (rmin < (st = (*aabb_max - *pt) * *invdir))
            rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
        if (rmax > (st = (*aabb_max - *pt) * *invdir))
            rmax = st;
        if (rmin < ((sv = (*aabb_min - *pt) * *invdir)))
            rmin = sv;
    } else {
        if ((*aabb_min > *pt) || (*aabb_max < *pt))
            return 0;   /* MISS */
    }

    /* Z axis */
    pt++; invdir++; aabb_max++; aabb_min++;
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
        if (rmax > (sv = (*aabb_min - *pt) * *invdir))
            rmax = sv;
        if (rmin < (st = (*aabb_max - *pt) * *invdir))
            rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
        if (rmax > (st = (*aabb_max - *pt) * *invdir))
            rmax = st;
        if (rmin < ((sv = (*aabb_min - *pt) * *invdir)))
            rmin = sv;
    } else {
        if ((*aabb_min > *pt) || (*aabb_max < *pt))
            return 0;   /* MISS */
    }

    /* If equal, RPP is actually a plane */
    if (rmin > rmax)
        return 0;       /* MISS */

    /* HIT.  Only now do r_min and r_max have to be written */
    (*r_min) = rmin;
    (*r_max) = rmax;
    return 1;           /* HIT */
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
