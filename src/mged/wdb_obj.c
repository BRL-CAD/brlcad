/*                       W D B _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libged */
/** @{ */
/** @file libged/wdb_obj.c
 *
 * A quasi-object-oriented database interface.
 *
 * A database object contains the attributes and methods for
 * controlling a BRL-CAD database.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "tcl.h"


#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/units.h"
#include "bn.h"
#include "vmath.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "ged.h"
#include "wdb.h"
#include "raytrace.h"
#include "tclcad.h"


/* A verbose message to attempt to soothe and advise the user */
#define WDB_TCL_ERROR_RECOVERY_SUGGESTION\
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit now, and resolve the I/O problem, before continuing.\n", (char *)NULL)


#define WDB_TCL_WRITE_ERR { \
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Database write error, aborting.\n", (char *)NULL); \
	WDB_TCL_ERROR_RECOVERY_SUGGESTION; }

#define WDB_TCL_WRITE_ERR_return { \
	WDB_TCL_WRITE_ERR; \
	return TCL_ERROR; }


#define WDB_TCL_CHECK_READ_ONLY \
    if ((Tcl_Interp *)wdbp->wdb_interp) { \
	if (wdbp->dbip->dbi_read_only) { \
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Sorry, this database is READ-ONLY\n", (char *)NULL); \
	    return TCL_ERROR; \
	} \
    } else { \
	bu_log("Sorry, this database is READ-ONLY\n"); \
    }


int
wdb_decode_dbip(const char *dbip_string, struct db_i **dbipp)
{
    if (sscanf(dbip_string, "%p", (void **)dbipp) != 1) {
	return BRLCAD_ERROR;
    }

    /* Could core dump */
    RT_CK_DBI(*dbipp);

    return TCL_OK;
}


/**
 * @brief
 * Open/Create the database and build the in memory directory.
 */
struct db_i *
wdb_prep_dbip(const char *filename)
{
    struct db_i *dbip;

    /* open database */
    if (((dbip = db_open(filename, DB_OPEN_READWRITE)) == DBI_NULL) &&
	    ((dbip = db_open(filename, DB_OPEN_READONLY)) == DBI_NULL)) {

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(filename, NULL) && !bu_file_readable(filename)) {
	    bu_log("wdb_prep_dbip: %s is not readable\n", filename);

	    return DBI_NULL;
	}

	/* db_create does a db_dirbuild */
	if ((dbip = db_create(filename, 5)) == DBI_NULL) {
	    bu_log("wdb_prep_dbip: failed to create %s\n", filename);

	    if (dbip == DBI_NULL)
		bu_log("wdb_prep_dbip: no database is currently opened!");

	    return DBI_NULL;
	}
    } else
	/* --- Scan geometry database and build in-memory directory --- */
	db_dirbuild(dbip);


    return dbip;
}

int
wdb_stub_cmd(struct rt_wdb *wdbp,
	int argc,
	const char *argv[])
{
    if (argc != 1) {
	struct bu_vls vls;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_%s %s", argv[0], argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "%s: no database is currently opened!", argv[0], (char *)NULL);
    return TCL_ERROR;
}


/**
 * Stub command callback for commands that only exist after a database
 * is opened (e.g., db).
 *
 * @returns false
 */
static int
wdb_stub_tcl(void *clientData,
	int argc,
	const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_stub_cmd(wdbp, argc-1, argv+1);
}

static void
wdb_find_ref(struct db_i *UNUSED(dbip),
	     struct rt_comb_internal *UNUSED(comb),
	     union tree *comb_leaf,
	     void *object,
	     void *comb_name_ptr,
	     void *user_ptr3,
	     void *UNUSED(user_ptr4))
{
    char *obj_name;
    char *comb_name;
    Tcl_Interp *interp = (Tcl_Interp *)user_ptr3;

    RT_CK_TREE(comb_leaf);

    obj_name = (char *)object;
    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj_name))
	return;

    comb_name = (char *)comb_name_ptr;

    Tcl_AppendElement(interp, comb_name);
}

int
wdb_find_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;
    struct bu_vls vls;
    int c;
    int aflag = 0;		/* look at all objects */

    if (argc < 2) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_find %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "a")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "Unrecognized option - %c", c);
		Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}
    }
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_COMB) ||
		(!aflag && (dp->d_flags & RT_DIR_HIDDEN)))
		continue;

	    if (rt_db_get_internal(&intern,
				   dp,
				   wdbp->dbip,
				   (fastf_t *)NULL,
				   &rt_uniresource) < 0) {
		Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    for (k = 1; k < argc; k++)
		db_tree_funcleaf(wdbp->dbip,
				 comb,
				 comb->tree,
				 wdb_find_ref,
				 (void *)argv[k],
				 (void *)dp->d_namep,
				 (void *)wdbp->wdb_interp,
				 (void *)NULL);

	    rt_db_free_internal(&intern);
	}
    }

    return TCL_OK;
}

/**
 * Usage:
 * procname find object(s)
 */
static int
wdb_find_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_find_cmd(wdbp, argc-1, argv+1);
}

/**
 * Usage:
 * importFg4Section name sdata
 */
static int
wdb_importFg4Section_tcl(void *clientData,
			 int argc,
			 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_importFg4Section_cmd(wdbp, argc-1, argv+1);
}

int
wdb_make_bb_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    int i;
    point_t rpp_min, rpp_max;
    struct directory *dp;
    struct rt_arb_internal *arb;
    struct rt_db_internal new_intern;
    const char *new_name;
    int use_air = 0;
    struct ged ged;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_make_bb %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /*XXX Temporary */
    GED_INIT(&ged, wdbp);

    i = 1;

    /* look for a USEAIR option */
    if (BU_STR_EQUAL(argv[i], "-u")) {
	use_air = 1;
	i++;
    }

    /* Since arguments may be paths, make sure first argument isn't */
    if (strchr(argv[i], '/')) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Do not use '/' in solid names: ", argv[i], "\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    new_name = argv[i++];
    if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, new_name, " already exists\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    if (ged_get_obj_bounds(&ged, argc-2, (const char **)argv+2, use_air, rpp_min, rpp_max) == TCL_ERROR) {
	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    /* release any allocated memory */
    ged_free(&ged);

    /* build bounding RPP */
    BU_ALLOC(arb, struct rt_arb_internal);
    VMOVE(arb->pt[0], rpp_min);
    VSET(arb->pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
    VSET(arb->pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
    VSET(arb->pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
    VSET(arb->pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
    VSET(arb->pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
    VMOVE(arb->pt[6], rpp_max);
    VSET(arb->pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* set up internal structure */
    RT_DB_INTERNAL_INIT(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type = ID_ARB8;
    new_intern.idb_meth = &OBJ[ID_ARB8];
    new_intern.idb_ptr = (void *)arb;

    dp = db_diradd(wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &new_intern, wdbp->wdb_resp) < 0) {
	rt_db_free_internal(&new_intern);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Database write error, aborting.\n", (char *)NULL);

	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @brief
 * Build an RPP bounding box for the list of objects
 * and/or paths passed to this routine
 *
 * Usage:
 * dbobjname make_bb bbname obj(s)
 */
static int
wdb_make_bb_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_make_bb_cmd(wdbp, argc-1, argv+1);
}

int
wdb_move_arb_edge_cmd(struct rt_wdb *wdbp,
		      int argc,
		      const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    plane_t planes[6];  /* ARBs defining plane equations */
    int arb_type;
    int edge;
    int bad_edge_id = 0;
    point_t pt;
    double scan[3];
    struct bu_vls error_msg;

    if (argc != 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_edge %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (tclcad_rt_import_from_path((Tcl_Interp *)wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%d", &edge) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad edge - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }
    edge -= 1;

    if (sscanf(argv[3], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }
    /* convert double to fastf_t */
    VMOVE(pt, scan);

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    /* check the arb type */
    switch (arb_type) {
	case ARB4:
	    if (edge < 0 || 4 < edge)
		bad_edge_id = 1;
	    break;
	case ARB5:
	    if (edge < 0 || 8 < edge)
		bad_edge_id = 1;
	    break;
	case ARB6:
	    if (edge < 0 || 9 < edge)
		bad_edge_id = 1;
	    break;
	case ARB7:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    break;
	case ARB8:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    break;
	default:
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "unrecognized arb type", (char *)NULL);
	    rt_db_free_internal(&intern);

	    return TCL_ERROR;
    }

    /* check the edge id */
    if (bad_edge_id) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad edge - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }

    if (rt_arb_edit(&error_msg, arb, arb_type, edge, pt, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    {
	int i;
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Move an arb's edge so that it intersects the given point. The new
 * vertices are returned via interp result.
 *
 * Usage:
 * procname move_arb_face arb face pt
 */
static int
wdb_move_arb_edge_tcl(void *clientData,
		      int argc,
		      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_arb_edge_cmd(wdbp, argc-1, argv+1);
}


int
wdb_move_arb_face_cmd(struct rt_wdb *wdbp,
		      int argc,
		      const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    struct bu_vls error_msg;

    /* intentionally double for scan */
    double pt[3];

    if (argc != 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_face %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (tclcad_rt_import_from_path((Tcl_Interp *)wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* change D of planar equation */
    planes[face][3] = VDOT(&planes[face][0], pt);

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)planes, &wdbp->wdb_tol);

    {
	int i;
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Move an arb's face so that its plane intersects the given
 * point. The new vertices are returned via interp result.
 *
 * Usage:
 * procname move_arb_face arb face pt
 */
static int
wdb_move_arb_face_tcl(void *clientData,
		      int argc,
		      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_arb_face_cmd(wdbp, argc-1, argv+1);
}

int
wdb_nmg_collapse_cmd(struct rt_wdb *wdbp,
		     int argc,
		     const char *argv[])
{
    const char *new_name;
    struct model *m;
    struct rt_db_internal intern;
    struct directory *dp;
    struct bu_ptbl faces;
    struct face *fp;
    long count;
    char count_str[32];
    fastf_t tol_coll;
    fastf_t min_angle;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_nmg_collapse %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (strchr(argv[2], '/')) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Do not use '/' in solid names: ", argv[2], "\n", (char *)NULL);
	return TCL_ERROR;
    }

    new_name = argv[2];

    if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, new_name, " already exists\n", (char *)NULL);
	return TCL_ERROR;
    }

    dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY);
    if (dp == RT_DIR_NULL)
	return TCL_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, argv[1], " is a combination, only NMG primitives are allowed here\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Failed to get internal form of ", argv[1], "!!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (intern.idb_type != ID_NMG) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, argv[1], " is not an NMG solid!!!!\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    tol_coll = atof(argv[3]) * wdbp->dbip->dbi_local2base;
    if (tol_coll <= 0.0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "tolerance distance too small\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 5) {
	min_angle = atof(argv[4]);
	if (min_angle < 0.0) {
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Minimum angle cannot be less than zero\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else
	min_angle = 0.0;

    m = (struct model *)intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* check that all faces are planar */
    nmg_face_tabulate(&faces, &m->magic, &RTG.rtg_vlfree);
    for (BU_PTBL_FOR (fp, (struct face *), &faces)) {
	if (fp->g.magic_p != NULL && *(fp->g.magic_p) != NMG_FACE_G_PLANE_MAGIC) {
	    bu_ptbl_free(&faces);
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, argv[0], " cannot be applied to \"", argv[1],
			     "\" because it has non-planar faces\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    bu_ptbl_free(&faces);

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &wdbp->wdb_tol);

    count = nmg_edge_collapse(m, &wdbp->wdb_tol, tol_coll, min_angle, &RTG.rtg_vlfree);

    dp = db_diradd(wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	WDB_TCL_WRITE_ERR_return;
    }

    rt_db_free_internal(&intern);

    sprintf(count_str, "%ld", count);
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, count_str, " edges collapsed\n", (char *)NULL);

    return TCL_OK;
}


/**
 * Usage:
 * procname nmg_collapse nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]
 */
static int
wdb_nmg_collapse_tcl(void *clientData,
		     int argc,
		     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_nmg_collapse_cmd(wdbp, argc-1, argv+1);
}

int
wdb_observer_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    if (argc < 2) {
	bu_log("ERROR: expecting two or more arguments\n");
	return TCL_ERROR;
    }

    return bu_observer_cmd((ClientData)&wdbp->wdb_observers, argc-1, (const char **)argv+1);
}


/**
 * @brief
 * Attach/detach observers to/from list.
 *
 * Usage:
 * procname observer cmd [args]
 *
 */
static int
wdb_observer_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_observer_cmd(wdbp, argc-1, argv+1);
}

int
wdb_reopen_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct db_i *dbip;
    struct bu_vls vls;

    /* get database filename */
    if (argc == 1) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, wdbp->dbip->dbi_filename, (char *)NULL);
	return TCL_OK;
    }

    /* set database filename */
    if (argc == 2) {
	if ((dbip = wdb_prep_dbip(argv[1])) == DBI_NULL) {
	    return TCL_ERROR;
	}

	/* close current database */
	db_close(wdbp->dbip);

	wdbp->dbip = dbip;

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, wdbp->dbip->dbi_filename, (char *)NULL);
	return TCL_OK;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias wdb_reopen %s", argv[0]);
    Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/**
 *
 * @par Usage:
 * procname open [filename]
 */
static int
wdb_reopen_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_reopen_cmd(wdbp, argc-1, argv+1);
}


struct wdb_id_names {
    struct bu_list l;
    struct bu_vls name;		/**< name associated with region id */
};


struct wdb_id_to_names {
    struct bu_list l;
    int id;				/**< starting id (i.e. region id or air code) */
    struct wdb_id_names headName;	/**< head of list of names */
};

int
wdb_rmap_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct wdb_id_to_names headIdName;
    struct wdb_id_to_names *itnp;
    struct wdb_id_names *inp;
    struct bu_vls vls;

    if (argc != 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_rmap %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (db_version(wdbp->dbip) < 5) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s is not available prior to version 5 of the .g file format\n", argv[0]);
	Tcl_SetResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    BU_LIST_INIT(&headIdName.l);

    /* For all regions not hidden */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    int found = 0;

	    if (!(dp->d_flags & RT_DIR_REGION) ||
		(dp->d_flags & RT_DIR_HIDDEN))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_init(&vls);
		bu_vls_strcat(&vls, "Database read error, aborting");
		Tcl_SetResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
		bu_vls_free(&vls);
		return TCL_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    for (BU_LIST_FOR (itnp, wdb_id_to_names, &headIdName.l)) {
		if ((comb->region_id == itnp->id) ||
		    (comb->aircode != 0 && -comb->aircode == itnp->id)) {
		    /* add region name to our name list for this region */
		    BU_ALLOC(inp, struct wdb_id_names);
		    bu_vls_init(&inp->name);
		    bu_vls_strcpy(&inp->name, dp->d_namep);
		    BU_LIST_INSERT(&itnp->headName.l, &inp->l);
		    found = 1;
		    break;
		}
	    }

	    if (!found) {
		/* create new id_to_names node */
		BU_ALLOC(itnp, struct wdb_id_to_names);
		if (0 < comb->region_id)
		    itnp->id = comb->region_id;
		else
		    itnp->id = -comb->aircode;
		BU_LIST_INSERT(&headIdName.l, &itnp->l);
		BU_LIST_INIT(&itnp->headName.l);

		/* add region name to our name list for this region */
		BU_ALLOC(inp, struct wdb_id_names);
		bu_vls_init(&inp->name);
		bu_vls_strcpy(&inp->name, dp->d_namep);
		BU_LIST_INSERT(&itnp->headName.l, &inp->l);
	    }

	    rt_db_free_internal(&intern);
	}
    }

    bu_vls_init(&vls);

    /* place data in a dynamic tcl string */
    while (BU_LIST_WHILE (itnp, wdb_id_to_names, &headIdName.l)) {
	/* add this id to the list */
	bu_vls_printf(&vls, "%d {", itnp->id);

	/* start sublist of names associated with this id */
	while (BU_LIST_WHILE (inp, wdb_id_names, &itnp->headName.l)) {
	    /* add the this name to this sublist */
	    if (strchr(bu_vls_addr(&inp->name), ' ')) {
		bu_vls_printf(&vls, "\"%s\" ", bu_vls_addr(&inp->name));
	    } else {
		bu_vls_printf(&vls, "%s ", bu_vls_addr(&inp->name));
	    }

	    BU_LIST_DEQUEUE(&inp->l);
	    bu_vls_free(&inp->name);
	    bu_free((void *)inp, "rmap: inp");
	}
	bu_vls_strcat(&vls, "} ");

	BU_LIST_DEQUEUE(&itnp->l);
	bu_free((void *)itnp, "rmap: itnp");

    }
    bu_vls_trimspace(&vls);

    Tcl_SetResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname rmap
 */
static int
wdb_rmap_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rmap_cmd(wdbp, argc-1, argv+1);
}

static short int rt_arb_vertices[5][24] = {
    { 1, 2, 3, 0, 1, 2, 4, 0, 2, 3, 4, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* arb4 */
    { 1, 2, 3, 4, 1, 2, 5, 0, 2, 3, 5, 0, 3, 4, 5, 0, 1, 4, 5, 0, 0, 0, 0, 0 },	/* arb5 */
    { 1, 2, 3, 4, 2, 3, 6, 5, 1, 5, 6, 4, 1, 2, 5, 0, 3, 4, 6, 0, 0, 0, 0, 0 },	/* arb6 */
    { 1, 2, 3, 4, 5, 6, 7, 0, 1, 4, 5, 0, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 5 },	/* arb7 */
    { 1, 2, 3, 4, 5, 6, 7, 8, 1, 5, 8, 4, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 8 }	/* arb8 */
};

int
wdb_rotate_arb_face_cmd(struct rt_wdb *wdbp,
			int argc,
			const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    int vi;
    int i;
    int pnt5;		/* special arb7 case */
    struct bu_vls error_msg;

    /* intentionally double for scan */
    double pt[3];

    if (argc != 5) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_face %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (tclcad_rt_import_from_path((Tcl_Interp *)wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%d", &vi) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad vertex index - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }


    /*XXX need better checking of the vertex index */
    vi -= 1;
    if (vi < 0 || 7 < vi) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad vertex - %s", argv[2]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[4], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* check if point 5 is in the face */
    pnt5 = 0;
    for (i = 0; i < 4; i++) {
	if (rt_arb_vertices[arb_type-4][face*4+i]==5)
	    pnt5=1;
    }

    /* special case for arb7 */
    if (arb_type == ARB7  && pnt5)
	vi = 4;

    {
	/* Apply incremental changes */
	vect_t tempvec;
	vect_t work;
	fastf_t *plane;
	mat_t rmat;

	bn_mat_angles(rmat, pt[X], pt[Y], pt[Z]);

	plane = &planes[face][0];
	VMOVE(work, plane);
	MAT4X3VEC(plane, rmat, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[vi]);

	/* set D of planar equation to anchor at fixed vertex */
	planes[face][3]=VDOT(plane, tempvec);
    }

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)planes, &wdbp->wdb_tol);

    {
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Rotate an arb's face to the given point. The new vertices are
 * returned via interp result.
 *
 * Usage:
 * procname rotate_arb_face arb face pt
 */
static int
wdb_rotate_arb_face_tcl(void *clientData,
			int argc,
			const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rotate_arb_face_cmd(wdbp, argc-1, argv+1);
}


/**
 *@brief
 * Called when the named proc created by rt_gettrees() is destroyed.
 */
static void
wdb_deleteProc_rt(void *clientData)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    RT_AP_CHECK(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_free_rti(rtip);
    ap->a_rt_i = (struct rt_i *)NULL;

    bu_free((void *)ap, "struct application");
}

int
wdb_rt_gettrees_cmd(struct rt_wdb *wdbp,
		    int argc,
		    const char *argv[])
{
    struct rt_i *rtip;
    struct application *ap;
    const char *newprocname;
    static struct resource resp = RT_RESOURCE_INIT_ZERO;

    RT_CK_WDB(wdbp);
    RT_CK_DBI(wdbp->dbip);

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_rt_gettrees %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    rtip = rt_new_rti(wdbp->dbip);
    newprocname = argv[1];

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand((Tcl_Interp *)wdbp->wdb_interp, newprocname);

    while (argc > 2 && argv[2][0] == '-') {
	if (BU_STR_EQUAL(argv[2], "-i")) {
	    rtip->rti_dont_instance = 1;
	    argc--;
	    argv++;
	    continue;
	}
	if (BU_STR_EQUAL(argv[2], "-u")) {
	    rtip->useair = 1;
	    argc--;
	    argv++;
	    continue;
	}
	break;
    }

    if (argc-2 < 1) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "rt_gettrees(): no geometry has been specified ", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_gettrees(rtip, argc-2, (const char **)&argv[2], 1) < 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "rt_gettrees() returned error", (char *)NULL);
	rt_free_rti(rtip);
	return TCL_ERROR;
    }

    /* Establish defaults for this rt_i */
    rtip->rti_hasty_prep = 1;	/* Tcl isn't going to fire many rays */

    /*
     * In case of multiple instances of the library, make sure that
     * each instance has a separate resource structure,
     * because the bit vector lengths depend on # of solids.
     * And the "overwrite" sequence in Tcl is to create the new
     * proc before running the Tcl_CmdDeleteProc on the old one,
     * which in this case would trash rt_uniresource.
     * Once on the rti_resources list, rt_clean() will clean 'em up.
     */
    rt_init_resource(&resp, 0, rtip);
    BU_ASSERT(BU_PTBL_GET(&rtip->rti_resources, 0) != NULL);

    BU_ALLOC(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_magic = RT_AP_MAGIC;
    ap->a_resource = &resp;
    ap->a_rt_i = rtip;
    ap->a_purpose = "Conquest!";

    rt_ck(rtip);

    /* Instantiate the proc, with clientData of wdb */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand((Tcl_Interp *)wdbp->wdb_interp, newprocname, tclcad_rt,
			    (ClientData)ap, wdb_deleteProc_rt);

    /* Return new function name as result */
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, newprocname, (char *)NULL);

    return TCL_OK;
}


/**
 *@brief
 * Given an instance of a database and the name of some treetops,
 * create a named "ray-tracing" object (proc) which will respond to
 * subsequent operations.
 * Returns new proc name as result.
 *
 * @par Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
static int
wdb_rt_gettrees_tcl(void *clientData,
		    int argc,
		    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rt_gettrees_cmd(wdbp, argc-1, argv+1);
}

int
wdb_tol_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    struct bu_vls vls;
    double f;

    if (argc < 1 || ((argc-1)%2 != 0)) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_tol %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* print all tolerance settings */
    if (argc == 1) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Current tolerance settings are:\n", (char *)NULL);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "Tessellation tolerances:\n", (char *)NULL);

	if (wdbp->wdb_ttol.abs > 0.0) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\tabs %g mm\n", wdbp->wdb_ttol.abs);
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "\tabs None\n", (char *)NULL);
	}

	if (wdbp->wdb_ttol.rel > 0.0) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\trel %g (%g%%)\n",
			  wdbp->wdb_ttol.rel, wdbp->wdb_ttol.rel * 100.0);
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "\trel None\n", (char *)NULL);
	}

	if (wdbp->wdb_ttol.norm > 0.0) {
	    int deg, min;
	    double sec;

	    bu_vls_init(&vls);
	    sec = wdbp->wdb_ttol.norm * RAD2DEG;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  wdbp->wdb_ttol.norm * RAD2DEG, deg, min, sec);
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "\tnorm None\n", (char *)NULL);
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "Calculational tolerances:\n");
	bu_vls_printf(&vls,
		      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)",
		      wdbp->wdb_tol.dist, wdbp->wdb_tol.perp,
		      acos(wdbp->wdb_tol.perp)*RAD2DEG);
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* get the specified tolerance */
    if (argc == 2) {
	int status = TCL_OK;

	bu_vls_init(&vls);

	switch (argv[1][0]) {
	    case 'a':
		if (wdbp->wdb_ttol.abs > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.abs);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'r':
		if (wdbp->wdb_ttol.rel > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.rel);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'n':
		if (wdbp->wdb_ttol.norm > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.norm);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'd':
		bu_vls_printf(&vls, "%g", wdbp->wdb_tol.dist);
		break;
	    case 'p':
		bu_vls_printf(&vls, "%g", wdbp->wdb_tol.perp);
		break;
	    default:
		bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[1]);
		status = TCL_ERROR;
		break;
	}

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return status;
    }

    /* skip the command name */
    argc--;
    argv++;

    /* iterate over the pairs of tolerance values */
    while (argc > 0) {

	/* set the specified tolerance(s) */
	if (sscanf(argv[1], "%lf", &f) != 1) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "bad tolerance - %s", argv[1]);
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	/* clamp negative to zero */
	if (f < 0.0) {
	    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, "negative tolerance clamped to 0.0\n", (char *)NULL);
	    f = 0.0;
	}

	switch (argv[0][0]) {
	    case 'a':
		/* Absolute tol */
		if (f < wdbp->wdb_tol.dist) {
		    bu_vls_init(&vls);
		    bu_vls_printf(&vls, "absolute tolerance cannot be less than distance tolerance, clamped to %f\n", wdbp->wdb_tol.dist);
		    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		    bu_vls_free(&vls);
		}
		wdbp->wdb_ttol.abs = f;
		break;
	    case 'r':
		if (f >= 1.0) {
		    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
				     "relative tolerance must be between 0 and 1, not changed\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		wdbp->wdb_ttol.rel = f;
		break;
	    case 'n':
		/* Normal tolerance, in degrees */
		if (f > 90.0) {
		    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
				     "Normal tolerance must be less than 90.0 degrees\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		wdbp->wdb_ttol.norm = f * DEG2RAD;
		break;
	    case 'd':
		/* Calculational distance tolerance */
		wdbp->wdb_tol.dist = f;
		wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
		break;
	    case 'p':
		/* Calculational perpendicularity tolerance */
		if (f > 1.0) {
		    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
				     "Calculational perpendicular tolerance must be from 0 to 1\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		wdbp->wdb_tol.perp = f;
		wdbp->wdb_tol.para = 1.0 - f;
		break;
	    default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[0]);
		Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	argc-=2;
	argv+=2;
    }

    return TCL_OK;
}


/**
 * Usage:
 * procname tol [abs|rel|norm|dist|perp [#]]
 *
 *@n abs # sets absolute tolerance.  # > 0.0
 *@n rel # sets relative tolerance.  0.0 < # < 1.0
 *@n norm # sets normal tolerance, in degrees.
 *@n dist # sets calculational distance tolerance
 *@n perp # sets calculational normal tolerance.
 *
 */
static int
wdb_tol_tcl(void *clientData,
	    int argc,
	    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_tol_cmd(wdbp, argc-1, argv+1);
}

int
wdb_track_cmd(void *data,
	int argc,
	const char *argv[])
{
    struct bu_vls log_str = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = (struct rt_wdb *)data;
    int retval;

    if (argc != 15) {
	bu_log("ERROR: expecting 15 arguments\n");
	return TCL_ERROR;
    }

    retval = ged_track2(&log_str, wdbp, argv);

    if (bu_vls_strlen(&log_str) > 0) {
	bu_log("%s", bu_vls_addr(&log_str));
    }

    switch (retval) {
	case BRLCAD_OK:
	    return TCL_OK;
	case BRLCAD_ERROR:
	    return TCL_ERROR;
    }

    /* This should never happen */
    return TCL_ERROR;
}

/**
 * Usage:
 * procname track args
 */
static int
wdb_track_tcl(void *clientData,
	      int argc,
	      const char *argv[]) {
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_track_cmd(wdbp, argc-1, argv+1);
}

int
wdb_units_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    double loc2mm;
    struct bu_vls vls;
    const char *str;
    int sflag = 0;

    bu_vls_init(&vls);
    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&vls, "helplib_alias wdb_units %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "-s")) {
	--argc;
	++argv;

	sflag = 1;
    }

    if (argc < 2) {
	str = bu_units_string(wdbp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	if (sflag)
	    bu_vls_printf(&vls, "%s", str);
	else
	    bu_vls_printf(&vls, "You are editing in '%s'.  1 %s = %g mm \n",
			  str, str, wdbp->dbip->dbi_local2base);

	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Allow inputs of the form "25cm" or "3ft" */
    if ((loc2mm = bu_mm_value(argv[1])) <= 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, argv[1], ": unrecognized unit\n",
			 "valid units: <um|mm|cm|m|km|in|ft|yd|mi>\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (db_update_ident(wdbp->dbip, wdbp->dbip->dbi_title, loc2mm) < 0) {
	Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp,
			 "Warning: unable to stash working units into database\n",
			 (char *)NULL);
    }

    wdbp->dbip->dbi_local2base = loc2mm;
    wdbp->dbip->dbi_base2local = 1.0 / loc2mm;

    str = bu_units_string(wdbp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    bu_vls_printf(&vls, "You are now editing in '%s'.  1 %s = %g mm \n",
		  str, str, wdbp->dbip->dbi_local2base);
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 *@brief
 * Set/get the database units.
 *
 * Usage:
 * dbobjname units [str]
 */
static int
wdb_units_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_units_cmd(wdbp, argc-1, argv+1);
}

int
wdb_version_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc != 1) {
	bu_vls_printf(&vls, "helplib_alias wdb_version %s", argv[0]);
	Tcl_Eval((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_printf(&vls, "%d", db_version(wdbp->dbip));
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, bu_vls_addr(&vls), (char *)0);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname version
 */
static int
wdb_version_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_version_cmd(wdbp, argc-1, argv+1);
}


static struct bu_cmdtab wdb_newcmds[] = {
    {"adjust",		(int (*)(void *, int, const char **))ged_adjust},
    {"arced",		(int (*)(void *, int, const char **))ged_arced},
    {"attr",		(int (*)(void *, int, const char **))ged_attr},
    {"bo",		(int (*)(void *, int, const char **))ged_bo},
    {"bot_face_sort",	(int (*)(void *, int, const char **))ged_bot_face_sort},
    {"bot_smooth",	(int (*)(void *, int, const char **))ged_bot_smooth},
    {"bot_smooth",	(int (*)(void *, int, const char **))ged_bot_smooth},
    {"c",		(int (*)(void *, int, const char **))ged_comb_std},
    {"cat",		(int (*)(void *, int, const char **))ged_cat},
    {"cc",		(int (*)(void *, int, const char **))ged_cc},
    {"color",		(int (*)(void *, int, const char **))ged_color},
    {"comb",		(int (*)(void *, int, const char **))ged_comb},
    {"comb_color",	(int (*)(void *, int, const char **))ged_comb_color},
    {"concat",		(int (*)(void *, int, const char **))ged_concat},
    {"copyeval",	(int (*)(void *, int, const char **))ged_copyeval},
    {"cp",		(int (*)(void *, int, const char **))ged_copy},
    {"dbip",		(int (*)(void *, int, const char **))ged_dbip},
    {"dump",		(int (*)(void *, int, const char **))ged_dump},
    {"dup",		(int (*)(void *, int, const char **))ged_dup},
    {"edcomb",		(int (*)(void *, int, const char **))ged_edcomb},
    {"edit",		(int (*)(void *, int, const char **))ged_edit},
    {"edmater",		(int (*)(void *, int, const char **))ged_edmater},
    {"expand",		(int (*)(void *, int, const char **))ged_expand},
    {"facetize",	(int (*)(void *, int, const char **))ged_facetize},
    {"form",		(int (*)(void *, int, const char **))ged_form},
    {"g",		(int (*)(void *, int, const char **))ged_group},
    {"get",		(int (*)(void *, int, const char **))ged_get},
    {"get_type",	(int (*)(void *, int, const char **))ged_get_type},
    {"hide",		(int (*)(void *, int, const char **))ged_hide},
    {"i",		(int (*)(void *, int, const char **))ged_instance},
    {"item",		(int (*)(void *, int, const char **))ged_item},
    {"keep",		(int (*)(void *, int, const char **))ged_keep},
    {"kill",		(int (*)(void *, int, const char **))ged_kill},
    {"killall",		(int (*)(void *, int, const char **))ged_killall},
    {"killtree",	(int (*)(void *, int, const char **))ged_killtree},
    {"l",		(int (*)(void *, int, const char **))ged_list},
    {"listeval",	(int (*)(void *, int, const char **))ged_pathsum},
    {"log",		(int (*)(void *, int, const char **))ged_log},
    {"ls",		(int (*)(void *, int, const char **))ged_ls},
    {"lt",		(int (*)(void *, int, const char **))ged_lt},
    {"make",		(int (*)(void *, int, const char **))ged_make},
    {"make_name",	(int (*)(void *, int, const char **))ged_make_name},
    {"match",		(int (*)(void *, int, const char **))ged_match},
    {"mater",		(int (*)(void *, int, const char **))ged_mater},
    {"mirror",		(int (*)(void *, int, const char **))ged_mirror},
    {"mv",		(int (*)(void *, int, const char **))ged_move},
    {"mvall",		(int (*)(void *, int, const char **))ged_move_all},
    {"nirt",		(int (*)(void *, int, const char **))ged_nirt},
    {"nirt",		(int (*)(void *, int, const char **))ged_nmg_simplify},
    {"ocenter",		(int (*)(void *, int, const char **))ged_ocenter},
    {"orotate",		(int (*)(void *, int, const char **))ged_orotate},
    {"oscale",		(int (*)(void *, int, const char **))ged_oscale},
    {"otranslate",	(int (*)(void *, int, const char **))ged_otranslate},
    {"pathlist",	(int (*)(void *, int, const char **))ged_pathlist},
    {"paths",		(int (*)(void *, int, const char **))ged_pathsum},
    {"prcolor",		(int (*)(void *, int, const char **))ged_prcolor},
    {"push",		(int (*)(void *, int, const char **))ged_push},
    {"put",		(int (*)(void *, int, const char **))ged_put},
    {"r",		(int (*)(void *, int, const char **))ged_region},
    {"rm",		(int (*)(void *, int, const char **))ged_remove},
    {"rmater",		(int (*)(void *, int, const char **))ged_rmater},
    {"shader",		(int (*)(void *, int, const char **))ged_shader},
    {"shells",		(int (*)(void *, int, const char **))ged_shells},
    {"showmats",	(int (*)(void *, int, const char **))ged_showmats},
    {"summary",		(int (*)(void *, int, const char **))ged_summary},
    {"title",		(int (*)(void *, int, const char **))ged_title},
    {"tops",		(int (*)(void *, int, const char **))ged_tops},
    {"unhide",		(int (*)(void *, int, const char **))ged_unhide},
    {"whatid",		(int (*)(void *, int, const char **))ged_whatid},
    {"whichair",	(int (*)(void *, int, const char **))ged_which},
    {"whichid",		(int (*)(void *, int, const char **))ged_which},
    {"wmater",		(int (*)(void *, int, const char **))ged_wmater},
    {"xpush",		(int (*)(void *, int, const char **))ged_xpush},
    {(const char *)NULL, BU_CMD_NULL}
};

static struct bu_cmdtab wdb_cmds[] = {
    {"db",               wdb_stub_tcl},
    {"find",             wdb_find_tcl},
    {"importFg4Section", wdb_importFg4Section_tcl},
    {"make_bb",          wdb_make_bb_tcl},
    {"move_arb_edge",    wdb_move_arb_edge_tcl},
    {"move_arb_face",    wdb_move_arb_face_tcl},
    {"nmg_collapse",     wdb_nmg_collapse_tcl},
    {"observer",         wdb_observer_tcl},
    {"open",             wdb_reopen_tcl},
    {"rmap",             wdb_rmap_tcl},
    {"rotate_arb_face",  wdb_rotate_arb_face_tcl},
    {"rt_gettrees",      wdb_rt_gettrees_tcl},
    {"tol",              wdb_tol_tcl},
    {"track",            wdb_track_tcl},
    {"units",            wdb_units_tcl},
    {"version",          wdb_version_tcl},
    {(const char *)NULL, BU_CMD_NULL}
};


/* used to suppress bu_log() output */
static int
do_nothing(void *UNUSED(nada1), void *UNUSED(nada2))
{
    return 0;
}


/**
 *@brief
 * Generic interface for database commands.
 *
 * @par Usage:
 * procname cmd ?args?
 *
 * @return result of wdb command.
 */
int
wdb_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
    struct ged ged;
    struct bu_hook_list save_hook_list = BU_HOOK_LIST_INIT_ZERO;
    int ret;

    /* look for the new libged commands before trying one of the old ones */
    GED_INIT(&ged, wdbp);

    bu_log_hook_save_all(&save_hook_list);

    /* suppress bu_log output because we don't care if the command
     * exists in wdb_newcmds since it might be in wdb_cmds.  this
     * prevents bu_cmd() from blathering.
     */
    bu_log_add_hook(do_nothing, NULL);

    if (bu_cmd(wdb_newcmds, argc-1, argv+1, 0, (ClientData)&ged, &ret) == BRLCAD_OK) {
	Tcl_SetResult(interp, bu_vls_addr(ged.ged_result_str), TCL_VOLATILE);
	ged_free(&ged);
	/* unsuppress bu_log output */
	bu_log_hook_restore_all(&save_hook_list);
	return ret;
    }

    /* unsuppress bu_log output */
    bu_log_hook_restore_all(&save_hook_list);

    /* release any allocated memory */
    ged_free(&ged);

    /* not a new command -- look for the command in the old command table */
    bu_cmd(wdb_cmds, argc, (const char **)argv, 1, clientData, &ret);

    return ret;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
wdb_deleteProc(ClientData clientData)
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    /* free observers */
    bu_observer_free(&wdbp->wdb_observers);

    /* close up shop */
    wdb_close(wdbp);
}


/**
 * @brief
 * Create a command named "oname" in "interp" using "wdbp" as its state.
 *
 */
int
wdb_create_cmd(struct rt_wdb *wdbp,	/* pointer to object */
	       const char *oname)	/* object name */
{
    if (wdbp == RT_WDB_NULL) {
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of wdbp */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand((Tcl_Interp *)wdbp->wdb_interp, oname, (Tcl_CmdProc *)wdb_cmd,
			    (ClientData)wdbp, wdb_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult((Tcl_Interp *)wdbp->wdb_interp, oname, (char *)NULL);

    return TCL_OK;
}


/**
 * @brief
 * Create an command/object named "oname" in "interp" using "wdbp" as
 * its state.  It is presumed that the wdbp has already been opened.
 */
int
wdb_init_obj(Tcl_Interp *interp,
	     struct rt_wdb *wdbp,	/* pointer to object */
	     const char *oname)	/* object name */
{
    if (wdbp == RT_WDB_NULL) {
	Tcl_AppendResult(interp, "wdb_open ", oname, " failed (wdb_init_obj)", NULL);
	return TCL_ERROR;
    }

    /* initialize rt_wdb */
    bu_vls_init(&wdbp->wdb_name);
    bu_vls_strcpy(&wdbp->wdb_name, oname);

    wdbp->wdb_interp = (void *)interp;

    /* append to list of rt_wdb's */
    BU_LIST_APPEND(&RTG.rtg_headwdb.l, &wdbp->l);

    return TCL_OK;
}


/**
 *@brief
 * A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 * @par Implicit return -
 * Creates a new TCL proc which responds to get/put/etc. arguments
 * when invoked.  clientData of that proc will be rt_wdb pointer
 * for this instance of the database.
 * Easily allows keeping track of multiple databases.
 *
 * @return wdb pointer, for more traditional C-style interfacing.
 *
 * @par Example -
 * set wdbp [wdb_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n wdb_open db file "bob.g"
 *@n db get white.r
 *@n db close
 */
static int
wdb_open_tcl(ClientData UNUSED(clientData),
	     Tcl_Interp *interp,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp;
    int ret;

    if (argc == 1) {
	/* get list of database objects */
	for (BU_LIST_FOR (wdbp, rt_wdb, &RTG.rtg_headwdb.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&wdbp->wdb_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc < 3 || 4 < argc) {
	Tcl_AppendResult(interp, "\
Usage: wdb_open\n\
       wdb_open newprocname file filename\n\
       wdb_open newprocname disk $dbip\n\
       wdb_open newprocname disk_append $dbip\n\
       wdb_open newprocname inmem $dbip\n\
       wdb_open newprocname inmem_append $dbip\n\
       wdb_open newprocname db filename\n\
       wdb_open newprocname filename\n",
			 NULL);
	return TCL_ERROR;
    }

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(interp, argv[1]);

    if (argc == 3 || BU_STR_EQUAL(argv[2], "db")) {
	struct db_i *dbip;
	int i;

	if (argc == 3)
	    i = 2;
	else
	    i = 3;

	if ((dbip = wdb_prep_dbip(argv[i])) == DBI_NULL)
	    return TCL_ERROR;
	RT_CK_DBI(dbip);

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    } else if (BU_STR_EQUAL(argv[2], "file")) {
	wdbp = wdb_fopen(argv[3]);
    } else {
	struct db_i *dbip;

	if (wdb_decode_dbip(argv[3], &dbip) != TCL_OK)
	    return TCL_ERROR;

	if (BU_STR_EQUAL(argv[2], "disk"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	else if (BU_STR_EQUAL(argv[2], "disk_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
	else if (BU_STR_EQUAL(argv[2], "inmem"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	else if (BU_STR_EQUAL(argv[2], "inmem_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
	else {
	    Tcl_AppendResult(interp, "wdb_open ", argv[2],
			     " target type not recognized", NULL);
	    return TCL_ERROR;
	}
    }

    if ((ret = wdb_init_obj(interp, wdbp, argv[1])) != TCL_OK)
	return ret;

    return wdb_create_cmd(wdbp, argv[1]);
}


/**
 * @brief create the Tcl command for wdb_open
 *
 */
int
Wdb_Init(Tcl_Interp *interp)
{
    (void)Tcl_CreateCommand(interp, (const char *)"wdb_open", wdb_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

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
