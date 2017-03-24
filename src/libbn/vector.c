/*                        V E C T O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup plot */
/** @{ */
/** @file libbn/vector.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "bn/mat.h"
#include "bn/plot3.h"

/**
 *@brief
 * Draw a vector between points "from" and "to", with the option of
 * having an arrowhead on either or both ends.
 *
 * The fromheadfract and toheadfract values indicate the length of the
 * arrowheads relative to the length of the vector to-from.  A typical
 * value is 0.1, making the head 10% of the size of the vector.  The
 * sign of the "fract" values indicates the pointing direction.
 * Positive points towards the "to" point, negative points towards
 * "from".  Upon return, the virtual pen is left at the "to" position.
 */
void
tp_3vector(FILE *plotfp, fastf_t *from, fastf_t *to, double fromheadfract, double toheadfract)
{
    register fastf_t len;
    register fastf_t hooklen;
    vect_t diff;
    vect_t c1, c2;
    vect_t h1, h2;
    vect_t backup;
    point_t tip;

    pdv_3line(plotfp, from, to);
    /* "pen" is left at "to" position */

    VSUB2(diff, to, from);
    if ((len = MAGNITUDE(diff)) < SMALL)  return;
    VSCALE(diff, diff, 1/len);
    bn_vec_ortho(c1, diff);
    VCROSS(c2, c1, diff);

    if (!ZERO(fromheadfract)) {
	hooklen = fromheadfract*len;
	VSCALE(backup, diff, -hooklen);

	VSCALE(h1, c1, hooklen);
	VADD3(tip, from, h1, backup);
	pdv_3move(plotfp, from);
	pdv_3cont(plotfp, tip);

	VSCALE(h2, c2, hooklen);
	VADD3(tip, from, h2, backup);
	pdv_3move(plotfp, tip);
    }
    if (!ZERO(toheadfract)) {
	hooklen = toheadfract*len;
	VSCALE(backup, diff, -hooklen);

	VSCALE(h1, c1, hooklen);
	VADD3(tip, to, h1, backup);
	pdv_3move(plotfp, to);
	pdv_3cont(plotfp, tip);

	VSCALE(h2, c2, hooklen);
	VADD3(tip, to, h2, backup);
	pdv_3move(plotfp, tip);
    }
    /* Be certain "pen" is left at "to" position */
    if (!ZERO(fromheadfract) || !ZERO(toheadfract))
	pdv_3cont(plotfp, to);

}

void
PL_FORTRAN(f3vect, F3VECT)(FILE **fp, float *fx, float *fy, float *fz, float *tx, float *ty, float *tz, float *fl , float *tl)
{
    point_t from, to;

    VSET(from, *fx, *fy, *fz);
    VSET(to, *tx, *ty, *tz);
    tp_3vector(*fp, from, to, *fl, *tl);
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
