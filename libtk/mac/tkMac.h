/*
 * tkMacInt.h --
 *
 *	Declarations of Macintosh specific exported variables and procedures.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacInt.h 1.58 97/05/06 16:45:18
 */

#ifndef _TKMAC
#define _TKMAC

#include <Windows.h>

/*
 * "export" is a MetroWerks specific pragma.  It flags the linker that  
 * any symbols that are defined when this pragma is on will be exported 
 * to shared libraries that link with this library.
 */
 
#pragma export on

/*
 * This variable is exported and can be used by extensions.  It is the
 * way Tk extensions should access the QD Globals.  This is so Tk
 * can support embedding itself in another window. 
 */

EXTERN QDGlobalsPtr tcl_macQdPtr;

/* 
 * The following functions are needed to create a shell, and so they must be exported
 * from the Tk library.  However, these are not the final form of these interfaces, so
 * they are not currently supported as public interfaces.
 */
 
/*
 * These functions are currently in tkMacInt.h.  They are just copied over here
 * so they can be exported.
 */

EXTERN void 	TkMacInitMenus _ANSI_ARGS_((Tcl_Interp 	*interp));
EXTERN void		TkMacInitAppleEvents _ANSI_ARGS_((Tcl_Interp *interp));

EXTERN int		TkMacConvertEvent _ANSI_ARGS_((EventRecord *eventPtr));

#pragma export reset

#endif /* _TKMAC */
