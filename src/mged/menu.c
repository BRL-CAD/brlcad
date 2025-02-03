/*                          M E N U . C
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
/** @file mged/menu.c
 *
 */

#include "common.h"

#include "tcl.h"

#include "vmath.h"
#include "raytrace.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./sedit.h"
#include "./menu.h"

#include "ged.h"


extern void sl_halt_scroll(struct rt_solid_edit *, int, int, int, void *);	/* in scroll.c */
extern void sl_toggle_scroll(struct rt_solid_edit *, int, int, int, void *);

void btn_head_menu(struct rt_solid_edit *s, int i, int menu, int item, void *data);
void btn_item_hit(struct rt_solid_edit *s, int arg, int menu, int item, void *data);

struct menu_item first_menu[] = {
    { "BUTTON MENU", btn_head_menu, 1 },		/* chg to 2nd menu */
    { "", NULL, 0 }
};
struct menu_item second_menu[] = {
    { "BUTTON MENU", btn_head_menu, 0 },	/* chg to 1st menu */
    { "REJECT Edit", btn_item_hit, BE_REJECT },
    { "ACCEPT Edit", btn_item_hit, BE_ACCEPT },
    { "35,25", btn_item_hit, BV_35_25 },
    { "Top", btn_item_hit, BV_TOP },
    { "Right", btn_item_hit, BV_RIGHT },
    { "Front", btn_item_hit, BV_FRONT },
    { "45,45", btn_item_hit, BV_45_45 },
    { "Restore View", btn_item_hit, BV_VRESTORE },
    { "Save View", btn_item_hit, BV_VSAVE },
    { "Ang/Dist Curs", btn_item_hit, BV_ADCURSOR },
    { "Reset Viewsize", btn_item_hit, BV_RESET },
    { "Zero Sliders", sl_halt_scroll, 0 },
    { "Sliders", sl_toggle_scroll, 0 },
    { "Rate/Abs", btn_item_hit, BV_RATE_TOGGLE },
    { "Zoom In 2X", btn_item_hit, BV_ZOOM_IN },
    { "Zoom Out 2X", btn_item_hit, BV_ZOOM_OUT },
    { "Primitive Illum", btn_item_hit, BE_S_ILLUMINATE },
    { "Matrix Illum", btn_item_hit, BE_O_ILLUMINATE },
    { "", NULL, 0 }
};
struct menu_item sed_menu[] = {
    { "*PRIMITIVE EDIT*", btn_head_menu, 2 },
    { "Edit Menu", btn_item_hit, BE_S_EDIT },
    { "Rotate", btn_item_hit, BE_S_ROTATE },
    { "Translate", btn_item_hit, BE_S_TRANS },
    { "Scale", btn_item_hit, BE_S_SCALE },
    { "", NULL, 0 }
};


struct menu_item oed_menu[] = {
    { "*MATRIX EDIT*", btn_head_menu, 2 },
    { "Scale", btn_item_hit, BE_O_SCALE },
    { "X Move", btn_item_hit, BE_O_X },
    { "Y Move", btn_item_hit, BE_O_Y },
    { "XY Move", btn_item_hit, BE_O_XY },
    { "Rotate", btn_item_hit, BE_O_ROTATE },
    { "Scale X", btn_item_hit, BE_O_XSCALE },
    { "Scale Y", btn_item_hit, BE_O_YSCALE },
    { "Scale Z", btn_item_hit, BE_O_ZSCALE },
    { "", NULL, 0 }
};


/*
 * Called when a menu item is hit
 */
void
btn_item_hit(struct rt_solid_edit *UNUSED(es), int arg, int menu, int UNUSED(item), void *data)
{
    struct mged_state *s = (struct mged_state *)data;
    button(s, arg);
    if (menu == MENU_GEN &&
	(arg != BE_O_ILLUMINATE && arg != BE_S_ILLUMINATE))
	menu_state->ms_flag = 0;
}

/*
 * Called to handle hits on menu heads.
 * Also called from main() with arg 0 in init.
 */
void
btn_head_menu(struct rt_solid_edit *UNUSED(es), int i, int UNUSED(menu), int UNUSED(item), void *data)
{
    struct mged_state *s = (struct mged_state *)data;
    switch (i) {
	case 0:
	    mmenu_set(s, MENU_GEN, first_menu);
	    break;
	case 1:
	    mmenu_set(s, MENU_GEN, second_menu);
	    break;
	case 2:
	    /* nothing happens */
	    break;
	default:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "btn_head_menu(%d): bad arg\n", i);
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
    }
}

void
chg_l2menu(struct mged_state *s, int i) {
    switch (i) {
	case ST_S_EDIT:
	    mmenu_set_all(s, MENU_L2, sed_menu);
	    break;
	case ST_S_NO_EDIT:
	    mmenu_set_all(s, MENU_L2, NULL);
	    break;
	case ST_O_EDIT:
	    mmenu_set_all(s, MENU_L2, oed_menu);
	    break;
	default:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "chg_l2menu(%d): bad arg\n", i);
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
    }
}



int
cmd_mmenu_get(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int index;

    if (argc > 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel mmenu_get");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 2) {
	struct menu_item **m, *mptr;

	if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK)
	    return TCL_ERROR;

	if (index < 0 || NMENU <= index) {
	    Tcl_AppendResult(interp, "index out of range", (char *)NULL);
	    return TCL_ERROR;
	}

	m = menu_state->ms_menus+index;
	if (*m == NULL)
	    return TCL_OK;

	for (mptr = *m; mptr->menu_string[0] != '\0'; mptr++)
	    Tcl_AppendElement(interp, mptr->menu_string);
    } else {
	struct menu_item **m;
	struct bu_vls result = BU_VLS_INIT_ZERO;
	int status;

	bu_vls_strcat(&result, "list");
	for (m = menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++)
	    bu_vls_printf(&result, " [%s %ld]", argv[0], (long int)(m-menu_state->ms_menus));

	status = Tcl_Eval(interp, bu_vls_addr(&result));
	bu_vls_free(&result);

	return status;
    }

    return TCL_OK;
}


/*
 * Clear global data
 */
void
mmenu_init(struct mged_state *s)
{
    menu_state->ms_flag = 0;
    menu_state->ms_menus[MENU_L1] = NULL;
    menu_state->ms_menus[MENU_L2] = NULL;
    menu_state->ms_menus[MENU_GEN] = NULL;
}


int
mged_mmenu_set(int UNUSED(ac), const char **UNUSED(av), void *d, void *ms)
{
    struct mged_state *s = (struct mged_state *)d;
    struct menu_item *value = (struct menu_item *)ms;
    int index = MENU_L1;

    // All uses of mmenu_set in primitives did this, so just do it here instead.
    menu_state->ms_flag = 0;

    Tcl_DString ds_menu;
    struct bu_vls menu_string = BU_VLS_INIT_ZERO;

    menu_state->ms_menus[index] = value;  /* Change the menu internally */

    Tcl_DStringInit(&ds_menu);

    bu_vls_printf(&menu_string, "mmenu_set %s %d ", bu_vls_addr(&curr_cmd_list->cl_name), index);

    (void)Tcl_Eval(s->interp, bu_vls_addr(&menu_string));

    Tcl_DStringFree(&ds_menu);
    bu_vls_free(&menu_string);

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *dlp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (menu_state == dlp->dm_menu_state &&
	    dlp->dm_mged_variables->mv_faceplate &&
	    dlp->dm_mged_variables->mv_orig_gui) {
	    dlp->dm_dirty = 1;
	    dm_set_dirty(dlp->dm_dmp, 1);
	}
    }

    return TCL_OK;
}


void
mmenu_set(struct mged_state *s, int index, struct menu_item *value)
{
    if (!value)
	return;

    Tcl_DString ds_menu;
    struct bu_vls menu_string = BU_VLS_INIT_ZERO;

    menu_state->ms_menus[index] = value;  /* Change the menu internally */

    Tcl_DStringInit(&ds_menu);

    bu_vls_printf(&menu_string, "mmenu_set %s %d ", bu_vls_addr(&curr_cmd_list->cl_name), index);

    (void)Tcl_Eval(s->interp, bu_vls_addr(&menu_string));

    Tcl_DStringFree(&ds_menu);
    bu_vls_free(&menu_string);

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *dlp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (menu_state == dlp->dm_menu_state &&
	    dlp->dm_mged_variables->mv_faceplate &&
	    dlp->dm_mged_variables->mv_orig_gui) {
	    dlp->dm_dirty = 1;
	    dm_set_dirty(dlp->dm_dmp, 1);
	}
    }
}


void
mmenu_set_all(struct mged_state *s, int index, struct menu_item *value)
{
    struct cmd_list *save_cmd_list;
    struct mged_dm *save_dm_list;

    save_cmd_list = curr_cmd_list;
    save_dm_list = s->mged_curr_dm;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (p->dm_tie)
	    curr_cmd_list = p->dm_tie;

	set_curr_dm(s, p);
	mmenu_set(s, index, value);
    }

    curr_cmd_list = save_cmd_list;
    set_curr_dm(s, save_dm_list);
}


void
mged_highlight_menu_item(struct mged_state *s, struct menu_item *mptr, int y)
{
    switch (mptr->menu_arg) {
	case BV_RATE_TOGGLE:
	    if (mged_variables->mv_rateknobs) {
		dm_set_fg(DMP,
			       color_scheme->cs_menu_text1[0],
			       color_scheme->cs_menu_text1[1],
			       color_scheme->cs_menu_text1[2], 1, 1.0);
		dm_draw_string_2d(DMP, "Rate",
				  GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
		dm_set_fg(DMP,
			       color_scheme->cs_menu_text2[0],
			       color_scheme->cs_menu_text2[1],
			       color_scheme->cs_menu_text2[2], 1, 1.0);
		dm_draw_string_2d(DMP, "/Abs",
				  GED2PM1(MENUX+4*40), GED2PM1(y-15), 0, 0);
	    } else {
		dm_set_fg(DMP,
			       color_scheme->cs_menu_text2[0],
			       color_scheme->cs_menu_text2[1],
			       color_scheme->cs_menu_text2[2], 1, 1.0);
		dm_draw_string_2d(DMP, "Rate/",
				  GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
		dm_set_fg(DMP,
			       color_scheme->cs_menu_text1[0],
			       color_scheme->cs_menu_text1[1],
			       color_scheme->cs_menu_text1[2], 1, 1.0);
		dm_draw_string_2d(DMP, "Abs",
				  GED2PM1(MENUX+5*40), GED2PM1(y-15), 0, 0);
	    }
	    break;
	default:
	    break;
    }
}


/*
 * Draw one or more menus onto the display.
 * If "menu_state->ms_flag" is non-zero, then the last selected
 * menu item will be indicated with an arrow.
 */
void
mmenu_display(struct mged_state *s, int y_top)
{
    static int menu, item;
    struct menu_item **m;
    struct menu_item *mptr;
    int y = y_top;

    menu_state->ms_top = y - MENU_DY / 2;
    dm_set_fg(DMP,
		   color_scheme->cs_menu_line[0],
		   color_scheme->cs_menu_line[1],
		   color_scheme->cs_menu_line[2], 1, 1.0);

    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    dm_draw_line_2d(DMP,
		    GED2PM1(MENUXLIM), GED2PM1(menu_state->ms_top),
		    GED2PM1((int)GED_MIN), GED2PM1(menu_state->ms_top));

    for (menu=0, m = menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++, menu++) {
	if (*m == NULL) continue;
	for (item=0, mptr = *m; mptr->menu_string[0] != '\0' && y > TITLE_YBASE; mptr++, y += MENU_DY, item++) {
	    if ((*m == (struct menu_item *)second_menu
		 && (mptr->menu_arg == BV_RATE_TOGGLE
		     || mptr->menu_arg == BV_EDIT_TOGGLE
		     || mptr->menu_arg == BV_EYEROT_TOGGLE)))
		mged_highlight_menu_item(s, mptr, y);
	    else {
		if (mptr == *m)
		    dm_set_fg(DMP,
				   color_scheme->cs_menu_title[0],
				   color_scheme->cs_menu_title[1],
				   color_scheme->cs_menu_title[2], 1, 1.0);
		else
		    dm_set_fg(DMP,
				   color_scheme->cs_menu_text2[0],
				   color_scheme->cs_menu_text2[1],
				   color_scheme->cs_menu_text2[2], 1, 1.0);
		dm_draw_string_2d(DMP, mptr->menu_string,
				  GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
	    }
	    dm_set_fg(DMP,
			   color_scheme->cs_menu_line[0],
			   color_scheme->cs_menu_line[1],
			   color_scheme->cs_menu_line[2], 1, 1.0);
	    dm_draw_line_2d(DMP,
			    GED2PM1(MENUXLIM), GED2PM1(y+(MENU_DY/2)),
			    GED2PM1((int)GED_MIN), GED2PM1(y+(MENU_DY/2)));
	    if (menu_state->ms_cur_item == item && menu_state->ms_cur_menu == menu && menu_state->ms_flag) {
		/* prefix item selected with "==>" */
		dm_set_fg(DMP,
			       color_scheme->cs_menu_arrow[0],
			       color_scheme->cs_menu_arrow[1],
			       color_scheme->cs_menu_arrow[2], 1, 1.0);
		dm_draw_string_2d(DMP, "==>",
				  GED2PM1((int)GED_MIN), GED2PM1(y-15), 0, 0);
	    }
	}
    }

    if (y == y_top)
	return;	/* no active menus */

    dm_set_fg(DMP,
		   color_scheme->cs_menu_line[0],
		   color_scheme->cs_menu_line[1],
		   color_scheme->cs_menu_line[2], 1, 1.0);

    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    dm_draw_line_2d(DMP,
		    GED2PM1(MENUXLIM), GED2PM1(menu_state->ms_top-1),
		    GED2PM1(MENUXLIM), GED2PM1(y-(MENU_DY/2)));
}


/*
 * Called with Y coordinate of pen in menu area.
 *
 * Returns:
 * 1 if menu claims these pen coordinates,
 * 0 if pen is BELOW menu
 * -1 if pen is ABOVE menu (error)
 */
int
mmenu_select(struct mged_state *s, int pen_y, int do_func)
{
    static int menu, item;
    struct menu_item **m;
    struct menu_item *mptr;
    int yy;

    if (pen_y > menu_state->ms_top)
	return -1;	/* pen above menu area */

    /*
     * Start at the top of the list and see if the pen is
     * above here.
     */
    yy = menu_state->ms_top;

    for (menu=0, m=menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++, menu++) {
	if (*m == NULL) continue;
	for (item=0, mptr = *m;
	     mptr->menu_string[0] != '\0';
	     mptr++, item++) {
	    yy += MENU_DY;
	    if (pen_y <= yy)
		continue;	/* pen is below this item */
	    menu_state->ms_cur_item = item;
	    menu_state->ms_cur_menu = menu;
	    menu_state->ms_flag = 1;
	    /* It's up to the menu_func to set menu_state->ms_flag=0
	     * if no arrow is desired */
	    if (do_func && mptr->menu_func != NULL)
		(*(mptr->menu_func))(s->s_edit, mptr->menu_arg, menu, item, s);

	    return 1;		/* menu claims pen value */
	}
    }
    return 0;		/* pen below menu area */
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
