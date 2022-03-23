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
 */

#include "common.h"
#include <stdlib.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>

#include "bu/malloc.h"
#include "bg/lod.h"
#include "bg/trimesh.h"

#define POP_MAXLEVEL 16

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};

// Structure to hold a vertex entry that is not transformed yet
class entry {
    public:
	float vx = 0.0, vy = 0.0, vz = 0.0;
};

class POPState {
    public:
	POPState(int mlevel);
	~POPState() {};
	int to_level(int val, int level);
	bool is_equal(rec r1, rec r2, int level);
	bool is_degenerate(rec r0, rec r1, rec r2, int level);

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	std::vector<entry> entries;
	std::vector<std::vector<rec> *> buckets;
    private:
	std::vector<unsigned short> PRECOMPUTED_MASKS;
};

POPState::POPState(int mlevel)
{
    // Precompute precision masks
    for (int i = 0; i < mlevel; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (mlevel - i - 1)));
    }

    // Create buckets for every level
    for (int i = 0; i < mlevel; i++) {
	buckets.push_back(new std::vector<rec>);
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

    l->i->s = new POPState(POP_MAXLEVEL);

    POPState *s = l->i->s;

    for (int i = 0; i < vcnt; i++) {
	entry e;
	e.vx = v[i][X];
	e.vy = v[i][Y];
	e.vz = v[i][Z];
	s->minx = (e.vx < s->minx) ? e.vx : s->minx;
	s->miny = (e.vy < s->miny) ? e.vy : s->miny;
	s->minz = (e.vz < s->minz) ? e.vz : s->minz;
	s->maxx = (e.vx > s->maxx) ? e.vx : s->maxx;
	s->maxy = (e.vy > s->maxy) ? e.vy : s->maxy;
	s->maxz = (e.vz > s->maxz) ? e.vz : s->maxz;
	s->entries.push_back(e);
    }

    for (int i = 0; i < fcnt; i++) {
	rec triangle[3];
	// Transform triangle vertices
	for (int j = 0; j < 3; j++) {
	    entry &e = s->entries[faces[3*i+j]];
	    triangle[j].x = floor((e.vx - s->minx) / (s->maxx - s->minx) * USHRT_MAX);
	    triangle[j].y = floor((e.vy - s->miny) / (s->maxy - s->miny) * USHRT_MAX);
	    triangle[j].z = floor((e.vz - s->minz) / (s->maxz - s->minz) * USHRT_MAX);
	}

	// Find the pop up level for this triangle
	int level = POP_MAXLEVEL - 1;
	for (int j = 0; j < POP_MAXLEVEL; j++) {
	    if (!s->is_degenerate(triangle[0], triangle[1], triangle[2], j)) {
		level = j;
		break;
	    }
	}

	// Store in the correct bucket
	for (int k = 0; k < 3; k++) {
	    s->buckets[level]->push_back(triangle[k]);
	}
    }

    for (int i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %d count: %zd\n", i, s->buckets[i]->size());
    }

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

extern "C" int
bg_lod_elist(struct bu_list *elist, struct bview *v, struct bg_mesh_lod *l)
{
    int ecnt = 0;
    if (!elist || !v || !l)
	return -1;


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
