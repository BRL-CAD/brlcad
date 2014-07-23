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


int bot_is_closed(const rt_bot_internal *bot)
{
    typedef std::pair<std::size_t, std::size_t> Edge;

    // map edges to number of incident faces
    std::map<Edge, int> edge_incidence_map;

    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

#define REGISTER_EDGE(va, vb) \
	do { \
	    const Edge e(std::min((va), (vb)), std::max((va), (vb))); \
	    ++edge_incidence_map[e]; \
	} while (false)

	REGISTER_EDGE(v1, v2);
	REGISTER_EDGE(v1, v3);
	REGISTER_EDGE(v2, v3);

#undef REGISTER_EDGE
    }

    // a mesh is closed if it has no boundary edges
    for (std::map<Edge, int>::const_iterator it = edge_incidence_map.begin();
	 it != edge_incidence_map.end(); ++it)
	if (it->second < 2) return false;

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
