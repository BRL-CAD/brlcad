/*               P U L L B A C K C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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

#include "PullbackCurve.h"
#include <assert.h>
#include <vector>
#include <limits>
#include "tnt.h"
#include "jama_lu.h"


#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

numeric_limits<double> real;

typedef struct pbc_data {
    double tolerance;
    double flatness;
    const ON_Curve* curve;
    const ON_Surface* surf;
    ON_2dPointArray samples;
} PBCData;

typedef struct _bspline {
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    vector<double> params;
    vector<double> knots;
    ON_2dPointArray controls;
} BSpline;


bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness) {
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}


bool
toUV(PBCData& data, ON_2dPoint& out_pt, double t) {
    double u = 0, v = 0;
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    if (data.surf->GetClosestPoint(pointOnCurve, &u, &v, data.tolerance)) {
	out_pt.Set(u, v);
	return true;
    } else
	return false;
}


double
randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
{
    assert(lo < hi);
    double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
    double newt = random_pos * (hi - lo) + lo;
    assert(toUV(data, out, newt));
    return newt;
}


bool
sample(PBCData& data,
       double t1,
       double t2,
       const ON_2dPoint& p1,
       const ON_2dPoint& p2)
{
    ON_2dPoint m;
    double t = randomPointFromRange(data, m, t1, t2);
    if (isFlat(p1, m, p2, data.flatness)) {
	data.samples.Append(p2);
    } else {
	sample(data, t1, t, p1, m);
	sample(data, t, t2, m, p2);
    }
}


void
generateKnots(BSpline& bspline) {
    int num_knots = bspline.m + 1;
    bspline.knots.reserve(num_knots);
    for (int i = 0; i <= bspline.p; i++) {
	bspline.knots[i] = 0.0;
    }
    for (int i = bspline.m-bspline.p; i <= bspline.m; i++) {
	bspline.knots[i] = 1.0;
    }
    for (int i = 1; i <= bspline.n-bspline.p; i++) {
	bspline.knots[bspline.p+i] = (double)i / (bspline.n-bspline.p+1.0);
    }
}


int
getKnotInterval(BSpline& bspline, double u) {
    int k = 0;
    while (u >= bspline.knots[k]) k++;
    k = (k == 0) ? k : k-1;
    return k;
}


int
getCoefficients(BSpline& bspline, Array1D<double>& N, double u) {
    // evaluate the b-spline basis function for the given parameter u
    // place the results in N[]
    N = 0.0;
    if (u == bspline.knots[0]) {
	N[0] = 1.0;
	return 0;
    } else if (u == bspline.knots[bspline.m]) {
	N[bspline.n] = 1.0;
	return bspline.n;
    }
    int k = getKnotInterval(bspline, u);
    N[k] = 1.0;
    for (int d = 1; d <= bspline.p; d++) {
	double uk_1 = bspline.knots[k+1];
	double uk_d_1 = bspline.knots[k-d+1];
	N[k-d] = ((uk_1 - u)/(uk_1 - uk_d_1)) * N[k-d+1];
	for (int i = k-d+1; i <= k-1; i++) {
	    double ui = bspline.knots[i];
	    double ui_1 = bspline.knots[i+1];
	    double ui_d = bspline.knots[i+d];
	    double ui_d_1 = bspline.knots[i+d+1];
	    N[i] = ((u - ui)/(ui_d - ui)) * N[i] + ((ui_d_1 - u)/(ui_d_1 - ui_1))*N[i+1];
	}
	double uk = bspline.knots[k];
	double uk_d = bspline.knots[k+d];
	N[k] = ((u - uk)/(uk_d - uk)) * N[k];
    }
    return k;
}


// FIXME: this function sucks...
void
generateParameters(BSpline& bspline) {
    double lastT = 0.0;
    bspline.params.reserve(bspline.n+1);
    Array2D<double> N(UNIVERSAL_SAMPLE_COUNT, bspline.n+1);
    for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
	double t = (double)i / (UNIVERSAL_SAMPLE_COUNT-1);
	Array1D<double> n = Array1D<double>(N.dim2(), N[i]);
	getCoefficients(bspline, n, t);
    }
    for (int i = 0; i < bspline.n+1; i++) {
	double max = real.min();
	for (int j = 0; j < UNIVERSAL_SAMPLE_COUNT; j++) {
	    double f = N[j][i];
	    double t = (double)j/(UNIVERSAL_SAMPLE_COUNT-1);
	    if (f > max) {
		max = f;
		if (j == UNIVERSAL_SAMPLE_COUNT-1) bspline.params[i] = t;
	    } else if (f < max) {
		bspline.params[i] = lastT;
		break;
	    }
	    lastT = t;
	}
    }
}


void
printMatrix(Array2D<double>& m) {
    printf("---\n");
    for (int i = 0; i < m.dim1(); i++) {
	for (int j = 0; j < m.dim2(); j++) {
	    printf("% 5.5f ", m[i][j]);
	}
	printf("\n");
    }
}


void
generateControlPoints(BSpline& bspline, PBCData& data)
{
    Array2D<double> bigN(bspline.n+1, bspline.n+1);
    for (int i = 0; i < bspline.n+1; i++) {
	Array1D<double> n = Array1D<double>(bigN.dim2(), bigN[i]);
	getCoefficients(bspline, n, bspline.params[i]);
    }
    Array2D<double> bigD(bspline.n+1, 2);
    for (int i = 0; i < bspline.n+1; i++) {
	bigD[i][0] = data.samples[i].x;
	bigD[i][1] = data.samples[i].y;
    }

    printMatrix(bigD);
    printMatrix(bigN);

    JAMA::LU<double> lu(bigN);
    assert(lu.isNonsingular() > 0);
    Array2D<double> bigP = lu.solve(bigD); // big linear algebra black box here...

    // extract the control points
    for (int i = 0; i < bspline.n+1; i++) {
	ON_2dPoint& p = bspline.controls.AppendNew();
	p.x = bigP[i][0];
	p.y = bigP[i][1];
    }
}


ON_NurbsCurve*
newNURBSCurve(BSpline& spline) {
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(3,
					  false,
					  spline.p+1,
					  spline.n+1);
    c->ReserveKnotCapacity(spline.knots.size());
    for (int i = 0; i < spline.knots.size(); i++) {
	c->m_knot[i] = spline.knots[i];
    }

    for (int i = 0; i < spline.controls.Count(); i++) {
	c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
}


ON_Curve*
interpolateCurve(PBCData& data) {
    if (data.samples.Count() == 2) {
	// build a line
	return new ON_LineCurve(data.samples[0], data.samples[1]);
    } else {
	// build a NURBS curve, then see if it can be simplified!
	BSpline spline;
	spline.p = 3;
	spline.n = data.samples.Count()-1;
	spline.m = spline.n + spline.p + 1;
	generateKnots(spline);
	generateParameters(spline);
	generateControlPoints(spline, data);
	ON_NurbsCurve* nurbs = newNURBSCurve(spline);

	return nurbs;
    }
}


ON_Curve*
pullback_curve(const ON_Surface* surface,
	       const ON_Curve* curve,
	       double tolerance,
	       double flatness) {
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surf = surface;

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = data.samples.AppendNew(); // new point is added to samples and returned
    if (toUV(data, start, tmin)) { return NULL; } // fails if first point is out of tolerance!

    ON_2dPoint p1, p2;
    toUV(data, p1, tmin);
    toUV(data, p2, tmax);
    if (!sample(data, tmin, tmax, p1, p2)) {
	return NULL;
    }

    for (int i = 0; i < data.samples.Count(); i++) {
	cerr << data.samples[i].x << ", " << data.samples[i].y << endl;
    }

    return interpolateCurve(data);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
