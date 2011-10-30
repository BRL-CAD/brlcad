/*               P U L L B A C K C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file step/PullbackCurve.cpp
 *
 * Pull curve back into UV space from 3D space
 *
 */

#include "common.h"

#include "vmath.h"
#include "dvec.h"

#include <assert.h>
#include <vector>
#include <list>
#include <limits>

#include "tnt.h"
#include "jama_lu.h"
#include "brep.h"

/* FIXME: should not be peeking into a private header and cannot be a
 * public header (of librt).
 */
#include "../../librt/opennurbs_ext.h"

/* interface header */
#include "PullbackCurve.h"


#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001


typedef struct _bspline
{
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    std::vector<double> params;
    std::vector<double> knots;
    ON_2dPointArray controls;
} BSpline;


class plane_ray
{
public:
    vect_t n1;
    fastf_t d1;

    vect_t n2;
    fastf_t d2;
};


bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness)
{
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}


void
brep_get_plane_ray(ON_Ray& r, plane_ray& pr)
{
    vect_t v1;
    VMOVE(v1, r.m_dir);
    fastf_t min = MAX_FASTF;
    int index = -1;
    for (int i = 0; i < 3; i++) {
	// find the smallest component
	if (fabs(v1[i]) < min) {
	    min = fabs(v1[i]);
	    index = i;
	}
    }
    v1[index] += 1; // alter the smallest component
    VCROSS(pr.n1, v1, r.m_dir); // n1 is perpendicular to v1
    VUNITIZE(pr.n1);
    VCROSS(pr.n2, pr.n1, r.m_dir);       // n2 is perpendicular to v1 and n1
    VUNITIZE(pr.n2);
    pr.d1 = VDOT(pr.n1, r.m_origin);
    pr.d2 = VDOT(pr.n2, r.m_origin);
    TRACE1("n1:" << ON_PRINT3(pr.n1) << " n2:" << ON_PRINT3(pr.n2) << " d1:" << pr.d1 << " d2:" << pr.d2);
}


void
brep_r(const ON_Surface* surf, plane_ray& pr, pt2d_t uv, ON_3dPoint& pt, ON_3dVector& su, ON_3dVector& sv, pt2d_t R)
{
    assert(surf->Ev1Der(uv[0], uv[1], pt, su, sv));
    R[0] = VDOT(pr.n1, ((fastf_t*)pt)) - pr.d1;
    R[1] = VDOT(pr.n2, ((fastf_t*)pt)) - pr.d2;
}


void
brep_newton_iterate(const ON_Surface* UNUSED(surf), plane_ray& pr, pt2d_t R, ON_3dVector& su, ON_3dVector& sv, pt2d_t uv, pt2d_t out_uv)
{
    mat2d_t jacob = { VDOT(pr.n1, ((fastf_t*)su)), VDOT(pr.n1, ((fastf_t*)sv)),
		      VDOT(pr.n2, ((fastf_t*)su)), VDOT(pr.n2, ((fastf_t*)sv)) };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	// check inverse validity
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, R);
	pt2dsub(out_uv, uv, tmp);
    } else {
	TRACE2("inverse failed"); // FIXME: how to handle this?
	move(out_uv, uv);
    }
}


void
utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d)
{
    ON_3dPoint ro(r.m_origin);
    ON_3dVector rdir(r.m_dir);
    double rdx, rdy, rdz;
    double rdxmag, rdymag, rdzmag;

    rdx = rdir.x;
    rdy = rdir.y;
    rdz = rdir.z;

    rdxmag = fabs(rdx);
    rdymag = fabs(rdy);
    rdzmag = fabs(rdz);

    if (rdxmag > rdymag && rdxmag > rdzmag)
	p1 = ON_3dVector(rdy, -rdx, 0);
    else
	p1 = ON_3dVector(0, rdz, -rdy);
    p1.Unitize();

    p2 = ON_CrossProduct(p1, rdir);

    p1d = -(p1 * ro);
    p2d = -(p2 * ro);
}


enum seam_direction
seam_direction(ON_2dPoint uv1, ON_2dPoint uv2)
{
    if (NEAR_EQUAL(uv1.x, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 0.0, PBC_TOL)) {
	return WEST_SEAM;
    } else if (NEAR_EQUAL(uv1.x, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 1.0, PBC_TOL)) {
	return EAST_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 0.0, PBC_TOL)) {
	return SOUTH_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 1.0, PBC_TOL)) {
	return NORTH_SEAM;
    } else {
	return UNKNOWN_SEAM_DIRECTION;
    }
}


bool
toUV(PBCData& data, ON_2dPoint& out_pt, double t, double knudge=0.0)
{
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = data.curve->PointAt(t+knudge);

    ON_2dPoint uv;
    if (data.surftree->getSurfacePoint((const ON_3dPoint&)pointOnCurve, uv, (const ON_3dPoint&)knudgedPointOnCurve, BREP_EDGE_MISS_TOLERANCE) > 0) {
	out_pt.Set(uv.x, uv.y);
	return true;
    } else {
	return false;
    }
}


bool
toUV(brlcad::SurfaceTree *surftree, const ON_Curve *curve, ON_2dPoint& out_pt, double t, double knudge=0.0)
{
    const ON_Surface *surf = surftree->getSurface();
    ON_3dPoint pointOnCurve = curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = curve->PointAt(t+knudge);
    ON_3dVector dt;
    curve->Ev1Der(t, pointOnCurve, dt);
    ON_3dVector tangent = curve->TangentAt(t);
    //data.surf->GetClosestPoint(pointOnCurve, &a, &b, 0.0001);
    ON_Ray r(pointOnCurve, tangent);
    plane_ray pr;
    brep_get_plane_ray(r, pr);
    ON_3dVector p1;
    double p1d;
    ON_3dVector p2;
    double p2d;

    utah_ray_planes(r, p1, p1d, p2, p2d);

    VMOVE(pr.n1, p1);
    pr.d1 = p1d;
    VMOVE(pr.n2, p2);
    pr.d2 = p2d;

    try {
	ON_2dPoint uv = surftree->getClosestPointEstimate(knudgedPointOnCurve);
	ON_3dVector dir = surf->NormalAt(uv.x, uv.y);
	dir.Reverse();
	ON_Ray ray(pointOnCurve, dir);
	brep_get_plane_ray(ray, pr);
	//know use this as guess to iterate to closer solution
	pt2d_t Rcurr;
	pt2d_t new_uv;
	ON_3dPoint pt;
	ON_3dVector su, sv;
	bool found=false;
	fastf_t Dlast = MAX_FASTF;
	for (int i = 0; i < 10; i++) {
	    brep_r(surf, pr, uv, pt, su, sv, Rcurr);
	    fastf_t d = v2mag(Rcurr);
	    if (d < BREP_INTERSECTION_ROOT_EPSILON) {
		TRACE1("R:"<<ON_PRINT2(Rcurr));
		found = true; break;
	    } else if (d > Dlast) {
		found = false; //break;
		break;
		//return brep_edge_check(found, sbv, face, surf, ray, hits);
	    }
	    brep_newton_iterate(surf, pr, Rcurr, su, sv, uv, new_uv);
	    move(uv, new_uv);
	    Dlast = d;
	}

///////////////////////////////////////
	out_pt.Set(uv.x, uv.y);
	return true;
    } catch(...) {
	return false;
    }
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
    ON_2dPointArray * samples = (*data.segments.end());
    if (isFlat(p1, m, p2, data.flatness)) {
	samples->Append(p2);
    } else {
	sample(data, t1, t, p1, m);
	sample(data, t, t2, m, p2);
    }
    return true;
}


void
generateKnots(BSpline& bspline)
{
    int num_knots = bspline.m + 1;
    bspline.knots.resize(num_knots);
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
getKnotInterval(BSpline& bspline, double u)
{
    int k = 0;
    while (u >= bspline.knots[k]) k++;
    k = (k == 0) ? k : k-1;
    return k;
}


int
getCoefficients(BSpline& bspline, TNT::Array1D<double>& N, double u)
{
    // evaluate the b-spline basis function for the given parameter u
    // place the results in N[]
    N = 0.0;
  if (NEAR_EQUAL(u,bspline.knots[0],PBC_TOL)) {
	N[0] = 1.0;
	return 0;
  } else if (NEAR_EQUAL(u,bspline.knots[bspline.m],PBC_TOL)) {
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


void
printMatrix(TNT::Array1D<double>& m) {
    printf("---\n");
    for (int i = 0; i < m.dim1(); i++) {
	printf("% 5.5f ", m[i]);
    }
    printf("\n");
}


// FIXME: this function sucks...
void
generateParameters(BSpline& bspline)
{
    double lastT = 0.0;
    bspline.params.resize(bspline.n + 1);
    TNT::Array2D<double> N(UNIVERSAL_SAMPLE_COUNT, bspline.n + 1);
    for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
	double t = (double) i / (UNIVERSAL_SAMPLE_COUNT - 1);
	TNT::Array1D<double> n = TNT::Array1D<double> (N.dim2(), N[i]);
	getCoefficients(bspline, n, t);
	//printMatrix(n);
    }

    for (int i = 0; i < bspline.n + 1; i++) {
	double max = 0.0; //real.min();
	for (int j = 0; j < UNIVERSAL_SAMPLE_COUNT; j++) {
	    double f = N[j][i];
	    double t = (double) j / (UNIVERSAL_SAMPLE_COUNT - 1);
	    if (f > max) {
		max = f;
		if (j == UNIVERSAL_SAMPLE_COUNT - 1)
		    bspline.params[i] = t;
	    } else if (f < max) {
		bspline.params[i] = lastT;
		break;
	    }
	    lastT = t;
	}
    }
}


void
printMatrix(TNT::Array2D<double>& m)
{
    printf("---\n");
    for (int i = 0; i < m.dim1(); i++) {
	for (int j = 0; j < m.dim2(); j++) {
	    printf("% 5.5f ", m[i][j]);
	}
	printf("\n");
    }
}

ON_NurbsCurve*
interpolateLocalCubicCurve(ON_2dPointArray &Q) {
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 4;
    std::vector < ON_2dVector > qarray(qsize);

    for (int i = 1; i < Q.Count(); i++) {
	qarray[i + 1] = Q[i] - Q[i - 1];
    }
    qarray[1] = 2.0 * qarray[2] - qarray[3];
    qarray[0] = 2.0 * qarray[1] - qarray[2];

    qarray[num_samples + 1] = 2 * qarray[num_samples] - qarray[num_samples - 1];
    qarray[num_samples + 2] = 2 * qarray[num_samples + 1] - qarray[num_samples];
    qarray[num_samples + 3] = 2 * qarray[num_samples + 2] - qarray[num_samples + 1];

    std::vector < ON_2dVector > T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector a = ON_CrossProduct(qarray[k], qarray[k + 1]);
	ON_3dVector b = ON_CrossProduct(qarray[k + 2], qarray[k + 3]);
	double alength = a.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (a.Length()) / (a.Length() + b.Length());
	}
	T[k] = (1.0 - A[k]) * qarray[k + 1] + A[k] * qarray[k + 2];
	T[k].Unitize();
    }
    std::vector < ON_2dPointArray > P(num_samples - 1);
    ON_2dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_2dPoint P0 = Q[i - 1];
	ON_2dPoint P3 = Q[i];
	ON_2dVector T0 = T[i - 1];
	ON_2dVector T3 = T[i];

	double a, b, c;

	ON_2dVector vT0T3 = T0 + T3;
	ON_2dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_2dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_2dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    //generateParameters(spline);
    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	u[k + 1] = u[k] + 3.0 * (P[k][1] - P[k][0]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 2;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}

ON_NurbsCurve*
interpolateLocalCubicCurve(ON_3dPointArray &Q)
{
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 3;
    std::vector<ON_3dVector> qarray(qsize);
    ON_3dVector *q = &qarray[1];

    for (int i = 1; i < Q.Count(); i++) {
	q[i] = Q[i] - Q[i - 1];
    }
    q[0] = 2.0 * q[1] - q[2];
    q[-1] = 2.0 * q[0] - q[1];

    q[num_samples] = 2 * q[num_samples - 1] - q[num_samples - 2];
    q[num_samples + 1] = 2 * q[num_samples] - q[num_samples - 1];
    q[num_samples + 2] = 2 * q[num_samples + 1] - q[num_samples];

    std::vector<ON_3dVector> T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector avec = ON_CrossProduct(q[k - 1], q[k]);
	ON_3dVector bvec = ON_CrossProduct(q[k + 1], q[k + 2]);
	double alength = avec.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (avec.Length()) / (avec.Length() + bvec.Length());
	}
	T[k] = (1.0 - A[k]) * q[k] + A[k] * q[k + 1];
	T[k].Unitize();
    }
    std::vector<ON_3dPointArray> P(num_samples - 1);
    ON_3dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_3dPoint P0 = Q[i - 1];
	ON_3dPoint P3 = Q[i];
	ON_3dVector T0 = T[i - 1];
	ON_3dVector T3 = T[i];

	double a, b, c;

	ON_3dVector vT0T3 = T0 + T3;
	ON_3dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_3dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_3dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    //generateParameters(spline);
    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	u[k + 1] = u[k] + 3.0 * (P[k][1] - P[k][0]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}


void
generateControlPoints(BSpline& bspline, ON_2dPointArray &samples)
{
    TNT::Array2D<double> bigN(bspline.n+1, bspline.n+1);
    //printMatrix(bigN);

    for (int i = 0; i < bspline.n+1; i++) {
	TNT::Array1D<double> n = TNT::Array1D<double>(bigN.dim2(), bigN[i]);
	getCoefficients(bspline, n, bspline.params[i]);
	//printMatrix(bigN);
    }
    TNT::Array2D<double> bigD(bspline.n+1, 2);
    for (int i = 0; i < bspline.n+1; i++) {
	bigD[i][0] = samples[i].x;
	bigD[i][1] = samples[i].y;
    }

    //printMatrix(bigD);
    //printMatrix(bigN);

    JAMA::LU<double> lu(bigN);
    assert(lu.isNonsingular() > 0);
    TNT::Array2D<double> bigP = lu.solve(bigD); // big linear algebra black box here...

    // extract the control points
    for (int i = 0; i < bspline.n+1; i++) {
	ON_2dPoint& p = bspline.controls.AppendNew();
	p.x = bigP[i][0];
	p.y = bigP[i][1];
    }
}


ON_NurbsCurve*
newNURBSCurve(BSpline& spline, int dimension=3)
{
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension,
					  false,
					  spline.p+1,
					  spline.n+1);
    c->ReserveKnotCapacity(spline.knots.size()-2);
  for (unsigned int i = 1; i < spline.knots.size()-1; i++) {
	c->m_knot[i-1] = spline.knots[i];
    }

    for (int i = 0; i < spline.controls.Count(); i++) {
	c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
}


ON_Curve*
interpolateCurve(ON_2dPointArray &samples)
{
    bool useBezier = false;
    if (samples.Count() == 2) {
	// build a line
	return new ON_LineCurve(samples[0], samples[1]);
    } else if (useBezier == true) {
	ON_BezierCurve *bezier = new ON_BezierCurve(
	    samples);
	ON_NurbsCurve nurbcurve;
	if (bezier->GetNurbForm(nurbcurve)) {
	    return ON_NurbsCurve::New(*bezier);
	}
	// build a NURBS curve, then see if it can be simplified!
	BSpline spline;
	spline.p = 3;
	spline.n = samples.Count() - 1;
	spline.m = spline.n + spline.p + 1;
	generateKnots(spline);
	generateParameters(spline);
	generateControlPoints(spline, samples);
	ON_NurbsCurve* nurbs = newNURBSCurve(spline);

	return nurbs;
    } else {
	ON_TextLog dump;
	ON_NurbsCurve* nurbs;
	if (false) { //(samples.Count() < 1000) { //
	    BSpline spline;
	    spline.p = 3;
	    spline.n = samples.Count()-1;
	    spline.m = spline.n + spline.p + 1;
	    generateKnots(spline);
	    generateParameters(spline);
	    generateControlPoints(spline, samples);
	    nurbs = newNURBSCurve(spline, 2);
	    //nurbs->Dump(dump);
	} else {
	    // local vs. global interpolation for large point sampled curves
	    nurbs = interpolateLocalCubicCurve(samples);
	    //nurbs->Dump(dump);
	}
	return nurbs;
    }
}


/*
 * rc = 0 Not on seam, 1 on East/West seam(umin/umax), 2 on North/South seam(vmin/vmax), 3 seam on both U/V boundaries
 */
int
IsAtSeam(const ON_Surface *surf, double u, double v)
{
    int rc = 0;
    int i;
    for (i=0; i<2; i++) {
	if (!surf->IsClosed(i))
	    continue;
	double p = (i) ? v : u;
	if (NEAR_EQUAL(p, surf->Domain(i)[0], PBC_SEAM_TOL) || NEAR_EQUAL(p, surf->Domain(i)[1], PBC_SEAM_TOL))
	    rc += (i+1);
    }

    return rc;
}


int
IsAtSingularity(const ON_Surface *surf, double u, double v)
{
    // 0 = south, 1 = east, 2 = north, 3 = west
    //std::cerr << "IsAtSingularity = u, v - " << u << ", " << v << std::endl;
    //std::cerr << "surf->Domain(0) - " << surf->Domain(0)[0] << ", " << surf->Domain(0)[1] << std::endl;
    //std::cerr << "surf->Domain(1) - " << surf->Domain(1)[0] << ", " << surf->Domain(1)[1] << std::endl;
    if (NEAR_EQUAL(u, surf->Domain(0)[0], PBC_TOL)) {
	if (surf->IsSingular(3))
	    return 3;
    } else if (NEAR_EQUAL(u, surf->Domain(0)[1], PBC_TOL)) {
	if (surf->IsSingular(1))
	    return 1;
    }
    if (NEAR_EQUAL(v, surf->Domain(1)[0], PBC_TOL)) {
	if (surf->IsSingular(0))
	    return 0;
    } else if (NEAR_EQUAL(v, surf->Domain(1)[1], PBC_TOL)) {
	if (surf->IsSingular(2))
	    return 2;
    }
    return -1;
}


ON_Curve*
test1_pullback_curve(const brlcad::SurfaceTree* surfacetree,
		     const ON_Curve* curve,
		     double UNUSED(tolerance),
		     double UNUSED(flatness))
{
    ON_NurbsCurve* orig = curve->NurbsCurve();
    bool isRational = false;
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(curve->Dimension(),
					  isRational,
					  orig->Degree()+1,
					  orig->m_cv_count+1);

    int numKnots = orig->Degree() + orig->m_cv_count -1;
    c->ReserveKnotCapacity(numKnots);
    double *span = new double[numKnots];
    if (orig->GetSpanVector(span)) {
	for (int i = 0; i < numKnots; i++) {
	    c->SetKnot(i, span[i]);
	}
    }

    ON_2dPoint uv;
    for (int i = 0; i < orig->m_cv_count; i++) {
	ON_3dPoint p;
	if (orig->GetCV(i, p)) {
	    if (surfacetree->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)p) > 0) {
		c->SetCV(i, p);
	    }
	}
    }

    return c;
}


ON_Curve*
test2_pullback_curve(const brlcad::SurfaceTree* surfacetree,
		     const ON_Curve* curve,
		     double tolerance,
		     double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray *samples= new ON_2dPointArray();

    data.segments.push_back(samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots+1];
    curve->GetSpanVector(knots);

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3*degree;
    } else {
	samplesperknotinterval = 18*degree;
    }
    ON_2dPoint pt;
    for (int i=0; i<=numKnots; i++) {
	if (i <= numKnots/2) {
	    if (i>0) {
		double delta = (knots[i] - knots[i-1])/(double)samplesperknotinterval;
		for (int j=1; j<samplesperknotinterval; j++) {
		    if (toUV(data, pt, knots[i-1]+j*delta, PBC_TOL)) {
			samples->Append(pt);
		    } else {
			//std::cout << "didn't find point on surface" << std::endl;
		    }
		}
	    }
	    if (toUV(data, pt, knots[i], PBC_TOL)) {
		samples->Append(pt);
	    } else {
		//std::cout << "didn't find point on surface" << std::endl;
	    }
	} else {
	    if (i>0) {
		double delta = (knots[i] - knots[i-1])/(double)samplesperknotinterval;
		for (int j=1; j<samplesperknotinterval; j++) {
		    if (toUV(data, pt, knots[i-1]+j*delta, -PBC_TOL)) {
			samples->Append(pt);
		    } else {
			//std::cout << "didn't find point on surface" << std::endl;
		    }
		}
		if (toUV(data, pt, knots[i], -PBC_TOL)) {
		    samples->Append(pt);
		} else {
		    //std::cout << "didn't find point on surface" << std::endl;
		}
	    }
	}
    }
    delete [] knots;

    std::cerr << std::endl << "samples:" << std::endl;
    for (int i = 0; i < samples->Count(); i++) {
	std::cerr << i << "- " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
    }

    /*
      ON_Surface *surf = (ON_Surface *)data.surftree->getSurface();
      ON_3dPoint p;
      for (int i = 0; i < data.samples.Count(); i++) {
      p=surf->PointAt(data.samples[i].x, data.samples[i].y);
      std::cerr << data.samples[i].x << ", " << data.samples[i].y;
      std::cerr << " --> "<< p.x << ", " << p.y << ", " << p.z << std::endl;
      }
    */
    return interpolateCurve(*samples);
}


ON_2dPointArray *
pullback_samples(PBCData* data,
		 double t,
		 double s)
{
    const ON_Curve* curve= data->curve;
    ON_2dPointArray *samples= new ON_2dPointArray();
    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots+1];
    curve->GetSpanVector(knots);

    int istart = 0;
    while (t >= knots[istart])
	istart++;

    if (istart > 0) {
	istart--;
	knots[istart] = t;
    }

    int istop = numKnots;
    while (s <= knots[istop])
	istop--;

    if (istop < numKnots) {
	istop++;
	knots[istop] = s;
    }

    //TODO: remove debugging code
    //std::cerr << "t - " << t << " istart - " << istart << "knots[istart] - " << knots[istart] << std::endl;
    //std::cerr << "s - " << s << " istop - " << istop << "knots[istop] - " << knots[istop] << std::endl;

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3*degree;
    } else {
	samplesperknotinterval = 18*degree;
    }
    ON_2dPoint pt;
    for (int i=istart; i<=istop; i++) {
	if (i <= numKnots/2) {
	    if (i>0) {
		double delta = (knots[i] - knots[i-1])/(double)samplesperknotinterval;
		for (int j=1; j<samplesperknotinterval; j++) {
		    if (toUV(*data, pt, knots[i-1]+j*delta, PBC_FROM_OFFSET)) {
			samples->Append(pt);
		    } else {
			//std::cout << "didn't find point on surface" << std::endl;
		    }
		}
	    }
	    if (toUV(*data, pt, knots[i], PBC_FROM_OFFSET)) {
		samples->Append(pt);
	    } else {
		//std::cout << "didn't find point on surface" << std::endl;
	    }
	} else {
	    if (i>0) {
		double delta = (knots[i] - knots[i-1])/(double)samplesperknotinterval;
		for (int j=1; j<samplesperknotinterval; j++) {
		    if (toUV(*data, pt, knots[i-1]+j*delta, -PBC_FROM_OFFSET)) {
			samples->Append(pt);
		    } else {
			//std::cout << "didn't find point on surface" << std::endl;
		    }
		}
		if (toUV(*data, pt, knots[i], -PBC_FROM_OFFSET)) {
		    samples->Append(pt);
		} else {
		    //std::cout << "didn't find point on surface" << std::endl;
		}
	    }
	}
    }
    delete [] knots;
    return samples;
}


PBCData *
pullback_samples(const brlcad::SurfaceTree* surfacetree,
		 const ON_Curve* curve,
		 double tolerance,
		 double flatness)
{
    PBCData *data = new PBCData;
    data->tolerance = tolerance;
    data->flatness = flatness;
    data->curve = curve;
    data->surftree = (brlcad::SurfaceTree*)surfacetree;
    const ON_Surface *surf = data->surftree->getSurface();

    double tmin, tmax;
    data->curve->GetDomain(&tmin, &tmax);

    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	if ((tmin < 0.0) && (tmax > 0.0)) {
	    ON_2dPoint uv;
	    if (toUV(*data, uv, 0.0, PBC_TOL)) {
		if (IsAtSeam(surf, uv.x, uv.y) > 0) {
		    ON_2dPointArray *samples1 = pullback_samples(data, tmin, 0.0);
		    ON_2dPointArray *samples2 = pullback_samples(data, 0.0, tmax);
		    if (samples1 != NULL) {
			data->segments.push_back(samples1);
		    }
		    if (samples2 != NULL) {
			data->segments.push_back(samples2);
		    }
		    //TODO: remove debugging code
		    if (false)
			std::cerr << "need to divide curve across the seam" << std::endl;
		} else {
		    ON_2dPointArray *samples = pullback_samples(data, tmin, tmax);
		    if (samples != NULL) {
			data->segments.push_back(samples);
		    }
		}
	    } else {
		std::cerr << "pullback_samples:Error: cannot evaluate curve at parameter 0.0" << std::endl;
		return NULL;
	    }
	} else {
	    ON_2dPointArray *samples = pullback_samples(data, tmin, tmax);
	    if (samples != NULL) {
		data->segments.push_back(samples);
	    }
	}
    } else {
	ON_2dPointArray *samples = pullback_samples(data, tmin, tmax);
	if (samples != NULL) {
	    data->segments.push_back(samples);
	}
    }
    return data;
}


ON_Curve*
refit_edge(const ON_BrepEdge* edge, double UNUSED(tolerance))
{
    double edge_tolerance = 0.01;
    ON_Brep *brep = edge->Brep();
    ON_3dPoint start = edge->PointAtStart();
    ON_3dPoint end = edge->PointAtEnd();

    ON_BrepTrim& trim1 = brep->m_T[edge->m_ti[0]];
    ON_BrepTrim& trim2 = brep->m_T[edge->m_ti[1]];
    ON_BrepFace *face1 = trim1.Face();
    ON_BrepFace *face2 = trim2.Face();
    const ON_Surface *surface1 = face1->SurfaceOf();
    const ON_Surface *surface2 = face2->SurfaceOf();
    bool removeTrimmed = false;
    brlcad::SurfaceTree *st1 = new brlcad::SurfaceTree(face1, removeTrimmed);
    brlcad::SurfaceTree *st2 = new brlcad::SurfaceTree(face2, removeTrimmed);

    ON_Curve *curve = brep->m_C3[edge->m_c3i];
    double t0, t1;
    curve->GetDomain(&t0, &t1);
    ON_Plane plane;
    curve->FrameAt(t0, plane);
    ON_3dPoint origin = plane.Origin();
    ON_3dVector xaxis = plane.Xaxis();
    ON_3dVector yaxis = plane.Yaxis();
    ON_3dVector zaxis = plane.zaxis;
    ON_3dPoint px = origin + xaxis;
    ON_3dPoint py = origin + yaxis;
    ON_3dPoint pz = origin + zaxis;

    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots+1];
    curve->GetSpanVector(knots);

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3*degree;
    } else {
	samplesperknotinterval = 18*degree;
    }
    ON_2dPoint pt;
    double t = 0.0;
    ON_3dPoint pointOnCurve;
    ON_3dPoint knudgedPointOnCurve;
    for (int i = 0; i <= numKnots; i++) {
	if (i <= numKnots / 2) {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;
		    while (!found) {
			ON_2dPoint uv;
			if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
	    }
	    t = knots[i];
	    pointOnCurve = curve->PointAt(t);
	    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);
	    ON_3dPoint point = pointOnCurve;
	    ON_3dPoint knudgepoint = knudgedPointOnCurve;
	    ON_3dPoint ps1;
	    ON_3dPoint ps2;
	    bool found = false;
	    double dist;

	    while (!found) {
		ON_2dPoint uv;
		if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
		    ps1 = surface1->PointAt(uv.x, uv.y);
		    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps2 = surface2->PointAt(uv.x, uv.y);
		    }
		}
		dist = ps1.DistanceTo(ps2);
		if (NEAR_ZERO(dist, PBC_TOL)) {
		    point = ps1;
		    found = true;
		} else {
		    ON_3dVector v1 = ps1 - point;
		    ON_3dVector v2 = ps2 - point;
		    knudgepoint = point;
		    ON_3dVector deltav = v1 + v2;
		    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			found = true; // as close as we are going to get
		    } else {
			point = point + v1 + v2;
		    }
		}
	    }
	} else {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;

		    while (!found) {
			ON_2dPoint uv;
			if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
		t = knots[i];
		pointOnCurve = curve->PointAt(t);
		knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);
		ON_3dPoint point = pointOnCurve;
		ON_3dPoint knudgepoint = knudgedPointOnCurve;
		ON_3dPoint ps1;
		ON_3dPoint ps2;
		bool found = false;
		double dist;

		while (!found) {
		    ON_2dPoint uv;
		    if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps1 = surface1->PointAt(uv.x, uv.y);
			if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps2 = surface2->PointAt(uv.x, uv.y);
			}
		    }
		    dist = ps1.DistanceTo(ps2);
		    if (NEAR_ZERO(dist, PBC_TOL)) {
			point = ps1;
			found = true;
		    } else {
			ON_3dVector v1 = ps1 - point;
			ON_3dVector v2 = ps2 - point;
			knudgepoint = point;
			ON_3dVector deltav = v1 + v2;
			if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			    found = true; // as close as we are going to get
			} else {
			    point = point + v1 + v2;
			}
		    }
		}
	    }
	}
    }
    delete [] knots;


    return NULL;
}


bool
has_singularity(const ON_Surface *surf)
{
    bool ret = false;
    // 0 = south, 1 = east, 2 = north, 3 = west
    for (int i=0;i<4;i++) {
	if (surf->IsSingular(i)) {
/*
  switch (i) {
  case 0:
  std::cout << "Singular South" << std::endl;
  break;
  case 1:
  std::cout << "Singular East" << std::endl;
  break;
  case 2:
  std::cout << "Singular North" << std::endl;
  break;
  case 3:
  std::cout << "Singular West" << std::endl;
  }
*/
	    ret= true;
	}
    }
    return ret;
}


bool is_closed(const ON_Surface *surf)
{
    bool ret = false;
    // dir 0 = "s", 1 = "t"
    for (int i=0;i<2;i++) {
	if (surf->IsClosed(i)) {
//			switch (i) {
//			case 0:
//				std::cout << "Closed in U" << std::endl;
//				break;
//			case 1:
//				std::cout << "Closed in V" << std::endl;
//			}
	    ret = true;
	}
    }
    return ret;
}


bool
check_pullback_closed(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    const ON_Surface *surf = (*d)->surftree->getSurface();
    //TODO:
    // 0 = U, 1 = V
    if (surf->IsClosed(0) && surf->IsClosed(1)) {
	//TODO: need to check how torus UV looks to determine checks
	std::cerr << "Is this some kind of torus????" << std::endl;
    } else if (surf->IsClosed(0)) {
	//check_pullback_closed_U(pbcs);
	std::cout << "check closed in U" << std::endl;
    } else if (surf->IsClosed(1)) {
	//check_pullback_closed_V(pbcs);
	std::cout << "check closed in V" << std::endl;
    }
    return true;
}


bool
check_pullback_singular_east(std::list<PBCData*> &pbcs)
{
    std::list<PBCData *>::iterator cs = pbcs.begin();
    const ON_Surface *surf = (*cs)->surftree->getSurface();
    double umin, umax;
    ON_2dPoint *prev = NULL;
    ON_2dPoint *noprev = NULL;

    surf->GetDomain(0, &umin, &umax);
    std::cout << "Umax: " << umax << std::endl;
    while (cs!=pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	int segcnt = 0;
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    std::cerr << std::endl << "Segment:" << ++segcnt << std::endl;
	    if (true) {
		int ilast = samples->Count() - 1;
		std::cerr << std::endl << 0 << "- " << (*samples)[0].x << ", " << (*samples)[0].y << std::endl;
		std::cerr << ilast << "- " << (*samples)[ilast].x << ", " << (*samples)[ilast].y << std::endl;
	    } else {
		for (int i = 0; i < samples->Count(); i++) {
		    if (NEAR_EQUAL((*samples)[i].x, umax, PBC_TOL)) {
			if (prev != NULL) {
			    std::cerr << "prev - " << prev->x << ", " << prev->y << std::endl;
			} else {
			    noprev = &(*samples)[i];
			}
			std::cerr << i << "- " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl << std::endl;
		    }
		    prev = &(*samples)[i];
		}
	    }
	    si ++;
	}
	cs++;
    }
    //std::cerr << "noprev - " << noprev->x << ", " << noprev->y << std::endl;
    //std::cerr << "last - " << prev->x << ", " << prev->y << std::endl;
    return true;
}


bool
check_pullback_singular(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    const ON_Surface *surf = (*d)->surftree->getSurface();
    int cnt = 0;

    for (int i=0;i<4;i++) {
	if (surf->IsSingular(i)) {
	    cnt++;
	}
    }
    if (cnt > 2) {
	//TODO: I don't think this makes sense but check out
	std::cerr << "Is this some kind of sickness????" << std::endl;
	return false;
    } else if (cnt == 2) {
	if (surf->IsSingular(0) && surf->IsSingular(2)) {
	    std::cout << "check singular North-South" << std::endl;
	} else if (surf->IsSingular(1) && surf->IsSingular(2)) {
	    std::cout << "check singular East-West" << std::endl;
	} else {
	    //TODO: I don't think this makes sense but check out
	    std::cerr << "Is this some kind of sickness????" << std::endl;
	    return false;
	}
    } else {
	if (surf->IsSingular(0)) {
	    std::cout << "check singular South" << std::endl;
	} else if (surf->IsSingular(1)) {
	    std::cout << "check singular East" << std::endl;
	    if (check_pullback_singular_east(pbcs)) {
		return true;
	    }
	} else if (surf->IsSingular(2)) {
	    std::cout << "check singular North" << std::endl;
	} else if (surf->IsSingular(3)) {
	    std::cout << "check singular West" << std::endl;
	}
    }
    return true;
}


void
print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    int trimcnt=0;
    if (justendpoints) {
	// print out endpoints before
	std::cerr << "EndPoints " << str << ":"<< std::endl;
	while (cs!=pbcs.end()) {
	    PBCData *data = (*cs);
	    const ON_Surface *surf = data->surftree->getSurface();
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    int segcnt = 0;
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		int ilast = samples->Count() - 1;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		int i=0;
		int singularity=IsAtSingularity(surf, (*samples)[i].x, (*samples)[i].y);
		int seam=IsAtSeam(surf, (*samples)[i].x, (*samples)[i].y);
		std::cerr << "--------";
		if ((seam>0) && (singularity>=0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (seam>0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (singularity>=0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
		}
		ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") "<< std::endl;

		i=ilast;
		singularity=IsAtSingularity(surf, (*samples)[i].x, (*samples)[i].y);
		seam=IsAtSeam(surf, (*samples)[i].x, (*samples)[i].y);
		std::cerr << "        ";
		if ((seam>0) && (singularity>=0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (seam>0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (singularity>=0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		}
		p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") "<< std::endl;
		si++;
	    }
	    cs++;
	}
    } else {
	// print out all points
	trimcnt=0;
	cs = pbcs.begin();
	std::cerr << str << ":" << std::endl;
	while (cs!=pbcs.end()) {
	    PBCData *data = (*cs);
	    const ON_Surface *surf = data->surftree->getSurface();
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    int segcnt = 0;
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		for (int i = 0; i < samples->Count(); i++) {
		    int singularity=IsAtSingularity(surf, (*samples)[i].x, (*samples)[i].y);
		    int seam=IsAtSeam(surf, (*samples)[i].x, (*samples)[i].y);
		    if (i == 0) {
			std::cerr << "--------";
		    } else {
			std::cerr << "        ";
		    }
		    if ((seam>0) && (singularity>=0)) {
			std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
		    } else if (seam>0) {
			std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
		    } else if (singularity>=0) {
			std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
		    } else {
			std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
		    }
		    ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		    std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") "<< std::endl;
		}
		si++;
	    }
	    cs++;
	}
    }
    /////
}


bool
resolve_seam_segment_from_prev(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *prev = NULL)
{
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin+umax)/2.0;
    vmid = (vmin+vmax)/2.0;

    for (int i = 0; i < segment.Count(); i++) {
	int singularity=IsAtSingularity(surface, segment[i].x, segment[i].y);
	if (singularity < 0) {
	    int seam=IsAtSeam(surface, segment[i].x, segment[i].y);
	    if ((seam > 0)) {
		if (prev != NULL) {
		    //std::cerr << " at seam " << seam << " but has prev" << std::endl;
		    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		    switch (seam) {
			case 1: //east/west
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		prev = &segment[i];
	    }
	} else {
	    prev = NULL;
	}
    }
    return complete;
}


bool
resolve_seam_segment_from_next(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *next = NULL)
{
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin+umax)/2.0;
    vmid = (vmin+vmax)/2.0;

    if (next != NULL) {
	complete=true;
	for (int i = segment.Count()-1; i >= 0; i--) {
	    int singularity=IsAtSingularity(surface, segment[i].x, segment[i].y);
	    if (singularity < 0) {
		int seam=IsAtSeam(surface, segment[i].x, segment[i].y);
		if ((seam > 0)) {
		    if (next != NULL) {
			switch (seam) {
			    case 1: //east/west
				if (next->x < umid) {
				    segment[i].x = umin;
				} else {
				    segment[i].x = umax;
				}
				break;
			    case 2: //north/south
				if (next->y < vmid) {
				    segment[i].y = vmin;
				} else {
				    segment[i].y = vmax;
				}
				break;
			    case 3: //both
				if (next->x < umid) {
				    segment[i].x = umin;
				} else {
				    segment[i].x = umax;
				}
				if (next->y < vmid) {
				    segment[i].y = vmin;
				} else {
				    segment[i].y = vmax;
				}
			}
		    } else {
			//std::cerr << " at seam and no prev" << std::endl;
			complete = false;
		    }
		} else {
		    next = &segment[i];
		}
	    } else {
		next = NULL;
	    }
	}
    }
    return complete;
}


bool
resolve_seam_segment(const ON_Surface *surface, ON_2dPointArray &segment)
{
    ON_2dPoint *prev = NULL;
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin+umax)/2.0;
    vmid = (vmin+vmax)/2.0;

    for (int i = 0; i < segment.Count(); i++) {
	int singularity=IsAtSingularity(surface, segment[i].x, segment[i].y);
	if (singularity < 0) {
	    int seam=IsAtSeam(surface, segment[i].x, segment[i].y);
	    if ((seam > 0)) {
		if (prev != NULL) {
		    //std::cerr << " at seam " << seam << " but has prev" << std::endl;
		    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		    switch (seam) {
			case 1: //east/west
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		prev = &segment[i];
	    }
	} else {
	    prev = NULL;
	}
    }
    if ((!complete) && (prev != NULL)) {
	complete=true;
	for (int i = segment.Count()-2; i >= 0; i--) {
	    int singularity=IsAtSingularity(surface, segment[i].x, segment[i].y);
	    if (singularity < 0) {
		int seam=IsAtSeam(surface, segment[i].x, segment[i].y);
		if ((seam > 0)) {
		    if (prev != NULL) {
			//std::cerr << " at seam " << seam << " but has prev" << std::endl;
			//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
			//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			switch (seam) {
			    case 1: //east/west
				if (prev->x < umid) {
				    segment[i].x = umin;
				} else {
				    segment[i].x = umax;
				}
				break;
			    case 2: //north/south
				if (prev->y < vmid) {
				    segment[i].y = vmin;
				} else {
				    segment[i].y = vmax;
				}
				break;
			    case 3: //both
				if (prev->x < umid) {
				    segment[i].x = umin;
				} else {
				    segment[i].x = umax;
				}
				if (prev->y < vmid) {
				    segment[i].y = vmin;
				} else {
				    segment[i].y = vmax;
				}
			}
		    } else {
			//std::cerr << " at seam and no prev" << std::endl;
			complete = false;
		    }
		} else {
		    prev = &segment[i];
		}
	    } else {
		prev = NULL;
	    }
	}
    }
    return complete;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_seams(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs;

    //TODO: remove debugging
    if (false)
	print_pullback_data("Before seam cleanup", pbcs, false);

    ///// Loop through and fix any seam ambiguities
    ON_2dPoint *prev = NULL;
    ON_2dPoint *next = NULL;
    cs = pbcs.begin();
    while (cs!=pbcs.end()) {
	PBCData *data = (*cs);
	const ON_Surface *surf = data->surftree->getSurface();

	double umin, umax, umid;
	double vmin, vmax, vmid;
	surf->GetDomain(0, &umin, &umax);
	surf->GetDomain(1, &vmin, &vmax);
	umid = (umin+umax)/2.0;
	vmid = (vmin+vmax)/2.0;

	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    if (resolve_seam_segment(surf, *samples)) {
		// Found a starting point
		//1) walk back up with resolved next point
		next = &(*samples)[0];
		std::list<PBCData*>::reverse_iterator rcs(cs);
		rcs--;
		std::list<ON_2dPointArray *>::reverse_iterator rsi(si);
		while (rcs != pbcs.rend()) {
		    PBCData *rdata = (*rcs);
		    if (data->segments.rend() == rdata->segments.rend()) {
			//TODO: remove debugging
			if (false)
			    std::cerr << "Ends match" << std::endl;
		    }
		    while (rsi != rdata->segments.rend()) {
			ON_2dPointArray *rsamples = (*rsi);
			// first try and resolve on own merits
			if (!resolve_seam_segment(surf, *rsamples)) {
			    resolve_seam_segment_from_next(surf, *rsamples, next);
			}
			next = &(*rsamples)[0];
			rsi++;
		    }
		    rcs++;
		    if (rcs != pbcs.rend()) {
			rdata = (*rcs);
			rsi = rdata->segments.rbegin();
		    }
		}

		//2) walk rest of way down with resolved prev point
		if (samples->Count() > 1)
		    prev = &(*samples)[samples->Count() -1];
		else
		    prev = NULL;
		si++;
		while (cs != pbcs.end()) {
		    while (si != data->segments.end()) {
			samples = (*si);
			// first try and resolve on own merits
			if (!resolve_seam_segment(surf, *samples)) {
			    resolve_seam_segment_from_prev(surf, *samples, prev);
			}
			if (samples->Count() > 1)
			    prev = &(*samples)[samples->Count() -1];
			else
			    prev = NULL;
			si++;
		    }
		    cs++;
		    if (cs != pbcs.end()) {
			data = (*cs);
			si = data->segments.begin();
		    }
		}
	    }
	    if (si != data->segments.end())
		si++;
	}
	if (cs != pbcs.end())
	    cs++;
    }
    //TODO: remove debugging
    if (false)
	print_pullback_data("After seam cleanup", pbcs, false);

    return true;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_singularities(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();

    //TODO: remove debugging
    if (false)
	print_pullback_data("Before singularity cleanup", pbcs, false);

    ///// Loop through and fix any seam ambiguities
    ON_2dPoint *prev = NULL;
    bool complete = false;
    int checkcnt = 0;

    prev = NULL;
    complete = false;
    checkcnt = 0;
    while (!complete && (checkcnt < 2)) {
	cs = pbcs.begin();
	complete = true;
	checkcnt++;
	//std::cerr << "Checkcnt - " << checkcnt << std::endl;
	while (cs!=pbcs.end()) {
	    int singularity;
	    prev=NULL;
	    PBCData *data = (*cs);
	    const ON_Surface *surf = data->surftree->getSurface();
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		for (int i = 0; i < samples->Count(); i++) {
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    if ((singularity=IsAtSingularity(surf, (*samples)[i].x, (*samples)[i].y)) >= 0) {
			if (prev != NULL) {
			    //std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
			    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    switch (singularity) {
				case 0: //south
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[0];
				    break;
				case 1: //east
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[1];
				    break;
				case 2: //north
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[1];
				    break;
				case 3: //west
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[0];
			    }
			    prev = NULL;
			    //std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			} else {
			    //std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    complete = false;
			}
		    } else {
			prev = &(*samples)[i];
		    }
		}
		if (!complete) {
		    //std::cerr << "Lets work backward:" << std::endl;
		    for (int i = samples->Count()-2; i >= 0; i--) {
			// 0 = south, 1 = east, 2 = north, 3 = west
			if ((singularity=IsAtSingularity(surf, (*samples)[i].x, (*samples)[i].y)) >= 0) {
			    if (prev != NULL) {
				//std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
				//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				switch (singularity) {
				    case 0: //south
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[0];
					break;
				    case 1: //east
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[1];
					break;
				    case 2: //north
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[1];
					break;
				    case 3: //west
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[0];
				}
				prev = NULL;
				//std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    } else {
				//std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				complete = false;
			    }
			} else {
			    prev = &(*samples)[i];
			}
		    }
		}
		si++;
	    }
	    cs++;
	}
    }

    //TODO: remove debugging
    if (false)
	print_pullback_data("After singularity cleanup", pbcs, false);

    return true;
}


void
remove_consecutive_intersegment_duplicates(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    while (cs!=pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    if (samples->Count() == 0) {
		si = data->segments.erase(si);
	    } else {
		for (int i = 0; i < samples->Count()-1; i++) {
		    while ((i < (samples->Count()-1)) && (*samples)[i].DistanceTo((*samples)[i+1]) < 1e-9) {
			//TODO: remove debugging code
			if (false)
			    std::cerr << "Sample Count was " << samples->Count();
			samples->Remove(i+1);
			//TODO: remove debugging code
			if (false)
			    std::cerr << " now " << samples->Count() << std::endl;
		    }
		}
		si++;
	    }
	}
	if (data->segments.empty()) {
	    cs = pbcs.erase(cs);
	} else {
	    cs++;
	}
    }
}


bool
check_pullback_data(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();

    //TODO: remove debugging code
    if (false)
	print_pullback_data("Before cleanup", pbcs, false);

    const ON_Surface *surf = (*d)->surftree->getSurface();

    bool singular = has_singularity(surf);
    bool closed = is_closed(surf);

    if (closed) {
	if (!resolve_pullback_seams(pbcs)) {
	    std::cerr << "Error: Can not resolve seam ambiguities." << std::endl;
	    return false;
	}
    }
    if (singular) {
	if (!resolve_pullback_singularities(pbcs)) {
	    std::cerr << "Error: Can not resolve singular ambiguities." << std::endl;
	}
    }

    // consecutive duplicates within segment will cause problems in curve fit
    remove_consecutive_intersegment_duplicates(pbcs);

    //TODO: remove debugging code
    if (false)
	print_pullback_data("After cleanup", pbcs, false);

    return true;
}


int
check_pullback_singularity_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (has_singularity(surf)) {
	int is, js;
	if (((is=IsAtSingularity(surf, p1.x, p1.y)) >= 0) && ((js=IsAtSingularity(surf, p2.x, p2.y)) >= 0)) {
	    //create new singular trim
	    if (is == js) {
		return is;
	    }
	}
    }
    return -1;
}


int
check_pullback_seam_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (is_closed(surf)) {
	int is, js;
	if (((is=IsAtSeam(surf, p1.x, p1.y)) > 0) && ((js=IsAtSeam(surf, p2.x, p2.y)) > 0)) {
	    //create new seam trim
	    if (is == js) {
		// need to check if seam 3d points are equal
		double endpoint_distance = p1.DistanceTo(p2);
		double t0, t1;

		int dir = is - 1;
		surf->GetDomain(dir, &t0, &t1);
		if (endpoint_distance > 0.5*(t1-t0)) {
		    return is;
		}
	    }
	}
    }
    return -1;
}


ON_Curve*
pullback_curve(const brlcad::SurfaceTree* surfacetree,
	       const ON_Curve* curve,
	       double tolerance,
	       double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments.push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) { return NULL; } // fails if first point is out of tolerance!

    ON_2dPoint uv;
    ON_3dPoint p = curve->PointAt(tmin);
    ON_3dPoint from = curve->PointAt(tmin+0.0001);
    brlcad::SurfaceTree *st = (brlcad::SurfaceTree *)surfacetree;
    if (!st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) > 0) {
	std::cerr << "Error: Can not get surface point." << std::endl;
    }

    ON_2dPoint p1, p2;
    const ON_Surface *surf = (data.surftree)->getSurface();

    toUV(data, p1, tmin, PBC_TOL);
    ON_3dPoint a = surf->PointAt(p1.x, p1.y);
    toUV(data, p2, tmax, -PBC_TOL);
    ON_3dPoint b = surf->PointAt(p2.x, p2.y);

    p = curve->PointAt(tmax);
    from = curve->PointAt(tmax-0.0001);
    if (!st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) > 0) {
	std::cerr << "Error: Can not get surface point." << std::endl;
    }

    if (!sample(data, tmin, tmax, p1, p2)) {
	return NULL;
    }

    for (int i = 0; i < samples.Count(); i++) {
	std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
    }

    return interpolateCurve(samples);
}


ON_Curve*
pullback_seam_curve(enum seam_direction seam_dir,
		    const brlcad::SurfaceTree* surfacetree,
		    const ON_Curve* curve,
		    double tolerance,
		    double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments.push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) { return NULL; } // fails if first point is out of tolerance!

    ON_2dPoint p1, p2;

    toUV(data, p1, tmin, PBC_TOL);
    toUV(data, p2, tmax, -PBC_TOL);
    if (!sample(data, tmin, tmax, p1, p2)) {
	return NULL;
    }

    for (int i = 0; i < samples.Count(); i++) {
	if (seam_dir == NORTH_SEAM) {
	    samples[i].y = 1.0;
	} else if (seam_dir == EAST_SEAM) {
	    samples[i].x = 1.0;
	} else if (seam_dir == SOUTH_SEAM) {
	    samples[i].y = 0.0;
	} else if (seam_dir == WEST_SEAM) {
	    samples[i].x = 0.0;
	}
	std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
    }

    return interpolateCurve(samples);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
