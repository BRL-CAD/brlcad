/*                       B U T T O N S . C
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
/** @file mged/buttons.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "dg.h"

#include "./mged.h"
#include "./mged_dm.h"
#include "./sedit.h"


extern int mged_svbase(void);
extern void set_e_axes_pos(int both);
extern int mged_zoom(double val);
extern void set_absolute_tran(void);	/* defined in set.c */
extern void set_scroll_private(void);	/* defined in set.c */
extern void adc_set_scroll(void);	/* defined in adc.c */

/* forward declarations for the buttons table */
int be_accept();
int be_o_illuminate();
int be_o_rotate();
int be_o_scale();
int be_o_x();
int be_o_xscale();
int be_o_xy();
int be_o_y();
int be_o_yscale();
int be_o_zscale();
int be_reject();
int be_s_edit();
int be_s_illuminate();
int be_s_rotate();
int be_s_scale();
int be_s_trans();
int bv_35_25();
int bv_45_45();
int bv_adcursor();
int bv_bottom();
int bv_front();
int bv_left();
int bv_rate_toggle();
int bv_rear();
int bv_reset();
int bv_right();
int bv_top();
int bv_vrestore();
int bv_vsave();
int bv_zoomin();
int bv_zoomout();


/*
 * This flag indicates that Primitive editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (eg, BE_S_SCALE).
 */
static int edsol;

/* This flag indicates that Matrix editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (eg, BE_O_XY).
 */
int edobj;		/* object editing */
int movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

/*
 * The "accumulation" solid rotation matrix and scale factor
 */
mat_t acc_rot_sol;
fastf_t acc_sc_sol;
fastf_t acc_sc_obj;     /* global object scale factor --- accumulations */
fastf_t acc_sc[3];	/* local object scale factors --- accumulations */

/* flag to toggle whether we perform continuous motion tracking
 * (e.g., for a tablet)
 */
int doMotion = 0;


struct buttons {
    int bu_code;	/* dm_values.dv_button */
    char *bu_name;	/* keyboard string */
    int (*bu_func)();	/* function to call */
}  button_table[] = {
    {BV_35_25,		"35,25",	bv_35_25},
    {BV_45_45,		"45,45",	bv_45_45},
    {BE_ACCEPT,		"accept",	be_accept},
    {BV_ADCURSOR,	"adc",		bv_adcursor},
    {BV_BOTTOM,		"bottom",	bv_bottom},
    {BV_FRONT,		"front",	bv_front},
    {BV_LEFT,		"left",		bv_left},
    {BE_O_ILLUMINATE,	"oill",		be_o_illuminate},
    {BE_O_ROTATE,	"orot",		be_o_rotate},
    {BE_O_SCALE,	"oscale",	be_o_scale},
    {BE_O_X,		"ox",		be_o_x},
    {BE_O_XY,		"oxy",		be_o_xy},
    {BE_O_XSCALE,	"oxscale",	be_o_xscale},
    {BE_O_Y,		"oy",		be_o_y},
    {BE_O_YSCALE,	"oyscale",	be_o_yscale},
    {BE_O_ZSCALE,	"ozscale",	be_o_zscale},
    {BV_REAR,		"rear",		bv_rear},
    {BE_REJECT,		"reject",	be_reject},
    {BV_RESET,		"reset",	bv_reset},
    {BV_VRESTORE,	"restore",	bv_vrestore},
    {BV_RIGHT,		"right",	bv_right},
    {BV_VSAVE,		"save",		bv_vsave},
    {BE_S_EDIT,		"sedit",	be_s_edit},
    {BE_S_ILLUMINATE,	"sill",		be_s_illuminate},
    {BE_S_ROTATE,	"srot",		be_s_rotate},
    {BE_S_SCALE,	"sscale",	be_s_scale},
    {BE_S_TRANS,	"sxy",		be_s_trans},
    {BV_TOP,		"top",		bv_top},
    {BV_ZOOM_IN,	"zoomin",	bv_zoomin},
    {BV_ZOOM_OUT,	"zoomout",	bv_zoomout},
    {BV_RATE_TOGGLE,	"rate",		bv_rate_toggle},
    {-1,		"-end-",	be_reject}
};


static mat_t sav_viewrot, sav_toviewcenter;
static fastf_t sav_vscale;
static int vsaved = 0;	/* set iff view saved */

extern void color_soltab(void);
extern void sl_halt_scroll(void);	/* in scroll.c */
extern void sl_toggle_scroll(void);

void btn_head_menu(int i, int menu, int item);
void btn_item_hit(int arg, int menu, int item);

static struct menu_item first_menu[] = {
    { "BUTTON MENU", btn_head_menu, 1 },		/* chg to 2nd menu */
    { "", (void (*)())NULL, 0 }
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
    { "", (void (*)())NULL, 0 }
};
struct menu_item sed_menu[] = {
    { "*PRIMITIVE EDIT*", btn_head_menu, 2 },
    { "Edit Menu", btn_item_hit, BE_S_EDIT },
    { "Rotate", btn_item_hit, BE_S_ROTATE },
    { "Translate", btn_item_hit, BE_S_TRANS },
    { "Scale", btn_item_hit, BE_S_SCALE },
    { "", (void (*)())NULL, 0 }
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
    { "", (void (*)())NULL, 0 }
};


/*
 * B U T T O N
 */
void
button(int bnum)
{
    struct buttons *bp;

    if (edsol && edobj)
	bu_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);

    /* Process the button function requested. */
    for (bp = button_table; bp->bu_code >= 0; bp++) {
	if (bnum != bp->bu_code)
	    continue;

	bp->bu_func((ClientData)NULL, INTERP, 0, NULL);
	return;
    }

    bu_log("button(%d):  Not a defined operation\n", bnum);
}


/*
 * F _ P R E S S
 *
 * Hook for displays with no buttons
 *
 * Given a string description of which button to press, simulate
 * pressing that button on the button box.
 *
 * These days, just hand off to the appropriate Tcl proc of the same name.
 */
int
f_press(ClientData clientData,
	Tcl_Interp *interp,
	int argc,
	const char *argv[])
{
    int i;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help press");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (i = 1; i < argc; i++) {
	const char *str = argv[i];
	struct buttons *bp;
	struct menu_item **m;
	int menu, item;
	struct menu_item *mptr;

	if (edsol && edobj) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	}

	if (BU_STR_EQUAL(str, "help")) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);

	    for (bp = button_table; bp->bu_code >= 0; bp++)
		vls_col_item(&vls, bp->bu_name);
	    vls_col_eol(&vls);

	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	    goto next;
	}

	/* Process the button function requested. */
	for (bp = button_table; bp->bu_code >= 0; bp++) {
	    if (!BU_STR_EQUAL(str, bp->bu_name))
		continue;

	    (void)bp->bu_func(clientData, interp, 2, argv+1);
	    goto next;
	}

	for (menu=0, m=menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++, menu++) {
	    if (*m == MENU_NULL) continue;
	    for (item=0, mptr = *m;
		 mptr->menu_string[0] != '\0';
		 mptr++, item++) {
		if (!BU_STR_EQUAL(str, mptr->menu_string))
		    continue;

		menu_state->ms_cur_item = item;
		menu_state->ms_cur_menu = menu;
		menu_state->ms_flag = 1;
		/* It's up to the menu_func to set menu_state->ms_flag=0
		 * if no arrow is desired */
		if (mptr->menu_func != ((void (*)())0))
		    (*(mptr->menu_func))(mptr->menu_arg, menu, item);

		goto next;
	    }
	}

	Tcl_AppendResult(interp, "press(", str,
			 "):  Unknown operation, type 'press help' for help\n", (char *)NULL);
    next:		;
    }

    return TCL_OK;
}


/*
 * L A B E L _ B U T T O N
 *
 * For a given GED button number, return the "press" ID string.
 * Useful for displays with programable button lables, etc.
 */
char *
label_button(int bnum)
{
    struct buttons *bp;

    /* Process the button function requested. */
    for (bp = button_table; bp->bu_code >= 0; bp++) {
	if (bnum != bp->bu_code)
	    continue;
	return bp->bu_name;
    }

    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "label_button(%d):  Not a defined operation\n", bnum);
	Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    return "";
}


int
bv_zoomin()
{
    (void)mged_zoom(2.0);
    return TCL_OK;
}


int
bv_zoomout()
{
    (void)mged_zoom(0.5);
    return TCL_OK;
}


int
bv_rate_toggle()
{
    mged_variables->mv_rateknobs = !mged_variables->mv_rateknobs;
    set_scroll_private();
    return TCL_OK;
}


int
bv_top()
{
    /* Top view */
    setview(0.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_bottom()
{
    /* Bottom view */
    setview(180.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_right()
{
    /* Right view */
    setview(270.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_left()
{
    /* Left view */
    setview(270.0, 0.0, 180.0);
    return TCL_OK;
}


int
bv_front()
{
    /* Front view */
    setview(270.0, 0.0, 270.0);
    return TCL_OK;
}


int
bv_rear()
{
    /* Rear view */
    setview(270.0, 0.0, 90.0);
    return TCL_OK;
}


int
bv_vrestore()
{
    /* restore to saved view */
    if (vsaved) {
	view_state->vs_gvp->gv_scale = sav_vscale;
	MAT_COPY(view_state->vs_gvp->gv_rotation, sav_viewrot);
	MAT_COPY(view_state->vs_gvp->gv_center, sav_toviewcenter);
	new_mats();

	(void)mged_svbase();
    }

    return TCL_OK;
}


int
bv_vsave()
{
    /* save current view */
    sav_vscale = view_state->vs_gvp->gv_scale;
    MAT_COPY(sav_viewrot, view_state->vs_gvp->gv_rotation);
    MAT_COPY(sav_toviewcenter, view_state->vs_gvp->gv_center);
    vsaved = 1;
    return TCL_OK;
}


/*
 * B V _ A D C U R S O R
 *
 * Toggle state of angle/distance cursor.
 * "press adc"
 * This command conflicts with existing "adc" command,
 * can't be bound as "adc", only as "press adc".
 */
int
bv_adcursor()
{
    if (adc_state->adc_draw) {
	/* Was on, turn off */
	adc_state->adc_draw = 0;
    } else {
	/* Was off, turn on */
	adc_state->adc_draw = 1;
    }

    adc_set_scroll();
    return TCL_OK;
}


int
bv_reset() {
    /* Reset view such that all solids can be seen */
    size_reset();
    setview(0.0, 0.0, 0.0);
    (void)mged_svbase();
    return TCL_OK;
}


int
bv_45_45() {
    setview(270.0+45.0, 0.0, 270.0-45.0);
    return TCL_OK;
}


int
bv_35_25() {
    /* Use Azmuth=35, Elevation=25 in GIFT's backwards space */
    setview(270.0+25.0, 0.0, 270.0-35.0);
    return TCL_OK;
}


/* returns 0 if error, !0 if success */
static int
ill_common(void) {
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    int is_empty = 1;

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->gdl_headSolid)) {
	    is_empty = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    if (is_empty) {
	Tcl_AppendResult(INTERP, "no solids in view\n", (char *)NULL);
	return 0;	/* BAD */
    }

    illum_gdlp = gdlp;
    illump = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);/* any valid solid would do */
    illump->s_iflag = UP;
    edobj = 0;		/* sanity */
    edsol = 0;		/* sanity */
    movedir = 0;		/* No edit modes set */
    MAT_IDN(modelchanges);	/* No changes yet */

    return 1;		/* OK */
}


int
be_o_illuminate()
{
    if (not_state(ST_VIEW, "Matrix Illuminate"))
	return TCL_ERROR;

    if (ill_common()) {
	(void)chg_state(ST_VIEW, ST_O_PICK, "Matrix Illuminate");
    }
    /* reset accumulation local scale factors */
    acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;

    /* reset accumulation global scale factors */
    acc_sc_obj = 1.0;
    return TCL_OK;
}


int
be_s_illuminate()
{
    if (not_state(ST_VIEW, "Primitive Illuminate"))
	return TCL_ERROR;

    if (ill_common()) {
	(void)chg_state(ST_VIEW, ST_S_PICK, "Primitive Illuminate");
    }
    return TCL_OK;
}


int
be_o_scale()
{
    if (not_state(ST_O_EDIT, "Matrix Scale"))
	return TCL_ERROR;

    edobj = BE_O_SCALE;
    movedir = SARROW;
    update_views = 1;
    set_e_axes_pos(1);

    edit_absolute_scale = acc_sc_obj - 1.0;
    if (edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_xscale()
{
    if (not_state(ST_O_EDIT, "Matrix Local X Scale"))
	return TCL_ERROR;

    edobj = BE_O_XSCALE;
    movedir = SARROW;
    update_views = 1;
    set_e_axes_pos(1);

    edit_absolute_scale = acc_sc[0] - 1.0;
    if (edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_yscale()
{
    if (not_state(ST_O_EDIT, "Matrix Local Y Scale"))
	return TCL_ERROR;

    edobj = BE_O_YSCALE;
    movedir = SARROW;
    update_views = 1;
    set_e_axes_pos(1);

    edit_absolute_scale = acc_sc[1] - 1.0;
    if (edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_zscale()
{
    if (not_state(ST_O_EDIT, "Matrix Local Z Scale"))
	return TCL_ERROR;

    edobj = BE_O_ZSCALE;
    movedir = SARROW;
    update_views = 1;
    set_e_axes_pos(1);

    edit_absolute_scale = acc_sc[2] - 1.0;
    if (edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_x()
{
    if (not_state(ST_O_EDIT, "Matrix X Motion"))
	return TCL_ERROR;

    edobj = BE_O_X;
    movedir = RARROW;
    update_views = 1;
    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_o_y()
{
    if (not_state(ST_O_EDIT, "Matrix Y Motion"))
	return TCL_ERROR;

    edobj = BE_O_Y;
    movedir = UARROW;
    update_views = 1;
    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_o_xy()
{
    if (not_state(ST_O_EDIT, "Matrix XY Motion"))
	return TCL_ERROR;

    edobj = BE_O_XY;
    movedir = UARROW | RARROW;
    update_views = 1;
    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_o_rotate()
{
    if (not_state(ST_O_EDIT, "Matrix Rotation"))
	return TCL_ERROR;

    edobj = BE_O_ROTATE;
    movedir = ROTARROW;
    update_views = 1;
    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_accept()
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct dm_list *dmlp;

    if (STATE == ST_S_EDIT) {
	/* Accept a solid edit */
	edsol = 0;

	sedit_accept();		/* zeros "edsol" var */

	mmenu_set_all(MENU_L1, MENU_NULL);
	mmenu_set_all(MENU_L2, MENU_NULL);

	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid)
		sp->s_iflag = DOWN;

	    gdlp = next_gdlp;
	}

	illum_gdlp = GED_DISPLAY_LIST_NULL;
	illump = SOLID_NULL;
	color_soltab();
	(void)chg_state(ST_S_EDIT, ST_VIEW, "Edit Accept");
    }  else if (STATE == ST_O_EDIT) {
	/* Accept an object edit */
	edobj = 0;
	movedir = 0;	/* No edit modes set */

	oedit_accept();

	mmenu_set_all(MENU_L2, MENU_NULL);

	illum_gdlp = GED_DISPLAY_LIST_NULL;
	illump = SOLID_NULL;
	color_soltab();
	(void)chg_state(ST_O_EDIT, ST_VIEW, "Edit Accept");
    } else {
	if (not_state(ST_S_EDIT, "Edit Accept"))
	    return TCL_ERROR;
	return TCL_OK;
    }

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	if (dmlp->dml_mged_variables->mv_transform == 'e')
	    dmlp->dml_mged_variables->mv_transform = 'v';

    {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "end_edit_callback");
	(void)Tcl_Eval(INTERP, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
    return TCL_OK;
}


int
be_reject()
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct dm_list *dmlp;

    update_views = 1;

    /* Reject edit */

    switch (STATE) {
	default:
	    state_err("Edit Reject");
	    return TCL_ERROR;

	case ST_S_EDIT:
	    /* Reject a solid edit */
	    mmenu_set_all(MENU_L1, MENU_NULL);
	    mmenu_set_all(MENU_L2, MENU_NULL);

	    sedit_reject();
	    break;

	case ST_O_EDIT:
	    mmenu_set_all(MENU_L2, MENU_NULL);

	    oedit_reject();
	    break;
	case ST_O_PICK:
	    break;
	case ST_S_PICK:
	    break;
	case ST_O_PATH:
	    break;
    }

    menu_state->ms_flag = 0;
    movedir = 0;
    edsol = 0;
    edobj = 0;
    es_edflag = -1;
    illum_gdlp = GED_DISPLAY_LIST_NULL;
    illump = SOLID_NULL;		/* None selected */

    /* Clear illumination flags */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid)
	    sp->s_iflag = DOWN;

	gdlp = next_gdlp;
    }

    color_soltab();
    (void)chg_state(STATE, ST_VIEW, "Edit Reject");

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	if (dmlp->dml_mged_variables->mv_transform == 'e')
	    dmlp->dml_mged_variables->mv_transform = 'v';

    {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "end_edit_callback");
	(void)Tcl_Eval(INTERP, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
    return TCL_OK;
}


int
be_s_edit()
{
    /* solid editing */
    if (not_state(ST_S_EDIT, "Primitive Edit (Menu)"))
	return TCL_ERROR;

    edsol = BE_S_EDIT;
    sedit_menu();		/* Install appropriate menu */
    return TCL_OK;
}


int
be_s_rotate()
{
    /* rotate solid */
    if (not_state(ST_S_EDIT, "Primitive Rotate"))
	return TCL_ERROR;

    es_edflag = SROT;
    edsol = BE_S_ROTATE;
    mmenu_set(MENU_L1, MENU_NULL);

    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_s_trans()
{
    /* translate solid */
    if (not_state(ST_S_EDIT, "Primitive Translate"))
	return TCL_ERROR;

    edsol = BE_S_TRANS;
    es_edflag = STRANS;
    movedir = UARROW | RARROW;
    mmenu_set(MENU_L1, MENU_NULL);

    set_e_axes_pos(1);
    return TCL_OK;
}


int
be_s_scale()
{
    /* scale solid */
    if (not_state(ST_S_EDIT, "Primitive Scale"))
	return TCL_ERROR;

    edsol = BE_S_SCALE;
    es_edflag = SSCALE;
    mmenu_set(MENU_L1, MENU_NULL);
    acc_sc_sol = 1.0;

    set_e_axes_pos(1);
    return TCL_OK;
}


/*
 * N O T _ S T A T E
 *
 * Returns 0 if current state is as desired,
 * Returns !0 and prints error message if state mismatch.
 */
int
not_state(int desired, char *str)
{
    if (STATE != desired) {
	Tcl_AppendResult(INTERP, "Unable to do <", str, "> from ", state_str[STATE], " state.\n", (char *)NULL);
	Tcl_AppendResult(INTERP, "Expecting ", state_str[desired], " state.\n", (char *)NULL);
	return -1;
    }

    return 0;
}


/*
 * Based upon new state, possibly do extra stuff, including enabling
 * continuous tablet tracking, object highlighting.
 */
static void
stateChange(int UNUSED(oldstate), int newstate)
{
    switch (newstate) {
	case ST_VIEW:
	    /* constant tracking OFF */
	    doMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    doMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    doMotion = 0;
	    break;
	default:
	    bu_log("statechange: unknown state %s\n", state_str[newstate]);
	    break;
    }

    ++update_views;
}


/*
 * C H G _ S T A T E
 *
 * Returns 0 if state change is OK,
 * Returns !0 and prints error message if error.
 */
int
chg_state(int from, int to, char *str)
{
    struct dm_list *p;
    struct dm_list *save_dm_list;
    struct bu_vls vls;

    if (STATE != from) {
	bu_log("Unable to do <%s> going from %s to %s state.\n", str, state_str[from], state_str[to]);
	return 1;	/* BAD */
    }

    STATE = to;

    stateChange(from, to);

    save_dm_list = curr_dm_list;
    FOR_ALL_DISPLAYS(p, &head_dm_list.l) {
	curr_dm_list = p;

	new_mats();
    }

    curr_dm_list = save_dm_list;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%s(state)", MGED_DISPLAY_VAR);
    Tcl_SetVar(INTERP, bu_vls_addr(&vls), state_str[STATE], TCL_GLOBAL_ONLY);
    bu_vls_free(&vls);

    return 0;		/* GOOD */
}


void
state_err(char *str)
{
    Tcl_AppendResult(INTERP, "Unable to do <", str, "> from ", state_str[STATE],
		     " state.\n", (char *)NULL);
}


/*
 * B T N _ I T E M _ H I T
 *
 * Called when a menu item is hit
 */
void
btn_item_hit(int arg, int menu, int UNUSED(item))
{
    button(arg);
    if (menu == MENU_GEN &&
	(arg != BE_O_ILLUMINATE && arg != BE_S_ILLUMINATE))
	menu_state->ms_flag = 0;
}


/*
 * B T N _ H E A D _ M E N U
 *
 * Called to handle hits on menu heads.
 * Also called from main() with arg 0 in init.
 */
void
btn_head_menu(int i, int UNUSED(menu), int UNUSED(item)) {
    switch (i) {
	case 0:
	    mmenu_set(MENU_GEN, first_menu);
	    break;
	case 1:
	    mmenu_set(MENU_GEN, second_menu);
	    break;
	case 2:
	    /* nothing happens */
	    break;
	default:
	    {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "btn_head_menu(%d): bad arg\n", i);
		Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
    }
}


void
chg_l2menu(int i) {
    switch (i) {
	case ST_S_EDIT:
	    mmenu_set_all(MENU_L2, sed_menu);
	    break;
	case ST_S_NO_EDIT:
	    mmenu_set_all(MENU_L2, MENU_NULL);
	    break;
	case ST_O_EDIT:
	    mmenu_set_all(MENU_L2, oed_menu);
	    break;
	default:
	    {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "chg_l2menu(%d): bad arg\n", i);
		Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
    }
}


/* TODO: below are functions not yet migrated to libged, still
 * referenced by mged's setup command table.  migrate to libged.
 */


int
f_be_accept(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_accept();
}


int
f_be_o_illuminate(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_illuminate();
}


int
f_be_o_rotate(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_rotate();
}


int
f_be_o_scale(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_scale();
}


int
f_be_o_x(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_x();
}


int
f_be_o_xscale(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_xscale();
}


int
f_be_o_xy(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_xy();
}


int
f_be_o_y(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_y();
}


int
f_be_o_yscale(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_yscale();
}


int
f_be_o_zscale(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_o_zscale();
}


int
f_be_reject(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_reject();
}


int
f_be_s_edit(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_edit();
}


int
f_be_s_illuminate(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_illuminate();
}


int
f_be_s_rotate(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_rotate();
}


int
f_be_s_scale(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_scale();
}


int
f_be_s_trans(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_trans();
}


int
f_bv_35_25(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_trans();
}


int
f_bv_45_45(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return be_s_trans();
}


int
f_bv_bottom(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_bottom();
}


int
f_bv_front(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_front();
}


int
f_bv_left(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_left();
}


int
f_bv_rate_toggle(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_rate_toggle();
}


int
f_bv_rear(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_rear();
}


int
f_bv_reset(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_reset();
}


int
f_bv_right(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_right();
}


int
f_bv_top(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_top();
}


int
f_bv_vrestore(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_vrestore();
}


int
f_bv_vsave(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_vsave();
}


int
f_bv_zoomin(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_zoomin();
}


int
f_bv_zoomout(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    return bv_zoomout();
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
