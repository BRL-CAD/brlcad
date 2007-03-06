/*                T C L C A D A U T O P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file tclcadAutoPath.c
 *
 * Locate the BRL-CAD tclscripts
 *
 * Author --
 *   Christopher Sean Morrison
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "tcl.h"
#include "tk.h"
#include "itcl.h"
#include "itk.h"
/*#include "blt.h"*/

/* incrTcl prior to 3.3 doesn't provide ITK_VERSION */
#ifndef ITK_VERSION
#  define ITK_VERSION ITCL_VERSION
#endif

#include "machine.h"
#include "bu.h"

#define MAX_BUF 1024

/* #ifdef _WIN32 */
/*     { */
/* 	/\* XXX - nasty little hack to convert paths *\/ */
/* 	int i; */

/* 	strcat(pathname,"/"); */
/* 	for (i = 0; i < strlen(pathname); i++) { */
/* 	    if (pathname[i]=='\\') */
/* 		pathname[i]='/'; */
/* 	} */
/*     } */
/* #endif */


static char *path_to_src_buf = NULL;

static void
free_pts_buf()
{
    if (path_to_src_buf) {
	bu_free(path_to_src_buf, "deallocate path_to_src_buf");
	path_to_src_buf = NULL;
    }
}


/* helper routine to determine whether 'path' includes a directory
 * named 'src'.  this is used to determine whether a particular
 * invocation is being run from the BRL-CAD source directories.
 */
static const char *
path_to_src(const char *path)
{
    const char *name;
    const char *subpath;

    if (!path) {
	return NULL;
    }
    free_pts_buf();

    path_to_src_buf = bu_strdupm(path, "allocate path_to_src_buf");
    atexit(free_pts_buf);

    subpath = path_to_src_buf;
    do {
	char *temp = bu_strdup(subpath);
	name = bu_basename(temp);
	bu_free(temp, "bu_strdup temp");
	subpath = bu_dirname(subpath);
    } while (name &&
	     (strlen(subpath) > 1) &&
	     (strcmp(name, "src") != 0));

    if (strcmp(name, "src") == 0) {
	return subpath;
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
 * BRLCAD_ROOT/lib/bltBLT_VERSION/pkgIndex.tcl
 * BRLCAD_DATA/tclscripts/pkgIndex.tcl and subdirs
 *
 ** source invocation paths
 * src/other/tcl/library/init.tcl
 * src/other/tk/library/tk.tcl
 * src/other/incrTcl/itcl/library/itcl.tcl
 * src/other/incrTcl/itk/library/itk.tcl
 * src/other/iwidgets/library/iwidgets.tcl
 * src/other/blt/pkgIndex.tcl
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

    const char *root = bu_brlcad_root("", 1);
    const char *data = bu_brlcad_data("", 1);
    char buffer[MAX_BUF] = {0};

    char *which_argv[2] = {NULL, NULL};
    int from_installed = 0;
    const char *srcpath = NULL;
    char *stp = NULL;

    bu_vls_init(&auto_path);
    bu_vls_init(&lappend);

    /* determine if TCLCAD_LIBRARY_PATH is set */
    library_path = getenv("TCLCAD_LIBRARY_PATH");
    if (library_path) {
	/* it is set, set auto_path. limit buf just because. */
	bu_vls_strncat(&auto_path, library_path, MAX_BUF);
    }

    /* get string of invocation binary */
    (void)bu_which(which_argv, 2, bu_argv0(NULL));

    /* get name of installation binary */
    snprintf(buffer, MAX_BUF, "%s/bin/%s", root, bu_getprogname());

    /* are we running from an installed binary? if so add to path */
    if (bu_file_exists(buffer) && bu_same_file(buffer, which_argv[0])) {
	from_installed = 1;
	bu_vls_printf(&auto_path, ":%s/lib", root);
	bu_vls_printf(&auto_path, ":%s/lib/tcl%s", root, TCL_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/tk%s", root, TK_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/itcl%s", root, ITCL_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/itk%s", root, ITK_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/iwidgets%s", root, IWIDGETS_VERSION);
	/*	bu_vls_printf(&auto_path, ":%s/lib/blt%s", root, BLT_VERSION); */
	bu_vls_printf(&auto_path, ":%s/tclscripts", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/lib", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/util", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/mged", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/geometree", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/rtwizard", data);
    }

    /* add search paths for source invocation */
    srcpath = path_to_src(which_argv[0]);
    if (srcpath) {
	bu_vls_printf(&auto_path, ":%s/src/other/tcl/unix", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/tcl/library", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/tk/unix", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/tk/library", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/incrTcl", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/incrTcl/itcl/library", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/incrTcl/itk/library", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/other/iwidgets/library", srcpath);
	/*	bu_vls_printf(&auto_path, ":%s/src/other/blt/library", srcpath); */
	bu_vls_printf(&auto_path, ":%s/src/tclscripts", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/tclscripts/lib", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/tclscripts/util", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/tclscripts/mged", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/tclscripts/geometree", srcpath);
	bu_vls_printf(&auto_path, ":%s/src/tclscripts/rtwizard", srcpath);
    }

    /* be sure to check installation paths even if we aren't running from there */
    if (!from_installed) {
	bu_vls_printf(&auto_path, ":%s/lib", root);
	bu_vls_printf(&auto_path, ":%s/lib/tcl%s", root, TCL_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/tk%s", root, TK_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/itcl%s", root, ITCL_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/itk%s", root, ITK_VERSION);
	bu_vls_printf(&auto_path, ":%s/lib/iwidgets%s", root, IWIDGETS_VERSION);
	/*	bu_vls_printf(&auto_path, ":%s/lib/blt%s", root, BLT_VERSION); */
	bu_vls_printf(&auto_path, ":%s/tclscripts", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/lib", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/util", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/mged", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/geometree", data);
	bu_vls_printf(&auto_path, ":%s/tclscripts/rtwizard", data);
    }

    /* iterate over the auto_path list and modify the real Tcl auto_path */
    for (srcpath = strtok_r(bu_vls_addr(&auto_path), ":", &stp);
	 srcpath;
	 srcpath = strtok_r(NULL, ":", &stp)) {
	bu_vls_sprintf(&lappend, "lappend auto_path \"%s\"", srcpath);
	(void)Tcl_Eval(interp, bu_vls_addr(&lappend));
    }

    bu_vls_free(&auto_path);
    bu_vls_free(&lappend);

    return;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
