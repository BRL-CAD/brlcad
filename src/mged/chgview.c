/*                       C H G V I E W . C
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
/** @file mged/chgview.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "mater.h"
#include "dg.h"
#include "nmg.h"
#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"


extern struct db_tree_state mged_initial_tree_state;	/* dodraw.c */

extern void color_soltab(void);
extern void set_absolute_tran(void); /* defined in set.c */
extern void set_absolute_view_tran(void); /* defined in set.c */
extern void set_absolute_model_tran(void); /* defined in set.c */

void solid_list_callback(void);
void knob_update_rate_vars(void);
int mged_svbase(void);
int mged_vrot(char origin, fastf_t *newrot);
int mged_zoom(double val);
void mged_center(fastf_t *center);
static void abs_zoom(void);
void usejoy(double xangle, double yangle, double zangle);

int knob_rot(vect_t rvec, char origin, int mf, int vf, int ef);
int knob_tran(fastf_t *tvec, int model_flag, int view_flag, int edit_flag);
int mged_etran(char coords, vect_t tvec);
int mged_mtran(const fastf_t *tvec);
int mged_otran(const fastf_t *tvec);
int mged_vtran(const fastf_t *tvec);
int mged_tran(fastf_t *tvec);


extern vect_t curr_e_axes_pos;
extern long nvectors;

extern vect_t e_axes_pos;

fastf_t ar_scale_factor = GED_MAX / ABS_ROT_FACTOR;
fastf_t rr_scale_factor = GED_MAX / RATE_ROT_FACTOR;
fastf_t adc_angle_scale_factor = GED_MAX / ADC_ANGLE_FACTOR;

vect_t edit_absolute_model_rotate;
vect_t edit_absolute_object_rotate;
vect_t edit_absolute_view_rotate;
vect_t last_edit_absolute_model_rotate;
vect_t last_edit_absolute_object_rotate;
vect_t last_edit_absolute_view_rotate;
vect_t edit_rate_model_rotate;
vect_t edit_rate_object_rotate;
vect_t edit_rate_view_rotate;
int edit_rateflag_model_rotate;
int edit_rateflag_object_rotate;
int edit_rateflag_view_rotate;

vect_t edit_absolute_model_tran;
vect_t edit_absolute_view_tran;
vect_t last_edit_absolute_model_tran;
vect_t last_edit_absolute_view_tran;
vect_t edit_rate_model_tran;
vect_t edit_rate_view_tran;
int edit_rateflag_model_tran;
int edit_rateflag_view_tran;

fastf_t edit_absolute_scale;
fastf_t edit_rate_scale;
int edit_rateflag_scale;

char edit_rate_model_origin;
char edit_rate_object_origin;
char edit_rate_view_origin;
char edit_rate_coords;
struct dm_list *edit_rate_mr_dm_list;
struct dm_list *edit_rate_or_dm_list;
struct dm_list *edit_rate_vr_dm_list;
struct dm_list *edit_rate_mt_dm_list;
struct dm_list *edit_rate_vt_dm_list;

struct bu_vls edit_rate_model_tran_vls[3];
struct bu_vls edit_rate_view_tran_vls[3];
struct bu_vls edit_rate_model_rotate_vls[3];
struct bu_vls edit_rate_object_rotate_vls[3];
struct bu_vls edit_rate_view_rotate_vls[3];
struct bu_vls edit_rate_scale_vls;
struct bu_vls edit_absolute_model_tran_vls[3];
struct bu_vls edit_absolute_view_tran_vls[3];
struct bu_vls edit_absolute_model_rotate_vls[3];
struct bu_vls edit_absolute_object_rotate_vls[3];
struct bu_vls edit_absolute_view_rotate_vls[3];
struct bu_vls edit_absolute_scale_vls;

double mged_abs_tol;
double mged_rel_tol = 0.01;		/* 1%, by default */
double mged_nrm_tol;			/* normal ang tol, radians */


/* DEBUG -- force view center */
/* Format: C x y z */
int
cmd_center(ClientData UNUSED(clientData),
	   Tcl_Interp *interp,
	   int argc,
	   const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_center(gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    if (argc > 1) {
	(void)mged_svbase();
	view_state->vs_flag = 1;
    }

    return TCL_OK;
}


void
mged_center(point_t center)
{
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (gedp == GED_NULL)
	return;

    snprintf(xbuf, 32, "%f", center[X]);
    snprintf(ybuf, 32, "%f", center[Y]);
    snprintf(zbuf, 32, "%f", center[Z]);

    av[0] = "center";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_center(gedp, 4, (const char **)av);
    (void)mged_svbase();
    view_state->vs_flag = 1;
}


/* DEBUG -- force viewsize */
/* Format: view size */
int
cmd_size(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_size(gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == GED_OK) {
	view_state->vs_absolute_scale = 1.0 - view_state->vs_gvp->gv_scale / view_state->vs_i_Viewscale;
	if (view_state->vs_absolute_scale < 0.0)
	    view_state->vs_absolute_scale /= 9.0;

	if (!ZERO(view_state->vs_absolute_tran[X])
	    || !ZERO(view_state->vs_absolute_tran[Y])
	    || !ZERO(view_state->vs_absolute_tran[Z]))
	{
	    set_absolute_tran();
	}

	if (argc > 1)
	    view_state->vs_flag = 1;

	return TCL_OK;
    }

    return TCL_ERROR;
}


/*
 * S I Z E _ R E S E T
 *
 * Reset view size and view center so that all solids in the solid table
 * are in view.
 * Caller is responsible for calling new_mats().
 */
void
size_reset(void)
{
    char *av[2];

    if (gedp == GED_NULL)
	return;

    av[0] = "autoview";
    av[1] = (char *)0;
    ged_autoview(gedp, 1, (const char **)av);
    view_state->vs_i_Viewscale = view_state->vs_gvp->gv_scale;
    view_state->vs_flag = 1;
}


/*
 * E D I T _ C O M
 *
 * B and e commands use this area as common
 */
int
edit_com(int argc,
	 const char *argv[],
	 int kind)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct dm_list *dmlp;
    struct dm_list *save_dmlp;
    struct cmd_list *save_cmd_list;
    int ret;
    int initial_blank_screen = 1;

    int flag_A_attr=0;
    int flag_R_noresize=0;
    int flag_o_nonunique=1;

    size_t i;
    int last_opt=0;
    struct bu_vls vls;

    CHECK_DBI_NULL;

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->gdl_headSolid)) {
	    initial_blank_screen = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    /* check args for "-A" (attributes) and "-o" and "-R" */
    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, argv[0]);
    for (i=1; i<(size_t)argc; i++) {
	char *ptr_A=NULL;
	char *ptr_o=NULL;
	char *ptr_R=NULL;
	const char *c;

	if (*argv[i] != '-') break;
	if ((ptr_A=strchr(argv[i], 'A'))) flag_A_attr = 1;
	if ((ptr_o=strchr(argv[i], 'o'))) flag_o_nonunique = 2;
	if ((ptr_R=strchr(argv[i], 'R'))) flag_R_noresize = 1;
	last_opt = i;

	if (!ptr_A && !ptr_o && !ptr_R) {
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, argv[i]);
	    continue;
	}

	if (strlen(argv[i]) == (size_t)(1 + (ptr_A != NULL) + (ptr_o != NULL) + (ptr_R != NULL))) {
	    /* argv[i] is just a "-A" or "-o" or "-R" */
	    continue;
	}

	/* copy args other than "-A" or "-o" or "-R" */
	bu_vls_putc(&vls, ' ');
	c = argv[i];
	while (*c != '\0') {
	    if (*c != 'A' && *c != 'o' && *c != 'R') {
		bu_vls_putc(&vls, *c);
	    }
	    c++;
	}
    }

    if (flag_A_attr) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	int max_count=0;
	int remaining_args=0;
	int new_argc=0;
	char **new_argv=NULL;
	struct bu_ptbl *tbl;

	remaining_args = argc - last_opt - 1;;
	if (remaining_args < 2 || remaining_args%2) {
	    bu_log("Error: must have even number of arguments (name/value pairs)\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	bu_avs_init(&avs, (argc - last_opt)/2, "edit_com avs");
	i = 1;
	while (i < (size_t)argc) {
	    if (*argv[i] == '-') {
		i++;
		continue;
	    }

	    /* this is a name/value pair */
	    if (flag_o_nonunique == 2) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	    i += 2;
	}

	tbl = db_lookup_by_attr(dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return TCL_OK;
	}
	for (i=0; i<BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, dp->d_namep);
	}

	max_count = BU_PTBL_LEN(tbl) + last_opt + 1;
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "edit_com ptbl");
	new_argv = (char **)bu_calloc(max_count+1, sizeof(char *), "edit_com new_argv");
	new_argc = bu_argv_from_string(new_argv, max_count, bu_vls_addr(&vls));

	ret = ged_draw(gedp, new_argc, (const char **)new_argv);
	if (ret == GED_ERROR) {
	    bu_log("ERROR: %s\n", bu_vls_addr(gedp->ged_result_str));
	    bu_vls_free(&vls);
	    bu_free((char *)new_argv, "edit_com new_argv");
	    return ret;
	} else if (ret == GED_HELP) {
	    bu_log("%s\n", bu_vls_addr(gedp->ged_result_str));
	    bu_vls_free(&vls);
	    bu_free((char *)new_argv, "edit_com new_argv");
	    return ret;
	}
	bu_vls_free(&vls);
	bu_free((char *)new_argv, "edit_com new_argv");
    } else {
	bu_vls_free(&vls);
	switch (kind) {
	    default:
	    case 1:
		ret = ged_draw(gedp, argc, (const char **)argv);
		break;
	    case 2:
		ret = ged_E(gedp, argc, (const char **)argv);
		break;
	    case 3:
		ret = ged_ev(gedp, argc, (const char **)argv);
		break;
	}
	if (ret == GED_ERROR) {
	    bu_log("ERROR: %s\n", bu_vls_addr(gedp->ged_result_str));
	    return TCL_ERROR;
	} else if (ret == GED_HELP) {
	    bu_log("%s\n", bu_vls_addr(gedp->ged_result_str));
	    return TCL_OK;
	}
    }

    update_views = 1;

    if (flag_R_noresize) {
	/* we're done */
	return TCL_OK;
    }

    /* update and resize the views */

    save_dmlp = curr_dm_list;
    save_cmd_list = curr_cmd_list;
    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l) {
	int non_empty = 0; /* start out empty */

	curr_dm_list = dmlp;
	if (curr_dm_list->dml_tie)
	    curr_cmd_list = curr_dm_list->dml_tie;
	else
	    curr_cmd_list = &head_cmd_list;

	gedp->ged_gvp = view_state->vs_gvp;

	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    if (BU_LIST_NON_EMPTY(&gdlp->gdl_headSolid)) {
		non_empty = 1;
		break;
	    }

	    gdlp = next_gdlp;
	}

	/* If we went from blank screen to non-blank, resize */
	if (mged_variables->mv_autosize && initial_blank_screen && non_empty) {
	    struct view_ring *vrp;
	    char *av[2];

	    av[0] = "autoview";
	    av[1] = (char *)0;
	    ged_autoview(gedp, 1, (const char **)av);

	    (void)mged_svbase();

	    for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l))
		vrp->vr_scale = view_state->vs_gvp->gv_scale;
	}
    }

    curr_dm_list = save_dmlp;
    curr_cmd_list = save_cmd_list;
    gedp->ged_gvp = view_state->vs_gvp;

    return TCL_OK;
}


int
emuves_com(int argc, const char *argv[])
{
    size_t i;
    struct bu_ptbl *tbl;
    struct bu_attribute_value_set avs;
    const char **objs;
    int ret;
    size_t num_opts=0;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(INTERP, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (i=1; i<(size_t)argc; i++) {
	if (*argv[i] == '-') {
	    num_opts++;
	} else {
	    break;
	}
    }

    bu_avs_init(&avs, argc/2, "muves_avs");
    for (i=1; i<(size_t)argc; i++) {
	bu_avs_add_nonunique(&avs, "MUVES_Component", argv[i]);
    }

    tbl = db_lookup_by_attr(dbip, RT_DIR_REGION, &avs, 2);

    bu_avs_free(&avs);

    if (!tbl) {
	return TCL_OK;
    }

    if (BU_PTBL_LEN(tbl) < 1) {
	bu_free((char *)tbl, "tbl returned by wdb_get_by_attr");
	return TCL_OK;
    }

    objs = (const char **)bu_calloc((BU_PTBL_LEN(tbl) + 1 + num_opts), sizeof(char *), "emuves_com objs");
    for (i=0; i<=num_opts; i++) {
	objs[i] = argv[i];
    }
    for (i=0; i<BU_PTBL_LEN(tbl); i++) {
	struct directory *dp;

	dp = (struct directory *)BU_PTBL_GET(tbl, i);
	objs[i+num_opts+1] = dp->d_namep;
    }

    ret = edit_com((BU_PTBL_LEN(tbl) + 1), objs, 1);
    bu_ptbl_free(tbl);
    bu_free((char *)tbl, "tbl returned by wdb_get_by_attr");
    bu_free((char *)objs, "emuves_com objs");
    return ret;
}


int
cmd_autoview(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct dm_list *dmlp;
    struct dm_list *save_dmlp;
    struct cmd_list *save_cmd_list;

    if (argc != 1) {
	struct bu_vls vls;

	bu_log("Unexpected parameter [%s]\n", argv[1]);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help autoview");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (gedp == GED_NULL)
	return TCL_OK;

    save_dmlp = curr_dm_list;
    save_cmd_list = curr_cmd_list;
    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l) {
	struct view_ring *vrp;

	curr_dm_list = dmlp;
	if (curr_dm_list->dml_tie)
	    curr_cmd_list = curr_dm_list->dml_tie;
	else
	    curr_cmd_list = &head_cmd_list;

	gedp->ged_gvp = view_state->vs_gvp;

	{
	    char *av[2];

	    av[0] = "autoview";
	    av[1] = (char *)0;
	    ged_autoview(gedp, 1, (const char **)av);
	    view_state->vs_flag = 1;
	}
	(void)mged_svbase();

	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l))
	    vrp->vr_scale = view_state->vs_gvp->gv_scale;
    }
    curr_dm_list = save_dmlp;
    curr_cmd_list = save_cmd_list;
    gedp->ged_gvp = view_state->vs_gvp;

    return TCL_OK;
}


void
solid_list_callback(void)
{
    struct bu_vls vls;
    Tcl_Obj *save_obj;

    /* save result */
    save_obj = Tcl_GetObjResult(INTERP);
    Tcl_IncrRefCount(save_obj);

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "solid_list_callback");
    (void)Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    /* restore result */
    Tcl_SetObjResult(INTERP, save_obj);
    Tcl_DecrRefCount(save_obj);
}


/*
 * F _ R E G D E B U G
 *
 * Display-manager specific "hardware register" debugging.
 */
int
f_regdebug(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    static int regdebug = 0;
    static char debug_str[10];

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help regdebug");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1)
	regdebug = !regdebug;	/* toggle */
    else
	regdebug = atoi(argv[1]);

    sprintf(debug_str, "%d", regdebug);

    Tcl_AppendResult(interp, "regdebug=", debug_str, "\n", (char *)NULL);

    DM_DEBUG(dmp, regdebug);

    return TCL_OK;
}


/*
 * To return all the free "struct bn_vlist" and "struct solid" items
 * lurking on their respective freelists, back to bu_malloc().
 * Primarily as an aid to tracking memory leaks.
 * WARNING:  This depends on knowledge of the macro GET_SOLID in mged/solid.h
 * and RT_GET_VLIST in h/raytrace.h.
 */
void
mged_freemem(void)
{
    struct solid *sp;
    struct bn_vlist *vp;

    FOR_ALL_SOLIDS(sp, &MGED_FreeSolid.l) {
	GET_SOLID(sp, &MGED_FreeSolid.l);
	bu_free((genptr_t)sp, "mged_freemem: struct solid");
    }

    while (BU_LIST_NON_EMPTY(&rt_g.rtg_vlfree)) {
	vp = BU_LIST_FIRST(bn_vlist, &rt_g.rtg_vlfree);
	BU_LIST_DEQUEUE(&(vp->l));
	bu_free((genptr_t)vp, "mged_freemem: struct bn_vlist");
    }
}


/* ZAP the display -- everything dropped */
/* Format: Z */
int
cmd_zap(ClientData UNUSED(clientData), Tcl_Interp *UNUSED(interp), int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    char *av[2] = {"zap", (char *)0};

    CHECK_DBI_NULL;

    update_views = 1;

    /* FIRST, reject any editing in progress */
    if (STATE != ST_VIEW)
	button(BE_REJECT);

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);
	freeDListsAll(BU_LIST_FIRST(solid, &gdlp->gdl_headSolid)->s_dlist,
		      BU_LIST_LAST(solid, &gdlp->gdl_headSolid)->s_dlist -
		      BU_LIST_FIRST(solid, &gdlp->gdl_headSolid)->s_dlist + 1);

	gdlp = next_gdlp;
    }

    ged_zap(gedp, 1, (const char **)av);

    /* Keeping freelists improves performance.  When debugging, give mem back */
    if (RT_G_DEBUG)
	mged_freemem();

    (void)chg_state(STATE, STATE, "zap");
    solid_list_callback();

    return TCL_OK;
}


int
f_status(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_vls vls;

    CHECK_DBI_NULL;

    if (argc < 1 || 2 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help status");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "STATE=%s, ", state_str[STATE]);
	bu_vls_printf(&vls, "Viewscale=%f (%f mm)\n",
		      view_state->vs_gvp->gv_scale*base2local, view_state->vs_gvp->gv_scale);
	bu_vls_printf(&vls, "base2local=%f\n", base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	bn_tcl_mat_print(interp, "toViewcenter", view_state->vs_gvp->gv_center);
	bn_tcl_mat_print(interp, "Viewrot", view_state->vs_gvp->gv_rotation);
	bn_tcl_mat_print(interp, "model2view", view_state->vs_gvp->gv_model2view);
	bn_tcl_mat_print(interp, "view2model", view_state->vs_gvp->gv_view2model);

	if (STATE != ST_VIEW) {
	    bn_tcl_mat_print(interp, "model2objview", view_state->vs_model2objview);
	    bn_tcl_mat_print(interp, "objview2model", view_state->vs_objview2model);
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "state")) {
	Tcl_AppendResult(interp, state_str[STATE], (char *)NULL);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "Viewscale")) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%f", view_state->vs_gvp->gv_scale*base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "base2local")) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%f", base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "local2base")) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%f", local2base);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "toViewcenter")) {
	bn_tcl_mat_print(interp, "toViewcenter", view_state->vs_gvp->gv_center);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "Viewrot")) {
	bn_tcl_mat_print(interp, "Viewrot", view_state->vs_gvp->gv_rotation);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "model2view")) {
	bn_tcl_mat_print(interp, "model2view", view_state->vs_gvp->gv_model2view);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "view2model")) {
	bn_tcl_mat_print(interp, "view2model", view_state->vs_gvp->gv_view2model);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "model2objview")) {
	bn_tcl_mat_print(interp, "model2objview", view_state->vs_model2objview);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "objview2model")) {
	bn_tcl_mat_print(interp, "objview2model", view_state->vs_objview2model);
	return TCL_OK;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help status");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    if (BU_STR_EQUAL(argv[1], "help"))
	return TCL_OK;

    return TCL_ERROR;
}


int
f_refresh(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help refresh");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    return TCL_OK;
}


static char ** path_parse (char *path);

/* Illuminate the named object */
int
f_ill(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct directory *dp;
    struct solid *sp;
    struct solid *lastfound = SOLID_NULL;
    int i, j;
    int nmatch;
    int c;
    int ri = 0;
    int nm_pieces;
    int illum_only = 0;
    char **path_piece = 0;
    char *mged_basename;
    char *sname;

    int early_out = 0;

    char **nargv;
    char **orig_nargv;
    struct bu_vls vlsargv;

    CHECK_DBI_NULL;

    if (argc < 2 || 5 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help ill");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&vlsargv);
    bu_vls_from_argv(&vlsargv, argc, argv);
    nargv = bu_calloc(argc+1, sizeof(char *), "calloc f_ill nargv");
    orig_nargv = nargv;
    c = bu_argv_from_string(nargv, argc, bu_vls_addr(&vlsargv));
    if (c != argc) {
	Tcl_AppendResult(interp, "ERROR: unable to processes command arguments for f_ill()\n", (char*)NULL);
	
	bu_free(orig_nargv, "free f_ill nargv");
	bu_vls_free(&vlsargv);

	return TCL_ERROR;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, nargv, "i:n")) != -1) {
	switch (c) {
	    case 'n':
		illum_only = 1;
		break;
	    case 'i':
		sscanf(bu_optarg, "%d", &ri);
		if (ri <= 0) {
		    Tcl_AppendResult(interp,
				     "the reference index must be greater than 0\n",
				     (char *)NULL);

		    early_out = 1;
		    break;
		}

		break;
	    default:
	    case 'h':
		{
		    struct bu_vls vls;

		    bu_vls_init(&vls);
		    bu_vls_printf(&vls, "help ill");
		    Tcl_Eval(interp, bu_vls_addr(&vls));
		    bu_vls_free(&vls);

		    early_out = 1;
		    break;
		}
	}
    }

    if (early_out) {
	return TCL_ERROR;
    }

    argc -= (bu_optind - 1);
    nargv += (bu_optind - 1);

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help ill");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	bu_free(orig_nargv, "free f_ill nargv");
	bu_vls_free(&vlsargv);

	return TCL_ERROR;
    }

    if (STATE != ST_S_PICK && STATE != ST_O_PICK) {
	state_err("keyboard illuminate pick");
	goto bail_out;
    }

    path_piece = path_parse(nargv[1]);
    for (nm_pieces = 0; path_piece[nm_pieces] != 0; ++nm_pieces)
	;

    if (nm_pieces == 0) {
	Tcl_AppendResult(interp, "Bad solid path: '", nargv[1], "'\n", (char *)NULL);
	goto bail_out;
    }

    mged_basename = path_piece[nm_pieces - 1];

    if ((dp = db_lookup(dbip,  mged_basename, LOOKUP_NOISY)) == RT_DIR_NULL) {
	Tcl_AppendResult(interp, "db_lookup failed for '", mged_basename, "'\n", (char *)NULL);
	goto bail_out;
    }

    nmatch = 0;
    if (!(dp->d_flags & RT_DIR_SOLID)) {
	Tcl_AppendResult(interp, mged_basename, " is not a solid\n", (char *)NULL);
	goto bail_out;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    int a_new_match;

/* XXX Could this make use of db_full_path_subset()? */
	    if (nmatch == 0 || nmatch != ri) {
		i = sp->s_fullpath.fp_len-1;
		if (DB_FULL_PATH_GET(&sp->s_fullpath, i) == dp) {
		    a_new_match = 1;
		    j = nm_pieces - 1;
		    for (; a_new_match && (i >= 0) && (j >= 0); --i, --j) {
			sname = DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_namep;
			if ((*sname != *(path_piece[j]))
			    || !BU_STR_EQUAL(sname, path_piece[j]))
			    a_new_match = 0;
		    }

		    if (a_new_match && ((i >= 0) || (j < 0))) {
			lastfound = sp;
			++nmatch;
		    }
		}
	    }

	    sp->s_iflag = DOWN;
	}

	gdlp = next_gdlp;
    }

    if (nmatch == 0) {
	Tcl_AppendResult(interp, nargv[1], " not being displayed\n", (char *)NULL);
	goto bail_out;
    }

    /* preserve same old behavior */
    if (nmatch > 1 && ri == 0) {
	Tcl_AppendResult(interp, nargv[1], " multiply referenced\n", (char *)NULL);
	goto bail_out;
    } else if (ri != 0 && ri != nmatch) {
	Tcl_AppendResult(interp,
			 "the reference index must be less than the number of references\n",
			 (char *)NULL);
	goto bail_out;
    }

    /* Make the specified solid the illuminated solid */
    illump = lastfound;
    illump->s_iflag = UP;

    if (!illum_only) {
	if (STATE == ST_O_PICK) {
	    ipathpos = 0;
	    (void)chg_state(ST_O_PICK, ST_O_PATH, "Keyboard illuminate");
	} else {
	    /* Check details, Init menu, set state=ST_S_EDIT */
	    init_sedit();
	}
    }

    update_views = 1;
    if (path_piece)
    {
	for (i = 0; path_piece[i] != 0; ++i)
	    bu_free((genptr_t)path_piece[i], "f_ill: char *");
	bu_free((genptr_t) path_piece, "f_ill: char **");
    }

    bu_free(orig_nargv, "free f_ill nargv");
    bu_vls_free(&vlsargv);

    return TCL_OK;

 bail_out:
    if (STATE != ST_VIEW) {
	struct bu_vls vls;

	bu_vls_init(&vls);

	bu_vls_printf(&vls, "%s", Tcl_GetStringResult(interp));
	button(BE_REJECT);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

	bu_vls_free(&vls);
    }

    if (path_piece)
    {
	for (i = 0; path_piece[i] != 0; ++i)
	    bu_free((genptr_t)path_piece[i], "f_ill: char *");
	bu_free((genptr_t) path_piece, "f_ill: char **");
    }

    bu_free(orig_nargv, "free f_ill nargv");
    bu_vls_free(&vlsargv);

    return TCL_ERROR;
}


/* Simulate pressing "Solid Edit" and doing an ILLuminate command */
int
f_sed(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    int is_empty = 1;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 5 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_VIEW, "keyboard solid edit start"))
	return TCL_ERROR;

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
	Tcl_AppendResult(interp, "no solids being displayed\n",  (char *)NULL);
	return TCL_ERROR;
    }

    update_views = 1;

    button(BE_S_ILLUMINATE);	/* To ST_S_PICK */

    argv[0] = "ill";

    /* Illuminate named solid --> ST_S_EDIT */
    if (f_ill(clientData, interp, argc, argv) == TCL_ERROR) {
	Tcl_Obj *save_result;

	save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);
	button(BE_REJECT);
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);
	return TCL_ERROR;
    }

    return TCL_OK;
}


static void
check_nonzero_rates(void)
{
    if (!ZERO(view_state->vs_rate_model_rotate[X])
	|| !ZERO(view_state->vs_rate_model_rotate[Y])
	|| !ZERO(view_state->vs_rate_model_rotate[Z]))
    {
	view_state->vs_rateflag_model_rotate = 1;
    } else {
	view_state->vs_rateflag_model_rotate = 0;
    }

    if (!ZERO(view_state->vs_rate_model_tran[X])
	|| !ZERO(view_state->vs_rate_model_tran[Y])
	|| !ZERO(view_state->vs_rate_model_tran[Z]))
    {
	view_state->vs_rateflag_model_tran = 1;
    } else {
	view_state->vs_rateflag_model_tran = 0;
    }

    if (!ZERO(view_state->vs_rate_rotate[X])
	|| !ZERO(view_state->vs_rate_rotate[Y])
	|| !ZERO(view_state->vs_rate_rotate[Z]))
    {
	view_state->vs_rateflag_rotate = 1;
    } else {
	view_state->vs_rateflag_rotate = 0;
    }

    if (!ZERO(view_state->vs_rate_tran[X])
	|| !ZERO(view_state->vs_rate_tran[Y])
	|| !ZERO(view_state->vs_rate_tran[Z]))
    {
	view_state->vs_rateflag_tran = 1;
    } else {
	view_state->vs_rateflag_tran = 0;
    }

    if (!ZERO(view_state->vs_rate_scale))
	view_state->vs_rateflag_scale = 1;
    else
	view_state->vs_rateflag_scale = 0;

    if (!ZERO(edit_rate_model_tran[X])
	|| !ZERO(edit_rate_model_tran[Y])
	|| !ZERO(edit_rate_model_tran[Z])) {
	edit_rateflag_model_tran = 1;
    } else {
	edit_rateflag_model_tran = 0;
    }

    if (!ZERO(edit_rate_view_tran[X])
	|| !ZERO(edit_rate_view_tran[Y])
	|| !ZERO(edit_rate_view_tran[Z]))
    {
	edit_rateflag_view_tran = 1;
    } else {
	edit_rateflag_view_tran = 0;
    }

    if (!ZERO(edit_rate_model_rotate[X])
	|| !ZERO(edit_rate_model_rotate[Y])
	|| !ZERO(edit_rate_model_rotate[Z]))
    {
	edit_rateflag_model_rotate = 1;
    } else {
	edit_rateflag_model_rotate = 0;
    }

    if (!ZERO(edit_rate_object_rotate[X])
	|| !ZERO(edit_rate_object_rotate[Y])
	|| !ZERO(edit_rate_object_rotate[Z]))
    {
	edit_rateflag_object_rotate = 1;
    } else {
	edit_rateflag_object_rotate = 0;
    }

    if (!ZERO(edit_rate_view_rotate[X])
	|| !ZERO(edit_rate_view_rotate[Y])
	|| !ZERO(edit_rate_view_rotate[Z]))
    {
	edit_rateflag_view_rotate = 1;
    } else {
	edit_rateflag_view_rotate = 0;
    }

    if (edit_rate_scale > SMALL_FASTF)
	edit_rateflag_scale = 1;
    else
	edit_rateflag_scale = 0;

    view_state->vs_flag = 1;	/* values changed so update faceplate */
}


void
knob_update_rate_vars(void)
{
    if (!mged_variables->mv_rateknobs)
	return;
}


int
mged_print_knobvals(Tcl_Interp *interp)
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (mged_variables->mv_rateknobs) {
	if (es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "x = %f\n", edit_rate_model_rotate[X]);
	    bu_vls_printf(&vls, "y = %f\n", edit_rate_model_rotate[Y]);
	    bu_vls_printf(&vls, "z = %f\n", edit_rate_model_rotate[Z]);
	} else {
	    bu_vls_printf(&vls, "x = %f\n", view_state->vs_rate_rotate[X]);
	    bu_vls_printf(&vls, "y = %f\n", view_state->vs_rate_rotate[Y]);
	    bu_vls_printf(&vls, "z = %f\n", view_state->vs_rate_rotate[Z]);
	}

	if (es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e')
	    bu_vls_printf(&vls, "S = %f\n", edit_rate_scale);
	else
	    bu_vls_printf(&vls, "S = %f\n", view_state->vs_rate_scale);

	if (es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "X = %f\n", edit_rate_model_tran[X]);
	    bu_vls_printf(&vls, "Y = %f\n", edit_rate_model_tran[Y]);
	    bu_vls_printf(&vls, "Z = %f\n", edit_rate_model_tran[Z]);
	} else {
	    bu_vls_printf(&vls, "X = %f\n", view_state->vs_rate_tran[X]);
	    bu_vls_printf(&vls, "Y = %f\n", view_state->vs_rate_tran[Y]);
	    bu_vls_printf(&vls, "Z = %f\n", view_state->vs_rate_tran[Z]);
	}
    } else {
	if (es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "ax = %f\n", edit_absolute_model_rotate[X]);
	    bu_vls_printf(&vls, "ay = %f\n", edit_absolute_model_rotate[Y]);
	    bu_vls_printf(&vls, "az = %f\n", edit_absolute_model_rotate[Z]);
	} else {
	    bu_vls_printf(&vls, "ax = %f\n", view_state->vs_absolute_rotate[X]);
	    bu_vls_printf(&vls, "ay = %f\n", view_state->vs_absolute_rotate[Y]);
	    bu_vls_printf(&vls, "az = %f\n", view_state->vs_absolute_rotate[Z]);
	}

	if (es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e')
	    bu_vls_printf(&vls, "aS = %f\n", edit_absolute_scale);
	else
	    bu_vls_printf(&vls, "aS = %f\n", view_state->vs_absolute_scale);

	if (es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "aX = %f\n", edit_absolute_model_tran[X]);
	    bu_vls_printf(&vls, "aY = %f\n", edit_absolute_model_tran[Y]);
	    bu_vls_printf(&vls, "aZ = %f\n", edit_absolute_model_tran[Z]);
	} else {
	    bu_vls_printf(&vls, "aX = %f\n", view_state->vs_absolute_tran[X]);
	    bu_vls_printf(&vls, "aY = %f\n", view_state->vs_absolute_tran[Y]);
	    bu_vls_printf(&vls, "aZ = %f\n", view_state->vs_absolute_tran[Z]);
	}
    }

    if (adc_state->adc_draw) {
	bu_vls_printf(&vls, "xadc = %d\n", adc_state->adc_dv_x);
	bu_vls_printf(&vls, "yadc = %d\n", adc_state->adc_dv_y);
	bu_vls_printf(&vls, "ang1 = %d\n", adc_state->adc_dv_a1);
	bu_vls_printf(&vls, "ang2 = %d\n", adc_state->adc_dv_a2);
	bu_vls_printf(&vls, "distadc = %d\n", adc_state->adc_dv_dist);
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/* Main processing of knob twists.  "knob id val id val ..." */
int
f_knob(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    int i;
    fastf_t f;
    fastf_t sf;
    vect_t tvec;
    vect_t rvec;
    const char *cmd;
    char origin = '\0';
    int do_tran = 0;
    int do_rot = 0;
    int incr_flag = 0;  /* interpret values as increments */
    int view_flag = 0;  /* manipulate view using view coords */
    int model_flag = 0; /* manipulate view using model coords */
    int edit_flag = 0;  /* force edit interpretation */

    CHECK_DBI_NULL;

    if (argc < 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help knob");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Check for options */
    {
	int c;

	bu_optind = 1;
	while ((c = bu_getopt(argc, (char * const *)argv, "eimo:v")) != -1) {
	    switch (c) {
		case 'e':
		    edit_flag = 1;
		    break;
		case 'i':
		    incr_flag = 1;
		    break;
		case 'm':
		    model_flag = 1;
		    break;
		case 'o':
		    origin = *bu_optarg;
		    break;
		case 'v':
		    view_flag = 1;
		    break;
		default:
		    break;
	    }
	}

	argv += bu_optind - 1;
	argc -= bu_optind - 1;
    }

    if (origin != 'v' && origin != 'm' && origin != 'e' && origin != 'k')
	origin = mged_variables->mv_rotate_about;

    /* print the current values */
    if (argc == 1)
	return mged_print_knobvals(interp);

    VSETALL(tvec, 0.0);
    VSETALL(rvec, 0.0);

    for (--argc, ++argv; argc; --argc, ++argv) {
	cmd = *argv;

	if (BU_STR_EQUAL(cmd, "zap") || BU_STR_EQUAL(cmd, "zero")) {
	    const char *av[3];

	    VSETALL(view_state->vs_rate_model_rotate, 0.0);
	    VSETALL(view_state->vs_rate_model_tran, 0.0);
	    VSETALL(view_state->vs_rate_rotate, 0.0);
	    VSETALL(view_state->vs_rate_tran, 0.0);
	    view_state->vs_rate_scale = 0.0;
	    VSETALL(edit_rate_model_rotate, 0.0);
	    VSETALL(edit_rate_object_rotate, 0.0);
	    VSETALL(edit_rate_view_rotate, 0.0);
	    VSETALL(edit_rate_model_tran, 0.0);
	    VSETALL(edit_rate_view_tran, 0.0);
	    edit_rate_scale = 0.0;
	    knob_update_rate_vars();

	    av[0] = "adc";
	    av[1] = "reset";
	    av[2] = NULL;

	    (void)f_adc(clientData, interp, 2, av);

	    (void)mged_svbase();
	} else if (BU_STR_EQUAL(cmd, "calibrate")) {
	    VSETALL(view_state->vs_absolute_tran, 0.0);
	} else {
	    if (argc - 1) {
		i = atoi(argv[1]);
		f = atof(argv[1]);
	    } else
		goto usage;

	    --argc;
	    ++argv;

	    if (cmd[1] == '\0') {

		switch (cmd[0]) {
		    case 'x':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[X] += f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[X] += f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[X] += f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[X] += f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[X] += f;
				    view_state->vs_rate_origin = origin;
				}

			    }
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[X] = f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[X] = f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[X] = f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[X] = f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[X] = f;
				    view_state->vs_rate_origin = origin;
				}

			    }
			}

			break;
		    case 'y':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[Y] += f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[Y] += f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[Y] += f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[Y] += f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[Y] += f;
				    view_state->vs_rate_origin = origin;
				}
			    }
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[Y] = f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[Y] = f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[Y] = f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[Y] = f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[Y] = f;
				    view_state->vs_rate_origin = origin;
				}
			    }
			}

			break;
		    case 'z':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[Z] += f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[Z] += f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[Z] += f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[Z] += f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[Z] += f;
				    view_state->vs_rate_origin = origin;
				}
			    }
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_rate_model_rotate[Z] = f;
					edit_rate_model_origin = origin;
					edit_rate_mr_dm_list = curr_dm_list;

					break;
				    case 'o':
					edit_rate_object_rotate[Z] = f;
					edit_rate_object_origin = origin;
					edit_rate_or_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_rotate[Z] = f;
					edit_rate_view_origin = origin;
					edit_rate_vr_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_rate_model_rotate[Z] = f;
				    view_state->vs_rate_model_origin = origin;
				} else {
				    view_state->vs_rate_rotate[Z] = f;
				    view_state->vs_rate_origin = origin;
				}
			    }
			}

			break;
		    case 'X':
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[X] += f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[X] += f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[X] += f;
				else
				    view_state->vs_rate_tran[X] += f;
			    }
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[X] = f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[X] = f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[X] = f;
				else
				    view_state->vs_rate_tran[X] = f;
			    }
			}

			break;
		    case 'Y':
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[Y] += f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[Y] += f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[Y] += f;
				else
				    view_state->vs_rate_tran[Y] += f;
			    }
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[Y] = f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[Y] = f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[Y] = f;
				else
				    view_state->vs_rate_tran[Y] = f;
			    }
			}

			break;
		    case 'Z':
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[Z] += f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[Z] += f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[Z] += f;
				else
				    view_state->vs_rate_tran[Z] += f;
			    }
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_rate_model_tran[Z] = f;
					edit_rate_mt_dm_list = curr_dm_list;

					break;
				    case 'v':
				    default:
					edit_rate_view_tran[Z] = f;
					edit_rate_vt_dm_list = curr_dm_list;

					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
				    view_state->vs_rate_model_tran[Z] = f;
				else
				    view_state->vs_rate_tran[Z] = f;
			    }
			}

			break;
		    case 'S':
			if (incr_flag) {
			    if (EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)) {
				edit_rate_scale += f;
			    } else {
				view_state->vs_rate_scale += f;
			    }
			} else {
			    if (EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)) {
				edit_rate_scale = f;
			    } else {
				view_state->vs_rate_scale = f;
			    }
			}

			break;
		    default:
			goto usage;
		}
	    } else if (cmd[0] == 'a' && cmd[1] != '\0' && cmd[2] == '\0') {
		switch (cmd[1]) {
		    case 'x':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_absolute_model_rotate[X] += f;
					break;
				    case 'o':
					edit_absolute_object_rotate[X] += f;
					break;
				    case 'v':
					edit_absolute_view_rotate[X] += f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_absolute_model_rotate[X] += f;
				} else {
				    view_state->vs_absolute_rotate[X] += f;
				}
			    }

			    rvec[X] = f;
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					rvec[X] = f - last_edit_absolute_model_rotate[X];
					edit_absolute_model_rotate[X] = f;
					break;
				    case 'o':
					rvec[X] = f - last_edit_absolute_object_rotate[X];
					edit_absolute_object_rotate[X] = f;
					break;
				    case 'v':
					rvec[X] = f - last_edit_absolute_view_rotate[X];
					edit_absolute_view_rotate[X] = f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    rvec[X] = f - view_state->vs_last_absolute_model_rotate[X];
				    view_state->vs_absolute_model_rotate[X] = f;
				} else {
				    rvec[X] = f - view_state->vs_last_absolute_rotate[X];
				    view_state->vs_absolute_rotate[X] = f;
				}
			    }
			}

			/* wrap around */
			if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
					     !view_flag && !model_flag) || edit_flag)) {
			    fastf_t *arp;
			    fastf_t *larp;

			    switch (mged_variables->mv_coords) {
				case 'm':
				    arp = edit_absolute_model_rotate;
				    larp = last_edit_absolute_model_rotate;
				    break;
				case 'o':
				    arp = edit_absolute_object_rotate;
				    larp = last_edit_absolute_object_rotate;
				    break;
				case 'v':
				    arp = edit_absolute_view_rotate;
				    larp = last_edit_absolute_view_rotate;
				    break;
				default:
				    bu_log("unknown mv_coords\n");
				    arp = larp = NULL;
			    }

			    if (arp[X] < -180.0)
				arp[X] = arp[X] + 360.0;
			    else if (arp[X] > 180.0)
				arp[X] = arp[X] - 360.0;

			    larp[X] = arp[X];
			} else {
			    fastf_t *arp;
			    fastf_t *larp;

			    if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				arp = view_state->vs_absolute_model_rotate;
				larp = view_state->vs_last_absolute_model_rotate;
			    } else {
				arp = view_state->vs_absolute_rotate;
				larp = view_state->vs_last_absolute_rotate;
			    }

			    if (arp[X] < -180.0)
				arp[X] = arp[X] + 360.0;
			    else if (arp[X] > 180.0)
				arp[X] = arp[X] - 360.0;

			    larp[X] = arp[X];
			}

			do_rot = 1;
			break;
		    case 'y':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_absolute_model_rotate[Y] += f;
					break;
				    case 'o':
					edit_absolute_object_rotate[Y] += f;
					break;
				    case 'v':
					edit_absolute_view_rotate[Y] += f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_absolute_model_rotate[Y] += f;
				} else {
				    view_state->vs_absolute_rotate[Y] += f;
				}
			    }

			    rvec[Y] = f;
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					rvec[Y] = f - last_edit_absolute_model_rotate[Y];
					edit_absolute_model_rotate[Y] = f;
					break;
				    case 'o':
					rvec[Y] = f - last_edit_absolute_object_rotate[Y];
					edit_absolute_object_rotate[Y] = f;
					break;
				    case 'v':
					rvec[Y] = f - last_edit_absolute_view_rotate[Y];
					edit_absolute_view_rotate[Y] = f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    rvec[Y] = f - view_state->vs_last_absolute_model_rotate[Y];
				    view_state->vs_absolute_model_rotate[Y] = f;
				} else {
				    rvec[Y] = f - view_state->vs_last_absolute_rotate[Y];
				    view_state->vs_absolute_rotate[Y] = f;
				}
			    }
			}

			/* wrap around */
			if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
					     !view_flag && !model_flag) || edit_flag)) {
			    fastf_t *arp;
			    fastf_t *larp;

			    switch (mged_variables->mv_coords) {
				case 'm':
				    arp = edit_absolute_model_rotate;
				    larp = last_edit_absolute_model_rotate;
				    break;
				case 'o':
				    arp = edit_absolute_object_rotate;
				    larp = last_edit_absolute_object_rotate;
				    break;
				case 'v':
				    arp = edit_absolute_view_rotate;
				    larp = last_edit_absolute_view_rotate;
				    break;
				default:
				    bu_log("unknown mv_transform\n");
				    arp = larp = NULL;
			    }

			    if (arp[Y] < -180.0)
				arp[Y] = arp[Y] + 360.0;
			    else if (arp[X] > 180.0)
				arp[Y] = arp[Y] - 360.0;

			    larp[Y] = arp[Y];
			} else {
			    fastf_t *arp;
			    fastf_t *larp;

			    if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				arp = view_state->vs_absolute_model_rotate;
				larp = view_state->vs_last_absolute_model_rotate;
			    } else {
				arp = view_state->vs_absolute_rotate;
				larp = view_state->vs_last_absolute_rotate;
			    }

			    if (arp[Y] < -180.0)
				arp[Y] = arp[Y] + 360.0;
			    else if (arp[Y] > 180.0)
				arp[Y] = arp[Y] - 360.0;

			    larp[Y] = arp[Y];
			}

			do_rot = 1;
			break;
		    case 'z':
			if (incr_flag) {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					edit_absolute_model_rotate[Z] += f;
					break;
				    case 'o':
					edit_absolute_object_rotate[Z] += f;
					break;
				    case 'v':
					edit_absolute_view_rotate[Z] += f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    view_state->vs_absolute_model_rotate[Z] += f;
				} else {
				    view_state->vs_absolute_rotate[Z] += f;
				}
			    }

			    rvec[Z] = f;
			} else {
			    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
						 !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
					rvec[Z] = f - last_edit_absolute_model_rotate[Z];
					edit_absolute_model_rotate[Z] = f;
					break;
				    case 'o':
					rvec[Z] = f - last_edit_absolute_object_rotate[Z];
					edit_absolute_object_rotate[Z] = f;
					break;
				    case 'v':
					rvec[Z] = f - last_edit_absolute_view_rotate[Z];
					edit_absolute_view_rotate[Z] = f;
					break;
				}
			    } else {
				if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				    rvec[Z] = f - view_state->vs_last_absolute_model_rotate[Z];
				    view_state->vs_absolute_model_rotate[Z] = f;
				} else {
				    rvec[Z] = f - view_state->vs_last_absolute_rotate[Z];
				    view_state->vs_absolute_rotate[Z] = f;
				}
			    }
			}

			/* wrap around */
			if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
					     !view_flag && !model_flag) || edit_flag)) {
			    fastf_t *arp;
			    fastf_t *larp;

			    switch (mged_variables->mv_coords) {
				case 'm':
				    arp = edit_absolute_model_rotate;
				    larp = last_edit_absolute_model_rotate;
				    break;
				case 'o':
				    arp = edit_absolute_object_rotate;
				    larp = last_edit_absolute_object_rotate;
				    break;
				case 'v':
				    arp = edit_absolute_view_rotate;
				    larp = last_edit_absolute_view_rotate;
				    break;
				default:
				    bu_log("unknown mv_coords\n");
				    arp = larp = NULL;
			    }

			    if (arp[Z] < -180.0)
				arp[Z] = arp[Z] + 360.0;
			    else if (arp[Z] > 180.0)
				arp[Z] = arp[Z] - 360.0;

			    larp[Z] = arp[Z];
			} else {
			    fastf_t *arp;
			    fastf_t *larp;

			    if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				arp = view_state->vs_absolute_model_rotate;
				larp = view_state->vs_last_absolute_model_rotate;
			    } else {
				arp = view_state->vs_absolute_rotate;
				larp = view_state->vs_last_absolute_rotate;
			    }

			    if (arp[Z] < -180.0)
				arp[Z] = arp[Z] + 360.0;
			    else if (arp[Z] > 180.0)
				arp[Z] = arp[Z] - 360.0;

			    larp[Z] = arp[Z];
			}

			do_rot = 1;
			break;
		    case 'X':
			sf = f * local2base / view_state->vs_gvp->gv_scale;
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_absolute_model_tran[X] += sf;
					last_edit_absolute_model_tran[X] = edit_absolute_model_tran[X];

					break;
				    case 'v':
					edit_absolute_view_tran[X] += sf;
					last_edit_absolute_view_tran[X] = edit_absolute_view_tran[X];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				view_state->vs_absolute_model_tran[X] += sf;
				view_state->vs_last_absolute_model_tran[X] = view_state->vs_absolute_model_tran[X];
			    } else {
				view_state->vs_absolute_tran[X] += sf;
				view_state->vs_last_absolute_tran[X] = view_state->vs_absolute_tran[X];
			    }

			    tvec[X] = f;
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					tvec[X] = f - last_edit_absolute_model_tran[X]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_model_tran[X] = sf;
					last_edit_absolute_model_tran[X] = edit_absolute_model_tran[X];

					break;
				    case 'v':
					tvec[X] = f - last_edit_absolute_view_tran[X]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_view_tran[X] = sf;
					last_edit_absolute_view_tran[X] = edit_absolute_view_tran[X];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				tvec[X] = f - view_state->vs_last_absolute_model_tran[X]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_model_tran[X] = sf;
				view_state->vs_last_absolute_model_tran[X] = view_state->vs_absolute_model_tran[X];
			    } else {
				tvec[X] = f - view_state->vs_last_absolute_tran[X]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_tran[X] = sf;
				view_state->vs_last_absolute_tran[X] = view_state->vs_absolute_tran[X];
			    }

			}

			do_tran = 1;
			break;
		    case 'Y':
			sf = f * local2base / view_state->vs_gvp->gv_scale;
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_absolute_model_tran[Y] += sf;
					last_edit_absolute_model_tran[Y] = edit_absolute_model_tran[Y];

					break;
				    case 'v':
					edit_absolute_view_tran[Y] += sf;
					last_edit_absolute_view_tran[Y] = edit_absolute_view_tran[Y];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				view_state->vs_absolute_model_tran[Y] += sf;
				view_state->vs_last_absolute_model_tran[Y] = view_state->vs_absolute_model_tran[Y];
			    } else {
				view_state->vs_absolute_tran[Y] += sf;
				view_state->vs_last_absolute_tran[Y] = view_state->vs_absolute_tran[Y];
			    }

			    tvec[Y] = f;
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					tvec[Y] = f - last_edit_absolute_model_tran[Y]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_model_tran[Y] = sf;
					last_edit_absolute_model_tran[Y] = edit_absolute_model_tran[Y];

					break;
				    case 'v':
					tvec[Y] = f - last_edit_absolute_view_tran[Y]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_view_tran[Y] = sf;
					last_edit_absolute_view_tran[Y] = edit_absolute_view_tran[Y];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				tvec[Y] = f - view_state->vs_last_absolute_model_tran[Y]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_model_tran[Y] = sf;
				view_state->vs_last_absolute_model_tran[Y] = view_state->vs_absolute_model_tran[Y];
			    } else {
				tvec[Y] = f - view_state->vs_last_absolute_tran[Y]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_tran[Y] = sf;
				view_state->vs_last_absolute_tran[Y] = view_state->vs_absolute_tran[Y];
			    }
			}

			do_tran = 1;
			break;
		    case 'Z':
			sf = f * local2base / view_state->vs_gvp->gv_scale;
			if (incr_flag) {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					edit_absolute_model_tran[Z] += sf;
					last_edit_absolute_model_tran[Z] = edit_absolute_model_tran[Z];

					break;
				    case 'v':
					edit_absolute_view_tran[Z] += sf;
					last_edit_absolute_view_tran[Z] = edit_absolute_view_tran[Z];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				view_state->vs_absolute_model_tran[Z] += sf;
				view_state->vs_last_absolute_model_tran[Z] = view_state->vs_absolute_model_tran[Z];
			    } else {
				view_state->vs_absolute_tran[Z] += sf;
				view_state->vs_last_absolute_tran[Z] = view_state->vs_absolute_tran[Z];
			    }

			    tvec[Z] = f;
			} else {
			    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
					       !view_flag && !model_flag) || edit_flag)) {
				switch (mged_variables->mv_coords) {
				    case 'm':
				    case 'o':
					tvec[Z] = f - last_edit_absolute_model_tran[Z]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_model_tran[Z] = sf;
					last_edit_absolute_model_tran[Z] = edit_absolute_model_tran[Z];

					break;
				    case 'v':
					tvec[Z] = f - last_edit_absolute_view_tran[Z]*view_state->vs_gvp->gv_scale*base2local;
					edit_absolute_view_tran[Z] = sf;
					last_edit_absolute_view_tran[Z] = edit_absolute_view_tran[Z];

					break;
				}
			    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
				tvec[Z] = f - view_state->vs_last_absolute_model_tran[Z]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_model_tran[Z] = sf;
				view_state->vs_last_absolute_model_tran[Z] = view_state->vs_absolute_model_tran[Z];
			    } else {
				tvec[Z] = f - view_state->vs_last_absolute_tran[Z]*view_state->vs_gvp->gv_scale*base2local;
				view_state->vs_absolute_tran[Z] = sf;
				view_state->vs_last_absolute_tran[Z] = view_state->vs_absolute_tran[Z];
			    }
			}

			do_tran = 1;
			break;
		    case 'S':
			if (incr_flag) {
			    if (EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)) {
				edit_absolute_scale += f;
				if (STATE == ST_S_EDIT)
				    sedit_abs_scale();
				else
				    oedit_abs_scale();
			    } else {
				view_state->vs_absolute_scale += f;
				abs_zoom();
			    }
			} else {
			    if (EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)) {
				edit_absolute_scale = f;
				if (STATE == ST_S_EDIT)
				    sedit_abs_scale();
				else
				    oedit_abs_scale();
			    } else {
				view_state->vs_absolute_scale = f;
				abs_zoom();
			    }
			}

			break;
		    default:
			goto usage;
		}
	    } else if (BU_STR_EQUAL(cmd, "xadc")) {
		const char *av[5];
		char sval[32];
		int nargs = 3;

		av[0] = "adc";
		if (incr_flag) {
		    ++nargs;
		    av[1] = "-i";
		    av[2] = "x";
		    av[3] = sval;
		    av[4] = NULL;
		} else {
		    av[1] = "x";
		    av[2] = sval;
		    av[3] = NULL;
		}

		sprintf(sval, "%d", i);
		(void)f_adc(clientData, interp, nargs, av);
	    } else if (BU_STR_EQUAL(cmd, "yadc")) {
		const char *av[5];
		char sval[32];
		int nargs = 3;

		av[0] = "adc";
		if (incr_flag) {
		    ++nargs;
		    av[1] = "-i";
		    av[2] = "y";
		    av[3] = sval;
		    av[4] = NULL;
		} else {
		    av[1] = "y";
		    av[2] = sval;
		    av[3] = NULL;
		}

		sprintf(sval, "%d", i);
		(void)f_adc(clientData, interp, nargs, av);
	    } else if (BU_STR_EQUAL(cmd, "ang1")) {
		const char *av[5];
		char sval[32];
		int nargs = 3;

		av[0] = "adc";
		if (incr_flag) {
		    ++nargs;
		    av[1] = "-i";
		    av[2] = "a1";
		    av[3] = sval;
		    av[4] = NULL;
		} else {
		    av[1] = "a1";
		    av[2] = sval;
		    av[3] = NULL;
		}

		sprintf(sval, "%f", f);
		(void)f_adc(clientData, interp, nargs, av);
	    } else if (BU_STR_EQUAL(cmd, "ang2")) {
		const char *av[5];
		char sval[32];
		int nargs = 3;

		av[0] = "adc";
		if (incr_flag) {
		    ++nargs;
		    av[1] = "-i";
		    av[2] = "a2";
		    av[3] = sval;
		    av[4] = NULL;
		} else {
		    av[1] = "a2";
		    av[2] = sval;
		    av[3] = NULL;
		}

		sprintf(sval, "%f", f);
		(void)f_adc(clientData, interp, nargs, av);
	    } else if (BU_STR_EQUAL(cmd, "distadc")) {
		const char *av[5];
		char sval[32];
		int nargs = 3;

		av[0] = "adc";
		if (incr_flag) {
		    ++nargs;
		    av[1] = "-i";
		    av[2] = "odst";
		    av[3] = sval;
		    av[4] = NULL;
		} else {
		    av[1] = "odst";
		    av[2] = sval;
		    av[3] = NULL;
		}

		sprintf(sval, "%d", i);
		(void)f_adc(clientData, interp, nargs, av);
	    } else {
	    usage:
		Tcl_AppendResult(interp,
				 "knob: x, y, z for rotation in degrees\n",
				 "knob: S for scale; X, Y, Z for slew (rates, range -1..+1)\n",
				 "knob: ax, ay, az for absolute rotation in degrees; aS for absolute scale.\n",
				 "knob: aX, aY, aZ for absolute slew.  calibrate to set current slew to 0\n",
				 "knob: xadc, yadc, distadc (values, range -2048..+2047)\n",
				 "knob: ang1, ang2 for adc angles in degrees\n",
				 "knob: zero (cancel motion)\n", (char *)NULL);

		return TCL_ERROR;
	    }
	}
    }

    if (do_tran)
	(void)knob_tran(tvec, model_flag, view_flag, edit_flag);

    if (do_rot)
	(void)knob_rot(rvec, origin, model_flag, view_flag, edit_flag);

    check_nonzero_rates();
    return TCL_OK;
}


int
knob_tran(vect_t tvec,
	  int model_flag,
	  int view_flag,
	  int edit_flag)
{
    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
		       !view_flag && !model_flag) || edit_flag))
	mged_etran(mged_variables->mv_coords, tvec);
    else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	mged_mtran(tvec);
    else if (mged_variables->mv_coords == 'o')
	mged_otran(tvec);
    else
	mged_vtran(tvec);

    return TCL_OK;
}


int
knob_rot(vect_t rvec,
	 char origin,
	 int model_flag,
	 int view_flag,
	 int edit_flag)
{
    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			 !view_flag && !model_flag) || edit_flag))
	mged_erot_xyz(origin, rvec);
    else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	mged_vrot_xyz(origin, 'm', rvec);
    else if (mged_variables->mv_coords == 'o')
	mged_vrot_xyz(origin, 'o', rvec);
    else
	mged_vrot_xyz(origin, 'v', rvec);

    return TCL_OK;
}


/* absolute_scale's value range is [-1.0, 1.0] */
static void
abs_zoom(void)
{
    char *av[3];

    /* Use initial Viewscale */
    if (-SMALL_FASTF < view_state->vs_absolute_scale && view_state->vs_absolute_scale < SMALL_FASTF)
	view_state->vs_gvp->gv_scale = view_state->vs_i_Viewscale;
    else {
	/* if positive */
	if (view_state->vs_absolute_scale > 0) {
	    /* scale i_Viewscale by values in [0.0, 1.0] range */
	    view_state->vs_gvp->gv_scale = view_state->vs_i_Viewscale * (1.0 - view_state->vs_absolute_scale);
	} else/* negative */
	    /* scale i_Viewscale by values in [1.0, 10.0] range */
	    view_state->vs_gvp->gv_scale = view_state->vs_i_Viewscale * (1.0 + (view_state->vs_absolute_scale * -9.0));
    }

    if (view_state->vs_gvp->gv_scale < RT_MINVIEWSIZE)
	view_state->vs_gvp->gv_scale = RT_MINVIEWSIZE;

    av[0] = "zoom";
    av[1] = "1.0";
    av[2] = (char *)0;
    ged_zoom(gedp, 2, (const char **)av);

    if (!ZERO(view_state->vs_absolute_tran[X])
	|| !ZERO(view_state->vs_absolute_tran[Y])
	|| !ZERO(view_state->vs_absolute_tran[Z]))
    {
	set_absolute_tran();
    }
}


int
mged_zoom(double val)
{
    int ret;
    char *av[3];
    char buf[32];
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    snprintf(buf, 32, "%f", val);

    av[0] = "zoom";
    av[1] = buf;
    av[2] = (char *)0;

    ret = ged_zoom(gedp, 2, (const char **)av);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(INTERP, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    view_state->vs_absolute_scale = 1.0 - view_state->vs_gvp->gv_scale / view_state->vs_i_Viewscale;

    if (view_state->vs_absolute_scale < 0.0)
	view_state->vs_absolute_scale /= 9.0;

    if (!ZERO(view_state->vs_absolute_tran[X])
	|| !ZERO(view_state->vs_absolute_tran[Y])
	|| !ZERO(view_state->vs_absolute_tran[Z]))
    {
	set_absolute_tran();
    }

    view_state->vs_flag = 1;

    return TCL_OK;
}


/*
 * F _ Z O O M
 *
 * A scale factor of 2 will increase the view size by a factor of 2,
 * (i.e., a zoom out) which is accomplished by reducing Viewscale in half.
 */
int
cmd_zoom(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help zoom");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    return mged_zoom(atof(argv[1]));
}


/*
 * P A T H _ P A R S E
 *
 * Break up a path string into its constituents.
 *
 * This function has one parameter:  a slash-separated path.
 * path_parse() allocates storage for and copies each constituent
 * of path.  It returns a null-terminated array of these copies.
 *
 * It is the caller's responsibility to free the copies and the
 * pointer to them.
 */
static char **
path_parse (char *path)
{
    int nm_constituents;
    int i;
    char *pp;
    char *start_addr;
    char **result;

    nm_constituents = ((*path != '/') && (*path != '\0'));
    for (pp = path; *pp != '\0'; ++pp)
	if (*pp == '/')
	{
	    while (*++pp == '/')
		;
	    if (*pp != '\0')
		++nm_constituents;
	}

    result = (char **) bu_malloc((nm_constituents + 1) * sizeof(char *),
				 "array of strings");

    for (i = 0, pp = path; i < nm_constituents; ++i)
    {
	while (*pp == '/')
	    ++pp;
	start_addr = pp;
	while ((*++pp != '/') && (*pp != '\0'))
	    ;
	result[i] = (char *) bu_malloc((pp - start_addr + 1) * sizeof(char), "string");
	bu_strlcpy(result[i], start_addr, (pp - start_addr + 1) * sizeof(char));
    }
    result[nm_constituents] = 0;

    return result;
}


int
cmd_setview(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    ret = ged_setview(gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    if (!ZERO(view_state->vs_absolute_tran[X])
	|| !ZERO(view_state->vs_absolute_tran[Y])
	|| !ZERO(view_state->vs_absolute_tran[Z]))
    {
	set_absolute_tran();
    }

    view_state->vs_flag = 1;

    return TCL_OK;
}


int
f_slewview(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;
    point_t old_model_center;
    point_t new_model_center;
    vect_t diff;
    mat_t delta;

    if (gedp == GED_NULL)
	return TCL_OK;

    /* this is for the ModelDelta calculation below */
    MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_gvp->gv_center);

    Tcl_DStringInit(&ds);

    ret = ged_slew(gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    view_state->vs_flag = 1;

    /* all this for ModelDelta */
    MAT_DELTAS_GET_NEG(new_model_center, view_state->vs_gvp->gv_center);
    VSUB2(diff, new_model_center, old_model_center);
    MAT_IDN(delta);
    MAT_DELTAS_VEC(delta, diff);
    bn_mat_mul2(delta, view_state->vs_ModelDelta);	/* updates ModelDelta */

    set_absolute_tran();
    return TCL_OK;
}


/* set view reference base */
int
mged_svbase(void)
{
    MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_gvp->gv_center);
    view_state->vs_i_Viewscale = view_state->vs_gvp->gv_scale;

    /* reset absolute slider values */
    VSETALL(view_state->vs_absolute_rotate, 0.0);
    VSETALL(view_state->vs_last_absolute_rotate, 0.0);
    VSETALL(view_state->vs_absolute_model_rotate, 0.0);
    VSETALL(view_state->vs_last_absolute_model_rotate, 0.0);
    VSETALL(view_state->vs_absolute_tran, 0.0);
    VSETALL(view_state->vs_last_absolute_tran, 0.0);
    VSETALL(view_state->vs_absolute_model_tran, 0.0);
    VSETALL(view_state->vs_last_absolute_model_tran, 0.0);
    view_state->vs_absolute_scale = 0.0;

    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui)
	curr_dm_list->dml_dirty = 1;

    return TCL_OK;
}


int
f_svbase(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int status;
    struct dm_list *dmlp;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	if (argv && argc > 1)
	    bu_log("Unexpected parameter [%s]\n", argv[1]);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel svb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    status = mged_svbase();

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	/* if sharing view while faceplate and original gui (i.e. button menu, sliders) are on */
	if (dmlp->dml_view_state == view_state &&
	    dmlp->dml_mged_variables->mv_faceplate &&
	    dmlp->dml_mged_variables->mv_orig_gui)
	    dmlp->dml_dirty = 1;

    return status;
}


/*
 * F _ V R O T _ C E N T E R
 *
 * Set the center of rotation, either in model coordinates, or
 * in view (+/-1) coordinates.
 * The default is to rotate around the view center: v=(0, 0, 0).
 */
int
f_vrot_center(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    if (argc < 5 || 5 < argc) {
	struct bu_vls vls;

	if (argv && argc > 5)
	    bu_log("Unexpected parameter [%s]\n", argv[5]);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help vrot_center");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* XXXX Actually, this is now available in LIBRT's view_obj.c */
    Tcl_AppendResult(interp, "Not ready until tomorrow.\n", (char *)NULL);
    return TCL_OK;
}


/*
 * U S E J O Y
 *
 * Apply the "joystick" delta rotation to the viewing direction,
 * where the delta is specified in terms of the *viewing* axes.
 * Rotation is performed about the view center, for now.
 * Angles are in radians.
 */
void
usejoy(double xangle, double yangle, double zangle)
{
    mat_t newrot;		/* NEW rot matrix, from joystick */

    /* NORMAL CASE.
     * Apply delta viewing rotation for non-edited parts.
     * The view rotates around the VIEW CENTER.
     */
    MAT_IDN(newrot);
    bn_mat_angles_rad(newrot, xangle, yangle, zangle);

    bn_mat_mul2(newrot, view_state->vs_gvp->gv_rotation);
    {
	mat_t newinv;
	bn_mat_inv(newinv, newrot);
	wrt_view(view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta);	/* Updates ModelDelta */
    }
    new_mats();
}


/*
 * S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
setview(double a1,
	double a2,
	double a3)		/* DOUBLE angles, in degrees */
{
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (gedp == GED_NULL)
	return;

    snprintf(xbuf, 32, "%f", a1);
    snprintf(ybuf, 32, "%f", a2);
    snprintf(zbuf, 32, "%f", a3);

    av[0] = "setview";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_setview(gedp, 4, (const char **)av);

    if (!ZERO(view_state->vs_absolute_tran[X])
	|| !ZERO(view_state->vs_absolute_tran[Y])
	|| !ZERO(view_state->vs_absolute_tran[Z]))
    {
	set_absolute_tran();
    }

    view_state->vs_flag = 1;
}


/*
 * S L E W V I E W
 *
 * Given a position in view space,
 * make that point the new view center.
 */
void
slewview(vect_t view_pos)
{
    point_t old_model_center;
    point_t new_model_center;
    vect_t diff;
    mat_t delta;
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (gedp == GED_NULL)
	return;

    /* this is for the ModelDelta calculation below */
    MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_gvp->gv_center);

    snprintf(xbuf, 32, "%f", view_pos[X]);
    snprintf(ybuf, 32, "%f", view_pos[Y]);
    snprintf(zbuf, 32, "%f", view_pos[Z]);

    av[0] = "slew";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_slew(gedp, 4, (const char **)av);

    /* all this for ModelDelta */
    MAT_DELTAS_GET_NEG(new_model_center, view_state->vs_gvp->gv_center);
    VSUB2(diff, new_model_center, old_model_center);
    MAT_IDN(delta);
    MAT_DELTAS_VEC(delta, diff);
    bn_mat_mul2(delta, view_state->vs_ModelDelta);	/* updates ModelDelta */

    set_absolute_tran();

    view_state->vs_flag = 1;
}


/*
 * Initialize vsp1 using vsp2 if vsp2 is not null.
 */
void
view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2)
{
    struct view_ring *vrp1;
    struct view_ring *vrp2;

    BU_LIST_INIT(&vsp1->vs_headView.l);

    if (vsp2 != (struct _view_state *)NULL) {
	struct view_ring *vrp1_current_view = NULL;
	struct view_ring *vrp1_last_view = NULL;

	for (BU_LIST_FOR(vrp2, view_ring, &vsp2->vs_headView.l)) {
	    BU_GETSTRUCT(vrp1, view_ring);
	    /* append to last list element */
	    BU_LIST_APPEND(vsp1->vs_headView.l.back, &vrp1->l);

	    MAT_COPY(vrp1->vr_rot_mat, vrp2->vr_rot_mat);
	    MAT_COPY(vrp1->vr_tvc_mat, vrp2->vr_tvc_mat);
	    vrp1->vr_scale = vrp2->vr_scale;
	    vrp1->vr_id = vrp2->vr_id;

	    if (vsp2->vs_current_view == vrp2)
		vrp1_current_view = vrp1;

	    if (vsp2->vs_last_view == vrp2)
		vrp1_last_view = vrp1;
	}

	vsp1->vs_current_view = vrp1_current_view;
	vsp1->vs_last_view = vrp1_last_view;
    } else {
	BU_GETSTRUCT(vrp1, view_ring);
	BU_LIST_APPEND(&vsp1->vs_headView.l, &vrp1->l);

	vrp1->vr_id = 1;
	vsp1->vs_current_view = vrp1;
	vsp1->vs_last_view = vrp1;
    }
}


void
view_ring_destroy(struct dm_list *dlp)
{
    struct view_ring *vrp;

    while (BU_LIST_NON_EMPTY(&dlp->dml_view_state->vs_headView.l)) {
	vrp = BU_LIST_FIRST(view_ring, &dlp->dml_view_state->vs_headView.l);
	BU_LIST_DEQUEUE(&vrp->l);
	bu_free((genptr_t)vrp, "view_ring_destroy: vrp");
    }
}


/*
 * SYNOPSIS
 * view_ring add|next|prev|toggle
 * view_ring delete # delete view #
 * view_ring goto # goto view #
 * view_ring get [-a]			get the current view
 *
 * DESCRIPTION
 *
 * EXAMPLES
 *
 */
int
f_view_ring(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int n;
    struct view_ring *vrp;
    struct view_ring *lv;
    struct bu_vls vls;

    if (argc < 2 || 3 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel view_ring");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "add")) {
	if (argc != 2) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	/* allocate memory and append to list */
	BU_GETSTRUCT(vrp, view_ring);
	lv = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);
	BU_LIST_APPEND(&lv->l, &vrp->l);

	/* assign a view number */
	vrp->vr_id = lv->vr_id + 1;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = vrp;
	(void)mged_svbase();

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "next")) {
	if (argc != 2) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l))
	    return TCL_OK;

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_current_view);

	if (BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l))
	    view_state->vs_current_view = BU_LIST_FIRST(view_ring, &view_state->vs_headView.l);

	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats();
	(void)mged_svbase();

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "prev")) {
	if (argc != 2) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l))
	    return TCL_OK;

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = BU_LIST_PLAST(view_ring, view_state->vs_current_view);

	if (BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l))
	    view_state->vs_current_view = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);

	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats();
	(void)mged_svbase();

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "toggle")) {
	struct view_ring *save_last_view;

	if (argc != 2) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	save_last_view = view_state->vs_last_view;
	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = save_last_view;
	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats();
	(void)mged_svbase();

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "delete")) {
	if (argc != 3) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* search for view with id of n */
	n = atoi(argv[2]);
	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    if (vrp->vr_id == n)
		break;
	}

	if (BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring delete: ", argv[2], " is not a valid view\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(vrp->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(vrp->l.back, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring delete: Cannot delete the only remaining view!\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if (vrp == view_state->vs_current_view) {
	    if (view_state->vs_current_view == view_state->vs_last_view) {
		view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_last_view);
		view_state->vs_last_view = view_state->vs_current_view;
	    } else
		view_state->vs_current_view = view_state->vs_last_view;

	    MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	    MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	    view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;
	    new_mats();
	    (void)mged_svbase();
	} else if (vrp == view_state->vs_last_view)
	    view_state->vs_last_view = view_state->vs_current_view;

	BU_LIST_DEQUEUE(&vrp->l);
	bu_free((genptr_t)vrp, "view_ring delete");

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "goto")) {
	if (argc != 3) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* search for view with id of n */
	n = atoi(argv[2]);
	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    if (vrp->vr_id == n)
		break;
	}

	if (BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring goto: ", argv[2], " is not a valid view\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	/* nothing to do */
	if (vrp == view_state->vs_current_view)
	    return TCL_OK;

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = vrp;
	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats();
	(void)mged_svbase();

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "get")) {
	/* return current view */
	if (argc == 2) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "%d", view_state->vs_current_view->vr_id);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_OK;
	}

	if (!BU_STR_EQUAL("-a", argv[2])) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	bu_vls_init(&vls);
	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    bu_vls_printf(&vls, "%d", vrp->vr_id);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));
	    bu_vls_trunc(&vls, 0);
	}

	bu_vls_free(&vls);
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "view_ring: unrecognized command - ", argv[1], (char *)NULL);
    return TCL_ERROR;
}


int
mged_erot(char coords,
	  char rotate_about,
	  mat_t newrot)
{
    int save_edflag;
    mat_t temp1, temp2;

    update_views = 1;

    switch (coords) {
	case 'm':
	    break;
	case 'o':
	    bn_mat_inv(temp1, acc_rot_sol);

	    /* transform into object rotations */
	    bn_mat_mul(temp2, acc_rot_sol, newrot);
	    bn_mat_mul(newrot, temp2, temp1);
	    break;
	case 'v':
	    bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);

	    /* transform into model rotations */
	    bn_mat_mul(temp2, temp1, newrot);
	    bn_mat_mul(newrot, temp2, view_state->vs_gvp->gv_rotation);
	    break;
    }

    if (STATE == ST_S_EDIT) {
	char save_rotate_about;

	save_rotate_about = mged_variables->mv_rotate_about;
	mged_variables->mv_rotate_about = rotate_about;

	save_edflag = es_edflag;
	if (!SEDIT_ROTATE)
	    es_edflag = SROT;

	inpara = 0;
	MAT_COPY(incr_change, newrot);
	bn_mat_mul2(incr_change, acc_rot_sol);
	sedit();

	mged_variables->mv_rotate_about = save_rotate_about;
	es_edflag = save_edflag;
    } else {
	point_t point;
	vect_t work;

	bn_mat_mul2(newrot, acc_rot_sol);

	/* find point for rotation to take place wrt */
	switch (rotate_about) {
	    case 'v':       /* View Center */
		VSET(work, 0.0, 0.0, 0.0);
		MAT4X3PNT(point, view_state->vs_gvp->gv_view2model, work);
		break;
	    case 'e':       /* Eye */
		VSET(work, 0.0, 0.0, 1.0);
		MAT4X3PNT(point, view_state->vs_gvp->gv_view2model, work);
		break;
	    case 'm':       /* Model Center */
		VSETALL(point, 0.0);
		break;
	    case 'k':
	    default:
		MAT4X3PNT(point, modelchanges, es_keypoint);
	}

	/*
	 * Apply newrot to the modelchanges matrix wrt "point"
	 */
	wrt_point(modelchanges, newrot, modelchanges, point);

	new_edit_mats();
    }

    return TCL_OK;
}


int
mged_erot_xyz(char rotate_about,
	      vect_t rvec)
{
    mat_t newrot;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    return mged_erot(mged_variables->mv_coords, rotate_about, newrot);
}


int
cmd_mrot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord; /* dummy argument for ged_rot_args */
	mat_t rmat;

	if (argc != 2 && argc != 4) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, "Usage: ", -1);
	    Tcl_DStringAppend(&ds, argv[0], -1);
	    Tcl_DStringAppend(&ds, " x y z", -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	/* We're only interested in getting rmat set */
	if (ged_rot_args(gedp, argc, (const char **)argv, &coord, rmat) != GED_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(view_state->vs_gvp->gv_coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_mrot(gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != GED_OK)
	    return TCL_ERROR;

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


/*
 * M G E D _ V R O T
 */
int
mged_vrot(char origin, fastf_t *newrot)
{
    mat_t newinv;

    bn_mat_inv(newinv, newrot);

    if (origin != 'v') {
	point_t rot_pt;
	point_t new_origin;
	mat_t viewchg, viewchginv;
	point_t new_cent_view;
	point_t new_cent_model;

	if (origin == 'e') {
	    /* "VR driver" method: rotate around "eye" point (0, 0, 1) viewspace */
	    VSET(rot_pt, 0.0, 0.0, 1.0);		/* point to rotate around */
	} else if (origin == 'k' && STATE == ST_S_EDIT) {
	    /* rotate around keypoint */
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
	} else if (origin == 'k' && STATE == ST_O_EDIT) {
	    point_t kpWmc;

	    MAT4X3PNT(kpWmc, modelchanges, es_keypoint);
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, kpWmc);
	} else {
	    /* rotate around model center (0, 0, 0) */
	    VSET(new_origin, 0.0, 0.0, 0.0);
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, new_origin);  /* point to rotate around */
	}

	bn_mat_xform_about_pt(viewchg, newrot, rot_pt);
	bn_mat_inv(viewchginv, viewchg);

	/* Convert origin in new (viewchg) coords back to old view coords */
	VSET(new_origin, 0.0, 0.0, 0.0);
	MAT4X3PNT(new_cent_view, viewchginv, new_origin);
	MAT4X3PNT(new_cent_model, view_state->vs_gvp->gv_view2model, new_cent_view);
	MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, new_cent_model);

	/* XXX This should probably capture the translation too */
	/* XXX I think the only consumer of ModelDelta is the predictor frame */
	wrt_view(view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta);		/* pure rotation */
    } else
	/* Traditional method:  rotate around view center (0, 0, 0) viewspace */
	wrt_view(view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta);

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2(newrot, view_state->vs_gvp->gv_rotation); /* pure rotation */
    new_mats();

    set_absolute_tran();

    return TCL_OK;
}


int
mged_vrot_xyz(char origin,
	      char coords,
	      vect_t rvec)
{
    mat_t newrot;
    mat_t temp1, temp2;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    if (coords == 'm') {
	/* transform model rotations into view rotations */
	bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);
	bn_mat_mul(temp2, view_state->vs_gvp->gv_rotation, newrot);
	bn_mat_mul(newrot, temp2, temp1);
    } else if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) && coords == 'o') {
	/* first, transform object rotations into model rotations */
	bn_mat_inv(temp1, acc_rot_sol);
	bn_mat_mul(temp2, acc_rot_sol, newrot);
	bn_mat_mul(newrot, temp2, temp1);

	/* now transform model rotations into view rotations */
	bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);
	bn_mat_mul(temp2, view_state->vs_gvp->gv_rotation, newrot);
	bn_mat_mul(newrot, temp2, temp1);
    } /* else assume already view rotations */

    return mged_vrot(origin, newrot);
}


int
cmd_vrot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    ret = ged_vrot(gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    view_state->vs_flag = 1;
    set_absolute_tran();

    return TCL_OK;
}


int
cmd_rot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord;
	mat_t rmat;

	if (ged_rot_args(gedp, argc, (const char **)argv, &coord, rmat) != GED_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_rot(gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != GED_OK)
	    return TCL_ERROR;

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
cmd_arot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    /* static const char *usage = "x y z angle"; */

    if (gedp == GED_NULL)
	return TCL_OK;

    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	mat_t rmat;

	if (ged_arot_args(gedp, argc, (const char **)argv, rmat) != GED_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(view_state->vs_gvp->gv_coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_arot(gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != GED_OK)
	    return TCL_ERROR;

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
mged_etran(char coords,
	   vect_t tvec)
{
    point_t p2;
    int save_edflag;
    point_t delta;
    point_t vcenter;
    point_t work;
    mat_t xlatemat;

    /* compute delta */
    switch (coords) {
	case 'm':
	    VSCALE(delta, tvec, local2base);
	    break;
	case 'o':
	    VSCALE(p2, tvec, local2base);
	    MAT4X3PNT(delta, acc_rot_sol, p2);
	    break;
	case 'v':
	default:
	    VSCALE(p2, tvec, local2base/view_state->vs_gvp->gv_scale);
	    MAT4X3PNT(work, view_state->vs_gvp->gv_view2model, p2);
	    MAT_DELTAS_GET_NEG(vcenter, view_state->vs_gvp->gv_center);
	    VSUB2(delta, work, vcenter);

	    break;
    }

    if (STATE == ST_S_EDIT) {
	save_edflag = es_edflag;
	if (!SEDIT_TRAN)
	    es_edflag = STRANS;

	VADD2(es_para, delta, curr_e_axes_pos);
	inpara = 3;
	sedit();
	es_edflag = save_edflag;
    } else {
	MAT_IDN(xlatemat);
	MAT_DELTAS_VEC(xlatemat, delta);
	bn_mat_mul2(xlatemat, modelchanges);

	new_edit_mats();
	update_views = 1;
    }

    return TCL_OK;
}


int
mged_otran(const vect_t tvec)
{
    vect_t work = {0.0, 0.0, 0.0};

    if (STATE == ST_S_EDIT || STATE == ST_O_EDIT) {
	/* apply acc_rot_sol to tvec */
	MAT4X3PNT(work, acc_rot_sol, tvec);
    }

    return mged_mtran(work);
}


int
mged_mtran(const vect_t tvec)
{
    point_t delta;
    point_t vc, nvc;

    VSCALE(delta, tvec, local2base);
    MAT_DELTAS_GET_NEG(vc, view_state->vs_gvp->gv_center);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, nvc);
    new_mats();

    /* calculate absolute_tran */
    set_absolute_view_tran();

    return TCL_OK;
}


int
mged_vtran(const vect_t tvec)
{
    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSCALE(tt, tvec, local2base/view_state->vs_gvp->gv_scale);
    MAT4X3PNT(work, view_state->vs_gvp->gv_view2model, tt);
    MAT_DELTAS_GET_NEG(vc, view_state->vs_gvp->gv_center);
    VSUB2(delta, work, vc);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, nvc);

    new_mats();

    /* calculate absolute_model_tran */
    set_absolute_model_tran();

    return TCL_OK;
}


int
mged_tran(vect_t tvec)
{
    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e')
	return mged_etran(mged_variables->mv_coords, tvec);

    /* apply to View */
    if (mged_variables->mv_coords == 'm')
	return mged_mtran(tvec);

    if (mged_variables->mv_coords == 'o')
	return mged_otran(tvec);

    return mged_vtran(tvec);
}


int
cmd_tra(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord;
	vect_t tvec;

	if (ged_tra_args(gedp, argc, (const char **)argv, &coord, tvec) != GED_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}
	
	return mged_etran(coord, tvec);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_tra(gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != GED_OK)
	    return TCL_ERROR;

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
mged_escale(fastf_t sfactor)
{
    fastf_t old_scale;

    if (-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF)
	return TCL_OK;

    if (STATE == ST_S_EDIT) {
	int save_edflag;

	save_edflag = es_edflag;
	if (!SEDIT_SCALE)
	    es_edflag = SSCALE;

	es_scale = sfactor;
	old_scale = acc_sc_sol;
	acc_sc_sol *= sfactor;

	if (acc_sc_sol < MGED_SMALL_SCALE) {
	    acc_sc_sol = old_scale;
	    es_edflag = save_edflag;
	    return TCL_OK;
	}

	if (acc_sc_sol >= 1.0)
	    edit_absolute_scale = (acc_sc_sol - 1.0) / 3.0;
	else
	    edit_absolute_scale = acc_sc_sol - 1.0;

	sedit();

	es_edflag = save_edflag;
    } else {
	point_t temp;
	point_t pos_model;
	mat_t smat;
	fastf_t inv_sfactor;

	inv_sfactor = 1.0 / sfactor;
	MAT_IDN(smat);

	switch (edobj) {
	    case BE_O_XSCALE:                            /* local scaling ... X-axis */
		smat[0] = sfactor;
		old_scale = acc_sc[X];
		acc_sc[X] *= sfactor;

		if (acc_sc[X] < MGED_SMALL_SCALE) {
		    acc_sc[X] = old_scale;
		    return TCL_OK;
		}
		break;
	    case BE_O_YSCALE:                            /* local scaling ... Y-axis */
		smat[5] = sfactor;
		old_scale = acc_sc[Y];
		acc_sc[Y] *= sfactor;

		if (acc_sc[Y] < MGED_SMALL_SCALE) {
		    acc_sc[Y] = old_scale;
		    return TCL_OK;
		}
		break;
	    case BE_O_ZSCALE:                            /* local scaling ... Z-axis */
		smat[10] = sfactor;
		old_scale = acc_sc[Z];
		acc_sc[Z] *= sfactor;

		if (acc_sc[Z] < MGED_SMALL_SCALE) {
		    acc_sc[Z] = old_scale;
		    return TCL_OK;
		}
		break;
	    case BE_O_SCALE:                             /* global scaling */
	    default:
		smat[15] = inv_sfactor;
		old_scale = acc_sc_sol;
		acc_sc_sol *= inv_sfactor;

		if (acc_sc_sol < MGED_SMALL_SCALE) {
		    acc_sc_sol = old_scale;
		    return TCL_OK;
		}
		break;
	}

	/* Have scaling take place with respect to keypoint,
	 * NOT the view center.
	 */
	VMOVE(temp, es_keypoint);
	MAT4X3PNT(pos_model, modelchanges, temp);
	wrt_point(modelchanges, smat, modelchanges, pos_model);

	new_edit_mats();
    }

    return TCL_OK;
}


int
mged_vscale(fastf_t sfactor)
{
    fastf_t f;

    if (-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF)
	return TCL_OK;

    view_state->vs_gvp->gv_scale *= sfactor;
    if (view_state->vs_gvp->gv_scale < RT_MINVIEWSIZE)
	view_state->vs_gvp->gv_scale = RT_MINVIEWSIZE;
    f = view_state->vs_gvp->gv_scale / view_state->vs_i_Viewscale;

    if (f >= 1.0)
	view_state->vs_absolute_scale = (f - 1.0) / -9.0;
    else
	view_state->vs_absolute_scale = 1.0 - f;

    new_mats();
    return TCL_OK;
}


int
cmd_sca(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) && mged_variables->mv_transform == 'e') {
	fastf_t sf1 = 0.0; /* combined xyz scale or x scale */
	fastf_t sf2 = 0.0; /* y scale */
	fastf_t sf3 = 0.0; /* z scale */
        int save_edobj;
        int ret;

	if (ged_scale_args(gedp, argc, (const char **)argv, &sf1, &sf2, &sf3) != GED_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);
	    return TCL_ERROR;
	}

        /* argc is 2 or 4 because otherwise ged_scale_args fails */
	if (argc == 2) {
	    if (sf1 <= SMALL_FASTF || INFINITY < sf1)
	        return TCL_OK;
	    return mged_escale(sf1);
        } else {
	    if (sf1 <= SMALL_FASTF || INFINITY < sf1)
	        return TCL_OK;
	    if (sf2 <= SMALL_FASTF || INFINITY < sf2)
	        return TCL_OK;
	    if (sf3 <= SMALL_FASTF || INFINITY < sf3)
	        return TCL_OK;
            if (STATE == ST_O_EDIT) {
	        save_edobj = edobj;
	        edobj = BE_O_XSCALE;
	        if ((ret = mged_escale(sf1)) == TCL_OK) {
	            edobj = BE_O_YSCALE;
	            if ((ret = mged_escale(sf2)) == TCL_OK) {
	                edobj = BE_O_ZSCALE;
	                ret = mged_escale(sf3);
	            }
	        }
	        edobj = save_edobj;
	        return ret;
	    } else {
                /* argc was 4 but state was ST_S_EDIT so do nothing */
	        bu_log("Error: Can only scale xyz independently on an object.\n");
	        return TCL_OK;
            }
	}
    } else {
	int ret;
	fastf_t f;

	Tcl_DStringInit(&ds);
	ret = ged_scale(gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != GED_OK)
	    return TCL_ERROR;

	f = view_state->vs_gvp->gv_scale / view_state->vs_i_Viewscale;
	if (f >= 1.0)
	    view_state->vs_absolute_scale = (f - 1.0) / -9.0;
	else
	    view_state->vs_absolute_scale = 1.0 - f;

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


/**
 * C M D _ P O V
 *
 * Process the "pov" command to change the point of view.
 */
int
cmd_pov(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    ret = ged_pov(gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    mged_variables->mv_perspective = view_state->vs_gvp->gv_perspective;

    if (argc > 1)
	view_state->vs_flag = 1;

    return TCL_OK;
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


