#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bn/plane_calc.h"
#include "bn/tol.h"
#include "shape_recognition.h"

int
subbrep_is_cylinder(struct subbrep_object_data *data, fastf_t cyl_tol)
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
            continue;
        };
        if (f_p != p1 && f_p != p2) return 0;
    }

    // Fourth, check that the two planes are parallel to each other.
    if (p1.Normal().IsParallelTo(p2.Normal(), cyl_tol) == 0) {
        //std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        //std::cout << "p2 Normal: " << p2.Normal().x << "," << p2.Normal().y << "," << p2.Normal().z << "\n";
        return 0;
    }

    // Fifth, remove from the active edge set all linear edges that have both faces
    // present in the subbrep data set.  For a whole cylinder, the circular edges
    // govern.
    std::set<int> active_edges;
    std::set<int>::iterator e_it;
    std::set<int> faces;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    array_to_set(&faces , data->faces, data->faces_cnt);
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	std::set<int> faces_found;
	const ON_BrepEdge *edge = &(data->brep->m_E[*e_it]);
	ON_Curve *ec = edge->EdgeCurveOf()->Duplicate();
	if (ec->IsLinear()) {
	    for (int i = 0; i < edge->TrimCount(); i++) {
		const ON_BrepTrim *trim = edge->Trim(i);
		int f_ind = trim->Face()->m_face_index;
		if (faces.find(f_ind) != faces.end()) faces_found.insert(f_ind);
	    }
	    if (faces_found.size() == 2) {
		active_edges.erase(*e_it);
	    }
	}
	delete ec;
    }

    // Sixth, check for any remaining linear segments.  For partial rcc
    // primitives (e.g. a single surface that defines part of a cylinder but
    // has no mating faces to complete the shape) those are expected, but for a
    // true cylinder the linear segments should all wash out in the degenerate
    // pass.  In principle this may not be necessary, since any such surfaces
    // shouldn't show up isolated in a proper brep...
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        const ON_BrepEdge *edge = &(data->brep->m_E[*e_it]);
	ON_Curve *ec = edge->EdgeCurveOf()->Duplicate();
        if (ec->IsLinear()) {
	    delete ec;
	    return 0;
	}
	delete ec;
    }

    // Seventh, sort the curved edges into one of two circles.  Again, in more
    // general cases we might have other curves but a true cylinder should have
    // all of its arcs on these two circles.  We don't need to check for closed
    // loops because we are assuming that in the input Brep; any curve except
    // arc curves that survived the degeneracy test has already resulted in an
    // earlier rejection.
    std::set<int> arc_set_1, arc_set_2;
    ON_Circle set1_c, set2_c;
    int arc1_circle_set= 0;
    int arc2_circle_set = 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        const ON_BrepEdge *edge = &(data->brep->m_E[*e_it]);
	ON_Curve *ec = edge->EdgeCurveOf()->Duplicate();
        ON_Arc arc;
        if (ec->IsArc(NULL, &arc, cyl_tol)) {
            int assigned = 0;
            ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
            //std::cout << "circ " << circ.Center().x << " " << circ.Center().y << " " << circ.Center().z << "\n";
            if (!arc1_circle_set) {
                arc1_circle_set = 1;
                set1_c = circ;
                //std::cout << "center 1 " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << "\n";
            } else {
                if (!arc2_circle_set) {
                    if (!(NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol))){
                        arc2_circle_set = 1;
                        set2_c = circ;
                        //std::cout << "center 2 " << set2_c.Center().x << " " << set2_c.Center().y << " " << set2_c.Center().z << "\n";
                    }
                }
            }
            if (NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol)){
                arc_set_1.insert(*e_it);
                assigned = 1;
            }
            if (arc2_circle_set) {
                if (NEAR_ZERO(circ.Center().DistanceTo(set2_c.Center()), cyl_tol)){
                    arc_set_2.insert(*e_it);
                    assigned = 1;
                }
            }
            if (!assigned) {
                std::cout << "found extra circle - no go\n";
		delete ec;
                return 0;
            }
        }
	delete ec;
    }

    data->type = CYLINDER;
    struct csg_object_params * obj;
    BU_GET(obj, struct csg_object_params);

    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

    data->params->bool_op= 'u';  // TODO - not always union
    data->params->origin[0] = set1_c.Center().x;
    data->params->origin[1] = set1_c.Center().y;
    data->params->origin[2] = set1_c.Center().z;
    data->params->hv[0] = hvect.x;
    data->params->hv[1] = hvect.y;
    data->params->hv[2] = hvect.z;
    data->params->radius = set1_c.Radius();

    return 1;
}


int
cylindrical_loop_planar_vertices(const ON_Brep *brep, int loop_index)
{
    std::set<int> verts;
    const ON_BrepLoop *loop = &(brep->m_L[loop_index]);
    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	const ON_BrepTrim& trim = brep->m_T[loop->m_ti[ti]];
	if (trim.m_ei != -1) {
	    const ON_BrepEdge *edge = &(brep->m_E[trim.m_ei]);
	    verts.insert(edge->Vertex(0)->m_vertex_index);
	    verts.insert(edge->Vertex(1)->m_vertex_index);
	}
    }
    if (verts.size() == 3) {
	//std::cout << "Three points - planar.\n";
	return 1;
    } else if (verts.size() >= 3) {
	std::set<int>::iterator v_it = verts.begin();
	ON_3dPoint p1 = brep->m_V[*v_it].Point();
	v_it++;
	ON_3dPoint p2 = brep->m_V[*v_it].Point();
	v_it++;
	ON_3dPoint p3 = brep->m_V[*v_it].Point();
	ON_Plane test_plane(p1, p2, p3);
	for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    if (!NEAR_ZERO(test_plane.DistanceTo(brep->m_V[*v_it].Point()), BREP_PLANAR_TOL)) {
		//std::cout << "vertex " << *v_it << " too far from plane, not planar: " << test_plane.DistanceTo(brep->m_V[*v_it].Point()) << "\n";
		return 0;
	    }
	}
	//std::cout << verts.size() << " points, planar\n";
	return 1;
    } else {
	//std::cout << "Closed single curve loop - planar only if surface is.";
	return 0;
    }
    return 0;
}

int
cylindrical_planar_vertices(struct subbrep_object_data *data, int face_index)
{
    std::set<int> loops;
    std::set<int>::iterator l_it;
    array_to_set(&loops, data->loops, data->loops_cnt);
    for(l_it = loops.begin(); l_it != loops.end(); l_it++) {
	return cylindrical_loop_planar_vertices(data->brep, data->brep->m_F[face_index].m_li[*l_it]);
    }
    return 0;
}

/* Return -1 if the cylinder face is pointing in toward the cylinder axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cylinder(struct subbrep_object_data *data, int face_index, double cyl_tol) {
    const ON_Surface *surf = data->brep->m_F[face_index].SurfaceOf();
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
    if (dotp < 0) return 1;
    return -1;
}


int
cylinder_csg(struct subbrep_object_data *data, fastf_t cyl_tol)
{
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
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
		std::cout << "what???\n";
                return 0;
                break;
        }
    }
    data->params->bool_op = 'u'; // Initialize to union

    // Check for multiple cylinders.  Can handle this, but for now punt.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder, cyl_tol);
    delete cs;
    std::set<int>::iterator f_it;
    int cyl_count = 0;
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
        ON_Cylinder f_cylinder;
	cyl_count++;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
        fcs->IsCylinder(&f_cylinder, cyl_tol);
	delete fcs;
	//std::cout << "cyl_count: " << cyl_count << "\n";
	if (f_cylinder.circle.Center().DistanceTo(cylinder.circle.Center()) > cyl_tol) {
	    std::cout << "\n\nMultiple cylinders found\n\n";
	    return 0;
	}
    }

    // Characterize the planes of the non-linear edges.  We need at least two planes - more
    // than that indicate some sort of subtraction behavior.
    ON_SimpleArray<ON_Plane> edge_planes;
    std::map<int, int> edge_plane_map;
    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	if (!ecv->IsLinear()) {
	    ON_Plane eplane;
	    ON_Curve *ecv2 = edge->EdgeCurveOf()->Duplicate();
	    if (!ecv2->IsPlanar(&eplane, cyl_tol)) {
		std::cout << "nonplanar edge in cylinder" << "\n";
		delete ecv;
		delete ecv2;
		return 0;
	    }
	    edge_planes.Append(eplane);
	    edge_plane_map[edge_planes.Count() - 1] = ei;
	    delete ecv2;
	}
	delete ecv;
    }

    // Now, build a list of unique planes
    ON_SimpleArray<ON_Plane> cyl_planes;
    for (int i = 0; i < edge_planes.Count(); i++) {
	int have_plane = 0;
	ON_Plane p1 = edge_planes[i];
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

    // CSG representable cylinders may represent one or both of the
    // following cases:
    //
    // a) non-parallel end caps - one or both capping planes are not
    //    perpendicular to the axis of the cylinder.
    //
    // b) partial cylindrical surface - some portion of the cylindrical
    //    surface is trimmed away.
    //
    // There are an infinite number of ways in which subsets of a cylinder
    // may be removed by trimming curves - the plan is for complexities
    // introduced into the outer loops to be reduced by recognizing the
    // complex portions of those curves as influences of other shapes.
    // Once recognized, the loops are simplified until we reach a shape
    // that can be handled by the above cases.
    //
    // For example, let's say a cylindrical face has the following
    // trim loop in its UV space:
    //
    //                   -------------------------
    //                   |                       |
    //                   |                       |
    //                   |     *************     |
    //                   |     *           *     |
    //                   -------           -------
    //
    // The starred portion of the trimming curve is not representable
    // in this CSG scheme, but if that portion of the curve is the
    // result of a subtraction of another shape in the parent brep,
    // then that portion of the curve can be treated as implicit in
    // the subtraction of that other object.  The complex lower trim
    // curve set can then be replaced by a line between the two corner
    // vertex points, which are not removed by the subtraction.
    //
    // Until such cases can be resolved, any curve complications of
    // this sort are a conversion blocker.  To make sure the supplied
    // inputs are cases that can be handled, we collect all of the
    // vertices in the data that are connected to one and only one
    // non-linear edge in the set.  Failure cases are:
    //
    // * More than four vertices that are mated with exactly one
    //   non-linear edge in the data set
    // * Four vertices meeting previous criteria that are non-planar
    // * Any vertex on a linear edge that is not coplanar with the
    //   plane described by the vertices meeting the above criteria
    std::set<int> candidate_verts;
    std::set<int> corner_verts; /* verts with one nonlinear edge */
    std::set<int> linear_verts; /* verts with only linear edges */
    std::set<int>::iterator v_it, e_it;
    std::set<int> edges;
    array_to_set(&edges, data->edges, data->edges_cnt);
    // collect all candidate vertices
    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	candidate_verts.insert(edge->Vertex(0)->m_vertex_index);
	candidate_verts.insert(edge->Vertex(1)->m_vertex_index);
    }
    for (v_it = candidate_verts.begin(); v_it != candidate_verts.end(); v_it++) {
	const ON_BrepVertex *vert = &(data->brep->m_V[*v_it]);
	int curve_cnt = 0;
	int line_cnt = 0;
	for (int i = 0; i < vert->m_ei.Count(); i++) {
	    int ei = vert->m_ei[i];
	    const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	    if (edges.find(edge->m_edge_index) != edges.end()) {
		ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
		if (ecv->IsLinear()) {
		    line_cnt++;
		} else {
		    curve_cnt++;
		}
		delete ecv;
	    }
	}
	if (curve_cnt == 1) {
	    corner_verts.insert(*v_it);
	    //std::cout << "found corner vert: " << *v_it << "\n";
	}
	if (line_cnt > 1 && curve_cnt == 0) {
	    linear_verts.insert(*v_it);
	    std::cout << "found linear vert: " << *v_it << "\n";
	}
    }

    // First, check corner count
    if (corner_verts.size() > 4) {
	std::cout << "more than 4 corners - complex\n";
	return 0;
    }

    // Second, create the candidate face plane.  Verify coplanar status of points if we've got 4.
    ON_Plane pcyl;
    if (corner_verts.size() == 4) {
	std::set<int>::iterator s_it = corner_verts.begin();
	ON_3dPoint p1 = data->brep->m_V[*s_it].Point();
	s_it++;
	ON_3dPoint p2 = data->brep->m_V[*s_it].Point();
	s_it++;
	ON_3dPoint p3 = data->brep->m_V[*s_it].Point();
	s_it++;
	ON_3dPoint p4 = data->brep->m_V[*s_it].Point();
	ON_Plane tmp_plane(p1, p2, p3);
	if (tmp_plane.DistanceTo(p4) > BREP_PLANAR_TOL) {
	    std::cout << "planar tol fail\n";
	    return 0;
	} else {
	    pcyl = tmp_plane;
	}
    } else {
	// TODO - If we have less than four corner points and no additional curve planes, we
	// must have a face subtraction that tapers to a point at the edge of the
	// cylinder.  Pull the linear edges from the two corner points to find the third point -
	// this is a situation where a simpler arb (arb6?) is adequate to make the subtraction.
    }

    // Third, if we had vertices with only linear edges, check to make sure they are in fact
    // on the candidate plane for the face (if not, we've got something more complex going on).
    if (linear_verts.size() > 0) {
	std::set<int>::iterator s_it;
	for (s_it = linear_verts.begin(); s_it != linear_verts.end(); s_it++) {
	    ON_3dPoint pnt = data->brep->m_V[*s_it].Point();
	    if (pcyl.DistanceTo(pnt) > BREP_PLANAR_TOL) {
		std::cout << "stray verts fail\n";
		return 0;
	    }
	}
    }

    // Check if the two circles are parallel to each other.  If they are, and we have
    // no corner points, then we have a complete cylinder
    if (cyl_planes.Count() == 2 && cyl_planes[0].Normal().IsParallelTo(cyl_planes[1].Normal(), 0.1/*cyl_tol*/) != 0 && cyl_planes[0].Normal().IsParallelTo(cylinder.Axis(), 0.1/*cyl_tol*/) != 0) {
	// We must have had arcs to get here - use them.
	std::set<int> arc_set_1, arc_set_2;
	ON_Circle set1_c, set2_c;
	int arc1_circle_set= 0;
	int arc2_circle_set = 0;
	for (int i = 0; i < data->edges_cnt; i++) {
	    int ei = data->edges[i];
	    const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	    ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	    if (!ecv->IsLinear()) {
		ON_Arc arc;
		ON_Curve *ecv2 = edge->EdgeCurveOf()->Duplicate();
		if (ecv2->IsArc(NULL, &arc, cyl_tol)) {
		    int assigned = 0;
		    ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
		    if (!arc1_circle_set) {
			arc1_circle_set = 1;
			set1_c = circ;
		    } else {
			if (!arc2_circle_set) {
			    if (!(NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol))){
				arc2_circle_set = 1;
				set2_c = circ;
			    }
			}
		    }
		    if (NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol)){
			arc_set_1.insert(ei);
			assigned = 1;
		    }
		    if (arc2_circle_set) {
			if (NEAR_ZERO(circ.Center().DistanceTo(set2_c.Center()), cyl_tol)){
			    arc_set_2.insert(ei);
			    assigned = 1;
			}
		    }
		    if (!assigned) {
			std::cout << "found extra circle - no go: " << bu_vls_addr(data->key) << "\n";
			std::cout << "center 1 " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << "\n";
			std::cout << "center 2 " << set2_c.Center().x << " " << set2_c.Center().y << " " << set2_c.Center().z << "\n";
			std::cout << "circ " << circ.Center().x << " " << circ.Center().y << " " << circ.Center().z << "\n";
			delete ecv;
			delete ecv2;
			return 0;
		    }
		}
		delete ecv2;
	    }
	    delete ecv;
	}

	ON_3dVector hvect(set2_c.Center() - set1_c.Center());

	if (corner_verts.size() == 0) {
	    //std::cout << "Full cylinder\n";
	    data->type = CYLINDER;

	    data->params->bool_op = 'u'; // TODO - not always union
	    data->params->origin[0] = set1_c.Center().x;
	    data->params->origin[1] = set1_c.Center().y;
	    data->params->origin[2] = set1_c.Center().z;
	    data->params->hv[0] = hvect.x;
	    data->params->hv[1] = hvect.y;
	    data->params->hv[2] = hvect.z;
	    data->params->radius = set1_c.Radius();

	    // If there are faces in the planar volume data matching a planar
	    // face associated with this cylinder, remove them - a full cylinder
	    // subshape will not contribute a planar face.
	    std::set<int>::iterator p_it;
	    for (p_it = planar_surfaces.begin(); p_it != planar_surfaces.end(); p_it++) {
		//subbrep_remove_planar_face(data->planar_obj, *p_it);
	    }
	    return 1;
	} else {
	    // We have parallel faces and corners - we need to use an arb.
	    data->type = COMB;

	    struct subbrep_object_data *cyl_obj;
	    BU_GET(cyl_obj, struct subbrep_object_data);
	    subbrep_object_init(cyl_obj, data->brep);
	    std::string key = face_set_key(cylindrical_surfaces);
	    bu_vls_sprintf(cyl_obj->key, "%s", key.c_str());
	    cyl_obj->type = CYLINDER;

	    if (BU_STR_EQUAL(bu_vls_addr(cyl_obj->key), "10")) {
		std::cout << bu_vls_addr(cyl_obj->key) << "\n";
	    }

	    // Flag the cyl/arb comb according to the negative or positive status of the
	    // cylinder surface.  Whether the comb is actually subtracted from the
	    // global object or unioned into a comb lower down the tree (or vice versa)
	    // is determined later.
	    int negative = negative_cylinder(data, *cylindrical_surfaces.begin(), cyl_tol);

	    switch (negative) {
		case -1:
		    data->params->bool_op = '-';
		    break;
		case 1:
		    data->params->bool_op = 'u';
		    break;
		default:
		    std::cout << "Could not determine cylinder status???????\n";
		    data->params->bool_op = 'u';
		    break;
	    }

	    // cylinder - positive object in this sub-comb
	    cyl_obj->params->bool_op = 'u';
	    cyl_obj->params->origin[0] = set1_c.Center().x;
	    cyl_obj->params->origin[1] = set1_c.Center().y;
	    cyl_obj->params->origin[2] = set1_c.Center().z;
	    cyl_obj->params->hv[0] = hvect.x;
	    cyl_obj->params->hv[1] = hvect.y;
	    cyl_obj->params->hv[2] = hvect.z;
	    cyl_obj->params->radius = set1_c.Radius();

	    bu_ptbl_ins(data->children, (long *)cyl_obj);

	    // arb8 - subtracted from the previous cylinder in this sub-comb

	    //                                       8
	    //                                    *  |   *
	    //                                 *     |       *
	    //                             4         |           7
	    //                             |    *    |        *  |
	    //                             |         *     *     |
	    //                             |         |  3        |
	    //                             |         |  |        |
	    //                             |         |  |        |
	    //                             |         5  |        |
	    //                             |       *    |*       |
	    //                             |   *        |    *   |
	    //                             1            |        6
	    //                                 *        |     *
	    //                                      *   |  *
	    //                                          2
	    //

	    struct subbrep_object_data *arb_obj;
	    BU_GET(arb_obj, struct subbrep_object_data);
	    subbrep_object_init(arb_obj, data->brep);
	    bu_vls_sprintf(arb_obj->key, "%s_arb8", key.c_str());
	    arb_obj->type = ARB8;


	    // First, find the two points closest to the set1_c and set2_c planes
	    double offset = 0.0;
	    std::set<int>::iterator s_it;
	    ON_SimpleArray<const ON_BrepVertex *> corner_pnts(4);
	    ON_SimpleArray<const ON_BrepVertex *> bottom_pnts(2);
	    ON_SimpleArray<const ON_BrepVertex *> top_pnts(2);
	    for (s_it = corner_verts.begin(); s_it != corner_verts.end(); s_it++) {
		ON_3dPoint p = data->brep->m_V[*s_it].Point();
		corner_pnts.Append(&(data->brep->m_V[*s_it]));
		double d = set1_c.Plane().DistanceTo(p);
		if (d > offset) offset = d;
	    }
	    for (int p = 0; p < corner_pnts.Count(); p++) {
		double poffset = set1_c.Plane().DistanceTo(corner_pnts[p]->Point());
		if (!NEAR_ZERO(poffset - offset, 0.01) && poffset < offset) {
		    bottom_pnts.Append(corner_pnts[p]);
		} else {
		    top_pnts.Append(corner_pnts[p]);
		}
	    }

	    // Second, select a point from an arc edge not on the subtraction
	    // plane, construct a vector from the circle center to that point,
	    // and determine if the pcyl plane direction is already in the opposite
	    // direction or needs to be reversed.
	    const ON_BrepEdge *edge = &(data->brep->m_E[*arc_set_1.begin()]);
	    ON_Arc arc;
	    ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	    (void)ecv->IsArc(NULL, &arc, cyl_tol);
	    delete ecv;
	    ON_3dPoint center = set1_c.Center();
	    ON_3dPoint midpt = arc.MidPoint();


	    ON_3dVector invec = center - midpt;
	    double dotp = ON_DotProduct(invec, pcyl.Normal());
	    if (dotp < 0) {
		pcyl.Flip();
	    }

	    // Third, construct the axis vector and determine the arb
	    // order of the bottom and top points
	    ON_3dVector cyl_axis = set2_c.Center() - set1_c.Center();

	    ON_3dVector vv1 = bottom_pnts[0]->Point() - bottom_pnts[1]->Point();
	    ON_3dVector v1x = ON_CrossProduct(vv1, cyl_axis);

	    double flag1 = ON_DotProduct(v1x, pcyl.Normal());

	    ON_3dVector w1 = top_pnts[0]->Point() - top_pnts[1]->Point();
	    ON_3dVector w1x = ON_CrossProduct(w1, cyl_axis);

	    double flag3 = ON_DotProduct(w1x, pcyl.Normal());

	    const ON_BrepVertex *v1, *v2, *v3, *v4;
	    if (flag1 < 0) {
		v1 = bottom_pnts[1];
		v2 = bottom_pnts[0];
		vv1 = -vv1;
	    } else {
		v1 = bottom_pnts[0];
		v2 = bottom_pnts[1];
	    }
	    if (flag3 < 0) {
		v3 = top_pnts[0];
		v4 = top_pnts[1];
	    } else {
		v3 = top_pnts[1];
		v4 = top_pnts[0];
	    }
#if 0
	    std::cout << "v1 (" << pout(v1->Point()) << ")\n";
	    std::cout << "v2 (" << pout(v2->Point()) << ")\n";
	    std::cout << "v3 (" << pout(v3->Point()) << ")\n";
	    std::cout << "v4 (" << pout(v4->Point()) << ")\n";
#endif

	    // Before we manipulate the points for arb construction,
	    // see if we need to use them to define a new face.
	    if (!data->is_island) {
		// The cylinder shape is a partial cylinder, and it is not
		// topologically isolated, so we need to make a new face
		// in the parent to replace this one.
		if (data->parent) {
		    // First, see if a local planar brep object has been generated for
		    // this shape.  Such a brep is not generated up front, because
		    // there is no way to be sure a planar parent is needed until
		    // we hit a case like this - for example, a brep consisting of
		    // stacked cylinders of different diameters will have multiple
		    // planar surfaces, but can be completely described without use
		    // of planar volumes in the final CSG expression.  If another
		    // shape has already triggered the generation we don't want to
		    // do it again,  but the first shape to make the need clear has
		    // to trigger the build.
		    if (!data->parent->planar_obj) {
			subbrep_planar_init(data);
		    }
		    // Now, add the new face
		    ON_SimpleArray<const ON_BrepVertex *> vert_loop(4);
		    vert_loop.Append(v1);
		    vert_loop.Append(v2);
		    vert_loop.Append(v3);
		    vert_loop.Append(v4);
		    subbrep_add_planar_face(data->parent, &pcyl, &vert_loop);
		}
	    }



	    // Once the 1,2,3,4 points are determined, scale them out
	    // along their respective line segment axis to make sure
	    // the resulting arb is large enough to subtract the full
	    // radius of the cylinder.
	    //
	    // TODO - Only need to do this if the
	    // center point of the cylinder is inside the subtracting arb -
	    // should be able to test that with the circle center point
	    // a distance to pcyl plane calculation for the second point,
	    // then subtract the center from the point on the plane and do
	    // a dot product test with pcyl's normal.
	    //
	    // TODO - Can optimize this - can make a narrower arb using
	    // the knowledge of the distance between p1/p2.  We only need
	    // to add enough extra length to clear the cylinder, which
	    // means the full radius length is almost always overkill.
	    ON_SimpleArray<ON_3dPoint> arb_points(8);
	    arb_points[0] = v1->Point();
	    arb_points[1] = v2->Point();
	    arb_points[2] = v3->Point();
	    arb_points[3] = v4->Point();
	    vv1.Unitize();
	    vv1 = vv1 * set1_c.Radius();
	    arb_points[0] = arb_points[0] + vv1;
	    arb_points[1] = arb_points[1] - vv1;
	    arb_points[2] = arb_points[2] - vv1;
	    arb_points[3] = arb_points[3] + vv1;
	    ON_3dVector hpad = arb_points[2] - arb_points[1];
	    hpad.Unitize();
	    hpad = hpad * (cyl_axis.Length() * 0.01);
	    arb_points[0] = arb_points[0] - hpad;
	    arb_points[1] = arb_points[1] - hpad;
	    arb_points[2] = arb_points[2] + hpad;
	    arb_points[3] = arb_points[3] + hpad;

	    // Once the final 1,2,3,4 points have been determined, use
	    // the pcyl normal direction and the cylinder radius to
	    // construct the remaining arb points.
	    ON_3dPoint p5, p6, p7, p8;
	    ON_3dVector arb_side = pcyl.Normal() * 2*set1_c.Radius();
	    arb_points[4] = arb_points[0] + arb_side;
	    arb_points[5] = arb_points[1] + arb_side;
	    arb_points[6] = arb_points[2] + arb_side;
	    arb_points[7] = arb_points[3] + arb_side;

	    arb_obj->params->bool_op = '-';
	    arb_obj->params->arb_type = 8;
	    for (int j = 0; j < 8; j++) {
		VMOVE(arb_obj->params->p[j], arb_points[j]);
	    }

	    bu_ptbl_ins(data->children, (long *)arb_obj);

	    return 1;
	}
    } else {
	if (cyl_planes.Count() == 2) {
#if 0
	    std::cout << "  key: " << bu_vls_addr(data->key) << "\n";
	    std::cout << "  normal 1: " << pout(cyl_planes[0].Normal()) << "\n";
	    std::cout << "  normal 2: " << pout(cyl_planes[1].Normal()) << "\n";
	    std::cout << "  axis: " << pout(cylinder.Axis()) << "\n";
#endif

	    if (corner_verts.size() == 0) {
		// We have non parallel faces and no corners - at least one and possible
		// both end caps need subtracting, but no other subtractions are needed.
		data->type = COMB;
		std::cout << "TODO: Minus one or more end-cap arbs\n";
		return 1;
	    } else {
		// We have non parallel faces and corners - at least one and possible
		// both end caps need subtracting, plus an arb to remove part of the
		// cylinder body.
		data->type = COMB;
		std::cout << "Minus one or more end-cap arbs and body arb\n";

		// If an end capping plane's normal is parallel to the cylinder axis,
		// no arb is needed for that cap and that face will form the base of
		// the cylinder.
		int parallel_1 = 0;
		int parallel_2 = 0;
		if (cyl_planes[0].Normal().IsParallelTo(cylinder.Axis(), 0.1/*cyl_tol*/)) parallel_1 = 1;
		if (cyl_planes[1].Normal().IsParallelTo(cylinder.Axis(), 0.1/*cyl_tol*/)) parallel_2 = 1;
		if (parallel_1 + parallel_2 == 1) {
		    ON_Plane base_plane;
		    ON_Plane tilted_plane;
		    if (parallel_1) {
			base_plane = cyl_planes[0];
			tilted_plane = cyl_planes[1];
		    } else {
			base_plane = cyl_planes[1];
			tilted_plane = cyl_planes[0];
		    }
		    std::cout << "plane normal: " << pout(tilted_plane.Normal()) << "\n";
		    double angle = acos(ON_DotProduct(cylinder.Axis(), tilted_plane.Normal()));
		    std::cout << "  dihedral angle: " << angle * ON_RADIANS_TO_DEGREES << "\n";
		    double diameter = cylinder.circle.Radius() * 2;
		    std::cout << "  diameter: " << diameter << "\n";
		    double hypotenuse = diameter / ON_DotProduct(cylinder.Axis(), tilted_plane.Normal());
		    std::cout << "  hypotenuse: " << hypotenuse << "\n";
		    std::cout << "  opposite: " << sin(angle) * hypotenuse << "\n";
		} else {

		    data->type = COMB;
		    // First order of business - find the intersection between the axis and the capping
		    // plane
		    point_t intersect_pt;
		    fastf_t dist;
		    point_t axis_pt;
		    vect_t axis_dir;
		    plane_t plane_eqn;
		    struct bn_tol intersect_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		    VMOVE(axis_pt, cylinder.circle.Center());
		    VMOVE(axis_dir, cylinder.Axis());
		    VUNITIZE(axis_dir);
		    plane_eqn[0] = cyl_planes[0].plane_equation.x;
		    plane_eqn[1] = cyl_planes[0].plane_equation.y;
		    plane_eqn[2] = cyl_planes[0].plane_equation.z;
		    plane_eqn[3] = cyl_planes[0].plane_equation.d;

		    if (bn_isect_line3_plane(&dist, axis_pt, axis_dir, plane_eqn, &intersect_tol) < 0) {
			plane_eqn[0] = -1 * cyl_planes[0].plane_equation.x;
			plane_eqn[1] = -1 * cyl_planes[0].plane_equation.y;
			plane_eqn[2] = -1 * cyl_planes[0].plane_equation.z;
			(void)bn_isect_line3_plane(&dist, axis_pt, axis_dir, plane_eqn, &intersect_tol);
		    }

		    VSCALE(axis_dir, axis_dir, dist);
		    VADD2(intersect_pt, axis_pt, axis_dir);

		    std::cout << "Intersect point 1: " << intersect_pt[0] << "," << intersect_pt[1] << "," << intersect_pt[2] << "\n";
		    ON_3dPoint ip1(intersect_pt[0], intersect_pt[1], intersect_pt[2]);

		    VMOVE(axis_dir, cylinder.Axis());
		    VUNITIZE(axis_dir);
		    plane_eqn[0] = cyl_planes[1].plane_equation.x;
		    plane_eqn[1] = cyl_planes[1].plane_equation.y;
		    plane_eqn[2] = cyl_planes[1].plane_equation.z;
		    plane_eqn[3] = cyl_planes[1].plane_equation.d;

		    if (bn_isect_line3_plane(&dist, axis_pt, axis_dir, plane_eqn, &intersect_tol) < 0) {
			plane_eqn[0] = -1 * cyl_planes[1].plane_equation.x;
			plane_eqn[1] = -1 * cyl_planes[1].plane_equation.y;
			plane_eqn[2] = -1 * cyl_planes[1].plane_equation.z;
			(void)bn_isect_line3_plane(&dist, axis_pt, axis_dir, plane_eqn, &intersect_tol);
		    }

		    VSCALE(axis_dir, axis_dir, dist);
		    VADD2(intersect_pt, axis_pt, axis_dir);

		    std::cout << "Intersect point 2: " << intersect_pt[0] << "," << intersect_pt[1] << "," << intersect_pt[2] << "\n";
		    ON_3dPoint ip2(intersect_pt[0], intersect_pt[1], intersect_pt[2]);

		    // Define the cylinder.
		    struct subbrep_object_data *cyl_obj;
		    BU_GET(cyl_obj, struct subbrep_object_data);
		    subbrep_object_init(cyl_obj, data->brep);
		    std::string key = face_set_key(cylindrical_surfaces);
		    bu_vls_sprintf(cyl_obj->key, "%s", key.c_str());
		    cyl_obj->type = CYLINDER;

		    // Flag the cyl/arb comb according to the negative or positive status of the
		    // cylinder surface.  Whether the comb is actually subtracted from the
		    // global object or unioned into a comb lower down the tree (or vice versa)
		    // is determined later.
		    int negative = negative_cylinder(data, *cylindrical_surfaces.begin(), cyl_tol);

		    switch (negative) {
			case -1:
			    data->params->bool_op = '-';
			    break;
			case 1:
			    data->params->bool_op = 'u';
			    break;
			default:
			    std::cout << "Could not determine cylinder status???????\n";
			    data->params->bool_op = 'u';
			    break;
		    }

		    // cylinder - positive object in this sub-comb
		    cyl_obj->params->bool_op = 'u';
		    VMOVE(cyl_obj->params->origin, ip1);
		    ON_3dVector hvect = ip2 - ip1;
		    cyl_obj->params->hv[0] = hvect.x;
		    cyl_obj->params->hv[1] = hvect.y;
		    cyl_obj->params->hv[2] = hvect.z;
		    cyl_obj->params->radius = cylinder.circle.Radius();

		    bu_ptbl_ins(data->children, (long *)cyl_obj);

		    // Create an OpenNURBS cylinder from the final cyl data
		    // so we can use it to guide the arb creation.
		    ON_Plane fplane(ip1, hvect);
		    ON_Circle fcirc(fplane, cylinder.circle.Radius());
		    ON_Cylinder fcyl(fcirc, hvect.Length());

		    ON_Plane cap1(ip1, cyl_planes[0].Normal());
		    if(ON_DotProduct(cap1.Normal(), hvect) > 0) cap1.Flip();
		    ON_Plane cap2(ip2, cyl_planes[1].Normal());
		    if(ON_DotProduct(cap2.Normal(), hvect) < 0) cap2.Flip();

#if 0
		    ON_3dPoint cp1 = ip1 + ext_dir_1;
		    std::cout << "CP1: " << pout(cp1) << "\n";
		    ON_3dPoint cp2 = ip2 + ext_dir_2;
		    std::cout << "CP2: " << pout(cp2) << "\n";
#endif
		    // The normal of the two end cap planes, when projected
		    // onto the axis plane, "points" the way towards the lowest
		    // point of the arb.  To form the arb, the cylinder face is
		    // bound by a square in the plane of the face where the
		    // pointing vector points to the middle of one of the
		    // edges.  This square provides 4 of the 6 arb points, and
		    // then the last two are made by dropping vectors down the
		    // cylinder axis from the two square points on the "low"
		    // side
		    {
			// Cap 1
			cap1.Flip();
			double angle = acos(ON_DotProduct(fplane.Normal(), cap1.Normal()));
			double diameter = fcyl.circle.Radius();
			double hypotenuse = diameter / ON_DotProduct(fplane.Normal(), cap1.Normal());
			double ext_length = fabs(sin(angle) * hypotenuse);
			ON_3dVector ext_dir = hvect;
			ext_dir.Unitize();
			ext_dir = ext_dir * ext_length;
			cap1.Flip();

			ON_Xform cyl_proj;
			cyl_proj.PlanarProjection(fplane);
			ON_3dVector v1 = cap1.Normal();
			v1.Transform(cyl_proj);
			v1.Unitize();
			ON_3dVector v2 = ON_CrossProduct(-1*fplane.Normal(), v1);
			v1 = v1 * fcyl.circle.Radius();
			v2 = v2 * fcyl.circle.Radius();

			ON_SimpleArray<ON_3dPoint> arb_points(6);
			arb_points[1] = ip1 + v1 - v2;
			arb_points[2] = ip1 + v1 + v2;
			arb_points[4] = ip1 - v2;
			arb_points[5] = ip1 + v2;

			arb_points[0] = arb_points[1] + ext_dir;
			arb_points[3] = arb_points[2] + ext_dir;

			struct subbrep_object_data *arb_obj_1;
			BU_GET(arb_obj_1, struct subbrep_object_data);
			subbrep_object_init(arb_obj_1, data->brep);
			bu_vls_sprintf(arb_obj_1->key, "%s_cap_1", key.c_str());
			arb_obj_1->type = ARB6;
			arb_obj_1->params->bool_op = '-';
			arb_obj_1->params->arb_type = 8;
			for (int j = 0; j < 6; j++) {
			    VMOVE(arb_obj_1->params->p[j], arb_points[j]);
			}

			bu_ptbl_ins(data->children, (long *)arb_obj_1);
		    }
		    {
			// Cap 2
			double angle = acos(ON_DotProduct(fplane.Normal(), cap2.Normal()));
			double diameter = fcyl.circle.Radius();
			double hypotenuse = diameter / ON_DotProduct(fplane.Normal(), cap2.Normal());
			double ext_length = fabs(sin(angle) * hypotenuse);
			ON_3dVector ext_dir = -hvect;
			ext_dir.Unitize();
			ext_dir = ext_dir * ext_length;

			ON_Xform cyl_proj;
			cyl_proj.PlanarProjection(fplane);
			ON_3dVector v1 = cap2.Normal();
			v1.Transform(cyl_proj);
			v1.Unitize();
			ON_3dVector v2 = ON_CrossProduct(fplane.Normal(), v1);
			v1 = v1 * fcyl.circle.Radius();
			v2 = v2 * fcyl.circle.Radius();

			ON_SimpleArray<ON_3dPoint> arb_points(6);
			arb_points[1] = ip2 + v1 - v2;
			arb_points[2] = ip2 + v1 + v2;
			arb_points[4] = ip2 - v2;
			arb_points[5] = ip2 + v2;

			arb_points[0] = arb_points[1] + ext_dir;
			arb_points[3] = arb_points[2] + ext_dir;

			struct subbrep_object_data *arb_obj_2;
			BU_GET(arb_obj_2, struct subbrep_object_data);
			subbrep_object_init(arb_obj_2, data->brep);
			bu_vls_sprintf(arb_obj_2->key, "%s_cap_2", key.c_str());
			arb_obj_2->type = ARB6;
			arb_obj_2->params->bool_op = '-';
			arb_obj_2->params->arb_type = 8;
			for (int j = 0; j < 6; j++) {
			    VMOVE(arb_obj_2->params->p[j], arb_points[j]);
			}

			bu_ptbl_ins(data->children, (long *)arb_obj_2);
		    }


		}

		return 1;
	    }
	} else {
	    std::cout << "More than two capping planes (count is " << cyl_planes.Count() << ") - currently unhandled\n";
	}
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
