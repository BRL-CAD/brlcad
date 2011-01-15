/*                    E L L _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file ell_brep.cpp
 *
 * Convert a Generalized Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


/**
 * R T _ E T O _ B R E P
 */
extern "C" void
rt_eto_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_eto_internal *eip;

    *b = NULL;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_eto_internal *)ip->idb_ptr;
    RT_ETO_CK_MAGIC(eip);

    point_t p_origin;
    vect_t v1, v1a, x_dir, y_dir;
    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    double ell_axis_len_1, ell_axis_len_2;

    //  First, find a plane in 3 space with x and y axes
    //  along an axis of the ellipse to be rotated, and its
    //  coordinate origin at the center of the ellipse.
    //
    //  To identify a point on the eto suitable for use (there
    //  are of course infinitely many such points described by
    //  a circle at radius eto_r from the eto vertex) obtain
    //  a vector at a right angle to the eto normal, unitize it
    //  and scale it.

    VCROSS(v1, eip->eto_C, eip->eto_N);
    VCROSS(v1a, v1, eip->eto_N);
    VSET(v1, -v1a[0], -v1a[1], -v1a[2]);
    VUNITIZE( v1 );
    VSCALE(v1, v1, eip->eto_r);
    VMOVE(x_dir, eip->eto_C);
    bn_vec_ortho( y_dir, x_dir);
    VSET(p_origin, v1[0], v1[1], v1[2]);
    plane_origin = ON_3dPoint(p_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);

    const ON_Plane* ell_plane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir);


    //  Once the plane has been created, create the ellipse
    //  within the plane.
    ell_axis_len_1 = MAGNITUDE(eip->eto_C);
    ell_axis_len_2 = eip->eto_rd;
    ON_Ellipse* ellipse = new ON_Ellipse(*ell_plane, ell_axis_len_1, ell_axis_len_2);


    //  Generate an ON_Curve from the ellipse and revolve it
    //  around eto_N

    ON_NurbsCurve ellcurve;
    ellipse->GetNurbForm(ellcurve);
    ON_3dPoint eto_vertex_pt = ON_3dPoint(eip->eto_V);
    ON_3dPoint eto_normal_pt = ON_3dPoint(eip->eto_N);
    ON_Line revaxis = ON_Line(eto_vertex_pt, eto_normal_pt);
    ON_RevSurface* eto_surf = ON_RevSurface::New();
    eto_surf->m_curve = &ellcurve;
    eto_surf->m_axis = revaxis;

    /* Create brep with one face*/
    *b = ON_Brep::New();
    ON_BrepFace *newface = (*b)->NewFace(*eto_surf);
    (*b)->FlipFace(*newface);
//    (*b)->Standardize();
 //   (*b)->Compact();
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
