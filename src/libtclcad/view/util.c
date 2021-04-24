/*                       T C L C A D _ V I E W S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2021 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/tclcad_views.c
 *
 * A quasi-object-oriented database interface.
 *
 * A GED object contains the attributes and methods for controlling a
 * BRL-CAD geometry edit object.
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../tclcad_private.h"
#include "../view/view.h"

int
to_is_viewable(struct bview *gdvp)
{
    int result_int;

    const struct bu_vls *pathvls = dm_get_pathname((struct dm *)gdvp->dmp);
    if (!pathvls || !bu_vls_strlen(pathvls)) {
	return 0;
    }

    /* stash any existing result so we can inspect our own */
    Tcl_Obj *saved_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_IncrRefCount(saved_result);

    const char *pathname = bu_vls_cstr(pathvls);
    if (pathname && tclcad_eval(current_top->to_interp, "winfo viewable", 1, &pathname) != TCL_OK) {
	return 0;
    }

    Tcl_Obj *our_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_GetIntFromObj(current_top->to_interp, our_result, &result_int);

    /* restore previous result */
    Tcl_SetObjResult(current_top->to_interp, saved_result);
    Tcl_DecrRefCount(saved_result);

    if (!result_int) {
	return 0;
    }

    return 1;
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
