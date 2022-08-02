// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

// Compute the closest points on the line segments P(s) = (1-s)*P0 + s*P1 and
// Q(t) = (1-t)*Q0 + t*Q1 for 0 <= s <= 1 and 0 <= t <= 1.  The algorithm is
// robust even for nearly parallel segments.  Effectively, it uses a conjugate
// gradient search for the minimum of the squared distance function, which
// avoids the numerical problems introduced by divisions in the case the
// minimum is located at an interior point of the domain.  See the document
//   http://www.geometrictools.com/Documentation/DistanceLine3Line3.pdf
// for details.

// This file is basically
// https://www.geometrictools.com/GTEngine/Include/Mathematics/GteDistSegmentSegment.h
// except we are using vmath.h types and the C programming language

#include "common.h"
#include "vmath.h"
#include "bg/lseg.h"

struct bg_lseg_result {
    double distance, sqrDistance;
    double parameter[2];
    vect_t closest[2];
};
struct bg_lseg_tmp_vals {
    // The coefficients of R(s,t), not including the constant term.
    double mA, mB, mC, mD, mE;
    // dR/ds(i,j) at the four corners of the domain
    double mF00, mF10, mF01, mF11;
    // dR/dt(i,j) at the four corners of the domain
    double mG00, mG10, mG01, mG11;
};

// Compute the root of h(z) = h0 + slope*z and clamp it to the interval
// [0,1].  It is required that for h1 = h(1), either (h0 < 0 and h1 > 0)
// or (h0 > 0 and h1 < 0).
static double
GetClampedRoot(double slope, double h0, double h1)
{
    // Theoretically, r is in (0,1).  However, when the slope is nearly zero,
    // then so are h0 and h1.  Significant numerical rounding problems can
    // occur when using floating-point arithmetic.  If the rounding causes r
    // to be outside the interval, clamp it.  It is possible that r is in
    // (0,1) and has rounding errors, but because h0 and h1 are both nearly
    // zero, the quadratic is nearly constant on (0,1).  Any choice of p
    // should not cause undesirable accuracy problems for the final distance
    // computation.
    //
    // NOTE:  You can use bisection to recompute the root or even use
    // bisection to compute the root and skip the division.  This is generally
    // slower, which might be a problem for high-performance applications.

    double r;
    if (h0 < 0.0) {
	if (h1 > 0.0) {
	    r = -h0 / slope;
	    if (r > 1.0) {
		r = 0.5;
	    }
	    // The slope is positive and -h0 is positive, so there is no
	    // need to test for a negative value and clamp it.
	} else {
	    r = 1.0;
	}
    } else {
	r = 0.0;
    }
    return r;
}

// Compute the intersection of the line dR/ds = 0 with the domain [0,1]^2.
// The direction of the line dR/ds is conjugate to (1,0), so the algorithm
// for minimization is effectively the conjugate gradient algorithm for a
// quadratic function.
static void
ComputeIntersection(double const sValue[2], int const classify[2], int edge[2], double end[2][2],
	struct bg_lseg_tmp_vals *l)
{
    // The divisions are theoretically numbers in [0,1].  Numerical rounding
    // errors might cause the result to be outside the interval->  When this
    // happens, it must be that both numerator and denominator are nearly
    // zero.  The denominator is nearly zero when the segments are nearly
    // perpendicular.  The numerator is nearly zero when the P-segment is
    // nearly degenerate (mF00 = a is small).  The choice of 0.5 should not
    // cause significant accuracy problems.
    //
    // NOTE:  You can use bisection to recompute the root or even use
    // bisection to compute the root and skip the division.  This is generally
    // slower, which might be a problem for high-performance applications.

    if (classify[0] < 0) {
	edge[0] = 0;
	end[0][0] = 0.0;
	end[0][1] = l->mF00 / l->mB;
	if (end[0][1] < 0.0 || end[0][1] > 1.0) {
	    end[0][1] = 0.5;
	}

	if (classify[1] == 0) {
	    edge[1] = 3;
	    end[1][0] = sValue[1];
	    end[1][1] = 1.0;
	} else { // classify[1] > 0
	    edge[1] = 1;
	    end[1][0] = 1.0;
	    end[1][1] = l->mF10 / l->mB;
	    if (end[1][1] < 0.0 || end[1][1] > 1.0) {
		end[1][1] = 0.5;
	    }
	}
    } else if (classify[0] == 0) {
	edge[0] = 2;
	end[0][0] = sValue[0];
	end[0][1] = 0.0;

	if (classify[1] < 0) {
	    edge[1] = 0;
	    end[1][0] = 0.0;
	    end[1][1] = l->mF00 / l->mB;
	    if (end[1][1] < 0.0 || end[1][1] > 1.0) {
		end[1][1] = 0.5;
	    }
	} else if (classify[1] == 0) {
	    edge[1] = 3;
	    end[1][0] = sValue[1];
	    end[1][1] = 1.0;
	} else {
	    edge[1] = 1;
	    end[1][0] = 1.0;
	    end[1][1] = l->mF10 / l->mB;
	    if (end[1][1] < 0.0 || end[1][1] > 1.0) {
		end[1][1] = 0.5;
	    }
	}
    } else { // classify[0] > 0
	edge[0] = 1;
	end[0][0] = 1.0;
	end[0][1] = l->mF10 / l->mB;
	if (end[0][1] < 0.0 || end[0][1] > 1.0) {
	    end[0][1] = 0.5;
	}

	if (classify[1] == 0) {
	    edge[1] = 3;
	    end[1][0] = sValue[1];
	    end[1][1] = 1.0;
	} else {
	    edge[1] = 0;
	    end[1][0] = 0.0;
	    end[1][1] = l->mF00 / l->mB;
	    if (end[1][1] < 0.0 || end[1][1] > 1.0) {
		end[1][1] = 0.5;
	    }
	}
    }
}


// Compute the location of the minimum of R on the segment of intersection
// for the line dR/ds = 0 and the domain [0,1]^2.
static void
ComputeMinimumParameters(int edge[2], double end[2][2], double parameter[2],
	struct bg_lseg_tmp_vals *l)
{
    double delta = end[1][1] - end[0][1];
    double h0 = delta * (-l->mB * end[0][0] + l->mC * end[0][1] - l->mE);
    if (h0 >= 0.0) {
	if (edge[0] == 0) {
	    parameter[0] = 0.0;
	    parameter[1] = GetClampedRoot(l->mC, l->mG00, l->mG01);
	} else if (edge[0] == 1) {
	    parameter[0] = 1.0;
	    parameter[1] = GetClampedRoot(l->mC, l->mG10, l->mG11);
	} else {
	    parameter[0] = end[0][0];
	    parameter[1] = end[0][1];
	}
    } else {
	double h1 = delta * (-l->mB * end[1][0] + l->mC * end[1][1] - l->mE);
	if (h1 <= 0.0) {
	    if (edge[1] == 0) {
		parameter[0] = 0.0;
		parameter[1] = GetClampedRoot(l->mC, l->mG00, l->mG01);
	    } else if (edge[1] == 1) {
		parameter[0] = 1.0;
		parameter[1] = GetClampedRoot(l->mC, l->mG10, l->mG11);
	    } else {
		parameter[0] = end[1][0];
		parameter[1] = end[1][1];
	    }
	} else { // h0 < 0 and h1 > 0
	    double h0h1, z, omz;
	    h0h1 = h0 / (h0 - h1);
	    h0h1 = (h0h1 < 0) ? 0.0 : h0h1;
	    z = (h0h1 > 1) ? 1.0 : h0h1;
	    omz = 1.0 - z;
	    parameter[0] = omz * end[0][0] + z * end[1][0];
	    parameter[1] = omz * end[0][1] + z * end[1][1];
	}
    }
}


double
bg_distsq_lseg3_lseg3(point_t *c1, point_t *c2,
		  const point_t P0, const point_t P1, const point_t Q0, const point_t Q1)
{
    vect_t diff, r1a, r1b, r2a, r2b, r1, r2;
    double ldist_sq;
    struct bg_lseg_result result;
    struct bg_lseg_tmp_vals v;

    // The code allows degenerate line segments; that is, P0 and P1 can be
    // the same point or Q0 and Q1 can be the same point.  The quadratic
    // function for squared distance between the segment is
    //   R(s,t) = a*s^2 - 2*b*s*t + c*t^2 + 2*d*s - 2*e*t + f
    // for (s,t) in [0,1]^2 where
    //   a = Dot(P1-P0,P1-P0), b = Dot(P1-P0,Q1-Q0), c = Dot(Q1-Q0,Q1-Q0),
    //   d = Dot(P1-P0,P0-Q0), e = Dot(Q1-Q0,P0-Q0), f = Dot(P0-Q0,P0-Q0)
    vect_t P1mP0, Q1mQ0, P0mQ0;
    VSUB2(P1mP0, P1, P0);
    VSUB2(Q1mQ0, Q1, Q0);
    VSUB2(P0mQ0, P0, Q0);
    v.mA = VDOT(P1mP0, P1mP0);
    v.mB = VDOT(P1mP0, Q1mQ0);
    v.mC = VDOT(Q1mQ0, Q1mQ0);
    v.mD = VDOT(P1mP0, P0mQ0);
    v.mE = VDOT(Q1mQ0, P0mQ0);

    v.mF00 = v.mD;
    v.mF10 = v.mF00 + v.mA;
    v.mF01 = v.mF00 - v.mB;
    v.mF11 = v.mF10 - v.mB;

    v.mG00 = -v.mE;
    v.mG10 = v.mG00 - v.mB;
    v.mG01 = v.mG00 + v.mC;
    v.mG11 = v.mG10 + v.mC;

    if (v.mA > 0.0 && v.mC > 0.0) {
	// Compute the solutions to dR/ds(s0,0) = 0 and dR/ds(s1,1) = 0.  The
	// location of sI on the s-axis is stored in classifyI (I = 0 or 1).  If
	// sI <= 0, classifyI is -1.  If sI >= 1, classifyI is 1.  If 0 < sI < 1,
	// classifyI is 0.  This information helps determine where to search for
	// the minimum point (s,t).  The fij values are dR/ds(i,j) for i and j in
	// {0,1}.

	double sValue[2];
	int classify[2];
	sValue[0] = GetClampedRoot(v.mA, v.mF00, v.mF10);
	sValue[1] = GetClampedRoot(v.mA, v.mF01, v.mF11);

	for (int i = 0; i < 2; ++i) {
	    if (sValue[i] <= 0.0) {
		classify[i] = -1;
	    } else if (sValue[i] >= 1.0) {
		classify[i] = +1;
	    } else {
		classify[i] = 0;
	    }
	}

	if (classify[0] == -1 && classify[1] == -1) {
	    // The minimum must occur on s = 0 for 0 <= t <= 1.
	    result.parameter[0] = 0.0;
	    result.parameter[1] = GetClampedRoot(v.mC, v.mG00, v.mG01);
	} else if (classify[0] == +1 && classify[1] == +1) {
	    // The minimum must occur on s = 1 for 0 <= t <= 1.
	    result.parameter[0] = 1.0;
	    result.parameter[1] = GetClampedRoot(v.mC, v.mG10, v.mG11);
	} else {
	    // The line dR/ds = 0 intersects the domain [0,1]^2 in a
	    // nondegenerate segment.  Compute the endpoints of that segment,
	    // end[0] and end[1].  The edge[i] flag tells you on which domain
	    // edge end[i] lives: 0 (s=0), 1 (s=1), 2 (t=0), 3 (t=1).
	    int edge[2];
	    double end[2][2];
	    ComputeIntersection(sValue, classify, edge, end, &v);

	    // The directional derivative of R along the segment of
	    // intersection is
	    //   H(z) = (end[1][1]-end[1][0])*dR/dt((1-z)*end[0] + z*end[1])
	    // for z in [0,1].  The formula uses the fact that dR/ds = 0 on
	    // the segment.  Compute the minimum of H on [0,1].
	    ComputeMinimumParameters(edge, end, result.parameter, &v);
	}
    } else {
	if (v.mA > 0.0) {
	    // The Q-segment is degenerate (Q0 and Q1 are the same point) and
	    // the quadratic is R(s,0) = a*s^2 + 2*d*s + f and has (half)
	    // first derivative F(t) = a*s + d.  The closest P-point is
	    // interior to the P-segment when F(0) < 0 and F(1) > 0.
	    result.parameter[0] = GetClampedRoot(v.mA, v.mF00, v.mF10);
	    result.parameter[1] = 0.0;
	} else if (v.mC > 0.0) {
	    // The P-segment is degenerate (P0 and P1 are the same point) and
	    // the quadratic is R(0,t) = c*t^2 - 2*e*t + f and has (half)
	    // first derivative G(t) = c*t - e.  The closest Q-point is
	    // interior to the Q-segment when G(0) < 0 and G(1) > 0.
	    result.parameter[0] = 0.0;
	    result.parameter[1] = GetClampedRoot(v.mC, v.mG00, v.mG01);
	} else {
	    // P-segment and Q-segment are degenerate.
	    result.parameter[0] = 0.0;
	    result.parameter[1] = 0.0;
	}
    }

    VSCALE(r1a, P0, (1.0 - result.parameter[0]));
    VSCALE(r1b, P1, result.parameter[0]);
    VSCALE(r2a, Q0, (1.0 - result.parameter[1]));
    VSCALE(r2b, Q1, result.parameter[1]);
    VADD2(r1, r1a, r1b);
    VADD2(r2, r2a, r2b);
    if (c1) {
	VMOVE(*c1, r1);
    }
    if (c2) {
	VMOVE(*c2, r2);
    }
    VSUB2(diff, r1, r2);
    ldist_sq = VDOT(diff, diff);
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
