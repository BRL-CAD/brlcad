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

#include <stdarg.h>
#include <string.h>

#include "tclcad_private.h"


int
tclcad_eval_var(Tcl_Interp *interp, int preserve_result,
		const char *command, ...)
{
    int result;

    Tcl_DString script;
    Tcl_DStringInit(&script);
    Tcl_DStringAppend(&script, command, -1);

    {
	va_list arguments;
	const char *arg;

	va_start(arguments, command);

	while ((arg = va_arg(arguments, const char *)))
	    Tcl_DStringAppendElement(&script, arg);

	va_end(arguments);
    }

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


int
tclcad_eval(Tcl_Interp *interp, const char *command, const char *arg)
{
    if (arg)
	return tclcad_eval_var(interp, 0, command, arg, NULL);
    else
	return tclcad_eval_var(interp, 0, command, NULL);
}


int
tclcad_eval_quiet(Tcl_Interp *interp, const char *command, const char *arg)
{
    if (arg)
	return tclcad_eval_var(interp, 1, command, arg, NULL);
    else
	return tclcad_eval_var(interp, 1, command, NULL);
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
