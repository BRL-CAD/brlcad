/*                       T R I M E S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

HIDDEN inline void
_trimesh_set_edge(struct bg_trimesh_halfedge *edge, int va, int vb)
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
_bg_trimesh_halfedge_compare(const void *pleft, const void *pright, void *UNUSED(context))
{
    const struct bg_trimesh_halfedge * const left = (struct bg_trimesh_halfedge *)pleft;
    const struct bg_trimesh_halfedge * const right = (struct bg_trimesh_halfedge *)pright;

    const int a = left->va - right->va;
    return a ? a : left->vb - right->vb;
}

struct bg_trimesh_halfedge *
bg_trimesh_generate_edge_list(int fcnt, int *f)
{
    int num_edges;
    struct bg_trimesh_halfedge *edge_list;
    int face_index;

    if (fcnt < 1 || !f) return NULL;

    num_edges = 3 * fcnt;
    edge_list = (struct bg_trimesh_halfedge *)bu_calloc(num_edges, sizeof(struct bg_trimesh_halfedge), "edge_list");

    for (face_index = 0; face_index < fcnt; ++face_index) {
	const int * const face = &f[face_index * 3];

	_trimesh_set_edge(&edge_list[face_index * 3 + 0], face[0], face[1]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 1], face[1], face[2]);
	_trimesh_set_edge(&edge_list[face_index * 3 + 2], face[2], face[0]);
    }

    bu_sort(edge_list, num_edges, sizeof(struct bg_trimesh_halfedge), _bg_trimesh_halfedge_compare, NULL);

    return edge_list;
}

int
bg_trimesh_face_exit(int UNUSED(face_idx), void *UNUSED(data))
{
    return 0;
}

int
bg_trimesh_face_continue(int UNUSED(face_idx), void *UNUSED(data))
{
    return 1;
}

int
bg_trimesh_face_gather(int face_idx, void *data)
{
    if (data && face_idx >= 0) {
	struct bg_trimesh_faces *faces = (struct bg_trimesh_faces *)data;
	faces->faces[faces->count++] = face_idx * 3;
    }
    return 1;
}

int
bg_trimesh_edge_exit(struct bg_trimesh_halfedge *UNUSED(edge), void *UNUSED(data))
{
    return 0;
}

int
bg_trimesh_edge_continue(struct bg_trimesh_halfedge *UNUSED(edge), void *UNUSED(data))
{
    return 1;
}

int
bg_trimesh_edge_gather(struct bg_trimesh_halfedge *edge, void *data)
{
    struct bg_trimesh_edges *bad_edges = (struct bg_trimesh_edges *)data;

    if (bad_edges) {
	bad_edges->edges[bad_edges->count * 2] = edge->va;
	bad_edges->edges[bad_edges->count * 2 + 1] = edge->vb;
	++(bad_edges->count);
    }

    return 1;
}

int
bg_trimesh_degenerate_faces(int num_faces, int *fpoints, bg_face_error_func_t degenerate_func, void *data)
{
    int face_index;
    int ecount = 0;

    if (num_faces < 1 || !fpoints) return ecount;

    for (face_index = 0; face_index < num_faces; ++face_index) {
	const int * const face = &fpoints[face_index * 3];

	/* degenerate face */
	if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
	    ++ecount;
	    if (degenerate_func && !degenerate_func(face_index, data)) {
		return ecount;
	    }
	}
    }
    return ecount;
}

/**
 * @param[out] edge_skip Number of edges after current caller should skip (initialized to 0).
 */
typedef int (*bg_edge_filter_func_t)(int *edge_skip, int num_edges, struct bg_trimesh_halfedge *edge_list, int cur_idx);

HIDDEN int
edge_filter_all(int *UNUSED(edge_skip), int UNUSED(num_edges), struct bg_trimesh_halfedge *UNUSED(edge_list), int UNUSED(cur_idx))
{
    return 1;
}

HIDDEN int
edge_copies(int num_edges, struct bg_trimesh_halfedge *edge_list, int cur_idx)
{
    int nxt_idx = cur_idx + 1;
    while (nxt_idx < num_edges && TRIMESH_EDGE_EQUAL(edge_list[cur_idx], edge_list[nxt_idx])) {
	++nxt_idx;
    }
    return nxt_idx - cur_idx;
}

HIDDEN int
edge_unmatched(int *edge_skip, int num_edges, struct bg_trimesh_halfedge *edge_list, int cur_idx)
{
    if (edge_copies(num_edges, edge_list, cur_idx) == 1) {
	return 1;
    }
    *edge_skip = 1;
    return 0;
}

HIDDEN int
edge_overmatched(int *edge_skip, int num_edges, struct bg_trimesh_halfedge *edge_list, int cur_idx)
{
    int copies = edge_copies(num_edges, edge_list, cur_idx);
    *edge_skip = copies - 1;

    return copies > 2;
}

HIDDEN int
edge_misoriented(int *edge_skip, int num_edges, struct bg_trimesh_halfedge *edge_list, int cur_idx)
{
    int copies = edge_copies(num_edges, edge_list, cur_idx);
    *edge_skip = copies - 1;

    return (copies != 2 || edge_list[cur_idx].flipped == edge_list[cur_idx + 1].flipped);
}

HIDDEN int
trimesh_count_error_edges(
    int num_edges,
    struct bg_trimesh_halfedge *edge_list,
    bg_edge_filter_func_t filter_func,
    bg_edge_error_funct_t error_edge_func,
    void *data)
{
    int i, skip;
    int matching_edges = 0;

    if (num_edges < 1 || !edge_list) return 0;

    if (!filter_func) {
	filter_func = edge_filter_all;
    }

    for (i = 0; i < num_edges; i += 1 + skip) {
	skip = 0;
	if (filter_func(&skip, num_edges, edge_list, i)) {
	    ++matching_edges;
	    if (error_edge_func && !error_edge_func(&edge_list[i], data)) {
		return matching_edges;
	    }
	}
    }
    return matching_edges;
}

int
bg_trimesh_unmatched_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data)
{
    return trimesh_count_error_edges(num_edges, edge_list, edge_unmatched, error_edge_func, data);
}

int
bg_trimesh_misoriented_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data)
{
    return trimesh_count_error_edges(num_edges, edge_list, edge_misoriented, error_edge_func, data);
}

int
bg_trimesh_excess_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data)
{  
    return trimesh_count_error_edges(num_edges, edge_list, edge_overmatched, error_edge_func, data);
}

int
bg_trimesh_solid2(int vcnt, int fcnt, fastf_t *v, int *f, struct bg_trimesh_solid_errors *errors)
{
    int num_edges = 3 * fcnt;
    struct bg_trimesh_halfedge *edge_list;
    int not_solid = 0;
    int degenerate_faces = 0;
    int unmatched_edges = 0;
    int excess_edges = 0;
    int misoriented_edges = 0;
    bg_edge_error_funct_t bad_edge_func;

    if (vcnt < 4 || fcnt < 4 || !v || !f) return 1;

    if (!errors && num_edges % 2) return 1;

    if (errors) {
	degenerate_faces = bg_trimesh_degenerate_faces(fcnt, f, bg_trimesh_face_continue, NULL);

	if (degenerate_faces) {
	    errors->degenerate.count = 0;
	    errors->degenerate.faces = (int *)bu_calloc(degenerate_faces, sizeof(int), "degenerate faces");
	    bg_trimesh_degenerate_faces(fcnt, f, bg_trimesh_face_gather, &(errors->degenerate));
	}
    } else {
	degenerate_faces = bg_trimesh_degenerate_faces(fcnt, f, bg_trimesh_face_exit, NULL);
    }

    if (degenerate_faces) {
	return 1;
    }

    if (!(edge_list = bg_trimesh_generate_edge_list(fcnt, f))) return 1;

    bad_edge_func = errors ? bg_trimesh_edge_continue : bg_trimesh_edge_exit;
    
    if ((unmatched_edges = bg_trimesh_unmatched_edges(num_edges, edge_list, bad_edge_func, NULL)) ||
	(misoriented_edges = bg_trimesh_misoriented_edges(num_edges, edge_list, bad_edge_func, NULL)) ||
	(excess_edges = bg_trimesh_excess_edges(num_edges, edge_list, bad_edge_func, NULL)))
    {
	if (!errors) { 
	    bu_free(edge_list, "edge_list"); 
	    return 1; 
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
	errors->unmatched.count = 0;
	errors->unmatched.edges = (int *)bu_calloc(unmatched_edges * 2, sizeof(int), "unmatched edge indices");
	bg_trimesh_unmatched_edges(num_edges, edge_list, bg_trimesh_edge_gather, &(errors->unmatched));
    }
    if (excess_edges) {
	errors->excess.count = 0;
	errors->excess.edges = (int *)bu_calloc(excess_edges * 2, sizeof(int), "excess edge indices");
	bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_gather, &(errors->excess));
    }
    if (misoriented_edges) {
	errors->misoriented.count = 0;
	errors->misoriented.edges = (int *)bu_calloc(misoriented_edges * 2, sizeof(int), "misoriented edge indices");
	bg_trimesh_misoriented_edges(num_edges, edge_list, bg_trimesh_edge_gather, &(errors->misoriented));
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
bg_trimesh_solid(int vcnt, int fcnt, fastf_t *v, int *f, int **bedges)
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
bg_trimesh_manifold_closed(int vcnt, int fcnt, fastf_t *v, int *f)
{
    int num_edges = 3 * fcnt;
    struct bg_trimesh_halfedge *edge_list;
    int i;

    if (vcnt < 4 || fcnt < 4 || !v || !f) return 0;

    if (num_edges % 2 || !(edge_list = bg_trimesh_generate_edge_list(fcnt, f))) return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	if (edge_copies(num_edges, edge_list, i) != 2) {
	    bu_free(edge_list, "edge_list");
	    return 0;
	}
    }

    bu_free(edge_list, "edge_list");
    return 1;
}


int
bg_trimesh_oriented(int vcnt, int fcnt, fastf_t *v, int *f)
{
    int num_edges = 3 * fcnt;
    struct bg_trimesh_halfedge *edge_list;
    int i, copies;

    if (vcnt < 4 || fcnt < 4 || !v || !f) return 0;

    if (!(edge_list = bg_trimesh_generate_edge_list(fcnt, f))) return 0;

    for (i = 0; i + 1 < num_edges; i += 2) {
	copies = edge_copies(num_edges, edge_list, i);

	/* skip if there is no adjacent half-edge */
	if (copies == 1) {
	    --i;
	    continue;
	}

	/* adjacent half-edges must be compatibly oriented */
	if (copies != 2 || edge_list[i].flipped == edge_list[i + 1].flipped) {
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
