/*                           B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2025 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/bot/bot.c
 *
 * Intersect a ray with a bag o' triangles.
 *
 */

#include "common.h"

#include <string.h> // needed for memset, memcpy, and strlen
#include <ctype.h> // needed for isdigit() and isspace() in rt_bot_adjust

#include "bg/trimesh.h" // needed for the call in rt_bot_bbox
#include "bg/tri_ray.h"
#include "vmath.h"

#include "rt/primitives/bot.h"
#include "bu/snooze.h"

/* private implementation headers */
#include "./bot_edge.h"
#include "../../librt_private.h"
#include "../../cut_hlbvh.h" /* for hlbvh functions */

#define BOT_MIN_DN 1.0e-9
#define HLBVH_STACK_SIZE 256
#define RT_DEFAULT_MAX_PRIMS_IN_NODE 8

#define BOT_UNORIENTED_NORM(_ap, _hitp, _norm, _out) {		    \
	if (!(_ap)->a_bot_reverse_normal_disabled) {		    \
	    if (_out) {	/* this is an exit */			    \
		if ((_hitp)->hit_vpriv[X] < 0.0) {		    \
		    VREVERSE((_hitp)->hit_normal, (_norm));	    \
		} else {					    \
		    VMOVE((_hitp)->hit_normal, (_norm));	    \
		}						    \
	    } else {	/* this is an entrance */		    \
		if ((_hitp)->hit_vpriv[X] > 0.0) {		    \
		    VREVERSE((_hitp)->hit_normal, (_norm));	    \
		} else {					    \
		    VMOVE((_hitp)->hit_normal, (_norm));	    \
		}						    \
	    }							    \
	} else {						    \
	    VMOVE((_hitp)->hit_normal, (_norm));		    \
	}							    \
    }

#ifdef USE_OPENCL
int
clt_bot_prep(struct soltab *stp, struct rt_bot_internal *bip, struct rt_i *rtip)
{
    struct bot_specific *bot;
    size_t idx;

    RT_BOT_CK_MAGIC(bip);
    (void)rtip;

    bot = (struct bot_specific *)stp->st_specific;

    bot->clt_header.ntri = bip->num_faces;
    bot->clt_header.orientation = bip->orientation;
    bot->clt_header.flags = bip->bot_flags;
    memset(bot->clt_header.offsets, 0, sizeof(bot->clt_header.offsets));
    bot->clt_header.offsets[0] = sizeof(bot->clt_header);

    if (bip->num_faces != 0) {
	bot->clt_header.offsets[2] = sizeof(struct clt_tri_specific)*bip->num_faces;
	bot->clt_triangles = (struct clt_tri_specific*)bu_calloc(1, bot->clt_header.offsets[2],
		"bot triangles");

	for (idx=0; idx<bip->num_faces; idx++) {
	    size_t i0, i1, i2;

	    i0 = i1 = i2 = bip->num_vertices;

	    switch (bip->orientation) {
	    case RT_BOT_CW:
		i0 = bip->faces[idx*3];
		i2 = bip->faces[idx*3 + 1];
		i1 = bip->faces[idx*3 + 2];
		break;
	    case RT_BOT_CCW:
	    case RT_BOT_UNORIENTED:
		i0 = bip->faces[idx*3];
		i1 = bip->faces[idx*3 + 1];
		i2 = bip->faces[idx*3 + 2];
		break;
	    }

	    if (i0 >= bip->num_vertices
		|| i1 >= bip->num_vertices
		|| i2 >= bip->num_vertices)
	    {
		bu_log("face number %zu of bot(%s) references a non-existent vertex\n",
			idx, stp->st_name);
		bu_free(bot->clt_triangles, "bot triangles");
		bot->clt_triangles = NULL;
		return -1;
	    } else {
		VMOVE(bot->clt_triangles[idx].v0, &bip->vertices[i0*3]);
		VMOVE(bot->clt_triangles[idx].v1, &bip->vertices[i1*3]);
		VMOVE(bot->clt_triangles[idx].v2, &bip->vertices[i2*3]);
		bot->clt_triangles[idx].surfno = idx;
	    }
	}
    } else {
	bot->clt_triangles = NULL;
    }

    if (bip->num_face_normals != 0) {
	BU_ASSERT(bip->num_face_normals == bip->num_faces);

	bot->clt_header.offsets[3] = sizeof(cl_double)*9*bip->num_face_normals;
	bot->clt_normals = (cl_double*)bu_calloc(1, bot->clt_header.offsets[3], "bot normals");

	for (idx=0; idx<bip->num_face_normals; idx++) {
	    size_t i0, i1, i2;

	    i0 = i1 = i2 = bip->num_normals;

	    switch (bip->orientation) {
	    case RT_BOT_CW:
		i0 = bip->face_normals[idx*3];
		i2 = bip->face_normals[idx*3 + 1];
		i1 = bip->face_normals[idx*3 + 2];
		break;
	    case RT_BOT_CCW:
	    case RT_BOT_UNORIENTED:
		i0 = bip->face_normals[idx*3];
		i1 = bip->face_normals[idx*3 + 1];
		i2 = bip->face_normals[idx*3 + 2];
		break;
	    }

	    if (i0 >= bip->num_normals
		|| i1 >= bip->num_normals
		|| i2 >= bip->num_normals)
	    {
		bu_log("face normal number %zu of bot(%s) references a non-existent vertex\n",
		       idx, stp->st_name);
		bu_free(bot->clt_normals, "bot normals");
		bot->clt_normals = NULL;
		return -1;
	    } else {
		VMOVE(&bot->clt_normals[idx*9+0], &bip->normals[i0*3]);
		VMOVE(&bot->clt_normals[idx*9+3], &bip->normals[i1*3]);
		VMOVE(&bot->clt_normals[idx*9+6], &bip->normals[i2*3]);
	    }
	}
    } else {
	bot->clt_normals = NULL;
    }
    return 0;
}

void
clt_bot_free(struct bot_specific *bot)
{
    if (bot->clt_triangles) {
	bu_free(bot->clt_triangles, "bot triangles");
	bot->clt_triangles = NULL;
    }
    if (bot->clt_normals) {
	bu_free(bot->clt_normals, "bot normals");
	bot->clt_normals = NULL;
    }
}

size_t
clt_bot_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
    unsigned ntri;
    size_t i;
    struct clt_linear_bvh_node *nodes;

    struct clt_bot_specific header;
    uint8_t *data;

    ntri = bot->clt_header.ntri;

    nodes = NULL;
    if (bot->clt_triangles) {
	struct clt_tri_specific *ordered_triangles;
	cl_double *ordered_normals;
	long *ordered_prims;
	cl_int total_nodes = 0;
	fastf_t *centroids;
	fastf_t *bounds;

	ordered_prims = NULL;

	/* Build BVH for bot. */
	centroids = (fastf_t*)bu_calloc(ntri, sizeof(fastf_t)*3, "bot centroids");
	for (i=0; i<ntri; i++) {
	    const struct clt_tri_specific *trip = &bot->clt_triangles[i];

	    VADD3(&centroids[i*3], trip->v0, trip->v1, trip->v2);
	    VSCALE(&centroids[i*3], &centroids[i*3], 1.0/3.0);
	}
	bounds = (fastf_t*)bu_calloc(ntri, sizeof(fastf_t)*6, "bot bounds");
	for (i=0; i<ntri; i++) {
	    const struct clt_tri_specific *trip = &bot->clt_triangles[i];

	    VMOVE(&bounds[i*6+0], trip->v0);
	    VMOVE(&bounds[i*6+3], trip->v0);
	    VMINMAX(&bounds[i*6+0], &bounds[i*6+3], trip->v1);
	    VMINMAX(&bounds[i*6+0], &bounds[i*6+3], trip->v2);

	    /* Prevent the RPP from being 0 thickness */
	    if (NEAR_EQUAL(bounds[i*6+X], bounds[i*6+3+X], SMALL_FASTF)) {
		bounds[i*6+X] -= SMALL_FASTF;
		bounds[i*6+3+X] += SMALL_FASTF;
	    }
	    if (NEAR_EQUAL(bounds[i*6+Y], bounds[i*6+3+Y], SMALL_FASTF)) {
		bounds[i*6+Y] -= SMALL_FASTF;
		bounds[i*6+3+Y] += SMALL_FASTF;
	    }
	    if (NEAR_EQUAL(bounds[i*6+Z], bounds[i*6+3+Z], SMALL_FASTF)) {
		bounds[i*6+Z] -= SMALL_FASTF;
		bounds[i*6+3+Z] += SMALL_FASTF;
	    }
	}
	nodes = NULL;
	clt_linear_bvh_create(ntri, &nodes, &ordered_prims, centroids, bounds,
			      &total_nodes);
	bu_free(centroids, "bot centroids");
	bu_free(bounds, "bot bounds");
	bot->clt_header.offsets[1] = sizeof(struct clt_linear_bvh_node)*total_nodes;

	/* reorder the triangles spatially */
	if (bot->clt_triangles) {
	    ordered_triangles = (struct clt_tri_specific*)bu_calloc(1, bot->clt_header.offsets[2],
		    "bot ordered triangles");
	    for (i=0; i<ntri; i++) {
		ordered_triangles[i] = bot->clt_triangles[ordered_prims[i]];
	    }
	    bu_free(bot->clt_triangles, "bot triangles");
	    bot->clt_triangles = ordered_triangles;
	}
	/* reorder the normals spatially */
	if (bot->clt_normals) {
	    ordered_normals = (cl_double*)bu_calloc(1, bot->clt_header.offsets[2],
		    "bot ordered normals");
	    for (i=0; i<ntri; i++) {
		memcpy(&ordered_normals[i*9], &bot->clt_normals[ordered_prims[i]*9], sizeof(cl_double)*9);
	    }
	    bu_free(bot->clt_normals, "bot normals");
	    bot->clt_normals = ordered_normals;
	}
	bu_free(ordered_prims, "bot ordered prims");
    }

    header = bot->clt_header;

    header.offsets[0] = 0;
    for (i=1; i <= 4; i++) {
	header.offsets[i] = header.offsets[i-1] + bot->clt_header.offsets[i-1];
    }
    data = (uint8_t*)bu_pool_alloc(pool, 1, header.offsets[4]);

    memcpy(data+header.offsets[0], &header, bot->clt_header.offsets[0]);
    if (nodes)
	memcpy(data+header.offsets[1], nodes, bot->clt_header.offsets[1]);
    if (bot->clt_triangles)
	memcpy(data+header.offsets[2], bot->clt_triangles, bot->clt_header.offsets[2]);
    if (bot->clt_normals)
	memcpy(data+header.offsets[3], bot->clt_normals, bot->clt_header.offsets[3]);

    if (nodes) bu_free(nodes, "bot nodes");

    return header.offsets[4];
}
#endif


/**
 * Calculate an RPP for a BoT
 */
int
rt_bot_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {

    RT_CK_DB_INTERNAL(ip);
    struct rt_bot_internal *bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    return bg_trimesh_aabb(min, max, bot_ip->faces, bot_ip->num_faces, (const point_t *)bot_ip->vertices, bot_ip->num_vertices);
}

typedef struct _hit_da {
    size_t count;
    size_t capacity;
    struct hit *items;
} hit_da;

const struct hit zeroed_hit_s = RT_HIT_INIT_ZERO;

#define DA_INIT_CAPACITY 128

// We include itemtype in DA_APPEND because
// the options are:
// 1. Get a -Wc++-compat warning
// 2. Push a diagnostic that ignores that warning
// 3. Do some sort of decltype shenanigans
// 4. Pass in the type of the dynamic array

// Append one item to a dynamic array
// This supports the use case of appending a compile
// time value to a dynamic array
// Ex. DA_APPEND(da_ints, 42, int)
#define DA_APPEND(da, item, itemtype)							\
    do {										\
	if ((da)->count >= (da)->capacity) {						\
	    if ((da)->capacity == 0) {							\
		(da)->items = (itemtype*)bu_calloc(DA_INIT_CAPACITY, sizeof(struct hit),\
						   "DA calloc __FILE__: __LINE__" );	\
		(da)->capacity = DA_INIT_CAPACITY;					\
	    } else {                                                                    \
		(da)->items = (itemtype*)bu_realloc((da)->items, 			\
						    (da)->capacity*2*sizeof(struct hit),\
						    "DA realloc __FILE__: __LINE__" );	\
		(da)->capacity *= 2;							\
	    }                                                                           \
	    BU_ASSERT((da)->items != NULL);						\
	}										\
											\
	(da)->items[(da)->count++] = (item); /* struct copy */				\
    } while (0)

struct spatial_partition_s {
    struct bvh_flat_node *root;
    triangle_s *tris;
    fastf_t *vertex_normals; /* for deallocation, access normals
				through triangle_s */
};

/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid BOT, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 BOT is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct bot_specific is created, and its address is stored in
 * stp->st_specific for use by bot_shot().
 */
int
rt_bot_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    RT_CK_DB_INTERNAL(ip);
    struct rt_bot_internal *bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (!bot_ip->num_faces || !bot_ip->num_vertices)
	return -1;

    struct bn_tol defaults = BN_TOL_INIT_TOL;
    struct bn_tol *tolp;
    if (rtip) {
	tolp = &rtip->rti_tol;
    } else {
	rt_tol_default(&defaults);
	tolp = &defaults;
    }

    // Copy settings over to bot, because we won't have access to
    // bot_ip in the shot function
    struct bot_specific *bot;
    BU_GET(bot, struct bot_specific);
    stp->st_specific = (void *)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;
    bot->bot_ntri = bot_ip->num_faces;

    // set up thickness if requested
    if (bot_ip->thickness) {
	bot->bot_thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "bot_thickness");
	for (size_t bot_ip_index = 0; bot_ip_index < bot_ip->num_faces; bot_ip_index++)
	    bot->bot_thickness[bot_ip_index] = bot_ip->thickness[bot_ip_index];
    } else {
	bot->bot_thickness = NULL;
    }

    // set up face_mode and facelist
    if (bot_ip->face_mode) {
	bot->bot_facemode = bu_bitv_dup(bot_ip->face_mode);
    } else {
	bot->bot_facemode = BU_BITV_NULL;
    }
    bot->bot_facelist = NULL;

    // look for a requested bundle size
    size_t bot_max_prims_in_node = RT_DEFAULT_MAX_PRIMS_IN_NODE ;
    const char *bmintie = getenv("LIBRT_BOT_MAX_PRIMS_IN_NODE");
    if (bmintie)
	bot_max_prims_in_node = atoi(bmintie);

    // set up centroids and bounds for hlbvh call
    fastf_t *centroids = (fastf_t*)bu_malloc(bot_ip->num_faces * sizeof(fastf_t)*3, "bot centroids");
    fastf_t *bounds    = (fastf_t*)bu_malloc(bot_ip->num_faces * sizeof(fastf_t)*6, "bot bounds");

    for (size_t i = 0; i < bot_ip->num_faces; i++) {
	fastf_t* v0 = (bot_ip->vertices+3*bot_ip->faces[i*3+0]);
	fastf_t* v1 = (bot_ip->vertices+3*bot_ip->faces[i*3+1]);
	fastf_t* v2 = (bot_ip->vertices+3*bot_ip->faces[i*3+2]);

	VADD3(&centroids[i*3], v0, v1, v2);
	VSCALE(&centroids[i*3], &centroids[i*3], 1.0/3.0);

	VMOVE(&bounds[i*6+0], v0);
	VMOVE(&bounds[i*6+3], v0);
	VMINMAX(&bounds[i*6+0], &bounds[i*6+3], v1);
	VMINMAX(&bounds[i*6+0], &bounds[i*6+3], v2);
    }
    struct bu_pool *pool = hlbvh_init_pool(bot_ip->num_faces);
    // implicit return values
    long nodes_created = 0;
    long *ordered_faces = NULL;
    struct bvh_build_node *build_root = hlbvh_create(bot_max_prims_in_node, pool, centroids, bounds, &nodes_created,
					       bot_ip->num_faces, &ordered_faces);

    bu_free(centroids, "bot centroids");
    bu_free(bounds, "bot bounds");

    struct bvh_flat_node *flat_root = hlbvh_flatten(build_root, nodes_created);
    bu_pool_delete(pool);

    int do_normals = (bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS)
		  && (bot_ip->bot_flags & RT_BOT_USE_NORMALS)
		  && (bot_ip->num_normals > 0);

    triangle_s *tris = (triangle_s *)bu_malloc(bot_ip->num_faces * sizeof(triangle_s), "ordered triangles");
    fastf_t *tri_norms = NULL;
    if (do_normals) {
	tri_norms = (fastf_t*) bu_malloc(bot_ip->num_faces * 9 * sizeof(fastf_t), "bot norms");
    }
    // copy triangles into order specified by ordered_faces
    for (size_t i = 0; i < bot_ip->num_faces; i++) {
	size_t bot_ip_index = ordered_faces[i];
	fastf_t *v0, *v1, *v2;
	v0 = (bot_ip->vertices+3*bot_ip->faces[bot_ip_index*3+0]);
	if (bot_ip->orientation == RT_BOT_CW) {
	    v2 = (bot_ip->vertices+3*bot_ip->faces[bot_ip_index*3+1]);
	    v1 = (bot_ip->vertices+3*bot_ip->faces[bot_ip_index*3+2]);
	} else {
	    v1 = (bot_ip->vertices+3*bot_ip->faces[bot_ip_index*3+1]);
	    v2 = (bot_ip->vertices+3*bot_ip->faces[bot_ip_index*3+2]);
	}
	VMOVE(tris[i].A, v0);
	VSUB2(tris[i].AB, v1, v0);
	VSUB2(tris[i].AC, v2, v0);
	vect_t wn, work;
	VCROSS(wn, tris[i].AB, tris[i].AC);
	tris[i].face_norm_scalar = MAGNITUDE(wn);

	// some error-checking
	fastf_t m1 = MAGSQ(tris[i].AB);
	fastf_t m2 = MAGSQ(tris[i].AC);
	VSUB2(work, v1, v2);
	fastf_t m3 = MAGSQ(work);
	fastf_t m4 = MAGSQ(wn);
	if (m1 < tolp->dist_sq ||
	    m2 < tolp->dist_sq ||
	    m3 < tolp->dist_sq ||
	    m4 < tolp->dist_sq)
	{
	    if (RT_G_DEBUG & RT_DEBUG_SHOOT) {
		bu_log("%s: degenerate facet #%zu\n", stp->st_dp?stp->st_name:"_unnamed_", bot_ip_index);
		bu_log("\t(%g %g %g) (%g %g %g) (%g %g %g)\n", V3ARGS(v0), V3ARGS(v1), V3ARGS(v2));
	    }
	}
	VUNITIZE(wn);
	VMOVE(tris[i].face_norm, wn);
	tris[i].norms = NULL;

	if (do_normals && bot_ip->num_face_normals > bot_ip_index) {
	    long idx[3];
	    VMOVE(idx, &bot_ip->face_normals[bot_ip_index*3]);
	    if (idx[0] >= 0 && idx[0] < (long)bot_ip->num_normals &&
		idx[1] >= 0 && idx[1] < (long)bot_ip->num_normals &&
		idx[2] >= 0 && idx[2] < (long)bot_ip->num_normals)
	    {
		tris[i].norms = &tri_norms[i*9];
		VMOVE(&tris[i].norms[0*3], &bot_ip->normals[idx[0]*3]);
		VMOVE(&tris[i].norms[1*3], &bot_ip->normals[idx[1]*3]);
		VMOVE(&tris[i].norms[2*3], &bot_ip->normals[idx[2]*3]);
	    } else if (RT_G_DEBUG & RT_DEBUG_SHOOT) {
		bu_log("%s: facet #%zu tried to have normals, but gave incorrect indexes\n", stp->st_dp?stp->st_name:"_unnamed_", bot_ip_index);
		bu_log("\t%zu %zu %zu a max number of %zu\n", V3ARGS(idx), bot_ip->num_normals);
	    }
	}
	tris[i].face_id = bot_ip_index;
    }

    bu_free(ordered_faces, "ordered faces");

    struct spatial_partition_s *sps;
    BU_GET(sps, struct spatial_partition_s);
    sps->root = flat_root;
    sps->tris = tris;
    sps->vertex_normals = tri_norms;

    bot->tie = (void *)sps;

    // struct bvh_build_node and struct bvh_flat_node are puns for fastf_t[6] which are the bounds
    fastf_t *min = (fastf_t *)sps->root;
    fastf_t *max = &min[3];

    VMOVE(stp->st_min, min);
    VMOVE(stp->st_max, max);

    /* zero thickness will get missed by the raytracer */
    BBOX_NONDEGEN(stp->st_min, stp->st_max, tolp->dist);

    VADD2SCALE(stp->st_center, min, max, 0.5);
    point_t dist_vec;
    VSUB2SCALE(dist_vec, max, min, 0.5);
    stp->st_aradius = FMAX(dist_vec[0], FMAX(dist_vec[1], dist_vec[2]));
    stp->st_bradius = MAGNITUDE(dist_vec);

#ifdef USE_OPENCL
    clt_bot_prep(stp, bot_ip, rtip);
#endif
    return 0;
}


void
rt_bot_print(const struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}

/* Forward declare for rt_bot_shot */
int
rt_bot_makesegs(hit_da *hits,
		struct soltab *stp,
		struct xray *rp,
		struct application *ap,
		struct seg *seghead,
		struct rt_piecestate *psp);


void
bot_shot_hlbvh_flat(struct bvh_flat_node *root, struct xray* rp, triangle_s *tris, size_t ntris, hit_da* hits, fastf_t toldist)
{
    struct bvh_flat_node *stack_node[HLBVH_STACK_SIZE];
    unsigned char stack_child_index[HLBVH_STACK_SIZE];
    int stack_ind = 0;
    stack_node[stack_ind] = root;
    stack_child_index[stack_ind] = 0;
    vect_t inverse_r_dir;
    VINVDIR(inverse_r_dir, rp->r_dir);

    while (stack_ind >= 0) {
	if (UNLIKELY(stack_ind >= HLBVH_STACK_SIZE)) {
	    // This should only ever happen if the BVH tree that was
	    // built had a depth greater than the defined stack size.
	    // Even if a BVH is built degenerately and has an average
	    // splitting factor of 1.2 (we expect this to be close to
	    // 2), then this stack should support >10^20 triangles
	    // There is a recursive function in cut_hlbvh that we use
	    // to flatten the tree, it has a depth parameter, and it
	    // does a similar traversal to the one here. (It would
	    // probably be good for debugging) - Apr 2024
	    bu_bomb("Stack size exceeded in bot shot");
	}
	if (stack_child_index[stack_ind] >= 2) {
	    stack_ind--;
	    continue;
	}
	struct bvh_flat_node* node = stack_node[stack_ind];
	// check bounds if it's the first time in this node
	if (!stack_child_index[stack_ind]) {
	    // TODO: do we want to handle NaNs correctly?
	    point_t lows_t, highs_t, low_ts, high_ts;

	    VSUB2( lows_t, &node->bounds[0], rp->r_pt);
	    VSUB2(highs_t, &node->bounds[3], rp->r_pt);

	    VELMUL( lows_t,  lows_t, inverse_r_dir);
	    VELMUL(highs_t, highs_t, inverse_r_dir);

	    VMOVE( low_ts, lows_t);
	    VMOVE(high_ts, lows_t);
	    VMINMAX(low_ts, high_ts, highs_t);

	    fastf_t high_t = FMIN(high_ts[0], FMIN(high_ts[1], high_ts[2]));
	    fastf_t  low_t = FMAX( low_ts[0], FMAX( low_ts[1],  low_ts[2]));
	    if ((high_t < -1.0) | (low_t > high_t)) {
		stack_ind--;
		continue;
	    }
	}
	if (node->n_primitives > 0) {
	    size_t end = node->data.first_prim_offset + node->n_primitives;
	    BU_ASSERT(end <= ntris);
	    // each leaf node has multiple primitives in it
	    for (size_t i = node->data.first_prim_offset; i < end; i++) {
		triangle_s* tri = &tris[i];
		vect_t wn, wxb, xp;
		fastf_t dn_plus_tol;

		// Calculate non-unitized face normal
		VSCALE(wn, tri->face_norm, tri->face_norm_scalar);

		// Ray direction dot wn (outward-pointing normal)
		fastf_t dn = VDOT(wn, rp->r_dir);

		// If ray lies directly along the face (dot product is zero),
		// drop the face
		fastf_t abs_dn = dn >= 0.0 ? dn : (-dn);
		if (abs_dn < BOT_MIN_DN)
		    continue;

		// Scale tolerance based on ray / triangle angle to reduce false negatives
		// if ray is perpendicular:	 abs_dn == 1.0, reduce tolerance (1 / (1 + 1)) = .5
		// if ray is parallel (grazing): abs_dn == 0, use full tolerance (1 / (1 + 0)) = 1
		fastf_t tol_multiplier = (1.0 / (1.0 + abs_dn));
		dn_plus_tol = abs_dn + (toldist * tol_multiplier);

		// Check for exceeding along the sides
		VSUB2(wxb, tri->A, rp->r_pt);
		VCROSS(xp, wxb, rp->r_dir);
		fastf_t beta = VDOT(tri->AB, xp);
		fastf_t gamma = VDOT(tri->AC, xp);
		beta = (dn > 0.0) ?  -beta :  beta;
		gamma = (dn < 0.0) ? -gamma : gamma;
		if ( (beta + gamma > dn_plus_tol) || (beta < -toldist) || (gamma < -toldist) )
		    continue;

		fastf_t dist = VDOT(wxb, wn) / dn;

		// Fill out hitdata
		struct hit cur_hit = {0};
		cur_hit.hit_magic = RT_HIT_MAGIC;
		cur_hit.hit_dist = dist;
		cur_hit.hit_vpriv[X] = VDOT(tri->face_norm, rp->r_dir);
		cur_hit.hit_vpriv[Y] = gamma / abs_dn;
		cur_hit.hit_vpriv[Z] =  beta / abs_dn;
		cur_hit.hit_private = tri;
		cur_hit.hit_surfno = tri->face_id;
		cur_hit.hit_rayp = rp;
		DA_APPEND(hits, cur_hit, struct hit);
	    }
	    stack_ind--;
	    continue;
	}
	// we hit the bounds and are not in a leaf
	// so we do the next child of this node

	// stack_child_index[stack_ind] either == 0 or == 1
	// because of the guard at the top of the loop
	stack_node[stack_ind+1] = (stack_child_index[stack_ind]) ? (node->data.other_child) : (node +1);
	stack_child_index[stack_ind] += 1;
	stack_child_index[stack_ind+1] = 0;
	stack_ind++;
    }
}


_Thread_local hit_da hits_per_cpu = {0};


/**
 * Intersect a ray with a bot.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Notes for rt_bot_norm(): hit_private contains pointer to the
 * triangle_s structure.
 * hit_vpriv[X] contains dot product of ray direction and unit normal from tri_specific.
 * hit_vpriv[Y] contains the barycentric coordinate associated with point B of the triangle.
 * hit_vpriv[Z] contains the barycentric coordinate associated with point C of the triangle.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_bot_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    if (UNLIKELY(!stp || !ap || !seghead))
	return 0;

    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
    if (UNLIKELY(!bot))
	return 0;

    struct spatial_partition_s *sps = (struct spatial_partition_s *)bot->tie;
    if (UNLIKELY(!sps))
	return 0;

    hits_per_cpu.count = 0; // New ray, new result count

    fastf_t toldist = 0.0;
    if (bot->bot_orientation != RT_BOT_UNORIENTED && bot->bot_mode == RT_BOT_SOLID) {
	// approximate ULP-derived tolerance, scaled by the object size
	// NOTE: the x10 is largely arbitrary, but serves to adjust the tolerance to a
	// practical value
	toldist = (DBL_EPSILON * stp->st_aradius * 10);
    }

    bot_shot_hlbvh_flat(sps->root, rp, sps->tris, bot->bot_ntri, &hits_per_cpu, toldist);

    if (hits_per_cpu.count == 0) {
	return 0;
    }
    // sort the hits
    // insertion sort
    {
	size_t nhits = hits_per_cpu.count;
	struct hit *hits = hits_per_cpu.items;
	for (size_t i = 1; i < nhits; i++) {
	    fastf_t i_dist = hits[i].hit_dist;
	    struct hit swap = hits[i];
	    int j;
	    for (j = i-1; j >= 0; j--) {
		fastf_t j_dist = hits[j].hit_dist;
		if (j_dist < i_dist) {
		    break;
		}
		hits[j+1] = hits[j];
	    }
	    hits[j+1] = swap;
	}
    }

    return rt_bot_makesegs(&hits_per_cpu, stp, rp, ap, seghead, NULL);
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_bot_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct bot_specific *bot=(struct bot_specific *)stp->st_specific;

    vect_t old_norm;
    triangle_s *trip=(triangle_s *)hitp->hit_private;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    VMOVE(old_norm, hitp->hit_normal);

    if ((bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && (bot->bot_flags & RT_BOT_USE_NORMALS) && trip->norms) {
	fastf_t old_ray_dot_norm, new_ray_dot_norm;
	fastf_t u, v, w; /*barycentric coords of hit point */
	size_t i;

	old_ray_dot_norm = VDOT(hitp->hit_normal, rp->r_dir);

	v = hitp->hit_vpriv[Y];
	if (v < 0.0) v = 0.0;
	if (v > 1.0) v = 1.0;

	w = hitp->hit_vpriv[Z];
	if (w < 0.0) w = 0.0;
	if (w > 1.0) w =  1.0;

	u = 1.0 - v - w;
	if (u < 0.0) u = 0.0;
	VSETALL(hitp->hit_normal, 0.0);

	for (i = X; i <= Z; i++) {
	    hitp->hit_normal[i] = u*trip->norms[i] + v*trip->norms[i+3] + w*trip->norms[i+6];
	}
	VUNITIZE(hitp->hit_normal);

	if (bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS) {
	    if (VDOT(old_norm, hitp->hit_normal) < 0.0) {
		VREVERSE(hitp->hit_normal, hitp->hit_normal);
	    }
	}

	new_ray_dot_norm = VDOT(hitp->hit_normal, rp->r_dir);

	if ((old_ray_dot_norm < 0.0 && new_ray_dot_norm > 0.0) ||
	    (old_ray_dot_norm > 0.0 && new_ray_dot_norm < 0.0)) {
	    /* surface normal interpolation has produced an
	     * incompatible normal direction clamp the normal to 90
	     * degrees to the ray direction
	     */

	    vect_t tmp;

	    VCROSS(tmp, rp->r_dir, hitp->hit_normal);
	    VCROSS(hitp->hit_normal, tmp, rp->r_dir);
	}

	VUNITIZE(hitp->hit_normal);
    }
}


/**
 * Return the curvature of the bot.
 */
void
rt_bot_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
    cvp->crv_c1 = cvp->crv_c2 = 0;
}


/**
 * For a hit on the surface of an bot, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_bot_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct bot_specific *bot;
    if (ap) RT_CK_APPLICATION(ap);

    if (!stp)
	return;
    RT_CK_SOLTAB(stp);

    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;

    bot = (struct bot_specific *)stp->st_specific;

    size_t i;

    if (!bot || !hitp || !uvp)
	return;
    triangle_s *trip = (triangle_s *)hitp->hit_private;
    if (!trip)
	return;

    if ((bot->bot_flags & RT_BOT_HAS_TEXTURE_UVS) /* && trip->tri_uvs */) {
	for (i = X; i <= Z; i++) {
	    /* TODO: do stuff */
	}
    }
}


void
rt_bot_free(struct soltab *stp)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;

    if (bot && bot->tie) {
	struct spatial_partition_s *sps = (struct spatial_partition_s*)bot->tie;
	bu_free(sps->root, "bot bvh flat nodes");
	bu_free(sps->tris, "bot triangles");
	bu_free(sps->vertex_normals, "bot normals");
	BU_PUT(sps, struct spatial_partition_s);
	bot->tie = NULL;
    }

    if (hits_per_cpu.capacity) {
	bu_free(hits_per_cpu.items, "DA free");
	hits_per_cpu.capacity = 0;
	hits_per_cpu.items = NULL;
    }

    if (bot) {
	BU_PUT(bot, struct bot_specific);
	stp->st_specific = NULL;
    }

#ifdef USE_OPENCL
    clt_bot_free(bot);
#endif
}


static int
rt_bot_plate_segs(struct hit *hits,
		  size_t nhits,
		  struct soltab *stp,
		  struct xray *rp,
		  struct application *ap,
		  struct seg *seghead,
		  struct bot_specific *bot)
{
    register struct seg *segp;
    register size_t i;
    register fastf_t los;
    size_t surfno;
    static const int IN_SEG = 0;
    static const int OUT_SEG = 1;

    if (rp) RT_CK_RAY(rp);

    for (i = 0; i < nhits; i++) {
	triangle_s *trip = (triangle_s *)hits[i].hit_private;

	surfno = hits[i].hit_surfno;

	los = 0.0;
	if (LIKELY(bot->bot_thickness != NULL)) {
	    if (bot->bot_mode == RT_BOT_PLATE_NOCOS) {
		los = bot->bot_thickness[surfno];
	    } else {
		los = bot->bot_thickness[surfno] / hits[i].hit_vpriv[X];
		if (los < 0.0)
		    los = -los;
	    }
	}

	if (LIKELY(bot->bot_facemode != NULL) && BU_BITTEST(bot->bot_facemode, hits[i].hit_surfno)) {

	    /* append thickness to hit point */
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    /* set in hit */
	    segp->seg_in = hits[i];
	    BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);

	    /* set out hit */
	    segp->seg_out.hit_surfno = surfno;
	    segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
	    VMOVE(segp->seg_out.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);
	    segp->seg_out.hit_private = segp->seg_in.hit_private;
	    segp->seg_out.hit_rayp = &ap->a_ray;

	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	} else {
	    /* center thickness about hit point */
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    /* set in hit */
	    segp->seg_in.hit_surfno = surfno;
	    VMOVE(segp->seg_in.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);
	    segp->seg_in.hit_private = hits[i].hit_private;
	    segp->seg_in.hit_dist = hits[i].hit_dist - (los*0.5);
	    segp->seg_in.hit_rayp = &ap->a_ray;

	    /* set out hit */
	    segp->seg_out.hit_surfno = surfno;
	    segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
	    VMOVE(segp->seg_out.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);
	    segp->seg_out.hit_private = hits[i].hit_private;
	    segp->seg_out.hit_rayp = &ap->a_ray;

	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
    }
    /* Every hit turns into two, and makes a seg.  No leftovers */
    return nhits*2;
}


int
rt_bot_surface_segs(struct hit *hits, size_t nhits, struct soltab *stp, struct application *ap, struct seg *seghead)
{
    register struct seg *segp;
    register ssize_t i;
    static const int IN_SEG = 0;
    static const int OUT_SEG = 1;
    /* TODO: review the use of a signed tmp var. Var i was changed to be signed in
     * r44239 as a bug in another project was segfaulting. */
    ssize_t snhits = (ssize_t)nhits;

    for (i = 0; i < snhits; i++) {
	triangle_s *trip=(triangle_s *)hits[i].hit_private;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* set in hit */
	segp->seg_in = hits[i];
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);

	/* set out hit */
	segp->seg_out = hits[i];
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    /* Every hit turns into two, and makes a seg.  No leftovers */
    return snhits*2;
}


static int
rt_bot_unoriented_segs(struct hit *hits,
		       size_t nhits,
		       struct soltab *stp,
		       struct xray *rp,
		       struct application *ap,
		       struct seg *seghead)
{
    register struct seg *segp;
    register size_t i, j;

    /*
     * RT_BOT_SOLID, RT_BOT_UNORIENTED.
     */
    fastf_t rm_dist = 0.0;
    int removed = 0;
    static const int IN_SEG = 0;
    static const int OUT_SEG = 1;

    if (nhits == 1) {
	triangle_s *trip = (triangle_s *)hits[0].hit_private;

	/* make a zero length partition */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* set in hit */
	segp->seg_in = hits[0];
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);

	/* set out hit */
	segp->seg_out = hits[0];
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	return 1;
    }

    /* Remove duplicate hits */
    for (i = 0; i < nhits - 1; i++) {
	fastf_t dist;

	dist = hits[i].hit_dist - hits[i+1].hit_dist;
	if (NEAR_ZERO(dist, ap->a_rt_i->rti_tol.dist)) {
	    removed++;
	    rm_dist = hits[i+1].hit_dist;
	    for (j = i; j < nhits - 1; j++)
		hits[j] = hits[j+1];
	    nhits--;
	    i--;
	}
    }

    if (nhits == 1)
	return 0;

    if (nhits&1 && removed) {
	/* If we have an odd number of hits and have removed a
	 * duplicate, then it was likely on an edge, so remove the one
	 * we left.
	 */
	for (i = 0; i < nhits; i++) {
	    if (ZERO(hits[i].hit_dist - rm_dist)) {
		for (j = i; j < nhits - 1; j++)
		    hits[j] = hits[j+1];
		nhits--;
		i--;
		break;
	    }
	}
    }

    for (i = 0; i < (nhits&~1); i += 2) {
	triangle_s *trip = (triangle_s *)hits[i].hit_private;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* set in hit */
	segp->seg_in = hits[i];
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);

	/* set out hit */
	segp->seg_out = hits[i+1];
	trip = (triangle_s *)hits[i+1].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    if (nhits&1) {
	if (RT_G_DEBUG & RT_DEBUG_SHOOT) {
	    bu_log("rt_bot_unoriented_segs(%s): WARNING: odd number of hits (%zu), last hit ignored\n",
		   stp->st_name, nhits);
	    bu_log("\tray = -p %g %g %g -d %g %g %g\n",
		   V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	}
	nhits--;
    }
    return nhits;
}


int
rt_bot_oriented_segs(hit_da *hits_da, struct soltab *stp, struct application *ap, struct seg *seghead, struct rt_piecestate *psp)
{
    register struct seg *segp;
    register ssize_t i;
    static const int IN_SEG = 0;
    static const int OUT_SEG = 1;
    /* TODO: review the use of a signed tmp var. Var i was changed to be signed in
     * r44239 as a bug in another project was segfaulting. */
    ssize_t snhits = (ssize_t)hits_da->count;
    struct hit *hits = hits_da->items;

    /* Remove duplicate hits */
    {
	register ssize_t j, k, l;

	for (i = 0; i < snhits-1; i++) {
	    fastf_t dist;
	    fastf_t dn;

	    dn = hits[i].hit_vpriv[X];

	    k = i + 1;
	    dist = hits[i].hit_dist - hits[k].hit_dist;

	    /* count number of hits at this distance */
	    while (NEAR_ZERO(dist, ap->a_rt_i->rti_tol.dist)) {
		k++;
		if (k > snhits - 1)
		    break;
		dist = hits[i].hit_dist - hits[k].hit_dist;
	    }

	    if ((k - i) == 2 && dn * hits[i+1].hit_vpriv[X] > 0) {
		/* a pair of hits at the same distance and both are exits or entrances,
		 * likely an edge hit, remove one */
		for (j = i; j < snhits - 1; j++)
		    hits[j] = hits[j+1];
		if (psp) {
		    psp->htab.end--;
		}
		snhits--;
		i--;
		continue;
	    } else if ((k - i) > 2) {
		ssize_t keep1 = -1, keep2 = -1;
		ssize_t enters = 0, exits = 0;
		int reorder = 0;
		int reorder_failed = 0;

		/* more than two hits at the same distance, likely a vertex hit
		 * try to keep just two, one entrance and one exit.
		 * unless they are all entrances or all exits, then just keep one */

		/* first check if we need to do anything */
		for (j = 0; j < k; j++) {
		    if (hits[j].hit_vpriv[X] > 0)
			exits++;
		    else
			enters++;
		}

		if (k%2) {
		    if (exits == (enters - 1)) {
			reorder = 1;
		    }
		} else {
		    if (exits == enters) {
			reorder = 1;
		    }
		}

		if (reorder) {
		    struct hit tmp_hit;
		    int changed = 0;

		    for (j = i; j < k; j++) {

			if (j%2) {
			    if (hits[j].hit_vpriv[X] > 0) {
				continue;
			    }
			    /* should be an exit here */
			    l = j+1;
			    while (l < k) {
				if (hits[l].hit_vpriv[X] > 0) {
				    /* swap with this exit */
				    tmp_hit = hits[j];
				    hits[j] = hits[l];
				    hits[l] = tmp_hit;
				    changed = 1;
				    break;
				}
				l++;
			    }
			    if (hits[j].hit_vpriv[X] < 0) {
				reorder_failed = 1;
				break;
			    }
			} else {
			    if (hits[j].hit_vpriv[X] < 0) {
				continue;
			    }
			    /* should be an entrance here */
			    l = j+1;
			    while (l < k) {
				if (hits[l].hit_vpriv[X] < 0) {
				    /* swap with this entrance */
				    tmp_hit = hits[j];
				    hits[j] = hits[l];
				    hits[l] = tmp_hit;
				    changed = 1;
				    break;
				}
				l++;
			    }
			    if (hits[j].hit_vpriv[X] > 0) {
				reorder_failed = 1;
				break;
			    }
			}
		    }
		    if (changed) {
			/* if we have re-ordered these hits, make sure they are really
			 * at the same distance.
			 */
			for (j = i + 1; j < k; j++) {
			    hits[j].hit_dist = hits[i].hit_dist;
			}
		    }
		}
		if (!reorder || reorder_failed) {

		    exits = 0;
		    enters = 0;
		    if (i == 0) {
			dn = 1.0;
		    } else {
			dn = hits[i-1].hit_vpriv[X];
		    }
		    for (j = i; j < k; j++) {
			if (hits[j].hit_vpriv[X] > 0)
			    exits++;
			else
			    enters++;
			if (dn * hits[j].hit_vpriv[X] < 0) {
			    if (keep1 == -1) {
				keep1 = j;
				dn = hits[j].hit_vpriv[X];
			    } else if (keep2 == -1) {
				keep2 = j;
				/* TODO: assignment not used - should it be? */
				/*dn = hits[j].hit_vpriv[X];*/
				break;
			    }
			}
		    }

		    if (keep2 == -1) {
			/* did not find two keepers, perhaps they were
			 * all entrances or all exits.
			 */
			if (exits == k - i || enters == k - i) {
			    /* eliminate all but one entrance or exit */
			    for (j = k - 1; j > i; j--) {
				/* delete this hit */
				for (l=j; l<snhits-1; l++)
				    hits[l] = hits[l+1];
				if (psp) {
				    psp->htab.end--;
				}
				snhits--;
			    }
			    i--;
			}
		    } else {
			/* found an entrance and an exit to keep */
			for (j = k - 1; j >= i; j--) {
			    if (j != keep1 && j != keep2) {
				/* delete this hit */
				for (l=j; l<snhits-1; l++)
				    hits[l] = hits[l+1];
				if (psp) {
				    psp->htab.end--;
				}
				snhits--;
			    }
			}
			i--;
		    }
		}
	    }
	}

	/*
	 * Handle cases where there are multiple adjacent entrances or
	 * exits along the shotline.  Using the FILO approach where we
	 * keep the "First In" from multiple entrances and the "Last
	 * Out" for multiple exits.
	 *
	 * Many of these cases were being generated when the shot ray
	 * grazed a surface.  Grazing shots should USUALLY be treated
	 * as non-hits (and is the case for other primitives).  With
	 * BoTs, however, these can cause multiple entrances and exits
	 * when adjacent surfaces are hit.
	 *
	 * Example #1: CROSS-SECTION OF CONVEX SOLID
	 *
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  ------------            ------------
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * For this grazing hit, LOS(X) was being shown as:
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  XXXXXXXXXXXX            XXXXXXXXXXXXX
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * Using a FILO approach, now LOS(X) shows as:
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 */
	for (i = 0; i < snhits-1; i++) {
	    if (hits[i].hit_vpriv[X] < 0.0) { /* entering */
		k = i + 1;
		while ((k < snhits) && (hits[k].hit_vpriv[X] < 0.0)) {
		    for (j = i; j < snhits-1; j++)
			hits[j] = hits[j+1];
		    snhits--;
		}
	    } else if (hits[i].hit_vpriv[X] > 0.0) { /* exiting */
		k = i + 1;
		while ((k < snhits) && (hits[k].hit_vpriv[X] > 0.0)) {
		    for (j = i + 1; j < snhits - 1; j++)
			hits[j] = hits[j+1];
		    snhits--;
		}
	    }
	}

	/*
	 * Note that we could try to use a LIFO approach so that we
	 * more consistently count all grazing hits as a miss, but
	 * there's an increased chance of slipping through a crack
	 * with BoTs without a change to check mesh neighbors:
	 *
	 * Using a LIFO approach, the above LOS(X) would have been:
	 *
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  -----------XXXXXXXXXXXXXX------------
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * Example #2: CROSS-SECTION OF CONCAVE SOLID
	 *
	 *   ray-->  ------------            ------------
	 *           |entrance  |exit        |entrance  |exit
	 *           |          |            |          |
	 *                      --------------
	 *
	 * Using LIFO, we would report a miss for the concave case,
	 * but with FILO this will return two hit segments.
	 *
	 *   ray-->  XXXXXXXXXXXX            XXXXXXXXXXXX
	 *           |entrance  |exit        |entrance  |exit
	 *           |          |            |          |
	 *                      --------------
	 */
    }

    /* if first hit is an exit, it is likely due to the "piece" for
     * the corresponding entrance not being processed (this is OK, but
     * we need to eliminate the stray exit hit)
     */
    while (snhits > 0 && hits[0].hit_vpriv[X] > 0.0) {
	ssize_t j;

	for (j = 1; j < snhits; j++) {
	    hits[j-1] = hits[j];
	}
	snhits--;
    }

    /* similar for trailing entrance hits */
    while (snhits > 0 && hits[snhits-1].hit_vpriv[X] < 0.0) {
	snhits--;
    }

    hits_da->count = snhits;

    if ((snhits&1)) {
	/*
	 * If this condition exists, it is almost certainly due to the
	 * dn==0 check above.  Thus, we will make the last surface
	 * rather thin.  This at least makes the presence of this
	 * solid known.  There may be something better we can do.
	 */

	if (snhits > 2) {
	    fastf_t dot1, dot2;
	    ssize_t j;

	    /* likely an extra hit, look for consecutive entrances or
	     * exits.
	     */

	    dot2 = 1.0;
	    i = 0;
	    while (i<snhits) {
		dot1 = dot2;
		dot2 = hits[i].hit_vpriv[X];
		if (dot1 > 0.0 && dot2 > 0.0) {
		    /* two consecutive exits, manufacture an entrance
		     * at same distance as second exit.
		     */
		    /* XXX This consumes an extra hit structure in the array */
		    DA_APPEND(hits_da, zeroed_hit_s, struct hit);
		    for (j = snhits; j > i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
		    dot2 = hits[i].hit_vpriv[X];
		    snhits++;
		    bu_log("\t\tadding fictitious entry at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		    bu_log("\t\t\tray = (%g %g %g) -> (%g %g %g)\n", V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir));
		} else if (dot1 < 0.0 && dot2 < 0.0) {
		    /* two consecutive entrances, manufacture an exit
		     * between them.
		     */
		    /* XXX This consumes an extra hit structure in the array */
		    DA_APPEND(hits_da, zeroed_hit_s, struct hit);
		    for (j = snhits; j > i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    hits[i] = hits[i-1];	/* struct copy */
		    hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
		    dot2 = hits[i].hit_vpriv[X];
		    snhits++;
		    bu_log("\t\tadding fictitious exit at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		    bu_log("\t\t\tray = (%g %g %g) -> (%g %g %g)\n", V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir));
		}
		i++;
	    }
	}
    }

    if ((snhits&1)) {
	DA_APPEND(hits_da, hits[snhits-1], struct hit);
	hits[snhits].hit_vpriv[X] = -hits[snhits].hit_vpriv[X];
	snhits++;
    }

    /* snhits is even, build segments */
    for (i = 0; i < snhits; i += 2) {
	triangle_s *trip;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[i];	/* struct copy */
	trip = (triangle_s *)hits[i].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, trip->face_norm, IN_SEG);
	segp->seg_out = hits[i+1];	/* struct copy */
	trip = (triangle_s *)hits[i+1].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, trip->face_norm, OUT_SEG);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }

    return snhits;			/* HIT */
}
/**
 * Given an array of hits, make sebgents out of them.  Exactly how
 * this is to be done depends on the mode of the BoT.
 */
int
rt_bot_makesegs(hit_da *hits, struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead, struct rt_piecestate *psp)
{
    RT_CK_SOLTAB(stp);
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;

    if (bot->bot_mode == RT_BOT_PLATE ||
	bot->bot_mode == RT_BOT_PLATE_NOCOS) {
	return rt_bot_plate_segs(hits->items, hits->count, stp, rp, ap, seghead, bot);
    }

    if (bot->bot_mode == RT_BOT_SURFACE) {
	return rt_bot_surface_segs(hits->items, hits->count, stp, ap, seghead);
    }

    BU_ASSERT(bot->bot_mode == RT_BOT_SOLID);

    if (bot->bot_orientation == RT_BOT_UNORIENTED) {
	return rt_bot_unoriented_segs(hits->items, hits->count, stp, rp, ap, seghead);
    }

    return rt_bot_oriented_segs(hits, stp, ap, seghead, psp);
}

void
rt_bot_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    size_t i;
    struct rt_bot_internal *bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    rt_bot_condense(bot_ip);
    VSETALL(*cent, 0.0);
    for (i = 0; i < bot_ip->num_vertices; i++) {
	VADD2(*cent, *cent, &bot_ip->vertices[i*3]);
    }
    VSCALE(*cent, *cent, 1.0 / (fastf_t)bot_ip->num_vertices);
}


/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_bot_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct rt_bot_internal *bot_ip;
    struct shell *s;
    struct vertex **verts;
    size_t i;
    struct bu_list *vlfree = &rt_vlfree;

    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);
    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
#define RT_BOT_TESS_MAX_FACES 1024
	size_t faces[RT_BOT_TESS_MAX_FACES];
	plane_t planes[RT_BOT_TESS_MAX_FACES];
	point_t center;

	rt_bot_centroid(&center, ip);
	bu_log("center pt = (%f %f %f)\n", V3ARGS(center));

	/* get the faces that use each vertex */
	for (i = 0; i < bot_ip->num_vertices; i++) {
	    size_t faceCount = 0;
	    size_t j;
	    for (j = 0; j < bot_ip->num_faces; j++) {
		size_t k;
		for (k = 0; k < 3; k++) {
		    if ((size_t)bot_ip->faces[j*3+k] == i) {
			/* this face uses vertex i */
			faces[faceCount] = j;
			faceCount++;
			break;
		    }
		}
	    }
	    bu_log("Vertex #%zu appears in %zu faces\n", i, faceCount);
	    if (faceCount == 0) {
		continue;
	    }
	    if (bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS)
		for (i = 0; i < faceCount; i++) {
		    size_t k;
		    fastf_t *plane;
		    for (k = 0; k < 3; k++) {
			size_t idx = faces[i] * 3 + k;
			VMOVE(planes[i], &bot_ip->normals[bot_ip->face_normals[idx]*3]);
			planes[i][3] = VDOT(planes[i], &bot_ip->vertices[bot_ip->faces[faces[i]*3]*3]);
		    }
		    plane = planes[i];
		    bu_log("\tplane #%zu = (%f %f %f %f)\n", i, V4ARGS(plane));
		}
	}
	return -1;
    }
    *r = nmg_mrsv(m);     /* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    verts = (struct vertex **)bu_calloc(bot_ip->num_vertices, sizeof(struct vertex *), "rt_bot_tess: *verts[]");

    for (i = 0; i < bot_ip->num_faces; i++) {
	struct faceuse *fu;
	struct vertex **corners[3];
	point_t pt[3];

	if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
	    continue; /* sanity */

	if (bot_ip->orientation == RT_BOT_CW) {
	    VMOVE(pt[2], &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	    VMOVE(pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	    VMOVE(pt[0], &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	    corners[2] = &verts[bot_ip->faces[i*3+0]];
	    corners[1] = &verts[bot_ip->faces[i*3+1]];
	    corners[0] = &verts[bot_ip->faces[i*3+2]];
	} else {
	    VMOVE(pt[0], &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	    VMOVE(pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	    VMOVE(pt[2], &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	    corners[0] = &verts[bot_ip->faces[i*3+0]];
	    corners[1] = &verts[bot_ip->faces[i*3+1]];
	    corners[2] = &verts[bot_ip->faces[i*3+2]];
	}

	if (!bg_3pnts_distinct(pt[0], pt[1], pt[2], tol)
	    || bg_3pnts_collinear(pt[0], pt[1], pt[2], tol))
	    continue;

	if ((fu=nmg_cmface(s, corners, 3)) == (struct faceuse *)NULL) {
	    bu_log("rt_bot_tess() nmg_cmface() failed for face #%zu\n", i);
	    continue;
	}

	if (!(*corners[0])->vg_p)
	    nmg_vertex_gv(*(corners[0]), pt[0]);
	if (!(*corners[1])->vg_p)
	    nmg_vertex_gv(*(corners[1]), pt[1]);
	if (!(*corners[2])->vg_p)
	    nmg_vertex_gv(*(corners[2]), pt[2]);

	if (nmg_calc_face_g(fu, vlfree))
	    nmg_kfu(fu);
	else if (bot_ip->mode == RT_BOT_SURFACE) {
	    struct vertex **tmp;

	    tmp = corners[0];
	    corners[0] = corners[2];
	    corners[2] = tmp;
	    if ((fu=nmg_cmface(s, corners, 3)) == (struct faceuse *)NULL)
		bu_log("rt_bot_tess() nmg_cmface() failed for face #%zu\n", i);
	    else
		nmg_calc_face_g(fu, vlfree);
	}
    }

    bu_free(verts, "rt_bot_tess *verts[]");

    nmg_mark_edges_real(&s->l.magic, vlfree);

    nmg_region_a(*r, tol);

    if (bot_ip->mode == RT_BOT_SOLID && bot_ip->orientation == RT_BOT_UNORIENTED)
	nmg_fix_normals(s, vlfree, tol);

    return 0;
}


/**
 * Import an BOT from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_bot_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_bot_internal *bot_ip;
    union record *rp;
    size_t i;
    size_t chars_used;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_BOT) {
	bu_log("rt_bot_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BOT;
    ip->idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(ip->idb_ptr, struct rt_bot_internal);

    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;

    bot_ip->num_vertices = ntohl(*(uint32_t *)&rp->bot.bot_num_verts[0]);
    bot_ip->num_faces = ntohl(*(uint32_t *)&rp->bot.bot_num_triangles[0]);
    bot_ip->orientation = rp->bot.bot_orientation;
    bot_ip->mode = rp->bot.bot_mode;
    bot_ip->bot_flags = 0;

    bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3, sizeof(fastf_t), "Bot vertices");
    bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3, sizeof(int), "Bot faces");

    if (mat == NULL) mat = bn_mat_identity;
    for (i = 0; i < bot_ip->num_vertices; i++) {
	double tmp[ELEMENTS_PER_POINT];

	bu_cv_ntohd((unsigned char *)tmp, (const unsigned char *)(&rp->bot.bot_data[i*24]), ELEMENTS_PER_POINT);
	MAT4X3PNT(&(bot_ip->vertices[i*ELEMENTS_PER_POINT]), mat, tmp);
    }

    chars_used = bot_ip->num_vertices * ELEMENTS_PER_POINT * 8;

    for (i = 0; i < bot_ip->num_faces; i++) {
	size_t idx=chars_used + i * 12;

	bot_ip->faces[i*ELEMENTS_PER_POINT + 0] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 0]);
	bot_ip->faces[i*ELEMENTS_PER_POINT + 1] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 4]);
	bot_ip->faces[i*ELEMENTS_PER_POINT + 2] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 8]);
    }

    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	chars_used = bot_ip->num_vertices * ELEMENTS_PER_POINT * 8 + bot_ip->num_faces * 12;

	bot_ip->thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "BOT thickness");
	for (i = 0; i < bot_ip->num_faces; i++) {
	    double scan;

	    bu_cv_ntohd((unsigned char *)&scan, (const unsigned char *)(&rp->bot.bot_data[chars_used + i*8]), 1);
	    bot_ip->thickness[i] = scan; /* convert double to fastf_t */
	}

	bot_ip->face_mode = bu_hex_to_bitv((const char *)(&rp->bot.bot_data[chars_used + bot_ip->num_faces * 8]));
    } else {
	bot_ip->thickness = (fastf_t *)NULL;
	bot_ip->face_mode = (struct bu_bitv *)NULL;
    }

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_bot_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_bot_internal *bot_ip;
    union record *rec;
    size_t i;
    size_t chars_used;
    size_t num_recs;
    struct bu_vls face_mode = BU_VLS_INIT_ZERO;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BOT) return -1;
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (bot_ip->num_normals > 0) {
	bu_log("BOT surface normals not supported in older database formats, normals not saved\n");
	bu_log("\tPlease update to current database format using \"dbupgrade\"\n");
    }

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(struct bot_rec) - 1 +
	bot_ip->num_vertices * ELEMENTS_PER_POINT * 8 + bot_ip->num_faces * ELEMENTS_PER_POINT * 4;
    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	if (!bot_ip->face_mode) {
	    bot_ip->face_mode = bu_bitv_new(bot_ip->num_faces);
	}
	if (!bot_ip->thickness)
	    bot_ip->thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "BOT thickness");
	bu_bitv_to_hex(&face_mode, bot_ip->face_mode);
	ep->ext_nbytes += bot_ip->num_faces * 8 + bu_vls_strlen(&face_mode) + 1;
    }

    /* round up to the nearest granule */
    if (ep->ext_nbytes % (sizeof(union record))) {
	ep->ext_nbytes += (sizeof(union record))
	    - ep->ext_nbytes % (sizeof(union record));
    }
    num_recs = ep->ext_nbytes / sizeof(union record) - 1;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "bot external");
    rec = (union record *)ep->ext_buf;

    rec->bot.bot_id = DBID_BOT;

    *(uint32_t *)&rec->bot.bot_nrec[0] = htonl((uint32_t)num_recs);
    rec->bot.bot_orientation = bot_ip->orientation;
    rec->bot.bot_mode = bot_ip->mode;
    rec->bot.bot_err_mode = 0;
    *(uint32_t *)&rec->bot.bot_num_verts[0] = htonl(bot_ip->num_vertices);
    *(uint32_t *)&rec->bot.bot_num_triangles[0] = htonl(bot_ip->num_faces);

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */


    /* convert from local editing units to mm and export to database
     * record format
     */
    for (i = 0; i < bot_ip->num_vertices; i++) {
	/* must be double for import and export */
	double tmp[ELEMENTS_PER_POINT];

	VSCALE(tmp, &bot_ip->vertices[i*ELEMENTS_PER_POINT], local2mm);
	bu_cv_htond((unsigned char *)&rec->bot.bot_data[i*24], (const unsigned char *)tmp, ELEMENTS_PER_POINT);
    }

    chars_used = bot_ip->num_vertices * 24;

    for (i = 0; i < bot_ip->num_faces; i++) {
	size_t idx=chars_used + i * 12;

	*(uint32_t *)&rec->bot.bot_data[idx + 0] = htonl(bot_ip->faces[i*ELEMENTS_PER_POINT+0]);
	*(uint32_t *)&rec->bot.bot_data[idx + 4] = htonl(bot_ip->faces[i*ELEMENTS_PER_POINT+1]);
	*(uint32_t *)&rec->bot.bot_data[idx + 8] = htonl(bot_ip->faces[i*ELEMENTS_PER_POINT+2]);
    }

    chars_used += bot_ip->num_faces * 12;

    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	for (i = 0; i < bot_ip->num_faces; i++) {
	    /* must be double for import and export */
	    double tmp;

	    tmp = bot_ip->thickness[i] * local2mm;
	    bu_cv_htond((unsigned char *)&rec->bot.bot_data[chars_used], (const unsigned char *)&tmp, 1);
	    chars_used += 8;
	}
	bu_strlcpy((char *)&rec->bot.bot_data[chars_used], bu_vls_addr(&face_mode), ep->ext_nbytes - (sizeof(struct bot_rec)-1) - chars_used);
	bu_vls_free(&face_mode);
    }

    return 0;
}

int
rt_bot_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_bot_internal *tip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(tip);
    struct rt_bot_internal *top = (struct rt_bot_internal *)rop->idb_ptr;
    RT_BOT_CK_MAGIC(top);

    if (tip->num_vertices != top->num_vertices)
	return BRLCAD_ERROR;
    if (tip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	if (!(top->bot_flags & RT_BOT_HAS_SURFACE_NORMALS))
	    return BRLCAD_ERROR;
	if (tip->num_normals != top->num_vertices)
	    return BRLCAD_ERROR;
    }
    if (tip->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	if (!(top->bot_flags & RT_BOT_HAS_TEXTURE_UVS))
	    return BRLCAD_ERROR;
	if (tip->num_uvs != top->num_uvs)
	    return BRLCAD_ERROR;
    }

    if (tip->vertices) {
	for (size_t i = 0; i < tip->num_vertices; i++) {
	    vect_t tmp_v;
	    VMOVE(tmp_v, &(tip->vertices[i*ELEMENTS_PER_POINT]));
	    MAT4X3PNT(&(top->vertices[i*ELEMENTS_PER_POINT]), mat, tmp_v);
	}
    }
    if (tip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	vect_t tmp_n;
	for (size_t i = 0; i < tip->num_normals; i++) {
	    VMOVE(tmp_n, &(tip->normals[i*ELEMENTS_PER_VECT]));
	    MAT4X3VEC(&(top->normals[i*ELEMENTS_PER_VECT]), mat, tmp_n);
	}
    }
    if (tip->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	vect_t tmp_uvs;
	for (size_t i = 0; i < tip->num_uvs; i++) {
	    VMOVE(tmp_uvs, &(tip->uvs[i*ELEMENTS_PER_VECT]));
	    MAT4X3VEC(&(top->uvs[i*ELEMENTS_PER_VECT]), mat, tmp_uvs);
	}
    }

    return BRLCAD_OK;
}


int
rt_bot_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_bot_internal *bip;
    unsigned char *cp;
    size_t i;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BOT;
    ip->idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(ip->idb_ptr, struct rt_bot_internal);

    bip = (struct rt_bot_internal *)ip->idb_ptr;
    bip->magic = RT_BOT_INTERNAL_MAGIC;

    cp = ep->ext_buf;
    bip->num_vertices = ntohl(*(uint32_t *)&cp[0]);
    cp += SIZEOF_NETWORK_LONG;
    bip->num_faces = ntohl(*(uint32_t *)&cp[0]);
    cp += SIZEOF_NETWORK_LONG;
    bip->orientation = *cp++;
    bip->mode = *cp++;
    bip->bot_flags = *cp++;

    if (bip->num_vertices > 0) {
	bip->vertices = (fastf_t *)bu_calloc(bip->num_vertices * ELEMENTS_PER_POINT, sizeof(fastf_t), "BOT vertices");
    } else {
	bip->vertices = (fastf_t *)NULL;
    }

    if (bip->num_faces > 0) {
	bip->faces = (int *)bu_calloc(bip->num_faces * 3, sizeof(int), "BOT faces");
    } else {
	bip->faces = (int *)NULL;
    }

    if (bip->vertices == NULL || bip->faces == NULL) {
	bu_log("WARNING: BoT contains %zu vertices, %zu faces\n", bip->num_vertices, bip->num_faces);
    }


    if (bip->vertices) {
	for (i = 0; i < bip->num_vertices; i++) {
	    /* must be double for import and export */
	    double tmp[ELEMENTS_PER_POINT];

	    bu_cv_ntohd((unsigned char *)tmp, (const unsigned char *)cp, ELEMENTS_PER_POINT);
	    cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_POINT;
	    VMOVE(&(bip->vertices[i*ELEMENTS_PER_POINT]), tmp);
	}
    }

    if (bip->faces) {
	for (i = 0; i < bip->num_faces; i++) {
	    bip->faces[i*3 + 0] = ntohl(*(uint32_t *)&cp[0]);
	    cp += SIZEOF_NETWORK_LONG;
	    bip->faces[i*3 + 1] = ntohl(*(uint32_t *)&cp[0]);
	    cp += SIZEOF_NETWORK_LONG;
	    bip->faces[i*3 + 2] = ntohl(*(uint32_t *)&cp[0]);
	    cp += SIZEOF_NETWORK_LONG;
	}
    }

    if (bip->num_faces && (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS)) {
	bip->thickness = (fastf_t *)bu_calloc(bip->num_faces, sizeof(fastf_t), "BOT thickness");
	for (i = 0; i < bip->num_faces; i++) {
	    double scan;
	    bu_cv_ntohd((unsigned char *)&scan, cp, 1);
	    bip->thickness[i] = scan; /* convert double to fastf_t */
	    cp += SIZEOF_NETWORK_DOUBLE;
	}
	bip->face_mode = bu_hex_to_bitv((const char *)cp);
	while (*(cp++) != '\0');
    } else {
	bip->thickness = (fastf_t *)NULL;
	bip->face_mode = (struct bu_bitv *)NULL;
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	/* must be double for import and export */
	double tmp[ELEMENTS_PER_VECT];

	bip->num_normals = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
	bip->num_face_normals = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;

	if (bip->num_normals <= 0) {
	    bip->normals = (fastf_t *)NULL;
	}
	if (bip->num_face_normals <= 0) {
	    bip->face_normals = (int *)NULL;
	}
	if (bip->num_normals > 0) {
	    bip->normals = (fastf_t *)bu_calloc(bip->num_normals * 3, sizeof(fastf_t), "BOT normals");

	    for (i = 0; i < bip->num_normals; i++) {
		bu_cv_ntohd((unsigned char *)tmp, (const unsigned char *)cp, ELEMENTS_PER_VECT);
		cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
		VMOVE(&(bip->normals[i*ELEMENTS_PER_VECT]), tmp);
	    }
	}
	if (bip->num_face_normals > 0) {
	    bip->face_normals = (int *)bu_calloc(bip->num_face_normals * 3, sizeof(int), "BOT face normals");

	    for (i = 0; i < bip->num_face_normals; i++) {
		bip->face_normals[i*3 + 0] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_normals[i*3 + 1] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_normals[i*3 + 2] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
	    }
	}
    } else {
	bip->normals = (fastf_t *)NULL;
	bip->face_normals = (int *)NULL;
	bip->num_normals = 0;
	bip->num_face_normals = 0;
    }

    if (bip->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	/* must be double for import and export */
	double tmp[ELEMENTS_PER_VECT];

	bip->num_uvs = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
	bip->num_face_uvs = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;

	if (bip->num_uvs <= 0) {
	    bip->uvs = (fastf_t *)NULL;
	}
	if (bip->num_face_uvs <= 0) {
	    bip->face_uvs = (int *)NULL;
	}
	if (bip->num_uvs > 0) {
	    bip->uvs = (fastf_t *)bu_calloc(bip->num_uvs * 3, sizeof(fastf_t), "BOT texture UVs");

	    for (i = 0; i < bip->num_uvs; i++) {
		bu_cv_ntohd((unsigned char *)tmp, (const unsigned char *)cp, ELEMENTS_PER_VECT);
		cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
		VMOVE(&(bip->uvs[i*ELEMENTS_PER_VECT]), tmp);
	    }
	}
	if (bip->num_face_uvs > 0) {
	    bip->face_uvs = (int *)bu_calloc(bip->num_face_uvs * 3, sizeof(int), "BOT face UVs");

	    for (i = 0; i < bip->num_face_uvs; i++) {
		bip->face_uvs[i*3 + 0] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_uvs[i*3 + 1] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_uvs[i*3 + 2] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
	    }
	}
    } else {
	bip->uvs = (fastf_t *)NULL;
	bip->face_uvs = (int *)NULL;
	bip->num_uvs = 0;
	bip->num_face_uvs = 0;
    }

    bip->tie = NULL;

    /* Apply transform */
    if (mat == NULL) mat = bn_mat_identity;
    return rt_bot_mat(ip, mat, ip);
}


int
rt_bot_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_bot_internal *bip;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    unsigned char *cp;
    size_t i;
    size_t rem;

    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    if (ip->idb_type != ID_BOT) return -1;
    bip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bip);

    BU_CK_EXTERNAL(ep);

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	/* build hex string for face mode */
	if (bip->face_mode)
	    bu_bitv_to_hex(&vls, bip->face_mode);
    }

    ep->ext_nbytes = 3				/* orientation, mode, bot_flags */
	+ SIZEOF_NETWORK_LONG * (bip->num_faces * 3 + 2) /* faces, num_faces, num_vertices */
	+ SIZEOF_NETWORK_DOUBLE * bip->num_vertices * 3; /* vertices */

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	ep->ext_nbytes += SIZEOF_NETWORK_DOUBLE * bip->num_faces /* face thicknesses */
	    + bu_vls_strlen(&vls) + 1;	/* face modes */
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	ep->ext_nbytes += SIZEOF_NETWORK_DOUBLE * bip->num_normals * 3 /* vertex normals */
	    + SIZEOF_NETWORK_LONG * (bip->num_face_normals * 3 + 2); /* indices into normals array, num_normals, num_face_normals */
    }

    if (bip->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	ep->ext_nbytes += SIZEOF_NETWORK_DOUBLE * bip->num_uvs * 3 /* vertex uvs */
	    + SIZEOF_NETWORK_LONG * (bip->num_face_uvs * 3 + 2); /* indices into uvs array, num_uvs, num_face_uvs */
    }

    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "BOT external");

    cp = ep->ext_buf;
    rem = ep->ext_nbytes;

    *(uint32_t *)&cp[0] = htonl(bip->num_vertices);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    *(uint32_t *)&cp[0] = htonl(bip->num_faces);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    *cp++ = bip->orientation;
    *cp++ = bip->mode;
    *cp++ = bip->bot_flags;
    rem -= 3;

    for (i = 0; i < bip->num_vertices; i++) {
	/* must be double for import and export */
	double tmp[ELEMENTS_PER_POINT];

	VSCALE(tmp, &bip->vertices[i*ELEMENTS_PER_POINT], local2mm);
	bu_cv_htond(cp, (unsigned char *)tmp, ELEMENTS_PER_POINT);
	cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_POINT;
	rem -= SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_POINT;
    }

    for (i = 0; i < bip->num_faces; i++) {
	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 0]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 1]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 2]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;
    }

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	for (i = 0; i < bip->num_faces; i++) {
	    /* must be double for import and export */
	    double tmp;

	    tmp = bip->thickness[i] * local2mm;
	    bu_cv_htond(cp, (const unsigned char *)&tmp, 1);
	    cp += SIZEOF_NETWORK_DOUBLE;
	    rem -= SIZEOF_NETWORK_DOUBLE;
	}
	bu_strlcpy((char *)cp, bu_vls_addr(&vls), rem);
	cp += bu_vls_strlen(&vls);
	rem -= bu_vls_strlen(&vls);
	*cp = '\0';
	cp++;
	rem--;
	bu_vls_free(&vls);
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	*(uint32_t *)&cp[0] = htonl(bip->num_normals);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->num_face_normals);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	if (bip->num_normals > 0) {
	    /* must be double for import and export */
	    double *normals;
	    normals = (double *)bu_malloc(bip->num_normals*ELEMENTS_PER_VECT*sizeof(double), "normals");

	    /* convert fastf_t to double */
	    for (i = 0; i < bip->num_normals * ELEMENTS_PER_VECT; i++) {
		normals[i] = bip->normals[i];
	    }

	    bu_cv_htond(cp, (unsigned char*)normals, bip->num_normals*ELEMENTS_PER_VECT);

	    bu_free(normals, "normals");

	    cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * bip->num_normals;
	    rem -= SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * bip->num_normals;
	}
	if (bip->num_face_normals > 0) {
	    for (i = 0; i < bip->num_face_normals; i++) {
		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*ELEMENTS_PER_VECT + 0]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*ELEMENTS_PER_VECT + 1]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*ELEMENTS_PER_VECT + 2]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;
	    }
	}
    }

    if (bip->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	*(uint32_t *)&cp[0] = htonl(bip->num_uvs);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->num_face_uvs);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	if (bip->num_uvs > 0) {
	    /* must be double for import and export */
	    double *uvs;
	    uvs = (double *)bu_malloc(bip->num_uvs*ELEMENTS_PER_VECT*sizeof(double), "uvs");

	    /* convert fastf_t to double */
	    for (i = 0; i < bip->num_uvs * ELEMENTS_PER_VECT; i++) {
		uvs[i] = bip->uvs[i];
	    }

	    bu_cv_htond(cp, (unsigned char*)uvs, bip->num_uvs*ELEMENTS_PER_VECT);

	    bu_free(uvs, "uvs");

	    cp += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * bip->num_uvs;
	    rem -= SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * bip->num_uvs;
	}
	if (bip->num_face_uvs > 0) {
	    for (i = 0; i < bip->num_face_uvs; i++) {
		*(uint32_t *)&cp[0] = htonl(bip->face_uvs[i*ELEMENTS_PER_VECT + 0]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_uvs[i*ELEMENTS_PER_VECT + 1]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_uvs[i*ELEMENTS_PER_VECT + 2]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;
	    }
	}
    }

    return 0;
}


static char *unoriented="unoriented";
static char *ccw="counter-clockwise";
static char *cw="clockwise";
static char *unknown_orientation="unknown orientation";
static char *surface="\tThis is a surface with no volume\n";
static char *solid="\tThis is a solid object (not just a surface)\n";
static char *plate="\tThis is a FASTGEN plate mode solid\n";
static char *nocos="\tThis is a plate mode solid with no obliquity angle effect\n";
static char *unknown_mode="\tunknown mode\n";

/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_bot_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_bot_internal *bot_ip =
	(struct rt_bot_internal *)ip->idb_ptr;
    char buf[256];
    char *orientation, *mode;
    size_t i;

    size_t badVertexCount = 0;

    RT_BOT_CK_MAGIC(bot_ip);
    bu_vls_strcat(str, "Bag of triangles (BOT)\n");

    switch (bot_ip->orientation) {
	case RT_BOT_UNORIENTED:
	    orientation = unoriented;
	    break;
	case RT_BOT_CCW:
	    orientation = ccw;
	    break;
	case RT_BOT_CW:
	    orientation = cw;
	    break;
	default:
	    orientation = unknown_orientation;
	    break;
    }
    switch (bot_ip->mode) {
	case RT_BOT_SURFACE:
	    mode = surface;
	    break;
	case RT_BOT_SOLID:
	    mode = solid;
	    break;
	case RT_BOT_PLATE:
	    mode = plate;
	    break;
	case RT_BOT_PLATE_NOCOS:
	    mode = nocos;
	    break;
	default:
	    mode = unknown_mode;
	    break;
    }
    snprintf(buf, 256, "\t%lu vertices, %lu faces (%s)\n",
	     (long unsigned)bot_ip->num_vertices,
	     (long unsigned)bot_ip->num_faces,
	     orientation);
    bu_vls_strcat(str, buf);
    bu_vls_strcat(str, mode);
    if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && bot_ip->num_normals > 0) {
	bu_vls_strcat(str, "\twith surface normals");
	if (bot_ip->bot_flags & RT_BOT_USE_NORMALS) {
	    bu_vls_strcat(str, " (they will be used)\n");
	} else {
	    bu_vls_strcat(str, " (they will be ignored)\n");
	}
    }

    /* NOTE: this value is not arbitrary, but is dependent on the init in libged/list/list.c */
    if (verbose <= 99) {
	bu_vls_strcat(str, "    use -v (verbose) for all data\n");
	return 0;
    }

    for (i = 0; i < bot_ip->num_faces; i++) {
	size_t j, k;
	point_t pt[3];

	snprintf(buf, 256, "\tface %lu:", (long unsigned)i);
	bu_vls_strcat(str, buf);

	for (j = 0; j < 3; j++) {
	    size_t ptnum;

	    ptnum = bot_ip->faces[i*3+j];
	    if (ptnum >= bot_ip->num_vertices) {
		bu_vls_strcat(str, " (\?\?\? \?\?\? \?\?\?)");
		badVertexCount++;
	    } else {
		VSCALE(pt[j], &bot_ip->vertices[ptnum*3], mm2local);
		snprintf(buf, 256, " (%g %g %g)", V3INTCLAMPARGS(pt[j]));
		bu_vls_strcat(str, buf);
	    }
	}
	if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && bot_ip->num_normals > 0) {
	    bu_vls_strcat(str, " normals: ");
	    for (k = 0; k < 3; k++) {
		size_t idx;

		idx = i*3 + k;
		if ((size_t)bot_ip->face_normals[idx] >= bot_ip->num_normals) {
		    bu_vls_strcat(str, "none ");
		} else {
		    snprintf(buf, 256, "(%g %g %g) ", V3INTCLAMPARGS(&bot_ip->normals[bot_ip->face_normals[idx]*3]));
		    bu_vls_strcat(str, buf);
		}
	    }
	    bu_vls_strcat(str, "\n");
	} else {
	    bu_vls_strcat(str, "\n");
	}
	if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	    char *face_mode;

	    if (BU_BITTEST(bot_ip->face_mode, i))
		face_mode = "appended to hit point";
	    else
		face_mode = "centered about hit point";
	    snprintf(buf, 256, "\t\tthickness = %g, %s\n", INTCLAMP(mm2local*bot_ip->thickness[i]), face_mode);
	    bu_vls_strcat(str, buf);
	}
    }
    if (badVertexCount > 0) {
	snprintf(buf, 256, "\tThis BOT has %lu invalid references to vertices\n", (long unsigned)badVertexCount);
	bu_vls_strcat(str, buf);
    }

    return 0;
}


void
rt_bot_internal_free(struct rt_bot_internal *bot_ip)
{
    if (!bot_ip)
	return;

    RT_BOT_CK_MAGIC(bot_ip);
    bot_ip->magic = 0;			/* sanity */
    bot_ip->tie = NULL;
    bot_ip->mode = '\0';
    bot_ip->orientation = '\0';
    bot_ip->bot_flags = '\0';

    bu_free(bot_ip->faces, "BOT faces");
    bot_ip->faces = NULL;
    bot_ip->num_faces = 0;

    bu_free(bot_ip->vertices, "BOT vertices");
    bot_ip->vertices = NULL;
    bot_ip->num_vertices = 0;

    bu_free(bot_ip->thickness, "BOT thickness");
    bot_ip->thickness = NULL;

    bu_free(bot_ip->face_mode, "face_mode");
    bot_ip->face_mode = NULL;

    bu_free(bot_ip->normals, "BOT normals");
    bot_ip->normals = NULL;
    bot_ip->num_normals = 0;

    bu_free(bot_ip->face_normals, "BOT face normals");
    bot_ip->face_normals = NULL;
    bot_ip->num_face_normals = 0;

    bu_free(bot_ip->uvs, "BOT uvs");
    bot_ip->uvs = NULL;
    bot_ip->num_uvs = 0;

    bu_free(bot_ip->face_uvs, "BOT face uvs");
    bot_ip->face_uvs = NULL;
    bot_ip->num_face_uvs = 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_bot_ifree(struct rt_db_internal *ip)
{
    struct rt_bot_internal *bot_ip;

    RT_CK_DB_INTERNAL(ip);

    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    rt_bot_internal_free(bot_ip);
    bu_free(bot_ip, "bot ifree");

    ip->idb_ptr = NULL; /* sanity */
}


int
rt_bot_xform(struct rt_db_internal *op, const fastf_t *mat, struct rt_db_internal *ip, const int release, struct db_i *dbip)
{
    struct rt_bot_internal *botip, *botop;
    size_t i;
    point_t pt;

    RT_CK_DB_INTERNAL(ip);
    botip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(botip);
    if (dbip) RT_CK_DBI(dbip);

    if (op != ip && !release) {
	RT_DB_INTERNAL_INIT(op);
	BU_ALLOC(botop, struct rt_bot_internal);
	botop->magic = RT_BOT_INTERNAL_MAGIC;
	botop->mode = botip->mode;
	botop->orientation = botip->orientation;
	botop->bot_flags = botip->bot_flags;
	botop->num_vertices = botip->num_vertices;
	botop->num_faces = botip->num_faces;
	if (botop->num_vertices > 0) {
	    botop->vertices = (fastf_t *)bu_malloc(botip->num_vertices * ELEMENTS_PER_POINT * sizeof(fastf_t), "botop->vertices");
	}
	if (botop->num_faces > 0) {
	    botop->faces = (int *)bu_malloc(botip->num_faces * 3 * sizeof(int), "botop->faces");
	    memcpy(botop->faces, botip->faces, botop->num_faces * 3 * sizeof(int));
	}
	if (botip->thickness)
	    botop->thickness = (fastf_t *)bu_malloc(botip->num_faces * sizeof(fastf_t), "botop->thickness");
	if (botip->face_mode)
	    botop->face_mode = bu_bitv_dup(botip->face_mode);
	if (botip->thickness) {
	    for (i = 0; i < botip->num_faces; i++)
		botop->thickness[i] = botip->thickness[i];
	}

	if (botop->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    botop->num_normals = botip->num_normals;
	    botop->normals = (fastf_t *)bu_calloc(botop->num_normals * 3, sizeof(fastf_t), "BOT normals");
	    botop->face_normals = (int *)bu_calloc(botop->num_faces * 3, sizeof(int), "BOT face normals");
	    memcpy(botop->face_normals, botip->face_normals, botop->num_faces * 3 * sizeof(int));
	}
	op->idb_ptr = (void *)botop;
	op->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	op->idb_type = ID_BOT;
	op->idb_meth = &OBJ[ID_BOT];
    } else
	botop = botip;

    if (ip != op) {
	if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_init(&op->idb_avs, ip->idb_avs.count, "avs");
	    bu_avs_merge(&op->idb_avs, &ip->idb_avs);
	}
    }

    for (i = 0; i < botip->num_vertices; i++) {
	MAT4X3PNT(pt, mat, &botip->vertices[i*ELEMENTS_PER_POINT]);
	VMOVE(&botop->vertices[i*ELEMENTS_PER_POINT], pt);
    }

    for (i = 0; i < botip->num_normals; i++) {
	MAT4X3VEC(pt, mat, &botip->normals[i*3]);
	VMOVE(&botop->normals[i*3], pt);
    }

    if (release && op != ip) {
	rt_bot_ifree(ip);
    }

    return 0;
}


int
rt_bot_find_v_nearest_pt2(
    const struct rt_bot_internal *bot,
    const point_t pt2,
    const mat_t mat)
{
    point_t v;
    size_t idx;
    fastf_t dist=MAX_FASTF;
    int closest=-1;

    RT_BOT_CK_MAGIC(bot);

    for (idx = 0; idx < bot->num_vertices; idx++) {
	fastf_t tmp_dist;
	fastf_t tmpx, tmpy;

	MAT4X3PNT(v, mat, &bot->vertices[idx*ELEMENTS_PER_POINT]);
	tmpx = v[X] - pt2[X];
	tmpy = v[Y] - pt2[Y];
	tmp_dist = tmpx * tmpx + tmpy * tmpy;
	if (tmp_dist < dist) {
	    dist = tmp_dist;
	    closest = idx;
	}
    }

    return closest;
}


size_t
rt_bot_get_edge_list(const struct rt_bot_internal *bot, size_t **edge_list)
{
    size_t i;
    size_t edge_count = 0;
    size_t v1, v2, v3;

    if (bot->num_faces < 1)
	return edge_count;

    *edge_list = (size_t *)bu_calloc(bot->num_faces * 3 * 2, sizeof(size_t), "bot edge list");

    for (i = 0; i < bot->num_faces; i++) {
	v1 = bot->faces[i*3 + 0];
	v2 = bot->faces[i*3 + 1];
	v3 = bot->faces[i*3 + 2];

	if (!rt_bot_edge_in_list(v1, v2, *edge_list, edge_count)) {
	    (*edge_list)[edge_count*2 + 0] = v1;
	    (*edge_list)[edge_count*2 + 1] = v2;
	    edge_count++;
	}
	if (!rt_bot_edge_in_list(v3, v2, *edge_list, edge_count)) {
	    (*edge_list)[edge_count*2 + 0] = v3;
	    (*edge_list)[edge_count*2 + 1] = v2;
	    edge_count++;
	}
	if (!rt_bot_edge_in_list(v1, v3, *edge_list, edge_count)) {
	    (*edge_list)[edge_count*2 + 0] = v1;
	    (*edge_list)[edge_count*2 + 1] = v3;
	    edge_count++;
	}
    }

    return edge_count;
}


int
rt_bot_edge_in_list(const size_t v1, const size_t v2, const size_t edge_list[], const size_t edge_count)
{
    size_t i;
    size_t ev1, ev2;

    for (i = 0; i < edge_count; i++) {
	ev1 = edge_list[i*2 + 0];
	ev2 = edge_list[i*2 + 1];

	if (ev1 == v1 && ev2 == v2)
	    return 1;

	if (ev1 == v2 && ev2 == v1)
	    return 1;
    }

    return 0;
}


/**
 * This routine finds the edge closest to the 2D point "pt2", and
 * returns the edge as two vertex indices (vert1 and vert2). These
 * vertices are ordered (closest to pt2 is first)
 */
int
rt_bot_find_e_nearest_pt2(
    int *vert1,
    int *vert2,
    const struct rt_bot_internal *bot,
    const point_t pt2,
    const mat_t mat)
{
    size_t i;
    fastf_t dist=MAX_FASTF, tmp_dist;
    size_t *edge_list;
    size_t edge_count = 0;
    struct bn_tol tol;

    RT_BOT_CK_MAGIC(bot);

    if (bot->num_faces < 1)
	return -1;

    /* first build a list of edges */
    if ((edge_count = rt_bot_get_edge_list(bot, &edge_list)) == 0)
	return -1;

    /* build a tolerance structure for the bn_dist routine */
    tol.magic   = BN_TOL_MAGIC;
    tol.dist    = 0.0;
    tol.dist_sq = 0.0;
    tol.perp    = 0.0;
    tol.para    =  1.0;

    /* now look for the closest edge */
    for (i = 0; i < edge_count; i++) {
	point_t p1, p2, pca;
	vect_t p1_to_pca, p1_to_p2;
	int ret;

	MAT4X3PNT(p1, mat, &bot->vertices[ edge_list[i*2+0]*3]);
	MAT4X3PNT(p2, mat, &bot->vertices[ edge_list[i*2+1]*3]);
	p1[Z] = 0.0;
	p2[Z] = 0.0;

	ret = bg_dist_pnt2_lseg2(&tmp_dist, pca, p1, p2, pt2, &tol);

	if (ret < 3 || tmp_dist < dist) {
	    switch (ret) {
		case 0:
		    dist = 0.0;
		    if (tmp_dist < 0.5) {
			*vert1 = edge_list[i*2+0];
			*vert2 = edge_list[i*2+1];
		    } else {
			*vert1 = edge_list[i*2+1];
			*vert2 = edge_list[i*2+0];
		    }
		    break;
		case 1:
		    dist = 0.0;
		    *vert1 = edge_list[i*2+0];
		    *vert2 = edge_list[i*2+1];
		    break;
		case 2:
		    dist = 0.0;
		    *vert1 = edge_list[i*2+1];
		    *vert2 = edge_list[i*2+0];
		    break;
		case 3:
		    dist = tmp_dist;
		    *vert1 = edge_list[i*2+0];
		    *vert2 = edge_list[i*2+1];
		    break;
		case 4:
		    dist = tmp_dist;
		    *vert1 = edge_list[i*2+1];
		    *vert2 = edge_list[i*2+0];
		    break;
		case 5:
		    dist = tmp_dist;
		    V2SUB2(p1_to_pca, pca, p1);
		    V2SUB2(p1_to_p2, p2, p1);
		    if (MAG2SQ(p1_to_pca) / MAG2SQ(p1_to_p2) < 0.25) {
			*vert1 = edge_list[i*2+0];
			*vert2 = edge_list[i*2+1];
		    } else {
			*vert1 = edge_list[i*2+1];
			*vert2 = edge_list[i*2+0];
		    }
		    break;
	    }
	}
    }

    bu_free(edge_list, "bot edge list");

    return 0;
}


static char *modes[]={
    "ERROR: Unrecognized mode",
    "surf",
    "volume",
    "plate",
    "plate_nocos"
};


static char *orientation[]={
    "ERROR: Unrecognized orientation",
    "no",
    "rh",
    "lh"
};


static char *los[]={
    "center",
    "append"
};


/**
 * Examples -
 * db get name fm	get los facemode bit vector
 * db get name fm#	get los face mode of face # (center, append)
 * db get name V	get coords for all vertices
 * db get name V#	get coords for vertex #
 * db get name F	get vertex indices for all faces
 * db get name F#	get vertex indices for face #
 * db get name f	get list of coords for all faces
 * db get name f#	get list of 3 3tuple coords for face #
 * db get name T	get thickness for all faces
 * db get name T#	get thickness for face #
 * db get name N	get list of normals
 * db get name N#	get coords for normal #
 * db get name fn	get list indices into normal vectors for all faces
 * db get name fn#	get list indices into normal vectors for face #
 * db get name nv	get num_vertices
 * db get name nt	get num_faces
 * db get name nn	get num_normals
 * db get name nfn	get num_face_normals
 * db get name mode	get mode (surf, volume, plate, plane_nocos)
 * db get name orient	get orientation (no, rh, lh)
 * db get name flags	get BOT flags
 */
int
rt_bot_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_bot_internal *bot=(struct rt_bot_internal *)intern->idb_ptr;
    int status;
    size_t i;
    long li;

    RT_BOT_CK_MAGIC(bot);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "bot");
	bu_vls_printf(logstr, " mode %s orient %s",
		      modes[bot->mode], orientation[bot->orientation]);
	bu_vls_printf(logstr, " flags {");
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_vls_printf(logstr, " has_normals");
	}
	if (bot->bot_flags & RT_BOT_USE_NORMALS) {
	    bu_vls_printf(logstr, " use_normals");
	}
	if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	    bu_vls_printf(logstr, " use_floats");
	}
	bu_vls_printf(logstr, "} V {");
	for (i = 0; i < bot->num_vertices; i++)
	    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
			  V3ARGS(&bot->vertices[i*3]));
	bu_vls_strcat(logstr, "} F {");
	for (i = 0; i < bot->num_faces; i++)
	    bu_vls_printf(logstr, " { %d %d %d }",
			  V3ARGS(&bot->faces[i*3]));
	bu_vls_strcat(logstr, "}");
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    bu_vls_strcat(logstr, " T {");
	    for (i = 0; i < bot->num_faces; i++)
		bu_vls_printf(logstr, " %.25G", bot->thickness[i]);
	    bu_vls_strcat(logstr, "} fm ");
	    bu_bitv_to_hex(logstr, bot->face_mode);
	}
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_vls_printf(logstr, " N {");
	    for (i = 0; i < bot->num_normals; i++) {
		bu_vls_printf(logstr, " { %.25G %.25G %.25G }", V3ARGS(&bot->normals[i*3]));
	    }
	    bu_vls_printf(logstr, "} fn {");
	    for (i = 0; i < bot->num_face_normals; i++) {
		bu_vls_printf(logstr, " { %d %d %d }", V3ARGS(&bot->face_normals[i*3]));
	    }
	    bu_vls_printf(logstr, "}");
	}
	status = BRLCAD_OK;
    } else {
	if (attr[0] == 'N') {
	    if (attr[1] == '\0') {
		if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_normals < 1) {
		    bu_vls_strcat(logstr, "{}");
		} else {
		    for (i = 0; i < bot->num_normals; i++) {
			bu_vls_printf(logstr, " { %.25G %.25G %.25G }", V3ARGS(&bot->normals[i*3]));
		    }
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_normals) {
		    bu_vls_printf(logstr, "Specified normal index [%ld] is out of range", li);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%.25G %.25G %.25G", V3ARGS(&bot->normals[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (!bu_strncmp(attr, "fn", 2)) {
	    if (attr[2] == '\0') {
		for (i = 0; i < bot->num_face_normals; i++) {
		    bu_vls_printf(logstr, " { %d %d %d }", V3ARGS(&bot->face_normals[i*3]));
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[2]);
		if (li < 0 || (size_t)li >= bot->num_face_normals) {
		    bu_vls_printf(logstr, "Specified face index [%ld] is out of range", li);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%d %d %d", V3ARGS(&bot->face_normals[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (BU_STR_EQUAL(attr, "nn")) {
	    if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_normals < 1) {
		bu_vls_strcat(logstr, "0");
	    } else {
		bu_vls_printf(logstr, "%lu", (long unsigned)bot->num_normals);
	    }
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "nfn")) {
	    if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_face_normals < 1) {
		bu_vls_strcat(logstr, "0");
	    } else {
		bu_vls_printf(logstr, "%lu", (long unsigned)bot->num_face_normals);
	    }
	    status = BRLCAD_OK;
	} else if (!bu_strncmp(attr, "fm", 2)) {
	    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		bu_vls_strcat(logstr, "Only plate mode BOTs have face_modes");
		status = BRLCAD_ERROR;
	    } else {
		if (attr[2] == '\0') {
		    bu_bitv_to_hex(logstr, bot->face_mode);
		    status = BRLCAD_OK;
		} else {
		    li = atol(&attr[2]);
		    if (li < 0 || (size_t)li >= bot->num_faces) {
			bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
			status = BRLCAD_ERROR;
		    } else {
			bu_vls_printf(logstr, "%s",
				      los[BU_BITTEST(bot->face_mode, li)?1:0]);
			status = BRLCAD_OK;
		    }
		}
	    }
	} else if (attr[0] == 'V') {
	    if (attr[1] != '\0') {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_vertices) {
		    bu_vls_printf(logstr, "vertex number [%ld] out of range (0..%zu)", li, bot->num_vertices-1);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%.25G %.25G %.25G",
				  V3ARGS(&bot->vertices[li*3]));
		    status = BRLCAD_OK;
		}
	    } else {
		for (i = 0; i < bot->num_vertices; i++)
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  V3ARGS(&bot->vertices[i*3]));
		status = BRLCAD_OK;
	    }
	} else if (attr[0] == 'F') {
	    /* Retrieve one face, as vertex indices */
	    if (attr[1] == '\0') {
		for (i = 0; i < bot->num_faces; i++)
		    bu_vls_printf(logstr, " { %d %d %d }",
				  V3ARGS(&bot->faces[i*3]));
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%d %d %d",
				  V3ARGS(&bot->faces[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (BU_STR_EQUAL(attr, "flags")) {
	    bu_vls_printf(logstr, "{");
	    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		bu_vls_printf(logstr, " has_normals");
	    }
	    if (bot->bot_flags & RT_BOT_USE_NORMALS) {
		bu_vls_printf(logstr, " use_normals");
	    }
	    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
		bu_vls_printf(logstr, " use_floats");
	    }
	    bu_vls_printf(logstr, "}");
	    status = BRLCAD_OK;
	} else if (attr[0] == 'f') {
	    size_t indx;
	    /* Retrieve one face, as list of 3 3tuple coordinates */
	    if (attr[1] == '\0') {
		for (i = 0; i < bot->num_faces; i++) {
		    indx = bot->faces[i*3+0];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[i*3+1];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[i*3+2];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    status = BRLCAD_ERROR;
		} else {
		    indx = bot->faces[li*3+0];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[li*3+1];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[li*3+2];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    status = BRLCAD_OK;
		}
	    }
	} else if (attr[0] == 'T') {
	    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		bu_vls_strcat(logstr, "Only plate mode BOTs have thicknesses");
		status = BRLCAD_ERROR;
	    } else {
		if (attr[1] == '\0') {
		    for (i = 0; i < bot->num_faces; i++)
			bu_vls_printf(logstr, " %.25G", bot->thickness[i]);
		    status = BRLCAD_OK;
		} else {
		    li = atol(&attr[1]);
		    if (li < 0 || (size_t)li >= bot->num_faces) {
			bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
			status = BRLCAD_ERROR;
		    } else {
			bu_vls_printf(logstr, " %.25G", bot->thickness[li]);
			status = BRLCAD_OK;
		    }
		}
	    }
	} else if (BU_STR_EQUAL(attr, "nv")) {
	    bu_vls_printf(logstr, "%zu", bot->num_vertices);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "nt")) {
	    bu_vls_printf(logstr, "%zu", bot->num_faces);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "mode")) {
	    bu_vls_printf(logstr, "%s", modes[bot->mode]);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "orient")) {
	    bu_vls_printf(logstr, "%s", orientation[bot->orientation]);
	    status = BRLCAD_OK;
	} else {
	    bu_vls_printf(logstr, "BoT has no attribute '%s'", attr);
	    status = BRLCAD_ERROR;
	}
    }

    return status;
}


int
bot_check_vertex_indices(struct bu_vls *logstr, struct rt_bot_internal *bot)
{
    size_t badVertexCount = 0;
    size_t i;
    for (i = 0; i < bot->num_faces; i++) {
	int k;
	for (k = 0; k < 3; k++) {
	    int vertex_no = bot->faces[i*3+k];
	    if (vertex_no < 0 || (size_t)vertex_no >= bot->num_vertices) {
		bu_vls_printf(logstr, "WARNING: BOT has illegal vertex index (%d) in face #(%zu)\n", vertex_no, i);
		badVertexCount++;
	    }
	}
    }
    return badVertexCount;
}


/**
 * Examples -
 * db adjust name fm		set los facemode bit vector
 * db adjust name fm#		set los face mode of face # (center, append)
 * db adjust name V		set coords for all vertices
 * db adjust name V#		set coords for vertex #
 * db adjust name F		set vertex indices for all faces
 * db adjust name F#		set vertex indices for face #
 * db adjust name T		set thickness for all faces
 * db adjust name T#		set thickness for face #
 * db adjust name N		set list of normals
 * db adjust name N#		set coords for normal #
 * db adjust name fn		set list indices into normal vectors for all faces
 * db adjust name fn#		set list indices into normal vectors for face #
 * db adjust name nn		set num_normals
 * db adjust name mode		set mode (surf, volume, plate, plane_nocos)
 * db adjust name orient	set orientation (no, rh, lh)
 * db adjust name flags		set flags
 */
int
rt_bot_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_bot_internal *bot;
    int len;
    size_t i;
    long li;

    RT_CK_DB_INTERNAL(intern);
    bot = (struct rt_bot_internal *)intern->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    while (argc >= 2) {
	if (!bu_strncmp(argv[0], "fm", 2)) {
	    if (argv[0][2] == '\0') {
		if (bot->face_mode)
		    bu_free(bot->face_mode, "bot->face_mode");
		bot->face_mode = bu_hex_to_bitv(argv[1]);
	    } else {
		li = atol(&(argv[0][2]));
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    return BRLCAD_ERROR;
		}

		if (isdigit((int)*argv[1])) {
		    if (atol(argv[1]) == 0)
			BU_BITCLR(bot->face_mode, li);
		    else
			BU_BITSET(bot->face_mode, li);
		} else if (BU_STR_EQUAL(argv[1], "append"))
		    BU_BITSET(bot->face_mode, li);
		else
		    BU_BITCLR(bot->face_mode, li);
	    }
	} else if (BU_STR_EQUAL(argv[0], "nn")) {
	    long new_num = 0;
	    size_t old_num = bot->num_normals;

	    new_num = atol(argv[1]);
	    if (new_num < 0) {
		bu_vls_printf(logstr, "Number of normals [%ld] may not be less than 0", new_num);
		return BRLCAD_ERROR;
	    }

	    if (new_num == 0) {
		bot->num_normals = 0;
		if (bot->normals) {
		    bu_free(bot->normals, "BOT normals");
		    bot->normals = (fastf_t *)NULL;
		}
		bot->num_normals = 0;
	    } else {
		if ((size_t)new_num != old_num) {
		    bot->num_normals = (size_t)new_num;
		    if (bot->normals) {
			bot->normals = (fastf_t *)bu_realloc((char *)bot->normals, bot->num_normals * 3 * sizeof(fastf_t), "BOT normals");
		    } else {
			bot->normals = (fastf_t *)bu_calloc(bot->num_normals * 3, sizeof(fastf_t), "BOT normals");
		    }

		    if ((size_t)new_num > old_num) {
			for (i = old_num; i < (size_t)new_num; i++) {
			    VSET(&bot->normals[i*3], 0, 0, 0);
			}
		    }
		}

	    }

	} else if (!bu_strncmp(argv[0], "fn", 2)) {
	    char *f_str;

	    if (argv[0][2] == '\0') {
		const char **obj_array = NULL;
		if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		    bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		    return BRLCAD_ERROR;
		}
		if ((size_t)len != bot->num_faces || len <= 0) {
		    bu_vls_printf(logstr, "Only %d face normals? Must provide normals for all faces.", len);
		    if (obj_array) bu_free((char *)obj_array, "obj_array");
		    return BRLCAD_ERROR;
		}
		if (bot->face_normals)
		    bu_free(bot->face_normals, "BOT face_normals");
		bot->face_normals = (int *)bu_calloc(len*3, sizeof(int), "BOT face_normals");
		bot->num_face_normals = len;
		for (i = 0; i < (size_t)len; i++) {
		    f_str = (char *)obj_array[i];
		    while (isspace((int)*f_str)) f_str++;

		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    /* normal [X, y, z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->face_normals[i*3+0] = (size_t)li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    /* normal [x, Y, z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    bot->face_normals[i*3+1] = li;
		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    /* normal [x, y, Z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->face_normals[i*3+2] = li;
		}
		bu_free((char *)obj_array, "obj_array");
		bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	    } else {
		li = atol(&argv[0][2]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face_normal number [%ld] out of range!!!", li);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		f_str = (char *)argv[1];
		while (isspace((int)*f_str)) f_str++;

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+0] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+1] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+2] = li;
	    }
	} else if (argv[0][0] == 'N') {
	    char *v_str;

	    if (argv[0][1] == '\0') {
		const char **obj_array = NULL;
		if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		    bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		    return BRLCAD_ERROR;
		}
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one normal.");
		    return BRLCAD_ERROR;
		}
		bot->num_normals = len;
		if (bot->normals) {
		    bu_free(bot->normals, "BOT normals");
		    bot->normals = (fastf_t *)NULL;
		}
		bot->num_normals = 0;
		bot->normals = (fastf_t *)bu_calloc(len*3, sizeof(fastf_t), "BOT normals");
		for (i = 0; i < (size_t)len; i++) {
		    v_str = (char *)obj_array[i];
		    while (isspace((int)*v_str)) v_str++;
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+0] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+1] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+2] = atof(v_str);
		}
		bu_free((char *)obj_array, "obj_array");
		bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_normals) {
		    bu_vls_printf(logstr, "normal number [%ld] out of range!!!", li);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		v_str = (char *)argv[1];
		while (isspace((int)*v_str)) v_str++;

		bot->normals[i*3+0] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete normal");
		    return BRLCAD_ERROR;
		}
		bot->normals[i*3+1] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete normal");
		    return BRLCAD_ERROR;
		}
		bot->normals[i*3+2] = atof(v_str);
	    }
	} else if (argv[0][0] == 'V') {
	    char *v_str;

	    if (argv[0][1] == '\0') {
		const char **obj_array = NULL;
		if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		    bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		    return BRLCAD_ERROR;
		}
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one vertex.");
		    return BRLCAD_ERROR;
		}
		bot->num_vertices = len;
		if (bot->vertices)
		    bu_free(bot->vertices, "BOT vertices");
		bot->vertices = (fastf_t *)bu_calloc(len*3, sizeof(fastf_t), "BOT vertices");
		for (i = 0; i < (size_t)len; i++) {
		    v_str = (char *)obj_array[i];
		    while (isspace((int)*v_str)) v_str++;
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+0] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+1] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+2] = atof(v_str);
		}
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_vertices) {
		    bu_vls_printf(logstr, "vertex number [%ld] out of range!!!", li);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		v_str = (char *)argv[1];
		while (isspace((int)*v_str)) v_str++;

		bot->vertices[i*3+0] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}
		bot->vertices[i*3+1] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}
		bot->vertices[i*3+2] = atof(v_str);
	    }
	} else if (argv[0][0] == 'F') {
	    char *f_str;

	    if (argv[0][1] == '\0') {
		const char **obj_array = NULL;
		if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		    bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		    return BRLCAD_ERROR;
		}
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one face.");
		    return BRLCAD_ERROR;
		}
		bot->num_faces = len;
		if (bot->faces)
		    bu_free(bot->faces, "BOT faces");
		bot->faces = (int *)bu_calloc(len*3, sizeof(int), "BOT faces");
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    if (!bot->face_normals) {
			bot->face_normals = (int *)bu_malloc(bot->num_faces * 3 * sizeof(int), "bot->face_normals");
			bot->num_face_normals = bot->num_faces;
			for (i = 0; i < bot->num_face_normals; i++) {
			    VSETALL(&bot->face_normals[i*3], -1);
			}
		    } else if (bot->num_face_normals < bot->num_faces) {
			bot->face_normals = (int *)bu_realloc(bot->face_normals, bot->num_faces * 3 * sizeof(int), "bot->face_normals");
			for (i = bot->num_face_normals; i < bot->num_faces; i++) {
			    VSETALL(&bot->face_normals[i*3], -1);
			}
			bot->num_face_normals = bot->num_faces;
		    }
		}
		for (i = 0; i < (size_t)len; i++) {
		    f_str = (char *)obj_array[i];
		    while (isspace((int)*f_str)) f_str++;

		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+0] = li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+1] = li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			bu_free((char *)obj_array, "obj_array");
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+2] = li;
		}
		bu_free((char *)obj_array, "obj_array");
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range!!!", li);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		f_str = (char *)argv[1];
		while (isspace((int)*f_str)) f_str++;

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+0] = li;

		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+1] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+2] = li;
	    }
	    bot_check_vertex_indices(logstr, bot);
	} else if (argv[0][0] ==  'T') {
	    char *t_str;

	    if (argv[0][1] == '\0') {
		const char **obj_array = NULL;
		if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		    bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		    return BRLCAD_ERROR;
		}
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one thickness.");
		    return BRLCAD_ERROR;
		}
		if ((size_t)len > bot->num_faces) {
		    bu_vls_printf(logstr, "Too many thicknesses (there are not that many faces)!!!");
		    bu_free((char *)obj_array, "obj_array");
		    return BRLCAD_ERROR;
		}
		if (!bot->thickness) {
		    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t),
							  "bot->thickness");
		}
		for (i = 0; i < (size_t)len; i++) {
		    bot->thickness[i] = atof(obj_array[i]);
		}
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range!!!", li);
		    return BRLCAD_ERROR;
		}
		if (!bot->thickness) {
		    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t),
							  "bot->thickness");
		}
		t_str = (char *)argv[1];
		bot->thickness[li] = atof(t_str);
	    }
	} else if (BU_STR_EQUAL(argv[0], "mode")) {
	    char *m_str;

	    m_str = (char *)argv[1];
	    if (isdigit((int)*m_str)) {
		int mode;

		mode = atoi(m_str);
		if (mode < RT_BOT_SURFACE || mode > RT_BOT_PLATE_NOCOS) {
		    bu_vls_printf(logstr, "unrecognized mode!!!");
		    return BRLCAD_ERROR;
		}
		bot->mode = mode;
	    } else {
		if (!bu_strncmp(m_str, modes[RT_BOT_SURFACE], 4))
		    bot->mode = RT_BOT_SURFACE;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_SOLID]))
		    bot->mode = RT_BOT_SOLID;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_PLATE]))
		    bot->mode = RT_BOT_PLATE;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_PLATE_NOCOS]))
		    bot->mode = RT_BOT_PLATE_NOCOS;
		else {
		    bu_vls_printf(logstr, "unrecognized mode!!!");
		    return BRLCAD_ERROR;
		}
	    }
	} else if (!bu_strncmp(argv[0], "orient", 6)) {
	    char *o_str;

	    o_str = (char *)argv[1];
	    if (isdigit((int)*o_str)) {
		int orient;

		orient = atoi(o_str);
		if (orient < RT_BOT_UNORIENTED || orient > RT_BOT_CW) {
		    bu_vls_printf(logstr, "unrecognized orientation!!!");
		    return BRLCAD_ERROR;
		}
		bot->orientation = orient;
	    } else {
		if (BU_STR_EQUAL(o_str, orientation[RT_BOT_UNORIENTED]))
		    bot->orientation = RT_BOT_UNORIENTED;
		else if (BU_STR_EQUAL(o_str, orientation[RT_BOT_CCW]))
		    bot->orientation = RT_BOT_CCW;
		else if (BU_STR_EQUAL(o_str, orientation[RT_BOT_CW]))
		    bot->orientation = RT_BOT_CW;
		else {
		    bu_vls_printf(logstr, "unrecognized orientation!!!");
		    return BRLCAD_ERROR;
		}
	    }
	} else if (BU_STR_EQUAL(argv[0], "flags")) {
	    const char **obj_array = NULL;
	    if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&obj_array) != 0) {
		bu_vls_printf(logstr, "tcl list parse error: %d.", len);
		return BRLCAD_ERROR;
	    }
	    bot->bot_flags = 0;
	    for (i = 0; i < (size_t)len; i++) {
		char *str;

		str = (char *)obj_array[i];
		if (BU_STR_EQUAL(str, "has_normals")) {
		    bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
		} else if (BU_STR_EQUAL(str, "use_normals")) {
		    bot->bot_flags |= RT_BOT_USE_NORMALS;
		} else if (BU_STR_EQUAL(str, "use_floats")) {
		    bot->bot_flags |= RT_BOT_USE_FLOATS;
		} else {
		    bu_vls_printf(logstr, "unrecognized flag (must be \"has_normals\", \"use_normals\", or \"use_floats\"!!!");
		    if (obj_array) bu_free((char *)obj_array, "obj_array");
		    return BRLCAD_ERROR;
		}
	    }
	    if (obj_array) bu_free((char *)obj_array, "obj_array");
	}

	argc -= 2;
	argv += 2;
    }

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	if (!bot->thickness)
	    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	if (!bot->face_mode) {
	    bot->face_mode = bu_bitv_new(bot->num_faces);
	}
    } else {
	if (bot->thickness) {
	    bu_free(bot->thickness, "BOT thickness");
	    bot->thickness = (fastf_t *)NULL;
	}
	if (bot->face_mode) {
	    bu_free(bot->face_mode, "BOT facemode");
	    bot->face_mode = (struct bu_bitv *)NULL;
	}
    }

    return BRLCAD_OK;
}


int
rt_bot_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr,
		  "mode {%%s} orient {%%s} V { {%%f %%f %%f} {%%f %%f %%f} ...} F { {%%lu %%lu %%lu} {%%lu %%lu %%lu} ...} T { %%f %%f %%f ... } fm %%s");

    return BRLCAD_OK;
}


int
rt_bot_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/*************************************************************************
 *
 * BoT support routines used by MGED, converters, etc.
 *
 *************************************************************************/


/**
 * Routine for finding the smallest edge length in a BoT.
 */
float
minEdge(struct rt_bot_internal *bot)
{
    size_t i;
    size_t numVerts;
    fastf_t epsilon = 1e-10;
    fastf_t *vertices;
    fastf_t currMag, minMag = MAX_FASTF;
    struct bot_edge *edge, *tmp;
    struct bot_edge **edges = NULL;
    vect_t start, curr;

    RT_BOT_CK_MAGIC(bot);

    numVerts = bot->num_vertices;
    vertices = bot->vertices;

    /* build edge list */
    bot_edge_table(bot, &edges);

    /* for each vertex */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	/* make sure this list exists */
	if (edge == (struct bot_edge*)NULL) {
	    continue;
	}

	/* starting vertex for these edges */
	VMOVE(start, &vertices[i]);

	/* for each edge beginning with this index */
	while (edge != (struct bot_edge*)NULL) {

	    /* calculate edge vector using current end vertex */
	    VSUB2(curr, start, &vertices[edge->v]);

	    /* see if it has the smallest magnitude so far */
	    currMag = MAGSQ(curr);

	    if (currMag < minMag && currMag > epsilon) {
		minMag = currMag;
	    }

	    edge = edge->next;
	}
    }

    /* free table memory */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	while (edge != (struct bot_edge*)NULL) {
	    tmp = edge;
	    edge = edge->next;
	    bu_free(tmp, "struct bot_edge");
	}
    }
    bu_free(edges, "bot edge table");
    edges = NULL;

    return sqrt(minMag);
}


/**
 * Routine for finding the largest edge length in a BoT.
 */
float
maxEdge(struct rt_bot_internal *bot)
{
    size_t i;
    size_t numVerts;
    fastf_t *vertices;
    fastf_t currMag, maxMag = 0.0;
    struct bot_edge *edge, *tmp;
    struct bot_edge **edges = NULL;
    vect_t start, curr;

    RT_BOT_CK_MAGIC(bot);

    numVerts = bot->num_vertices;
    vertices = bot->vertices;

    /* build edge list */
    bot_edge_table(bot, &edges);

    /* for each vertex */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	/* make sure this list exists */
	if (edge == (struct bot_edge*)NULL) {
	    continue;
	}

	/* starting vertex for these edges */
	VMOVE(start, &vertices[i]);

	/* for each edge beginning with this index */
	while (edge != (struct bot_edge*)NULL) {

	    /* calculate edge vector using current end vertex */
	    VSUB2(curr, start, &vertices[edge->v]);

	    /* see if it has the largest magnitude so far */
	    currMag = MAGSQ(curr);

	    if (currMag > maxMag) {
		maxMag = currMag;
	    }

	    edge = edge->next;
	}
    }

    /* free table memory */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	while (edge != (struct bot_edge*)NULL) {
	    tmp = edge;
	    edge = edge->next;
	    bu_free(tmp, "struct bot_edge");
	}
    }
    bu_free(edges, "bot edge table");
    edges = NULL;

    return sqrt(maxMag);
}


/**
 * RT_BOT_PROPGET
 *
 * Command used to query BoT property values. Returns parseable
 * values rather than formatted strings.
 *
 * Returns -1 on error.
 */
fastf_t
rt_bot_propget(struct rt_bot_internal *bot, const char *property)
{
    size_t len;

    RT_BOT_CK_MAGIC(bot);

    len = strlen(property);

    /* return value of requested property */
    if (bu_strncmp(property, "faces", len) == 0) {
	return bot->num_faces;
    }
    else if (bu_strncmp(property, "orientation", len) == 0) {
	return bot->orientation;
    }
    else if (bu_strncmp(property, "type", len) == 0 ||
	     bu_strncmp(property, "mode", len) == 0)
    {
	return bot->mode;
    }
    else if (bu_strncmp(property, "vertices", len) == 0) {
	return bot->num_vertices;
    }
    else if (bu_strncmp(property, "minEdge", len) == 0 ||
	     bu_strncmp(property, "minedge", len) == 0)
    {
	return minEdge(bot);
    }
    else if (bu_strncmp(property, "maxEdge", len) == 0 ||
	     bu_strncmp(property, "maxedge", len) == 0)
    {
	return maxEdge(bot);
    }

    return -1;
}


/**
 * This routine adjusts the vertex pointers in each face so that
 * pointers to duplicate vertices end up pointing to the same vertex.
 * The unused vertices are removed and the resulting bot is condensed.
 * Returns the number of vertices fused.
 */
int
rt_bot_vertex_fuse(struct rt_bot_internal *bot, const struct bn_tol *tol)
{
#if 1
    /* THE OLD WAY .. possibly O(n^3) with the vertex shifting */

    size_t i, j, k;
    size_t count = 0;

    RT_BOT_CK_MAGIC(bot);

    for (i = 0; i < bot->num_vertices; i++) {
	j = i + 1;
	while (j < bot->num_vertices) {
	    if (bg_pnt3_pnt3_equal(&bot->vertices[i*3], &bot->vertices[j*3], tol)) {
		count++;

		/* update bot */
		bot->num_vertices--;

		/* shift vertices down */
		for (k = j; k < bot->num_vertices; k++)
		    VMOVE(&bot->vertices[k*3], &bot->vertices[(k+1)*3]);

		/* update face references */
		for (k = 0; k < bot->num_faces*3; k++) {
		    if ((size_t)bot->faces[k] == j) {
			bot->faces[k] = i;
		    } else if ((size_t)bot->faces[k] > j)
			bot->faces[k]--;
		}
	    } else {
		j++;
	    }
	}
    }
#else

    /* THE NEW WAY .. possibly O(n) with basic bin sorting */

    size_t i, j, k;
    size_t count = 0;

    long slot;
    size_t total = 0;
    long *bin[256];
    size_t bin_capacity[256];
    size_t bin_todonext[256];
    const int DEFAULT_CAPACITY = 32;
    fastf_t min_xval = (fastf_t)LONG_MAX;
    fastf_t max_xval = (fastf_t)LONG_MIN;
    fastf_t delta = (fastf_t)0.0;

    vect_t deleted;
    VSETALL(deleted, INFINITY);

    RT_BOT_CK_MAGIC(bot);

    /* initialize a simple 256-slot integer bin space partitioning */
    for (slot = 0; slot < 256; slot++) {
	bin_todonext[slot] = 0;
	bin_capacity[slot] = DEFAULT_CAPACITY;
	bin[slot] = bu_calloc(DEFAULT_CAPACITY, sizeof(long *), "vertices bin");
    }

    /* first pass to get the range of vertex values */
    for (i = 0; i < bot->num_vertices; i++) {
	/* bins are assigned based on X value */
	if ((&bot->vertices[i*3])[X] < min_xval)
	    min_xval = (&bot->vertices[i*3])[X];
	if ((&bot->vertices[i*3])[X] > max_xval)
	    max_xval = (&bot->vertices[i*3])[X];

	/* sanity to make sure our book-keeping doesn't go haywire */
	if (ZERO((&bot->vertices[i*3])[X] - deleted[X])) {
	    bu_log("WARNING: Unable to fuse due to vertex with infinite value (idx=%ld)\n", i);
	    return 0;
	}
    }
    /* sanity swap */
    if (min_xval > max_xval) {
	fastf_t t;
	t = min_xval;
	min_xval = max_xval;
	max_xval = t;
    }
    /* range sanity */
    if (max_xval > (fastf_t)LONG_MAX) {
	max_xval = (fastf_t)LONG_MAX;
    }
    if (min_xval < (fastf_t)LONG_MIN) {
	min_xval = (fastf_t)LONG_MIN;
    }
    if (ZERO(max_xval - min_xval)) {
	if (ZERO(max_xval - (fastf_t)LONG_MAX)) {
	    max_xval += VDIVIDE_TOL;
	} else {
	    min_xval -= VDIVIDE_TOL;
	}
    }

    /* calculate the width of a bin */
    delta = fabs(max_xval - min_xval) / (fastf_t)256.0;
    if (ZERO(delta))
	delta = (fastf_t)1.0;

    /* second pass to sort the vertices into bins based on their X value */
    for (i = 0; i < bot->num_vertices; i++) {
	if ((&bot->vertices[i*3])[X] > (fastf_t)LONG_MAX) {
	    /* exceeds our range, put in last bin */
	    slot = 255;
	} else if ((&bot->vertices[i*3])[X] < (fastf_t)LONG_MIN) {
	    /* exceeds our range, put in first bin */
	    slot = 0;
	} else {
	    /* bins are assigned based on X value */
	    slot = (long)(((&bot->vertices[i*3])[X] - min_xval) / delta);
	}

	/* extra sanity that we don't imagine non-existent bins */
	if (slot < 0) {
	    slot = 0;
	} else if (slot > 255) {
	    slot = 255;
	}

	if (bin_todonext[slot] + 1 > bin_capacity[slot]) {

/* bu_log("increasing %i from capacity %ld given next is %ld\n", slot, bin_capacity[slot], bin_todonext[slot]); */

	    BU_ASSERT(bin_capacity[slot] < LONG_MAX / 2);

	    bin[slot] = bu_realloc(bin[slot], bin_capacity[slot] * 2 * sizeof(int), "increase vertices bin");
	    bin_capacity[slot] *= 2;

	    /* init to zero for sanity */
	    for (j = bin_todonext[slot]+1; j < bin_capacity[slot]; j++) {
		bin[slot][j] = 0;
	    }
	}

/* bu_log("setting bin[%lu][%lu] = %ld\n", slot, bin_todonext[slot], i); */

	bin[slot][bin_todonext[slot]++] = i;
    }

    /* third pass to check the vertices in each bin */
    for (slot = 0; slot < 256; slot++) {

	/* iterate over all vertices in this bin */
	for (i = 0; i < bin_todonext[slot]; i++) {

	    /* compare to the other vertices in this bin */
	    for (j = i + 1; j < bin_todonext[slot]; j++) {

		/* specifically not using tolerances here (except underlying representation tolerance) */
		if (VEQUAL(&bot->vertices[bin[slot][i]*3], &bot->vertices[bin[slot][j]*3])) {
		    count++;

		    /*  update face references */
		    for (k = 0; k < bot->num_faces*3; k++) {
			if (bot->faces[k] == bin[slot][j]) {
			    bot->faces[k] = bin[slot][i];
			}
		    }

		    /* wipe out the vertex marking it for cleanup later */
		    VMOVE(&bot->vertices[bin[slot][j]*3], deleted);
		}
	    }
	}
    }

    /* clean up and compress */
    k = rt_bot_condense(bot);
    if (k < count) {
	bu_log("WARNING: Condensed fewer vertices than expected (%ld < %ld)\n", k, count);
    }

    /* sanity check, there should be no deleted vertices */
    for (i = 0; i < bot->num_vertices; i++) {
	if (VEQUAL(&bot->vertices[i*3], deleted)) {
	    bu_bomb("INTERNAL ERROR: encountered unexpected state during vertex fusing\n");
	}
    }

    /* clear and release the memory for our integer bin space partitioning */
    for (slot = 0; slot < 256; slot++) {
	total += bin_todonext[slot];
/*	bu_log("[%lu]: %ld (of %ld)\n", slot, bin_todonext[slot], bin_capacity[slot]); */
	bu_free(bin[slot], "vertices bin");
	bin_capacity[slot] = bin_todonext[slot] = 0;
    }
    memset(bin, 0, 256 * sizeof(long *));

/*    bu_log("sorted %lu of %lu vertices\n", total, bot->num_vertices); */
#endif

    return count;
}


int
rt_bot_same_orientation(const int *a, const int *b)
{
    int i;

    for (i = 0; i < 3; i++) {
	if (a[0] == b[i]) {
	    i++;
	    if (i == 3)
		i = 0;
	    if (a[1] == b[i])
		return 1;
	    else
		return 0;
	}
    }

    return 0;
}


int
rt_bot_face_fuse(struct rt_bot_internal *bot)
{
    size_t num_faces;
    size_t i, j, k, l;
    int count = 0;

    RT_BOT_CK_MAGIC(bot);

    num_faces = bot->num_faces;
    for (i = 0; i < num_faces; i++) {
	j = i + 1;

	while (j<num_faces) {
	    /* each pass through this loop either increments j or
	     * decrements num_faces
	     */
	    int match = 0;
	    int elim;

	    for (k = i * 3; k < (i+1) * 3; k++) {
		for (l = j * 3; l < (j+1) * 3; l++) {
		    if (bot->faces[k] == bot->faces[l]) {
			match++;
			break;
		    }
		}
	    }

	    if (match != 3) {
		j++;
		continue;
	    }

	    /* these two faces have the same vertices */
	    elim = -1;
	    switch (bot->mode) {
		case RT_BOT_PLATE:
		case RT_BOT_PLATE_NOCOS:
		    /* check the face thickness and face mode */
		    if (!ZERO(bot->thickness[i] - bot->thickness[j]) ||
			(BU_BITTEST(bot->face_mode, i)?1:0) != (BU_BITTEST(bot->face_mode, j)?1:0))
			break;
		    /* fall through */
		case RT_BOT_SOLID:
		case RT_BOT_SURFACE:
		    if (bot->orientation == RT_BOT_UNORIENTED) {
			/* faces are identical, so eliminate one */
			elim = j;
		    } else {
			/* need to check orientation */
			if (rt_bot_same_orientation(&bot->faces[i*3], &bot->faces[j*3]))
			    elim = j;
		    }
		    break;
		default:
		    bu_bomb("bot_face_condense: Unrecognized BOT mode!!!\n");
		    break;
	    }

	    if (elim == -1) {
		j++;
		continue;
	    }

	    /* we are eliminating face number "elim" */
	    for (l = elim; l < num_faces - 1; l++) {
		VMOVE(&bot->faces[l*3], &bot->faces[(l+1)*3]);
	    }

	    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
		for (l = elim; l < num_faces - 1; l++) {
		    bot->thickness[l] = bot->thickness[l+1];
		    if (BU_BITTEST(bot->face_mode, l+1))
			BU_BITSET(bot->face_mode, l);
		    else
			BU_BITCLR(bot->face_mode, l);
		}
	    }
	    num_faces--;
	}
    }

    count = bot->num_faces - num_faces;

    if (count) {
	bot->num_faces = num_faces;
	bot->faces = (int *)bu_realloc(bot->faces, num_faces*3*sizeof(int), "BOT faces realloc");
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    struct bu_bitv *new_mode;

	    bot->thickness = (fastf_t *)bu_realloc(bot->thickness, num_faces*sizeof(fastf_t), "BOT thickness realloc");
	    new_mode = bu_bitv_new(num_faces);
	    for (l = 0; l < num_faces; l++) {
		if (BU_BITTEST(bot->face_mode, l))
		    BU_BITSET(new_mode, l);
	    }
	    if (bot->face_mode)
		bu_free(bot->face_mode, "BOT face_mode");
	    bot->face_mode = new_mode;
	}
    }

    return count;
}


/**
 * Get rid of unused vertices
 */
int
rt_bot_condense(struct rt_bot_internal *bot)
{
    size_t i, j, k;
    size_t num_verts;
    size_t dead_verts = 0;
    int *verts;

    RT_BOT_CK_MAGIC(bot);

    num_verts = bot->num_vertices;
    verts = (int *)bu_calloc(num_verts, sizeof(int), "rt_bot_condense: verts");

    /* walk the list of vertices, and mark each one if it is used */

    for (i = 0; i < bot->num_faces*3; i++) {
	j = bot->faces[i];
	if (j >= num_verts) {
	    bu_log("Illegal vertex number %zu, should be 0 through %zu\n", j, num_verts-1);
	    bu_bomb("Illegal vertex number\n");
	}
	verts[j] = 1;
    }

    /* Walk the list of vertices, eliminate each unused vertex by
     * copying the rest of the array downwards
     */
    for (i = 0; i < num_verts-dead_verts; i++) {
	while (!verts[i] && i < num_verts-dead_verts) {
	    dead_verts++;
	    for (j = i; j < num_verts-dead_verts; j++) {
		k = j+1;
		VMOVE(&bot->vertices[j*3], &bot->vertices[k*3]);
		verts[j] = verts[k];
	    }
	    for (j = 0; j < bot->num_faces * 3; j++) {
		if ((size_t)bot->faces[j] >= i)
		    bot->faces[j]--;
	    }
	}
    }

    bu_free((char *)verts, "rt_bot_condense: verts");

    if (!dead_verts) return 0;

    /* Reallocate the vertex array (which should free the space we are
     * no longer using)
     */
    bot->num_vertices -= dead_verts;
    bot->vertices = (fastf_t *)bu_realloc(bot->vertices, bot->num_vertices*3*sizeof(fastf_t), "rt_bot_condense: realloc vertices");

    return dead_verts;
}


int
find_closest_face(fastf_t **centers, int *piece, int *old_faces, size_t num_faces, fastf_t *vertices)
{
    pointp_t v0, v1, v2;
    point_t center;
    size_t i;
    fastf_t one_third = 1.0/3.0;
    fastf_t min_dist;
    size_t min_face=-1;

    if ((*centers) == NULL) {
	/* need to build the centers array */
	(*centers) = (fastf_t *)bu_malloc(num_faces * 3 * sizeof(fastf_t), "center");
	for (i = 0; i < num_faces; i++) {
	    if (old_faces[i*3] == -1) {
		continue;
	    }
	    v0 = &vertices[old_faces[i*3+0]*3];
	    v1 = &vertices[old_faces[i*3+1]*3];
	    v2 = &vertices[old_faces[i*3+2]*3];
	    VADD3(center, v0, v1, v2);
	    VSCALE(&(*centers)[i*3], center, one_third);
	}
    }

    v0 = &vertices[piece[0]*3];
    v1 = &vertices[piece[1]*3];
    v2 = &vertices[piece[2]*3];

    VADD3(center, v0, v1, v2);
    VSCALE(center, center, one_third);

    min_dist = MAX_FASTF;

    for (i = 0; i < num_faces; i++) {
	vect_t diff;
	fastf_t dist;

	if (old_faces[i*3] == -1) {
	    continue;
	}

	VSUB2(diff, center, &(*centers)[i*3]);
	dist = MAGSQ(diff);
	if (dist < min_dist) {
	    min_dist = dist;
	    min_face = i;
	}
    }

    return min_face;
}


void
Add_unique_verts(int *piece_verts, int *v)
{
    int i, j;
    int *ptr=v;

    for (j = 0; j < 3; j++) {
	i = -1;
	while (piece_verts[++i] != -1) {
	    if (piece_verts[i] == (*ptr)) {
		break;
	    }
	}
	if (piece_verts[i] == -1) {
	    piece_verts[i] = (*ptr);
	}
	ptr++;
    }
}


/**
 * This routine sorts the faces of the BOT such that when they are
 * taken in groups of "tris_per_piece", * each group (piece) will
 * consist of adjacent faces
 */
int
rt_bot_sort_faces(struct rt_bot_internal *bot, size_t tris_per_piece)
{
    fastf_t *centers = (fastf_t *)NULL;	/* triangle centers, used when all else fails */
    int *new_faces = (int *)NULL;	/* the sorted list of faces to be attached to the BOT at the end of this routine */
    int *new_norms = (int *)NULL;	/* the sorted list of vertex normals corresponding to the "new_faces" list */
    int *old_faces = (int *)NULL;	/* a copy of the original face list from the BOT */
    int *piece = (int *)NULL;		/* a small face list, for just the faces in the current piece */
    int *piece_norms = (int *)NULL;	/* vertex normals for faces in the current piece */
    int *piece_verts = (int *)NULL;	/* a list of vertices in the current piece (each vertex appears only once) */
    unsigned char *vert_count;		/* an array used to hold the number of piece vertices that appear in each BOT face */
    size_t new_face_count = 0;		/* the current number of faces in the "new_faces" list */
    size_t faces_left = 0;		/* the number of faces in the "old_faces" array that have not yet been used */
    size_t piece_len = 0;		/* the current number of faces in the piece */
    size_t max_verts = 0;		/* the maximum number of piece_verts found in a single unused face */
    size_t i = 0;
    size_t j = 0;

    RT_BOT_CK_MAGIC(bot);

    /* allocate memory for all the data */
    new_faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "new_faces");
    old_faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "old_faces");
    piece = (int *)bu_calloc(tris_per_piece * 3, sizeof(int), "piece");
    vert_count = (unsigned char *)bu_malloc(bot->num_faces * sizeof(unsigned char), "vert_count");
    piece_verts = (int *)bu_malloc((tris_per_piece * 3 + 1) * sizeof(int), "piece_verts");
    centers = (fastf_t *)NULL;

    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	new_norms = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "new_norms");
	piece_norms = (int *)bu_calloc(tris_per_piece * 3, sizeof(int), "piece_norms");
    }

    /* make a copy of the faces list, this list will be modified
     * during the process.
     */
    for (i = 0; i < bot->num_faces * 3; i++) {
	old_faces[i] = bot->faces[i];
    }

    /* process until we have sorted all the faces */
    faces_left = bot->num_faces;
    while (faces_left) {
	size_t cur_face;
	int done_with_piece;

	/* initialize piece_verts */
	for (i = 0; i < tris_per_piece * 3 + 1; i++) {
	    piece_verts[i] = -1;
	}

	/* choose first unused face on the list */
	cur_face = 0;
	while (cur_face < bot->num_faces && old_faces[cur_face*3] == -1) {
	    cur_face++;
	}

	if (cur_face >= bot->num_faces) {
	    /* all faces used, we must be done */
	    break;
	}

	/* copy that face to start the piece */
	VMOVE(piece, &old_faces[cur_face*3]);
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    VMOVE(piece_norms, &bot->face_normals[cur_face*3]);
	}

	/* also copy it to the piece vertex list */
	VMOVE(piece_verts, piece);

	/* mark this face as used */
	VSETALL(&old_faces[cur_face*3], -1);

	/* update counts */
	piece_len = 1;
	faces_left--;

	if (faces_left == 0) {
	    /* handle the case where the first face in a piece is the
	     * only face left.
	     */
	    for (j = 0; j < piece_len; j++) {
		VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
		}
		new_face_count++;
	    }
	    piece_len = 0;

	    /* set flag to skip the loop below */
	    done_with_piece = 1;
	} else {
	    done_with_piece = 0;
	}

	while (!done_with_piece) {
	    size_t max_verts_min;

	    /* count the number of times vertices from the current
	     * piece appear in the remaining faces.
	     */
	    (void)memset(vert_count, '\0', bot->num_faces);
	    max_verts = 0;
	    for (i = 0; i < bot->num_faces; i++) {
		size_t vert_num;
		int v0, v1, v2;

		vert_num = i * 3;
		if (old_faces[vert_num] == -1) {
		    continue;
		}
		v0 = old_faces[vert_num];
		v1 = old_faces[vert_num+1];
		v2 = old_faces[vert_num+2];

		j = (size_t)-1;
		while (piece_verts[++j] != -1) {
		    if (v0 == piece_verts[j]
			|| v1 == piece_verts[j]
			|| v2 == piece_verts[j])
		    {
			vert_count[i]++;
		    }
		}

		if (vert_count[i] > 1) {
		    /* add this face to the piece */
		    VMOVE(&piece[piece_len*3], &old_faces[i*3]);
		    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			VMOVE(&piece_norms[piece_len*3], &bot->face_normals[i*3]);
		    }

		    /* Add its vertices to the list of piece vertices */
		    Add_unique_verts(piece_verts, &old_faces[i*3]);

		    /* mark this face as used */
		    VSETALL(&old_faces[i*3], -1);

		    /* update counts */
		    piece_len++;
		    faces_left--;
		    vert_count[i] = 0;

		    /* check if this piece is done */
		    if (piece_len == tris_per_piece || faces_left == 0) {
			/* copy this piece to the "new_faces" list */
			for (j = 0; j < piece_len; j++) {
			    VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
			    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
				VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
			    }
			    new_face_count++;
			}
			piece_len = 0;
			max_verts = 0;
			done_with_piece = 1;
			break;
		    }
		}
		if (vert_count[i] > max_verts) {
		    max_verts = vert_count[i];
		}
	    }

	    /* set this variable to 2, means look for faces with at
	     * least common edges.
	     */
	    max_verts_min = 2;

	    if (max_verts == 0 && !done_with_piece) {
		/* none of the remaining faces has any vertices in
		 * common with the current piece.
		 */
		size_t face_to_add;

		/* resort to using triangle centers. find the closest
		 * face to the first face in the piece
		 */
		face_to_add = find_closest_face(&centers, piece, old_faces, bot->num_faces, bot->vertices);

		/* Add this face to the current piece */
		VMOVE(&piece[piece_len*3], &old_faces[face_to_add*3]);
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    VMOVE(&piece_norms[piece_len*3], &bot->face_normals[face_to_add*3]);
		}

		/* Add its vertices to the list of piece vertices */
		Add_unique_verts(piece_verts, &old_faces[face_to_add*3]);

		/* mark this face as used */
		VSETALL(&old_faces[face_to_add*3], -1);

		/* update counts */
		piece_len++;
		faces_left--;

		/* check if this piece is done */
		if (piece_len == tris_per_piece || faces_left == 0) {
		    /* copy this piece to the "new_faces" list */
		    for (j = 0; j < piece_len; j++) {
			VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
			if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
			}
			new_face_count++;
		    }
		    piece_len = 0;
		    max_verts = 0;
		    done_with_piece = 1;
		}
	    } else if (max_verts == 1 && !done_with_piece) {
		/* the best we can find is common vertices */
		max_verts_min = 1;
	    } else if (!done_with_piece) {
		/* there are some common edges, so ignore simple shared vertices */
		max_verts_min = 2;
	    }

	    /* now add the faces with the highest counts to the
	     * current piece; do this in a loop that starts by only
	     * accepting the faces with the most vertices in common
	     * with the current piece
	     */
	    while (max_verts >= max_verts_min && !done_with_piece) {
		/* check every face */
		for (i = 0; i < bot->num_faces; i++) {
		    /* if this face has enough vertices in common with
		     * the piece, add it to the piece
		     */
		    if (vert_count[i] == max_verts) {
			VMOVE(&piece[piece_len*3], &old_faces[i*3]);
			if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			    VMOVE(&piece_norms[piece_len*3], &bot->face_normals[i*3]);
			}
			Add_unique_verts(piece_verts, &old_faces[i*3]);
			VSETALL(&old_faces[i*3], -1);

			piece_len++;
			faces_left--;

			/* Check if we are done */
			if (piece_len == tris_per_piece || faces_left == 0) {
			    /* copy this piece to the "new_faces" list */
			    for (j = 0; j < piece_len; j++) {
				VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
				if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
				    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
				}
				new_face_count++;
			    }
			    piece_len = 0;
			    max_verts = 0;
			    done_with_piece = 1;
			    break;
			}
		    }
		}
		max_verts--;
	    }
	}
    }

    bu_free(old_faces, "old_faces");
    bu_free(piece, "piece");
    bu_free(vert_count, "vert_count");
    bu_free(piece_verts, "piece_verts");
    if (centers) {
	bu_free(centers, "centers");
	centers = NULL;
    }

    /* do some checking on the "new_faces" */
    if (new_face_count != bot->num_faces) {
	bu_log("new_face_count = %zu, should be %zu\n", new_face_count, bot->num_faces);
	bu_free(new_faces, "new_faces");
	return 1;
    }

    if (bot->faces)
	bu_free(bot->faces, "bot->faces");

    bot->faces = new_faces;

    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	bu_free(piece_norms, "piece_norms");
	if (bot->face_normals)
	    bu_free(bot->face_normals, "bot->face_normals");
	bot->face_normals = new_norms;
    }

    return 0;
}


static int
bot_smooth_miss(struct application *ap)
{
    if (ap) RT_CK_APPLICATION(ap);
    return 0;
}


static int
bot_smooth_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(seg))
{
    struct partition *pp;
    struct soltab *stp;
    vect_t inormal, onormal;
    vect_t *normals=(vect_t *)ap->a_uptr;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	stp = pp->pt_inseg->seg_stp;
	RT_HIT_NORMAL(inormal, pp->pt_inhit, stp, &(ap->a_ray), pp->pt_inflip);

	stp = pp->pt_outseg->seg_stp;
	RT_HIT_NORMAL(onormal, pp->pt_outhit, stp, &(ap->a_ray), pp->pt_outflip);
	if (pp->pt_inhit->hit_surfno == ap->a_user) {
	    VMOVE(normals[pp->pt_inhit->hit_surfno], inormal);
	    break;
	}
	if (pp->pt_outhit->hit_surfno == ap->a_user) {
	    VMOVE(normals[pp->pt_outhit->hit_surfno], onormal);
	    break;
	}
    }

    return 1;
}


int
rt_bot_smooth(struct rt_bot_internal *bot, const char *bot_name, struct db_i *dbip, fastf_t norm_tol_angle)
{
    int vert_no;
    size_t i, j, k;
    struct rt_i *rtip;
    struct application ap;
    fastf_t normal_dot_tol = 0.0;
    vect_t *normals;
    const double ONE_THIRD = 1.0 / 3.0;

    RT_BOT_CK_MAGIC(bot);

    if (norm_tol_angle < 0.0 || norm_tol_angle > M_PI) {
	bu_log("normal tolerance angle must be from 0 to Pi\n");
	return -2;
    }

    if ((bot->orientation == RT_BOT_UNORIENTED) && (bot->mode != RT_BOT_SOLID)) {
	bu_log("Cannot smooth unoriented BOT primitives unless they are solid objects\n");
	return -3;
    }

    normal_dot_tol = cos(norm_tol_angle);

    if (bot->normals) {
	bu_free(bot->normals, "bot->normals");
	bot->normals = NULL;
    }

    if (bot->face_normals) {
	bu_free(bot->face_normals, "bot->face_normals");
	bot->face_normals = NULL;
    }

    bot->bot_flags &= ~(RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS);
    bot->num_normals = 0;
    bot->num_face_normals = 0;

    /* build an array of surface normals */
    normals = (vect_t *)bu_calloc(bot->num_faces, sizeof(vect_t), "normals");

    if (bot->orientation == RT_BOT_UNORIENTED) {
	/* need to do raytracing, do prepping */
	rtip = rt_new_rti(dbip);

	RT_APPLICATION_INIT(&ap);
	ap.a_rt_i = rtip;
	ap.a_hit = bot_smooth_hit;
	ap.a_miss = bot_smooth_miss;
	ap.a_uptr = (void *)normals;
	if (rt_gettree(rtip, bot_name)) {
	    bu_log("rt_gettree failed for %s\n", bot_name);
	    return -1;
	}
	rt_prep(rtip);

	/* find the surface normal for each face */
	for (i = 0; i < bot->num_faces; i++) {
	    vect_t a, b;
	    vect_t inv_dir;

	    if (bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices)
		continue; /* sanity */

	    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VCROSS(ap.a_ray.r_dir, a, b);
	    VUNITIZE(ap.a_ray.r_dir);

	    /* calculate ray start point */
	    VADD3(ap.a_ray.r_pt, &bot->vertices[bot->faces[i*3]*3],
		  &bot->vertices[bot->faces[i*3+1]*3],
		  &bot->vertices[bot->faces[i*3+2]*3]);
	    VSCALE(ap.a_ray.r_pt, ap.a_ray.r_pt, ONE_THIRD);

	    /* back out to bounding box limits */

	    /* Compute inverse of the direction cosines */
	    VINVDIR(inv_dir, ap.a_ray.r_dir);

	    if (!rt_in_rpp(&ap.a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max)) {
		bu_log("ERROR: Ray missed target!!!\n");
	    }
	    VJOIN1(ap.a_ray.r_pt, ap.a_ray.r_pt, ap.a_ray.r_min, ap.a_ray.r_dir);
	    ap.a_user = i;
	    (void) rt_shootray(&ap);
	}
	rt_free_rti(rtip);
    } else {
	/* calculate normals */
	for (i = 0; i < bot->num_faces; i++) {
	    vect_t a, b;

	    if (bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices)
		continue; /* sanity */

	    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VCROSS(normals[i], a, b);
	    VUNITIZE(normals[i]);
	    if (bot->orientation == RT_BOT_CW) {
		VREVERSE(normals[i], normals[i]);
	    }
	}
    }

    bot->num_normals = bot->num_faces * 3;
    bot->num_face_normals = bot->num_faces;

    bot->normals = (fastf_t *)bu_calloc(bot->num_normals * 3, sizeof(fastf_t), "bot->normals");
    bot->face_normals = (int *)bu_calloc(bot->num_face_normals * 3, sizeof(int), "bot->face_normals");

    /* process each face */
    for (i = 0; i < bot->num_faces; i++) {
	vect_t def_norm; /* default normal for this face */

	VMOVE(def_norm, normals[i]);

	/* process each vertex in his face */
	for (k = 0; k < 3; k++) {
	    vect_t ave_norm = VINIT_ZERO;

	    /* the actual vertex index */
	    vert_no = bot->faces[i*3+k];

	    /* find all the faces that use this vertex */
	    for (j = 0; j < bot->num_faces * 3; j++) {
		if (bot->faces[j] == vert_no) {
		    size_t the_face;

		    the_face = j / 3;

		    /* add all the normals that are within tolerance
		     * this also gets def_norm
		     */
		    if (VDOT(normals[the_face], def_norm) >= normal_dot_tol) {
			VADD2(ave_norm, ave_norm, normals[the_face]);
		    }
		}
	    }
	    VUNITIZE(ave_norm);
	    VMOVE(&bot->normals[(i*3+k)*3], ave_norm);
	    bot->face_normals[i*3+k] = i*3+k;
	}
    }

    bu_free(normals, "normals");
    normals = NULL;

    bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
    bot->bot_flags |= RT_BOT_USE_NORMALS;

    return 0;
}


int
rt_bot_flip(struct rt_bot_internal *bot)
{
    size_t i;
    size_t tmp_index;

    RT_BOT_CK_MAGIC(bot);

    for (i = 0; i < bot->num_faces; ++i) {
	/* Swap any two vertex references. Here we're swapping 1 and 2. */
	tmp_index = bot->faces[i*3+1];
	bot->faces[i*3+1] = bot->faces[i*3+2];
	bot->faces[i*3+2] = tmp_index;
    }

    return 0;
}

/* One common form of incorrect BoT geometry is an inside-out BoT - for
 * example, a sphere where all the triangle normals are pointed in towards the
 * center rather than outward.  This error is visually simple to spot when
 * drawing a shaded view in OpenGL, but harder to see in a data inspection.  We
 * define a test with raytracing to determine whether a given BoT is inside-out
 * or not. */
int
rt_bot_inside_out(struct rt_bot_internal *bot)
{
    int is_flipped = -1;

    if (!bot || bot->mode != RT_BOT_SOLID || bot->orientation == RT_BOT_UNORIENTED)
	return 0;

    point_t bmin, bmax;
    if (bg_trimesh_aabb(&bmin, &bmax, bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices))
	return 0;

    point_t rpnt;
    VADD2SCALE(rpnt, bmin, bmax, 0.5);

    // Move the bbox center point out of the bounding box in the z direction to
    // give us a starting point for the ray guaranteed to be outside the mesh
    double bdiag = DIST_PNT_PNT(bmin, bmax);
    rpnt[Z] += bdiag;

    // We want to shoot an interrogation ray in a direction that gives us an
    // unambiguous result for the first hit point (i.e. a non-grazing hit), but
    // there is no way to know in advance what a "good" direction is.  So
    // instead we iterate over the faces of the mesh and use their center
    // points as targets - sooner or later (most likely sooner) a shotline
    // through one of them will give us our answer.
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t verts[3];
	point_t tcenter;
	VMOVE(verts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(verts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(verts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, verts[0], verts[1], verts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	vect_t rdir;
	VSUB2(rdir, tcenter, rpnt);
	VUNITIZE(rdir);

	// The LIBRT raytracer a) requires an rtip setup using the parent
	// database and b) corrects the normal we want to use for this test
	// anyway, so use the low level bg_isect_tri_ray test and a naive walk
	// over faces.  After we merge in the new high performance BoT
	// raytracing work it will probably make sense to revisit this, but for
	// now this will do the job. The parent loop quits as soon as it can
	// make a determination, so it won't be the full n^2 hit in practice.
	point_t cpnt = {FLT_MAX, FLT_MAX, FLT_MAX};
	double cdsq = DIST_PNT_PNT_SQ(rpnt, cpnt);
	vect_t cnorm = VINIT_ZERO;
	for (size_t j = 0; j < bot->num_faces; j++) {
	    point_t isect;
	    point_t jverts[3];
	    VMOVE(jverts[0], &bot->vertices[bot->faces[j*3+0]*3]);
	    VMOVE(jverts[1], &bot->vertices[bot->faces[j*3+1]*3]);
	    VMOVE(jverts[2], &bot->vertices[bot->faces[j*3+2]*3]);
	    int is_hit = bg_isect_tri_ray(rpnt, rdir, jverts[0], jverts[1], jverts[2], &isect);
	    if (!is_hit)
		continue;

	    double wdsq = DIST_PNT_PNT_SQ(rpnt, isect);
	    if (wdsq < cdsq) {
		VMOVE(cpnt, isect);
		vect_t vnorm, v1, v2;
		VSUB2(v1, jverts[1], jverts[0]);
		VSUB2(v2, jverts[2], jverts[0]);
		VCROSS(vnorm, v1, v2);
		VUNITIZE(vnorm);
		VMOVE(cnorm, vnorm);
	    }
	}

	// If we found a normal that works, set is_flipped
	if (MAGNITUDE(cnorm) > 0.9) {
	    double ndot = VDOT(rdir, cnorm);
	    if (!NEAR_ZERO(ndot, VUNITIZE_TOL))
		is_flipped = (ndot < 0) ? 0 : 1;
	}

	// If we're >= 0, we have our answer
	if (is_flipped != -1)
	    break;
    }

    if (is_flipped == -1)
	return is_flipped;

    return (bot->orientation == RT_BOT_CCW) ? is_flipped : !is_flipped;
}

/*****************************************************************************/


struct tri_edges {
    struct bu_list l;
    size_t edge_1[2];
    size_t edge_2[2];
    size_t edge_3[2];
    size_t tri;
};


struct tri_pts {
    struct bu_list l;
    int a;
    int b;
    int c;
    int tri;
};


void
rt_bot_sync_func(struct rt_bot_internal *bot,
		 struct tri_edges *tep,
		 struct tri_edges *headTep,
		 struct tri_edges *usedTep)
{
    struct tri_edges *neighbor_tep;
    struct tri_edges **stack = (struct tri_edges **)bu_calloc(bot->num_faces, sizeof(struct tri_edges *), "rt_bot_sync_func: stack");
    register size_t si = 0;
    register int not_done = 1;

    while (not_done) {
    begin:
	for (BU_LIST_FOR(neighbor_tep, tri_edges, &headTep->l)) {
	    if ((tep->edge_1[0] == neighbor_tep->edge_1[0] &&
		 tep->edge_1[1] == neighbor_tep->edge_1[1]) ||
		(tep->edge_1[0] == neighbor_tep->edge_2[0] &&
		 tep->edge_1[1] == neighbor_tep->edge_2[1]) ||
		(tep->edge_1[0] == neighbor_tep->edge_3[0] &&
		 tep->edge_1[1] == neighbor_tep->edge_3[1]) ||
		(tep->edge_2[0] == neighbor_tep->edge_1[0] &&
		 tep->edge_2[1] == neighbor_tep->edge_1[1]) ||
		(tep->edge_2[0] == neighbor_tep->edge_2[0] &&
		 tep->edge_2[1] == neighbor_tep->edge_2[1]) ||
		(tep->edge_2[0] == neighbor_tep->edge_3[0] &&
		 tep->edge_2[1] == neighbor_tep->edge_3[1]) ||
		(tep->edge_3[0] == neighbor_tep->edge_1[0] &&
		 tep->edge_3[1] == neighbor_tep->edge_1[1]) ||
		(tep->edge_3[0] == neighbor_tep->edge_2[0] &&
		 tep->edge_3[1] == neighbor_tep->edge_2[1]) ||
		(tep->edge_3[0] == neighbor_tep->edge_3[0] &&
		 tep->edge_3[1] == neighbor_tep->edge_3[1])) {
		/* Found a shared edge of a neighboring triangle whose
		 * orientation needs to be reversed.
		 */
		size_t tmp_index;

		BU_LIST_DEQUEUE(&neighbor_tep->l);
		BU_LIST_APPEND(&usedTep->l, &neighbor_tep->l);

		/* Swap any two vertex references. Here we're swapping 1 and 2. */
		tmp_index = bot->faces[neighbor_tep->tri*3+1];
		bot->faces[neighbor_tep->tri*3+1] = bot->faces[neighbor_tep->tri*3+2];
		bot->faces[neighbor_tep->tri*3+2] = tmp_index;

		/* Also need to reverse the edges in neighbor_tep */
		tmp_index = neighbor_tep->edge_1[0];
		neighbor_tep->edge_1[0] = neighbor_tep->edge_1[1];
		neighbor_tep->edge_1[1] = tmp_index;
		tmp_index = neighbor_tep->edge_2[0];
		neighbor_tep->edge_2[0] = neighbor_tep->edge_2[1];
		neighbor_tep->edge_2[1] = tmp_index;
		tmp_index = neighbor_tep->edge_3[0];
		neighbor_tep->edge_3[0] = neighbor_tep->edge_3[1];
		neighbor_tep->edge_3[1] = tmp_index;

		stack[++si] = tep;
		tep = neighbor_tep;
		goto begin;
	    } else if ((tep->edge_1[0] == neighbor_tep->edge_1[1] &&
			tep->edge_1[1] == neighbor_tep->edge_1[0]) ||
		       (tep->edge_1[0] == neighbor_tep->edge_2[1] &&
			tep->edge_1[1] == neighbor_tep->edge_2[0]) ||
		       (tep->edge_1[0] == neighbor_tep->edge_3[1] &&
			tep->edge_1[1] == neighbor_tep->edge_3[0]) ||
		       (tep->edge_2[0] == neighbor_tep->edge_1[1] &&
			tep->edge_2[1] == neighbor_tep->edge_1[0]) ||
		       (tep->edge_2[0] == neighbor_tep->edge_2[1] &&
			tep->edge_2[1] == neighbor_tep->edge_2[0]) ||
		       (tep->edge_2[0] == neighbor_tep->edge_3[1] &&
			tep->edge_2[1] == neighbor_tep->edge_3[0]) ||
		       (tep->edge_3[0] == neighbor_tep->edge_1[1] &&
			tep->edge_3[1] == neighbor_tep->edge_1[0]) ||
		       (tep->edge_3[0] == neighbor_tep->edge_2[1] &&
			tep->edge_3[1] == neighbor_tep->edge_2[0]) ||
		       (tep->edge_3[0] == neighbor_tep->edge_3[1] &&
			tep->edge_3[1] == neighbor_tep->edge_3[0])) {
		/* Found a shared edge of a neighboring triangle whose
		 * orientation is fine.
		 */

		BU_LIST_DEQUEUE(&neighbor_tep->l);
		BU_LIST_APPEND(&usedTep->l, &neighbor_tep->l);

		stack[++si] = tep;
		tep = neighbor_tep;
		goto begin;
	    }
	}

	if (si < 1)
	    not_done = 0;
	else
	    tep = stack[si--];
    }

    bu_free((void *)stack, "rt_bot_sync_func: stack");
}


int
rt_bot_sync(struct rt_bot_internal *bot)
{
    size_t i;
    struct tri_edges headTep;
    struct tri_edges usedTep;
    struct tri_edges *tep;
    struct tri_edges *alltep;
    size_t pt_A, pt_B, pt_C;

    RT_BOT_CK_MAGIC(bot);

    /* Nothing to do */
    if (bot->num_faces < 2)
	return 0;

    BU_LIST_INIT(&headTep.l);
    BU_LIST_INIT(&usedTep.l);

    alltep = (struct tri_edges *)bu_calloc(bot->num_faces, sizeof(struct tri_edges), "rt_bot_sync: alltep");

    /* Initialize tep list */
    for (i = 0; i < bot->num_faces; ++i) {
	tep = &alltep[i];
	BU_LIST_APPEND(&headTep.l, &tep->l);

	pt_A = bot->faces[i*3+0];
	pt_B = bot->faces[i*3+1];
	pt_C = bot->faces[i*3+2];

	tep->tri = i;
	tep->edge_1[0] = pt_A;
	tep->edge_1[1] = pt_B;
	tep->edge_2[0] = pt_B;
	tep->edge_2[1] = pt_C;
	tep->edge_3[0] = pt_C;
	tep->edge_3[1] = pt_A;
    }

    while (BU_LIST_WHILE(tep, tri_edges, &headTep.l)) {
	BU_LIST_DEQUEUE(&tep->l);
	BU_LIST_APPEND(&usedTep.l, &tep->l);

	rt_bot_sync_func(bot, tep, &headTep, &usedTep);
    }

    while (BU_LIST_WHILE(tep, tri_edges, &usedTep.l)) {
	BU_LIST_DEQUEUE(&tep->l);
    }

    bu_free((void *)alltep, "rt_bot_sync: alltep");

    return 0;
}


void
rt_bot_split_func(struct rt_bot_internal *bot,
		  struct tri_pts *tpp,
		  struct tri_pts *headTpp,
		  struct tri_pts *usedTpp)
{
    struct tri_pts *neighbor_tpp;
    struct tri_pts **stack = (struct tri_pts **)bu_calloc(bot->num_faces, sizeof(struct tri_pts *), "rt_bot_split_func: stack");
    register size_t si = 0;
    register int not_done = 1;

    while (not_done) {
    begin:
	for (BU_LIST_FOR(neighbor_tpp, tri_pts, &headTpp->l)) {
	    if ((tpp->a == neighbor_tpp->a && tpp->b == neighbor_tpp->b) ||
		(tpp->a == neighbor_tpp->b && tpp->b == neighbor_tpp->a) ||
		(tpp->a == neighbor_tpp->b && tpp->b == neighbor_tpp->c) ||
		(tpp->a == neighbor_tpp->c && tpp->b == neighbor_tpp->b) ||
		(tpp->a == neighbor_tpp->a && tpp->b == neighbor_tpp->c) ||
		(tpp->a == neighbor_tpp->c && tpp->b == neighbor_tpp->a) ||
		(tpp->a == neighbor_tpp->a && tpp->c == neighbor_tpp->b) ||
		(tpp->a == neighbor_tpp->b && tpp->c == neighbor_tpp->a) ||
		(tpp->a == neighbor_tpp->b && tpp->c == neighbor_tpp->c) ||
		(tpp->a == neighbor_tpp->c && tpp->c == neighbor_tpp->b) ||
		(tpp->a == neighbor_tpp->a && tpp->c == neighbor_tpp->c) ||
		(tpp->a == neighbor_tpp->c && tpp->c == neighbor_tpp->a) ||
		(tpp->b == neighbor_tpp->a && tpp->c == neighbor_tpp->b) ||
		(tpp->b == neighbor_tpp->b && tpp->c == neighbor_tpp->a) ||
		(tpp->b == neighbor_tpp->b && tpp->c == neighbor_tpp->c) ||
		(tpp->b == neighbor_tpp->c && tpp->c == neighbor_tpp->b) ||
		(tpp->b == neighbor_tpp->a && tpp->c == neighbor_tpp->c) ||
		(tpp->b == neighbor_tpp->c && tpp->c == neighbor_tpp->a)) {
		/* Found a shared edge of a neighboring triangle */

		BU_LIST_DEQUEUE(&neighbor_tpp->l);
		BU_LIST_APPEND(&usedTpp->l, &neighbor_tpp->l);

		stack[++si] = tpp;
		tpp = neighbor_tpp;
		goto begin;
	    }
	}

	if (si < 1)
	    not_done = 0;
	else
	    tpp = stack[si--];
    }

    bu_free((void *)stack, "rt_bot_split_func: stack");
}


#define REMAP_BOT_VERTS(_oldbot, _newbot, _vmap, _vcount, _ovi, _i) { \
	size_t vmi; \
	\
	for (vmi = 0; vmi < _vcount; vmi++) { \
	    if (_ovi == _vmap[vmi]) { \
		_newbot->faces[_i] = vmi; \
		break; \
	    } \
	} \
	\
	if (vmi == _vcount) { \
	    _vmap[_vcount] = _ovi; \
	    _newbot->faces[_i] = _vcount; \
	    VMOVE(&_newbot->vertices[_vcount*3], &_oldbot->vertices[_ovi*3]); \
	    ++_vcount; \
	} \
    }


struct rt_bot_internal *
rt_bot_create(struct rt_bot_internal *bot, struct tri_pts *newTpp)
{
    size_t i;
    struct tri_pts *tpp;
    struct rt_bot_internal *newbot;

    BU_ALLOC(newbot, struct rt_bot_internal);

    newbot->num_faces = 0;
    for (BU_LIST_FOR(tpp, tri_pts, &newTpp->l)) {
	++newbot->num_faces;
    }

    newbot->magic = bot->magic;
    newbot->mode = bot->mode;
    newbot->orientation = bot->orientation;
    newbot->bot_flags = bot->bot_flags;

    {
	size_t vcount;
	int *vmap = (int *)bu_calloc(bot->num_vertices * 3, sizeof(int), "Bot vertices");

	newbot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "Bot vertices");
	newbot->faces = (int *)bu_calloc(newbot->num_faces * 3, sizeof(int), "Bot faces");
	if (bot->mode == RT_BOT_PLATE) {
	    newbot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "Bot thickness");
	    newbot->face_mode = bu_bitv_new(newbot->num_faces);
	}

	i = 0;
	vcount = 0;
	for (BU_LIST_FOR(tpp, tri_pts, &newTpp->l)) {

	    REMAP_BOT_VERTS(bot, newbot, vmap, vcount, tpp->a, i*3);
	    REMAP_BOT_VERTS(bot, newbot, vmap, vcount, tpp->b, i*3+1);
	    REMAP_BOT_VERTS(bot, newbot, vmap, vcount, tpp->c, i*3+2);

	    if (bot->mode == RT_BOT_PLATE) {
		newbot->thickness[i] = bot->thickness[tpp->tri];

		if (BU_BITTEST(bot->face_mode, tpp->tri))
		    BU_BITSET(newbot->face_mode, i);
		/* else already cleared via bu_bitv_new() */
	    }

	    ++i;
	}

	newbot->num_vertices = vcount;
	bu_free(vmap, "rt_bot_create: vmap");
    }

    return newbot;
}


struct rt_bot_list *
rt_bot_split(struct rt_bot_internal *bot)
{
    size_t i;
    size_t first;
    struct tri_pts headTp;
    struct tri_pts usedTp;
    struct tri_pts *tpp;
    struct tri_pts *alltpp;
    struct rt_bot_list *headRblp = (struct rt_bot_list *)0;
    struct rt_bot_list *rblp;

    RT_BOT_CK_MAGIC(bot);

    BU_ALLOC(headRblp, struct rt_bot_list);
    BU_LIST_INIT(&headRblp->l);

    /* Nothing to do */
    if (bot->num_faces < 2)
	return headRblp;

    BU_LIST_INIT(&headTp.l);
    BU_LIST_INIT(&usedTp.l);

    alltpp = (struct tri_pts *)bu_calloc(bot->num_faces, sizeof(struct tri_pts), "rt_bot_split: alltpp");

    /* Initialize tpp list */
    for (i = 0; i < bot->num_faces; ++i) {
	tpp = &alltpp[i];
	BU_LIST_APPEND(&headTp.l, &tpp->l);

	tpp->tri = i;
	tpp->a = bot->faces[i*3+0];
	tpp->b = bot->faces[i*3+1];
	tpp->c = bot->faces[i*3+2];
    }

    first = 1;
    while (BU_LIST_WHILE(tpp, tri_pts, &headTp.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
	BU_LIST_APPEND(&usedTp.l, &tpp->l);

	rt_bot_split_func(bot, tpp, &headTp, &usedTp);

	if (first) {
	    first = 0;

	    if (BU_LIST_NON_EMPTY(&headTp.l)) {
		/* Create a new bot */
		BU_ALLOC(rblp, struct rt_bot_list);
		rblp->bot = rt_bot_create(bot, &usedTp);
		BU_LIST_APPEND(&headRblp->l, &rblp->l);
	    }
	} else {
	    /* Create a new bot */
	    BU_ALLOC(rblp, struct rt_bot_list);
	    rblp->bot = rt_bot_create(bot, &usedTp);
	    BU_LIST_APPEND(&headRblp->l, &rblp->l);
	}

	while (BU_LIST_WHILE(tpp, tri_pts, &usedTp.l)) {
	    BU_LIST_DEQUEUE(&tpp->l);
	}
    }

    bu_free((void *)alltpp, "rt_bot_split: alltpp");

    return headRblp;
}


struct rt_bot_list *
rt_bot_patches(struct rt_bot_internal *bot)
{
    size_t i, j;
    struct tri_pts headTp;
    struct tri_pts xplus;
    struct tri_pts xminus;
    struct tri_pts yplus;
    struct tri_pts yminus;
    struct tri_pts zplus;
    struct tri_pts zminus;
    struct tri_pts *tpp;
    struct tri_pts *alltpp;
    struct rt_bot_list *headRblp = (struct rt_bot_list *)0;
    struct rt_bot_list *rblp;

    vect_t from_xplus = {-1, 0, 0};
    vect_t from_xminus = {1, 0, 0};
    vect_t from_yplus = {0, -1, 0};
    vect_t from_yminus = {0, 1, 0};
    vect_t from_zplus = {0, 0, -1};
    vect_t from_zminus = {0, 0, 1};

    RT_BOT_CK_MAGIC(bot);

    BU_ALLOC(headRblp, struct rt_bot_list);
    BU_LIST_INIT(&headRblp->l);

    /* Nothing to do */
    if (bot->num_faces < 2)
	return NULL;

    BU_LIST_INIT(&headTp.l);
    BU_LIST_INIT(&xplus.l);
    BU_LIST_INIT(&xminus.l);
    BU_LIST_INIT(&yplus.l);
    BU_LIST_INIT(&yminus.l);
    BU_LIST_INIT(&zplus.l);
    BU_LIST_INIT(&zminus.l);

    alltpp = (struct tri_pts *)bu_calloc(bot->num_faces, sizeof(struct tri_pts), "patches alltpp");

    for (i = 0; i < bot->num_faces; ++i) {
	vect_t a, b, norm_dir;
	fastf_t results[6];
	int result_max = 0;
	fastf_t tmp = 0.0;
	VSETALLN(results, 0, 6);

	tpp = &alltpp[i];
	tpp->tri = i;
	tpp->a = bot->faces[i*3+0];
	tpp->b = bot->faces[i*3+1];
	tpp->c = bot->faces[i*3+2];

	VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
	results[0] = VDOT(from_xplus, norm_dir);
	results[1] = VDOT(from_xminus, norm_dir);
	results[2] = VDOT(from_yplus, norm_dir);
	results[3] = VDOT(from_yminus, norm_dir);
	results[4] = VDOT(from_zplus, norm_dir);
	results[5] = VDOT(from_zminus, norm_dir);

	for (j = 0; j < 6; j++) {
	    if (results[j] > tmp) {
		result_max = j;
		tmp = results[j];
	    }
	}


	if (result_max == 0) {
	    BU_LIST_APPEND(&xplus.l, &tpp->l);
	}
	if (result_max == 1) {
	    BU_LIST_APPEND(&xminus.l, &tpp->l);
	}
	if (result_max == 2) {
	    BU_LIST_APPEND(&yplus.l, &tpp->l);
	}
	if (result_max == 3) {
	    BU_LIST_APPEND(&yminus.l, &tpp->l);
	}
	if (result_max == 4) {
	    BU_LIST_APPEND(&zplus.l, &tpp->l);
	}
	if (result_max == 5) {
	    BU_LIST_APPEND(&zminus.l, &tpp->l);
	}

    }
    if (BU_LIST_NON_EMPTY(&xplus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &xplus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }
    if (BU_LIST_NON_EMPTY(&xminus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &xminus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }
    if (BU_LIST_NON_EMPTY(&yplus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &yplus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }
    if (BU_LIST_NON_EMPTY(&yminus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &yminus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }
    if (BU_LIST_NON_EMPTY(&zplus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &zplus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }
    if (BU_LIST_NON_EMPTY(&zminus.l)) {
	/* Create a new bot */
	BU_ALLOC(rblp, struct rt_bot_list);
	rblp->bot = rt_bot_create(bot, &zminus);
	BU_LIST_APPEND(&headRblp->l, &rblp->l);
    }

    while (BU_LIST_WHILE(tpp, tri_pts, &xplus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }

    while (BU_LIST_WHILE(tpp, tri_pts, &xminus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }

    while (BU_LIST_WHILE(tpp, tri_pts, &yplus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }

    while (BU_LIST_WHILE(tpp, tri_pts, &yminus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }


    while (BU_LIST_WHILE(tpp, tri_pts, &zplus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }


    while (BU_LIST_WHILE(tpp, tri_pts, &zminus.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
    }

    bu_free((void *)alltpp, "rt_bot_patches: alltpp");

    return headRblp;
}


void
rt_bot_list_free(struct rt_bot_list *headRblp, int fbflag)
{
    struct rt_bot_list *rblp;

    while (BU_LIST_WHILE(rblp, rt_bot_list, &headRblp->l)) {
	/* Remove from list and free */
	BU_LIST_DEQUEUE(&rblp->l);

	if (fbflag)
	    rt_bot_internal_free(rblp->bot);

	bu_free(rblp, "rt_bot_list_free: rblp");
    }

    bu_free(headRblp, "rt_bot_list_free: headRblp");
}


void
rt_bot_volume(fastf_t *volume, const struct rt_db_internal *ip)
{
    /* contains information used to analyze a polygonal face */
    struct poly_face
    {
	char label[5];
	size_t npts;
	point_t *pts;
	plane_t plane_eqn;
	fastf_t area;
    };
    size_t i;
    struct poly_face face = { { 0, 0, 0, 0, 0 }, 0, NULL, HINIT_ZERO, 0.0 };
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    struct bn_tol tol;
    RT_BOT_CK_MAGIC(bot);

    *volume = 0.0;
    if (bot->mode == RT_BOT_SURFACE)
	return;

    /* allocate pts array, 3 vertices per bot face */
    face.pts = (point_t *)bu_calloc(3, sizeof(point_t), "rt_bot_volume: pts");
    BN_TOL_INIT(&tol);
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;


    for (face.npts = 0, i = 0; i < bot->num_faces; face.npts = 0, i++) {
	int a, b, c;

	/* find indices of the 3 vertices that make up this face */
	a = bot->faces[i * ELEMENTS_PER_POINT + 0];
	b = bot->faces[i * ELEMENTS_PER_POINT + 1];
	c = bot->faces[i * ELEMENTS_PER_POINT + 2];

	/* find normal, needed to calculate volume later */
	if (bot->bot_flags == RT_BOT_HAS_SURFACE_NORMALS && bot->normals) {
	    /* bot->normals array already exists, use those instead */
	    VMOVE(face.plane_eqn, &bot->normals[i * ELEMENTS_PER_VECT]);
	} else if (UNLIKELY(bg_make_plane_3pnts(face.plane_eqn,
						(&bot->vertices[(a) * ELEMENTS_PER_POINT]),
						(&bot->vertices[(b) * ELEMENTS_PER_POINT]),
						(&bot->vertices[(c) * ELEMENTS_PER_POINT]), &tol) < 0)) {
	    continue;
	}

	VMOVE((face).pts[(face).npts], (&bot->vertices[(a) * ELEMENTS_PER_POINT])); (face).npts++;
	VMOVE((face).pts[(face).npts], (&bot->vertices[(b) * ELEMENTS_PER_POINT])); (face).npts++;
	VMOVE((face).pts[(face).npts], (&bot->vertices[(c) * ELEMENTS_PER_POINT])); (face).npts++;

	/* SURFACE AREA */

	/* sort points */
	bg_3d_polygon_sort_ccw(face.npts, face.pts, face.plane_eqn);
	bg_3d_polygon_area(&face.area, face.npts, (const point_t *)face.pts);

	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    *volume += face.area * bot->thickness[i];
	} else {
	    /* VOLUME */
	    *volume += VDOT(face.pts[0], face.plane_eqn)*face.area;
	}
    }
    *volume = fabs(*volume)/3.0;
    bu_free((char *)face.pts, "rt_bot_volume: pts");
}

void
rt_bot_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    typedef point_t triangle[3];
    struct rt_bot_internal *bot_ip =
	(struct rt_bot_internal *)ip->idb_ptr;
    size_t a, b, j;
    triangle *whole_bot_vertices = (triangle *)bu_calloc(bot_ip->num_faces, sizeof(triangle), "rt_bot_surf_area: whole_bot_vertices"); /* [face][corner][x,y,z] */
    fastf_t whole_bot_overall_area;

    whole_bot_overall_area = 0;

    for (a = 0; a < bot_ip->num_faces; a++) {
	point_t pt[3];

	for (j = 0; j < 3; j++) {
	    size_t ptnum;
	    ptnum = bot_ip->faces[a*3+j];
	    VSCALE(pt[j], &bot_ip->vertices[ptnum*3], 1);
	    /* transfer the vertices into an array, which is structured after the faces, which is necessary for later comparisons, if the bot is a plate */
	    switch (bot_ip->mode) {
	    case RT_BOT_PLATE:
	    case RT_BOT_PLATE_NOCOS:
		whole_bot_vertices[a][j][0] = pt[j][X];
		whole_bot_vertices[a][j][1] = pt[j][Y];
		whole_bot_vertices[a][j][2] = pt[j][Z];
		break;
	    }
	}

	whole_bot_overall_area += bg_area_of_triangle((const fastf_t *)&pt[0], (const fastf_t *)&pt[1], (const fastf_t *)&pt[2]);
    }

    switch (bot_ip->mode) {
    case RT_BOT_PLATE:
    case RT_BOT_PLATE_NOCOS:
	for (a = 0; a-1 < bot_ip->num_faces; a++) {
	    int a_is_exterior_edge, b_is_exterior_edge, c_is_exterior_edge;
	    a_is_exterior_edge = 1;
	    b_is_exterior_edge = 1;
	    c_is_exterior_edge = 1;
	    /* get exterior edges by checking each possible combination between the faces a and b */
	    for (b = 0; b < bot_ip->num_faces; b++) {
		if (a == b)
		    continue; /* can't check against own face */
		/* if both vertices are equal, a and b have a common edge, so it can't be an exterior edge, so the variable is set to 0(false) */
		if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][1]))
		    a_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][2]))
		    a_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][0]))
		    a_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][0]))
		    a_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][1]))
		    a_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][2]))
		    a_is_exterior_edge = 0;
		if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][1]))
		    b_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][2]))
		    b_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][0]))
		    b_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][0]))
		    b_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][1]))
		    b_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][1], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][2]))
		    b_is_exterior_edge = 0;
		if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][1]))
		    c_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][2]))
		    c_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][0]))
		    c_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][1]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][0]))
		    c_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][2]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][1]))
		    c_is_exterior_edge = 0;
		else if (EQUAL(whole_bot_vertices[a][0], whole_bot_vertices[b][0]) && EQUAL(whole_bot_vertices[a][2], whole_bot_vertices[b][2]))
		    c_is_exterior_edge = 0;

	    }
	    if (a_is_exterior_edge == 1) {
		fastf_t rectangle_size, edge_length;

		edge_length = bg_dist_pnt3_pnt3(whole_bot_vertices[a][0], whole_bot_vertices[a][1]);
		rectangle_size = bot_ip->thickness[a] * edge_length;
		whole_bot_overall_area += rectangle_size;

	    }
	    if (b_is_exterior_edge == 1) {
		fastf_t rectangle_size, edge_length;

		edge_length = bg_dist_pnt3_pnt3(whole_bot_vertices[a][1], whole_bot_vertices[a][2]);
		rectangle_size = bot_ip->thickness[a] * edge_length;
		whole_bot_overall_area += rectangle_size;
	    }
	    if (c_is_exterior_edge == 1) {
		fastf_t rectangle_size, edge_length;

		edge_length = bg_dist_pnt3_pnt3(whole_bot_vertices[a][2], whole_bot_vertices[a][0]);
		rectangle_size = bot_ip->thickness[a] * edge_length;
		whole_bot_overall_area += rectangle_size;
	    }
	}

    }
    *area = whole_bot_overall_area;
    bu_free((char *)whole_bot_vertices, "rt_bot_surf_area: whole_bot_vertices");
    return;
}


struct rt_bot_internal *
rt_bot_merge(size_t num_bots, const struct rt_bot_internal * const *bots)
{
    struct rt_bot_internal *result;
    size_t avail_vert, avail_face;
    size_t i, face;
    int *reverse_flags;

    /* create a new bot */
    BU_ALLOC(result, struct rt_bot_internal);
    result->mode = 0;
    result->orientation = RT_BOT_UNORIENTED;
    result->bot_flags = 0;
    result->num_vertices = 0;
    result->num_faces = 0;
    result->faces = (int *)0;
    result->vertices = (fastf_t *)0;
    result->thickness = (fastf_t *)0;
    result->face_mode = (struct bu_bitv *)0;
    result->num_normals = 0;
    result->normals = (fastf_t *)0;
    result->num_face_normals = 0;
    result->face_normals = 0;
    result->magic = RT_BOT_INTERNAL_MAGIC;

    if (!num_bots)
	return result;

    for (i = 0; i < num_bots; ++i) {
	RT_BOT_CK_MAGIC(bots[i]);

	result->num_vertices += bots[i]->num_vertices;
	result->num_faces += bots[i]->num_faces;
    }

    reverse_flags = (int *)bu_calloc(num_bots, sizeof(int), "reverse_flags");

    for (i = 0; i < num_bots; ++i) {
	/* check for surface normals */
	if (result->mode) {
	    if (result->mode != bots[i]->mode) {
		bu_log("rt_bot_merge(): Warning: not all bots share same mode\n");
	    }
	} else {
	    result->mode = bots[i]->mode;
	}

	if (bots[i]->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) result->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	if (bots[i]->bot_flags & RT_BOT_USE_NORMALS) result->bot_flags |= RT_BOT_USE_NORMALS;

	if (result->orientation) {
	    if (bots[i]->orientation == RT_BOT_UNORIENTED) {
		result->orientation = RT_BOT_UNORIENTED;
	    } else {
		reverse_flags[i] = 1; /* set flag to reverse order of faces */
	    }
	} else {
	    result->orientation = bots[i]->orientation;
	}
    }


    result->vertices = (fastf_t *)bu_calloc(result->num_vertices*3, sizeof(fastf_t), "verts");
    result->faces = (int *)bu_calloc(result->num_faces*3, sizeof(int), "verts");

    if (result->mode == RT_BOT_PLATE || result->mode == RT_BOT_PLATE_NOCOS) {
	result->thickness = (fastf_t *)bu_calloc(result->num_faces, sizeof(fastf_t), "thickness");
	result->face_mode = (struct bu_bitv *)bu_calloc(result->num_faces, sizeof(struct bu_bitv), "face_mode");
    }

    avail_vert = 0;
    avail_face = 0;


    for (i = 0; i < num_bots; ++i) {
	/* copy the vertices */
	memcpy(&result->vertices[3*avail_vert], bots[i]->vertices, bots[i]->num_vertices*3*sizeof(fastf_t));

	/* copy/convert the faces, potentially maintaining a common orientation */
	if (result->orientation != RT_BOT_UNORIENTED && reverse_flags[i]) {
	    /* copy and reverse */
	    for (face=0; face < bots[i]->num_faces; face++) {
		/* copy the 3 verts of this face and convert to new index */
		result->faces[avail_face*3+face*3+2] = bots[i]->faces[face*3  ] + avail_vert;
		result->faces[avail_face*3+face*3+1] = bots[i]->faces[face*3+1] + avail_vert;
		result->faces[avail_face*3+face*3  ] = bots[i]->faces[face*3+2] + avail_vert;

		if (result->mode == RT_BOT_PLATE || result->mode == RT_BOT_PLATE_NOCOS) {
		    if (bots[i]->thickness)
			result->thickness[avail_face+face] = bots[i]->thickness[face];
		    if (bots[i]->face_mode)
			result->face_mode[avail_face+face] = bots[i]->face_mode[face];
		}
	    }
	} else {
	    /* just copy */
	    for (face=0; face < bots[i]->num_faces; face++) {
		/* copy the 3 verts of this face and convert to new index */
		result->faces[avail_face*3+face*3  ] = bots[i]->faces[face*3  ] + avail_vert;
		result->faces[avail_face*3+face*3+1] = bots[i]->faces[face*3+1] + avail_vert;
		result->faces[avail_face*3+face*3+2] = bots[i]->faces[face*3+2] + avail_vert;

		if (result->mode == RT_BOT_PLATE || result->mode == RT_BOT_PLATE_NOCOS) {
		    if (bots[i]->thickness)
			result->thickness[avail_face+face] = bots[i]->thickness[face];
		    if (bots[i]->face_mode)
			result->face_mode[avail_face+face] = bots[i]->face_mode[face];
		}
	    }
	}

	/* copy surface normals */
	if (result->bot_flags == RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_log("not yet copying surface normals\n");
	    if (bots[i]->bot_flags == RT_BOT_HAS_SURFACE_NORMALS) {
	    } else {
	    }
	}

	avail_vert += bots[i]->num_vertices;
	avail_face += bots[i]->num_faces;
    }

    return result;
}

const char *
rt_bot_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, bot->vertices);
	goto bot_kpt_end;
    }

    // No keystr matches - failed
    return NULL;

bot_kpt_end:
    MAT4X3PNT(*pt, mat, mpt);

    return k;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
