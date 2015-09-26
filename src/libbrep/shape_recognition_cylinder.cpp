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
cylinder_csg(struct bu_vls *msgs, struct subbrep_shoal_data *data, fastf_t cyl_tol)
{
    //bu_log("cyl processing %s\n", bu_vls_addr(data->i->key));

    int implicit_plane_ind = -1;
    std::set<int> cylindrical_surfaces;
    std::set<int>::iterator c_it;

    const ON_Brep *brep = data->i->brep;

    // Collect faces, edges and edge midpoints.
    ON_SimpleArray<ON_3dPoint> edge_midpnts;
    std::set<int> edges;
    for (int i = 0; i < data->shoal_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->shoal_loops[i]]);
	cylindrical_surfaces.insert(loop->Face()->m_face_index);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
		ON_3dPoint midpt = edge->EdgeCurveOf()->PointAt(edge->EdgeCurveOf()->Domain().Mid());
		edge_midpnts.Append(midpt);
	    }
	}
    }


    // Collect edges that link to one plane in the shoal.  Two non-planar faces
    // both in the shoal means a culled edge and verts
    std::set<int> nondegen_edges;
    std::set<int> degen_edges;
    for (c_it = edges.begin(); c_it != edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	int face_cnt = 0;
	for (int i = 0; i < edge->m_ti.Count(); i++) {
	    const ON_BrepTrim *trim = &(brep->m_T[edge->m_ti[i]]);
	    if (((surface_t *)data->i->face_surface_types)[trim->Face()->m_face_index] == SURFACE_PLANE) continue;
	    if (cylindrical_surfaces.find(trim->Face()->m_face_index) != cylindrical_surfaces.end())
		face_cnt++;
	}
	if (face_cnt == 2) {
	    degen_edges.insert(*c_it);
	    //bu_log("edge %d is degenerate\n", *s_it);
	} else {
	    nondegen_edges.insert(*c_it);
	}
    }

    // Update the island's info on degenerate edges and vertices
    std::set<int> nullv;
    std::set<int> nulle;
    array_to_set(&nullv, data->i->null_verts, data->i->null_vert_cnt);
    array_to_set(&nulle, data->i->null_edges, data->i->null_edge_cnt);
    for (c_it = degen_edges.begin(); c_it != degen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	nullv.insert(edge->Vertex(0)->m_vertex_index);
	nullv.insert(edge->Vertex(1)->m_vertex_index);
	nulle.insert(*c_it);
    }	
    set_to_array(&(data->i->null_verts), &(data->i->null_vert_cnt), &nullv);
    set_to_array(&(data->i->null_edges), &(data->i->null_edge_cnt), &nulle);


    // Now, any non-linear edge that isn't degenerate should be supplying a
    // plane.  (We don't currently handle non-planar edges like those from a
    // sphere subtracted from a cylinder.)  Liner curves are saved - they need
    // special interpretation for cylinders and cones (spheres shouldn't have
    // any.)
    ON_SimpleArray<ON_Plane> non_linear_edge_planes;
    std::set<int> linear_edges;
    std::map<int, int> p_2_ei_a;
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	if (!ecv->IsLinear()) {
	    ON_Plane eplane;
	    int have_planar_face = 0;
	    // First, see if the edge has a real planar face associated with it.  If it does,
	    // go with that plane.
	    for (int ti = 0; ti < edge->m_ti.Count(); ti++) {
		const ON_BrepTrim *t = &(brep->m_T[edge->m_ti[ti]]);
		const ON_BrepFace *f = t->Face();
		surface_t st = ((surface_t *)data->i->face_surface_types)[f->m_face_index];
		if (st == SURFACE_PLANE) {
		    f->SurfaceOf()->IsPlanar(&eplane, BREP_PLANAR_TOL);
		    have_planar_face = 1;
		    break;
		}
	    }
	    // No real face - deduce a plane from the curve
	    if (!have_planar_face) {
		ON_Curve *ecv2 = edge->EdgeCurveOf()->Duplicate();
		if (!ecv2->IsPlanar(&eplane, cyl_tol)) {
		    if (msgs) bu_vls_printf(msgs, "%*sNonplanar edge in cylinder (%s) - no go\n", L3_OFFSET, " ", bu_vls_addr(data->i->key));
		    delete ecv;
		    delete ecv2;
		    return 0;
		}
		delete ecv2;
	    }
	    non_linear_edge_planes.Append(eplane);
	    p_2_ei_a[non_linear_edge_planes.Count() - 1] = edge->m_edge_index;
	} else {
	    linear_edges.insert(*c_it);
	}
	delete ecv;
    }

    // Now, build a list of unique planes
    ON_SimpleArray<ON_Plane> cyl_planes;
    std::map<int, int> p_to_ei;
    for (int i = 0; i < non_linear_edge_planes.Count(); i++) {
	int have_plane = 0;
	ON_Plane p1 = non_linear_edge_planes[i];
	ON_3dVector p1n = p1.Normal();
	for (int j = 0; j < cyl_planes.Count(); j++) {
	    ON_Plane p2 = cyl_planes[j];
	    ON_3dVector p2n = p2.Normal();
	    if (p2n.IsParallelTo(p1n, 0.01) && fabs(p2.DistanceTo(p1.Origin())) < 0.001) {
		have_plane = 1;
		break;
	    }
	}
	if (!have_plane) {
	    cyl_planes.Append(p1);
	    p_to_ei[cyl_planes.Count() - 1] = p_2_ei_a[i];
	}
    }

    ////////// Cylinder (and cone) specific /////////////////////////////////////
    // If we have two non-linear edges remaining, construct a plane from them.
    // If we have a count other than two or zero, return.
    if (linear_edges.size() != 2 && linear_edges.size() != 0)
       	return 0;
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
	if (pf.DistanceTo(points[3]) > BREP_PLANAR_TOL) return 0;

	cyl_planes.Append(pf);
	implicit_plane_ind = cyl_planes.Count() - 1;
    }
    ////////// END Cylinder (and cone) specific /////////////////////////////////////

    // Spheres will need their own thing here, based on walking the non-degenerate
    // edge loop and collecting unique planes from sequential coplanar points.


    // Need to make sure the normals are pointed the "right way"
    //
    // Check each normal direction to see whether it would trim away any edge
    // midpoints known to be present.  If both directions do so, we have a
    // concave situation and we're done.
    for (int i = 0; i < cyl_planes.Count(); i++) {
	ON_Plane p = cyl_planes[i];
	//bu_log("in rcc_%d.s rcc %f, %f, %f %f, %f, %f 0.1\n", i, p.origin.x, p.origin.y, p.origin.z, p.Normal().x, p.Normal().y, p.Normal().z);
	int flipped = 0;
	for (int j = 0; j < edge_midpnts.Count(); j++) {
	    ON_3dVector v = edge_midpnts[j] - p.origin;
	    double dotp = ON_DotProduct(v, p.Normal());
	    //bu_log("dotp: %f\n", dotp);
	    if (dotp > 0 && !NEAR_ZERO(dotp, BREP_PLANAR_TOL)) {
		if (!flipped) {
		    j = -1; // Check all points again
		    p.Flip();
		    flipped = 1;
		} else {
		    //bu_log("%*sConcave planes in %s - no go\n", L3_OFFSET, " ", bu_vls_addr(data->i->key));
		    return 0;
		}
	    }
	}
	if (flipped) cyl_planes[i].Flip();
    }

#if 0
    // Planes should be oriented correctly now
    for (int i = 0; i < cyl_planes.Count(); i++) {
	ON_Plane p = cyl_planes[i];
	bu_log("plane %d origin: %f, %f, %f\n", i, p.origin.x, p.origin.y, p.origin.z);
	bu_log("plane %d normal: %f, %f, %f\n", i, p.Normal().x, p.Normal().y, p.Normal().z);
    }
#endif

    if (implicit_plane_ind != -1) {
	//bu_log("have implicit plane\n");
	ON_Plane shoal_implicit_plane = cyl_planes[implicit_plane_ind];
	shoal_implicit_plane.Flip();
	BN_VMOVE(data->params->implicit_plane_origin, shoal_implicit_plane.Origin());
	BN_VMOVE(data->params->implicit_plane_normal, shoal_implicit_plane.Normal());
	data->params->have_implicit_plane = 1;

	// All well and good to have an implicit plane, but if we have face edges being cut by it 
	// we've got a no-go.
	std::set<int> shoal_connected_edges;
	std::set<int>::iterator scl_it;
	for (int i = 0; i < data->shoal_loops_cnt; i++) {
	    const ON_BrepLoop *loop = &(brep->m_L[data->shoal_loops[i]]);
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		int vert_ind;
		const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		if (trim->m_bRev3d) {
		    vert_ind = edge->Vertex(0)->m_vertex_index;
		} else {
		    vert_ind = edge->Vertex(1)->m_vertex_index;
		}
		// Get vertex edges.
		const ON_BrepVertex *v = &(brep->m_V[vert_ind]);
		for (int ei = 0; ei < v->EdgeCount(); ei++) {
		    const ON_BrepEdge *e = &(brep->m_E[v->m_ei[ei]]);
		    //bu_log("insert edge %d\n", e->m_edge_index);
		    shoal_connected_edges.insert(e->m_edge_index);
		}
	    }
	}
	for (scl_it = shoal_connected_edges.begin(); scl_it != shoal_connected_edges.end(); scl_it++) {
	    const ON_BrepEdge *edge= &(brep->m_E[(int)*scl_it]);
	    //bu_log("Edge: %d\n", edge->m_edge_index);
	    ON_3dPoint p1 = brep->m_V[edge->Vertex(0)->m_vertex_index].Point();
	    ON_3dPoint p2 = brep->m_V[edge->Vertex(1)->m_vertex_index].Point();
	    double dotp1 = ON_DotProduct(p1 - shoal_implicit_plane.origin, shoal_implicit_plane.Normal());
	    double dotp2 = ON_DotProduct(p2 - shoal_implicit_plane.origin, shoal_implicit_plane.Normal());
	    if (NEAR_ZERO(dotp1, BREP_PLANAR_TOL) || NEAR_ZERO(dotp2, BREP_PLANAR_TOL)) continue;
	    if ((dotp1 < 0 && dotp2 > 0) || (dotp1 > 0 && dotp2 < 0)) {
		return 0;
	    }
	}
    }


    ////////////// Cylinder specific stuff - break into function //////////////////////////


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
    if (cyl_planes.Count() == 2) {
	ON_Plane p1, p2;
	int perpendicular = 2;
	if (cyl_planes[0].Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) perpendicular--;
	if (cyl_planes[1].Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) perpendicular--;
	if (perpendicular == 2) {
	    //bu_log("perfect cylinder\n");
	    need_arbn = 0;
	}
    }


    // Find end cap planes via intersection points between planes and cylinder axis.
    //
    // Intersect all planes not parallel with the cylindrical axis to get a set of intersection
    // points along the axis.  Then, trim out all points that would be "clipped" by other planes.
    // This *should* leave us with two points, since we're restricting ourselves to convex shapes.
    // Given those two points, find the planes that contain them.  If a point is contained by two
    // or more planes, pick the one with the surface normal most parallel to the cylinder axis.

    // Use the intersection and a vector to an edge midpoint associated with
    // the plane to calculate min and max points on the cylinder from this
    // plane.
    ON_3dPointArray axis_pts_init;
    for (int i = 0; i < cyl_planes.Count(); i++) {
	// Note - don't intersect the implicit plane, since it doesn't play a role in defining the main cylinder
	if (!cyl_planes[i].Normal().IsPerpendicularTo(cylinder.Axis(), VUNITIZE_TOL) && i != implicit_plane_ind) {
	    ON_3dPoint ipoint = ON_LinePlaneIntersect(l, cyl_planes[i]);
	    const ON_BrepEdge *edge = &(brep->m_E[p_to_ei[i]]);
	    ON_3dPoint midpt = edge->EdgeCurveOf()->PointAt(edge->EdgeCurveOf()->Domain().Mid());
	    ON_3dVector pvect = midpt - ipoint;
	    pvect.Unitize();
	    double dpc = fabs(ON_DotProduct(cyl_planes[i].Normal(), cylinder.Axis()));
	    if (!NEAR_ZERO(dpc, VUNITIZE_TOL)) {
		double hypotenuse = cylinder.circle.Radius() / dpc;
		ON_3dPoint p1 = ipoint + pvect * hypotenuse;
		ON_3dPoint p2 = ipoint + -1*pvect * hypotenuse;
		axis_pts_init.Append(p1);
		axis_pts_init.Append(p2);
	    }
	}
    }

    // Trim out points that are above the bounding planes
    ON_3dPointArray axis_pts_2nd;
    for (int i = 0; i < axis_pts_init.Count(); i++) {
	int trimmed = 0;
	for (int j = 0; j < cyl_planes.Count(); j++) {
	    // Don't trim with the implicit plane - the implicit plane is defined "after" the
	    // primary shapes are formed, so it doesn't get a vote in removing capping points
	    if (j != implicit_plane_ind) {
		ON_3dVector v = axis_pts_init[i] - cyl_planes[j].origin;
		double dotp = ON_DotProduct(v, cyl_planes[j].Normal());
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
    data->params->negative = negative_cylinder(brep, *cylindrical_surfaces.begin(), cyl_tol);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';
    // Assign an object id
    data->params->csg_id = (*(data->i->obj_cnt))++;
    // Cylinder geometric data
    ON_3dVector cyl_axis = l.PointAt(tmax) - l.PointAt(tmin);
    BN_VMOVE(data->params->origin, l.PointAt(tmin));
    BN_VMOVE(data->params->hv, cyl_axis);
    data->params->radius = cylinder.circle.Radius();

    ////////////// END Cylinder specific stuff - break into function //////////////////////////

    if (!need_arbn) {
	//bu_log("Perfect cylinder shoal found in %s\n", bu_vls_addr(data->i->key));
	return 1;
    }

    ////////////// Cylinder specific stuff - break into function //////////////////////////
    // Use avg normal to constructed oriented bounding box planes around cylinder
    ON_3dVector v1 = cylinder.circle.Plane().xaxis;
    ON_3dVector v2 = cylinder.circle.Plane().yaxis;
    v1.Unitize();
    v2.Unitize();
    v1 = v1 * cylinder.circle.Radius();
    v2 = v2 * cylinder.circle.Radius();
    ON_3dPoint arbmid = (l.PointAt(tmax) + l.PointAt(tmin)) * 0.5;
    ON_3dVector cyl_axis_unit = l.PointAt(tmax) - l.PointAt(tmin);
    cyl_axis_unit.Unitize();

    cyl_planes.Append(ON_Plane(l.PointAt(tmin), -1 * cyl_axis_unit));
    cyl_planes.Append(ON_Plane(l.PointAt(tmax), cyl_axis_unit));
    cyl_planes.Append(ON_Plane(arbmid + v1, cylinder.circle.Plane().xaxis));
    cyl_planes.Append(ON_Plane(arbmid - v1, -1 *cylinder.circle.Plane().xaxis));
    cyl_planes.Append(ON_Plane(arbmid + v2, cylinder.circle.Plane().yaxis));
    cyl_planes.Append(ON_Plane(arbmid - v2, -1 * cylinder.circle.Plane().yaxis));
    ////////////// END Cylinder specific stuff - break into function //////////////////////////


    /* "Cull" any planes that are not needed to form a bounding arbn.  We do this
     * both for data minimization and because the arbn primitive doesn't tolerate
     * "extra" planes that don't contribute to the final shape. */

    // First, remove any arb planes that are parallel (*not* anti-parallel) with another plane.
    // This will always be a bounding arb plane if any planes are to be removed, so we use that knowledge
    // and start at the top.
    ON_SimpleArray<ON_Plane> uniq_planes;
    for (int i = cyl_planes.Count() - 1; i >= 0; i--) {
	int keep_plane = 1;
	ON_Plane p1 = cyl_planes[i];
	for (int j = i - 1; j >= 0; j--) {
	    ON_Plane p2 = cyl_planes[j];
	    // Check for one to avoid removing anti-parallel planes
	    if (p1.Normal().IsParallelTo(p2.Normal(), VUNITIZE_TOL) == 1) {
		keep_plane = 0;
		break;
	    }
	}
	if (keep_plane) {
	    uniq_planes.Append(p1);
	}
    }

    // Second, check for plane intersection points that are inside/outside the
    // arb.  If a set of three planes defines a point that is inside, those
    // planes are part of the final arb.  Based on the arbn prep test.
    int *planes_used = (int *)bu_calloc(uniq_planes.Count(), sizeof(int), "usage flags");
    convex_plane_usage(&uniq_planes, &planes_used);
    // Finally, based on usage tests, construct the set of planes that will define the arbn.
    // If it doesn't have 3 or more uses, it's not a net contributor to the shape.
    ON_SimpleArray<ON_Plane> arbn_planes;
    for (int i = 0; i < uniq_planes.Count(); i++) {
	//if (planes_used[i] != 0 && planes_used[i] < 3) bu_log("%d: have %d uses for plane %d\n", *data->i->obj_cnt + 1, planes_used[i], i);
	if (planes_used[i] != 0 && planes_used[i] > 2) arbn_planes.Append(uniq_planes[i]);
    }
    bu_free(planes_used, "planes_used");


    // Construct the arbn to intersect with the cylinder to form the final shape
    if (arbn_planes.Count() > 3) {
	struct csg_object_params *sub_param;
	BU_GET(sub_param, struct csg_object_params);
	csg_object_params_init(sub_param, data);
	sub_param->csg_id = (*(data->i->obj_cnt))++;
	sub_param->csg_type = ARBN;
	sub_param->bool_op = '+'; // arbn is intersected with primary primitive
	sub_param->planes = (plane_t *)bu_calloc(arbn_planes.Count(), sizeof(plane_t), "planes");
	sub_param->plane_cnt = arbn_planes.Count();
	for (int i = 0; i < arbn_planes.Count(); i++) {
	    ON_Plane p = arbn_planes[i];
	    double d = p.DistanceTo(ON_3dPoint(0,0,0));
	    sub_param->planes[i][0] = p.Normal().x;
	    sub_param->planes[i][1] = p.Normal().y;
	    sub_param->planes[i][2] = p.Normal().z;
	    sub_param->planes[i][3] = -1 * d;
	}
	bu_ptbl_ins(data->shoal_children, (long *)sub_param);
    }


    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
