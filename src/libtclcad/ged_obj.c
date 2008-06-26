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

#if 1
/*XXX Temporary */
#include "dg.h"
#endif

static struct ged_obj HeadGedObj;
static int ged_open_tcl(ClientData	clientData,
			Tcl_Interp	*interp,
			int	argc,
			const char	**argv);

static struct bu_cmdtab ged_cmds[] = {
    {"arced",		ged_arced},
    {"adjust",		ged_adjust},
    {"attr",		ged_attr},
    {"comb_color",	ged_comb_color},
    {"edcomb",		ged_edcomb},
    {"edmater",		ged_edmater},
    {"form",		ged_form},
    {"get",		ged_get},
    {"item",		ged_item},
    {"l",		ged_list},
    {"listeval",	ged_pathsum},
    {"log",		ged_log},
    {"ls",		ged_ls},
    {"make",		ged_make},
    {"make_name",	ged_make_name},
    {"mater",		ged_mater},
    {"mirror",		ged_mirror},
#if GED_USE_RUN_RT
    {"nirt",		ged_nirt},
#endif
    {"ocenter",		ged_ocenter},
    {"orotate",		ged_orotate},
    {"oscale",		ged_oscale},
    {"otranslate",	ged_otranslate},
    {"paths",		ged_pathsum},
    {"put",		ged_put},
    {"rmater",		ged_rmater},
    {"shader",		ged_shader},
    {"wmater",		ged_wmater},
    {(char *)NULL,	(int (*)())0}
};


/**
 * @brief create the Tcl command for ged_open
 *
 */
int
Ged_Init(Tcl_Interp *interp)
{
    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;

    BU_LIST_INIT(&HeadGedObj.l);
    (void)Tcl_CreateCommand(interp, (const char *)"ged_open", ged_open_tcl,
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
 *			G E D _ C M D
 *@brief
 * Generic interface for database commands.
 *
 * @par Usage:
 *        procname cmd ?args?
 *
 * @return result of ged command.
 */
static int
ged_cmd(ClientData	clientData,
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
	for (ctp = ged_cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->ct_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    for (ctp = ged_cmds; ctp->ct_name != (char *)0; ctp++) {
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

	for (ctp = ged_cmds; ctp->ct_name != (char *)NULL; ctp++) {
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

    if (ret == GED_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
ged_deleteProc(ClientData clientData)
{
    struct ged_obj *gop = (struct ged_obj *)clientData;

#if 0
    GED_CHECK_OBJ(gop);
#endif
    BU_LIST_DEQUEUE(&gop->l);
    bu_vls_free(&gop->go_name);
    ged_close(gop->go_gedp);
    bu_free((genptr_t)gop, "struct ged_obj");
}

/**
 * @brief
 * Create a command named "oname" in "interp" using "gedp" as its state.
 *
 */
int
ged_create_cmd(Tcl_Interp	*interp,
	       struct ged_obj	*gop,	/* pointer to object */
	       const char	*oname)	/* object name */
{
    if (gop == GED_OBJ_NULL) {
	Tcl_AppendResult(interp, "ged_create_cmd ", oname, " failed", NULL);
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of gop */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(interp, oname, (Tcl_CmdProc *)ged_cmd,
			    (ClientData)gop, ged_deleteProc);

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
ged_init_obj(Tcl_Interp		*interp,
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
 *	set gop [ged_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n	ged_open db file "bob.g"
 *@n	db get white.r
 *@n	db close
 */
static int
ged_open_tcl(ClientData	clientData,
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
Usage: ged_open\n\
       ged_open newprocname file filename\n\
       ged_open newprocname disk $dbip\n\
       ged_open newprocname disk_append $dbip\n\
       ged_open newprocname inmem $dbip\n\
       ged_open newprocname inmem_append $dbip\n\
       ged_open newprocname db filename\n\
       ged_open newprocname filename\n",
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

    /* append to list of ged_obj */
    BU_LIST_APPEND(&HeadGedObj.l, &gop->l);

#if 0
    if ((ret = ged_init_obj(interp, gop, argv[1])) != TCL_OK)
	return ret;
#endif

    return ged_create_cmd(interp, gop, argv[1]);
}


/****************** GED Object Methods ********************/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
