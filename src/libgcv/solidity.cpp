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
/** @file libgcv/solidity.cpp
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


class Edge
{
public:
    Edge() : m_vertices(0, 0), m_was_flipped(false)
    {}

    bool set(int va, int vb)
    {
	if (va == vb)
	    return false;

	if (va < vb) {
	    m_vertices.first = va;
	    m_vertices.second = vb;
	    m_was_flipped = false;
	} else {
	    m_vertices.first = vb;
	    m_vertices.second = va;
	    m_was_flipped = true;
	}

	return true;
    }


    const std::pair<int, int> &get() const
    {
	return m_vertices;
    }


    const bool &was_flipped() const
    {
	return m_was_flipped;
    }


private:
    std::pair<int, int> m_vertices;
    bool m_was_flipped;
};


static bool
edge_compare(const Edge &edge_a, const Edge &edge_b)
{
    return edge_a.get() < edge_b.get();
}


}


int
bot_is_solid(const rt_bot_internal *bot)
{
    const std::size_t num_edges = 3 * bot->num_faces;

    if (bot->num_faces < 4 || bot->num_vertices < 4 || num_edges % 2)
	return false;

    std::vector<Edge> edges(num_edges);
    {
	std::vector<Edge>::iterator edge_it = edges.begin();

	for (std::size_t face_index = 0; face_index < bot->num_faces; ++face_index) {
	    const int *face = &bot->faces[face_index * 3];

	    if (!((edge_it++)->set(face[0], face[1])
		  && (edge_it++)->set(face[1], face[2])
		  && (edge_it++)->set(face[2], face[0])))
		return false;
	}
    }

    std::sort(edges.begin(), edges.end(), edge_compare);

    for (std::vector<Edge>::const_iterator it = edges.begin(), next = it + 1;
	 it != edges.end(); it += 2, next += 2) {
	// each edge must have two half-edges
	if (it->get() != next->get()) return false;

	// adjacent half-edges must be compatably oriented
	if (it->was_flipped() == next->was_flipped()) return false;

	// only two half-edges may share an edge
	if ((next + 1) != edges.end())
	    if (it->get() == (next + 1)->get()) return false;
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
