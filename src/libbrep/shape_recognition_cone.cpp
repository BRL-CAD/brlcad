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
    std::set<int> active_edges;
    // First, check surfaces.  If a surface is anything other than planes or cones,
    // the verdict is no.  If we don't have one planar surface and one or more
    // conic surfaces, the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
	ON_BrepFace *used_face = &(data->brep->m_F[f_ind]);
        int surface_type = (int)GetSurfaceType(used_face->SurfaceOf(), NULL);
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
    std::set<int> degenerate;
    for (int i = 0; i < data->edges_cnt; i++) {
	int e_it = data->edges[i];
	if (degenerate.find(e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (int j = 0; j < data->edges_cnt; j++) {
		    int f_ind = data->edges[j];
		    ON_BrepEdge& edge2 = data->brep->m_E[f_ind];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(e_it);
			    degenerate.insert(f_ind);
			    break;
			}
		    }
		}
	    }
	}
    }
    for (int i = 0; i < data->edges_cnt; i++) {
	active_edges.insert(data->edges[i]);
    }
    std::set<int>::iterator e_it;
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
        //std::cout << "erasing " << *e_it << "\n";
        active_edges.erase(*e_it);
    }


    // Sixth, check for any remaining linear segments.  If we have a real
    // cone and not just a partial, all the linear segments should have
    // washed out.
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        if (edge.EdgeCurveOf()->IsLinear()) return 0;
    }

    // Seventh, make sure all the curved edges are on the same circle.
    // TODO - this is only for a true cone object - a TGC, which can also
    // be handled here, will have parallel circles
    ON_Circle circle;
    int circle_set= 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        ON_Arc arc;
        if (edge.EdgeCurveOf()->IsArc(NULL, &arc, 0.01)) {
            ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
            if (!circle_set) {
                circle_set = 1;
                circle = circ;
            }
            if (!NEAR_ZERO(circ.Center().DistanceTo(circle.Center()), 0.01)){
                std::cout << "found extra circle - no go\n";
                return 0;
            }
        }
    }

    data->type = CONE;

    ON_3dVector hvect(cone.ApexPoint() - cone.BasePoint());

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
    bu_log("process partial cone\n");
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
