/*                    B O T _ P L O T . C P P
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
/** @file primitives/bot/bot_plot.cpp
 *
 *
 */

#include "common.h"

extern "C" {
#include "vds.h"

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
    struct bu_list *vhead;
    struct bu_list *vlfree;
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
    struct bu_list *vhead = nd->vhead;
    struct bu_list *vlfree = nd->vlfree;

    while (t != NULL) {
	vdsUpdateTriProxies(t);
	BV_ADD_VLIST(vlfree, vhead, t->proxies[2]->coord, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, t->proxies[0]->coord, BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, t->proxies[1]->coord, BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, t->proxies[2]->coord, BV_VLIST_LINE_DRAW);
	t = t->next;
    }
}

static fastf_t
avg_sample_spacing(const struct bview *gvp)
{
    fastf_t view_aspect = (fastf_t)gvp->gv_width / gvp->gv_height;
    fastf_t x_size = gvp->gv_size;
    fastf_t y_size = x_size / view_aspect;
    fastf_t avg_view_size = (x_size + y_size) / 2.0;
    fastf_t avg_view_samples = (gvp->gv_width + gvp->gv_height) / 2.0;
    return avg_view_size / avg_view_samples;
}

extern "C" int
rt_bot_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct bview *v, fastf_t UNUSED(s_size))
{
    double d1, d2, d3;
    point_t min;
    point_t max;

    vdsNode *vertex_tree;
    struct vdsState vdss = VDS_STATE_INIT_ZERO;
    struct bot_fold_data fold_data;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &rt_vlfree;

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
    nd.vhead = vhead;
    nd.vlfree = vlfree;
    vdsRenderTree(vertex_tree, plot_node, NULL, (void *)&nd);
    vdsFreeTree(vertex_tree);

    return 0;
}

/* TODO - duplicated from brep_debug.cpp - probably should refactor into proper
 * internal API (maybe even libbn, if that makes sense) ... */
#define BOT_BBOX_ARB_FACE(valp, a, b, c, d)             \
    BV_ADD_VLIST(vlfree, vhead, valp[a], BV_VLIST_LINE_MOVE);   \
    BV_ADD_VLIST(vlfree, vhead, valp[b], BV_VLIST_LINE_DRAW);   \
    BV_ADD_VLIST(vlfree, vhead, valp[c], BV_VLIST_LINE_DRAW);   \
    BV_ADD_VLIST(vlfree, vhead, valp[d], BV_VLIST_LINE_DRAW);

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
rt_bot_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *info)
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

    if (!info || !info->gv_s->bot_threshold || (info->gv_s->bot_threshold > bot_ip->num_faces)) {
	for (i = 0; i < bot_ip->num_faces; i++) {
	    if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
		continue; /* sanity */

	    BV_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+2]*3], BV_VLIST_LINE_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BV_VLIST_LINE_DRAW);
	}
    } else {
	/* too big - just draw the bbox */
	point_t min, max;
	bg_trimesh_aabb(&min, &max, bot_ip->faces, bot_ip->num_faces, (const point_t *)bot_ip->vertices, bot_ip->num_vertices);
	BOT_BB_PLOT_VLIST(min, max);
    }

    return 0;
}


extern "C" int
rt_bot_plot_poly(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
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

    /* XXX Should consider orientation here, flip if necessary. */
    for (i = 0; i < bot_ip->num_faces; i++) {
	point_t aa, bb, cc;
	vect_t ab, ac;
	vect_t norm;

	if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
	    continue; /* sanity */

	VMOVE(aa, &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	if (bot_ip->orientation == RT_BOT_CW) {
	    VMOVE(bb, &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	    VMOVE(cc, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	} else {
	    VMOVE(bb, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	    VMOVE(cc, &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	}

	VSUB2(ab, aa, bb);
	VSUB2(ac, aa, cc);
	VCROSS(norm, ab, ac);
	VUNITIZE(norm);
	BV_ADD_VLIST(vlfree, vhead, norm, BV_VLIST_TRI_START);

	if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) &&
		(bot_ip->bot_flags & RT_BOT_USE_NORMALS)) {
	    vect_t na, nb, nc;

	    VMOVE(na, &bot_ip->normals[bot_ip->face_normals[i*3+0]*3]);
	    if (bot_ip->orientation == RT_BOT_CW) {
		VMOVE(nb, &bot_ip->normals[bot_ip->face_normals[i*3+2]*3]);
		VMOVE(nc, &bot_ip->normals[bot_ip->face_normals[i*3+1]*3]);
	    } else {
		VMOVE(nb, &bot_ip->normals[bot_ip->face_normals[i*3+1]*3]);
		VMOVE(nc, &bot_ip->normals[bot_ip->face_normals[i*3+2]*3]);
	    }
	    BV_ADD_VLIST(vlfree, vhead, na, BV_VLIST_TRI_VERTNORM);
	    BV_ADD_VLIST(vlfree, vhead, aa, BV_VLIST_TRI_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, nb, BV_VLIST_TRI_VERTNORM);
	    BV_ADD_VLIST(vlfree, vhead, bb, BV_VLIST_TRI_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, nc, BV_VLIST_TRI_VERTNORM);
	    BV_ADD_VLIST(vlfree, vhead, cc, BV_VLIST_TRI_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, aa, BV_VLIST_TRI_END);
	} else {
	    BV_ADD_VLIST(vlfree, vhead, aa, BV_VLIST_TRI_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, bb, BV_VLIST_TRI_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, cc, BV_VLIST_TRI_DRAW);
	    BV_ADD_VLIST(vlfree, vhead, aa, BV_VLIST_TRI_END);
	}
    }

    return 0;
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
