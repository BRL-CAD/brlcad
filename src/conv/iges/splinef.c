/*                       S P L I N E F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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

#include "vmath.h"

#include "bu/defines.h"

/*
 * Evaluate a cubic polynomial at parameter s.
 *
 * Given the four coefficients c[0..3] of a cubic (constant, linear,
 * quadratic, and cubic terms), returns c[0] + c[1]*s + c[2]*s^2 +
 * c[3]*s^3.  Used to evaluate a single coordinate of an IGES
 * parametric spline (entity type 112) segment.
 */
fastf_t
splinef(fastf_t c[4], fastf_t s)
{
    int i;
    fastf_t retval;
    double stopow = 1.0;

    retval = c[0];
    for (i = 1; i < 4; i++) {
	stopow *= s;
	if (!ZERO(c[i]))
	    retval += c[i]*stopow;
    }

    return retval;
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
