/*                    E P A _ B R E P . C P P
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
/** @file epa_brep.cpp
 *
 * Convert an Elliptical Paraboloid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "primitives/brep/primitive_brep.h"

extern "C" void
rt_epa_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_epa_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(eip);

    point_t p1_origin;
    ON_3dPoint plane1_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    //  First, find plane in 3 space corresponding to the bottom face of the EPA.

    vect_t x_dir, y_dir;

    VMOVE(x_dir, eip->epa_Au);
    VCROSS(y_dir, eip->epa_Au, eip->epa_H);
    VUNITIZE(y_dir);

    VMOVE(p1_origin, eip->epa_V);
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane epa_bottom_plane = ON_Plane(plane1_origin, plane_x_dir, plane_y_dir);

    //  Now, the hard part.  Need an elliptical parabolic NURBS surface

    ON_NurbsSurface* epacurvedsurf = ON_NurbsSurface::New(3, true, 3, 3, 9, 3);
    epacurvedsurf->SetKnot(0, 0, 0);
    epacurvedsurf->SetKnot(0, 1, 0);
    epacurvedsurf->SetKnot(0, 2, 1.571);
    epacurvedsurf->SetKnot(0, 3, 1.571);
    epacurvedsurf->SetKnot(0, 4, 3.142);
    epacurvedsurf->SetKnot(0, 5, 3.142);
    epacurvedsurf->SetKnot(0, 6, 4.713);
    epacurvedsurf->SetKnot(0, 7, 4.713);
    epacurvedsurf->SetKnot(0, 8, 6.284);
    epacurvedsurf->SetKnot(0, 9, 6.284);
    epacurvedsurf->SetKnot(1, 0, 0);
    epacurvedsurf->SetKnot(1, 1, 0);
    epacurvedsurf->SetKnot(1, 2, eip->epa_r1*2);
    epacurvedsurf->SetKnot(1, 3, eip->epa_r1*2);

    double h = MAGNITUDE(eip->epa_H);
    double r1 = eip->epa_r1;
    double r2 = eip->epa_r2;

    ON_4dPoint pt01 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(0, 0, pt01);
    ON_4dPoint pt02 = ON_4dPoint(0, r2/2, h, 1);
    epacurvedsurf->SetCV(0, 1, pt02);
    ON_4dPoint pt03 = ON_4dPoint(0, r2, 0, 1);
    epacurvedsurf->SetCV(0, 2, pt03);

    ON_4dPoint pt04 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 0, pt04);
    ON_4dPoint pt05 = ON_4dPoint(r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 1, pt05);
    ON_4dPoint pt06 = ON_4dPoint(r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 2, pt06);

    ON_4dPoint pt07 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(2, 0, pt07);
    ON_4dPoint pt08 = ON_4dPoint(r1/2, 0, h, 1);
    epacurvedsurf->SetCV(2, 1, pt08);
    ON_4dPoint pt09 = ON_4dPoint(r1, 0, 0, 1);
    epacurvedsurf->SetCV(2, 2, pt09);

    ON_4dPoint pt10 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 0, pt10);
    ON_4dPoint pt11 = ON_4dPoint(r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 1, pt11);
    ON_4dPoint pt12 = ON_4dPoint(r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 2, pt12);

    ON_4dPoint pt13 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(4, 0, pt13);
    ON_4dPoint pt14 = ON_4dPoint(0, -r2/2, h, 1);
    epacurvedsurf->SetCV(4, 1, pt14);
    ON_4dPoint pt15 = ON_4dPoint(0, -r2, 0, 1);
    epacurvedsurf->SetCV(4, 2, pt15);

    ON_4dPoint pt16 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 0, pt16);
    ON_4dPoint pt17 = ON_4dPoint(-r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 1, pt17);
    ON_4dPoint pt18 = ON_4dPoint(-r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 2, pt18);

    ON_4dPoint pt19 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(6, 0, pt19);
    ON_4dPoint pt20 = ON_4dPoint(-r1/2, 0, h, 1);
    epacurvedsurf->SetCV(6, 1, pt20);
    ON_4dPoint pt21 = ON_4dPoint(-r1, 0, 0, 1);
    epacurvedsurf->SetCV(6, 2, pt21);

    ON_4dPoint pt22 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 0, pt22);
    ON_4dPoint pt23 = ON_4dPoint(-r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 1, pt23);
    ON_4dPoint pt24 = ON_4dPoint(-r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 2, pt24);

    ON_4dPoint pt25 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(8, 0, pt25);
    ON_4dPoint pt26 = ON_4dPoint(0, r2/2, h, 1);
    epacurvedsurf->SetCV(8, 1, pt26);
    ON_4dPoint pt27 = ON_4dPoint(0, r2, 0, 1);
    epacurvedsurf->SetCV(8, 2, pt27);

    // caculate rigid transformation between local coordinate and world coordinate
    vect_t origin_Y = {0, 1, 0};
    vect_t origin_Z = {0, 0, 1};
    vect_t origin_X;
    VCROSS(origin_X, origin_Y, origin_Z);
    vect_t end_X, end_Y, end_Z;
    VMOVE(end_Y, eip->epa_Au);
    VMOVE(end_Z, eip->epa_H);
    VUNITIZE(end_Z);
    VUNITIZE(end_Y);
    VCROSS(end_X, end_Y, end_Z);

    // note: there is a 90 degree rotation in the beginning, so swap +x and +y
    mat_t origin_mat = {
	0, 1, 0, 0,
	1, 0, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
    };

    // reference: https://stackoverflow.com/questions/34391968/how-to-find-the-rotation-matrix-between-two-coordinate-systems
    mat_t rotate_mat = {
	VDOT(end_X, origin_X), VDOT(end_Y, origin_X), VDOT(end_Z, origin_X), 0,
	VDOT(end_X, origin_Y), VDOT(end_Y, origin_Y), VDOT(end_Z, origin_Y), 0,
	VDOT(end_X, origin_Z), VDOT(end_Y, origin_Z), VDOT(end_Z, origin_Z), 0,
	0, 0, 0, 1
    };
    mat_t transform_mat = {0};
    // add origin rotation
    bn_mat_mul(transform_mat, rotate_mat, origin_mat);

    // add pan transform
    transform_mat[3] = plane1_origin.x;
    transform_mat[7] = plane1_origin.y;
    transform_mat[11] = plane1_origin.z;

    ON_Xform trans(transform_mat);
    epacurvedsurf->Transform(trans);

    ON_Brep *epa_brep = new ON_Brep();
    const int surface_index = epa_brep->AddSurface(epacurvedsurf);
    ON_BrepFace &side_face = epa_brep->NewFace(surface_index);
    if (!epa_brep->NewOuterLoop(side_face.m_face_index)) {
	delete epa_brep;
	bu_log("rt_epa_brep: unable to construct side boundary loop!\n");
	return;
    }

    int side_edge_index = -1;
    for (int i = 0; i < epa_brep->m_E.Count(); ++i) {
	if (epa_brep->m_E[i].m_ti.Count() == 1 && epa_brep->m_E[i].IsClosed()) {
	    if (side_edge_index >= 0) {
		delete epa_brep;
		bu_log("rt_epa_brep: side has multiple candidate bottom edges!\n");
		return;
	    }
	    side_edge_index = i;
	}
    }
    if (side_edge_index < 0 || !rt_brep_mate_planar_cap(*epa_brep,
	side_edge_index, epa_bottom_plane, true, tol, NULL)) {
	delete epa_brep;
	bu_log("rt_epa_brep: unable to mate bottom cap to side surface!\n");
	return;
    }

    epa_brep->Compact();
    epa_brep->SetTolerancesBoxesAndFlags(false);
    const double model_tolerance = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    for (int i = 0; i < epa_brep->m_E.Count(); ++i)
	if (epa_brep->m_E[i].m_tolerance < 0.0)
	    epa_brep->m_E[i].m_tolerance = model_tolerance;
    if (!epa_brep->IsValid() || !epa_brep->IsSolid()) {
	delete epa_brep;
	bu_log("rt_epa_brep: generated BRep is not a valid manifold solid!\n");
	return;
    }

    **b = *epa_brep;
    delete epa_brep;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
