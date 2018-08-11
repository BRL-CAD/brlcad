/*                         B O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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
/** @file libged/bot.c
 *
 * Bot command for simple bot primitive operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/sort.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"
#include "./ged_private.h"


HIDDEN void
_bot_show_help(struct ged *gedp, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "Usage: bot [options] [subcommand] [subcommand arguments]\n\n");

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  get   (faces|minEdge|maxEdge|orientation|type|vertices) <bot>\n");
    bu_vls_printf(&str, "    - Get specific BoT information.\n\n");
    bu_vls_printf(&str, "  check (solid|degen_faces|open_edges|extra_edges|flipped_edges) <bot>\n");
    bu_vls_printf(&str, "    - Check the BoT for problems (see bot_check man page).\n\n");
    bu_vls_printf(&str, "  chull <bot> <output_bot>\n");
    bu_vls_printf(&str, "    - Store the BoT's convex hull in <output_bot>.\n\n");

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}

/* for bsearch() */
HIDDEN int
edge_compare(const void *a, const void *b)
{
    const int *edge_a = (int *)a;
    const int *edge_b = (int *)b;
    int diff = edge_a[0] - edge_b[0];
    return diff ? diff : edge_a[1] - edge_b[1];
}

/* for bu_sort() */
HIDDEN int
edge_cmp(const void *a, const void *b, void *UNUSED(context))
{
    return edge_compare(a, b);
}

HIDDEN int
is_edge_in_list(int edge[2], struct bg_trimesh_edges list)
{
    return (bsearch(edge, list.edges, list.count, sizeof(int) * 2, edge_compare) != NULL);
}

HIDDEN int
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

HIDDEN struct bg_trimesh_edges*
make_edges(int edge_count)
{
    struct bg_trimesh_edges *edges;
    BU_ALLOC(edges, struct bg_trimesh_edges);
    edges->count = 0;
    edges->edges = (int *)bu_malloc(edge_count * 2 * sizeof(int), "make edges");
    return edges;
}

HIDDEN void
copy_edge(int dst[2], int src[2])
{
    dst[0] = src[0];
    dst[1] = src[1];
}

HIDDEN void
append_edge(struct bg_trimesh_edges *list, int edge[2])
{
    copy_edge(&list->edges[list->count * 2], edge);
    ++(list->count);
}

HIDDEN void
append_edge_if_not_in_lists(struct bg_trimesh_edges *dst, int edge[2], struct bg_trimesh_edges lists[], int num_lists)
{
    if (!is_edge_in_lists(edge, lists, num_lists)) {
	append_edge(dst, edge);
    }
}

HIDDEN struct bg_trimesh_edges*
edges_not_in_lists(struct bg_trimesh_edges all, struct bg_trimesh_edges lists[], int num_lists)
{
    int i;
    struct bg_trimesh_edges *remaining = make_edges(all.count);

    for (i = 0; i < all.count; ++i) {
	append_edge_if_not_in_lists(remaining, &all.edges[i * 2], lists, num_lists);
    }
    return remaining;
}

HIDDEN struct bg_trimesh_edges*
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

HIDDEN void
standardize_edge(int edge[2])
{
    if (edge[1] < edge[0]) {
	int tmp = edge[0];
	edge[0] = edge[1];
	edge[1] = tmp;
    }
}

HIDDEN void
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

HIDDEN struct bg_trimesh_edges*
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

HIDDEN struct bg_trimesh_edges*
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

HIDDEN struct bg_trimesh_faces*
make_faces(int num_faces)
{
    struct bg_trimesh_faces *faces;
    BU_ALLOC(faces, struct bg_trimesh_faces);

    faces->count = 0;
    faces->faces = (int *)bu_malloc(sizeof(int) * num_faces, "make faces");
    return faces;
}

HIDDEN struct bg_trimesh_faces*
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

HIDDEN struct bg_trimesh_edges*
edges_from_bot(struct rt_bot_internal *bot)
{
    struct bg_trimesh_faces *faces = faces_from_bot(bot);
    struct bg_trimesh_edges *edges = face_edges(*faces, bot);

    bg_free_trimesh_faces(faces);
    BU_FREE(faces, struct bg_trimesh_faces);

    return edges;
}

HIDDEN void
draw_edges(struct ged *gedp, struct rt_bot_internal *bot, int num_edges, int edges[], int draw_color[3], const char *draw_name)
{
    int curr_edge = 0;
    struct bu_list *vhead;
    point_t a, b;
    struct bn_vlblock *vbp;
    struct bu_list local_vlist;

    BU_LIST_INIT(&local_vlist);
    vbp = bn_vlblock_init(&local_vlist, 32);

    /* Clear any previous visual */
    if (db_lookup(gedp->ged_wdbp->dbip, draw_name, LOOKUP_QUIET) != RT_DIR_NULL)
	dl_erasePathFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, draw_name, 1, gedp->freesolid);

    for (curr_edge = 0; curr_edge < num_edges; curr_edge++) {
	int p1 = edges[curr_edge*2];
	int p2 = edges[curr_edge*2+1];
	VSET(a, bot->vertices[p1*3], bot->vertices[p1*3+1], bot->vertices[p1*3+2]);
	VSET(b, bot->vertices[p2*3], bot->vertices[p2*3+1], bot->vertices[p2*3+2]);
	vhead = bn_vlblock_find(vbp, draw_color[0], draw_color[1], draw_color[2]);
	BN_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BN_VLIST_LINE_MOVE);
	BN_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BN_VLIST_LINE_DRAW);
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, draw_name, 0);
    bn_vlist_cleanup(&local_vlist);
    bn_vlblock_free(vbp);
}

HIDDEN int
bot_check(struct ged *gedp, int argc, const char *argv[], struct bu_opt_desc *d, struct rt_db_internal *ip, int visualize_results)
{
    const char *check = argv[1];
    const char * const *sub, *subcommands[] = {"solid", "degen_faces", "open_edges", "extra_edges", "flipped_edges", NULL};
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    struct bg_trimesh_halfedge *edge_list;
    int (*edge_test)(int, struct bg_trimesh_halfedge *, bg_edge_error_funct_t, void *);
    int num_vertices, num_faces, num_edges;
    int found;
    int red[] = {255, 0, 0};
    int blue[] = {0, 0, 255};
    int yellow[] = {255, 255, 0};
    int orange[] = {255, 128, 0};
    int purple[] = {255, 0, 255};

    /* must be wanting help */
    if (argc < 2) {
	_bot_show_help(gedp, d);
	rt_db_free_internal(ip);
	return GED_ERROR;
    }

    num_vertices = (int)bot->num_vertices;
    num_faces = (int)bot->num_faces;
    num_edges = num_faces * 3;

    if (argc < 3 || BU_STR_EQUAL(check, "solid")) {
	struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	int not_solid = bg_trimesh_solid2(num_vertices, num_faces, bot->vertices, bot->faces, visualize_results ? &errors : NULL);
	bu_vls_printf(gedp->ged_result_str, not_solid ? "0" : "1");

	if (not_solid && visualize_results) {
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

	    draw_edges(gedp, bot, other_edges->count, other_edges->edges, red, "other faces");
	    draw_edges(gedp, bot, errors.unmatched.count, errors.unmatched.edges, yellow, "unmatched edges");
	    draw_edges(gedp, bot, errors.misoriented.count, errors.misoriented.edges, orange, "misoriented edges");
	    draw_edges(gedp, bot, errors.excess.count, errors.excess.edges, purple, "excess edges");

	    if (errors.degenerate.count > 0) {
		draw_edges(gedp, bot, degen_edges->count, degen_edges->edges, blue, "degenerate faces");

		bg_free_trimesh_edges(degen_edges);
		BU_FREE(degen_edges, struct bg_trimesh_edges);
	    }
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	    bg_free_trimesh_solid_errors(&errors);
	}
	rt_db_free_internal(ip);
	return GED_OK;
    }

    /* check for one of the individual tests */
    found = 0;
    for (sub = subcommands; sub != NULL; ++sub) {
	if (BU_STR_EQUAL(check, *sub)) {
	    found = 1;
	    break;
	}
    }
    if (!found) {
	bu_vls_printf(gedp->ged_result_str, "check: %s is not a recognized check subcommand!\n", check);
	rt_db_free_internal(ip);
	return GED_ERROR;
    }

    /* face test */
    if (BU_STR_EQUAL(check, "degen_faces")) {
	struct bg_trimesh_faces degenerate = BG_TRIMESH_FACES_INIT_NULL;
	struct bg_trimesh_edges *degen_edges, *all_edges, *other_edges;
	int degenerate_faces = 0;

	if (visualize_results) {
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

		draw_edges(gedp, bot, degen_edges->count, degen_edges->edges, yellow, "degenerate faces");
		draw_edges(gedp, bot, other_edges->count, other_edges->edges, red, "othererate faces");

		bg_free_trimesh_edges(degen_edges);
		BU_FREE(degen_edges, struct bg_trimesh_edges);
		bg_free_trimesh_edges(other_edges);
		BU_FREE(other_edges, struct bg_trimesh_edges);
	    }
	} else {
	    /* fast path - exit on first error */
	    degenerate_faces = bg_trimesh_degenerate_faces(num_faces, bot->faces, bg_trimesh_face_exit, NULL);
	}

	bu_vls_printf(gedp->ged_result_str, degenerate_faces ? "1" : "0");

	rt_db_free_internal(ip);
	return GED_OK;
    }

    /* must be doing one of the edge tests */

    /* generate half-edge list */
    if (!(edge_list = bg_trimesh_generate_edge_list(num_faces, bot->faces))) {
	rt_db_free_internal(ip);
	bu_vls_printf(gedp->ged_result_str, "ERROR: failed to generate an edge list\n");
	return GED_ERROR;
    }

    /* select edge test */
    if (BU_STR_EQUAL(check, "open_edges")) {
	edge_test = bg_trimesh_unmatched_edges;
    } else if (BU_STR_EQUAL(check, "flipped_edges")) {
	edge_test = bg_trimesh_misoriented_edges;
    } else if (BU_STR_EQUAL(check, "extra_edges")) {
	edge_test = bg_trimesh_excess_edges;
    } else {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unrecognized edge test [%s]\n", check);
	return GED_ERROR;
    }

    if (visualize_results) {
	/* first pass - count errors */
	struct bg_trimesh_edges error_edges = BG_TRIMESH_EDGES_INIT_NULL;
	struct bg_trimesh_edges *all_edges, *other_edges;

	found = edge_test(num_edges, edge_list, bg_trimesh_edge_continue, NULL);

	if (found) {
	    /* second pass - generate error edge array and draw it */
	    error_edges.count = 0;
	    error_edges.edges = (int *)bu_calloc(found * 2, sizeof(int), "error edges");
	    found = edge_test(num_edges, edge_list, bg_trimesh_edge_gather, &error_edges);

	    all_edges = edges_from_half_edges(edge_list, num_edges);
	    other_edges = edges_not_in_lists(*all_edges, &error_edges, 1);
	    bg_free_trimesh_edges(all_edges);
	    BU_FREE(all_edges, struct bg_trimesh_edges);

	    draw_edges(gedp, bot, error_edges.count, error_edges.edges, yellow, "error edges");
	    draw_edges(gedp, bot, other_edges->count, other_edges->edges, red, "other edges");

	    bg_free_trimesh_edges(&error_edges);
	    bg_free_trimesh_edges(other_edges);
	    BU_FREE(other_edges, struct bg_trimesh_edges);
	}
    } else {
	/* fast path - exit on first error */
	found = edge_test(num_edges, edge_list, bg_trimesh_edge_exit, NULL);
    }

    bu_free(edge_list, "edge list");

    bu_vls_printf(gedp->ged_result_str, found ? "1" : "0");
    rt_db_free_internal(ip);
    return GED_OK;
}

int
ged_bot(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *bot_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    const char *cmd = argv[0];
    const char *sub = NULL;
    const char *arg = NULL;
    const char *primitive = NULL;
    size_t len;
    fastf_t tmp;
    fastf_t propVal;
    int i;
    int print_help = 0;
    int visualize_results = 0;
    int opt_ret = 0;
    int opt_argc;
    struct bu_opt_desc d[3];
    const char * const bot_subcommands[] = {"check","chull","get", NULL};
    BU_OPT(d[0], "h", "help",      "", NULL, &print_help,        "Print help and exit");
    BU_OPT(d[1], "V", "visualize", "", NULL, &visualize_results, "Use subcommand's 3D visualization.");
    BU_OPT_NULL(d[2]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	_bot_show_help(gedp, d);
	return GED_ERROR;
    }

    /* See if we have any options to deal with.  Once we hit a subcommand, we're done */
    opt_argc = argc;
    for (i = 1; i < argc; ++i) {
	const char * const *subcmd = bot_subcommands;

	for (; *subcmd != NULL; ++subcmd) {
	    if (BU_STR_EQUAL(argv[i], *subcmd)) {
		opt_argc = i;
		i = argc;
		break;
	    }
	}
    }

    if (opt_argc >= argc) {
	/* no subcommand given */
	_bot_show_help(gedp, d);
	return GED_ERROR;
    }

    if (opt_argc > 1) {
	/* parse standard options */
	opt_ret = bu_opt_parse(NULL, opt_argc, argv, d);
	if (opt_ret < 0) _bot_show_help(gedp, d);
    }

    /* shift past standard options to subcommand args */
    argc -= opt_argc;
    argv = &argv[opt_argc];

    if (argc < 2) {
	_bot_show_help(gedp, d);
	return GED_ERROR;
    }

    /* determine subcommand */
    sub = argv[0];
    len = strlen(sub);
    if (bu_strncmp(sub, "get", len) == 0) {
	primitive = argv[argc - 1];
    } else if (bu_strncmp(sub, "chull", len) == 0) {
	primitive = argv[1];
    } else if (bu_strncmp(sub, "check", len) == 0) {
	primitive = argv[argc - 1];
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
	return GED_ERROR;
    }

    /* get bot */
    GED_DB_LOOKUP(gedp, bot_dp, primitive, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!", cmd, primitive);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (bu_strncmp(sub, "get", len) == 0) {
	arg = argv[1];
	propVal = rt_bot_propget(bot, arg);

	/* print result string */
	if (!EQUAL(propVal, -1.0)) {

	    tmp = (int) propVal;

	    /* int result */
	    if (EQUAL(propVal, tmp)) {
		bu_vls_printf(gedp->ged_result_str, "%d", (int) propVal);
	    }

	    /* float result */
	    else {
		bu_vls_printf(gedp->ged_result_str, "%f", propVal);
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a valid argument!", sub, arg);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }
    if (bu_strncmp(sub, "chull", len) == 0) {
	int retval = 0;
	int fc = 0;
	int vc = 0;
	point_t *vert_array;
	int *faces;
	unsigned char err = 0;

	/* must be wanting help */
	if (argc < 3) {
	    _bot_show_help(gedp, d);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)bot->vertices, (int)bot->num_vertices);

	if (retval != 3) {
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	retval = mk_bot(gedp->ged_wdbp, argv[2], RT_BOT_SOLID, RT_BOT_CCW, err, vc, fc, (fastf_t *)vert_array, faces, NULL, NULL);

	if (retval) {
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }
    if (bu_strncmp(sub, "check", len) == 0) {
	return bot_check(gedp, argc, argv, d, &intern, visualize_results);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
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
