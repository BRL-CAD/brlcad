/*                          E V A L . C
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
 * Functions for escaping and evaluating TCL commands.
 *
 */


#include "common.h"

#include <tcl.h>


int
tclcad_eval(Tcl_Interp *interp, const char *command, size_t num_args,
	    const char * const *args)
{
    int ret;
    size_t i;

    Tcl_DString script;
    Tcl_DStringInit(&script);
    Tcl_DStringAppend(&script, command, -1);

    for (i = 0; i < num_args; ++i)
	Tcl_DStringAppendElement(&script, args[i]);

    ret = Tcl_Eval(interp, Tcl_DStringValue(&script));
    Tcl_DStringFree(&script);

    return ret;
}


int
tclcad_eval_noresult(Tcl_Interp *interp, const char *command, size_t num_args,
		     const char * const *args)
{
    int ret;

    Tcl_Obj *saved_result = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(saved_result);
    ret = tclcad_eval(interp, command, num_args, args);
    Tcl_SetObjResult(interp, saved_result);
    Tcl_DecrRefCount(saved_result);

    return ret;
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
