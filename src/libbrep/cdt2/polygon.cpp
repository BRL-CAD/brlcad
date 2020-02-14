/*                   P O L Y G O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file polygon.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "brep/pullback.h"
#include "./cdt.h"


bool
polygon_t::add_ordered_edge(poly_edge_t &pe)
{
    long prev = -1;
    long next = -1;

    // TODO - an RTree lookup to find the connectivity candidates rather than
    // wading through all edges - won't matter much for small loops, but we
    // want the RTree anyway and it may help for large, heavily subdivided
    // loops produced by finer triangulations.
    for (size_t i = 0; i < p_pedges_vect.size(); i++) {
	poly_edge_t &ce = p_pedges_vect[i];
	if (ce == pe) continue;

	if (ce.v[1] == pe.v[0]) {
	    prev = ce.vect_ind;
	}

	if (ce.v[0] == pe.v[1]) {
	    next = ce.vect_ind;
	}
    }

    if (prev != -1) {
	p_pedges_vect[prev].next = pe.vect_ind;
	pe.prev = prev;
    }

    if (next != -1) {
	p_pedges_vect[next].prev = pe.vect_ind;
	pe.next = next;
    }

    p_pnts_vect[pe.v[0]].ecnt++;
    p_pnts_vect[pe.v[1]].ecnt++;

    // TODO - add to RTree

    return true;
}

void
polygon_t::remove_ordered_edge(poly_edge_t &pe)
{
    p_pnts_vect[pe.v[0]].ecnt--;
    p_pnts_vect[pe.v[1]].ecnt--;

    // TODO - use RTree.
    for (size_t i = 0; i < p_pedges_vect.size(); i++) {
	poly_edge_t &oe = p_pedges_vect[i];
	if (oe.prev == pe.vect_ind) {
	    oe.prev = -1;
	}
	if (oe.next == pe.vect_ind) {
	    oe.next = -1;
	}
    }

    // TODO - erase from RTree

    // TODO - return pe to queue
}



/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

