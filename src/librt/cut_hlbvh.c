
/********************************************************************************
    pbrt source code is Copyright(c) 1998-2015
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Derived from src/accelerators/bvh.cpp
    within the pbrt-v3 project, https://github.com/mmp/pbrt-v3/

    Direct browse link:
    https://github.com/mmp/pbrt-v3/blob/master/src/accelerators/bvh.cpp

    Implements the HLBVH construction algorithm as in:
      "Simpler and Faster HLBVH with Work Queues" by
      Kirill Garanzha, Jacopo Pantaleoni, David McAllister.
      (Proc. High Performance Graphics 2011, pg 59)
 ********************************************************************************/

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/parallel.h"
#include "bu/sort.h"
#include "vmath.h"
#include "raytrace.h"
#include "bg/plane.h"
#include "bview/plot3.h"

struct bvh_build_node {
    fastf_t bounds[6];
    struct bvh_build_node *children[2];
    long first_prim_offset, n_primitives;
    uint8_t split_axis;
};

struct morton_primitive {
    long primitive_index;
    uint32_t morton_code;
};

struct lbvh_treelet {
    long start_index, n_primitives;
    struct bvh_build_node *build_nodes;
};


static void
bvh_bounds_union(fastf_t a[6], const fastf_t b[6], const fastf_t c[6])
{
    VMOVE(&a[0], &b[0]);
    VMIN(&a[0], &c[0]);
    VMOVE(&a[3], &b[3]);
    VMAX(&a[3], &c[3]);
}

static void
init_leaf(struct bvh_build_node *node, long first, long n, const fastf_t b[6])
{
    node->first_prim_offset = first;
    node->n_primitives = n;
    VMOVE(&node->bounds[0], &b[0]);
    VMOVE(&node->bounds[3], &b[3]);
    node->children[0] = node->children[1] = NULL;
}

static void
init_interior(struct bvh_build_node *node, uint8_t axis, struct bvh_build_node *c0, struct bvh_build_node *c1)
{
    node->children[0] = c0;
    node->children[1] = c1;
    bvh_bounds_union(node->bounds, c0->bounds, c1->bounds);
    node->split_axis = axis;
    node->n_primitives = 0;
}


/* utility functions */
static inline uint32_t left_shift3(uint32_t x)
{
    BU_ASSERT(x <= (1 << 10));
    if (x == (1 << 10)) --x;
    x = (x | (x << 16)) & 0x30000ff;
    /* x = ---- --98 ---- ---- ---- ---- 7654 3210 */
    x = (x | (x << 8)) & 0x300f00f;
    /* x = ---- --98 ---- ---- 7654 ---- ---- 3210 */
    x = (x | (x << 4)) & 0x30c30c3;
    /* x = ---- --98 ---- 76-- --54 ---- 32-- --10 */
    x = (x | (x << 2)) & 0x9249249;
    /* x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0 */
    return x;
}

static inline uint32_t encode_morton3(const point_t v)
{
    BU_ASSERT(v[X] >= 0 && v[X] <= (1 << 10));
    BU_ASSERT(v[Y] >= 0 && v[Y] <= (1 << 10));
    BU_ASSERT(v[Z] >= 0 && v[Z] <= (1 << 10));
    return (left_shift3(v[Z]) << 2) | (left_shift3(v[Y]) << 1) | left_shift3(v[X]);
}


static void radix_sort(size_t v_len, struct morton_primitive **v)
{
    struct morton_primitive *temp_vector;
#define bits_per_pass 6
    const uint32_t n_bits = 30;
    const uint32_t n_passes = n_bits / bits_per_pass;
    uint32_t pass;
    BU_ASSERT((n_bits % bits_per_pass) == 0);

    temp_vector = (struct morton_primitive *)bu_calloc(v_len, sizeof(struct morton_primitive), "radix_sort");
    for (pass = 0; pass < n_passes; ++pass) {
        /* Perform one pass of radix sort, sorting bits_per_pass bits */
        uint32_t low_bit = pass * bits_per_pass;
#define n_buckets (1 << bits_per_pass)
        size_t bucket_count[n_buckets];
        /* Compute starting index in output array for each bucket */
        size_t out_index[n_buckets];
        size_t i, j;

        /* Set in and out vector pointers for radix sort pass */
        struct morton_primitive *in = (pass & 1) ? temp_vector : *v;
        struct morton_primitive *out = (pass & 1) ? *v : temp_vector;

        /* Count number of zero bits in array for current radix sort bit */
        const uint32_t bit_mask = (1 << bits_per_pass) - 1;

	memset(bucket_count, 0, sizeof(bucket_count));

        for (j=0; j<v_len; j++) {
            const struct morton_primitive *mp = &in[j];
            uint32_t bucket = (mp->morton_code >> low_bit) & bit_mask;
	    BU_ASSERT(bucket < n_buckets);
            bucket_count[bucket]++;
        }

        out_index[0] = 0;
        for (i=1; i<n_buckets; ++i)
            out_index[i] = out_index[i - 1] + bucket_count[i - 1];

        /* Store sorted values in output array */
        for (j=0; j<v_len; j++) {
            const struct morton_primitive *mp = &in[j];
            uint32_t bucket = (mp->morton_code >> low_bit) & bit_mask;
            out[out_index[bucket]++] = *mp;
        }
    }
    /* Copy final result from temp_vector, if needed */
    if ((n_passes & 1)) {
        struct morton_primitive *t;
        t = temp_vector;
        temp_vector = *v;
        *v = t;
    }
    bu_free(temp_vector, "radix_sort");
#undef bits_per_pass
#undef n_buckets
}


static struct bvh_build_node *
emit_lbvh(long max_prims_in_node,
    struct bvh_build_node **build_nodes, const fastf_t *bounds_prims,
    struct morton_primitive *morton_prims, long n_primitives, long *total_nodes,
    long *ordered_prims, long *ordered_prims_offset, int bit_index) {

    BU_ASSERT(n_primitives > 0);
    if (bit_index < 0 || n_primitives < max_prims_in_node) {
        struct bvh_build_node *node;
        fastf_t bounds[6] = {MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};
        long first_prim_offset;
        long i;

        /* Create and return leaf node of LBVH treelet */
        ++*total_nodes;
        node = (*build_nodes)++;

	{
	    first_prim_offset = *ordered_prims_offset;
	    (*ordered_prims_offset) += n_primitives; /* atomic_add */
	}

        for (i = 0; i < n_primitives; ++i) {
            fastf_t bounds2[6];
            long primitive_index = morton_prims[i].primitive_index;
            ordered_prims[first_prim_offset + i] = primitive_index;

            VMOVE(&bounds2[0], &bounds_prims[primitive_index*6+0]);
            VMOVE(&bounds2[3], &bounds_prims[primitive_index*6+3]);
            bvh_bounds_union(bounds, bounds, bounds2);
        }
        init_leaf(node, first_prim_offset, n_primitives, bounds);
        return node;
    } else {
        const uint32_t mask = (1 << bit_index);
        long search_start = 0, search_end = n_primitives - 1;
        long split_offset;
        struct bvh_build_node *node;
        struct bvh_build_node *lbvh[2];
        uint8_t axis = (bit_index % 3);

        /* Advance to next subtree level if there's no LBVH split for this bit */
        if ((morton_prims[0].morton_code & mask) ==
            (morton_prims[n_primitives - 1].morton_code & mask))
            return emit_lbvh(max_prims_in_node, build_nodes, bounds_prims,
			     morton_prims, n_primitives, total_nodes, ordered_prims,
			     ordered_prims_offset, bit_index - 1);

        /* Find LBVH split point for this dimension */
        while (search_start + 1 != search_end) {
	    long mid;

	    BU_ASSERT(search_start != search_end);
            mid = (search_start + search_end) / 2;
            if ((morton_prims[search_start].morton_code & mask) ==
                (morton_prims[mid].morton_code & mask))
                search_start = mid;
            else {
                BU_ASSERT((morton_prims[mid].morton_code & mask) ==
                       (morton_prims[search_end].morton_code & mask));
                search_end = mid;
            }
        }
        split_offset = search_end;
	BU_ASSERT(split_offset <= n_primitives - 1);
	BU_ASSERT((morton_prims[split_offset - 1].morton_code & mask) !=
		(morton_prims[split_offset].morton_code & mask));

        /* Create and return interior LBVH node */
        ++*total_nodes;
        node = (*build_nodes)++;
        lbvh[0] = emit_lbvh(max_prims_in_node, build_nodes, bounds_prims, morton_prims,
			    split_offset, total_nodes, ordered_prims,
			    ordered_prims_offset, bit_index - 1);
        lbvh[1] = emit_lbvh(max_prims_in_node, build_nodes, bounds_prims,
			    &morton_prims[split_offset], n_primitives - split_offset,
			    total_nodes, ordered_prims, ordered_prims_offset,
			    bit_index - 1);
        init_interior(node, axis, lbvh[0], lbvh[1]);
        return node;
    }
}


static inline fastf_t
surface_area(const fastf_t b[6])
{
    vect_t d;
    VSUB2(d, &b[3], &b[0]);
    return (2.0 * (d[X] * d[Y] + d[X] * d[Z] + d[Y] * d[Z]));
}

static inline uint8_t
maximum_extent(const fastf_t b[6])
{
    vect_t d;
    VSUB2(d, &b[3], &b[0]);
    if (d[X] > d[Y] && d[X] > d[Z])
        return 0;
    else if (d[Y] > d[Z])
        return 1;
    else
        return 2;
}

static inline int
pred(const struct bvh_build_node *node, fastf_t *centroid_bounds,
     uint8_t dim, long min_cost_split_bucket)
{
#define n_buckets 12
    fastf_t centroid = (node->bounds[0+dim] + node->bounds[3+dim]) * 0.5;
    long b = n_buckets * ((centroid - centroid_bounds[0+dim]) /
	(centroid_bounds[3+dim] - centroid_bounds[0+dim]));

    if (b == n_buckets) b = n_buckets - 1;
    BU_ASSERT(b >= 0 && b < n_buckets);
    return (b <= min_cost_split_bucket);
#undef n_buckets
}

static struct bvh_build_node *
build_upper_sah(struct bu_pool *pool, struct bvh_build_node **treelet_roots,
	        long start, long end, long *total_nodes)
{
    long n_nodes;

    BU_ASSERT(start < end);
    n_nodes = end - start;
    if (n_nodes == 1) {
	return treelet_roots[start];
    } else {
	struct bvh_build_node *node;
	fastf_t bounds[6] =
		{MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};
	long i;
	uint8_t dim;

	/* Allocate bucket_info for SAH partition buckets */
    #define n_buckets 12
	struct bucket_info {
	    long count;
	    fastf_t bounds[6];
	};
	struct bucket_info buckets[n_buckets];
	fastf_t centroid_bounds[6] =
		{MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};

	fastf_t cost[n_buckets - 1];
	fastf_t min_cost;
	long min_cost_split_bucket;

	struct bvh_build_node **pmid;
	struct bvh_build_node *lbvh[2];
	long mid;

	node = (struct bvh_build_node*)bu_pool_alloc(pool, 1, sizeof(*node));

	for (i = 0; i<n_buckets; i++) {
	    buckets[i].count = 0;
	}

	/* Compute bounds of all nodes under this HLBVH node */
	for (i = start; i < end; ++i)
	    bvh_bounds_union(bounds, bounds, treelet_roots[i]->bounds);

	/* Compute bound of HLBVH node centroids, choose split dimension _dim_ */
	for (i = start; i < end; ++i) {
	    point_t centroid;
	    VADD2SCALE(centroid, &treelet_roots[i]->bounds[0],
		       &treelet_roots[i]->bounds[3], 0.5);
	    VMIN(&centroid_bounds[0], centroid);
	    VMAX(&centroid_bounds[3], centroid);
	}
	dim = maximum_extent(centroid_bounds);
	/* FIXME: if this hits, what do we need to do?
	 * Make sure the SAH split below does something... ?
	 */
	if (!ZERO(centroid_bounds[3+dim] - centroid_bounds[0+dim])) {
            /* Initialize bucket_info for HLBVH SAH partition buckets */
            for (i = start; i < end; ++i) {
                fastf_t centroid = (treelet_roots[i]->bounds[0+dim] +
                                    treelet_roots[i]->bounds[3+dim]) * 0.5;
                long b = n_buckets * ((centroid - centroid_bounds[0+dim]) /
                                      (centroid_bounds[3+dim] - centroid_bounds[0+dim]));
                if (b == n_buckets) b = n_buckets - 1;
                BU_ASSERT(b >= 0 && b < n_buckets);
                buckets[b].count++;
                bvh_bounds_union(buckets[b].bounds, buckets[b].bounds,
                                 treelet_roots[i]->bounds);
            }

            /* Compute costs for splitting after each bucket */
            for (i = 0; i < n_buckets - 1; ++i) {
                fastf_t b0[6] =
                    {MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};
                fastf_t b1[6] =
                    {MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};
                long count0 = 0, count1 = 0;
                long j;

                for (j = 0; j <= i; ++j) {
                    bvh_bounds_union(b0, b0, buckets[j].bounds);
                    count0 += buckets[j].count;
                }
                for (j = i + 1; j < n_buckets; ++j) {
                    bvh_bounds_union(b1, b1, buckets[j].bounds);
                    count1 += buckets[j].count;
                }
                cost[i] = .125 + (count0 * surface_area(b0) + count1 * surface_area(b1)) /
                    surface_area(bounds);
            }

            /* Find bucket to split at that minimizes SAH metric */
            min_cost = cost[0];
            min_cost_split_bucket = 0;
            for (i = 1; i < n_buckets - 1; ++i) {
                if (cost[i] < min_cost) {
                    min_cost = cost[i];
                    min_cost_split_bucket = i;
                }
            }

            /* Split nodes and create interior HLBVH SAH node */
            {
                struct bvh_build_node **first, **last, *t;
                first = &treelet_roots[start];
                last = &treelet_roots[end - 1] + 1;

                for (;;) {
                    for (;;)
                        if (first == last)
                            goto out;
                        else if (pred(*first, centroid_bounds, dim, min_cost_split_bucket))
                            ++first;
                        else
                            break;
                    --last;
                    for (;;)
                        if (first == last)
                            goto out;
                        else if (!pred(*last, centroid_bounds, dim, min_cost_split_bucket))
                            --last;
                        else
                            break;
                    t = *first;
                    *first = *last;
                    *last = t;

                    ++first;
                }
    out:
                pmid = first;
            }

            mid = pmid - treelet_roots;
        } else {
            mid = start+(end-start)/2;
        }

        BU_ASSERT(mid > start && mid < end);

	++*total_nodes;
	lbvh[0] = build_upper_sah(pool, treelet_roots, start, mid, total_nodes);
	lbvh[1] = build_upper_sah(pool, treelet_roots, mid, end, total_nodes);
	init_interior(node, dim, lbvh[0], lbvh[1]);
	return node;
    }
#undef n_buckets
}


struct bvh_build_node *
hlbvh_create(long max_prims_in_node, struct bu_pool *pool, const fastf_t *centroids_prims,
	     const fastf_t *bounds_prims, long *total_nodes,
	     const long n_primitives, long **ordered_prims)
{
    fastf_t bounds[6] = {MAX_FASTF,MAX_FASTF,MAX_FASTF, -MAX_FASTF,-MAX_FASTF,-MAX_FASTF};
    long i;
    struct morton_primitive *morton_prims;

    struct lbvh_treelet *treelets_to_build;
    struct lbvh_treelet *treelets_to_build_end;

    struct bvh_build_node *ret;

    struct bvh_build_node **finished_treelets;
    long start, end;
    long ordered_prims_offset = 0;
    long atomic_total = 0;
    long treelets_size;

    /* Compute bounding box of all primitive centroids */
    for (i = 0; i<n_primitives; i++) {
	VMIN(&bounds[0], &centroids_prims[i*3]);
	VMAX(&bounds[3], &centroids_prims[i*3]);
    }

    morton_prims = (struct morton_primitive*)bu_calloc(n_primitives,
						       sizeof(struct morton_primitive),
						       "hlbvh_create");
    /* Compute Morton indices of primitives */
    for (i = 0; i<n_primitives; i++) {
        /* Initialize morton_prims[i] for ith primitive */
        const uint32_t morton_bits = 10;
        const uint32_t morton_scale = 1 << morton_bits;
        point_t o;

        morton_prims[i].primitive_index = i;

	VSUB2(o, &centroids_prims[i*3], &bounds[0]);
        if (bounds[3+X] > bounds[0+X]) o[X] /= (bounds[3+X] - bounds[0+X]);
        if (bounds[3+Y] > bounds[0+Y]) o[Y] /= (bounds[3+Y] - bounds[0+Y]);
        if (bounds[3+Z] > bounds[0+Z]) o[Z] /= (bounds[3+Z] - bounds[0+Z]);

        o[X] *= morton_scale;
        o[Y] *= morton_scale;
        o[Z] *= morton_scale;
        morton_prims[i].morton_code = encode_morton3(o);
    }

    /* Radix sort primitive Morton indices */
    radix_sort(n_primitives, &morton_prims);

    /* Create LBVH treelets at bottom of BVH */

    /* Find intervals of primitives for each treelet */
    treelets_to_build = (struct lbvh_treelet*)bu_calloc(n_primitives,
							sizeof(struct lbvh_treelet),
							"hlbvh_create");
    treelets_to_build_end = treelets_to_build;
    for (start = 0, end = 1; end <= n_primitives; ++end) {
        uint32_t mask = 0x3ffc0000;
        if (end == n_primitives ||
            ((morton_prims[start].morton_code & mask) !=
             (morton_prims[end].morton_code & mask))) {
            /* Add entry to treelets_to_build for this treelet */
            long n_prims = end - start;
            long max_bvh_nodes = 2 * n_prims;
            struct bvh_build_node *nodes;
            nodes = (struct bvh_build_node*)bu_pool_alloc(pool, max_bvh_nodes,
							  sizeof(struct bvh_build_node));
            treelets_to_build_end->start_index = start;
            treelets_to_build_end->n_primitives = n_prims;
            treelets_to_build_end->build_nodes = nodes;
            treelets_to_build_end++;
            start = end;
        }
    }

    /* Create LBVHs for treelets in parallel */
    *ordered_prims = (long*)bu_calloc(n_primitives, sizeof(long), "hlbvh_create");
    treelets_size = treelets_to_build_end-treelets_to_build;
    for (i=0; i<treelets_size; i++) {
	struct lbvh_treelet *treelet;
        /* Generate i_th LBVH treelet */
	long nodes_created = 0;
        const int first_bit_index = 29 - 12;

        treelet = &treelets_to_build[i];
        treelet->build_nodes = emit_lbvh(max_prims_in_node, &treelet->build_nodes,
					 bounds_prims,
					 &morton_prims[treelet->start_index],
					 treelet->n_primitives, &nodes_created,
					 *ordered_prims, &ordered_prims_offset,
					 first_bit_index);
	atomic_total += nodes_created;
    }
    bu_free(morton_prims, "hlbvh_create");
    *total_nodes = atomic_total;

    /* Create and return SAH BVH from LBVH treelets */
    finished_treelets =
	(struct bvh_build_node**)bu_calloc(treelets_size, sizeof(struct bvh_build_node*),
 					   "hlbvh_create");
    for (i=0; i<treelets_size; i++) {
	struct lbvh_treelet *treelet;
	treelet = &treelets_to_build[i];
        finished_treelets[i] = treelet->build_nodes;
    }
    bu_free(treelets_to_build, "hlbvh_create");
    ret = build_upper_sah(pool, finished_treelets, 0, treelets_size, total_nodes);
    bu_free(finished_treelets, "hlbvh_create");
    return ret;
}


#ifdef USE_OPENCL
static cl_int
flatten_bvh_tree(cl_int *offset, struct clt_linear_bvh_node *nodes, long total_nodes,
		 const struct bvh_build_node *node, long depth)
{
    cl_int my_offset = *offset;
    struct clt_linear_bvh_node *linear_node;

    BU_ASSERT(my_offset < total_nodes);
    ++*offset;
    linear_node = &nodes[my_offset];

    VMOVE(linear_node->bounds.p_min, &node->bounds[0]);
    VMOVE(linear_node->bounds.p_max, &node->bounds[3]);
    if (node->n_primitives > 0) {
	BU_ASSERT(!node->children[0] && !node->children[1]);
	BU_ASSERT(node->n_primitives < 65536);
        linear_node->u.primitives_offset = node->first_prim_offset;
        linear_node->n_primitives = node->n_primitives;
    } else {
        /* Create interior flattened BVH node */
        linear_node->axis = node->split_axis;
        linear_node->n_primitives = 0;
	flatten_bvh_tree(offset, nodes, total_nodes, node->children[0], depth + 1);
        linear_node->u.second_child_offset =
	    flatten_bvh_tree(offset, nodes, total_nodes, node->children[1], depth + 1);
    }
    return my_offset;
}

void
clt_linear_bvh_create(long n_primitives, struct clt_linear_bvh_node **nodes_p,
		      long **ordered_prims, const fastf_t *centroids_prims,
		      const fastf_t *bounds_prims, cl_int *total_nodes)
{
    struct clt_linear_bvh_node *nodes;
    cl_int lnodes_created = 0;

    nodes = NULL;
    if (n_primitives != 0) {
        /* Build BVH tree for primitives */
	struct bu_pool *pool;
	long nodes_created = 0;
        struct bvh_build_node *root;

        /*
         * This pool must have enough size to fit the whole tree or the
         * algorithm will fail. It stores pointers to itself and a
         * realloc would make the pointers invalid.
         *
         * total_nodes = treelets_size + upper_sah_size,  where:
         *  treelets_size < 2*n_primitives
         *  upper_sah_size < 2*2^popcnt(0x3ffc0000)   i.e. 2*4096
         */
	pool = bu_pool_create(sizeof(struct bvh_build_node)*(2*n_primitives+2*4096));
        root = hlbvh_create(4, pool, centroids_prims, bounds_prims, &nodes_created,
			    n_primitives, ordered_prims);

        /* Compute representation of depth-first traversal of BVH tree */
        nodes = (struct clt_linear_bvh_node*)bu_calloc(nodes_created, sizeof(*nodes),
						       "bvh create");
        flatten_bvh_tree(&lnodes_created, nodes, nodes_created, root, 0);
	bu_pool_delete(pool);

	if (RT_G_DEBUG&RT_DEBUG_CUT) {
	    bu_log("HLBVH: %ld nodes, %ld primitives (%.2f KB)\n",
		   nodes_created, n_primitives,
		   (double)(sizeof(*nodes) * nodes_created) / (1024.0));
	}

	if (RT_G_DEBUG&RT_DEBUG_CUT) {
	    int i;
	    long j;
	    for (i=0; i<lnodes_created; i++) {
		if (nodes[i].n_primitives != 0) {
		    bu_log("#%d: %d\n", i, nodes[i].n_primitives);
		    for (j=0; j<nodes[i].n_primitives; j++) {
			bu_log("  %ld\n", (*ordered_prims)[nodes[i].u.primitives_offset+j]);
		    }
		} else {
		    bu_log("#%d> #%d\n", i, nodes[i].u.second_child_offset);

		}
	    }

	    for (i=0; i<n_primitives; i++) {
		bu_log(":%ld\n", (*ordered_prims)[i]);
	    }
	}
    }
    *nodes_p = nodes;
    *total_nodes = lnodes_created;
}
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
