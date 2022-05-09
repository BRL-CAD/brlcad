/*                       T R I M E S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include <set>
#include <map>

#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bg/trimesh.h"
#include "bn/plane.h"

#define TRIMESH_EDGE_EQUAL(e1, e2) ((e1).va == (e2).va && (e1).vb == (e2).vb)

static inline void
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
	faces->faces[faces->count++] = face_idx;
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

    bu_free(edges->edges, "bg trimesh edges");
    edges->count = 0;
}

void
bg_free_trimesh_faces(struct bg_trimesh_faces *faces)
{
    if (!faces) return;

    bu_free(faces->faces, "bg trimesh faces");
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

HIDDEN struct bg_trimesh_edges *
get_unmatched_edges(int num_faces, int *faces)
{
    int count;
    int num_edges = 3 * num_faces;
    struct bg_trimesh_edges *unmatched = NULL;
    struct bg_trimesh_halfedge *edge_list = NULL;

    if (!faces || num_faces < 1) {
	return NULL;
    }

    /* find open edges */
    edge_list = bg_trimesh_generate_edge_list(num_faces, faces);
    count = bg_trimesh_unmatched_edges(num_edges, edge_list, bg_trimesh_edge_continue, NULL);

    if (count > 0) {
	BU_GET(unmatched, struct bg_trimesh_edges);
	unmatched->edges = (int *)bu_calloc(unmatched->count * 2, sizeof(int), "unmatched edges");
	unmatched->count = bg_trimesh_unmatched_edges(num_edges, edge_list, bg_trimesh_edge_gather, &unmatched);
    }

    return unmatched;
}

struct edge_list {
    struct bu_list l;
    int edge[2];
    int used;
};

HIDDEN struct bu_list *
edges_to_list(struct bg_trimesh_edges *edges) {
    int i;
    struct bu_list *headp;
    struct edge_list *item;

    BU_GET(headp, struct bu_list);
    BU_LIST_INIT(headp);

    for (i = 0; i < edges->count; ++i) {
	BU_GET(item, struct edge_list);
	V2MOVE(item->edge, &edges->edges[i * 2]);
	item->used = 0;
	BU_LIST_APPEND(headp, &item->l);
    }

    return headp;
}

HIDDEN struct bg_trimesh_edges *
list_to_edges(struct bu_list *edge_list)
{
    int i, num_edges = bu_list_len(edge_list);
    struct edge_list *item;
    struct bg_trimesh_edges *edges = NULL;

    if (num_edges < 1) return NULL;

    BU_GET(edges, struct bg_trimesh_edges);

    edges->count = num_edges;
    edges->edges = (int *)bu_calloc(num_edges * 2, sizeof(int), "trimesh edges");

    i = 0;
    for (BU_LIST_FOR(item, edge_list, edge_list)) {
	V2MOVE(&(edges->edges[i * 2]), item->edge);
	++i;
    }

    return edges;
}

HIDDEN struct edge_list *
first_unused_edge_with_endpoint(struct bu_list *headp, int idx)
{
    struct edge_list *item;

    for (BU_LIST_FOR(item, edge_list, headp)) {
	if (!item->used && (item->edge[0] == idx || item->edge[1] == idx)) {
	    return item;
	}
    }

    return NULL;
}

#if 0
HIDDEN int
list_has_endpoints(struct bu_list *headp, int endpoints[2])
{
    int i, j;
    struct edge_list *first, *last;
    first = (struct edge_list *)BU_LIST_FIRST(bu_list, headp);
    last = (struct edge_list *)BU_LIST_LAST(bu_list, headp);

    for (i = 0; i < 2; i++) {
	for (j = 0; j < 2; j++ ) {
	    if (first->edge[i] == endpoints[0] && last->edge[j] == endpoints[1]) {
		return 1;
	    }
	}
    }
    return 0;
}
#endif

HIDDEN void
flip_edge(int edge[2])
{
    int tmp = edge[0];
    edge[0] = edge[1];
    edge[1] = tmp;
}

HIDDEN void
start_edge_from_endpoint(int edge[2], int endpoint)
{
    if (edge[0] != endpoint) flip_edge(edge);
}

HIDDEN struct edge_list *
dup_edge_item(struct edge_list *item)
{
    struct edge_list *new_item;

    BU_GET(new_item, struct edge_list);
    *new_item = *item;

    return new_item;
}

HIDDEN struct bu_list *
get_edges_starting_from_endpoint(struct bu_list *search_list, int startpt)
{
    struct bu_list *edges;
    struct edge_list *item;

    BU_GET(edges, struct bu_list);
    BU_LIST_INIT(edges);

    while ((item = first_unused_edge_with_endpoint(search_list, startpt))) {
	struct edge_list *new_item = dup_edge_item(item);

	start_edge_from_endpoint(new_item->edge, startpt);

	BU_LIST_APPEND(edges, &new_item->l);
    }

    if (BU_LIST_IS_EMPTY(edges)) {
	BU_PUT(edges, struct bu_list);
	edges = NULL;
    }

    return edges;
}

HIDDEN struct edge_list *
get_parent_edge(struct bu_list *parent_options, struct edge_list *child)
{
    struct edge_list *item;
    for (BU_LIST_FOR(item, edge_list, parent_options)) {
	if (item->edge[1] == child->edge[0]) {
	    return item;
	}
    }
    return NULL;
}

HIDDEN struct bu_list *
get_chain_ending_at_edge(struct bu_list *edge_options[], int num_parents, struct edge_list *end)
{
    int i;
    struct bu_list *chain;
    struct edge_list *copy, *parent, *child;

    BU_GET(chain, struct bu_list);
    BU_LIST_INIT(chain);

    child = end;
    for (i = num_parents; i >= 0; --i) {
	parent = get_parent_edge(edge_options[i], child);
	copy = dup_edge_item(parent);
	BU_LIST_PUSH(chain, copy);
	child = parent;
    }

    if (BU_LIST_IS_EMPTY(chain)) {
	BU_PUT(chain, struct bu_list);
	chain = NULL;
    }

    return chain;
}

HIDDEN struct bg_trimesh_edges *
get_edges_ending_at_edge(struct bu_list *edge_options[], int num_parents, struct edge_list *end)
{
    return list_to_edges(get_chain_ending_at_edge(edge_options, num_parents, end));
}

HIDDEN struct edge_list *
get_edge_ending_at_endpoint(struct bu_list *edge_list, int endpoint)
{
    struct edge_list *item;
    for (BU_LIST_FOR(item, edge_list, edge_list)) {
	if (item->edge[1] == endpoint) {
	    return item;
	}
    }
    return NULL;
}

HIDDEN struct bu_list *
get_edges_that_follow_edges(struct bu_list *search_list, struct bu_list *UNUSED(start_edges))
{
    struct edge_list *item;
    struct bu_list *next_edges, *cur_next;

    BU_GET(next_edges, struct bu_list);
    BU_LIST_INIT(next_edges);

    for (BU_LIST_FOR(item, edge_list, search_list)) {
	cur_next = get_edges_starting_from_endpoint(search_list, item->edge[1]);

	if (cur_next) {
	    BU_LIST_APPEND_LIST(next_edges, cur_next);
	}
    }

    return next_edges;
}

HIDDEN struct bg_trimesh_edges *
find_chain_with_endpoints(int endpoints[2], struct bg_trimesh_edges *edge_set, int max_chain_edges)
{
    int i;
    int found_chain;
    struct edge_list *terminal_edge;
    struct bg_trimesh_edges *chain_edges = NULL;
    struct bu_list *search_list = edges_to_list(edge_set);
    struct bu_list **edge_options;

    edge_options = (struct bu_list **)bu_calloc(max_chain_edges, sizeof(struct bu_list *), "edge options");
    edge_options[0] = get_edges_starting_from_endpoint(search_list, endpoints[0]);

    found_chain = 0;

    for (i = 1; i <= max_chain_edges && !found_chain; ++i) {
	edge_options[i] = get_edges_that_follow_edges(search_list, edge_options[i - 1]);

	terminal_edge = get_edge_ending_at_endpoint(edge_options[i], endpoints[1]);
	if (terminal_edge != NULL) {
	    found_chain = 1;
	    chain_edges = get_edges_ending_at_edge(edge_options, i, terminal_edge);
	    break;
	}
    }

    for (; i >= 0; --i) {
	bu_list_free(edge_options[i]);
	BU_PUT(edge_options[i], struct bu_list);
    }
    bu_free(edge_options, "edge options");

    return chain_edges;
}

HIDDEN double
distance_point_to_edge(int UNUSED(num_vertices), fastf_t *vertices, int point, int edge[2])
{
    int rc;
    double dist;
    struct bn_tol tol;
    point_t pt, pca;
    point_t edge_pts[2];

    VMOVE(pt, &(vertices[point]));
    VMOVE(edge_pts[0], &(vertices[edge[0] * 3]));
    VMOVE(edge_pts[1], &(vertices[edge[1] * 3]));

    BN_TOL_INIT(&tol);
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;
    tol.perp = SMALL_FASTF;
    tol.para = 1 - SMALL_FASTF;

    rc = bn_dist_pnt3_lseg3(&dist, pca, edge_pts[0], edge_pts[1], pt, &tol);

    return rc ? dist : 0.0;
}

HIDDEN double
max_distance_chain_to_edge(int num_vertices, fastf_t *vertices, struct bg_trimesh_edges *chain, int edge[2])
{
    int i;
    double max_dist;

    max_dist = 0.0;
    for (i = 0; i < chain->count; i += 2) {
	double dist = distance_point_to_edge(num_vertices, vertices, chain->edges[i], edge);
	V_MAX(max_dist, dist);
    }

    return max_dist;
}

int bg_trimesh_hanging_nodes(int num_vertices, int num_faces, fastf_t *vertices, int *faces, struct bg_trimesh_solid_errors *errors)
{
    int i, hanging_nodes = 0;
    struct bu_list unclosed_edges;
    struct bg_trimesh_edges *unmatched = get_unmatched_edges(num_faces, faces);

    if (!unmatched) {
	return 0;
    }

    BU_LIST_INIT(&unclosed_edges);

    for (i = 0; i < unmatched->count; ++i) {
	struct bg_trimesh_edges *chain;
	int *edge = &(unmatched->edges[i * 2]);

	chain = find_chain_with_endpoints(edge, unmatched, 4);

	if (!chain || max_distance_chain_to_edge(num_vertices, vertices, chain, edge) > BN_TOL_DIST) {
	    struct edge_list *item;
	    BU_GET(item, struct edge_list);
	    V2MOVE(item->edge, edge);

	    BU_LIST_APPEND(&unclosed_edges, &item->l);
	}

	if (chain) {
	    hanging_nodes += chain->count - 2;
	    bg_free_trimesh_edges(chain);
	    BU_PUT(chain, struct bg_trimesh_edges);
	}
    }

    if (errors && BU_LIST_NON_EMPTY(&unclosed_edges)) {
	struct bg_trimesh_edges *nunmatched = list_to_edges(&unclosed_edges);
	int num_edges = nunmatched->count;

	errors->unmatched.count = num_edges;
	errors->unmatched.edges = (int *)bu_calloc(num_edges * 2, sizeof(int), "unmatched edges");
	memcpy(errors->unmatched.edges, nunmatched->edges, num_edges * 2);

	bg_free_trimesh_edges(nunmatched);
	BU_PUT(nunmatched, struct bg_trimesh_edges);
    }

    bu_list_free(&unclosed_edges);

    return hanging_nodes;
}


int
bg_trimesh_aabb(point_t *min, point_t *max, int *faces, int num_faces, point_t *p, int num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!min || !max)
	return -1;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* If inputs are insufficient, we can't produce a bbox */
    if (!faces || num_faces <= 0 || !p || num_pnts <= 0)
	return -1;

    /* First Pass: coherently iterate through all faces of the BoT and
     * mark vertices in a bit-vector that are referenced by a face. */
    struct bu_bitv *visit_vert = bu_bitv_new(num_pnts);
    for (size_t tri_index = 0; tri_index < (size_t)num_faces; tri_index++) {
	BU_BITSET(visit_vert, faces[tri_index*3 + X]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Y]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Z]);
    }

    /* Second Pass: check max and min of vertices marked */
    for(size_t vert_index = 0; vert_index < (size_t)num_pnts; vert_index++){
	if(BU_BITTEST(visit_vert,vert_index)){
	    VMINMAX((*min), (*max), p[vert_index]);
	}
    }

    /* Done with bitv */
    bu_bitv_free(visit_vert);

    /* Make sure the RPP created is not of zero volume */
    if (NEAR_EQUAL((*min)[X], (*max)[X], SMALL_FASTF)) {
	(*min)[X] -= SMALL_FASTF;
	(*max)[X] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Y], (*max)[Y], SMALL_FASTF)) {
	(*min)[Y] -= SMALL_FASTF;
	(*max)[Y] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Z], (*max)[Z], SMALL_FASTF)) {
	(*min)[Z] -= SMALL_FASTF;
	(*max)[Z] += SMALL_FASTF;
    }

    /* Success */
    return 0;
}


int
bg_trimesh_2d_gc(int **ofaces, point2d_t **opnts, int *n_opnts,
	const int *faces, int num_faces, const point2d_t *in_pts)
{
    std::set<int> active_pnts;
    std::map<int,int> o2n;

    if (!ofaces || !opnts || !n_opnts || !faces || num_faces <= 0 || !in_pts) {
	return -1;
    }

    // Build unique active point set
    for (int i = 0; i < num_faces; i++) {
	active_pnts.insert(faces[i*3+0]);
	active_pnts.insert(faces[i*3+1]);
	active_pnts.insert(faces[i*3+2]);
    }

    point2d_t *op = (point2d_t *)bu_calloc(active_pnts.size(), sizeof(point2d_t), "new points array");
    int *ofcs = (int *)bu_calloc(num_faces*3, sizeof(int), "new faces array");

    // Assign unique points to new array, building map from old array indices to new
    int nind = 0;
    std::set<int>::iterator ap;
    for (ap = active_pnts.begin(); ap != active_pnts.end(); ap++) {
	V2MOVE(op[nind], in_pts[*ap]);
	o2n[*ap] = nind;
	nind++;
    }

    // Use map to create new face index array
    for (int i = 0; i < num_faces; i++) {
	ofcs[i*3+0] = o2n[faces[i*3+0]];
	ofcs[i*3+1] = o2n[faces[i*3+1]];
	ofcs[i*3+2] = o2n[faces[i*3+2]];
    }

    // Assign results
    (*ofaces) = ofcs;
    (*opnts) = op;
    (*n_opnts) = (int)active_pnts.size();

    return num_faces;
}

int
bg_trimesh_3d_gc(int **ofaces, point_t **opnts, int *n_opnts,
	const int *faces, int num_faces, const point_t *in_pts)
{
    std::set<int> active_pnts;
    std::map<int,int> o2n;

    if (!ofaces || !opnts || !n_opnts || !faces || num_faces <= 0 || !in_pts) {
	return -1;
    }

    // Build unique active point set
    for (int i = 0; i < num_faces; i++) {
	active_pnts.insert(faces[i*3+0]);
	active_pnts.insert(faces[i*3+1]);
	active_pnts.insert(faces[i*3+2]);
    }

    point_t *op = (point_t *)bu_calloc(active_pnts.size(), sizeof(point_t), "new points array");
    int *ofcs = (int *)bu_calloc(num_faces*3, sizeof(int), "new faces array");

    // Assign unique points to new array, building map from old array indices to new
    int nind = 0;
    std::set<int>::iterator ap;
    for (ap = active_pnts.begin(); ap != active_pnts.end(); ap++) {
	V2MOVE(op[nind], in_pts[*ap]);
	o2n[*ap] = nind;
	nind++;
    }

    // Use map to create new face index array
    for (int i = 0; i < num_faces; i++) {
	ofcs[i*3+0] = o2n[faces[i*3+0]];
	ofcs[i*3+1] = o2n[faces[i*3+1]];
	ofcs[i*3+2] = o2n[faces[i*3+2]];
    }

    // Assign results
    (*ofaces) = ofcs;
    (*opnts) = op;
    (*n_opnts) = (int)active_pnts.size();

    return num_faces;
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
