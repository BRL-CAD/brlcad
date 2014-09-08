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

/* defined in draw_calc.cpp */
extern fastf_t brep_est_avg_curve_len(struct rt_brep_internal *bi);

struct display_list *
dl_addToDisplay(struct bu_list *hdlp, struct db_i *dbip,
                 const char *name)
{
    struct directory *dp = NULL;
    struct display_list *gdlp = NULL;
    char *cp = NULL;
    int found_namepath = 0;
    struct db_full_path namepath;

    cp = strrchr(name, '/');
    if (!cp)
        cp = (char *)name;
    else
        ++cp;

    if ((dp = db_lookup(dbip, cp, LOOKUP_NOISY)) == RT_DIR_NULL) {
        gdlp = GED_DISPLAY_LIST_NULL;
        goto end;
    }

    if (db_string_to_path(&namepath, dbip, name) == 0)
        found_namepath = 1;

    /* Make sure name is not already in the list */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        if (BU_STR_EQUAL(name, bu_vls_addr(&gdlp->dl_path)))
            goto end;

                if (found_namepath) {
            struct db_full_path gdlpath;

            if (db_string_to_path(&gdlpath, dbip, bu_vls_addr(&gdlp->dl_path)) == 0) {
                if (db_full_path_match_top(&gdlpath, &namepath)) {
                    db_free_full_path(&gdlpath);
                    goto end;
                }

                db_free_full_path(&gdlpath);
            }
        }

        gdlp = BU_LIST_PNEXT(display_list, gdlp);
    }

    BU_ALLOC(gdlp, struct display_list);
    BU_LIST_INIT(&gdlp->l);
    BU_LIST_INSERT(hdlp, &gdlp->l);
    BU_LIST_INIT(&gdlp->dl_headSolid);
    gdlp->dl_dp = (void *)dp;
    bu_vls_init(&gdlp->dl_path);
    bu_vls_printf(&gdlp->dl_path, "%s", name);

end:
    if (found_namepath)
        db_free_full_path(&namepath);

    return gdlp;
}


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


/*
 * Pass through the solid table and set pointer to appropriate
 * mater structure.
 */
void
dl_color_soltab(struct bu_list *hdlp)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    color_soltab(sp);
	}

	gdlp = next_gdlp;
    }
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

static fastf_t
view_avg_size(struct bview *gvp)
{
    fastf_t view_aspect, x_size, y_size;

    view_aspect = (fastf_t)gvp->gv_x_samples / gvp->gv_y_samples;
    x_size = gvp->gv_size;
    y_size = x_size / view_aspect;

    return (x_size + y_size) / 2.0;
}

static fastf_t
view_avg_sample_spacing(struct bview *gvp)
{
    fastf_t avg_view_size, avg_view_samples;

    avg_view_size = view_avg_size(gvp);
    avg_view_samples = (gvp->gv_x_samples + gvp->gv_y_samples) / 2.0;

    return avg_view_size / avg_view_samples;
}

static fastf_t
solid_point_spacing(struct bview *gvp, fastf_t solid_width)
{
    fastf_t radius, avg_view_size, avg_sample_spacing;
    point2d_t p1, p2;

    if (solid_width < SQRT_SMALL_FASTF)
        solid_width = SQRT_SMALL_FASTF;

    avg_view_size = view_avg_size(gvp);

    /* Now, for the sake of simplicity we're going to make
     * several assumptions:
     *  - our samples represent a grid of square pixels
     *  - a circle with a diameter half the width of the solid is a
     *    good proxy for the kind of curve that will be plotted
     */
    radius = solid_width / 4.0;
    if (avg_view_size < solid_width) {
        /* If the solid is larger than the view, it is
         * probably only partly visible and likely isn't the
         * primary focus of the user. We'll cap the point
         * spacing and avoid wasting effort.
         */
        radius = avg_view_size / 4.0;
    }

    /* We imagine our representative circular curve lying in
     * the XY plane centered at the origin.
     *
     * Suppose we're viewing the circle head on, and that the
     * apex of the curve (0, radius) lies just inside the
     * top edge of a pixel. Here we place a plotted point p1.
     *
     * As we continue clockwise around the circle we pass
     * through neighboring pixels in the same row, until we
     * vertically drop a distance equal to the pixel spacing,
     * in which case we just barely enter a pixel in the next
     * row. Here we place a plotted point p2 (y = radius -
     * avg_sample_spacing).
     *
     * In theory, the line segment between p1 and p2 passes
     * through all the same pixels that the actual curve does,
     * and thus produces the exact same rasterization as if
     * the curve between p1 and p2 was approximated with an
     * infinite number of line segments.
     *
     * We assume that the distance between p1 and p2 is the
     * maximum point sampling distance we can use for the
     * curve which will give a perfect rasterization, i.e.
     * the same rasterization as if we chose a point distance
     * of 0.
    */
    p1[Y] = radius;
    p1[X] = 0.0;

    avg_sample_spacing = view_avg_sample_spacing(gvp);
    if (avg_sample_spacing < radius) {
        p2[Y] = radius - (avg_sample_spacing);
    } else {
        /* no particular reason other than symmetry, just need
         * to prevent sqrt(negative).
         */
        p2[Y] = radius;
    }
    p2[X] = sqrt((radius * radius) - (p2[Y] * p2[Y]));

    return DIST_PT2_PT2(p1, p2);
}


/* Choose a point spacing for the given solid (sp, ip) s.t. solid
 * curves plotted with that spacing will look smooth when rasterized
 * in the given view (gvp).
 *
 * TODO: view_avg_sample_spacing() might be sufficient if we can
 * develop a general decimation routine for the resulting plots, in
 * which case, this function could be removed.
 */
static fastf_t
solid_point_spacing_for_view(
        struct solid *sp,
        struct rt_db_internal *ip,
        struct bview *gvp)
{
    fastf_t point_spacing = 0.0;

    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD) {
        switch (ip->idb_minor_type) {
            case DB5_MINORTYPE_BRLCAD_TGC: {
                struct rt_tgc_internal *tgc;
                fastf_t avg_diameter;
                fastf_t tgc_mag_a, tgc_mag_b, tgc_mag_c, tgc_mag_d;

                RT_CK_DB_INTERNAL(ip);
                tgc = (struct rt_tgc_internal *)ip->idb_ptr;
                RT_TGC_CK_MAGIC(tgc);

                tgc_mag_a = MAGNITUDE(tgc->a);
                tgc_mag_b = MAGNITUDE(tgc->b);
                tgc_mag_c = MAGNITUDE(tgc->c);
                tgc_mag_d = MAGNITUDE(tgc->d);

                avg_diameter = tgc_mag_a + tgc_mag_b + tgc_mag_c + tgc_mag_d;
                avg_diameter /= 2.0;
                point_spacing = solid_point_spacing(gvp, avg_diameter);
            }
                break;
            case DB5_MINORTYPE_BRLCAD_BOT:
                point_spacing = view_avg_sample_spacing(gvp);
                break;
            case DB5_MINORTYPE_BRLCAD_BREP: {
                struct rt_brep_internal *bi;

                RT_CK_DB_INTERNAL(ip);
                bi = (struct rt_brep_internal *)ip->idb_ptr;
                RT_BREP_CK_MAGIC(bi);

                point_spacing = solid_point_spacing(gvp,
                        brep_est_avg_curve_len(bi) * M_2_PI * 2.0);
            }
                break;
            default:
                point_spacing = solid_point_spacing(gvp, sp->s_size);
        }
    } else {
        point_spacing = solid_point_spacing(gvp, sp->s_size);
    }

    return point_spacing;
}


static fastf_t
draw_solid_wireframe(struct solid *sp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp,  void (*callback)(struct solid *))
{
    int ret;
    struct bu_list vhead;
    struct rt_db_internal dbintern;
    struct rt_db_internal *ip = &dbintern;

    BU_LIST_INIT(&vhead);
    ret = -1;

    ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(&sp->s_fullpath),
	    dbip, sp->s_mat, &rt_uniresource);

    if (ret < 0) {
	return -1;
    }

    if (gvp && gvp->gv_adaptive_plot && ip->idb_meth->ft_adaptive_plot) {
	struct rt_view_info info;

	info.vhead = &vhead;
	info.tol = tsp->ts_tol;

	info.point_spacing = solid_point_spacing_for_view(sp, ip, gvp);

	info.curve_spacing = sp->s_size / 2.0;

	info.point_spacing /= gvp->gv_point_scale;
	info.curve_spacing /= gvp->gv_curve_scale;

	ret = ip->idb_meth->ft_adaptive_plot(ip, &info);
    } else if (ip->idb_meth->ft_plot) {
	ret = ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol,
		tsp->ts_tol, NULL);
    }

    rt_db_free_internal(ip);

    if (ret < 0) {
	bu_log("%s: plot failure\n", DB_FULL_PATH_CUR_DIR(&sp->s_fullpath)->d_namep);

	return -1;
    }

    /* add plot to solid */
    solid_append_vlist(sp, (struct bn_vlist *)&vhead);

    if (callback != GED_CREATE_VLIST_CALLBACK_PTR_NULL) {
	(*callback)(sp);
    }

    return 0;
}


int
redraw_solid(struct solid *sp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp, void (*callback)(struct solid *))
{
    if (sp->s_dmode == _GED_WIREFRAME) {
	/* replot wireframe */
	if (BU_LIST_NON_EMPTY(&sp->s_vlist)) {
	    RT_FREE_VLIST(&sp->s_vlist);
	}
	return draw_solid_wireframe(sp, dbip, tsp, gvp, callback);
    } else {
	/* non-wireframe replot - let's not and say we did */
	if (callback != GED_CREATE_VLIST_CALLBACK_PTR_NULL)
	    (*callback)(sp);
    }

    return 0;
}


int
dl_redraw(struct display_list *gdlp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp, void (*callback)(struct solid *))
{
    int ret = 0;
    struct solid *sp;
    for (BU_LIST_FOR(sp, solid, &gdlp->dl_headSolid)) {
	ret = redraw_solid(sp, dbip, tsp, gvp, callback);
	if (ret < 0) return ret;
    }
    return ret;
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
