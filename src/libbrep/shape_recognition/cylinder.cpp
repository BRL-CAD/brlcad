/*  S H A P E _ R E C O G N I T I O N _ C Y L I N D E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file shape_recognition_cylinder.cpp
 *
 * Cylinder shape recognition routines
 *
 */

#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bio.h"  // for NOMINMAX

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "brep/util.h"
#include "shape_recognition.h"

#define L3_OFFSET 6
#define L4_OFFSET 8

#define TIKZ_OUT1 0

int
cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cylinder corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCylinder(&corig, BREP_CYLINDRICAL_TOL);
    delete csorig;
    ON_Line lorig(corig.circle.Center(), corig.circle.Center() + corig.Axis());

    ON_Cylinder ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCylinder(&ccand, BREP_CYLINDRICAL_TOL);
    delete cscand;
    double d1 = lorig.DistanceTo(ccand.circle.Center());
    double d2 = lorig.DistanceTo(ccand.circle.Center() + ccand.Axis());

    // Make sure the cylinder axes are colinear
    if (corig.Axis().IsParallelTo(ccand.Axis(), VUNITIZE_TOL) == 0) return 0;
    if (fabs(d1) > BREP_CYLINDRICAL_TOL) return 0;
    if (fabs(d2) > BREP_CYLINDRICAL_TOL) return 0;

    // Make sure the radii are the same
    if (!NEAR_ZERO(corig.circle.Radius() - ccand.circle.Radius(), VUNITIZE_TOL)) return 0;

    // TODO - make sure negative/positive status for both cyl faces is the same.

    return 1;
}


/* Return -1 if the cylinder face is pointing in toward the cylinder axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cylinder(const ON_Brep *brep, int face_index, double cyl_tol) {
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Cylinder cylinder;
    ON_Surface *cs = surf->Duplicate();
    cs->IsCylinder(&cylinder, cyl_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;
    ON_3dPoint axis_pnt = cylinder.circle.Center();

    ON_3dVector axis_vect = pnt - axis_pnt;
    double dotp = ON_DotProduct(axis_vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
cyl_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cyl_planes)
{
    std::set<int> linear_edges;
    std::set<int>::iterator c_it;
    array_to_set(&linear_edges, le, lc);
    // If we have two non-linear edges remaining, construct a plane from them.
    // If we have a count other than two or zero, return.
    if (linear_edges.size() != 2 && linear_edges.size() != 0)
       	return -2;
    if (linear_edges.size() == 2) {
	std::set<int> verts;
	// If both edges share a pre-existing face that is planar, use the inverse of that
	// plane.  Otherwise, construct a plane from the vertex points.
	//bu_log("found two linear edges\n");
	for (c_it = linear_edges.begin(); c_it != linear_edges.end(); c_it++) {
	    const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	    verts.insert(edge->m_vi[0]);
	    verts.insert(edge->m_vi[1]);
	}

	// For a plane we need at least 3 unique points
	if (verts.size() < 3)
	    return -1;

	ON_3dPointArray points;
	points.SetCapacity(3);
	for (c_it = verts.begin(); c_it != verts.end(); c_it++) {
	    const ON_BrepVertex *v = &(brep->m_V[*c_it]);
	    points.Append(v->Point());
	}

	ON_Plane pf(points[0], points[1], points[2]);
#if TIKZ_OUT1
	pf.Flip();
	ON_3dPoint pforigin = points[0] + points[1] + points[2] + points[3];
	pforigin = pforigin*0.25;

	bu_log("\\coordinate (impo) at (%f, %f, %f);\n", pforigin.x, pforigin.y, pforigin.z);
	bu_log("\\coordinate (impn) at (%f, %f, %f);\n", pforigin.x + -0.6*pf.Normal().x, pforigin.y + -0.6*pf.Normal().y, pforigin.z + -0.6*pf.Normal().z);
	bu_log("\\draw[red, ->] (impo) -- (impn);\n");
	ON_3dVector v1 = pf.Xaxis() * points[0].DistanceTo(points[1]) * 0.8;
	ON_3dVector v2 = pf.Yaxis() * points[1].DistanceTo(points[2]) * 0.6;
	ON_3dPoint plane1 = pforigin + v1 + v2;
	ON_3dPoint plane2 = pforigin + v1 - v2;
	ON_3dPoint plane3 = pforigin - v1 + v2;
	ON_3dPoint plane4 = pforigin - v1 - v2;
	bu_log("\\coordinate (impc1) at (%f, %f, %f);\n", plane1.x, plane1.y, plane1.z);
	bu_log("\\coordinate (impc2) at (%f, %f, %f);\n", plane2.x, plane2.y, plane2.z);
	bu_log("\\coordinate (impc3) at (%f, %f, %f);\n", plane3.x, plane3.y, plane3.z);
	bu_log("\\coordinate (impc4) at (%f, %f, %f);\n", plane4.x, plane4.y, plane4.z);
	bu_log("\\draw[red] (impc1) -- (impc2);\n");
	bu_log("\\draw[red] (impc2) -- (impc4);\n");
	bu_log("\\draw[red] (impc4) -- (impc3);\n");
	bu_log("\\draw[red] (impc3) -- (impc1);\n");
#endif

	// If the fourth point is not coplanar with the other three, we've hit a case
	// that we don't currently handlinear_edges.- hault. (TODO - need test case)
	if (pf.DistanceTo(points[3]) > BREP_PLANAR_TOL) return -2;

	(*cyl_planes).Append(pf);
	return (*cyl_planes).Count() - 1;
    }

    return -1;
}


// returns whether we need an arbn, the params in data, and bounding arb planes appended to cyl_planes
int
cyl_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *cyl_planes, int implicit_plane_ind, int ndc, int *nde, int shoal_nonplanar_face, int nonlinear_edge)
{
    const ON_Brep *brep = data->i->brep;
    std::set<int> nondegen_edges;
    std::set<int>::iterator c_it;
    array_to_set(&nondegen_edges, nde, ndc);

    // Make a starting cylinder from one of the cylindrical surfaces and construct the axis line
    ON_Cylinder cylinder;
    ON_Surface *cs = brep->m_L[data->shoal_loops[0]].Face()->SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder, BREP_CYLINDRICAL_TOL);
    delete cs;
    double height[2];
    height[0] = (NEAR_ZERO(cylinder.height[0], VUNITIZE_TOL)) ? -1000 : 2*cylinder.height[0];
    height[1] = (NEAR_ZERO(cylinder.height[1], VUNITIZE_TOL)) ? 1000 : 2*cylinder.height[1];
    ON_Circle c = cylinder.circle;
    ON_Line l(c.plane.origin + height[0]*c.plane.zaxis, c.plane.origin + height[1]*c.plane.zaxis);


    // If we have only two cylinder planes and both are perpendicular to the axis, we have
    // an rcc and do not need an intersecting arbn.
    int need_arbn = 1;
    if ((*cyl_planes).Count() == 2) {
	ON_Plane p1, p2;
	int perpendicular = 2;
	if ((*cyl_planes)[0].Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) perpendicular--;
	if ((*cyl_planes)[1].Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) perpendicular--;
	if (perpendicular == 2) {
	    //bu_log("perfect cylinder\n");
	    need_arbn = 0;
	}
    }

    // We need a cylinder large enough to bound the positive volume we are describing -
    // find all the limits from the various planes and edges.
    // Build up a list of all unique vertices from the nondegenerate edge vertices
    std::set<int> apinit;
    std::set<int>::iterator apit;
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	apinit.insert(edge->m_vi[0]);
	apinit.insert(edge->m_vi[1]);
    }
    // Add the points to a 3DPoint array
    ON_3dPointArray axis_pts_init;
    for (apit = apinit.begin(); apit != apinit.end(); apit++) {
	const ON_BrepVertex *v = &(brep->m_V[*apit]);
	axis_pts_init.Append(v->Point());
    }

    // Use the intersection and the projection of the cylinder axis onto the plane
    // to calculate min and max points on the cylinder from this plane.
    for (int i = 0; i < (*cyl_planes).Count(); i++) {
	// Note - don't intersect the implicit plane, since it doesn't play a role in defining the main cylinder
	if (!(*cyl_planes)[i].Normal().IsPerpendicularTo(cylinder.Axis(), VUNITIZE_TOL) && i != implicit_plane_ind) {
	    ON_3dPoint ipoint = ON_LinePlaneIntersect(l, (*cyl_planes)[i]);
#if TIKZ_OUT1
	    bu_log("\\coordinate (ip%d) at (%f, %f, %f);\n", i, ipoint.x, ipoint.y, ipoint.z);
	    ON_3dPoint npoint = ipoint + 0.6*(*cyl_planes)[i].Normal();
	    bu_log("\\coordinate (np%d) at (%f, %f, %f);\n", i, npoint.x, npoint.y, npoint.z);
	    bu_log("\\draw[orange, ->] (ip%d) -- (np%d);\n", i, i);
#endif
	    if ((*cyl_planes)[i].Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL)) {
		axis_pts_init.Append(ipoint);
	    } else {
		double dpc = ON_DotProduct(cylinder.Axis(), (*cyl_planes)[i].Normal());
		ON_3dVector pvect = cylinder.Axis() - ((*cyl_planes)[i].Normal() * dpc);
		pvect.Unitize();
		if (!NEAR_ZERO(dpc, VUNITIZE_TOL)) {
		    double hypotenuse = cylinder.circle.Radius() / dpc;
		    ON_3dPoint p1 = ipoint + pvect * hypotenuse;
		    ON_3dPoint p2 = ipoint + -1*pvect * hypotenuse;
		    axis_pts_init.Append(p1);
		    axis_pts_init.Append(p2);
#if TIKZ_OUT1
		    // Visualiztion points
		    double radius = cylinder.circle.Radius();
		    ON_3dVector ov = ON_CrossProduct(cylinder.Axis(), pvect);
		    ON_3dPoint plane1 = ipoint + pvect * 1.5*radius + ov * 1.5*radius;
		    ON_3dPoint plane2 = ipoint + pvect * 1.5*radius + -ov * 1.5*radius;
		    ON_3dPoint plane3 = ipoint + -pvect * 1.5*radius + ov * 1.5*radius;
		    ON_3dPoint plane4 = ipoint + -pvect * 1.5*radius + -ov * 1.5*radius;
		    bu_log("\\coordinate (p%dc1) at (%f, %f, %f);\n", i, plane1.x, plane1.y, plane1.z);
		    bu_log("\\coordinate (p%dc2) at (%f, %f, %f);\n", i, plane2.x, plane2.y, plane2.z);
		    bu_log("\\coordinate (p%dc3) at (%f, %f, %f);\n", i, plane3.x, plane3.y, plane3.z);
		    bu_log("\\coordinate (p%dc4) at (%f, %f, %f);\n", i, plane4.x, plane4.y, plane4.z);
		    bu_log("\\draw[orange] (p%dc1) -- (p%dc2);\n", i, i);
		    bu_log("\\draw[orange] (p%dc2) -- (p%dc4);\n", i, i);
		    bu_log("\\draw[orange] (p%dc4) -- (p%dc3);\n", i, i);
		    bu_log("\\draw[orange] (p%dc3) -- (p%dc1);\n", i, i);
#endif
		}
	    }
	}
    }

    // Trim out points that are above the bounding planes
    ON_3dPointArray axis_pts_2nd;
    for (int i = 0; i < axis_pts_init.Count(); i++) {
	int trimmed = 0;
	for (int j = 0; j < (*cyl_planes).Count(); j++) {
	    ON_3dVector v = axis_pts_init[i] - (*cyl_planes)[j].origin;
	    double dotp = ON_DotProduct(v, (*cyl_planes)[j].Normal());
	    if (dotp > 0 && !NEAR_ZERO(dotp, VUNITIZE_TOL)) {
		trimmed = 1;
		break;
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
    data->params->csg_type = CYLINDER;
    // Flag the cylinder according to the negative or positive status of the
    // cylinder surface.
    data->params->negative = negative_cylinder(brep, shoal_nonplanar_face, BREP_CYLINDRICAL_TOL);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';
    // Assign an object id
    data->params->csg_id = (*(data->i->obj_cnt))++;
    // Cylinder geometric data
    ON_3dVector cyl_axis = l.PointAt(tmax) - l.PointAt(tmin);
    BN_VMOVE(data->params->origin, l.PointAt(tmin));
    BN_VMOVE(data->params->hv, cyl_axis);
    data->params->radius = cylinder.circle.Radius();

    // Now that we have the implicit plane and the cylinder, see how much of the cylinder
    // we've got as positive volume.  This information is needed in certain situations to resolve booleans.
    if (implicit_plane_ind != -1) {
	if (nonlinear_edge != -1) {
	    const ON_BrepEdge *edge = &(brep->m_E[nonlinear_edge]);
	    ON_3dPoint midpt = edge->EdgeCurveOf()->PointAt(edge->EdgeCurveOf()->Domain().Mid());
	    ON_Plane p = (*cyl_planes)[implicit_plane_ind];
	    ON_3dVector ve = midpt - p.origin;
	    ON_3dVector va = l.PointAt(tmin) - p.origin;
	    ON_3dVector v1 = p.Normal() * ON_DotProduct(ve, p.Normal());
	    ON_3dVector v2 = p.Normal() * ON_DotProduct(va, p.Normal());
	    if (va.Length() > VUNITIZE_TOL) {
		if (ON_DotProduct(v1, v2) > 0) {
		    data->params->half_cyl = -1;
		} else {
		    data->params->half_cyl = 1;
		}
	    } else {
		data->params->half_cyl = 0;
	    }
	} else {
	    return -1;
	}
    }

    // Use avg normal to constructed oriented bounding box planes around cylinder
    if (need_arbn) {
	ON_3dVector v1 = cylinder.circle.Plane().xaxis;
	ON_3dVector v2 = cylinder.circle.Plane().yaxis;
	v1.Unitize();
	v2.Unitize();
	v1 = v1 * cylinder.circle.Radius();
	v2 = v2 * cylinder.circle.Radius();
	ON_3dPoint arbmid = (l.PointAt(tmax) + l.PointAt(tmin)) * 0.5;
	ON_3dVector cyl_axis_unit = l.PointAt(tmax) - l.PointAt(tmin);
	double axis_len = cyl_axis_unit.Length();
	cyl_axis_unit.Unitize();
	// Bump the top and bottom planes out slightly to avoid problems when the capping plane normals
	// are almost but not quite parallel to the cylinder axis
	ON_3dPoint arbmax = l.PointAt(tmax) + 0.01 * axis_len * cyl_axis_unit;
	ON_3dPoint arbmin = l.PointAt(tmin) - 0.01 * axis_len * cyl_axis_unit;

#if TIKZ_OUT1
	ON_3dPoint bpo, bpn;
#endif
	(*cyl_planes).Append(ON_Plane(arbmin, -1 * cyl_axis_unit));
#if TIKZ_OUT1
	bpo = arbmin;
	bpn = arbmin + -0.75*cyl_axis_unit;
	bu_log("\\coordinate (bpo1) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn1) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo1) -- (bpn1);\n");
#endif
	(*cyl_planes).Append(ON_Plane(arbmax, cyl_axis_unit));
#if TIKZ_OUT1
	bpo = arbmax;
	bpn = arbmax + 0.75*cyl_axis_unit;
	bu_log("\\coordinate (bpo2) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn2) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo2) -- (bpn2);\n");
#endif
	(*cyl_planes).Append(ON_Plane(arbmid + v1, cylinder.circle.Plane().xaxis));
#if TIKZ_OUT1
	bpo = arbmid + v1;
	bpn = bpo + 0.75*cylinder.circle.Plane().xaxis;
	bu_log("\\coordinate (bpo3) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn3) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo3) -- (bpn3);\n");
#endif
	(*cyl_planes).Append(ON_Plane(arbmid - v1, -1 *cylinder.circle.Plane().xaxis));
#if TIKZ_OUT1
	bpo = arbmid-v1;
	bpn = bpo + -0.75*cylinder.circle.Plane().xaxis;
	bu_log("\\coordinate (bpo4) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn4) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo4) -- (bpn4);\n");
#endif
	(*cyl_planes).Append(ON_Plane(arbmid + v2, cylinder.circle.Plane().yaxis));
#if TIKZ_OUT1
	bpo = arbmid+v2;
	bpn = bpo + 0.75*cylinder.circle.Plane().yaxis;
	bu_log("\\coordinate (bpo5) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn5) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo5) -- (bpn5);\n");
#endif
	(*cyl_planes).Append(ON_Plane(arbmid - v2, -1 * cylinder.circle.Plane().yaxis));
#if TIKZ_OUT1
	bpo = arbmid-v2;
	bpn = bpo + -0.75*cylinder.circle.Plane().yaxis;
	bu_log("\\coordinate (bpo6) at (%f, %f, %f);\n", bpo.x, bpo.y, bpo.z);
	bu_log("\\coordinate (bpn6) at (%f, %f, %f);\n", bpn.x, bpn.y, bpn.z);
	bu_log("\\draw[->] (bpo6) -- (bpn6);\n");
#endif


#if TIKZ_OUT1
	ON_3dPoint bbp1 = arbmin + v1 + v2;
	bu_log("\\coordinate (bb1) at (%f, %f, %f);\n", bbp1.x, bbp1.y, bbp1.z);
	ON_3dPoint bbp2 = arbmin + v1 - v2;
	bu_log("\\coordinate (bb2) at (%f, %f, %f);\n", bbp2.x, bbp2.y, bbp2.z);
	ON_3dPoint bbp3 = arbmin - v1 + v2;
	bu_log("\\coordinate (bb3) at (%f, %f, %f);\n", bbp3.x, bbp3.y, bbp3.z);
	ON_3dPoint bbp4 = arbmin - v1 - v2;
	bu_log("\\coordinate (bb4) at (%f, %f, %f);\n", bbp4.x, bbp4.y, bbp4.z);
	ON_3dPoint bbp5 = arbmax + v1 + v2;
	bu_log("\\coordinate (bb5) at (%f, %f, %f);\n", bbp5.x, bbp5.y, bbp5.z);
	ON_3dPoint bbp6 = arbmax + v1 - v2;
	bu_log("\\coordinate (bb6) at (%f, %f, %f);\n", bbp6.x, bbp6.y, bbp6.z);
	ON_3dPoint bbp7 = arbmax - v1 + v2;
	bu_log("\\coordinate (bb7) at (%f, %f, %f);\n", bbp7.x, bbp7.y, bbp7.z);
	ON_3dPoint bbp8 = arbmax - v1 - v2;
	bu_log("\\coordinate (bb8) at (%f, %f, %f);\n", bbp8.x, bbp8.y, bbp8.z);
	bu_log("%%\\node at (bb1) {bb1};\n", bbp1.x, bbp1.y, bbp1.z);
	bu_log("%%\\node at (bb2) {bb2};\n", bbp2.x, bbp2.y, bbp2.z);
	bu_log("%%\\node at (bb3) {bb3};\n", bbp3.x, bbp3.y, bbp3.z);
	bu_log("%%\\node at (bb4) {bb4};\n", bbp4.x, bbp4.y, bbp4.z);
	bu_log("%%\\node at (bb5) {bb5};\n", bbp5.x, bbp5.y, bbp5.z);
	bu_log("%%\\node at (bb6) {bb6};\n", bbp6.x, bbp6.y, bbp6.z);
	bu_log("%%\\node at (bb7) {bb7};\n", bbp7.x, bbp7.y, bbp7.z);
	bu_log("%%\\node at (bb8) {bb8};\n", bbp8.x, bbp8.y, bbp8.z);
	bu_log("\\draw (bb1) -- (bb5) -- (bb6) -- (bb2) -- (bb1);\n");
	bu_log("\\draw (bb1) -- (bb3) -- (bb4) -- (bb2) -- (bb1);\n");
	bu_log("\\draw (bb1) -- (bb5) -- (bb7) -- (bb3) -- (bb1);\n");
	bu_log("\\draw (bb8) -- (bb6) -- (bb2) -- (bb4) -- (bb8);\n");
	bu_log("\\draw (bb8) -- (bb7) -- (bb3) -- (bb4) -- (bb8);\n");
	bu_log("\\draw (bb8) -- (bb7) -- (bb5) -- (bb6) -- (bb8);\n");
#endif
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
