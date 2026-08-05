/*                 T R I M E S H _ S P L I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
/** @file trimesh_split.cpp
 *
 * Given an array of faces, identify the topologically connected subsets
 * and return them.
 *
 */

#include "common.h"

#include <deque>
#include <map>
#include <vector>

#include "bu/malloc.h"
#include "bg/trimesh.h"

class tm_split_uedge {
    public:
	tm_split_uedge(int vert1, int vert2) {
	    v1 = (vert1 < vert2) ? vert1 : vert2;
	    v2 = (vert2 < vert1) ? vert1 : vert2;
	}
	~tm_split_uedge() {};
	int v1;
	int v2;

	bool operator==(tm_split_uedge other) const
	{
	    bool c1 = (v1 == other.v1);
	    bool c2 = (v2 == other.v2);
	    return (c1 && c2);
	}

	bool operator<(tm_split_uedge other) const
	{
	    bool c1 = (v1 < other.v1);
	    bool c1e = (v1 == other.v1);
	    bool c2 = (v2 < other.v2);
	    return (c1 || (c1e && c2));
	}

};

class tm_split_sface {
    public:
	int v1;
	int v2;
	int v3;
};

extern "C" int
bg_trimesh_split(int ***of, int **oc, int *f, int fcnt)
{
    if (!of || !oc || !f || fcnt < 0)
	return -1;

    std::map<tm_split_uedge, std::vector<size_t>> ue_fmap;
    std::vector<tm_split_sface> afaces;

    for (int i = 0; i < fcnt; ++i) {
	tm_split_sface nface;
	nface.v1 = f[i*3+0];
	nface.v2 = f[i*3+1];
	nface.v3 = f[i*3+2];
	afaces.push_back(nface);
	ue_fmap[tm_split_uedge(nface.v1,nface.v2)].push_back(i);
	ue_fmap[tm_split_uedge(nface.v2,nface.v3)].push_back(i);
	ue_fmap[tm_split_uedge(nface.v3,nface.v1)].push_back(i);
    }

    // Traverse face adjacency explicitly.  A mesh edge may be non-manifold
    // and have more than two incident faces, so every face in ue_fmap[edge]
    // must be visited.  The former edge-wavefront implementation marked an
    // edge complete after following its first neighbor.  Which of three or
    // more incident faces was left behind then depended on hash iteration
    // order, and a connected mesh could be reported as multiple components.
    std::vector<bool> visited((size_t)fcnt, false);
    std::vector<int *> fsets;
    std::vector<int> fset_cnts;
    for (size_t seed = 0; seed < afaces.size(); seed++) {
	if (visited[seed])
	    continue;

	std::deque<size_t> pending;
	std::vector<size_t> component;
	visited[seed] = true;
	pending.push_back(seed);

	while (!pending.empty()) {
	    size_t f_ind = pending.front();
	    pending.pop_front();
	    component.push_back(f_ind);

	    const tm_split_sface &face = afaces[f_ind];
	    tm_split_uedge edges[3] = {
		tm_split_uedge(face.v1, face.v2),
		tm_split_uedge(face.v2, face.v3),
		tm_split_uedge(face.v3, face.v1)
	    };
	    for (size_t edge_ind = 0; edge_ind < 3; edge_ind++) {
		const std::vector<size_t> &neighbors = ue_fmap[edges[edge_ind]];
		for (size_t neighbor : neighbors) {
		    if (!visited[neighbor]) {
			visited[neighbor] = true;
			pending.push_back(neighbor);
		    }
		}
	    }
	}

	int *fset = (int *)bu_calloc(component.size(), 3*sizeof(int), "face set");
	for (size_t i = 0; i < component.size(); i++) {
	    const tm_split_sface &face = afaces[component[i]];
	    fset[i*3+0] = face.v1;
	    fset[i*3+1] = face.v2;
	    fset[i*3+2] = face.v3;
	}
	fsets.push_back(fset);
	fset_cnts.push_back((int)component.size());
    }

    if (!fsets.size())
	return 0;

    int **ofs = (int **)bu_calloc(fsets.size(), sizeof(int *), "final set of sets");
    for (size_t i = 0; i < fsets.size(); i++) {
	ofs[i] = fsets[i];
    }
    int *ofs_cnt = (int *)bu_calloc(fset_cnts.size(), sizeof(int), "final set of cnts");
    for (size_t i = 0; i < fset_cnts.size(); i++) {
	ofs_cnt[i] = fset_cnts[i];
    }

    (*of) = ofs;
    (*oc) = ofs_cnt;

    return (int)fsets.size();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
