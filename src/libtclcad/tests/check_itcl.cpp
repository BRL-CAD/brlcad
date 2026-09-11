/*                 C H E C K _ I T C L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file check_itcl.cpp
 *
 * Verify that libtclcad can initialize its minimum supported Itcl.
 */

#include "common.h"

#include <cstdio>

#include "bu/app.h"
#include "bu/vls.h"
#include "tclcad.h"
#include "../tclcad_private.h"

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    Tcl_FindExecutable(argv[0]);

    Tcl_Interp *interp = Tcl_CreateInterp();
    if (!interp) {
	std::fprintf(stderr, "Unable to create a Tcl interpreter\n");
	return 1;
    }

    struct bu_vls log = BU_VLS_INIT_ZERO;
    if (tclcad_init(interp, 0, &log) != TCL_OK) {
	std::fprintf(stderr, "%s", bu_vls_cstr(&log));
	bu_vls_free(&log);
	Tcl_DeleteInterp(interp);
	return 1;
    }
    bu_vls_free(&log);

    const char *version = Tcl_PkgRequire(interp, "Itcl",
	TCLCAD_ITCL_MIN_VERSION, 0);
    if (!version) {
	std::fprintf(stderr, "Itcl version check failed: %s\n",
	    Tcl_GetStringResult(interp));
	Tcl_DeleteInterp(interp);
	return 1;
    }

    std::printf("Itcl %s satisfies the libtclcad minimum %s\n",
	version, TCLCAD_ITCL_MIN_VERSION);
    Tcl_DeleteInterp(interp);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
