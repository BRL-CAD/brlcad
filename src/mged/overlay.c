/*                       O V E R L A Y . C
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
/** @file mged/overlay.c
 *
 */

#include "common.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "mater.h"
#include "dg.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* Usage:  overlay file.pl [name] */
int
cmd_overlay(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;
    int ac;
    const char *av[5];
    struct bu_vls char_size;

    if (gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    if (argc == 1) {
	Tcl_DStringAppend(&ds, "file.pl [name]", -1);
	Tcl_DStringResult(interp, &ds);
	return TCL_OK;
    }

    ac = argc + 1;
    bu_vls_init(&char_size);
    bu_vls_printf(&char_size, "%lf", view_state->vs_gvp->gv_scale * 0.01);
    av[0] = argv[0];		/* command name */
    av[1] = argv[1];		/* plotfile name */
    av[2] = bu_vls_addr(&char_size);
    if (argc == 3) {
	av[3] = argv[2];	/* name */
	av[4] = (char *)0;
    } else
	av[3] = (char *)0;

    ret = ged_overlay(gedp, ac, (const char **)av);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    update_views = 1;

    bu_vls_free(&char_size);
    return ret;
}


/* Usage:  labelvert solid(s) */
int
f_labelvert(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    int i;
    struct bn_vlblock*vbp;
    struct directory *dp;
    mat_t mat;
    fastf_t scale;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help labelvert");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;		/* divide by # chars/screen */

    for (i=1; i<argc; i++) {
	struct solid *s;
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	/* Find uses of this solid in the solid table */
	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(s, &gdlp->gdl_headSolid) {
		if (db_full_path_search(&s->s_fullpath, dp)) {
		    rt_label_vlist_verts(vbp, &s->s_vlist, mat, scale, base2local);
		}
	    }

	    gdlp = next_gdlp;
	}
    }

    cvt_vlblock_to_solids(vbp, "_LABELVERT_", 0);

    rt_vlblock_free(vbp);
    update_views = 1;
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
