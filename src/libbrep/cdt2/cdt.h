/*                        C D T . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libbrep */
/** @{ */
/** @file cdt.h
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */


/*
Design thoughts:

When performing triangle insertions from the CDT, can use the ordered
edges and unordered edges to valid the triangles as they go in, rather
than trying to do it after the fact:

1.  Any triangle with two vertices on a singularity is degenerate - skip.

2.  Before inserting a triangle into the mesh, make sure all three triangle
edges would reference unique unordered edges in 3D.  We've seen at least one
instance of a 2D triangle that ended up using the same interior edge for 2 of
its edges - that's a degenerate triangle, just filter it out.

3.  For a proposed insertion triangle, check it's three edges against the
unordered edge tree.   RTree uedge lookup will be used for this - in principle
it should be quicker than a std::map lookup by taking advantage of the spatial
grouping of the mesh.  There should be at least one pre existing edge (from the
mesh boundary if not from an already inserted triangle) that is already in the
containers and will be linked to the new triangle.  Check that unordered edge's
associated ordered edge that doesn't have an associated triangle against the
new triangle edges -  if there's not a match, flip the triangle ordering and
try again.

What this will do is force all triangles in 3D to form a valid topology,
regardless of whether or not the original CDT triangles would have formed such
a topology if naively translated.  Then we can check the triangle normals
against the brep normals and locally address those problem areas, but without
unmated edges being one of the potential issues.  I.e. the mesh will stay
as "valid" as possible in BoT terms at all times, even during the initial build.


During the initial edge breakdown, split all curved edges to a 45 degree normal
tolerance at an absolute minimum.  Do the same for the surface.  Drive curve
refinement using the size of the surface patches overlapping the edges, and
vice versa - needs to be handled so we don't get infinite looping, but the idea
is that curves, surfaces, and otherwise unsplit curves and surfaces in the
vicinity of curves and surfaces needing splitting will be (locally) refined
consistently.  Should terminate with those curve/surface areas that need it
(and only those) refined per tolerance settings, or to minimal standard.
Hopefully that will avoid the need for the various special case rules currently
defined based on linear/non-linear pairings and such, which don't always do
what we want/need locally in any event.

*/


#include "common.h"

#include <unordered_map>
#include <queue>
#include <string>

#include "../cdt/RTree.h"
#include "vmath.h"
#include "bn/tol.h"
#include "brep/defines.h"
#include "brep/cdt2.h"

#define BREP_PLANAR_TOL 0.05

class mesh_point_t;
class mesh_edge_t;
class mesh_uedge_t;
class mesh_tri_t;
class mesh_t;
class poly_edge_t;
class polygon_t;

///////////////////////////////////////////////////////////////////////////////
//
//  Point containers - hold 2D and 3D points used in mesh processing, as well
//  as associated metadata and edge connectivity information.
//
///////////////////////////////////////////////////////////////////////////////


typedef enum {
    B_VERT,
    B_EDGE,
    B_SURF
} mesh_point_type;


class poly_point_t {
    public:
	// Associated 3D point used to derive this 2D point.
	mesh_point_t *mp = NULL;
	long vect_ind = -1;
	double u;
	double v;
	std::set<size_t> pedges;
};

class mesh_point_t {
    public:
	// Routine to update point bounding box in light of updates to
	// associated edges.
	void update();

	// Moves the point to the new value, sets the new normal, and updates
	// all impacted data (vert bboxes, edge bboxes and lengths, triangle
	// bboxes, RTrees, etc.) using edge connectivity to find everything
	// that needs updating.
	//
	// This operation (and only this operation) may also change associated
	// 2D polygon points and the bboxes of associated polygon edges.
	void adjust(ON_3dPoint &np, ON_3dVector *nn);

	// Connect/disconnect edges to this point, updating any
	// associated information that needs updating
	void connect(poly_edge_t *ue);
	void connect(mesh_uedge_t *ue);
	void disconnect(poly_edge_t *ue);
	void disconnect(mesh_uedge_t *ue);

	// Primary 3D point and normal information
	ON_3dPoint p;
	ON_3dVector n;

	// Metadata from BRep indicating what type of point this is
	mesh_point_type type;

	// Origin information - specifics about what vertex, edge or surface on the
	// mesh is connected to this particular point.  Which set of information is
	// valid for this particular point depends on its type.
	// B_VERT
	int vert_index;
	bool singular;
	// B_EDGE
	int trim_index;
	int edge_index;
	double t;
	// B_SURF
	int face_index;
	double u;
	double v;

	// Variables used to store the associated index when assembling BoT
	// meshes
	int bot_vert_ind = -1;
	int bot_norm_ind = -1;

	// Index of this container in b_pnts vector.  Used primarily
	// to allow RTree box lookups to identify specific points.
	size_t vect_ind;

	// Topology information: associated object and edges
	struct brep_cdt *cdt;
	std::set<poly_point_t *> poly_pts;
	std::set<mesh_uedge_t *> uedges;

	// Bounding box around the vertex point, with sized defined based
	// on connected edge lengths.
	ON_BoundingBox bb;
};

///////////////////////////////////////////////////////////////////////////////
//
//  Edge containers - hold 2D and 3D edges used in mesh processing, as well
//  as associated metadata.
//
///////////////////////////////////////////////////////////////////////////////

typedef enum {
    B_UNSET,
    B_SINGULAR,
    B_BOUNDARY,
    B_INTERIOR
} edge_type_t;


class poly_edge_t {
    public:

	// Routine to update information associated with this edge in case
	// supporting points have been updated.
	void update();

	// The polygon context in which this edge is defined
	polygon_t *polygon = NULL;

	// Index identifying the current edge's position in the
	// polygon->p_pedges_vect array
	int vect_ind = -1;

	// Indices into the associated polygon's p_pnts_vect array that define
	// the start v[0] and end v[1] points of the polyedge in 2D.
	long v[2] = {-1, -1};

	// Indices into the associated polygon's p_pedges_vect array that
	// identify the previous and next edges in the polygon loop. 
	int prev = -1;
	int next = -1;

	// Pointer to the 3D ordered edge associated with this polygon edge.
	mesh_edge_t *e3d = NULL;

	// BRep metadata specific to this edge
	edge_type_t type = B_UNSET;
	int m_trim_index = -1;
	bool m_bRev3d = false;

	// When performing refinement searches in 2D, we need to track the
	// splitting status of polyedges.  The least complicated way to do
	// that is to provide a means of tagging the edge itself.
	int split_status = 0;

	// 2D bounding box containing this polyedge in the polygon plane
	ON_BoundingBox bb;

	// Called when a poly_edge_t is returned to the
	// queue for reuse
	void reset();
};

// The ordered 3D edge generally stores minimal information. Users should refer
// to the associated unordered edge for information that is not specific to the
// ordered version of the edge.
class mesh_edge_t {
    public:

	// Mesh context in which the 3D edge is defined.  Unlike unordered
	// edges, ordered edges are always specific to a mesh context.
	mesh_t *m = NULL;

	// Index of this edge in m_edges_vect
	int vect_ind = -1;

	// b_pnts indices of points defining the edge
	long v[2] = {-1, -1};

	// Type of edge
	edge_type_t type = B_UNSET;

	// Associated uedge.  All 3D edges should have an associated uedge
	mesh_uedge_t *uedge = NULL;

	// Associated polygon edge.  Not all 3D edges will have an associated
	// polygon edge, but all those involved with a BRep face edge should.
	poly_edge_t *pe = NULL;

	// Post CDT, all 3D edges should have exactly one associated triangle.
	mesh_tri_t *tri = NULL;

	// Called when a mesh_edge_t is returned to the
	// queue for reuse
	void reset();
};

// An unordered edge (except in cases where the BRep is not closed) should
// reference two triangles via two ordered edges.  Unordered edges are the
// center of mesh processing and refinement, as only they have the necessary
// information to update all relevant portions of the mesh when changes are
// made.
class mesh_uedge_t {
    public:

	// Routine to update information associated with this edge in case
	// supporting points have been updated.
	void update();

	// Split the uedge at the closest edge or surface point to the supplied
	// 3D point.  This method will invalidate the current edge (placing it
	// on the queue for reuse) and produce two new edges if successful -
	// otherwise it will return a NULL pair and leave the current edge
	// intact.  A successful uedge split will propagate changes down to
	// ordered edges and polygon edges as well.
	std::pair<mesh_uedge_t *, mesh_uedge_t *> split(ON_3dPoint &p);

	// b_pnts indices denoting the original ordered points associated with
	// the BRep edge start and end points of the parent BRep edge, if this
	// edge is derived from a BRep edge.
	long edge_pnts[2] = {-1, -1};

	// b_pnts indices of points defining the edge.  Unlike an ordered
	// edge, these will be sorted so that v[0] <= v[1]
	long v[2] = {-1, -1};

	// Index of this edge in m_uedges_vect
	int vect_ind = -1;

	// Type of edge
	edge_type_t type = B_UNSET;

	// Associated ordered edges
	mesh_edge_t *e[2] = {NULL, NULL};

	// Associated brep.  Unlike ordered edges, an unordered edge may
	// be associated with triangles in more than one mesh.
	struct brep_cdt *cdt = NULL;

	// Information for cases when the uedge is also on a BRep boundary edge
	ON_BrepEdge *edge = NULL;
	double cp_len = DBL_MAX;
	ON_NurbsCurve *nc = NULL;
	double t_start = DBL_MAX;
	double t_end = DBL_MAX;

	// When splitting, one of the criteria is based on the change of slopes
	// at beginning and ending of curves.  These slopes are specific to
	// edges - the same point may have two different tangents if it is used
	// by multiple faces - so those tangents are stored with the edge
	// curves.
	ON_3dVector tangents[2];
	// Because the vertices are unordered, we store in t_verts the specific
	// point associated with each tangent so they may be properly passed down
	// to child curves during a split operation.
	int t_verts[2];

	ON_BoundingBox bb;

	bool linear = false;

	// Called when a mesh_uedge_t is returned to the
	// queue for reuse
	void reset();

	double len;
};


///////////////////////////////////////////////////////////////////////////////
//
//  Triangle container - hold 3D triangles used to define meshes.
//
///////////////////////////////////////////////////////////////////////////////


class mesh_tri_t {
    public:

	// Routine to update triangle bounding box in light of updates to
	// associated edges.
	void update();

	ON_3dPoint &center();
	ON_3dVector &bnorm();
	ON_3dVector &tnorm();

	long v[3] = {-1, -1, -1};
	mesh_edge_t *e[3] = {NULL, NULL, NULL};
	long vect_ind = -1;
	mesh_t *m = NULL;

	ON_BoundingBox bb;

	mesh_tri_t& operator=(const mesh_tri_t &other) = default;

	bool operator==(mesh_tri_t other) const
	{
	    if (m != other.m) {
		return false;
	    }
	    bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]) || (v[0] == other.v[2]));
	    bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]) || (v[1] == other.v[2]));
	    bool c3 = ((v[2] == other.v[0]) || (v[2] == other.v[1]) || (v[2] == other.v[2]));
	    return (c1 && c2 && c3);
	}
	bool operator!=(mesh_tri_t other) const
	{
	    if (m != other.m) {
		return true;
	    }
	    bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]) && (v[0] != other.v[2]));
	    bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]) && (v[1] != other.v[2]));
	    bool c3 = ((v[2] != other.v[0]) && (v[2] != other.v[1]) && (v[2] != other.v[2]));
	    return (c1 || c2 || c3);
	}

	// Called when a mesh_tri_t is returned to the
	// queue for reuse
	void reset() {};

    private:

	ON_Plane tplane;
	ON_Plane bplane;
};


///////////////////////////////////////////////////////////////////////////////
//
//  Polygon container - defines 2D loops which bound faces, and is also
//  used to define and bound subsets of triangle meshes for repair
//  operations.
//
///////////////////////////////////////////////////////////////////////////////

class polygon_t {
    public:

	// Add a polygon point associated with an existing 3D mesh point.
	// Returns -1 if a point cannot be created.
	long add_point(mesh_point_t *meshp);

	// Add a polygon point associated with an existing 3D mesh point, when
	// an existing 2D point has already been calculated and the point
	// addition doesn't need to do the work itself (usually happens when a
	// point is added from a trim curve).  Returns -1 if no such point can
	// be created.
	long add_point(mesh_point_t *meshp, ON_2dPoint &p2d);

	// Operations involving BRep loops (whether initializing or splitting)
	// should always use ordered operations.  Ordered poly_edges are
	// generated based on ordering from the BRep trimming loops or valid
	// triangles (as opposed to unordered edge insertions, which generate
	// poly_edges to satisfy the existing polygon.)
	poly_edge_t *add_ordered_edge(long p1, long p2, int trim_ind);
	void remove_ordered_edge(poly_edge_t &pe);

	// Polygons being constructed on mesh interiors for repair are built
	// and manipulated using existing edges as a guide.  Because the polygon
	// may need to flip these edges, the original mesh_edge_t ordering is
	// not guaranteed to persist in the polygon.  The repair routine in the
	// parent mesh must manage the growth of a loop to ensure its boundaries
	// will mate properly with the broader mesh.
	poly_edge_t *add_edge(mesh_edge_t &e);
	bool remove_edge(mesh_edge_t &e);

	// If we are representing a BRep face loop, that index is stored here
	// and the plane of the polygon is the 2D parametric space of the
	// NURBS surface associated with the face.
	long loop_id = -1;

	// If the polygon is defined in a plane in space (as opposed to the
	// parametric domain of a NURBS surface) that plane is stored here.)
	ON_Plane p_plane;


	// The polygon points store a pointer to their origin 3D point, but
	// to go the other way we need to keep track in a map.
	std::unordered_map<long, long> o2p;

	// Pointer to the parent mesh.  All polygons will have a parent mesh
	mesh_t *m;

	// Polygon index in the parent mesh.  For BRep loops this should match
	// the corresponding index in the ON_Brep structure.
	size_t vect_ind;

	// 2D spatial lookup for polygon edges.  Used to check for self
	// intersections in the polygon (a variation on the "Bush" approach
	// from https://github.com/anvaka/isect - we don't use a static
	// container since the polygon is being refined, but otherwise the
	// idea is similar.)
	RTree<size_t, double, 2> p_edges_tree;

	// Called when a mesh_uedge_t is returned to the
	// queue for reuse
	void reset();

	// Functions to manage object reuse via the queue
	poly_edge_t *get_pedge();
	void put_pedge(poly_edge_t *pe);

	// Containers holding all actual polygon edge and vertex information.
	std::vector<poly_edge_t> p_pedges_vect;
	std::vector<poly_point_t> p_pnts_vect;

    private:
	std::queue<size_t> p_pequeue; // Available (unused) entries in p_pedges_vect
};


///////////////////////////////////////////////////////////////////////////////
//
//  Primary mesh container - holds polygon loops and triangles associated
//  with an individual BRep face.
//
///////////////////////////////////////////////////////////////////////////////

class mesh_t
{
    public:
	// Primary containers for face edges
	std::vector<mesh_edge_t> m_edges_vect;

	// Primary triangle container
	std::vector<mesh_tri_t> m_tris_vect;
	RTree<size_t, double, 3> tris_tree;

	// All polygon loops (boundary or interior) used by the face
	std::vector<polygon_t> m_polys_vect;

	// Tree of face trimming loop approximating edges
	RTree<void *, double, 2> loops_tree;

	// Identify and return the ordered edge associated with the ordered
	// pedge, or create such an ordered edge if one does not already
	// exist.
	mesh_edge_t *edge(poly_edge_t &pe);

	// Identify and return the unordered edge associated with the ordered
	// edge, or create such an unordered edge if one does not already
	// exist.
	mesh_uedge_t *uedge(mesh_edge_t &e);

	// Find the closest point to a 3D point p on the surface of the BRep
	// face associated with this mesh.  Will optionally also calculate the
	// normal vector at that point, if sn is non-NULL.
	bool closest_surf_pt(ON_3dVector *sn, ON_3dPoint &s3d, ON_2dPoint &s2d, ON_3dPoint *p, double tol);

	// Analyze polygon loops for segments that are too close to other
	// segments in their own or other loops relative to their length,
	// and refine them.
	bool split_close_edges();

	size_t outer_loop;

	void delete_edge(mesh_edge_t &e);
	void delete_tri(mesh_tri_t &t);


	ON_BoundingBox bb;
	void bbox_insert();

	struct brep_cdt *cdt;
	ON_BrepFace *f;

	mesh_edge_t *get_edge();
	void put_edge(mesh_edge_t *e);
	mesh_tri_t *get_tri();
	void put_tri(mesh_tri_t *t);
	polygon_t *get_poly(int l_id);
	polygon_t *get_poly(ON_Plane &polyp);
	void put_poly(polygon_t *p);

    private:
	std::queue<size_t> m_equeue; // Available (unused) entries in m_edges_vect
	std::queue<size_t> m_tqueue; // Available (unused) entries in tris_vect
	std::queue<size_t> m_pqueue; // Available (unused) entries in loops_vect
};


///////////////////////////////////////////////////////////////////////////////
//
//  Primary CDT state container - holds information about relationships
//  between BRep faces and edge curves.
//
///////////////////////////////////////////////////////////////////////////////

class brep_cdt_state {
    public:
	// Primary container for points
	std::vector<mesh_point_t> b_pnts;
	// Spatial lookup tree for points
	RTree<size_t, double, 3> b_pnts_tree;

	// All brep unordered edges
	std::vector<mesh_uedge_t> b_uedges_vect;
	// Spatial lookup for unordered edges
	RTree<size_t, double, 3> b_uedges_tree;

	// Spatial lookup for faces
	std::vector<mesh_t> b_faces_vect;
	RTree<size_t, double, 3> b_faces_tree;

	long find_uedge(mesh_edge_t &e);
	void delete_uedge(mesh_uedge_t &ue);

	ON_Brep *orig_brep = NULL;
	ON_Brep *brep = NULL;

	std::string name;
	struct brep_cdt *cdt;

	void brep_cpy_init();
	void verts_init();
	void uedges_init();
	void faces_init();

	mesh_uedge_t *get_uedge();
	void put_uedge(mesh_uedge_t *ue);
    private:
	std::queue<size_t> b_uequeue; // Available (unused) entries in uedges_vect
};

struct brep_cdt_impl {
    brep_cdt_state s;
};


// Vertex processing routines
ON_3dVector singular_vert_norm(ON_Brep *brep, int index);

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

