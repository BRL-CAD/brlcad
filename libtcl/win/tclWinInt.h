/*
 * tclWinInt.h --
 *
 *	Declarations of Windows-specific shared variables and procedures.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinInt.h 1.7 97/06/25 10:56:14
 */

#ifndef _TCLWININT
#define _TCLWININT

#ifndef _TCLINT
#include "tclInt.h"
#endif
#ifndef _TCLPORT
#include "tclPort.h"
#endif

/*
 * Some versions of Borland C have a define for the OSVERSIONINFO for
 * Win32s and for NT, but not for Windows 95.
 */

#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS 1
#endif

EXTERN int		TclWinSynchSpawn(void *args, int type, void **trans,
				      Tcl_Pid *pidPtr);
EXTERN int		TclWinGetPlatformId(void);


#endif	/* _TCLWININT */
