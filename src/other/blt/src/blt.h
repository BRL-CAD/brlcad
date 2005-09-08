/*
 * blt.h --
 *
 * Copyright 1991-1998 by Bell Labs Innovations for Lucent
 * Technologies.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#ifndef _BLT_H
#define _BLT_H

#define BLT_MAJOR_VERSION 	2
#define BLT_MINOR_VERSION 	4
#define BLT_VERSION		"2.4"
#define BLT_PATCH_LEVEL		"2.4z"
#define BLT_RELEASE_SERIAL	0

#include <tcl.h>

#ifndef EXPORT
#define EXPORT
#endif

#undef EXTERN

#ifdef __cplusplus
#   define EXTERN extern "C" EXPORT
#else
#   define EXTERN extern EXPORT
#endif

#ifndef _ANSI_ARGS_
#   define _ANSI_ARGS_(x)       ()
#endif

#include <bltVector.h>
#include <bltHash.h>

typedef char *Blt_Uid;

EXTERN Blt_Uid Blt_GetUid _ANSI_ARGS_((char *string));
EXTERN void Blt_FreeUid _ANSI_ARGS_((Blt_Uid uid));
EXTERN Blt_Uid Blt_FindUid _ANSI_ARGS_((char *string));

#if (TCL_MAJOR_VERSION >= 8)
EXTERN int Blt_GetArrayFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, Blt_HashTable **tablePtrPtr));
EXTERN Tcl_Obj *Blt_NewArrayObj _ANSI_ARGS_((int objc, Tcl_Obj *objv[]));
EXTERN void Blt_RegisterArrayObj _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Blt_IsArrayObj _ANSI_ARGS_((Tcl_Obj *obj));
#endif /* TCL_MAJOR_VERSION >= 8 */

#endif /*_BLT_H*/
