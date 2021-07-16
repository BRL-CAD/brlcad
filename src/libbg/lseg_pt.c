// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2020
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 4.0.2019.08.13

// This file is basically
// https://www.geometrictools.com/GTE/Mathematics/DistPointSegment.h
// except we are using vmath.h types and the C programming language

#include "common.h"
#include "vmath.h"
#include "bg/lseg.h"

double
bg_distsq_lseg3_pt(point_t *c, const point_t P0, const point_t P1, const point_t Q)
{
    double ldist_sq;
    vect_t closest, dir, diff;

    // Note:  the dir vector is not unit length.  The normalization is
    // deferred until it is needed.
    VSUB2(dir, P1, P0);
    VSUB2(diff, Q, P1);
    double t = VDOT(dir, diff);
    if (t > 0.0 || NEAR_EQUAL(t, 0.0, SMALL_FASTF)) {
	VMOVE(closest, P1);
    } else {
	VSUB2(diff, Q, P0);
	t = VDOT(dir, diff);
	if (t < 0.0 || NEAR_EQUAL(t, 0.0, SMALL_FASTF)) {
	    VMOVE(closest, P0);
	} else {
	    double sqrLength = VDOT(dir, dir);
	    if (sqrLength > 0.0) {
		point_t ra, rb;
		t /= sqrLength;
		VSCALE(ra, P0, (1.0 - t));
		VSCALE(rb, P1, t);
		VADD2(closest, ra, rb);
	    } else {
		VMOVE(closest, P0);
	    }
	}
    }

    VSUB2(diff, Q, closest);
    ldist_sq = VDOT(diff, diff);
    if (c) {
	VMOVE(*c, closest);
    }
    return ldist_sq ;
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
