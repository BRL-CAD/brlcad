/*                        E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file edit.cpp
 *
 * Implementation of edit support for brep.
 *
 */

#include "brep/edit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include <vector>

void *brep_create()
{
    ON_Brep *brep = new ON_Brep();
    return (void *)brep;
}

int brep_vertex_create(ON_Brep *brep, ON_3dPoint point)
{
    ON_BrepVertex& v = brep->NewVertex(point);
    v.m_tolerance = 0.0;
    return brep->m_V.Count() - 1;
}

bool brep_vertex_remove(ON_Brep *brep, int v_id)
{
    if (v_id < 0 || v_id >= brep->m_V.Count()) {
	bu_log("v_id is out of range\n");
	return false;
    }
    brep->m_V.Remove(v_id);
    return true;
}

int brep_curve2d_make_line(ON_Brep *brep, const ON_2dPoint &from, const ON_2dPoint &to)
{
    ON_Curve* c2 = new ON_LineCurve(from, to);
    c2->SetDomain(0.0, 1.0);
    brep->m_C2.Append(c2);
    return brep->m_C2.Count() - 1;
}

bool brep_curve2d_remove(ON_Brep *brep, int curve_id)
{
    if (curve_id < 0 || curve_id >= brep->m_C2.Count()) {
	bu_log("curve_id is out of range\n");
	return false;
    }
    brep->m_C2.Remove(curve_id);
    return true;
}

int brep_curve_make(ON_Brep *brep, const ON_3dPoint &position)
{
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, 3, 4);
    curve->SetCV(0, ON_3dPoint(-0.1, -1.5, 0));
    curve->SetCV(1, ON_3dPoint(0.1, -0.5, 0));
    curve->SetCV(2, ON_3dPoint(0.1, 0.5, 0));
    curve->SetCV(3, ON_3dPoint(-0.1, 1.5, 0));
    curve->MakeClampedUniformKnotVector();
    curve->Translate(position);
    return brep->AddEdgeCurve(curve);
}

// TODO: add more options about knot vector
int brep_curve_in(ON_Brep *brep, bool is_rational, int order, int cv_count, std::vector<ON_4dPoint> cv)
{
    int dim = 3;
    if (cv.size() != (size_t)cv_count) {
	bu_log("cv_count is not equal to cv.size()\n");
	return -1;
    }
    ON_NurbsCurve *curve = ON_NurbsCurve::New(dim, is_rational, order, cv_count);

    for (int i = 0; i < cv_count; i++) {
	curve->SetCV(i, cv[i]);
    }

    // make uniform knot vector
    curve->MakeClampedUniformKnotVector();
    return brep->AddEdgeCurve(curve);
}

ON_NurbsCurve *brep_get_nurbs_curve(ON_Brep *brep, int curve_id)
{
    if (curve_id < 0 || curve_id >= brep->m_C3.Count()) {
	bu_log("curve_id is out of range\n");
	return NULL;
    }
    ON_NurbsCurve *curve = dynamic_cast<ON_NurbsCurve *>(brep->m_C3[curve_id]);

    if (!curve) {
	bu_log("curve %d is not a NURBS curve\n", curve_id);
	return NULL;
    }
    return curve;
}

ON_NurbsSurface *brep_get_nurbs_surface(ON_Brep *brep, int surface_id)
{
    if (surface_id < 0 || surface_id >= brep->m_S.Count()) {
	bu_log("surface_id is out of range\n");
	return NULL;
    }
    ON_NurbsSurface *surface = dynamic_cast<ON_NurbsSurface *>(brep->m_S[surface_id]);

    if (!surface) {
	bu_log("surface %d is not a NURBS surface\n", surface_id);
	return NULL;
    }
    return surface;
}

/**
 * calculate tangent vectors for each point
 */
std::vector<ON_3dVector> calculateTangentVectors(const std::vector<ON_3dPoint> &points);

/**
 * calculate control points and knot vector for interpolating a curve
 */
void calcuBsplineCVsKnots(std::vector<ON_3dPoint> &cvs, std::vector<double> &knots, const std::vector<ON_3dPoint> &endPoints, const std::vector<ON_3dVector> &tangentVectors);

int brep_curve_interpCrv(ON_Brep *brep, std::vector<ON_3dPoint> points)
{
    std::vector<ON_3dVector> tangentVectors = calculateTangentVectors(points);
    std::vector<ON_3dPoint> controlPs;
    std::vector<double> knotVector;

    calcuBsplineCVsKnots(controlPs, knotVector, points, tangentVectors);
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, 4, controlPs.size());
    for (size_t i = 0; i < controlPs.size(); i++) {
	curve->SetCV(i, controlPs[i]);
    }
    for(size_t i = 0; i < knotVector.size(); i++) {
	curve->SetKnot(i, knotVector[i]);
    }
    return brep->AddEdgeCurve(curve);
}

int brep_curve_copy(ON_Brep *brep, int curve_id)
{
    if (curve_id < 0 || curve_id >= brep->m_C3.Count()) {
	bu_log("curve_id is out of range\n");
	return -1;
    }
    ON_Curve *curve = brep->m_C3[curve_id];
    if (!curve) {
	return -1;
    }
    ON_Curve *curve_copy = curve->DuplicateCurve();
    return brep->AddEdgeCurve(curve_copy);
}

bool brep_curve_remove(ON_Brep *brep, int curve_id)
{
    if (curve_id < 0 || curve_id >= brep->m_C3.Count()) {
	bu_log("curve_id is out of range\n");
	return false;
    }
    brep->m_C3.Remove(curve_id);
    return true;
}

bool brep_curve_move(ON_Brep *brep, int curve_id, const ON_3dVector &point)
{
    /// the curve could be a NURBS curve or not
    if (curve_id < 0 || curve_id >= brep->m_C3.Count()) {
	bu_log("curve_id is out of range\n");
	return false;
    }
    ON_Curve *curve = brep->m_C3[curve_id];
    if (!curve) {
	return false;
    }
    return curve->Translate(point);
}

bool brep_curve_set_cv(ON_Brep *brep, int curve_id, int cv_id, const ON_4dPoint &point)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve) {
	return false;
    }
    return curve->SetCV(cv_id, point);
}

bool brep_curve_reverse(ON_Brep *brep, int curve_id)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve) {
	return false;
    }
    return curve->Reverse();
}

bool brep_curve_insert_knot(ON_Brep *brep, int curve_id, double knot, int multiplicity)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve) {
	return false;
    }
    return curve->InsertKnot(knot, multiplicity);
}

bool brep_curve_trim(ON_Brep *brep, int curve_id, double t0, double t1)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve) {
	return false;
    }
    ON_Interval interval(t0, t1);
    return curve->Trim(interval);
}

bool brep_curve_split(ON_Brep *brep, int curve_id, double t)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve) {
	return false;
    }
    ON_Curve *curve1 = NULL;
    ON_Curve *curve2 = NULL;
    bool flag = curve->Split(t, curve1, curve2);
    if (flag) {
	brep->m_C3.Remove(curve_id);
	brep->AddEdgeCurve(curve1);
	brep->AddEdgeCurve(curve2);
	bu_log("old curve removed, id: %d, new curve id: %d, %d\n", curve_id, brep->m_C3.Count() - 2, brep->m_C3.Count() - 1);
    }
    return flag;
}

int brep_curve_join(ON_Brep *brep, int curve_id_1, int curve_id_2)
{
    ON_NurbsCurve *curve1 = brep_get_nurbs_curve(brep, curve_id_1);
    ON_NurbsCurve *curve2 = brep_get_nurbs_curve(brep, curve_id_2);
    if (!curve1 || !curve2) {
	return -1;
    }

    /// force move ends of the two curves to the same location
    if (!ON_ForceMatchCurveEnds(*curve1, 1, *curve2, 0)) {
	bu_log("ON_ForceMatchCurveEnds failed\n");
	return -1;
    }

    ON_SimpleArray<const ON_Curve *> in_curves;
    in_curves.Append(curve1);
    in_curves.Append(curve2);

    ON_SimpleArray<ON_Curve *> out_curves;
    /// join the two curves
    int joined_num = ON_JoinCurves(in_curves, out_curves, 0.0f, 0.0f, false);
    if (joined_num != 1) {
	bu_log("ON_JoinCurves failed\n");
	return -1;
    }

    /// remove the two curves.
    /// Remark: the index of m_C3 is massed up after removing the curves!
    brep->m_C3.Remove(curve_id_1);
    brep->m_C3.Remove(curve_id_2 - (curve_id_1 < curve_id_2 ? 1 : 0));

    return brep->AddEdgeCurve(*out_curves.At(0));
}

int brep_surface_make(ON_Brep *brep, const ON_3dPoint &position)
{
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, true, 3, 3, 4, 4);
    surface->SetCV(0, 0, ON_4dPoint(-1.5, -1.5, 0, 1));
    surface->SetCV(0, 1, ON_4dPoint(-0.5, -1.5, 0, 1));
    surface->SetCV(0, 2, ON_4dPoint(0.5, -1.5, 0, 1));
    surface->SetCV(0, 3, ON_4dPoint(1.5, -1.5, 0, 1));
    surface->SetCV(1, 0, ON_4dPoint(-1.5, -0.5, 0, 1));
    surface->SetCV(1, 1, ON_4dPoint(-0.5, -0.5, 0.5, 1));
    surface->SetCV(1, 2, ON_4dPoint(0.5, -0.5, 0.5, 1));
    surface->SetCV(1, 3, ON_4dPoint(1.5, -0.5, 0, 1));
    surface->SetCV(2, 0, ON_4dPoint(-1.5, 0.5, 0, 1));
    surface->SetCV(2, 1, ON_4dPoint(-0.5, 0.5, 0.5, 1));
    surface->SetCV(2, 2, ON_4dPoint(0.5, 0.5, 0.5, 1));
    surface->SetCV(2, 3, ON_4dPoint(1.5, 0.5, 0, 1));
    surface->SetCV(3, 0, ON_4dPoint(-1.5, 1.5, 0, 1));
    surface->SetCV(3, 1, ON_4dPoint(-0.5, 1.5, 0, 1));
    surface->SetCV(3, 2, ON_4dPoint(0.5, 1.5, 0, 1));
    surface->SetCV(3, 3, ON_4dPoint(1.5, 1.5, 0, 1));
    surface->MakeClampedUniformKnotVector(0, 1);
    surface->MakeClampedUniformKnotVector(1, 1);
    surface->Translate(position);
    return brep->AddSurface(surface);
}

int brep_surface_extract_vertex(ON_Brep *brep, int surface_id, double u, double v)
{
    ON_NurbsSurface *surface = brep_get_nurbs_surface(brep, surface_id);
    if (!surface) {
	return -1;
    }
    ON_3dPoint point;
    bool res = surface->Evaluate(u, v, 0, 3, point);
    if(!res) {
	return -1;
    }
    ON_BrepVertex& vertex = brep->NewVertex(point);
    vertex.m_tolerance = 0.0;
    return brep->m_V.Count() - 1;
}

int brep_surface_extract_curve(ON_Brep *brep, int surface_id, int dir, double t)
{
    ON_NurbsSurface *surface = brep_get_nurbs_surface(brep, surface_id);
    if (!surface) {
	return -1;
    }
    ON_Curve *curve = surface->IsoCurve(dir, t);
    if(!curve) {
	return -1;
    }
    return brep->AddEdgeCurve(curve);
}

int brep_surface_copy(ON_Brep *brep, int surface_id)
{
    if (surface_id < 0 || surface_id >= brep->m_S.Count()) {
	bu_log("surface_id is out of range\n");
	return -1;
    }
    ON_Surface *surface = brep->m_S[surface_id];
    if (!surface) {
	return -1;
    }
    ON_Surface *surface_copy = surface->DuplicateSurface();
    return brep->AddSurface(surface_copy);
}

/**
 * caculate parameter values of each point
 */
int SurfMeshParams(int n, int m, std::vector<ON_3dPoint> points, std::vector<double> &uk, std::vector<double> &ul);

/**
 * global cubic curve interpolation with C2 continuity
 * input: n, knots, Q
 * output: P
 * reference: The NURBS Book (2nd Edition), chapter 9.2.3
 * TODO: while n <= 3, the special case should be considered
 */
void globalCurveInterp(int n, std::vector<double> &knots, const std::vector<ON_3dPoint> &Q, std::vector<ON_3dPoint> &P);

int brep_surface_interpCrv(ON_Brep *brep, int cv_count_x, int cv_count_y, std::vector<ON_3dPoint> points)
{
    cv_count_x = cv_count_x < 2 ? 2 : cv_count_x;
    cv_count_y = cv_count_y < 2 ? 2 : cv_count_y;
    if (points.size() != (size_t)(cv_count_x * cv_count_y)) {
	return -1;
    }
    int n = cv_count_x - 1;
    int m = cv_count_y - 1;

    /// calculate parameter values of each point
    std::vector<double> uk, ul;
    SurfMeshParams(n, m, points, uk, ul);

    /// calculate knots of the cubic B-spline surface
    std::vector<double> knots_u, knots_v;
    for (size_t i = 0; i < 4; i++) {
	knots_u.push_back(0);
	knots_v.push_back(0);
    }
    for (size_t i = 1; i < uk.size() - 1; i++) {
	knots_u.push_back(uk[i]);
    }
    for (size_t i = 1; i < ul.size() - 1; i++) {
	knots_v.push_back(ul[i]);
    }
    for (int i = 0; i < 4; i++) {
	knots_u.push_back(1);
	knots_v.push_back(1);
    }

    /// curve interpolation in v direction
    // temporary control points
    std::vector<std::vector<ON_3dPoint>> R(n + 1, std::vector<ON_3dPoint>(m + 3));

    for (int l = 0; l <= n; l++) {
	std::vector<ON_3dPoint> Q(m + 1);
	for (int k = 0; k <= m; k++) {
	    Q[k] = points[l * (m + 1) + k];
	}
	globalCurveInterp(m, knots_v, Q, R[l]);
    }

    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 4, 4, n + 3, m + 3);
    for (int i = 0; i < m + 3; i++) {
	std::vector<ON_3dPoint> Q(n + 1);
	for (int j = 0; j < n + 1; j++) {
	    Q[j] = R[j][i];
	}
	std::vector<ON_3dPoint> P(n + 3);
	globalCurveInterp(n, knots_u, Q, P);
	for (int j = 0; j < n + 3; j++) {
	    surface->SetCV(j, i, P[j]);
	}
    }
    /// the knot vector of openNURBS is different from the NURBS book
    for (size_t i = 1; i < knots_u.size() - 1; i++) {
	surface->SetKnot(0, i - 1, knots_u[i]);
    }
    for (size_t i = 1; i < knots_v.size() - 1; i++) {
	surface->SetKnot(1, i - 1, knots_v[i]);
    }
    return brep->AddSurface(surface);
}

bool brep_surface_move(ON_Brep *brep, int surface_id, const ON_3dVector &point)
{
    /// the surface could be a NURBS surface or not
    if (surface_id < 0 || surface_id >= brep->m_S.Count()) {
	bu_log("surface_id is out of range\n");
	return false;
    }
    ON_Surface *surface = brep->m_S[surface_id];
    if (!surface) {
	return false;
    }
    return surface->Translate(point);
}

bool brep_surface_set_cv(ON_Brep *brep, int surface_id, int cv_id_u, int cv_id_v, const ON_4dPoint &point)
{
    ON_NurbsSurface *surface = brep_get_nurbs_surface(brep, surface_id);
    if (!surface) {
	return false;
    }
    return surface->SetCV(cv_id_u, cv_id_v, point);
}

bool brep_surface_trim(ON_Brep *brep, int surface_id, int dir, double t0, double t1)
{
    ON_NurbsSurface *surface = brep_get_nurbs_surface(brep, surface_id);
    if (!surface) {
	return false;
    }
    ON_Interval interval(t0, t1);
    return surface->Trim(dir, interval);
}

bool brep_surface_split(ON_Brep *brep, int surface_id, int dir, double t)
{
    ON_NurbsSurface *surface = brep_get_nurbs_surface(brep, surface_id);
    if (!surface) {
	return false;
    }
    ON_Surface *surf1=NULL, *surf2=NULL;
    bool flag = surface->Split(dir, t, surf1, surf2);
    if (flag) {
	brep->m_S.Remove(surface_id);
	brep->AddSurface(surf1);
	brep->AddSurface(surf2);
    bu_log("brep_surface_split: split surface %d into %d and %d\n", surface_id, brep->m_S.Count()-2, brep->m_S.Count()-1);
    }
    return flag;
}

int brep_surface_create_ruled(ON_Brep *brep, int curve_id0, int curve_id1)
{
    ON_NurbsCurve *curve0 = brep_get_nurbs_curve(brep, curve_id0);
    ON_NurbsCurve *curve1 = brep_get_nurbs_curve(brep, curve_id1);
    if (!curve0 || !curve1) {
	return -1;
    }
    ON_NurbsSurface *surface = ON_NurbsSurface::New();
    if (!surface->CreateRuledSurface(*curve0, *curve1)) {
	delete surface;
	return -1;
    }
    return brep->AddSurface(surface);
}

int brep_surface_tensor_product(ON_Brep *brep, int curve_id0, int curve_id1)
{
    ON_NurbsCurve *curve0 = brep_get_nurbs_curve(brep, curve_id0);
    ON_NurbsCurve *curve1 = brep_get_nurbs_curve(brep, curve_id1);
    if (!curve0 || !curve1) {
	return -1;
    }
    ON_SumSurface *surface = ON_SumSurface::New();
    if(!surface->Create(*curve0, *curve1)) {
	delete surface;
	return -1;
    }
    ON_NurbsSurface *nurbs_surface = surface->NurbsSurface();
    delete surface;
    if(nurbs_surface == NULL) {
	return -1;
    }
    return brep->AddSurface(nurbs_surface);
}

int brep_surface_revolution(ON_Brep *brep, int curve_id0, ON_3dPoint line_start, ON_3dPoint line_end, double angle)
{

    ON_NurbsCurve *curve0 = brep_get_nurbs_curve(brep, curve_id0);
    if(!curve0) {
	return -1;
    }
    ON_RevSurface* rev_surf = ON_RevSurface::New();
    ON_Line line = ON_Line(line_start, line_end);
    if(angle < ON_ZERO_TOLERANCE) {
	angle = -angle;
    line.Reverse();
    }
    if (angle > 2 * ON_PI) {
	angle = 2 * ON_PI;
    }
    rev_surf->m_curve = curve0;
    rev_surf->m_axis = line;
    rev_surf->m_angle = ON_Interval(0, angle);

    // Get the NURBS form of the surface
    ON_NurbsSurface *nurbs_surface = ON_NurbsSurface::New();
    rev_surf->GetNurbForm(*nurbs_surface, 0.0);
    delete rev_surf;
    return brep->AddSurface(nurbs_surface);
}

bool brep_surface_remove(ON_Brep *brep, int surface_id)
{
    if (surface_id < 0 || surface_id >= brep->m_S.Count()) {
	bu_log("surface_id is out of range\n");
	return false;
    }
    brep->m_S.Remove(surface_id);
    return true;
}

int brep_edge_create(ON_Brep *brep, int from, int to, int curve)
{
    ON_BrepVertex& v0 = brep->m_V[from];
    ON_BrepVertex& v1 = brep->m_V[to];
    ON_BrepEdge& edge = brep->NewEdge(v0, v1, curve);
    edge.m_tolerance = 0.0;
    return brep->m_E.Count() - 1;
}

int brep_face_create(ON_Brep *brep, int surface)
{
    if(surface < 0 || surface >= brep->m_S.Count()) {
	bu_log("surface is out of range\n");
	return -1;
    }
    brep->NewFace(surface);
    return brep->m_F.Count() - 1;
}

bool brep_face_reverse(ON_Brep *brep, int face)
{
    if(face < 0 || face >= brep->m_F.Count()) {
	bu_log("face is out of range\n");
	return false;
    }
    ON_BrepFace& f = brep->m_F[face];
    f.m_bRev = !f.m_bRev;
    return true;
}

ON_Curve* getEdgeCurve(const ON_Surface& s, int side)
{
    ON_2dPoint from, to;
    double u0, u1, v0, v1;
    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    switch (side) {
	case 0:
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    break;
	case 1:
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    break;
	case 2:
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    break;
	case 3:
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    break;
	default:
	    return NULL;
    }
    ON_Curve* c2d = new ON_LineCurve(from, to);
    c2d->SetDomain(0.0, 1.0);
    return c2d;
}

int brep_loop_create(ON_Brep *brep, int face_id)
{
    if(face_id < 0 || face_id >= brep->m_F.Count()) {
	bu_log("face_id is out of range\n");
	return -1;
    }
    ON_BrepFace& face = brep->m_F[face_id];
    ON_BrepLoop& loop = brep->NewLoop(ON_BrepLoop::outer, face);
    return loop.m_loop_index;
}

int brep_trim_create(ON_Brep *brep, int loop_id, int edge_id, int orientation, int para_curve_id)
{
    if(loop_id < 0 || loop_id >= brep->m_L.Count()) {
	bu_log("loop_id is out of range\n");
	return -1;
    }
    if(edge_id < 0 || edge_id >= brep->m_E.Count()) {
	bu_log("edge_id is out of range\n");
	return -1;
    }
    if(para_curve_id < 0 || para_curve_id >= brep->m_C2.Count()) {
	bu_log("para_curve_id is out of range\n");
	return -1;
    }
    ON_BrepLoop& loop = brep->m_L[loop_id];
    ON_BrepTrim& trim = brep->NewTrim(brep->m_E[edge_id], orientation, loop, para_curve_id);
    trim.m_type = ON_BrepTrim::mated;
    const ON_Curve* c2 = brep->m_C2[trim.m_c2i];
    const ON_Surface* s = loop.SurfaceOf();
    ON_Interval PD = trim.ProxyCurveDomain();
    trim.m_iso = s->IsIsoparametric(*c2, &PD);
    trim.m_tolerance[0] = 0.0;
    trim.m_tolerance[1] = 0.0;
    return trim.m_trim_index;
}

std::vector<ON_3dVector> calculateTangentVectors(const std::vector<ON_3dPoint> &points)
{
    int n = points.size() - 1;

    /// calculate qk
    ON_3dVectorArray qk(n + 3);
    ON_3dVector q_;
    for (size_t i = 1; i < points.size(); ++i) {
	qk[(int)i] = ON_3dVector(points[i] - points[i - 1]);
    }
    qk[0] = 2 * qk[1] - qk[2];
    q_ = 2 * qk[0] - qk[1];
    qk[n + 1] = 2 * qk[n] - qk[n - 1];
    qk[n + 2] = 2 * qk[n + 1] - qk[n];

    /// tk is a middle variable for calculating ak. tk=|qk-1 x qk|
    double *tk = (double *)bu_calloc(n + 3, sizeof(double), "tk");
    tk[0] = ON_CrossProduct(q_, qk[0]).Length();
    for (int i = 1; i < n + 3; ++i) {
	tk[i] = ON_CrossProduct(qk[i - 1], qk[i]).Length();
    }

    /// calculate ak
    double *ak = (double *)bu_calloc(n + 1, sizeof(double), "ak");
    for (int i = 0; i < n + 1; ++i) {
	ak[i] = tk[i] / (tk[i] + tk[i + 2]);
    }
    bu_free(tk, "tk");

    /// calculate vk
    ON_3dVectorArray vk(n + 1);
    for (int i = 0; i < n + 1; ++i) {
	vk[i] = (1 - ak[i]) * qk[i] + ak[i] * qk[i];
    }
    bu_free(ak, "ak");

    std::vector<ON_3dVector> tangentVectors;

    for (int i = 0; i < n + 1; ++i) {
	tangentVectors.push_back(vk[i].UnitVector());
    }
    return tangentVectors;
}

double getPosRoot(const double a, const double b,const double c)
{
    double delta = b * b - 4 * a * c;
    if (delta < 0) {
	return -1;
    }
    else if (NEAR_ZERO(delta, 1e-6)) {
	return -b / (2 * a);
    }
    else {
	double x1 = (-b + sqrt(delta)) / (2 * a);
	if (x1 > 0) {
	    return x1;
	}
	else {
	    return -1;
	}
    }

}

void calcuBsplineCVsKnots(std::vector<ON_3dPoint> &cvs, std::vector<double> &knots, const std::vector<ON_3dPoint> &endPoints, const std::vector<ON_3dVector> &tangentVectors)
{
    std::vector<double> uk;
    uk.push_back(0);
    for (size_t i = 0; i < endPoints.size() - 1; i++) {
	cvs.push_back(endPoints[i]);
	double a = 16 - (tangentVectors[i] + tangentVectors[i + 1]).LengthSquared();
	double b = 12 * (tangentVectors[i] + tangentVectors[i + 1])*(endPoints[i + 1] - endPoints[i]);
	double c = -36 * (endPoints[i + 1] - endPoints[i]).LengthSquared();
	double d = getPosRoot(a, b, c);
	ON_3dPoint p1 = endPoints[i] + tangentVectors[i] * d / 3.0;
	ON_3dPoint p2 = endPoints[i + 1] - tangentVectors[i + 1] * d / 3.0;
	cvs.push_back(p1);
	cvs.push_back(p2);
	uk.push_back(uk[i] + 3 * (p1.DistanceTo(p2)));
    }
    cvs.push_back(endPoints[endPoints.size() - 1]);


    knots.clear();
    for (size_t i = 0; i < 3; i++)
	knots.push_back(0);
    for (size_t i = 1; i < uk.size() - 1; i++) {
	knots.push_back(uk[i] / uk[uk.size() - 1]);
	knots.push_back(uk[i] / uk[uk.size() - 1]);
	knots.push_back(uk[i] / uk[uk.size() - 1]);
    }
    for (size_t i = 0; i < 3; i++)
	knots.push_back(1);
}

// caculate parameters of the surface
// input: n, m, points
// output: uk, ul
int SurfMeshParams(int n, int m, std::vector<ON_3dPoint> points, std::vector<double> &uk, std::vector<double> &ul)
{
    std::vector<double> cds;
    n > m ? cds.resize(n + 1) : cds.resize(m + 1);
    int num = m + 1; // number of nondegenerate rows
    uk.resize(n + 1);
    uk[0] = 0.0f;
    uk[n] = 1.0f;
    for (int i = 1; i < n; i++)
	uk[i] = 0;
    for (int i = 0; i <= m; i++) {
	double sum = 0;
	for (int j = 1; j <= n; j++) {
	    cds[j] = points[i * (n + 1) + j].DistanceTo(points[i * (n + 1) + j - 1]);
	    sum += cds[j];
	}
	if (sum <= 0)
	    num--;
	else
	{
	    double d = 0.0f;
	    for (int j = 1; j < n; j++)
	    {
		d += cds[j];
		uk[j] += d / sum;
	    }
	}
    }
    if (num == 0)
	return -1;
    for (int i = 1; i < n; i++)
	uk[i] /= num;

    num = n + 1;
    ul.resize(m + 1);
    ul[0] = 0.0f;
    ul[m] = 1.0f;
    for (int i = 1; i < m; i++)
	ul[i] = 0;
    for (int i = 0; i <= n; i++) {
	double sum = 0;
	for (int j = 1; j <= m; j++) {
	    cds[j] = points[j * (n + 1) + i].DistanceTo(points[(j - 1) * (n + 1) + i]);
	    sum += cds[j];
	}
	if (sum <= 0)
	    num--;
	else
	{
	    double d = 0.0f;
	    for (int j = 1; j < m; j++)
	    {
		d += cds[j];
		ul[j] += d / sum;
	    }
	}
    }
    if (num == 0)
	return -1;
    for (int i = 1; i < m; i++)
	ul[i] /= num;
    return 0;
}

void bsplineBasisFuns(int i, double u, int p, std::vector<double> U,  std::vector<double> &N)
{
    double *left = (double *)bu_calloc(p + 1, sizeof(double), "left");
    double *right = (double *)bu_calloc(p + 1, sizeof(double), "right");
    double saved, temp;
    int j, r;
    N[0] = 1.0;

    for (j = 1; j <= p; j++) {
	left[j] = u - U[i + 1 - j];
	right[j] = U[i + j] - u;
	saved = 0.0;

	for (r = 0; r < j; r++) {
	    temp = N[r] / (right[r + 1] + left[j - r]);
	    N[r] = saved + right[r + 1] * temp;
	    saved = left[j - r] * temp;
	}
	N[j] = saved;
    }
    bu_free(left, "left");
    bu_free(right, "right");
}

/**
 * solve tridiagonal equation for cubic spline interpolation
 */
int solveTridiagonalint(int n, std::vector<ON_3dPoint> Q, std::vector<double> U, std::vector<ON_3dPoint>& P)
{
    std::vector<double> abc(4);
    std::vector<ON_3dPoint> R(n + 1);
    std::vector<double> dd(n + 1);
    double den;
    int i;

    for (i = 3; i < n; i++) {
	R[i] = Q[i - 1];
    }

    bsplineBasisFuns(4, U[4], 3, U, abc);
    den = abc[1];

    /* P[2] */
    P[2] = (Q[1] - abc[0] * P[1]) / den;

    for (i = 3; i < n; i++) {
	dd[i] = abc[2] / den;

	bsplineBasisFuns(i + 2, U[i + 2], 3, U, abc);
	den = abc[1] - abc[0] * dd[i];
	P[i] = (R[i] - abc[0] * P[i - 1]) / den;
    }

    dd[n] = abc[2] / den;

    bsplineBasisFuns(n + 2, U[n + 2], 3, U, abc);
    den = abc[1] - abc[0] * dd[n];

    P[n] = (Q[n - 1] - abc[2] * P[n + 1] - abc[0] * P[n - 1]) / den;

    for (i = (n - 1); i >= 2; i--) {
	P[i] = P[i] - dd[i + 1] * P[i + 1];
    }

    if (n == 2) {
	P[2] /= 4.0f / 3.0f;
    }
    return 1;
}

void globalCurveInterp(int n, std::vector<double> &knots, const std::vector<ON_3dPoint> &Q, std::vector<ON_3dPoint> &P)
{
    /// initialize control points of P[0], P[1], P[n+1], P[n+2]
    P.resize(n + 3);
    P[0] = Q[0];
    P[1] = Q[0] + (Q[1] - Q[0]) / 3.0f * knots[4];
    P[n + 2] = Q[n];
    P[n + 1] = Q[n] - (Q[n] - Q[n - 1]) / 3.0f * (1.0f - knots[n + 2]);

    solveTridiagonalint(n, Q, knots, P);
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
