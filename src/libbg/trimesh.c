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
#include "string.h" /* for memcpy */
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


HIDDEN int
trimesh_degenerate_faces(size_t num_faces, int *fpoints, struct bg_trimesh_solid_errors *errors)
{
    size_t face_index, dindex;

    if (!num_faces || !fpoints) return 1;

    if (errors) {
	errors->degenerate.count = 0;
    }

    for (face_index = 0; face_index < num_faces; ++face_index) {
	const int * const face = &fpoints[face_index * 3];

	/* degenerate face */
	if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
	    if (!errors) return 1;
	    ++(errors->degenerate.count);
	}
    }

    if (errors->degenerate.count == 0) {
	return 0;
    }

    errors->degenerate.faces = (int *)bu_calloc(errors->degenerate.count, sizeof(int), "degenerate faces");
    dindex = 0;

    for (face_index = 0; face_index < num_faces; ++face_index) {
	const int * const face = &fpoints[face_index * 3];

	/* append degenerate face */
	if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
	    errors->degenerate.faces[dindex++] = face_index * 3;
	}
    }
    return 1;
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

	_trimesh_set_edge(&edge_list[face_index * 3 + 0], face[0], face[1]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 1], face[1], face[2]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 2], face[2], face[0]);
    }

    bu_sort(edge_list, num_edges, sizeof(struct trimesh_halfedge), _trimesh_halfedge_compare, NULL);

    return edge_list;
}

int
bg_trimesh_solid2(size_t vcnt, size_t fcnt, fastf_t *v, int *f, struct bg_trimesh_solid_errors *errors)
{
    size_t num_edges;
    struct trimesh_halfedge *edge_list;
    size_t i;
    int not_solid = 0;
    int unmatched_edges = 0;
    int excess_edges = 0;
    int misoriented_edges = 0;

    if (!vcnt || !v || !fcnt || !f) return 1;

    num_edges = 3 * fcnt;


    if (fcnt < 4 || vcnt < 4) return 1;
    if (!errors && num_edges % 2) return 1;

    if (trimesh_degenerate_faces(fcnt, f, errors)) return 1;
    if (!(edge_list = _trimesh_generate_edge_list(fcnt, f))) return 1;

    
#define UNTRACKED_EXIT \
    if (!errors) { \
	bu_free(edge_list, "edge_list"); \
	return 1; \
    }

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    UNTRACKED_EXIT;
	    ++unmatched_edges;
	    continue;
	}
	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    UNTRACKED_EXIT;
	    ++misoriented_edges;
	    continue;
	}
	/* only two half-edges may share an edge */
	if (i + 2 < num_edges && TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
	    UNTRACKED_EXIT;
	    ++excess_edges;
	}
    }

    /* If we either aren't tracking edges or we didn't have any problems, we're done */
    not_solid = unmatched_edges + excess_edges + misoriented_edges;
    if (!errors || !not_solid) {
	bu_free(edge_list, "edge_list");
	return 0;
    }

    /* If we got here, we're interesting in knowing what the problems are */
    if (unmatched_edges) {
	errors->unmatched.edges = (int *)bu_calloc(unmatched_edges, sizeof(int), "unmatched edge indices");
    }
    if (excess_edges) {
	errors->excess.edges = (int *)bu_calloc(excess_edges, sizeof(int), "excess edge indices");
    }
    if (misoriented_edges) {
	errors->misoriented.edges = (int *)bu_calloc(misoriented_edges, sizeof(int), "misoriented edge indices");
    }

    errors->unmatched.count = 0;
    errors->excess.count = 0;
    errors->misoriented.count = 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	/* each edge must have two half-edges */
	if (!TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 1])) {
	    errors->unmatched.edges[errors->unmatched.count * 2] = edge_list[i].va;
	    errors->unmatched.edges[errors->unmatched.count * 2 + 1] = edge_list[i].vb;
	    ++(errors->unmatched.count);
	    continue;
	}
	/* adjacent half-edges must be compatibly oriented */
	if (edge_list[i].flipped == edge_list[i + 1].flipped) {
	    errors->misoriented.edges[errors->misoriented.count * 2] = edge_list[i].va;
	    errors->misoriented.edges[errors->misoriented.count * 2 + 1] = edge_list[i].vb;
	    ++(errors->misoriented.count);
	    continue;
	}
	/* only two half-edges may share an edge */
	if (i + 2 < num_edges && TRIMESH_EDGE_EQUAL(edge_list[i], edge_list[i + 2])) {
	    errors->excess.edges[errors->excess.count * 2] = edge_list[i].va;
	    errors->excess.edges[errors->excess.count * 2 + 1] = edge_list[i].vb;
	    ++(errors->excess.count);
	}
    }

    bu_free(edge_list, "edge_list");
    return not_solid;
}

void
bg_free_trimesh_edges(struct bg_trimesh_edges *edges)
{
    if (!edges) return;

    if (edges->edges) {
	bu_free(edges->edges, "bg trimesh edges");
    }
    edges->count = 0;
}

void
bg_free_trimesh_faces(struct bg_trimesh_faces *faces)
{
    if (!faces) return;

    if (faces->faces) {
	bu_free(faces->faces, "bg trimesh faces");
    }
    faces->count = 0;
}

 void
 bg_free_trimesh_solid_errors(struct bg_trimesh_solid_errors *errors)
 {
     if (!errors) return;

     bg_free_trimesh_edges(&(errors->unmatched));
     bg_free_trimesh_edges(&(errors->misoriented));
     bg_free_trimesh_edges(&(errors->excess));
     bg_free_trimesh_faces(&(errors->degenerate));
 }

int
bg_trimesh_solid(size_t vcnt, size_t fcnt, fastf_t *v, int *f, int **bedges)
{
    int bedge_cnt = 0;

    if (bedges) {
	int copy_cnt = 0;
	struct bg_trimesh_solid_errors errors;

	bedge_cnt = bg_trimesh_solid2(vcnt, fcnt, v, f, &errors);
	*bedges = (int *)bu_calloc(bedge_cnt * 2, sizeof(int), "bad edges");

	memcpy(*bedges, errors.unmatched.edges, errors.unmatched.count * 2 * sizeof(int));
	copy_cnt += errors.unmatched.count * 2;

	memcpy(*bedges + copy_cnt, errors.misoriented.edges, errors.misoriented.count * 2 * sizeof(int));
	copy_cnt += errors.misoriented.count * 2;

	memcpy(*bedges + copy_cnt, errors.excess.edges, errors.excess.count * 2 * sizeof(int));

	bg_free_trimesh_solid_errors(&errors);
    } else {
	bedge_cnt = bg_trimesh_solid2(vcnt, fcnt, v, f, NULL);
    }

    return bedge_cnt;
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
