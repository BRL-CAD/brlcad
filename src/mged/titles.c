/*                        T I T L E S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/titles.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/units.h"
#include "bn.h"
#include "ged.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./menu.h"

#define USE_OLD_MENUS 0

char *state_str[] = {
    "-ZOT-",
    "VIEWING",
    "SOL PICK",
    "SOL EDIT",
    "OBJ PICK",
    "OBJ PATH",
    "OBJ EDIT",
    "VERTPICK",
    "UNKNOWN",
};


/* Ew. Global. */
extern mat_t perspective_mat;  /* defined in dozoom.c */

/*
 * Prepare the numerical display of the currently edited solid/object.
 */
void
create_text_overlay(struct mged_state *s, struct bu_vls *vp)
{
    struct directory *dp;

    BU_CK_VLS(vp);

    /*
     * Set up for character output.  For the best generality, we
     * don't assume that the display can process a CRLF sequence,
     * so each line is written with a separate call to dm_draw_string_2d().
     */

    /* print solid info at top of screen
     * Check if the illuminated solid still exists or it has been killed
     * before Accept was clicked.
     */
    if (es_edflag >= 0 && illump != NULL && illump->s_u_data != NULL) {
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	dp = LAST_SOLID(bdata);

	bu_vls_strcat(vp, "** SOLID -- ");
	bu_vls_strcat(vp, dp->d_namep);
	bu_vls_strcat(vp, ": ");

	vls_solid(s, vp, &s->edit_state.es_int, bn_mat_identity);

	if (bdata->s_fullpath.fp_len > 1) {
	    bu_vls_strcat(vp, "\n** PATH --  ");
	    db_path_to_vls(vp, &bdata->s_fullpath);
	    bu_vls_strcat(vp, ": ");

	    /* print the evaluated (path) solid parameters */
	    vls_solid(s, vp, &s->edit_state.es_int, s->edit_state.e_mat);
	}
    }

    /* display path info for object editing also */
    if (s->edit_state.global_editing_state == ST_O_EDIT && illump != NULL && illump->s_u_data != NULL) {
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	bu_vls_strcat(vp, "** PATH --  ");
	db_path_to_vls(vp, &bdata->s_fullpath);
	bu_vls_strcat(vp, ": ");

	/* print the evaluated (path) solid parameters */
	if (illump->s_old.s_Eflag == 0) {
	    mat_t new_mat;
	    /* NOT an evaluated region */
	    /* object edit option selected */
	    bn_mat_mul(new_mat, s->edit_state.model_changes, s->edit_state.e_mat);

	    vls_solid(s, vp, &s->edit_state.es_int, new_mat);
	}
    }

    {
	char *start;
	char *p;
	int imax = 0;
	int i = 0;
	int j;
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	start = bu_vls_addr(vp);
	/*
	 * Some display managers don't handle TABs properly, so
	 * we replace any TABs with spaces. Also, look for the
	 * maximum line length.
	 */
	for (p = start; *p != '\0'; ++p) {
	    if (*p == '\t')
		*p = ' ';
	    else if (*p == '\n') {
		if (i > imax)
		    imax = i;
		i = 0;
	    } else
		++i;
	}

	if (i > imax)
	    imax = i;

	/* Prep string for use with Tcl/Tk */
	++imax;
	i = 0;
	for (p = start; *p != '\0'; ++p) {
	    if (*p == '\n') {
		for (j = 0; j < imax - i; ++j)
		    bu_vls_putc(&vls, ' ');

		bu_vls_putc(&vls, *p);
		i = 0;
	    } else {
		bu_vls_putc(&vls, *p);
		++i;
	    }
	}

	Tcl_SetVar(s->interp, "edit_info", bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
	bu_vls_free(&vls);
    }
}


/*
 * Output a vls string to the display manager,
 * as a text overlay on the graphics area (ugh).
 *
 * Set up for character output.  For the best generality, we
 * don't assume that the display can process a CRLF sequence,
 * so each line is written with a separate call to dm_draw_string_2d().
 */
void
screen_vls(
	struct mged_state *s,
	int xbase,
	int ybase,
	struct bu_vls *vp)
{
    char *start;
    char *end;
    int y;

    BU_CK_VLS(vp);
    y = ybase;

    dm_set_fg(DMP,
		   color_scheme->cs_edit_info[0],
		   color_scheme->cs_edit_info[1],
		   color_scheme->cs_edit_info[2], 1, 1.0);

    start = bu_vls_addr(vp);
    while (*start != '\0') {
	if ((end = strchr(start, '\n')) == NULL) return;

	*end = '\0';

	dm_draw_string_2d(DMP, start,
			  GED2PM1(xbase), GED2PM1(y), 0, 0);
	start = end+1;
	y += TEXT0_DY;
    }
}


/*
 * Produce titles, etc., on the screen.
 * NOTE that this routine depends on being called AFTER dozoom();
 */
void
dotitles(struct mged_state *s, struct bu_vls *overlay_vls)
{
    size_t i = 0;

    /* for menu computations */
    int x = 0;
    int y = 0;

    int yloc = 0;
    int xloc = 0;
    int scroll_ybot = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    char cent_x[80] = {0};
    char cent_y[80] = {0};
    char cent_z[80] = {0};
    char size[80] = {0};
    char ang_x[80] = {0};
    char ang_y[80] = {0};
    char ang_z[80] = {0};

    int ss_line_not_drawn=1; /* true if the second status line has not been drawn */
    vect_t temp = VINIT_ZERO;
    fastf_t tmp_val = 0.0;

    if (s->dbip == DBI_NULL)
	return;

    /* Set the Tcl variables to the appropriate values. */

    if (illump != NULL && illump->s_u_data != NULL) {
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	struct bu_vls path_lhs = BU_VLS_INIT_ZERO;
	struct bu_vls path_rhs = BU_VLS_INIT_ZERO;
	struct directory *dp;
	struct db_full_path *dbfp = &bdata->s_fullpath;

	if (!dbfp) {
	    bu_vls_free(&vls);
	    return;
	}
	RT_CK_FULL_PATH(dbfp);

	for (i = 0; i < (size_t)ipathpos; i++) {
	    if ((size_t)i < (size_t)dbfp->fp_len) {
		dp = DB_FULL_PATH_GET(dbfp, i);
		if (dp && dp->d_namep) {
		    bu_vls_printf(&path_lhs, "/%s", dp->d_namep);
		}
	    }
	}
	for (; i < dbfp->fp_len; i++) {
	    dp = DB_FULL_PATH_GET(dbfp, i);
	    if (dp && dp->d_namep) {
		bu_vls_printf(&path_rhs, "/%s", dp->d_namep);
	    }
	}

	bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), bu_vls_addr(&path_lhs), TCL_GLOBAL_ONLY);
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), bu_vls_addr(&path_rhs), TCL_GLOBAL_ONLY);
	bu_vls_free(&path_rhs);
	bu_vls_free(&path_lhs);
    } else {
	bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
    }

    /* take some care here to avoid buffer overrun */
    tmp_val = -view_state->vs_gvp->gv_center[MDX]*s->dbip->dbi_base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_x, "%.3f", tmp_val);
    } else {
	sprintf(cent_x, "%.3g", tmp_val);
    }
    tmp_val = -view_state->vs_gvp->gv_center[MDY]*s->dbip->dbi_base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_y, "%.3f", tmp_val);
    } else {
	sprintf(cent_y, "%.3g", tmp_val);
    }
    tmp_val = -view_state->vs_gvp->gv_center[MDZ]*s->dbip->dbi_base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_z, "%.3f", tmp_val);
    } else {
	sprintf(cent_z, "%.3g", tmp_val);
    }
    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "cent=(%s %s %s)", cent_x, cent_y, cent_z);
    Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_center_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    tmp_val = view_state->vs_gvp->gv_size*s->dbip->dbi_base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(size, "sz=%.3f", tmp_val);
    } else {
	sprintf(size, "sz=%.3g", tmp_val);
    }
    Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_size_name),
	       size, TCL_GLOBAL_ONLY);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "%s(units)", MGED_DISPLAY_VAR);
    Tcl_SetVar(s->interp, bu_vls_addr(&vls),
	       (char *)bu_units_string(s->dbip->dbi_local2base), TCL_GLOBAL_ONLY);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "az=%3.2f  el=%3.2f  tw=%3.2f", V3ARGS(view_state->vs_gvp->gv_aet));
    Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_aet_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    sprintf(ang_x, "%.2f", view_state->vs_gvp->k.rot_v[X]);
    sprintf(ang_y, "%.2f", view_state->vs_gvp->k.rot_v[Y]);
    sprintf(ang_z, "%.2f", view_state->vs_gvp->k.rot_v[Z]);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "ang=(%s %s %s)", ang_x, ang_y, ang_z);
    Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_ang_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    /* Label the vertices of the edited solid */
    if (es_edflag >= 0 || (s->edit_state.global_editing_state == ST_O_EDIT && illump->s_old.s_Eflag == 0)) {
	mat_t xform;
	struct rt_point_labels pl[8+1];
	point_t lines[2*4];	/* up to 4 lines to draw */
	int num_lines=0;

	if (view_state->vs_gvp->gv_perspective <= 0)
	    bn_mat_mul(xform, view_state->vs_model2objview, s->edit_state.e_mat);
	else {
	    mat_t tmat;

	    bn_mat_mul(tmat, view_state->vs_model2objview, s->edit_state.e_mat);
	    bn_mat_mul(xform, perspective_mat, tmat);
	}

	label_edited_solid(s, &num_lines, lines,  pl, 8+1, xform, &s->edit_state.es_int);

	dm_set_fg(DMP,
		       color_scheme->cs_geo_label[0],
		       color_scheme->cs_geo_label[1],
		       color_scheme->cs_geo_label[2], 1, 1.0);
	for (i=0; i<(size_t)num_lines; i++)
	    dm_draw_line_2d(DMP,
			    GED2PM1(((int)(lines[i*2][X]*BV_MAX))),
			    GED2PM1(((int)(lines[i*2][Y]*BV_MAX)) * dm_get_aspect(DMP)),
			    GED2PM1(((int)(lines[i*2+1][X]*BV_MAX))),
			    GED2PM1(((int)(lines[i*2+1][Y]*BV_MAX)) * dm_get_aspect(DMP)));
	for (i=0; i<8+1; i++) {
	    if (pl[i].str[0] == '\0') break;
	    dm_draw_string_2d(DMP, pl[i].str,
			      GED2PM1(((int)(pl[i].pt[X]*BV_MAX))+15),
			      GED2PM1(((int)(pl[i].pt[Y]*BV_MAX))+15), 0, 1);
	}
    }

    if (mged_variables->mv_faceplate) {
	/* Line across the bottom, above two bottom status lines */
	dm_set_fg(DMP,
		       color_scheme->cs_other_line[0],
		       color_scheme->cs_other_line[1],
		       color_scheme->cs_other_line[2], 1, 1.0);
	dm_draw_line_2d(DMP,
			GED2PM1((int)BV_MIN), GED2PM1(TITLE_YBASE-TEXT1_DY),
			GED2PM1((int)BV_MAX), GED2PM1(TITLE_YBASE-TEXT1_DY));

	if (mged_variables->mv_orig_gui) {
	    /* Enclose window in decorative box.  Mostly for alignment. */
	    dm_draw_line_2d(DMP,
			    GED2PM1((int)BV_MIN), GED2PM1((int)BV_MIN),
			    GED2PM1((int)BV_MAX), GED2PM1((int)BV_MIN));
	    dm_draw_line_2d(DMP,
			    GED2PM1((int)BV_MAX), GED2PM1((int)BV_MIN),
			    GED2PM1((int)BV_MAX), GED2PM1((int)BV_MAX));
	    dm_draw_line_2d(DMP,
			    GED2PM1((int)BV_MAX), GED2PM1((int)BV_MAX),
			    GED2PM1((int)BV_MIN), GED2PM1((int)BV_MAX));
	    dm_draw_line_2d(DMP,
			    GED2PM1((int)BV_MIN), GED2PM1((int)BV_MAX),
			    GED2PM1((int)BV_MIN), GED2PM1((int)BV_MIN));

	    /* Display scroll bars */
	    scroll_ybot = scroll_display(s, SCROLLY);
	    y = MENUY;
	    x = MENUX;

	    /* Display state and local unit in upper left corner, boxed */
	    dm_set_fg(DMP,
			   color_scheme->cs_state_text1[0],
			   color_scheme->cs_state_text1[1],
			   color_scheme->cs_state_text1[2], 1, 1.0);
	    dm_draw_string_2d(DMP, state_str[s->edit_state.global_editing_state],
			      GED2PM1(MENUX), GED2PM1(MENUY - MENU_DY), 1, 0);
	} else {
	    scroll_ybot = SCROLLY;
	    x = (int)BV_MIN + 20;
	    y = (int)BV_MAX+TEXT0_DY;
	}

	/*
	 * Print information about object illuminated
	 */
	if (illump != NULL && illump->s_u_data != NULL &&
	    (s->edit_state.global_editing_state == ST_O_PATH || s->edit_state.global_editing_state==ST_O_PICK || s->edit_state.global_editing_state==ST_S_PICK)) {

	    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	    for (i=0; i < bdata->s_fullpath.fp_len; i++) {
		if (i == (size_t)ipathpos  &&  s->edit_state.global_editing_state == ST_O_PATH) {
		    dm_set_fg(DMP,
				   color_scheme->cs_state_text1[0],
				   color_scheme->cs_state_text1[1],
				   color_scheme->cs_state_text1[2], 1, 1.0);
		    dm_draw_string_2d(DMP, "[MATRIX]",
				      GED2PM1(x), GED2PM1(y), 0, 0);
		    y += MENU_DY;
		}
		dm_set_fg(DMP,
			       color_scheme->cs_state_text2[0],
			       color_scheme->cs_state_text2[1],
			       color_scheme->cs_state_text2[2], 1, 1.0);
		dm_draw_string_2d(DMP,
				  DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep,
				  GED2PM1(x), GED2PM1(y), 0, 0);
		y += MENU_DY;
	    }
	}

	if (mged_variables->mv_orig_gui) {
	    dm_set_fg(DMP,
			   color_scheme->cs_other_line[0],
			   color_scheme->cs_other_line[1],
			   color_scheme->cs_other_line[2], 1, 1.0);
	    dm_draw_line_2d(DMP,
			    GED2PM1(MENUXLIM), GED2PM1(y),
			    GED2PM1(MENUXLIM), GED2PM1((int)BV_MAX));	/* vert. */
	    /*
	     * The top of the menu (if any) begins at the Y value specified.
	     */
	    mmenu_display(s, y);

	    /* print parameter locations on screen */
	    if (s->edit_state.global_editing_state == ST_O_EDIT && illump->s_old.s_Eflag) {
		/* region is a processed region */
		MAT4X3PNT(temp, view_state->vs_model2objview, s->edit_state.e_keypoint);
		xloc = (int)(temp[X]*BV_MAX);
		yloc = (int)(temp[Y]*BV_MAX);
		dm_set_fg(DMP,
			       color_scheme->cs_edit_info[0],
			       color_scheme->cs_edit_info[1],
			       color_scheme->cs_edit_info[2], 1, 1.0);
		dm_draw_line_2d(DMP,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		dm_draw_line_2d(DMP,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		dm_draw_line_2d(DMP,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		dm_draw_line_2d(DMP,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		dm_draw_line_2d(DMP,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		dm_draw_line_2d(DMP,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
	    }
	}

	/*
	 * Prepare the numerical display of the currently edited solid/object.
	 */
	/* create_text_overlay(s, &vls); */
	if (mged_variables->mv_orig_gui) {
	    screen_vls(s, SOLID_XBASE, scroll_ybot+TEXT0_DY, overlay_vls);
	} else {
	    screen_vls(s, x, y, overlay_vls);
	}

	/*
	 * General status information on first status line
	 */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls,
		      " cent=(%s, %s, %s), %s %s, ", cent_x, cent_y, cent_z,
		      size, bu_units_string(s->dbip->dbi_local2base));
	bu_vls_printf(&vls, "az=%3.2f el=%3.2f tw=%3.2f ang=(%s, %s, %s)", V3ARGS(view_state->vs_gvp->gv_aet),
		      ang_x, ang_y, ang_z);
	dm_set_fg(DMP,
		       color_scheme->cs_status_text1[0],
		       color_scheme->cs_status_text1[1],
		       color_scheme->cs_status_text1[2], 1, 1.0);
	dm_draw_string_2d(DMP, bu_vls_addr(&vls),
			  GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE), 1, 0);
    } /* if faceplate !0 */

    /*
     * Second status line
     */

    /* Priorities for what to display:
     * 1.  adc info
     * 2.  keypoint
     * 3.  illuminated path
     *
     * This way the adc info will be displayed during editing
     */

    if (adc_state->adc_draw) {
	fastf_t f;

	f = view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local;
	/* Angle/Distance cursor */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls,
		      " curs:  a1=%.1f,  a2=%.1f,  dst=%.3f,  cent=(%.3f, %.3f),  delta=(%.3f, %.3f)",
		      adc_state->adc_a1, adc_state->adc_a2,
		      adc_state->adc_dst * f,
		      adc_state->adc_pos_grid[X] * f, adc_state->adc_pos_grid[Y] * f,
		      adc_state->adc_pos_view[X] * f, adc_state->adc_pos_view[Y] * f);
	if (mged_variables->mv_faceplate) {
	    dm_set_fg(DMP,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    dm_draw_string_2d(DMP, bu_vls_addr(&vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
	}
	Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_adc_name),
		   bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
	ss_line_not_drawn = 0;
    } else {
	Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_adc_name), "", TCL_GLOBAL_ONLY);
    }

    if (s->edit_state.global_editing_state == ST_S_EDIT || s->edit_state.global_editing_state == ST_O_EDIT) {
	struct bu_vls kp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&kp_vls,
		      " Keypoint: %s %s: (%g, %g, %g)",
		      OBJ[s->edit_state.es_int.idb_type].ft_name+3,	/* Skip ID_ */
		      s->edit_state.e_keytag,
		      s->edit_state.e_keypoint[X] * s->dbip->dbi_base2local,
		      s->edit_state.e_keypoint[Y] * s->dbip->dbi_base2local,
		      s->edit_state.e_keypoint[Z] * s->dbip->dbi_base2local);
	if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	    dm_set_fg(DMP,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    dm_draw_string_2d(DMP, bu_vls_addr(&kp_vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
	    ss_line_not_drawn = 0;
	}

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), bu_vls_addr(&kp_vls), TCL_GLOBAL_ONLY);

	bu_vls_free(&kp_vls);
    } else {
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	Tcl_SetVar(s->interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
    }

    if (illump != NULL && illump->s_u_data != NULL) {

	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	    bu_vls_trunc(&vls, 0);

	    /* Illuminated path */
	    bu_vls_strcat(&vls, " Path: ");
	    for (i=0; i < bdata->s_fullpath.fp_len; i++) {
		if (i == (size_t)ipathpos  &&
		    (s->edit_state.global_editing_state == ST_O_PATH || s->edit_state.global_editing_state == ST_O_EDIT))
		    bu_vls_strcat(&vls, "/__MATRIX__");
		bu_vls_printf(&vls, "/%s",
			      DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep);
	    }
	    dm_set_fg(DMP,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    dm_draw_string_2d(DMP, bu_vls_addr(&vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);

	    ss_line_not_drawn = 0;
	}
    }

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "%.2f fps", 1/frametime);
    if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	dm_set_fg(DMP,
		       color_scheme->cs_status_text2[0],
		       color_scheme->cs_status_text2[1],
		       color_scheme->cs_status_text2[2], 1, 1.0);
	dm_draw_string_2d(DMP, bu_vls_addr(&vls),
			  GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
    }
    Tcl_SetVar(s->interp, bu_vls_addr(&s->mged_curr_dm->dm_fps_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    bu_vls_free(&vls);
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
