#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

// Remove degenerate edge sets. A degenerate edge set is defined as two
// linear segments having the same two vertices.  (To be sure, we should probably
// check curve directions in loops in some fashion...)
void
subbrep_remove_degenerate_edges(struct subbrep_object_data *data, std::set<int> *edges){
    std::set<int> degenerate;
    std::set<int>::iterator e_it;
    for (e_it = edges->begin(); e_it != edges->end(); e_it++) {
	if (degenerate.find(*e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[*e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (int j = 0; j < data->edges_cnt; j++) {
		    int f_ind = data->edges[j];
		    ON_BrepEdge& edge2 = data->brep->m_E[f_ind];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(*e_it);
			    degenerate.insert(f_ind);
			    break;
			}
		    }
		}
	    }
	}
    }
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
	//std::cout << "erasing " << *e_it << "\n";
	edges->erase(*e_it);
    }
}


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
	ON_BrepFace *used_face = &(data->brep->m_F[f_ind]);
        int surface_type = (int)GetSurfaceType(used_face->SurfaceOf(), NULL);
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
        std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        std::cout << "p2 Normal: " << p2.Normal().x << "," << p2.Normal().y << "," << p2.Normal().z << "\n";
        return 0;
    }

    // Fifth, remove degenerate edge sets.
    std::set<int> active_edges;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    subbrep_remove_degenerate_edges(data, &active_edges);
    
    // Sixth, check for any remaining linear segments.  For rpc primitives
    // those are expected, but for a true cylinder the linear segments should
    // all wash out in the degenerate pass
    std::set<int>::iterator e_it;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        if (edge.EdgeCurveOf()->IsLinear()) return 0;
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
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        ON_Arc arc;
        if (edge.EdgeCurveOf()->IsArc(NULL, &arc, cyl_tol)) {
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
                return 0;
            }
        }
    }

    data->type = CYLINDER;

    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

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
cylindrical_loop_planar_vertices(ON_BrepFace *face, int loop_index)
{
    std::set<int> verts;
    ON_Brep *brep = face->Brep();
    ON_BrepLoop *loop = &(brep->m_L[loop_index]);
    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	ON_BrepTrim& trim = brep->m_T[loop->m_ti[ti]];
	if (trim.m_ei != -1) {
	    ON_BrepEdge& edge = brep->m_E[trim.m_ei];
	    verts.insert(edge.Vertex(0)->m_vertex_index);
	    verts.insert(edge.Vertex(1)->m_vertex_index);
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
    ON_BrepFace *face = &(data->brep->m_F[face_index]);
    array_to_set(&loops, data->loops, data->loops_cnt);
    for(l_it = loops.begin(); l_it != loops.end(); l_it++) {
	return cylindrical_loop_planar_vertices(face, face->m_li[*l_it]);
    }
    return 0;
}

int
cylinder_csg(struct subbrep_object_data *data, fastf_t cyl_tol)
{
    bu_log("process partial cylinder\n");
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
	ON_BrepFace *used_face = &(data->brep->m_F[f_ind]);
        int surface_type = (int)GetSurfaceType(used_face->SurfaceOf(), NULL);
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

    // Characterize the planes of the non-linear edges.  We need two planes - more
    // than that indicate some sort of subtraction behavior.  TODO - Eventually we should
    // be able to characterize which edges form the subtraction shape candidate, but
    // for now just bail unless we're dealing with the simple case.
    //
    // TODO - this test is adequate only for RCC shapes.  Need to generalize
    // this to check for both arcs and shared planes in non-arc curves to
    // accomidate csg situations.
    std::set<int> arc_set_1, arc_set_2;
    ON_Circle set1_c, set2_c;
    int arc1_circle_set= 0;
    int arc2_circle_set = 0;

    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	ON_BrepEdge& edge = data->brep->m_E[ei];
	if (!edge.EdgeCurveOf()->IsLinear()) {

	    ON_Arc arc;
	    if (edge.EdgeCurveOf()->IsArc(NULL, &arc, cyl_tol)) {
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
		    std::cout << "found extra circle - no go\n";
		    return 0;
		}
	    }
	}
    }


    // CSG representable cylinders may represent one or both of the
    // following cases:
    //
    // a) non-parallel end caps - one or both capping planes are not
    //    perpendicular to the axis of the cylinder.
    //
    // b) partial cylindrical surface - some portion of the cylinderical
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
	ON_BrepEdge& edge = data->brep->m_E[ei];
	candidate_verts.insert(edge.Vertex(0)->m_vertex_index);
	candidate_verts.insert(edge.Vertex(1)->m_vertex_index);
    }
    for (v_it = candidate_verts.begin(); v_it != candidate_verts.end(); v_it++) {
	ON_BrepVertex& vert = data->brep->m_V[*v_it];
	int curve_cnt = 0;
	int line_cnt = 0;
	for (int i = 0; i < vert.m_ei.Count(); i++) {
	    int ei = vert.m_ei[i];
	    ON_BrepEdge& edge = data->brep->m_E[ei];
	    if (edges.find(edge.m_edge_index) != edges.end()) {
		if (edge.EdgeCurveOf()->IsLinear()) {
		    line_cnt++;
		} else {
		    curve_cnt++;
		}
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
	ON_3dPoint p1 = data->brep->m_V[*v_it].Point();
	s_it++;
	ON_3dPoint p2 = data->brep->m_V[*v_it].Point();
	s_it++;
	ON_3dPoint p3 = data->brep->m_V[*v_it].Point();
	s_it++;
	ON_3dPoint p4 = data->brep->m_V[*v_it].Point();
	ON_Plane tmp_plane(p1, p2, p3);
	if (tmp_plane.DistanceTo(p4) > BREP_PLANAR_TOL) {
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
		return 0;
	    }
	}
    }

    // Now, get the cylindrical surface properties.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder);
    delete cs;

    // Check if the two circles are parallel to each other.  If they are, and we have
    // no corner points, then we have a complete cylinder
    if (set1_c.Plane().Normal().IsParallelTo(set2_c.Plane().Normal(), cyl_tol) != 0) {
	if (corner_verts.size() == 0) {
	    std::cout << "Full cylinder\n";
	    data->type = CYLINDER;

	    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

	    data->params->origin[0] = set1_c.Center().x;
	    data->params->origin[1] = set1_c.Center().y;
	    data->params->origin[2] = set1_c.Center().z;
	    data->params->hv[0] = hvect.x;
	    data->params->hv[1] = hvect.y;
	    data->params->hv[2] = hvect.z;
	    data->params->radius = set1_c.Radius();

	    return 1;
	} else {
	    // We have parallel faces and corners - we need to subtract an arb
	    data->type = COMB;
	    std::cout << "Minus body arb\n";
	    return 1;
	}
    } else {
	if (corner_verts.size() == 0) {
	    // We have non parallel faces and no corners - at least one and possible
	    // both end caps need subtracting, but no other subtractions are needed.
	    data->type = COMB;
	    std::cout << "Minus one or more end-cap arbs\n";
	    return 1;
	} else {
	    // We have non parallel faces and corners - at least one and possible
	    // both end caps need subtracting, plus an arb to remove part of the
	    // cylinder body.
	    data->type = COMB;
	    std::cout << "Minus one or more end-cap arbs and body arb\n";
	    return 1;
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
