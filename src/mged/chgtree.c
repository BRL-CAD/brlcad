/*                       C H G T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2021 United States Government as represented by
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
/** @file mged/chgtree.c
 *
 * This module contains functions which change the tree structure
 * of the model, and delete solids or combinations or combination elements.
 *
 */

#include "common.h"

#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "tcl.h"

#include "vmath.h"
#include "bn.h"
#include "wdb.h"
#include "rt/geom.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"


/**
 * Copy a cylinder and position at end of original cylinder
 * Used in making "wires"
 *
 * Format: cpi old new
 */
int
f_copy_inv(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct directory *proto;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_tgc_internal *tgc_ip;
    int id;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help cpi");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((proto = db_lookup(DBIP,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (db_lookup(DBIP,  argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	aexists(argv[2]);
	return TCL_ERROR;
    }

    if ((id = rt_db_get_internal(&internal, proto, DBIP, (fastf_t *)NULL, &rt_uniresource)) < 0) {
	TCL_READ_ERR_return;
    }
    /* make sure it is a TGC */
    if (id != ID_TGC) {
	Tcl_AppendResult(interp, "f_copy_inv: ", argv[1],
			 " is not a cylinder\n", (char *)NULL);
	rt_db_free_internal(&internal);
	return TCL_ERROR;
    }
    tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

    /* translate to end of "original" cylinder */
    VADD2(tgc_ip->v, tgc_ip->v, tgc_ip->h);

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    if ((dp = db_diradd(DBIP, argv[2], -1L, 0, proto->d_flags, &proto->d_minor_type)) == RT_DIR_NULL) {
	TCL_ALLOC_ERR_return;
    }

    if (rt_db_put_internal(dp, DBIP, &internal, &rt_uniresource) < 0) {
	TCL_WRITE_ERR_return;
    }

    {
	const char *av[3];

	av[0] = "e";
	av[1] = argv[2]; /* depends on solid name being in argv[2] */
	av[2] = NULL;

	/* draw the new solid */
	(void)cmd_draw(clientData, interp, 2, av);
    }

    if (STATE == ST_VIEW) {
	const char *av[3];

	av[0] = "sed";
	av[1] = argv[2];  /* new name in argv[2] */
	av[2] = NULL;

	/* solid edit this new cylinder */
	(void)f_sed(clientData, interp, 2, av);
    }

    return TCL_OK;
}


struct bv_scene_obj *
find_solid_with_path(struct db_full_path *pathp)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    int count = 0;
    struct bv_scene_obj *ret = (struct bv_scene_obj *)NULL;

    RT_CK_FULL_PATH(pathp);

    gdlp = BU_LIST_NEXT(display_list, GEDP->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, GEDP->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    if (!db_identical_full_paths(pathp, &bdata->s_fullpath)) continue;

	    /* Paths are the same */
	    illum_gdlp = gdlp;
	    ret = sp;
	    count++;
	}

	gdlp = next_gdlp;
    }

    if (count > 1) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "find_solid_with_path() found %d matches\n", count);
	Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    return ret;
}


/**
 * Transition from VIEW state to OBJECT EDIT state in a single
 * command, rather than requiring "press oill", "ill leaf", "matpick
 * a/b".
 *
 * Takes two parameters which when combined represent the full path to
 * the reference solid of the object to be edited.  The conceptual
 * slash between the two strings signifies which matrix is to be
 * edited.
 */
int
cmd_oed(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct db_full_path lhs;
    struct db_full_path rhs;
    struct db_full_path both;
    const char *new_argv[4];
    char number[32];
    int is_empty = 1;

    CHECK_DBI_NULL;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help oed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_VIEW, "Object Illuminate")) {
	return TCL_ERROR;
    }

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(display_list, GEDP->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, GEDP->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
	    is_empty = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    if (is_empty) {
	Tcl_AppendResult(interp, "no solids in view", (char *)NULL);
	return TCL_ERROR;
    }

    if (db_string_to_path(&lhs, DBIP, argv[1]) < 0) {
	Tcl_AppendResult(interp, "bad lhs path", (char *)NULL);
	return TCL_ERROR;
    }
    if (db_string_to_path(&rhs, DBIP, argv[2]) < 0) {
	db_free_full_path(&lhs);
	Tcl_AppendResult(interp, "bad rhs path", (char *)NULL);
	return TCL_ERROR;
    }
    if (rhs.fp_len <= 0) {
	db_free_full_path(&lhs);
	db_free_full_path(&rhs);
	Tcl_AppendResult(interp, "rhs must not be null", (char *)NULL);
	return TCL_ERROR;
    }

    db_full_path_init(&both);
    db_dup_full_path(&both, &lhs);
    db_append_full_path(&both, &rhs);

    /* Patterned after ill_common() ... */
    illum_gdlp = gdlp;
    illump = BU_LIST_NEXT(bv_scene_obj, &gdlp->dl_head_scene_obj);/* any valid solid would do */
    edobj = 0;		/* sanity */
    movedir = 0;		/* No edit modes set */
    MAT_IDN(modelchanges);	/* No changes yet */
    (void)chg_state(ST_VIEW, ST_O_PICK, "internal change of state");
    /* reset accumulation local scale factors */
    acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;
    new_mats();

    /* Find the one solid, set s_iflag UP, point illump at it */
    illump = find_solid_with_path(&both);
    if (!illump) {
	db_free_full_path(&lhs);
	db_free_full_path(&rhs);
	db_free_full_path(&both);
	Tcl_AppendResult(interp, "Unable to find solid matching path", (char *)NULL);
	illum_gdlp = GED_DISPLAY_LIST_NULL;
	illump = 0;
	(void)chg_state(ST_O_PICK, ST_VIEW, "error recovery");
	return TCL_ERROR;
    }
    (void)chg_state(ST_O_PICK, ST_O_PATH, "internal change of state");

    /* Select the matrix */
    sprintf(number, "%lu", (long unsigned)lhs.fp_len);
    new_argv[0] = "matpick";
    new_argv[1] = number;
    new_argv[2] = NULL;
    if (f_matpick(clientData, interp, 2, new_argv) != TCL_OK) {
	db_free_full_path(&lhs);
	db_free_full_path(&rhs);
	db_free_full_path(&both);
	Tcl_AppendResult(interp, "error detected inside f_matpick", (char *)NULL);
	return TCL_ERROR;
    }
    if (not_state(ST_O_EDIT, "Object EDIT")) {
	db_free_full_path(&lhs);
	db_free_full_path(&rhs);
	db_free_full_path(&both);
	Tcl_AppendResult(interp, "MGED state did not advance to Object EDIT", (char *)NULL);
	return TCL_ERROR;
    }
    db_free_full_path(&lhs);
    db_free_full_path(&rhs);
    db_free_full_path(&both);
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
