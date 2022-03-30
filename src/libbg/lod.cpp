/*                       L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Based off of code from https://github.com/bhaettasch/pop-buffer-demo
 * Copyright (c) 2016 Benjamin Hättasch and X3DOM
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file lod.cpp
 *
 * This file implements level-of-detail routines.  Eventually it may have libbg
 * wrappers around more sophisticated algorithms...
 *
 * The POP Buffer: Rapid Progressive Clustering by Geometry Quantization
 * https://x3dom.org/pop/files/popbuffer2013.pdf
 *
 * Useful discussion of applying POP buffers here:
 * https://medium.com/@petroskataras/the-ayotzinapa-case-447a72d89e58
 *
 * TODO - if we're doing the pre-processing work, we should go ahead and prepare
 * a separate set of information for triangle edges.  Getting wireframes from
 * the triangles for every draw operation is inefficient - we end up drawing all
 * internal edges twice.  We should be able to adjust the degenerate test of POP
 * to check if the edge length is degenerate, rather than the triangle, and prepare
 * a wireframe/edge only version of the mesh data for drawing that is more efficient.
 * Then we can tailor the loaded data to the drawing mode.
 *
 * Notes on caching:
 *
 * Management of LoD cached data is actually a bit of a challenge.  A full
 * content hash of the original ver/tri arrays is the most reliable approach
 * but is a potentially expensive operation, which also requires reading the
 * entire original geometry to get the hash value to do lookups.  Ideally, we'd
 * like for the application to never have to access more of the data than is
 * needed for display purposes.  However, object names are not unique across .g
 * files and so are not useful for this purpose.  Also, at this level of the
 * logic we (deliberately) are separated from any notion of .g objects.
 *
 * What we do is generate a hash value of the data on initialization, when we
 * need the full data set to perform the initial LoD setup.  We then provide
 * that value back to the caller for them to manage at a higher level.
 */

#include "common.h"
#include <stdlib.h>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <fstream>

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h> /* for mkdir */
#endif

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "RTree.h"

#include "bio.h"

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/time.h"
#include "bv/plot3.h"
#include "bg/lod.h"
#include "bg/sat.h"
#include "bg/trimesh.h"

#define POP_MAXLEVEL 16
#define POP_CACHEDIR ".POPLoD"
#define MBUMP 1.01

static void
lod_dir(char *dir)
{
#ifdef HAVE_WINDOWS_H
    CreateDirectory(dir, NULL);
#else
    /* mode: 775 */
    mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}

// Unordered edge container
struct uedge_t {
    long v[2];

    uedge_t()
    {
	v[0] = v[1] = -1;
    }

    uedge_t(long i, long j)
    {
	v[0] = (i <= j) ? i : j;
	v[1] = (i > j) ? i : j;
    }

    uedge_t(const uedge_t &other) {
	v[0] = other.v[0];
	v[1] = other.v[1];
    }

    bool operator<(uedge_t other) const
    {
	bool c1 = (v[0] < other.v[0]);
	bool c1e = (v[0] == other.v[0]);
	bool c2 = (v[1] < other.v[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(uedge_t other) const
    {
	bool c1 = (v[0] == other.v[0]);
	bool c2 = (v[1] == other.v[1]);
	return (c1 && c2);
    }
    struct hash
    {
	size_t operator()(const uedge_t& uedge) const
	{
	    size_t v1 = std::hash<int>()(uedge.v[0]);
	    size_t v2 = std::hash<int>()(uedge.v[1]) << 1;
	    return v1 ^ v2;
	}
    };
};

static int
lod_trimesh_aabb(point_t *min, point_t *max, std::vector<int> &afaces, int *faces, int num_faces, const point_t *p, int num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!min || !max)
	return -1;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* If inputs are insufficient, we can't produce a bbox */
    if (!faces || num_faces <= 0 || !p || num_pnts <= 0)
	return -1;

    /* First Pass: coherently iterate through all active faces of the BoT and
     * mark vertices in a bit-vector that are referenced by a face. */
    struct bu_bitv *visit_vert = bu_bitv_new(num_pnts);
    for (size_t i = 0; i < afaces.size(); i++) {
	int tri_index = afaces[i];
	BU_BITSET(visit_vert, faces[tri_index*3 + X]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Y]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Z]);
    }

    /* Second Pass: check max and min of vertices marked */
    for(size_t vert_index = 0; vert_index < (size_t)num_pnts; vert_index++){
	if(BU_BITTEST(visit_vert,vert_index)){
	    VMINMAX((*min), (*max), p[vert_index]);
	}
    }

    /* Done with bitv */
    bu_bitv_free(visit_vert);

    /* Make sure the RPP created is not of zero volume */
    if (NEAR_EQUAL((*min)[X], (*max)[X], SMALL_FASTF)) {
	(*min)[X] -= SMALL_FASTF;
	(*max)[X] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Y], (*max)[Y], SMALL_FASTF)) {
	(*min)[Y] -= SMALL_FASTF;
	(*max)[Y] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Z], (*max)[Z], SMALL_FASTF)) {
	(*min)[Z] -= SMALL_FASTF;
	(*max)[Z] += SMALL_FASTF;
    }

    /* Success */
    return 0;
}

static int
lod_edges_aabb(point_t *min, point_t *max, std::vector<int> &einds, std::vector<uedge_t> &edges, const point_t *p, int num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!min || !max)
	return -1;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* If inputs are insufficient, we can't produce a bbox */
    if (einds.size() == 0 || edges.size() == 0 || !p || num_pnts <= 0)
	return -1;

    /* First Pass: coherently iterate through all edges and mark vertices in a
     * bit-vector that are referenced by an edge. */
    struct bu_bitv *visit_vert = bu_bitv_new(num_pnts);
    for (size_t i = 0; i < einds.size(); i++) {
	BU_BITSET(visit_vert, edges[einds[i]].v[0]);
	BU_BITSET(visit_vert, edges[einds[i]].v[1]);
    }

    /* Second Pass: check max and min of vertices marked */
    for(size_t vert_index = 0; vert_index < (size_t)num_pnts; vert_index++){
	if(BU_BITTEST(visit_vert,vert_index)){
	    VMINMAX((*min), (*max), p[vert_index]);
	}
    }

    /* Done with bitv */
    bu_bitv_free(visit_vert);

    /* Make sure the RPP created is not of zero volume */
    if (NEAR_EQUAL((*min)[X], (*max)[X], SMALL_FASTF)) {
	(*min)[X] -= SMALL_FASTF;
	(*max)[X] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Y], (*max)[Y], SMALL_FASTF)) {
	(*min)[Y] -= SMALL_FASTF;
	(*max)[Y] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Z], (*max)[Z], SMALL_FASTF)) {
	(*min)[Z] -= SMALL_FASTF;
	(*max)[Z] += SMALL_FASTF;
    }

    /* Success */
    return 0;
}

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};

class POPState {
    public:

	// Create cached data (doesn't create a usable container)
	POPState(const point_t *v, int vcnt, int *faces, int fcnt);

	// Load cached data (DOES create a usable container)
	POPState(unsigned long long key);

	// Cleanup
	~POPState() {};

	// Load/unload data level
	void set_level(int level);

	// Get the "current" position of a point, given its level
	void level_pnt(point_t *o, const point_t *p, int level);

	// Debugging
	void plot(const char *root);

	// Active faces needed by the current LoD (indexes into npnts).
	std::vector<int> lod_tris;

	// This is where we store the active points - i.e., those needed for
	// the current LoD.  When initially creating the breakout from BoT data
	// we calculate all levels, but the goal is to not hold in memory any
	// more than we need to support the LoD drawing.  nfaces will index
	// into npnts.
	std::vector<fastf_t> lod_tri_pnts;

	// Similar containers for edges
	std::vector<int> lod_edges;
	std::vector<fastf_t> lod_edge_pnts;

	// Containers for sub-mesh grouping
	RTree<size_t, double, 3> tri_rtree;
	std::vector<std::vector<int>> tri_sets;
	std::vector<fastf_t> triset_bboxes;

	RTree<size_t, double, 3> edge_rtree;
	std::vector<std::vector<int>> edge_sets;
	std::vector<fastf_t> edgeset_bboxes;

	// Current level of detail information loaded into nfaces/npnts
	int curr_level = -1;

	// Maximum level for which POP info is defined.  Above that level,
	// need to shift to full rendering
	int max_tri_pop_level = 0;
	int max_edge_pop_level = 0;

	// Used by calling functions to detect initialization errors
	bool is_valid = false;

	// Content based hash key
	unsigned long long hash;

    private:

	void edge_process();
	void tri_process();

	// Clamping of points to various detail levels
	int to_level(int val, int level);

	// Check if two points are equal at a clamped level
	bool is_equal(rec r1, rec r2, int level);

	// Degeneracy test for edges
	bool edge_degenerate(rec r0, rec r1, int level);

	// Degeneracy test for triangles
	bool tri_degenerate(rec r0, rec r1, rec r2, int level);

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	std::vector<unsigned short> PRECOMPUTED_MASKS;

	// Write data out to cache (only used during initialization from
	// external data)
	void cache();
	void cache_edge(struct bu_vls *vkey);
	void cache_tri(struct bu_vls *vkey);

	// Specific loading and unloading methods
	void tri_pop_load(int start_level, int level);
	void tri_pop_trim(int level);
	void tri_rtree_load();

	void edge_pop_load(int start_level, int level);
	void edge_pop_trim(int level);
	void edge_rtree_load();

	// Global binning of vertices for sub-mesh drawing data
	std::unordered_map<short, std::unordered_map<short, std::unordered_map<short, std::vector<int>>>> tri_boxes;
	std::unordered_map<short, std::unordered_map<short, std::unordered_map<short, std::vector<int>>>> edge_boxes;

	// Processing containers used for initial triangle data characterization
	std::vector<int> tri_ind_map;
	std::vector<int> vert_tri_minlevel;
	std::map<int, std::set<int>> level_tri_verts;
	std::vector<std::vector<int>> level_tris;
	size_t tri_threshold = 0;

	// Processing containers used for initial edge data characterization
	std::vector<int> edge_ind_map;
	std::vector<int> vert_edge_minlevel;
	std::vector<uedge_t> edges;
	std::map<int, std::set<int>> level_edge_verts;
	std::vector<std::vector<int>> level_edges;
	size_t edge_threshold = 0;

	// Pointers to original input data
	int vert_cnt = 0;
	const point_t *verts_array = NULL;
	int faces_cnt = 0;
	int *faces_array = NULL;
};

void
POPState::edge_process()
{
    // Build the set of unique edges for wireframe optimization
    std::unordered_set<uedge_t, uedge_t::hash> eset;
    for (int i = 0; i < faces_cnt; i++) {
	int v1 = faces_array[3*i+0];
	int v2 = faces_array[3*i+1];
	int v3 = faces_array[3*i+2];
	uedge_t e1(v1, v2);
	uedge_t e2(v2, v3);
	uedge_t e3(v3, v1);
	eset.insert(e1);
	eset.insert(e2);
	eset.insert(e3);
    }

    // Make a vector of the unique edges so we can reference by index
    std::unordered_set<uedge_t, uedge_t::hash>::iterator e_it;
    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
	edges.push_back(*e_it);
    }
    // Having ensured edge uniqueness and assigned indices, we are now done
    // with the unordered set - let the program know it can have the memory
    eset = std::unordered_set<uedge_t, uedge_t::hash>();

    // Until we prove otherwise, all edges are assumed to appear only at the
    // last level (and consequently, their vertices are only needed then).  Set
    // the level accordingly.
    vert_edge_minlevel.reserve(vert_cnt);
    for (int i = 0; i < vert_cnt; i++) {
	vert_edge_minlevel.push_back(POP_MAXLEVEL - 1);
    }

    // Reserve memory for level containers
    level_edges.reserve(POP_MAXLEVEL);
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	level_edges.push_back(std::vector<int>(0));
    }

    // Walk the edges and perform the LoD characterization
    for (size_t i = 0; i < edges.size(); i++) {
	rec edge[2];
	// Transform edge vertices
	for (int j = 0; j < 2; j++) {
	    edge[j].x = floor((verts_array[edges[i].v[j]][X] - minx) / (maxx - minx) * USHRT_MAX);
	    edge[j].y = floor((verts_array[edges[i].v[j]][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    edge[j].z = floor((verts_array[edges[i].v[j]][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}

	// Find the pop up level for this edge (i.e., when it will first
	// appear as we step up the zoom levels.)
	int level = POP_MAXLEVEL - 1;
	for (int j = 0; j < POP_MAXLEVEL; j++) {
	    if (!edge_degenerate(edge[0], edge[1], j)) {
		level = j;
		break;
	    }
	}
	// Add this edge to its "pop" level
	level_edges[level].push_back(i);

	// Let the vertices know they will be needed at this level, if another
	// edge doesn't already need them sooner
	for (int j = 0; j < 2; j++) {
	    if (vert_edge_minlevel[edges[i].v[j]] > level)
		vert_edge_minlevel[edges[i].v[j]] = level;
	}
    }

    // The edge vertices now know when they will first need to appear.  Build level
    // sets of vertices
    for (size_t i = 0; i < vert_edge_minlevel.size(); i++) {
	level_edge_verts[vert_edge_minlevel[i]].insert(i);
    }

    // Having sorted the vertices into level sets, we may now define a new global
    // vertex ordering that respects the needs of the levels.
    edge_ind_map.reserve(vert_cnt);
    for (int i = 0; i < vert_cnt; i++) {
	edge_ind_map.push_back(i);
    }
    int vind = 0;
    std::map<int, std::set<int>>::iterator l_it;
    std::set<int>::iterator s_it;
    for (l_it = level_edge_verts.begin(); l_it != level_edge_verts.end(); l_it++) {
	for (s_it = l_it->second.begin(); s_it != l_it->second.end(); s_it++) {
	    edge_ind_map[*s_it] = vind;
	    vind++;
	}
    }

    // Beyond a certain depth, there is little benefit to the POP process.  If
    // we check the count of level_edges, we will find a level at which the
    // majority of the edges are active. Above that level, we change how we are
    // managing the data.
    size_t edgesum = 0;
    size_t fcnt2 = (size_t)((fastf_t)faces_cnt/2.0);
    for (size_t i = 0; i < level_edges.size(); i++) {
	edgesum += level_edges[i].size();
	if (edgesum > fcnt2) {
	    edge_threshold = i;
	    break;
	}
    }
    //bu_log("Edge threshold level: %zd\n", edge_threshold);

    // At the point when most edges are active, we switch to trying to
    // recognize what parts of the mesh are in the view and only drawing those
    // edges, rather than doing the LoD point snapping.  Bin the edges into
    // discrete volumes (non-uniquely - the main concern here is to make sure
    // we draw all the edges we need to draw in a given area.) Once we shift to
    // this mode we have to load all the vertices and faces_array into memory,
    // but by the time we make that switch we would have had comparable data in
    // memory from the LoD info anyway.
    for (size_t i = 0; i < edges.size(); i++) {
	short edge[2][3];
	// This is a tradeoff - smaller means fewer boxes but more edges
	// per box.  Will have to see what a practical value is with real data.
	int factor = 32;
	// Transform edge vertices
	for (int j = 0; j < 2; j++) {
	    edge[j][0] = floor((verts_array[faces_array[3*i+j]][X] - minx) / (maxx - minx) * factor);
	    edge[j][1] = floor((verts_array[faces_array[3*i+j]][Y] - miny) / (maxy - miny) * factor);
	    edge[j][2] = floor((verts_array[faces_array[3*i+j]][Z] - minz) / (maxz - minz) * factor);
	}

	// The end points are trivially active.
	edge_boxes[edge[0][0]][edge[0][1]][edge[0][2]].push_back(i);

	// If the edge is fully inside the first box, we're done.  If not,
	// we have to figure out which other boxes it intersects.  The other
	// end point box is trivially active, but there may also be intermediate
	// boxes which intersect the line.
	if (edge[0][0] != edge[1][0] || edge[0][1] != edge[1][1] || edge[0][2] != edge[1][2]) {

	    // Other end point is trivially active
	    edge_boxes[edge[1][0]][edge[1][1]][edge[1][2]].push_back(i);

	    // Will be doing ray box test
	    point_t lorigin, bcenter;
	    vect_t dir, bextent;
	    VSET(lorigin, edge[0][0], edge[0][1], edge[0][2]);
	    VSET(dir, edge[1][0] - edge[0][0], edge[1][1] - edge[0][1], edge[1][2] - edge[0][2]);
	    VSET(bextent, 0.51, 0.51, 0.51);

	    // Iterate over boxes other than the end points
	    int start[3], end[3];
	    start[0] = edge[0][0] > edge[1][0] ? edge[1][0] : edge[0][0];
	    start[1] = edge[0][1] > edge[1][1] ? edge[1][1] : edge[0][1];
	    start[2] = edge[0][2] > edge[1][2] ? edge[1][2] : edge[0][2];
	    end[0] = edge[0][0] < edge[1][0] ? edge[1][0] : edge[0][0];
	    end[1] = edge[0][1] < edge[1][1] ? edge[1][1] : edge[0][1];
	    end[2] = edge[0][2] < edge[1][2] ? edge[1][2] : edge[0][2];
	    for (int e = start[0]; e <= end[0]; e++) {
		for (int j = start[1]; j <= end[1]; j++) {
		    for (int k = start[2]; k <= end[2]; k++) {
			bool endpt = false;
			for (int l = 0; l < 2; l++) {
			    if (edge[l][0] == e && edge[l][1] == j && edge[l][2] == k) {
				endpt = true;
				break;
			    }
			}
			if (endpt)
			    continue;
			VSET(bcenter, (fastf_t)e+0.5, (fastf_t)j+0.5, (fastf_t)k+0.5);
			if (bg_sat_line_aabb(lorigin, dir, bcenter, bextent)) {
			    edge_boxes[e][j][k].push_back(i);
			    //bu_log("line hit: %d %d %d\n", e, j ,k);
			} else {
			    //bu_log("MISS: %d %d %d\n", e, j ,k);
			}
		    }
		}
	    }
	    //bu_log("edge: %d,%d,%d -> %d,%d,%d\n", V3ARGS(edge[0]), V3ARGS(edge[1]));
	    //bu_log("edge range: %d,%d,%d -> %d,%d,%d\n", V3ARGS(start), V3ARGS(end));
	}
    }
}

void
POPState::tri_process()
{
    // Until we prove otherwise, all edges are assumed to appear only at the
    // last level (and consequently, their vertices are only needed then).  Set
    // the level accordingly.
    vert_tri_minlevel.reserve(vert_cnt);
    for (int i = 0; i < vert_cnt; i++) {
	vert_tri_minlevel.push_back(POP_MAXLEVEL - 1);
    }

    // Reserve memory for level containers
    level_tris.reserve(POP_MAXLEVEL);
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	level_tris.push_back(std::vector<int>(0));
    }

    // Walk the triangles and perform the LoD characterization
    for (int i = 0; i < faces_cnt; i++) {
	rec triangle[3];
	// Transform triangle vertices
	for (int j = 0; j < 3; j++) {
	    triangle[j].x = floor((verts_array[faces_array[3*i+j]][X] - minx) / (maxx - minx) * USHRT_MAX);
	    triangle[j].y = floor((verts_array[faces_array[3*i+j]][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    triangle[j].z = floor((verts_array[faces_array[3*i+j]][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}

	// Find the pop up level for this triangle (i.e., when it will first
	// appear as we step up the zoom levels.)
	int level = POP_MAXLEVEL - 1;
	for (int j = 0; j < POP_MAXLEVEL; j++) {
	    if (!tri_degenerate(triangle[0], triangle[1], triangle[2], j)) {
		level = j;
		break;
	    }
	}
	// Add this triangle to its "pop" level
	level_tris[level].push_back(i);

	// Let the vertices know they will be needed at this level, if another
	// triangle doesn't already need them sooner
	for (int j = 0; j < 3; j++) {
	    if (vert_tri_minlevel[faces_array[3*i+j]] > level) {
		vert_tri_minlevel[faces_array[3*i+j]] = level;
	    }
	}
    }

    // The vertices now know when they will first need to appear.  Build level
    // sets of vertices
    for (size_t i = 0; i < vert_tri_minlevel.size(); i++) {
	level_tri_verts[vert_tri_minlevel[i]].insert(i);
    }

    // Having sorted the vertices into level sets, we may now define a new global
    // vertex ordering that respects the needs of the levels.
    tri_ind_map.reserve(vert_cnt);
    for (int i = 0; i < vert_cnt; i++) {
	tri_ind_map.push_back(i);
    }
    int vind = 0;
    std::map<int, std::set<int>>::iterator l_it;
    std::set<int>::iterator s_it;
    for (l_it = level_tri_verts.begin(); l_it != level_tri_verts.end(); l_it++) {
	for (s_it = l_it->second.begin(); s_it != l_it->second.end(); s_it++) {
	    tri_ind_map[*s_it] = vind;
	    vind++;
	}
    }

    // Beyond a certain depth, there is little benefit to the POP process.  If
    // we check the count of level_tris, we will find a level at which the
    // majority of the triangles are active.
    size_t trisum = 0;
    size_t faces_array_cnt2 = (size_t)((fastf_t)faces_cnt/2.0);
    for (size_t i = 0; i < level_tris.size(); i++) {
	trisum += level_tris[i].size();
	if (trisum > faces_array_cnt2) {
	    tri_threshold = i;
	    break;
	}
    }
    //bu_log("Triangle threshold level: %zd\n", tri_threshold);

    // At the point when most tris are active, try to recognize what parts of
    // the mesh are in the view and only draw those, rather than doing the LoD
    // point snapping.  Bin the triangles into discrete volumes (non-uniquely -
    // the main concern here is to make sure we draw all the triangles we need
    // to draw in a given area.) Once we shift to this mode we have to load all
    // the vertices and faces_array into memory, but by the time we make that
    // switch we would have had comparable data in memory from the LoD info
    // anyway.
    for (int i = 0; i < faces_cnt; i++) {
	short tri[3][3];
	// TODO: This is a tradeoff - smaller means fewer boxes but more tris
	// per box.  Will have to see what a practical value is with real data.
	int factor = 32;
	// Transform tri vertices
	for (int j = 0; j < 3; j++) {
	    tri[j][0] = floor((verts_array[faces_array[3*i+j]][X] - minx) / (maxx - minx) * factor);
	    tri[j][1] = floor((verts_array[faces_array[3*i+j]][Y] - miny) / (maxy - miny) * factor);
	    tri[j][2] = floor((verts_array[faces_array[3*i+j]][Z] - minz) / (maxz - minz) * factor);
	}
	//bu_log("tri: %d %d %d -> %d %d %d -> %d %d %d\n", V3ARGS(tri[0]), V3ARGS(tri[1]), V3ARGS(tri[2]));
	// TODO - this is inadequate - a triangle large compared to the box
	// sizes might need to be drawn even if none of the vertices are on
	// the screen.  Need to intersect the triangle with the set of boxes
	// within the tri xyz range.
	tri_boxes[tri[0][0]][tri[0][1]][tri[0][2]].push_back(i);
	if (tri[0][0] != tri[1][0] || tri[0][1] != tri[1][1] || tri[0][2] != tri[1][2])
	    tri_boxes[tri[1][0]][tri[1][1]][tri[1][2]].push_back(i);
	if ((tri[0][0] != tri[2][0] || tri[0][1] != tri[2][1] || tri[0][2] != tri[2][2]) ||
		(tri[1][0] != tri[2][0] || tri[1][1] != tri[2][1] || tri[1][2] != tri[2][2]))
	    tri_boxes[tri[2][0]][tri[2][1]][tri[2][2]].push_back(i);
    }
}

POPState::POPState(const point_t *v, int vcnt, int *faces, int fcnt)
{
    // Hash the data to generate a key
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, v, vcnt*sizeof(point_t));
    XXH64_update(&h_state, faces, 3*fcnt*sizeof(int));
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    hash = (unsigned long long)hash_val;

    // Make sure there's no cache before performing the full initializing from
    // the original data.  In this mode the POPState creation is used to
    // initialize the cache, not to create a workable data state from that
    // data.  The hash is set, which is all we really need - loading data from
    // the cache is handled elsewhere.
    char dir[MAXPATHLEN];
    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), NULL);
    if (bu_file_exists(dir, NULL)) {
	bu_vls_free(&vkey);
	is_valid = true;
	return;
    }
    bu_vls_free(&vkey);

    curr_level = POP_MAXLEVEL - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    // Store source data info
    vert_cnt = vcnt;
    verts_array = v;
    faces_cnt = fcnt;
    faces_array = faces;

    // Find our min and max values, initialize levels
    for (int i = 0; i < vcnt; i++) {
	minx = (v[i][X] < minx) ? v[i][X] : minx;
	miny = (v[i][Y] < miny) ? v[i][Y] : miny;
	minz = (v[i][Z] < minz) ? v[i][Z] : minz;
	maxx = (v[i][X] > maxx) ? v[i][X] : maxx;
	maxy = (v[i][Y] > maxy) ? v[i][Y] : maxy;
	maxz = (v[i][Z] > maxz) ? v[i][Z] : maxz;
    }

    // Bump out the min and max bounds slightly so none of our actual
    // points are too close to these limits
    minx = minx-fabs(MBUMP*minx);
    miny = miny-fabs(MBUMP*miny);
    minz = minz-fabs(MBUMP*minz);
    maxx = maxx+fabs(MBUMP*maxx);
    maxy = maxy+fabs(MBUMP*maxy);
    maxz = maxz+fabs(MBUMP*maxz);


    // Characterize triangle edges (wireframe drawing)
    edge_process();


    // Characterize triangle faces (solid drawing)
    tri_process();


    // We're now ready to write out the data
    is_valid = true;
    cache();

    if (!is_valid)
	return;

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %d count: %zd\n", i, level_tris[i].size());
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %d count: %zd\n", i, level_tri_verts[i].size());
    }
}

POPState::POPState(unsigned long long key)
{
    if (!key)
	return;

    // Initialize so set_level will read in the first level of triangles
    curr_level = - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", key);
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), NULL);
    if (!bu_file_exists(dir, NULL)) {
	bu_vls_free(&vkey);
	return;
    }
    hash = key;

    // Reserve memory for level containers
    level_edges.reserve(POP_MAXLEVEL);
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	level_edges.push_back(std::vector<int>(0));
    }

    // Reserve memory for level containers
    level_tris.reserve(POP_MAXLEVEL);
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	level_tris.push_back(std::vector<int>(0));
    }

    // Read in min/max bounds
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "minmax", NULL);
	std::ifstream minmaxfile(dir, std::ios::in | std::ofstream::binary);
	float minmax[6];
	minmaxfile.read(reinterpret_cast<char *>(&minmax), sizeof(minmax));
	minx = minmax[0];
	miny = minmax[1];
	minz = minmax[2];
	maxx = minmax[3];
	maxy = minmax[4];
	maxz = minmax[5];
    }

    // Find the maximum POP level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "tris_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&vfile), NULL);
	if (bu_file_exists(dir, NULL)) {
	    max_tri_pop_level = i;
	}
	bu_vls_sprintf(&vfile, "edges_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&vfile), NULL);
	if (bu_file_exists(dir, NULL)) {
	    max_edge_pop_level = i;
	}
    }

    // Read in the zero level vertices and triangles
    set_level(0);

    // All set - ready for LoD
    bu_vls_free(&vkey);
    is_valid = 1;
}

void
POPState::edge_pop_load(int start_level, int level)
{
    char dir[MAXPATHLEN];
    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);

    // Read in the level vertices
    for (int i = start_level+1; i <= level; i++) {
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "edge_verts_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&vfile), NULL);
	if (!bu_file_exists(dir, NULL))
	    continue;

	std::ifstream vifile(dir, std::ios::in | std::ofstream::binary);
	int vicnt = 0;
	vifile.read(reinterpret_cast<char *>(&vicnt), sizeof(vicnt));
	for (int j = 0; j < vicnt; j++) {
	    point_t nv;
	    vifile.read(reinterpret_cast<char *>(&nv), sizeof(point_t));
	    for (int k = 0; k < 3; k++) {
		lod_edge_pnts.push_back(nv[k]);
	    }
	    level_edge_verts[i].insert(lod_edge_pnts.size() - 1);
	}
	vifile.close();
	bu_vls_free(&vfile);
    }

    // Read in the level edge sets
    for (int i = start_level+1; i <= level; i++) {
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "edges_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&tfile), NULL);
	if (!bu_file_exists(dir, NULL))
	    continue;

	std::ifstream tifile(dir, std::ios::in | std::ofstream::binary);
	int ticnt = 0;
	tifile.read(reinterpret_cast<char *>(&ticnt), sizeof(ticnt));
	for (int j = 0; j < ticnt; j++) {
	    int vf[2];
	    tifile.read(reinterpret_cast<char *>(&vf), 2*sizeof(int));
	    for (int k = 0; k < 2; k++) {
		lod_edges.push_back(vf[k]);
	    }
	    level_edges[i].push_back(lod_edges.size() / 2 - 1);
	}
	tifile.close();
	bu_vls_free(&tfile);
    }

    bu_vls_free(&vkey);
}

void
POPState::edge_pop_trim(int level)
{
    // Clear the level_edges info for everything above the target level - it will be reloaded
    // if we need it again
    for (size_t i = level+1; i < POP_MAXLEVEL; i++) {
	level_edges[i].clear();
	level_edges[i].shrink_to_fit();
    }
    // Tally all the lower level verts and edges - those are the ones we need to keep
    size_t vkeep_cnt = 0;
    size_t fkeep_cnt = 0;
    for (size_t i = 0; i <= (size_t)level; i++) {
	vkeep_cnt += level_edge_verts[i].size();
	fkeep_cnt += level_edges[i].size();
    }

    // Shrink the main arrays (note that in C++11 shrink_to_fit may or may
    // not actually shrink memory usage on any given call.)
    lod_edge_pnts.resize(vkeep_cnt*3);
    lod_edge_pnts.shrink_to_fit();
    lod_edges.resize(fkeep_cnt*2);
    lod_edges.shrink_to_fit();

}

void
POPState::edge_rtree_load()
{
    lod_edge_pnts.clear();
    lod_edges.clear();
    edges.clear();

    size_t ecnt = 0;
    char dir[MAXPATHLEN];
    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "all_verts", NULL);
    std::ifstream avfile(dir, std::ios::in | std::ofstream::binary);
    avfile.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    for (size_t i = 0; i < ecnt; i++) {
	point_t nv;
	avfile.read(reinterpret_cast<char *>(&nv), sizeof(point_t));
	for (int k = 0; k < 3; k++) {
	    lod_edge_pnts.push_back(nv[k]);
	}
    }

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "all_edges", NULL);
    std::ifstream aefile(dir, std::ios::in | std::ofstream::binary);
    aefile.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    for (size_t i = 0; i < ecnt; i++) {
	uedge_t e;
	aefile.read(reinterpret_cast<char *>(&e.v), sizeof(e.v));
	edges.push_back(e);
    }

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "edge_sets", NULL);
    std::ifstream tset_file(dir, std::ios::in | std::ofstream::binary);
    tset_file.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    edge_sets.reserve(ecnt);
    for (size_t i = 0; i < ecnt; i++) {
	edge_sets.push_back(std::vector<int>(0));
    }
    edgeset_bboxes.reserve(6*ecnt);
    for (size_t i = 0; i < 6*ecnt; i++) {
	edgeset_bboxes.push_back(MAX_FASTF);
    }
    for (size_t i = 0; i < ecnt; i++) {
	point_t bbox[2] = {VINIT_ZERO, VINIT_ZERO};
	tset_file.read(reinterpret_cast<char *>(&(bbox[0][0])), sizeof(point_t));
	tset_file.read(reinterpret_cast<char *>(&(bbox[1][0])), sizeof(point_t));
	edgeset_bboxes[i*6+0] = bbox[0][X];
	edgeset_bboxes[i*6+1] = bbox[0][Y];
	edgeset_bboxes[i*6+2] = bbox[0][Z];
	edgeset_bboxes[i*6+3] = bbox[1][X];
	edgeset_bboxes[i*6+4] = bbox[1][Y];
	edgeset_bboxes[i*6+5] = bbox[1][Z];

	edge_rtree.Insert(bbox[0], bbox[1], i);

	size_t tcnt = 0;
	tset_file.read(reinterpret_cast<char *>(&tcnt), sizeof(tcnt));
	edge_sets[i].reserve(tcnt);
	for (size_t j = 0; j < tcnt; j++) {
	    int tind;
	    tset_file.read(reinterpret_cast<char *>(&tind), sizeof(int));
	    edge_sets[i].push_back(tind);
	}
    }
}

void
POPState::tri_pop_load(int start_level, int level)
{
    char dir[MAXPATHLEN];
    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);

    // Read in the level vertices
    for (int i = start_level+1; i <= level; i++) {
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "tri_verts_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&vfile), NULL);
	if (!bu_file_exists(dir, NULL))
	    continue;

	std::ifstream vifile(dir, std::ios::in | std::ofstream::binary);
	int vicnt = 0;
	vifile.read(reinterpret_cast<char *>(&vicnt), sizeof(vicnt));
	for (int j = 0; j < vicnt; j++) {
	    point_t nv;
	    vifile.read(reinterpret_cast<char *>(&nv), sizeof(point_t));
	    for (int k = 0; k < 3; k++) {
		lod_tri_pnts.push_back(nv[k]);
	    }
	    level_tri_verts[i].insert(lod_tri_pnts.size() - 1);
	}
	vifile.close();
	bu_vls_free(&vfile);
    }

    // Read in the level triangles
    for (int i = start_level+1; i <= level; i++) {
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "tris_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&tfile), NULL);
	if (!bu_file_exists(dir, NULL))
	    continue;

	std::ifstream tifile(dir, std::ios::in | std::ofstream::binary);
	int ticnt = 0;
	tifile.read(reinterpret_cast<char *>(&ticnt), sizeof(ticnt));
	for (int j = 0; j < ticnt; j++) {
	    int vf[3];
	    tifile.read(reinterpret_cast<char *>(&vf), 3*sizeof(int));
	    for (int k = 0; k < 3; k++) {
		lod_tris.push_back(vf[k]);
	    }
	    level_tris[i].push_back(lod_tris.size() / 3 - 1);
	}
	tifile.close();
	bu_vls_free(&tfile);
    }

    bu_vls_free(&vkey);
}

void
POPState::tri_pop_trim(int level)
{
    // Clear the level_tris info for everything above the target level - it will be reloaded
    // if we need it again
    for (size_t i = level+1; i < POP_MAXLEVEL; i++) {
	level_tris[i].clear();
	level_tris[i].shrink_to_fit();
    }
    // Tally all the lower level verts and tris - those are the ones we need to keep
    size_t vkeep_cnt = 0;
    size_t fkeep_cnt = 0;
    for (size_t i = 0; i <= (size_t)level; i++) {
	vkeep_cnt += level_tri_verts[i].size();
	fkeep_cnt += level_tris[i].size();
    }

    // Shrink the main arrays (note that in C++11 shrink_to_fit may or may
    // not actually shrink memory usage on any given call.)
    lod_tri_pnts.resize(vkeep_cnt*3);
    lod_tri_pnts.shrink_to_fit();
    lod_tris.resize(fkeep_cnt*3);
    lod_tris.shrink_to_fit();

}

void
POPState::tri_rtree_load()
{
    lod_tri_pnts.clear();
    lod_tris.clear();

    size_t ecnt = 0;
    char dir[MAXPATHLEN];
    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "all_verts", NULL);
    std::ifstream avfile(dir, std::ios::in | std::ofstream::binary);
    avfile.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    for (size_t i = 0; i < ecnt; i++) {
	point_t nv;
	avfile.read(reinterpret_cast<char *>(&nv), sizeof(point_t));
	for (int k = 0; k < 3; k++) {
	    lod_tri_pnts.push_back(nv[k]);
	}
    }

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "all_faces", NULL);
    std::ifstream affile(dir, std::ios::in | std::ofstream::binary);
    affile.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    for (size_t i = 0; i < ecnt; i++) {
	int vf[3];
	affile.read(reinterpret_cast<char *>(&vf), 3*sizeof(int));
	for (int k = 0; k < 3; k++) {
	    lod_tris.push_back(vf[k]);
	}
    }


    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "tri_sets", NULL);
    std::ifstream tset_file(dir, std::ios::in | std::ofstream::binary);
    tset_file.read(reinterpret_cast<char *>(&ecnt), sizeof(ecnt));
    tri_sets.reserve(ecnt);
    for (size_t i = 0; i < ecnt; i++) {
	tri_sets.push_back(std::vector<int>(0));
    }
    triset_bboxes.reserve(6*ecnt);
    for (size_t i = 0; i < 6*ecnt; i++) {
	triset_bboxes.push_back(MAX_FASTF);
    }
    for (size_t i = 0; i < ecnt; i++) {
	point_t bbox[2] = {VINIT_ZERO, VINIT_ZERO};
	tset_file.read(reinterpret_cast<char *>(&(bbox[0][0])), sizeof(point_t));
	tset_file.read(reinterpret_cast<char *>(&(bbox[1][0])), sizeof(point_t));
	triset_bboxes[i*6+0] = bbox[0][X];
	triset_bboxes[i*6+1] = bbox[0][Y];
	triset_bboxes[i*6+2] = bbox[0][Z];
	triset_bboxes[i*6+3] = bbox[1][X];
	triset_bboxes[i*6+4] = bbox[1][Y];
	triset_bboxes[i*6+5] = bbox[1][Z];

	tri_rtree.Insert(bbox[0], bbox[1], i);

	size_t tcnt = 0;
	tset_file.read(reinterpret_cast<char *>(&tcnt), sizeof(tcnt));
	tri_sets[i].reserve(tcnt);
	for (size_t j = 0; j < tcnt; j++) {
	    int tind;
	    tset_file.read(reinterpret_cast<char *>(&tind), sizeof(int));
	    tri_sets[i].push_back(tind);
	}
    }
}


// NOTE: at some point it may be worth investigating using bu_open_mapped_file
// and friends for this, depending on what profiling shows in real-world usage.
void
POPState::set_level(int level)
{
    // If we're already there, no work to do
    if (level == curr_level)
	return;

    int64_t start, elapsed;
    fastf_t seconds;
    start = bu_gettime();

    // Triangles

    // If we need to pull more data, do so
    if (level > curr_level && level <= max_tri_pop_level) {
	tri_pop_load(curr_level, level);
    }

    // If we need to trim back the POP data, do that
    if (level < curr_level && level <= max_tri_pop_level && curr_level <= max_tri_pop_level) {
	tri_pop_trim(level);
    }

    // If we were operating beyond POP detail levels (i.e. using RTree
    // management) we need to reset our POP data and clear the more detailed
    // info from the containers to free up memory.
    if (level < curr_level && level <= max_tri_pop_level && curr_level > max_tri_pop_level) {
	// We're (re)entering the POP range - clear the high detail data and start over
	lod_tris.clear();
	lod_tri_pnts.clear();

	// Full reset, not an incremental load.
	tri_pop_load(-1, level);

	// We may have substantially shrunk these arrays - see if we can open
	// up the memory
	lod_tris.shrink_to_fit();
	lod_tri_pnts.shrink_to_fit();
    }

    // If we're jumping into details levels beyond POP range, clear the POP containers
    // and load the more detailed data management info
    if (level > curr_level && level > max_tri_pop_level && curr_level <= max_tri_pop_level) {
	for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	    level_tris[i].clear();
	    level_tris[i].shrink_to_fit();
	}

	// Load data for managing more detailed rendering
	tri_rtree_load();
    }

    // Edges

    // If we need to pull more data, do so
    if (level > curr_level && level <= max_edge_pop_level) {
	edge_pop_load(curr_level, level);
    }

    // If we need to trim back the POP data, do that
    if (level < curr_level && level <= max_edge_pop_level && curr_level <= max_tri_pop_level) {
	edge_pop_trim(level);
    }

    // If we were operating beyond POP detail levels (i.e. using RTree
    // management) we need to reset our POP data and clear the more detailed
    // info from the containers to free up memory.
    if (level < curr_level && level <= max_edge_pop_level && curr_level > max_edge_pop_level) {
	// We're (re)entering the POP range - clear the high detail data and start over
	lod_edges.clear();
	lod_edge_pnts.clear();
	edges.clear();

	// Full reset, not an incremental load.
	edge_pop_load(-1, level);

	// We may have substantially shrunk these arrays - see if we can open
	// up the memory
	lod_edges.shrink_to_fit();
	lod_edge_pnts.shrink_to_fit();
	edges.shrink_to_fit();
    }

    // If we're jumping into details levels beyond POP range, clear the POP containers
    // and load the more detailed data management info
    if (level > curr_level && level > max_edge_pop_level && curr_level <= max_edge_pop_level) {
	edge_rtree_load();
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("lod set_level(%d): %f sec\n", level, seconds);

    curr_level = level;
}

void
POPState::cache_edge(struct bu_vls *vkey)
{
    char dir[MAXPATHLEN];

    // Write out the level vertices
    for (size_t i = 0; i <= edge_threshold; i++) {
	if (level_edge_verts.find(i) == level_edge_verts.end())
	    continue;
	if (!level_edge_verts[i].size())
	    continue;
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "edge_verts_level_%zd", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), bu_vls_cstr(&vfile), NULL);

	std::ofstream vofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level vert vector
	int sv = level_edge_verts[i].size();
	vofile.write(reinterpret_cast<const char *>(&sv), sizeof(sv));

	// Write out the vertex points
	std::set<int>::iterator s_it;
	for (s_it = level_edge_verts[i].begin(); s_it != level_edge_verts[i].end(); s_it++) {
	    point_t v;
	    VMOVE(v, verts_array[*s_it]);
	    vofile.write(reinterpret_cast<const char *>(&v[0]), sizeof(point_t));
	}

	vofile.close();
	bu_vls_free(&vfile);
    }


    // Write out the level edges
    for (size_t i = 0; i <= edge_threshold; i++) {
	if (!level_edges[i].size())
	    continue;
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "edges_level_%zd", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), bu_vls_cstr(&tfile), NULL);

	std::ofstream tofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level edge vector
	int st = level_edges[i].size();
	tofile.write(reinterpret_cast<const char *>(&st), sizeof(st));

	// Write out the mapped edge vertex indices
	std::vector<int>::iterator s_it;
	for (s_it = level_edges[i].begin(); s_it != level_edges[i].end(); s_it++) {
	    int ev[2];
	    ev[0] = edge_ind_map[edges[*s_it].v[0]];
	    ev[1] = edge_ind_map[edges[*s_it].v[1]];
	    tofile.write(reinterpret_cast<const char *>(&ev[0]), sizeof(ev));
	}

	tofile.close();
	bu_vls_free(&tfile);
    }

    // The edges array is saved, so the boxes may reference only a single
    // integer index rather than having to store two vertex indices.  This is
    // also intended to facilitate uniqueness operations in loading code - if
    // an OBB from a view intersects many leaf nodes, to avoid multiple drawing
    // of the same edge from multiple leaf box sources an initial uniqueness
    // pass is needed.
    //
    // NOTE:  The high fidelity data, unlike the POP levels, just uses the
    // original vertex ordering written out in all_verts.   The reason that the
    // POP edge data uses one vertex ordering and the all_edges another is to
    // avoid storing multiple copies of the full vertex set for faces and
    // edges.  The vertex sorting for faces and edges is not guaranteed to be
    // the same, and rather than involve the POP sorting in a scheme that
    // doesn't use it we stick to the original orderings for clarity and
    // simplicity.
    {
	size_t ecnt;
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "all_edges", NULL);
	std::ofstream efile(dir, std::ios::out | std::ofstream::binary);
	ecnt = edges.size();
	efile.write(reinterpret_cast<const char *>(&ecnt), sizeof(ecnt));
	for (size_t i = 0; i < edges.size(); i++) {
	    efile.write(reinterpret_cast<const char *>(&edges[i].v[0]), sizeof(edges[i].v));
	}
	efile.close();
    }

    // Write out the edge sets.  We calculate and write out the bounding boxes
    // of each set so the loading code need only read the values to prepare an
    // RTree.  These edge indices are based on the all_verts array.
    std::unordered_map<short, std::unordered_map<short, std::unordered_map<short, std::vector<int>>>>::iterator b1_it;
    std::unordered_map<short, std::unordered_map<short, std::vector<int>>>::iterator b2_it;
    std::unordered_map<short, std::vector<int>>::iterator b3_it;
    size_t bcnt = 0;
    size_t ecnt = 0;
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "edge_sets", NULL);
	std::ofstream esfile(dir, std::ios::out | std::ofstream::binary);
	if (!esfile.is_open())
	    return;

	for (b1_it = edge_boxes.begin(); b1_it != edge_boxes.end(); b1_it++) {
	    for (b2_it = b1_it->second.begin(); b2_it != b1_it->second.end(); b2_it++) {
		for (b3_it = b2_it->second.begin(); b3_it != b2_it->second.end(); b3_it++) {
		    bcnt++;
		}
	    }
	}
	esfile.write(reinterpret_cast<const char *>(&bcnt), sizeof(bcnt));

	for (b1_it = edge_boxes.begin(); b1_it != edge_boxes.end(); b1_it++) {
	    for (b2_it = b1_it->second.begin(); b2_it != b1_it->second.end(); b2_it++) {
		for (b3_it = b2_it->second.begin(); b3_it != b2_it->second.end(); b3_it++) {
		    // This shouldn't happen, but if the logic changes to
		    // somehow clear a set we don't want an empty set
		    if (!b3_it->second.size())
			continue;

		    // Calculate the bbox
		    point_t min, max;
		    if (lod_edges_aabb(&min, &max, b3_it->second, edges, verts_array, vert_cnt) < 0) {
			bu_log("Error - edge set bbox calculation failed\n");
			esfile.close();
			continue;
		    }
		    esfile.write(reinterpret_cast<const char *>(&min[0]), sizeof(point_t));
		    esfile.write(reinterpret_cast<const char *>(&max[0]), sizeof(point_t));

		    // Record how many edges we've got, and then write the edge
		    // indices themselves
		    ecnt = b3_it->second.size();
		    esfile.write(reinterpret_cast<const char *>(&ecnt), sizeof(ecnt));
		    esfile.write(reinterpret_cast<const char *>(&b3_it->second[0]), b3_it->second.size() * sizeof(int));
		}
	    }
	}

	esfile.close();
    }
}

void
POPState::cache_tri(struct bu_vls *vkey)
{
    char dir[MAXPATHLEN];

    // Write out the level vertices
    for (size_t i = 0; i <= tri_threshold; i++) {
	if (level_tri_verts.find(i) == level_tri_verts.end())
	    continue;
	if (!level_tri_verts[i].size())
	    continue;
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "tri_verts_level_%zd", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), bu_vls_cstr(&vfile), NULL);

	std::ofstream vofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level vert vector
	int sv = level_tri_verts[i].size();
	vofile.write(reinterpret_cast<const char *>(&sv), sizeof(sv));

	// Write out the vertex points
	std::set<int>::iterator s_it;
	for (s_it = level_tri_verts[i].begin(); s_it != level_tri_verts[i].end(); s_it++) {
	    point_t v;
	    VMOVE(v, verts_array[*s_it]);
	    vofile.write(reinterpret_cast<const char *>(&v[0]), sizeof(point_t));
	}

	vofile.close();
	bu_vls_free(&vfile);
    }


    // Write out the level triangles
    for (size_t i = 0; i <= tri_threshold; i++) {
	if (!level_tris[i].size())
	    continue;
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "tris_level_%zd", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), bu_vls_cstr(&tfile), NULL);

	std::ofstream tofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level tri vector
	int st = level_tris[i].size();
	tofile.write(reinterpret_cast<const char *>(&st), sizeof(st));

	// Write out the mapped triangle indices
	std::vector<int>::iterator s_it;
	for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
	    int vt[3];
	    vt[0] = tri_ind_map[faces_array[3*(*s_it)+0]];
	    vt[1] = tri_ind_map[faces_array[3*(*s_it)+1]];
	    vt[2] = tri_ind_map[faces_array[3*(*s_it)+2]];
	    tofile.write(reinterpret_cast<const char *>(&vt[0]), sizeof(vt));
	}

	tofile.close();
	bu_vls_free(&tfile);
    }

    // Now the high-fidelity mode data - Write out all faces, the boxes and
    // their face sets.  We calculate and write out the bounding boxes of each
    // set so the loading code need only read the values to prepare an RTree
    {
	size_t fcnt;
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "all_faces", NULL);
	std::ofstream ffile(dir, std::ios::out | std::ofstream::binary);
	fcnt = faces_cnt;
	ffile.write(reinterpret_cast<const char *>(&fcnt), sizeof(fcnt));
	ffile.write(reinterpret_cast<const char *>(&faces_array[0]), faces_cnt * 3 * sizeof(int));
	ffile.close();
    }
    std::unordered_map<short, std::unordered_map<short, std::unordered_map<short, std::vector<int>>>>::iterator b1_it;
    std::unordered_map<short, std::unordered_map<short, std::vector<int>>>::iterator b2_it;
    std::unordered_map<short, std::vector<int>>::iterator b3_it;
    size_t bcnt = 0;
    size_t tcnt = 0;
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "tri_sets", NULL);
	std::ofstream tsfile(dir, std::ios::out | std::ofstream::binary);
	if (!tsfile.is_open())
	    return;

	for (b1_it = tri_boxes.begin(); b1_it != tri_boxes.end(); b1_it++) {
	    for (b2_it = b1_it->second.begin(); b2_it != b1_it->second.end(); b2_it++) {
		for (b3_it = b2_it->second.begin(); b3_it != b2_it->second.end(); b3_it++) {
		    bcnt++;
		}
	    }
	}
	tsfile.write(reinterpret_cast<const char *>(&bcnt), sizeof(bcnt));

	for (b1_it = tri_boxes.begin(); b1_it != tri_boxes.end(); b1_it++) {
	    for (b2_it = b1_it->second.begin(); b2_it != b1_it->second.end(); b2_it++) {
		for (b3_it = b2_it->second.begin(); b3_it != b2_it->second.end(); b3_it++) {
		    // This shouldn't happen, but if the logic changes to somehow
		    // clear a triangle set we don't want an empty set
		    if (!b3_it->second.size())
			continue;

		    // Calculate the bbox
		    point_t min, max;
		    if (lod_trimesh_aabb(&min, &max, b3_it->second, faces_array, faces_cnt, verts_array, vert_cnt) < 0) {
			bu_log("Error - tri bbox calculation failed\n");
			tsfile.close();
			continue;
		    }
		    tsfile.write(reinterpret_cast<const char *>(&min[0]), sizeof(point_t));
		    tsfile.write(reinterpret_cast<const char *>(&max[0]), sizeof(point_t));

		    // Record how many triangles we've got, and then write the triangle
		    // indices themselves
		    tcnt = b3_it->second.size();
		    tsfile.write(reinterpret_cast<const char *>(&tcnt), sizeof(tcnt));
		    tsfile.write(reinterpret_cast<const char *>(&b3_it->second[0]), b3_it->second.size() * sizeof(int));

		}
	    }
	}

	tsfile.close();
    }
}

// Write out the generated LoD data to the BRL-CAD cache
void
POPState::cache()
{
    if (!hash) {
	is_valid = false;
	return;
    }

    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    if (!bu_file_exists(dir, NULL)) {
	lod_dir(dir);
    }

    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), NULL);
    if (bu_file_exists(dir, NULL)) {
	// If the directory already exists, we should already be good to go
	is_valid = true;
	return;
    } else {
	lod_dir(dir);
    }

    // Note a format, so we can detect if what's there isn't compatible
    // with what this logic expects (in anticipation of future changes
    // to the on-disk format).
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "format", NULL);
    FILE *fp = fopen(dir, "w");
    if (!fp) {
	bu_vls_free(&vkey);
	is_valid = false;
	return;
    }
    fprintf(fp, "1\n");
    fclose(fp);

    // Stash the min and max bounds, which will be used in decoding
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "minmax", NULL);
	std::ofstream minmaxfile(dir, std::ios::out | std::ofstream::binary);
	minmaxfile.write(reinterpret_cast<const char *>(&minx), sizeof(minx));
	minmaxfile.write(reinterpret_cast<const char *>(&miny), sizeof(miny));
	minmaxfile.write(reinterpret_cast<const char *>(&minz), sizeof(minz));
	minmaxfile.write(reinterpret_cast<const char *>(&maxx), sizeof(maxx));
	minmaxfile.write(reinterpret_cast<const char *>(&maxy), sizeof(maxy));
	minmaxfile.write(reinterpret_cast<const char *>(&maxz), sizeof(maxz));
	minmaxfile.close();
    }

    // Write out the set of all vertices
    {
	size_t ecnt;
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "all_verts", NULL);
	std::ofstream vfile(dir, std::ios::out | std::ofstream::binary);
	ecnt = vert_cnt;
	vfile.write(reinterpret_cast<const char *>(&ecnt), sizeof(ecnt));
	vfile.write(reinterpret_cast<const char *>(&verts_array[0]), vert_cnt * sizeof(point_t));
	vfile.close();
    }

    // Write edge-specific data
    cache_edge(&vkey);

    // Write triangle-specific data
    cache_tri(&vkey);

    bu_vls_free(&vkey);

    is_valid = true;
}

// Transfer coordinate into level precision
int
POPState::to_level(int val, int level)
{
    int ret = floor(val/double(PRECOMPUTED_MASKS[level]));
    //bu_log("to_level: %d, %d : %d\n", val, level, ret);
    return ret;
}

// Transfer coordinate into level-appropriate value
void
POPState::level_pnt(point_t *o, const point_t *p, int level)
{
    point_t in_pt;
    VMOVE(in_pt, *p);
    unsigned int x,y,z;
    x = floor(((*p)[X] - minx) / (maxx - minx) * USHRT_MAX);
    y = floor(((*p)[Y] - miny) / (maxy - miny) * USHRT_MAX);
    z = floor(((*p)[Z] - minz) / (maxz - minz) * USHRT_MAX);
    int lx = floor(x/double(PRECOMPUTED_MASKS[level]));
    int ly = floor(y/double(PRECOMPUTED_MASKS[level]));
    int lz = floor(z/double(PRECOMPUTED_MASKS[level]));
    // Back to point values
    fastf_t x1 = lx * double(PRECOMPUTED_MASKS[level]);
    fastf_t y1 = ly * double(PRECOMPUTED_MASKS[level]);
    fastf_t z1 = lz * double(PRECOMPUTED_MASKS[level]);
    fastf_t nx = ((x1 / USHRT_MAX) * (maxx - minx)) + minx;
    fastf_t ny = ((y1 / USHRT_MAX) * (maxy - miny)) + miny;
    fastf_t nz = ((z1 / USHRT_MAX) * (maxz - minz)) + minz;
    VSET(*o, nx, ny, nz);

    double poffset = DIST_PNT_PNT(*o, in_pt);
    if (poffset > (maxx - minx) && poffset > (maxy - miny) && poffset > (maxz - minz)) {
	bu_log("Error: %f %f %f -> %f %f %f\n", V3ARGS(in_pt), V3ARGS(*p));
	bu_log("bound: %f %f %f -> %f %f %f\n", minx, miny, minz, maxx, maxy, maxz);
	bu_log("  xyz: %d %d %d -> %d %d %d -> %f %f %f -> %f %f %f\n", x, y, z, lx, ly, lz, x1, y1, z1, nx, ny, nz);
    }
}

// Compares two coordinates for equality (on a given precision level)
bool
POPState::is_equal(rec r1, rec r2, int level)
{
    bool tl_x = (to_level(r1.x, level) == to_level(r2.x, level));
    bool tl_y = (to_level(r1.y, level) == to_level(r2.y, level));
    bool tl_z = (to_level(r1.z, level) == to_level(r2.z, level));
    return (tl_x && tl_y && tl_z);
}

// Checks whether an edge is degenerate (coordinates are the same on the given
// precision level)
bool
POPState::edge_degenerate(rec r0, rec r1, int level)
{
    return is_equal(r0, r1, level);
}

// Checks whether a triangle is degenerate (at least two coordinates are the
// same on the given precision level)
bool
POPState::tri_degenerate(rec r0, rec r1, rec r2, int level)
{
    return is_equal(r0, r1, level) || is_equal(r1, r2, level) || is_equal(r0, r2, level);
}

void
POPState::plot(const char *root)
{
    if (curr_level < 0)
	return;

    struct bu_vls name = BU_VLS_INIT_ZERO;
    FILE *plot_file = NULL;
    bu_vls_init(&name);
    if (!root) {
	bu_vls_sprintf(&name, "init_tris_level_%.2d.plot3", curr_level);
    } else {
	bu_vls_sprintf(&name, "%s_tris_level_%.2d.plot3", root, curr_level);
    }
    plot_file = fopen(bu_vls_addr(&name), "wb");

    if (curr_level <= max_tri_pop_level) {
	pl_color(plot_file, 0, 255, 0);

	for (int i = 0; i <= curr_level; i++) {
	    std::vector<int>::iterator s_it;
	    for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		int f_ind = *s_it;
		int v1ind, v2ind, v3ind;
		if (faces_array) {
		    v1ind = faces_array[3*f_ind+0];
		    v2ind = faces_array[3*f_ind+1];
		    v3ind = faces_array[3*f_ind+2];
		} else {
		    v1ind = lod_tris[3*f_ind+0];
		    v2ind = lod_tris[3*f_ind+1];
		    v3ind = lod_tris[3*f_ind+2];
		}
		point_t p1, p2, p3, o1, o2, o3;
		if (verts_array) {
		    VMOVE(p1, verts_array[v1ind]);
		    VMOVE(p2, verts_array[v2ind]);
		    VMOVE(p3, verts_array[v3ind]);
		} else {
		    VSET(p1, lod_tri_pnts[3*v1ind+0], lod_tri_pnts[3*v1ind+1], lod_tri_pnts[3*v1ind+2]);
		    VSET(p2, lod_tri_pnts[3*v2ind+0], lod_tri_pnts[3*v2ind+1], lod_tri_pnts[3*v2ind+2]);
		    VSET(p3, lod_tri_pnts[3*v3ind+0], lod_tri_pnts[3*v3ind+1], lod_tri_pnts[3*v3ind+2]);
		}
		// We iterate over the level i triangles, but our target level is
		// curr_level so we "decode" the points to that level, NOT level i
		level_pnt(&o1, &p1, curr_level);
		level_pnt(&o2, &p2, curr_level);
		level_pnt(&o3, &p3, curr_level);
		pdv_3move(plot_file, o1);
		pdv_3cont(plot_file, o2);
		pdv_3cont(plot_file, o3);
		pdv_3cont(plot_file, o1);
	    }
	}

    } else {
	RTree<size_t, double, 3>::Iterator tree_it;
	tri_rtree.GetFirst(tree_it);
	while (!tree_it.IsNull()) {
	    size_t i = *tree_it;
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
	    pl_color_buc(plot_file, &c);
	    for (size_t j = 0; j < tri_sets[i].size(); j++) {
		int v1ind, v2ind, v3ind;
		point_t p1, p2, p3;
		v1ind = lod_tris[3*tri_sets[i][j]+0];
		v2ind = lod_tris[3*tri_sets[i][j]+1];
		v3ind = lod_tris[3*tri_sets[i][j]+2];
		VSET(p1, lod_tri_pnts[3*v1ind+0], lod_tri_pnts[3*v1ind+1], lod_tri_pnts[3*v1ind+2]);
		VSET(p2, lod_tri_pnts[3*v2ind+0], lod_tri_pnts[3*v2ind+1], lod_tri_pnts[3*v2ind+2]);
		VSET(p3, lod_tri_pnts[3*v3ind+0], lod_tri_pnts[3*v3ind+1], lod_tri_pnts[3*v3ind+2]);
		pdv_3move(plot_file, p1);
		pdv_3cont(plot_file, p2);
		pdv_3cont(plot_file, p3);
		pdv_3cont(plot_file, p1);
	    }
	    ++tree_it;
	}
    }

    fclose(plot_file);

    if (!root) {
	bu_vls_sprintf(&name, "init_edges_level_%.2d.plot3", curr_level);
    } else {
	bu_vls_sprintf(&name, "%s_edges_level_%.2d.plot3", root, curr_level);
    }
    plot_file = fopen(bu_vls_addr(&name), "wb");

    if (curr_level <= max_edge_pop_level) {
    	pl_color(plot_file, 0, 255, 0);

	for (int i = 0; i <= curr_level; i++) {
	    std::vector<int>::iterator s_it;
	    if (i == 0) {
		bu_log("edge cnt: %zd\n", level_edges[i].size());
	    }
	    for (s_it = level_edges[i].begin(); s_it != level_edges[i].end(); s_it++) {
		int v1ind = lod_edges[2*(*s_it)+0];
		int v2ind = lod_edges[2*(*s_it)+1];
		point_t p1, p2, o1, o2;
		VSET(p1, lod_edge_pnts[3*v1ind+0], lod_edge_pnts[3*v1ind+1], lod_edge_pnts[3*v1ind+2]);
		VSET(p2, lod_edge_pnts[3*v2ind+0], lod_edge_pnts[3*v2ind+1], lod_edge_pnts[3*v2ind+2]);
		if (i == 0) {
		    bu_log("Edge_pts: %f %f %f -> %f %f %f\n", V3ARGS(p1), V3ARGS(p2));
		}

		// We iterate over the level i edges, but our target level is
		// curr_level so we "decode" the points to that level, NOT level i
		level_pnt(&o1, &p1, curr_level);
		level_pnt(&o2, &p2, curr_level);
		if (i == 0) {
		    bu_log("snapped edge_pts: %f %f %f -> %f %f %f\n", V3ARGS(o1), V3ARGS(o2));
		}
		pdv_3move(plot_file, o1);
		pdv_3cont(plot_file, o2);
	    }
	}

    } else {
	RTree<size_t, double, 3>::Iterator tree_it;
	edge_rtree.GetFirst(tree_it);
	while (!tree_it.IsNull()) {
	    size_t i = *tree_it;
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
	    pl_color_buc(plot_file, &c);
	    for (size_t j = 0; j < edge_sets[i].size(); j++) {
		int v1ind, v2ind;
		point_t p1, p2;
		v1ind = edges[j].v[0];
		v2ind = edges[j].v[1];
		VSET(p1, lod_edge_pnts[3*v1ind+0], lod_edge_pnts[3*v1ind+1], lod_edge_pnts[3*v1ind+2]);
		VSET(p2, lod_edge_pnts[3*v2ind+0], lod_edge_pnts[3*v2ind+1], lod_edge_pnts[3*v2ind+2]);
		pdv_3move(plot_file, p1);
		pdv_3cont(plot_file, p2);
	    }
	    ++tree_it;
	}
 
    }

    bu_vls_free(&name);
}


struct bg_mesh_lod_internal {
    POPState *s;
};

extern "C" unsigned long long
bg_mesh_lod_cache(const point_t *v, int vcnt, int *faces, int fcnt)
{
    unsigned long long key = 0;

    if (!v || !vcnt || !faces || !fcnt)
	return 0;

    POPState p(v, vcnt, faces, fcnt);
    if (!p.is_valid)
	return 0;

    key = p.hash;

    return key;
}

extern "C" struct bg_mesh_lod *
bg_mesh_lod_init(unsigned long long key)
{
    if (!key)
	return NULL;

    POPState *p = new POPState(key);
    if (!p)
	return NULL;

    if (!p->is_valid) {
	delete p;
	return NULL;
    }

    struct bg_mesh_lod *l = NULL;
    BU_GET(l, struct bg_mesh_lod);
    BU_GET(l->i, struct bg_mesh_lod_internal);
    l->i->s = p;

    return l;
}

extern "C" int
bg_mesh_lod_view(struct bg_mesh_lod *l, struct bview *v, int scale)
{

    if (!l)
	return -1;
    if (!v)
	return l->i->s->curr_level;

    int vscale = l->i->s->curr_level + scale;
    vscale = (vscale < 0) ? 0 : vscale;
    vscale = (vscale >= POP_MAXLEVEL) ? POP_MAXLEVEL-1 : vscale;

    return vscale;
}

extern "C" int
bg_mesh_lod_level(struct bg_mesh_lod *l, int level)
{
    if (!l)
	return -1;
    if (level == -1)
	return l->i->s->curr_level;

    return -1;
}

// TODO - need to have awareness of whether we're in wireframe
// or shaded data mode - we ideally don't want to load both, since
// it's a waste of memory, but some of these functions will need
// to know to handle that.  In particular, lod_faces will be a
// no-op if the lod container has wireframe data.
extern "C" size_t
bg_mesh_lod_verts(const point_t **v, struct bg_mesh_lod *l)
{
    if (!l)
	return 0;
    if (!v)
	return l->i->s->lod_tri_pnts.size();

    (*v) = (const point_t *)l->i->s->lod_tri_pnts.data();
    return l->i->s->lod_tri_pnts.size();
}

extern "C" void
bg_mesh_lod_vsnap(point_t *o, const point_t *v, struct bg_mesh_lod *l)
{
    if (!l || !v || !o)
	return;

    l->i->s->level_pnt(o, v, l->i->s->curr_level);
}

extern "C" size_t
bg_mesh_lod_faces(const int **f, struct bg_mesh_lod *l)
{
    if (!l)
	return 0;
    if (!f)
	return l->i->s->lod_tris.size();

    (*f) = (const int *)l->i->s->lod_tris.data();
    return l->i->s->lod_tris.size();
}

extern "C" void
bg_mesh_lod_destroy(struct bg_mesh_lod *l)
{
    if (!l)
	return;

    delete l->i->s;
    BU_PUT(l->i, struct bg_mesh_lod_internal);
    BU_PUT(l, struct bg_mesh_lod);
}


extern "C" void
bg_mesh_lod_clear(unsigned long long key)
{
    if (key == 0)
	bu_log("Remove all\n");
}

extern "C" int
bg_lod_elist(struct bu_list *elist, struct bview *v, struct bg_mesh_lod *l, const char *pname)
{
    int ecnt = 0;
    if (!l)
	return -1;

    // TODO
    if (elist || v)
	return -1;

    // For debugging purposes, write out plot files of each level
    POPState *s = l->i->s;
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	s->set_level(i);
	s->plot(pname);
    }

    for (int i = POP_MAXLEVEL - 1; i >= 0; i--) {
	s->set_level(i);
	s->plot("shrunk");
    }

    return ecnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
