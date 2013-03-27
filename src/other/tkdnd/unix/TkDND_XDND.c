/*
 * TkDND_XDND.h -- Tk XDND Drag'n'Drop Protocol Implementation
 * 
 *    This file implements the unix portion of the drag&drop mechanism
 *    for the tk toolkit. The protocol in use under unix is the
 *    XDND protocol.
 *
 * This software is copyrighted by:
 * Georgios Petasis, Athens, Greece.
 * e-mail: petasisg@yahoo.gr, petasis@iit.demokritos.gr
 *
 * The following terms apply to all files associated
 * with the software unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */
#include "tcl.h"
#include "tk.h"
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#ifdef HAVE_LIMITS_H
#include "limits.h"
#else
#define LONG_MAX 0x7FFFFFFFL
#endif

/*
#define TKDND_SET_XDND_PROPERTY_ON_TARGET
#define TKDND_SET_XDND_PROPERTY_ON_WRAPPER
#define DEBUG_CLIENTMESSAGE_HANDLER
 */
#define TKDND_SET_XDND_PROPERTY_ON_TOPLEVEL

#define TkDND_TkWindowChildren(tkwin) \
    ((Tk_Window) (((Tk_FakeWin *) (tkwin))->dummy2))

#define TkDND_TkWindowLastChild(tkwin) \
    ((Tk_Window) (((Tk_FakeWin *) (tkwin))->dummy3))

#define TkDND_TkWin(x) \
  (Tk_NameToWindow(interp, Tcl_GetString(x), Tk_MainWindow(interp)))

#define TkDND_Eval(objc) \
  for (i=0; i<objc; ++i) Tcl_IncrRefCount(objv[i]);\
  if (Tcl_EvalObjv(interp, objc, objv, TCL_EVAL_GLOBAL) != TCL_OK) \
      Tk_BackgroundError(interp); \
  for (i=0; i<objc; ++i) Tcl_DecrRefCount(objv[i]);

#define TkDND_Status_Eval(objc) \
  for (i=0; i<objc; ++i) Tcl_IncrRefCount(objv[i]);\
  status = Tcl_EvalObjv(interp, objc, objv, TCL_EVAL_GLOBAL);\
  if (status != TCL_OK) Tk_BackgroundError(interp); \
  for (i=0; i<objc; ++i) Tcl_DecrRefCount(objv[i]);

#define TkDND_Dict_Put(dict, k, v) \
  key   = Tcl_NewStringObj(k, -1); Tcl_IncrRefCount(key); \
  value = Tcl_NewStringObj(v, -1); Tcl_IncrRefCount(value); \
  Tcl_DictObjPut(interp, dict, key, value); \
  Tcl_DecrRefCount(key); Tcl_DecrRefCount(value);

#define TkDND_Dict_PutInt(dict, k, v) \
  key   = Tcl_NewStringObj(k, -1); Tcl_IncrRefCount(key); \
  value = Tcl_NewIntObj(v); Tcl_IncrRefCount(value); \
  Tcl_DictObjPut(interp, dict, key, value); \
  Tcl_DecrRefCount(key); Tcl_DecrRefCount(value);

#define TkDND_Dict_PutLong(dict, k, v) \
  key   = Tcl_NewStringObj(k, -1); Tcl_IncrRefCount(key); \
  value = Tcl_NewLongObj(v); Tcl_IncrRefCount(value); \
  Tcl_DictObjPut(interp, dict, key, value); \
  Tcl_DecrRefCount(key); Tcl_DecrRefCount(value);

#define TkDND_Dict_PutObj(dict, k, value) \
  key   = Tcl_NewStringObj(k, -1); Tcl_IncrRefCount(key); \
  Tcl_IncrRefCount(value); \
  Tcl_DictObjPut(interp, dict, key, value); \
  Tcl_DecrRefCount(key); Tcl_DecrRefCount(value);

#ifndef Tk_Interp
/*
 * Tk 8.5 has a new function to return the interpreter that is associated with a
 * window. Under 8.4 and earlier versions, simulate this function.
 */
#include "tkInt.h"
Tcl_Interp * TkDND_Interp(Tk_Window tkwin) {
  if (tkwin != NULL && ((TkWindow *)tkwin)->mainPtr != NULL) {
    return ((TkWindow *)tkwin)->mainPtr->interp;
  }
  return NULL;
} /* Tk_Interp */
#define Tk_Interp TkDND_Interp
#endif /* Tk_Interp */

/*
 * XDND Section
 */
#define XDND_VERSION 5

/* XdndEnter */
#define XDND_THREE 3
#define XDND_ENTER_SOURCE_WIN(e)        ((e)->xclient.data.l[0])
#define XDND_ENTER_THREE_TYPES(e)       (((e)->xclient.data.l[1] & 0x1UL) == 0)
#define XDND_ENTER_THREE_TYPES_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_ENTER_VERSION(e)           ((e)->xclient.data.l[1] >> 24)
#define XDND_ENTER_VERSION_SET(e,v)     (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~(0xFF << 24)) | ((v) << 24)
#define XDND_ENTER_TYPE(e,i)            ((e)->xclient.data.l[2 + i])    /* i => (0, 1, 2) */

/* XdndPosition */
#define XDND_POSITION_SOURCE_WIN(e)     ((e)->xclient.data.l[0])
#define XDND_POSITION_ROOT_X(e)         (((e)->xclient.data.l[2] & 0xffff0000) >> 16)
#define XDND_POSITION_ROOT_Y(e)         ((e)->xclient.data.l[2] & 0x0000ffff)
#define XDND_POSITION_ROOT_SET(e,x,y)   (e)->xclient.data.l[2]  = ((x) << 16) | ((y) & 0xFFFFUL)
#define XDND_POSITION_TIME(e)           ((e)->xclient.data.l[3])
#define XDND_POSITION_ACTION(e)         ((e)->xclient.data.l[4])

/* XdndStatus */
#define XDND_STATUS_TARGET_WIN(e)       ((e)->xclient.data.l[0])
#define XDND_STATUS_WILL_ACCEPT(e)      ((e)->xclient.data.l[1] & 0x1L)
#define XDND_STATUS_WILL_ACCEPT_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_STATUS_WANT_POSITION(e)    ((e)->xclient.data.l[1] & 0x2UL)
#define XDND_STATUS_WANT_POSITION_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x2UL) | (((b) == 0) ? 0 : 0x2UL)
#define XDND_STATUS_RECT_X(e)           ((e)->xclient.data.l[2] >> 16)
#define XDND_STATUS_RECT_Y(e)           ((e)->xclient.data.l[2] & 0xFFFFL)
#define XDND_STATUS_RECT_WIDTH(e)       ((e)->xclient.data.l[3] >> 16)
#define XDND_STATUS_RECT_HEIGHT(e)      ((e)->xclient.data.l[3] & 0xFFFFL)
#define XDND_STATUS_RECT_SET(e,x,y,w,h) {(e)->xclient.data.l[2] = ((x) << 16) | ((y) & 0xFFFFUL); (e)->xclient.data.l[3] = ((w) << 16) | ((h) & 0xFFFFUL); }
#define XDND_STATUS_ACTION(e)           ((e)->xclient.data.l[4])

/* XdndLeave */
#define XDND_LEAVE_SOURCE_WIN(e)        ((e)->xclient.data.l[0])

/* XdndDrop */
#define XDND_DROP_SOURCE_WIN(e)         ((e)->xclient.data.l[0])
#define XDND_DROP_TIME(e)               ((e)->xclient.data.l[2])

/* XdndFinished */
#define XDND_FINISHED_TARGET_WIN(e)     ((e)->xclient.data.l[0])
/*
#define XDND_FINISHED_ACCEPTED(e)       ((e)->xclient.data.l[1] & 0x1L)
#define XDND_FINISHED_ACCEPTED_SET(e,b)  (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
*/
#define XDND_FINISHED_ACCEPTED(e)       ((e)->xclient.data.l[1] &   (1 << 1))
#define XDND_FINISHED_ACCEPTED_NO(e)    ((e)->xclient.data.l[1] &= ~(1 << 1))
#define XDND_FINISHED_ACCEPTED_YES(e)   ((e)->xclient.data.l[1] |=  (1 << 1))
#define XDND_FINISHED_ACTION(e)         ((e)->xclient.data.l[2])

#ifndef CONST86
#define CONST86
#endif

extern int TkDND_GetSelection(Tcl_Interp *interp, Tk_Window tkwin,
                              Atom selection,
                              Atom target, Time time,
                              Tk_GetSelProc *proc, ClientData clientData);
extern void TkDND_InitialiseCursors(Tcl_Interp *interp);
extern Tk_Cursor TkDND_GetCursor(Tcl_Interp *interp, Tcl_Obj *name);

/*
 * Support for getting the wrapper window for our top-level...
 */

int TkDND_RegisterTypesObjCmd(ClientData clientData, Tcl_Interp *interp,
                              int objc, Tcl_Obj *CONST objv[]) {

  Atom version       = XDND_VERSION;
  Tk_Window path     = NULL;
  Tk_Window toplevel = NULL;

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "path toplevel types-list");
    return TCL_ERROR;
  }

  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  Tk_MakeWindowExist(path);

#if defined(TKDND_SET_XDND_PROPERTY_ON_WRAPPER) || \
    defined(TKDND_SET_XDND_PROPERTY_ON_TOPLEVEL)
  toplevel = TkDND_TkWin(objv[2]);
  if (!Tk_IsTopLevel(toplevel)) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "path \"", Tcl_GetString(objv[2]),
                             "\" is not a toplevel window!", (char *) NULL);
    return TCL_ERROR;
  }
  Tk_MakeWindowExist(toplevel);
  Tk_MapWindow(toplevel);
#endif

  /*
   * We must make the toplevel that holds this widget XDND aware. This means
   * that we have to set the XdndAware property on our toplevel.
   */
#ifdef TKDND_SET_XDND_PROPERTY_ON_TARGET
  XChangeProperty(Tk_Display(path), Tk_WindowId(path),
                  Tk_InternAtom(path, "XdndAware"),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char *) &version, 1);
#endif /* TKDND_SET_XDND_PROPERTY_ON_TARGET */

#ifdef TKDND_SET_XDND_PROPERTY_ON_WRAPPER
  if (Tk_HasWrapper(toplevel)) {
  }
#endif /* TKDND_SET_XDND_PROPERTY_ON_WRAPPER */

#ifdef TKDND_SET_XDND_PROPERTY_ON_TOPLEVEL
  Window root_return, parent, *children_return = 0;
  unsigned int nchildren_return;
  XQueryTree(Tk_Display(toplevel), Tk_WindowId(toplevel),
             &root_return, &parent,
             &children_return, &nchildren_return);
  if (children_return) XFree(children_return);
  XChangeProperty(Tk_Display(toplevel), parent,
                  Tk_InternAtom(toplevel, "XdndAware"),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char *) &version, 1);
#endif /* TKDND_SET_XDND_PROPERTY_ON_TOPLEVEL */
  return TCL_OK;
} /* TkDND_RegisterTypesObjCmd */

Tk_Window TkDND_GetToplevelFromWrapper(Tk_Window tkwin) {
  Window root_return, parent, *children_return = 0;
  unsigned int nchildren_return;
  Tk_Window toplevel = NULL;
  if (tkwin == NULL || Tk_IsTopLevel(tkwin)) return tkwin;
  XQueryTree(Tk_Display(tkwin), Tk_WindowId(tkwin),
           &root_return, &parent,
           &children_return, &nchildren_return);
  if (nchildren_return == 1) {
    toplevel = Tk_IdToWindow(Tk_Display(tkwin), children_return[0]);
  }
  if (children_return) XFree(children_return);
  return toplevel;
}; /* TkDND_GetToplevelFromWrapper */

Window TkDND_GetVirtualRootWindowOfScreen(Tk_Window tkwin) {
  static Screen *screen, *save_screen = (Screen *)0;
  static Window root = (Window)0;
  
  screen = Tk_Screen(tkwin);
  if (screen != save_screen) {
    Display *dpy = DisplayOfScreen(screen);
    int i;
    Window rootReturn, parentReturn, *children;
    unsigned int numChildren;
    Atom __SWM_VROOT = Tk_InternAtom(tkwin, "__SWM_VROOT"),
         __SWM_ROOT  = Tk_InternAtom(tkwin, "__SWM_ROOT"),
         __WM_ROOT   = Tk_InternAtom(tkwin, "__WM_ROOT");
    
    root = RootWindowOfScreen(screen);
    
    /* go look for a virtual root */
    if (XQueryTree(dpy, root, &rootReturn, &parentReturn,
                   &children, &numChildren)) {
      for (i = 0; i < numChildren; i++) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytesafter;
        Window *newRoot = (Window *)0;
    
        if (
             (XGetWindowProperty(dpy, children[i],
                __WM_ROOT, 0, (long) 1, False, XA_WINDOW,
                &actual_type, &actual_format, &nitems, &bytesafter,
                (unsigned char **) &newRoot) == Success
                && newRoot && (actual_type == XA_WINDOW)) ||
             (XGetWindowProperty(dpy, children[i],
                __SWM_ROOT, 0, (long) 1, False, XA_WINDOW,
                &actual_type, &actual_format, &nitems, &bytesafter,
                (unsigned char **) &newRoot) == Success
                && newRoot && (actual_type == XA_WINDOW)) ||
             (XGetWindowProperty(dpy, children[i],
                __SWM_VROOT, 0, (long) 1, False, XA_WINDOW,
                &actual_type, &actual_format, &nitems, &bytesafter,
                (unsigned char **) &newRoot) == Success
                && newRoot && (actual_type == XA_WINDOW))
           ) {
          root = *newRoot;
          break;
        }
      }
      if (children) XFree((char *)children);
    }
    save_screen = screen;
  }
  return root;
}; /* TkDND_GetVirtualRootWindowOfScreen */

int TkDND_HandleXdndEnter(Tk_Window tkwin, XEvent *xevent) {
  Tcl_Interp *interp = Tk_Interp(tkwin);
  Tk_Window toplevel;
  Atom *typelist = NULL;
  int i, version = (int) XDND_ENTER_VERSION(xevent);
  Window drag_source;
  // Window drop_toplevel, drop_window;
  Tcl_Obj* objv[4], *element;

  if (interp == NULL) return False;
  if (version > XDND_VERSION) return False;
#if XDND_VERSION >= 3
  if (version < 3) return False;
#endif

//#if XDND_VERSION >= 3
//  /* XdndEnter is delivered to the toplevel window, which is the wrapper
//   *  window for the Tk toplevel. We don't yet know the sub-window the mouse
//   *  is in... */
//  drop_toplevel = xevent->xany.window;
//  drop_window   = 0;
//#else
//  drop_toplevel = 0
//  drop_window   = xevent->xany.window;
//#endif
  drag_source = XDND_ENTER_SOURCE_WIN(xevent);
  toplevel    = TkDND_GetToplevelFromWrapper(tkwin);
  if (toplevel == NULL) toplevel = tkwin;

  if (XDND_ENTER_THREE_TYPES(xevent)) {
    typelist = (Atom *) Tcl_Alloc(sizeof(Atom)*4);
    if (typelist == NULL) return False;
    typelist[0] = xevent->xclient.data.l[2];
    typelist[1] = xevent->xclient.data.l[3];
    typelist[2] = xevent->xclient.data.l[4];
    typelist[3] = None;
  } else {
    /* Get the types from XdndTypeList property. */
    Atom actualType = None;
    int actualFormat;
    unsigned long itemCount, remainingBytes;
    Atom *data;
    XGetWindowProperty(xevent->xclient.display, drag_source,
                       Tk_InternAtom(tkwin, "XdndTypeList"), 0,
                       LONG_MAX, False, XA_ATOM, &actualType, &actualFormat,
                       &itemCount, &remainingBytes, (unsigned char **) &data);
    typelist = (Atom *) Tcl_Alloc(sizeof(Atom)*(itemCount+1));
    if (typelist == NULL) return False;
    for (i=0; i<itemCount; i++) { typelist[i] = data[i]; }
    typelist[itemCount] = None;
    if (data) XFree((unsigned char*)data);
  }
  /* We have all the information we need. Its time to pass it at the Tcl
   * level.*/
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndEnter", -1);
  objv[1] = Tcl_NewStringObj(Tk_PathName(toplevel), -1);
  objv[2] = Tcl_NewLongObj(drag_source);
  objv[3] = Tcl_NewListObj(0, NULL);
  for (i=0; typelist[i] != None; ++i) {
    element = Tcl_NewStringObj(Tk_GetAtomName(tkwin, typelist[i]), -1);
    Tcl_ListObjAppendElement(NULL, objv[3], element);
  }
  TkDND_Eval(4);
  Tcl_Free((char *) typelist);
  return True;
} /* TkDND_HandleXdndEnter */

int TkDND_HandleXdndPosition(Tk_Window tkwin, XEvent *xevent) {
  Tcl_Interp *interp = Tk_Interp(tkwin);
  Tk_Window mouse_tkwin = NULL, toplevel;
  Window drag_source, virtual_root, dummyChild;
  Tcl_Obj* result;
  Tcl_Obj* objv[5];
  int rootX, rootY, dx, dy, i, index, status, w, h;
  XEvent response;
  int width = 1, height = 1;
  static char *DropActions[] = {
    "copy", "move", "link", "ask",  "private", "refuse_drop", "default",
    (char *) NULL
  };
  enum dropactions {
    ActionCopy, ActionMove, ActionLink, ActionAsk, ActionPrivate,
    refuse_drop, ActionDefault
  };
/*Time time;
  Atom action;*/

  if (interp == NULL || tkwin == NULL) return False;

  drag_source = XDND_POSITION_SOURCE_WIN(xevent);
  /* Get the coordinates from the event... */
  rootX  = XDND_POSITION_ROOT_X(xevent);
  rootY  = XDND_POSITION_ROOT_Y(xevent);
  /* Get the time from the event... */
  /* time   = XDND_POSITION_TIME(xevent); */
  /* Get the user action from the event... */
  /* action = XDND_POSITION_ACTION(xevent); */

  /* The event may have been delivered to the toplevel wrapper.
   * Try to find the toplevel window... */
  toplevel = TkDND_GetToplevelFromWrapper(tkwin);
  if (toplevel == NULL) toplevel = tkwin;
  /* Get the virtual root window... */
  virtual_root = TkDND_GetVirtualRootWindowOfScreen(tkwin);
  if (virtual_root != None) {
    //XTranslateCoordinates(Tk_Display(tkwin), DefaultRootWindow(Tk_Display(tkwin)),
    //                      virtual_root, rootX, rootY, &dx, &dy, &dummyChild);
    XTranslateCoordinates(Tk_Display(tkwin), virtual_root,
                          Tk_WindowId(toplevel), rootX, rootY, &dx, &dy, &dummyChild);
    mouse_tkwin = Tk_IdToWindow(Tk_Display(tkwin), dummyChild);
  }
  if (!mouse_tkwin) {
    Tk_GetVRootGeometry(toplevel, &dx, &dy, &w, &h);
    mouse_tkwin = Tk_CoordsToWindow(rootX, rootY, toplevel);
  }
  if (!mouse_tkwin) mouse_tkwin = Tk_CoordsToWindow(rootX + dx, rootY + dy, tkwin);
#if 0
  printf("mouse_win: %p (%s) (%d, %d %p %s) i=%p\n", mouse_tkwin,
          mouse_tkwin?Tk_PathName(mouse_tkwin):"",
          rootX, rootY, tkwin, Tk_PathName(tkwin), interp);
#endif
  /* Ask the Tk widget whether it will accept the drop... */
  index = refuse_drop;
  if (mouse_tkwin != NULL) {
    objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndPosition", -1);
    objv[1] = Tcl_NewStringObj(Tk_PathName(mouse_tkwin), -1);
    objv[2] = Tcl_NewIntObj(rootX);
    objv[3] = Tcl_NewIntObj(rootY);
    objv[4] = Tcl_NewLongObj(drag_source);
    TkDND_Status_Eval(5);
    if (status == TCL_OK) {
      /* Get the returned action... */
      result = Tcl_GetObjResult(interp); Tcl_IncrRefCount(result);
      status = Tcl_GetIndexFromObj(interp, result, (const char **) DropActions,
                              "dropactions", 0, &index);
      Tcl_DecrRefCount(result);
      if (status != TCL_OK) index = refuse_drop;
    }
  }
  /* Sent a XdndStatus event, to notify the drag source */
  memset (&response, 0, sizeof(xevent));
  response.xany.type            = ClientMessage;
  response.xany.display         = xevent->xclient.display;
  response.xclient.window       = drag_source;
  response.xclient.message_type = Tk_InternAtom(tkwin, "XdndStatus");
  response.xclient.format       = 32;
  XDND_STATUS_WILL_ACCEPT_SET(&response, 1);
  XDND_STATUS_WANT_POSITION_SET(&response, 1);
  XDND_STATUS_RECT_SET(&response, rootX, rootY, width, height);
#if XDND_VERSION >= 3
  XDND_STATUS_TARGET_WIN(&response) = Tk_WindowId(tkwin);
#else
  XDND_STATUS_TARGET_WIN(&response) = xevent->xany.window;
#endif
  switch ((enum dropactions) index) {
    case ActionDefault:
    case ActionCopy:
      XDND_STATUS_ACTION(&response) = Tk_InternAtom(tkwin, "XdndActionCopy");
      break;
    case ActionMove:
      XDND_STATUS_ACTION(&response) = Tk_InternAtom(tkwin, "XdndActionMove");
      break;
    case ActionLink:
      XDND_STATUS_ACTION(&response) = Tk_InternAtom(tkwin, "XdndActionLink");
      break;
    case ActionAsk:
      XDND_STATUS_ACTION(&response) = Tk_InternAtom(tkwin, "XdndActionAsk");
      break;
    case ActionPrivate: 
      XDND_STATUS_ACTION(&response) = Tk_InternAtom(tkwin, "XdndActionPrivate");
      break;
    case refuse_drop: {
      XDND_STATUS_WILL_ACCEPT_SET(&response, 0); /* Refuse drop. */
    }
  }
  XSendEvent(response.xany.display, response.xclient.window,
             False, NoEventMask, (XEvent*)&response);
  return True;
} /* TkDND_HandleXdndPosition */

int TkDND_HandleXdndLeave(Tk_Window tkwin, XEvent *xevent) {
  Tcl_Interp *interp = Tk_Interp(tkwin);
  Tcl_Obj* objv[1];
  int i;
  if (interp == NULL) return False; 
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndLeave", -1);
  TkDND_Eval(1);
  return True;
} /* TkDND_HandleXdndLeave */

int TkDND_HandleXdndDrop(Tk_Window tkwin, XEvent *xevent) {
  XEvent finished;
  Tcl_Interp *interp = Tk_Interp(tkwin);
  Tcl_Obj* objv[2], *result;
  int status, i, index;
  Time time = CurrentTime;
  static char *DropActions[] = {
    "copy", "move", "link", "ask",  "private", "refuse_drop", "default",
    (char *) NULL
  };
  enum dropactions {
    ActionCopy, ActionMove, ActionLink, ActionAsk, ActionPrivate,
    refuse_drop, ActionDefault
  };
    
  if (interp == NULL) return False;
  if (XDND_DROP_TIME(xevent) != 0) {
    time = ((sizeof(Time) == 8 && XDND_DROP_TIME(xevent) < 0)
             ? (unsigned int) (XDND_DROP_TIME(xevent))
             :  XDND_DROP_TIME(xevent));
  }

  memset(&finished, 0, sizeof(XEvent));
  finished.xclient.type         = ClientMessage;
  finished.xclient.window       = XDND_DROP_SOURCE_WIN(xevent);
  finished.xclient.message_type = Tk_InternAtom(tkwin, "XdndFinished");
  finished.xclient.format       = 32;
#if XDND_VERSION >= 3
  XDND_FINISHED_TARGET_WIN(&finished) = Tk_WindowId(tkwin);
#else
  XDND_FINISHED_TARGET_WIN(&finished) = xevent->xany.window;
#endif
  XDND_FINISHED_ACCEPTED_YES(&finished);
  //XFlush(Tk_Display(tkwin));

  /* Call out Tcl callback. */
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndDrop", -1);
  objv[1] = Tcl_NewLongObj(time);
  TkDND_Status_Eval(2);
  if (status == TCL_OK) {
    /* Get the returned action... */
    result = Tcl_GetObjResult(interp); Tcl_IncrRefCount(result);
    status = Tcl_GetIndexFromObj(interp, result, (const char **) DropActions,
                            "dropactions", 0, &index);
    Tcl_DecrRefCount(result);
    if (status != TCL_OK) index = refuse_drop;
    switch ((enum dropactions) index) {
      case ActionDefault:
      case ActionCopy:
        XDND_FINISHED_ACTION(&finished) =
            Tk_InternAtom(tkwin, "XdndActionCopy");    break;
      case ActionMove:
        XDND_FINISHED_ACTION(&finished) =
            Tk_InternAtom(tkwin, "XdndActionMove");    break;
      case ActionLink:
        XDND_FINISHED_ACTION(&finished) =
            Tk_InternAtom(tkwin, "XdndActionLink");    break;
      case ActionAsk:
        XDND_FINISHED_ACTION(&finished) =
            Tk_InternAtom(tkwin, "XdndActionAsk");     break;
      case ActionPrivate: 
        XDND_FINISHED_ACTION(&finished) =
            Tk_InternAtom(tkwin, "XdndActionPrivate"); break;
      case refuse_drop: {
        XDND_FINISHED_ACCEPTED_NO(&finished); /* Drop canceled. */
        XDND_FINISHED_ACTION(&finished) = None;
      }
    }
  } else {
    XDND_FINISHED_ACCEPTED_NO(&finished);
    XDND_FINISHED_ACTION(&finished) = None;
  }
  /* Send XdndFinished. */
  XSendEvent(Tk_Display(tkwin), finished.xclient.window,
             False, NoEventMask, (XEvent*)&finished);
  return True;
} /* TkDND_HandleXdndDrop */

int TkDND_HandleXdndStatus(Tk_Window tkwin, XEvent *xevent) {
  Tcl_Interp *interp = Tk_Interp(tkwin);
  Tcl_Obj *objv[2], *key, *value;
  int i;
  Atom action;
  if (interp == NULL) return False; 
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndStatus", -1);
  objv[1] = Tcl_NewDictObj();
  /* data.l[0] contains the XID of the target window */
  TkDND_Dict_PutLong(objv[1], "target", xevent->xclient.data.l[0]);
  /* data.l[1] bit 0 is set if the current target will accept the drop */
  TkDND_Dict_PutInt(objv[1], "accept", XDND_STATUS_WILL_ACCEPT(xevent) ? 1:0);
  /* data.l[1] bit 1 is set if the target wants XdndPosition messages while
   * the mouse moves inside the rectangle in data.l[2,3] */
  TkDND_Dict_PutInt(objv[1], "want_position",
    XDND_STATUS_WANT_POSITION(xevent) ? 1 : 0);
  /* data.l[4] contains the action accepted by the target */
  action = XDND_STATUS_ACTION(xevent);
  if (action == Tk_InternAtom(tkwin, "XdndActionCopy")) {
    TkDND_Dict_Put(objv[1], "action", "copy");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionMove")) {
    TkDND_Dict_Put(objv[1], "action", "move");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionLink")) {
    TkDND_Dict_Put(objv[1], "action", "link");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionAsk")) {
    TkDND_Dict_Put(objv[1], "action", "ask");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionPrivate")) {
    TkDND_Dict_Put(objv[1], "action", "private");
  } else {
    TkDND_Dict_Put(objv[1], "action", "refuse_drop");
  }
  TkDND_Dict_PutInt(objv[1], "x", XDND_STATUS_RECT_X(xevent));
  TkDND_Dict_PutInt(objv[1], "y", XDND_STATUS_RECT_Y(xevent));
  TkDND_Dict_PutInt(objv[1], "w", XDND_STATUS_RECT_WIDTH(xevent));
  TkDND_Dict_PutInt(objv[1], "h", XDND_STATUS_RECT_HEIGHT(xevent));
  
  TkDND_Eval(2);
  return True;
} /* TkDND_HandleXdndStatus */

int TkDND_HandleXdndFinished(Tk_Window tkwin, XEvent *xevent) {
   Tcl_Interp *interp = Tk_Interp(tkwin);
  Tcl_Obj *objv[2], *key, *value;
  int i;
  Atom action;
  if (interp == NULL) return False; 
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_HandleXdndFinished", -1);
  objv[1] = Tcl_NewDictObj();
  /* data.l[0] contains the XID of the target window */
  TkDND_Dict_PutLong(objv[1], "target", xevent->xclient.data.l[0]);
  /* data.l[1] bit 0 is set if the current target accepted the drop and
   *  successfully performed the accepted drop action */
  TkDND_Dict_PutInt(objv[1], "accept", (xevent->xclient.data.l[1] & 0x1L)?1:0);
  /* data.l[2] contains the action performed by the target */
  action = xevent->xclient.data.l[2];
  if (action == Tk_InternAtom(tkwin, "XdndActionCopy")) {
    TkDND_Dict_Put(objv[1], "action", "copy");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionMove")) {
    TkDND_Dict_Put(objv[1], "action", "move");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionLink")) {
    TkDND_Dict_Put(objv[1], "action", "link");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionAsk")) {
    TkDND_Dict_Put(objv[1], "action", "ask");
  } else if (action == Tk_InternAtom(tkwin, "XdndActionPrivate")) {
    TkDND_Dict_Put(objv[1], "action", "private");
  } else {
    TkDND_Dict_Put(objv[1], "action", "refuse_drop");
  }
  TkDND_Eval(2);
  return True;
} /* TkDND_HandleXdndFinished */

static int TkDND_XDNDHandler(Tk_Window tkwin, XEvent *xevent) {
  if (xevent->type != ClientMessage) return False;

  if (xevent->xclient.message_type == Tk_InternAtom(tkwin, "XdndPosition")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndPosition\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndPosition(tkwin, xevent);
  } else if (xevent->xclient.message_type== Tk_InternAtom(tkwin, "XdndEnter")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndEnter\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndEnter(tkwin, xevent);
  } else if (xevent->xclient.message_type==Tk_InternAtom(tkwin, "XdndStatus")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndStatus\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndStatus(tkwin, xevent);
  } else if (xevent->xclient.message_type== Tk_InternAtom(tkwin, "XdndLeave")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndLeave\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndLeave(tkwin, xevent);
  } else if (xevent->xclient.message_type == Tk_InternAtom(tkwin, "XdndDrop")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndDrop\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndDrop(tkwin, xevent);
  } else if (xevent->xclient.message_type == 
                                         Tk_InternAtom(tkwin, "XdndFinished")) {
#ifdef DEBUG_CLIENTMESSAGE_HANDLER
    printf("XDND_HandleClientMessage: Received XdndFinished\n");
#endif /* DEBUG_CLIENTMESSAGE_HANDLER */
    return TkDND_HandleXdndFinished(tkwin, xevent);
  } else {
#ifdef TKDND_ENABLE_MOTIF_DROPS
    if (MotifDND_HandleClientMessage(dnd, xevent)) return True;
#endif /* TKDND_ENABLE_MOTIF_DROPS */
  }
  return False;
} /* TkDND_XDNDHandler */

/*
 * The following two functions were copied from tkSelect.c
 * If TIP 370 gets implemented, they will not be required.
 */
static int TkDND_SelGetProc(ClientData clientData,
                            Tcl_Interp *interp, CONST86 char *portion) {
  Tcl_DStringAppend(clientData, portion, -1);
  return TCL_OK;
}; /* TkDND_SelGetProc */

int TkDND_GetSelectionObjCmd(ClientData clientData, Tcl_Interp *interp,
                             int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window tkwin = Tk_MainWindow(interp);
  Atom target;
  Atom selection;
  Time time = CurrentTime;
  const char *targetName = NULL;
  Tcl_DString selBytes;
  int result;
  static const char *const getOptionStrings[] = {
      "-displayof", "-selection", "-time", "-type", NULL
  };
  enum getOptions { GET_DISPLAYOF, GET_SELECTION, GET_TIME, GET_TYPE };
  int getIndex;
  int count;
  Tcl_Obj **objs;
  const char *string;
  const char *path = NULL;
  const char *selName = NULL;

  for (count = objc-1, objs = ((Tcl_Obj **)objv)+1; count>0;
                count-=2, objs+=2) {
    string = Tcl_GetString(objs[0]);
    if (string[0] != '-') {
        break;
    }
    if (count < 2) {
        Tcl_AppendResult(interp, "value for \"", string,
                                 "\" missing", NULL);
        return TCL_ERROR;
    }
    
    if (Tcl_GetIndexFromObj(interp, objs[0], (const char **) getOptionStrings,
            "option", 0, &getIndex) != TCL_OK) {
        return TCL_ERROR;
    }
    
    switch ((enum getOptions) getIndex) {
    case GET_DISPLAYOF:
        path = Tcl_GetString(objs[1]);
        break;
    case GET_SELECTION:
        selName = Tcl_GetString(objs[1]);
        break;
    case GET_TYPE:
        targetName = Tcl_GetString(objs[1]);
        break;
    case GET_TIME:
        if (Tcl_GetLongFromObj(interp, objs[1], (long *) &time) != TCL_OK) {
          return TCL_ERROR;
        }
        break;
    }
  }
  if (path != NULL) {
      tkwin = Tk_NameToWindow(interp, path, tkwin);
  }
  if (tkwin == NULL) {
      return TCL_ERROR;
  }
  if (selName != NULL) {
      selection = Tk_InternAtom(tkwin, selName);
  } else {
      selection = XA_PRIMARY;
  }
  if (count > 1) {
      Tcl_WrongNumArgs(interp, 1, objv, "?-option value ...?");
      return TCL_ERROR;
  } else if (count == 1) {
      target = Tk_InternAtom(tkwin, Tcl_GetString(objs[0]));
  } else if (targetName != NULL) {
      target = Tk_InternAtom(tkwin, targetName);
  } else {
      target = XA_STRING;
  }
  Tcl_DStringInit(&selBytes);
  result = TkDND_GetSelection(interp, tkwin, selection, target, time,
                              TkDND_SelGetProc, &selBytes);
  if (result == TCL_OK) {
      Tcl_DStringResult(interp, &selBytes);
  }
  Tcl_DStringFree(&selBytes);
  return result;
} /* TkDND_GetSelectionObjCmd */

int TkDND_AnnounceTypeListObjCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window path;
  Tcl_Obj **type;
  int status, i, types;
  Atom *typelist;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "path types-list");
    return TCL_ERROR;
  }
  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  status = Tcl_ListObjGetElements(interp, objv[2], &types, &type);
  if (status != TCL_OK) return status;
  typelist = (Atom *) Tcl_Alloc(types * sizeof(Atom));
  if (typelist == NULL) return TCL_ERROR;
  for (i = 0; i < types; ++i) {
    typelist[i] = Tk_InternAtom(path, Tcl_GetString(type[i]));
  }
  XChangeProperty(Tk_Display(path), Tk_WindowId(path),
                  Tk_InternAtom(path, "XdndTypeList"),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char*) typelist, types);
  Tcl_Free((char *) typelist);
  return TCL_OK;
}; /* TkDND_AnnounceTypeListObjCmd */

int TkDND_AnnounceActionListObjCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window path;
  Tcl_Obj **action, **description;
  int status, i, actions, descriptions;
  Atom actionlist[10], descriptionlist[10];

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "path actions-list descriptions-list");
    return TCL_ERROR;
  }
  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  status = Tcl_ListObjGetElements(interp, objv[2], &actions, &action);
  if (status != TCL_OK) return status;
  status = Tcl_ListObjGetElements(interp, objv[3], &descriptions, &description);
  if (status != TCL_OK) return status;

  if (actions != descriptions) {
    Tcl_SetResult(interp, "number of actions != number of descriptions",
                                                              TCL_STATIC);
    return TCL_ERROR;
  }
  if (actions > 10) {
    Tcl_SetResult(interp, "too many actions/descriptions", TCL_STATIC);
    return TCL_ERROR;
  }

  for (i = 0; i < actions; ++i) {
    actionlist[i]      = Tk_InternAtom(path, Tcl_GetString(action[i]));
    descriptionlist[i] = Tk_InternAtom(path, Tcl_GetString(description[i]));
  }
  XChangeProperty(Tk_Display(path), Tk_WindowId(path),
                  Tk_InternAtom(path, "XdndActionList"),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char*) actionlist, actions);
  XChangeProperty(Tk_Display(path), Tk_WindowId(path),
                  Tk_InternAtom(path, "XdndActionDescription"),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char*) descriptionlist, descriptions);
  return TCL_OK;
}; /* TkDND_AnnounceActionListObjCmd */

int TkDND_GrabPointerObjCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window path;
  Tk_Cursor cursor;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "path cursor");
    return TCL_ERROR;
  }

  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  Tk_MakeWindowExist(path);

  cursor = TkDND_GetCursor(interp, objv[2]);
  if (cursor == None) {
    Tcl_SetResult(interp, "invalid cursor name: ", TCL_STATIC);
    Tcl_AppendResult(interp, Tcl_GetString(objv[2]));
    return TCL_ERROR;
  }

  if (XGrabPointer(Tk_Display(path), Tk_WindowId(path), False,
       ButtonPressMask   | ButtonReleaseMask |
       PointerMotionMask | EnterWindowMask   | LeaveWindowMask,
       GrabModeAsync, GrabModeAsync,
       None, (Cursor) cursor, CurrentTime) != GrabSuccess) {
    Tcl_SetResult(interp, "unable to grab mouse pointer", TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}; /* TkDND_GrabPointerObjCmd */

int TkDND_UnrabPointerObjCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window path;
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "path");
    return TCL_ERROR;
  }
  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  XUngrabPointer(Tk_Display(path), CurrentTime);
  return TCL_OK;
}; /* TkDND_GrabPointerObjCmd */

int TkDND_SetPointerCursorObjCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window path;
  Tk_Cursor cursor;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "path cursor");
    return TCL_ERROR;
  }

  path     = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  Tk_MakeWindowExist(path);

  cursor = TkDND_GetCursor(interp, objv[2]);
  if (cursor == None) {
    Tcl_SetResult(interp, "invalid cursor name: ", TCL_STATIC);
    Tcl_AppendResult(interp, Tcl_GetString(objv[2]));
    return TCL_ERROR;
  }

  if (XChangeActivePointerGrab(Tk_Display(path),
       ButtonPressMask   | ButtonReleaseMask |
       PointerMotionMask | EnterWindowMask   | LeaveWindowMask,
       (Cursor) cursor, CurrentTime) != GrabSuccess) {
    /* Tcl_SetResult(interp, "unable to update mouse pointer", TCL_STATIC);
       return TCL_ERROR; */
  }
  return TCL_OK;
}; /* TkDND_SetPointerCursorObjCmd */

void TkDND_AddStateInformation(Tcl_Interp *interp, Tcl_Obj *dict,
                               unsigned int state) {
  Tcl_Obj *key, *value;
  TkDND_Dict_PutInt(dict, "state",   state);
  /* Masks... */
  TkDND_Dict_PutInt(dict, "1",       state & Button1Mask ? 1 : 0);
  TkDND_Dict_PutInt(dict, "2",       state & Button2Mask ? 1 : 0);
  TkDND_Dict_PutInt(dict, "3",       state & Button3Mask ? 1 : 0);
  TkDND_Dict_PutInt(dict, "4",       state & Button4Mask ? 1 : 0);
  TkDND_Dict_PutInt(dict, "5",       state & Button5Mask ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Mod1",    state & Mod1Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Mod2",    state & Mod2Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Mod3",    state & Mod3Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Mod4",    state & Mod4Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Mod5",    state & Mod5Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Alt",     state & Mod1Mask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Shift",   state & ShiftMask   ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Lock",    state & LockMask    ? 1 : 0);
  TkDND_Dict_PutInt(dict, "Control", state & ControlMask ? 1 : 0);
}; /* TkDND_AddStateInformation */

int TkDND_HandleGenericEvent(ClientData clientData, XEvent *eventPtr) {
  Tcl_Interp *interp = (Tcl_Interp *) clientData;
  Tcl_Obj *dict, *key, *value;
  Tcl_Obj *objv[2], *result;
  int status, i;
  KeySym sym;
  Tk_Window main_window;

  if (interp == NULL) return 0;
  dict = Tcl_NewDictObj();

  switch (eventPtr->type) {
    case MotionNotify:
      TkDND_Dict_Put(dict,     "type",   "MotionNotify");
      TkDND_Dict_PutInt(dict,  "x",       eventPtr->xmotion.x);
      TkDND_Dict_PutInt(dict,  "y",       eventPtr->xmotion.y);
      TkDND_Dict_PutInt(dict,  "x_root",  eventPtr->xmotion.x_root);
      TkDND_Dict_PutInt(dict,  "y_root",  eventPtr->xmotion.y_root);
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xmotion.time);
      TkDND_AddStateInformation(interp,   dict,     eventPtr->xmotion.state);
      break;
    case ButtonPress:
      TkDND_Dict_Put(dict,     "type",   "ButtonPress");
      TkDND_Dict_PutInt(dict,  "x",       eventPtr->xbutton.x);
      TkDND_Dict_PutInt(dict,  "y",       eventPtr->xbutton.y);
      TkDND_Dict_PutInt(dict,  "x_root",  eventPtr->xbutton.x_root);
      TkDND_Dict_PutInt(dict,  "y_root",  eventPtr->xbutton.y_root);
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xbutton.time);
      TkDND_AddStateInformation(interp,   dict,     eventPtr->xbutton.state);
      TkDND_Dict_PutInt(dict,  "button",  eventPtr->xbutton.button);
      break;
    case ButtonRelease:
      TkDND_Dict_Put(dict,     "type",   "ButtonRelease");
      TkDND_Dict_PutInt(dict,  "x",       eventPtr->xbutton.x);
      TkDND_Dict_PutInt(dict,  "y",       eventPtr->xbutton.y);
      TkDND_Dict_PutInt(dict,  "x_root",  eventPtr->xbutton.x_root);
      TkDND_Dict_PutInt(dict,  "y_root",  eventPtr->xbutton.y_root);
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xbutton.time);
      TkDND_AddStateInformation(interp,   dict,     eventPtr->xbutton.state);
      TkDND_Dict_PutInt(dict,  "button",  eventPtr->xbutton.button);
      break;
    case KeyPress:
      TkDND_Dict_Put(dict,     "type",   "KeyPress");
      TkDND_Dict_PutInt(dict,  "x",       eventPtr->xkey.x);
      TkDND_Dict_PutInt(dict,  "y",       eventPtr->xkey.y);
      TkDND_Dict_PutInt(dict,  "x_root",  eventPtr->xkey.x_root);
      TkDND_Dict_PutInt(dict,  "y_root",  eventPtr->xkey.y_root);
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xkey.time);
      TkDND_AddStateInformation(interp,   dict,     eventPtr->xkey.state);
      TkDND_Dict_PutInt(dict,  "keycode", eventPtr->xkey.keycode);
      main_window = Tk_MainWindow(interp);
      sym = XKeycodeToKeysym(Tk_Display(main_window),
                             eventPtr->xkey.keycode, 0);
      TkDND_Dict_Put(dict,     "keysym",   XKeysymToString(sym));
      break;
    case KeyRelease:
      TkDND_Dict_Put(dict,     "type",   "KeyRelease");
      TkDND_Dict_PutInt(dict,  "x",       eventPtr->xkey.x);
      TkDND_Dict_PutInt(dict,  "y",       eventPtr->xkey.y);
      TkDND_Dict_PutInt(dict,  "x_root",  eventPtr->xkey.x_root);
      TkDND_Dict_PutInt(dict,  "y_root",  eventPtr->xkey.y_root);
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xkey.time);
      TkDND_AddStateInformation(interp,   dict,     eventPtr->xkey.state);
      TkDND_Dict_PutInt(dict,  "keycode", eventPtr->xkey.keycode);
      main_window = Tk_MainWindow(interp);
      sym = XKeycodeToKeysym(Tk_Display(main_window),
                             eventPtr->xkey.keycode, 0);
      TkDND_Dict_Put(dict,     "keysym",   XKeysymToString(sym));
      break;
    case EnterNotify:
      return 0;
      TkDND_Dict_Put(dict, "type", "EnterNotify");
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xcrossing.time);
      break;
    case LeaveNotify:
      return 0;
      TkDND_Dict_Put(dict, "type", "LeaveNotify");
      TkDND_Dict_PutLong(dict, "time",    eventPtr->xcrossing.time);
      break;
    case SelectionRequest:
      main_window = Tk_MainWindow(interp);
      TkDND_Dict_Put(dict, "type", "SelectionRequest");
      TkDND_Dict_PutLong(dict, "time",     eventPtr->xselectionrequest.time);
      TkDND_Dict_PutLong(dict, "owner",    eventPtr->xselectionrequest.owner);
      TkDND_Dict_PutLong(dict, "requestor",
                                eventPtr->xselectionrequest.requestor);
      TkDND_Dict_Put(dict,     "selection",
            Tk_GetAtomName(main_window, eventPtr->xselectionrequest.selection));
      TkDND_Dict_Put(dict,     "target",
            Tk_GetAtomName(main_window, eventPtr->xselectionrequest.target));
      TkDND_Dict_Put(dict,     "property",
            Tk_GetAtomName(main_window, eventPtr->xselectionrequest.property));
      break;
    default:
      Tcl_DecrRefCount(dict);
      return 0;
  }
  /* Call out Tcl callback. */
  objv[0] = Tcl_NewStringObj("tkdnd::xdnd::_process_drag_events", -1);
  objv[1] = dict;
  TkDND_Status_Eval(2);
  if (status == TCL_OK) {
    result = Tcl_GetObjResult(interp); Tcl_IncrRefCount(result);
    status = Tcl_GetIntFromObj(interp, result, &i);
    Tcl_DecrRefCount(result);
    if (status == TCL_OK) return i;
  } else {
    /* An error occured, stop the drag action... */
    Tcl_SetVar(interp, "::tkdnd::xdnd::_dragging", "0", TCL_GLOBAL_ONLY);
  }
  return 0;
}; /* TkDND_HandleGenericEvent */

int TkDND_RegisterGenericEventHandlerObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  if (objc != 1) {
    Tcl_WrongNumArgs(interp, 1, objv, "");
    return TCL_ERROR;
  }
  Tk_CreateGenericHandler(TkDND_HandleGenericEvent, interp);
  return TCL_OK;
}; /* TkDND_RegisterGenericEventHandlerObjCmd */

int TkDND_UnregisterGenericEventHandlerObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  if (objc != 1) {
    Tcl_WrongNumArgs(interp, 1, objv, "");
    return TCL_ERROR;
  }
  Tk_DeleteGenericHandler(TkDND_HandleGenericEvent, interp);
  return TCL_OK;
}; /* TkDND_UnegisterGenericEventHandlerObjCmd */

int TkDND_FindDropTargetWindowObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  int rootx, rooty;
  Tk_Window path;
  Window root, src, t;
  Window target = 0;
  int lx = 0, ly = 0, lx2, ly2;
  Display *display;
  Atom XdndAware;
  Atom type = 0;
  int f;
  unsigned long n, a;
  unsigned char *data = 0;

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "path rootx rooty");
    return TCL_ERROR;
  }
  path = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[2], &rootx) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &rooty) != TCL_OK) return TCL_ERROR;
  root = RootWindowOfScreen(Tk_Screen(path));
  display = Tk_Display(path);

  if (!XTranslateCoordinates(display, root, root, rootx, rooty,
                             &lx, &ly, &target)) return TCL_ERROR;
  if (target == root) return TCL_ERROR;
  src = root;
  XdndAware = Tk_InternAtom(path, "XdndAware");
  while (target != 0) {
    if (!XTranslateCoordinates(display, src, target, lx, ly, &lx2, &ly2, &t)) {
      target = 0; break; /* Error... */
    }
    lx = lx2; ly = ly2; src = target; type = 0; data = NULL;
    /* Check if we can find the XdndAware property... */
    XGetWindowProperty(display, target, XdndAware, 0, 0, False,
                       AnyPropertyType, &type, &f,&n,&a,&data);
    if (data) XFree(data);
    if (type) break; /* We have found a target! */
    /* Find child at the coordinates... */
    if (!XTranslateCoordinates(display, src, src, lx, ly, &lx2, &ly2, &target)){
      target = 0; break; /* Error */
    }
  }
  if (target) {
    Tcl_SetObjResult(interp, Tcl_NewLongObj(target));
  } else {
    Tcl_ResetResult(interp);
  }

  return TCL_OK;
}; /* TkDND_FindDropTargetWindowObjCmd */

int TkDND_FindDropTargetProxyObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  Window target, proxy, *proxy_ptr;
  Atom type = None;
  int f;
  unsigned long n, a;
  unsigned char *retval = NULL;
  Display *display;
  Tk_Window path;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "source target");
    return TCL_ERROR;
  }

  path = TkDND_TkWin(objv[1]);
  if (!path) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  display = Tk_Display(path);
  proxy = target;
  XGetWindowProperty(display, target, Tk_InternAtom(path, "XdndProxy"), 0, 1,
                     False, XA_WINDOW, &type, &f,&n,&a,&retval);
  proxy_ptr = (Window *) retval;
  if (type == XA_WINDOW && proxy_ptr) {
    proxy = *proxy_ptr;
    XFree(proxy_ptr);
    proxy_ptr = NULL;
    /* Is the XdndProxy property pointing to the same window? */
    XGetWindowProperty(display, proxy, Tk_InternAtom(path, "XdndProxy"), 0, 1,
                       False, XA_WINDOW, &type, &f,&n,&a,&retval);
    proxy_ptr = (Window *) retval;
    if (type != XA_WINDOW || !proxy_ptr || *proxy_ptr != proxy) {
      proxy = target;
    }
  }
  if (proxy_ptr) XFree(proxy_ptr);
  Tcl_SetObjResult(interp, Tcl_NewLongObj(proxy));

  return TCL_OK;
}; /* TkDND_FindDropTargetProxyObjCmd */

int TkDND_SendXdndEnterObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XEvent event;
  Tk_Window source;
  Window target, proxy;
  Display *display;
  int types, r, f, *tv, target_version = XDND_VERSION, flags, status, i;
  Atom t = None;
  unsigned long n, a;
  unsigned char *retval;
  Tcl_Obj **type;

  if (objc != 5) {
    Tcl_WrongNumArgs(interp, 1, objv, "source target proxy types_len");
    return TCL_ERROR;
  }
  
  source = TkDND_TkWin(objv[1]);
  if (!source) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetLongFromObj(interp, objv[3], (long *) &proxy) != TCL_OK) {
    return TCL_ERROR;
  }
  status = Tcl_ListObjGetElements(interp, objv[4], &types, &type);
  if (status != TCL_OK) return status;
  display = Tk_Display(source);

  /* Get the XDND version supported by the target... */
  r = XGetWindowProperty(display, proxy, Tk_InternAtom(source, "XdndAware"), 0, 1,
                         False, AnyPropertyType, &t, &f,&n,&a,&retval);
  if (r != Success) {
    Tcl_SetResult(interp, "cannot retrieve XDND version from target",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  tv = (int *)retval;
  if (tv) {
    if (*tv < target_version) target_version = *tv;
    XFree(tv);
  } 

  memset (&event, 0, sizeof(event));
  event.type                    = ClientMessage;
  event.xclient.window          = target;
  event.xclient.format          = 32;
  event.xclient.message_type    = Tk_InternAtom(source, "XdndEnter");
  XDND_ENTER_SOURCE_WIN(&event) = Tk_WindowId(source);
  flags = target_version << 24;
  if (types > 3) flags |= 0x0001;
  event.xclient.data.l[1] = flags;
  for (i = 0; i < types && i < 3; ++i) {
    event.xclient.data.l[2+i] = Tk_InternAtom(source, Tcl_GetString(type[i]));
  }
  XSendEvent(display, proxy, False, NoEventMask, &event);

  return TCL_OK;
}; /* TkDND_SendXdndEnterObjCmd */

int TkDND_SendXdndPositionObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  static char *DropActions[] = {
    "copy", "move", "link", "ask",  "private", "default",
    (char *) NULL
  };
  enum dropactions {
    ActionCopy, ActionMove, ActionLink, ActionAsk, ActionPrivate, ActionDefault
  };

  XEvent event;
  Tk_Window source;
  Window target, proxy;
  Display *display;
  int rootx, rooty, status, index;

  if (objc != 7) {
    Tcl_WrongNumArgs(interp, 1, objv, "source target proxy rootx rooty action");
    return TCL_ERROR;
  }
  
  source = TkDND_TkWin(objv[1]);
  if (!source) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetLongFromObj(interp, objv[3], (long *) &proxy) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, objv[4], &rootx) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[5], &rooty) != TCL_OK) return TCL_ERROR;
  status = Tcl_GetIndexFromObj(interp, objv[6], (const char **) DropActions,
                            "dropactions", 0, &index);
  if (status != TCL_OK) return status;
  display = Tk_Display(source);

  memset (&event, 0, sizeof(event));
  event.type                       = ClientMessage;
  event.xclient.window             = target;
  event.xclient.format             = 32;
  event.xclient.message_type       = Tk_InternAtom(source, "XdndPosition");
  event.xclient.data.l[0]          = Tk_WindowId(source);
  event.xclient.data.l[1]          = 0; // flags
  event.xclient.data.l[2]          = (rootx << 16) + rooty;
  event.xclient.data.l[3]          = CurrentTime;
  switch ((enum dropactions) index) {
    case ActionDefault:
    case ActionCopy:
      XDND_POSITION_ACTION(&event) =
          Tk_InternAtom(source, "XdndActionCopy");    break;
    case ActionMove:
      XDND_POSITION_ACTION(&event) =
          Tk_InternAtom(source, "XdndActionMove");    break;
    case ActionLink:
      XDND_POSITION_ACTION(&event) =
          Tk_InternAtom(source, "XdndActionLink");    break;
    case ActionAsk:
      XDND_POSITION_ACTION(&event) =
          Tk_InternAtom(source, "XdndActionAsk");     break;
    case ActionPrivate: 
      XDND_POSITION_ACTION(&event) =
          Tk_InternAtom(source, "XdndActionPrivate"); break;
  }

  XSendEvent(display, proxy, False, NoEventMask, &event);

  return TCL_OK;
}; /* TkDND_SendXdndPositionObjCmd */

int TkDND_SendXdndLeaveObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XEvent event;
  Tk_Window source;
  Window target, proxy;

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "source target proxy");
    return TCL_ERROR;
  }

  source = TkDND_TkWin(objv[1]);
  if (!source) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetLongFromObj(interp, objv[3], (long *) &proxy) != TCL_OK) {
    return TCL_ERROR;
  }

  memset (&event, 0, sizeof(event));
  event.type                       = ClientMessage;
  event.xclient.window             = target;
  event.xclient.format             = 32;
  event.xclient.message_type       = Tk_InternAtom(source, "XdndLeave");
  event.xclient.data.l[0]          = Tk_WindowId(source);
  XSendEvent(Tk_Display(source), proxy, False, NoEventMask, &event);
  return TCL_OK;
}; /* TkDND_SendXdndLeaveObjCmd */

int TkDND_SendXdndDropObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XEvent event;
  Tk_Window source;
  Window target, proxy;

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "source target proxy");
    return TCL_ERROR;
  }

  source = TkDND_TkWin(objv[1]);
  if (!source) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetLongFromObj(interp, objv[3], (long *) &proxy) != TCL_OK) {
    return TCL_ERROR;
  }

  memset (&event, 0, sizeof(event));
  event.type                       = ClientMessage;
  event.xclient.window             = target;
  event.xclient.format             = 32;
  event.xclient.message_type       = Tk_InternAtom(source, "XdndDrop");
  event.xclient.data.l[0]          = Tk_WindowId(source);
  event.xclient.data.l[2]          = CurrentTime;
  XSendEvent(Tk_Display(source), proxy, False, NoEventMask, &event);
  Tcl_SetObjResult(interp, Tcl_NewLongObj(event.xclient.data.l[2]));
  return TCL_OK;
}; /* TkDND_SendXdndDropObjCmd */

int TkDND_XChangePropertyObjCmd(ClientData clientData,
                         Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XEvent event;
  Window target;
  Atom property = None, type = None;
  int format, numItems, numFields, i;
  Display *display;
  Tk_Window source;
  Time time;
  unsigned char *data = NULL;
  Tcl_Obj **field;

  if (objc != 9) {
    Tcl_WrongNumArgs(interp, 1, objv,
        "source requestor property type format time data data_items");
    return TCL_ERROR;
  }

  source = TkDND_TkWin(objv[1]);
  if (!source) return TCL_ERROR;
  if (Tcl_GetLongFromObj(interp, objv[2], (long *) &target) != TCL_OK) {
    return TCL_ERROR;
  }
  display  = Tk_Display(source);
  property = Tk_InternAtom(source, Tcl_GetString(objv[3]));
  type     = Tk_InternAtom(source, Tcl_GetString(objv[4]));
  if (Tcl_GetIntFromObj(interp, objv[5], &format) != TCL_OK) {
    return TCL_ERROR;
  }
  if (format != 8 && format != 16 && format != 32) {
    Tcl_SetResult(interp, "unsupported format: not 8, 16 or 32", TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, objv[5], &format) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetLongFromObj(interp, objv[6], (long *) &time) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, objv[8], &numItems) != TCL_OK) {
    return TCL_ERROR;
  }
  if (!time) time = CurrentTime;
  switch (format) {
    case 8:
      data = (unsigned char *) Tcl_GetString(objv[7]);
      break;
    case 16: {
      short *propPtr = (short *) Tcl_Alloc(sizeof(short)*numItems);
      data = (unsigned char *) propPtr;
      if (Tcl_ListObjGetElements(interp, objv[7], &numFields, &field)
                                                                   != TCL_OK) {
        return TCL_ERROR;
      }
      for (i = 0; i < numItems; i++) {
	char *dummy;
	propPtr[i] = (short) strtol(Tcl_GetString(field[i]), &dummy, 0);
      }
      break;
    }
    case 32: {
      long *propPtr  = (long *) Tcl_Alloc(sizeof(long)*numItems);
      data = (unsigned char *) propPtr;
      if (Tcl_ListObjGetElements(interp, objv[7], &numFields, &field)
                                                                   != TCL_OK) {
        return TCL_ERROR;
      }
      for (i = 0; i < numItems; i++) {
	char *dummy;
	propPtr[i] = (short) strtol(Tcl_GetString(field[i]), &dummy, 0);
      }
      break;
    }
  }
  XChangeProperty(display, target, property, type, format, PropModeReplace,
         (unsigned char *) data, numItems);
  if (format > 8 && data) Tcl_Free((char *) data);
  /* Send selection notify to requestor... */
  event.xselection.type      = SelectionNotify;
  event.xselection.display   = display;
  event.xselection.requestor = target;
  event.xselection.selection = Tk_InternAtom(source, "XdndSelection");
  event.xselection.target    = type;
  event.xselection.property  = property;
  event.xselection.time      = time;
  XSendEvent(display, target, False, NoEventMask, &event);
  return TCL_OK;
}; /* TkDND_XChangePropertyObjCmd */

/*
 * For C++ compilers, use extern "C"
 */
#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Tkdnd_Init(Tcl_Interp *interp);
DLLEXPORT int Tkdnd_SafeInit(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif

int DLLEXPORT Tkdnd_Init(Tcl_Interp *interp) {
  int major, minor, patchlevel;
  Tcl_CmdInfo info;

  if (
#ifdef USE_TCL_STUBS 
      Tcl_InitStubs(interp, "8.3", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.3", 0)
#endif /* USE_TCL_STUBS */
            == NULL) {
            return TCL_ERROR;
  }
  if (
#ifdef USE_TK_STUBS
       Tk_InitStubs(interp, "8.3", 0)
#else
       Tcl_PkgRequire(interp, "Tk", "8.3", 0)
#endif /* USE_TK_STUBS */
            == NULL) {
            return TCL_ERROR;
  }

  /*
   * Get the version, because we really need 8.3.3+.
   */
  Tcl_GetVersion(&major, &minor, &patchlevel, NULL);
  if ((major == 8) && (minor == 3) && (patchlevel < 3)) {
    Tcl_SetResult(interp, "tkdnd requires Tk 8.3.3 or greater", TCL_STATIC);
    return TCL_ERROR;
  }

  if (Tcl_GetCommandInfo(interp, "selection", &info) == 0) {
    Tcl_SetResult(interp, "selection Tk command not found", TCL_STATIC);
    return TCL_ERROR;
  }

  /* Register the various commands */
  if (Tcl_CreateObjCommand(interp, "_register_types",
           (Tcl_ObjCmdProc*) TkDND_RegisterTypesObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_selection_get",
           (Tcl_ObjCmdProc*) TkDND_GetSelectionObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_grab_pointer",
           (Tcl_ObjCmdProc*) TkDND_GrabPointerObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_ungrab_pointer",
           (Tcl_ObjCmdProc*) TkDND_UnrabPointerObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_set_pointer_cursor",
           (Tcl_ObjCmdProc*) TkDND_SetPointerCursorObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_register_generic_event_handler",
           (Tcl_ObjCmdProc*) TkDND_RegisterGenericEventHandlerObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_unregister_generic_event_handler",
           (Tcl_ObjCmdProc*) TkDND_UnregisterGenericEventHandlerObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }
  
  if (Tcl_CreateObjCommand(interp, "_announce_type_list",
           (Tcl_ObjCmdProc*) TkDND_AnnounceTypeListObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_announce_action_list",
           (Tcl_ObjCmdProc*) TkDND_AnnounceActionListObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_find_drop_target_window",
           (Tcl_ObjCmdProc*) TkDND_FindDropTargetWindowObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_find_drop_target_proxy",
           (Tcl_ObjCmdProc*) TkDND_FindDropTargetProxyObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_send_XdndEnter",
           (Tcl_ObjCmdProc*) TkDND_SendXdndEnterObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_send_XdndPosition",
           (Tcl_ObjCmdProc*) TkDND_SendXdndPositionObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_send_XdndLeave",
           (Tcl_ObjCmdProc*) TkDND_SendXdndLeaveObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_send_XdndDrop",
           (Tcl_ObjCmdProc*) TkDND_SendXdndDropObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "XChangeProperty",
           (Tcl_ObjCmdProc*) TkDND_XChangePropertyObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
    return TCL_ERROR;
  }

  /* Create the cursors... */
  TkDND_InitialiseCursors(interp);

  /* Finally, register the XDND Handler... */
  Tk_CreateClientMessageHandler(&TkDND_XDNDHandler);

  Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);
  return TCL_OK;
} /* Tkdnd_Init */

int DLLEXPORT Tkdnd_SafeInit(Tcl_Interp *interp) {
  return Tkdnd_Init(interp);
} /* Tkdnd_SafeInit */
