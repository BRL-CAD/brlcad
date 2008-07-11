/*                       G E D _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2008 United States Government as represented by
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
/** @file ged_obj.c
 *
 * A quasi-object-oriented database interface.
 *
 * A GED object contains the attributes and methods for
 * controlling a BRL-CAD geometry edit object.
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
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "vmath.h"
#include "db.h"
#include "rtgeom.h"
#include "wdb.h"
#include "mater.h"
#include "tclcad.h"

#include "solid.h"
#include "dm.h"
#include "dm_xvars.h"

#if defined(DM_X) || defined(DM_TK)
#  include "tk.h"
#  include <X11/Xutil.h>
#endif /* DM_X or DM_TK*/

#ifdef DM_X
#  include "dm-X.h"
#endif /* DM_X */

#ifdef DM_TK
#  include "dm-tk.h"
#endif /* DM_TK */

#ifdef DM_OGL
#  include "dm-ogl.h"
#endif /* DM_OGL */

#ifdef DM_WGL
#  include <tkwinport.h>
#  include "dm-wgl.h"
#endif /* DM_WGL */

#if 1
/*XXX Temporary */
#include "dg.h"
#endif

static int go_open_tcl(ClientData clientData,
			Tcl_Interp *interp,
			int argc,
			const char **argv);

static int go_aet(struct ged *gedp, int argc, const char *argv[]);
static int go_arot(struct ged *gedp, int argc, const char *argv[]);
static int go_autoview(struct ged *gedp, int argc, const char *argv[]);
static int go_blast(struct ged *gedp, int argc, const char *argv[]);
static int go_center(struct ged *gedp, int argc, const char *argv[]);
static int go_configure(struct ged *gedp, int argc, const char *argv[]);
static int go_constrain_rmode(struct ged *gedp, int argc, const char *argv[]);
static int go_constrain_tmode(struct ged *gedp, int argc, const char *argv[]);
static int go_delete_view(struct ged *gedp, int argc, const char *argv[]);
static int go_draw(struct ged *gedp, int argc, const char *argv[]);
static int go_E(struct ged *gedp, int argc, const char *argv[]);
static int go_erase(struct ged *gedp, int argc, const char *argv[]);
static int go_erase_all(struct ged *gedp, int argc, const char *argv[]);
static int go_eye(struct ged *gedp, int argc, const char *argv[]);
static int go_eye_pos(struct ged *gedp, int argc, const char *argv[]);
static int go_idle_mode(struct ged *gedp, int argc, const char *argv[]);
static int go_illum(struct ged *gedp, int argc, const char *argv[]);
static int go_list_views(struct ged *gedp, int argc, const char *argv[]);
#ifdef USE_FBSERV
static int go_listen(struct ged *gedp, int argc, const char *argv[]);
#endif
static int go_make(struct ged *gedp, int argc, const char *argv[]);
static int go_mirror(struct ged *gedp, int argc, const char *argv[]);
static int go_mouse_constrain_rot(struct ged *gedp, int argc, const char *argv[]);
static int go_mouse_constrain_trans(struct ged *gedp, int argc, const char *argv[]);
static int go_mouse_rot(struct ged *gedp, int argc, const char *argv[]);
static int go_mouse_scale(struct ged *gedp, int argc, const char *argv[]);
static int go_mouse_trans(struct ged *gedp, int argc, const char *argv[]);
static int go_new_view(struct ged *gedp, int argc, const char *argv[]);
static int go_overlay(struct ged *gedp, int argc, const char *argv[]);
static int go_refresh(struct ged *gedp, int argc, const char *argv[]);
static int go_refresh_all(struct ged *gedp, int argc, const char *argv[]);
static int go_rot(struct ged *gedp, int argc, const char *argv[]);
static int go_rotate_mode(struct ged *gedp, int argc, const char *argv[]);
#if GED_USE_RUN_RT
static int go_rt(struct ged *gedp, int argc, const char *argv[]);
#endif
static int go_scale_mode(struct ged *gedp, int argc, const char *argv[]);
static int go_set_coord(struct ged *gedp, int argc, const char *argv[]);
static int go_set_fb_mode(struct ged *gedp, int argc, const char *argv[]);
static int go_set_transparency(struct ged *gedp, int argc, const char *argv[]);
static int go_size(struct ged *gedp, int argc, const char *argv[]);
static int go_tra(struct ged *gedp, int argc, const char *argv[]);
static int go_translate_mode(struct ged *gedp, int argc, const char *argv[]);
static int go_vmake(struct ged *gedp, int argc, const char *argv[]);
static int go_vslew(struct ged *gedp, int argc, const char *argv[]);
static int go_zap(struct ged *gedp, int argc, const char *argv[]);
static int go_zbuffer(struct ged *gedp, int argc, const char *argv[]);
static int go_zoom(struct ged *gedp, int argc, const char *argv[]);



static void go_drawSolid(struct dm *dmp, struct solid *sp);
static int go_drawSList(struct dm *dmp, struct bu_list *hsp);
#ifdef USE_FBSERV
static int go_close_fbs(struct ged_dm_view *gdvp);
static void go_fbs_callback();
static int go_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp);
#endif
static void go_refresh_view(struct ged_dm_view *gdvp);
static void go_refresh_all_views(struct ged_obj *gop);
static void go_autoview_view(struct ged_dm_view *gdvp);
static void go_autoview_all_views(struct ged_obj *gop);


static struct ged_obj HeadGedObj;
static struct ged_obj *go_current_gop = GED_OBJ_NULL;

static struct bu_cmdtab go_cmds[] = {
    {"adjust",		ged_adjust},
    {"ae2dir",		ged_ae2dir},
    {"aet",		go_aet},
    {"arced",		ged_arced},
    {"arot",		go_arot},
    {"attr",		ged_attr},
    {"autoview",	go_autoview},
    {"binary",		ged_binary},
    {"blast",		go_blast},
    {"bot_decimate",	ged_bot_decimate},
    {"bot_face_sort",	ged_bot_face_sort},
    {"center",		go_center},
    {"comb_color",	ged_comb_color},
    {"configure",	go_configure},
    {"constrain_rmode",	go_constrain_rmode},
    {"constrain_tmode",	go_constrain_tmode},
    {"delete_view",	go_delete_view},
    {"dir2ae",		ged_dir2ae},
    {"draw",		go_draw},
    {"E",		go_E},
    {"edcomb",		ged_edcomb},
    {"edmater",		ged_edmater},
    {"erase",		go_erase},
    {"erase_all",	go_erase_all},
    {"eye",		go_eye},
    {"eye_pos",		go_eye_pos},
    {"form",		ged_form},
    {"get",		ged_get},
    {"get_autoview",	ged_get_autoview},
    {"get_eyemodel",	ged_get_eyemodel},
    {"how",		ged_how},
    {"idle_mode",	go_idle_mode},
    {"illum",		go_illum},
#if 0
    {"importFg4Section",	ged_importFg4Section},
#endif
    {"item",		ged_item},
    {"l",		ged_list},
    {"list_views",	go_list_views},
#ifdef USE_FBSERV
    {"listen",		go_listen},
#endif
    {"listeval",	ged_pathsum},
    {"log",		ged_log},
    {"ls",		ged_ls},
    {"make",		go_make},
    {"make_name",	ged_make_name},
    {"mater",		ged_mater},
    {"mirror",		go_mirror},
    {"mouse_constrain_rot",	go_mouse_constrain_rot},
    {"mouse_constrain_trans",	go_mouse_constrain_trans},
    {"mouse_rot",	go_mouse_rot},
    {"mouse_trans",	go_mouse_trans},
    {"mouse_scale",	go_mouse_scale},
    {"mrot",		go_rot},
    {"mtra",		go_tra},
    {"new_view",	go_new_view},
#if 0
#if GED_USE_RUN_RT
    {"nirt",		go_nirt},
#endif
#endif
    {"ocenter",		ged_ocenter},
    {"orotate",		ged_orotate},
    {"oscale",		ged_oscale},
    {"otranslate",	ged_otranslate},
    {"overlay",		go_overlay},
    {"paths",		ged_pathsum},
    {"put",		ged_put},
    {"qray",		ged_qray},
    {"refresh",		go_refresh},
    {"refresh_all",	go_refresh_all},
    {"report",		ged_report},
    {"rmater",		ged_rmater},
    {"rot",		go_rot},
    {"rotate_mode",	go_rotate_mode},
#if GED_USE_RUN_RT
    {"rt",		go_rt},
#if 0
    {"rtabort",		go_rtabort},
    {"rtcheck",		go_rtcheck},
    {"rtedge",		go_rtedge},
#endif
#endif
    {"scale_mode",	go_scale_mode},
    {"set_coord",	go_set_coord},
    {"set_fb_mode",	go_set_fb_mode},
    {"set_transparency",	go_set_transparency},
    {"set_uplotOutputMode",	ged_set_uplotOutputMode},
    {"shader",		ged_shader},
    {"size",		go_size},
    {"tra",		go_tra},
    {"translate_mode",	go_translate_mode},
    {"tree",		ged_tree},
    {"vmake",		go_vmake},
    {"vrot",		go_rot},
    {"vtra",		go_tra},
    {"vslew",		go_vslew},
    {"who",		ged_who},
    {"wmater",		ged_wmater},
    {"zap",		go_zap},
    {"zbuffer",		go_zbuffer},
    {"zoom",		go_zoom},
    {(char *)NULL,	(int (*)())0}
};


/**
 * @brief create the Tcl command for go_open
 *
 */
int
Go_Init(Tcl_Interp *interp)
{
    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;

    BU_LIST_INIT(&HeadGedObj.l);
    (void)Tcl_CreateCommand(interp, (const char *)"go_open", go_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

#if 1
    /*XXX Temporary */
    /* initialize database objects */
    Wdb_Init(interp);

    /* initialize drawable geometry objects */
    Dgo_Init(interp);

    /* initialize view objects */
    Vo_Init(interp);
#endif

    return TCL_OK;
}

/**
 *			G O _ C M D
 *@brief
 * Generic interface for database commands.
 *
 * @par Usage:
 *        procname cmd ?args?
 *
 * @return result of ged command.
 */
static int
go_cmd(ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		**argv)
{
    register struct bu_cmdtab *ctp;
    struct ged_obj *gop = (struct ged_obj *)clientData;
    Tcl_DString ds;
    int ret;
#if 0
    char flags[128];

    GED_CHECK_OBJ(gop);
#endif

    Tcl_DStringInit(&ds);

    if (argc < 2) {
	Tcl_DStringAppend(&ds, "subcommand not specfied; must be one of: ", -1);
	for (ctp = go_cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->ct_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    go_current_gop = gop;

    for (ctp = go_cmds; ctp->ct_name != (char *)0; ctp++) {
	if (ctp->ct_name[0] == argv[1][0] &&
	    !strcmp(ctp->ct_name, argv[1])) {
	    ret = (*ctp->ct_func)(gop->go_gedp, argc-1, argv+1);
	    break;
	}
    }

    /* Command not found. */
    if (ctp->ct_name == (char *)0) {
	Tcl_DStringAppend(&ds, "unknown subcommand: ", -1);
	Tcl_DStringAppend(&ds, argv[1], 01);
	Tcl_DStringAppend(&ds, "; must be one of: ", -1);

	for (ctp = go_cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", 01);
	    Tcl_DStringAppend(&ds, ctp->ct_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

#if 0
    snprintf(flags, 127, "%u", gop->go_gedp->ged_result_flags);
    Tcl_DStringAppend(&ds, flags, -1);
#endif
    Tcl_DStringAppend(&ds, bu_vls_addr(&gop->go_gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
go_deleteProc(ClientData clientData)
{
    struct ged_obj *gop = (struct ged_obj *)clientData;
    struct ged_dm_view *gdvp;

    if (go_current_gop == gop)
	go_current_gop = GED_OBJ_NULL;

#if 0
    GED_CHECK_OBJ(gop);
#endif
    BU_LIST_DEQUEUE(&gop->l);
    bu_vls_free(&gop->go_name);
    ged_close(gop->go_gedp);
#if 1
    while (BU_LIST_WHILE(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	BU_LIST_DEQUEUE(&(gdvp->l));
	bu_vls_free(&gdvp->gdv_name);
	DM_CLOSE(gdvp->gdv_dmp);
	bu_free((genptr_t)gdvp->gdv_view, "ged_view");
#ifdef USE_FBSERV
	go_close_fbs(gdvp);
#endif
	bu_free((genptr_t)gdvp, "ged_dm_view");
    }
#else
    for (i = 0; i < GED_OBJ_NUM_VIEWS; ++i)
	bu_free((genptr_t)gop->go_views[i], "struct ged_view");
    if (gop->go_dmp != DM_NULL)
	DM_CLOSE(gop->go_dmp);
#endif
    bu_free((genptr_t)gop, "struct ged_obj");
}

/**
 * @brief
 * Create a command named "oname" in "interp" using "gedp" as its state.
 *
 */
int
go_create_cmd(Tcl_Interp	*interp,
	      struct ged_obj	*gop,	/* pointer to object */
	      const char	*oname)	/* object name */
{
    if (gop == GED_OBJ_NULL) {
	Tcl_AppendResult(interp, "go_create_cmd ", oname, " failed", NULL);
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of gop */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(interp, oname, (Tcl_CmdProc *)go_cmd,
			    (ClientData)gop, go_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult(interp, oname, (char *)NULL);

    return TCL_OK;
}

#if 0
/**
 * @brief
 * Create an command/object named "oname" in "interp" using "gop" as
 * its state.  It is presumed that the gop has already been opened.
 */
int
go_init_obj(Tcl_Interp		*interp,
	     struct ged_obj	*gop,	/* pointer to object */
	     const char		*oname)	/* object name */
{
    if (gop == GED_OBJ_NULL) {
	Tcl_AppendResult(interp, "ged_init_obj ", oname, " failed (ged_init_obj)", NULL);
	return TCL_ERROR;
    }

    /* initialize ged_obj */
    bu_vls_init(&gop->go_name);
    bu_vls_strcpy(&gop->go_name, oname);

    BU_LIST_INIT(&gop->go_observers.l);
    gop->go_interp = interp;

    /* append to list of ged_obj */
    BU_LIST_APPEND(&HeadGedObj.l, &gop->l);

    return TCL_OK;
}
#endif

/**
 *			G E D _ O P E N _ T C L
 *@brief
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  @par Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be ged_obj pointer
 *	for this instance of the database.
 *	Easily allows keeping track of multiple databases.
 *
 *  @return wdb pointer, for more traditional C-style interfacing.
 *
 *  @par Example -
 *	set gop [go_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n	go_open db file "bob.g"
 *@n	db get white.r
 *@n	db close
 */
static int
go_open_tcl(ClientData	clientData,
	     Tcl_Interp	*interp,
	     int	argc,
	     const char	**argv)
{
    struct ged_obj *gop;
    struct ged *gedp;

    if (argc == 1) {
	/* get list of database objects */
	for (BU_LIST_FOR(gop, ged_obj, &HeadGedObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&gop->go_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc < 3 || 4 < argc) {
	Tcl_AppendResult(interp, "\
Usage: go_open\n\
       go_open newprocname file filename\n\
       go_open newprocname disk $dbip\n\
       go_open newprocname disk_append $dbip\n\
       go_open newprocname inmem $dbip\n\
       go_open newprocname inmem_append $dbip\n\
       go_open newprocname db filename\n\
       go_open newprocname filename\n",
			 NULL);
	return TCL_ERROR;
    }

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(interp, argv[1]);

    if (argc == 3 || strcmp(argv[2], "db") == 0) {
	if (argc == 3)
	    gedp = ged_open("filename", argv[2]); 
	else
	    gedp = ged_open("db", argv[3]); 
    } else
	gedp = ged_open(argv[2], argv[3]); 

    /* initialize ged_obj */
    BU_GETSTRUCT(gop, ged_obj);
    gop->go_gedp = gedp;
    bu_vls_init(&gop->go_name);
    bu_vls_strcpy(&gop->go_name, argv[1]);
    BU_LIST_INIT(&gop->go_observers.l);
    gop->go_interp = interp;

#if 1
    BU_LIST_INIT(&gop->go_head_views.l);
#else
    for (i = 0; i < GED_OBJ_NUM_VIEWS; ++i) {
	BU_GETSTRUCT(gop->go_views[i], ged_view);
	ged_view_init(gop->go_views[i]);
    }

    /*XXX
     *   Temporarily acquiring a single display manager window
     *   on Windows because its #1 :-)
     */
    {
	int type = DM_TYPE_WGL;
	int ac = 5;
	char *av[6];

	av[0] = argv[0];
	av[1] = "-s";
	av[2] = "700";
	av[3] = "-n";
	av[4] = ".bob";
	av[5] = (char *)0;
	if ((gop->go_dmp = dm_open(interp, type, ac, av)) == DM_NULL) {
	    Tcl_AppendResult(interp,
			     bu_vls_addr(&gop->go_name), " failed to create display manager",
			     (char *)NULL);
	}
    }

    /* Initialize using first view */
    gop->go_gedp->ged_gvp = gop->go_views[0];
#endif

    /* append to list of ged_obj */
    BU_LIST_APPEND(&HeadGedObj.l, &gop->l);

#if 0
    if ((ret = ged_init_obj(interp, gop, argv[1])) != TCL_OK)
	return ret;
#endif

    return go_create_cmd(interp, gop, argv[1]);
}


/*************************** Local Command Functions ***************************/
static int
go_aet(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[6];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [[-i] az el [tw]]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (6 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (argc == 2) {
	ac = 1;
	av[1] = (char *)0;
    } else {
	register int i;

	ac = argc-1;
	for (i = 2; i < argc; ++i)
	    av[i-1] = (char *)argv[i];
	av[i-1] = (char *)0;
    }

    ret = ged_aet(gedp, ac, (const char **)av);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_arot(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    int ret;
    int ac;
    char *av[6];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y z angle";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;

    ret = ged_arot(gedp, ac, (const char **)av);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_autoview(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    static const char *usage = "vname";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_autoview_view(gdvp);

    return BRLCAD_OK;
}

static int
go_blast(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_blast(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_center(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[5];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [x y z]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (5 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (argc == 2) {
	ac = 1;
	av[1] = (char *)0;
    } else {
	register int i;

	ac = argc-1;
	for (i = 2; i < argc; ++i)
	    av[i-1] = (char *)argv[i];
	av[i-1] = (char *)0;
    }
    ret = ged_center(gedp, ac, (const char **)av);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_configure(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    int	status;
    static const char *usage = "vname";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* configure the display manager window */
    status = DM_CONFIGURE_WIN(gdvp->gdv_dmp);

#ifdef USE_FBSERV
    /* configure the framebuffer window */
    if (gdvp->gdv_fbs.fbs_fbp != FBIO_NULL)
	fb_configureWindow(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_dmp->dm_width,
			   gdvp->gdv_dmp->dm_height);
#endif

    if (status == TCL_OK) {
	go_refresh_view(gdvp);
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
go_draw(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int aflag = 0;

    {
	char *av[2];

	av[0] = "who";
	av[1] = (char *)0;
	ret = ged_who(gedp, 1, (const char **)av);

	if (ret == BRLCAD_OK && strlen(bu_vls_addr(&gedp->ged_result_str)) == 0)
	    aflag = 1;
    }

    ret = ged_draw(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    if (aflag)
	go_autoview_all_views(go_current_gop);
    else
	go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_E(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_E(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_constrain_rmode(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x|y|z x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' &&
	 argv[2][0] != 'y' &&
	 argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {%S mouse_constrain_rot %S %s %%x %%y}; break",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

static int
go_constrain_tmode(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x|y|z x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' &&
	 argv[2][0] != 'y' &&
	 argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {%S mouse_constrain_trans %S %s %%x %%y}; break",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

static int
go_delete_view(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    static const char *usage = "vname";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    BU_LIST_DEQUEUE(&(gdvp->l));
    bu_vls_free(&gdvp->gdv_name);
    DM_CLOSE(gdvp->gdv_dmp);
    bu_free((genptr_t)gdvp->gdv_view, "ged_view");
    bu_free((genptr_t)gdvp, "ged_dm_view");

    return BRLCAD_OK;
}

static int
go_erase(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_erase(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_erase_all(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_erase_all(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_eye(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    int ret;
    int ac;
    char *av[5];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [x y z]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (5 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = ged_eye(gedp, ac, (const char **)av);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_eye_pos(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    int ret;
    int ac;
    char *av[5];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [x y z]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (5 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = ged_eye_pos(gedp, ac, (const char **)av);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_idle_mode(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {}",
		  &gdvp->gdv_dmp->dm_pathName);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

static int
go_illum(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_illum(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_list_views(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l))
	bu_vls_printf(&gedp->ged_result_str, "%S ", &gdvp->gdv_name);

    return BRLCAD_OK;
}

#ifdef USE_FBSERV
static int
go_listen(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [port]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (gdvp->gdv_fbs.fbs_fbp == FBIO_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s listen: framebuffer not open!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* return the port number */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    if (argc == 3) {
	int port;

	if (sscanf(argv[2], "%d", &port) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "listen: bad value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (port >= 0)
	    fbs_open(&gdvp->gdv_fbs, port);
	else {
	    fbs_close(&gdvp->gdv_fbs);
	}
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

#endif

static int
go_make(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char *av[3];
    struct ged_dm_view *gdvp;

    ret = ged_make(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    av[0] = "draw";
    av[1] = (char *)argv[argc-2];
    av[2] = (char *)0;
    go_draw(gedp, 2, (const char **)av);

    return ret;
}

static int
go_mirror(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char *av[3];
    struct ged_dm_view *gdvp;

    ret = ged_mirror(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    av[0] = "draw";
    av[1] = (char *)argv[argc-1];
    av[2] = (char *)0;
    go_draw(gedp, 2, (const char **)av);

    return ret;
}

static int
go_mouse_constrain_rot(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    struct bu_vls rot_vls;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname coord x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseX - x;
    dy = y - gdvp->gdv_view->gv_prevMouseY;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    bu_vls_init(&rot_vls);
    switch (argv[2][0]) {
    case 'x':
	bu_vls_printf(&rot_vls, "%lf 0 0", sf);
    case 'y':
	bu_vls_printf(&rot_vls, "0 %lf 0", sf);
    case 'z':
	bu_vls_printf(&rot_vls, "0 0 %lf", sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = "rot";
    av[1] = bu_vls_addr(&rot_vls);
    av[2] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_mouse_constrain_trans(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls tran_vls;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname coord x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseX - x;
    dy = y - gdvp->gdv_view->gv_prevMouseY;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_size;
    dy *= inv_width * gdvp->gdv_view->gv_size;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    bu_vls_init(&tran_vls);
    switch (argv[2][0]) {
    case 'x':
	bu_vls_printf(&tran_vls, "%lf 0 0", sf);
    case 'y':
	bu_vls_printf(&tran_vls, "0 %lf 0", sf);
    case 'z':
	bu_vls_printf(&tran_vls, "0 0 %lf", sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = "tra";
    av[1] = bu_vls_addr(&tran_vls);
    av[2] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&tran_vls);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_mouse_rot(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[4];
    fastf_t x, y;
    fastf_t dx, dy;
    struct bu_vls rot_vls;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseY - y;
    dy = gdvp->gdv_view->gv_prevMouseX - x;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    bu_vls_init(&rot_vls);
    bu_vls_printf(&rot_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "rot";
    av[1] = "-v";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_mouse_scale(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls zoom_vls;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) > fabs(dy))
	sf = 1.0 + dx;
    else
	sf = 1.0 + dy;

    bu_vls_init(&zoom_vls);
    bu_vls_printf(&zoom_vls, "%lf", sf);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = "zoom";
    av[1] = bu_vls_addr(&zoom_vls);
    av[2] = (char *)0;

    ret = ged_zoom(gedp, ac, (const char **)av);
    bu_vls_free(&zoom_vls);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_mouse_trans(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[4];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t inv_width;
    struct bu_vls trans_vls;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseX - x;
    dy = y - gdvp->gdv_view->gv_prevMouseY;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_size;
    dy *= inv_width * gdvp->gdv_view->gv_size;

    bu_vls_init(&trans_vls);
    bu_vls_printf(&trans_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "tra";
    av[1] = "-v";
    av[2] = bu_vls_addr(&trans_vls);
    av[3] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&trans_vls);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_new_view(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *new_gdvp = BU_LIST_LAST(ged_dm_view, &go_current_gop->go_head_views.l);
    static const int name_index = 1;
    static const char *usage = "vname type [args]";
    int type = DM_TYPE_BAD;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* find display manager type */
#ifdef DM_X
    if (argv[2][0] == 'X' || argv[2][0] == 'x')
	type = DM_TYPE_X;
#endif /* DM_X */

#ifdef DM_TK
    if (!strcmp(argv[2], "tk"))
	type = DM_TYPE_TK;
#endif /* DM_TK */

#ifdef DM_OGL
    if (!strcmp(argv[2], "ogl"))
	type = DM_TYPE_OGL;
#endif /* DM_OGL */

#ifdef DM_WGL
    if (!strcmp(argv[2], "wgl"))
	type = DM_TYPE_WGL;
#endif /* DM_WGL */

    if (type == DM_TYPE_BAD) {
	bu_vls_printf(&gedp->ged_result_str, "Unsupported display manager type - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    BU_GETSTRUCT(new_gdvp, ged_dm_view);
    BU_GETSTRUCT(new_gdvp->gdv_view, ged_view);

    {
	int i;
	int arg_start = 3;
	int newargs = 0;
	int ac;
	char **av;

	ac = argc + newargs;
	av = (char **)bu_malloc(sizeof(char *) * (ac+1), "go_new_view: av");
	av[0] = (char *)argv[0];

	/*
	 * Stuff name into argument list.
	 */
	av[1] = "-n";
	av[2] = (char *)argv[name_index];

	/* copy the rest */
	for (i = arg_start; i < argc; ++i)
	    av[i+newargs] = (char *)argv[i];
	av[i+newargs] = (char *)NULL;

	if ((new_gdvp->gdv_dmp = dm_open(go_current_gop->go_interp, type, ac, av)) == DM_NULL) {
	    bu_free((genptr_t)new_gdvp->gdv_view, "ged_view");
	    bu_free((genptr_t)new_gdvp, "ged_dm_view");
	    bu_free((genptr_t)av, "go_new_view: av");

	    bu_vls_printf(&gedp->ged_result_str, "Failed to create %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	bu_free((genptr_t)av, "go_new_view: av");

    }

    new_gdvp->gdv_gop = go_current_gop;
    bu_vls_init(&new_gdvp->gdv_name);
    bu_vls_printf(&new_gdvp->gdv_name, argv[name_index]);
    ged_view_init(new_gdvp->gdv_view);
    BU_LIST_INSERT(&go_current_gop->go_head_views.l, &new_gdvp->l);

    bu_vls_printf(&gedp->ged_result_str, "%s", argv[name_index]);

#ifdef USE_FBSERV
    new_gdvp->gdv_fbs.fbs_listener.fbsl_fbsp = &new_gdvp->gdv_fbs;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_fd = -1;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_port = -1;
    new_gdvp->gdv_fbs.fbs_fbp = FBIO_NULL;
    new_gdvp->gdv_fbs.fbs_callback = go_fbs_callback;
    new_gdvp->gdv_fbs.fbs_clientData = new_gdvp;
    new_gdvp->gdv_fbs.fbs_interp = go_current_gop->go_interp;

    /* open the framebuffer */
    go_open_fbs(new_gdvp, go_current_gop->go_interp);
#endif

    /* Set default bindings */
    {
	struct bu_vls bindings;

	bu_vls_init(&bindings);
	bu_vls_printf(&bindings, "bind %S <Configure> {%S configure %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Expose> {%S refresh %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "wm protocol %S WM_DELETE_WINDOW {%S delete_view %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <2> {%S vslew %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <1> {%S zoom %S 0.5; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <3> {%S zoom %S 2.0; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Idle Mode */
	bu_vls_printf(&bindings, "bind %S <ButtonRelease> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Control_L> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Control_R> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Shift_L> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Shift_R> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Alt_L> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <KeyRelease-Alt_R> {%S idle_mode %S; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Rotate Mode */
	bu_vls_printf(&bindings, "bind %S <Control-ButtonPress-1> {%S rotate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Control-ButtonPress-2> {%S rotate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Control-ButtonPress-3> {%S rotate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Translate Mode */
	bu_vls_printf(&bindings, "bind %S <Shift-ButtonPress-1> {%S translate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Shift-ButtonPress-2> {%S translate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Shift-ButtonPress-3> {%S translate_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Scale Mode */
	bu_vls_printf(&bindings, "bind %S <Control-Shift-ButtonPress-1> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Control-Shift-ButtonPress-2> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Control-Shift-ButtonPress-3> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Control-Shift-ButtonPress-1> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Control-Shift-ButtonPress-2> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Control-Shift-ButtonPress-3> {%S scale_mode %S %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Constrained Rotate Mode */
	bu_vls_printf(&bindings, "bind %S <Alt-Control-ButtonPress-1> {%S constrain_rmode %S x %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Control-ButtonPress-2> {%S constrain_rmode %S y %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Control-ButtonPress-3> {%S constrain_rmode %S z %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Constrained Translate Mode */
	bu_vls_printf(&bindings, "bind %S <Alt-Shift-ButtonPress-1> {%S constrain_tmode %S x %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Shift-ButtonPress-2> {%S constrain_tmode %S y %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S <Alt-Shift-ButtonPress-3> {%S constrain_tmode %S z %%x %%y; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	/* Key Bindings */
	bu_vls_printf(&bindings, "bind %S 3 {%S aet %S 35 25; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S 4 {%S aet %S 45 45; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S f {%S aet %S 0 0; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S R {%S aet %S 180 0; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S r {%S aet %S 270 0; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S l {%S aet %S 90 0; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S t {%S aet %S 0 90; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "bind %S b {%S aet %S 0 270; break}; ",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);
	bu_vls_printf(&bindings, "event generate %S <Configure>; %S autoview %S",
		      &new_gdvp->gdv_dmp->dm_pathName,
		      &go_current_gop->go_name,
		      &new_gdvp->gdv_name);

	Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
	bu_vls_free(&bindings);
    }

    return BRLCAD_OK;
}

static int
go_overlay(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_overlay(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

/*XXX For now, doing only simple refresh */
static int
go_refresh(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    static const char *usage = "vname";

#if 0
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
#endif

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

#if 0
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
#endif

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_refresh_all(struct ged *gedp, int argc, const char *argv[])
{
    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    go_refresh_all_views(go_current_gop);

    return BRLCAD_OK;
}

static int
go_rot(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    int ret;
    int ac;
    char *av[6];
    int alt_flag;
    char *coord;
    struct ged_dm_view *gdvp;
    static const char *alt_usage = "vname x y z";
    static const char *usage = "vname [-m|-v] x y z";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;

	if (argv[0][0] == 'm' || argv[0][0] == 'v')
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], alt_usage);
	else
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

	return BRLCAD_OK;
    }

    if (argv[0][0] == 'm' || argv[0][0] == 'v') {
	if (argc != 5) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], alt_usage);
	    return BRLCAD_ERROR;
	}

	if (argv[0][0] == 'm')
	    coord = "-m";
	else
	    coord = "-v";

	alt_flag = 1;
    } else {
	if (argc != 5 && argc != 6) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}

	alt_flag = 0;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (alt_flag) {
	ac = argc;
	av[1] = coord;
	for (i = 2; i < argc; ++i)
	    av[i] = (char *)argv[i];
	av[i] = (char *)0;
    } else {
	ac = argc-1;
	for (i = 2; i < argc; ++i)
	    av[i-1] = (char *)argv[i];
	av[i-1] = (char *)0;
    }
    ret = ged_rot(gedp, ac, (const char **)av);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_rotate_mode(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {%S mouse_rot %S %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

#if GED_USE_RUN_RT
#define GO_MAX_RT_ARGS 64
static int
go_rt(struct ged *gedp, int argc, const char *argv[])
{
    int ac;
    char *av[GO_MAX_RT_ARGS];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (GO_MAX_RT_ARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (argc == 2) {
	ac = 1;
	av[1] = (char *)0;
    } else {
	register int i;

	ac = argc-1;
	for (i = 2; i < argc; ++i)
	    av[i-1] = (char *)argv[i];
	av[i-1] = (char *)0;
    }
    return ged_rt(gedp, ac, (const char **)av);
}
#endif

static int
go_scale_mode(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {%S mouse_scale %S %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

static int
go_set_coord(struct ged *gedp, int argc, const char *argv[])
{
    char coord;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [v|m]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Get coord */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%c", gdvp->gdv_view->gv_coord);
	return BRLCAD_OK;
    }

    /* Set coord */
    if ((argv[2][0] != 'm' && argv[2][0] != 'v') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "set_coord: bad value - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_coord = argv[2][0];

    return BRLCAD_OK;
}

static int
go_set_fb_mode(struct ged *gedp, int argc, const char *argv[])
{
    int mode;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [mode]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Get fb mode */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_mode);
	return BRLCAD_OK;
    }

    /* Set fb mode */
    if (sscanf(argv[2], "%d", &mode) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "set_fb_mode: bad value - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if (mode < 0)
	mode = 0;
    else if (GED_OBJ_FB_MODE_OVERLAY < mode)
	mode = GED_OBJ_FB_MODE_OVERLAY;

    gdvp->gdv_fbs.fbs_mode = mode;
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_set_transparency(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    ret = ged_set_transparency(gedp, argc, argv);

    if (ret == BRLCAD_ERROR || gedp->ged_result_flags & GED_RESULT_FLAGS_HELP_BIT)
	return ret;

    go_refresh_all_views(go_current_gop);

    return ret;
}

static int
go_size(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [size]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (argc == 2) {
	ac = 1;
	av[1] = (char *)0;
    } else {
	ac = 2;
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
    }

    ret = ged_size(gedp, ac, (const char **)av);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_tra(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    int ret;
    int ac;
    int alt_flag;
    char *coord;
    char *av[6];
    struct ged_dm_view *gdvp;
    static const char *alt_usage = "vname x y z";
    static const char *usage = "vname [-m|-v] x y z";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;

	if (argv[0][0] == 'm' || argv[0][0] == 'v')
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], alt_usage);
	else
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

	return BRLCAD_OK;
    }

    if (argv[0][0] == 'm' || argv[0][0] == 'v') {
	if (argc != 5) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], alt_usage);
	    return BRLCAD_ERROR;
	}

	if (argv[0][0] == 'm')
	    coord = "-m";
	else
	    coord = "-v";

	alt_flag = 1;
    } else {
	if (argc != 5 && argc != 6) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}

	alt_flag = 0;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    if (alt_flag) {
	ac = argc;
	av[1] = coord;
	for (i = 2; i < argc; ++i)
	    av[i] = (char *)argv[i];
	av[i] = (char *)0;
    } else {
	ac = argc-1;
	for (i = 2; i < argc; ++i)
	    av[i-1] = (char *)argv[i];
	av[i-1] = (char *)0;
    }
    ret = ged_tra(gedp, ac, (const char **)av);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_translate_mode(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname x y";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %S <Motion> {%S mouse_trans %S %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

static int
go_vmake(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_dm_view *gdvp;
    static const char *usage = "vname pname ptype";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    {
	int ret;
	char *av[8];
	char center[512];
	char scale[128];

	sprintf(center, "%lf %lf %lf",
		-gdvp->gdv_view->gv_center[MDX],
		-gdvp->gdv_view->gv_center[MDY],
		-gdvp->gdv_view->gv_center[MDZ]);
	sprintf(scale, "%lf", gdvp->gdv_view->gv_scale * 2.0);

	av[0] = (char *)argv[0];
	av[1] = "-o";
	av[2] = center;
	av[3] = "-s";
	av[4] = scale;
	av[5] = (char *)argv[2];
	av[6] = (char *)argv[3];
	av[7] = (char *)0;

	ret = ged_make(gedp, 7, av);

	if (ret == BRLCAD_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    go_draw(gedp, 2, (const char **)av);
	}

	return ret;
    }
}

static int
go_vslew(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x1, y1;
    fastf_t x2, y2;
    fastf_t sf;
    struct bu_vls slew_vec;
    struct ged_dm_view *gdvp;
#if 1
    static const char *usage = "vname x y";
#else
    /*XXX still need code to handle optional z */
    static const char *usage = "vname x y [z]";
#endif

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x1) != 1 ||
	sscanf(argv[3], "%lf", &y1) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    x2 = 0.5 * gdvp->gdv_dmp->dm_width;
    y2 = 0.5 * gdvp->gdv_dmp->dm_height;
    sf = 2.0 / gdvp->gdv_dmp->dm_width;

    bu_vls_init(&slew_vec);
    bu_vls_printf(&slew_vec, "%lf %lf", (x1 - x2) * sf, (y2 - y1) * sf);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = (char *)argv[0];
    av[1] = bu_vls_addr(&slew_vec);
    av[2] = (char *)0;

    ret = ged_slew(gedp, ac, (const char **)av);
    bu_vls_free(&slew_vec);

    if (ac != 1 && ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return ret;
}

static int
go_zap(struct ged *gedp, int argc, const char *argv[])
{
    int ret = ged_zap(gedp, argc, argv);

    if (ret == BRLCAD_OK) {
	go_refresh_all_views(go_current_gop);
    }

    return ret;
}

static int
go_zbuffer(struct ged *gedp, int argc, const char *argv[])
{
    int zbuffer;
    struct ged_dm_view *gdvp;
    static const char *usage = "vname [0|1]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get zbuffer flag */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_dmp->dm_zbuffer);
	return BRLCAD_OK;
    }

    /* set zbuffer flag */
    if (sscanf(argv[2], "%d", &zbuffer) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (zbuffer < 0)
	zbuffer = 0;
    else if (1 < zbuffer)
	zbuffer = 1;

    DM_SET_ZBUFFER(gdvp->gdv_dmp, zbuffer);
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

static int
go_zoom(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int ac;
    char *av[3];
    struct ged_dm_view *gdvp;
    static const char *usage = "vname sf";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    ac = 2;
    av[0] = (char *)argv[0];
    av[1] = (char *)argv[2];
    av[2] = (char *)0;
    ret = ged_zoom(gedp, ac, (const char **)av);
    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}


/*************************** Local Utility Functions ***************************/
static void
go_drawSolid(struct dm *dmp, struct solid *sp)
{
    if (sp->s_iflag == UP)
	DM_SET_FGCOLOR(dmp, 255, 255, 255, 0, sp->s_transparency);
    else
	DM_SET_FGCOLOR(dmp,
		       (unsigned char)sp->s_color[0],
		       (unsigned char)sp->s_color[1],
		       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

    DM_DRAW_VLIST(dmp, (struct bn_vlist *)&sp->s_vlist);
}

/* Draw all solids in the list */
static int
go_drawSList(struct dm *dmp, struct bu_list *hsp)
{
    struct solid *sp;
    int linestyle = -1;

    if (dmp->dm_transparency) {
	/* First, draw opaque stuff */
	FOR_ALL_SOLIDS(sp, hsp) {
	    if (sp->s_transparency < 1.0)
		continue;

	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, linestyle);
	    }

	    go_drawSolid(dmp, sp);
	}

	/* disable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 0);

	/* Second, draw transparent stuff */
	FOR_ALL_SOLIDS(sp, hsp) {
	    /* already drawn above */
	    if (sp->s_transparency == 1.0)
		continue;

	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, linestyle);
	    }

	    go_drawSolid(dmp, sp);
	}

	/* re-enable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 1);
    } else {

	FOR_ALL_SOLIDS(sp, hsp) {
	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, linestyle);
	    }

	    go_drawSolid(dmp, sp);
	}
    }

    return BRLCAD_OK;
}

#ifdef USE_FBSERV
static void
go_fbs_callback(genptr_t clientData)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientData;

    go_refresh_view(gdvp);
}

static int
go_close_fbs(struct ged_dm_view *gdvp)
{
    if (gdvp->gdv_fbs.fbs_fbp == FBIO_NULL)
	return TCL_OK;

    _fb_pgflush(gdvp->gdv_fbs.fbs_fbp);

    switch (gdvp->gdv_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
	    X24_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
#ifdef DM_TK
/* XXX TJM: not ready yet
   case DM_TYPE_TK:
   tk_close_existing(gdvp->gdv_fbs.fbs_fbp);
   break;
*/
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
	    ogl_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    wgl_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
    }

    /* free framebuffer memory */
    if (gdvp->gdv_fbs.fbs_fbp->if_pbase != PIXEL_NULL)
	free((void *)gdvp->gdv_fbs.fbs_fbp->if_pbase);
    free((void *)gdvp->gdv_fbs.fbs_fbp->if_name);
    free((void *)gdvp->gdv_fbs.fbs_fbp);
    gdvp->gdv_fbs.fbs_fbp = FBIO_NULL;

    return TCL_OK;
}

/*
 * Open/activate the display managers framebuffer.
 */
static int
go_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp)
{

    /* already open */
    if (gdvp->gdv_fbs.fbs_fbp != FBIO_NULL)
	return TCL_OK;

    /* don't use bu_calloc so we can fail slightly more gradefully */
    if ((gdvp->gdv_fbs.fbs_fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	Tcl_Obj	*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n",
			       (char *)NULL);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
    }

    switch (gdvp->gdv_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
	    *gdvp->gdv_fbs.fbs_fbp = X24_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/X")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/X", strlen("/dev/X")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _X24_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->pix,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->gc);
	    break;
#endif
#ifdef DM_TK
#if 0
/* XXX TJM implement _tk_open_existing */
	case DM_TYPE_TK:
	    *gdvp->gdv_fbs.fbs_fbp = tk_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/tk")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/tk", strlen("/dev/tk")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _tk_open_existing(gdvp->gdv_fbs.fbs_fbp,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			      ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->pix,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			      gdvp->gdv_dmp->dm_width,
			      gdvp->gdv_dmp->dm_height,
			      ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->gc);
	    break;
#endif
#endif

#ifdef DM_OGL
	case DM_TYPE_OGL:
	    *gdvp->gdv_fbs.fbs_fbp = ogl_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/ogl")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/ogl", strlen("/dev/ogl")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _ogl_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct ogl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc,
			       ((struct ogl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
			       0);
	    break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    *gdvp->gdv_fbs.fbs_fbp = wgl_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/wgl")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/wgl", strlen("/dev/wgl")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _wgl_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->hdc,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct wgl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc,
			       ((struct wgl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
			       0);
	    break;
#endif
    }

    return TCL_OK;
}
#endif

static void
go_refresh_view(struct ged_dm_view *gdvp)
{
    DM_DRAW_BEGIN(gdvp->gdv_dmp);

#ifdef USE_FBSERV
    if (gdvp->gdv_fbs.fbs_mode)
	fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		   gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);

    if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_OVERLAY) {
	DM_DRAW_END(gdvp->gdv_dmp);
	return;
    } else if (gdvp->gdv_fbs.fbs_mode < GED_OBJ_FB_MODE_INTERLAY) {
	DM_LOADMATRIX(gdvp->gdv_dmp, gdvp->gdv_view->gv_model2view, 0);
	go_drawSList(gdvp->gdv_dmp, &gdvp->gdv_gop->go_gedp->ged_gdp->gd_headSolid);
    }
#else
    DM_LOADMATRIX(gdvp->gdv_dmp, gdvp->gdv_view->gv_model2view, 0);
    go_drawSList(gdvp->gdv_dmp, &gdvp->gdv_gop->go_gedp->ged_gdp->gd_headSolid);
#endif

    /* Restore to non-rotated, full brightness */
    DM_NORMAL(gdvp->gdv_dmp);

#if 1
    /* Draw center dot */
    DM_SET_FGCOLOR(gdvp->gdv_dmp,
		   255, 255, 0, 1, 1.0);
#else
    /* Draw center dot */
    DM_SET_FGCOLOR(gdvp->gdv_dmp,
		   color_scheme->cs_center_dot[0],
		   color_scheme->cs_center_dot[1],
		   color_scheme->cs_center_dot[2], 1, 1.0);
#endif
    DM_DRAW_POINT_2D(gdvp->gdv_dmp, 0.0, 0.0);

    DM_DRAW_END(gdvp->gdv_dmp);
}

static void
go_refresh_all_views(struct ged_obj *gop)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	go_refresh_view(gdvp);
    }
}

static void
go_autoview_view(struct ged_dm_view *gdvp)
{
    int ret;
    char *av[2];

    gdvp->gdv_gop->go_gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "autoview";
    av[1] = (char *)0;
    ret = ged_autoview(gdvp->gdv_gop->go_gedp, 1, (const char **)av);

    if (ret == BRLCAD_OK)
	go_refresh_view(gdvp);
}

static void
go_autoview_all_views(struct ged_obj *gop)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	go_autoview_view(gdvp);
    }
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
