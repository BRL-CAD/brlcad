/*                         D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "mater.h"
#include "solid.h"

#include "./ged_private.h"


/* declare our callbacks used by _ged_drawtrees() */
static int drawtrees_depth = 0;


static union tree *
draw_check_region_end(struct db_tree_state *tsp,
			 const struct db_full_path *pathp,
			 union tree *curtree,
			 genptr_t UNUSED(client_data))
{
    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);
    if (curtree) RT_CK_TREE(curtree);

    return curtree;
}


static union tree *
draw_check_leaf(struct db_tree_state *tsp,
		   const struct db_full_path *pathp,
		   struct rt_db_internal *ip,
		   genptr_t client_data)
{
    union tree *curtree;
    int ac = 1;
    const char *av[2];
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    av[0] = db_path_to_string(pathp);
    av[1] = (char *)0;

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    /*
     * Use gedp->ged_gdp->gd_shaded_mode if set and not being overridden. Otherwise use dgcdp->shaded_mode_override.
     */

    switch (dgcdp->dmode) {
	case _GED_SHADED_MODE_BOTS:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
		struct bu_list vhead;

		BU_LIST_INIT(&vhead);

		(void)rt_bot_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		_ged_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	    } else if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		       ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY) {
		struct bu_list vhead;

		BU_LIST_INIT(&vhead);

		(void)rt_pg_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		_ged_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	    } else {
		/* save shaded mode states */
		int save_shaded_mode = dgcdp->gedp->ged_gdp->gd_shaded_mode;
		int save_shaded_mode_override = dgcdp->shaded_mode_override;
		int save_dmode = dgcdp->dmode;

		/* turn shaded mode off for this non-bot/non-poly object */
		dgcdp->gedp->ged_gdp->gd_shaded_mode = 0;
		dgcdp->shaded_mode_override = -1;
		dgcdp->dmode = _GED_WIREFRAME;

		_ged_drawtrees(dgcdp->gedp, ac, av, 1, client_data);

		/* restore shaded mode states */
		dgcdp->gedp->ged_gdp->gd_shaded_mode = save_shaded_mode;
		dgcdp->shaded_mode_override = save_shaded_mode_override;
		dgcdp->dmode = save_dmode;
	    }

	    break;
	case _GED_SHADED_MODE_ALL:
	    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD &&
		ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
		if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
		    struct bu_list vhead;

		    BU_LIST_INIT(&vhead);

		    (void)rt_bot_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		    _ged_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
		} else if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_POLY) {
		    struct bu_list vhead;

		    BU_LIST_INIT(&vhead);

		    (void)rt_pg_plot_poly(&vhead, ip, tsp->ts_ttol, tsp->ts_tol);
		    _ged_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
		} else
		    _ged_drawtrees(dgcdp->gedp, ac, av, 3, client_data);
	    } else {
		/* save shaded mode states */
		int save_shaded_mode = dgcdp->gedp->ged_gdp->gd_shaded_mode;
		int save_shaded_mode_override = dgcdp->shaded_mode_override;
		int save_dmode = dgcdp->dmode;

		/* turn shaded mode off for this pipe object */
		dgcdp->gedp->ged_gdp->gd_shaded_mode = 0;
		dgcdp->shaded_mode_override = -1;
		dgcdp->dmode = _GED_WIREFRAME;

		_ged_drawtrees(dgcdp->gedp, ac, av, 1, client_data);

		/* restore shaded mode states */
		dgcdp->gedp->ged_gdp->gd_shaded_mode = save_shaded_mode;
		dgcdp->shaded_mode_override = save_shaded_mode_override;
		dgcdp->dmode = save_dmode;
	    }

	    break;
    }

    bu_free((genptr_t)av[0], "bot_check_leaf: av[0]");

    return curtree;
}


/**
 * Compute the min, max, and center points of the solid.  Also finds
 * s_vlen.
 *
 * XXX Should split out a separate bn_vlist_rpp() routine, for
 * librt/vlist.c
 */
static void
bound_solid(struct ged *gedp, struct solid *sp)
{
    struct bn_vlist *vp;
    double xmax, ymax, zmax;
    double xmin, ymin, zmin;

    xmax = ymax = zmax = -INFINITY;
    xmin = ymin = zmin =  INFINITY;
    sp->s_vlen = 0;
    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
	int j;
	int nused = vp->nused;
	int *cmd = vp->cmd;
	point_t *pt = vp->pt;
	for (j = 0; j < nused; j++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		    /* Has normal vector, not location */
		    break;
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		    V_MIN(xmin, (*pt)[X]);
		    V_MAX(xmax, (*pt)[X]);
		    V_MIN(ymin, (*pt)[Y]);
		    V_MAX(ymax, (*pt)[Y]);
		    V_MIN(zmin, (*pt)[Z]);
		    V_MAX(zmax, (*pt)[Z]);
		    break;
		default:
		    {
			bu_vls_printf(gedp->ged_result_str, "unknown vlist op %d\n", *cmd);
		    }
	    }
	}
	sp->s_vlen += nused;
    }

    sp->s_center[X] = (xmin + xmax) * 0.5;
    sp->s_center[Y] = (ymin + ymax) * 0.5;
    sp->s_center[Z] = (zmin + zmax) * 0.5;

    sp->s_size = xmax - xmin;
    V_MAX(sp->s_size, ymax - ymin);
    V_MAX(sp->s_size, zmax - zmin);
}


/**
 * G E D _ D R A W H _ P A R T 2
 *
 * Once the vlist has been created, perform the common tasks
 * in handling the drawn solid.
 *
 * This routine must be prepared to run in parallel.
 */
void
_ged_drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct solid *existing_sp, struct _ged_client_data *dgcdp)
{
    struct solid *sp;

    if (!existing_sp) {
	/* Handling a new solid */
	GET_SOLID(sp, &_FreeSolid.l);
	/* NOTICE:  The structure is dirty & not initialized for you! */

	if (BU_LIST_NON_EMPTY(&dgcdp->gdlp->gdl_headSolid)) {
	    sp->s_dlist = BU_LIST_LAST(solid, &dgcdp->gdlp->gdl_headSolid)->s_dlist + 1;
	} else
	    sp->s_dlist = 1;
    } else {
	/* Just updating an existing solid.
	 * 'tsp' and 'pathpos' will not be used
	 */
	sp = existing_sp;
    }


    /*
     * Compute the min, max, and center points.
     */
    BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
    bound_solid(dgcdp->gedp, sp);

    /*
     * If this solid is new, fill in its information.
     * Otherwise, don't touch what is already there.
     */
    if (!existing_sp) {
	/* Take note of the base color */
	if (dgcdp->wireframe_color_override) {
	    /* a user specified the color, so arrange to use it */
	    sp->s_uflag = 1;
	    sp->s_dflag = 0;
	    sp->s_basecolor[0] = dgcdp->wireframe_color[0];
	    sp->s_basecolor[1] = dgcdp->wireframe_color[1];
	    sp->s_basecolor[2] = dgcdp->wireframe_color[2];
	} else {
	    sp->s_uflag = 0;
	    if (tsp) {
		if (tsp->ts_mater.ma_color_valid) {
		    sp->s_dflag = 0;	/* color specified in db */
		    sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
		    sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
		    sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
		} else {
		    sp->s_dflag = 1;	/* default color */
		    sp->s_basecolor[0] = 255;
		    sp->s_basecolor[1] = 0;
		    sp->s_basecolor[2] = 0;
		}
	    }
	}
	sp->s_cflag = 0;
	sp->s_flag = DOWN;
	sp->s_iflag = DOWN;
	sp->s_soldash = dashflag;
	sp->s_Eflag = 0;	/* This is a solid */
	db_dup_full_path(&sp->s_fullpath, pathp);
	sp->s_regionid = tsp->ts_regionid;
	sp->s_transparency = dgcdp->transparency;
	sp->s_dmode = dgcdp->dmode;
	sp->s_hiddenLine = dgcdp->hiddenLine;

	/* Add to linked list of solid structs */
	bu_semaphore_acquire(RT_SEM_MODEL);
	BU_LIST_APPEND(dgcdp->gdlp->gdl_headSolid.back, &sp->l);
	bu_semaphore_release(RT_SEM_MODEL);
    }
}


static union tree *
wireframe_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t UNUSED(client_data))
{
    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);
    if (curtree) RT_CK_TREE(curtree);

    return curtree;
}


/**
 * G E D _ W I R E F R A M E _ L E A F
 *
 * This routine must be prepared to run in parallel.
 */
static union tree *
wireframe_leaf(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
    int dashflag; /* draw with dashed lines */
    union tree *curtree;
    struct bu_list vhead;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_RESOURCE(tsp->ts_resp);
    if (!dgcdp) return TREE_NULL;

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "wireframe_leaf(%s) path='%s'\n",
		      ip->idb_meth->ft_name, sofar);
	bu_free((genptr_t)sofar, "path string");
    }

    if (dgcdp->draw_solid_lines_only)
	dashflag = 0;
    else
	dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));

    if (!ip->idb_meth->ft_plot
	|| ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol, tsp->ts_tol) < 0)
    {
	bu_vls_printf(dgcdp->gedp->ged_result_str, "%s: plot failure\n", DB_FULL_PATH_CUR_DIR(pathp)->d_namep);
	return TREE_NULL;		/* ERROR */
    }

    /*
     * XXX HACK CTJ - _ged_drawH_part2 sets the default color of a
     * solid by looking in tps->ts_mater.ma_color, for pseudo
     * solids, this needs to be something different and drawH
     * has no idea or need to know what type of solid this is.
     */
    if (ip->idb_type == ID_GRIP) {
	int r, g, b;
	r= tsp->ts_mater.ma_color[0];
	g= tsp->ts_mater.ma_color[1];
	b= tsp->ts_mater.ma_color[2];
	tsp->ts_mater.ma_color[0] = 0;
	tsp->ts_mater.ma_color[1] = 128;
	tsp->ts_mater.ma_color[2] = 128;
	_ged_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	tsp->ts_mater.ma_color[0] = r;
	tsp->ts_mater.ma_color[1] = g;
	tsp->ts_mater.ma_color[2] = b;
    } else {
	_ged_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
    }

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    return curtree;
}


/**
 * G E D _ N M G _ R E G I O N _ S T A R T
 *
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
draw_nmg_region_start(struct db_tree_state *tsp, const struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
    union tree *tp;
    struct directory *dp;
    struct rt_db_internal intern;
    mat_t xform;
    matp_t matp;
    struct bu_list vhead;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);
	bu_vls_printf(dgcdp->gedp->ged_result_str, "nmg_region_start(%s)\n", sofar);
	bu_free((genptr_t)sofar, "path string");
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
    if (tsp->ts_mat) {
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
		if (RT_G_DEBUG&DEBUG_TREEWALK) {
		    bu_log("fastpath draw ID_POLY %s\n", dp->d_namep);
		}
		if (dgcdp->draw_wireframes) {
		    (void)rt_pg_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
		} else {
		    (void)rt_pg_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
		}
	    }
	    goto out;
	case ID_BOT:
	    {
		if (RT_G_DEBUG&DEBUG_TREEWALK) {
		    bu_log("fastpath draw ID_BOT %s\n", dp->d_namep);
		}
		if (dgcdp->draw_wireframes) {
		    (void)rt_bot_plot(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
		} else {
		    (void)rt_bot_plot_poly(&vhead, &intern, tsp->ts_ttol, tsp->ts_tol);
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
	_ged_drawH_part2(0, &vhead, &pp, tsp, SOLID_NULL, dgcdp);

	db_free_full_path(&pp);
    }

    rt_db_free_internal(&intern);
    dgcdp->fastpath_count++;
    return -1;	/* SKIP THIS REGION */
}


static int
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct _ged_client_data *dgcdp)
{
    int result = 1;

    if (!BU_SETJUMP) {
	/* try */

	result = nmg_boolean(curtree, *tsp->ts_m, tsp->ts_tol, tsp->ts_resp);

    } else {
	/* catch */
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "WARNING: Boolean evaluation of %s failed!!!\n", sofar);
	bu_free((genptr_t)sofar, "path string");
    } BU_UNSETJUMP;

    return result;
}


static int
process_triangulation(struct db_tree_state *tsp, const struct db_full_path *pathp, struct _ged_client_data *dgcdp)
{
    int result = 1;

    if (!BU_SETJUMP) {
	/* try */

	nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
	result = 0;

    } else {
	/* catch */

	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "WARNING: Triangulation of %s failed!!!\n", sofar);
	bu_free((genptr_t)sofar, "path string");

    } BU_UNSETJUMP;

    return result;
}


/**
 * G E D _ N M G _ R E G I O N _ E N D
 *
 * This routine must be prepared to run in parallel.
 */
static union tree *
draw_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    struct nmgregion *r;
    struct bu_list vhead;
    int failed;
    struct _ged_client_data *dgcdp = (struct _ged_client_data *)client_data;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);
    RT_CK_RESOURCE(tsp->ts_resp);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "nmg_region_end() path='%s'\n", sofar);
	bu_free((genptr_t)sofar, "path string");
    } else {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(dgcdp->gedp->ged_result_str, "%s:\n", sofar);
	bu_free((genptr_t)sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP) return curtree;

    failed = 1;
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

	if (dgcdp->draw_wireframes) {
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
	nmg_r_to_vlist(&vhead, r, style);

	_ged_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);

	if (dgcdp->draw_edge_uses) {
	    nmg_vlblock_r(dgcdp->draw_edge_uses_vbp, r, 1);
	}
	/* NMG region is no longer necessary, only vlist remains */
	db_free_tree(curtree, tsp->ts_resp);
	return (union tree *)NULL;
    }

    /* Return tree -- it needs to be freed (by caller) */
    return curtree;
}


/**
 * C V T _ V L B L O C K _ T O _ S O L I D S
 */
void
_ged_cvt_vlblock_to_solids(struct ged *gedp, struct bn_vlblock *vbp, char *name, int copy)
{
    size_t i;
    char shortname[32];
    char namebuf[64];

    bu_strlcpy(shortname, name, sizeof(shortname));

    for (i=0; i < vbp->nused; i++) {
	if (BU_LIST_IS_EMPTY(&(vbp->head[i])))
	    continue;

	snprintf(namebuf, 64, "%s%lx", shortname, vbp->rgb[i]);
	_ged_invent_solid(gedp, namebuf, &vbp->head[i], vbp->rgb[i], copy, 0.0, 0);
    }
}


/*
 * G E D _ D R A W T R E E S
 *
 * This routine is the drawable geometry object's analog of rt_gettrees().
 * Add a set of tree hierarchies to the active set.
 * Note that argv[0] should be ignored, it has the command name in it.
 *
 * Kind =
 * 1 regular wireframes
 * 2 big-E
 * 3 NMG polygons
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
    struct _ged_client_data *dgcdp;
    int i;
    int ac = 1;
    char *av[2];

    RT_CHECK_DBI(gedp->ged_wdbp->dbip);

    if (argc <= 0)
	return -1;	/* FAIL */

    ++drawtrees_depth;
    av[1] = (char *)0;

    /* options are already parsed into _dgcdp */
    if (_dgcdp != (struct _ged_client_data *)0) {
	BU_GETSTRUCT(dgcdp, _ged_client_data);
	*dgcdp = *_dgcdp;            /* struct copy */
    } else {

	BU_GETSTRUCT(dgcdp, _ged_client_data);
	dgcdp->gedp = gedp;

	/* Initial values for options, must be reset each time */
	dgcdp->draw_nmg_only = 0;	/* no booleans */
	dgcdp->nmg_triangulate = 1;
	dgcdp->draw_wireframes = 0;
	dgcdp->draw_normals = 0;
	dgcdp->draw_solid_lines_only = 0;
	dgcdp->draw_no_surfaces = 0;
	dgcdp->shade_per_vertex_normals = 0;
	dgcdp->draw_edge_uses = 0;
	dgcdp->wireframe_color_override = 0;
	dgcdp->fastpath_count = 0;

	/* default color - red */
	dgcdp->wireframe_color[0] = 255;
	dgcdp->wireframe_color[1] = 0;
	dgcdp->wireframe_color[2] = 0;

	/* default transparency - opaque */
	dgcdp->transparency = 1.0;

	/* -1 indicates flag not set */
	dgcdp->shaded_mode_override = -1;

	enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 0;		/* re-init bu_getopt() */
	while ((c = bu_getopt(argc, (char * const *)argv, "dfhm:nqstuvwx:C:STP:A:oR")) != -1) {
	    switch (c) {
		case 'u':
		    dgcdp->draw_edge_uses = 1;
		    break;
		case 's':
		    dgcdp->draw_solid_lines_only = 1;
		    break;
		case 't':
		    nmg_use_tnurbs = 1;
		    break;
		case 'v':
		    dgcdp->shade_per_vertex_normals = 1;
		    break;
		case 'w':
		    dgcdp->draw_wireframes = 1;
		    break;
		case 'S':
		    dgcdp->draw_no_surfaces = 1;
		    break;
		case 'T':
		    dgcdp->nmg_triangulate = 0;
		    break;
		case 'n':
		    dgcdp->draw_normals = 1;
		    break;
		case 'P':
		    ncpu = atoi(bu_optarg);
		    break;
		case 'q':
		    dgcdp->do_not_draw_nmg_solids_during_debugging = 1;
		    break;
		case 'd':
		    dgcdp->draw_nmg_only = 1;
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

			dgcdp->wireframe_color_override = 1;
			dgcdp->wireframe_color[0] = r;
			dgcdp->wireframe_color[1] = g;
			dgcdp->wireframe_color[2] = b;
		    }
		    break;
		case 'h':
		    dgcdp->hiddenLine = 1;
		    dgcdp->shaded_mode_override = _GED_SHADED_MODE_ALL;
		    break;
		case 'm':
		    /* clamp it to [-infinity, 2] */
		    dgcdp->shaded_mode_override = atoi(bu_optarg);
		    if (2 < dgcdp->shaded_mode_override)
			dgcdp->shaded_mode_override = 2;

		    break;
		case 'x':
		    dgcdp->transparency = atof(bu_optarg);

		    /* clamp it to [0, 1] */
		    if (dgcdp->transparency < 0.0)
			dgcdp->transparency = 0.0;

		    if (1.0 < dgcdp->transparency)
			dgcdp->transparency = 1.0;

		    break;
		case 'A':
		case 'o':
		case 'R':
		    /* nothing to do, handled by edit_com wrapper on the front-end */
		    break;
		default:
		    {
			bu_vls_printf(gedp->ged_result_str, "unrecognized option - %c\n", c);
			bu_free((genptr_t)dgcdp, "_ged_drawtrees: dgcdp");
			--drawtrees_depth;
			return GED_ERROR;
		    }
	    }
	}
	argc -= bu_optind;
	argv += bu_optind;

	switch (kind) {
	    case 1:
		if (gedp->ged_gdp->gd_shaded_mode && dgcdp->shaded_mode_override < 0) {
		    dgcdp->dmode = gedp->ged_gdp->gd_shaded_mode;
		} else if (0 <= dgcdp->shaded_mode_override)
		    dgcdp->dmode = dgcdp->shaded_mode_override;
		else
		    dgcdp->dmode = _GED_WIREFRAME;

		break;
	    case 2:
	    case 3:
		dgcdp->dmode = _GED_BOOL_EVAL;
		break;
	}

    }

    switch (kind) {
	default:
	    bu_vls_printf(gedp->ged_result_str, "ERROR, bad kind\n");
	    bu_free((genptr_t)dgcdp, "_ged_drawtrees: dgcdp");
	    --drawtrees_depth;
	    return -1;
	case 1:		/* Wireframes */
	    {
		union tree *(*reg_end_func) (struct db_tree_state *, const struct db_full_path *, union tree *, genptr_t);
		union tree *(*leaf_func) (struct db_tree_state *, const struct db_full_path *, struct rt_db_internal *, genptr_t);

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
		if (_GED_SHADED_MODE_BOTS <= dgcdp->dmode && dgcdp->dmode <= _GED_SHADED_MODE_ALL) {
		    reg_end_func = draw_check_region_end;
		    leaf_func = draw_check_leaf;
		} else {
		    reg_end_func = wireframe_region_end;
		    leaf_func = wireframe_leaf;
		}

		for (i = 0; i < argc; ++i) {
		    if (drawtrees_depth == 1)
			dgcdp->gdlp = ged_addToDisplay(gedp, argv[i]);

		    if (dgcdp->gdlp == GED_DISPLAY_LIST_NULL)
			continue;

		    av[0] = (char *)argv[i];
		    ret = db_walk_tree(gedp->ged_wdbp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &gedp->ged_wdbp->wdb_initial_tree_state,
				       0,
				       reg_end_func,
				       leaf_func,
				       (genptr_t)dgcdp);
		}
	    }
	    break;
	case 2:		/* Big-E */
	    bu_vls_printf(gedp->ged_result_str, "drawtrees:  can't do big-E here\n");
	    bu_free((genptr_t)dgcdp, "_ged_drawtrees: dgcdp");
	    --drawtrees_depth;
	    return -1;
	case 3:
	    {
		/* NMG */
		nmg_model = nmg_mm();
		gedp->ged_wdbp->wdb_initial_tree_state.ts_m = &nmg_model;
		if (dgcdp->draw_edge_uses) {
		    bu_vls_printf(gedp->ged_result_str, "Doing the edgeuse thang (-u)\n");
		    dgcdp->draw_edge_uses_vbp = rt_vlblock_init();
		}

		for (i = 0; i < argc; ++i) {
		    if (drawtrees_depth == 1)
			dgcdp->gdlp = ged_addToDisplay(gedp, argv[i]);

		    if (dgcdp->gdlp == GED_DISPLAY_LIST_NULL)
			continue;

		    av[0] = (char *)argv[i];
		    ret = db_walk_tree(gedp->ged_wdbp->dbip,
				       ac,
				       (const char **)av,
				       ncpu,
				       &gedp->ged_wdbp->wdb_initial_tree_state,
				       enable_fastpath ? draw_nmg_region_start : 0,
				       draw_nmg_region_end,
				       nmg_use_tnurbs ? nmg_booltree_leaf_tnurb : nmg_booltree_leaf_tess,
				       (genptr_t)dgcdp);
		}

		if (dgcdp->draw_edge_uses) {
		    _ged_cvt_vlblock_to_solids(gedp, dgcdp->draw_edge_uses_vbp, "_EDGEUSES_", 0);
		    rt_vlblock_free(dgcdp->draw_edge_uses_vbp);
		    dgcdp->draw_edge_uses_vbp = (struct bn_vlblock *)NULL;
		}

		/* Destroy NMG */
		nmg_km(nmg_model);
		break;
	    }
    }

    --drawtrees_depth;

    if (dgcdp->fastpath_count) {
	bu_log("%d region%s rendered through polygon fastpath\n",
	       dgcdp->fastpath_count, dgcdp->fastpath_count==1?"":"s");
    }

    bu_free((genptr_t)dgcdp, "_ged_drawtrees: dgcdp");

    if (ret < 0)
	return -1;

    return 0;	/* OK */
}


/*
 * I N V E N T _ S O L I D
 *
 * Invent a solid by adding a fake entry in the database table,
 * adding an entry to the solid table, and populating it with
 * the given vector list.
 *
 * This parallels much of the code in dodraw.c
 */
int
_ged_invent_solid(struct ged *gedp,
		  char *name,
		  struct bu_list *vhead,
		  long int rgb,
		  int copy,
		  fastf_t transparency,
		  int dmode)
{
    struct directory *dp;
    struct solid *sp;
    struct ged_display_list *gdlp;
    unsigned char type='0';

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return 0;

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	if (dp->d_addr != RT_DIR_PHONY_ADDR) {
	    bu_vls_printf(gedp->ged_result_str,
			  "_ged_invent_solid(%s) would clobber existing database entry, ignored\n", name);
	    return -1;
	}

	/*
	 * Name exists from some other overlay,
	 * zap any associated solids
	 */
	ged_erasePathFromDisplay(gedp, name, 0);
    }
    /* Need to enter phony name in directory structure */
    dp = db_diradd(gedp->ged_wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&type);

    /* Obtain a fresh solid structure, and fill it in */
    GET_SOLID(sp, &_FreeSolid.l);

    if (copy) {
	BU_LIST_INIT(&(sp->s_vlist));
	rt_vlist_copy(&(sp->s_vlist), vhead);
    } else {
	/* For efficiency, just swipe the vlist */
	BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
	BU_LIST_INIT(vhead);
    }
    bound_solid(gedp, sp);

    /* set path information -- this is a top level node */
    db_add_node_to_full_path(&sp->s_fullpath, dp);

    gdlp = ged_addToDisplay(gedp, name);

    sp->s_iflag = DOWN;
    sp->s_soldash = 0;
    sp->s_Eflag = 1;		/* Can't be solid edited! */
    sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
    sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
    sp->s_color[2] = sp->s_basecolor[2] = (rgb) & 0xFF;
    sp->s_regionid = 0;

    if (BU_LIST_NON_EMPTY(&gdlp->gdl_headSolid)) {
	sp->s_dlist = BU_LIST_LAST(solid, &gdlp->gdl_headSolid)->s_dlist + 1;
    } else
	sp->s_dlist = 1;

    sp->s_uflag = 0;
    sp->s_dflag = 0;
    sp->s_cflag = 0;
    sp->s_wflag = 0;

    sp->s_transparency = transparency;
    sp->s_dmode = dmode;

    /* Solid successfully drawn, add to linked list of solid structs */
    BU_LIST_APPEND(gdlp->gdl_headSolid.back, &sp->l);

    return 0;		/* OK */
}


/*
 * C O L O R _ S O L T A B
 *
 * Pass through the solid table and set pointer to appropriate
 * mater structure.
 */
void
ged_color_soltab(struct bu_list *hdlp)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    const struct mater *mp;

    gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    sp->s_cflag = 0;

	    /* the user specified the color, so use it */
	    if (sp->s_uflag) {
		sp->s_color[0] = sp->s_basecolor[0];
		sp->s_color[1] = sp->s_basecolor[1];
		sp->s_color[2] = sp->s_basecolor[2];

		continue;
	    }

	    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		if (sp->s_regionid <= mp->mt_high &&
		    sp->s_regionid >= mp->mt_low) {
		    sp->s_color[0] = mp->mt_r;
		    sp->s_color[1] = mp->mt_g;
		    sp->s_color[2] = mp->mt_b;

		    goto done;
		}
	    }

	    /*
	     * There is no region-id-based coloring entry in the
	     * table, so use the combination-record ("mater"
	     * command) based color if one was provided. Otherwise,
	     * use the default wireframe color.
	     * This is the "new way" of coloring things.
	     */

	    /* use wireframe_default_color */
	    if (sp->s_dflag)
		sp->s_cflag = 1;
	    /* Be conservative and copy color anyway, to avoid black */
	    sp->s_color[0] = sp->s_basecolor[0];
	    sp->s_color[1] = sp->s_basecolor[1];
	    sp->s_color[2] = sp->s_basecolor[2];

	done: ;
	}

	gdlp = next_gdlp;
    }
}


int
ged_draw_guts(struct ged *gedp, int argc, const char *argv[], int kind)
{
    size_t i;
    int flag_A_attr=0;
    int flag_o_nonunique=1;
    int last_opt=0;
    struct bu_vls vls;
    static const char *usage = "<[-R -C#/#/# -s] objects> | <-o -A attribute name/value pairs>";

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

    /* skip past cmd */
    --argc;
    ++argv;

    /* check args for "-A" (attributes) and "-o" */
    bu_vls_init(&vls);
    for (i=0; i<(size_t)argc; i++) {
	char *ptr_A=NULL;
	char *ptr_o=NULL;
	char *c;

	if (*argv[i] != '-')
	    break;

	ptr_A=strchr(argv[i], 'A');
	if (ptr_A)
	    flag_A_attr = 1;

	ptr_o=strchr(argv[i], 'o');
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

	tbl = db_lookup_by_attr(gedp->ged_wdbp->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return TCL_OK;
	}
	for (i=0; i<BU_PTBL_LEN(tbl); i++) {
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
	    if (new_argv[i][0] == '-')
		continue;

	    ged_erasePathFromDisplay(gedp, new_argv[i], 0);
	}

	_ged_drawtrees(gedp, new_argc, (const char **)new_argv, kind, (struct _ged_client_data *)0);
	bu_vls_free(&vls);
	bu_free((char *)new_argv, "ged_draw_guts new_argv");
    } else {
	bu_vls_free(&vls);

	/* First, delete any mention of these objects.
	 * Silently skip any leading options (which start with minus signs).
	 */
	for (i = 0; i < (size_t)argc; ++i) {
	    /* Skip any options */
	    if (argv[i][0] == '-')
		continue;

	    ged_erasePathFromDisplay(gedp, argv[i], 0);
	}

	_ged_drawtrees(gedp, argc, argv, kind, (struct _ged_client_data *)0);
    }

    ged_color_soltab(&gedp->ged_gdp->gd_headDisplay);

    return GED_OK;
}


int
ged_draw(struct ged *gedp, int argc, const char *argv[])
{
    return ged_draw_guts(gedp, argc, argv, 1);
}


int
ged_ev(struct ged *gedp, int argc, const char *argv[])
{
    return ged_draw_guts(gedp, argc, argv, 3);
}


struct ged_display_list *
ged_addToDisplay(struct ged *gedp,
		 const char *name)
{
    struct directory *dp = NULL;
    struct ged_display_list *gdlp = NULL;
    char *cp = NULL;
    int found_namepath = 0;
    struct db_full_path namepath;

    cp = strrchr(name, '/');
    if (!cp)
	cp = (char *)name;
    else
	++cp;

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, cp, LOOKUP_NOISY)) == RT_DIR_NULL) {
	gdlp = GED_DISPLAY_LIST_NULL;
	goto end;
    }

    if (db_string_to_path(&namepath, gedp->ged_wdbp->dbip, name) == 0)
	found_namepath = 1;

    /* Make sure name is not already in the list */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	if (BU_STR_EQUAL(name, bu_vls_addr(&gdlp->gdl_path)))
	    goto end;

	/*
	 */
	if (found_namepath) {
	    struct db_full_path gdlpath;

	    if (db_string_to_path(&gdlpath, gedp->ged_wdbp->dbip, bu_vls_addr(&gdlp->gdl_path)) == 0) {
		if (db_full_path_match_top(&gdlpath, &namepath)) {
		    db_free_full_path(&gdlpath);
		    goto end;
		}

		db_free_full_path(&gdlpath);
	    }
	}

	gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);
    }

    BU_GETSTRUCT(gdlp, ged_display_list);
    BU_LIST_INSERT(&gedp->ged_gdp->gd_headDisplay, &gdlp->l);
    BU_LIST_INIT(&gdlp->gdl_headSolid);
    gdlp->gdl_dp = dp;
    bu_vls_init(&gdlp->gdl_path);
    bu_vls_printf(&gdlp->gdl_path, "%s", name);

end:
    if (found_namepath)
	db_free_full_path(&namepath);

    return gdlp;
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
