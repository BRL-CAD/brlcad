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
 * wrappers around more sophisticated algorithms, but for now its purpose is to
 * hold logic intended to help with edge-only wireframe displays of large
 * meshes.
 *
 * Useful discussion of applying POP buffers here:
 * https://medium.com/@petroskataras/the-ayotzinapa-case-447a72d89e58
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

#include "bu/malloc.h"
#include "bv/plot3.h"
#include "bg/lod.h"
#include "bg/trimesh.h"

#define POP_MAXLEVEL 16

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};

class POPState {
    public:
	POPState(int mlevel, const point_t *v, int vcnt, int *faces, int fcnt);
	~POPState() {};

	// When we characterize the levels of the verts,
	// we will need to reorder them so we can load only
	// subsets at need.
	std::unordered_map<int, int> ind_map;

	// Once we have characterized the triangles, we can
	// determine which vertices are active at which levels
	std::unordered_map<int, int> vert_minlevel;
	std::map<int, std::set<int>> level_verts;
	std::unordered_map<int, std::unordered_set<int>> level_tris;

	// When the vertices are reordered, we need to create a new faces
	// array which uses the new indices from the ind_map;
	int *nfaces;

	// Number of discrete levels of detail defined
	int max_level;

	int vert_cnt;
	const point_t *verts_array;
	int faces_cnt;
	int *faces_array;

    private:
	int to_level(int val, int level);
	bool is_equal(rec r1, rec r2, int level);
	bool is_degenerate(rec r0, rec r1, rec r2, int level);

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	std::vector<unsigned short> PRECOMPUTED_MASKS;
};

POPState::POPState(int mlevel, const point_t *v, int vcnt, int *faces, int fcnt)
{
    // Store the number of levels
    max_level = mlevel;

    // Store source data info
    vert_cnt = vcnt;
    verts_array = v;
    faces_cnt = fcnt;
    faces_array = faces;

    // Precompute precision masks for each level
    for (int i = 0; i < mlevel; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (mlevel - i - 1)));
    }

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
	vert_minlevel[i] = mlevel - 1;
    }

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
	int level = mlevel - 1;
	for (int j = 0; j < mlevel; j++) {
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

    // A new faces array referencing the level sorted vertices is now needed
    nfaces = (int *)bu_calloc(fcnt, 3*sizeof(int), "update faces");
    for (int i = 0; i < fcnt * 3; i++) {
	nfaces[i] = ind_map[faces[i]];
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %d count: %zd\n", i, level_tris[i].size());
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %d count: %zd\n", i, level_verts[i].size());
    }

}

// Transfer coordinate into level precision
int
POPState::to_level(int val, int level)
{
    int ret = floor(val/double(PRECOMPUTED_MASKS[level]));
    //bu_log("to_level: %d, %d : %d\n", val, level, ret);
    return ret;
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

struct bg_mesh_lod_internal {
    POPState *s;
};

extern "C" struct bg_mesh_lod *
bg_mesh_lod_create(const point_t *v, int vcnt, int *faces, int fcnt)
{
    if (!v || !vcnt || !faces || !fcnt)
	return NULL;

    struct bg_mesh_lod *l = NULL;
    BU_GET(l, struct bg_mesh_lod);
    BU_GET(l->i, struct bg_mesh_lod_internal);

    l->i->s = new POPState(POP_MAXLEVEL, v, vcnt, faces, fcnt);

    return l;
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

static
void plot_level(POPState *s, int l)
{
    if (!s || l < 0 || l > s->max_level - 1)
	return;

    struct bu_vls name;
    FILE *plot_file = NULL;
    bu_vls_init(&name);
    bu_vls_printf(&name, "pop_level_%.2d.plot3", l);
    plot_file = fopen(bu_vls_addr(&name), "wb");
    pl_color(plot_file, 0, 255, 0);

    std::unordered_set<int>::iterator s_it;
    for (s_it = s->level_tris[l].begin(); s_it != s->level_tris[l].end(); s_it++) {
	int f_ind = *s_it;
	int v1ind = s->faces_array[3*f_ind+0];
	int v2ind = s->faces_array[3*f_ind+1];
	int v3ind = s->faces_array[3*f_ind+2];
	pdv_3move(plot_file, s->verts_array[v1ind]);
	pdv_3cont(plot_file, s->verts_array[v2ind]);
	pdv_3cont(plot_file, s->verts_array[v3ind]);
	pdv_3cont(plot_file, s->verts_array[v1ind]);
    }

    fclose(plot_file);
    bu_vls_free(&name);
}

extern "C" int
bg_lod_elist(struct bu_list *elist, struct bview *v, struct bg_mesh_lod *l)
{
    int ecnt = 0;
    if (!l)
	return -1;

    // TODO
    if (elist || v)
	return -1;

    // For debugging purposes, write out plot files of each level
    POPState *s = l->i->s;
    for (int i = 0; i < s->max_level; i++) {
	plot_level(s, i);
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
