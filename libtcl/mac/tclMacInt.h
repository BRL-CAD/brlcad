/*
 * tclMacInt.h --
 *
 *	Declarations of Macintosh specific shared variables and procedures.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacInt.h 1.24 97/09/09 16:22:01
 */

#ifndef _TCLMACINT
#define _TCLMACINT

#ifndef _TCL
#   include "tcl.h"
#endif
#ifndef _TCLMAC
#   include "tclMac.h"
#endif

#include <Events.h>
#include <Files.h>

#pragma export on

/*
 * Defines to control stack behavior
 */

#define TCL_MAC_68K_STACK_GROWTH (256*1024)
#define TCL_MAC_STACK_THRESHOLD 16384

/*
 * This flag is passed to TclMacRegisterResourceFork
 * by a file (usually a library) whose resource fork
 * should not be closed by the resource command.
 */
 
#define TCL_RESOURCE_DONT_CLOSE  2

/*
 * Typedefs used by Macintosh parts of Tcl.
 */
typedef pascal void (*ExitToShellProcPtr)(void);

/*
 * Prototypes for functions found in the tclMacUtil.c compatability library.
 */

EXTERN int 	FSpGetDefaultDir _ANSI_ARGS_((FSSpecPtr theSpec));
EXTERN int 	FSpSetDefaultDir _ANSI_ARGS_((FSSpecPtr theSpec));
EXTERN OSErr 	FSpFindFolder _ANSI_ARGS_((short vRefNum, OSType folderType,
		    Boolean createFolder, FSSpec *spec));
EXTERN void	GetGlobalMouse _ANSI_ARGS_((Point *mouse));

/*
 * Prototypes of Mac only internal functions.
 */

EXTERN void	TclCreateMacEventSource _ANSI_ARGS_((void));
EXTERN int	TclMacConsoleInit _ANSI_ARGS_((void));
EXTERN void	TclMacExitHandler _ANSI_ARGS_((void));
EXTERN void	TclMacInitExitToShell _ANSI_ARGS_((int usePatch));
EXTERN OSErr	TclMacInstallExitToShellPatch _ANSI_ARGS_((
		    ExitToShellProcPtr newProc));
EXTERN int	TclMacOSErrorToPosixError _ANSI_ARGS_((int error));
EXTERN void	TclMacRemoveTimer _ANSI_ARGS_((void *timerToken));
EXTERN void *	TclMacStartTimer _ANSI_ARGS_((long ms));
EXTERN int	TclMacTimerExpired _ANSI_ARGS_((void *timerToken));
EXTERN int	TclMacRegisterResourceFork _ANSI_ARGS_((short fileRef, Tcl_Obj *tokenPtr,
                    int insert));	
EXTERN short	TclMacUnRegisterResourceFork _ANSI_ARGS_((char *tokenPtr, Tcl_Obj *resultPtr));	
		    
#pragma export reset

#endif /* _TCLMACINT */
