/*                    S O L I D I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file solidity.cpp
 *
 * Library of algorithms to determine whether a BoT is solid.
 *
 */


#include "solidity.h"


#include <map>
#include <set>
#include <vector>


namespace
{


typedef std::pair<int, int> Edge;


static inline Edge
ordered_edge(int va, int vb)
{
    return Edge(std::min(va, vb), std::max(va, vb));
}


static bool
bot_is_orientable_register(std::map<Edge, int> &edge_order_map, int va, int vb)
{
    enum { ORDER_NONE = 0, ORDER_MIN_MAX, ORDER_MAX_MIN, ORDER_EDGE_FULL };

    int &order = edge_order_map[ordered_edge(va, vb)];

    switch (order) {
	case ORDER_NONE:
	    order = va < vb ? ORDER_MIN_MAX : ORDER_MAX_MIN;
	    return va != vb;

	case ORDER_MIN_MAX:
	    order = ORDER_EDGE_FULL;
	    return va > vb;

	case ORDER_MAX_MIN:
	    order = ORDER_EDGE_FULL;
	    return va < vb;

	case ORDER_EDGE_FULL:
	default:
	    return false;
    }
}


}


int
bot_is_closed(const rt_bot_internal *bot, int must_be_fan)
{
    // map edges to number of incident faces
    std::map<Edge, int> edge_face_count_map;

    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

#define REGISTER_EDGE(va, vb) \
	do { \
	    ++edge_face_count_map[ordered_edge((va), (vb))]; \
	} while (false)

	REGISTER_EDGE(v1, v2);
	REGISTER_EDGE(v2, v3);
	REGISTER_EDGE(v3, v1);

#undef REGISTER_EDGE
    }

    for (std::map<Edge, int>::const_iterator it = edge_face_count_map.begin();
	 it != edge_face_count_map.end(); ++it) {

	if (must_be_fan) {
	    // a mesh forms a closed fan if all edges are
	    // incident to exactly two faces
	    if (it->second != 2) return false;
	}  else {
	    // a mesh is closed if it has no boundary edges
	    if (it->second == 1) return false;
	}

    }

    return true;
}


int
bot_is_orientable(const rt_bot_internal *bot)
{
    std::map<Edge, int> edge_order_map;

    // a mesh is orientable if any two adjacent faces
    // have compatible orientation
    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

	if (!(bot_is_orientable_register(edge_order_map, v1, v2)
	      && bot_is_orientable_register(edge_order_map, v2, v3)
	      && bot_is_orientable_register(edge_order_map, v3, v1)))
	    return false;
    }

    return true;
}


int
bot_is_manifold(const rt_bot_internal *bot)
{
    std::map<Edge, int> edge_face_count_map;

    // map vertices to (num adjacent faces, incident_edges)
    typedef std::vector<std::pair<int, std::set<Edge> > > VERTEX_MAP;
    VERTEX_MAP vertex_map(bot->num_vertices);

    // a mesh is manifold if:
    // 1) each edge is incident to only one or two faces
    // 2) the faces incident to a vertex form a closed or open fan
    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

	++vertex_map[v1].first;
	++vertex_map[v2].first;
	++vertex_map[v3].first;

#define REGISTER_EDGE(va, vb) \
	do { \
	    const Edge edge = ordered_edge((va), (vb)); \
	    if (++edge_face_count_map[edge] > 2) \
		return false; \
	    vertex_map[(va)].second.insert(edge); \
	    vertex_map[(vb)].second.insert(edge); \
	} while (false)

	REGISTER_EDGE(v1, v2);
	REGISTER_EDGE(v2, v3);
	REGISTER_EDGE(v3, v1);

#undef REGISTER_EDGE
    }


    // I have seen two (seemingly) different definitions of "manifold"
    // in online references - some definitions simply require (1)
    // (i.e. http://www.lsi.upc.edu/~virtual/SGI/english/3_queries.html);
    // others additionally require (2)
    // (i.e. http://www.cs.mtu.edu/~shene/COURSES/cs3621/SLIDES/Mesh.pdf)
    //
    // Removing (2) gives results identical to those of the openNURBS algorithms,
    // so the code below is disabled.

#if 0

    for (VERTEX_MAP::const_iterator it = vertex_map.begin(); it != vertex_map.end();
	 ++it) {
	// check if the vertex is either
	// a) open fan: the vertex is at a border,
	//     with valence = (num adjacent faces) + 1
	// b) closed fan: the vertex is an inner vertex,
	//     with valence = (num adjacent faces)

	// valence: number of incident edges to a vertex
	const int valence = it->second.size();
	const int num_adj_faces = it->first;

	if (valence != num_adj_faces && valence != num_adj_faces + 1)
	    return false;
    }

#endif

    return true;
}


int bot_is_solid(const rt_bot_internal *bot)
{
    // note: self-intersection tests
    return
	bot_is_closed(bot, true)
	&& bot_is_orientable(bot)
	&& bot_is_manifold(bot);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
