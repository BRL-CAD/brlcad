/*                         D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/draw.c
 *
 * The draw command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "bu/getopt.h"
#include "bu/parallel.h"
#include "bu/time.h"
#include "raytrace.h"


#include "../ged_private.h"

/* declare our callbacks used by _ged_drawtrees() */
static int drawtrees_depth = 0;


static union tree *
draw_check_region_end(struct db_tree_state *tsp,
			 const struct db_full_path *pathp,
			 union tree *curtree,
			 void *UNUSED(client_data))
{
    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);
    if (curtree) RT_CK_TREE(curtree);

    return curtree;
}

static void
draw_forced_wireframe(
    const struct db_full_path *pathp,
    const struct _ged_client_data *dgcdp)
{
    int ac = 1;
    const char *av[2];

    /* draw the path with the given client data, but force wireframe mode */
    struct _ged_client_data dgcd = *dgcdp;
    dgcd.gedp->ged_gdp->gd_shaded_mode = 0;
    dgcd.vs.s_dmode = _GED_WIREFRAME;

    av[0] = db_path_to_string(pathp);
    av[1] = (char *)0;

    _ged_drawtrees(dgcd.gedp, ac, av, _GED_DRAW_WIREFRAME, &dgcd);

    bu_free((void *)av[0], "draw_forced_wireframe: av[0]");
}

static void
plot_shaded(
    struct db_tree_state *tsp,
    const struct db_full_path *pathp,
    struct rt_db_internal *ip,
    struct _ged_client_data *dgcdp)
{
    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
	(ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT   ||
	 ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY  ||
	 ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP))
    {
	struct bu_list vhead;
	BU_LIST_INIT(&vhead);

	switch (ip->idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_BOT:
		(void)rt_bot_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		break;
	    case DB5_MINORTYPE_BRLCAD_POLY:
		(void)rt_pg_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		(void)rt_brep_plot_poly(&vhead, pathp, ip, tsp->ts_ttol,
			tsp->ts_tol, NULL);
	}
	_ged_drawH_part2(0, &vhead, pathp, tsp, dgcdp);
    } else {
	int ac = 1;
	const char *av[2];

	av[0] = db_path_to_string(pathp);
	av[1] = (char *)0;

	_ged_drawtrees(dgcdp->gedp, ac, av, _GED_DRAW_NMG_POLY, dgcdp);

	bu_free((void *)av[0], "plot_shaded: av[0]");
    }
}

static union tree *
draw_check_leaf(struct db_tree_state *tsp,
		   const struct db_full_path *pathp,
		   struct rt_db_internal *ip,
		   void *client_data)
{
    union tree *curtree;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    /* Indicate success by returning something other than TREE_NULL */
    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;

    if (dgcdp->vs.draw_non_subtract_only && (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER)))
	return curtree;

    switch (dgcdp->vs.s_dmode) {
	case _GED_SHADED_MODE_BOTS:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		(ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT   ||
		 ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY  ||
		 ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP))
	    {
		plot_shaded(tsp, pathp, ip, dgcdp);
	    } else {
		draw_forced_wireframe(pathp, dgcdp);
	    }
	    break;
	case _GED_SHADED_MODE_ALL:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE)
	    {
		plot_shaded(tsp, pathp, ip, dgcdp);
	    } else {
		draw_forced_wireframe(pathp, dgcdp);
	    }
	    break;
	case _GED_HIDDEN_LINE:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE)
	    {
		plot_shaded(tsp, pathp, ip, dgcdp);
	    } else {
		draw_forced_wireframe(pathp, dgcdp);
	    }

	    break;
    }
    return curtree;
}

static int
get_path_and_state(
    struct db_tree_state *tsp,
    struct db_full_path *pathp,
    const char *path_name,
    struct ged *gedp)
{
    *tsp = gedp->ged_wdbp->wdb_initial_tree_state;
    tsp->ts_dbip = gedp->dbip;
    tsp->ts_resp = &rt_uniresource;

    return db_follow_path_for_state(tsp, pathp, path_name, LOOKUP_QUIET);
}

static int
plot_shaded_eval(
    struct ged *gedp,
    const char *path_name,
    struct _ged_client_data *dgcdp)
{
    int ret;
    const char *av[3];
    const char *tmp_basename = "tmp_shaded_eval_obj";
    char *brep_name;

    /* make a name for the temp brep */
    av[0] = "make_name";
    av[1] = tmp_basename;

    ged_make_name(gedp, 2, (const char **)av);

    brep_name = bu_vls_strdup(gedp->ged_result_str);
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* create temp evaluated brep from named object */
    av[0] = "brep";
    av[1] = path_name;
    av[2] = brep_name;
    ret = ged_brep(gedp, 3, av);

    if (ret == GED_OK) {
	int brep_made = 0;
	struct db_tree_state ts;
	struct rt_db_internal brep_intern;
	struct db_full_path input_path, brep_path;

	RT_DB_INTERNAL_INIT(&brep_intern);
	db_full_path_init(&input_path);
	db_full_path_init(&brep_path);

	/* get brep internal */
	ret = get_path_and_state(&ts, &brep_path, brep_name, gedp);
	if (ret == GED_OK) {
	    struct directory *dp = DB_FULL_PATH_CUR_DIR(&brep_path);

	    if (dp->d_flags & RT_DIR_COMB) {
		ret = rt_db_get_internal(&brep_intern, dp, ts.ts_dbip, NULL,
			ts.ts_resp);
	    } else {
		ret = rt_db_get_internal(&brep_intern, dp, ts.ts_dbip, ts.ts_mat,
			ts.ts_resp);
	    }
	    if (ret >= 0) {
		brep_made = 1;
	    }
	    db_free_full_path(&brep_path);
	}

	/* plot brep, but use the path and state of the input object */
	if (brep_made) {
	    ret = get_path_and_state(&ts, &input_path, path_name, gedp);
	    if (ret == GED_OK) {
		plot_shaded(&ts, &input_path, &brep_intern, dgcdp);

		rt_db_free_internal(&brep_intern);
		db_free_full_path(&input_path);
	    }
	}

	/* kill temp brep */
	av[0] = "kill";
	av[1] = brep_name;
	ged_kill(gedp, 2, av);
    }
    bu_free((char *)brep_name, "vls_strdup");

    return ret;
}

static union tree *
wireframe_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);
    if (curtree) RT_CK_TREE(curtree);

    return curtree;
}


/**
 * When performing "ev" on a region, consider whether to process the
 * whole subtree recursively.
 *
 * Normally, say "yes" to all regions by returning 0.
 *
 * Check for special case: a region of one solid, which can be
 * directly drawn as polygons without going through NMGs.  If we draw
 * it here, then return -1 to signal caller to ignore further
 * processing of this region.  A hack to view polygonal models
 * (converted from FASTGEN) more rapidly.
 */
static int
draw_nmg_region_start(struct db_tree_state *tsp, const struct db_full_path *pathp, const struct rt_comb_internal *combp, void *client_data)
{
    union tree *tp;
    struct directory *dp;
    struct rt_db_internal intern;
    mat_t xform;
    matp_t matp;
    struct bu_list vhead;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);
	bu_vls_printf(dgcdp->gedp->ged_result_str, "nmg_region_start(%s)\n", sofar);
	bu_free((void *)sofar, "path string");
	rt_pr_tree(combp->tree, 1);
	db_pr_tree_state(tsp);
    }

    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    RT_CK_COMB(combp);
    tp = combp->tree;
    if (!tp)
	return -1;
    RT_CK_TREE(tp);
    if (tp->tr_l.tl_op != OP_DB_LEAF)
	return 0;	/* proceed as usual */

    /* The subtree is a single node.  It may be a combination, though */

    /* Fetch by name, check to see if it's an easy type */
    dp = db_lookup(tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_NOISY);
    if (!dp)
	return 0;	/* proceed as usual */

    if (!bn_mat_is_identity(tsp->ts_mat)) {
	if (tp->tr_l.tl_mat) {
	    matp = xform;
	    bn_mat_mul(xform, tsp->ts_mat, tp->tr_l.tl_mat);
	} else {
	    matp = tsp->ts_mat;
	}
    } else {
	if (tp->tr_l.tl_mat) {
	    matp = tp->tr_l.tl_mat;
	} else {
	    matp = (matp_t)NULL;
	}
    }

    if (rt_db_get_internal(&intern, dp, tsp->ts_dbip, matp, &rt_uniresource) < 0)
	return 0;	/* proceed as usual */

    switch (intern.idb_type) {
	case ID_POLY:
	    {
		if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
		    bu_log("fastpath draw ID_POLY %s\n", dp->d_namep);
		}
		if (dgcdp->nmg_fast_wireframe_draw) {
		    (void)rt_pg_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
		} else {
		    (void)rt_pg_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
		}
	    }
	    goto out;
	case ID_BOT:
	    {
		if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
		    bu_log("fastpath draw ID_BOT %s\n", dp->d_namep);
		}
		if (dgcdp->nmg_fast_wireframe_draw) {
		    (void)rt_bot_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
		} else {
		    (void)rt_bot_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
		}
	    }
	    goto out;
	case ID_BREP:
	    {
		if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
		    bu_log("fastpath draw ID_BREP %s\n", dp->d_namep);
		}
		if (dgcdp->nmg_fast_wireframe_draw) {
		    (void)rt_brep_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
		} else {
		    (void)rt_brep_plot_poly(&vhead, pathp, &intern, tsp->ts_ttol, tsp->ts_tol, NULL);
		}
	    }
	    goto out;
	case ID_COMBINATION:
	default:
	    break;
    }
    rt_db_free_internal(&intern);
    return 0;

out:
    {
	struct db_full_path pp;
	db_full_path_init(&pp);
	db_dup_full_path(&pp, pathp);

	/* Successful fastpath drawing of this solid */
	db_add_node_to_full_path(&pp, dp);
	_ged_drawH_part2(0, &vhead, &pp, tsp, dgcdp);

	db_free_full_path(&pp);
    }

    rt_db_free_internal(&intern);
    dgcdp->fastpath_count++;
    return -1;	/* SKIP THIS REGION */
}


static int
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct _ged_client_data *dgcdp)
{
    static int result; /* static due to jumping */

    result = 1;

    if (!BU_SETJUMP) {
	/* try */

	result = nmg_boolean(curtree, *tsp->ts_m, &RTG.rtg_vlfree, tsp->ts_tol, tsp->ts_resp);

    } else {
	/* catch */
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "WARNING: Boolean evaluation of %s failed!\n", sofar);
	bu_free((void *)sofar, "path string");
    } BU_UNSETJUMP;

    return result;
}


static int
process_triangulation(struct db_tree_state *tsp, const struct db_full_path *pathp, struct _ged_client_data *dgcdp)
{
    static int result; /* static due to jumping */

    result = 1;

    if (!BU_SETJUMP) {
	/* try */

	nmg_triangulate_model(*tsp->ts_m, &RTG.rtg_vlfree, tsp->ts_tol);
	result = 0;

    } else {
	/* catch */

	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "WARNING: Triangulation of %s failed!\n", sofar);
	bu_free((void *)sofar, "path string");

    } BU_UNSETJUMP;

    return result;
}


/**
 * This routine must be prepared to run in parallel.
 */
static union tree *
draw_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    struct nmgregion *r;
    struct bu_list vhead;
    int failed = 1;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "nmg_region_end() path='%s'\n", sofar);
	bu_free((void *)sofar, "path string");
    } else {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "%s:\n", sofar);
	bu_free((void *)sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP) return curtree;

    if (!dgcdp->draw_nmg_only) {

	failed = process_boolean(curtree, tsp, pathp, dgcdp);
	if (failed) {
	    db_free_tree(curtree, tsp->ts_resp);
	    return (union tree *)NULL;
	}

    } else if (curtree->tr_op != OP_NMG_TESS) {
	bu_vls_printf(dgcdp->gedp->ged_result_str, "Cannot use '-d' option when Boolean evaluation is required\n");
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }
    r = curtree->tr_d.td_r;
    NMG_CK_REGION(r);

    if (dgcdp->do_not_draw_nmg_solids_during_debugging && r) {
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }

    if (dgcdp->nmg_triangulate) {
	failed = process_triangulation(tsp, pathp, dgcdp);
	if (failed) {
	    db_free_tree(curtree, tsp->ts_resp);
	    return (union tree *)NULL;
	}
    }

    if (r != 0) {
	int style;
	/* Convert NMG to vlist */
	NMG_CK_REGION(r);

	if (dgcdp->nmg_fast_wireframe_draw) {
	    /* Draw in vector form */
	    style = NMG_VLIST_STYLE_VECTOR;
	} else {
	    /* Default -- draw polygons */
	    style = NMG_VLIST_STYLE_POLYGON;
	}
	if (dgcdp->draw_normals) {
	    style |= NMG_VLIST_STYLE_VISUALIZE_NORMALS;
	}
	if (dgcdp->shade_per_vertex_normals) {
	    style |= NMG_VLIST_STYLE_USE_VU_NORMALS;
	}
	if (dgcdp->draw_no_surfaces) {
	    style |= NMG_VLIST_STYLE_NO_SURFACES;
	}
	nmg_r_to_vlist(&vhead, r, style, &RTG.rtg_vlfree);

	_ged_drawH_part2(0, &vhead, pathp, tsp, dgcdp);

	if (dgcdp->draw_edge_uses) {
	    nmg_vlblock_r(dgcdp->draw_edge_uses_vbp, r, 1, &RTG.rtg_vlfree);
	}
	/* NMG region is no longer necessary, only vlist remains */
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }

    /* Return tree -- it needs to be freed (by caller) */
    return curtree;
}

/*
 * This routine is the drawable geometry object's analog of rt_gettrees().
 * Add a set of tree hierarchies to the active set.
 * Note that argv[0] should be ignored, it has the command name in it.
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 */
int
_ged_drawtrees(struct ged *gedp, int argc, const char *argv[], int kind, struct _ged_client_data *_dgcdp)
{
    int ret = 0;
    int c;
    int ncpu = 1;
    int nmg_use_tnurbs = 0;
    int enable_fastpath = 0;
    struct model *nmg_model;
    struct _ged_client_data dgcdp;
    int i;
    int ac = 1;
    char *av[3];
    int bot_threshold = 0;
    int threshold_cached = 0;
    int shaded_mode_override = _GED_SHADED_MODE_UNSET;

    RT_CHECK_DBI(gedp->dbip);

    if (argc <= 0)
	return -1;	/* FAIL */

    ++drawtrees_depth;
    av[1] = (char *)0;

    /* options are already parsed into _dgcdp */
    if (_dgcdp != (struct _ged_client_data *)0) {
	dgcdp = *_dgcdp;            /* struct copy */
    } else {
	struct bview *gvp;

	memset(&dgcdp, 0, sizeof(struct _ged_client_data));
	dgcdp.gedp = gedp;

	gvp = gedp->ged_gvp;

	if (gedp && gedp->ged_gvp) threshold_cached = gvp->gv_s->bot_threshold;

	if (gvp && gvp->gv_s->adaptive_plot)
	    dgcdp.autoview = 1;
	else
	    dgcdp.autoview = 0;

	/* Initial values for options, must be reset each time */
	dgcdp.draw_nmg_only = 0;	/* no booleans */
	dgcdp.nmg_triangulate = 1;
	dgcdp.nmg_fast_wireframe_draw = 0;
	dgcdp.draw_normals = 0;
	dgcdp.vs.draw_solid_lines_only = 0;
	dgcdp.draw_no_surfaces = 0;
	dgcdp.vs.draw_non_subtract_only = 0;
	dgcdp.shade_per_vertex_normals = 0;
	dgcdp.draw_edge_uses = 0;
	dgcdp.vs.color_override = 0;
	dgcdp.fastpath_count = 0;

	/* default color - red */
	dgcdp.vs.color[0] = 255;
	dgcdp.vs.color[1] = 0;
	dgcdp.vs.color[2] = 0;

	/* default transparency - opaque */
	dgcdp.vs.transparency = 1.0;

	/* free_scene_obj */
	dgcdp.free_scene_obj = gedp->free_scene_obj;

	enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 0;		/* re-init bu_getopt() */
	while ((c = bu_getopt(argc, (char * const *)argv, "dfhm:nqstuvwx:C:STP:A:oRL:M")) != -1) {
	    switch (c) {
		case 'u':
		    dgcdp.draw_edge_uses = 1;
		    break;
		case 's':
		    dgcdp.vs.draw_solid_lines_only = 1;
		    break;
		case 't':
		    nmg_use_tnurbs = 1;
		    break;
		case 'v':
		    dgcdp.shade_per_vertex_normals = 1;
		    break;
		case 'w':
		    dgcdp.nmg_fast_wireframe_draw = 1;
		    break;
		case 'S':
		    dgcdp.draw_no_surfaces = 1;
		    dgcdp.vs.draw_non_subtract_only = 1;
		    break;
		case 'T':
		    dgcdp.nmg_triangulate = 0;
		    break;
		case 'n':
		    dgcdp.draw_normals = 1;
		    break;
		case 'P':
		    ncpu = atoi(bu_optarg);
		    break;
		case 'q':
		    dgcdp.do_not_draw_nmg_solids_during_debugging = 1;
		    break;
		case 'd':
		    dgcdp.draw_nmg_only = 1;
		    break;
		case 'f':
		    enable_fastpath = 1;
		    break;
		case 'C':
		    {
			int r, g, b;
			char *cp = bu_optarg;

			r = atoi(cp);
			while ((*cp >= '0' && *cp <= '9')) cp++;
			while (*cp && (*cp < '0' || *cp > '9')) cp++;
			g = atoi(cp);
			while ((*cp >= '0' && *cp <= '9')) cp++;
			while (*cp && (*cp < '0' || *cp > '9')) cp++;
			b = atoi(cp);

			if (r < 0 || r > 255) r = 255;
			if (g < 0 || g > 255) g = 255;
			if (b < 0 || b > 255) b = 255;

			dgcdp.vs.color_override = 1;
			dgcdp.vs.color[0] = r;
			dgcdp.vs.color[1] = g;
			dgcdp.vs.color[2] = b;
		    }
		    break;
		case 'h':
		    shaded_mode_override = _GED_HIDDEN_LINE;
		    break;
		case 'm':
		    shaded_mode_override = atoi(bu_optarg);

		    switch (shaded_mode_override) {
			case 0:
			    shaded_mode_override = _GED_WIREFRAME;
			    break;
			case 1:
			    shaded_mode_override = _GED_SHADED_MODE_BOTS;
			    break;
			case 2:
			    shaded_mode_override = _GED_SHADED_MODE_ALL;
			    break;
			case 3:
			    shaded_mode_override = _GED_SHADED_MODE_EVAL;
			    break;
			case 4:
			    shaded_mode_override = _GED_HIDDEN_LINE;
			    break;
			default:
			    if (shaded_mode_override < 0) {
				shaded_mode_override = _GED_SHADED_MODE_UNSET;
			    } else {
				shaded_mode_override = _GED_SHADED_MODE_ALL;
			    }
		    }
		    break;
		case 'x':
		    dgcdp.vs.transparency = atof(bu_optarg);

		    /* clamp it to [0, 1] */
		    if (dgcdp.vs.transparency < 0.0)
			dgcdp.vs.transparency = 0.0;

		    if (1.0 < dgcdp.vs.transparency)
			dgcdp.vs.transparency = 1.0;

		    break;
		case 'R':
		    dgcdp.autoview = 0;
		    break;
		case 'L':
		    {
			int t = 0;
			char *cp = bu_optarg;
			if (cp) {
			    t = atoi(cp);
			    if (t >= 0) {
				bot_threshold = (size_t)t;
			    } else {
				bu_vls_printf(gedp->ged_result_str, "invalid -L argument: %s\n", cp);
				--drawtrees_depth;
				return GED_ERROR;
			    }
			} else {
			    bu_vls_printf(gedp->ged_result_str, "-L requires an option\n");
			    --drawtrees_depth;
			    return GED_ERROR;
			}
		    }
		    break;
		case 'A':
		case 'o':
		    /* nothing to do, handled by edit_com wrapper on the front-end */
		    break;
		default:
		    {
			bu_vls_printf(gedp->ged_result_str, "unrecognized option - %c\n", c);
			--drawtrees_depth;
			return GED_ERROR;
		    }
	    }
	}
	argc -= bu_optind;
	argv += bu_optind;

	switch (kind) {
	    case _GED_DRAW_WIREFRAME:
		dgcdp.vs.s_dmode = _GED_WIREFRAME;
		if (shaded_mode_override != _GED_SHADED_MODE_UNSET) {
		    dgcdp.vs.s_dmode = shaded_mode_override;
		} else if (gedp->ged_gdp->gd_shaded_mode) {
		    dgcdp.vs.s_dmode = gedp->ged_gdp->gd_shaded_mode;
		}
		break;
	    case _GED_DRAW_NMG_POLY:
		dgcdp.vs.s_dmode = _GED_BOOL_EVAL;
		break;
	}

    }

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "Please specify one or more objects to be drawn.\n");
	--drawtrees_depth;
	return -1;
    }

    switch (kind) {
	default:
	    bu_vls_printf(gedp->ged_result_str, "ERROR, bad kind\n");
	    --drawtrees_depth;
	    return -1;
	case _GED_DRAW_WIREFRAME:
	    /*
	     * If asking for wireframe and in shaded_mode and no shaded mode override,
	     * or asking for wireframe and shaded mode is being overridden with a value
	     * greater than 0, then draw shaded polygons for each object's primitives if possible.
	     *
	     * Note -
	     * If shaded_mode is _GED_SHADED_MODE_BOTS, only BOTS and polysolids
	     * will be shaded. The rest is drawn as wireframe.
	     * If shaded_mode is _GED_SHADED_MODE_ALL, everything except pipe solids
	     * are drawn as shaded polygons.
	     */
	    if (dgcdp.vs.s_dmode == _GED_SHADED_MODE_BOTS ||
		dgcdp.vs.s_dmode == _GED_SHADED_MODE_ALL  ||
		dgcdp.vs.s_dmode == _GED_SHADED_MODE_EVAL ||
		dgcdp.vs.s_dmode == _GED_HIDDEN_LINE)
	    {
		struct _ged_client_data dgcdp_save;

		for (i = 0; i < argc; ++i) {
		    if (drawtrees_depth == 1)
			dgcdp.gdlp = dl_addToDisplay(gedp->ged_gdp->gd_headDisplay, gedp->dbip, argv[i]);

		    if (dgcdp.gdlp == GED_DISPLAY_LIST_NULL)
			continue;

		    dgcdp_save = dgcdp;

		    if (dgcdp.vs.s_dmode == _GED_SHADED_MODE_EVAL) {
			ret = plot_shaded_eval(gedp, argv[i], &dgcdp);
			if (ret == GED_OK) {
			    continue;
			}
			/* if evaluated shading failed, fall back to "all" mode */
			dgcdp.gedp->ged_gdp->gd_shaded_mode = 0;
			dgcdp.vs.s_dmode = _GED_SHADED_MODE_ALL;
		    }

		    av[0] = (char *)argv[i];
		    ret = db_walk_tree(gedp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &gedp->ged_wdbp->wdb_initial_tree_state,
				       NULL,
				       draw_check_region_end,
				       draw_check_leaf,
				       (void *)&dgcdp);

		    dgcdp = dgcdp_save;
		}
	    } else {
		struct display_list **paths_to_draw;
		struct display_list *gdlp;

		paths_to_draw = (struct display_list **)
		    bu_malloc(sizeof(struct display_list *) * argc,
		    "redraw paths");

		/* create solids */
		for (i = 0; i < argc; ++i) {
		    struct ged_solid_data bv_data;
		    bv_data.draw_solid_lines_only = dgcdp.vs.draw_solid_lines_only;
		    bv_data.wireframe_color_override = dgcdp.vs.color_override;
		    bv_data.wireframe_color[0]= dgcdp.vs.color[0];
		    bv_data.wireframe_color[1]= dgcdp.vs.color[1];
		    bv_data.wireframe_color[2]= dgcdp.vs.color[2];
		    bv_data.transparency= dgcdp.vs.transparency;
		    bv_data.dmode = dgcdp.vs.s_dmode;
		    bv_data.free_scene_obj = (void *)gedp->free_scene_obj;

		    dgcdp.gdlp = dl_addToDisplay(gedp->ged_gdp->gd_headDisplay, gedp->dbip, argv[i]);
		    bv_data.gdlp = dgcdp.gdlp;

		    /* store draw path */
		    paths_to_draw[i] = dgcdp.gdlp;

		    if (dgcdp.gdlp == GED_DISPLAY_LIST_NULL) {
			continue;
		    }

		    av[0] = (char *)argv[i];
		    ret = db_walk_tree(gedp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &gedp->ged_wdbp->wdb_initial_tree_state,
				       NULL,
				       wireframe_region_end,
				       append_solid_to_display_list,
				       (void *)&bv_data);
		}

		/* We need to know the view size in order to choose
		 * appropriate input values for the adaptive plot
		 * routines. Unless we're keeping the current view,
		 * we need to autoview now so we have the correct
		 * view size for plotting.
		 */
		if (dgcdp.autoview) {
		    const char *autoview_args[2] = {"autoview", NULL};
		    ged_autoview(gedp, 1, autoview_args);
		}

		/* Set the view threshold */
		if (gedp && gedp->ged_gvp) gedp->ged_gvp->gv_s->bot_threshold = bot_threshold;

		/* calculate plot vlists for solids of each draw path */
		for (i = 0; i < argc; ++i) {
		    gdlp = paths_to_draw[i];

		    if (gdlp == GED_DISPLAY_LIST_NULL) {
			continue;
		    }

		    ret = dl_redraw(gdlp, gedp, dgcdp.vs.draw_non_subtract_only);
		    if (ret < 0) {
			/* restore view bot threshold */
			if (gedp && gedp->ged_gvp) gedp->ged_gvp->gv_s->bot_threshold = threshold_cached;

			bu_vls_printf(gedp->ged_result_str, "%s: %s redraw failure\n", argv[0], argv[i]);
			return GED_ERROR;
		    }
		}

		/* restore view bot threshold */
		if (gedp && gedp->ged_gvp) gedp->ged_gvp->gv_s->bot_threshold = threshold_cached;

		bu_free(paths_to_draw, "draw paths");
	    }
	    break;
	case _GED_DRAW_NMG_POLY:
	    {
		nmg_model = nmg_mm();
		gedp->ged_wdbp->wdb_initial_tree_state.ts_m = &nmg_model;
		if (dgcdp.draw_edge_uses) {
		    bu_vls_printf(gedp->ged_result_str, "Doing the edgeuse thang (-u)\n");
		    dgcdp.draw_edge_uses_vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
		}

		for (i = 0; i < argc; ++i) {
		    if (drawtrees_depth == 1)
			dgcdp.gdlp = dl_addToDisplay(gedp->ged_gdp->gd_headDisplay, gedp->dbip, argv[i]);

		    if (dgcdp.gdlp == GED_DISPLAY_LIST_NULL)
			continue;

		    av[0] = (char *)argv[i];
		    ret = db_walk_tree(gedp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &gedp->ged_wdbp->wdb_initial_tree_state,
				       enable_fastpath ? draw_nmg_region_start : 0,
				       draw_nmg_region_end,
				       nmg_use_tnurbs ? nmg_booltree_leaf_tnurb : nmg_booltree_leaf_tess,
				       (void *)&dgcdp);
		}

		if (dgcdp.draw_edge_uses) {
		    _ged_cvt_vlblock_to_solids(gedp, dgcdp.draw_edge_uses_vbp, "_EDGEUSES_", 0);
		    bv_vlblock_free(dgcdp.draw_edge_uses_vbp);
		    dgcdp.draw_edge_uses_vbp = (struct bv_vlblock *)NULL;
		}

		/* Destroy NMG */
		nmg_km(nmg_model);
		break;
	    }
    }

    --drawtrees_depth;

    if (dgcdp.fastpath_count) {
	bu_log("%d region%s rendered through polygon fastpath\n",
	       dgcdp.fastpath_count, dgcdp.fastpath_count == 1 ? "" : "s");
    }

    if (ret < 0)
	return -1;

    return 0;	/* OK */
}


int
ged_draw_guts(struct ged *gedp, int argc, const char *argv[], int kind)
{
    size_t i;
    int drawtrees_retval;
    int flag_A_attr=0;
    int flag_o_nonunique=1;
    int last_opt=0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    static const char *usage = "<[-R -C#/#/# -s] objects> | <-o -A attribute name/value pairs>";

/* #define DEBUG_TIMING 1 */

#ifdef DEBUG_TIMING
    int64_t elapsedtime;
#endif

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

#ifdef DEBUG_TIMING
    elapsedtime = bu_gettime();
#endif

    /* skip past cmd */
    --argc;
    ++argv;

    /* check args for "-A" (attributes) and "-o" */
    for (i = 0; i < (size_t)argc; i++) {
	char *ptr_A=NULL;
	char *ptr_o=NULL;
	char *c;

	if (*argv[i] != '-') {
	    /* Done checking options. If our display is non-empty,
	     * add -R to keep current view.
	     */
	    if (BU_LIST_NON_EMPTY(gedp->ged_gdp->gd_headDisplay)) {
		bu_vls_strcat(&vls, " -R");
	    }
	    break;
	}

	ptr_A=strchr(argv[i], 'A');
	if (ptr_A)
	    flag_A_attr = 1;

	ptr_o = strchr(argv[i], 'o');
	if (ptr_o)
	    flag_o_nonunique = 2;

	last_opt = i;

	if (!ptr_A && !ptr_o) {
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, argv[i]);
	    continue;
	}

	if (strlen(argv[i]) == ((size_t)1 + (ptr_A != NULL) + (ptr_o != NULL))) {
	    /* argv[i] is just a "-A" or "-o" */
	    continue;
	}

	/* copy args other than "-A" or "-o" */
	bu_vls_putc(&vls, ' ');
	c = (char *)argv[i];
	while (*c != '\0') {
	    if (*c != 'A' && *c != 'o') {
		bu_vls_putc(&vls, *c);
	    }
	    c++;
	}
    }

    if (flag_A_attr) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	int max_count=0;
	int remaining_args=0;
	int new_argc=0;
	char **new_argv=NULL;
	struct bu_ptbl *tbl;

	remaining_args = argc - last_opt - 1;
	if (remaining_args < 2 || remaining_args%2) {
	    bu_vls_printf(gedp->ged_result_str, "Error: must have even number of arguments (name/value pairs)\n");
	    bu_vls_free(&vls);
	    return GED_ERROR;
	}

	bu_avs_init(&avs, (argc - last_opt)/2, "ged_draw_guts avs");
	i = 0;
	while (i < (size_t)argc) {
	    if (*argv[i] == '-') {
		i++;
		continue;
	    }

	    /* this is a name/value pair */
	    if (flag_o_nonunique == 2) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	    i += 2;
	}

	tbl = db_lookup_by_attr(gedp->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return GED_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return GED_OK;
	}
	for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, dp->d_namep);
	}

	max_count = BU_PTBL_LEN(tbl) + last_opt + 1;
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "ged_draw_guts ptbl");
	new_argv = (char **)bu_calloc(max_count+1, sizeof(char *), "ged_draw_guts new_argv");
	new_argc = bu_argv_from_string(new_argv, max_count, bu_vls_addr(&vls));

	/* First, delete any mention of these objects.
	 * Silently skip any leading options (which start with minus signs).
	 */
	for (i = 0; i < (size_t)new_argc; ++i) {
	    /* Skip any options */
	    if (new_argv[i][0] == '-') {
		/* If this option requires an argument which was
		 * provided separately (e.g. '-C 0/255/0' instead of
		 * '-C0/255/0'), skip the argument too.
		 */
		if (strlen(argv[i]) == 2 && strchr("mxCP", argv[i][1])) {
		    i++;
		}
		continue;
	    }

	    dl_erasePathFromDisplay(gedp, new_argv[i], 0);
	}

	drawtrees_retval = _ged_drawtrees(gedp, new_argc, (const char **)new_argv, kind, (struct _ged_client_data *)0);
	bu_vls_free(&vls);
	bu_free((char *)new_argv, "ged_draw_guts new_argv");
	if (drawtrees_retval) {
	    return GED_ERROR;
	}
    } else {
	int empty_display;
	bu_vls_free(&vls);

	empty_display = 1;
	if (BU_LIST_NON_EMPTY(gedp->ged_gdp->gd_headDisplay)) {
	    empty_display = 0;
	}

	/* First, delete any mention of these objects.
	 * Silently skip any leading options (which start with minus signs).
	 */
	for (i = 0; i < (size_t)argc; ++i) {
	    /* Skip any options */
	    if (argv[i][0] == '-') {
		/* If this option requires an argument which was
		 * provided separately (e.g. '-C 0/255/0' instead of
		 * '-C0/255/0'), skip the argument too.
		 */
		if (strlen(argv[i]) == 2 && strchr("mxCPL", argv[i][1])) {
		    i++;
		}
		continue;
	    }

	    dl_erasePathFromDisplay(gedp, argv[i], 0);
	}

	/* if our display is non-empty add -R to keep current view */
	if (!empty_display) {
	    int new_argc;
	    char **new_argv;

	    new_argc = argc + 1;
	    new_argv = (char **)bu_malloc(new_argc * sizeof(char *), "ged_draw_guts new_argv");

	    new_argv[0] = bu_strdup("-R");
	    for (i = 0; i < (size_t)argc; ++i) {
		new_argv[i + 1] = bu_strdup(argv[i]);
	    }

	    drawtrees_retval = _ged_drawtrees(gedp, new_argc, (const char **)new_argv, kind, (struct _ged_client_data *)0);

	    for (i = 0; i < (size_t)new_argc; ++i) {
		bu_free(new_argv[i], "ged_draw_guts new_argv[i] - bu_strdup(argv[i])");
	    }
	    bu_free(new_argv, "ged_draw_guts new_argv");
	} else {
	    drawtrees_retval = _ged_drawtrees(gedp, argc, argv, kind, (struct _ged_client_data *)0);
	}
	if (drawtrees_retval) {
	    return GED_ERROR;
	}
    }

#ifdef DEBUG_TIMING
    elapsedtime = bu_gettime() - elapsedtime;
    {
	int seconds = elapsedtime / 1000000;
	int minutes = seconds / 60;
	int hours = minutes / 60;

	minutes = minutes % 60;
	seconds = seconds %60;

	bu_vls_printf(gedp->ged_result_str, "Elapsed time: %02d:%02d:%02d\n", hours, minutes, seconds);
    }
#endif

    return GED_OK;
}


extern int ged_draw2_core(struct ged *gedp, int argc, const char *argv[]);
int
ged_draw_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
	return ged_draw2_core(gedp, argc, argv);

    return ged_draw_guts(gedp, argc, argv, _GED_DRAW_WIREFRAME);
}


int
ged_ev_core(struct ged *gedp, int argc, const char *argv[])
{
    return ged_draw_guts(gedp, argc, argv, _GED_DRAW_NMG_POLY);
}

extern int ged_redraw2_core(struct ged *gedp, int argc, const char *argv[]);
int
ged_redraw_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
	return ged_redraw2_core(gedp, argc, argv);

    int ret;
    struct display_list *gdlp;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    RT_CHECK_DBI(gedp->dbip);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	/* redraw everything */
	for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay))
	{
	    ret = dl_redraw(gdlp, gedp, 0);
	    if (ret < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: redraw failure\n", argv[0]);
		return GED_ERROR;
	    }
	}
    } else {
	int i, found_path;
	struct db_full_path obj_path, dl_path;

	/* redraw the specified paths */
	for (i = 1; i < argc; ++i) {
	    ret = db_string_to_path(&obj_path, gedp->dbip, argv[i]);
	    if (ret < 0) {
		bu_vls_printf(gedp->ged_result_str,
			"%s: %s is not a valid path\n", argv[0], argv[i]);
		return GED_ERROR;
	    }

	    found_path = 0;
	    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay))
	    {
		ret = db_string_to_path(&dl_path, gedp->dbip,
			bu_vls_addr(&gdlp->dl_path));
		if (ret < 0) {
		    bu_vls_printf(gedp->ged_result_str,
			    "%s: %s is not a valid path\n", argv[0],
			    bu_vls_addr(&gdlp->dl_path));
		    return GED_ERROR;
		}

		/* this display list path matches/contains the redraw path */
		if (db_full_path_match_top(&dl_path, &obj_path)) {
		    found_path = 1;
		    db_free_full_path(&dl_path);

		    ret = dl_redraw(gdlp, gedp, 0);
		    if (ret < 0) {
			bu_vls_printf(gedp->ged_result_str,
				"%s: %s redraw failure\n", argv[0], argv[i]);
			return GED_ERROR;
		    }
		    break;
		}
		db_free_full_path(&dl_path);
	    }

	    db_free_full_path(&obj_path);

	    if (!found_path) {
		bu_vls_printf(gedp->ged_result_str,
			"%s: %s is not being displayed\n", argv[0], argv[i]);
		return GED_ERROR;
	    }
	}
    }

    return GED_OK;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl draw_cmd_impl = {"draw", ged_draw_core, GED_CMD_DEFAULT};
const struct ged_cmd draw_cmd = { &draw_cmd_impl };

struct ged_cmd_impl e_cmd_impl = {"e", ged_draw_core, GED_CMD_DEFAULT};
const struct ged_cmd e_cmd = { &e_cmd_impl };

struct ged_cmd_impl ev_cmd_impl = {"ev", ged_ev_core, GED_CMD_DEFAULT};
const struct ged_cmd ev_cmd = { &ev_cmd_impl };

struct ged_cmd_impl redraw_cmd_impl = {"redraw", ged_redraw_core, GED_CMD_DEFAULT};
const struct ged_cmd redraw_cmd = { &redraw_cmd_impl };

extern int ged_loadview_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl loadview_cmd_impl = {"loadview", ged_loadview_core, GED_CMD_DEFAULT};
const struct ged_cmd loadview_cmd = { &loadview_cmd_impl };

extern int ged_preview_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl preview_cmd_impl = {"preview", ged_preview_core, GED_CMD_DEFAULT};
const struct ged_cmd preview_cmd = { &preview_cmd_impl };

const struct ged_cmd *draw_cmds[] = { &draw_cmd, &e_cmd, &ev_cmd, &redraw_cmd, &loadview_cmd, &preview_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  draw_cmds, 6 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
