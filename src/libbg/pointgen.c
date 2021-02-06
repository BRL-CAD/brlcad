/*                      P O I N T G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file pointgen.c
 *
 * Brief description
 *
 */

#include "common.h"
#include <stdlib.h>

#include "bu/malloc.h"
#include "bn/numgen.h"
#include "bg/pointgen.h"

size_t
bg_sph_sample(point_t *pnts, size_t cnt, const point_t center, const fastf_t radius, bn_numgen n)
{
    size_t i = 0;
    size_t ret = 0;

    ret = bn_sph_sample(pnts, cnt, n);

    if (ret != cnt) {
	bu_log("Unable to generate %d points (%d generated), aborting\n", cnt, ret);
	return 0;
    }

    for (i = 0; i < cnt; i++) {
	/* If we've got a non-unit sph radius, scale the point */
	if (!NEAR_EQUAL(radius, 1.0, SMALL_FASTF)) {
	    VSCALE(sample, sample, radius);
	}

	/* If we've got a non-zero sph center, translate the point */
	if (!VNEAR_ZERO(sample, SMALL_FASTF)) {
	    VADD2(sample, sample, center);
	}
    }

    return cnt;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
