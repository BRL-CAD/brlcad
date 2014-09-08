/*                  D I S P L A Y _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/display_list.c
 *
 * Collect display list manipulation logic here as it is refactored.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "solid.h"

#include "./ged_private.h"

void
headsolid_split(struct bu_list *hdlp, struct db_i *dbip, struct solid *sp, int newlen)
{
    int savelen;
    struct display_list *new_gdlp;
    char *pathname;

    savelen = sp->s_fullpath.fp_len;
    sp->s_fullpath.fp_len = newlen;
    pathname = db_path_to_string(&sp->s_fullpath);
    sp->s_fullpath.fp_len = savelen;

    new_gdlp = dl_addToDisplay(hdlp, dbip, pathname);
    bu_free((void *)pathname, "headsolid_split pathname");

    BU_LIST_DEQUEUE(&sp->l);
    BU_LIST_INSERT(&new_gdlp->dl_headSolid, &sp->l);
}



int
headsolid_splitGDL(struct bu_list *hdlp, struct db_i *dbip, struct display_list *gdlp, struct db_full_path *path)
{
    struct solid *sp;
    struct solid *nsp;
    int newlen = path->fp_len + 1;

    if (BU_LIST_IS_EMPTY(&gdlp->dl_headSolid)) return 0;

    if (newlen < 3) {
	while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
	    headsolid_split(hdlp, dbip, sp, newlen);
	}
    } else {
	sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	    nsp = BU_LIST_PNEXT(solid, sp);
	    if (db_full_path_match_top(path, &sp->s_fullpath)) {
		headsolid_split(hdlp, dbip, sp, newlen);
	    }
	    sp = nsp;
	}

	--path->fp_len;
	headsolid_splitGDL(hdlp, dbip, gdlp, path);
	++path->fp_len;
    }

    return 1;
}


int
dl_bounding_sph(struct bu_list *hdlp, vect_t *min, vect_t *max)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    vect_t minus, plus;
    int is_empty = 1;

    VSETALL((*min),  INFINITY);
    VSETALL((*max), -INFINITY);

    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    minus[X] = sp->s_center[X] - sp->s_size;
	    minus[Y] = sp->s_center[Y] - sp->s_size;
	    minus[Z] = sp->s_center[Z] - sp->s_size;
	    VMIN((*min), minus);
	    plus[X] = sp->s_center[X] + sp->s_size;
	    plus[Y] = sp->s_center[Y] + sp->s_size;
	    plus[Z] = sp->s_center[Z] + sp->s_size;
	    VMAX((*max), plus);

	    is_empty = 0;
	}

	gdlp = next_gdlp;
    }

    return is_empty;
}

struct bu_ptbl *
dl_get_solids(struct display_list *gdlp)
{
    struct solid *sp;
    struct bu_ptbl *solids = NULL;
    BU_ALLOC(solids, struct bu_ptbl);
    bu_ptbl_init(solids, 8, "initialize ptr table");
    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	bu_ptbl_ins(solids, (long *)(&(sp->s_fullpath)));
    }

    return solids;
}

int
dl_get_color(long *curr_solid, int color)
{
    struct solid *sp = (struct solid *)curr_solid;
    if (color == RED) return sp->s_color[0];
    if (color == GRN) return sp->s_color[1];
    if (color == BLU) return sp->s_color[2];
    return 0;
}

struct directory *
dl_get_dp(long *curr_solid)
{
    struct solid *sp = (struct solid *)curr_solid;
    if (UNLIKELY(!sp)) return NULL;
    return sp->s_fullpath.fp_names[sp->s_fullpath.fp_len-1];
}


fastf_t
dl_get_transparency(long *curr_solid)
{
    struct solid *sp = (struct solid *)curr_solid;
    if (UNLIKELY(!sp)) return 0.0;
    return sp->s_transparency;
}

/*
 * Erase/remove the display list item from headDisplay if path matches the list item's path.
 *
 */
void
dl_erasePathFromDisplay(struct bu_list *hdlp,
	struct db_i *dbip, void (*callback)(unsigned int, int),
       	const char *path, int allow_split)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct display_list *last_gdlp;
    struct solid *sp;
    struct directory *dp;
    struct db_full_path subpath;
    int found_subpath;

    if (db_string_to_path(&subpath, dbip, path) == 0)
	found_subpath = 1;
    else
	found_subpath = 0;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    last_gdlp = BU_LIST_LAST(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_STR_EQUAL(path, bu_vls_addr(&gdlp->dl_path))) {
	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL) {

		/* We can't assume the display lists are contiguous */
		FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		    (*callback)(BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist, 1);
		}
	    }

	    /* Free up the solids list associated with this display list */
	    while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		    (void)db_dirdelete(dbip, dp);
		}

		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp);
	    }

	    BU_LIST_DEQUEUE(&gdlp->l);
	    bu_vls_free(&gdlp->dl_path);
	    free((void *)gdlp);

	    break;
	} else if (found_subpath) {
	    int need_split = 0;
	    struct solid *nsp;

	    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
	    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
		nsp = BU_LIST_PNEXT(solid, sp);

		if (db_full_path_match_top(&subpath, &sp->s_fullpath)) {
		    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
			(*callback)(sp->s_dlist, 1);

		    BU_LIST_DEQUEUE(&sp->l);
		    FREE_SOLID(sp);
		    need_split = 1;
		}

		sp = nsp;
	    }

	    if (BU_LIST_IS_EMPTY(&gdlp->dl_headSolid)) {
		BU_LIST_DEQUEUE(&gdlp->l);
		bu_vls_free(&gdlp->dl_path);
		free((void *)gdlp);
	    } else if (allow_split && need_split) {
		BU_LIST_DEQUEUE(&gdlp->l);

		--subpath.fp_len;
		(void)headsolid_splitGDL(hdlp, dbip, gdlp, &subpath);
		++subpath.fp_len;

		/* Free up the display list */
		bu_vls_free(&gdlp->dl_path);
		free((void *)gdlp);
	    }
	}

	if (gdlp == last_gdlp)
	    gdlp = (struct display_list *)hdlp;
	else
	    gdlp = next_gdlp;
    }

    if (found_subpath)
	db_free_full_path(&subpath);
}


HIDDEN void
eraseAllSubpathsFromSolidList(struct display_list *gdlp,
			      struct db_full_path *subpath,
			      void (*callback)(unsigned int, int),
			      const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;

    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
		(*callback)(sp->s_dlist, 1);

	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp);
	}
	sp = nsp;
    }
}


/*
 * Erase/remove display list item from headDisplay if name is found anywhere along item's path with
 * the exception that the first path element is skipped if skip_first is true.
 *
 * Note - name is not expected to contain path separators.
 *
 */
void
_dl_eraseAllNamesFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
       	void (*callback)(unsigned int, int), const char *name, const int skip_first)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	char *dup_path;
	char *tok;
	int first = 1;
	int found = 0;

	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	dup_path = strdup(bu_vls_addr(&gdlp->dl_path));
	tok = strtok(dup_path, "/");
	while (tok) {
	    if (first) {
		first = 0;

		if (skip_first) {
		    tok = strtok((char *)NULL, "/");
		    continue;
		}
	    }

	    if (BU_STR_EQUAL(tok, name)) {
		_dl_freeDisplayListItem(dbip, callback, gdlp);
		found = 1;

		break;
	    }

	    tok = strtok((char *)NULL, "/");
	}

	/* Look for name in solids list */
	if (!found) {
	    struct db_full_path subpath;

	    if (db_string_to_path(&subpath, dbip, name) == 0) {
		eraseAllSubpathsFromSolidList(gdlp, &subpath, callback, skip_first);
		db_free_full_path(&subpath);
	    }
	}

	free((void *)dup_path);
	gdlp = next_gdlp;
    }
}


int
_dl_eraseFirstSubpath(struct bu_list *hdlp, struct db_i *dbip,
       	               void (*callback)(unsigned int, int),
		       struct display_list *gdlp,
		       struct db_full_path *subpath,
		       const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;
    struct db_full_path dup_path;

    db_full_path_init(&dup_path);

    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    int ret;
	    int full_len = sp->s_fullpath.fp_len;

	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
		(*callback)(sp->s_dlist, 1);

	    sp->s_fullpath.fp_len = full_len - 1;
	    db_dup_full_path(&dup_path, &sp->s_fullpath);
	    sp->s_fullpath.fp_len = full_len;
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp);

	    BU_LIST_DEQUEUE(&gdlp->l);

	    ret = headsolid_splitGDL(hdlp, dbip, gdlp, &dup_path);

	    db_free_full_path(&dup_path);

	    /* Free up the display list */
	    bu_vls_free(&gdlp->dl_path);
	    free((void *)gdlp);

	    return ret;
	}
	sp = nsp;
    }

    return 0;
}


/*
 * Erase/remove display list item from headDisplay if path is a subset of item's path.
 */
void
_dl_eraseAllPathsFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
       	                      void (*callback)(unsigned int, int),
			      const char *path,
			      const int skip_first)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct db_full_path fullpath, subpath;

    if (db_string_to_path(&subpath, dbip, path) == 0) {
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    gdlp->dl_wflag = 0;
	    gdlp = BU_LIST_PNEXT(display_list, gdlp);
	}

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    /* This display list has already been visited. */
	    if (gdlp->dl_wflag) {
		gdlp = next_gdlp;
		continue;
	    }

	    /* Mark as being visited. */
	    gdlp->dl_wflag = 1;

	    if (db_string_to_path(&fullpath, dbip, bu_vls_addr(&gdlp->dl_path)) == 0) {
		if (db_full_path_subset(&fullpath, &subpath, skip_first)) {
		    _dl_freeDisplayListItem(dbip, callback, gdlp);
		} else if (_dl_eraseFirstSubpath(hdlp, dbip, callback, gdlp, &subpath, skip_first)) {
		    gdlp = BU_LIST_NEXT(display_list, hdlp);
		    db_free_full_path(&fullpath);
		    continue;
		}

		db_free_full_path(&fullpath);
	    }

	    gdlp = next_gdlp;
	}

	db_free_full_path(&subpath);
    }
}


void
_dl_freeDisplayListItem (struct db_i *dbip,
       	void (*callback)(unsigned int, int),
	struct display_list *gdlp)
{
    struct solid *sp;
    struct directory *dp;

    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL) {

	/* We can't assume the display lists are contiguous */
	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    (*callback)(BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist, 1);
	}
    }

    /* Free up the solids list associated with this display list */
    while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    (void)db_dirdelete(dbip, dp);
	}

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp);
    }

    /* Free up the display list */
    BU_LIST_DEQUEUE(&gdlp->l);
    bu_vls_free(&gdlp->dl_path);
    free((void *)gdlp);
}


void
color_soltab(struct solid *sp)
{
    const struct mater *mp;

    sp->s_cflag = 0;

    /* the user specified the color, so use it */
    if (sp->s_uflag) {
	sp->s_color[0] = sp->s_basecolor[0];
	sp->s_color[1] = sp->s_basecolor[1];
	sp->s_color[2] = sp->s_basecolor[2];

	return;
    }

    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
	if (sp->s_regionid <= mp->mt_high &&
		sp->s_regionid >= mp->mt_low) {
	    sp->s_color[0] = mp->mt_r;
	    sp->s_color[1] = mp->mt_g;
	    sp->s_color[2] = mp->mt_b;

	    return;
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
}


/* Set solid's basecolor, color, and color flags based on client data and tree
 *  * state. If user color isn't set in client data, the solid's region id must be
 *   * set for proper material lookup.
 *    */
void
solid_set_color_info(
	struct solid *sp,
	unsigned char *wireframe_color_override,
	struct db_tree_state *tsp)
{
    unsigned char bcolor[3] = {255, 0, 0}; /* default */

    sp->s_uflag = 0;
    sp->s_dflag = 0;
    if (wireframe_color_override) {
	sp->s_uflag = 1;

	bcolor[RED] = wireframe_color_override[RED];
	bcolor[GRN] = wireframe_color_override[GRN];
	bcolor[BLU] = wireframe_color_override[BLU];
    } else if (tsp) {
	if (tsp->ts_mater.ma_color_valid) {
	    bcolor[RED] = tsp->ts_mater.ma_color[RED] * 255.0;
	    bcolor[GRN] = tsp->ts_mater.ma_color[GRN] * 255.0;
	    bcolor[BLU] = tsp->ts_mater.ma_color[BLU] * 255.0;
	} else {
	    sp->s_dflag = 1;
	}
    }

    sp->s_basecolor[RED] = bcolor[RED];
    sp->s_basecolor[GRN] = bcolor[GRN];
    sp->s_basecolor[BLU] = bcolor[BLU];

    color_soltab(sp);
}

/**
 *  * Compute the min, max, and center points of the solid.
 *   */
void
bound_solid(struct solid *sp)
{
    struct bn_vlist *vp;
    point_t bmin, bmax;
    int cmd;
    VSET(bmin, INFINITY, INFINITY, INFINITY);
    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);

    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
	cmd = bn_vlist_bbox(vp, &bmin, &bmax);
	if (cmd) {
	    bu_log("unknown vlist op %d\n", cmd);
	}
    }

    sp->s_center[X] = (bmin[X] + bmax[X]) * 0.5;
    sp->s_center[Y] = (bmin[Y] + bmax[Y]) * 0.5;
    sp->s_center[Z] = (bmin[Z] + bmax[Z]) * 0.5;

    sp->s_size = bmax[X] - bmin[X];
    V_MAX(sp->s_size, bmax[Y] - bmin[Y]);
    V_MAX(sp->s_size, bmax[Z] - bmin[Z]);
}


void
solid_append_vlist(struct solid *sp, struct bn_vlist *vlist)
{
    if (BU_LIST_IS_EMPTY(&(sp->s_vlist))) {
	sp->s_vlen = 0;
    }

    sp->s_vlen += bn_vlist_cmd_cnt(vlist);
    BU_LIST_APPEND_LIST(&(sp->s_vlist), &(vlist->l));
}

void
dl_add_path(struct display_list *gdlp, int dashflag, int transparency, int dmode, int hiddenLine, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct solid *existing_sp, unsigned char *wireframe_color_override, void (*callback)(struct solid *))
{
    struct solid *sp;

    if (!existing_sp) {
	GET_SOLID(sp);
    } else {
	sp = existing_sp;
    }

    solid_append_vlist(sp, (struct bn_vlist *)vhead);

    bound_solid(sp);

    if (!existing_sp) {
	db_dup_full_path(&sp->s_fullpath, pathp);

	sp->s_flag = DOWN;
	sp->s_iflag = DOWN;
	sp->s_soldash = dashflag;
	sp->s_Eflag = 0;

	if (tsp) {
	    sp->s_regionid = tsp->ts_regionid;
	}

	solid_set_color_info(sp, wireframe_color_override, tsp);

	sp->s_dlist = 0;
	sp->s_transparency = transparency;
	sp->s_dmode = dmode;
	sp->s_hiddenLine = hiddenLine;

	/* append solid to display list */
	bu_semaphore_acquire(RT_SEM_MODEL);
	BU_LIST_APPEND(gdlp->dl_headSolid.back, &sp->l);
	bu_semaphore_release(RT_SEM_MODEL);
    }

    if (callback != GED_CREATE_VLIST_CALLBACK_PTR_NULL) {
	(*callback)(sp);
    }

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
