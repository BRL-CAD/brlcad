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


// TODO - need a way to detect if a set of planar faces would form a
// self-intersecting polyhedron.  Self-intersecting polyhedrons are the ones
// with the potential to make both positive and negative contributions to a
// solid. One possible idea:
//
// For all edges in polyhedron test whether each edge intersects any face in
// the polyhedron to which it does not belong.  The test can be simple - for
// each vertex, construct a vector from the vertex to some point on the
// candidate face (center point is probably a good start) and see if the dot
// products of those two vectors with the face normal vector agree in sign.
// For a given edge, if all dot products agree in pair sets, then the edge does
// not intersect any face in the polyhedron.  If no edges intersect, the
// polyhedron is not self intersecting.  If some edges do intersect (probably
// at least three...) then those edges identify sub-shapes that need to be
// constructed.


// Returns 1 if point set forms a convex polyhedron, 0 if the point set
// forms a degenerate chull, and -1 if the point set is concave
int
convex_point_set(struct subbrep_object_data *data, std::set<int> *verts)
{
    // Use chull3d to find the set of vertices that are on the convex
    // hull.  If all of them are, the point set defines a convex polyhedron.
    // If the points are coplanar, return 0 - not a volume
    return 0;
}

/* These functions will return 2 if successful, 1 if unsuccessful but point set
 * is convex, 0 if unsuccessful and vertex set's chull is degenerate (i.e. the
 * planar component of this CSG shape contributes no positive volume),  and -1
 * if unsuccessful and point set is neither degenerate nor convex */
int
point_set_is_arb4(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);
    if (is_convex == 1) {
	// TODO - deduce and set up proper arb4 point ordering
	return 2;
    }
    return 0;
}
int
point_set_is_arb5(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb5 test
    return 0;
}
int
point_set_is_arb6(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb6 test
    return 0;
}
int
point_set_is_arb7(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb7 test
    return 0;
}
int
point_set_is_arb8(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb8 test
    return 0;
}

/* If we're going with an arbn, we need to add one plane for each face.  To
 * make sure the normal is in the correct direction, find the center point of
 * the verts and the center point of the face verts to construct a vector which
 * can be used in a dot product test with the face normal.*/
int
point_set_is_arbn(struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts, int do_test)
{
    int is_convex;
    if (!do_test) {
	is_convex = 1;
    } else {
	is_convex = convex_point_set(data, verts);
    }
    if (!is_convex == 1) return is_convex;

    // TODO - arbn assembly
    return 2;
}

// In the worst case, make a brep for later conversion into an nmg.
// The other possibility here is an arbn csg tree, but that needs
// more thought...
int
subbrep_make_planar_brep(struct subbrep_object_data *data)
{
    // TODO - check for self intersections in the candidate shape, and handle
    // if any are discovered.
    return 0;
}

int
planar_switch(int ret, struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts)
{
    switch (ret) {
	case -1:
	    return subbrep_make_planar_brep(data);
	    break;
	case 0:
	    return 0;
	    break;
	case 1:
	    return point_set_is_arbn(data, faces, verts, 0);
	    break;
	case 2:
	    return 1;
	    break;
    }
    return 0;
}


int
subbrep_make_planar(struct subbrep_object_data *data)
{
    // First step is to count vertices, using the edges
    std::set<int> subbrep_verts;
    std::set<int> faces;
    for (int i = 0; i < data->edges_cnt; i++) {
	const ON_BrepEdge *edge = &(data->brep->m_E[i]);
	subbrep_verts.insert(edge->Vertex(0)->m_vertex_index);
	subbrep_verts.insert(edge->Vertex(1)->m_vertex_index);
    }
    array_to_set(&faces, data->faces, data->faces_cnt);

    int vert_cnt = subbrep_verts.size();
    int ret = 0;
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
	    if (point_set_is_arb4(data, &subbrep_verts) != 2) {
		return 0;
	    }
	    return 1;
	    break;
	case 5:
	    ret = point_set_is_arb5(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 6:
	    ret = point_set_is_arb6(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 7:
	    ret = point_set_is_arb7(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 8:
	    ret = point_set_is_arb8(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	default:
	    ret = point_set_is_arbn(data, &faces, &subbrep_verts, 1);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
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
