/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  This file contains procedures that belong in the Tcl/Tk core.
 *  Hopefully, they'll migrate there soon.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"


/*
 *----------------------------------------------------------------------
 *
 * _Tcl_GetCallFrame --
 *
 *	Checks the call stack and returns the call frame some number
 *	of levels up.  It is often useful to know the invocation
 *	context for a command.
 *
 * Results:
 *	Returns a token for the call frame 0 or more levels up in
 *	the call stack.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_CallFrame*
_Tcl_GetCallFrame(interp, level)
    Tcl_Interp *interp;  /* interpreter being queried */
    int level;           /* number of levels up in the call stack (>= 0) */
{
    Interp *iPtr = (Interp*)interp;
    CallFrame *framePtr;

    if (level < 0) {
        Tcl_Panic("itcl: _Tcl_GetCallFrame called with bad number of levels");
    }

    framePtr = iPtr->varFramePtr;
    while (framePtr && level > 0) {
        framePtr = framePtr->callerVarPtr;
        level--;
    }
    return (Tcl_CallFrame*)framePtr;
}


/*
 *----------------------------------------------------------------------
 *
 * _Tcl_ActivateCallFrame --
 *
 *	Makes an existing call frame the current frame on the
 *	call stack.  Usually called in conjunction with
 *	_Tcl_GetCallFrame to simulate the effect of an "uplevel"
 *	command.
 *
 *	Note that this procedure is different from Tcl_PushCallFrame,
 *	which adds a new call frame to the call stack.  This procedure
 *	assumes that the call frame is already initialized, and it
 *	merely activates it on the call stack.
 *
 * Results:
 *	Returns a token for the call frame that was in effect before
 *	activating the new context.  That call frame can be restored
 *	by calling _Tcl_ActivateCallFrame again.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_CallFrame*
_Tcl_ActivateCallFrame(interp, framePtr)
    Tcl_Interp *interp;        /* interpreter being queried */
    Tcl_CallFrame *framePtr;   /* call frame to be activated */
{
    Interp *iPtr = (Interp*)interp;
    CallFrame *oldFramePtr;

    oldFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = (CallFrame *) framePtr;

    return (Tcl_CallFrame *) oldFramePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * _TclNewVar --
 *
 *      Create a new heap-allocated variable that will eventually be
 *      entered into a hashtable.
 *
 * Results:
 *      The return value is a pointer to the new variable structure. It is
 *      marked as a scalar variable (and not a link or array variable). Its
 *      value initially is NULL. The variable is not part of any hash table
 *      yet. Since it will be in a hashtable and not in a call frame, its
 *      name field is set NULL. It is initially marked as undefined.
 *
 * Side effects:
 *      Storage gets allocated.
 *
 *----------------------------------------------------------------------
 */

Var *
_TclNewVar()
{
    register Var *varPtr;

    varPtr = (Var *) ckalloc(sizeof(Var));
    varPtr->value.objPtr = NULL;
    varPtr->name = NULL;
    varPtr->nsPtr = NULL;
    varPtr->hPtr = NULL;
    varPtr->refCount = 0;
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;
    varPtr->flags = (VAR_SCALAR | VAR_UNDEFINED | VAR_IN_HASHTABLE);
    return varPtr;
}
