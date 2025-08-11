/*                       L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2025 United States Government as represented by
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
#include <cstring>
#include <stdlib.h>
#include <map>
#include <set>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "lmdb.h"
}

#include "bio.h"

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/cache.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bg/plane.h"
#include "bg/sat.h"
#include "bg/trimesh.h"
#include "bv/objs.h"
#include "bv/plot3.h"
#include "bv/lod.h"
#include "bv/util.h"

// Number of levels of detail to define
#define POP_MAXLEVEL 16

// Factor by which to bump out bounds to avoid points on box edges
#define MBUMP 1.01

// Maximum database size.  For detailed views we fall back on just
// displaying the full data set, and we need to be able to memory map the
// file, so go with a 4Gb per file limit.
#define CACHE_MAX_DB_SIZE 4294967296

/* There are various individual pieces of data in the cache associated with
 * each object key.  For lookup they use short suffix strings to distinguish
 * them - we define those strings here to have consistent definitions for use
 * in multiple functions.
 *
 *
 * Changing any of these requires incrementing BV_CACHE_CURRENT_FORMAT. */
#define CACHE_POP_MAX_LEVEL "th"
#define CACHE_POP_SWITCH_LEVEL "sw"
#define CACHE_VERTEX_COUNT "vc"
#define CACHE_TRI_COUNT "tc"
#define CACHE_VERT_LEVEL "v"
#define CACHE_VERTNORM_LEVEL "vn"
#define CACHE_TRI_LEVEL "t"
#define CACHE_OBJ_LBOUNDS "lbb"

typedef int (*full_detail_clbk_t)(struct bv_lod_mesh *, void *);

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};


class POPState;
struct bv_lod_mesh_internal {
    POPState *s;
};

class POPState {
    public:

	// Create cached data (doesn't create a usable container)
	POPState(struct bu_ptbl *cache_items, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_face_cnt_threshold_ratio);

	// Load cached data (DOES create a usable container)
	POPState(struct bu_cache *ctx, unsigned long long key);

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
	std::vector<fastf_t> lod_tri_norms;

	// Current level of detail information loaded into nfaces/npnts
	int curr_level = -1;

	// Force a data reload even if the level hasn't changed (i.e. undo a
	// shrink memory operation when level is set.)
	bool force_update = false;

	// Maximum level for which POP info is defined.  Above that level,
	// need to shift to full rendering
	int max_pop_threshold_level = 0;

	// Used by calling functions to detect initialization errors
	bool is_valid = false;

	// Content based hash key
	unsigned long long hash;

	// Methods for full detail
	full_detail_clbk_t full_detail_setup_clbk = NULL;
	full_detail_clbk_t full_detail_clear_clbk = NULL;
	full_detail_clbk_t full_detail_free_clbk = NULL;
	void *detail_clbk_data = NULL;

	// Bounding box of original mesh
	point_t bbmin, bbmax;

	// Info for drawing
	struct bv_lod_mesh *lod = NULL;

    private:

	void tri_process();

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	fastf_t max_face_ratio = 0.66;

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
	void cache_tri();
	void cache_item(const char *component, void *data, size_t len);
	size_t cache_get(void **data, const char *component);
	void cache_done();
	MDB_val mdb_key, mdb_data[2];

	// Specific loading and unloading methods
	void tri_pop_load(int start_level, int level);
	void tri_pop_trim(int level);
	size_t level_vcnt[POP_MAXLEVEL+1] = {0};
	size_t level_tricnt[POP_MAXLEVEL+1] = {0};

	// Processing containers used for initial triangle data characterization
	std::vector<size_t> tri_ind_map;
	std::vector<size_t> vert_tri_minlevel;
	std::map<size_t, std::unordered_set<size_t>> level_tri_verts;
	std::vector<std::vector<size_t>> level_tris;

	// Pointers to original input data
	size_t vert_cnt = 0;
	const point_t *verts_array = NULL;
	const vect_t *vnorms_array = NULL;
	size_t faces_cnt = 0;
	int *faces_array = NULL;

	// Cache
	char keystr[BU_CACHE_KEY_MAXLEN];
	struct bu_cache *c;
	struct bu_cache_txn *txn = NULL;
	struct bu_ptbl *cache_items;
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
	bool bad_face = false;
	for (size_t j = 0; j < 3; j++) {
	    int f_ind = faces_array[3*i+j];
	    if ((size_t)f_ind >= vert_cnt || f_ind < 0) {
		bu_log("bad face %zd - skipping\n", i);
		bad_face = true;
		break;
	    }
	    triangle[j].x = floor((verts_array[f_ind][X] - minx) / (maxx - minx) * USHRT_MAX);
	    triangle[j].y = floor((verts_array[f_ind][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    triangle[j].z = floor((verts_array[f_ind][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}
	if (bad_face)
	    continue;

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
    std::map<size_t, std::unordered_set<size_t>>::iterator l_it;
    std::unordered_set<size_t>::iterator s_it;
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
    // point snapping trades off - 0.66 is just a guess.  We also need to
    // calculate this maximum size ratio as a function of the overall database
    // mesh data size, which will need to be passed in as a parameter - if the
    // original is too large, we may not be able to fit higher LoD levels in
    // the database and need to make this ratio smaller - probably a maximum of
    // 0.66(?) and drop it lower of the original database is very large (i.e.
    // we need to hold a lot of mesh data).
    size_t trisum = 0;
    if (max_face_ratio > 0.99 || max_face_ratio < 0) {
	max_pop_threshold_level = level_tris.size() - 1;
    } else {
	size_t faces_array_cnt2 = (size_t)((fastf_t)faces_cnt * max_face_ratio);
	for (size_t i = 0; i < level_tris.size(); i++) {
	    trisum += level_tris[i].size();
	    if (trisum > faces_array_cnt2) {
		// If we're using two thirds of the triangles, this is our
		// threshold level.  If we've got ALL the triangles, back
		// down one level.
		if (trisum < (size_t)faces_cnt) {
		    max_pop_threshold_level = i;
		} else {
		    // Handle the case where we get i == 0
		    max_pop_threshold_level = (i) ? i - 1 : 0;
		}
		break;
	    }
	}
    }
    //bu_log("Max LoD POP level: %zd\n", max_pop_threshold_level);
}

POPState::POPState(struct bu_ptbl *ci, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_facecnt_threshold_ratio)
{
    // Store the output ptbl
    cache_items = ci;

    // Caller set parameter telling us when to switch from POP data
    // to just drawing the full mesh
    max_face_ratio = pop_facecnt_threshold_ratio;

    // Hash the data to generate a POP key
    struct bu_data_hash_state *s = bu_data_hash_create();
    bu_data_hash_update(s, v, vcnt*sizeof(point_t));
    bu_data_hash_update(s, faces, 3*fcnt*sizeof(int));
    hash = bu_data_hash_val(s);
    bu_data_hash_destroy(s);

    // Stash the user_key to hash mapping
    struct bu_cache_item *itm;
    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu", user_key);
    itm->data = bu_malloc(sizeof(unsigned long long), "key");
    itm->data_len = sizeof(unsigned long long);
    memcpy(itm->data, &hash, sizeof(unsigned long long));
    bu_ptbl_ins(cache_items, (long *)itm);

    curr_level = POP_MAXLEVEL - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++)
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));

    // Store source data info
    vert_cnt = vcnt;
    verts_array = v;
    vnorms_array = vn;
    faces_cnt = fcnt;
    faces_array = faces;

    // Calculate the full mesh bounding box for later use
    // TODO - see if this has already been stashed in the cache for us
    // As six points under CACHE_OBJ_BOUNDS
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
	bu_log("cache failed\n");

#if 0
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %zu count: %zu\n", i, level_tris[i].size());
    }

    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %zu count: %zu\n", i, level_tri_verts[i].size());
    }
#endif
}

POPState::POPState(struct bu_cache *ctx, unsigned long long user_key)
{
    // Store the context
    c = ctx;

    if (!user_key)
	return;

    // Initialize so set_level will read in the first level of triangles
    curr_level = - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    // Translate a user name key to the content hash
    {
	// Construct lookup key
	struct bu_vls ukey = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ukey, "%llu", user_key);
	void *kmem;
	size_t bsize = bu_cache_get(&kmem, bu_vls_cstr(&ukey), c, &txn);
	if (bsize != sizeof(unsigned long long)) {
	    cache_done();
	    return;
	}
	memcpy(&hash, kmem, sizeof(unsigned long long));
	cache_done();
    }

    // Find the maximum POP level
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_POP_MAX_LEVEL);
	if (!bsize) {
	    cache_done();
	    return;
	}
	if (bsize != sizeof(max_pop_threshold_level)) {
	    bu_log("Incorrect data size found loading max LoD POP threshold\n");
	    cache_done();
	    return;
	}
	memcpy(&max_pop_threshold_level, b, sizeof(max_pop_threshold_level));
	cache_done();
    }

    // Load the POP level where we switch from POP to full
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_POP_SWITCH_LEVEL);
	if (bsize && bsize != sizeof(max_face_ratio)) {
	    bu_log("Incorrect data size found loading LoD POP switch threshold\n");
	    cache_done();
	    return;
	}
	if (bsize) {
	    memcpy(&max_face_ratio, b, sizeof(max_face_ratio));
	} else {
	    max_face_ratio = 0.66;
	}
	cache_done();
    }

    // Load level counts for vectors and tris
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_VERTEX_COUNT);
	if (bsize != sizeof(level_vcnt)) {
	    bu_log("Incorrect data size found loading level vertex counts\n");
	    cache_done();
	    return;
	}
	memcpy(&level_vcnt, b, sizeof(level_vcnt));
	cache_done();
    }
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_TRI_COUNT);
	if (bsize != sizeof(level_tricnt)) {
	    bu_log("Incorrect data size found loading level triangle counts\n");
	    cache_done();
	    return;
	}
	memcpy(&level_tricnt, b, sizeof(level_tricnt));
	cache_done();
    }

    // Read in min/max bounds
    {
	float minmax[6];
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_OBJ_LBOUNDS);
	if (bsize != (sizeof(bbmin) + sizeof(bbmax) + sizeof(minmax))) {
	    bu_log("Incorrect data size found loading cached bounds data\n");
	    cache_done();
	    return;
	}
	memcpy(&bbmin, b, sizeof(bbmin));
	b += sizeof(bbmin);
	memcpy(&bbmax, b, sizeof(bbmax));
	b += sizeof(bbmax);
	//bu_log("bbmin: %f %f %f bbmax: %f %f %f\n", V3ARGS(bbmin), V3ARGS(bbmax));
	memcpy(&minmax, b, sizeof(minmax));
	minx = minmax[0];
	miny = minmax[1];
	minz = minmax[2];
	maxx = minmax[3];
	maxy = minmax[4];
	maxz = minmax[5];
	cache_done();
    }

    // Read in the zero level vertices, vertex normals (if defined) and triangles
    set_level(0);

    // All set - ready for LoD
    is_valid = 1;
}

POPState::~POPState()
{
    if (full_detail_free_clbk) {
	(*full_detail_free_clbk)(lod, detail_clbk_data);
	detail_clbk_data = NULL;
    }
}

void
POPState::tri_pop_load(int start_level, int level)
{
    struct bu_vls kbuf = BU_VLS_INIT_ZERO;

    // Read in the level vertices
    for (int i = start_level+1; i <= level; i++) {
	if (!level_vcnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERT_LEVEL, i);
	fastf_t *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize != level_vcnt[i]*sizeof(point_t)) {
	    bu_log("Incorrect data size found loading level %d point data\n", i);
	    return;
	}
	lod_tri_pnts.insert(lod_tri_pnts.end(), &b[0], &b[level_vcnt[i]*3]);
	cache_done();
    }
    // Re-snap all vertices currently loaded at the new level
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
	if (!level_tricnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_TRI_LEVEL, i);
	int *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize != level_tricnt[i]*3*sizeof(int)) {
	    bu_log("Incorrect data size found loading level %d tri data\n", i);
	    return;
	}
	lod_tris.insert(lod_tris.end(), &b[0], &b[level_tricnt[i]*3]);
	cache_done();
    }

    // Read in the vertex normals, if we have them
    for (int i = start_level+1; i <= level; i++) {
	if (!level_tricnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERTNORM_LEVEL, i);
	fastf_t *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize > 0 && bsize != level_tricnt[i]*sizeof(vect_t)*3) {
	    bu_log("Incorrect data size found loading level %d normal data\n", i);
	    return;
	}
	if (bsize) {
	    lod_tri_norms.insert(lod_tri_norms.end(), &b[0], &b[level_tricnt[i]*3*3]);
	}
	cache_done();
    }
}

void
POPState::shrink_memory()
{
    lod_tri_pnts.clear();
    lod_tri_pnts.shrink_to_fit();
    lod_tri_norms.clear();
    lod_tri_norms.shrink_to_fit();
    lod_tris.clear();
    lod_tris.shrink_to_fit();
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.shrink_to_fit();
}

void
POPState::tri_pop_trim(int level)
{
    // Tally all the lower level verts and tris - those are the ones we need to keep
    size_t vkeep_cnt = 0;
    size_t fkeep_cnt = 0;
    for (size_t i = 0; i <= (size_t)level; i++) {
	vkeep_cnt += level_vcnt[i];
	fkeep_cnt += level_tricnt[i];
    }

    // Shrink the main arrays (note that in C++11 shrink_to_fit may or may
    // not actually shrink memory usage on any given call.)
    lod_tri_pnts.resize(vkeep_cnt*3);
    lod_tri_pnts.shrink_to_fit();
    lod_tri_norms.resize(fkeep_cnt*3*3);
    lod_tri_norms.shrink_to_fit();
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
    fastf_t bdiag = 0;
    VSET(bmin, minx, miny, minz);
    VSET(bmax, maxx, maxy, maxz);
    bdiag = DIST_PNT_PNT(bmin, bmax);

    // If all views are orthogonal we just need the diagonal of the bbox, but
    // if we have any active perspective matrices that may change the answer.
    // In principle we might also be able to back the LoD down further for very
    // distant objects, but a quick test with that resulted in too much loss of
    // detail so for now just look at the "need more" case.
    struct bview *v = (lod && lod->s) ? lod->s->s_v : NULL;
    if (v && SMALL_FASTF < v->gv_perspective) {
	fastf_t cdist = 0;
	point_t pbmin, pbmax;
	MAT4X3PNT(pbmin, v->gv_pmat, bmin);
	MAT4X3PNT(pbmax, v->gv_pmat, bmax);
	bdiag = DIST_PNT_PNT(pbmin, pbmax);
	if (cdist > bdiag)
	    bdiag = cdist;
    }

    for (int lev = 0; lev < POP_MAXLEVEL; lev++) {
	fastf_t diag_slice = bdiag/pow(2,lev);
	if (diag_slice < delta) {
	    bv_log(2, "POPState::get_level %g->%d", vlen, lev);
	    return lev;
	}
    }
    bv_log(2, "POPState::get_level %g->%d", vlen, POP_MAXLEVEL - 1);
    return POP_MAXLEVEL - 1;
}

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
    if (level > curr_level && level <= max_pop_threshold_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_load(curr_level, level);
	}
    }

    // If we need to trim back the POP data, do that
    if (level < curr_level && level <= max_pop_threshold_level && curr_level <= max_pop_threshold_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_trim(level);
	}
    }

    // If we were operating beyond POP detail levels (i.e. using RTree
    // management) we need to reset our POP data and clear the more detailed
    // info from the containers to free up memory.
    if (level < curr_level && level <= max_pop_threshold_level && curr_level > max_pop_threshold_level) {
	// We're (re)entering the POP range - clear the high detail data and start over
	if (full_detail_clear_clbk)
	    (*full_detail_clear_clbk)(lod, detail_clbk_data);

	// Full reset, not an incremental load.
	tri_pop_load(-1, level);
    }

    // If we're jumping into details levels beyond POP range, clear the POP containers
    // and load the more detailed data management info
    if (level > curr_level && level > max_pop_threshold_level && curr_level <= max_pop_threshold_level) {

	// Clear the LoD data - we need the full set now
	lod_tri_pnts_snapped.clear();
	lod_tri_pnts_snapped.shrink_to_fit();
	lod_tri_pnts.clear();
	lod_tri_pnts.shrink_to_fit();
	lod_tri_norms.clear();
	lod_tri_norms.shrink_to_fit();
	lod_tris.clear();
	lod_tris.shrink_to_fit();

	// Use the callback to set up the full data pointers
	if (full_detail_setup_clbk)
	    (*full_detail_setup_clbk)(lod, detail_clbk_data);
    }

    //elapsed = bu_gettime() - start;
    //seconds = elapsed / 1000000.0;
    //bu_log("lod set_level(%d): %f sec\n", level, seconds);

    curr_level = level;
}

// Rather than committing all data to LMDB in one transaction, use keys with
// appended strings to the hash to denote the individual pieces - basically
// what we were doing with files, but in the db instead
//
// This will also allow easier removal of larger subcomponents if we need to
// back off on saved LoD.
void
POPState::cache_item(const char *component, void *data, size_t len)
{
    struct bu_cache_item *itm;
    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, component);
    itm->data = bu_malloc(len, "data");
    itm->data_len = len;
    memcpy(itm->data, data, len);
    bu_ptbl_ins(cache_items, (long *)itm);
}

// This pulls the data, but doesn't close the transaction because the
// calling code will want to manipulate the data.  After that process
// is complete, cache_done() should be called to prepare for subsequent
// operations.
size_t
POPState::cache_get(void **data, const char *component)
{
    // Construct lookup key
    snprintf(keystr, BU_CACHE_KEY_MAXLEN, "%llu:%s", hash, component);

    // Initialize
    (*data) = NULL;

    // Do the lookup
    return bu_cache_get(data, keystr, c, &txn);
}

void
POPState::cache_done()
{
    bu_cache_get_done(&txn);
}

void
POPState::cache_tri()
{
    // Threshold level - above this level,
    // we need to switch to full-detail drawing
    int tmp_maxlevel = max_pop_threshold_level;
    cache_item(CACHE_POP_MAX_LEVEL, &tmp_maxlevel, sizeof(int));

    // Switch level
    fastf_t tmp_max_face_ratio = max_face_ratio;
    cache_item(CACHE_POP_SWITCH_LEVEL, &tmp_max_face_ratio, sizeof(fastf_t));

    // Vertex counts for all active levels
    size_t vldata[POP_MAXLEVEL+1] = {0};
    for (size_t i = 0; i <= POP_MAXLEVEL; i++) {
	if (level_tri_verts.find(i) == level_tri_verts.end())
	    continue;
	if ((int)i > max_pop_threshold_level || !level_tri_verts[i].size())
	    continue;
	vldata[i] = level_tri_verts[i].size();
    }
    cache_item(CACHE_VERTEX_COUNT, &vldata, sizeof(vldata));

    // Write out the triangle counts for all active levels
    size_t tldata[POP_MAXLEVEL+1] = {0};
    for (size_t i = 0; i <= POP_MAXLEVEL; i++) {
	if ((int)i > max_pop_threshold_level || !level_tris[i].size())
	    continue;
	// Store the size of the level tri vector
	tldata[i] = level_tris[i].size();
    }
    cache_item(CACHE_TRI_COUNT, &tldata, sizeof(level_tricnt));

    // Write out the vertices in LoD order for each level
    struct bu_vls kbuf = BU_VLS_INIT_ZERO;
    for (int i = 0; i <= max_pop_threshold_level; i++) {
	if (level_tri_verts.find(i) == level_tri_verts.end())
	    continue;
	if (!level_tri_verts[i].size())
	    continue;
	// Write out the vertex points
	std::vector<fastf_t> vpnts;
	std::unordered_set<size_t>::iterator s_it;
	for (s_it = level_tri_verts[i].begin(); s_it != level_tri_verts[i].end(); s_it++) {
	    point_t v;
	    VMOVE(v, verts_array[*s_it]);
	    for (int j = 0; j < 3; j++)
		vpnts.push_back(v[j]);
	}
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERT_LEVEL, i);
	cache_item(bu_vls_cstr(&kbuf), vpnts.data(), vpnts.size()*sizeof(fastf_t));
    }

    // Write out the triangles in LoD order for each level
    for (int i = 0; i <= max_pop_threshold_level; i++) {
	if (!level_tris[i].size())
	    continue;
	// Write out the mapped triangle indices
	std::vector<int> tinds;
	std::vector<size_t>::iterator s_it;
	for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); ++s_it) {
	    tinds.push_back((int)tri_ind_map[faces_array[3*(*s_it)+0]]);
	    tinds.push_back((int)tri_ind_map[faces_array[3*(*s_it)+1]]);
	    tinds.push_back((int)tri_ind_map[faces_array[3*(*s_it)+2]]);
	}
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_TRI_LEVEL, i);
	cache_item(bu_vls_cstr(&kbuf), tinds.data(), tinds.size()*sizeof(int));
    }

    // Write out the vertex normals in LoD order for each level, if we have them
    if (vnorms_array) {
	for (int i = 0; i <= max_pop_threshold_level; i++) {
	    if (!level_tris[i].size())
		continue;
	    // Write out the normals associated with the triangle indices
	    std::vector<fastf_t> vnorms;
	    std::vector<size_t>::iterator s_it;
	    for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		vect_t v;
		int tind;
		tind = 3*(*s_it)+0;
		VMOVE(v, vnorms_array[tind]);
		for (int j = 0; j < 3; j++)
		    vnorms.push_back(v[j]);
		tind = 3*(*s_it)+1;
		VMOVE(v, vnorms_array[tind]);
		for (int j = 0; j < 3; j++)
		    vnorms.push_back(v[j]);
		tind = 3*(*s_it)+2;
		VMOVE(v, vnorms_array[tind]);
		for (int j = 0; j < 3; j++)
		    vnorms.push_back(v[j]);
	    }
	    bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERTNORM_LEVEL, i);
	    cache_item(bu_vls_cstr(&kbuf), vnorms.data(), vnorms.size()*sizeof(fastf_t));
	}
    }

    bu_vls_free(&kbuf);
}

// Write out the generated LoD data to the BRL-CAD cache
void
POPState::cache()
{
    if (!hash) {
	is_valid = false;
	return;
    }

    // Stash the original mesh bbox and the min and max bounds, which will be used in decoding
    void *bb = malloc(sizeof(point_t) * 2 + sizeof(float) * 6);
    VMOVE(((point_t *)bb)[0], bbmin);
    VMOVE(((point_t *)bb)[1], bbmax);
    float *fb = (float *)((char *)bb + 2*sizeof(point_t));
    fb[0] = minx;
    fb[1] = miny;
    fb[2] = minz;
    fb[3] = maxx;
    fb[4] = maxy;
    fb[5] = maxz;
    cache_item(CACHE_OBJ_LBOUNDS, (char *)bb,  sizeof(point_t) * 2 + sizeof(float) * 6);
    bu_free(bb, "bb array");

    // Serialize triangle-specific data
    cache_tri();

    // Produced the bu_cache_item set
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

    if (curr_level <= max_pop_threshold_level) {
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


extern "C" int
bv_lod_mesh_gen(struct bu_ptbl *cache_items, const char *name, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, double fratio)
{

    if (!cache_items || !name || !v || !vcnt || !faces || !fcnt)
	return 0;

    unsigned long long user_key = bu_data_hash(name, strlen(name)*sizeof(char));

    POPState p(cache_items, v, vcnt, vn, faces, fcnt, user_key, fratio);
    if (!BU_PTBL_LEN(cache_items))
	return 0;

    return 1;
}

extern "C" struct bv_lod_mesh *
bv_lod_mesh_get(struct bu_cache *c, const char *name)
{
    if (!c || !name)
	return NULL;

    // Name -> user_key
    unsigned long long user_key = bu_data_hash(name, strlen(name)*sizeof(char));

    POPState *p = new POPState(c, user_key);
    if (!p)
	return NULL;

    if (!p->is_valid) {
	delete p;
	return NULL;
    }

    // Set up info container
    struct bv_lod_mesh *lod;
    BU_GET(lod, struct bv_lod_mesh);
    lod->magic = LOD_MESH_MAGIC;
    BU_GET(lod->i, struct bv_lod_mesh_internal);
    ((struct bv_lod_mesh_internal *)lod->i)->s = p;
    lod->c = c;
    p->lod = lod;

    // Important - parent codes need to have a sense of the size of
    // the object, and we want that size to be consistent regardless
    // of the LoD actually in use.  Set the bbox dimensions at a level
    // where external codes can see and use them.
    VMOVE(lod->bmin, p->bbmin);
    VMOVE(lod->bmax, p->bbmax);

    return lod;
}

extern "C" void
bv_lod_mesh_put(struct bv_lod_mesh *lod)
{
    if (!lod)
	return;

    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)lod->i;
    delete i->s;
    i->s = NULL;
    BU_PUT(i, struct bv_lod_mesh_internal);
    lod->i = NULL;
    BU_PUT(lod, struct bv_lod_mesh);
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
bv_lod_level(struct bv_scene_obj *s, int level, int reset)
{
    if (!s)
	return -1;

    // If we have anything other than mesh data, we can't (right now
    // at least) worth with it.
    if (!s->draw_data || (*((const uint32_t *)(s->draw_data)) != (uint32_t)(LOD_MESH_MAGIC)))
	return -1;

    struct bv_lod_mesh *l = (struct bv_lod_mesh *)s->draw_data;
    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)l->i;
    POPState *sp = i->s;
    if (level < 0)
	return sp->curr_level;

    int old_level = sp->curr_level;

    sp->force_update = (reset) ? true : false;
    sp->set_level(level);

    // If we're in POP territory use the local arrays - otherwise, they
    // were already set by the full detail callback.
    if (sp->curr_level <= sp->max_pop_threshold_level) {
	l->fcnt = (int)sp->lod_tris.size()/3;
	l->faces = sp->lod_tris.data();
	l->points_orig = (const point_t *)sp->lod_tri_pnts.data();
	l->porig_cnt = (int)sp->lod_tri_pnts.size();
#if 0
	// TODO - there's still some error with normals - they seem to work,
	// but when zooming way out and back in (at least on Windows) we're
	// getting an access violation with some geometry...
	if (sp->lod_tri_norms.size() >= sp->lod_tris.size()) {
	    l->normals = (const vect_t *)sp->lod_tri_norms.data();
	} else {
	    l->normals = NULL;
	}
#else
	l->normals = NULL;
#endif
	l->points = (const point_t *)sp->lod_tri_pnts_snapped.data();
	l->pcnt = (int)sp->lod_tri_pnts_snapped.size();
    }

    bv_log(2, "bv_lod_level %s[%d](%d): %d", bu_vls_cstr(&s->s_name), level, reset, l->fcnt);

    // If the data changed, any Display List we may have previously generated
    // is now obsolete
    if (old_level != sp->curr_level)
	dlist_stale(s);

    return sp->curr_level;
}

extern "C" int
bv_lod_calc_level(struct bv_scene_obj *s, const struct bview *v)
{
    if (!s || !v)
	return -1;

    // If we have anything other than mesh data, we can't (right now
    // at least) worth with it.
    if (!s->draw_data || (*((const uint32_t *)(s->draw_data)) != (uint32_t)(LOD_MESH_MAGIC)))
	return -1;

    struct bv_lod_mesh *l = (struct bv_lod_mesh *)s->draw_data;
    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)l->i;
    POPState *sp = i->s;
    int vscale = (int)((double)sp->get_level(v->gv_size) * v->gv_s->lod_scale);
    vscale = (vscale < 0) ? 0 : vscale;
    vscale = (vscale >= POP_MAXLEVEL) ? POP_MAXLEVEL-1 : vscale;

    return vscale;
}

extern "C" int
bv_lod_view(struct bv_scene_obj *s, const struct bview *v, int reset)
{
    if (!s || !v)
	return -1;

    // If we have anything other than mesh data, we can't (right now
    // at least) worth with it.
    if (!s->draw_data || (*((const uint32_t *)(s->draw_data)) != (uint32_t)(LOD_MESH_MAGIC)))
	return -1;

    // Unpack the pop state
    struct bv_lod_mesh *l = (struct bv_lod_mesh *)s->draw_data;
    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)l->i;
    POPState *sp = i->s;

    // If the object is not visible in the scene, nothing to do - we're
    // not changing data for a non-visible object.
    if (!bv_view_obj_vis(v, s))
	return sp->curr_level;

    int vscale = bv_lod_calc_level(s, v);

    int ret = bv_lod_level(s, vscale, reset);

    bv_log(2, "bv_lod_view %s[%s][%d]", bu_vls_cstr(&s->s_name), bu_vls_cstr(&v->gv_name), vscale);

    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(s->bmin), V3ARGS(s->bmax));
    return ret;
}

extern "C" void
bv_lod_memshrink(struct bv_scene_obj *s)
{
    if (!s)
	return;

    // If we have anything other than mesh data, we can't (right now
    // at least) shrink it.
    if (!s->draw_data || (*((const uint32_t *)(s->draw_data)) != (uint32_t)(LOD_MESH_MAGIC)))
	return;

    struct bv_lod_mesh *l = (struct bv_lod_mesh *)s->draw_data;
    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)l->i;
    POPState *sp = i->s;
    sp->shrink_memory();
    bu_log("memshrink\n");
}

extern "C" void
bv_lod_clear_gen(struct bu_ptbl *tbl, const char *name, struct bu_cache *c)
{
    if (!tbl || !name)
	return;

    unsigned long long key = bu_data_hash(name, strlen(name)*sizeof(char));

    struct bu_cache_item *itm;
    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_POP_MAX_LEVEL);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_POP_SWITCH_LEVEL);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_VERTEX_COUNT);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_TRI_COUNT);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_OBJ_LBOUNDS);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_VERT_LEVEL);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_VERTNORM_LEVEL);
    bu_ptbl_ins(tbl, (long *)itm);

    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu:%s", key, CACHE_TRI_LEVEL);
    bu_ptbl_ins(tbl, (long *)itm);

    // If we weren't given a cache we've done all we can
    if (!c)
	return;

    // Translate database object name key to data key
    char ckey[BU_CACHE_KEY_MAXLEN];
    snprintf(ckey, BU_CACHE_KEY_MAXLEN, "%llu", key);
    void *cdata = NULL;
    struct bu_cache_txn *txn = NULL;
    if (bu_cache_get(&cdata, ckey, c, &txn) != sizeof(unsigned long long)) {
	bu_cache_get_done(&txn);
	return;
    }
    // Found something - assign it to lhash
    unsigned long long lhash = 0;
    memcpy(&lhash, cdata, sizeof(lhash));
    bu_cache_get_done(&txn);

    // Make lhash into a key and make the item for the actual LoD data
    BU_GET(itm, struct bu_cache_item);
    snprintf(itm->key, BU_CACHE_KEY_MAXLEN, "%llu", lhash);
    bu_ptbl_ins(tbl, (long *)itm);
}

extern "C" void
bv_lod_mesh_detail_setup_clbk(
	struct bv_lod_mesh *lod,
	int (*clbk)(struct bv_lod_mesh *, void *),
	void *clbk_data
	)
{
    if (!lod || !clbk)
	return;

    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_setup_clbk = clbk;
    s->detail_clbk_data = clbk_data;
}

extern "C" void
bv_lod_mesh_detail_clear_clbk(
	struct bv_lod_mesh *lod,
	int (*clbk)(struct bv_lod_mesh *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_clear_clbk = clbk;
}

extern "C" void
bv_lod_mesh_detail_free_clbk(
	struct bv_lod_mesh *lod,
	int (*clbk)(struct bv_lod_mesh *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_free_clbk = clbk;
}

void
bv_lod_free(struct bv_scene_obj *s)
{
    if (!s || !s->draw_data)
	return;
    struct bv_lod_mesh *l = (struct bv_lod_mesh *)s->draw_data;
    struct bv_lod_mesh_internal *i = (struct bv_lod_mesh_internal *)l->i;
    delete i->s;
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
