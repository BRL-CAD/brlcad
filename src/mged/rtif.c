/*                          R T I F . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2026 United States Government as represented by
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
/** @file mged/rtif.c
 *
 * Routines to interface to RT, and RT-style command files
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <sys/stat.h>
#include "bresource.h"

#include "tcl.h"

#include "vmath.h"
#include "raytrace.h"
#include "ged/view.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"


/* Callback: find the displayed shape for database solid "EYE" for rmats. */
struct _rtif_eye_data {
    struct mged_state *s;
    struct directory *dp;
    Tcl_Interp *interp;
    vect_t sav_start;
    vect_t sav_center;
    ged_draw_shape_ref ref; /* set on match */
    int found;
};

static int
_rtif_shape_last_point(struct mged_state *s, ged_draw_shape_ref ref, point_t out)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(ref))
	return 0;
    return ged_draw_shape_ref_last_point(s->gedp, ref, out);
}

static int
_rtif_shape_translate_geometry(struct mged_state *s, ged_draw_shape_ref ref, const vect_t xlate)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(ref))
	return 0;
    return ged_draw_shape_ref_translate_geometry(s->gedp, ref, xlate);
}

static int
_rtif_shape_set_center(struct mged_state *s, ged_draw_shape_ref ref, const point_t center)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(ref))
	return 0;
    return ged_draw_shape_ref_set_center(s->gedp, ref, center);
}

static int
_rtif_eye_shape_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct _rtif_eye_data *d = (struct _rtif_eye_data *)ud;
    if (!rec || !rec->fullpath || rec->fullpath->fp_len <= 0) return 1;
    if (DB_FULL_PATH_CUR_DIR(rec->fullpath) != d->dp) return 1;
    if (!_rtif_shape_last_point(d->s, rec->ref, d->sav_start)) return 1;
    VMOVE(d->sav_center, rec->center);
    d->ref = rec->ref;
    Tcl_AppendResult(d->interp, "animating EYE solid\n", (char *)NULL);
    d->found = 1;
    return 0; /* stop visiting */
}

static void
_rtif_use_current_view(struct mged_state *s)
{
    if (!s || !s->gedp || !view_state || !view_state->vs_gvp)
	return;

    s->gedp->ged_gvp = view_state->vs_gvp;
    if (s->mged_curr_dm)
	s->gedp->ged_gvp->dmp = (void *)s->mged_curr_dm->dm_dmp;
}

static void
_rtif_request_current_view_refresh(struct mged_state *s)
{
    mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
    if (s)
	mged_dm_repaint_request(s->mged_curr_dm, MGED_REPAINT_INTERACTION);
}

/**
 * rt, rtarea, rtweight, rtcheck, and rtedge all use this.
 */
int
cmd_rt(ClientData clientData,
       Tcl_Interp *interp,
       int argc,
       const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	bu_strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    Tcl_DStringInit(&ds);

    ret = ged_exec(s->gedp, argc, (const char **)argv);

    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * Invoke any program with the current view & stuff, just like
 * an "rt" command (above).
 * Typically used to invoke a remote RT (hence the name).
 */
int
cmd_rrt(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help rrt");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(s, ST_VIEW, "Ray-trace of current view"))
	return TCL_ERROR;

    Tcl_DStringInit(&ds);

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * Read in one view in the old RT format.
 */
static int
rt_read(FILE *fp, fastf_t *scale, fastf_t *eye, fastf_t *mat)
{
    int i;
    double d;

    if (fscanf(fp, "%lf", &d) != 1) return -1;
    *scale = d*0.5;
    if (fscanf(fp, "%lf", &d) != 1) return -1;
    eye[X] = d;
    if (fscanf(fp, "%lf", &d) != 1) return -1;
    eye[Y] = d;
    if (fscanf(fp, "%lf", &d) != 1) return -1;
    eye[Z] = d;
    for (i=0; i < 16; i++) {
	if (fscanf(fp, "%lf", &d) != 1)
	    return -1;
	mat[i] = d;
    }
    return 0;
}


/**
 * Load view matrices from a file.  rmats filename [mode]
 *
 * Modes:
 * -1 put eye in viewcenter (default)
 * 0 put eye in viewcenter, don't rotate.
 * 1 leave view alone, animate solid named "EYE"
 */
int
f_rmats(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    FILE *fp = NULL;
    fastf_t scale = 0.0;
    mat_t rot;
    struct directory *dp = NULL;
    vect_t eye_model = VINIT_ZERO;
    vect_t sav_center = VINIT_ZERO;
    vect_t sav_start = VINIT_ZERO;
    vect_t xlate = VINIT_ZERO;

    /* static due to setjmp */
    static int mode = 0;
    static ged_draw_shape_ref sp_ref;

    CHECK_DBI_NULL;

    if (argc < 2 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help rmats");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(s, ST_VIEW, "animate from matrix file"))
	return TCL_ERROR;

    if ((fp = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return TCL_ERROR;
    }

    sp_ref = GED_DRAW_SHAPE_REF_NULL;

    mode = -1;
    if (argc > 2)
	mode = atoi(argv[2]);
    switch (mode) {
	case 1:
	    if ((dp = db_lookup(s->dbip, "EYE", LOOKUP_NOISY)) == RT_DIR_NULL) {
		mode = -1;
		break;
	    }

	    {
		struct _rtif_eye_data d;
		d.s = s;
		d.dp = dp;
		d.interp = interp;
		VSETALL(d.sav_start, 0.0);
		VSETALL(d.sav_center, 0.0);
		d.ref = GED_DRAW_SHAPE_REF_NULL;
		d.found = 0;
		ged_draw_foreach_shape_record(s->gedp, _rtif_eye_shape_cb, &d);
		if (d.found) {
		    VMOVE(sav_start, d.sav_start);
		    VMOVE(sav_center, d.sav_center);
		    sp_ref = d.ref;
		    goto work;
		}
	    }
	    /* Fall through */
	default:
	case -1:
	    mode = -1;
	    Tcl_AppendResult(interp, "default mode:  eyepoint at (0, 0, 1) viewspace\n", (char *)NULL);
	    break;
	case 0:
	    Tcl_AppendResult(interp, "rotation suppressed, center is eyepoint\n", (char *)NULL);
	    break;
    }
work:
    /* FIXME: this isn't portable or seem well thought-out */
    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    while (!feof(fp) &&
	   rt_read(fp, &scale, eye_model, rot) >= 0) {
	switch (mode) {
	    case -1:
		/* First step:  put eye in center */
		view_state->vs_gvp->gv_scale = scale;
		MAT_COPY(view_state->vs_gvp->gv_rotation, rot);
		MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, eye_model);
		new_mats(s);
		/* Second step:  put eye in front */
		VSET(xlate, 0.0, 0.0, -1.0);	/* correction factor */
		MAT4X3PNT(eye_model, view_state->vs_gvp->gv_view2model, xlate);
		MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, eye_model);
		new_mats(s);
		break;
	    case 0:
		view_state->vs_gvp->gv_scale = scale;
		MAT_IDN(view_state->vs_gvp->gv_rotation);	/* top view */
		MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, eye_model);
		new_mats(s);
		break;
	    case 1:
		/* Adjust center for drawn scene devices */
		_rtif_shape_set_center(s, sp_ref, eye_model);

		/* Adjust vector list for non-dl devices */
		if (!_rtif_shape_last_point(s, sp_ref, xlate)) break;
		VSUB2(xlate, eye_model, xlate);
		_rtif_shape_translate_geometry(s, sp_ref, xlate);
		break;
	}
	mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
	refresh(s);	/* Draw new display */
    }

    if (mode == 1) {
	_rtif_shape_set_center(s, sp_ref, sav_center);
	if (_rtif_shape_last_point(s, sp_ref, xlate)) {
	    VSUB2(xlate, sav_start, xlate);
	    _rtif_shape_translate_geometry(s, sp_ref, xlate);
	}
    }

    fclose(fp);
    (void)mged_svbase(s);

    (void)signal(SIGINT, SIG_IGN);
    return TCL_OK;
}


/**
 * Invoke nirt with the current view & stuff
 */
int
f_nirt(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	bu_strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    Tcl_DStringInit(&ds);
    _rtif_use_current_view(s);

    if (mged_variables->mv_use_air) {
	int insertArgc = 2;
	char *insertArgv[3];
	int newArgc;
	char **newArgv;

	insertArgv[0] = "-u";
	insertArgv[1] = "1";
	insertArgv[2] = (char *)0;
	newArgv = bu_argv_dupinsert(1, insertArgc, (const char **)insertArgv, argc, (const char **)argv);
	newArgc = argc + insertArgc;
	ret = ged_exec(s->gedp, newArgc, (const char **)newArgv);
	bu_argv_free(newArgc, newArgv);
    } else {
	ret = ged_exec(s->gedp, argc, (const char **)argv);
    }

    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_OK) {
	_rtif_request_current_view_refresh(s);
	return TCL_OK;
    }

    return TCL_ERROR;
}


int
f_vnirt(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	bu_strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    Tcl_DStringInit(&ds);
    _rtif_use_current_view(s);

    ret = ged_exec(s->gedp, argc, (const char **)argv);

    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_OK) {
	_rtif_request_current_view_refresh(s);
	return TCL_OK;
    }

    return TCL_ERROR;
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
