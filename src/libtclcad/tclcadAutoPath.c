/*                T C L C A D A U T O P A T H . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
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

#include "machine.h"
#include "bu.h"


void
tclcad_auto_path(Tcl_Interp *interp)
{
    struct bu_vls str;
    char *pathname;

    /* Locate the BRL-CAD-specific Tcl scripts quietly. */
    pathname = bu_brlcad_data( "tclscripts", 1 );

#ifdef _WIN32
    {
	/* XXX - nasty little hack to convert paths */
	int i;
	strcat(pathname,"/");
	for (i=0;i<strlen(pathname);i++) {
	    if(pathname[i]=='\\')
		pathname[i]='/'; }
    }
#endif

    bu_vls_init(&str);
    if (pathname) {
	bu_vls_printf(&str, "lappend auto_path \"%s\" \"%s/lib\" \"%s/util\" \"%s/mged\" \"%s/geometree\"",
		      pathname, pathname, pathname, pathname, pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	/* hunt for the tclscripts since we're probably just not installed yet */
	bu_log("WARNING: BRL-CAD %s is not apparently installed yet.\n", BRLCAD_VERSION);
	if (bu_file_exists("./tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path tclscripts tclscripts/lib tclscripts/util tclscripts/mged tclscripts/geometree");
	} else if (bu_file_exists("../tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../tclscripts ../tclscripts/lib ../tclscripts/util ../tclscripts/mged ../tclscripts/geometree");
	} else if (bu_file_exists("src/tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path src/tclscripts src/tclscripts/lib src/tclscripts/util src/tclscripts/mged src/tclscripts/geometree");
	} else if (bu_file_exists("../src/tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../src/tclscripts ../src/tclscripts/lib ../src/tclscripts/util ../src/tclscripts/mged ../src/tclscripts/geometree");
	} else if (bu_file_exists("../../src/tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../src/tclscripts ../../src/tclscripts/lib ../../src/tclscripts/util ../../src/tclscripts/mged ../../src/tclscripts/geometree");
	} else if (bu_file_exists("../../../src/tclscripts")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../../src/tclscripts ../../../src/tclscripts/lib ../../../src/tclscripts/util ../../../src/tclscripts/mged ../../../src/tclscripts/geometree");
	} else {
	    /* fail noisily */
	    (void)bu_brlcad_data("tclscripts", 0);
	}
    }

    /* make sure tcl's scripts are in the path */
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "lib/tcl%s", TCL_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );
    if (pathname) {
	bu_vls_printf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_printf(&str, "set tcl_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	/* hunt for the tclscripts since we're probably just not installed yet */
	if (bu_file_exists("./other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ./other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library ./other/libtcl/library");
	} else if (bu_file_exists("../other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library ../other/libtcl/library");
	} else if (bu_file_exists("src/other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path src/other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library src/other/libtcl/library");
	} else if (bu_file_exists("../src/other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../src/other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library ../src/other/libtcl/library");
	} else if (bu_file_exists("../../src/other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../src/other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library ../../src/other/libtcl/library");
	} else if (bu_file_exists("../../../src/other/libtcl/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../../src/other/libtcl/library");
	    (void)Tcl_Eval(interp, "set tcl_library ../../../src/other/libtcl/library");
	} else {
	    /* fail noisily */
	    bu_vls_printf(&str, "lib/tcl%s", TCL_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
    }
    bu_vls_printf(&str, "set tcl_version %s", TCL_VERSION);
    (void)Tcl_Eval(interp, bu_vls_addr(&str));


    /* make sure tk's scripts are in the path */
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "lib/tk%s", TK_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );
    if (pathname) {
	bu_vls_printf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_printf(&str, "set tk_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	/* hunt for the tclscripts since we're probably just not installed yet */
	if (bu_file_exists("./other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ./other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library ./other/libtk/library");
	} else if (bu_file_exists("../other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library ../other/libtk/library");
	} else if (bu_file_exists("src/other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path src/other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library src/other/libtk/library");
	} else if (bu_file_exists("../src/other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../src/other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library ../src/other/libtk/library");
	} else if (bu_file_exists("../../src/other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../src/other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library ../../src/other/libtk/library");
	} else if (bu_file_exists("../../../src/other/libtk/library")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../../src/other/libtk/library");
	    (void)Tcl_Eval(interp, "set tk_library ../../../src/other/libtk/library");
	} else {
	    /* fail noisily */
	    bu_vls_printf(&str, "lib/tk%s", TK_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
    }
    bu_vls_printf(&str, "set tk_version %s", TK_VERSION);
    (void)Tcl_Eval(interp, bu_vls_addr(&str));

    /* make sure iwidget's scripts are in the path */
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "lib/iwidgets%s", IWIDGETS_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );
    if (pathname) {
	bu_vls_printf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_printf(&str, "set iwidgets_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	/* hunt for the tclscripts since we're probably just not installed yet */
	if (bu_file_exists("./other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ./other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library ./other/iwidgets");
	} else if (bu_file_exists("../other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library ../other/iwidgets");
	} else if (bu_file_exists("src/other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path src/other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library src/other/iwidgets");
	} else if (bu_file_exists("../src/other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../src/other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library ../src/other/iwidgets");
	} else if (bu_file_exists("../../src/other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../src/other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library ../../src/other/iwidgets");
	} else if (bu_file_exists("../../../src/other/iwidgets")) {
	    (void)Tcl_Eval(interp, "lappend auto_path ../../../src/other/iwidgets");
	    (void)Tcl_Eval(interp, "set iwidgets_library ../../../src/other/iwidgets");
	} else {
	    /* fail noisily */
	    bu_vls_printf(&str, "lib/iwidgets%s", IWIDGETS_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
    }
    bu_vls_printf(&str, "set iwidgets_version %s", IWIDGETS_VERSION);
    (void)Tcl_Eval(interp, bu_vls_addr(&str));

    bu_vls_free(&str);

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
