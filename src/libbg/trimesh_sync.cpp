/*                 T R I M E S H _ S Y N C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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
#include <queue>
#include <set>
//#include <functional>
//#include <unordered_map>
//#include <unordered_set>
#include <vector>

#include "bu/log.h"
#include "bg/trimesh.h"

class oedge {
    public:
	oedge(int vert1, int vert2, int face_ind)
	    : v1(vert1), v2(vert2), f_ind(face_ind) {}
	~oedge() {};

	int v1;
	int v2;
	int f_ind;

	void flip()
	{
	    int tmp = v2;
	    v2 = v1;
	    v1 = tmp;
	}

	bool operator==(oedge other) const
	{
	    bool c1 = (v1 == other.v1);
	    bool c2 = (v2 == other.v2);
	    return (c1 && c2);
	}
};


class uedge {
    public:
	uedge(int vert1, int vert2) {
	    v1 = (vert1 < vert2) ? vert1 : vert2;
	    v2 = (vert2 < vert1) ? vert1 : vert2;
	}
	~uedge() {};
	int v1;
	int v2;

	bool operator==(uedge other) const
	{
	    bool c1 = (v1 == other.v1);
	    bool c2 = (v2 == other.v2);
	    return (c1 && c2);
	}

	bool operator<(uedge other) const
	{
	    bool c1 = (v1 < other.v1);
	    bool c1e = (v1 == other.v1);
	    bool c2 = (v2 < other.v2);
	    return (c1 || (c1e && c2));
	}

};

#if 0
// https://stackoverflow.com/a/27952689/2037687
class uedge_hash {
    public:
	size_t operator()(const uedge& p) const
	{
	    size_t hout = p.v1;
	    hout ^= p.v1 + 0x9e3779b97f4a7c16 + (p.v2 << 6) + (p.v2 >> 2);
	    return hout;
	}
};
#endif

class sface {
    public:
	size_t e1;
	size_t e2;
	size_t e3;
};

int
check_flip(
	size_t c1,
	size_t n1,
	std::vector<oedge> &ordered_edges,
	std::vector<sface> &synced_faces)
{
    oedge &curr = ordered_edges[c1];
    oedge &ne = ordered_edges[n1];

    //bu_log("curr: %d,%d ne: %d,%d\n", curr.v1, curr.v2, ne.v1, ne.v2);
    if (curr.v1 == ne.v1) {
	// Yes
	ordered_edges[synced_faces[ne.f_ind].e1].flip();
	ordered_edges[synced_faces[ne.f_ind].e2].flip();
	ordered_edges[synced_faces[ne.f_ind].e3].flip();

	oedge tmp = ordered_edges[synced_faces[ne.f_ind].e3];
	ordered_edges[synced_faces[ne.f_ind].e3] = ordered_edges[synced_faces[ne.f_ind].e2];
	ordered_edges[synced_faces[ne.f_ind].e2] = tmp;
	bu_log("flip %d\n", ordered_edges[n1].f_ind);
	return 1;
    }
    //bu_log("no flip %d\n", ordered_edges[synced_faces[ne.f_ind].e1].f_ind);
    return 0;
}


/* First, we take the first edge from that set and add it to our processing
 * queue.  We find the two associated oriented edges, and use the edge with the
 * lowest associated face id as the canonical "syncing" ordering to use for all
 * connected faces.  We then sync the other face, if necessary, and queue up
 * all other unordered edges associated with either face for further
 * processing. */
int
do_seeding(
	std::queue<size_t> &to_process,
	std::set<uedge> &to_check,
	std::map<uedge, std::vector<size_t>> &ue_emap,
	std::vector<oedge> &ordered_edges,
	std::vector<sface> &synced_faces)
{
    int ret = 0;

    if (!to_check.size())
	return ret;

    uedge seed = *to_check.begin();
    to_check.erase(to_check.begin());
    size_t s1 = ue_emap[seed][0];
    size_t s2 = ue_emap[seed][1];
    oedge e1 = ordered_edges[s1];
    oedge e2 = ordered_edges[s2];
    size_t eseed_ind = (e1.f_ind < e2.f_ind) ? s1 : s2;
    size_t esync_ind = (e1.f_ind > e2.f_ind) ? s1 : s2;
    oedge eseed = ordered_edges[eseed_ind];
    oedge esync = ordered_edges[esync_ind];

    // First step - do flip if needed
    ret = check_flip(eseed_ind, esync_ind, ordered_edges, synced_faces);

    // Get the 2 unprocessed edges from the seed face - they are
    // the beginnings of our processing
    {
	uedge se1(ordered_edges[synced_faces[eseed.f_ind].e1].v1, ordered_edges[synced_faces[eseed.f_ind].e1].v2);
	uedge se2(ordered_edges[synced_faces[eseed.f_ind].e2].v1, ordered_edges[synced_faces[eseed.f_ind].e2].v2);
	uedge se3(ordered_edges[synced_faces[eseed.f_ind].e3].v1, ordered_edges[synced_faces[eseed.f_ind].e3].v2);
	if (to_check.find(se1) != to_check.end()) {
	    to_process.push(synced_faces[eseed.f_ind].e1);
	    to_check.erase(se1);
	}
	if (to_check.find(se2) != to_check.end()) {
	    to_process.push(synced_faces[eseed.f_ind].e2);
	    to_check.erase(se2);
	}
	if (to_check.find(se3) != to_check.end()) {
	    to_process.push(synced_faces[eseed.f_ind].e3);
	    to_check.erase(se3);
	}
    }

    return ret;
}

extern "C" int
bg_trimesh_sync(int *of, int *f, int fcnt)
{
    if (!of || !f || fcnt < 0)
	return -1;

    std::vector<oedge> ordered_edges;
    std::map<uedge, std::vector<size_t>> ue_emap;
    std::map<uedge, std::vector<size_t>> ue_fmap;
    std::vector<sface> synced_faces;

    for (int i = 0; i < fcnt; ++i) {
	int v1 = f[i*3+0];
	int v2 = f[i*3+1];
	int v3 = f[i*3+2];
	sface nface;
	ordered_edges.push_back(oedge(v1, v2, i));
	ue_emap[uedge(v1,v2)].push_back(ordered_edges.size()-1);
	nface.e1 = ordered_edges.size()-1;
	ordered_edges.push_back(oedge(v2, v3, i));
	ue_emap[uedge(v2,v3)].push_back(ordered_edges.size()-1);
	nface.e2 = ordered_edges.size()-1;
	ordered_edges.push_back(oedge(v3, v1, i));
	ue_emap[uedge(v3,v1)].push_back(ordered_edges.size()-1);
	nface.e3 = ordered_edges.size()-1;
	synced_faces.push_back(nface);
	ue_fmap[uedge(v1,v2)].push_back(i);
	ue_fmap[uedge(v2,v3)].push_back(i);
	ue_fmap[uedge(v3,v1)].push_back(i);
    }

    /* Any unordered edge with less than or more than two associated faces
     * can't be used to trigger a sync operation.  Check the others.
     *
     * Because we are syncing, we must begin with a seed and "walk outward"
     * from there along the topology.  Because there is no guarantee that all
     * faces will be topologically connected to the seed, we need to be able to
     * re-start the process if we run out of queued edges but have not yet
     * processed all valid edges. We want topologically connected subsets of
     * the face set to be synced, even if "synced" is not well defined for
     * the entire face set.
     *
     * To support this, we first build up a set of all the edges we must check.
     */
    std::set<uedge> to_check;
    std::map<uedge, std::vector<size_t>>::iterator ue_it;
    for (ue_it = ue_emap.begin(); ue_it != ue_emap.end(); ue_it++) {
	if (ue_it->second.size() == 2)
	    to_check.insert(ue_it->first);
    }

    /* Find the first seed edge to use for syncing, and prepare the data
     * structures */
    std::queue<size_t> to_process;
    int flip_cnt = 0;
    flip_cnt += do_seeding(to_process, to_check, ue_emap, ordered_edges, synced_faces);

    while (to_process.size()) {

	size_t curr_ind = to_process.front();
	to_process.pop();

	oedge curr = ordered_edges[curr_ind];
	uedge uc(curr.v1, curr.v2);
	to_check.erase(uc);

	// Look up the other edge, flip the associated face if needed, and queue up
	// any not-yet-processed edges
	size_t e1_ind = ue_emap[uc][0];
	size_t e2_ind = ue_emap[uc][1];
	oedge e1 = ordered_edges[e1_ind];
	oedge e2 = ordered_edges[e2_ind];
	oedge ne = (e1 == curr) ? e2 : e1;
	size_t ne_ind = (e1 == curr) ? e2_ind : e1_ind;

	// First step - do edge flips if needed to reverse face
	flip_cnt += check_flip(curr_ind, ne_ind, ordered_edges, synced_faces);

	// Get the 2 unprocessed edges from the new face - queue them up unless
	// we have already see them
	uedge ue_ne1(ordered_edges[synced_faces[ne.f_ind].e1].v1, ordered_edges[synced_faces[ne.f_ind].e1].v2);
	uedge ue_ne2(ordered_edges[synced_faces[ne.f_ind].e2].v1, ordered_edges[synced_faces[ne.f_ind].e2].v2);
	uedge ue_ne3(ordered_edges[synced_faces[ne.f_ind].e3].v1, ordered_edges[synced_faces[ne.f_ind].e3].v2);
	if (to_check.find(ue_ne1) != to_check.end()) {
	    to_process.push(synced_faces[ne.f_ind].e1);
	    to_check.erase(ue_ne1);
	}
	if (to_check.find(ue_ne2) != to_check.end()) {
	    to_process.push(synced_faces[ne.f_ind].e2);
	    to_check.erase(ue_ne2);
	}
	if (to_check.find(ue_ne3) != to_check.end()) {
	    to_process.push(synced_faces[ne.f_ind].e3);
	    to_check.erase(ue_ne3);
	}

	if (!to_process.size()) {
	    // If we've cleared the queue, see if there are any other topological
	    // islands that we need to seed
	    flip_cnt += do_seeding(to_process, to_check, ue_emap, ordered_edges, synced_faces);
	}
    }

    // Once syncing operations are complete, make the final vertex assignments to
    // the face array.
    for (int i = 0; i < fcnt; ++i) {
	//bu_log("old: %d %d %d new: %d %d %d\n", of[i*3+0], of[i*3+1], of[i*3+2], ordered_edges[synced_faces[i].e1].v1, ordered_edges[synced_faces[i].e2].v1, ordered_edges[synced_faces[i].e3].v1);
	of[i*3+0] = ordered_edges[synced_faces[i].e1].v1;
	of[i*3+1] = ordered_edges[synced_faces[i].e2].v1;
	of[i*3+2] = ordered_edges[synced_faces[i].e3].v1;
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

