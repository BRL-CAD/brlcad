/*                          I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/**
 *
 * Functions for initializing Tcl environments and BRL-CAD's libtclcad
 * interface.
 *
 */


#include "common.h"

#define RESOURCE_INCLUDED 1
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif

#include "vmath.h"
#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "bn.h"
#include "dm.h"
#include "raytrace.h"
#include "tclcad.h"

/* Private headers */
#include "brlcad_version.h"
#include "./tclcad_private.h"


int
library_initialized(int setit)
{
    static int initialized = 0;
    if (setit)
	initialized = 1;

    return initialized;
}


static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

    return ctp->ct_func(interp, argc, argv);
}


void
tclcad_register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, wrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


/* avoid including itcl.h/itk.h due to their usage of internal headers */
extern int Itcl_Init(Tcl_Interp *);
#ifdef HAVE_TK
extern int Itk_Init(Tcl_Interp *);
#endif

int
tclcad_init(Tcl_Interp *interp, int init_gui, struct bu_vls *tlog)
{
    if (library_initialized(0))
	return TCL_OK;

    /* Tcl_Init needs init.tcl.  It can be tricky to find init.tcl - help out,
     * if we can.  Per the Tcl_Init() definition in generic/tclInterp.c in the
     * Tcl source code, setting tcl_library is the recommended way for embedding
     * applications to assist Tcl_Init in finding this file... */
    char libdir[MAXPATHLEN] = {0};
    bu_dir(libdir, MAXPATHLEN, BU_DIR_LIB, NULL);
    if (strlen(libdir)) {
	struct bu_vls lib_path = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&lib_path, "%s%ctcl%s/init.tcl", libdir, BU_DIR_SEPARATOR, TCL_VERSION);
	if (bu_file_exists(bu_vls_cstr(&lib_path), NULL)) {
	    struct bu_vls initpath = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&lib_path, "%s%ctcl%s", libdir, BU_DIR_SEPARATOR, TCL_VERSION);
	    bu_vls_printf(&initpath, "set tcl_library {%s}", bu_vls_cstr(&lib_path));
	    if (Tcl_Eval(interp, bu_vls_addr(&initpath))) {
		bu_log("Problem initializing tcl_library to init.tcl path: Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    }
	    bu_vls_free(&initpath);
	}
	bu_vls_free(&lib_path);
    }

    if (Tcl_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Tcl init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    if (init_gui) {
#ifdef HAVE_TK
	if (Tk_Init(interp) == TCL_ERROR) {
	    if (tlog)
		bu_vls_printf(tlog, "Tk init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
#endif
    }

    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
    tclcad_auto_path(interp);

    /* Initialize [incr Tcl] */
    if (Tcl_Eval(interp, "package require Itcl 3") != TCL_OK) {
	if (tlog)
	    bu_vls_printf(tlog, "Itcl init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize [incr Tk] */
    if (init_gui) {
#ifdef HAVE_TK
	if (Tcl_Eval(interp, "package require Itk 3") != TCL_OK) {
	    if (tlog)
	       	bu_vls_printf(tlog, "Itk init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}

	/* Initialize the Iwidgets package */
	if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
	    if (tlog)
	       	bu_vls_printf(tlog, "Iwidgets init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
#endif
    }

    /* Initialize libbu */
    if (Bu_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Bu_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize libbn */
    if (Bn_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Bn_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize librt */
    if (Rt_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Rt_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize libdm */
    if (Dm_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Dm_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize the GED object */
    if (Ged_Init(interp) == TCL_ERROR) {
	if (tlog)
	    bu_vls_printf(tlog, "Ged_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* initialize command history objects */
    Cho_Init(interp);

    Tcl_PkgProvide(interp, "Tclcad", brlcad_version());

    (void)library_initialized(1);

    /* Import Itcl/Itk and iwidgets into global namespace
     *
     * TODO - this is probably a bad idea - figure out why we're doing it and
     * whether we really need to... */

    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		"::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
	if (tlog)
	    bu_vls_printf(tlog, "Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    if (init_gui) {
#ifdef HAVE_TK
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		    "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
	    if (tlog)
	       	bu_vls_printf(tlog, "Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		    "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
	    if (tlog)
	       	bu_vls_printf(tlog, "Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
#endif
    }

    if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itcl::* }") != TCL_OK) {
	if (tlog)
	    bu_vls_printf(tlog, "Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

#ifdef HAVE_TK
    if (init_gui) {
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::tk::* }") != TCL_OK) {
	    if (tlog)
		bu_vls_printf(tlog, "Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itk::* }") != TCL_OK) {
	    if (tlog)
		bu_vls_printf(tlog, "Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
    }

    /* If we're doing Tk, make sure we have a window */
    if (init_gui) {
	Tk_Window tkwin = Tk_MainWindow(interp);
	if (!tkwin) return TCL_ERROR;
    }
#endif

    return TCL_OK;
}

/* This is here to allow tclsh to load the .so file and have it initialize.
 * Application code in BRL-CAD should call tclcad_init and specify GUI/no-GUI
 * and handle logging as appropriate. */
TCLCAD_EXPORT int
Tclcad_Init(Tcl_Interp *interp)
{
    return tclcad_init(interp, 1, NULL);
}

void
tclcad_set_argv(Tcl_Interp *interp, int argc, const char **argv)
{
    char buf[TCL_INTEGER_SPACE] = {0};
    char *args;
    Tcl_DString argString;
    char nstr = '\0';
    const char **av;

    if (!interp) return;

    /* create the tcl variables, even if they're empty */
    av = (!argv) ? (const char **)&nstr : argv;

    /* argc */
    sprintf(buf, "%ld", (long)(argc));
    Tcl_SetVar(interp, "argc", buf, TCL_GLOBAL_ONLY);

    /* argv */
    args = Tcl_Merge(argc, (const char * const *)av);
    Tcl_ExternalToUtfDString(NULL, args, -1, &argString);
    Tcl_SetVar(interp, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&argString);
    ckfree(args);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
