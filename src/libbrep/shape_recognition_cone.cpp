#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"


int
subbrep_is_cone(struct subbrep_object_data *data, fastf_t cone_tol)
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
        std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        std::cout << "cone axis: " << cone.Axis().x << "," << cone.Axis().y << "," << cone.Axis().z << "\n";
        return 0;
    }


    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    // Fifth, remove degenerate edge sets.
    std::set<int> active_edges;
    std::set<int>::iterator e_it;
    array_to_set(&active_edges, data->edges, data->edges_cnt);
    subbrep_remove_degenerate_edges(data, &active_edges);

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
                std::cout << "found extra circle - no go\n";
		delete ecurve;
                return 0;
            }
        }
	delete ecurve;
    }

    data->type = CONE;

    ON_3dVector hvect(cone.ApexPoint() - cone.BasePoint());

    data->params->bool_op = 'u';  //TODO - not always a union - need to determine positive/negative
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
cone_csg(struct subbrep_object_data *data, fastf_t cone_tol)
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
		    std::cout << "found extra circle - no go\n";
		    return 0;
		}
	    }
	}
    }

    if (!arc2_circle_set) {
	std::cout << "True cone!\n";
	data->type = CONE;

	ON_3dVector hvect(cone.ApexPoint() - cone.BasePoint());
	ON_3dPoint closest_to_base = set1_c.Plane().ClosestPointTo(cone.BasePoint());
	struct csg_object_params * obj;
        BU_GET(obj, struct csg_object_params);
	data->params->bool_op = 'u';  //TODO - not always a union - need to determine positive/negative
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

    } else {
	std::cout << "TGC!\n";
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
