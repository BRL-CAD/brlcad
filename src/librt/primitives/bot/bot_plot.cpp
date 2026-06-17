/*                    B O T _ P L O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1999-2026 United States Government as represented by
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
/** @file primitives/bot/bot_plot.cpp
 *
 *
 */

#include "common.h"

#include <limits.h>

#include "../../librt_private.h"

extern "C" {
#include "vds.h"

#include "bsg/vlist.h"
#include "bsg/view_state.h"
#include "bg/trimesh.h" // needed for the call in rt_bot_bbox
#include "bg/tri_ray.h"
#include "vmath.h"

#include "rt/defines.h"
#include "rt/global.h"
#include "rt/db_internal.h"
#include "rt/primitives/bot.h"
}

static vdsNode *
build_vertex_tree(struct vdsState *s, struct rt_bot_internal *bot)
{
    size_t i, node_indices, tri_indices;
    vect_t normal = {1.0, 0.0, 0.0};
    unsigned char color[] = {255, 0 , 0};
    vdsNode *leaf_nodes;
    vdsNode **node_list;

    node_indices = bot->num_vertices * 3;
    tri_indices = bot->num_faces * 3;

    vdsBeginVertexTree(s);
    vdsBeginGeometry(s);

    /* create nodes */
    for (i = 0; i < node_indices; i += 3) {
	vdsAddNode(s, bot->vertices[i], bot->vertices[i + 1], bot->vertices[i + 2]);
    }

    /* create triangles */
    for (i = 0; i < tri_indices; i += 3) {
	vdsAddTri(s, bot->faces[i], bot->faces[i + 1], bot->faces[i + 2],
		normal, normal, normal, color, color, color);
    }

    leaf_nodes = vdsEndGeometry(s);

    node_list = (vdsNode **)bu_malloc(bot->num_vertices * sizeof(vdsNode *), "node_list");
    for (i = 0; i < bot->num_vertices; ++i) {
	node_list[i] = &leaf_nodes[i];
    }

    vdsClusterOctree(node_list, bot->num_vertices, 0);
    bu_free(node_list, "node_list");

    return vdsEndVertexTree(s);
}


struct bot_fold_data {
    double dmin;
    double dmax;
    vdsNode *root;
    fastf_t point_spacing;
};


static int
should_fold(const vdsNode *node, void *udata)
{
    int i;
    int num_edges = 0;
    int short_edges = 0;
    fastf_t dist_01, dist_12, dist_20;
    vdsNode *corner_nodes[3];
    struct bot_fold_data *fold_data = (struct bot_fold_data *)udata;

    if (node->nsubtris < 1) {
	return 0;
    }

    /* If it's really small, fold */
    if (fold_data->dmax < fold_data->point_spacing) return 1;

    /* Long, thin objects shouldn't disappear */
    if (fold_data->dmax/fold_data->dmin > 5.0 && node->nsubtris < 30) return 0;

    num_edges = node->nsubtris * 3;

    for (i = 0; i < node->nsubtris; ++i) {
	/* get the three nodes corresponding to the three corner */
	corner_nodes[0] = vdsFindNode(node->subtris[i].corners[0].id, fold_data->root);
	corner_nodes[1] = vdsFindNode(node->subtris[i].corners[1].id, fold_data->root);
	corner_nodes[2] = vdsFindNode(node->subtris[i].corners[2].id, fold_data->root);

	dist_01 = DIST_PNT_PNT(corner_nodes[0]->coord, corner_nodes[1]->coord);
	dist_12 = DIST_PNT_PNT(corner_nodes[1]->coord, corner_nodes[2]->coord);
	dist_20 = DIST_PNT_PNT(corner_nodes[2]->coord, corner_nodes[0]->coord);

	/* check triangle edge point spacing against target point spacing */
	if (dist_01 < fold_data->point_spacing) {
	    ++short_edges;
	}
	if (dist_12 < fold_data->point_spacing) {
	    ++short_edges;
	}
	if (dist_20 < fold_data->point_spacing) {
	    ++short_edges;
	}
    }

    if (((fastf_t)short_edges / num_edges) > .2 && node->nsubtris > 10) {
	return 1;
    }

    return 0;
}

struct node_data {
    struct rt_primitive_lod_realization *realization;
    int ok;
};

static void
plot_node(const vdsNode *node, void *udata)
{
    if (!node)
	return;
    vdsTri *t = node->vistris;
    if (!t)
	return;
    struct node_data *nd = (struct node_data *)udata;

    while (t != NULL) {
	vdsUpdateTriProxies(t);
	if (nd->ok && !primitive_lod_line_set_append(nd->realization,
		t->proxies[2]->coord, BSG_GEOMETRY_LINE_MOVE))
	    nd->ok = 0;
	if (nd->ok && !primitive_lod_line_set_append(nd->realization,
		t->proxies[0]->coord, BSG_GEOMETRY_LINE_DRAW))
	    nd->ok = 0;
	if (nd->ok && !primitive_lod_line_set_append(nd->realization,
		t->proxies[1]->coord, BSG_GEOMETRY_LINE_DRAW))
	    nd->ok = 0;
	if (nd->ok && !primitive_lod_line_set_append(nd->realization,
		t->proxies[2]->coord, BSG_GEOMETRY_LINE_DRAW))
	    nd->ok = 0;
	t = t->next;
    }
}

static fastf_t
avg_sample_spacing(const struct bsg_view *gvp)
{
    fastf_t view_aspect = (fastf_t)gvp->gv_width / gvp->gv_height;
    fastf_t x_size = gvp->gv_size;
    fastf_t y_size = x_size / view_aspect;
    fastf_t avg_view_size = (x_size + y_size) / 2.0;
    fastf_t avg_view_samples = (gvp->gv_width + gvp->gv_height) / 2.0;
    return avg_view_size / avg_view_samples;
}

static int
rt_bot_lod_line_set_vds(struct rt_primitive_lod_realization *realization, struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct bsg_view *v, fastf_t UNUSED(s_size))
{
    double d1, d2, d3;
    point_t min;
    point_t max;

    vdsNode *vertex_tree;
    struct vdsState vdss = VDS_STATE_INIT_ZERO;
    struct bot_fold_data fold_data;

    if (!realization)
	return -1;
    RT_CK_DB_INTERNAL(ip);

    RT_CK_DB_INTERNAL(ip);
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    vertex_tree = build_vertex_tree(&vdss, bot);

    fold_data.root = vertex_tree;
    fold_data.point_spacing = avg_sample_spacing(v);
    bg_trimesh_aabb(&min, &max, bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices);
    d1 = max[0] - min[0];
    d2 = max[1] - min[1];
    d3 = max[2] - min[2];
    fold_data.dmin = d1;
    if (d2 < fold_data.dmin) fold_data.dmin = d2;
    if (d3 < fold_data.dmin) fold_data.dmin = d3;
    fold_data.dmax = d1;
    if (d2 > fold_data.dmax) fold_data.dmax = d2;
    if (d3 > fold_data.dmax) fold_data.dmax = d3;

    vdsAdjustTreeTopDown(vertex_tree, should_fold, (void *)&fold_data);
    struct node_data nd;
    nd.realization = realization;
    nd.ok = 1;
    vdsRenderTree(vertex_tree, plot_node, NULL, (void *)&nd);
    vdsFreeTree(vertex_tree);

    return nd.ok ? 0 : -1;
}

extern "C" int
rt_bot_lod_realize(struct rt_primitive_lod_realization *realization, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bsg_view *v, fastf_t s_size)
{
    if (!primitive_lod_line_set_begin(realization))
	return -1;

    int ret = rt_bot_lod_line_set_vds(realization, ip, tol, v, s_size);
    if (ret < 0)
	return ret;
    return primitive_lod_line_set_finish(realization) ? ret : -1;
}

/* TODO - duplicated from brep_debug.cpp - probably should refactor into proper
 * internal API (maybe even libbn, if that makes sense) ... */
#define BOT_BBOX_ARB_FACE(valp, a, b, c, d)             \
    BSG_ADD_VLIST(vlfree, vhead, valp[a], BSG_VLIST_LINE_MOVE);   \
    BSG_ADD_VLIST(vlfree, vhead, valp[b], BSG_VLIST_LINE_DRAW);   \
    BSG_ADD_VLIST(vlfree, vhead, valp[c], BSG_VLIST_LINE_DRAW);   \
    BSG_ADD_VLIST(vlfree, vhead, valp[d], BSG_VLIST_LINE_DRAW);

#define BOT_BB_PLOT_VLIST(_min, _max) {             \
    fastf_t pt[8][3];                       \
    VSET(pt[0], _max[X], _min[Y], _min[Z]); \
    VSET(pt[1], _max[X], _max[Y], _min[Z]); \
    VSET(pt[2], _max[X], _max[Y], _max[Z]); \
    VSET(pt[3], _max[X], _min[Y], _max[Z]); \
    VSET(pt[4], _min[X], _min[Y], _min[Z]); \
    VSET(pt[5], _min[X], _max[Y], _min[Z]); \
    VSET(pt[6], _min[X], _max[Y], _max[Z]); \
    VSET(pt[7], _min[X], _min[Y], _max[Z]); \
    BOT_BBOX_ARB_FACE(pt, 0, 1, 2, 3);      \
    BOT_BBOX_ARB_FACE(pt, 4, 0, 3, 7);      \
    BOT_BBOX_ARB_FACE(pt, 5, 4, 7, 6);      \
    BOT_BBOX_ARB_FACE(pt, 1, 5, 6, 2);      \
}

// TODO - while this routine makes the vlists per the standard librt API, BoTs
// are a case where we probably should be passing the data directly to the
// drawing routines as face and vert arrays for the hot drawing paths WITHOUT
// making the vlist copies... this duplication results in massive additional
// memory usage for large BoTs.
extern "C" int
rt_bot_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bsg_view *info)
{
    struct rt_bot_internal *bot_ip;
    size_t i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &rt_vlfree;
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (bot_ip->num_vertices <= 0 || !bot_ip->vertices || bot_ip->num_faces <= 0 || !bot_ip->faces)
	return 0;

    if (!info || !primitive_lod_bot_threshold(info) || ((size_t)primitive_lod_bot_threshold(info) > bot_ip->num_faces)) {
	for (i = 0; i < bot_ip->num_faces; i++) {
	    if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
		continue; /* sanity */

	    BSG_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BSG_VLIST_LINE_MOVE);
	    BSG_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], BSG_VLIST_LINE_DRAW);
	    BSG_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+2]*3], BSG_VLIST_LINE_DRAW);
	    BSG_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BSG_VLIST_LINE_DRAW);
	}
    } else {
	/* too big - just draw the bbox */
	point_t min, max;
	bg_trimesh_aabb(&min, &max, bot_ip->faces, bot_ip->num_faces, (const point_t *)bot_ip->vertices, bot_ip->num_vertices);
	BOT_BB_PLOT_VLIST(min, max);
    }

    return 0;
}


static int
bot_indexed_face_order(const struct rt_bot_internal *bot,
		       size_t face_idx,
		       int vertex_order[3],
		       int normal_order[3])
{
    const int *face;

    if (!bot || !bot->faces || !vertex_order || !normal_order)
	return 0;

    face = &bot->faces[face_idx * 3];
    for (int i = 0; i < 3; i++) {
	if (face[i] < 0 || (size_t)face[i] >= bot->num_vertices)
	    return 0;
    }

    vertex_order[0] = face[0];
    normal_order[0] = 0;
    if (bot->orientation == RT_BOT_CW) {
	vertex_order[1] = face[2];
	vertex_order[2] = face[1];
	normal_order[1] = 2;
	normal_order[2] = 1;
    } else {
	vertex_order[1] = face[1];
	vertex_order[2] = face[2];
	normal_order[1] = 1;
	normal_order[2] = 2;
    }

    return 1;
}


static void
bot_indexed_face_normal(const struct rt_bot_internal *bot,
			const int vertex_order[3],
			vect_t normal)
{
    vect_t ab, ac;
    const fastf_t *aa;
    const fastf_t *bb;
    const fastf_t *cc;

    VSETALL(normal, 0.0);
    if (!bot || !vertex_order)
	return;

    aa = &bot->vertices[(size_t)vertex_order[0] * 3];
    bb = &bot->vertices[(size_t)vertex_order[1] * 3];
    cc = &bot->vertices[(size_t)vertex_order[2] * 3];
    VSUB2(ab, aa, bb);
    VSUB2(ac, aa, cc);
    VCROSS(normal, ab, ac);
    if (MAGSQ(normal) > SMALL_FASTF)
	VUNITIZE(normal);
    else
	VSETALL(normal, 0.0);
}


static int
bot_indexed_face_vertex_normal(const struct rt_bot_internal *bot,
			       size_t face_idx,
			       int normal_slot,
			       vect_t normal)
{
    int normal_idx;

    if (!bot || !normal ||
	    !(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) ||
	    !(bot->bot_flags & RT_BOT_USE_NORMALS) ||
	    !bot->normals || !bot->face_normals ||
	    face_idx >= bot->num_face_normals ||
	    normal_slot < 0 || normal_slot > 2)
	return 0;

    normal_idx = bot->face_normals[face_idx * 3 + normal_slot];
    if (normal_idx < 0 || (size_t)normal_idx >= bot->num_normals)
	return 0;

    VMOVE(normal, &bot->normals[(size_t)normal_idx * 3]);
    return 1;
}


extern "C" int
rt_bot_indexed_face_set(struct rt_primitive_indexed_face_set *face_set,
			struct rt_db_internal *ip,
			const struct bg_tess_tol *UNUSED(ttol),
			const struct bn_tol *UNUSED(tol),
			const struct bsg_view *UNUSED(info))
{
    struct rt_bot_internal *bot;
    point_t *points = NULL;
    vect_t *normals = NULL;
    int *indices = NULL;
    size_t valid_faces = 0;
    size_t index_count;
    size_t normal_count;
    size_t index_idx = 0;
    size_t normal_idx = 0;

    if (!face_set || !ip)
	return BRLCAD_ERROR;

    RT_CK_DB_INTERNAL(ip);
    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (!bot->vertices || !bot->faces || !bot->num_vertices || !bot->num_faces)
	return BRLCAD_OK;
    if (bot->num_vertices > (size_t)INT_MAX)
	return BRLCAD_ERROR;

    for (size_t i = 0; i < bot->num_faces; i++) {
	int vertex_order[3];
	int normal_order[3];
	if (bot_indexed_face_order(bot, i, vertex_order, normal_order))
	    valid_faces++;
    }

    if (!valid_faces)
	return BRLCAD_OK;

    index_count = valid_faces * 4;
    normal_count = valid_faces * 3;
    points = (point_t *)bu_calloc(bot->num_vertices, sizeof(point_t),
	    "BoT indexed-face points");
    normals = (vect_t *)bu_calloc(normal_count, sizeof(vect_t),
	    "BoT indexed-face normals");
    indices = (int *)bu_calloc(index_count, sizeof(int),
	    "BoT indexed-face indices");

    for (size_t i = 0; i < bot->num_vertices; i++)
	VMOVE(points[i], &bot->vertices[i * 3]);

    for (size_t i = 0; i < bot->num_faces; i++) {
	int vertex_order[3];
	int normal_order[3];
	vect_t face_normal;

	if (!bot_indexed_face_order(bot, i, vertex_order, normal_order))
	    continue;

	bot_indexed_face_normal(bot, vertex_order, face_normal);
	for (int j = 0; j < 3; j++) {
	    indices[index_idx] = vertex_order[j];
	    if (!bot_indexed_face_vertex_normal(bot, i, normal_order[j],
		    normals[normal_idx]))
		VMOVE(normals[normal_idx], face_normal);
	    normal_idx++;
	    index_idx++;
	}
	indices[index_idx] = -1;
	index_idx++;
    }

    face_set->points = points;
    face_set->point_count = bot->num_vertices;
    face_set->normals = normals;
    face_set->normal_count = normal_count;
    face_set->indices = indices;
    face_set->index_count = index_count;
    face_set->source_identity = (uint64_t)(uintptr_t)bot;
    face_set->geometry_revision++;
    return BRLCAD_OK;
}


/** @} */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
