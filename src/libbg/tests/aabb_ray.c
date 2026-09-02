/*                      A A B B _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 */

#include "common.h"

#include <math.h>

#include "bu.h"
#include "bg/aabb_ray.h"


static int
test_inverse_direction(void)
{
    vect_t dir = VINIT_ZERO;
    vect_t original_dir = VINIT_ZERO;
    vect_t invdir = VINIT_ZERO;

    VSET(dir, 0.5 * SQRT_SMALL_FASTF, 1.0, -2.0);
    VMOVE(original_dir, dir);
    bg_ray_invdir(&invdir, dir);

    return (!VNEAR_EQUAL(dir, original_dir, SMALL_FASTF)
	|| !isinf(invdir[X])
	|| !NEAR_EQUAL(invdir[Y], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(invdir[Z], -0.5, SMALL_FASTF));
}


static int
test_line_intersections(void)
{
    point_t origin = VINIT_ZERO;
    vect_t dir = VINIT_ZERO;
    vect_t invdir = VINIT_ZERO;
    point_t aabb_min = VINIT_ZERO;
    point_t aabb_max = VINIT_ZERO;
    fastf_t rmin = 0.0;
    fastf_t rmax = 0.0;

    VSET(dir, 1.0, 0.0, 0.0);
    bg_ray_invdir(&invdir, dir);

    VSET(aabb_min, 5.0, -1.0, -1.0);
    VSET(aabb_max, 7.0, 1.0, 1.0);
    if (!bg_isect_aabb_ray(&rmin, &rmax, origin, invdir, aabb_min, aabb_max)
	|| !NEAR_EQUAL(rmin, 5.0, SMALL_FASTF)
	|| !NEAR_EQUAL(rmax, 7.0, SMALL_FASTF)) {
	return 1;
    }

    VSET(aabb_min, -7.0, -1.0, -1.0);
    VSET(aabb_max, -5.0, 1.0, 1.0);
    if (!bg_isect_aabb_ray(&rmin, &rmax, origin, invdir, aabb_min, aabb_max)
	|| !NEAR_EQUAL(rmin, -7.0, SMALL_FASTF)
	|| !NEAR_EQUAL(rmax, -5.0, SMALL_FASTF)) {
	return 1;
    }

    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    return test_inverse_direction() || test_line_intersections();
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
