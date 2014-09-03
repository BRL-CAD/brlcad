/*                   T C L C A D _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file tclcad_eval.c
 *
 * Functions for escaping and evaluating TCL commands.
 *
 */


#include "common.h"

#include <tcl.h>


int
tclcad_eval(Tcl_Interp *interp, int preserve_result, const char *command, size_t num_args, const char * const *args)
{
    int result;
    size_t i;

    Tcl_DString script;
    Tcl_DStringInit(&script);
    Tcl_DStringAppend(&script, command, -1);

    for (i = 0; i < num_args; ++i)
	Tcl_DStringAppendElement(&script, args[i]);

    if (!preserve_result) {
	result = Tcl_Eval(interp, Tcl_DStringValue(&script));
    } else {
	Tcl_Obj *saved_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(saved_result);
	result = Tcl_Eval(interp, Tcl_DStringValue(&script));
	Tcl_SetObjResult(interp, saved_result);
	Tcl_DecrRefCount(saved_result);
    }

    Tcl_DStringFree(&script);
    return result;
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
