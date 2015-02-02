#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
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

    obj->type = CYLINDER;
    obj->origin[0] = set1_c.Center().x;
    obj->origin[1] = set1_c.Center().y;
    obj->origin[2] = set1_c.Center().z;
    obj->hv[0] = hvect.x;
    obj->hv[1] = hvect.y;
    obj->hv[2] = hvect.z;
    obj->radius = set1_c.Radius();

    bu_ptbl_ins(data->objs, (long *)obj);

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

int
cylinder_csg(struct subbrep_object_data *data, fastf_t cyl_tol)
{
    bu_log("process partial cylinder\n");
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

    // Check for multiple cylinders.  Can handle this, but for now punt.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder);
    delete cs;
    std::set<int>::iterator f_it;
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
        ON_Cylinder f_cylinder;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
        fcs->IsCylinder(&f_cylinder);
	delete fcs;
	if (f_cylinder.circle.Center().DistanceTo(cylinder.circle.Center()) > BREP_CYLINDRICAL_TOL) {
	    std::cout << "\n\nMultiple cylinders found\n\n";
	    return 0;
	}
    }

    // Characterize the planes of the non-linear edges.  We need two planes - more
    // than that indicate some sort of subtraction behavior.  TODO - Eventually we should
    // be able to characterize which edges form the subtraction shape candidate, but
    // for now just bail unless we're dealing with the simple case.
    //
    // TODO - this test is adequate only for RCC shapes.  Need to generalize
    // this to check for both arcs and shared planes in non-arc curves to
    // accommodate csg situations.
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
		    delete ecv;
		    delete ecv2;
		    return 0;
		}
	    }
	    delete ecv2;
	}
	delete ecv;
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
    if (set1_c.Plane().Normal().IsParallelTo(set2_c.Plane().Normal(), cyl_tol) != 0) {
	if (corner_verts.size() == 0) {
	    std::cout << "Full cylinder\n";
	    data->type = CYLINDER;
	    struct csg_object_params * obj;
	    BU_GET(obj, struct csg_object_params);

	    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

	    obj->type = CYLINDER;
	    obj->origin[0] = set1_c.Center().x;
	    obj->origin[1] = set1_c.Center().y;
	    obj->origin[2] = set1_c.Center().z;
	    obj->hv[0] = hvect.x;
	    obj->hv[1] = hvect.y;
	    obj->hv[2] = hvect.z;
	    obj->radius = set1_c.Radius();

	    bu_ptbl_ins(data->objs, (long *)obj);

	    return 1;
	} else {
	    // We have parallel faces and corners - we need to subtract an arb
	    data->type = COMB;
	    std::cout << "Minus body arb\n";

	    // cylinder
	    struct csg_object_params * obj;
	    BU_GET(obj, struct csg_object_params);

	    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

	    obj->type = CYLINDER;
	    obj->origin[0] = set1_c.Center().x;
	    obj->origin[1] = set1_c.Center().y;
	    obj->origin[2] = set1_c.Center().z;
	    obj->hv[0] = hvect.x;
	    obj->hv[1] = hvect.y;
	    obj->hv[2] = hvect.z;
	    obj->radius = set1_c.Radius();

	    bu_ptbl_ins(data->objs, (long *)obj);

	    // arb8
	    struct csg_object_params * arb;
	    BU_GET(arb, struct csg_object_params);


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

	    // First, find the two points closest to the set1_c and set2_c planes
	    double offset = 0.0;
	    std::set<int>::iterator s_it;
	    ON_SimpleArray<ON_3dPoint> corner_pnts(4);
	    ON_SimpleArray<ON_3dPoint> bottom_pnts(2);
	    ON_SimpleArray<ON_3dPoint> top_pnts(2);
	    for (s_it = corner_verts.begin(); s_it != corner_verts.end(); s_it++) {
		ON_3dPoint p = data->brep->m_V[*s_it].Point();
		corner_pnts.Append(p);
		double d = set1_c.Plane().DistanceTo(p);
		if (d > offset) offset = d;
		std::cout << "d(" << (int)*s_it << "): " << d << "\n";
	    }
	    for (int p = 0; p < corner_pnts.Count(); p++) {
		if (set1_c.Plane().DistanceTo(corner_pnts[p]) < offset) {
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
	    std::cout << "dotp: " << dotp << "\n";
	    if (dotp > 0) {
		pcyl.Flip();
		double dotp2 = ON_DotProduct(invec, pcyl.Normal());
		std::cout << "dotp2: " << dotp2 << "\n";
	    }

	    // Third, construct the axis vector and determine the arb
	    // order of the bottom and top points
	    ON_3dVector cyl_axis = set2_c.Center() - set1_c.Center();

	    ON_3dVector v1 = bottom_pnts[0] - bottom_pnts[1];
	    ON_3dVector v1x = ON_CrossProduct(v1, cyl_axis);
	    ON_3dVector v2 = bottom_pnts[1] - bottom_pnts[0];
	    ON_3dVector v2x = ON_CrossProduct(v2, cyl_axis);

	    double flag1 = ON_DotProduct(v1x, pcyl.Normal());
	    std::cout << "flag1: " << flag1 << "\n";
	    double flag2 = ON_DotProduct(v2x, pcyl.Normal());
	    std::cout << "flag2: " << flag2 << "\n";

	    ON_3dVector w1 = top_pnts[0] - top_pnts[1];
	    ON_3dVector w1x = ON_CrossProduct(w1, cyl_axis);
	    ON_3dVector w2 = top_pnts[1] - top_pnts[0];
	    ON_3dVector w2x = ON_CrossProduct(w2, cyl_axis);

	    double flag3 = ON_DotProduct(w1x, pcyl.Normal());
	    std::cout << "flag3: " << flag3 << "\n";
	    double flag4 = ON_DotProduct(w2x, pcyl.Normal());
	    std::cout << "flag4: " << flag4 << "\n";

	    // Once the 1,2,3,4 points are determined, scale them out
	    // along their respective line segment axis to make sure
	    // the resulting arb is large enough to subtract the full
	    // radius of the cylinder.  Only need to do this if the
	    // center point of the cylinder is inside the subtracting arb -
	    // should be able to test that with the circle center point
	    // a distance to pcyl plane calculation for the second point,
	    // then subtract the center from the point on the plane and do
	    // a dot product test with pcyl's normal.

	    // Once the final 1,2,3,4 points have been determined, use
	    // the pcyl normal direction and the cylinder radius to
	    // construct the remaining arb points.



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
