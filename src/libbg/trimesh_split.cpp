/*                 T R I M E S H _ S P L I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include <map>
#include <set>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bu/log.h"
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

// https://stackoverflow.com/a/27952689/2037687
class tm_split_uedge_hash {
    public:
	size_t operator()(const tm_split_uedge& p) const
	{
	    size_t hout = p.v1;
	    hout ^= p.v1 + 0x9e3779b97f4a7c16 + (p.v2 << 6) + (p.v2 >> 2);
	    return hout;
	}
};

class tm_split_sface {
    public:
	size_t v1;
	size_t v2;
	size_t v3;
};

extern "C" int
bg_trimesh_split(int ***of, int **oc, int *f, int fcnt)
{
    if (!of || !f || fcnt < 0)
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

    // All triangles go somewhere.
    std::unordered_set<size_t> active_tris;
    for (int i = 0; i < fcnt; ++i) {
	active_tris.insert(i);
    }
    int set_cnt = 0;
    std::vector<int *> fsets;
    std::vector<int> fset_cnts;
    while (active_tris.size()) {
	std::unordered_set<size_t> new_mesh;
	// Seed the wavefront with a triangle
	std::unordered_set<tm_split_uedge, tm_split_uedge_hash> wavefront_edges;
	std::unordered_set<tm_split_uedge, tm_split_uedge_hash> interior_edges;
	tm_split_sface &f_seed= afaces[*active_tris.begin()];
	new_mesh.insert(*active_tris.begin());
	active_tris.erase(active_tris.begin());
	wavefront_edges.insert(tm_split_uedge(f_seed.v1, f_seed.v2));
	wavefront_edges.insert(tm_split_uedge(f_seed.v2, f_seed.v3));
	wavefront_edges.insert(tm_split_uedge(f_seed.v3, f_seed.v1));
	while (wavefront_edges.size()) {
	    tm_split_uedge cedge = *wavefront_edges.begin();
	    // If we have a boundary edge, mark it as interior (i.e. not part
	    // of the wavefront)
	    if (ue_fmap[cedge].size() == 1) {
		interior_edges.insert(cedge);
		wavefront_edges.erase(wavefront_edges.begin());
		continue;
	    }
	    // Get the two triangles associated with this tm_split_uedge.  If one of
	    // them is unvisited, process it
	    int f_ind = -1;
	    for (size_t j = 0; j < ue_fmap[cedge].size(); j++) {
		if (active_tris.find(ue_fmap[cedge][j]) == active_tris.end()) {
		    continue;
		}
		f_ind = ue_fmap[cedge][j];
		new_mesh.insert(f_ind);
		active_tris.erase(f_ind);
		break;
	    }

	    // If neither face is unvisited, there's no work to do
	    if (f_ind == -1) {
		wavefront_edges.erase(wavefront_edges.begin());
		continue;
	    }

	    tm_split_sface &bface = afaces[f_ind];
	    tm_split_uedge ue[3] = {tm_split_uedge(bface.v1, bface.v2), tm_split_uedge(bface.v2, bface.v3), tm_split_uedge(bface.v3, bface.v1)};
	    // For the new face, any edges that aren't already wavefront
	    // edges and aren't part of the interior set get added, and any
	    // that are already in the wavefront get moved to the interior set.
	    for (size_t k = 0; k < 3; k++) {
		if (wavefront_edges.find(ue[k]) != wavefront_edges.end()) {
		    interior_edges.insert(ue[k]);
		    wavefront_edges.erase(ue[k]);
		} else {
		    // If the mesh loops back on itself, the wavefront might hit
		    // edges it has already seen.  Let's not infinite loop...
		    if (interior_edges.find(ue[k]) == interior_edges.end())
			wavefront_edges.insert(ue[k]);
		}
	    }

	}

	if (new_mesh.size()) {
	    set_cnt++;
	    int *fset = (int *)bu_calloc(new_mesh.size(), 3*sizeof(int), "face set");
	    std::unordered_set<size_t>::iterator s_it;
	    int f_ind = 0;
	    for (s_it = new_mesh.begin(); s_it != new_mesh.end(); s_it++) {
		fset[f_ind*3+0] = afaces[*s_it].v1;
		fset[f_ind*3+1] = afaces[*s_it].v2;
		fset[f_ind*3+2] = afaces[*s_it].v3;
		f_ind++;
	    }
	    fsets.push_back(fset);
	    fset_cnts.push_back(f_ind);
	}
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

