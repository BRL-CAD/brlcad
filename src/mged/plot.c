/*                          P L O T . C
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
/** @file mged/plot.c
 *
 * Provide UNIX-plot output of the current view.
 *
 */

#include "common.h"

#include <math.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "dg.h"
#include "plot3.h"

#include "./mged.h"
#include "./mged_dm.h"


int
f_area(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    static vect_t last;
    static vect_t fin;
    char result[RT_MAXLINE] = {0};
    char tol_str[32] = {0};
    int is_empty = 1;

#ifndef _WIN32
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct bn_vlist *vp;
    FILE *fp_r;
    FILE *fp_w;
    int rpid;
    int pid1;
    int pid2;
    int fd1[2]; /* mged | cad_boundp */
    int fd2[2]; /* cad_boundp | cad_parea */
    int fd3[2]; /* cad_parea | mged */
    int retcode;
    const char *tol_ptr;

    /* XXX needs fixing */

    CHECK_DBI_NULL;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help area");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_VIEW, "Presented Area Calculation") == TCL_ERROR)
	return TCL_ERROR;

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
	Tcl_AppendResult(interp, "No objects displayed!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    if (!sp->s_Eflag && sp->s_soldash != 0) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help area");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	    }
	}

	gdlp = next_gdlp;
    }

    if (argc == 2) {
	Tcl_AppendResult(interp, "Tolerance is ", argv[1], "\n", (char *)NULL);
	tol_ptr = argv[1];
    } else {
	struct bu_vls tmp_vls;
	double tol = 0.005;

	bu_vls_init(&tmp_vls);
	sprintf(tol_str, "%e", tol);
	tol_ptr = tol_str;
	bu_vls_printf(&tmp_vls, "Auto-tolerance is %s\n", tol_str);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    if (pipe(fd1) != 0) {
	perror("f_area");
	return TCL_ERROR;
    }

    if (pipe(fd2) != 0) {
	perror("f_area");
	return TCL_ERROR;
    }

    if (pipe(fd3) != 0) {
	perror("f_area");
	return TCL_ERROR;
    }

    if ((pid1 = fork()) == 0) {
	const char *cad_boundp = bu_brlcad_root("bin/cad_boundp", 1);

	dup2(fd1[0], fileno(stdin));
	dup2(fd2[1], fileno(stdout));

	close(fd1[0]);
	close(fd1[1]);
	close(fd2[0]);
	close(fd2[1]);
	close(fd3[0]);
	close(fd3[1]);

	execlp(cad_boundp, cad_boundp, "-t", tol_ptr, (char *)NULL);
    }

    if ((pid2 = fork()) == 0) {
	const char *cad_parea = bu_brlcad_root("bin/cad_parea", 1);

	dup2(fd2[0], fileno(stdin));
	dup2(fd3[1], fileno(stdout));

	close(fd1[0]);
	close(fd1[1]);
	close(fd2[0]);
	close(fd2[1]);
	close(fd3[0]);
	close(fd3[1]);

	execlp(cad_parea, cad_parea, (char *)NULL);
    }

    close(fd1[0]);
    close(fd2[0]);
    close(fd2[1]);
    close(fd3[1]);

    fp_w = fdopen(fd1[1], "w");
    fp_r = fdopen(fd3[0], "r");

    /*
     * Write out rotated but unclipped, untranslated,
     * and unscaled vectors
     */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int i;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		for (i = 0; i < nused; i++, cmd++, pt++) {
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			    continue;
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_LINE_MOVE:
			    /* Move, not draw */
			    MAT4X3VEC(last, view_state->vs_gvp->gv_rotation, *pt);
			    continue;
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			case BN_VLIST_LINE_DRAW:
			    /* draw.  */
			    MAT4X3VEC(fin, view_state->vs_gvp->gv_rotation, *pt);
			    break;
		    }

		    fprintf(fp_w, "%.9e %.9e %.9e %.9e\n",
			    last[X] * base2local,
			    last[Y] * base2local,
			    fin[X] * base2local,
			    fin[Y] * base2local);

		    VMOVE(last, fin);
		}
	    }
	}

	gdlp = next_gdlp;
    }

    fclose(fp_w);

    Tcl_AppendResult(interp, "Presented area from this viewpoint, square ",
		     bu_units_string(dbip->dbi_local2base), ":\n", (char *)NULL);

    /* Read result */
    bu_fgets(result, RT_MAXLINE, fp_r);
    Tcl_AppendResult(interp, result, "\n", (char *)NULL);

    while ((rpid = wait(&retcode)) != pid1 && rpid != -1);
    while ((rpid = wait(&retcode)) != pid2 && rpid != -1);

    fclose(fp_r);
    close(fd1[1]);
    close(fd3[0]);
#endif

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
