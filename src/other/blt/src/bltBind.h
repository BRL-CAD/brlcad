/*
 * bltBind.h --
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

#ifndef _BLT_BIND_H
#define _BLT_BIND_H

#include <bltList.h>

typedef struct Blt_BindTableStruct *Blt_BindTable;

typedef ClientData (Blt_BindPickProc) _ANSI_ARGS_((ClientData clientData,
	int x, int y, ClientData *contextPtr));

typedef void (Blt_BindTagProc) _ANSI_ARGS_((Blt_BindTable bindTable, 
	ClientData object, ClientData context, Blt_List list));


/*
 *  Binding structure information:
 */

struct Blt_BindTableStruct {
    unsigned int flags;
    Tk_BindingTable bindingTable;
				/* Table of all bindings currently defined.
				 * NULL means that no bindings exist, so the
				 * table hasn't been created.  Each "object"
				 * used for this table is either a Tk_Uid for
				 * a tag or the address of an item named by
				 * id. */

    ClientData currentItem;	/* The item currently containing the mouse
				 * pointer, or NULL if none. */
    ClientData currentContext;	/* One word indicating what kind of object
				 * was picked. */

    ClientData newItem;		/* The item that is about to become the
				 * current one, or NULL.  This field is
				 * used to detect deletions of the new
				 * current item pointer that occur during
				 * Leave processing of the previous current
				 * tab.  */
    ClientData newContext;	/* One-word indicating what kind of object 
				 * was just picked. */

    ClientData focusItem;
    ClientData focusContext;

    XEvent pickEvent;		/* The event upon which the current choice
				 * of the current tab is based.  Must be saved
				 * so that if the current item is deleted,
				 * we can pick another. */
    int activePick;		/* The pick event has been initialized so
				 * that we can repick it */

    int state;			/* Last known modifier state.  Used to
				 * defer picking a new current object
				 * while buttons are down. */

    ClientData clientData;
    Tk_Window tkwin;
    Blt_BindPickProc *pickProc;	/* Routine to report the item the mouse is
				 * currently over. */
    Blt_BindTagProc *tagProc;	/* Routine to report tags picked items. */
};

EXTERN void Blt_DestroyBindingTable _ANSI_ARGS_((Blt_BindTable table));

EXTERN Blt_BindTable Blt_CreateBindingTable _ANSI_ARGS_((Tcl_Interp *interp,
	Tk_Window tkwin, ClientData clientData, Blt_BindPickProc *pickProc,
	Blt_BindTagProc *tagProc));

EXTERN int Blt_ConfigureBindings _ANSI_ARGS_((Tcl_Interp *interp,
	Blt_BindTable table, ClientData item, int argc, char **argv));

#if (TCL_MAJOR_VERSION >= 8)
EXTERN int Blt_ConfigureBindingsFromObj _ANSI_ARGS_((Tcl_Interp *interp,
	Blt_BindTable table, ClientData item, int objc, Tcl_Obj *CONST *objv));
#endif

EXTERN void Blt_PickCurrentItem _ANSI_ARGS_((Blt_BindTable table));

EXTERN void Blt_DeleteBindings _ANSI_ARGS_((Blt_BindTable table,
	ClientData object));

EXTERN void Blt_MoveBindingTable _ANSI_ARGS_((Blt_BindTable table, 
	Tk_Window tkwin));

#define Blt_SetFocusItem(bindPtr, object, context) \
	((bindPtr)->focusItem = (ClientData)(object),\
	 (bindPtr)->focusContext = (ClientData)(context))

#define Blt_SetCurrentItem(bindPtr, object, context) \
	((bindPtr)->currentItem = (ClientData)(object),\
	 (bindPtr)->currentContext = (ClientData)(context))

#define Blt_GetCurrentItem(bindPtr)  ((bindPtr)->currentItem)
#define Blt_GetCurrentContext(bindPtr)  ((bindPtr)->currentContext)
#define Blt_GetLatestItem(bindPtr)  ((bindPtr)->newItem)

#define Blt_GetBindingData(bindPtr)  ((bindPtr)->clientData)

#endif /*_BLT_BIND_H*/
