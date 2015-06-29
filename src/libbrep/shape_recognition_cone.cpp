#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L3_OFFSET 6

/* Return -1 if the cone face is pointing in toward the axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cone(struct subbrep_object_data *data, int face_index, double cone_tol) {
    int ret = 0;
    const ON_Surface *surf = data->brep->m_F[face_index].SurfaceOf();
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

    if (NEAR_ZERO(dotp, 0.000001)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (data->brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
subbrep_is_cone(struct bu_vls *msgs, struct subbrep_object_data *data, fastf_t cone_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> conic_surfaces;
    // First, check surfaces.  If a surface is anything other than planes or cones,
    // the verdict is no.  If we don't have one planar surface and one or more
    // conic surfaces, the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CONE:
                conic_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }
    if (planar_surfaces.size() < 2) return 0;
    if (conic_surfaces.size() < 1) return 0;

    // Second, check if all conic surfaces share the same base point, apex point and radius.
    ON_Cone cone;
    data->brep->m_F[*conic_surfaces.begin()].SurfaceOf()->IsCone(&cone);
    for (f_it = conic_surfaces.begin(); f_it != conic_surfaces.end(); f_it++) {
        ON_Cone f_cone;
        data->brep->m_F[(*f_it)].SurfaceOf()->IsCone(&f_cone);
        ON_3dPoint fbp = f_cone.BasePoint();
        ON_3dPoint bp = cone.BasePoint();
        if (fbp.DistanceTo(bp) > 0.01) return 0;
        ON_3dPoint fap = f_cone.ApexPoint();
        ON_3dPoint ap = cone.ApexPoint();
        if (fap.DistanceTo(ap) > 0.01) return 0;
        if (!(NEAR_ZERO(cone.radius - f_cone.radius, 0.01))) return 0;
    }

    // Third, see if the planar face and the planes of any non-linear edges from the conic
    // faces are coplanar. TODO - do we need to do this?
    ON_Plane p1;
    data->brep->m_F[*planar_surfaces.begin()].SurfaceOf()->IsPlanar(&p1);

    // Fourth, check that the conic axis and the planar face are perpendicular.  If they
    // are not it isn't fatal, but we need to add a subtracting tgc and return a comb
    // to handle this situation so for now for simplicity require the perpendicular condition
    if (p1.Normal().IsParallelTo(cone.Axis(), 0.01) == 0) {
        //std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        //std::cout << "cone axis: " << cone.Axis().x << "," << cone.Axis().y << "," << cone.Axis().z << "\n";
        return 0;
    }


    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    // Fifth, remove degenerate edge sets.
    std::set<int> active_edges;
    std::set<int>::iterator e_it;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    //subbrep_remove_linear_degenerate_edges(data, &active_edges);

    // Sixth, check for any remaining linear segments.  If we have a real
    // cone and not just a partial, all the linear segments should have
    // washed out.
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_Curve *cv = data->brep->m_E[*e_it].EdgeCurveOf()->Duplicate();
        if (cv->IsLinear()) {
	    delete cv;
	    return 0;
	}
	delete cv;
    }

    // Seventh, make sure all the curved edges are on the same circle.
    // TODO - this is only for a true cone object - a TGC, which can also
    // be handled here, will have parallel circles
    ON_Circle circle;
    int circle_set= 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        const ON_BrepEdge *edge = &(data->brep->m_E[*e_it]);
        ON_Curve *ecurve = edge->EdgeCurveOf()->Duplicate();
        ON_Arc arc;
        if (ecurve->IsArc(NULL, &arc, 0.01)) {
            ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
            if (!circle_set) {
                circle_set = 1;
                circle = circ;
            }
            if (!NEAR_ZERO(circ.Center().DistanceTo(circle.Center()), 0.01)){
                if (msgs) bu_vls_printf(msgs, "%*sfound extra circle in %s - no go\n", L3_OFFSET, " ", bu_vls_addr(data->key));
		delete ecurve;
                return 0;
            }
        }
	delete ecurve;
    }

    data->type = CONE;

    data->negative_shape = negative_cone(data, *conic_surfaces.begin(), cone_tol);


    ON_3dPoint center_bottom = cone.BasePoint();
    ON_3dPoint center_top = cone.ApexPoint();

    // If we've got a negative cylinder, bump the center points out
    // very slightly
    if (data->negative_shape == -1) {
	ON_3dVector cvector(center_top - center_bottom);
	double len = cvector.Length();
	cvector.Unitize();
	cvector = cvector * (len * 0.001);

	center_top = center_top + cvector;
	center_bottom = center_bottom - cvector;
    }

    ON_3dVector hvect(center_top - center_bottom);


    data->params->bool_op = (data->negative_shape == -1) ? '-' : 'u';
    data->params->origin[0] = cone.BasePoint().x;
    data->params->origin[1] = cone.BasePoint().y;
    data->params->origin[2] = cone.BasePoint().z;
    data->params->hv[0] = hvect.x;
    data->params->hv[1] = hvect.y;
    data->params->hv[2] = hvect.z;
    data->params->radius = circle.Radius();
    data->params->r2 = 0.000001;
    data->params->height = hvect.Length();

    return 1;
}

int
cone_csg(struct bu_vls *msgs, struct subbrep_object_data *data, fastf_t cone_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> conic_surfaces;

    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
        int surface_type = (int)GetSurfaceType(data->brep->m_F[f_ind].SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CONE:
                conic_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }

    // Second, check if all conic surfaces share the same base point, apex point and radius.
    ON_Cone cone;
    data->brep->m_F[*conic_surfaces.begin()].SurfaceOf()->IsCone(&cone, cone_tol);
    for (f_it = conic_surfaces.begin(); f_it != conic_surfaces.end(); f_it++) {
        ON_Cone f_cone;
        data->brep->m_F[(*f_it)].SurfaceOf()->IsCone(&f_cone, cone_tol);
        ON_3dPoint fbp = f_cone.BasePoint();
        ON_3dPoint bp = cone.BasePoint();
        if (fbp.DistanceTo(bp) > cone_tol) return 0;
        ON_3dPoint fap = f_cone.ApexPoint();
        ON_3dPoint ap = cone.ApexPoint();
        if (fap.DistanceTo(ap) > cone_tol) return 0;
        if (!(NEAR_ZERO(cone.radius - f_cone.radius, cone_tol))) return 0;
    }

    // Characterize the planes of the non-linear edges.  We need one or two planes - one
    // indicates a true cone, two indicates a truncated cone.  More indicates something
    // we aren't currently set up to handle.
    std::set<int> arc_set_1, arc_set_2;
    ON_Circle set1_c, set2_c;
    int arc1_circle_set= 0;
    int arc2_circle_set = 0;

    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	if (!edge->EdgeCurveOf()->IsLinear()) {

	    ON_Arc arc;
	    if (edge->EdgeCurveOf()->IsArc(NULL, &arc, cone_tol)) {
		int assigned = 0;
		ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
		//std::cout << "circ " << circ.Center().x << " " << circ.Center().y << " " << circ.Center().z << "\n";
		if (!arc1_circle_set) {
		    arc1_circle_set = 1;
		    set1_c = circ;
		    //std::cout << "center 1 " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << "\n";
		} else {
		    if (!arc2_circle_set) {
			if (!(NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cone_tol))){
			    arc2_circle_set = 1;
			    set2_c = circ;
			    //std::cout << "center 2 " << set2_c.Center().x << " " << set2_c.Center().y << " " << set2_c.Center().z << "\n";
			}
		    }
		}
		if (NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cone_tol)){
		    arc_set_1.insert(ei);
		    assigned = 1;
		}
		if (arc2_circle_set) {
		    if (NEAR_ZERO(circ.Center().DistanceTo(set2_c.Center()), cone_tol)){
			arc_set_2.insert(ei);
			assigned = 1;
		    }
		}
		if (!assigned) {
		    if (msgs) bu_vls_printf(msgs, "%*sfound extra circle in %s - no go\n", L3_OFFSET, " ", bu_vls_addr(data->key));
		    return 0;
		}
	    }
	}
    }

    if (!arc2_circle_set) {
	//std::cout << "True cone!\n";
	data->type = CONE;
	data->obj_cnt = data->parent->obj_cnt;
	(*data->obj_cnt)++;
	bu_vls_sprintf(data->name_root, "%s_%d_cone", bu_vls_addr(data->parent->name_root), *(data->obj_cnt));


	ON_3dVector hvect(cone.ApexPoint() - cone.BasePoint());
	ON_3dPoint closest_to_base = set1_c.Plane().ClosestPointTo(cone.BasePoint());
	struct csg_object_params * obj;
        BU_GET(obj, struct csg_object_params);

	data->negative_shape = negative_cone(data, *conic_surfaces.begin(), cone_tol);

	data->params->bool_op = (data->negative_shape == -1) ? '-' : 'u';
	data->params->origin[0] = closest_to_base.x;
	data->params->origin[1] = closest_to_base.y;
	data->params->origin[2] = closest_to_base.z;
	data->params->hv[0] = hvect.x;
	data->params->hv[1] = hvect.y;
	data->params->hv[2] = hvect.z;
	data->params->radius = set1_c.Radius();
	data->params->r2 = 0.000001;
	data->params->height = set1_c.Plane().DistanceTo(cone.ApexPoint());
	if (data->params->height < 0) data->params->height = data->params->height * -1;
	return 1;
    } else {
	ON_3dPoint base = set1_c.Center();
	ON_3dVector hvect = set2_c.Center() - set1_c.Center();

	int *corner_verts_array = NULL;
	ON_Plane pcone;
	std::set<int> corner_verts; /* verts with one nonlinear edge */
	int corner_verts_cnt = subbrep_find_corners(data, &corner_verts_array, &pcone);

	if (corner_verts_cnt == -1) return 0;
	if (corner_verts_cnt > 0) {
	    array_to_set(&corner_verts, corner_verts_array, corner_verts_cnt);
	    bu_free(corner_verts_array, "free tmp array");
	    if (msgs) bu_vls_printf(msgs, "%*sFound partial TGC!\n", L3_OFFSET, " ");
	}

	if (corner_verts_cnt == 0) {
	    // Full TGC cone
	    struct csg_object_params * obj;
	    BU_GET(obj, struct csg_object_params);
	    data->type = CONE;
	    data->obj_cnt = data->parent->obj_cnt;
	    (*data->obj_cnt)++;
	    bu_vls_sprintf(data->name_root, "%s_%d_cone", bu_vls_addr(data->parent->name_root), *(data->obj_cnt));

	    data->negative_shape = negative_cone(data, *conic_surfaces.begin(), cone_tol);

	    data->params->bool_op = (data->negative_shape == -1) ? '-' : 'u';
	    data->params->origin[0] = base.x;
	    data->params->origin[1] = base.y;
	    data->params->origin[2] = base.z;
	    data->params->hv[0] = hvect.x;
	    data->params->hv[1] = hvect.y;
	    data->params->hv[2] = hvect.z;
	    data->params->radius = set1_c.Radius();
	    data->params->r2 = set2_c.Radius();
	    data->params->height = set1_c.Center().DistanceTo(set2_c.Center());
	    if (data->params->height < 0) data->params->height = data->params->height * -1;

	    return 1;
	} else {
	    // Have corners, need arb
	    data->type = COMB;
	    data->obj_cnt = data->parent->obj_cnt;
	    (*data->obj_cnt)++;
	    bu_vls_sprintf(data->name_root, "%s_%d_comb", bu_vls_addr(data->parent->name_root), *(data->obj_cnt));

	    data->negative_shape = negative_cone(data, *conic_surfaces.begin(), cone_tol);
	    data->params->bool_op = (data->negative_shape == -1) ? '-' : 'u';

	    struct subbrep_object_data *cone_obj;
	    BU_GET(cone_obj, struct subbrep_object_data);
	    subbrep_object_init(cone_obj, data->brep);
	    std::string key = face_set_key(conic_surfaces);
	    bu_vls_sprintf(cone_obj->key, "%s", key.c_str());
	    cone_obj->obj_cnt = data->parent->obj_cnt;
	    (*cone_obj->obj_cnt)++;
	    //bu_log("obj_cnt: %d\n", *(cone_obj->obj_cnt));
	    bu_vls_sprintf(cone_obj->name_root, "%s_%d_cone", bu_vls_addr(data->parent->name_root), *(cone_obj->obj_cnt));
	    cone_obj->type = CONE;

	    // cone - positive object in this sub-comb
	    cone_obj->params->bool_op = 'u';
	    cone_obj->params->origin[0] = base.x;
	    cone_obj->params->origin[1] = base.y;
	    cone_obj->params->origin[2] = base.z;
	    cone_obj->params->hv[0] = hvect.x;
	    cone_obj->params->hv[1] = hvect.y;
	    cone_obj->params->hv[2] = hvect.z;
	    cone_obj->params->radius = set1_c.Radius();
	    cone_obj->params->r2 = set2_c.Radius();
	    cone_obj->params->height = set1_c.Center().DistanceTo(set2_c.Center());

	    bu_ptbl_ins(data->children, (long *)cone_obj);

            // arb8 - subtracted from the previous cone in this sub-comb

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
	    arb_obj->obj_cnt = data->parent->obj_cnt;
	    (*arb_obj->obj_cnt)++;
	    bu_vls_sprintf(arb_obj->name_root, "%s_%d_arb8", bu_vls_addr(data->parent->name_root), *(arb_obj->obj_cnt));
            arb_obj->type = ARB8;


            // First, find the two points closest to the set1_c and set2_c planes
            ON_SimpleArray<const ON_BrepVertex *> bottom_pnts;
            ON_SimpleArray<const ON_BrepVertex *> top_pnts;
            ON_Plane b_plane = set1_c.Plane();
            ON_Plane t_plane = set2_c.Plane();
            if (subbrep_top_bottom_pnts(data, &corner_verts, &t_plane, &b_plane, &top_pnts, &bottom_pnts)) {
                if (msgs) bu_vls_printf(msgs, "%*sPoint top/bottom sorting failed: %s\n", L3_OFFSET, " ", bu_vls_addr(arb_obj->key));
                return 0;
            }

            // Second, select a point from an arc edge not on the subtraction
            // plane, construct a vector from the circle center to that point,
            // and determine if the pcone plane direction is already in the opposite
            // direction or needs to be reversed.
            const ON_BrepEdge *edge = &(data->brep->m_E[*arc_set_1.begin()]);
            ON_Arc arc;
            ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
            (void)ecv->IsArc(NULL, &arc, cone_tol);
            delete ecv;
            ON_3dPoint center = set1_c.Center();
            ON_3dPoint midpt = arc.MidPoint();

            ON_3dVector invec = center - midpt;
            double dotp = ON_DotProduct(invec, pcone.Normal());
            if (dotp < 0) {
                pcone.Flip();
            }

            // Third, construct the axis vector and determine the arb
            // order of the bottom and top points
            ON_3dVector cone_axis = set2_c.Center() - set1_c.Center();

            ON_3dVector vv1 = bottom_pnts[0]->Point() - bottom_pnts[1]->Point();
            ON_3dVector v1x = ON_CrossProduct(vv1, cone_axis);

            double flag1 = ON_DotProduct(v1x, pcone.Normal());

            ON_3dVector w1 = top_pnts[0]->Point() - top_pnts[1]->Point();
            ON_3dVector w1x = ON_CrossProduct(w1, cone_axis);

            double flag3 = ON_DotProduct(w1x, pcone.Normal());

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
                // The coneinder shape is a partial coneinder, and it is not
                // topologically isolated, so we need to make a new face
                // in the parent to replace this one.
                if (data->parent) {
                    // First, see if a local planar brep object has been generated for
                    // this shape.  Such a brep is not generated up front, because
                    // there is no way to be sure a planar parent is needed until
                    // we hit a case like this - for example, a brep consisting of
                    // stacked coneinders of different diameters will have multiple
                    // planar surfaces, but can be completely described without use
                    // of planar volumes in the final CSG expression.  If another
                    // shape has already triggered the generation we don't want to
                    // do it again,  but the first shape to make the need clear has
                    // to trigger the build.
                    if (!data->parent->planar_obj) {
                        subbrep_planar_init(data->parent);
                    }
                    // Now, add the new face
                    ON_SimpleArray<const ON_BrepVertex *> vert_loop(4);
                    vert_loop.Append(v1);
                    vert_loop.Append(v2);
                    vert_loop.Append(v3);
                    vert_loop.Append(v4);
                    subbrep_add_planar_face(data->parent, &pcone, &vert_loop, data->negative_shape);
                }
            }



            // Once the 1,2,3,4 points are determined, scale them out
            // along their respective line segment axis to make sure
            // the resulting arb is large enough to subtract the full
            // radius of the coneinder.
            //
            // TODO - Only need to do this if the
            // center point of the coneinder is inside the subtracting arb -
            // should be able to test that with the circle center point
            // a distance to pcone plane calculation for the second point,
            // then subtract the center from the point on the plane and do
            // a dot product test with pcone's normal.
            //
            // TODO - Can optimize this - can make a narrower arb using
            // the knowledge of the distance between p1/p2.  We only need
            // to add enough extra length to clear the coneinder, which
	    // means the full radius length is almost always overkill.
	    double max_radius = set1_c.Radius();
	    if (set2_c.Radius() > max_radius) max_radius = set2_c.Radius();
	    ON_SimpleArray<ON_3dPoint> arb_points(8);
	    arb_points[0] = v1->Point();
            arb_points[1] = v2->Point();
            arb_points[2] = v3->Point();
            arb_points[3] = v4->Point();
            vv1.Unitize();
            vv1 = vv1 * max_radius;
            arb_points[0] = arb_points[0] + vv1;
            arb_points[1] = arb_points[1] - vv1;
            arb_points[2] = arb_points[2] - vv1;
            arb_points[3] = arb_points[3] + vv1;
            ON_3dVector hpad = arb_points[2] - arb_points[1];
            hpad.Unitize();
            hpad = hpad * (cone_axis.Length() * 0.01);
            arb_points[0] = arb_points[0] - hpad;
            arb_points[1] = arb_points[1] - hpad;
            arb_points[2] = arb_points[2] + hpad;
            arb_points[3] = arb_points[3] + hpad;

            // Once the final 1,2,3,4 points have been determined, use
            // the pcone normal direction and the coneinder radius to
            // construct the remaining arb points.
            ON_3dPoint p5, p6, p7, p8;
            ON_3dVector arb_side = pcone.Normal() * 2*max_radius;
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
    }
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
