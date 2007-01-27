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

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "tclcad.h"
#include "tk.h"


void
tclcad_auto_path(Tcl_Interp *interp)
{
    static const int SEARCH_DEPTH = 7;
    struct bu_vls str;
    char *pathname;

    /****************************************************/
    /* Locate the BRL-CAD-specific Tcl scripts quietly. */
    /****************************************************/
    pathname = bu_brlcad_data( "tclscripts", 1 );

    bu_vls_init(&str);
    if (pathname) {
#ifdef _WIN32
    {
	/* XXX - nasty little hack to convert paths */
	register int i;

	strcat(pathname,"/");
	for (i = 0; i < strlen(pathname); i++) {
	    if (pathname[i]=='\\')
		pathname[i]='/';
	}
    }
#endif

	bu_vls_sprintf(&str, "lappend auto_path \"%s\" \"%s/lib\" \"%s/util\" \"%s/mged\" \"%s/geometree\"",
		      pathname, pathname, pathname, pathname, pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	int i, j;
	struct bu_vls prefix, base;

	/* hunt for the tclscripts since we're probably just not
	 * installed yet.  must go at least as many levels deep as the
	 * src/tclscripts hierarchy.
	 */
	bu_log("WARNING: BRL-CAD %s is apparently not installed yet.\n", BRLCAD_VERSION);

	bu_vls_init(&prefix);
	bu_vls_init(&base);

	/* go up to SEARCH_DEPTH levels deep */
	bu_vls_sprintf(&base, "src/tclscripts");
	for (i = 0; i < SEARCH_DEPTH; i++) {
	    bu_vls_trunc(&prefix, 0);
	    for (j=0; j < i; j++) {
		bu_vls_printf(&prefix, "../");
	    }
	    bu_vls_sprintf(&str, "%S%S", &prefix, &base);
	    if (bu_file_exists(bu_vls_addr(&str))) {
		bu_vls_sprintf(&str, "lappend auto_path \"%S%S\" \"%S%S/lib\" \"%S%S/util\" \"%S%S/mged\" \"%S%S/geometree\" \"%S%S/rtwizard\"",
			      &prefix, &base, &prefix, &base, &prefix, &base, &prefix, &base, &prefix, &base, &prefix, &base);
		/* make sure tcl's scripts are in the path */
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		break;
	    }
	}

	if (i >= SEARCH_DEPTH) {
	    /* fail noisily */
	    (void)bu_brlcad_data("tclscripts", 0);
	}
	bu_vls_free(&prefix);
	bu_vls_free(&base);
    }


    /*******************************************/
    /* make sure tcl's scripts are in the path */
    /*******************************************/
    bu_vls_sprintf(&str, "lib/tcl%s", TCL_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );

    if (pathname) {
#ifdef _WIN32
    {
	/* XXX - nasty little hack to convert paths */
	register int i;

	strcat(pathname,"/");
	for (i = 0; i < strlen(pathname); i++) {
	    if (pathname[i]=='\\')
		pathname[i]='/';
	}
    }
#endif

	bu_vls_sprintf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_sprintf(&str, "set tcl_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	int i, j;
	struct bu_vls prefix, base;

	/* hunt for the tcl library since we're probably just not
	 * installed yet.
	 */

	bu_vls_init(&prefix);
	bu_vls_init(&base);

	/* go up to SEARCH_DEPTH levels deep */
	bu_vls_sprintf(&base, "src/other/libtcl/library");
	for (i = 0; i < SEARCH_DEPTH; i++) {
	    bu_vls_trunc(&prefix, 0);
	    for (j=0; j < i; j++) {
		bu_vls_printf(&prefix, "../");
	    }
	    bu_vls_sprintf(&str, "%S%S", &prefix, &base);
	    if (bu_file_exists(bu_vls_addr(&str))) {
		bu_vls_sprintf(&str, "lappend auto_path \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		bu_vls_sprintf(&str, "set tcl_library \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		break;
	    }
	}

	if (i >= SEARCH_DEPTH) {
	    /* fail noisily */
	    bu_vls_sprintf(&str, "lib/tcl%s", TCL_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
	bu_vls_free(&prefix);
	bu_vls_free(&base);
    }
    bu_vls_sprintf(&str, "set tcl_version %s", TCL_VERSION);
    (void)Tcl_Eval(interp, bu_vls_addr(&str));


    /******************************************/
    /* make sure tk's scripts are in the path */
    /******************************************/
    bu_vls_sprintf(&str, "lib/tk%s", TK_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );
    if (pathname) {
#ifdef _WIN32
    {
	/* XXX - nasty little hack to convert paths */
	int i;

	strcat(pathname,"/");
	for (i=0;i<strlen(pathname);i++) {
	    if(pathname[i]=='\\')
		pathname[i]='/';
	}
    }
#endif

	bu_vls_sprintf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_sprintf(&str, "set tk_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	int i, j;
	struct bu_vls prefix, base;

	/* hunt for the tk library since we're probably just not
	 * installed yet.
	 */

	bu_vls_init(&prefix);
	bu_vls_init(&base);

	/* go up to SEARCH_DEPTH levels deep */
	bu_vls_sprintf(&base, "src/other/libtk/library");
	for (i = 0; i < SEARCH_DEPTH; i++) {
	    bu_vls_trunc(&prefix, 0);
	    for (j=0; j < i; j++) {
		bu_vls_printf(&prefix, "../");
	    }
	    bu_vls_sprintf(&str, "%S%S", &prefix, &base);
	    if (bu_file_exists(bu_vls_addr(&str))) {
		bu_vls_sprintf(&str, "lappend auto_path \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		bu_vls_sprintf(&str, "set tk_library \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		break;
	    }
	}

	if (i >= SEARCH_DEPTH) {
	    /* fail noisily */
	    bu_vls_sprintf(&str, "lib/tk%s", TK_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
	bu_vls_free(&prefix);
	bu_vls_free(&base);
    }
    bu_vls_sprintf(&str, "set tk_version %s", TK_VERSION);
    (void)Tcl_Eval(interp, bu_vls_addr(&str));


    /***********************************************/
    /* make sure iwidget's scripts are in the path */
    /***********************************************/
    bu_vls_sprintf(&str, "lib/iwidgets%s", IWIDGETS_VERSION);
    pathname = bu_brlcad_root( bu_vls_addr(&str), 1 );
    if (pathname) {
#ifdef _WIN32
    {
	/* XXX - nasty little hack to convert paths */
	int i;

	strcat(pathname,"/");
	for (i = 0; i < strlen(pathname); i++) {
	    if (pathname[i]=='\\')
		pathname[i]='/';
	}
    }
#endif

	bu_vls_sprintf(&str, "lappend auto_path \"%s\"", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
	bu_vls_sprintf(&str, "set iwidgets_library %s", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&str));
    } else {
	int i, j;
	struct bu_vls prefix, base;

	/* hunt for the tk library since we're probably just not
	 * installed yet.
	 */

	bu_vls_init(&prefix);
	bu_vls_init(&base);

	/* go up to SEARCH_DEPTH levels deep */
	bu_vls_sprintf(&base, "src/other/iwidgets");
	for (i = 0; i < SEARCH_DEPTH; i++) {
	    bu_vls_trunc(&prefix, 0);
	    for (j=0; j < i; j++) {
		bu_vls_printf(&prefix, "../");
	    }
	    bu_vls_sprintf(&str, "%S%S", &prefix, &base);
	    if (bu_file_exists(bu_vls_addr(&str))) {
		bu_vls_sprintf(&str, "lappend auto_path \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		bu_vls_sprintf(&str, "set iwidgets_library \"%S%S\"", &prefix, &base);
		(void)Tcl_Eval(interp, bu_vls_addr(&str));
		break;
	    }
	}

	if (i >= SEARCH_DEPTH) {
	    /* fail noisily */
	    bu_vls_sprintf(&str, "lib/iwidgets%s", IWIDGETS_VERSION);
	    (void)bu_brlcad_data(bu_vls_addr(&str), 0);
	}
	bu_vls_free(&prefix);
	bu_vls_free(&base);
    }
    bu_vls_sprintf(&str, "set iwidgets_version %s", IWIDGETS_VERSION);
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
