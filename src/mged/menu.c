/*                          M E N U . C
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

static void
cline_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_edflag = arg;
    sedit(s);
}

struct menu_item cline_menu[] = {
    { "CLINE MENU",		NULL, 0 },
    { "Set H",		cline_ed, ECMD_CLINE_SCALE_H },
    { "Move End H",		cline_ed, ECMD_CLINE_MOVE_H },
    { "Set R",		cline_ed, ECMD_CLINE_SCALE_R },
    { "Set plate thickness", cline_ed, ECMD_CLINE_SCALE_T },
    { "", NULL, 0 }
};

/*ARGSUSED*/
static void
extr_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_edflag = arg;
    sedit(s);
}
struct menu_item extr_menu[] = {
    { "EXTRUSION MENU",	NULL, 0 },
    { "Set H",		extr_ed, ECMD_EXTR_SCALE_H },
    { "Move End H",		extr_ed, ECMD_EXTR_MOV_H },
    { "Rotate H",		extr_ed, ECMD_EXTR_ROT_H },
    { "Referenced Sketch",	extr_ed, ECMD_EXTR_SKT_NAME },
    { "", NULL, 0 }
};


static void
tgc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;
    if (arg == MENU_TGC_ROT_H)
	es_edflag = ECMD_TGC_ROT_H;
    if (arg == MENU_TGC_ROT_AB)
	es_edflag = ECMD_TGC_ROT_AB;
    if (arg == MENU_TGC_MV_H)
	es_edflag = ECMD_TGC_MV_H;
    if (arg == MENU_TGC_MV_HH)
	es_edflag = ECMD_TGC_MV_HH;

    set_e_axes_pos(s, 1);
}

struct menu_item tgc_menu[] = {
    { "TGC MENU", NULL, 0 },
    { "Set H",	tgc_ed, MENU_TGC_SCALE_H },
    { "Set H (move V)", tgc_ed, MENU_TGC_SCALE_H_V },
    { "Set H (adj C,D)",	tgc_ed, MENU_TGC_SCALE_H_CD },
    { "Set H (move V, adj A,B)", tgc_ed, MENU_TGC_SCALE_H_V_AB },
    { "Set A",	tgc_ed, MENU_TGC_SCALE_A },
    { "Set B",	tgc_ed, MENU_TGC_SCALE_B },
    { "Set C",	tgc_ed, MENU_TGC_SCALE_C },
    { "Set D",	tgc_ed, MENU_TGC_SCALE_D },
    { "Set A,B",	tgc_ed, MENU_TGC_SCALE_AB },
    { "Set C,D",	tgc_ed, MENU_TGC_SCALE_CD },
    { "Set A,B,C,D", tgc_ed, MENU_TGC_SCALE_ABCD },
    { "Rotate H",	tgc_ed, MENU_TGC_ROT_H },
    { "Rotate AxB",	tgc_ed, MENU_TGC_ROT_AB },
    { "Move End H(rt)", tgc_ed, MENU_TGC_MV_H },
    { "Move End H", tgc_ed, MENU_TGC_MV_HH },
    { "", NULL, 0 }
};

static void
tor_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}


struct menu_item tor_menu[] = {
    { "TORUS MENU", NULL, 0 },
    { "Set Radius 1", tor_ed, MENU_TOR_R1 },
    { "Set Radius 2", tor_ed, MENU_TOR_R2 },
    { "", NULL, 0 }
};

static void
eto_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    if (arg == MENU_ETO_ROT_C)
	es_edflag = ECMD_ETO_ROT_C;
    else
	es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item eto_menu[] = {
    { "ELL-TORUS MENU", NULL, 0 },
    { "Set r", eto_ed, MENU_ETO_R },
    { "Set D", eto_ed, MENU_ETO_RD },
    { "Set C", eto_ed, MENU_ETO_SCALE_C },
    { "Rotate C", eto_ed, MENU_ETO_ROT_C },
    { "", NULL, 0 }
};

static void
ell_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item ell_menu[] = {
    { "ELLIPSOID MENU", NULL, 0 },
    { "Set A", ell_ed, MENU_ELL_SCALE_A },
    { "Set B", ell_ed, MENU_ELL_SCALE_B },
    { "Set C", ell_ed, MENU_ELL_SCALE_C },
    { "Set A,B,C", ell_ed, MENU_ELL_SCALE_ABC },
    { "", NULL, 0 }
};

/*ARGSUSED*/
static void
spline_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    /* XXX Why wasn't this done by setting es_edflag = ECMD_SPLINE_VPICK? */
    if (arg < 0) {
	/* Enter picking state */
	chg_state(s, ST_S_EDIT, ST_S_VPICK, "Vertex Pick");
	return;
    }
    /* For example, this will set es_edflag = ECMD_VTRANS */
    es_edflag = arg;
    sedit(s);

    set_e_axes_pos(s, 1);
}
struct menu_item spline_menu[] = {
    { "SPLINE MENU", NULL, 0 },
    { "Pick Vertex", spline_ed, -1 },
    { "Move Vertex", spline_ed, ECMD_VTRANS },
    { "", NULL, 0 }
};

static void
part_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item part_menu[] = {
    { "Particle MENU", NULL, 0 },
    { "Set H", part_ed, MENU_PART_H },
    { "Set v", part_ed, MENU_PART_v },
    { "Set h", part_ed, MENU_PART_h },
    { "", NULL, 0 }
};

static void
rpc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item rpc_menu[] = {
    { "RPC MENU", NULL, 0 },
    { "Set B", rpc_ed, MENU_RPC_B },
    { "Set H", rpc_ed, MENU_RPC_H },
    { "Set r", rpc_ed, MENU_RPC_R },
    { "", NULL, 0 }
};

static void
rhc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item rhc_menu[] = {
    { "RHC MENU", NULL, 0 },
    { "Set B", rhc_ed, MENU_RHC_B },
    { "Set H", rhc_ed, MENU_RHC_H },
    { "Set r", rhc_ed, MENU_RHC_R },
    { "Set c", rhc_ed, MENU_RHC_C },
    { "", NULL, 0 }
};

static void
epa_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item epa_menu[] = {
    { "EPA MENU", NULL, 0 },
    { "Set H", epa_ed, MENU_EPA_H },
    { "Set A", epa_ed, MENU_EPA_R1 },
    { "Set B", epa_ed, MENU_EPA_R2 },
    { "", NULL, 0 }
};

static void
ehy_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item ehy_menu[] = {
    { "EHY MENU", NULL, 0 },
    { "Set H", ehy_ed, MENU_EHY_H },
    { "Set A", ehy_ed, MENU_EHY_R1 },
    { "Set B", ehy_ed, MENU_EHY_R2 },
    { "Set c", ehy_ed, MENU_EHY_C },
    { "", NULL, 0 }
};

static void
hyp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    switch (arg) {
	case MENU_HYP_ROT_H:
	    es_edflag = ECMD_HYP_ROT_H;
	    break;
	default:
	    es_edflag = PSCALE;
	    break;
    }
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item  hyp_menu[] = {
    { "HYP MENU", NULL, 0 },
    { "Set H", hyp_ed, MENU_HYP_H },
    { "Set A", hyp_ed, MENU_HYP_SCALE_A },
    { "Set B", hyp_ed, MENU_HYP_SCALE_B },
    { "Set c", hyp_ed, MENU_HYP_C },
    { "Rotate H", hyp_ed, MENU_HYP_ROT_H },
    { "", NULL, 0 }
};

static void
vol_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_VOL_FNAME:
	    es_edflag = ECMD_VOL_FNAME;
	    break;
	case MENU_VOL_FSIZE:
	    es_edflag = ECMD_VOL_FSIZE;
	    break;
	case MENU_VOL_CSIZE:
	    es_edflag = ECMD_VOL_CSIZE;
	    break;
	case MENU_VOL_THRESH_LO:
	    es_edflag = ECMD_VOL_THRESH_LO;
	    break;
	case MENU_VOL_THRESH_HI:
	    es_edflag = ECMD_VOL_THRESH_HI;
	    break;
    }

    sedit(s);
    set_e_axes_pos(s, 1);
}

struct menu_item vol_menu[] = {
    {"VOL MENU", NULL, 0 },
    {"File Name", vol_ed, MENU_VOL_FNAME },
    {"File Size (X Y Z)", vol_ed, MENU_VOL_FSIZE },
    {"Voxel Size (X Y Z)", vol_ed, MENU_VOL_CSIZE },
    {"Threshold (low)", vol_ed, MENU_VOL_THRESH_LO },
    {"Threshold (hi)", vol_ed, MENU_VOL_THRESH_HI },
    { "", NULL, 0 }
};

static void
ebm_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_EBM_FNAME:
	    es_edflag = ECMD_EBM_FNAME;
	    break;
	case MENU_EBM_FSIZE:
	    es_edflag = ECMD_EBM_FSIZE;
	    break;
	case MENU_EBM_HEIGHT:
	    es_edflag = ECMD_EBM_HEIGHT;
	    break;
    }

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item ebm_menu[] = {
    {"EBM MENU", NULL, 0 },
    {"File Name", ebm_ed, MENU_EBM_FNAME },
    {"File Size (W N)", ebm_ed, MENU_EBM_FSIZE },
    {"Extrude Depth", ebm_ed, MENU_EBM_HEIGHT },
    { "", NULL, 0 }
};

static void
dsp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_DSP_FNAME:
	    es_edflag = ECMD_DSP_FNAME;
	    break;
	case MENU_DSP_FSIZE:
	    es_edflag = ECMD_DSP_FSIZE;
	    break;
	case MENU_DSP_SCALE_X:
	    es_edflag = ECMD_DSP_SCALE_X;
	    break;
	case MENU_DSP_SCALE_Y:
	    es_edflag = ECMD_DSP_SCALE_Y;
	    break;
	case MENU_DSP_SCALE_ALT:
	    es_edflag = ECMD_DSP_SCALE_ALT;
	    break;
    }
    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item dsp_menu[] = {
    {"DSP MENU", NULL, 0 },
    {"Name", dsp_ed, MENU_DSP_FNAME },
    {"Set X", dsp_ed, MENU_DSP_SCALE_X },
    {"Set Y", dsp_ed, MENU_DSP_SCALE_Y },
    {"Set ALT", dsp_ed, MENU_DSP_SCALE_ALT },
    { "", NULL, 0 }
};

static void
bot_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = arg;

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item bot_menu[] = {
    { "BOT MENU", NULL, 0 },
    { "Pick Vertex", bot_ed, ECMD_BOT_PICKV },
    { "Pick Edge", bot_ed, ECMD_BOT_PICKE },
    { "Pick Triangle", bot_ed, ECMD_BOT_PICKT },
    { "Move Vertex", bot_ed, ECMD_BOT_MOVEV },
    { "Move Edge", bot_ed, ECMD_BOT_MOVEE },
    { "Move Triangle", bot_ed, ECMD_BOT_MOVET },
    { "Delete Triangle", bot_ed, ECMD_BOT_FDEL },
    { "Select Mode", bot_ed, ECMD_BOT_MODE },
    { "Select Orientation", bot_ed, ECMD_BOT_ORIENT },
    { "Set flags", bot_ed, ECMD_BOT_FLAGS },
    { "Set Face Thickness", bot_ed, ECMD_BOT_THICK },
    { "Set Face Mode", bot_ed, ECMD_BOT_FMODE },
    { "", NULL, 0 }
};

static void
superell_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b)) {
    es_menu = arg;
    es_edflag = PSCALE;
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item superell_menu[] = {
    { "SUPERELLIPSOID MENU", NULL, 0 },
    { "Set A", superell_ed, MENU_SUPERELL_SCALE_A },
    { "Set B", superell_ed, MENU_SUPERELL_SCALE_B },
    { "Set C", superell_ed, MENU_SUPERELL_SCALE_C },
    { "Set A,B,C", superell_ed, MENU_SUPERELL_SCALE_ABC },
    { "", NULL, 0 }
};

static void
metaball_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    struct wdb_metaball_pnt *next, *prev;

    if (s->dbip == DBI_NULL)
	return;

    switch (arg) {
	case MENU_METABALL_SET_THRESHOLD:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_SET_METHOD:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_PT_SET_GOO:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_SELECT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_PICK;
	    break;
	case MENU_METABALL_NEXT_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		return;
	    }
	    next = BU_LIST_NEXT(wdb_metaball_pnt, &es_metaball_pnt->l);
	    if (next->l.magic == BU_LIST_HEAD_MAGIC) {
		Tcl_AppendResult(s->interp, "Current point is the last\n", (char *)NULL);
		return;
	    }
	    es_metaball_pnt = next;
	    rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
	    es_menu = arg;
	    es_edflag = IDLE;
	    sedit(s);
	    break;
	case MENU_METABALL_PREV_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		return;
	    }
	    prev = BU_LIST_PREV(wdb_metaball_pnt, &es_metaball_pnt->l);
	    if (prev->l.magic == BU_LIST_HEAD_MAGIC) {
		Tcl_AppendResult(s->interp, "Current point is the first\n", (char *)NULL);
		return;
	    }
	    es_metaball_pnt = prev;
	    rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
	    es_menu = arg;
	    es_edflag = IDLE;
	    sedit(s);
	    break;
	case MENU_METABALL_MOV_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		es_edflag = IDLE;
		return;
	    }
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_MOV;
	    sedit(s);
	    break;
	case MENU_METABALL_PT_FLDSTR:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		es_edflag = IDLE;
		return;
	    }
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_DEL_PT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_DEL;
	    sedit(s);
	    break;
	case MENU_METABALL_ADD_PT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_ADD;
	    break;
    }
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item metaball_menu[] = {
    { "METABALL MENU", NULL, 0 },
    { "Set Threshold", metaball_ed, MENU_METABALL_SET_THRESHOLD },
    { "Set Render Method", metaball_ed, MENU_METABALL_SET_METHOD },
    { "Select Point", metaball_ed, MENU_METABALL_SELECT },
    { "Next Point", metaball_ed, MENU_METABALL_NEXT_PT },
    { "Previous Point", metaball_ed, MENU_METABALL_PREV_PT },
    { "Move Point", metaball_ed, MENU_METABALL_MOV_PT },
    { "Scale Point fldstr", metaball_ed, MENU_METABALL_PT_FLDSTR },
    { "Scale Point \"goo\" value", metaball_ed, MENU_METABALL_PT_SET_GOO },
    { "Delete Point", metaball_ed, MENU_METABALL_DEL_PT },
    { "Add Point", metaball_ed, MENU_METABALL_ADD_PT },
    { "", NULL, 0 }
};


struct menu_item *which_menu[] = {
    point4_menu,
    edge5_menu,
    edge6_menu,
    edge7_menu,
    edge8_menu,
    mv4_menu,
    mv5_menu,
    mv6_menu,
    mv7_menu,
    mv8_menu,
    rot4_menu,
    rot5_menu,
    rot6_menu,
    rot7_menu,
    rot8_menu
};


extern struct menu_item second_menu[], sed_menu[];

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


void
mmenu_set(struct mged_state *s, int index, struct menu_item *value)
{
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
		(*(mptr->menu_func))(s, mptr->menu_arg, menu, item);

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
