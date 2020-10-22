/*                       A U T O _ P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/**
 *
 * Locate the BRL-CAD tclscripts
 *
 */

#include "common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu/app.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "tclcad.h"

#define MAX_BUF 2048

/**
 * Set up the Tcl auto_path for locating various necessary BRL-CAD
 * scripting resources. Detect whether the current invocation is from
 * an installed binary or not and append to the auto_path accordingly
 * for where the needed tclscript resources should be found.
 */
void
tclcad_auto_path(Tcl_Interp *interp)
{

    if (!interp) {
	/* nothing to do */
	return;
    }

    /* Used for getenv calls */
    const char *env = NULL;


    /* If we are using an external Tcl, we need the
     * location of its init file */
    int tcl_set = 0;
#ifdef TCL_SYSTEM_INITTCL_PATH
    struct bu_vls initpath = BU_VLS_INIT_ZERO;
    bu_vls_printf(&initpath, "set tcl_library {%s}", TCL_SYSTEM_INITTCL_PATH);
    if (Tcl_Eval(interp, bu_vls_addr(&initpath))) {
	bu_log("Problem initializing tcl_library to system init.tcl path: Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
    } else {
	tcl_set = 1;
    }
    bu_vls_free(&initpath);
#endif

    struct bu_ptbl paths = BU_PTBL_INIT_ZERO;

    /* If TCLCAD_LIBRARY_PATH is set, add to active paths. */
    env = getenv("TCLCAD_LIBRARY_PATH");
    if (env) {
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	/* limit path length from env var for sanity */
	bu_vls_strncat(&buffer, env, MAX_BUF);
	const char *p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&paths, (long *)p);
	bu_vls_free(&buffer);
    }

    /* See if user set ITCL_LIBRARY override */
    int itcl_set = 0;
    env = getenv("ITCL_LIBRARY");
    if (env) {
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	bu_vls_strncat(&buffer, env, MAX_BUF);
	bu_vls_printf(&buffer, "%citcl.tcl", BU_DIR_SEPARATOR);
	if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
	    const char *p = bu_strdup(bu_vls_cstr(&buffer));
	    bu_ptbl_ins(&paths, (long *)p);
	    itcl_set = 1;
	} else {
	    bu_log("Warning: ITCL_LIBRARY environment variable is set to %s, but file itcl.tcl is not found in that directory, skipping.\n", env);
	}
	bu_vls_free(&buffer);
    }

    /* See if user set ITK_LIBRARY override */
    int itk_set = 0;
    env = getenv("ITK_LIBRARY");
    if (env) {
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	bu_vls_strncat(&buffer, env, MAX_BUF);
	bu_vls_printf(&buffer, "%citk.tcl", BU_DIR_SEPARATOR);
	if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
	    const char *p = bu_strdup(bu_vls_cstr(&buffer));
	    bu_ptbl_ins(&paths, (long *)p);
	} else {
	    bu_log("Warning: ITK_LIBRARY environment variable is set to %s, but file itk.tcl is not found in that directory, skipping.\n", env);
	}
	bu_vls_free(&buffer);
    }

    /* If tcl_library is defined in the interp, capture it for addition */
    {
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	/* limit path length from env var for sanity */
	bu_vls_sprintf(&buffer, "set tcl_library");
	Tcl_Eval(interp, bu_vls_cstr(&buffer));
	bu_vls_sprintf(&buffer, "%s", Tcl_GetStringResult(interp));
	if (bu_vls_strlen(&buffer)) {
	    const char *p = bu_strdup(bu_vls_cstr(&buffer));
	    bu_ptbl_ins(&paths, (long *)p);
	}
	bu_vls_free(&buffer);
    }

    /* Set up the subdirectories of interest.  Some are static, but some are
     * version specific - we construct a table of all of them for easier
     * use in subsequent testing. */
    struct bu_ptbl lib_subpaths = BU_PTBL_INIT_ZERO;
    struct bu_ptbl data_subpaths = BU_PTBL_INIT_ZERO;
    {
	const char *p = NULL;
	struct bu_vls buffer = BU_VLS_INIT_ZERO;

	bu_vls_sprintf(&buffer, "tcl%s", TCL_VERSION);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "itcl%s", ITCL_VERSION);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

#ifdef HAVE_TK
	bu_vls_sprintf(&buffer, "tk%s", TK_VERSION);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "itk%s", ITCL_VERSION);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "Iwidgets%s", IWIDGETS_VERSION);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "Tkhtml3.0");
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "Tktable2.10");
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&lib_subpaths, (long *)p);
#endif

	bu_vls_sprintf(&buffer, "tclscripts");
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%clib", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%cutil", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%cmged", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%cgeometree", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%crtwizard", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%carcher", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%cboteditor", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%cchecker", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);

	bu_vls_sprintf(&buffer, "tclscripts%clod", BU_DIR_SEPARATOR);
	p = bu_strdup(bu_vls_cstr(&buffer));
	bu_ptbl_ins(&data_subpaths, (long *)p);
    }

    // First off, see what's in BU_DIR_LIB.
    char libdir[MAXPATHLEN] = {0};
    struct bu_vls lib_path = BU_VLS_INIT_ZERO;
    bu_dir(libdir, MAXPATHLEN, BU_DIR_LIB, NULL);
    {
	struct bu_ptbl found_subpaths = BU_PTBL_INIT_ZERO;
	if (strlen(libdir)) {
	    // Have a library directory, see what's in it
	    for (size_t i = 0; i < BU_PTBL_LEN(&lib_subpaths); i++) {
		const char *fname = (const char *)BU_PTBL_GET(&lib_subpaths, i);
		bu_vls_sprintf(&lib_path, "%s%c%s", libdir, BU_DIR_SEPARATOR, fname);
		if (bu_file_exists(bu_vls_cstr(&lib_path), NULL)) {
		    // Have a path
		    const char *p = bu_strdup(bu_vls_cstr(&lib_path));
		    bu_ptbl_ins(&paths, (long *)p);
		    bu_ptbl_ins(&found_subpaths, (long *)fname);
		}
	    }
	    // Clear anything we found out of lib_subpaths - we don't need or
	    // want to find it again.  The more local to the install, the
	    // better...
	    for (size_t i = 0; i < BU_PTBL_LEN(&found_subpaths); i++) {
		bu_ptbl_rm(&lib_subpaths, BU_PTBL_GET(&found_subpaths, i));
	    }
	}
	bu_ptbl_free(&found_subpaths);
    }

    // Now that we've looked for the libs, handle the data dirs.
    struct bu_vls data_path = BU_VLS_INIT_ZERO;
    char datadir[MAXPATHLEN] = {0};
    bu_dir(datadir, MAXPATHLEN, BU_DIR_DATA, NULL);
    if (strlen(datadir)) {
	// Have a directory, see what's in it
	for (size_t i = 0; i < BU_PTBL_LEN(&data_subpaths); i++) {
	    const char *fname = (const char *)BU_PTBL_GET(&data_subpaths, i);
	    bu_vls_sprintf(&data_path, "%s%c%s", datadir, BU_DIR_SEPARATOR, fname);
	    if (bu_file_exists(bu_vls_cstr(&data_path), NULL)) {
		// Have a path
		const char *p = bu_strdup(bu_vls_cstr(&data_path));
		bu_ptbl_ins(&paths, (long *)p);
	    } else {
		bu_log("Warning: data path %s is not present in directory %s\n", fname, datadir);
	    }
	}
    }

    /* Iterate over the paths set and modify the real Tcl auto_path */
    int tk_set = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&paths); i++) {
	const char *path = (const char *)BU_PTBL_GET(&paths, i);
	/* make sure it exists before appending */
	if (bu_file_exists(path, NULL)) {
	    //printf("APPENDING: %s\n", path);
	    struct bu_vls lappend = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&lappend, "lappend auto_path {%s}", path);
	    (void)Tcl_Eval(interp, bu_vls_addr(&lappend));
	    bu_vls_free(&lappend);
	} else {
	    //printf("NOT APPENDING: %s\n", path);
	    continue;
	}

	/* Look for a number of special case initializations */

	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	/* specifically look for init.tcl so we can set tcl_library */
	if (!tcl_set) {
	    bu_vls_sprintf(&buffer, "%s%cinit.tcl", path, BU_DIR_SEPARATOR);
	    if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
		/* this really sets it */
		bu_vls_sprintf(&buffer, "set tcl_library {%s}", path);
		if (Tcl_Eval(interp, bu_vls_cstr(&buffer))) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    tcl_set=1;
		}

		/* extra measures necessary for "create interp": determine if
		 * TCL_LIBRARY is set, and set it if not. */
		env = getenv("TCL_LIBRARY");
		if (!env) {
		    /* this REALLY sets it */
		    bu_vls_sprintf(&buffer, "set env(TCL_LIBRARY) {%s}", path);
		    if (Tcl_Eval(interp, bu_vls_cstr(&buffer))) {
			bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		    }
		}
	    }
	}

	/* specifically look for tk.tcl so we can set tk_library */
	if (!tk_set) {
	    bu_vls_sprintf(&buffer, "%s%ctk.tcl", path, BU_DIR_SEPARATOR);
	    if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
		/* this really sets it */
		bu_vls_sprintf(&buffer, "set tk_library {%s}", path);
		if (Tcl_Eval(interp, bu_vls_cstr(&buffer))) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    tk_set=1;
		}
	    }
	}

	/* specifically look for itcl.tcl so we can set ITCL_LIBRARY */
	if (!itcl_set) {
	    bu_vls_sprintf(&buffer, "%s%citcl.tcl", path, BU_DIR_SEPARATOR);
	    if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
		/* this really sets it */
		bu_vls_sprintf(&buffer, "set env(ITCL_LIBRARY) {%s}", path);
		if (Tcl_Eval(interp, bu_vls_cstr(&buffer))) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    itcl_set=1;
		}
	    }
	}

	/* specifically look for itk.tcl so we can set ITK_LIBRARY */
	if (!itk_set) {
	    bu_vls_sprintf(&buffer, "%s%citk.tcl", path, BU_DIR_SEPARATOR);
	    if (bu_file_exists(bu_vls_cstr(&buffer), NULL)) {
		/* this really sets it */
		bu_vls_sprintf(&buffer, "set env(ITK_LIBRARY) {%s}", path);
		if (Tcl_Eval(interp, bu_vls_cstr(&buffer))) {
		    bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
		} else {
		    itk_set=1;
		}
	    }
	}

	bu_vls_free(&buffer);
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&lib_subpaths); i++) {
	char *str = (char *)BU_PTBL_GET(&lib_subpaths, i);
	bu_free(str, "subpath string");
    }
    bu_ptbl_free(&lib_subpaths);
    for (size_t i = 0; i < BU_PTBL_LEN(&data_subpaths); i++) {
	char *str = (char *)BU_PTBL_GET(&data_subpaths, i);
	bu_free(str, "subpath string");
    }
    bu_ptbl_free(&data_subpaths);
    for (size_t i = 0; i < BU_PTBL_LEN(&paths); i++) {
	char *str = (char *)BU_PTBL_GET(&paths, i);
	bu_free(str, "subpath string");
    }
    bu_ptbl_free(&paths);

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
