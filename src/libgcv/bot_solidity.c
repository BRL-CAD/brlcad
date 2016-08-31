/*                  B O T _ S O L I D I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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
/** @file libgcv/bot_solidity.c
 *
 * Topological tests for determining whether a given BoT satisfies
 * the conditions for solidity.
 *
 */


#include "common.h"
#include "bu/sort.h"
#include "gcv/util.h"

#include <stdlib.h>


#define EDGE_EQUAL(e1, e2) ((e1).va == (e2).va && (e1).vb == (e2).vb)


struct halfedge {
    int va, vb;
    int flipped;
};


HIDDEN inline void
set_edge(struct halfedge *edge, int va, int vb)
{
    if (va < vb) {
	edge->va = va;
	edge->vb = vb;
	edge->flipped = 0;
    } else {
	edge->va = vb;
	edge->vb = va;
	edge->flipped = 1;
    }
}


HIDDEN int
halfedge_compare(const void *pleft, const void *pright, void *UNUSED(context))
{
    const struct halfedge * const left = (struct halfedge *)pleft;
    const struct halfedge * const right = (struct halfedge *)pright;

    const int a = left->va - right->va;
    return a ? a : left->vb - right->vb;
}


HIDDEN struct halfedge *
generate_edge_list(const struct rt_bot_internal *bot)
{
    size_t num_edges;
    struct halfedge *edge_list;
    size_t face_index;

    RT_BOT_CK_MAGIC(bot);

    num_edges = 3 * bot->num_faces;
    edge_list = (struct halfedge *)bu_calloc(num_edges, sizeof(struct halfedge),
					     "edge_list");

    for (face_index = 0; face_index < bot->num_faces; ++face_index) {
	const int * const face = &bot->faces[face_index * 3];

	if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
	    bu_free(edge_list, "edge_list");
	    return NULL;
	}

	set_edge(&edge_list[face_index * 3 + 0], face[0], face[1]);
	set_edge(&edge_list[face_index * 3 + 1], face[1], face[2]);
	set_edge(&edge_list[face_index * 3 + 2], face[2], face[0]);
    }

    bu_sort(edge_list, num_edges, sizeof(struct halfedge), halfedge_compare, NULL);

    return edge_list;
}


int
gcv_bot_is_solid(const struct rt_bot_internal *bot)
{
    size_t num_edges;
    struct halfedge *edge_list;
    size_t i;

    RT_BOT_CK_MAGIC(bot);

    num_edges = 3 * bot->num_faces;

    if (bot->num_faces < 4 || bot->num_vertices < 4 || num_edges % 2)
	return 0;

    if (!(edge_list = generate_edge_list(bot)))
	return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges)
	    if (EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
		bu_free(edge_list, "edge_list");
		return 0;
	    }
    }

    bu_free(edge_list, "edge_list");
    return 1;
}


int
gcv_bot_is_closed_fan(const struct rt_bot_internal *bot)
{
    size_t num_edges;
    struct halfedge *edge_list;
    size_t i;

    RT_BOT_CK_MAGIC(bot);

    num_edges = 3 * bot->num_faces;

    if (bot->num_faces < 4 || bot->num_vertices < 4 || num_edges % 2)
	return 0;

    if (!(edge_list = generate_edge_list(bot)))
	return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges)
	    if (EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
		bu_free(edge_list, "edge_list");
		return 0;
	    }
    }

    bu_free(edge_list, "edge_list");
    return 1;
}


int
gcv_bot_is_orientable(const struct rt_bot_internal *bot)
{
    size_t num_edges;
    struct halfedge *edge_list;
    size_t i;

    RT_BOT_CK_MAGIC(bot);

    num_edges = 3 * bot->num_faces;

    if (bot->num_faces < 4 || bot->num_vertices < 4)
	return 0;

    if (!(edge_list = generate_edge_list(bot)))
	return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* skip if there is no adjacent half-edge */
	if (!EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    --i;
	    continue;
	}

	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges)
	    if (EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
		bu_free(edge_list, "edge_list");
		return 0;
	    }
    }

    bu_free(edge_list, "edge_list");
    return 1;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
