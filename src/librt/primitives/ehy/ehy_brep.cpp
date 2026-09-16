/*                    E H Y _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file ehy_brep.cpp
 *
 * Convert an Elliptical Hyperboloid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "primitives/brep/primitive_brep.h"


extern "C" void
rt_ehy_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_ehy_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(eip);

    // Check the parameters
    if (!NEAR_ZERO(VDOT(eip->ehy_Au, eip->ehy_H), RT_DOT_TOL)) {
	bu_log("rt_ehy_brep: Au and H are not perpendicular!\n");
	return;
    }

    if (!NEAR_EQUAL(MAGNITUDE(eip->ehy_Au), 1.0, RT_LEN_TOL)) {
	bu_log("rt_ehy_brep: Au not a unit vector!\n");
	return;
    }

    if (MAGNITUDE(eip->ehy_H) < RT_LEN_TOL
	|| eip->ehy_c < RT_LEN_TOL
	|| eip->ehy_r1 < RT_LEN_TOL
	|| eip->ehy_r2 < RT_LEN_TOL) {
	bu_log("rt_ehy_brep: not all dimensions positive!\n");
	return;
    }

    if (eip->ehy_r2 > eip->ehy_r1) {
	bu_log("rt_ehy_brep: semi-minor axis cannot be longer than semi-major axis!\n");
	return;
    }

    vect_t y_dir;
    VCROSS(y_dir, eip->ehy_Au, eip->ehy_H);
    VUNITIZE(y_dir);

    //  Now, the hard part.  Need an elliptical hyperbolic NURBS surface
    //  First step is to create a nurbs curve.

    double intercept_calc = (eip->ehy_c)*(eip->ehy_c)/(MAGNITUDE(eip->ehy_H) + eip->ehy_c);
    double intercept_dist = MAGNITUDE(eip->ehy_H) + eip->ehy_c - intercept_calc;
    double intercept_length = intercept_dist - MAGNITUDE(eip->ehy_H);
    double MX = MAGNITUDE(eip->ehy_H);
    double MP = MX + intercept_length;
    double w = (MX/MP)/(1-MX/MP);

    point_t ep1, ep2, ep3;
    VSET(ep1, -eip->ehy_r1, 0, 0);
    VSET(ep2, 0, 0, w*intercept_dist);
    VSET(ep3, eip->ehy_r1, 0, 0);
    ON_3dPoint onp1 = ON_3dPoint(ep1);
    ON_3dPoint onp2 = ON_3dPoint(ep2);
    ON_3dPoint onp3 = ON_3dPoint(ep3);

    ON_3dPointArray cpts(3);
    cpts.Append(onp1);
    cpts.Append(onp2);
    cpts.Append(onp3);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve* tnurbscurve = ON_NurbsCurve::New();
    bcurve->GetNurbForm(*tnurbscurve);
    ON_NurbsCurve* hypbnurbscurve = ON_NurbsCurve::New();
    const ON_Interval subinterval = ON_Interval(0, 0.5);
    tnurbscurve->GetNurbForm(*hypbnurbscurve, 0.0, &subinterval);

    // Next, rotate that curve around the height vector.

    point_t revpoint1, revpoint2;
    VSET(revpoint1, 0, 0, 0);
    VSET(revpoint2, 0, 0, MX);
    ON_3dPoint rpnt1 = ON_3dPoint(revpoint1);
    ON_3dPoint rpnt2 = ON_3dPoint(revpoint2);

    ON_Line revaxis = ON_Line(rpnt1, rpnt2);
    ON_RevSurface* hyp_surf = ON_RevSurface::New();
    hyp_surf->m_curve = hypbnurbscurve;
    hyp_surf->m_axis = revaxis;
    hyp_surf->m_angle = ON_Interval(0, 2*ON_PI);
    hyp_surf->m_t = hyp_surf->m_angle;
    hyp_surf->m_bTransposed = false;
    hyp_surf->BoundingBox();

    ON_NurbsSurface *ehycurvedsurf = ON_NurbsSurface::New();
    if (hyp_surf->GetNurbForm(*ehycurvedsurf, 0.0) <= 0) {
	delete ehycurvedsurf;
	delete hyp_surf;
	delete tnurbscurve;
	delete bcurve;
	bu_log("rt_ehy_brep: unable to construct exact NURBS side surface!\n");
	return;
    }
    delete hyp_surf;
    delete tnurbscurve;
    delete bcurve;

    /* Map the canonical circular NURBS into the EHY's world-space elliptical
     * frame before creating trims.  A nonuniform transform of an already
     * trimmed plane would change its parameterization without changing its
     * 2D trim curves. */
    vect_t Hu;
    VSCALE(Hu, eip->ehy_H, 1/MAGNITUDE(eip->ehy_H));
    const double yscale = eip->ehy_r2 / eip->ehy_r1;
    ON_Xform transform(1.0);
    transform[0][0] = eip->ehy_Au[0];
    transform[0][1] = yscale * y_dir[0];
    transform[0][2] = Hu[0];
    transform[0][3] = eip->ehy_V[0];
    transform[1][0] = eip->ehy_Au[1];
    transform[1][1] = yscale * y_dir[1];
    transform[1][2] = Hu[1];
    transform[1][3] = eip->ehy_V[1];
    transform[2][0] = eip->ehy_Au[2];
    transform[2][1] = yscale * y_dir[2];
    transform[2][2] = Hu[2];
    transform[2][3] = eip->ehy_V[2];


    if (!ehycurvedsurf->Transform(transform)) {
	delete ehycurvedsurf;
	bu_log("rt_ehy_brep: unable to transform surface into the EHY frame!\n");
	return;
    }

    ON_Brep *ehy_brep = new ON_Brep();
    const int side_surface_index = ehy_brep->AddSurface(ehycurvedsurf);
    ON_BrepFace &side_face = ehy_brep->NewFace(side_surface_index);
    ehy_brep->FlipFace(side_face);
    if (!ehy_brep->NewOuterLoop(side_face.m_face_index)) {
	delete ehy_brep;
	bu_log("rt_ehy_brep: unable to construct side boundary loop!\n");
	return;
    }

    /* The full revolution has one naked closed edge at its bottom and a
     * paired seam running to the apex.  Duplicate that exact bottom edge to
     * trim the cap, then merge the duplicate topology so the two faces share
     * one edge rather than merely occupying the same location. */
    int side_edge_index = -1;
    for (int i = 0; i < ehy_brep->m_E.Count(); ++i) {
	if (ehy_brep->m_E[i].m_ti.Count() == 1 && ehy_brep->m_E[i].IsClosed()) {
	    if (side_edge_index >= 0) {
		delete ehy_brep;
		bu_log("rt_ehy_brep: side has multiple candidate bottom edges!\n");
		return;
	    }
	    side_edge_index = i;
	}
    }
    if (side_edge_index < 0) {
	delete ehy_brep;
	bu_log("rt_ehy_brep: side has no closed bottom boundary edge!\n");
	return;
    }

    const ON_Plane bottom_plane(ON_3dPoint(eip->ehy_V),
	ON_3dVector(eip->ehy_Au), ON_3dVector(y_dir));
    if (!rt_brep_mate_planar_cap(*ehy_brep, side_edge_index, bottom_plane,
	false, tol, NULL)) {
	delete ehy_brep;
	bu_log("rt_ehy_brep: unable to mate bottom cap to side surface!\n");
	return;
    }

    ehy_brep->Compact();
    ehy_brep->SetTolerancesBoxesAndFlags(false);
    /* openNURBS can retain an unset tolerance on the shared closed ellipse
     * after replacing the analytic side surface.  The edge and both trims
     * are still exact; bound only that sentinel with the caller's model
     * tolerance so structural validation can evaluate the topology. */
    const double model_tolerance = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    for (int i = 0; i < ehy_brep->m_E.Count(); ++i)
	if (ehy_brep->m_E[i].m_tolerance < 0.0)
	    ehy_brep->m_E[i].m_tolerance = model_tolerance;
    ON_wString messages;
    ON_TextLog log(messages);
    const bool valid = ehy_brep->IsValid(&log);
    const bool solid = ehy_brep->IsSolid();
    if (!valid || !solid) {
	ON_String text(messages);
	delete ehy_brep;
	bu_log("rt_ehy_brep: generated BRep is not a valid manifold solid "
	    "(valid=%d, solid=%d):\n%s", valid, solid, text.Array());
	return;
    }

    **b = *ehy_brep;
    delete ehy_brep;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
