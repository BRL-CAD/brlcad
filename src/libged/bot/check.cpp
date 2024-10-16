/*                       C H E C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
/** @file check.cpp
 *
 * Implementation of routines for checking the topological integrity
 * of a mesh defined with a BoT primitive.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "manifold/manifold.h"

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"
#include "./ged_bot.h"

static bool
manifold_check(struct rt_bot_internal *bot)
{
    if (!bot)
	return false;

    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < bot->num_vertices ; j++) {
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+0]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+1]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+2]);
    }
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+0]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	}
    }

    manifold::Manifold omanifold(bot_mesh);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// BoT is manifold
	return true;
    }

    // Not manifold
    return false;
}

struct _ged_bot_icheck {
    struct _ged_bot_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

/* for bsearch() */
static int
edge_compare(const void *a, const void *b)
{
    const int *edge_a = (int *)a;
    const int *edge_b = (int *)b;
    int diff = edge_a[0] - edge_b[0];
    return diff ? diff : edge_a[1] - edge_b[1];
}

/* for bu_sort() */
static int
edge_cmp(const void *a, const void *b, void *UNUSED(context))
{
    return edge_compare(a, b);
}

static int
is_edge_in_list(int edge[2], struct bg_trimesh_edges list)
{
    return (bsearch(edge, list.edges, list.count, sizeof(int) * 2, edge_compare) != NULL);
}

static int
is_edge_in_lists(int edge[2], struct bg_trimesh_edges lists[], int num_lists)
{
    int i;
    for (i = 0; i < num_lists; ++i) {
	if (is_edge_in_list(edge, lists[i])) {
	    return 1;
	}
    }
    return 0;
}

static struct bg_trimesh_edges*
make_edges(int edge_count)
{
    struct bg_trimesh_edges *edges;
    BU_ALLOC(edges, struct bg_trimesh_edges);
    edges->count = 0;
    edges->edges = (int *)bu_malloc(edge_count * 2 * sizeof(int), "make edges");
    return edges;
}

static void
copy_edge(int dst[2], int src[2])
{
    dst[0] = src[0];
    dst[1] = src[1];
}

static void
append_edge(struct bg_trimesh_edges *list, int edge[2])
{
    copy_edge(&list->edges[list->count * 2], edge);
    ++(list->count);
}

static void
append_edge_if_not_in_lists(struct bg_trimesh_edges *dst, int edge[2], struct bg_trimesh_edges lists[], int num_lists)
{
    if (!is_edge_in_lists(edge, lists, num_lists)) {
	append_edge(dst, edge);
    }
}

static struct bg_trimesh_edges*
edges_not_in_lists(struct bg_trimesh_edges all, struct bg_trimesh_edges lists[], int num_lists)
{
    int i;
    struct bg_trimesh_edges *remaining = make_edges(all.count);

    for (i = 0; i < all.count; ++i) {
	append_edge_if_not_in_lists(remaining, &all.edges[i * 2], lists, num_lists);
    }
    return remaining;
}

static struct bg_trimesh_edges*
edges_from_half_edges(struct bg_trimesh_halfedge edge_list[], int num_edges)
{
    int i;
    struct bg_trimesh_edges *edges = make_edges(num_edges);

    for (i = 0; i < num_edges; ++i) {
	int half_edge[2];
	half_edge[0] = edge_list[i].va;
	half_edge[1] = edge_list[i].vb;
	append_edge_if_not_in_lists(edges, half_edge, edges, 1);
    }
    return edges;
}

static void
standardize_edge(int edge[2])
{
    if (edge[1] < edge[0]) {
	int tmp = edge[0];
	edge[0] = edge[1];
	edge[1] = tmp;
    }
}

static void
append_face_edges(struct bg_trimesh_edges *edges, int face[3])
{
    int i, edge[2];
    for (i = 0; i < 3; ++i) {
	edge[0] = face[i];
	edge[1] = face[(i + 1) % 3];
	standardize_edge(edge);
	append_edge(edges, edge);
    }
}

static struct bg_trimesh_edges*
non_unique_face_edges(struct bg_trimesh_faces faces, struct rt_bot_internal *bot)
{
    int i;
    struct bg_trimesh_edges *edges = make_edges(faces.count * 3);

    for (i = 0; i < faces.count; ++i) {
	append_face_edges(edges, &bot->faces[faces.faces[i] * 3]);
    }
    bu_sort(edges->edges, edges->count, sizeof(int) * 2, edge_cmp, NULL);
    return edges;
}

static struct bg_trimesh_edges*
face_edges(struct bg_trimesh_faces faces, struct rt_bot_internal *bot)
{
    int i;
    struct bg_trimesh_edges *unique_edges = make_edges(faces.count * 3);
    struct bg_trimesh_edges *all_edges = non_unique_face_edges(faces, bot);

    for (i = 0; i < all_edges->count; ++i) {
	append_edge_if_not_in_lists(unique_edges, &all_edges->edges[i * 2], unique_edges, 1);
    }
    bg_free_trimesh_edges(all_edges);
    BU_FREE(all_edges, struct bg_trimesh_edges);

    return unique_edges;
}

static struct bg_trimesh_faces*
make_faces(int num_faces)
{
    struct bg_trimesh_faces *faces;
    BU_ALLOC(faces, struct bg_trimesh_faces);

    faces->count = 0;
    faces->faces = (int *)bu_malloc(sizeof(int) * num_faces, "make faces");
    return faces;
}

static struct bg_trimesh_faces*
faces_from_bot(struct rt_bot_internal *bot)
{
    int i;
    struct bg_trimesh_faces *faces = make_faces((int)bot->num_faces);
    faces->count = (int)bot->num_faces;
    for (i = 0; i < faces->count; ++i) {
	faces->faces[i] = i;
    }
    return faces;
}

static struct bg_trimesh_edges*
edges_from_bot(struct rt_bot_internal *bot)
{
    struct bg_trimesh_faces *faces = faces_from_bot(bot);
    struct bg_trimesh_edges *edges = face_edges(*faces, bot);

    bg_free_trimesh_faces(faces);
    BU_FREE(faces, struct bg_trimesh_faces);

    return edges;
}

static void
draw_edges(struct ged *gedp, struct rt_bot_internal *bot, int num_edges, int edges[], struct bu_color *color, const char *draw_name)
{
    struct bu_list *vhead;
    point_t a,b;
    unsigned char draw_color[3];
    bu_color_to_rgb_chars(color, draw_color);
    struct bv_vlblock *vbp;
    struct bu_list local_vlist;

    BU_LIST_INIT(&local_vlist);
    vbp = bv_vlblock_init(&local_vlist, 32);

    for (int curr_edge = 0; curr_edge < num_edges; curr_edge++) {
	int p1 = edges[curr_edge*2];
	int p2 = edges[curr_edge*2+1];
	VSET(a, bot->vertices[p1*3], bot->vertices[p1*3+1], bot->vertices[p1*3+2]);
	VSET(b, bot->vertices[p2*3], bot->vertices[p2*3+1], bot->vertices[p2*3+2]);
	vhead = bv_vlblock_find(vbp, draw_color[0], draw_color[1], draw_color[2]);
	BV_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BV_VLIST_LINE_DRAW);
    }

    const char *nview = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(nview, "1")) {
	struct bu_vls nroot = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nroot, "bot_check::%s", draw_name);
	struct bview *view = gedp->ged_gvp;
	bv_vlblock_obj(vbp, view, bu_vls_cstr(&nroot));
	bu_vls_free(&nroot);
    } else {
	_ged_cvt_vlblock_to_solids(gedp, vbp, draw_name, 0);
    }
    bv_vlist_cleanup(&local_vlist);
    bv_vlblock_free(vbp);
}

static int
_bot_check_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_icheck *gb = (struct _ged_bot_icheck *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}


extern "C" int
_bot_cmd_degen_faces(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] degen_faces <objname>";
    const char *purpose_string = "Check BoT for degenerate faces";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    struct bu_color *color = gib->gb->color;

    struct bg_trimesh_faces degenerate = BG_TRIMESH_FACES_INIT_NULL;
    struct bg_trimesh_edges *degen_edges, *all_edges, *other_edges;
    int degenerate_faces = 0;
    int num_faces = (int)bot->num_faces;

    if (gib->gb->visualize) {
	/* first pass - count errors */
	degenerate_faces = bg_trimesh_degenerate_faces(num_faces, bot->faces, bg_trimesh_face_continue, NULL);

	if (degenerate_faces) {
	    /* second pass - generate error faces array and draw it */
	    degenerate.count = 0;
	    degenerate.faces = (int *)bu_calloc(degenerate_faces, sizeof(int), "degenerate faces");
	    bg_trimesh_degenerate_faces(num_faces, bot->faces, bg_trimesh_face_gather, &degenerate);

	    degen_edges = face_edges(degenerate, bot);
	    bg_free_trimesh_faces(&degenerate);

	    all_edges = edges_from_bot(bot);
	    other_edges = edges_not_in_lists(*all_edges, degen_edges, 1);
	    bg_free_trimesh_edges(all_edges);
	    BU_FREE(all_edges, struct bg_trimesh_edges);

	    struct bu_vls dg_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&dg_name, "%s_degen_faces", gib->gb->dp->d_namep);

	    draw_edges(gib->gb->gedp, bot, degen_edges->count, degen_edges->edges, color, bu_vls_cstr(&dg_name));
	    struct bu_color red = BU_COLOR_INIT_ZERO;
	    bu_color_from_str(&red, "255/0/0");
	    draw_edges(gib->gb->gedp, bot, other_edges->count, other_edges->edges, &red, bu_vls_cstr(&dg_name));

	    bu_vls_free(&dg_name);

	    bg_free_trimesh_edges(degen_edges);
	    BU_FREE(degen_edges, struct bg_trimesh_edges);
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	}
    } else {
	/* fast path - exit on first error */
	degenerate_faces = bg_trimesh_degenerate_faces(num_faces, bot->faces, bg_trimesh_face_exit, NULL);
    }

    bu_vls_printf(gib->vls, degenerate_faces ? "1" : "0");

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_extra_edges(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] extra_edges <objname>";
    const char *purpose_string = "Check BoT for edges which are not part of any triangle faces";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    struct bu_color *color = gib->gb->color;

    int num_faces, num_edges;
    num_faces = (int)bot->num_faces;
    num_edges = num_faces * 3;

    /* generate half-edge list */
    struct bg_trimesh_halfedge *edge_list;
    if (!(edge_list = bg_trimesh_generate_edge_list(num_faces, bot->faces))) {
	bu_vls_printf(gib->vls, "ERROR: failed to generate an edge list\n");
	return BRLCAD_ERROR;
    }

    int extra_edges = 0;

    if (gib->gb->visualize) {
	/* first pass - count errors */
	struct bg_trimesh_edges error_edges = BG_TRIMESH_EDGES_INIT_NULL;
	struct bg_trimesh_edges *all_edges, *other_edges;

	extra_edges = bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_continue, NULL);

	if (extra_edges) {
	    /* second pass - generate error edge array and draw it */
	    error_edges.count = 0;
	    error_edges.edges = (int *)bu_calloc(extra_edges * 2, sizeof(int), "error edges");
	    extra_edges = bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_gather, &error_edges);

	    all_edges = edges_from_half_edges(edge_list, num_edges);
	    other_edges = edges_not_in_lists(*all_edges, &error_edges, 1);
	    bg_free_trimesh_edges(all_edges);
	    BU_FREE(all_edges, struct bg_trimesh_edges);

	    struct bu_vls ee_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&ee_name, "%s_extra_edges", gib->gb->dp->d_namep);

	    draw_edges(gib->gb->gedp, bot, error_edges.count, error_edges.edges, color, bu_vls_cstr(&ee_name));
	    struct bu_color red = BU_COLOR_INIT_ZERO;
	    bu_color_from_str(&red, "255/0/0");
	    draw_edges(gib->gb->gedp, bot, other_edges->count, other_edges->edges, &red, bu_vls_cstr(&ee_name));

	    bu_vls_free(&ee_name);

	    bg_free_trimesh_edges(&error_edges);
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	}
    } else {
	/* fast path - exit on first error */
	extra_edges = bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_exit, NULL);
    }

    bu_free(edge_list, "edge list");

    bu_vls_printf(gib->vls, extra_edges ? "1" : "0");

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_flipped_edges(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] flipped_edges <objname>";
    const char *purpose_string = "Check BoT for edges which are incorrectly oriented";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    struct bu_color *color = gib->gb->color;

    int num_faces, num_edges;
    num_faces = (int)bot->num_faces;
    num_edges = num_faces * 3;

    /* generate half-edge list */
    struct bg_trimesh_halfedge *edge_list;
    if (!(edge_list = bg_trimesh_generate_edge_list(num_faces, bot->faces))) {
	bu_vls_printf(gib->vls, "ERROR: failed to generate an edge list\n");
	return BRLCAD_ERROR;
    }

    int flipped_edges = 0;

    if (gib->gb->visualize) {
	/* first pass - count errors */
	struct bg_trimesh_edges error_edges = BG_TRIMESH_EDGES_INIT_NULL;
	struct bg_trimesh_edges *all_edges, *other_edges;

	flipped_edges = bg_trimesh_misoriented_edges(num_edges, edge_list, bg_trimesh_edge_continue, NULL);

	if (flipped_edges) {
	    /* second pass - generate error edge array and draw it */
	    error_edges.count = 0;
	    error_edges.edges = (int *)bu_calloc(flipped_edges * 2, sizeof(int), "flipped edges");
	    flipped_edges = bg_trimesh_misoriented_edges(num_edges, edge_list, bg_trimesh_edge_gather, &error_edges);

	    all_edges = edges_from_half_edges(edge_list, num_edges);
	    other_edges = edges_not_in_lists(*all_edges, &error_edges, 1);
	    bg_free_trimesh_edges(all_edges);
	    BU_FREE(all_edges, struct bg_trimesh_edges);

	    struct bu_vls ee_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&ee_name, "%s_flipped_edges", gib->gb->dp->d_namep);

	    draw_edges(gib->gb->gedp, bot, error_edges.count, error_edges.edges, color, bu_vls_cstr(&ee_name));
	    struct bu_color red = BU_COLOR_INIT_ZERO;
	    bu_color_from_str(&red, "255/0/0");
	    draw_edges(gib->gb->gedp, bot, other_edges->count, other_edges->edges, &red, bu_vls_cstr(&ee_name));

	    bu_vls_free(&ee_name);

	    bg_free_trimesh_edges(&error_edges);
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	}
    } else {
	/* fast path - exit on first error */
	flipped_edges = bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_exit, NULL);
    }

    bu_free(edge_list, "edge list");

    bu_vls_printf(gib->vls, flipped_edges ? "1" : "0");

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_manifold(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] manifold <objname>";
    const char *purpose_string = "Check BoT for validity using the Manifold library";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    bool is_manifold = manifold_check(bot);
    bu_vls_printf(gib->vls, is_manifold ? "1" : "0");

    return BRLCAD_OK;
}


extern "C" int
_bot_cmd_open_edges(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] open_edges <objname>";
    const char *purpose_string = "Check BoT for edges which are not connected to two triangle faces";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    struct bu_color *color = gib->gb->color;

    int num_faces, num_edges;
    num_faces = (int)bot->num_faces;
    num_edges = num_faces * 3;

    /* generate half-edge list */
    struct bg_trimesh_halfedge *edge_list;
    if (!(edge_list = bg_trimesh_generate_edge_list(num_faces, bot->faces))) {
	bu_vls_printf(gib->vls, "ERROR: failed to generate an edge list\n");
	return BRLCAD_ERROR;
    }

    int open_edges = 0;

    if (gib->gb->visualize) {
	/* first pass - count errors */
	struct bg_trimesh_edges error_edges = BG_TRIMESH_EDGES_INIT_NULL;
	struct bg_trimesh_edges *all_edges, *other_edges;

	open_edges = bg_trimesh_unmatched_edges(num_edges, edge_list, bg_trimesh_edge_continue, NULL);

	if (open_edges) {
	    /* second pass - generate error edge array and draw it */
	    error_edges.count = 0;
	    error_edges.edges = (int *)bu_calloc(open_edges * 2, sizeof(int), "error edges");
	    open_edges = bg_trimesh_unmatched_edges(num_edges, edge_list, bg_trimesh_edge_gather, &error_edges);

	    all_edges = edges_from_half_edges(edge_list, num_edges);
	    other_edges = edges_not_in_lists(*all_edges, &error_edges, 1);
	    bg_free_trimesh_edges(all_edges);
	    BU_FREE(all_edges, struct bg_trimesh_edges);

	    struct bu_vls ee_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&ee_name, "%s_open_edges", gib->gb->dp->d_namep);

	    draw_edges(gib->gb->gedp, bot, error_edges.count, error_edges.edges, color, bu_vls_cstr(&ee_name));
	    struct bu_color red = BU_COLOR_INIT_ZERO;
	    bu_color_from_str(&red, "255/0/0");
	    draw_edges(gib->gb->gedp, bot, other_edges->count, other_edges->edges, &red, bu_vls_cstr(&ee_name));

	    bu_vls_free(&ee_name);

	    bg_free_trimesh_edges(&error_edges);
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	}
    } else {
	/* fast path - exit on first error */
	open_edges = bg_trimesh_excess_edges(num_edges, edge_list, bg_trimesh_edge_exit, NULL);
    }

    bu_free(edge_list, "edge list");

    bu_vls_printf(gib->vls, open_edges ? "1" : "0");

    return BRLCAD_OK;
}


extern "C" int
_bot_cmd_solid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] check solid <objname>";
    const char *purpose_string = "Check if BoT defines a topologically closed solid";
    if (_bot_check_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_icheck *gib = (struct _ged_bot_icheck *)bs;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gib->gb->intern->idb_ptr);
    struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int not_solid;

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	// By definition we consider plate modes solid
	bu_vls_printf(gib->vls, "1");
	return BRLCAD_OK;
    }

    // Do the manifold check first
    bool not_manifold = !manifold_check(bot);

    int num_vertices = (int)bot->num_vertices;
    int num_faces = (int)bot->num_faces;

    not_solid = bg_trimesh_solid2(num_vertices, num_faces, bot->vertices, bot->faces, gib->gb->visualize ? &errors : NULL);
    bu_vls_printf(gib->vls, "%d", (not_solid || not_manifold) ? 0 : 1);

    if (not_solid != not_manifold)
	bu_log("Warning - bg_trimesh_solid2(%d) and Manifold(%d) disagree\n", (int)!not_solid, (int)!not_manifold);

    if (not_solid && gib->gb->visualize) {
	struct bg_trimesh_edges *degen_edges = NULL, *all_edges, *other_edges;
	struct bg_trimesh_edges error_lists[4];
	int num_lists = 0;

	error_lists[num_lists++] = errors.unmatched;
	error_lists[num_lists++] = errors.misoriented;
	error_lists[num_lists++] = errors.excess;
	if (errors.degenerate.count > 0) {
	    degen_edges = face_edges(errors.degenerate, bot);
	    error_lists[num_lists++] = *degen_edges;
	}

	all_edges = edges_from_bot(bot);
	other_edges = edges_not_in_lists(*all_edges, error_lists, num_lists);
	bg_free_trimesh_edges(all_edges);
	BU_FREE(all_edges, struct bg_trimesh_edges);

	struct bu_color red = BU_COLOR_INIT_ZERO;
	bu_color_from_str(&red, "255/0/0");
	struct bu_color yellow = BU_COLOR_INIT_ZERO;
	bu_color_from_str(&yellow, "255/255/0");
	struct bu_color orange = BU_COLOR_INIT_ZERO;
	bu_color_from_str(&orange, "255/128/0");
	struct bu_color purple = BU_COLOR_INIT_ZERO;
	bu_color_from_str(&purple, "255/0/255");

	struct bu_vls ns_name = BU_VLS_INIT_ZERO;

	bu_vls_sprintf(&ns_name, "%s_non_solid_ne", gib->gb->dp->d_namep);
	draw_edges(gib->gb->gedp, bot, other_edges->count, other_edges->edges, &red, bu_vls_cstr(&ns_name));
	bu_vls_sprintf(&ns_name, "%s_non_solid_ue", gib->gb->dp->d_namep);
	draw_edges(gib->gb->gedp, bot, errors.unmatched.count, errors.unmatched.edges, &yellow, bu_vls_cstr(&ns_name));
	bu_vls_sprintf(&ns_name, "%s_non_solid_me", gib->gb->dp->d_namep);
	draw_edges(gib->gb->gedp, bot, errors.misoriented.count, errors.misoriented.edges, &orange, bu_vls_cstr(&ns_name));
	bu_vls_sprintf(&ns_name, "%s_non_solid_ee", gib->gb->dp->d_namep);
	draw_edges(gib->gb->gedp, bot, errors.excess.count, errors.excess.edges, &purple, bu_vls_cstr(&ns_name));

	if (errors.degenerate.count > 0) {
	    struct bu_color blue = BU_COLOR_INIT_ZERO;
	    bu_color_from_str(&blue, "0/0/255");
	    bu_vls_sprintf(&ns_name, "%s_non_solid_de", gib->gb->dp->d_namep);
	    draw_edges(gib->gb->gedp, bot, degen_edges->count, degen_edges->edges, &blue, bu_vls_cstr(&ns_name));

	    bg_free_trimesh_edges(degen_edges);
	    BU_FREE(degen_edges, struct bg_trimesh_edges);
	}
	bu_vls_free(&ns_name);
	bg_free_trimesh_edges(other_edges);
	BU_FREE(other_edges, struct bg_trimesh_edges);
	bg_free_trimesh_solid_errors(&errors);

    }

    return BRLCAD_OK;
}

static void
_bot_check_help(struct _ged_bot_icheck *bs, int argc, const char **argv)
{
    struct _ged_bot_icheck *gb = (struct _ged_bot_icheck *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "bot [options] check [subcommand] <objname>\n");
	bu_vls_printf(gb->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	size_t maxcmdlen = 0;
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    maxcmdlen = (maxcmdlen > strlen(ctp->ct_name)) ? maxcmdlen : strlen(ctp->ct_name);
	}
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gb->vls, "  %s%*s", ctp->ct_name, (int)(maxcmdlen - strlen(ctp->ct_name)) + 2, " ");
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
	}
    } else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _bot_check_cmds[] = {
    { "degen_faces",   _bot_cmd_degen_faces},
    { "extra_edges",   _bot_cmd_extra_edges},
    { "flipped_edges", _bot_cmd_flipped_edges},
    { "manifold",      _bot_cmd_manifold},
    { "open_edges",    _bot_cmd_open_edges},
    { "solid",         _bot_cmd_solid},
    { (char *)NULL,      NULL}
};

int
_bot_cmd_check(void *bs, int argc, const char **argv)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct _ged_bot_icheck gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _bot_check_cmds;

    const char *purpose_string = "Check topological integrity of mesh defined by BoT";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }

    if (!argc) {
	_bot_check_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_bot_check_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    // Skip the "check" string
    argc--;argv++;

    if (!argc) {
	_bot_check_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    if (_bot_obj_setup(gb, argv[argc-1]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }
    argc--;

    if (!argc) {
	// No subcommand - do the solid check
	return _bot_cmd_solid((void *)&gib, 0, NULL);
    }

    // Have subcommand - must have valid subcommand to process
    if (bu_cmd_valid(_bot_check_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_bot_check_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_bot_check_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return BRLCAD_ERROR;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
