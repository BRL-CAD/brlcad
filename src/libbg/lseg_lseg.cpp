/*                   L S E G _ L S E G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file lseg_lseg.cpp
 *
 * Compute the closest points on the line segments P(s) = (1-s)*P0 + s*P1 and
 * Q(t) = (1-t)*Q0 + t*Q1 for 0 <= s <= 1 and 0 <= t <= 1.  The algorithm is
 * robust even for nearly parallel segments.  Effectively, it uses a conjugate
 * gradient search for the minimum of the squared distance function, which
 * avoids the numerical problems introduced by divisions in the case the
 * minimum is located at an interior point of the domain.  See the document
 * http://www.geometrictools.com/Documentation/DistanceLine3Line3.pdf
 * for details.
 */

#include "common.h"
#include <string.h>

#include "Mathematics/DistSegmentSegment.h"

#include "vmath.h"
#include "bg/lseg.h"

double
bg_distsq_lseg3_lseg3(point_t *c1, point_t *c2,
		  const point_t P0, const point_t P1, const point_t Q0, const point_t Q1)
{

    // TODO - In principle, we could also use the Rational version of this, but
    // it might be significantly slower... probably worth testing, since if
    // there isn't a major penalty in most cases the robustness could be worth
    // it...
    gte::DCPSegment3Segment3<fastf_t> LLQ;

    gte::Vector<3,fastf_t> GTE_P0{P0[0], P0[1], P0[2]};
    gte::Vector<3,fastf_t> GTE_P1{P1[0], P1[1], P1[2]};
    gte::Vector<3,fastf_t> GTE_Q0{Q0[0], Q0[1], Q0[2]};
    gte::Vector<3,fastf_t> GTE_Q1{Q1[0], Q1[1], Q1[2]};

    auto result = LLQ.ComputeRobust(GTE_P0, GTE_P1, GTE_Q0, GTE_Q1);

    double ldist_sq = result.sqrDistance;

    if (c1) {
	VSET(*c1, result.closest[0][0], result.closest[0][1], result.closest[0][2]);
    }
    if (c2) {
	VSET(*c2, result.closest[1][0], result.closest[1][1], result.closest[1][2]);
    }
    return ldist_sq;
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
