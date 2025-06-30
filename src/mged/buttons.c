/*                       B U T T O N S . C
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
/** @file mged/buttons.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"

#include "./mged.h"
#include "./sedit.h"
#include "./menu.h"

/* external sp_hook function */
extern void set_scroll_private(const struct bu_structparse *, const char *, void *, const char *, void *);	/* defined in set.c */

extern void set_e_axes_pos(struct mged_state *s, int both);
extern int mged_zoom(struct mged_state *s, double val);
extern void adc_set_scroll(struct mged_state *s);	/* defined in adc.c */

/* forward declarations for the buttons table */
int be_accept(ClientData, Tcl_Interp *, int, char **);
int be_o_illuminate(ClientData, Tcl_Interp *, int, char **);
int be_o_rotate(ClientData, Tcl_Interp *, int, char **);
int be_o_scale(ClientData, Tcl_Interp *, int, char **);
int be_o_x(ClientData, Tcl_Interp *, int, char **);
int be_o_xscale(ClientData, Tcl_Interp *, int, char **);
int be_o_xy(ClientData, Tcl_Interp *, int, char **);
int be_o_y(ClientData, Tcl_Interp *, int, char **);
int be_o_yscale(ClientData, Tcl_Interp *, int, char **);
int be_o_zscale(ClientData, Tcl_Interp *, int, char **);
int be_reject(ClientData, Tcl_Interp *, int, char **);
int be_s_edit(ClientData, Tcl_Interp *, int, char **);
int be_s_illuminate(ClientData, Tcl_Interp *, int, char **);
int be_s_rotate(ClientData, Tcl_Interp *, int, char **);
int be_s_scale(ClientData, Tcl_Interp *, int, char **);
int be_s_trans(ClientData, Tcl_Interp *, int, char **);
int bv_35_25(ClientData, Tcl_Interp *, int, char **);
int bv_45_45(ClientData, Tcl_Interp *, int, char **);
int bv_adcursor(ClientData, Tcl_Interp *, int, char **);
int bv_bottom(ClientData, Tcl_Interp *, int, char **);
int bv_front(ClientData, Tcl_Interp *, int, char **);
int bv_left(ClientData, Tcl_Interp *, int, char **);
int bv_rate_toggle(ClientData, Tcl_Interp *, int, char **);
int bv_rear(ClientData, Tcl_Interp *, int, char **);
int bv_reset(ClientData, Tcl_Interp *, int, char **);
int bv_right(ClientData, Tcl_Interp *, int, char **);
int bv_top(ClientData, Tcl_Interp *, int, char **);
int bv_vrestore(ClientData, Tcl_Interp *, int, char **);
int bv_vsave(ClientData, Tcl_Interp *, int, char **);
int bv_zoomin(ClientData, Tcl_Interp *, int, char **);
int bv_zoomout(ClientData, Tcl_Interp *, int, char **);


/*
 * This flag indicates that Primitive editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (e.g., BE_S_SCALE).
 */
static int edsol;

/* This flag indicates that Matrix editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (e.g., BE_O_XY).
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
    int (*bu_func)(ClientData, Tcl_Interp *, int, char **);	/* function to call */
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
static int vsaved = 0;	/* set if view saved */

extern void mged_color_soltab(struct mged_state *s);
extern void sl_halt_scroll(struct mged_state *s, int, int, int);	/* in scroll.c */
extern void sl_toggle_scroll(struct mged_state *s, int, int, int);

void btn_head_menu(struct mged_state *s, int i, int menu, int item);
void btn_item_hit(struct mged_state *s, int arg, int menu, int item);

static struct menu_item first_menu[] = {
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


void
button(struct mged_state *s, int bnum)
{
    struct buttons *bp;

    if (edsol && edobj)
	bu_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);

    /* Process the button function requested. */
    for (bp = button_table; bp->bu_code >= 0; bp++) {
	if (bnum != bp->bu_code)
	    continue;

	struct cmdtab bcmdtab = {MGED_CMD_MAGIC, bp->bu_name, NULL, NULL, s};
	bp->bu_func((ClientData)&bcmdtab, s->interp, 0, NULL);
	return;
    }

    bu_log("button(%d):  Not a defined operation\n", bnum);
}


/*
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
	char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int i;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 2) {
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
	    bu_vls_printf(&vls, "WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	}

	if (BU_STR_EQUAL(str, "help")) {

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

	for (menu = 0, m = menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++, menu++) {
	    if (*m == NULL) continue;
	    for (item = 0, mptr = *m;
		 mptr->menu_string[0] != '\0';
		 mptr++, item++) {
		if (!BU_STR_EQUAL(str, mptr->menu_string))
		    continue;

		menu_state->ms_cur_item = item;
		menu_state->ms_cur_menu = menu;
		menu_state->ms_flag = 1;
		/* It's up to the menu_func to set menu_state->ms_flag = 0
		 * if no arrow is desired */
		if (mptr->menu_func != NULL)
		    (*(mptr->menu_func))(s, mptr->menu_arg, menu, item);

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
 * For a given GED button number, return the "press" ID string.
 * Useful for displays with programmable button labels, etc.
 */
char *
label_button(struct mged_state *s, int bnum)
{
    struct buttons *bp;

    /* Process the button function requested. */
    for (bp = button_table; bp->bu_code >= 0; bp++) {
	if (bnum != bp->bu_code)
	    continue;
	return bp->bu_name;
    }

    {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "label_button(%d):  Not a defined operation\n", bnum);
	Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    return "";
}


int
bv_zoomin(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    (void)mged_zoom(s, 2.0);
    return TCL_OK;
}


int
bv_zoomout(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    (void)mged_zoom(s, 0.5);
    return TCL_OK;
}


int
bv_rate_toggle(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    mged_variables->mv_rateknobs = !mged_variables->mv_rateknobs;

    {
	/* need dummy values for func signature--they are unused in the func */
	const struct bu_structparse *sdp = 0;
	const char name[] = "name";
	void *base = 0;
	const char value[] = "value";
	set_scroll_private(sdp, name, base, value, s);
    }

    return TCL_OK;
}


int
bv_top(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Top view */
    setview(s, 0.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_bottom(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Bottom view */
    setview(s, 180.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_right(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Right view */
    setview(s, 270.0, 0.0, 0.0);
    return TCL_OK;
}


int
bv_left(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Left view */
    setview(s, 270.0, 0.0, 180.0);
    return TCL_OK;
}


int
bv_front(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Front view */
    setview(s, 270.0, 0.0, 270.0);
    return TCL_OK;
}


int
bv_rear(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Rear view */
    setview(s, 270.0, 0.0, 90.0);
    return TCL_OK;
}


int
bv_vrestore(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
     /* restore to saved view */
    if (vsaved) {
	view_state->vs_gvp->gv_scale = sav_vscale;
	MAT_COPY(view_state->vs_gvp->gv_rotation, sav_viewrot);
	MAT_COPY(view_state->vs_gvp->gv_center, sav_toviewcenter);
	new_mats(s);

	(void)mged_svbase(s);
    }

    return TCL_OK;
}


int
bv_vsave(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
     /* save current view */
    sav_vscale = view_state->vs_gvp->gv_scale;
    MAT_COPY(sav_viewrot, view_state->vs_gvp->gv_rotation);
    MAT_COPY(sav_toviewcenter, view_state->vs_gvp->gv_center);
    vsaved = 1;
    return TCL_OK;
}


/*
 * Toggle state of angle/distance cursor.
 * "press adc"
 * This command conflicts with existing "adc" command,
 * can't be bound as "adc", only as "press adc".
 */
int
bv_adcursor(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (adc_state->adc_draw) {
	/* Was on, turn off */
	adc_state->adc_draw = 0;
    } else {
	/* Was off, turn on */
	adc_state->adc_draw = 1;
    }

    adc_set_scroll(s);
    return TCL_OK;
}


int
bv_reset(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[])) {
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Reset view such that all solids can be seen */
    size_reset(s);
    setview(s, 0.0, 0.0, 0.0);
    (void)mged_svbase(s);
    return TCL_OK;
}


int
bv_45_45(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[])) {
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    setview(s, 270.0+45.0, 0.0, 270.0-45.0);
    return TCL_OK;
}


int
bv_35_25(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[])) {
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    /* Use Azimuth=35, Elevation=25 in GIFT's backwards space */
    setview(s, 270.0+25.0, 0.0, 270.0-35.0);
    return TCL_OK;
}


/* returns 0 if error, !0 if success */
static int
ill_common(struct mged_state *s) {
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int is_empty = 1;

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
	    is_empty = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    if (is_empty) {
	Tcl_AppendResult(s->interp, "no solids in view\n", (char *)NULL);
	return 0;	/* BAD */
    }

    illum_gdlp = gdlp;
    illump = BU_LIST_NEXT(bv_scene_obj, &gdlp->dl_head_scene_obj);/* any valid solid would do */
    illump->s_iflag = UP;
    edobj = 0;		/* sanity */
    edsol = 0;		/* sanity */
    movedir = 0;		/* No edit modes set */
    MAT_IDN(modelchanges);	/* No changes yet */

    return 1;		/* OK */
}


int
be_o_illuminate(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_VIEW, "Matrix Illuminate"))
	return TCL_ERROR;

    if (ill_common(s)) {
	(void)chg_state(s, ST_VIEW, ST_O_PICK, "Matrix Illuminate");
    }
    /* reset accumulation local scale factors */
    acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;

    /* reset accumulation global scale factors */
    acc_sc_obj = 1.0;
    return TCL_OK;
}


int
be_s_illuminate(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_VIEW, "Primitive Illuminate"))
	return TCL_ERROR;

    if (ill_common(s)) {
	(void)chg_state(s, ST_VIEW, ST_S_PICK, "Primitive Illuminate");
    }
    return TCL_OK;
}


int
be_o_scale(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Scale"))
	return TCL_ERROR;

    edobj = BE_O_SCALE;
    movedir = SARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);

    s->edit_state.edit_absolute_scale = acc_sc_obj - 1.0;
    if (s->edit_state.edit_absolute_scale > 0.0)
	s->edit_state.edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_xscale(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Local X Scale"))
	return TCL_ERROR;

    edobj = BE_O_XSCALE;
    movedir = SARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);

    s->edit_state.edit_absolute_scale = acc_sc[0] - 1.0;
    if (s->edit_state.edit_absolute_scale > 0.0)
	s->edit_state.edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_yscale(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Local Y Scale"))
	return TCL_ERROR;

    edobj = BE_O_YSCALE;
    movedir = SARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);

    s->edit_state.edit_absolute_scale = acc_sc[1] - 1.0;
    if (s->edit_state.edit_absolute_scale > 0.0)
	s->edit_state.edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_zscale(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Local Z Scale"))
	return TCL_ERROR;

    edobj = BE_O_ZSCALE;
    movedir = SARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);

    s->edit_state.edit_absolute_scale = acc_sc[2] - 1.0;
    if (s->edit_state.edit_absolute_scale > 0.0)
	s->edit_state.edit_absolute_scale /= 3.0;
    return TCL_OK;
}


int
be_o_x(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix X Motion"))
	return TCL_ERROR;

    edobj = BE_O_X;
    movedir = RARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_o_y(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Y Motion"))
	return TCL_ERROR;

    edobj = BE_O_Y;
    movedir = UARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_o_xy(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix XY Motion"))
	return TCL_ERROR;

    edobj = BE_O_XY;
    movedir = UARROW | RARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_o_rotate(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (not_state(s, ST_O_EDIT, "Matrix Rotation"))
	return TCL_ERROR;

    edobj = BE_O_ROTATE;
    movedir = ROTARROW;
    update_views = 1;
    dm_set_dirty(DMP, 1);
    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_accept(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (GEOM_EDIT_STATE == ST_S_EDIT) {
	/* Accept a solid edit */
	edsol = 0;

	sedit_accept(s);		/* zeros "edsol" var */

	mmenu_set_all(s, MENU_L1, NULL);
	mmenu_set_all(s, MENU_L2, NULL);

	dl_set_iflag((struct bu_list *)ged_dl(s->gedp), DOWN);

	illum_gdlp = GED_DISPLAY_LIST_NULL;
	illump = NULL;
	mged_color_soltab(s);
	(void)chg_state(s, ST_S_EDIT, ST_VIEW, "Edit Accept");
    }  else if (GEOM_EDIT_STATE == ST_O_EDIT) {
	/* Accept an object edit */
	edobj = 0;
	movedir = 0;	/* No edit modes set */

	oedit_accept(s);

	mmenu_set_all(s, MENU_L2, NULL);

	illum_gdlp = GED_DISPLAY_LIST_NULL;
	illump = NULL;
	mged_color_soltab(s);
	(void)chg_state(s, ST_O_EDIT, ST_VIEW, "Edit Accept");
    } else {
	if (not_state(s, ST_S_EDIT, "Edit Accept"))
	    return TCL_ERROR;
	return TCL_OK;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	if (m_dmp->dm_mged_variables->mv_transform == 'e')
	    m_dmp->dm_mged_variables->mv_transform = 'v';
    }

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "end_edit_callback");
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
    return TCL_OK;
}


int
be_reject(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    update_views = 1;
    dm_set_dirty(DMP, 1);

    /* Reject edit */

    switch (GEOM_EDIT_STATE) {
	default:
	    state_err(s, "Edit Reject");
	    return TCL_ERROR;

	case ST_S_EDIT:
	    /* Reject a solid edit */
	    mmenu_set_all(s, MENU_L1, NULL);
	    mmenu_set_all(s, MENU_L2, NULL);

	    sedit_reject(s);
	    break;

	case ST_O_EDIT:
	    mmenu_set_all(s, MENU_L2, NULL);

	    oedit_reject(s);
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
    illump = NULL;		/* None selected */

    /* Clear illumination flags */
    dl_set_iflag((struct bu_list *)ged_dl(s->gedp), DOWN);

    mged_color_soltab(s);
    (void)chg_state(s, GEOM_EDIT_STATE, ST_VIEW, "Edit Reject");

    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	if (m_dmp->dm_mged_variables->mv_transform == 'e')
	    m_dmp->dm_mged_variables->mv_transform = 'v';
    }

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "end_edit_callback");
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
    return TCL_OK;
}


int
be_s_edit(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    /* solid editing */
    if (not_state(s, ST_S_EDIT, "Primitive Edit (Menu)"))
	return TCL_ERROR;

    edsol = BE_S_EDIT;
    sedit_menu(s);		/* Install appropriate menu */
    return TCL_OK;
}


int
be_s_rotate(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    /* rotate solid */
    if (not_state(s, ST_S_EDIT, "Primitive Rotate"))
	return TCL_ERROR;

    es_edflag = SROT;
    edsol = BE_S_ROTATE;
    mmenu_set(s, MENU_L1, NULL);

    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_s_trans(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    /* translate solid */
    if (not_state(s, ST_S_EDIT, "Primitive Translate"))
	return TCL_ERROR;

    edsol = BE_S_TRANS;
    es_edflag = STRANS;
    movedir = UARROW | RARROW;
    mmenu_set(s, MENU_L1, NULL);

    set_e_axes_pos(s, 1);
    return TCL_OK;
}


int
be_s_scale(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    /* scale solid */
    if (not_state(s, ST_S_EDIT, "Primitive Scale"))
	return TCL_ERROR;

    edsol = BE_S_SCALE;
    es_edflag = SSCALE;
    mmenu_set(s, MENU_L1, NULL);
    acc_sc_sol = 1.0;

    set_e_axes_pos(s, 1);
    return TCL_OK;
}


/*
 * Returns 0 if current state is as desired,
 * Returns !0 and prints error message if state mismatch.
 */
int
not_state(struct mged_state *s, int desired, char *str)
{
    if (GEOM_EDIT_STATE != desired) {
	Tcl_AppendResult(s->interp, "Unable to do <", str, "> from ", state_str[GEOM_EDIT_STATE], " state.\n", (char *)NULL);
	Tcl_AppendResult(s->interp, "Expecting ", state_str[desired], " state.\n", (char *)NULL);
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
 * Returns 0 if state change is OK,
 * Returns !0 and prints error message if error.
 */
int
chg_state(struct mged_state *s, int from, int to, char *str)
{
    struct mged_dm *save_dm_list;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (GEOM_EDIT_STATE != from) {
	bu_log("Unable to do <%s> going from %s to %s state.\n", str, state_str[from], state_str[to]);
	return 1;	/* BAD */
    }

    GEOM_EDIT_STATE = to;

    stateChange(from, to);

    save_dm_list = s->mged_curr_dm;
    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *p = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	set_curr_dm(s, p);

	new_mats(s);
    }

    set_curr_dm(s, save_dm_list);

    bu_vls_printf(&vls, "%s(state)", MGED_DISPLAY_VAR);
    Tcl_SetVar(s->interp, bu_vls_addr(&vls), state_str[GEOM_EDIT_STATE], TCL_GLOBAL_ONLY);
    bu_vls_free(&vls);

    return 0;		/* GOOD */
}


void
state_err(struct mged_state *s, char *str)
{
    Tcl_AppendResult(s->interp, "Unable to do <", str, "> from ", state_str[GEOM_EDIT_STATE],
		     " state.\n", (char *)NULL);
}


/*
 * Called when a menu item is hit
 */
void
btn_item_hit(struct mged_state *s, int arg, int menu, int UNUSED(item))
{
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
btn_head_menu(struct mged_state *s, int i, int UNUSED(menu), int UNUSED(item)) {
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


/* TODO: below are functions not yet migrated to libged, still
 * referenced by mged's setup command table.  migrate to libged.
 */


int
f_be_accept(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_accept(clientData, interp, argc, argv);
}


int
f_be_o_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_illuminate(clientData, interp, argc, argv);
}


int
f_be_o_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_rotate(clientData, interp, argc, argv);
}


int
f_be_o_scale(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_scale(clientData, interp, argc, argv);
}


int
f_be_o_x(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_x(clientData, interp, argc, argv);
}


int
f_be_o_xscale(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_xscale(clientData, interp, argc, argv);
}


int
f_be_o_xy(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_xy(clientData, interp, argc, argv);
}


int
f_be_o_y(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_y(clientData, interp, argc, argv);
}


int
f_be_o_yscale(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_yscale(clientData, interp, argc, argv);
}


int
f_be_o_zscale(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_o_zscale(clientData, interp, argc, argv);
}


int
f_be_reject(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_reject(clientData, interp, argc, argv);
}


int
f_be_s_edit(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_edit(clientData, interp, argc, argv);
}


int
f_be_s_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_illuminate(clientData, interp, argc, argv);
}


int
f_be_s_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_rotate(clientData, interp, argc, argv);
}


int
f_be_s_scale(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_scale(clientData, interp, argc, argv);
}


int
f_be_s_trans(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_trans(clientData, interp, argc, argv);
}


int
f_bv_35_25(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_trans(clientData, interp, argc, argv);
}


int
f_bv_45_45(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return be_s_trans(clientData, interp, argc, argv);
}


int
f_bv_bottom(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_bottom(clientData, interp, argc, argv);
}


int
f_bv_front(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_front(clientData, interp, argc, argv);
}


int
f_bv_left(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_left(clientData, interp, argc, argv);
}


int
f_bv_rate_toggle(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_rate_toggle(clientData, interp, argc, argv);
}


int
f_bv_rear(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_rear(clientData, interp, argc, argv);
}


int
f_bv_reset(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_reset(clientData, interp, argc, argv);
}


int
f_bv_right(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_right(clientData, interp, argc, argv);
}


int
f_bv_top(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_top(clientData, interp, argc, argv);
}


int
f_bv_vrestore(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_vrestore(clientData, interp, argc, argv);
}


int
f_bv_vsave(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_vsave(clientData, interp, argc, argv);
}


int
f_bv_zoomin(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_zoomin(clientData, interp, argc, argv);
}


int
f_bv_zoomout(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    return bv_zoomout(clientData, interp, argc, argv);
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
