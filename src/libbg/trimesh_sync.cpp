/*                 T R I M E S H _ S Y N C . C P P
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
/** @file trimesh_sync.cpp
 *
 * Sync up triangle mesh faces to all point in a consistent direction
 * per their shared topology. (Does not guaranteed that they will be
 * "correct" (i.e. pointing outward) for a closed solid.)
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
#include "bg/trimesh.h"

class tm_sync_oedge {
    public:
	tm_sync_oedge(int vert1, int vert2, int face_ind)
	    : v1(vert1), v2(vert2), f_ind(face_ind) {}
	~tm_sync_oedge() {};

	int v1;
	int v2;
	int f_ind;

	void flip()
	{
	    int tmp = v2;
	    v2 = v1;
	    v1 = tmp;
	}

	bool operator==(tm_sync_oedge other) const
	{
	    bool c1 = (v1 == other.v1);
	    bool c2 = (v2 == other.v2);
	    return (c1 && c2);
	}
};


class tm_sync_uedge {
    public:
	tm_sync_uedge(int vert1, int vert2) {
	    v1 = (vert1 < vert2) ? vert1 : vert2;
	    v2 = (vert2 < vert1) ? vert1 : vert2;
	}
	~tm_sync_uedge() {};
	int v1;
	int v2;

	bool operator==(tm_sync_uedge other) const
	{
	    bool c1 = (v1 == other.v1);
	    bool c2 = (v2 == other.v2);
	    return (c1 && c2);
	}

	bool operator<(tm_sync_uedge other) const
	{
	    bool c1 = (v1 < other.v1);
	    bool c1e = (v1 == other.v1);
	    bool c2 = (v2 < other.v2);
	    return (c1 || (c1e && c2));
	}

};

// https://stackoverflow.com/a/27952689/2037687
class tm_sync_uedge_hash {
    public:
	size_t operator()(const tm_sync_uedge& p) const
	{
	    size_t hout = p.v1;
	    hout ^= p.v1 + 0x9e3779b97f4a7c16 + (p.v2 << 6) + (p.v2 >> 2);
	    return hout;
	}
};

class tm_sync_sface {
    public:
	size_t v1;
	size_t v2;
	size_t v3;
	size_t e[3];
	void flip()
	{
	    size_t tmp = v2;
	    v2 = v1;
	    v1 = tmp;
	}
};


extern "C" int
bg_trimesh_sync(int *of, int *f, int fcnt)
{
    if (!of || !f || fcnt < 0)
	return -1;

    std::vector<tm_sync_oedge> ordered_edges;
    std::map<tm_sync_uedge, std::vector<size_t>> ue_emap;
    std::map<tm_sync_uedge, std::vector<size_t>> ue_fmap;
    std::vector<tm_sync_sface> synced_faces;

    for (int i = 0; i < fcnt; ++i) {
	tm_sync_sface nface;
	nface.v1 = f[i*3+0];
	nface.v2 = f[i*3+1];
	nface.v3 = f[i*3+2];
	ordered_edges.push_back(tm_sync_oedge(nface.v1, nface.v2, i));
	nface.e[0] = ordered_edges.size()-1;
	ue_emap[tm_sync_uedge(nface.v1,nface.v2)].push_back(nface.e[0]);
	ordered_edges.push_back(tm_sync_oedge(nface.v2, nface.v3, i));
	nface.e[1] = ordered_edges.size()-1;
	ue_emap[tm_sync_uedge(nface.v2,nface.v3)].push_back(nface.e[1]);
	ordered_edges.push_back(tm_sync_oedge(nface.v3, nface.v1, i));
	nface.e[2] = ordered_edges.size()-1;
	ue_emap[tm_sync_uedge(nface.v3,nface.v1)].push_back(nface.e[2]);
	synced_faces.push_back(nface);
	ue_fmap[tm_sync_uedge(nface.v1,nface.v2)].push_back(i);
	ue_fmap[tm_sync_uedge(nface.v2,nface.v3)].push_back(i);
	ue_fmap[tm_sync_uedge(nface.v3,nface.v1)].push_back(i);
    }

    /* Any unordered edge with more than two associated faces rules out those
     * faces being associated with a sync - all others are in play. */
    std::unordered_set<size_t> active_tris;
    for (int i = 0; i < fcnt; ++i) {
	tm_sync_sface &cface = synced_faces[i];
	tm_sync_uedge ue1(cface.v1, cface.v2);
	tm_sync_uedge ue2(cface.v1, cface.v2);
	tm_sync_uedge ue3(cface.v1, cface.v2);
	if (ue_fmap[ue1].size() > 2)
	    continue;
	if (ue_fmap[ue2].size() > 2)
	    continue;
	if (ue_fmap[ue3].size() > 2)
	    continue;
	active_tris.insert(i);
    }

    // Because a set of triangles may contain more than one topologically
    // distinct mesh, we make sure the process continues (by re-seeding as
    // needed) until all active triangles have been considered.
    int flip_cnt = 0;
    while (active_tris.size()) {
	// Seed the wavefront with three edges from an active triangle.  This triangle
	// sets the orientation for the mesh - all other triangles will be flipped as
	// needed so the orientations are all consistent
	std::unordered_set<tm_sync_uedge, tm_sync_uedge_hash> wavefront_edges;
	std::unordered_set<tm_sync_uedge, tm_sync_uedge_hash> interior_edges;
	tm_sync_sface &f_seed= synced_faces[*active_tris.begin()];
	active_tris.erase(active_tris.begin());
	wavefront_edges.insert(tm_sync_uedge(f_seed.v1, f_seed.v2));
	wavefront_edges.insert(tm_sync_uedge(f_seed.v2, f_seed.v3));
	wavefront_edges.insert(tm_sync_uedge(f_seed.v3, f_seed.v1));
	while (wavefront_edges.size()) {
	    tm_sync_uedge cedge = *wavefront_edges.begin();
	    // If we have a boundary edge, mark it as interior (i.e. not part
	    // of the wavefront)
	    if (ue_fmap[cedge].size() == 1) {
		interior_edges.insert(cedge);
		wavefront_edges.erase(wavefront_edges.begin());
		continue;
	    }
	    // Get the two triangles associated with this tm_sync_uedge.  If one of
	    // them is unvisited, process it
	    int f_ind = -1;
	    for (size_t j = 0; j < ue_fmap[cedge].size(); j++) {
		if (active_tris.find(ue_fmap[cedge][j]) == active_tris.end()) {
		    continue;
		}
		f_ind = ue_fmap[cedge][j];
		break;
	    }

	    // If neither face is unvisited, there's no work to do
	    if (f_ind == -1) {
		wavefront_edges.erase(wavefront_edges.begin());
		continue;
	    }

	    tm_sync_sface &bface = synced_faces[f_ind];
	    tm_sync_uedge ue[3] = {tm_sync_uedge(bface.v1, bface.v2), tm_sync_uedge(bface.v2, bface.v3), tm_sync_uedge(bface.v3, bface.v1)};
	    // For the new face, any edges that aren't already wavefront
	    // edges and aren't part of the interior set get added, and any
	    // that are already in the wavefront get moved to the interior set.
	    int ue_flag[3] = {0};
	    int tedge_ind = -1;
	    for (size_t k = 0; k < 3; k++) {
		if (wavefront_edges.find(ue[k]) != wavefront_edges.end()) {
		    ue_flag[k] = 1;
		}
	    }
	    for (size_t k = 0; k < 3; k++) {
		if (ue_flag[k]) {
		    interior_edges.insert(ue[k]);
		    wavefront_edges.erase(ue[k]);
		    if (ue_emap[ue[k]].size() == 2)
			tedge_ind = k;
		} else {
		    // If another part of the walk hasn't already reached
		    // this area of the mesh, extend the wavefront
		    if (interior_edges.find(ue[k]) == interior_edges.end()) {
			wavefront_edges.insert(ue[k]);
		    }
		}
	    }

	    // If we have all interior edges, no action to take
	    if (tedge_ind == -1)
		continue;

	    // If both the oriented edges assigned to ue[tedge_ind] are the same, then
	    // the new face needs to be flipped
	    if (ordered_edges[ue_emap[ue[tedge_ind]][0]] == ordered_edges[ue_emap[ue[tedge_ind]][1]]) {
		synced_faces[f_ind].flip();
		flip_cnt++;
		for (size_t k = 0; k < 3; k++) {
		    ordered_edges[synced_faces[f_ind].e[k]].flip();
		}
	    }
	}
    }

    // Once syncing operations are complete, make the final vertex assignments to
    // the face array.
    for (int i = 0; i < fcnt; ++i) {
	//bu_log("old: %d %d %d new: %d %d %d\n", of[i*3+0], of[i*3+1], of[i*3+2], synced_faces[i].v1, ordered_edges[synced_faces[i].e2].v1, ordered_edges[synced_faces[i].e3].v1);
	of[i*3+0] = synced_faces[i].v1;
	of[i*3+1] = synced_faces[i].v2;
	of[i*3+2] = synced_faces[i].v3;
    }

    return flip_cnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

