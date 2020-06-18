// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2019/07/30)
//
// This file is basically
// https://www.geometrictools.com/GTEngine/Include/Mathematics/GteDistPointTriangle.h
// except we are using vmath.h types and the C programming language

#include "common.h"
#include "vmath.h"
#include "bg/tri_pt.h"

static void GetMinEdge02(vect2d_t *p, double a11, double b1)
{
    (*p)[0] = 0.0;
    if (b1 >= 0.0) {
	(*p)[1] = 0.0;
	return;
    }
    if (a11 + b1 <= 0.0) {
	(*p)[1] = 1.0;
	return;
    }
    (*p)[1] = -b1 / a11;
}

static void GetMinEdge12(vect2d_t *p,
	double a01, double a11, double b1,
	double f10, double f01)
{
    double h0 = a01 + b1 - f10;
    if (h0 >= 0.0) {
	(*p)[1] = 0.0;
    } else {
	double h1 = a11 + b1 - f01;
	(*p)[1] = (h1 <= 0.0) ? 1.0 : h0 / (h0 - h1);
    }
    (*p)[0] = 1.0 - (*p)[1];
}

static void GetMinInterior(vect2d_t *p,
	vect2d_t p0, double h0,
	vect2d_t p1, double h1)
{
    double z = h0 / (h0 - h1);
    vect2d_t t0, t1;
    V2SCALE(t0, p0, (1.0 - z));
    V2SCALE(t1, p1, z);
    V2ADD2(*p, t0, t1);
}

double bg_tri_closest_pt(point_t *closest_pt, const point_t tp, const point_t V0, const point_t V1, const point_t V2)
{
    vect_t diff, edge0, edge1;
    vect_t e0scaled, e1scaled;
    vect_t closest;
    vect2d_t p0, p1, p;
    double a00, a01, a11, b0, b1, f00, f10, f01;
    double dt1, h0, h1;

    VSUB2(diff, tp, V0);
    VSUB2(edge0, V1, V0);
    VSUB2(edge1, V2, V0);
    a00 = VDOT(edge0, edge0);
    a01 = VDOT(edge0, edge1);
    a11 = VDOT(edge1, edge1);
    b0 = -1*VDOT(diff, edge0);
    b1 = -1*VDOT(diff, edge1);

    f00 = b0;
    f10 = b0 + a00;
    f01 = b0 + a01;

    /* Compute the endpoints p0 and p1 of the segment. The segment is
     * parameterized by L(z) = (1-z)*p0 + z*p1 for z in [0,1] and the
     * directional derivative of half the quadratic on the segment is
     * H(z) = Dot(p1-p0,gradient[Q](L(z))/2), where gradient[Q]/2 =
     * (F,G). By design, F(L(z)) = 0 for cases (2), (4), (5), and (6).
     * Cases (1) and (3) can correspond to no-intersection or
     * intersection of F = 0 with the triangle. */
    if (f00 >= 0.0) {
	if (f01 >= 0.0) {
	    /* (1) p0 = (0,0), p1 = (0,1), H(z) = G(L(z)) */
	    GetMinEdge02(&p, a11, b1);
	} else {
	    /* (2) p0 = (0,t10), p1 = (t01,1-t01),
	     * H(z) = (t11 - t10)*G(L(z)) */
	    p0[0] = 0.0;
	    p0[1] = f00 / (f00 - f01);
	    p1[0] = f01 / (f01 - f10);
	    p1[1] = 1.0 - p1[0];
	    dt1 = p1[1] - p0[1];
	    h0 = dt1 * (a11 * p0[1] + b1);
	    if (h0 >= 0.0) {
		GetMinEdge02(&p, a11, b1);
	    } else {
		h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
		if (h1 <= 0.0) {
		    GetMinEdge12(&p, a01, a11, b1, f10, f01);
		} else {
		    GetMinInterior(&p, p0, h0, p1, h1);
		}
	    }
	}
    } else if (f01 <= 0.0) {
	if (f10 <= 0.0) {
	    /* (3) p0 = (1,0), p1 = (0,1),
	     * H(z) = G(L(z)) - F(L(z)) */
	    GetMinEdge12(&p, a01, a11, b1, f10, f01);
	} else {
	    // (4) p0 = (t00,0), p1 = (t01,1-t01), H(z) = t11*G(L(z))
	    p0[0] = f00 / (f00 - f10);
	    p0[1] = 0.0;
	    p1[0] = f01 / (f01 - f10);
	    p1[1] = 1.0 - p1[0];
	    h0 = p1[1] * (a01 * p0[0] + b1);
	    if (h0 >= 0.0) {
		V2MOVE(p, p0);  /* GetMinEdge01 */
	    } else {
		h1 = p1[1] * (a01 * p1[0] + a11 * p1[1] + b1);
		if (h1 <= 0.0) {
		    GetMinEdge12(&p, a01, a11, b1, f10, f01);
		} else {
		    GetMinInterior(&p, p0, h0, p1, h1);
		}
	    }
	}
    } else if (f10 <= 0.0) {
	/* (5) p0 = (0,t10), p1 = (t01,1-t01),
	 * H(z) = (t11 - t10)*G(L(z)) */
	p0[0] = 0.0;
	p0[1] = f00 / (f00 - f01);
	p1[0] = f01 / (f01 - f10);
	p1[1] = 1.0 - p1[0];
	dt1 = p1[1] - p0[1];
	h0 = dt1 * (a11 * p0[1] + b1);
	if (h0 >= 0.0) {
	    GetMinEdge02(&p, a11, b1);
	} else {
	    h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
	    if (h1 <= 0.0) {
		GetMinEdge12(&p, a01, a11, b1, f10, f01);
	    } else {
		GetMinInterior(&p, p0, h0, p1, h1);
	    }
	}
    } else {
	/* (6) p0 = (t00,0), p1 = (0,t11), H(z) = t11*G(L(z)) */
	p0[0] = f00 / (f00 - f10);
	p0[1] = 0.0;
	p1[0] = 0.0;
	p1[1] = f00 / (f00 - f01);
	h0 = p1[1] * (a01 * p0[0] + b1);
	if (h0 >= 0.0) {
	    V2MOVE(p, p0);  /* GetMinEdge01 */
	} else {
	    h1 = p1[1] * (a11 * p1[1] + b1);
	    if (h1 <= 0.0) {
		GetMinEdge02(&p, a11, b1);
	    } else {
		GetMinInterior(&p, p0, h0, p1, h1);
	    }
	}
    }

    VSCALE(e0scaled, edge0, p[0]);
    VSCALE(e1scaled, edge1, p[1]);
    VADD3(closest, V0, e0scaled, e1scaled);
    if (closest_pt) {
	VMOVE(*closest_pt, closest);
    }
    return DIST_PNT_PNT(tp, closest);
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
