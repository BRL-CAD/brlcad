/*              O V E R L A P _ A C C E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/**
 * @file overlap_accel.cpp
 *
 * RTree-accelerated region bounding-box overlap pre-filter.
 *
 * When an analysis is concerned only with finding geometric overlaps, it is
 * wasteful to shoot rays over the entire model bounding box.  This module:
 *
 *  1. Collects the conservatively-computed axis-aligned bounding box (AABB)
 *     for every region in the raytracing instance using rt_bound_tree().
 *  2. Inserts all region AABBs into an R-Tree.
 *  3. Queries the R-Tree to obtain the set of region pairs whose AABBs
 *     intersect.
 *  4. For each such pair, computes the intersection AABB and returns it.
 *
 * The returned pairs can be used by the caller to restrict ray sampling to
 * the intersection volumes of candidate-overlap pairs, dramatically reducing
 * the number of rays needed when only a small portion of the model overlaps.
 *
 * Reference: A. Guttman, "R-Trees: A Dynamic Index Structure for Spatial
 *            Searching", SIGMOD 1984.
 */

#include "common.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "raytrace.h"
#include "vmath.h"
#include "../libbg/RTree.h"

#include "analyze.h"


/*
 * Callback used during RTree traversal: collects region index pairs.
 */
struct overlap_pair_callback_data {
    std::set<std::pair<int, int>> *pairs;
    int query_idx; /* region whose bbox we are querying against */
};

static bool
collect_overlap_pair(int idx, void *ctx)
{
    struct overlap_pair_callback_data *d = (struct overlap_pair_callback_data *)ctx;
    if (idx == d->query_idx)
	return true; /* skip self-intersection */

    /* store pair with lower index first for deduplication */
    int a = (d->query_idx < idx) ? d->query_idx : idx;
    int b = (d->query_idx < idx) ? idx : d->query_idx;
    d->pairs->insert(std::make_pair(a, b));
    return true;
}


int
analyze_overlapping_region_pairs(struct rt_i *rtip, struct bu_ptbl *result_pairs)
{
    if (!rtip || !result_pairs)
	return -1;

    RT_CK_RTI(rtip);

    /* Count regions */
    int nregions = 0;
    {
	struct region *regp;
	for (BU_LIST_FOR(regp, region, &rtip->HeadRegion))
	    nregions++;
    }

    if (nregions == 0)
	return 0;

    /* Build per-region bounding boxes */
    point_t *rmins = (point_t *)bu_calloc(nregions, sizeof(point_t), "region mins");
    point_t *rmaxs = (point_t *)bu_calloc(nregions, sizeof(point_t), "region maxs");
    struct region **regptrs = (struct region **)bu_calloc(nregions, sizeof(struct region *), "region ptrs");

    int i = 0;
    {
	struct region *regp;
	for (BU_LIST_FOR(regp, region, &rtip->HeadRegion)) {
	    regptrs[i] = regp;

	    /* rt_bound_tree() computes a tight axis-aligned bounding box
	     * for the CSG tree rooted at regp->reg_treetop.
	     * Returns 0 on success, -1 on failure. */
	    if (rt_bound_tree(regp->reg_treetop, rmins[i], rmaxs[i]) < 0) {
		/* Fall back to the model bounding box if tree-bound fails */
		VMOVE(rmins[i], rtip->mdl_min);
		VMOVE(rmaxs[i], rtip->mdl_max);
	    }
	    i++;
	}
    }

    /* Insert all region AABBs into an RTree */
    RTree<int, double, 3> tree;
    for (i = 0; i < nregions; i++) {
	double lo[3] = { rmins[i][0], rmins[i][1], rmins[i][2] };
	double hi[3] = { rmaxs[i][0], rmaxs[i][1], rmaxs[i][2] };
	tree.Insert(lo, hi, i);
    }

    /* Query each region's AABB against the whole tree to find pairs */
    std::set<std::pair<int, int>> candidate_pairs;
    for (i = 0; i < nregions; i++) {
	double lo[3] = { rmins[i][0], rmins[i][1], rmins[i][2] };
	double hi[3] = { rmaxs[i][0], rmaxs[i][1], rmaxs[i][2] };

	struct overlap_pair_callback_data ctx;
	ctx.pairs = &candidate_pairs;
	ctx.query_idx = i;

	tree.Search(lo, hi, collect_overlap_pair, &ctx);
    }

    /* Convert to bu_ptbl of analyze_region_overlap_pair */
    bu_ptbl_init(result_pairs, candidate_pairs.size() + 1, "overlap pairs");
    for (const auto &pr : candidate_pairs) {
	int a = pr.first;
	int b = pr.second;

	struct analyze_region_overlap_pair *op =
	    (struct analyze_region_overlap_pair *)bu_malloc(
		sizeof(struct analyze_region_overlap_pair), "overlap pair");

	op->r1 = regptrs[a];
	op->r2 = regptrs[b];

	/* Intersection AABB */
	VMOVE(op->isect_min, rmins[a]);
	VMOVE(op->isect_max, rmaxs[a]);
	/* clamp to r2's bbox */
	for (int k = 0; k < 3; k++) {
	    if (rmins[b][k] > op->isect_min[k]) op->isect_min[k] = rmins[b][k];
	    if (rmaxs[b][k] < op->isect_max[k]) op->isect_max[k] = rmaxs[b][k];
	}

	bu_ptbl_ins(result_pairs, (long *)op);
    }

    bu_free(rmins,    "region mins");
    bu_free(rmaxs,    "region maxs");
    bu_free(regptrs,  "region ptrs");

    return (int)candidate_pairs.size();
}


void
analyze_free_region_overlap_pairs(struct bu_ptbl *pairs)
{
    if (!pairs)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(pairs); i++) {
	struct analyze_region_overlap_pair *op =
	    (struct analyze_region_overlap_pair *)BU_PTBL_GET(pairs, i);
	bu_free(op, "overlap pair");
    }
    bu_ptbl_free(pairs);
}


/**
 * analyze_cluster_overlapping_pairs - Group AABB pairs into connected components.
 *
 * Algorithm (identical in structure to gdiff's cluster_content()):
 *  1. Collect the unique set of region pointers from all pairs.
 *  2. Build an adjacency list: for each pair (r1, r2) add an undirected edge.
 *  3. Iterative DFS to find connected components (clusters).
 *  4. For each cluster, accumulate the union of all intra-cluster pairwise
 *     intersection AABBs — this is the minimum volume that must be covered
 *     by the focused ray-sampling pass for the cluster.
 */
int
analyze_cluster_overlapping_pairs(struct bu_ptbl *pairs, struct bu_ptbl *clusters)
{
    if (!pairs || !clusters)
	return -1;

    bu_ptbl_init(clusters, 4, "overlap clusters");

    size_t npairs = BU_PTBL_LEN(pairs);
    if (npairs == 0)
	return 0;

    /* Collect unique region pointers from all pairs */
    std::vector<struct region *> regions;
    regions.reserve(npairs * 2);
    for (size_t k = 0; k < npairs; k++) {
	struct analyze_region_overlap_pair *op =
	    (struct analyze_region_overlap_pair *)BU_PTBL_GET(pairs, k);
	regions.push_back(op->r1);
	regions.push_back(op->r2);
    }
    std::sort(regions.begin(), regions.end());
    regions.erase(std::unique(regions.begin(), regions.end()), regions.end());
    size_t nregions = regions.size();

    /* Helper: index of a region pointer in the sorted unique vector */
    auto idx_of = [&](struct region *r) -> size_t {
	return (size_t)(std::lower_bound(regions.begin(), regions.end(), r)
			- regions.begin());
    };

    /* Build undirected adjacency list (same as gdiff cluster_content) */
    std::vector<std::vector<size_t>> adj(nregions);
    for (size_t k = 0; k < npairs; k++) {
	struct analyze_region_overlap_pair *op =
	    (struct analyze_region_overlap_pair *)BU_PTBL_GET(pairs, k);
	size_t a = idx_of(op->r1);
	size_t b = idx_of(op->r2);
	adj[a].push_back(b);
	adj[b].push_back(a);
    }

    /* Iterative DFS to find connected components */
    std::vector<bool> visited(nregions, false);
    std::vector<std::vector<size_t>> components;

    for (size_t i = 0; i < nregions; i++) {
	if (visited[i]) continue;

	std::vector<size_t> component;
	std::vector<size_t> stack{i};
	visited[i] = true;

	while (!stack.empty()) {
	    size_t cur = stack.back();
	    stack.pop_back();
	    component.push_back(cur);
	    for (size_t nb : adj[cur]) {
		if (!visited[nb]) {
		    visited[nb] = true;
		    stack.push_back(nb);
		}
	    }
	}
	components.push_back(std::move(component));
    }

    /* Build struct overlap_cluster for each connected component */
    for (const auto &comp : components) {
	std::set<size_t> comp_set(comp.begin(), comp.end());

	struct overlap_cluster *cl =
	    (struct overlap_cluster *)bu_malloc(sizeof(struct overlap_cluster),
					       "overlap cluster");
	bu_ptbl_init(&cl->regions, (int)comp.size() + 1, "cluster regions");
	VSETALL(cl->isect_union_min,  INFINITY);
	VSETALL(cl->isect_union_max, -INFINITY);

	/* Add region pointers for every member of this component */
	for (size_t ridx : comp)
	    bu_ptbl_ins(&cl->regions, (long *)regions[ridx]);

	/* Union all pairwise intersection AABBs whose both endpoints are
	 * inside this cluster — that gives the minimum volume the focused
	 * ray grid must cover to detect all candidate overlaps. */
	for (size_t k = 0; k < npairs; k++) {
	    struct analyze_region_overlap_pair *op =
		(struct analyze_region_overlap_pair *)BU_PTBL_GET(pairs, k);
	    size_t a = idx_of(op->r1);
	    size_t b = idx_of(op->r2);
	    if (comp_set.count(a) && comp_set.count(b)) {
		VMIN(cl->isect_union_min, op->isect_min);
		VMAX(cl->isect_union_max, op->isect_max);
	    }
	}

	bu_ptbl_ins(clusters, (long *)cl);
    }

    return (int)components.size();
}


void
analyze_free_overlap_clusters(struct bu_ptbl *clusters)
{
    if (!clusters)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(clusters); i++) {
	struct overlap_cluster *cl =
	    (struct overlap_cluster *)BU_PTBL_GET(clusters, i);
	bu_ptbl_free(&cl->regions);
	bu_free(cl, "overlap cluster");
    }
    bu_ptbl_free(clusters);
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
