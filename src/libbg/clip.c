/*                          C L I P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
#include <math.h>
#include <string.h>

#include "bu/str.h"
#include "vmath.h"
#include "bg/clip.h"

/* XXX need to test more thoroughly
   #define ANGLE_EPSILON 0.0001
   #define CLIP_DISTANCE 1000000000.0
*/
#define EPSILON 0.0001
#define CLIP_DISTANCE 100000000.0

static int
clip_code(fastf_t x, fastf_t y, fastf_t clip_min, fastf_t clip_max)
{
    int cval;

    cval = 0;
    if (x < clip_min)
	cval |= 01;
    else if (x > clip_max)
	cval |= 02;

    if (y < clip_min)
	cval |= 04;
    else if (y > clip_max)
	cval |= 010;

    return cval;
}


int
bg_lseg_clip(fastf_t *xp1, fastf_t *yp1, fastf_t *xp2, fastf_t *yp2, fastf_t clip_min, fastf_t clip_max)
{
    char code1, code2;

    code1 = clip_code(*xp1, *yp1, clip_min, clip_max);
    code2 = clip_code(*xp2, *yp2, clip_min, clip_max);

    while (code1 || code2) {
	if (code1 & code2)
	    return 1;   /* No part is visible */

	/* SWAP codes, X's, and Y's */
	if (code1 == 0) {
	    char ctemp;
	    fastf_t temp;

	    ctemp = code1;
	    code1 = code2;
	    code2 = ctemp;

	    temp = *xp1;
	    *xp1 = *xp2;
	    *xp2 = temp;

	    temp = *yp1;
	    *yp1 = *yp2;
	    *yp2 = temp;
	}

	if (code1 & 01) {
	    /* Push toward left edge */
	    *yp1 = *yp1 + (*yp2-*yp1)*(clip_min-*xp1)/(*xp2-*xp1);
	    *xp1 = clip_min;
	} else if (code1 & 02) {
	    /* Push toward right edge */
	    *yp1 = *yp1 + (*yp2-*yp1)*(clip_max-*xp1)/(*xp2-*xp1);
	    *xp1 = clip_max;
	} else if (code1 & 04) {
	    /* Push toward bottom edge */
	    *xp1 = *xp1 + (*xp2-*xp1)*(clip_min-*yp1)/(*yp2-*yp1);
	    *yp1 = clip_min;
	} else if (code1 & 010) {
	    /* Push toward top edge */
	    *xp1 = *xp1 + (*xp2-*xp1)*(clip_max-*yp1)/(*yp2-*yp1);
	    *yp1 = clip_max;
	}

	code1 = clip_code(*xp1, *yp1, clip_min, clip_max);
    }

    return 0;
}


int
bg_ray_vclip(point_t a, point_t b, fastf_t *min_pt, fastf_t *max_pt)
{
    static vect_t diff;
    static double sv;
    static double st;
    static double mindist, maxdist;
    fastf_t *pt = &a[0];
    fastf_t *dir = &diff[0];
    int i;

    mindist = -CLIP_DISTANCE;
    maxdist = CLIP_DISTANCE;
    VSUB2(diff, b, a);

    for (i = 0; i < 3; i++, pt++, dir++, max_pt++, min_pt++) {
	if (*dir < -EPSILON) {
	    sv = (*min_pt - *pt) / *dir;
	    if (sv < 0.0)
		return 0;       /* MISS */

	    st = (*max_pt - *pt) / *dir;
	    V_MAX(mindist, st);
	    V_MIN(maxdist, sv);

	}  else if (*dir > EPSILON) {
	    st = (*max_pt - *pt) / *dir;
	    if (st < 0.0)
		return 0;       /* MISS */

	    sv = (*min_pt - *pt) / *dir;
	    V_MAX(mindist, sv);
	    V_MIN(maxdist, st);
	} else {
	    /*
	     * If direction component along this axis is NEAR 0,
	     * (i.e., this ray is aligned with this axis),
	     * merely check against the boundaries.
	     */
	    if ((*min_pt > *pt) || (*max_pt < *pt))
		return 0;       /* MISS */
	}
    }
    if (mindist >= maxdist)
	return 0;       /* MISS */

    if (mindist > 1 || maxdist < 0)
	return 0;       /* MISS */

    if (mindist <= 0 && maxdist >= 1)
	return 1;       /* HIT, no clipping needed */

    /* Don't grow one end of a contained segment */
    V_MAX(mindist, 0);
    V_MIN(maxdist, 1);

    /* Compute actual intercept points */
    VJOIN1(b, a, maxdist, diff);                /* b must go first */
    VJOIN1(a, a, mindist, diff);
    return 1;           /* HIT */
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
