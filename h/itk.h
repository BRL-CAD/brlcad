/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tk]
 *  DESCRIPTION:  Building mega-widgets with [incr Tcl]
 *
 *  [incr Tk] provides a framework for building composite "mega-widgets"
 *  using [incr Tcl] classes.  It defines a set of base classes that are
 *  specialized to create all other widgets.
 *
 *  ADDING [incr Tk] TO A Tcl-BASED APPLICATION:
 *
 *    To add [incr Tk] facilities to a Tcl application, modify the
 *    Tcl_AppInit() routine as follows:
 *
 *    1) Include the header files for [incr Tcl] and [incr Tk] near
 *       the top of the file containing Tcl_AppInit():
 *
 *         #include "itcl.h"
 *         #include "itk.h"
 *
 *    2) Within the body of Tcl_AppInit(), add the following lines:
 *
 *         if (Itcl_Init(interp) == TCL_ERROR) {
 *             return TCL_ERROR;
 *         }
 *         if (Itk_Init(interp) == TCL_ERROR) {
 *             return TCL_ERROR;
 *         }
 *
 *    3) Link your application with libitcl.a and libitk.a
 *
 *    NOTE:  An example file "tkAppInit.c" containing the changes shown
 *           above is included in this distribution.
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
#ifndef ITK_H
#define ITK_H

/*
 * A special definition used to allow this header file to be included
 * in resource files so that they can get obtain version information from
 * this file.  Resource compilers don't like all the C stuff, like typedefs
 * and procedure declarations, that occur below.
 */

#ifndef RESOURCE_INCLUDED

#include "itclInt.h"
#include "tk.h"

#ifdef BUILD_itk
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 *  List of options in alphabetical order:
 */
typedef struct ItkOptList {
    Tcl_HashTable *options;     /* list containing the real options */
    Tcl_HashEntry **list;       /* gives ordering of options */
    int len;                    /* number of entries in order list */
    int max;                    /* maximum size of order list */
} ItkOptList;

/*
 *  List of options created in the class definition:
 */
typedef struct ItkClassOptTable {
    Tcl_HashTable options;        /* option storage with fast lookup */
    ItkOptList order;             /* gives ordering of options */
} ItkClassOptTable;

/*
 *  Each option created in the class definition:
 */
typedef struct ItkClassOption {
    ItclMember *member;           /* info about this option */
    char *resName;                /* resource name in X11 database */
    char *resClass;               /* resource class name in X11 database */
    char *init;                   /* initial value for option */
} ItkClassOption;


/*
 *  Exported functions:
 */
EXTERN int Itk_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Itk_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *  Functions used internally by this package:
 */
EXTERN int Itk_ConfigBodyCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itk_UsualCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));

/*
 *  Functions for managing options included in class definitions:
 */
EXTERN int Itk_ClassOptionDefineCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itk_ClassOptionIllegalCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));

EXTERN int Itk_ConfigClassOption _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj, ClientData cdata, char* newVal));

EXTERN ItkClassOptTable* Itk_CreateClassOptTable _ANSI_ARGS_((
    Tcl_Interp *interp, ItclClass *cdefn));
EXTERN ItkClassOptTable* Itk_FindClassOptTable _ANSI_ARGS_((
    ItclClass *cdefn));
EXTERN void Itk_DeleteClassOptTable _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass *cdefn));

EXTERN int Itk_CreateClassOption _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass *cdefn, char *switchName, char *resName, char *resClass,
    char *defVal, char *config, ItkClassOption **optPtr));
EXTERN ItkClassOption* Itk_FindClassOption _ANSI_ARGS_((
    ItclClass *cdefn, char *switchName));
EXTERN void Itk_DelClassOption _ANSI_ARGS_((ItkClassOption *opt));

/*
 *  Functions needed for the Archetype base class:
 */
EXTERN int Itk_ArchetypeInit _ANSI_ARGS_((Tcl_Interp* interp));

/*
 *  Functions for maintaining the ordered option list:
 */
EXTERN void Itk_OptListInit _ANSI_ARGS_((ItkOptList* olist,
    Tcl_HashTable *options));
EXTERN void Itk_OptListFree _ANSI_ARGS_((ItkOptList* olist));

EXTERN void Itk_OptListAdd _ANSI_ARGS_((ItkOptList* olist,
    Tcl_HashEntry *entry));
EXTERN void Itk_OptListRemove _ANSI_ARGS_((ItkOptList* olist,
    Tcl_HashEntry *entry));

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

#endif /* RESOURCE INCLUDED */
#endif /* ITK_H */
