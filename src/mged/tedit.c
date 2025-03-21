/*                         T E D I T . C
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
/** @file mged/tedit.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bu/app.h"
#include "bu/env.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

#define V3BASE2LOCAL(_pt)	(_pt)[X]*s->dbip->dbi_base2local, (_pt)[Y]*s->dbip->dbi_base2local, (_pt)[Z]*s->dbip->dbi_base2local

static char tmpfil[MAXPATHLEN] = {0};

int writesolid(struct mged_state *), readsolid(struct mged_state *);

/*
 *
 * No-frills edit - opens an editor on the supplied
 * file name.
 *
 */
int
editit(struct mged_state *s, const char *tempfile) {
    int argc = 3;
    const char *av[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

    CHECK_DBI_NULL;

    if (!ged_set_editor(s->gedp, s->classic_mged))
	return TCL_ERROR;

    av[0] = "editit";
    av[1] = "-f";
    av[2] = tempfile;
    av[3] = NULL;

    ged_exec(s->gedp, argc, (const char **)av);

    ged_clear_editor(s->gedp);
    return TCL_OK;
}

int
f_tedit(ClientData clientData, Tcl_Interp *interp, int argc, const char **UNUSED(argv))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    FILE *fp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help ted");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Only do this if in solid edit state */
    if (not_state(s, ST_S_EDIT, "Primitive Text Edit"))
	return TCL_ERROR;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (fp == NULL)
	return TCL_ERROR;

    if (writesolid(s)) {
	bu_file_delete(tmpfil);
	fclose(fp);
	return TCL_ERROR;
    }

    (void)fclose(fp);

    if (editit(s, tmpfil) == TCL_OK) {
	if (readsolid(s)) {
	    bu_file_delete(tmpfil);
	    return TCL_ERROR;
	}

	/* Update the display */
	replot_editing_solid(0, NULL, s, NULL);
	view_state->vs_flag = 1;
	Tcl_AppendResult(interp, "done\n", (char *)NULL);
    }

    bu_file_delete(tmpfil);

    return TCL_OK;
}


/* Write numerical parameters of a solid into a file */
int
writesolid(struct mged_state *s)
{
    FILE *fp;

    CHECK_DBI_NULL;

    struct rt_db_internal *ip = &s->s_edit->es_int;
    if (!EDOBJ[ip->idb_type].ft_write_params) {
	Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	return 1;
    }

    struct bu_vls params = BU_VLS_INIT_ZERO;
    (*EDOBJ[ip->idb_type].ft_write_params)(&params, ip, &s->tol.tol, s->dbip->dbi_base2local);
    fp = fopen(tmpfil, "w");
    fprintf(fp, "%s", bu_vls_cstr(&params));
    (void)fclose(fp);
    return 0;
}

/* Read numerical parameters of solid from file */
int
readsolid(struct mged_state *s)
{
    CHECK_DBI_NULL;

    struct rt_db_internal *ip = &s->s_edit->es_int;

    if (!EDOBJ[ip->idb_type].ft_read_params) {
	Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	return 1;
    }

    struct bu_vls solid_in = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mf = bu_open_mapped_file(tmpfil, (char *)NULL);
    if (!mf) {
	bu_log("cannot read temporary file \"%s\"\n", tmpfil);
	return 1;	/* FAIL */
    }
    bu_vls_strncpy(&solid_in, (char *)mf->buf, mf->buflen);
    bu_close_mapped_file(mf);

    if ((*EDOBJ[ip->idb_type].ft_read_params)(ip, bu_vls_cstr(&solid_in), &s->tol.tol, s->dbip->dbi_local2base) == BRLCAD_ERROR) {
	bu_vls_free(&solid_in);
	return 1;   /* FAIL */
    }

    bu_vls_free(&solid_in);
    return 0;
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
