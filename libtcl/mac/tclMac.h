/*
 * tclMac.h --
 *
 *	Declarations of Macintosh specific public variables and procedures.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMac.h 1.8 97/06/24 18:59:08
 */

#ifndef _TCLMAC
#define _TCLMAC

#ifndef _TCL
#   include "tcl.h"
#endif
#include <Types.h>
#include <Files.h>
#include <Events.h>

/*
 * "export" is a MetroWerks specific pragma.  It flags the linker that  
 * any symbols that are defined when this pragma is on will be exported 
 * to shared libraries that link with this library.
 */
 
#pragma export on

typedef int (*Tcl_MacConvertEventPtr) _ANSI_ARGS_((EventRecord *eventPtr));

/*
 * This is needed by the shells to handle Macintosh events.
 */
 
EXTERN void  Tcl_MacSetEventProc _ANSI_ARGS_((Tcl_MacConvertEventPtr procPtr));

/*
 * These routines are useful for handling using scripts from resources 
 * in the application shell
 */
 
EXTERN char *	Tcl_MacConvertTextResource _ANSI_ARGS_((Handle resource));
EXTERN int 	Tcl_MacEvalResource _ANSI_ARGS_((Tcl_Interp *interp,
		    char *resourceName, int resourceNumber, char *fileName));
EXTERN Handle	Tcl_MacFindResource _ANSI_ARGS_((Tcl_Interp *interp,
		    long resourceType, char *resourceName,
		    int resourceNumber, char *resFileRef, int * releaseIt));

/* These routines support the new OSType object type (i.e. the packed 4
 * character type and creator codes).
 */
 								  
EXTERN int		Tcl_GetOSTypeFromObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, OSType *osTypePtr));
EXTERN void		Tcl_SetOSTypeObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    OSType osType));
EXTERN Tcl_Obj *	Tcl_NewOSTypeObj _ANSI_ARGS_((OSType osType));



/*
 * The following routines are utility functions in Tcl.  They are exported
 * here because they are needed in Tk.  They are not officially supported,
 * however.  The first set are from the MoreFiles package.
 */

EXTERN pascal	OSErr	FSpGetDirectoryID(const FSSpec *spec,
			    long *theDirID, Boolean *isDirectory);
EXTERN pascal	short	FSpOpenResFileCompat(const FSSpec *spec,
			    SignedByte permission);
EXTERN pascal	void	FSpCreateResFileCompat(const FSSpec *spec,
			    OSType creator, OSType fileType,
			    ScriptCode scriptTag);
/* 
 * Like the MoreFiles routines these fix problems in the standard
 * Mac calls.  These routines is from tclMacUtils.h.
 */

EXTERN int 	FSpLocationFromPath _ANSI_ARGS_((int length, char *path,
		    FSSpecPtr theSpec));
EXTERN OSErr 	FSpPathFromLocation _ANSI_ARGS_((FSSpecPtr theSpec,
		    int *length, Handle *fullPath));

/*
 * These are not in MSL 2.1.2, so we need to export them from the
 * Tcl shared library.  They are found in the compat directory
 * except the panic routine which is found in tclMacPanic.h.
 */
 
EXTERN int	strncasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2, size_t n));
EXTERN int	strcasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2));
EXTERN void	panic _ANSI_ARGS_(TCL_VARARGS(char *,format));

#pragma export reset

#endif /* _TCLMAC */
