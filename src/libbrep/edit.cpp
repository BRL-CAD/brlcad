/*                        E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2023 United States Government as represented by
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
#include <vector>

void *brep_create()
{
    ON_Brep *brep = new ON_Brep();
    return (void *)brep;
}

int brep_curve_make(ON_Brep *brep, const ON_3dPoint &position)
{
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, 3, 4);
    curve->SetCV(0, ON_3dPoint(-0.1, -1.5, 0));
    curve->SetCV(1, ON_3dPoint(0.1, -0.5, 0));
    curve->SetCV(2, ON_3dPoint(0.1, 0.5, 0));
    curve->SetCV(3, ON_3dPoint(-0.1, 1.5, 0));
    curve->MakeClampedUniformKnotVector();
    if (position) {
	curve->Translate(position);
    }
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
    if (surface_id < 0 || surface_id >= brep->m_C3.Count()) {
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8