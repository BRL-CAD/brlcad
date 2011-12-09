/*                T C L C A D A U T O P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *
 */
/** @file libtclcad/tclcadAutoPath.c
 *
 * Locate the BRL-CAD tclscripts
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tcl.h"

#ifdef HAVE_TK
#  include "tk.h"
#endif

#ifndef ITCL_VERSION
#  include "itcl.h"
#endif

#ifdef HAVE_TK
# ifndef ITK_VERSION
#  include "itk.h"
# endif
#endif

/* incrTcl prior to 3.3 doesn't provide ITK_VERSION */
#ifndef ITK_VERSION
#  define ITK_VERSION ITCL_VERSION
#endif

#include "bu.h"
#include "tclcad.h"

#define MAX_BUF 2048

/* FIXME: we utilize this Tcl internal in here */
#if !defined(_WIN32) || defined(__CYGWIN__)
extern Tcl_Obj *TclGetLibraryPath (void);
#endif


/* helper routine to determine whether the full 'path' includes a
 * directory named 'src'.  this is used to determine whether a
 * particular invocation is being run from the BRL-CAD source
 * directories or from some install directory.
 *
 * returns a pointer to the subpath that contains the 'src' directory.
 * e.g. provided /some/path/to/src/dir/blah will return /some/path/to
 */
static const char *
path_to_src(const char *path)
{
    static char buffer[MAX_BUF] = {0};
    char *match = NULL;

    if (!path) {
	return NULL;
    }
    if (strlen(path)+2 > MAX_BUF) {
	/* path won't fit */
	return NULL;
    }

    snprintf(buffer, MAX_BUF, "%s%c", path, BU_DIR_SEPARATOR);

    match = strstr(buffer, "/src/");
    if (match) {
	*(match) = '\0';
	return buffer;
    }
    return NULL;
}

/**
 * Set up the Tcl auto_path for locating various necessary BRL-CAD
 * scripting resources. Detect whether the current invocation is from
 * an installed binary or not and append to the auto_path accordingly
 * for where the needed tclscript resources should be found.
 *
 ** installed invocation paths
 * BRLCAD_ROOT/lib/tclTCL_VERSION/init.tcl
 * BRLCAD_ROOT/lib/tclTK_VERSION/tk.tcl
 * BRLCAD_ROOT/lib/itclITCL_VERSION/itcl.tcl
 * BRLCAD_ROOT/lib/itkITK_VERSION/itk.tcl
 * BRLCAD_ROOT/lib/iwidgetsIWIDGETS_VERSION/iwidgets.tcl
 * BRLCAD_DATA/tclscripts/pkgIndex.tcl and subdirs
 *
 ** source invocation paths
 * src/other/tcl/library/init.tcl
 * src/other/tk/library/tk.tcl
 * src/other/incrTcl/itcl/library/itcl.tcl
 * src/other/incrTcl/itk/library/itk.tcl
 * src/other/iwidgets/library/iwidgets.tcl
 * src/tclscripts/pkgIndex.tcl and subdirs
 *
 * if TCLCAD_LIBRARY_PATH is set
 *   append to search path
 * get installation directory and invocation path
 * if being run from installation directory
 *   add installation paths to search path
 * if being run from source directory
 *   add source paths to search path
 * add installation paths to search path
 */
void
tclcad_auto_path(Tcl_Interp *interp)
{
    struct bu_vls auto_path;
    struct bu_vls lappend;
    const char *library_path = NULL;

    struct bu_vls root_buf;
    const char *root = NULL;
    const char *data = NULL;
    char buffer[MAX_BUF] = {0};

    const char *which_argv = NULL;
    const char *srcpath = NULL;
    int from_installed = 0;

    int found_init_tcl = 0;
    int found_tk_tcl = 0;
    int found_itcl_tcl = 0;
    int found_itk_tcl = 0;

    char pathsep[2] = { BU_PATH_SEPARATOR, '\0' };

    if (!interp) {
	/* nothing to do */
	return;
    }

    bu_vls_init(&root_buf);
    root = bu_brlcad_root("", 1);
    bu_vls_printf(&root_buf, "%s", root);
    root = bu_vls_addr(&root_buf);
    data = bu_brlcad_data("", 1);

    bu_vls_init(&auto_path);
    bu_vls_init(&lappend);

    /* determine if TCLCAD_LIBRARY_PATH is set */
    library_path = getenv("TCLCAD_LIBRARY_PATH");
    if (library_path) {
	/* it is set, set auto_path. limit buf just because. */
	bu_vls_strncat(&auto_path, library_path, MAX_BUF);
    }

    /* make sure tcl_library path is in the auto_path */
    snprintf(buffer, MAX_BUF, "set tcl_library");
    Tcl_Eval(interp, buffer);
    bu_vls_strncat(&auto_path, Tcl_GetStringResult(interp), MAX_BUF);
    
    /* get string of invocation binary */
    which_argv = bu_which(bu_argv0_full_path());
    if (!which_argv) {
	which_argv = bu_argv0_full_path();
    }

    /* get name of installation binary */
    snprintf(buffer, MAX_BUF, "%s%cbin%c%s", root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, bu_getprogname());

    /* are we running from an installed binary? if so add to path */
    if (bu_file_exists(buffer) && bu_same_file(buffer, which_argv)) {
	from_installed = 1;
	bu_vls_printf(&auto_path, "%c%s%clib",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%clib%ctcl%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, TCL_VERSION);
#ifdef HAVE_TK
	bu_vls_printf(&auto_path, "%c%s%clib%ctk%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, TK_VERSION);
#endif
	bu_vls_printf(&auto_path, "%c%s%clib%citcl%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, ITCL_VERSION);
#ifdef HAVE_TK
	bu_vls_printf(&auto_path, "%c%s%clib%citk%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, ITK_VERSION);
	bu_vls_printf(&auto_path, "%c%s%clib%ciwidgets%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, IWIDGETS_VERSION);
#endif
	bu_vls_printf(&auto_path, "%c%s%ctclscripts",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%clib",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cutil",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cmged",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cgeometree",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%crtwizard",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%carcher",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
    }

    /* are we running uninstalled? */
    srcpath = path_to_src(which_argv);

    /* add search paths for source invocation */
    if (srcpath) {
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%ctcl%cunix",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%ctcl%clibrary",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%ctk%cunix",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%ctk%clibrary",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%cincrTcl",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%cincrTcl%citcl%clibrary",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%cincrTcl%citk%clibrary",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%cother%ciwidgets",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%clib",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%cutil",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%cmged",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%cgeometree",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%crtwizard",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%csrc%ctclscripts%carcher",
		      BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
    }

    /* add search paths for dist invocation */
    if (srcpath) {
	snprintf(buffer, MAX_BUF, "%s%c..%csrc%cother%ctcl%cunix",
		 srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	if (bu_file_exists(buffer)) {
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%ctcl%cunix",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%ctcl%clibrary",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%ctk%cunix",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%ctk%clibrary",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%cincrTcl",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%cincrTcl%citcl%clibrary",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%cincrTcl%citk%clibrary",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%cother%ciwidgets",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%clib",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%cutil",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%cmged",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%cgeometree",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%crtwizard",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	    bu_vls_printf(&auto_path, "%c%s%c..%csrc%ctclscripts%carcher",
			  BU_PATH_SEPARATOR, srcpath, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	}
    }

    /* be sure to check installation paths even if we aren't running from there */
    if (!from_installed) {
	bu_vls_printf(&auto_path, "%c%s%clib",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%clib%ctcl%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, TCL_VERSION);
#ifdef HAVE_TK
	bu_vls_printf(&auto_path, "%c%s%clib%ctk%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, TK_VERSION);
#endif
	bu_vls_printf(&auto_path, "%c%s%clib%citcl%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, ITCL_VERSION);
#ifdef HAVE_TK
	bu_vls_printf(&auto_path, "%c%s%clib%citk%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, ITK_VERSION);
	bu_vls_printf(&auto_path, "%c%s%clib%ciwidgets%s",
		      BU_PATH_SEPARATOR, root, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, IWIDGETS_VERSION);
#endif
	bu_vls_printf(&auto_path, "%c%s%ctclscripts",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%clib",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cutil",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cmged",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%cgeometree",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%crtwizard",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
	bu_vls_printf(&auto_path, "%c%s%ctclscripts%carcher",
		      BU_PATH_SEPARATOR, data, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR);
    }

    /*    printf("AUTO_PATH IS %s\n", bu_vls_addr(&auto_path)); */

    /* see if user already set ITCL_LIBRARY override */
    library_path = getenv("ITCL_LIBRARY");
    if (!found_itcl_tcl && library_path) {
	snprintf(buffer, MAX_BUF, "%s%citcl.tcl", library_path, BU_DIR_SEPARATOR);
	if (bu_file_exists(buffer)) {
	    found_itcl_tcl=1;
	}
    }

    /* see if user already set ITK_LIBRARY override */
    library_path = getenv("ITK_LIBRARY");
    if (!found_itk_tcl && library_path) {
	snprintf(buffer, MAX_BUF, "%s%citk.tcl", library_path, BU_DIR_SEPARATOR);
	if (bu_file_exists(buffer)) {
	    found_itk_tcl=1;
	}
    }

    /* iterate over the auto_path list and modify the real Tcl auto_path */
    for (srcpath = strtok(bu_vls_addr(&auto_path), pathsep);
	 srcpath;
	 srcpath = strtok(NULL, pathsep)) {

	/* make sure it exists before appending */
	if (bu_file_exists(srcpath)) {
	    /*		printf("APPENDING: %s\n", srcpath); */
	    bu_vls_sprintf(&lappend, "lappend auto_path {%s}", srcpath);
	    (void)Tcl_Eval(interp, bu_vls_addr(&lappend));
	} else {
	    /*		printf("NOT APPENDING: %s\n", srcpath); */
	    continue;
	}

	/* specifically look for init.tcl so we can set tcl_library */
	if (!found_init_tcl) {
	    snprintf(buffer, MAX_BUF, "%s%cinit.tcl", srcpath, BU_DIR_SEPARATOR);
	    if (bu_file_exists(buffer)) {
		/* this really sets it */
		snprintf(buffer, MAX_BUF, "set tcl_library {%s}", srcpath);
		if (Tcl_Eval(interp, buffer)) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    found_init_tcl=1;
		}

		/* extra measures necessary for "create interp":
		 * determine if TCL_LIBRARY is set, and set it if not.
		 */
		library_path = getenv("TCL_LIBRARY");
		if (!library_path) {
		    /* this REALLY sets it */
		    snprintf(buffer, MAX_BUF, "set env(TCL_LIBRARY) {%s}", srcpath);
		    if (Tcl_Eval(interp, buffer)) {
			bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		    }
		}
	    }
	}

	/* specifically look for tk.tcl so we can set tk_library */
	if (!found_tk_tcl) {
	    snprintf(buffer, MAX_BUF, "%s%ctk.tcl", srcpath, BU_DIR_SEPARATOR);
	    if (bu_file_exists(buffer)) {
		/* this really sets it */
		snprintf(buffer, MAX_BUF, "set tk_library {%s}", srcpath);
		if (Tcl_Eval(interp, buffer)) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    found_tk_tcl=1;
		}
	    }
	}

	/* specifically look for itcl.tcl so we can set ITCL_LIBRARY */
	if (!found_itcl_tcl) {
	    snprintf(buffer, MAX_BUF, "%s%citcl.tcl", srcpath, BU_DIR_SEPARATOR);
	    if (bu_file_exists(buffer)) {
		/* this really sets it */
		snprintf(buffer, MAX_BUF, "set env(ITCL_LIBRARY) {%s}", srcpath);
		if (Tcl_Eval(interp, buffer)) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    found_itcl_tcl=1;
		}
	    }
	}

	/* specifically look for itk.tcl so we can set ITK_LIBRARY */
	if (!found_itk_tcl) {
	    snprintf(buffer, MAX_BUF, "%s%citk.tcl", srcpath, BU_DIR_SEPARATOR);
	    if (bu_file_exists(buffer)) {
		/* this really sets it */
		snprintf(buffer, MAX_BUF, "set env(ITK_LIBRARY) {%s}", srcpath);
		if (Tcl_Eval(interp, buffer)) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    found_itk_tcl=1;
		}
	    }
	}
    }

    which_argv = NULL;
    bu_vls_free(&auto_path);
    bu_vls_free(&lappend);
    bu_vls_free(&root_buf);

    return;
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
