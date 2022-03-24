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
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <fstream>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h> /* for mkdir */
#endif
#include "bu/app.h"
#include "bu/malloc.h"
#include "bv/plot3.h"
#include "bg/lod.h"
#include "bg/trimesh.h"

#define POP_MAXLEVEL 16
#define POP_CACHEDIR ".POPLoD"
#define MBUMP 1.01

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};

class POPState {
    public:
	POPState(const point_t *v, int vcnt, int *faces, int fcnt);
	POPState(unsigned long long key);

	~POPState() {};

	// Content based hash key
	unsigned long long hash;


	bool is_valid = false;
	void plot_level(int l, const char *root);
	bool cache();


	// Active faces needed by the current LoD (indexes into npnts).
	std::vector<int> nfaces;

	// This is where we store the active points - i.e., those needed for
	// the current LoD.  When initially creating the breakout from BoT data
	// we calculate all levels, but the goal is to not hold in memory any
	// more than we need to support the LoD drawing.  nfaces will index
	// into npnts.
	std::vector<fastf_t> npnts;

	// Current level of detail information loaded into nfaces/npnts
	int curr_level;

	int vert_cnt = 0;
	const point_t *verts_array = NULL;
	int faces_cnt = 0;
	int *faces_array = NULL;

    private:
	int to_level(int val, int level);
	bool is_equal(rec r1, rec r2, int level);
	bool is_degenerate(rec r0, rec r1, rec r2, int level);

	void level_pnt(point_t *p, int level);


	// When we characterize the levels of the verts,
	// we will need to reorder them so we can load only
	// subsets at need.
	std::unordered_map<int, int> ind_map;

	// Once we have characterized the triangles, we can
	// determine which vertices are active at which levels
	std::unordered_map<int, int> vert_minlevel;
	std::map<int, std::set<int>> level_verts;
	std::unordered_map<int, std::unordered_set<int>> level_tris;


	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	std::vector<unsigned short> PRECOMPUTED_MASKS;
};

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
	// Until we prove otherwise, all triangles are assumed to appear only
	// at the last level (and consequently, their vertices are only needed
	// then).  Set the level accordingly.
	vert_minlevel[i] = POP_MAXLEVEL - 1;
    }

    // Bump out the min and max bounds slightly so none of our actual
    // points are too close to these limits
    minx = minx-fabs(MBUMP*minx);
    miny = miny-fabs(MBUMP*miny);
    minz = minz-fabs(MBUMP*minz);
    maxx = maxx+fabs(MBUMP*maxx);
    maxy = maxy+fabs(MBUMP*maxy);
    maxz = maxz+fabs(MBUMP*maxz);

    for (int i = 0; i < fcnt; i++) {
	rec triangle[3];
	// Transform triangle vertices
	for (int j = 0; j < 3; j++) {
	    triangle[j].x = floor((v[faces[3*i+j]][X] - minx) / (maxx - minx) * USHRT_MAX);
	    triangle[j].y = floor((v[faces[3*i+j]][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    triangle[j].z = floor((v[faces[3*i+j]][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}

	// Find the pop up level for this triangle (i.e., when it will first
	// appear as we step up the zoom levels.)
	int level = POP_MAXLEVEL - 1;
	for (int j = 0; j < POP_MAXLEVEL; j++) {
	    if (!is_degenerate(triangle[0], triangle[1], triangle[2], j)) {
		level = j;
		break;
	    }
	}
	// Add this triangle to its "pop" level
	level_tris[level].insert(i);

	// Let the vertices know they will be needed at this level, if another
	// triangle doesn't already need them sooner
	for (int j = 0; j < 3; j++) {
	    if (vert_minlevel[faces[3*i+j]] > level) {
		vert_minlevel[faces[3*i+j]] = level;
	    }
	}
    }

    // The vertices now know when they will first need to appear.  Build level
    // sets of vertices
    std::unordered_map<int, int>::iterator v_it;
    for (v_it = vert_minlevel.begin(); v_it != vert_minlevel.end(); v_it++) {
	level_verts[v_it->second].insert(v_it->first);
    }

    // Having sorted the vertices into level sets, we may now define a new global
    // vertex ordering that respects the needs of the levels.
    int vind = 0;
    std::map<int, std::set<int>>::iterator l_it;
    std::set<int>::iterator s_it;
    for (l_it = level_verts.begin(); l_it != level_verts.end(); l_it++) {
	for (s_it = l_it->second.begin(); s_it != l_it->second.end(); s_it++) {
	    ind_map[*s_it] = vind;
	    vind++;
	}
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %d count: %zd\n", i, level_tris[i].size());
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %d count: %zd\n", i, level_verts[i].size());
    }

    is_valid = true;
}

POPState::POPState(unsigned long long key)
{
    if (!key)
	return;

    // TODO - once we get incremental loading working this will start at zero...
    curr_level = POP_MAXLEVEL - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", key);

    // If we have cached data, use that
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), NULL);
    if (!bu_file_exists(dir, NULL)) {
	bu_vls_free(&vkey);
	return;
    }

    // Read in the level vertices
    // TODO - once we have incremental loading, load only the first level
    for (int i = 0; i < curr_level; i++) {
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "verts_level_%d", i);
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
		npnts.push_back(nv[k]);
	    }
	    level_verts[i].insert(npnts.size() - 1);
	}
	vifile.close();
	bu_vls_free(&vfile);
    }

    // Calculate min and max so we can do the level snapping for vertices
    // TODO - this needs to be written out to the cache so we don't have to
    // read all points to recalc it
    for (size_t i = 0; i < npnts.size()/3; i++) {
	minx = (npnts[i*3+0] < minx) ? npnts[i*3+0] : minx;
	miny = (npnts[i*3+1] < miny) ? npnts[i*3+1] : miny;
	minz = (npnts[i*3+2] < minz) ? npnts[i*3+2] : minz;
	maxx = (npnts[i*3+0] > maxx) ? npnts[i*3+0] : maxx;
	maxy = (npnts[i*3+1] > maxy) ? npnts[i*3+1] : maxy;
	maxz = (npnts[i*3+2] > maxz) ? npnts[i*3+2] : maxz;
    }
    // Bump out the min and max bounds slightly so none of our actual
    // points are too close to these limits
    minx = minx-fabs(MBUMP*minx);
    miny = miny-fabs(MBUMP*miny);
    minz = minz-fabs(MBUMP*minz);
    maxx = maxx+fabs(MBUMP*maxx);
    maxy = maxy+fabs(MBUMP*maxy);
    maxz = maxz+fabs(MBUMP*maxz);

    // Read in the level triangles
    // TODO - once we have incremental loading, load only the first level
    for (int i = 0; i < curr_level; i++) {
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
		nfaces.push_back(vf[k]);
	    }
	    level_tris[i].insert(nfaces.size() / 3 - 1);
	}
	tifile.close();
	bu_vls_free(&tfile);
    }

    bu_vls_free(&vkey);
    is_valid = 1;
}

// Write out the generated LoD data to the BRL-CAD cache
bool
POPState::cache()
{
    if (!is_valid || !hash)
	return false;

    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    if (!bu_file_exists(dir, NULL)) {
#ifdef HAVE_WINDOWS_H
	CreateDirectory(dir, NULL);
#else
	/* mode: 775 */
	mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    }

    struct bu_vls vkey = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vkey, "%llu", hash);
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), NULL);
    if (bu_file_exists(dir, NULL)) {
	// If the directory already exists, we should already be good to go
	return true;
    } else {
#ifdef HAVE_WINDOWS_H
	CreateDirectory(dir, NULL);
#else
	/* mode: 775 */
	mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    }

    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), "format", NULL);
    FILE *fp = fopen(dir, "w");
    if (!fp) {
	bu_vls_free(&vkey);
	is_valid = false;
	return false;
    }
    fprintf(fp, "1\n");
    fclose(fp);

    // Write out the level vertices
    for (int i = 0; i < curr_level; i++) {
	if (level_verts.find(i) == level_verts.end())
	    continue;
	if (!level_verts[i].size())
	    continue;
	struct bu_vls vfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vfile, "verts_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&vfile), NULL);

	std::ofstream vofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level vert vector
	int sv = level_verts[i].size();
	vofile.write(reinterpret_cast<const char *>(&sv), sizeof(sv));

	// Write out the vertex points
	std::set<int>::iterator s_it;
	for (s_it = level_verts[i].begin(); s_it != level_verts[i].end(); s_it++) {
	    point_t v;
	    VMOVE(v, verts_array[*s_it]);
	    vofile.write(reinterpret_cast<const char *>(&v[0]), sizeof(point_t));
	}

	vofile.close();
	bu_vls_free(&vfile);
    }


    // Write out the level triangles
    for (int i = 0; i < curr_level; i++) {
	if (level_tris.find(i) == level_tris.end())
	    continue;
	if (!level_tris[i].size())
	    continue;
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "tris_level_%d", i);
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&vkey), bu_vls_cstr(&tfile), NULL);

	std::ofstream tofile(dir, std::ios::out | std::ofstream::binary);

	// Store the size of the level tri vector
	int st = level_tris[i].size();
	tofile.write(reinterpret_cast<const char *>(&st), sizeof(st));

	// Write out the mapped triangle indices
	std::unordered_set<int>::iterator s_it;
	for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
	    int vt[3];
	    vt[0] = ind_map[faces_array[3*(*s_it)+0]];
	    vt[1] = ind_map[faces_array[3*(*s_it)+1]];
	    vt[2] = ind_map[faces_array[3*(*s_it)+2]];
	    tofile.write(reinterpret_cast<const char *>(&vt[0]), sizeof(vt));
	}

	tofile.close();
	bu_vls_free(&tfile);
    }

    bu_vls_free(&vkey);

    return true;
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
POPState::level_pnt(point_t *p, int level)
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
    VSET(*p, nx, ny, nz);

    double poffset = DIST_PNT_PNT(*p, in_pt);
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

// Checks whether a triangle is degenerate (at least two coordinates are the
// same on the given precision level)
bool
POPState::is_degenerate(rec r0, rec r1, rec r2, int level)
{
    return is_equal(r0, r1, level) || is_equal(r1, r2, level) || is_equal(r0, r2, level);
}

void
POPState::plot_level(int l, const char *root)
{
    if (l < 0 || l > curr_level - 1)
	return;

    struct bu_vls name;
    FILE *plot_file = NULL;
    bu_vls_init(&name);
    if (!root) {
	bu_vls_printf(&name, "init_level_%.2d.plot3", l);
    } else {
	bu_vls_printf(&name, "%s_level_%.2d.plot3", root, l);
    }
    plot_file = fopen(bu_vls_addr(&name), "wb");
    pl_color(plot_file, 0, 255, 0);

    for (int i = 0; i <= l; i++) {
	std::unordered_set<int>::iterator s_it;
	for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
	    int f_ind = *s_it;
	    int v1ind, v2ind, v3ind;
	    if (faces_array) {
		v1ind = faces_array[3*f_ind+0];
		v2ind = faces_array[3*f_ind+1];
		v3ind = faces_array[3*f_ind+2];
	    } else {
		v1ind = nfaces[3*f_ind+0];
		v2ind = nfaces[3*f_ind+1];
		v3ind = nfaces[3*f_ind+2];
	    }
	    point_t p1, p2, p3;
	    if (verts_array) {
		VMOVE(p1, verts_array[v1ind]);
		VMOVE(p2, verts_array[v2ind]);
		VMOVE(p3, verts_array[v3ind]);
	    } else {
		VSET(p1, npnts[3*v1ind+0], npnts[3*v1ind+1], npnts[3*v1ind+2]);
		VSET(p2, npnts[3*v2ind+0], npnts[3*v2ind+1], npnts[3*v2ind+2]);
		VSET(p3, npnts[3*v3ind+0], npnts[3*v3ind+1], npnts[3*v3ind+2]);
	    }
	    // We iterate over the level i triangles, but our target level
	    // is l so we "decode" the points to that level, NOT i
	    level_pnt(&p1, l);
	    level_pnt(&p2, l);
	    level_pnt(&p3, l);
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
bg_mesh_lod_cache(const point_t *v, int vcnt, int *faces, int fcnt)
{
    unsigned long long key = 0;

    if (!v || !vcnt || !faces || !fcnt)
	return 0;

    POPState p(v, vcnt, faces, fcnt);
    if (!p.cache() || !p.is_valid)
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
bg_mesh_lod_view(struct bg_mesh_lod *l, struct bview *v)
{
    if (!l)
	return -1;
    if (!v)
	return l->i->s->curr_level;

    return -1;
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
    for (int i = 0; i < s->curr_level; i++) {
	s->plot_level(i, pname);
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
