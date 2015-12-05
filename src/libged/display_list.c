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

#include "bu/ptbl.h"
#include "bu/str.h"
#include "bn/plot3.h"

#include "rt/solid.h"
#include "./ged_private.h"



/* defined in draw_calc.cpp */
extern fastf_t brep_est_avg_curve_len(struct rt_brep_internal *bi);
extern void createDListSolid(struct solid *sp);

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
dl_bounding_sph(struct bu_list *hdlp, vect_t *min, vect_t *max, int pflag)
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
	    /* Skip pseudo-solids unless pflag is set */
	    if (!pflag &&
		    sp->s_fullpath.fp_names != (struct directory **)0 &&
		    sp->s_fullpath.fp_names[0] != (struct directory *)0 &&
		    sp->s_fullpath.fp_names[0]->d_addr == RT_DIR_PHONY_ADDR)
		continue;

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

/*
 * Erase/remove the display list item from headDisplay if path matches the list item's path.
 *
 */
void
dl_erasePathFromDisplay(struct bu_list *hdlp,
	struct db_i *dbip, void (*callback)(unsigned int, int),
       	const char *path, int allow_split, struct solid *freesolid)
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
		if (sp) {
		    dp = FIRST_SOLID(sp);
		    RT_CK_DIR(dp);
		    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
			(void)db_dirdelete(dbip, dp);
		    }

		    BU_LIST_DEQUEUE(&sp->l);
		    FREE_SOLID(sp, &freesolid->l);
		}
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
		    FREE_SOLID(sp, &freesolid->l);
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
			      const int skip_first, struct solid *freesolid)
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
	    FREE_SOLID(sp, &freesolid->l);
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
       	void (*callback)(unsigned int, int), const char *name, const int skip_first, struct solid *freesolid)
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

	dup_path = bu_strdup(bu_vls_addr(&gdlp->dl_path));
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
		_dl_freeDisplayListItem(dbip, callback, gdlp, freesolid);
		found = 1;

		break;
	    }

	    tok = strtok((char *)NULL, "/");
	}

	/* Look for name in solids list */
	if (!found) {
	    struct db_full_path subpath;

	    if (db_string_to_path(&subpath, dbip, name) == 0) {
		eraseAllSubpathsFromSolidList(gdlp, &subpath, callback, skip_first, freesolid);
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
		       const int skip_first, struct solid *freesolid)
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
	    FREE_SOLID(sp, &freesolid->l);

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
			      const int skip_first,
			      struct solid *freesolid)
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
		    _dl_freeDisplayListItem(dbip, callback, gdlp, freesolid);
		} else if (_dl_eraseFirstSubpath(hdlp, dbip, callback, gdlp, &subpath, skip_first, freesolid)) {
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
	struct display_list *gdlp, struct solid *freesolid)
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
	if (sp) {
	    dp = FIRST_SOLID(sp);
	    RT_CK_DIR(dp);
	    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		(void)db_dirdelete(dbip, dp);
	    }

	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &freesolid->l);
	}
    }

    /* Free up the display list */
    BU_LIST_DEQUEUE(&gdlp->l);
    bu_vls_free(&gdlp->dl_path);
    free((void *)gdlp);
}


static void
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
static void
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
static void
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


static void
solid_append_vlist(struct solid *sp, struct bn_vlist *vlist)
{
    if (BU_LIST_IS_EMPTY(&(sp->s_vlist))) {
	sp->s_vlen = 0;
    }

    sp->s_vlen += bn_vlist_cmd_cnt(vlist);
    BU_LIST_APPEND_LIST(&(sp->s_vlist), &(vlist->l));
}

void
dl_add_path(struct display_list *gdlp, int dashflag, int transparency, int dmode, int hiddenLine, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, unsigned char *wireframe_color_override, void (*callback)(struct solid *sp), struct solid *freesolid)
{
    struct solid *sp;
    GET_SOLID(sp, &freesolid->l);

    solid_append_vlist(sp, (struct bn_vlist *)vhead);

    bound_solid(sp);

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

    if (callback != GED_CREATE_VLIST_SOLID_CALLBACK_PTR_NULL) {
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
draw_solid_wireframe(struct solid *sp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp)
{
    int ret;
    struct bu_list vhead;
    struct rt_db_internal dbintern;
    struct rt_db_internal *ip = &dbintern;

    BU_LIST_INIT(&vhead);

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

    return 0;
}


int
redraw_solid(struct solid *sp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp)
{
    if (sp->s_dmode == _GED_WIREFRAME) {
	/* replot wireframe */
	if (BU_LIST_NON_EMPTY(&sp->s_vlist)) {
	    RT_FREE_VLIST(&sp->s_vlist);
	}
	return draw_solid_wireframe(sp, dbip, tsp, gvp);
    }
    return 0;
}


int
dl_redraw(struct display_list *gdlp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp, void (*callback)(struct display_list *))
{
    int ret = 0;
    struct solid *sp;
    for (BU_LIST_FOR(sp, solid, &gdlp->dl_headSolid)) {
	ret += redraw_solid(sp, dbip, tsp, gvp);
    }
    if (callback != GED_CREATE_VLIST_CALLBACK_PTR_NULL)
	(*callback)(gdlp);
    return ret;
}

union tree *
append_solid_to_display_list(
        struct db_tree_state *tsp,
        const struct db_full_path *pathp,
        struct rt_db_internal *ip,
        void *client_data)
{
    point_t min, max;
    struct solid *sp;
    union tree *curtree;
    struct bview_client_data *bview_data = (struct bview_client_data *)client_data;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_RESOURCE(tsp->ts_resp);

    VSETALL(min, INFINITY);
    VSETALL(max, -INFINITY);

    if (!bview_data) {
        return TREE_NULL;
    }

    if (RT_G_DEBUG & DEBUG_TREEWALK) {
        char *sofar = db_path_to_string(pathp);

        bu_log("append_solid_to_display_list(%s) path='%s'\n", ip->idb_meth->ft_name, sofar);

        bu_free((void *)sofar, "path string");
    }

    /* create solid */
    GET_SOLID(sp, &(((struct solid *)bview_data->freesolid)->l));

    sp->s_size = 0;
    VSETALL(sp->s_center, 0.0);

    if (ip->idb_meth->ft_bbox) {
        if (ip->idb_meth->ft_bbox(ip, &min, &max, tsp->ts_tol) < 0) {
            bu_log("%s: plot failure\n", DB_FULL_PATH_CUR_DIR(pathp)->d_namep);

            return TREE_NULL;
        }

        sp->s_center[X] = (min[X] + max[X]) * 0.5;
        sp->s_center[Y] = (min[Y] + max[Y]) * 0.5;
        sp->s_center[Z] = (min[Z] + max[Z]) * 0.5;

        sp->s_size = max[X] - min[X];
        V_MAX(sp->s_size, max[Y] - min[Y]);
        V_MAX(sp->s_size, max[Z] - min[Z]);
    } else if (ip->idb_meth->ft_plot) {
        /* As a fallback for primitives that don't have a bbox function, use
         * the old bounding method of calculating a plot for the primitive and
         * using the extent of the plotted segments as the bounds.
         */
        int plot_status;
        struct bu_list vhead;
        struct bn_vlist *vp;

        BU_LIST_INIT(&vhead);

        plot_status = ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol,
                tsp->ts_tol, NULL);

        if (plot_status < 0) {
            bu_log("%s: plot failure\n", DB_FULL_PATH_CUR_DIR(pathp)->d_namep);

            return TREE_NULL;
        }

        solid_append_vlist(sp, (struct bn_vlist *)&vhead);

        bound_solid(sp);

        while (BU_LIST_WHILE(vp, bn_vlist, &(sp->s_vlist))) {
            BU_LIST_DEQUEUE(&vp->l);
            bu_free(vp, "solid vp");
        }
    }

    sp->s_vlen = 0;
    db_dup_full_path(&sp->s_fullpath, pathp);
    sp->s_flag = DOWN;
    sp->s_iflag = DOWN;

    if (bview_data->draw_solid_lines_only) {
        sp->s_soldash = 0;
    } else {
        sp->s_soldash = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));
    }

    sp->s_Eflag = 0;
    sp->s_regionid = tsp->ts_regionid;

    if (ip->idb_type == ID_GRIP) {
        float mater_color[3];

        /* Temporarily change mater color for pseudo solid to get the desired
         * default color.
         */
        mater_color[RED] = tsp->ts_mater.ma_color[RED];
        mater_color[GRN] = tsp->ts_mater.ma_color[GRN];
        mater_color[BLU] = tsp->ts_mater.ma_color[BLU];

        tsp->ts_mater.ma_color[RED] = 0;
        tsp->ts_mater.ma_color[GRN] = 128;
        tsp->ts_mater.ma_color[BLU] = 128;

        if (bview_data->wireframe_color_override) {
            solid_set_color_info(sp, (unsigned char *)&(bview_data->wireframe_color), tsp);
        } else {
            solid_set_color_info(sp, NULL, tsp);
        }

        tsp->ts_mater.ma_color[RED] = mater_color[RED];
        tsp->ts_mater.ma_color[GRN] = mater_color[GRN];
        tsp->ts_mater.ma_color[BLU] = mater_color[BLU];

    } else {
        if (bview_data->wireframe_color_override) {
	    unsigned char wire_color[3];
	    wire_color[RED] = (unsigned char)bview_data->wireframe_color[RED];
	    wire_color[GRN] = (unsigned char)bview_data->wireframe_color[GRN];
	    wire_color[BLU] = (unsigned char)bview_data->wireframe_color[BLU];
            solid_set_color_info(sp, wire_color, tsp);
        } else {
            solid_set_color_info(sp, NULL, tsp);
        }
    }

    sp->s_dlist = 0;
    sp->s_transparency = bview_data->transparency;
    sp->s_dmode = bview_data->dmode;
    sp->s_hiddenLine = bview_data->hiddenLine;
    MAT_COPY(sp->s_mat, tsp->ts_mat);

    /* append solid to display list */
    bu_semaphore_acquire(RT_SEM_MODEL);
    BU_LIST_APPEND(bview_data->gdlp->dl_headSolid.back, &sp->l);
    bu_semaphore_release(RT_SEM_MODEL);

    /* indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    return curtree;
}

static void
solid_copy_vlist(struct solid *sp, struct bn_vlist *vlist)
{
    BU_LIST_INIT(&(sp->s_vlist));
    rt_vlist_copy(&(sp->s_vlist), (struct bu_list *)vlist);
    sp->s_vlen = bn_vlist_cmd_cnt((struct bn_vlist *)(&(sp->s_vlist)));
}

int invent_solid(struct bu_list *hdlp, struct db_i *dbip,
       	void (*callback_create)(struct solid *), void (*callback_free)(unsigned int, int),
       	char *name, struct bu_list *vhead, long int rgb, int copy, fastf_t transparency, int dmode,
       	struct solid *freesolid, int csoltab)
{
    struct directory *dp;
    struct solid *sp;
    struct display_list *gdlp;
    unsigned char type='0';

    if (dbip == DBI_NULL)
	return 0;

    if ((dp = db_lookup(dbip, name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	if (dp->d_addr != RT_DIR_PHONY_ADDR) {
	    bu_log("invent_solid(%s) would clobber existing database entry, ignored\n", name);
	    return -1;
	}

	/*
	 * Name exists from some other overlay,
	 * zap any associated solids
	 */
	dl_erasePathFromDisplay(hdlp, dbip, callback_free, name, 0, freesolid);
    }
    /* Need to enter phony name in directory structure */
    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&type);

    /* Obtain a fresh solid structure, and fill it in */
    GET_SOLID(sp, &freesolid->l);

    if (copy) {
	solid_copy_vlist(sp, (struct bn_vlist *)vhead);
    } else {
	solid_append_vlist(sp, (struct bn_vlist *)vhead);
	BU_LIST_INIT(vhead);
    }
    bound_solid(sp);

    /* set path information -- this is a top level node */
    db_add_node_to_full_path(&sp->s_fullpath, dp);

    gdlp = dl_addToDisplay(hdlp, dbip, name);

    sp->s_iflag = DOWN;
    sp->s_soldash = 0;
    sp->s_Eflag = 1;            /* Can't be solid edited! */
    sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
    sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
    sp->s_color[2] = sp->s_basecolor[2] = (rgb) & 0xFF;
    sp->s_regionid = 0;
    sp->s_dlist = 0;

    sp->s_uflag = 0;
    sp->s_dflag = 0;
    sp->s_cflag = 0;
    sp->s_wflag = 0;

    sp->s_transparency = transparency;
    sp->s_dmode = dmode;

    /* Solid successfully drawn, add to linked list of solid structs */
    BU_LIST_APPEND(gdlp->dl_headSolid.back, &sp->l);

    if (csoltab)
	color_soltab(sp);

    if (callback_create != GED_CREATE_VLIST_SOLID_CALLBACK_PTR_NULL)
	(*callback_create)(sp);

    return 0;           /* OK */

}

int
dl_set_illum(struct display_list *gdlp, const char *obj, int illum)
{
    int found = 0;
    struct solid *sp;

    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	size_t i;

	for (i = 0; i < sp->s_fullpath.fp_len; ++i) {
	    if (*obj == *DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep &&
		    BU_STR_EQUAL(obj, DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep)) {
		found = 1;
		if (illum)
		    sp->s_iflag = UP;
		else
		    sp->s_iflag = DOWN;
	    }
	}
    }
    return found;
}

void
dl_set_iflag(struct bu_list *hdlp, int iflag)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    sp->s_iflag = iflag;
	}

	gdlp = next_gdlp;
    }
}

void
dl_set_flag(struct bu_list *hdlp, int flag)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    sp->s_flag = flag;
	}

	gdlp = next_gdlp;
    }
}

void
dl_set_wflag(struct bu_list *hdlp, int wflag)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    sp->s_wflag = wflag;
	}

	gdlp = next_gdlp;
    }
}

void
dl_zap(struct bu_list *hdlp, struct db_i *dbip, void (*callback)(unsigned int, int), struct solid *freesolid)
{
    struct solid *sp = SOLID_NULL;
    struct display_list *gdlp = NULL;
    struct bu_ptbl dls = BU_PTBL_INIT_ZERO;
    struct directory *dp = RT_DIR_NULL;
    size_t i = 0;

    while (BU_LIST_WHILE(gdlp, display_list, hdlp)) {

	if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL && BU_LIST_NON_EMPTY(&gdlp->dl_headSolid))
	    (*callback)(BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist,
		    BU_LIST_LAST(solid, &gdlp->dl_headSolid)->s_dlist -
		    BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist + 1);

	while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
	    dp = FIRST_SOLID(sp);
	    RT_CK_DIR(dp);
	    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		if (db_dirdelete(dbip, dp) < 0) {
		    bu_log("ged_zap: db_dirdelete failed\n");
		}
	    }

	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &freesolid->l);
	}

	BU_LIST_DEQUEUE(&gdlp->l);
	/* queue up for free */
	bu_ptbl_ins_unique(&dls, (long *)gdlp);
	gdlp = NULL;
    }

    /* Free all display lists */
    for(i = 0; i < BU_PTBL_LEN(&dls); i++) {
	gdlp = (struct display_list *)BU_PTBL_GET(&dls, i);
	bu_vls_free(&gdlp->dl_path);
	free((void *)gdlp);
    }
    bu_ptbl_free(&dls);
}

int
dl_how(struct bu_list *hdlp, struct bu_vls *vls, struct directory **dpp, int both)
{
    size_t i;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    struct directory **tmp_dpp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    for (i = 0, tmp_dpp = dpp;
		    i < sp->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
		    ++i, ++tmp_dpp) {
		if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		    break;
	    }

	    if (*tmp_dpp != RT_DIR_NULL)
		continue;


	    /* found a match */
	    if (sp->s_hiddenLine) {
		if (both)
		    bu_vls_printf(vls, "%d 1", _GED_HIDDEN_LINE);
		else
		    bu_vls_printf(vls, "%d", _GED_HIDDEN_LINE);
	    } else {
		if (both)
		    bu_vls_printf(vls, "%d %g", sp->s_dmode, sp->s_transparency);
		else
		    bu_vls_printf(vls, "%d", sp->s_dmode);
	    }

	    return 1;
	}

	gdlp = next_gdlp;
    }

    return 0;
}


void
dl_plot(struct bu_list *hdlp, FILE *fp, mat_t model2view, int floating, mat_t center, fastf_t scale, int Three_D, int Z_clip)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
    static vect_t clipmin, clipmax;
    static vect_t last;         /* last drawn point */
    static vect_t fin;
    static vect_t start;
    int Dashing;                        /* linetype is dashed */

    if (floating) {
        pd_3space(fp,
                  -center[MDX] - scale,
                  -center[MDY] - scale,
                  -center[MDZ] - scale,
                  -center[MDX] + scale,
                  -center[MDY] + scale,
                  -center[MDZ] + scale);
        Dashing = 0;
        pl_linmod(fp, "solid");

        gdlp = BU_LIST_NEXT(display_list, hdlp);
        while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
            next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

            FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
                /* Could check for differences from last color */
                pl_color(fp,
                         sp->s_color[0],
                         sp->s_color[1],
                         sp->s_color[2]);
                if (Dashing != sp->s_soldash) {
                    if (sp->s_soldash)
                        pl_linmod(fp, "dotdashed");
                    else
                        pl_linmod(fp, "solid");
                    Dashing = sp->s_soldash;
                }
                rt_vlist_to_uplot(fp, &(sp->s_vlist));
            }

            gdlp = next_gdlp;
        }

        return;
    }
    /*
     * Integer output version, either 2-D or 3-D.
     * Viewing region is from -1.0 to +1.0
     * which is mapped to integer space -2048 to +2048 for plotting.
     * Compute the clipping bounds of the screen in view space.
     */
    clipmin[X] = -1.0;
    clipmax[X] =  1.0;
    clipmin[Y] = -1.0;
    clipmax[Y] =  1.0;
    if (Z_clip) {
        clipmin[Z] = -1.0;
        clipmax[Z] =  1.0;
    } else {
        clipmin[Z] = -1.0e20;
        clipmax[Z] =  1.0e20;
    }

    if (Three_D)
        pl_3space(fp, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MAX, (int)DG_GED_MAX, (int)DG_GED_MAX);
    else
        pl_space(fp, (int)DG_GED_MIN, (int)DG_GED_MIN, (int)DG_GED_MAX, (int)DG_GED_MAX);
    pl_erase(fp);
    Dashing = 0;
    pl_linmod(fp, "solid");

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            if (Dashing != sp->s_soldash) {
                if (sp->s_soldash)
                    pl_linmod(fp, "dotdashed");
                else
                    pl_linmod(fp, "solid");
                Dashing = sp->s_soldash;
            }
            for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
                int i;
                int nused = vp->nused;
                int *cmd = vp->cmd;
                point_t *pt = vp->pt;
                for (i = 0; i < nused; i++, cmd++, pt++) {
                    switch (*cmd) {
                        case BN_VLIST_POLY_START:
                        case BN_VLIST_POLY_VERTNORM:
                        case BN_VLIST_TRI_START:
                        case BN_VLIST_TRI_VERTNORM:
                            continue;
                        case BN_VLIST_POLY_MOVE:
                        case BN_VLIST_LINE_MOVE:
                        case BN_VLIST_TRI_MOVE:
                            /* Move, not draw */
                            MAT4X3PNT(last, model2view, *pt);
                            continue;
                        case BN_VLIST_LINE_DRAW:
                        case BN_VLIST_POLY_DRAW:
                        case BN_VLIST_POLY_END:
                        case BN_VLIST_TRI_DRAW:
                        case BN_VLIST_TRI_END:
                            /* draw */
                            MAT4X3PNT(fin, model2view, *pt);
                            VMOVE(start, last);
                            VMOVE(last, fin);
                            break;
                    }
                    if (bn_ray_vclip(start, fin, clipmin, clipmax) == 0)
                        continue;

                    if (Three_D) {
                        /* Could check for differences from last color */
                       /* Could check for differences from last color */
                        pl_color(fp,
                                 sp->s_color[0],
                                 sp->s_color[1],
                                 sp->s_color[2]);
                        pl_3line(fp,
                                 (int)(start[X] * DG_GED_MAX),
                                 (int)(start[Y] * DG_GED_MAX),
                                 (int)(start[Z] * DG_GED_MAX),
                                 (int)(fin[X] * DG_GED_MAX),
                                 (int)(fin[Y] * DG_GED_MAX),
                                 (int)(fin[Z] * DG_GED_MAX));
                    } else {
                        pl_line(fp,
                                (int)(start[0] * DG_GED_MAX),
                                (int)(start[1] * DG_GED_MAX),
                                (int)(fin[0] * DG_GED_MAX),
                                (int)(fin[1] * DG_GED_MAX));
                    }
                }
            }
        }

        gdlp = next_gdlp;
    }
}


struct coord {
    short x;
    short y;
};

struct stroke {
    struct coord pixel; /* starting scan, nib */
    short xsign;        /* 0 or +1 */
    short ysign;        /* -1, 0, or +1 */
    int ymajor;         /* true if Y is major dir. */
    short major;        /* major dir delta (nonneg) */
    short minor;        /* minor dir delta (nonneg) */
    short e;            /* DDA error accumulator */
    short de;           /* increment for `e' */
};

/*
 * Method:
 * Modified Bresenham algorithm.  Guaranteed to mark a dot for
 * a zero-length stroke.
 */
static void
raster(unsigned char **image, struct stroke *vp, unsigned char *color, size_t size)
{
    size_t dy;          /* raster within active band */

    /*
     * Write the color of this vector on all pixels.
     */
    for (dy = vp->pixel.y; dy <= size;) {

        /* set the appropriate pixel in the buffer to color */
        image[size-dy][vp->pixel.x*3] = color[0];
        image[size-dy][vp->pixel.x*3+1] = color[1];
        image[size-dy][vp->pixel.x*3+2] = color[2];

        if (vp->major-- == 0)
            return;             /* Done */

        if (vp->e < 0) {
            /* advance major & minor */
            dy += vp->ysign;
            vp->pixel.x += vp->xsign;
            vp->e += vp->de;
        } else {
            /* advance major only */
            if (vp->ymajor)     /* Y is major dir */
                ++dy;
            else                /* X is major dir */
                vp->pixel.x += vp->xsign;
            vp->e -= vp->minor;
        }
    }
}

static void
draw_stroke(unsigned char **image, struct coord *coord1, struct coord *coord2, unsigned char *color, size_t size)
{
    struct stroke cur_stroke;
    struct stroke *vp = &cur_stroke;

    /* arrange for pt1 to have the smaller Y-coordinate: */
    if (coord1->y > coord2->y) {
        struct coord *temp;     /* temporary for swap */

        temp = coord1;          /* swap pointers */
        coord1 = coord2;
        coord2 = temp;
    }

    /* set up DDA parameters for stroke */
    vp->pixel = *coord1;                /* initial pixel */
    vp->major = coord2->y - vp->pixel.y;        /* always nonnegative */
    vp->ysign = vp->major ? 1 : 0;
    vp->minor = coord2->x - vp->pixel.x;
    vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0;
    if (vp->xsign < 0)
        vp->minor = -vp->minor;

    /* if Y is not really major, correct the assignments */
    vp->ymajor = vp->minor <= vp->major;
    if (!vp->ymajor) {
        short temp;     /* temporary for swap */

        temp = vp->minor;
        vp->minor = vp->major;
        vp->major = temp;
    }

    vp->e = vp->major / 2 - vp->minor;  /* initial DDA error */
    vp->de = vp->major - vp->minor;

    raster(image, vp, color, size);
}


static void
draw_png_solid(fastf_t perspective, unsigned char **image, struct solid *sp, matp_t psmat, size_t size, size_t half_size)
{
    static vect_t last;
    point_t clipmin = {-1.0, -1.0, -MAX_FASTF};
    point_t clipmax = {1.0, 1.0, MAX_FASTF};
    struct bn_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    struct bn_vlist *vp = (struct bn_vlist *)&sp->s_vlist;
    fastf_t delta;
    struct coord coord1;
    struct coord coord2;

    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = psmat[15]*0.0001;
    if (delta < 0.0)
        delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
        delta = SQRT_SMALL_FASTF;

    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
        int i;
        int nused = tvp->nused;
        int *cmd = tvp->cmd;
        point_t *pt = tvp->pt;
        for (i = 0; i < nused; i++, cmd++, pt++) {
            static vect_t start, fin;
            switch (*cmd) {
                case BN_VLIST_POLY_START:
                case BN_VLIST_POLY_VERTNORM:
                case BN_VLIST_TRI_START:
                case BN_VLIST_TRI_VERTNORM:
                    continue;
                case BN_VLIST_POLY_MOVE:
                case BN_VLIST_LINE_MOVE:
                case BN_VLIST_TRI_MOVE:
                    /* Move, not draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            pt_prev = pt;
                            dist_prev = dist;
                            continue;
                        } else {
                            MAT4X3PNT(last, psmat, *pt);
                            dist_prev = dist;
                            pt_prev = pt;
                        }
                    } else
                        MAT4X3PNT(last, psmat, *pt);
                    continue;
                case BN_VLIST_POLY_DRAW:
                case BN_VLIST_POLY_END:
                case BN_VLIST_LINE_DRAW:
                case BN_VLIST_TRI_DRAW:
                case BN_VLIST_TRI_END:
                    /* draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            if (dist_prev <= 0.0) {
                                /* nothing to plot */
                                dist_prev = dist;
                                pt_prev = pt;
                                continue;
                            } else {
                                if (pt_prev) {
                                fastf_t alpha;
                                vect_t diff;
                                point_t tmp_pt;

                                /* clip this end */
                                VSUB2(diff, *pt, *pt_prev);
                                alpha = (dist_prev - delta) / (dist_prev - dist);
                                VJOIN1(tmp_pt, *pt_prev, alpha, diff);
                                MAT4X3PNT(fin, psmat, tmp_pt);
                                }
                            }
                        } else {
                            if (dist_prev <= 0.0) {
                                if (pt_prev) {
                                fastf_t alpha;
                                vect_t diff;
                                point_t tmp_pt;

                                /* clip other end */
                                VSUB2(diff, *pt, *pt_prev);
                                alpha = (-dist_prev + delta) / (dist - dist_prev);
                                VJOIN1(tmp_pt, *pt_prev, alpha, diff);
                                MAT4X3PNT(last, psmat, tmp_pt);
                                MAT4X3PNT(fin, psmat, *pt);
                                }
                            } else {
                                MAT4X3PNT(fin, psmat, *pt);
                            }
                        }
                    } else
                        MAT4X3PNT(fin, psmat, *pt);
                    VMOVE(start, last);
                    VMOVE(last, fin);
                    break;
            }

            if (bn_ray_vclip(start, fin, clipmin, clipmax) == 0)
                continue;

            coord1.x = start[0] * half_size + half_size;
            coord1.y = start[1] * half_size + half_size;
            coord2.x = fin[0] * half_size + half_size;
            coord2.y = fin[1] * half_size + half_size;
            draw_stroke(image, &coord1, &coord2, sp->s_color, size);
        }
    }
}


void
dl_png(struct bu_list *hdlp, mat_t model2view, fastf_t perspective, vect_t eye_pos, size_t size, size_t half_size, unsigned char **image)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t newmat;
    matp_t mat;
    mat_t perspective_mat;
    struct solid *sp;

    mat = model2view;

    if (0 < perspective) {
        point_t l, h;

        VSET(l, -1.0, -1.0, -1.0);
        VSET(h, 1.0, 1.0, 200.0);

        if (ZERO(eye_pos[Z] - 1.0)) {
            /* This way works, with reasonable Z-clipping */
            persp_mat(perspective_mat, perspective,
                          (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
        } else {
            /* This way does not have reasonable Z-clipping,
             * but includes shear, for GDurf's testing.
             */
            deering_persp_mat(perspective_mat, l, h, eye_pos);
        }

        bn_mat_mul(newmat, perspective_mat, mat);
        mat = newmat;
    }

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            draw_png_solid(perspective, image, sp, mat, size, half_size);
        }

        gdlp = next_gdlp;
    }
}

static struct bu_structparse polygon_desc[] = {
    {"%d", 1, "magic", bu_offsetof(struct polygon_header, magic), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "ident", bu_offsetof(struct polygon_header, ident), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "interior", bu_offsetof(struct polygon_header, interior), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "normal", bu_offsetof(struct polygon_header, normal), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c", 3, "color", bu_offsetof(struct polygon_header, color), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "npts", bu_offsetof(struct polygon_header, npts), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

static struct bu_structparse vertex_desc[] = {
    {"%f", 3, "vertex", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

void
dl_polybinout(struct bu_list *hdlp, struct polygon_header *ph, FILE *fp)
{
    int pno = 1;

    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
#define MAX_VERTS 10000
    vect_t verts[MAX_VERTS];
    struct bu_external obuf;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
                int i;
                int nused = vp->nused;
                int *cmd = vp->cmd;
                point_t *pt = vp->pt;
                for (i = 0; i < nused; i++, cmd++, pt++) {
                    /* For each polygon, spit it out.  Ignore vectors */
                    switch (*cmd) {
                        case BN_VLIST_LINE_MOVE:
                            /* Move, start line */
                            break;
                        case BN_VLIST_LINE_DRAW:
                            /* Draw line */
                            break;
                        case BN_VLIST_POLY_VERTNORM:
                        case BN_VLIST_TRI_VERTNORM:
                            /* Ignore per-vertex normal */
                            break;
                        case BN_VLIST_POLY_START:
                        case BN_VLIST_TRI_START:
                            /* Start poly marker & normal, followed by POLY_MOVE */
                            ph->magic = POLYGON_HEADER_MAGIC;
                            ph->ident = pno++;
                            ph->interior = 0;
                            memcpy(ph->color, sp->s_basecolor, 3);
                            ph->npts = 0;
                            /* Set surface normal (vl_pnt points outward) */
                            VMOVE(ph->normal, *pt);
                            break;
                        case BN_VLIST_POLY_MOVE:
                        case BN_VLIST_TRI_MOVE:
                            /* Start of polygon, has first point */
                            /* fall through to... */
                        case BN_VLIST_POLY_DRAW:
                        case BN_VLIST_TRI_DRAW:
                            /* Polygon Draw */
                            if (ph->npts >= MAX_VERTS) {
                                bu_log("excess vertex skipped\n");
                                break;
                            }
                            VMOVE(verts[ph->npts], *pt);
                            ph->npts++;
                            break;
                        case BN_VLIST_POLY_END:
                        case BN_VLIST_TRI_END:
                            /*
                             * End Polygon.  Point given is repeat of
                             * first one, ignore it.
                             * XXX note:  if poly_markers was not set,
                             * XXX poly will end with next POLY_MOVE.
                             */
                            if (ph->npts < 3) {
                                bu_log("polygon with %d points discarded\n", ph->npts);
                                break;
                            }
                            if (bu_struct_export(&obuf, (void *)&ph, polygon_desc) < 0) {
                                bu_log("header export error\n");
                                break;
                            }
                            if (bu_struct_put(fp, &obuf) != obuf.ext_nbytes) {
                                perror("bu_struct_put");
                                break;
                            }
                            bu_free_external(&obuf);
                            /* Now export the vertices */
                            vertex_desc[0].sp_count = ph->npts * 3;
                            if (bu_struct_export(&obuf, (void *)verts, vertex_desc) < 0) {
                                bu_log("vertex export error\n");
                                break;
                            }
                            if (bu_struct_put(fp, &obuf) != obuf.ext_nbytes) {
                                perror("bu_struct_wrap_buf");
                                break;
                            }
                            bu_free_external(&obuf);
                            ph->npts = 0;                /* sanity */
                            break;
                    }
                }
            }
      }

        gdlp = next_gdlp;
    }
}

static void
ps_draw_header(FILE *fp, char *font, char *title, char *creator, int linewidth, fastf_t scale, int xoffset, int yoffset)
{
    fprintf(fp, "%%!PS-Adobe-1.0\n\
%%begin(plot)\n\
%%%%DocumentFonts:  %s\n",
            font);

    fprintf(fp, "%%%%Title: %s\n", title);

    fprintf(fp, "\
%%%%Creator: %s\n\
%%%%BoundingBox: 0 0 324 324    %% 4.5in square, for TeX\n\
%%%%EndComments\n\
\n",
            creator);

    fprintf(fp, "\
%d setlinewidth\n\
\n", linewidth);

    fprintf(fp, "\
%% Sizes, made functions to avoid scaling if not needed\n\
/FntH /%s findfont 80 scalefont def\n\
/DFntL { /FntL /%s findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /%s findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /%s findfont 44 scalefont def } def\n\
\n", font, font, font, font);

    fprintf(fp, "\
%% line styles\n\
/NV { [] 0 setdash } def        %% normal vectors\n\
/DV { [8] 0 setdash } def       %% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def       %% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def   %% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def   %% long-dash vectors\n\
\n\
/NEWPG {\n\
        %d %d translate\n\
        %f %f scale     %% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH setfont\n\
NEWPG\n\
",
            xoffset, yoffset, scale, scale);
}

static void
ps_draw_solid(fastf_t perspective, FILE *fp, struct solid *sp, matp_t psmat)
{
    static vect_t last;
    point_t clipmin = {-1.0, -1.0, -MAX_FASTF};
    point_t clipmax = {1.0, 1.0, MAX_FASTF};
    struct bn_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    struct bn_vlist *vp = (struct bn_vlist *)&sp->s_vlist;
    fastf_t delta;

    fprintf(fp, "%f %f %f setrgbcolor\n",
            PS_COLOR(sp->s_color[0]),
            PS_COLOR(sp->s_color[1]),
            PS_COLOR(sp->s_color[2]));

    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = psmat[15]*0.0001;
    if (delta < 0.0)
        delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
        delta = SQRT_SMALL_FASTF;

    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
        int i;
        int nused = tvp->nused;
        int *cmd = tvp->cmd;
        point_t *pt = tvp->pt;
        for (i = 0; i < nused; i++, cmd++, pt++) {
            static vect_t start, fin;
            switch (*cmd) {
                case BN_VLIST_POLY_START:
                case BN_VLIST_POLY_VERTNORM:
                case BN_VLIST_TRI_START:
                case BN_VLIST_TRI_VERTNORM:
                    continue;
                case BN_VLIST_POLY_MOVE:
                case BN_VLIST_LINE_MOVE:
                case BN_VLIST_TRI_MOVE:
                    /* Move, not draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            pt_prev = pt;
                            dist_prev = dist;
                            continue;
                        } else {
                            MAT4X3PNT(last, psmat, *pt);
                            dist_prev = dist;
                            pt_prev = pt;
                        }
                    } else
                        MAT4X3PNT(last, psmat, *pt);
                    continue;
                case BN_VLIST_POLY_DRAW:
                case BN_VLIST_POLY_END:
                case BN_VLIST_LINE_DRAW:
                case BN_VLIST_TRI_DRAW:
               case BN_VLIST_TRI_END:
                    /* draw */
                    if (perspective > 0) {
                        /* cannot apply perspective transformation to
                         * points behind eye plane!!!!
                         */
                        dist = VDOT(*pt, &psmat[12]) + psmat[15];
                        if (dist <= 0.0) {
                            if (dist_prev <= 0.0) {
                                /* nothing to plot */
                                dist_prev = dist;
                                pt_prev = pt;
                                continue;
                            } else {
                                if (pt_prev) {
                                fastf_t alpha;
                                vect_t diff;
                                point_t tmp_pt;

                                /* clip this end */
                                VSUB2(diff, *pt, *pt_prev);
                                alpha = (dist_prev - delta) / (dist_prev - dist);
                                VJOIN1(tmp_pt, *pt_prev, alpha, diff);
                                MAT4X3PNT(fin, psmat, tmp_pt);
                                }
                            }
                        } else {
                            if (dist_prev <= 0.0) {
                                if (pt_prev) {
                                fastf_t alpha;
                                vect_t diff;
                                point_t tmp_pt;

                                /* clip other end */
                                VSUB2(diff, *pt, *pt_prev);
                                alpha = (-dist_prev + delta) / (dist - dist_prev);
                                VJOIN1(tmp_pt, *pt_prev, alpha, diff);
                                MAT4X3PNT(last, psmat, tmp_pt);
                                MAT4X3PNT(fin, psmat, *pt);
                                }
                            } else {
                                MAT4X3PNT(fin, psmat, *pt);
                            }
                        }
                    } else
                        MAT4X3PNT(fin, psmat, *pt);
                    VMOVE(start, last);
                    VMOVE(last, fin);
                    break;
            }

            if (bn_ray_vclip(start, fin, clipmin, clipmax) == 0)
                continue;

            fprintf(fp,
                    "newpath %d %d moveto %d %d lineto stroke\n",
                    PS_COORD(start[0] * 2047),
                    PS_COORD(start[1] * 2047),
                    PS_COORD(fin[0] * 2047),
                    PS_COORD(fin[1] * 2047));
        }
    }
}

static void
ps_draw_body(struct bu_list *hdlp, FILE *fp, mat_t model2view, fastf_t perspective, vect_t eye_pos)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t newmat;
    matp_t mat;
    mat_t perspective_mat;
    struct solid *sp;

    mat = model2view;

    if (0 < perspective) {
        point_t l, h;

        VSET(l, -1.0, -1.0, -1.0);
        VSET(h, 1.0, 1.0, 200.0);

        if (ZERO(eye_pos[Z] - 1.0)) {
            /* This way works, with reasonable Z-clipping */
            persp_mat(perspective_mat, perspective,
                          (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
        } else {
            /* This way does not have reasonable Z-clipping,
             * but includes shear, for GDurf's testing.
             */
            deering_persp_mat(perspective_mat, l, h, eye_pos);
        }

        bn_mat_mul(newmat, perspective_mat, mat);
        mat = newmat;
    }

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            ps_draw_solid(perspective, fp, sp, mat);
        }

        gdlp = next_gdlp;
    }
}


static void
ps_draw_border(FILE *fp, float red, float green, float blue)
{
    fprintf(fp, "%f %f %f setrgbcolor\n", red, green, blue);
    fprintf(fp, "newpath 0 0 moveto 4096 0 lineto stroke\n");
    fprintf(fp, "newpath 4096 0 moveto 4096 4096 lineto stroke\n");
    fprintf(fp, "newpath 4096 4096 moveto 0 4096 lineto stroke\n");
    fprintf(fp, "newpath 0 4096 moveto 0 0 lineto stroke\n");
}


static void
ps_draw_footer(FILE *fp)
{
    fputs("% showpage   % uncomment to use raw file\n", fp);
    fputs("%end(plot)\n", fp);
}

void
dl_ps(struct bu_list *hdlp, FILE *fp, int border, char *font, char *title, char *creator, int linewidth, fastf_t scale, int xoffset, int yoffset, mat_t model2view, fastf_t perspective, vect_t eye_pos, float red, float green, float blue)
{
    ps_draw_header(fp, font, title, creator, linewidth, scale, xoffset, yoffset);
    if (border)
	ps_draw_border(fp, red, green, blue);
    ps_draw_body(hdlp, fp, model2view, perspective, eye_pos);
    ps_draw_footer(fp);

}


void
dl_print_schain(struct bu_list *hdlp, struct db_i *dbip, int lvl, int vlcmds, struct bu_vls *vls)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;

    if (!vlcmds) {
	/*
	 * Given a pointer to a member of the circularly linked list of solids
	 * (typically the head), chase the list and print out the information
	 * about each solid structure.
	 */
	int nvlist;
	int npts;

	if (dbip == DBI_NULL) return;

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		if (lvl <= -2) {
		    /* print only leaves */
		    bu_vls_printf(vls, "%s ", LAST_SOLID(sp)->d_namep);
		    continue;
		}

		db_path_to_vls(vls, &sp->s_fullpath);

		if ((lvl != -1) && (sp->s_iflag == UP))
		    bu_vls_printf(vls, " ILLUM");

		bu_vls_printf(vls, "\n");

		if (lvl <= 0)
		    continue;

		/* convert to the local unit for printing */
		bu_vls_printf(vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
			sp->s_center[X]*dbip->dbi_base2local,
			sp->s_center[Y]*dbip->dbi_base2local,
			sp->s_center[Z]*dbip->dbi_base2local,
			sp->s_size*dbip->dbi_base2local);
		bu_vls_printf(vls, "reg=%d\n", sp->s_regionid);
		bu_vls_printf(vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s%s\n",
			sp->s_basecolor[0],
			sp->s_basecolor[1],
			sp->s_basecolor[2],
			sp->s_color[0],
			sp->s_color[1],
			sp->s_color[2],
			sp->s_uflag?" U":"",
			sp->s_dflag?" D":"",
			sp->s_cflag?" C":"");

		if (lvl <= 1)
		    continue;

		/* Print the actual vector list */
		nvlist = 0;
		npts = 0;
		for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		    int i;
		    int nused = vp->nused;
		    int *cmd = vp->cmd;
		    point_t *pt = vp->pt;

		    BN_CK_VLIST(vp);
		    nvlist++;
		    npts += nused;

		    if (lvl <= 2)
			continue;

		    for (i = 0; i < nused; i++, cmd++, pt++) {
			bu_vls_printf(vls, "  %s (%g, %g, %g)\n",
				      bn_vlist_get_cmd_description(*cmd),
				      V3ARGS(*pt));
		    }
		}

		bu_vls_printf(vls, "  %d vlist structures, %d pts\n", nvlist, npts);
		bu_vls_printf(vls, "  %d pts (via bn_ck_vlist)\n", bn_ck_vlist(&(sp->s_vlist)));
	    }

	    gdlp = next_gdlp;
	}

    } else {
	/*
	 * Given a pointer to a member of the circularly linked list of solids
	 * (typically the head), chase the list and print out the vlist cmds
	 * for each structure.
	 */

	if (dbip == DBI_NULL) return;

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		bu_vls_printf(vls, "-1 %d %d %d\n",
			sp->s_color[0],
			sp->s_color[1],
			sp->s_color[2]);

		/* Print the actual vector list */
		for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		    int i;
		    int nused = vp->nused;
		    int *cmd = vp->cmd;
		    point_t *pt = vp->pt;

		    BN_CK_VLIST(vp);

		    for (i = 0; i < nused; i++, cmd++, pt++)
			bu_vls_printf(vls, "%d %g %g %g\n", *cmd, V3ARGS(*pt));
		}
	    }

	    gdlp = next_gdlp;
	}

    }
}

void
dl_bitwise_and_fullpath(struct bu_list *hdlp, int flag_val)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    size_t i;
    struct solid *sp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            for (i = 0; i < sp->s_fullpath.fp_len; i++) {
                DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= flag_val;
            }
        }

        gdlp = next_gdlp;
    }
}

void
dl_write_animate(struct bu_list *hdlp, FILE *fp)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    size_t i;
    struct solid *sp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            for (i = 0; i < sp->s_fullpath.fp_len; i++) {
                if (!(DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags & RT_DIR_USED)) {
                    struct animate *anp;
                    for (anp = DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_animate; anp;
                         anp=anp->an_forw) {
                        db_write_anim(fp, anp);
                    }
                    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags |= RT_DIR_USED;
                }
            }
        }

        gdlp = next_gdlp;
    }
}



int
dl_select(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag)
{
   struct display_list *gdlp = NULL;
    struct display_list *next_gdlp = NULL;
    struct solid *sp = NULL;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    if (rflag) {
        vr = vwidth;
    } else {
        vmin_x = vx;
        vmin_y = vy;

        if (vwidth > 0)
            vmax_x = vx + vwidth;
        else {
            vmin_x = vx + vwidth;
            vmax_x = vx;
        }

        if (vheight > 0)
            vmax_y = vy + vheight;
        else {
            vmin_y = vy + vheight;
            vmax_y = vy;
        }
    }

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            point_t vmin, vmax;
            struct bn_vlist *vp;

            vmax[X] = vmax[Y] = vmax[Z] = -INFINITY;
            vmin[X] = vmin[Y] = vmin[Z] =  INFINITY;

            for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
                int j;
                int nused = vp->nused;
                int *cmd = vp->cmd;
                point_t *pt = vp->pt;
                point_t vpt;
                for (j = 0; j < nused; j++, cmd++, pt++) {
                    switch (*cmd) {
                        case BN_VLIST_POLY_START:
                        case BN_VLIST_POLY_VERTNORM:
                        case BN_VLIST_TRI_START:
                        case BN_VLIST_TRI_VERTNORM:
                        case BN_VLIST_POINT_SIZE:
                        case BN_VLIST_LINE_WIDTH:
                            /* attribute, not location */
                            break;
                        case BN_VLIST_LINE_MOVE:
                        case BN_VLIST_LINE_DRAW:
                        case BN_VLIST_POLY_MOVE:
                        case BN_VLIST_POLY_DRAW:
                        case BN_VLIST_POLY_END:
                        case BN_VLIST_TRI_MOVE:
                        case BN_VLIST_TRI_DRAW:
                        case BN_VLIST_TRI_END:
                            MAT4X3PNT(vpt, model2view, *pt);
                            V_MIN(vmin[X], vpt[X]);
                            V_MAX(vmax[X], vpt[X]);
                            V_MIN(vmin[Y], vpt[Y]);
                            V_MAX(vmax[Y], vpt[Y]);
                            V_MIN(vmin[Z], vpt[Z]);
                            V_MAX(vmax[Z], vpt[Z]);
                            break;
                        default: {
                            bu_vls_printf(vls, "unknown vlist op %d\n", *cmd);
                        }
                    }
                }
            }

            if (rflag) {
                point_t vloc;
                vect_t diff;
                fastf_t mag;

                VSET(vloc, vx, vy, vmin[Z]);
                VSUB2(diff, vmin, vloc);
                mag = MAGNITUDE(diff);

                if (mag > vr)
                    continue;

                VSET(vloc, vx, vy, vmax[Z]);
                VSUB2(diff, vmax, vloc);
                mag = MAGNITUDE(diff);

                if (mag > vr)
                    continue;

                db_path_to_vls(vls, &sp->s_fullpath);
                bu_vls_printf(vls, "\n");
            } else {
                if (vmin_x <= vmin[X] && vmax[X] <= vmax_x &&
                    vmin_y <= vmin[Y] && vmax[Y] <= vmax_y) {
                    db_path_to_vls(vls, &sp->s_fullpath);
                    bu_vls_printf(vls, "\n");
                }
            }
        }

        gdlp = next_gdlp;
    }

    return GED_OK;
}

int
dl_select_partial(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag)
{
    struct display_list *gdlp = NULL;
    struct display_list *next_gdlp = NULL;
    struct solid *sp = NULL;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    if (rflag) {
        vr = vwidth;
    } else {
        vmin_x = vx;
        vmin_y = vy;

        if (vwidth > 0)
            vmax_x = vx + vwidth;
        else {
            vmin_x = vx + vwidth;
            vmax_x = vx;
        }

        if (vheight > 0)
            vmax_y = vy + vheight;
        else {
            vmin_y = vy + vheight;
            vmax_y = vy;
        }
    }

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            struct bn_vlist *vp;

            for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
                int j;
                int nused = vp->nused;
                int *cmd = vp->cmd;
                point_t *pt = vp->pt;
                point_t vpt;
                for (j = 0; j < nused; j++, cmd++, pt++) {
                    switch (*cmd) {
                        case BN_VLIST_POLY_START:
                        case BN_VLIST_POLY_VERTNORM:
                        case BN_VLIST_TRI_START:
                        case BN_VLIST_TRI_VERTNORM:
                            /* Has normal vector, not location */
                            break;
                        case BN_VLIST_LINE_MOVE:
                        case BN_VLIST_LINE_DRAW:
                        case BN_VLIST_POLY_MOVE:
                        case BN_VLIST_POLY_DRAW:
                        case BN_VLIST_POLY_END:
                        case BN_VLIST_TRI_MOVE:
                        case BN_VLIST_TRI_DRAW:
                        case BN_VLIST_TRI_END:
                            MAT4X3PNT(vpt, model2view, *pt);

                            if (rflag) {
                                point_t vloc;
                                vect_t diff;
                                fastf_t mag;

                                VSET(vloc, vx, vy, vpt[Z]);
                                VSUB2(diff, vpt, vloc);
                                mag = MAGNITUDE(diff);

                                if (mag > vr)
                                    continue;

                                db_path_to_vls(vls, &sp->s_fullpath);
                                bu_vls_printf(vls, "\n");

                                goto solid_done;
                           } else {
                                if (vmin_x <= vpt[X] && vpt[X] <= vmax_x &&
                                    vmin_y <= vpt[Y] && vpt[Y] <= vmax_y) {
                                    db_path_to_vls(vls, &sp->s_fullpath);
                                    bu_vls_printf(vls, "\n");

                                    goto solid_done;
                                }
                            }

                            break;
                        default: {
                            bu_vls_printf(vls, "unknown vlist op %d\n", *cmd);
                        }
                    }
                }
            }

            solid_done:
            ;
        }

        gdlp = next_gdlp;
    }

    return GED_OK;
}


void
dl_set_transparency(struct bu_list *hdlp, struct directory **dpp, double transparency, void (*callback)(struct display_list *))
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    size_t i;
    struct directory **tmp_dpp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
            for (i = 0, tmp_dpp = dpp;
                 i < sp->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
                 ++i, ++tmp_dpp) {
                if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
                    break;
            }

            if (*tmp_dpp != RT_DIR_NULL)
                continue;

	    /* found a match */
	    sp->s_transparency = transparency;

	}

	if (callback != GED_CREATE_VLIST_CALLBACK_PTR_NULL)
	    (*callback)(gdlp);

        gdlp = next_gdlp;
    }

}

void
dl_botdump(struct bu_list *hdlp, struct db_i *dbip, FILE *fp, int fd, char *file_ext, int output_type, int *red, int *green, int *blue, fastf_t *alpha)
{
    int ret;
    mat_t mat;
    struct display_list *gdlp;

    MAT_IDN(mat);

    for (BU_LIST_FOR(gdlp, display_list, hdlp)) {
	struct solid *sp;

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    struct directory *dp;
	    struct rt_db_internal intern;
	    struct rt_bot_internal *bot;

	    dp = sp->s_fullpath.fp_names[sp->s_fullpath.fp_len-1];

	    /* get the internal form */
	    ret=rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource);

	    if (ret < 0) {
		bu_log("rt_get_internal failure %d on %s\n", ret, dp->d_namep);
		continue;
	    }

	    if (ret != ID_BOT) {
		bu_log("%s is not a bot (ignored)\n", dp->d_namep);
		rt_db_free_internal(&intern);
		continue;
	    }

	    /* Write out object color */
	    if (output_type == OTYPE_OBJ) {
		(*red) = sp->s_color[0];
		(*green) = sp->s_color[1];
		(*blue) = sp->s_color[2];
		(*alpha) = sp->s_transparency;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    _ged_bot_dump(dp, bot, fp, fd, file_ext, dbip->dbi_filename);
	    rt_db_free_internal(&intern);
	}
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
