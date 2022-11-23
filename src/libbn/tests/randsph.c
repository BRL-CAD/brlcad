/*                      R A N D S P H . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "vmath.h"
#include "bn/rand.h"

/* TODO - make this into a proper test.  Right now it just puts out a bunch of
 * MGED commands to make spheres so the results can be visually inspected... */

int
main(int argc, const char *argv[])
{
    int i = 0;
    point_t *parray;
    point_t center = VINIT_ZERO;
    fastf_t radius = 1.0;

    bu_setprogname(argv[0]);

    if (argc != 1) {
	bu_exit(1, "ERROR: unknown args [%s]\n", argv[0]);
    }

    parray = (point_t *)bu_malloc(500 * sizeof(point_t), "point array");
    for(i = 0; i < 500; i++) {
	(void)bn_rand_sph_sample(parray[i], center, radius);
    }

    for (i=0; i < 500; i++) {
	printf("in r_unit_pnt%d.s sph %f %f %f 0.01\n", i, parray[i][0], parray[i][1], parray[i][2]);
    }
    bu_free(parray, "free array");

    radius = 10.0;
    parray = (point_t *)bu_malloc(5000 * sizeof(point_t), "point array");
    for(i = 0; i < 5000; i++) {
	(void)bn_rand_sph_sample(parray[i], center, radius);
    }

    for (i=0; i < 5000; i++) {
	printf("in r_10_pnt%d.s sph %f %f %f 0.1\n", i, parray[i][0], parray[i][1], parray[i][2]);
    }
    bu_free(parray, "free array");


    VSET(center, 50, 50, 30);
    parray = (point_t *)bu_malloc(5000 * sizeof(point_t), "point array");
    for(i = 0; i < 5000; i++) {
	(void)bn_rand_sph_sample(parray[i], center, radius);
    }

    for (i=0; i < 5000; i++) {
	printf("in r_offset_pnt%d.s sph %f %f %f 0.1\n", i, parray[i][0], parray[i][1], parray[i][2]);
    }
    bu_free(parray, "free array");


    return 1;
}


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
