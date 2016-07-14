/*                        D O D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2016 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"		/* for ID_POLY special support */
#include "raytrace.h"
#include "rt/db4.h"

#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"

void
cvt_vlblock_to_solids(struct bn_vlblock *vbp, const char *name, int copy)
{
    size_t i;
    char shortname[32];
    char namebuf[64];
    char *av[4];

    bu_strlcpy(shortname, name, sizeof(shortname));

    /* Remove any residue colors from a previous overlay w/same name */
    if (dbip->dbi_read_only) {
	av[0] = "erase";
	av[1] = shortname;
	av[2] = NULL;
	(void)ged_erase(gedp, 2, (const char **)av);
    } else {
	av[0] = "kill";
	av[1] = "-f";
	av[2] = shortname;
	av[3] = NULL;
	(void)ged_kill(gedp, 3, (const char **)av);
    }

    for (i=0; i < vbp->nused; i++) {
	if (BU_LIST_IS_EMPTY(&(vbp->head[i]))) continue;

	snprintf(namebuf, 32, "%s%lx",	shortname, vbp->rgb[i]);
	/*invent_solid(namebuf, &vbp->head[i], vbp->rgb[i], copy);*/
	invent_solid(gedp->ged_gdp->gd_headDisplay, dbip, createDListSolid, gedp->ged_free_vlist_callback, namebuf, &vbp->head[i], vbp->rgb[i], copy, 0.0,0, gedp->freesolid, 0);
    }
}


/*
 * Compute the min, max, and center points of the solid.
 * Also finds s_vlen;
 */
static void
mged_bound_solid(struct solid *sp)
{
    struct bn_vlist *vp;

    point_t bmin, bmax;
    int cmd;
    VSET(bmin, INFINITY, INFINITY, INFINITY);
    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);

    sp->s_vlen = 0;



    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
	cmd = bn_vlist_bbox(vp, &bmin, &bmax);
	if (cmd) {
	    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", cmd);
	    Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	}
	sp->s_vlen += vp->nused;
    }

    sp->s_center[X] = (bmin[X] + bmax[X]) * 0.5;
    sp->s_center[Y] = (bmin[Y] + bmax[Y]) * 0.5;
    sp->s_center[Z] = (bmin[Z] + bmax[Z]) * 0.5;

    sp->s_size = bmax[X] - bmin[X];
    V_MAX(sp->s_size, bmax[Y] - bmin[Y]);
    V_MAX(sp->s_size, bmax[Z] - bmin[Z]);
}


/*
 * Once the vlist has been created, perform the common tasks
 * in handling the drawn solid.
 *
 * This routine must be prepared to run in parallel.
 */
void
drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct solid *existing_sp)
{
    struct display_list *gdlp;
    struct solid *sp;

    if (!existing_sp) {
	/* Handling a new solid */
	GET_SOLID(sp, &gedp->freesolid->l);
	BU_LIST_APPEND(&gedp->freesolid->l, &((sp)->l) );
	sp->s_dlist = 0;
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
    mged_bound_solid(sp);

    /*
     * If this solid is new, fill in its information.
     * Otherwise, don't touch what is already there.
     */
    if (!existing_sp) {
	/* Take note of the base color */
	sp->s_uflag = 0;
	if (tsp) {
	    if (tsp->ts_mater.ma_color_valid) {
		sp->s_dflag = 0;	/* color specified in db */
	    } else {
		sp->s_dflag = 1;	/* default color */
	    }
	    /* Copy into basecolor anyway, to prevent black */
	    sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.0;
	    sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.0;
	    sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.0;
	}
	sp->s_cflag = 0;
	sp->s_iflag = DOWN;
	sp->s_soldash = dashflag;
	sp->s_Eflag = 0;	/* This is a solid */
	db_dup_full_path(&sp->s_fullpath, pathp);
	if (tsp)
	    sp->s_regionid = tsp->ts_regionid;
    }

    createDListSolid(sp);

    /* Solid is successfully drawn */
    if (!existing_sp) {
	/* Add to linked list of solid structs */
	bu_semaphore_acquire(RT_SEM_MODEL);

	/* Grab the last display list */
	gdlp = BU_LIST_PREV(display_list, gedp->ged_gdp->gd_headDisplay);
	BU_LIST_APPEND(gdlp->dl_headSolid.back, &sp->l);

	bu_semaphore_release(RT_SEM_MODEL);
    } else {
	/* replacing existing solid -- struct already linked in */
	sp->s_iflag = UP;
    }
}

/*
 * Given an existing solid structure that may have been subjected to
 * solid editing, recompute the vector list, etc., to make the solid
 * the same as it originally was.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
int
replot_original_solid(struct solid *sp)
{
    struct rt_db_internal intern;
    struct directory *dp;
    mat_t mat;

    if (dbip == DBI_NULL)
	return 0;

    dp = LAST_SOLID(sp);
    if (sp->s_Eflag) {
	Tcl_AppendResult(INTERP, "replot_original_solid(", dp->d_namep,
			 "): Unable to plot evaluated regions, skipping\n", (char *)NULL);
	return -1;
    }
    (void)db_path_to_mat(dbip, &sp->s_fullpath, mat, sp->s_fullpath.fp_len-1, &rt_uniresource);

    if (rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource) < 0) {
	Tcl_AppendResult(INTERP, dp->d_namep, ":  solid import failure\n", (char *)NULL);
	return -1;		/* ERROR */
    }
    RT_CK_DB_INTERNAL(&intern);

    if (replot_modified_solid(sp, &intern, bn_mat_identity) < 0) {
	rt_db_free_internal(&intern);
	return -1;
    }
    rt_db_free_internal(&intern);
    return 0;
}


/*
 * Given the solid structure of a solid that has already been drawn,
 * and a new database record and transform matrix,
 * create a new vector list for that solid, and substitute.
 * Used for solid editing mode.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
int
replot_modified_solid(
    struct solid *sp,
    struct rt_db_internal *ip,
    const mat_t mat)
{
    struct rt_db_internal intern;
    struct bu_list vhead;

    RT_DB_INTERNAL_INIT(&intern);

    BU_LIST_INIT(&vhead);

    if (sp == SOLID_NULL) {
	Tcl_AppendResult(INTERP, "replot_modified_solid() sp==NULL?\n", (char *)NULL);
	return -1;
    }

    /* Release existing vlist of this solid */
    RT_FREE_VLIST(&(sp->s_vlist));

    /* Draw (plot) a normal solid */
    RT_CK_DB_INTERNAL(ip);

    mged_ttol.magic = RT_TESS_TOL_MAGIC;
    mged_ttol.abs = mged_abs_tol;
    mged_ttol.rel = mged_rel_tol;
    mged_ttol.norm = mged_nrm_tol;

    transform_editing_solid(&intern, mat, ip, 0);

    if (OBJ[ip->idb_type].ft_plot(&vhead, &intern, &mged_ttol, &mged_tol, NULL) < 0) {
	Tcl_AppendResult(INTERP, LAST_SOLID(sp)->d_namep,
			 ": re-plot failure\n", (char *)NULL);
	return -1;
    }
    rt_db_free_internal(&intern);

    /* Write new displaylist */
    drawH_part2(sp->s_soldash, &vhead,
		(struct db_full_path *)0,
		(struct db_tree_state *)0, sp);

    view_state->vs_flag = 1;
    return 0;
}

void
add_solid_path_to_result(
    Tcl_Interp *interp,
    struct solid *sp)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    db_path_to_vls(&str, &sp->s_fullpath);
    Tcl_AppendResult(interp, bu_vls_addr(&str), " ", NULL);
    bu_vls_free(&str);
}

int
redraw_visible_objects(void)
{
    int ret, ac = 1;
    char *av[] = {NULL, NULL};

    av[0] = "redraw";
    ret = ged_redraw(gedp, ac, (const char **)av);

    if (ret == GED_ERROR) {
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
