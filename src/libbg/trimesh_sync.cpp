/*                 T R I M E S H _ S Y N C . C P P
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
/** @file trimesh_sync.cpp
 *
 * Sync up triangle mesh faces to all point in a consistent direction
 * per their shared topology. (Does not guaranteed that they will be
 * "correct" (i.e. pointing outward) for a closed solid.)
 *
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <queue>
#include <vector>

#include "bu/log.h"
#include "bg/trimesh.h"

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


extern "C" int
bg_trimesh_sync(int *of, int *f, int fcnt)
{
    if (!of || !f || fcnt < 0)
	return -1;

    struct edge_use {
	int face;
	bool ascending;
    };
    struct adjacency {
	int face;
	bool opposite_parity;
    };

    std::map<tm_sync_uedge, std::vector<edge_use>> edge_uses;
    std::vector<bool> active((size_t)fcnt, true);
    for (int face = 0; face < fcnt; ++face) {
	for (int edge = 0; edge < 3; ++edge) {
	    const int first = f[3 * face + edge];
	    const int second = f[3 * face + (edge + 1) % 3];
	    edge_uses[tm_sync_uedge(first, second)].push_back(
		{face, first < second});
	}
    }

    /* Do not propagate through a face incident to non-manifold edge
     * topology.  Other components remain independently orientable. */
    for (const auto &entry : edge_uses) {
	if (entry.second.size() <= 2)
	    continue;
	for (const edge_use &use : entry.second)
	    active[(size_t)use.face] = false;
    }

    std::vector<std::vector<adjacency>> neighbors((size_t)fcnt);
    for (const auto &entry : edge_uses) {
	if (entry.second.size() != 2)
	    continue;
	const edge_use first = entry.second[0];
	const edge_use second = entry.second[1];
	if (!active[(size_t)first.face] || !active[(size_t)second.face])
	    continue;
	const bool opposite_parity = first.ascending == second.ascending;
	neighbors[(size_t)first.face].push_back(
	    {second.face, opposite_parity});
	neighbors[(size_t)second.face].push_back(
	    {first.face, opposite_parity});
    }

    std::vector<int> parity((size_t)fcnt, -1);
    int flip_count = 0;
    for (int seed = 0; seed < fcnt; ++seed) {
	if (!active[(size_t)seed] || parity[(size_t)seed] >= 0)
	    continue;
	std::queue<int> pending;
	std::vector<int> component;
	bool orientable = true;
	parity[(size_t)seed] = 0;
	pending.push(seed);
	while (!pending.empty()) {
	    const int face = pending.front();
	    pending.pop();
	    component.push_back(face);
	    for (const adjacency &next : neighbors[(size_t)face]) {
		const int required = parity[(size_t)face] ^
		    (next.opposite_parity ? 1 : 0);
		if (parity[(size_t)next.face] < 0) {
		    parity[(size_t)next.face] = required;
		    pending.push(next.face);
		} else if (parity[(size_t)next.face] != required) {
		    orientable = false;
		}
	    }
	}
	if (!orientable)
	    return -1;
	for (int face : component)
	    flip_count += parity[(size_t)face] ? 1 : 0;
    }

    if (of != f)
	std::copy(f, f + 3 * fcnt, of);
    for (int face = 0; face < fcnt; ++face) {
	if (parity[(size_t)face] == 1)
	    std::swap(of[3 * face], of[3 * face + 1]);
    }
    return flip_count;
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
