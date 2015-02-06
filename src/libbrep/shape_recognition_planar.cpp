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



void
subbrep_planar_init(struct subbrep_object_data *sdata)
{
    struct subbrep_object_data *data = sdata->parent;

    if (!data) return;
    if (data->planar_obj) return;
    BU_GET(data->planar_obj, struct subbrep_object_data);
    subbrep_object_init(data->planar_obj, data->brep);
    bu_vls_sprintf(data->planar_obj->key, "%s_planar", bu_vls_addr(data->key));


    data->planar_obj->local_brep = ON_Brep::New();
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> loop_map;
    std::map<int, int> c3_map;
    std::map<int, int> c2_map;
    std::map<int, int> trim_map;
    std::set<int> faces;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int> isolated_trims;  // collect 2D trims whose parent loops aren't fully included here
    array_to_set(&faces, data->faces, data->faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);
    array_to_set(&loops, data->loops, data->loops_cnt);

    for (int i = 0; i < data->edges_cnt; i++) {
        int c3i;
        const ON_BrepEdge *old_edge = &(data->brep->m_E[data->edges[i]]);
        //std::cout << "old edge: " << old_edge->Vertex(0)->m_vertex_index << "," << old_edge->Vertex(1)->m_vertex_index << "\n";

        // Get the 3D curves from the edges
        if (c3_map.find(old_edge->EdgeCurveIndexOf()) == c3_map.end()) {
            ON_Curve *nc = old_edge->EdgeCurveOf()->Duplicate();
	    // Don't continue unless the edge is linear
            ON_Curve *tc = old_edge->EdgeCurveOf()->Duplicate();
	    if (tc->IsLinear()) {
		c3i = data->planar_obj->local_brep->AddEdgeCurve(nc);
		c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
	    } else {
		continue;
	    }
        } else {
            c3i = c3_map[old_edge->EdgeCurveIndexOf()];
        }



        // Get the vertices from the edges
        int v0i, v1i;
        if (vertex_map.find(old_edge->Vertex(0)->m_vertex_index) == vertex_map.end()) {
            ON_BrepVertex& newv0 = data->planar_obj->local_brep->NewVertex(old_edge->Vertex(0)->Point(), old_edge->Vertex(0)->m_tolerance);
            v0i = newv0.m_vertex_index;
            vertex_map[old_edge->Vertex(0)->m_vertex_index] = v0i;
        } else {
            v0i = vertex_map[old_edge->Vertex(0)->m_vertex_index];
        }
        if (vertex_map.find(old_edge->Vertex(1)->m_vertex_index) == vertex_map.end()) {
            ON_BrepVertex& newv1 = data->planar_obj->local_brep->NewVertex(old_edge->Vertex(1)->Point(), old_edge->Vertex(1)->m_tolerance);
            v1i = newv1.m_vertex_index;
            vertex_map[old_edge->Vertex(1)->m_vertex_index] = v1i;
        } else {
            v1i = vertex_map[old_edge->Vertex(1)->m_vertex_index];
        }
        ON_BrepEdge& new_edge = data->planar_obj->local_brep->NewEdge(data->planar_obj->local_brep->m_V[v0i], data->planar_obj->local_brep->m_V[v1i], c3i, NULL ,0);
        edge_map[old_edge->m_edge_index] = new_edge.m_edge_index;
        //std::cout << "new edge: " << v0i << "," << v1i << "\n";

        // Get the 2D curves from the trims
        for (int j = 0; j < old_edge->TrimCount(); j++) {
            ON_BrepTrim *old_trim = old_edge->Trim(j);
            if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
                if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
                    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
                    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
                    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
                    //std::cout << "c2i: " << c2i << "\n";
                }
            }
        }

        // Get the faces and surfaces from the trims
        for (int j = 0; j < old_edge->TrimCount(); j++) {
            ON_BrepTrim *old_trim = old_edge->Trim(j);
            if (face_map.find(old_trim->Face()->m_face_index) == face_map.end()) {
                if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
                    ON_Surface *ns = old_trim->Face()->SurfaceOf()->Duplicate();
                    ON_Surface *ts = old_trim->Face()->SurfaceOf()->Duplicate();
		    if (ts->IsPlanar(NULL, BREP_PLANAR_TOL)) {
			int nsid = data->planar_obj->local_brep->AddSurface(ns);
			surface_map[old_trim->Face()->SurfaceIndexOf()] = nsid;
			ON_BrepFace &new_face = data->planar_obj->local_brep->NewFace(nsid);
			face_map[old_trim->Face()->m_face_index] = new_face.m_face_index;
			if (fil.find(old_trim->Face()->m_face_index) != fil.end()) {
			    data->planar_obj->local_brep->FlipFace(new_face);
			}
		    }
                }
            }
        }

        // Get the loops from the trims
        for (int j = 0; j < old_edge->TrimCount(); j++) {
            ON_BrepTrim *old_trim = old_edge->Trim(j);
            ON_BrepLoop *old_loop = old_trim->Loop();
            if (face_map.find(old_trim->Face()->m_face_index) != face_map.end()) {
                if (loops.find(old_loop->m_loop_index) != loops.end()) {
                    if (loop_map.find(old_loop->m_loop_index) == loop_map.end()) {
                        // After the initial breakout, all loops in any given subbrep are outer loops,
                        // whatever they were in the original brep.
                        ON_BrepLoop &nl = data->planar_obj->local_brep->NewLoop(ON_BrepLoop::outer, data->planar_obj->local_brep->m_F[face_map[old_loop->m_fi]]);
                        loop_map[old_loop->m_loop_index] = nl.m_loop_index;
                    }
                } 
            }
        }
    }

    // Now, create new trims using the old loop definitions and the maps
    std::map<int, int>::iterator loop_it;
    for (loop_it = loop_map.begin(); loop_it != loop_map.end(); loop_it++) {
        const ON_BrepLoop *old_loop = &(data->brep->m_L[(*loop_it).first]);
        ON_BrepLoop &new_loop = data->planar_obj->local_brep->m_L[(*loop_it).second];
        for (int j = 0; j < old_loop->TrimCount(); j++) {
            const ON_BrepTrim *old_trim = old_loop->Trim(j);
            ON_BrepEdge *o_edge = old_trim->Edge();
            if (o_edge) {
                ON_BrepEdge &n_edge = data->planar_obj->local_brep->m_E[edge_map[o_edge->m_edge_index]];
                ON_BrepTrim &nt = data->planar_obj->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2_map[old_trim->TrimCurveIndexOf()]);
                nt.m_tolerance[0] = old_trim->m_tolerance[0];
                nt.m_tolerance[1] = old_trim->m_tolerance[1];
                nt.m_iso = old_trim->m_iso;
            } else {
                /* If we didn't have an edge originally, we need to add the 2d curve here */
                if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
                    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
                    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
                    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
                }
                if (vertex_map.find(old_trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
                    ON_BrepVertex& newvs = data->planar_obj->local_brep->NewVertex(old_trim->Vertex(0)->Point(), old_trim->Vertex(0)->m_tolerance);
		    vertex_map[old_trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;
                    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(newvs, new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
                    nt.m_tolerance[0] = old_trim->m_tolerance[0];
                    nt.m_tolerance[1] = old_trim->m_tolerance[1];
                } else {
                    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(data->planar_obj->local_brep->m_V[vertex_map[old_trim->Vertex(0)->m_vertex_index]], new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
                    nt.m_tolerance[0] = old_trim->m_tolerance[0];
                    nt.m_tolerance[1] = old_trim->m_tolerance[1];
                }
            }
        }
    }

    // Need to preserve the vertex map for this, since we're not done building up the brep
    map_to_array(&(data->planar_obj_vert_map), &(data->planar_obj_vert_cnt), &vertex_map);
}


int subbrep_add_planar_face(struct subbrep_object_data *data, ON_Plane *pcyl,
       	ON_SimpleArray<const ON_BrepVertex *> *vert_loop)
{
    // We use the planar_obj's local_brep to store new faces.  The planar local
    // brep contains the relevant linear and planar components from its parent
    // - our job here is to add the new surface, identify missing edges to
    // create, find existing edges to re-use, and call NewFace with the
    // results.  At the end we should have just the faces needed
    // to define the planar volume of interest.

    // First step - find or create ON_BrepEdge objects using the vert_loop.  
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
//
// Note that this is also a concern for non-planar surfaces that have been
// reduced to planar surfaces as part of the process - probably should
// incorporate a bounding box test to determine if such surfaces can be
// part of sub-object definitions (say, a cone subtracted from a subtracting
// cylinder to make a positive cone volume inside the cylinder) or if the
// cone has to be a top level unioned positive object (if the cone's apex
// point is outside the cylinder's subtracting area, for example, the tip
// of the code will not be properly added as a positive volume if it is just
// a subtraction from the cylinder.


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
