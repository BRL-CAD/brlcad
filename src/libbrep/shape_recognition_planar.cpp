#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"


int
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	if (GetSurfaceType(data->brep->m_F[data->faces[i]].SurfaceOf(), NULL) != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;
    return 1;
}

// TODO - implement tests to recognize arb4-arb8 and valid arbn (no concave face) primitives

// Returns true if point set forms a convex polyhedron, and false otherwise
bool
convex_point_set(struct subbrep_object_data *data, std::set<int> *verts)
{
    // Use chull3d to find the set of vertices that are on the convex
    // hull.  If all of them are, the point set defines a convex polyhedron.
    // If the points are coplanar, return false - not a volume
}


int
point_set_is_arb4(struct subbrep_object_data *data, std::set<int> *verts)
{
}
int
point_set_is_arb5(struct subbrep_object_data *data, std::set<int> *verts)
{
}
int
point_set_is_arb6(struct subbrep_object_data *data, std::set<int> *verts)
{
}
int
point_set_is_arb7(struct subbrep_object_data *data, std::set<int> *verts)
{
}
int
point_set_is_arb8(struct subbrep_object_data *data, std::set<int> *verts)
{
}
int
point_set_is_arbn(struct subbrep_object_data *data, std::set<int> *verts)
{
}

// In the worst case, make a brep for later conversion into an nmg.
// The other possibility here is an arbn csg tree, but that needs
// more thought...
int
subbrep_make_planar_brep(struct subbrep_object_data *data)
{
}

int
subbrep_make_planar(struct subbrep_object_data *data)
{
    // First step is to count vertices, using the edges
    std::set<int> subbrep_verts;
    for (int i = 0; i < data->edges_cnt; i++) {
	const ON_BrepEdge *edge = &(data->brep->m_E[i]);
	subbrep_verts.insert(edge->Vertex(0)->m_vertex_index);
	subbrep_verts.insert(edge->Vertex(1)->m_vertex_index);
    }

    int vert_cnt = subbrep_verts.size();
    switch (vert_cnt) {
	case 0:
	    std::cout << "no verts???\n";
	    return 0;
	    break;
	case 1:
	    std::cout << "one vertex - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 2:
	    std::cout << "two vertices - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 3:
	    std::cout << "three vertices - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 4:
	    if (!point_set_is_arb4(data, &subbrep_verts)) {
		return subbrep_make_planar_brep(data);
	    }
	    return 1;
	    break;
	case 5:
	    if (!point_set_is_arb5(data, &subbrep_verts)) {
		if (!point_set_is_arbn(data, &subbrep_verts)) {
		    return subbrep_make_planar_brep(data);
		}
	    }
	    return 1;
	    break;
	case 6:
	    if (!point_set_is_arb6(data, &subbrep_verts)) {
		if (!point_set_is_arbn(data, &subbrep_verts)) {
		    return subbrep_make_planar_brep(data);
		}
	    }
	    return 1;
	    break;
	case 7:
	    if (!point_set_is_arb7(data, &subbrep_verts)) {
		if (!point_set_is_arbn(data, &subbrep_verts)) {
		    return subbrep_make_planar_brep(data);
		}
	    }
	    return 1;
	    break;
	case 8:
	    if (!point_set_is_arb8(data, &subbrep_verts)) {
		if (!point_set_is_arbn(data, &subbrep_verts)) {
		    return subbrep_make_planar_brep(data);
		}
	    }
	    return 1;
	    break;
	default:
	    if (!point_set_is_arbn(data, &subbrep_verts)) {
		return subbrep_make_planar_brep(data);
	    }
	    return 1;
	    break;

    }
    return 0;
}


// TODO - need to check for self-intersecting planar objects - any
// such object needs to be further deconstructed, since it has
// components that may be making both positive and negative contributions


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
