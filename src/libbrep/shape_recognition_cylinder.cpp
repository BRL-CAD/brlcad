/* FIXME: header missing, run sh/header.sh */

#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L3_OFFSET 6
#define L4_OFFSET 8


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
    if (linear_edges.size() == 2 ) {
	std::set<int> verts;
	// If both edges share a pre-existing face that is planar, use the inverse of that
	// plane.  Otherwise, construct a plane from the vertex points.
	//bu_log("found two linear edges\n");
	for (c_it = linear_edges.begin(); c_it != linear_edges.end(); c_it++) {
	    const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	    verts.insert(edge->m_vi[0]);
	    verts.insert(edge->m_vi[1]);
	}
	ON_3dPointArray points;
	for (c_it = verts.begin(); c_it != verts.end(); c_it++) {
	    const ON_BrepVertex *v = &(brep->m_V[*c_it]);
	    points.Append(v->Point());
	}

	ON_Plane pf(points[0], points[1], points[2]);

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
    // find all the limits from the various planes and edges
    ON_3dPointArray axis_pts_init;
    // Add in all the nondegenerate edge vertices
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	axis_pts_init.Append(edge->Vertex(0)->Point());
	axis_pts_init.Append(edge->Vertex(1)->Point());
    }

    // Use the intersection and the projection of the cylinder axis onto the plane
    // to calculate min and max points on the cylinder from this plane.
    for (int i = 0; i < (*cyl_planes).Count(); i++) {
	// Note - don't intersect the implicit plane, since it doesn't play a role in defining the main cylinder
	if (!(*cyl_planes)[i].Normal().IsPerpendicularTo(cylinder.Axis(), VUNITIZE_TOL) && i != implicit_plane_ind) {
	    ON_3dPoint ipoint = ON_LinePlaneIntersect(l, (*cyl_planes)[i]);
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
		}
	    }
	}
    }

    // Trim out points that are above the bounding planes
    ON_3dPointArray axis_pts_2nd;
    for (int i = 0; i < axis_pts_init.Count(); i++) {
	int trimmed = 0;
	for (int j = 0; j < (*cyl_planes).Count(); j++) {
	    // Don't trim with the implicit plane - the implicit plane is defined "after" the
	    // primary shapes are formed, so it doesn't get a vote in removing capping points
	    if (j != implicit_plane_ind) {
		ON_3dVector v = axis_pts_init[i] - (*cyl_planes)[j].origin;
		double dotp = ON_DotProduct(v, (*cyl_planes)[j].Normal());
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

	(*cyl_planes).Append(ON_Plane(arbmin, -1 * cyl_axis_unit));
	(*cyl_planes).Append(ON_Plane(arbmax, cyl_axis_unit));
	(*cyl_planes).Append(ON_Plane(arbmid + v1, cylinder.circle.Plane().xaxis));
	(*cyl_planes).Append(ON_Plane(arbmid - v1, -1 *cylinder.circle.Plane().xaxis));
	(*cyl_planes).Append(ON_Plane(arbmid + v2, cylinder.circle.Plane().yaxis));
	(*cyl_planes).Append(ON_Plane(arbmid - v2, -1 * cylinder.circle.Plane().yaxis));
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
