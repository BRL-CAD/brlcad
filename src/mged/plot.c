/*                          P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
#include "bio.h"
#include "bresource.h"

#include "bu/app.h"
#include "bu/units.h"
#include "vmath.h"
#include "raytrace.h"
#include "bsg/export.h"
#include "bsg/plot3.h"
#include "bsg/render.h"

#include "./mged.h"
#include "./mged_dm.h"

#if defined(HAVE_FDOPEN) && !defined(HAVE_DECL_FDOPEN)
extern FILE *fdopen(int fd, const char *mode);
#endif

static int
_area_has_unsupported_subtraction(struct mged_state *s)
{
    if (!s || !s->gedp)
	return 0;

    int area_err = 0;
    struct bsg_export_request request;
    bsg_export_request_init(&request, s->gedp->ged_gvp);
    request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY | BSG_EXPORT_QUERY_DB_OBJECTS;
    request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;

    struct bsg_export_result *export_result = bsg_export_query(&request);
    if (!export_result)
	return 0;

    for (size_t i = 0; i < bsg_export_result_count(export_result); i++) {
	const struct bsg_export_record *rec =
	    bsg_export_result_get(export_result, i);
	if (rec && !rec->evaluated_region && rec->line_style != 0) {
	    area_err = 1;
	    break;
	}
    }
    bsg_export_result_free(export_result);
    return area_err;
}

/* Callback: write shape vlists to cad_boundp pipe. */
struct _area_write_data {
    FILE *fp_w;
    const mat_t *rotation;
    struct db_i *dbip;
};

static int
_area_write_segment(const point_t a, const point_t b, void *data)
{
    struct _area_write_data *d = (struct _area_write_data *)data;
    vect_t last;
    vect_t fin;

    if (!d || !d->fp_w || !d->rotation || !d->dbip)
	return 0;

    MAT4X3VEC(last, *d->rotation, a);
    MAT4X3VEC(fin, *d->rotation, b);
    fprintf(d->fp_w, "%.9e %.9e %.9e %.9e\n",
	    last[X] * d->dbip->dbi_base2local,
	    last[Y] * d->dbip->dbi_base2local,
	    fin[X]  * d->dbip->dbi_base2local,
	    fin[Y]  * d->dbip->dbi_base2local);
    return 1;
}

static void
_area_write_record(const struct bsg_export_record *rec, struct _area_write_data *d)
{
    (void)bsg_export_record_foreach_segment(rec, _area_write_segment, d);
}

/* ------------------------------------------------------------------ */

int
f_area(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    char result[RT_MAXLINE] = {0};
    char tol_str[32] = {0};

#ifndef _WIN32
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
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help area");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(s, ST_VIEW, "Presented Area Calculation") == TCL_ERROR)
	return TCL_ERROR;

    if (!ged_draw_has_shapes(s->gedp)) {
	Tcl_AppendResult(interp, "No objects displayed!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    {
	if (_area_has_unsupported_subtraction(s)) {
	    struct bu_vls vls = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vls, "help area");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
    }

    if (argc == 2) {
	Tcl_AppendResult(interp, "Tolerance is ", argv[1], "\n", (char *)NULL);
	tol_ptr = argv[1];
    } else {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	double tol = 0.0005;

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
	const char *cad_boundp = bu_dir(NULL, 0, BU_DIR_BIN, "cad_boundp", BU_DIR_EXT, NULL);

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
	const char *cad_parea = bu_dir(NULL, 0, BU_DIR_BIN, "cad_parea", BU_DIR_EXT, NULL);

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
    {
	struct _area_write_data wd;
	wd.fp_w = fp_w;
	wd.rotation = (const mat_t *)&view_state->vs_gvp->gv_rotation;
	wd.dbip = s->dbip;
	struct bsg_export_request request;
	bsg_export_request_init(&request, s->gedp->ged_gvp);
	request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY | BSG_EXPORT_QUERY_DB_OBJECTS;
	request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
	struct bsg_export_result *export_result = bsg_export_query(&request);
	if (export_result) {
	    for (size_t i = 0; i < bsg_export_result_count(export_result); i++)
		_area_write_record(bsg_export_result_get(export_result, i), &wd);
	    bsg_export_result_free(export_result);
	}
    }

    fclose(fp_w);

    Tcl_AppendResult(interp, "Presented area from this viewpoint, square ",
		     bu_units_string(s->dbip->dbi_local2base), ":\n", (char *)NULL);

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
