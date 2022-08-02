/*      S H A P E _ R E C O G N I T I O N _ C O N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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
/** @file shape_recognition_cone.cpp
 *
 * Cone shape recognition routines
 *
 */

#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bio.h"  // for NOMINMAX

#include "bu/str.h"
#include "bu/malloc.h"
#include "brep/util.h"
#include "shape_recognition.h"

#define L3_OFFSET 6


int
cone_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cone corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCone(&corig, BREP_CONIC_TOL);
    delete csorig;
    ON_Line lorig(corig.BasePoint(), corig.ApexPoint());

    ON_Cone ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCone(&ccand, BREP_CONIC_TOL);
    delete cscand;
    double d1 = lorig.DistanceTo(ccand.BasePoint());
    double d2 = lorig.DistanceTo(ccand.ApexPoint());

    // Make sure the cone axes are colinear
    if (corig.Axis().IsParallelTo(ccand.Axis(), VUNITIZE_TOL) == 0) return 0;
    if (fabs(d1) > BREP_CONIC_TOL) return 0;
    if (fabs(d2) > BREP_CONIC_TOL) return 0;

    // Make sure the AngleInRadians parameter is the same for both cones
    if (!NEAR_ZERO(corig.AngleInRadians() - ccand.AngleInRadians(), VUNITIZE_TOL)) return 0;

    // Make sure the ApexPoint parameter is the same for both cones
    if (corig.ApexPoint().DistanceTo(ccand.ApexPoint()) > BREP_CONIC_TOL) return 0;

    // TODO - make sure negative/positive status for both cyl faces is the same.

    return 1;
}


/* Return -1 if the cone face is pointing in toward the axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cone(const ON_Brep *brep, int face_index, double cone_tol) {
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Cone cone;
    ON_Surface *cs = surf->Duplicate();
    cs->IsCone(&cone, cone_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;
    ON_3dPoint base_pnt = cone.BasePoint();

    ON_3dVector base_vect = pnt - base_pnt;
    double dotp = ON_DotProduct(base_vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
cone_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cone_planes)
{
    return cyl_implicit_plane(brep, lc, le, cone_planes);
}


int
cone_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *cone_planes, int implicit_plane_ind, int ndc, int *nde, int shoal_nonplanar_face, int UNUSED(nonlinear_edge))
{

    const ON_Brep *brep = data->i->brep;
    std::set<int> nondegen_edges;
    std::set<int>::iterator c_it;
    array_to_set(&nondegen_edges, nde, ndc);
    std::set<int> notrim_planes;

    // Make a starting cone from one of the cylindrical surfaces and construct the axis line
    ON_Cone cone;
    ON_Surface *cs = brep->m_L[data->shoal_loops[0]].Face()->SurfaceOf()->Duplicate();
    cs->IsCone(&cone, BREP_CONIC_TOL);
    delete cs;
    ON_Line l(cone.BasePoint(), cone.ApexPoint());

    // Need at least one plane to make an implicit shape
    if ((*cone_planes).Count() == 0) {
	return -1;
    }

    int need_arbn = 1;
    if ((*cone_planes).Count() <= 2) {
	int perpendicular = 0;
	for (int i = 0; i < 2; i++) {
	    ON_Plane p = (*cone_planes)[i];
	    if (p.Normal().IsParallelTo(cone.Axis(), VUNITIZE_TOL) != 0) perpendicular++;
	}
	if (perpendicular == (*cone_planes).Count()) need_arbn = 0;
    }

    // We'll be using a truncated cone to represent this, and we don't want to make it any
    // larger than we have to.  Find all the limit points, starting with the apex point and
    // adding in the planes and edges.
    ON_3dPointArray axis_pts_init;

    // It may be trimmed away by planes, but not in the simplest case.  Add in apex point.
    axis_pts_init.Append(cone.ApexPoint());
    double apex_t;
    l.ClosestPointTo(cone.ApexPoint(), &apex_t);

    // Add in all the nondegenerate edge vertices
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
        const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
        axis_pts_init.Append(edge->Vertex(0)->Point());
        axis_pts_init.Append(edge->Vertex(1)->Point());
    }

    // Use the intersection, cone angle, and the projection of the cone axis onto the plane
    // to calculate min and max points on the cone from this plane.
    for (int i = 0; i < (*cone_planes).Count(); i++) {
        // Don't intersect the implicit plane, since it doesn't play a role in defining the main cone
	if (i == implicit_plane_ind) {
	    notrim_planes.insert(i);
	    continue;
	}
        if (!(*cone_planes)[i].Normal().IsPerpendicularTo(cone.Axis(), VUNITIZE_TOL)) {
            ON_3dPoint ipoint = ON_LinePlaneIntersect(l, (*cone_planes)[i]);
            if ((*cone_planes)[i].Normal().IsParallelTo(cone.Axis(), VUNITIZE_TOL)) {
                axis_pts_init.Append(ipoint);
            } else {
		double C, a, b;
		ON_3dVector av = cone.ApexPoint() - ipoint;
		double side = av.Length();
		av.Unitize();
                double dpc = ON_DotProduct(av, (*cone_planes)[i].Normal());
                if (dpc < 0) {
		    dpc = ON_DotProduct(av, -1*(*cone_planes)[i].Normal());
		}
		C = M_PI - (M_PI/2 - acos(dpc)) - fabs(cone.AngleInRadians());
		a = fabs(side * sin(cone.AngleInRadians()) / sin(C));
		C = M_PI - (M_PI/2 + acos(dpc)) - fabs(cone.AngleInRadians());
		b = fabs(side * sin(fabs(cone.AngleInRadians())) / sin(C));

		ON_3dVector cone_unit_axis = cone.Axis();
		cone_unit_axis.Unitize();
                ON_3dVector pvect = cone.Axis() - ((*cone_planes)[i].Normal() * ON_DotProduct(cone_unit_axis, (*cone_planes)[i].Normal()));
                pvect.Unitize();
                if (!NEAR_ZERO(dpc, VUNITIZE_TOL)) {
		    double tp1, tp2;
                    ON_3dPoint p1 = ipoint + pvect * a;
                    ON_3dPoint p2 = ipoint + -1*pvect * b;
		    l.ClosestPointTo(p1, &tp1);
		    l.ClosestPointTo(p2, &tp2);
		    if (tp1 < apex_t) { axis_pts_init.Append(p1); } else { notrim_planes.insert(i); }
		    if (tp2 < apex_t) { axis_pts_init.Append(p2); } else { notrim_planes.insert(i); }
                }
            }
        } else {
	    notrim_planes.insert(i);
	}
    }

    // Trim out points that are above the bounding planes
    ON_3dPointArray axis_pts_2nd;
    for (int i = 0; i < axis_pts_init.Count(); i++) {
        int trimmed = 0;
        for (int j = 0; j < (*cone_planes).Count(); j++) {
            // Don't trim with the implicit plane - the implicit plane is defined "after" the
            // primary shapes are formed, so it doesn't get a vote in removing capping points
            if (notrim_planes.find(j) == notrim_planes.end()) {
                ON_3dVector v = axis_pts_init[i] - (*cone_planes)[j].origin;
                double dotp = ON_DotProduct(v, (*cone_planes)[j].Normal());
                if (dotp > 0 && !NEAR_ZERO(dotp, VUNITIZE_TOL)) {
                    trimmed = 1;
                    break;
                }
            }
        }
        if (!trimmed) axis_pts_2nd.Append(axis_pts_init[i]);
    }

    // For everything that's left, project it back onto the central axis line and see
    // if it's further up or down the line than anything previously checked.  We want
    // the min and the max points on the centeral axis.
    double tmin = std::numeric_limits<double>::max();
    double tmax = std::numeric_limits<double>::min();
    for (int i = 0; i < axis_pts_2nd.Count(); i++) {
        double t;
        if (l.ClosestPointTo(axis_pts_2nd[i], &t)) {
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
    }

    // Populate the CSG implicit primitive data
    data->params->csg_type = CONE;
    // Flag the cone according to the negative or positive status of the
    // cone surface.
    data->params->negative = negative_cone(brep, shoal_nonplanar_face, BREP_CONIC_TOL);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';

    fastf_t hdelta = 0;
    if (l.PointAt(tmin).DistanceTo(cone.ApexPoint()) > BREP_CONIC_TOL) {
	hdelta = cone.BasePoint().DistanceTo(l.PointAt(tmin));
	hdelta = (cone.height < 0) ? -1*hdelta : hdelta;
	data->params->radius = cone.CircleAt(cone.height - hdelta).Radius();
	BN_VMOVE(data->params->origin, l.PointAt(tmin));
    }

    if (l.PointAt(tmax).DistanceTo(cone.ApexPoint()) > BREP_CONIC_TOL) {
	fastf_t hdelta2 = cone.BasePoint().DistanceTo(l.PointAt(tmax));
	hdelta2 = (cone.height < 0) ? -1*hdelta2 : hdelta2;
	data->params->r2 = cone.CircleAt(cone.height - hdelta2).Radius();
    } else {
	data->params->r2 = 0.000001;
    }

    ON_3dVector hvect(l.PointAt(tmax) - l.PointAt(tmin));
    BN_VMOVE(data->params->hv, hvect);
    data->params->height = hvect.Length();

    // constructed oriented bounding box planes
    if (need_arbn) {
	ON_3dVector v1 = cone.CircleAt(cone.height - hdelta).Plane().xaxis;
	ON_3dVector v2 = cone.CircleAt(cone.height - hdelta).Plane().yaxis;
	v1.Unitize();
	v2.Unitize();
	v1 = v1 * cone.CircleAt(cone.height - hdelta).Radius();
	v2 = v2 * cone.CircleAt(cone.height - hdelta).Radius();
	ON_3dPoint arbmid = (l.PointAt(tmax) + l.PointAt(tmin)) * 0.5;
	ON_3dVector cone_axis_unit = l.PointAt(tmax) - l.PointAt(tmin);
	double axis_len = cone_axis_unit.Length();
	cone_axis_unit.Unitize();
	// Bump the top and bottom planes out slightly to avoid problems when the capping plane normals
	// are almost but not quite parallel to the cone axis
	ON_3dPoint arbmax = l.PointAt(tmax) + 0.01 * axis_len * cone_axis_unit;
	ON_3dPoint arbmin = l.PointAt(tmin) - 0.01 * axis_len * cone_axis_unit;

	(*cone_planes).Append(ON_Plane(arbmin, -1 * cone_axis_unit));
	(*cone_planes).Append(ON_Plane(arbmax, cone_axis_unit));
	(*cone_planes).Append(ON_Plane(arbmid + v1, cone.CircleAt(cone.height - hdelta).Plane().xaxis));
	(*cone_planes).Append(ON_Plane(arbmid - v1, -1 *cone.CircleAt(cone.height - hdelta).Plane().xaxis));
	(*cone_planes).Append(ON_Plane(arbmid + v2, cone.CircleAt(cone.height - hdelta).Plane().yaxis));
	(*cone_planes).Append(ON_Plane(arbmid - v2, -1 * cone.CircleAt(cone.height - hdelta).Plane().yaxis));
    }


    return need_arbn;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
