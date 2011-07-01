/*                          R T I F . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "dg.h"
#include "mater.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./qray.h"
#include "./cmd.h"


/**
 * C M D _ R T
 *
 * rt, rtarea, rtweight, rtcheck, and rtedge all use this.
 */
int
cmd_rt(ClientData UNUSED(clientData),
       Tcl_Interp *interp,
       int argc,
       const char *argv[])
{
    int doRtcheck;
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    if (BU_STR_EQUAL(argv[0], "rtcheck"))
	doRtcheck = 1;
    else
	doRtcheck = 0;

    Tcl_DStringInit(&ds);

    if (doRtcheck)
	ret = ged_rtcheck(gedp, argc, (const char **)argv);
    else
	ret = ged_rt(gedp, argc, (const char **)argv);

    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == GED_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * C M D _ R R T
 *
 * Invoke any program with the current view & stuff, just like
 * an "rt" command (above).
 * Typically used to invoke a remote RT (hence the name).
 */
int
cmd_rrt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help rrt");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_VIEW, "Ray-trace of current view"))
	return TCL_ERROR;

    Tcl_DStringInit(&ds);

    ret = ged_rrt(gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == GED_OK)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * R T _ R E A D
 *
 * Read in one view in the old RT format.
 */
HIDDEN int
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
 * F _ R M A T S
 *
 * Load view matrixes from a file.  rmats filename [mode]
 *
 * Modes:
 * -1 put eye in viewcenter (default)
 * 0 put eye in viewcenter, don't rotate.
 * 1 leave view alone, animate solid named "EYE"
 */
int
f_rmats(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    FILE *fp = NULL;
    fastf_t scale = 0.0;
    mat_t rot;
    struct bn_vlist *vp = NULL;
    struct directory *dp = NULL;
    struct ged_display_list *gdlp = NULL;
    struct ged_display_list *next_gdlp = NULL;
    vect_t eye_model = {0.0, 0.0, 0.0};
    vect_t sav_center = {0.0, 0.0, 0.0};
    vect_t sav_start = {0.0, 0.0, 0.0};
    vect_t xlate = {0.0, 0.0, 0.0};

    /* static due to setjmp */
    static int mode = 0;
    static struct solid *sp;

    CHECK_DBI_NULL;

    if (argc < 2 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help rmats");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_VIEW, "animate from matrix file"))
	return TCL_ERROR;

    if ((fp = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return TCL_ERROR;
    }

    sp = SOLID_NULL;

    mode = -1;
    if (argc > 2)
	mode = atoi(argv[2]);
    switch (mode) {
	case 1:
	    if ((dp = db_lookup(dbip, "EYE", LOOKUP_NOISY)) == RT_DIR_NULL) {
		mode = -1;
		break;
	    }

	    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
		next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

		FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		    if (LAST_SOLID(sp) != dp) continue;
		    if (BU_LIST_IS_EMPTY(&(sp->s_vlist))) continue;
		    vp = BU_LIST_LAST(bn_vlist, &(sp->s_vlist));
		    VMOVE(sav_start, vp->pt[vp->nused-1]);
		    VMOVE(sav_center, sp->s_center);
		    Tcl_AppendResult(interp, "animating EYE solid\n", (char *)NULL);
		    goto work;
		}

		gdlp = next_gdlp;
	    }
	    /* Fall through */
	default:
	case -1:
	    mode = -1;
	    Tcl_AppendResult(interp, "default mode:  eyepoint at (0, 0, 1) viewspace\n", (char *)NULL);
	    break;
	case 0:
	    Tcl_AppendResult(interp, "rotation supressed, center is eyepoint\n", (char *)NULL);
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
		new_mats();
		/* Second step:  put eye in front */
		VSET(xlate, 0.0, 0.0, -1.0);	/* correction factor */
		MAT4X3PNT(eye_model, view_state->vs_gvp->gv_view2model, xlate);
		MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, eye_model);
		new_mats();
		break;
	    case 0:
		view_state->vs_gvp->gv_scale = scale;
		MAT_IDN(view_state->vs_gvp->gv_rotation);	/* top view */
		MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, eye_model);
		new_mats();
		break;
	    case 1:
		/* Adjust center for displaylist devices */
		VMOVE(sp->s_center, eye_model);

		/* Adjust vector list for non-dl devices */
		if (BU_LIST_IS_EMPTY(&(sp->s_vlist))) break;
		vp = BU_LIST_LAST(bn_vlist, &(sp->s_vlist));
		VSUB2(xlate, eye_model, vp->pt[vp->nused-1]);
		for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		    int i;
		    int nused = vp->nused;
		    int *cmd = vp->cmd;
		    point_t *pt = vp->pt;
		    for (i = 0; i < nused; i++, cmd++, pt++) {
			switch (*cmd) {
			    case BN_VLIST_POLY_START:
			    case BN_VLIST_POLY_VERTNORM:
				break;
			    case BN_VLIST_LINE_MOVE:
			    case BN_VLIST_LINE_DRAW:
			    case BN_VLIST_POLY_MOVE:
			    case BN_VLIST_POLY_DRAW:
			    case BN_VLIST_POLY_END:
				VADD2(*pt, *pt, xlate);
				break;
			}
		    }
		}
		break;
	}
	view_state->vs_flag = 1;
	refresh();	/* Draw new display */
    }

    if (mode == 1) {
	VMOVE(sp->s_center, sav_center);
	if (BU_LIST_NON_EMPTY(&(sp->s_vlist))) {
	    vp = BU_LIST_LAST(bn_vlist, &(sp->s_vlist));
	    VSUB2(xlate, sav_start, vp->pt[vp->nused-1]);
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		for (i = 0; i < nused; i++, cmd++, pt++) {
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			    break;
			case BN_VLIST_LINE_MOVE:
			case BN_VLIST_LINE_DRAW:
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			    VADD2(*pt, *pt, xlate);
			    break;
		    }
		}
	    }
	}
    }

    fclose(fp);
    (void)mged_svbase();

    (void)signal(SIGINT, SIG_IGN);
    return TCL_OK;
}


/**
 * F _ N I R T
 *
 * Invoke nirt with the current view & stuff
 */
int
f_nirt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    Tcl_DStringInit(&ds);

    if (mged_variables->mv_use_air) {
	int insertArgc = 2;
	char *insertArgv[3];
	int newArgc;
	char **newArgv;

	insertArgv[0] = "-u";
	insertArgv[1] = "1";
	insertArgv[2] = (char *)0;
	newArgv = bu_dupinsert_argv(1, insertArgc, (const char **)insertArgv, argc, (const char **)argv);
	newArgc = argc + insertArgc;
	ret = ged_nirt(gedp, newArgc, (const char **)newArgv);
	bu_free_argv(newArgc, newArgv);
    } else {
	ret = ged_nirt(gedp, argc, (const char **)argv);
    }

    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == GED_OK)
	return TCL_OK;

    return TCL_ERROR;
}


int
f_vnirt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    CHECK_DBI_NULL;

    /* skip past _mged_ */
    if (argv[0][0] == '_' && argv[0][1] == 'm' &&
	strncmp(argv[0], "_mged_", 6) == 0)
	argv[0] += 6;

    Tcl_DStringInit(&ds);

    ret = ged_vnirt(gedp, argc, (const char **)argv);

    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == GED_OK)
	return TCL_OK;

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
