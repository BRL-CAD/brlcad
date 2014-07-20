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
 * Library to determine whether a BoT is closed.
 *
 */


#include "solidity.h"


#include <map>
#include <set>


namespace
{


typedef std::pair<std::size_t, std::size_t> Edge;


static bool register_edge(
    std::vector<std::set<Edge> > &vertex_edges,
    std::map<Edge, int> &edge_incidence_map,
    std::size_t va, std::size_t vb)
{
    const Edge edge(std::min(va, vb), std::max(va, vb));
    vertex_edges[va].insert(edge);
    vertex_edges[vb].insert(edge);

    // test fails if any edge borders more than two faces
    return ++edge_incidence_map[edge] <= 2;
}


}


int bot_is_closed(const rt_bot_internal *bot)
{
    std::map<Edge, int> edge_incidence_map;
    std::vector<std::set<Edge> > vertex_edges(bot->num_vertices);

    // map edges to number of incident faces
    // map vertices to edges
    for (std::size_t fi = 0; fi < bot->num_faces; ++fi) {
	const std::size_t v1 = bot->faces[fi * 3];
	const std::size_t v2 = bot->faces[fi * 3 + 1];
	const std::size_t v3 = bot->faces[fi * 3 + 2];

	bool valid = register_edge(vertex_edges, edge_incidence_map, v1, v2)
		     && register_edge(vertex_edges, edge_incidence_map, v1, v3)
		     && register_edge(vertex_edges, edge_incidence_map, v2, v3);

	if (!valid) return false;
    }

    // all edges must border exactly two faces
    for (std::map<Edge, int>::const_iterator it = edge_incidence_map.begin();
	 it != edge_incidence_map.end(); ++it)
	if (it->second != 2) return false;

    for (std::vector<std::set<Edge> >::const_iterator
	 vertex_it = vertex_edges.begin();
	 vertex_it != vertex_edges.end(); ++vertex_it) {

	// each vertex must be incident to at least two edges
	if (vertex_it->size() < 2) return false;

	// check if the edges form a closed or open fan
	for (std::set<Edge>::const_iterator it = vertex_it->begin();
	     it != vertex_it->end(); ++it) {
	    // TODO
	}
    }

    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
