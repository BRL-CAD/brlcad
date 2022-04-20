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

#include "bio.h"

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/time.h"
#include "bv/plot3.h"
#include "bg/lod.h"
#include "bg/plane.h"
#include "bg/sat.h"
#include "bg/trimesh.h"

static void
obj_bb(int *have_objs, vect_t *min, vect_t *max, struct bv_scene_obj *s, struct bview *v)
{
    vect_t minus, plus;
    if (bv_scene_obj_bound(s, v)) {
	*have_objs = 1;
	minus[X] = s->s_center[X] - s->s_size;
	minus[Y] = s->s_center[Y] - s->s_size;
	minus[Z] = s->s_center[Z] - s->s_size;
	VMIN(*min, minus);
	plus[X] = s->s_center[X] + s->s_size;
	plus[Y] = s->s_center[Y] + s->s_size;
	plus[Z] = s->s_center[Z] + s->s_size;
	VMAX(*max, plus);
    }
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *sc = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	obj_bb(have_objs, min, max, sc, v);
    }
}

void
bg_view_obb(struct bview *v)
{
    if (!v || !v->gv_width || !v->gv_height)
	return;

    // Get the radius of the scene.
    plane_t p;
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    vect_t min, max, work;
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    int have_objs = 0;
    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	obj_bb(&have_objs, &min, &max, g, v);
    }
    struct bu_ptbl *sol = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    if (so != sol) {
	for (size_t i = 0; i < BU_PTBL_LEN(sol); i++) {
	    struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(sol, i);
	    obj_bb(&have_objs, &min, &max, g, v);
	}
    }
    if (have_objs) {
	VADD2SCALE(sbbc, max, min, 0.5);
	VSUB2SCALE(work, max, min, 0.5);
	radius = MAGNITUDE(work);
    }

    // Get the model space points for the mid points of the top and right edges
    // of the view.  If we don't have a width or height, we will use the
    // existing min and max since we don't have a "screen" to base the box on
    int w = v->gv_width;
    int h = v->gv_height;
    int x = (int)(w * 0.5);
    int y = (int)(h * 0.5);
    //bu_log("w,h,x,y: %d %d %d %d\n", w,h, x, y);
    fastf_t x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, xc = 0.0, yc = 0.0;
    bv_screen_to_view(v, &x1, &y1, x, h);
    bv_screen_to_view(v, &x2, &y2, w, y);
    bv_screen_to_view(v, &xc, &yc, x, y);
    //bu_log("x1,y1: %f %f\n", x1, y1);
    //bu_log("x2,y2: %f %f\n", x2, y2);
    //bu_log("xc,yc: %f %f\n", xc, yc);
    point_t vp1, vp2, vc, ep1, ep2, ec;
    VSET(vp1, x1, y1, 0);
    VSET(vp2, x2, y2, 0);
    VSET(vc, xc, yc, 0);
    MAT4X3PNT(ep1, v->gv_view2model, vp1);
    MAT4X3PNT(ep2, v->gv_view2model, vp2);
    MAT4X3PNT(ec, v->gv_view2model, vc);
    //bu_log("view center: %f %f %f\n", V3ARGS(ec));
    //bu_log("edge point 1: %f %f %f\n", V3ARGS(ep1));
    //bu_log("edge point 2: %f %f %f\n", V3ARGS(ep2));

    // Need the direction vector - i.e., where the camera is looking.  Got this
    // trick from the libged nirt code...
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, -1.0);

    // Box center is the closest point to the view center on the plane defined
    // by the scene's center and the view dir
    bg_plane_pt_nrml(&p, sbbc, dir);
    fastf_t pu, pv;
    bg_plane_closest_pt(&pu, &pv, p, ec);
    bg_plane_pt_at(&v->obb_center, p, pu, pv);

    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(v->obb_extent1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(v->obb_extent2, ep1, ec);
    VSUB2(v->obb_extent3, ep2, ec);

#if 0
    // For debugging purposes, construct an arb
    point_t arb[8];
    // 1 - c - e1 - e2 - e3
    VSUB2(arb[0], v->obb_center, v->obb_extent1);
    VSUB2(arb[0], arb[0], v->obb_extent2);
    VSUB2(arb[0], arb[0], v->obb_extent3);
    // 2 - c - e1 - e2 + e3
    VSUB2(arb[1], v->obb_center, v->obb_extent1);
    VSUB2(arb[1], arb[1], v->obb_extent2);
    VADD2(arb[1], arb[1], v->obb_extent3);
    // 3 - c - e1 + e2 + e3
    VSUB2(arb[2], v->obb_center, v->obb_extent1);
    VADD2(arb[2], arb[2], v->obb_extent2);
    VADD2(arb[2], arb[2], v->obb_extent3);
    // 4 - c - e1 + e2 - e3
    VSUB2(arb[3], v->obb_center, v->obb_extent1);
    VADD2(arb[3], arb[3], v->obb_extent2);
    VSUB2(arb[3], arb[3], v->obb_extent3);
    // 1 - c + e1 - e2 - e3
    VADD2(arb[4], v->obb_center, v->obb_extent1);
    VSUB2(arb[4], arb[4], v->obb_extent2);
    VSUB2(arb[4], arb[4], v->obb_extent3);
    // 2 - c + e1 - e2 + e3
    VADD2(arb[5], v->obb_center, v->obb_extent1);
    VSUB2(arb[5], arb[5], v->obb_extent2);
    VADD2(arb[5], arb[5], v->obb_extent3);
    // 3 - c + e1 + e2 + e3
    VADD2(arb[6], v->obb_center, v->obb_extent1);
    VADD2(arb[6], arb[6], v->obb_extent2);
    VADD2(arb[6], arb[6], v->obb_extent3);
    // 4 - c + e1 + e2 - e3
    VADD2(arb[7], v->obb_center, v->obb_extent1);
    VADD2(arb[7], arb[7], v->obb_extent2);
    VSUB2(arb[7], arb[7], v->obb_extent3);

    bu_log("center: %f %f %f\n", V3ARGS(v->obb_center));
    bu_log("e1: %f %f %f\n", V3ARGS(v->obb_extent1));
    bu_log("e2: %f %f %f\n", V3ARGS(v->obb_extent2));
    bu_log("e3: %f %f %f\n", V3ARGS(v->obb_extent3));

    bu_log("in obb.s arb8 %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
	    V3ARGS(arb[0]), V3ARGS(arb[1]), V3ARGS(arb[2]), V3ARGS(arb[3]), V3ARGS(arb[4]), V3ARGS(arb[5]), V3ARGS(arb  [6]), V3ARGS(arb[7]));
#endif
}

#define POP_MAXLEVEL 16
#define POP_CACHEDIR ".POPLoD"
#define MBUMP 1.01

typedef int (*draw_clbk_t)(void *, struct bv_mesh_lod_info *);

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

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};

class POPState {
    public:

	// Create cached data (doesn't create a usable container)
	POPState(const point_t *v, size_t vcnt, int *faces, size_t fcnt);

	// Load cached data (DOES create a usable container)
	POPState(unsigned long long key);

	// Cleanup
	~POPState();

	// Based on a view size, recommend a level
	int get_level(fastf_t len);

	// Load/unload data level
	void set_level(int level);

	// Shrink memory usage (level set routines will have to do more work
	// after this is run, but the POPState is still viable).  Used after a
	// client code has done all that is needed with the level data, such as
	// generating an OpenGL display list, but isn't done with the object.
	void shrink_memory();

	// Get the "current" position of a point, given its level
	void level_pnt(point_t *o, const point_t *p, int level);

	// Debugging
	void plot(const char *root);

	// Active faces needed by the current LoD (indexes into lod_tri_pnts).
	std::vector<int> lod_tris;

	// This is where we store the active points - i.e., those needed for
	// the current LoD.  When initially creating the breakout from BoT data
	// we calculate all levels, but the goal is to not hold in memory any
	// more than we need to support the LoD drawing.  lod_tris will index
	// into lod_tri_pnts.
	std::vector<fastf_t> lod_tri_pnts;
	std::vector<fastf_t> lod_tri_pnts_snapped;

	// Current level of detail information loaded into nfaces/npnts
	int curr_level = -1;

	// Force a data reload even if the level hasn't changed (i.e. undo a
	// shrink memory operation when level is set.)
	bool force_update = false;

	// Maximum level for which POP info is defined.  Above that level,
	// need to shift to full rendering
	int max_tri_pop_level = 0;

	// Used by calling functions to detect initialization errors
	bool is_valid = false;

	// Content based hash key
	unsigned long long hash;

	// Drawing trigger
	void draw(void *ctx, struct bv_scene_obj *s);
	void set_callback(draw_clbk_t clbk);

	// Parent container
	struct bg_mesh_lod *lod;

	// Bounding box of original mesh
	point_t bbmin, bbmax;

    private:

	void tri_process();

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	// Clamping of points to various detail levels
	int to_level(int val, int level);

	// Snap value to level value
	fastf_t snap(fastf_t val, fastf_t min, fastf_t max, int level);

	// Check if two points are equal at a clamped level
	bool is_equal(rec r1, rec r2, int level);

	// Degeneracy test for triangles
	bool tri_degenerate(rec r0, rec r1, rec r2, int level);

	std::vector<unsigned short> PRECOMPUTED_MASKS;

	// Write data out to cache (only used during initialization from
	// external data)
	void cache();
	void cache_tri(struct bu_vls *vkey);

	// Specific loading and unloading methods
	void tri_pop_load(int start_level, int level);
	void tri_pop_trim(int level);

	// Processing containers used for initial triangle data characterization
	std::vector<size_t> tri_ind_map;
	std::vector<size_t> vert_tri_minlevel;
	std::map<size_t, std::set<size_t>> level_tri_verts;
	std::vector<std::vector<size_t>> level_tris;
	size_t tri_threshold = 0;

	// Pointers to original input data
	size_t vert_cnt = 0;
	const point_t *verts_array = NULL;
	size_t faces_cnt = 0;
	int *faces_array = NULL;

	// Function to use when doing draw operations
	draw_clbk_t draw_clbk;
};

void
POPState::tri_process()
{
    // Until we prove otherwise, all edges are assumed to appear only at the
    // last level (and consequently, their vertices are only needed then).  Set
    // the level accordingly.
    vert_tri_minlevel.reserve(vert_cnt);
    for (size_t i = 0; i < vert_cnt; i++) {
	vert_tri_minlevel.push_back(POP_MAXLEVEL - 1);
    }

    // Reserve memory for level containers
    level_tris.reserve(POP_MAXLEVEL);
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	level_tris.push_back(std::vector<size_t>(0));
    }

    // Walk the triangles and perform the LoD characterization
    for (size_t i = 0; i < faces_cnt; i++) {
	rec triangle[3];
	// Transform triangle vertices
	for (size_t j = 0; j < 3; j++) {
	    triangle[j].x = floor((verts_array[faces_array[3*i+j]][X] - minx) / (maxx - minx) * USHRT_MAX);
	    triangle[j].y = floor((verts_array[faces_array[3*i+j]][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    triangle[j].z = floor((verts_array[faces_array[3*i+j]][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}

	// Find the pop up level for this triangle (i.e., when it will first
	// appear as we step up the zoom levels.)
	size_t level = POP_MAXLEVEL - 1;
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
	for (size_t j = 0; j < 3; j++) {
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
    for (size_t i = 0; i < vert_cnt; i++) {
	tri_ind_map.push_back(i);
    }
    size_t vind = 0;
    std::map<size_t, std::set<size_t>>::iterator l_it;
    std::set<size_t>::iterator s_it;
    for (l_it = level_tri_verts.begin(); l_it != level_tri_verts.end(); l_it++) {
	for (s_it = l_it->second.begin(); s_it != l_it->second.end(); s_it++) {
	    tri_ind_map[*s_it] = vind;
	    vind++;
	}
    }

    // Beyond a certain depth, there is little benefit to the POP process.  If
    // we check the count of level_tris, we will find a level at which most of
    // the triangles are active. 
    // TODO: Not clear yet when the tradeoff between memory and the work of LoD
    // point snapping trades off - 0.66 is just a guess.
    size_t trisum = 0;
    size_t faces_array_cnt2 = (size_t)((fastf_t)faces_cnt * 0.66);
    for (size_t i = 0; i < level_tris.size(); i++) {
	trisum += level_tris[i].size();
	if (trisum > faces_array_cnt2) {
	    // If we're using two thirds of the triangles, this is our
	    // threshold level.  If we've got ALL the triangles, back
	    // down one level.
	    if (trisum < (size_t)faces_cnt) {
		tri_threshold = i;
	    } else {
		// It shouldn't happen, but to be sure handle the case where we
		// get here with i == 0
		tri_threshold = (i) ? i - 1 : 0;
	    }
	    break;
	}
    }
    //bu_log("Triangle threshold level: %zd\n", tri_threshold);
}

POPState::POPState(const point_t *v, size_t vcnt, int *faces, size_t fcnt)
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

    // Calculate the full mesh bounding box for later use
    bg_trimesh_aabb(&bbmin, &bbmax, faces_array, faces_cnt, verts_array, vert_cnt);

    // Find our min and max values, initialize levels
    for (size_t i = 0; i < vcnt; i++) {
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

    // Characterize triangle faces
    tri_process();

    // We're now ready to write out the data
    is_valid = true;
    cache();

    if (!is_valid)
	return;

#if 0
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %zu count: %zu\n", i, level_tris[i].size());
    }

    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %zu count: %zu\n", i, level_tri_verts[i].size());
    }
#endif
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
    level_tris.reserve(POP_MAXLEVEL);
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	level_tris.push_back(std::vector<size_t>(0));
    }

    // Read in min/max bounds
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "minmax", NULL);
	std::ifstream minmaxfile(dir, std::ios::in | std::ofstream::binary);
	minmaxfile.read(reinterpret_cast<char *>(&bbmin), sizeof(bbmin));
	minmaxfile.read(reinterpret_cast<char *>(&bbmax), sizeof(bbmax));
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
	bu_vls_free(&vfile);
    }

    // Read in the zero level vertices and triangles
    set_level(0);

    // All set - ready for LoD
    bu_vls_free(&vkey);
    is_valid = 1;
}

POPState::~POPState()
{

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
	size_t vicnt = 0;
	vifile.read(reinterpret_cast<char *>(&vicnt), sizeof(vicnt));
	for (size_t j = 0; j < vicnt; j++) {
	    point_t nv;
	    vifile.read(reinterpret_cast<char *>(&nv), sizeof(point_t));
	    for (size_t k = 0; k < 3; k++) {
		lod_tri_pnts.push_back(nv[k]);
	    }
	    level_tri_verts[i].insert(lod_tri_pnts.size() - 1);
	}
	vifile.close();
	bu_vls_free(&vfile);
    }
    // Re-snap all vertices loaded at the new level
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.reserve(lod_tri_pnts.size());
    for (size_t i = 0; i < lod_tri_pnts.size()/3; i++) {
	point_t p, sp;
	VSET(p, lod_tri_pnts[3*i+0], lod_tri_pnts[3*i+1], lod_tri_pnts[3*i+2]);
	level_pnt(&sp, &p, level);
	for (int k = 0; k < 3; k++) {
	    lod_tri_pnts_snapped.push_back(sp[k]);
	}
    }

    // Read in the level triangles
    for (int i = start_level+1; i <= level; i++) {
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "tris_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&tfile), NULL);
	if (!bu_file_exists(dir, NULL))
	    continue;

	std::ifstream tifile(dir, std::ios::in | std::ofstream::binary);
	size_t ticnt = 0;
	tifile.read(reinterpret_cast<char *>(&ticnt), sizeof(ticnt));
	for (size_t j = 0; j < ticnt; j++) {
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
POPState::shrink_memory()
{
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	level_tris[i].clear();
	level_tris[i].shrink_to_fit();
    }
    lod_tri_pnts.clear();
    lod_tri_pnts.shrink_to_fit();
    lod_tris.clear();
    lod_tris.shrink_to_fit();
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.shrink_to_fit();
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

    // Re-snap all vertices loaded at the new level
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.reserve(lod_tri_pnts.size());
    for (size_t i = 0; i < lod_tri_pnts.size()/3; i++) {
	point_t p, sp;
	VSET(p, lod_tri_pnts[3*i+0], lod_tri_pnts[3*i+1], lod_tri_pnts[3*i+2]);
	level_pnt(&sp, &p, level);
	for (int k = 0; k < 3; k++) {
	    lod_tri_pnts_snapped.push_back(sp[k]);
	}
    }
}

int
POPState::get_level(fastf_t vlen)
{
    fastf_t delta = 0.01*vlen;
    point_t bmin, bmax;
    VSET(bmin, minx, miny, minz);
    VSET(bmax, maxx, maxy, maxz);
    fastf_t bdiag = DIST_PNT_PNT(bmin, bmax);
    for (int lev = 0; lev < POP_MAXLEVEL; lev++) {
	fastf_t diag_slice = bdiag/pow(2,lev);
	if (diag_slice < delta) {
	    return lev;
	}
    }
    return POP_MAXLEVEL - 1;
}

// NOTE: at some point it may be worth investigating using bu_open_mapped_file
// and friends for this, depending on what profiling shows in real-world usage.
void
POPState::set_level(int level)
{
    // If we're already there and we're not undoing a memshrink, no work to do
    if (level == curr_level && !force_update)
	return;

    // If we're doing a forced update, it's like starting from
    // scratch - reset to 0
    if (force_update) {
	force_update = false;
	set_level(0);
	set_level(level);
    }

//    int64_t start, elapsed;
//    fastf_t seconds;
//    start = bu_gettime();

    // Triangles

    // If we need to pull more data, do so
    if (level > curr_level && level <= max_tri_pop_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_load(curr_level, level);
	}
    }

    // If we need to trim back the POP data, do that
    if (level < curr_level && level <= max_tri_pop_level && curr_level <= max_tri_pop_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_trim(level);
	}
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

	// Read the complete tris and verts for full drawing
	lod_tri_pnts_snapped.clear();
	lod_tri_pnts.clear();
	lod_tris.clear();
	{
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
	    bu_vls_free(&vkey);
	}

    }

    //elapsed = bu_gettime() - start;
    //seconds = elapsed / 1000000.0;
    //bu_log("lod set_level(%d): %f sec\n", level, seconds);

    curr_level = level;
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
	size_t sv = level_tri_verts[i].size();
	vofile.write(reinterpret_cast<const char *>(&sv), sizeof(sv));

	// Write out the vertex points
	std::set<size_t>::iterator s_it;
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
	size_t st = level_tris[i].size();
	tofile.write(reinterpret_cast<const char *>(&st), sizeof(st));

	// Write out the mapped triangle indices
	std::vector<size_t>::iterator s_it;
	for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
	    int vt[3];
	    vt[0] = (int)tri_ind_map[faces_array[3*(*s_it)+0]];
	    vt[1] = (int)tri_ind_map[faces_array[3*(*s_it)+1]];
	    vt[2] = (int)tri_ind_map[faces_array[3*(*s_it)+2]];
	    tofile.write(reinterpret_cast<const char *>(&vt[0]), sizeof(vt));
	}

	tofile.close();
	bu_vls_free(&tfile);
    }

    // Now the high-fidelity mode data - Write out all vertices, faces, the
    // boxes and their face sets.  We calculate and write out the bounding
    // boxes of each set so the loading code need only read the values to
    // prepare an RTree
    //
    // TODO - in an ideal world, if this RTree hierarchy is useful (which isn't
    // clear yet) we would do it not simply for the full scale mesh but
    // whenever we're having to deal with an active triangle set large enough
    // that there are benefits to not holding the whole thing in memory and
    // processing it (for sufficiently large meshes some of the deeper LoD
    // levels will cause problems similar to the big mesh itself.)  If that
    // proves desirable we'll need to recast this in terms of a "per-level"
    // setup and have an active tri count trigger.

    // Write out the set of all vertices
    {
	size_t ecnt;
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "all_verts", NULL);
	std::ofstream vfile(dir, std::ios::out | std::ofstream::binary);
	ecnt = vert_cnt;
	vfile.write(reinterpret_cast<const char *>(&ecnt), sizeof(ecnt));
	vfile.write(reinterpret_cast<const char *>(&verts_array[0]), vert_cnt * sizeof(point_t));
	vfile.close();
    }

    // Write out the set of all triangles
    {
	size_t fcnt;
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(vkey), "all_faces", NULL);
	std::ofstream ffile(dir, std::ios::out | std::ofstream::binary);
	fcnt = faces_cnt;
	ffile.write(reinterpret_cast<const char *>(&fcnt), sizeof(fcnt));
	ffile.write(reinterpret_cast<const char *>(&faces_array[0]), faces_cnt * 3 * sizeof(int));
	ffile.close();
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
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, NULL);
    if (!bu_file_exists(dir, NULL)) {
	lod_dir(dir);
    }

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

    // Stash the original mesh bbox and the min and max bounds, which will be used in decoding
    {
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "minmax", NULL);
	std::ofstream minmaxfile(dir, std::ios::out | std::ofstream::binary);
	minmaxfile.write(reinterpret_cast<const char *>(&bbmin), sizeof(bbmin));
	minmaxfile.write(reinterpret_cast<const char *>(&bbmax), sizeof(bbmax));
	minmaxfile.write(reinterpret_cast<const char *>(&minx), sizeof(minx));
	minmaxfile.write(reinterpret_cast<const char *>(&miny), sizeof(miny));
	minmaxfile.write(reinterpret_cast<const char *>(&minz), sizeof(minz));
	minmaxfile.write(reinterpret_cast<const char *>(&maxx), sizeof(maxx));
	minmaxfile.write(reinterpret_cast<const char *>(&maxy), sizeof(maxy));
	minmaxfile.write(reinterpret_cast<const char *>(&maxz), sizeof(maxz));
	minmaxfile.close();
    }

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

fastf_t
POPState::snap(fastf_t val, fastf_t min, fastf_t max, int level)
{
    unsigned int vf = floor((val - min) / (max - min) * USHRT_MAX);
    int lv = floor(vf/double(PRECOMPUTED_MASKS[level]));
    unsigned int vc = ceil((val - min) / (max - min) * USHRT_MAX);
    int hc = ceil(vc/double(PRECOMPUTED_MASKS[level]));
    fastf_t v = ((fastf_t)lv + (fastf_t)hc)*0.5 * double(PRECOMPUTED_MASKS[level]);
    fastf_t vs = ((v / USHRT_MAX) * (max - min)) + min;
    return vs;
}

// Transfer coordinate into level-appropriate value
void
POPState::level_pnt(point_t *o, const point_t *p, int level)
{
    fastf_t nx = snap((*p)[X], minx, maxx, level);
    fastf_t ny = snap((*p)[Y], miny, maxy, level);
    fastf_t nz = snap((*p)[Z], minz, maxz, level);
    VSET(*o, nx, ny, nz);
#if 0
    double poffset = DIST_PNT_PNT(*o, *p);
    if (poffset > (maxx - minx) && poffset > (maxy - miny) && poffset > (maxz - minz)) {
	bu_log("Error: %f %f %f -> %f %f %f\n", V3ARGS(*p), V3ARGS(*o));
	bu_log("bound: %f %f %f -> %f %f %f\n", minx, miny, minz, maxx, maxy, maxz);
    }
#endif
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

// Checks whether a triangle is degenerate (at least two coordinates are the
// same on the given precision level)
bool
POPState::tri_degenerate(rec r0, rec r1, rec r2, int level)
{
    return is_equal(r0, r1, level) || is_equal(r1, r2, level) || is_equal(r0, r2, level);
}

void
POPState::draw(void *ctx, struct bv_scene_obj *s)
{
    if (draw_clbk) {
	struct bv_mesh_lod_info info;
	info.s = s;
	info.fset_cnt = 0;
	info.fset = NULL;
	info.fcnt = (int)lod_tris.size()/3;
	info.faces = lod_tris.data();
	info.points_orig = (const point_t *)lod_tri_pnts.data();
	if (curr_level <= max_tri_pop_level) {
	    info.points = (const point_t *)lod_tri_pnts_snapped.data();
	} else {
	    info.points = (const point_t *)lod_tri_pnts.data();
	}
	info.face_normals = NULL;
	info.normals = NULL;
	info.lod = lod;
	(*draw_clbk)(ctx, &info);
    }
}

void
POPState::set_callback(draw_clbk_t clbk)
{
    draw_clbk = clbk;
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
	    std::vector<size_t>::iterator s_it;
	    for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		size_t f_ind = *s_it;
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
	for (size_t i = 0; i < lod_tris.size() / 3; i++) {
	    int v1ind, v2ind, v3ind;
	    point_t p1, p2, p3;
	    v1ind = lod_tris[3*i+0];
	    v2ind = lod_tris[3*i+1];
	    v3ind = lod_tris[3*i+2];
	    VSET(p1, lod_tri_pnts[3*v1ind+0], lod_tri_pnts[3*v1ind+1], lod_tri_pnts[3*v1ind+2]);
	    VSET(p2, lod_tri_pnts[3*v2ind+0], lod_tri_pnts[3*v2ind+1], lod_tri_pnts[3*v2ind+2]);
	    VSET(p3, lod_tri_pnts[3*v3ind+0], lod_tri_pnts[3*v3ind+1], lod_tri_pnts[3*v3ind+2]);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p2);
	    pdv_3cont(plot_file, p3);
	    pdv_3cont(plot_file, p1);
	}
    }

    fclose(plot_file);

    bu_vls_free(&name);
}


struct bg_mesh_lod_internal {
    POPState *s;
};

extern "C" unsigned long long
bg_mesh_lod_cache(const point_t *v, size_t vcnt, int *faces, size_t fcnt)
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

extern "C" struct bv_mesh_lod_info *
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

    struct bv_mesh_lod_info *i = NULL;
    BU_GET(i, struct bv_mesh_lod_info);
    BU_GET(i->lod, struct bg_mesh_lod);
    BU_GET(((struct bg_mesh_lod *)i->lod)->i, struct bg_mesh_lod_internal);
    ((struct bg_mesh_lod *)i->lod)->i->s = p;
    p->lod = (struct bg_mesh_lod *)i->lod;

    VMOVE(i->bmin, p->bbmin);
    VMOVE(i->bmax, p->bbmax);

    return i;
}

extern "C" void
bg_mesh_lod_destroy(struct bv_mesh_lod_info *i)
{
    if (!i)
	return;

    struct bg_mesh_lod *l = (struct bg_mesh_lod *)i->lod;
    delete l->i->s;
    BU_PUT(l->i, struct bg_mesh_lod_internal);
    BU_PUT(l, struct bg_mesh_lod);
    BU_PUT(i, struct bg_mesh_lod_info);
}

extern "C" int
bg_mesh_lod_view(struct bg_mesh_lod *l, struct bview *v, int reset)
{

    if (!l)
	return -1;
    if (!v)
	return l->i->s->curr_level;

    POPState *s = l->i->s;
    int vscale = (int)((double)s->get_level(v->gv_size) * v->gv_s->lod_scale);
    vscale = (vscale < 0) ? 0 : vscale;
    vscale = (vscale >= POP_MAXLEVEL) ? POP_MAXLEVEL-1 : vscale;
    bg_mesh_lod_level(l, vscale, reset);
    return vscale;
}

extern "C" int
bg_mesh_lod_level(struct bg_mesh_lod *l, int level, int reset)
{
    if (!l)
	return -1;
    POPState *s = l->i->s;
    if (!s)
	return -1;
    if (level < 0)
	return s->curr_level;

    s->force_update = (reset) ? true : false;

    s->set_level(level);

    return s->curr_level;
}

static void
dlist_stale(struct bv_scene_obj *s)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	dlist_stale(cg);
    }
    s->s_dlist_stale = 1;
}

extern "C" int
bg_mesh_lod_update(struct bv_scene_obj *s, struct bview *v, int UNUSED(offset))
{
    if (!s || !v)
	return -1;
    struct bv_mesh_lod_info *i = (struct bv_mesh_lod_info *)s->draw_data;
    if (!i)
	return -1;
    struct bg_mesh_lod *l = (struct bg_mesh_lod *)i->lod;
    if (!l)
	return -1;

    POPState *sp = l->i->s;

    VMOVE(s->bmin, sp->bbmin);
    VMOVE(s->bmax, sp->bbmax);

    int old_level = sp->curr_level;
    int new_level = old_level;

    // If the object is not visible in the scene, don't change the data
    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(s->bmin), V3ARGS(s->bmax));
    if (bg_sat_aabb_obb(s->bmin, s->bmax, v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3)) {
	new_level = bg_mesh_lod_view(l, v, 0);
    }

    if (old_level != new_level) {
	dlist_stale(s);
    }

    return new_level;
}

extern "C" void
bg_mesh_lod_memshrink(struct bv_scene_obj *s)
{
    if (!s)
	return;
    struct bv_mesh_lod_info *i = (struct bv_mesh_lod_info *)s->draw_data;
    if (!i)
	return;
    struct bg_mesh_lod *l = (struct bg_mesh_lod *)i->lod;
    if (!l)
	return;

    POPState *sp = l->i->s;
    sp->shrink_memory();
    bu_log("memshrink\n");
}

extern "C" void
bg_mesh_lod_clear_cache(unsigned long long key)
{
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    if (!bu_file_exists(dir, NULL))
	return;
    if (key == 0) {
	char **filenames;
	size_t nfiles = bu_file_list(dir, "*", &filenames);
	for (size_t i = 0; i < nfiles; i++) {
	    char cdir[MAXPATHLEN] = {0};
	    bu_dir(cdir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, filenames[i], NULL);
	    char **cfilenames;
	    size_t ncfiles = bu_file_list(cdir, "*", &cfilenames);
	    for (size_t j = 0; j < ncfiles; j++) {
		char cfile[MAXPATHLEN] = {0};
		bu_dir(cfile, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, filenames[i], cfilenames[j], NULL);
		bu_file_delete(cfile);
	    }
	    bu_file_delete(cdir);
	}
    }
}

extern "C" void
bg_mesh_lod_set_draw_callback(
	struct bg_mesh_lod *lod,
	int (*clbk)(void *ctx, struct bv_mesh_lod_info *info)
	)
{
    if (!lod || !clbk)
	return;

    POPState *s = lod->i->s;
    s->set_callback(clbk);
}

extern "C" void
bg_mesh_lod_draw(void *ctx, struct bv_scene_obj *s)
{
    if (!s || !ctx)
	return;

    struct bv_mesh_lod_info *i = (struct bv_mesh_lod_info *)s->draw_data;
    struct bg_mesh_lod *l = (struct bg_mesh_lod *)i->lod;
    POPState *ps = l->i->s;
    ps->draw(ctx, s);
}

void
bg_mesh_lod_free(struct bv_scene_obj *s)
{
    if (!s || !s->draw_data)
	return;
    struct bv_mesh_lod_info *i = (struct bv_mesh_lod_info *)s->draw_data;
    struct bg_mesh_lod *l = (struct bg_mesh_lod *)i->lod;
    POPState *ps = l->i->s;
    delete ps;
    BU_PUT(l->i, struct bg_mesh_lod_internal);
    BU_PUT(l, struct bg_mesh_lod);
    BU_PUT(i, struct bg_mesh_lod_info);
    s->draw_data = NULL;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
