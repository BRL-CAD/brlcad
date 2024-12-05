/*                        D O Z O O M . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/dozoom.c
 *
 */

#include "common.h"

#include <math.h>
#include "vmath.h"
#include "bn.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

mat_t perspective_mat;
mat_t incr_change;
mat_t modelchanges;
mat_t identity;


/* This is a holding place for the current display managers default wireframe color */
unsigned char geometry_default_color[] = { 255, 0, 0 };

/*
 * This routine reviews all of the solids in the solids table,
 * to see if they are visible within the current viewing
 * window.  If they are, the routine computes the scale and appropriate
 * screen position for the object.
 */
void
dozoom(struct mged_state *s, int which_eye)
{
    int ndrawn = 0;
    fastf_t inv_viewsize = 0.0;
    mat_t newmat = MAT_INIT_ZERO;
    matp_t mat = newmat;
    short r = -1;
    short g = -1;
    short b = -1;

    /*
     * The vectorThreshold stuff in libdm may turn the
     * Tcl-crank causing mged_curr_dm to change.
     */
    struct mged_dm *save_dm_list = mged_curr_dm;

    mged_curr_dm->dm_ndrawn = 0;
    inv_viewsize = view_state->vs_gvp->gv_isize;

    /*
     * Draw all solids not involved in an edit.
     */
    if (view_state->vs_gvp->gv_perspective < SMALL_FASTF && EQUAL(view_state->vs_gvp->gv_eye_pos[Z], 1.0)) {
	mat = view_state->vs_gvp->gv_model2view;
    } else {
	/*
	 * There are two strategies that could be used:
	 * 1) Assume a standard head location w.r.t. the
	 * screen, and fix the perspective angle.
	 * 2) Based upon the perspective angle, compute
	 * where the head should be to achieve that field of view.
	 * Try strategy #2 for now.
	 */
	fastf_t to_eye_scr;	/* screen space dist to eye */
	fastf_t eye_delta_scr;	/* scr, 1/2 inter-occular dist */
	point_t l, h, eye;

	/* Determine where eye should be */
	to_eye_scr = 1 / tan(view_state->vs_gvp->gv_perspective * DEG2RAD * 0.5);

#define SCR_WIDTH_PHYS 330	/* Assume a 330 mm wide screen */

	eye_delta_scr = mged_variables->mv_eye_sep_dist * 0.5 / SCR_WIDTH_PHYS;

	VSET(l, -1.0, -1.0, -1.0);
	VSET(h, 1.0, 1.0, 200.0);
	if (which_eye) {
	    printf("d=%gscr, d=%gmm, delta=%gscr\n", to_eye_scr, to_eye_scr * SCR_WIDTH_PHYS, eye_delta_scr);
	    VPRINT("l", l);
	    VPRINT("h", h);
	}
	VSET(eye, 0.0, 0.0, to_eye_scr);

	switch (which_eye) {
	    case 0:
		/* Non-stereo case */
		mat = view_state->vs_gvp->gv_model2view;
		if (EQUAL(view_state->vs_gvp->gv_eye_pos[Z], 1.0)) {
		    /* This way works, with reasonable Z-clipping */
		    persp_mat(perspective_mat, view_state->vs_gvp->gv_perspective,
			    (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
		} else {
		    /* This way does not have reasonable Z-clipping,
		     * but includes shear, for GDurf's testing.
		     */
		    deering_persp_mat(perspective_mat, l, h, view_state->vs_gvp->gv_eye_pos);
		}
		break;
	    case 1:
		/* R */
		mat = view_state->vs_gvp->gv_model2view;
		eye[X] = eye_delta_scr;
		deering_persp_mat(perspective_mat, l, h, eye);
		break;
	    case 2:
		/* L */
		mat = view_state->vs_gvp->gv_model2view;
		eye[X] = -eye_delta_scr;
		deering_persp_mat(perspective_mat, l, h, eye);
		break;
	}
	bn_mat_mul(newmat, perspective_mat, mat);
	mat = newmat;
    }

    dm_loadmatrix(DMP, mat, which_eye);

    if (dm_get_transparency(DMP)) {
	/* First, draw opaque stuff */

	ndrawn = dm_draw_head_dl(DMP, s->GEDP->ged_gdp->gd_headDisplay, 1.0, inv_viewsize,
				      r, g, b, mged_variables->mv_linewidth, mged_variables->mv_dlist, 0,
				      geometry_default_color, 1, mged_variables->mv_dlist);

	/* The vectorThreshold stuff in libdm may turn the Tcl-crank causing mged_curr_dm to change. */
	if (mged_curr_dm != save_dm_list) set_curr_dm(s, save_dm_list);

	mged_curr_dm->dm_ndrawn += ndrawn;

	/* disable write to depth buffer */
	dm_set_depth_mask(DMP, 0);

	/* Second, draw transparent stuff */

	ndrawn = dm_draw_head_dl(DMP, s->GEDP->ged_gdp->gd_headDisplay, 0.0, inv_viewsize,
				      r, g, b, mged_variables->mv_linewidth, mged_variables->mv_dlist, 0,
				      geometry_default_color, 0, mged_variables->mv_dlist);

	/* re-enable write of depth buffer */
	dm_set_depth_mask(DMP, 1);

    } else {

	ndrawn = dm_draw_head_dl(DMP, s->GEDP->ged_gdp->gd_headDisplay, 1.0, inv_viewsize,
				      r, g, b, mged_variables->mv_linewidth, mged_variables->mv_dlist, 0,
				      geometry_default_color, 1, mged_variables->mv_dlist);

    }

    /* The vectorThreshold stuff in libdm may turn the Tcl-crank causing mged_curr_dm to change. */
    if (mged_curr_dm != save_dm_list) set_curr_dm(s, save_dm_list);

    mged_curr_dm->dm_ndrawn += ndrawn;


    /* draw predictor vlist */
    if (mged_variables->mv_predictor) {
	dm_set_fg(DMP,
		       color_scheme->cs_predictor[0],
		       color_scheme->cs_predictor[1],
		       color_scheme->cs_predictor[2], 1, 1.0);
	dm_draw_vlist(DMP, (struct bv_vlist *)&mged_curr_dm->dm_p_vlist);
    }

    /*
     * Draw all solids involved in editing.
     * They may be getting transformed away from the other solids.
     */
    if (STATE == ST_VIEW)
	return;

    if (view_state->vs_gvp->gv_perspective <= 0) {
	mat = view_state->vs_model2objview;
    } else {
	bn_mat_mul(newmat, perspective_mat, view_state->vs_model2objview);
	mat = newmat;
    }
    dm_loadmatrix(DMP, mat, which_eye);
    inv_viewsize /= modelchanges[15];
    dm_set_fg(DMP,
		   color_scheme->cs_geo_hl[0],
		   color_scheme->cs_geo_hl[1],
		   color_scheme->cs_geo_hl[2], 1, 1.0);


    ndrawn = dm_draw_head_dl(DMP, s->GEDP->ged_gdp->gd_headDisplay, 1.0, inv_viewsize,
	    r, g, b, mged_variables->mv_linewidth, mged_variables->mv_dlist, 1,
	    geometry_default_color, 0, mged_variables->mv_dlist);

    mged_curr_dm->dm_ndrawn += ndrawn;

    /* The vectorThreshold stuff in libdm may turn the Tcl-crank causing mged_curr_dm to change. */
    if (mged_curr_dm != save_dm_list) set_curr_dm(s, save_dm_list);
}

/*
 * Create Display Lists
 */
void
createDLists(struct bu_list *hdlp)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	dm_set_dirty(DMP, 1);
	dm_draw_display_list(DMP, gdlp);

	gdlp = next_gdlp;
    }
}

/*
 * Create a display list for "sp" for every display manager
 * manager that:
 * 1 - supports display lists
 * 2 - is actively using display lists
 * 3 - has not already been created (i.e. sharing with a
 * display manager that has already created the display list)
 */
void
createDListSolid(void *vlist_ctx, struct bv_scene_obj *sp)
{
    struct mged_state *s = (struct mged_state *)vlist_ctx;
    struct mged_dm *save_dlp;

    save_dlp = mged_curr_dm;

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *dlp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (dlp->dm_mapped &&
		dm_get_displaylist(dlp->dm_dmp) &&
		dlp->dm_mged_variables->mv_dlist) {
	    if (sp->s_dlist == 0)
		sp->s_dlist = dm_gen_dlists(DMP, 1);

	    dm_set_dirty(DMP, 1);
	    (void)dm_make_current(DMP);
	    (void)dm_begin_dlist(DMP, sp->s_dlist);
	    if (sp->s_iflag == UP)
		(void)dm_set_fg(DMP, 255, 255, 255, 0, sp->s_os->transparency);
	    else
		(void)dm_set_fg(DMP,
			(unsigned char)sp->s_color[0],
			(unsigned char)sp->s_color[1],
			(unsigned char)sp->s_color[2], 0, sp->s_os->transparency);
	    (void)dm_draw_vlist(DMP, (struct bv_vlist *)&sp->s_vlist);
	    (void)dm_end_dlist(DMP);
	}

	dlp->dm_dirty = 1;
	dm_set_dirty(DMP, 1);
    }

    set_curr_dm(s, save_dlp);
}

/*
 * Create a display list for "sp" for every display manager
 * manager that:
 * 1 - supports display lists
 * 2 - is actively using display lists
 * 3 - has not already been created (i.e. sharing with a
 * display manager that has already created the display list)
 */
void
createDListAll(void *vlist_ctx, struct display_list *gdlp)
{
    struct mged_state *s = (struct mged_state *)vlist_ctx;
    struct bv_scene_obj *sp;
    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	createDListSolid(s, sp);
    }
}


/*
 * Free the range of display lists for all display managers
 * that support display lists and have them activated.
 */
void
freeDListsAll(unsigned int dlist, int range)
{
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *dlp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (dm_get_displaylist(dlp->dm_dmp) &&
	    dlp->dm_mged_variables->mv_dlist) {
	    (void)dm_make_current(DMP);
	    (void)dm_free_dlists(dlp->dm_dmp, dlist, range);
	}

	dlp->dm_dirty = 1;
	dm_set_dirty(DMP, 1);
    }
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
