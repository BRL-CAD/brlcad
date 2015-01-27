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
    std::set<int> active_edges;
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
    //std::cout << "Active Edge set: ";
#if 0
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        std::cout << (int)(*e_it);
        f_it = e_it;
        f_it++;
        if (f_it != active_edges.end()) std::cout << ",";
    }
    std::cout << "\n";
#endif

    // Sixth, check for any remaining linear segments.  For rpc primitives
    // those are expected, but for a true cylinder the linear segments should
    // all wash out in the degenerate pass.
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
cylinder_csg(struct subbrep_object_data *data)
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
    // Second, get cylindrical surface properties.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder);
    delete cs;

    // Third, characterize the edges.  The sets of coplanar non-linear
    // edges may either form loops  or arcs.  The former case does not
    // require arb subtraction, the latter does.

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
