/*                        T I T L E S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "ged.h"

#include "./mged.h"
#include "./titles.h"
#include "./sedit.h"
#include "./mged_dm.h"

#define USE_OLD_MENUS 0

/* should only be accessed via STATE define in mged.h */
int ged_state;

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


extern mat_t perspective_mat;  /* defined in dozoom.c */
extern struct rt_db_internal es_int;

/*
 * C R E A T E _ T E X T _ O V E R L A Y
 *
 * Prepare the numerical display of the currently edited solid/object.
 */
void
create_text_overlay(struct bu_vls *vp)
{
    struct directory *dp;

    BU_CK_VLS(vp);

    /*
     * Set up for character output.  For the best generality, we
     * don't assume that the display can process a CRLF sequence,
     * so each line is written with a separate call to DM_DRAW_STRING_2D().
     */

    /* print solid info at top of screen */
    if (es_edflag >= 0) {
	dp = LAST_SOLID(illump);

	bu_vls_strcat(vp, "** SOLID -- ");
	bu_vls_strcat(vp, dp->d_namep);
	bu_vls_strcat(vp, ": ");

	vls_solid(vp, &es_int, bn_mat_identity);

	if (illump->s_fullpath.fp_len > 1) {
	    bu_vls_strcat(vp, "\n** PATH --  ");
	    db_path_to_vls(vp, &illump->s_fullpath);
	    bu_vls_strcat(vp, ": ");

	    /* print the evaluated (path) solid parameters */
	    vls_solid(vp, &es_int, es_mat);
	}
    }

    /* display path info for object editing also */
    if (STATE == ST_O_EDIT) {
	bu_vls_strcat(vp, "** PATH --  ");
	db_path_to_vls(vp, &illump->s_fullpath);
	bu_vls_strcat(vp, ": ");

	/* print the evaluated (path) solid parameters */
	if (illump->s_Eflag == 0) {
	    mat_t new_mat;
	    /* NOT an evaluated region */
	    /* object edit option selected */
	    bn_mat_mul(new_mat, modelchanges, es_mat);

	    vls_solid(vp, &es_int, new_mat);
	}
    }

    {
	char *start;
	char *p;
	int imax = 0;
	int i = 0;
	int j;
	struct bu_vls vls;

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
	bu_vls_init(&vls);
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

	Tcl_SetVar(INTERP, "edit_info", bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
	bu_vls_free(&vls);
    }
}


/*
 * S C R E E N _ V L S
 *
 * Output a vls string to the display manager,
 * as a text overlay on the graphics area (ugh).
 *
 * Set up for character output.  For the best generality, we
 * don't assume that the display can process a CRLF sequence,
 * so each line is written with a separate call to DM_DRAW_STRING_2D().
 */
void
screen_vls(
    int xbase,
    int ybase,
    struct bu_vls *vp)
{
    char *start;
    char *end;
    int y;

    BU_CK_VLS(vp);
    y = ybase;

    DM_SET_FGCOLOR(dmp,
		   color_scheme->cs_edit_info[0],
		   color_scheme->cs_edit_info[1],
		   color_scheme->cs_edit_info[2], 1, 1.0);

    start = bu_vls_addr(vp);
    while (*start != '\0') {
	if ((end = strchr(start, '\n')) == NULL) return;

	*end = '\0';

	DM_DRAW_STRING_2D(dmp, start,
			  GED2PM1(xbase), GED2PM1(y), 0, 0);
	start = end+1;
	y += TEXT0_DY;
    }
}


/*
 * D O T I T L E S
 *
 * Produce titles, etc, on the screen.
 * NOTE that this routine depends on being called AFTER dozoom();
 */
void
dotitles(struct bu_vls *overlay_vls)
{
    size_t i = 0;

    /* for menu computations */
    int x = 0;
    int y = 0;

    int yloc = 0;
    int xloc = 0;
    int scroll_ybot = 0;
    struct bu_vls vls;

    char cent_x[80] = {0};
    char cent_y[80] = {0};
    char cent_z[80] = {0};
    char size[80] = {0};
    char ang_x[80] = {0};
    char ang_y[80] = {0};
    char ang_z[80] = {0};

    int ss_line_not_drawn=1; /* true if the second status line has not been drawn */
    vect_t temp = {0.0, 0.0, 0.0};
    fastf_t tmp_val = 0.0;

    if (dbip == DBI_NULL)
	return;

    bu_vls_init(&vls);

    /* Set the Tcl variables to the appropriate values. */

    if (illump != SOLID_NULL) {
	struct bu_vls path_lhs, path_rhs;
	struct directory *dp;
	struct db_full_path *dbfp = &illump->s_fullpath;

	if (!dbfp) {
	    bu_vls_free(&vls);
	    return;
	}
	RT_CK_FULL_PATH(dbfp);

	bu_vls_init(&path_lhs);
	bu_vls_init(&path_rhs);
	for (i = 0; i < (size_t)ipathpos; i++) {
	    dp = DB_FULL_PATH_GET(dbfp, i);
	    if (dp && dp->d_namep) {
		bu_vls_printf(&path_lhs, "/%s", dp->d_namep);
	    }
	}
	for (; i < dbfp->fp_len; i++) {
	    dp = DB_FULL_PATH_GET(dbfp, i);
	    if (dp && dp->d_namep) {
		bu_vls_printf(&path_rhs, "/%s", dp->d_namep);
	    }
	}

	bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), bu_vls_addr(&path_lhs), TCL_GLOBAL_ONLY);
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), bu_vls_addr(&path_rhs), TCL_GLOBAL_ONLY);
	bu_vls_free(&path_rhs);
	bu_vls_free(&path_lhs);
    } else {
	bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
    }

    /* take some care here to avoid buffer overrun */
    tmp_val = -view_state->vs_gvp->gv_center[MDX]*base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_x, "%.3f", tmp_val);
    } else {
	sprintf(cent_x, "%.3g", tmp_val);
    }
    tmp_val = -view_state->vs_gvp->gv_center[MDY]*base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_y, "%.3f", tmp_val);
    } else {
	sprintf(cent_y, "%.3g", tmp_val);
    }
    tmp_val = -view_state->vs_gvp->gv_center[MDZ]*base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(cent_z, "%.3f", tmp_val);
    } else {
	sprintf(cent_z, "%.3g", tmp_val);
    }
    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "cent=(%s %s %s)", cent_x, cent_y, cent_z);
    Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_center_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    tmp_val = view_state->vs_gvp->gv_size*base2local;
    if (fabs(tmp_val) < 10e70) {
	sprintf(size, "sz=%.3f", tmp_val);
    } else {
	sprintf(size, "sz=%.3g", tmp_val);
    }
    Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_size_name),
	       size, TCL_GLOBAL_ONLY);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "%s(units)", MGED_DISPLAY_VAR);
    Tcl_SetVar(INTERP, bu_vls_addr(&vls),
	       (char *)bu_units_string(dbip->dbi_local2base), TCL_GLOBAL_ONLY);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "az=%3.2f  el=%3.2f  tw=%3.2f", V3ARGS(view_state->vs_gvp->gv_aet));
    Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_aet_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    sprintf(ang_x, "%.2f", view_state->vs_rate_rotate[X]);
    sprintf(ang_y, "%.2f", view_state->vs_rate_rotate[Y]);
    sprintf(ang_z, "%.2f", view_state->vs_rate_rotate[Z]);

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "ang=(%s %s %s)", ang_x, ang_y, ang_z);
    Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_ang_name),
	       bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

    DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);

    /* Label the vertices of the edited solid */
    if (es_edflag >= 0 || (STATE == ST_O_EDIT && illump->s_Eflag == 0)) {
	mat_t xform;
	struct rt_point_labels pl[8+1];
	point_t lines[2*4];	/* up to 4 lines to draw */
	int num_lines=0;

	if (view_state->vs_gvp->gv_perspective <= 0)
	    bn_mat_mul(xform, view_state->vs_model2objview, es_mat);
	else {
	    mat_t tmat;

	    bn_mat_mul(tmat, view_state->vs_model2objview, es_mat);
	    bn_mat_mul(xform, perspective_mat, tmat);
	}

	label_edited_solid(&num_lines, lines,  pl, 8+1, xform, &es_int);

	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_geo_label[0],
		       color_scheme->cs_geo_label[1],
		       color_scheme->cs_geo_label[2], 1, 1.0);
	for (i=0; i<(size_t)num_lines; i++)
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(((int)(lines[i*2][X]*GED_MAX))),
			    GED2PM1(((int)(lines[i*2][Y]*GED_MAX)) * dmp->dm_aspect),
			    GED2PM1(((int)(lines[i*2+1][X]*GED_MAX))),
			    GED2PM1(((int)(lines[i*2+1][Y]*GED_MAX)) * dmp->dm_aspect));
	for (i=0; i<8+1; i++) {
	    if (pl[i].str[0] == '\0') break;
	    DM_DRAW_STRING_2D(dmp, pl[i].str,
			      GED2PM1(((int)(pl[i].pt[X]*GED_MAX))+15),
			      GED2PM1(((int)(pl[i].pt[Y]*GED_MAX))+15), 0, 1);
	}
    }

    if (mged_variables->mv_faceplate) {
	/* Line across the bottom, above two bottom status lines */
	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_other_line[0],
		       color_scheme->cs_other_line[1],
		       color_scheme->cs_other_line[2], 1, 1.0);
	DM_DRAW_LINE_2D(dmp,
			GED2PM1(XMIN), GED2PM1(TITLE_YBASE-TEXT1_DY),
			GED2PM1(XMAX), GED2PM1(TITLE_YBASE-TEXT1_DY));

	if (mged_variables->mv_orig_gui) {
	    /* Enclose window in decorative box.  Mostly for alignment. */
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(XMIN), GED2PM1(YMIN),
			    GED2PM1(XMAX), GED2PM1(YMIN));
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(XMAX), GED2PM1(YMIN),
			    GED2PM1(XMAX), GED2PM1(YMAX));
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(XMAX), GED2PM1(YMAX),
			    GED2PM1(XMIN), GED2PM1(YMAX));
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(XMIN), GED2PM1(YMAX),
			    GED2PM1(XMIN), GED2PM1(YMIN));

	    /* Display scroll bars */
	    scroll_ybot = scroll_display(SCROLLY);
	    y = MENUY;
	    x = MENUX;

	    /* Display state and local unit in upper left corner, boxed */
	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_state_text1[0],
			   color_scheme->cs_state_text1[1],
			   color_scheme->cs_state_text1[2], 1, 1.0);
	    DM_DRAW_STRING_2D(dmp, state_str[STATE],
			      GED2PM1(MENUX), GED2PM1(MENUY - MENU_DY), 1, 0);
	} else {
	    scroll_ybot = SCROLLY;
	    x = XMIN + 20;
	    y = YMAX+TEXT0_DY;
	}

	/*
	 * Print information about object illuminated
	 */
	if (illump != SOLID_NULL &&
	    (STATE==ST_O_PATH || STATE==ST_O_PICK || STATE==ST_S_PICK)) {
	    for (i=0; i < illump->s_fullpath.fp_len; i++) {
		if (i == (size_t)ipathpos  &&  STATE == ST_O_PATH) {
		    DM_SET_FGCOLOR(dmp,
				   color_scheme->cs_state_text1[0],
				   color_scheme->cs_state_text1[1],
				   color_scheme->cs_state_text1[2], 1, 1.0);
		    DM_DRAW_STRING_2D(dmp, "[MATRIX]",
				      GED2PM1(x), GED2PM1(y), 0, 0);
		    y += MENU_DY;
		}
		DM_SET_FGCOLOR(dmp,
			       color_scheme->cs_state_text2[0],
			       color_scheme->cs_state_text2[1],
			       color_scheme->cs_state_text2[2], 1, 1.0);
		DM_DRAW_STRING_2D(dmp,
				  DB_FULL_PATH_GET(&illump->s_fullpath, i)->d_namep,
				  GED2PM1(x), GED2PM1(y), 0, 0);
		y += MENU_DY;
	    }
	}

	if (mged_variables->mv_orig_gui) {
	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_other_line[0],
			   color_scheme->cs_other_line[1],
			   color_scheme->cs_other_line[2], 1, 1.0);
	    DM_DRAW_LINE_2D(dmp,
			    GED2PM1(MENUXLIM), GED2PM1(y),
			    GED2PM1(MENUXLIM), GED2PM1(YMAX));	/* vert. */
	    /*
	     * The top of the menu (if any) begins at the Y value specified.
	     */
	    mmenu_display(y);

	    /* print parameter locations on screen */
	    if (STATE == ST_O_EDIT && illump->s_Eflag) {
		/* region is a processed region */
		MAT4X3PNT(temp, view_state->vs_model2objview, es_keypoint);
		xloc = (int)(temp[X]*GED_MAX);
		yloc = (int)(temp[Y]*GED_MAX);
		DM_SET_FGCOLOR(dmp,
			       color_scheme->cs_edit_info[0],
			       color_scheme->cs_edit_info[1],
			       color_scheme->cs_edit_info[2], 1, 1.0);
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
	    }
	}

	/*
	 * Prepare the numerical display of the currently edited solid/object.
	 */
	/* create_text_overlay(&vls); */
	if (mged_variables->mv_orig_gui) {
	    screen_vls(SOLID_XBASE, scroll_ybot+TEXT0_DY, overlay_vls);
	} else {
	    screen_vls(x, y, overlay_vls);
	}

	/*
	 * General status information on first status line
	 */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls,
		      " cent=(%s, %s, %s), %s %s, ", cent_x, cent_y, cent_z,
		      size, bu_units_string(dbip->dbi_local2base));
	bu_vls_printf(&vls, "az=%3.2f el=%3.2f tw=%3.2f ang=(%s, %s, %s)", V3ARGS(view_state->vs_gvp->gv_aet),
		      ang_x, ang_y, ang_z);
	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_status_text1[0],
		       color_scheme->cs_status_text1[1],
		       color_scheme->cs_status_text1[2], 1, 1.0);
	DM_DRAW_STRING_2D(dmp, bu_vls_addr(&vls),
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

	f = view_state->vs_gvp->gv_scale * base2local;
	/* Angle/Distance cursor */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls,
		      " curs:  a1=%.1f,  a2=%.1f,  dst=%.3f,  cent=(%.3f, %.3f),  delta=(%.3f, %.3f)",
		      adc_state->adc_a1, adc_state->adc_a2,
		      adc_state->adc_dst * f,
		      adc_state->adc_pos_grid[X] * f, adc_state->adc_pos_grid[Y] * f,
		      adc_state->adc_pos_view[X] * f, adc_state->adc_pos_view[Y] * f);
	if (mged_variables->mv_faceplate) {
	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    DM_DRAW_STRING_2D(dmp, bu_vls_addr(&vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
	}
	Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_adc_name),
		   bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
	ss_line_not_drawn = 0;
    } else {
	Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_adc_name), "", TCL_GLOBAL_ONLY);
    }

    if (STATE == ST_S_EDIT || STATE == ST_O_EDIT) {
	struct bu_vls kp_vls;

	bu_vls_init(&kp_vls);
	bu_vls_printf(&kp_vls,
		      " Keypoint: %s %s: (%g, %g, %g)",
		      rt_functab[es_int.idb_type].ft_name+3,	/* Skip ID_ */
		      es_keytag,
		      es_keypoint[X] * base2local,
		      es_keypoint[Y] * base2local,
		      es_keypoint[Z] * base2local);
	if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    DM_DRAW_STRING_2D(dmp, bu_vls_addr(&kp_vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
	    ss_line_not_drawn = 0;
	}

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), bu_vls_addr(&kp_vls), TCL_GLOBAL_ONLY);

	bu_vls_free(&kp_vls);
    } else {
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	Tcl_SetVar(INTERP, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
    }

    if (illump != SOLID_NULL) {
	if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	    bu_vls_trunc(&vls, 0);

	    /* Illuminated path */
	    bu_vls_strcat(&vls, " Path: ");
	    for (i=0; i < illump->s_fullpath.fp_len; i++) {
		if (i == (size_t)ipathpos  &&
		    (STATE == ST_O_PATH || STATE == ST_O_EDIT))
		    bu_vls_strcat(&vls, "/__MATRIX__");
		bu_vls_printf(&vls, "/%s",
			      DB_FULL_PATH_GET(&illump->s_fullpath, i)->d_namep);
	    }
	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_status_text2[0],
			   color_scheme->cs_status_text2[1],
			   color_scheme->cs_status_text2[2], 1, 1.0);
	    DM_DRAW_STRING_2D(dmp, bu_vls_addr(&vls),
			      GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);

	    ss_line_not_drawn = 0;
	}
    }

    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "%.2f fps", 1/frametime);
    if (mged_variables->mv_faceplate && ss_line_not_drawn) {
	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_status_text2[0],
		       color_scheme->cs_status_text2[1],
		       color_scheme->cs_status_text2[2], 1, 1.0);
	DM_DRAW_STRING_2D(dmp, bu_vls_addr(&vls),
			  GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0);
    }
    Tcl_SetVar(INTERP, bu_vls_addr(&curr_dm_list->dml_fps_name),
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
