/*
 * bltNsUtil.h --
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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

#ifndef BLT_NS_UTIL_H
#define BLT_NS_UTIL_H 1

#if defined(ITCL_NAMESPACES) || (TCL_MAJOR_VERSION >= 8)
#define HAVE_NAMESPACES 1
#else
#define HAVE_NAMESPACES 0
#endif

#if (TCL_MAJOR_VERSION <= 7)

/* 
 * Namespaces and callframes don't exist before Tcl version 8.0.
 * We'll define them as opaque pointers.  In reality, they
 * point to the interpreter token.
 */
typedef struct Tcl_NamespaceStruct Tcl_Namespace;
typedef struct Tcl_CallFrameStruct *Tcl_CallFrame;
#endif

#ifndef TCL_NAMESPACE_ONLY
#define TCL_NAMESPACE_ONLY TCL_GLOBAL_ONLY
#endif


/*
EXTERN Tcl_Command Tcl_FindCommand _ANSI_ARGS_((Tcl_Interp *interp,
	char *name, Tcl_Namespace *nsPtr, int flags));
*/

#define NS_SEARCH_NONE		(0)
#define NS_SEARCH_CURRENT	(1<<0)
#define NS_SEARCH_GLOBAL	(1<<1)
#define NS_SEARCH_BOTH		(NS_SEARCH_GLOBAL | NS_SEARCH_CURRENT)


/*
 * Namespace procedures not prototyped defined in Tcl.h 
 */
EXTERN Tcl_Namespace *Tcl_GetCurrentNamespace _ANSI_ARGS_((Tcl_Interp *interp));

EXTERN Tcl_Namespace *Tcl_GetGlobalNamespace _ANSI_ARGS_((Tcl_Interp *interp));

#if (TCL_MAJOR_VERSION >= 8)
/*
EXTERN Tcl_Namespace *Tcl_CreateNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	char *name, ClientData clientData, Tcl_NamespaceDeleteProc *nsDelProc));
*/
EXTERN void Tcl_DeleteNamespace _ANSI_ARGS_((Tcl_Namespace *nsPtr));
/*
EXTERN Tcl_Namespace *Tcl_FindNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	char *name, Tcl_Namespace *context, int flags));

EXTERN int Tcl_Export _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	char *name, int resetFlag));
*/
EXTERN Tcl_Var Tcl_FindNamespaceVar _ANSI_ARGS_((Tcl_Interp *interp, char *name,
	Tcl_Namespace *contextNsPtr, int flags));

EXTERN void Tcl_PopCallFrame _ANSI_ARGS_((Tcl_Interp *interp));

EXTERN int Tcl_PushCallFrame _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_CallFrame * framePtr, Tcl_Namespace *nsPtr, int isProcCallFrame));

extern Tcl_HashTable *Blt_GetArrayVariableTable _ANSI_ARGS_((
	Tcl_Interp *interp, CONST char *varName, int flags));

#endif /* TCL_MAJOR_VERSION >= 8 */


/* 
 * Auxillary procedures 
 */
EXTERN Tcl_Namespace *Blt_GetVariableNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	CONST char *varName));

EXTERN Tcl_Namespace *Blt_GetCommandNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Command cmdToken));

EXTERN Tcl_CallFrame *Blt_EnterNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Namespace *nsPtr));

EXTERN void Blt_LeaveNamespace _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_CallFrame * framePtr));

EXTERN int Blt_ParseQualifiedName _ANSI_ARGS_((Tcl_Interp *interp, 
	CONST char *name, Tcl_Namespace **nsPtrPtr, CONST char **namePtr));

EXTERN char *Blt_GetQualifiedName _ANSI_ARGS_((Tcl_Namespace *nsPtr, 
	CONST char *name, Tcl_DString *resultPtr));

EXTERN Tcl_Command Blt_CreateCommand _ANSI_ARGS_((Tcl_Interp *interp,
	CONST char *cmdName, Tcl_CmdProc *proc, ClientData clientData,
	Tcl_CmdDeleteProc *deleteProc));


#endif /* BLT_NS_UTIL_H */
