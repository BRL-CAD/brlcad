/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *  
 *  ADDING [incr Tcl] TO A Tcl-BASED APPLICATION:
 *
 *    To add [incr Tcl] facilities to a Tcl application, modify the
 *    Tcl_AppInit() routine as follows:
 *
 *    1) Include this header file near the top of the file containing
 *       Tcl_AppInit():
 *
 *         #include "itcl.h"
 *
 *    2) Within the body of Tcl_AppInit(), add the following lines:
 *
 *         if (Itcl_Init(interp) == TCL_ERROR) {
 *             return TCL_ERROR;
 *         }
 * 
 *    3) Link your application with libitcl.a
 *
 *    NOTE:  An example file "tclAppInit.c" containing the changes shown
 *           above is included in this distribution.
 *  
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *       modified for Stubs 5/20/1999 by 
 *           David Gravereaux <davygrvy@bigfoot.com>
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#ifndef ITCL_H
#define ITCL_H

#include "tcl.h"

#undef TCL_STORAGE_CLASS
#ifdef BUILD_itcl
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_ITCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

#define ITCL_VERSION "3.2"
#define ITCL_PATCH_LEVEL "3.2.1"
#define ITCL_MAJOR_VERSION 3
#define ITCL_MINOR_VERSION 2
#define ITCL_RELEASE_LEVEL 1

/* 
 * A special definition used to allow this header file to be included 
 * in resource files so that they can get obtain version information from
 * this file.  Resource compilers don't like all the C stuff, like typedefs
 * and procedure declarations, that occur below.
 */

#ifndef RC_INVOKED

/*
 * Protection levels:
 *
 * ITCL_PUBLIC    - accessible from any namespace
 * ITCL_PROTECTED - accessible from namespace that imports in "protected" mode
 * ITCL_PRIVATE   - accessible only within the namespace that contains it
 */
#define ITCL_PUBLIC           1
#define ITCL_PROTECTED        2
#define ITCL_PRIVATE          3
#define ITCL_DEFAULT_PROTECT  4


/*
 *  Generic stack.
 */
typedef struct Itcl_Stack {
    ClientData *values;          /* values on stack */
    int len;                     /* number of values on stack */
    int max;                     /* maximum size of stack */
    ClientData space[5];         /* initial space for stack data */
} Itcl_Stack;

#define Itcl_GetStackSize(stackPtr) ((stackPtr)->len)

/*
 *  Generic linked list.
 */
struct Itcl_List;
typedef struct Itcl_ListElem {
    struct Itcl_List* owner;     /* list containing this element */
    ClientData value;            /* value associated with this element */
    struct Itcl_ListElem *prev;  /* previous element in linked list */
    struct Itcl_ListElem *next;  /* next element in linked list */
} Itcl_ListElem;

typedef struct Itcl_List {
    int validate;                /* validation stamp */
    int num;                     /* number of elements */
    struct Itcl_ListElem *head;  /* previous element in linked list */
    struct Itcl_ListElem *tail;  /* next element in linked list */
} Itcl_List;

#define Itcl_FirstListElem(listPtr) ((listPtr)->head)
#define Itcl_LastListElem(listPtr)  ((listPtr)->tail)
#define Itcl_NextListElem(elemPtr)  ((elemPtr)->next)
#define Itcl_PrevListElem(elemPtr)  ((elemPtr)->prev)
#define Itcl_GetListLength(listPtr) ((listPtr)->num)
#define Itcl_GetListValue(elemPtr)  ((elemPtr)->value)

/*
 *  Token representing the state of an interpreter.
 */
typedef struct Itcl_InterpState_ *Itcl_InterpState;


/*
 * Include the public function declarations that are accessible via
 * the stubs table.
 */

#include "itclDecls.h"


/*
 * Itcl_InitStubs is used by extensions like Itk that can be linked
 * against the itcl stubs library.  If we are not using stubs
 * then this reduces to package require.
 */

#ifdef USE_ITCL_STUBS

#ifdef __cplusplus
extern "C"
#endif
CONST char *	Itcl_InitStubs _ANSI_ARGS_((Tcl_Interp *interp,
			    char *version, int exact));
#else
#define Itcl_InitStubs(interp, version, exact) \
      Tcl_PkgRequire(interp, "Itcl", version, exact)
#endif

/*
 * Public functions that are not accessible via the stubs table.
 */


#endif /* RC_INVOKED */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* ITCL_H */
