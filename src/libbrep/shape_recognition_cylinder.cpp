#include "common.h"

#include <set>
#include <map>

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

    if (NEAR_ZERO(dotp, 0.000001)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
subbrep_is_cylinder(struct bu_vls *UNUSED(msgs), struct subbrep_island_data *data, fastf_t cyl_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
    // First, check surfaces.  If a surface is anything other than a plane or cylindrical,
    // the verdict is no.  If we don't have at least two planar surfaces and one
    // cylindrical, the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CYLINDER:
                cylindrical_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }
    if (planar_surfaces.size() < 2) return 0;
    if (cylindrical_surfaces.size() < 1) return 0;

    // Second, check if all cylindrical surfaces share the same axis.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder);
    delete cs;
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
        ON_Cylinder f_cylinder;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
        fcs->IsCylinder(&f_cylinder);
	delete fcs;
	if (f_cylinder.circle.Center().DistanceTo(cylinder.circle.Center()) > BREP_CYLINDRICAL_TOL) return 0;
    }

    // Third, see if all planes are coplanar with two and only two planes.
    ON_Plane p1, p2;
    int p2_set = 0;
    data->brep->m_F[*planar_surfaces.begin()].SurfaceOf()->IsPlanar(&p1);
    for (f_it = planar_surfaces.begin(); f_it != planar_surfaces.end(); f_it++) {
        ON_Plane f_p;
        data->brep->m_F[(*f_it)].SurfaceOf()->IsPlanar(&f_p);
        if (!p2_set && f_p != p1) {
            p2 = f_p;
	    p2_set = 1;
            continue;
        };
        if (f_p != p1 && f_p != p2) return 0;
    }

    // Fourth, check that the two planes are perpendicular to the cylinder axis.
    if (p1.Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) {
	return 0;
    }
    if (p2.Normal().IsParallelTo(cylinder.Axis(), VUNITIZE_TOL) == 0) {
	return 0;
    }

    // Populate the primitive data
    data->type = CYLINDER;
    double height[2];
    height[0] = (NEAR_ZERO(cylinder.height[0], VUNITIZE_TOL)) ? -1000 : 2*cylinder.height[0];
    height[1] = (NEAR_ZERO(cylinder.height[1], VUNITIZE_TOL)) ? 1000 : 2*cylinder.height[1];
    ON_Circle c = cylinder.circle;
    ON_Line l(c.plane.origin + height[0]*c.plane.zaxis, c.plane.origin + height[1]*c.plane.zaxis);

    ON_3dPoint center_bottom = ON_LinePlaneIntersect(l, p1);
    ON_3dPoint center_top = ON_LinePlaneIntersect(l, p2);

    // Flag the cylinder according to the negative or positive status of the
    // cylinder surface.  Whether it is actually subtracted from the
    // global object or unioned into a comb lower down the tree (or vice versa)
    // is determined later.
    int negative_shape = negative_cylinder(data->brep, *cylindrical_surfaces.begin(), cyl_tol);

    // If we've got a negative cylinder, bump the center points out very slightly
    // to avoid problems with raytracing.
    if (negative_shape == -1) {
	ON_3dVector cvector(center_top - center_bottom);
	double len = cvector.Length();
	cvector.Unitize();
	cvector = cvector * (len * 0.001);

	center_top = center_top + cvector;
	center_bottom = center_bottom - cvector;
    }

    ON_3dVector hvect(center_top - center_bottom);

    struct csg_object_params *cyl;
    BU_GET(cyl, struct csg_object_params);

    cyl->negative = negative_shape;
    cyl->bool_op = (negative_shape == -1) ? '-' : 'u';
    cyl->origin[0] = center_bottom.x;
    cyl->origin[1] = center_bottom.y;
    cyl->origin[2] = center_bottom.z;
    cyl->hv[0] = hvect.x;
    cyl->hv[1] = hvect.y;
    cyl->hv[2] = hvect.z;
    cyl->radius = cylinder.circle.Radius();

    bu_ptbl_ins(data->children, (long *)cyl);

    return 1;
}

int
cylinder_csg(struct bu_vls *msgs, struct subbrep_shoal_data *data, fastf_t cyl_tol)
{
    int implicit_plane_ind = -1;
    std::set<int> cylindrical_surfaces;
    std::set<int>::iterator c_it;

    const ON_Brep *brep = data->i->brep;
    data->params->bool_op = 'u'; // Initialize to union

    // Characterize the planes of the non-linear edges.
    ON_SimpleArray<ON_Plane> non_linear_edge_planes;
    ON_SimpleArray<ON_3dPoint> edge_midpnts;
    std::set<int> edges;
    std::set<int> linear_edges;
    for (int i = 0; i < data->loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->loops[i]]);
	cylindrical_surfaces.insert(loop->Face()->m_face_index);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
	    }
	}
    }

    for (c_it = edges.begin(); c_it != edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	ON_3dPoint midpt = ecv->PointAt(ecv->Domain().Mid());
	edge_midpnts.Append(midpt);
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
		    f->SurfaceOf()->IsPlanar(&eplane);
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
	} else {
	    linear_edges.insert(*c_it);
	}
	delete ecv;
    }

    // Now, build a list of unique planes
    ON_SimpleArray<ON_Plane> cyl_planes;
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
	}
    }

    // If we have linear edges, filter out edges that a) have two non-planar
    // faces and b) link to faces under consideration in the island.
    std::set<int> le;
    std::set<int>::iterator s_it;
    for (s_it = linear_edges.begin(); s_it != linear_edges.end(); s_it++) {
	std::vector<int> faces;
	const ON_BrepEdge *edge = &(brep->m_E[*s_it]);
	for (int i = 0; i < edge->m_ti.Count(); i++) {
	    const ON_BrepTrim *trim = &(brep->m_T[edge->m_ti[i]]);
	    if (((surface_t *)data->i->face_surface_types)[trim->Face()->m_face_index] == SURFACE_PLANE) break;
	    faces.push_back(trim->Face()->m_face_index);
	}
	if (faces.size() != 2) {
	    le.insert(*s_it);
	    continue;
	}
	int have_both_faces = 1;
	for (unsigned int i = 0; i < faces.size(); i++) {
	    if (cylindrical_surfaces.find(faces[i]) == cylindrical_surfaces.end()) {
		have_both_faces = 0;
		break;
	    }
	}
	if (have_both_faces) {
	    std::set<int> nullv;
	    std::set<int> nulle;
	    bu_log("edge %d is degenerate\n", *s_it);
	    array_to_set(&nullv, data->i->null_verts, data->i->null_vert_cnt);
	    array_to_set(&nulle, data->i->null_edges, data->i->null_edge_cnt);
	    nullv.insert(edge->Vertex(0)->m_vertex_index);
	    nullv.insert(edge->Vertex(1)->m_vertex_index);
	    nulle.insert(*s_it);
	    set_to_array(&(data->i->null_verts), &(data->i->null_vert_cnt), &nullv);
	    set_to_array(&(data->i->null_edges), &(data->i->null_edge_cnt), &nulle);
	}
    }

    // If we have two non-linear edges remaining, construct a plane from them.
    // If we have a count other than two or zero, return.
    if (le.size() != 2 && le.size() != 0) return 0;
    if (le.size() == 2 ) {
	std::set<int> verts;
	// If both edges share a pre-existing face that is planar, use the inverse of that
	// plane.  Otherwise, construct a plane from the vertex points.
	bu_log("found two linear edges\n");
	for (s_it = le.begin(); s_it != le.end(); s_it++) {
	    const ON_BrepEdge *edge = &(brep->m_E[*s_it]);
	    verts.insert(edge->m_vi[0]);
	    verts.insert(edge->m_vi[1]);
	}
	ON_3dPointArray points;
	for (s_it = verts.begin(); s_it != verts.end(); s_it++) {
	    const ON_BrepVertex *v = &(brep->m_V[*s_it]);
	    points.Append(v->Point());
	}

	ON_Plane pf(points[0], points[1], points[2]);
	cyl_planes.Append(pf);
	implicit_plane_ind = cyl_planes.Count() - 1;
    }


    // Need to make sure the normals are pointed the "right way"
    //
    // Check each normal direction to see whether it would trim away any edge midpoints
    // known to be present.  If both directions do so, we have a concave situation and we're done.
    for (int i = 0; i < cyl_planes.Count(); i++) {
	ON_Plane p = cyl_planes[i];
	int flipped = 0;
	for (int j = 0; j < edge_midpnts.Count(); j++) {
	    ON_3dVector v = edge_midpnts[j] - p.origin;
	    double dotp = ON_DotProduct(v, p.Normal());
	    if (dotp > 0 && !NEAR_ZERO(dotp, VUNITIZE_TOL)) {
		if (!flipped) {
		    j = -1; // Check all points again
		    p.Flip();
		    flipped = 1;
		} else {
		    if (msgs) bu_vls_printf(msgs, "%*sConcave planes in %s - no go\n", L3_OFFSET, " ", bu_vls_addr(data->i->key));
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
	bu_log("have implicit plane\n");
	ON_Plane p = cyl_planes[implicit_plane_ind];
	p.Flip();
	bu_log("implicit plane origin: %f, %f, %f\n", p.Origin().x, p.Origin().y, p.Origin().z);
	bu_log("implicit plane normal: %f, %f, %f\n", p.Normal().x, p.Normal().y, p.Normal().z);
	BN_VMOVE(data->params->implicit_plane_origin, p.Origin());
	BN_VMOVE(data->params->implicit_plane_normal, p.Normal());
	data->params->have_implicit_plane = 1;
    }

    // Make a starting cylinder from one of the cylindrical surfaces and construct the axis line
    ON_Cylinder cylinder;
    ON_Surface *cs = brep->m_L[data->loops[0]].Face()->SurfaceOf()->Duplicate();
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
	    bu_log("perfect cylinder\n");
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

    // Grab all the intersections
    ON_3dPointArray axis_pts_init;
    for (int i = 0; i < cyl_planes.Count(); i++) {
	// Note - don't intersect the implicit plane, since it doesn't play a role in defining the main cylinder
	if (!cyl_planes[i].Normal().IsPerpendicularTo(cylinder.Axis(), VUNITIZE_TOL) && i != implicit_plane_ind) {
	    axis_pts_init.Append(ON_LinePlaneIntersect(l, cyl_planes[i]));
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

    // Need to merge points that are "close" so we don't
    // end up with more than two points when multiple planes
    // end up with the same axis intersection. (TODO - need
    // to create a test case for this...)
    ON_3dPointArray axis_pts;
    for (int i = 0; i < axis_pts_2nd.Count(); i++) {
	int has_duplicate = 0;
	for (int j = 0; j < axis_pts.Count(); j++) {
	    if (axis_pts_2nd[i].DistanceTo(axis_pts[j]) < VUNITIZE_TOL) {
		has_duplicate = 1;
		break;
	    }
	}
	if (!has_duplicate) axis_pts.Append(axis_pts_2nd[i]);
    }

    // If, after all that, we still have more than two cap points, we've got a problem
    if (axis_pts.Count() != 2) {
	bu_log("incorrect number of axis points for caps (%d)??\n", axis_pts.Count());
	return 0;
    }

    // Try to handle the situation where multiple end caps all end up
    // with the same intersection on the axis - we need to pick the "flattest"
    // plane for each cap so the cylinder is no larger than it needs to be.
    int end_caps[2] = {-1,-1};
    for (int i = 0; i < axis_pts.Count(); i++) {
	std::set<int> candidates;
	double dp = 2.0;
	for (int j = 0; j < cyl_planes.Count(); j++) {
	    double d = cyl_planes[j].DistanceTo(axis_pts[i]);
	    if (NEAR_ZERO(d, VUNITIZE_TOL)) candidates.insert(j);
	}
	for (c_it = candidates.begin(); c_it != candidates.end(); c_it++) {
	    double dpv = fabs(ON_DotProduct(cyl_planes[*c_it].Normal(), cylinder.Axis()));
	    if (fabs(1-dpv) < fabs(1-dp)) {
		dp = dpv;
		end_caps[i] = *c_it;
	    }
	}
    }
    bu_log("end cap planes: %d,%d\n", end_caps[0], end_caps[1]);


    // Construct "large enough" cylinder based on end cap planes
    double extensions[2] = {0.0, 0.0};
    for (int i = 0; i < 2; i++) {
	double dpc = fabs(ON_DotProduct(cyl_planes[end_caps[i]].Normal(), cylinder.Axis()));
	if (!NEAR_ZERO(dpc, VUNITIZE_TOL)) {
	    double theta = acos(dpc);
	    bu_log("theta: %f\n", theta);
	    double radius = cylinder.circle.Radius();
	    bu_log("radius: %f\n", radius);
	    double hypotenuse = radius / dpc;
	    extensions[i] = fabs(sin(theta) * hypotenuse);
	}
    }
    bu_log("extension 1: %f\n", extensions[0]);
    bu_log("extension 2: %f\n", extensions[1]);

    // Populate the primitive data
    data->params->type = CYLINDER;
    // Flag the cylinder according to the negative or positive status of the
    // cylinder surface.  Whether it is actually subtracted from the
    // global object or unioned into a comb lower down the tree (or vice versa)
    // is determined later.
    data->params->negative = negative_cylinder(brep, *cylindrical_surfaces.begin(), cyl_tol);

    // Now we decide (arbitrarily) what the vector will be for our final cylinder
    // and calculate the true minimum and maximum points along the axis
    ON_3dVector cyl_axis_unit = axis_pts[1] - axis_pts[0];
    size_t cyl_axis_length = cyl_axis_unit.Length();
    cyl_axis_unit.Unitize();
    ON_3dVector extv0 = -1 *cyl_axis_unit;
    ON_3dVector extv1 = cyl_axis_unit;
    extv0.Unitize();
    extv1.Unitize();
    if (data->params->negative == -1) {
	extv0 = extv0 * (extensions[0] + 0.00001 * cyl_axis_length);
	extv1 = extv1 * (extensions[1] + 0.00001 * cyl_axis_length);
    } else {
	extv0 = extv0 * extensions[0];
	extv1 = extv1 * extensions[1];
    }

    axis_pts[0] = axis_pts[0] + extv0;
    axis_pts[1] = axis_pts[1] + extv1;
    ON_3dVector cyl_axis = axis_pts[1] - axis_pts[0];

    BN_VMOVE(data->params->origin, axis_pts[0]);
    BN_VMOVE(data->params->hv, cyl_axis);
    data->params->radius = cylinder.circle.Radius();

    bu_log("in rcc.s rcc %f %f %f %f %f %f %f \n", axis_pts[0].x, axis_pts[0].y, axis_pts[0].z, cyl_axis.x, cyl_axis.y, cyl_axis.z, cylinder.circle.Radius());

    if (!need_arbn) {
	// TODO - may need bbox for subsequent testing...
	bu_log("Perfect cylinder\n");
	return 1;
    }

    // Use avg normal to constructed oriented bounding box planes around cylinder
    ON_3dVector v1 = cylinder.circle.Plane().xaxis;
    ON_3dVector v2 = cylinder.circle.Plane().yaxis;
    v1.Unitize();
    v2.Unitize();
    v1 = v1 * cylinder.circle.Radius();
    v2 = v2 * cylinder.circle.Radius();
    ON_3dPoint arbmid = (axis_pts[1] + axis_pts[0]) * 0.5;
    cyl_planes.Append(ON_Plane(axis_pts[0], -1 * cyl_axis_unit));
    cyl_planes.Append(ON_Plane(axis_pts[1], cyl_axis_unit));
    cyl_planes.Append(ON_Plane(arbmid + v1, cylinder.circle.Plane().xaxis));
    cyl_planes.Append(ON_Plane(arbmid - v1, -1 *cylinder.circle.Plane().xaxis));
    cyl_planes.Append(ON_Plane(arbmid + v2, cylinder.circle.Plane().yaxis));
    cyl_planes.Append(ON_Plane(arbmid - v2, -1 * cylinder.circle.Plane().yaxis));


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
	} else {
	    bu_log("rejecting plane %d\n", i);
	}
    }

    // Second, check for plane intersection points that are inside/outside the
    // arb.  If a set of three planes defines a point that is inside, those
    // planes are part of the final arb.  Based on the arbn prep test.
    int *planes_used = (int *)bu_calloc(uniq_planes.Count(), sizeof(int), "usage flags");
    convex_plane_usage(&uniq_planes, &planes_used);
    // Finally, based on usage tests, construct the set of planes that will define the arbn
    ON_SimpleArray<ON_Plane> arbn_planes;
    for (int i = 0; i < uniq_planes.Count(); i++) {
	if (planes_used[i] != 0) arbn_planes.Append(uniq_planes[i]);
    }
    bu_free(planes_used, "planes_used");


    // Construct the arbn to intersect with the cylinder to form the final shape

    if (arbn_planes.Count() > 3) {
	struct csg_object_params *sub_param;
	BU_GET(data->sub_params, struct bu_ptbl);
	bu_ptbl_init(data->sub_params, 8, "sub_params table");
	BU_GET(sub_param, struct csg_object_params);
	(*(data->i->obj_cnt))++;
	sub_param->id = (*(data->i->obj_cnt));
	sub_param->type = ARBN;
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
	bu_ptbl_ins(data->sub_params, (long *)sub_param);

	struct bu_vls arbn = BU_VLS_INIT_ZERO;
	bu_vls_printf(&arbn, "in arbn.s arbn %d ", arbn_planes.Count());
	for (int i = 0; i < arbn_planes.Count(); i++) {
	    ON_Plane p = arbn_planes[i];
	    double d = p.DistanceTo(ON_3dPoint(0,0,0));
	    bu_vls_printf(&arbn, "%f %f %f %f ", p.Normal().x, p.Normal().y, p.Normal().z, -1*d);
	    bu_log("in half_%d.s half %f %f %f %f\n", i, p.Normal().x, p.Normal().y, p.Normal().z, -1*d);
	}
	bu_log("%s\n", bu_vls_addr(&arbn));
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
