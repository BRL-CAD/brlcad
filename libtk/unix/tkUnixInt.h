/*
 * tkUnixInt.h --
 *
 *	This file contains declarations that are shared among the
 *	UNIX-specific parts of Tk but aren't used by the rest of
 *	Tk.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixInt.h 1.9 97/05/08 11:20:12
 */

#ifndef _TKUNIXINT
#define _TKUNIXINT

/*
 * Prototypes for procedures that are referenced in files other
 * than the ones they're defined in.
 */

EXTERN void		TkCreateXEventSource _ANSI_ARGS_((void));
EXTERN TkWindow *	TkpGetContainer _ANSI_ARGS_((TkWindow *embeddedPtr));
EXTERN TkWindow *	TkpGetWrapperWindow _ANSI_ARGS_((TkWindow *winPtr));
EXTERN Window		TkUnixContainerId _ANSI_ARGS_((TkWindow *winPtr));
EXTERN int		TkUnixDoOneXEvent _ANSI_ARGS_((Tcl_Time *timePtr));
EXTERN void		TkUnixSetMenubar _ANSI_ARGS_((Tk_Window tkwin,
				Tk_Window menubar));

#endif /* _TKUNIXINT */
