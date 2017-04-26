/*                       T R I M E S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

#include "common.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bg/trimesh.h"

#define TRIMESH_EDGE_EQUAL(e1, e2) ((e1).va == (e2).va && (e1).vb == (e2).vb)


struct trimesh_halfedge {
    int va, vb;
    int flipped;
};


HIDDEN inline void
_trimesh_set_edge(struct trimesh_halfedge *edge, int va, int vb)
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
_trimesh_halfedge_compare(const void *pleft, const void *pright, void *UNUSED(context))
{
    const struct trimesh_halfedge * const left = (struct trimesh_halfedge *)pleft;
    const struct trimesh_halfedge * const right = (struct trimesh_halfedge *)pright;

    const int a = left->va - right->va;
    return a ? a : left->vb - right->vb;
}

HIDDEN struct trimesh_halfedge *
_trimesh_generate_edge_list(size_t fcnt, int *f)
{
    size_t num_edges;
    struct trimesh_halfedge *edge_list;
    size_t face_index;

    if (!fcnt || !f) return NULL;

    num_edges = 3 * fcnt;
    edge_list = (struct trimesh_halfedge *)bu_calloc(num_edges, sizeof(struct trimesh_halfedge), "edge_list");

    for (face_index = 0; face_index < fcnt; ++face_index) {
	const int * const face = &f[face_index * 3];

	if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
	    bu_free(edge_list, "edge_list");
	    return NULL;
	}

	_trimesh_set_edge(&edge_list[face_index * 3 + 0], face[0], face[1]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 1], face[1], face[2]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 2], face[2], face[0]);
    }

    bu_sort(edge_list, num_edges, sizeof(struct trimesh_halfedge), _trimesh_halfedge_compare, NULL);

    return edge_list;
}

int
bg_trimesh_solid(size_t vcnt, size_t fcnt, fastf_t *v, int *f)
{
    size_t num_edges;
    struct trimesh_halfedge *edge_list;
    size_t i;

    if (!vcnt || !v || !fcnt || !f) return 0;

    num_edges = 3 * fcnt;

    if (fcnt < 4 || vcnt < 4 || num_edges % 2) return 0;

    if (!(edge_list = _trimesh_generate_edge_list(fcnt, f))) return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges && TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}
    }

    bu_free(edge_list, "edge_list");
    return 1;
}

int
bg_trimesh_closed_fan(size_t vcnt, size_t fcnt, fastf_t *v, int *f)
{
    size_t num_edges;
    struct trimesh_halfedge *edge_list;
    size_t i;

    if (!vcnt || !v || !fcnt || !f) return 0;

    num_edges = 3 * fcnt;

    if (fcnt < 4 || vcnt < 4 || num_edges % 2) return 0;

    if (!(edge_list = _trimesh_generate_edge_list(fcnt, f))) return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges && TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}
    }

    bu_free(edge_list, "edge_list");
    return 1;
}


int
bg_trimesh_orientable(size_t vcnt, size_t fcnt, fastf_t *v, int *f)
{
    size_t num_edges;
    struct trimesh_halfedge *edge_list;
    size_t i;

    if (!vcnt || !v || !fcnt || !f) return 0;

    num_edges = 3 * fcnt;

    if (fcnt < 4 || vcnt < 4) return 0;

    if (!(edge_list = _trimesh_generate_edge_list(fcnt, f))) return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* skip if there is no adjacent half-edge */
	if (!TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    --i;
	    continue;
	}

	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}

	/* only two half-edges may share an edge */
	if (i + 2 < num_edges && TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
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
