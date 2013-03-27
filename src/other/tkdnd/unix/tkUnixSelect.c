/*
 * tkUnixSelect.c --
 *
 *	This file contains X specific routines for manipulating selections.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tk.h"
#include "X11/Xutil.h"

/*
 * Forward declarations for functions defined in this file:
 */
typedef struct {
  Tcl_Interp     *interp;
  Tk_GetSelProc  *proc;
  ClientData      clientData;
  Tcl_TimerToken  timeout;
  Tk_Window       tkwin;
  Atom            property;
  int             result;
  int             idleTime;
} TkDND_ProcDetail;

void TkDND_SelectionNotifyEventProc(ClientData clientData, XEvent *eventPtr);
void TkDND_PropertyNotifyEventProc(ClientData clientData, XEvent *eventPtr);
static void TkDND_SelTimeoutProc(ClientData clientData);

static inline int maxSelectionIncr(Display *dpy) {
  return XMaxRequestSize(dpy) > 65536 ? 65536*4 :
                               XMaxRequestSize(dpy)*4 - 100;
}; /* maxSelectionIncr */

int TkDND_ClipboardReadProperty(Tk_Window tkwin,
                                Atom property, int deleteProperty,
                                TkDND_ProcDetail *detail,
                                int *size, Atom *type, int *format) {
    Display *display = Tk_Display(tkwin);
    Window   win     = Tk_WindowId(tkwin);
    int      maxsize = maxSelectionIncr(display);
    unsigned long    bytes_left; // bytes_after
    unsigned long    length;     // nitems
    unsigned char   *data;
    Atom     dummy_type;
    int      dummy_format;
    int      r;
    Tcl_DString *buffer = (Tcl_DString *) detail->clientData;

    if (!type)                                // allow null args
        type = &dummy_type;
    if (!format)
        format = &dummy_format;

    // Don't read anything, just get the size of the property data
    r = XGetWindowProperty(display, win, property, 0, 0, False,
                            AnyPropertyType, type, format,
                            &length, &bytes_left, &data);
    if (r != Success || (type && *type == None)) {
        return 0;
    }
    XFree((char*)data);

    int offset = 0, format_inc = 1, proplen = bytes_left;

    switch (*format) {
    case 8:
    default:
        format_inc = sizeof(char) / 1;
        break;

    case 16:
        format_inc = sizeof(short) / 2;
        proplen   *= sizeof(short) / 2;
        break;

    case 32:
        format_inc = sizeof(long) / 4;
        proplen   *= sizeof(long) / 4;
        break;
    }

    while (bytes_left) {
      r = XGetWindowProperty(display, win, property, offset, maxsize/4,
                             False, AnyPropertyType, type, format,
                             &length, &bytes_left, &data);
      if (r != Success || (type && *type == None))
          break;
      switch (*format) {
        case 8:
        default:
          offset += length / (32 / *format);
          length *= format_inc * (*format) / 8;
          Tcl_DStringAppend(buffer, (char *) data, length);
          break;
        case 16: {
          register unsigned short *propPtr = (unsigned short *) data;
          for (; length > 0; propPtr++, length--) {
            char buf[12];

	    sprintf(buf, "0x%04x", (unsigned short) *propPtr);
	    Tcl_DStringAppendElement(buffer, buf);
          }
          Tcl_DStringAppend(buffer, " ", 1);
          break;
        }
        case 32: {
          register unsigned long *propPtr = (unsigned long *) data;
          for (; length > 0; propPtr++, length--) {
            char buf[12];

	    sprintf(buf, "0x%x", (unsigned int) *propPtr);
	    Tcl_DStringAppendElement(buffer, buf);
          }
          Tcl_DStringAppend(buffer, " ", 1);
          break;
        }
      }

      XFree((char*)data);
    }
#if 0
    printf("Selection details:\n");
    printf("  type: %s\n", XGetAtomName(display, *type));
    printf("  format: %d %s\n", *format, XGetAtomName(display, *format));
    printf("  length: %d\n", Tcl_DStringLength(buffer));
    printf("  data: \"%s\"\n", Tcl_DStringValue(buffer));
#endif

    if (*format == 8 && *type == Tk_InternAtom(tkwin, "COMPOUND_TEXT")) {
      // convert COMPOUND_TEXT to a multibyte string
      XTextProperty textprop;
      textprop.encoding = *type;
      textprop.format = *format;
      textprop.nitems = Tcl_DStringLength(buffer);
      textprop.value = (unsigned char *) Tcl_DStringValue(buffer);

      char **list_ret = 0;
      int count;
      if (XmbTextPropertyToTextList(display, &textprop, &list_ret,
                   &count) == Success && count && list_ret) {
        Tcl_DStringFree(buffer);
        Tcl_DStringInit(buffer);
        Tcl_DStringAppend(buffer, list_ret[0], -1);
      }
      if (list_ret) XFreeStringList(list_ret);
    }

    // correct size, not 0-term.
    if (size) *size = Tcl_DStringLength(buffer);
    if (deleteProperty) XDeleteProperty(display, win, property);
    //XFlush(display);
    return 1;
}; /* TkDND_ClipboardReadProperty */

int TkDND_ClipboardReadIncrementalProperty(Tk_Window tkwin,
                                           Atom property,
                                           TkDND_ProcDetail *detail) {
  TkDND_ProcDetail detail2;
  Tcl_DString     *buffer  = (Tcl_DString *) detail->clientData;
  detail2.interp           = detail->interp;
  detail2.tkwin            = detail->tkwin;
  detail2.property         = detail->property;
  detail2.proc             = NULL;
  detail2.clientData       = buffer;
  detail2.result           = -1;
  detail2.idleTime         = 0;
  Tcl_DStringFree(buffer);
  Tcl_DStringInit(buffer);

  //XFlush(display);
  /* Install a handler for PropertyNotify events... */
  Tk_CreateEventHandler(tkwin, PropertyNotify,
                        TkDND_PropertyNotifyEventProc, &detail2);
  /*
   * Enter a loop processing X events until the selection has been retrieved
   * and processed. If no response is received within a few seconds, then
   * timeout.
   */
  detail2.timeout = Tcl_CreateTimerHandler(1000, TkDND_SelTimeoutProc,
                                           &detail2);
  while (detail2.result == -1) {
    //XFlush(display);
    Tcl_DoOneEvent(0);
  }
  Tk_DeleteEventHandler(tkwin, PropertyNotify,
                        TkDND_PropertyNotifyEventProc, &detail2);
  if (detail2.timeout) Tcl_DeleteTimerHandler(detail2.timeout);
  return detail2.result;
}; /* TkDND_ClipboardReadIncrementalProperty */

void TkDND_SelectionNotifyEventProc(ClientData clientData, XEvent *eventPtr) {
  TkDND_ProcDetail *detail = (TkDND_ProcDetail *) clientData;
  int status, size, format;
  Atom type;

  status = TkDND_ClipboardReadProperty(detail->tkwin, detail->property, 1,
                                       detail, &size, &type, &format);
  if (status) {
#ifdef TKDND_DebugSelectionRequests
    if (eventPtr != NULL) {
    printf("SelectionNotify: selection: %s, target: %s, property: %s,\
            type: %s, format: %d\n",
            Tk_GetAtomName(detail->tkwin, eventPtr->xselection.selection),
            Tk_GetAtomName(detail->tkwin, eventPtr->xselection.target),
            Tk_GetAtomName(detail->tkwin, eventPtr->xselection.property),
            Tk_GetAtomName(detail->tkwin, type), format);
    }
#endif /* TKDND_DebugSelectionRequests */
    if (type == Tk_InternAtom(detail->tkwin, "INCR")) {
      status = TkDND_ClipboardReadIncrementalProperty(detail->tkwin,
                                       detail->property, detail);
    }
  }
  if (status) detail->result = TCL_OK;
  else {
    /* Do not report the error if this has not be called by a
     *  SelectionNotify event... */
    if (eventPtr != NULL) detail->result = TCL_ERROR;
  }
}; /* TkDND_SelectionNotifyEventProc */

void TkDND_PropertyNotifyEventProc(ClientData clientData, XEvent *eventPtr) {
  TkDND_ProcDetail *detail = (TkDND_ProcDetail *) clientData;
  Tcl_DString      *buffer = (Tcl_DString *) detail->clientData;
  Tcl_DString       ds;
  int status, size, format;
  Atom type;
  if (eventPtr->xproperty.atom != detail->property ||
      eventPtr->xproperty.state != PropertyNewValue) return;
  /* We will call TkDND_ClipboardReadProperty to read the property. Ensure that
   * a temporary DString will be used... */ 
  Tcl_DStringInit(&ds);
  detail->clientData = &ds;
  status = TkDND_ClipboardReadProperty(detail->tkwin, detail->property, 1,
                                       detail, &size, &type, &format);
  detail->clientData = buffer;
  if (status) {
    if (size == 0) {
      /* We are done! */
      detail->result = status;
    } else {
      Tcl_DStringAppend(buffer, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    }
  } else {
    /* An error occured... */
    detail->result = status;
  }
  Tcl_DStringFree(&ds);
}; /* TkDND_PropertyNotifyEventProc */

/*
 *----------------------------------------------------------------------
 *
 * TkDNDSelGetSelection --
 *
 *	Retrieve the specified selection from another process.
 *
 * Results:
 *	The return value is a standard Tcl return value. If an error occurs
 *	(such as no selection exists) then an error message is left in the
 *	interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkDNDSelGetSelection(
    Tcl_Interp *interp,		/* Interpreter to use for reporting errors. */
    Tk_Window tkwin,		/* Window on whose behalf to retrieve the
				 * selection (determines display from which to
				 * retrieve). */
    Atom selection,		/* Selection to retrieve. */
    Atom target,		/* Desired form in which selection is to be
				 * returned. */
    Time time,
    Tk_GetSelProc *proc,	/* Function to call to process the selection,
				 * once it has been retrieved. */
    ClientData clientData)	/* Arbitrary value to pass to proc. */
{
    TkDND_ProcDetail detail;
    Tk_Window sel_tkwin = Tk_MainWindow(interp);
    Display *display    = Tk_Display(tkwin);
    detail.interp       = interp;
    detail.tkwin        = sel_tkwin;
    detail.property     = selection;
    detail.proc         = proc;
    detail.clientData   = clientData;
    detail.result       = -1;
    detail.idleTime     = 0;

    XFlush(display);
    if (XGetSelectionOwner(display, selection) == None) {
      Tcl_SetResult(interp, "no owner for selection", TCL_STATIC);
      return TCL_ERROR;
    }
    /*
     * Initiate the request for the selection. Note: can't use TkCurrentTime
     * for the time. If we do, and this application hasn't received any X
     * events in a long time, the current time will be way in the past and
     * could even predate the time when the selection was made; if this
     * happens, the request will be rejected.
     */
    Tcl_ThreadAlert(Tcl_GetCurrentThread());
    /* Register an event handler for tkwin... */
    Tk_CreateEventHandler(sel_tkwin, SelectionNotify,
                          TkDND_SelectionNotifyEventProc, &detail);
    XConvertSelection(display, selection, target,
	              selection, Tk_WindowId(sel_tkwin), time);
    XFlush(display);
    /*
     * Enter a loop processing X events until the selection has been retrieved
     * and processed. If no response is received within a few seconds, then
     * timeout.
     */
    detail.timeout = Tcl_CreateTimerHandler(70, TkDND_SelTimeoutProc,
	                                    &detail);
    while (detail.result == -1) {
      TkDND_SelectionNotifyEventProc(&detail, NULL);
      Tcl_DoOneEvent(0);
    }
    Tk_DeleteEventHandler(sel_tkwin, SelectionNotify,
                          TkDND_SelectionNotifyEventProc, &detail);
    if (detail.timeout) Tcl_DeleteTimerHandler(detail.timeout);

    return detail.result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkDND_SelTimeoutProc --
 *
 *	This function is invoked once every second while waiting for the
 *	selection to be returned. After a while it gives up and aborts the
 *	selection retrieval.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new timer callback is created to call us again in another second,
 *	unless time has expired, in which case an error is recorded for the
 *	retrieval.
 *
 *----------------------------------------------------------------------
 */

static void
TkDND_SelTimeoutProc(
    ClientData clientData)	/* Information about retrieval in progress. */
{
    register TkDND_ProcDetail *retrPtr = (TkDND_ProcDetail *) clientData;

    /*
     * Make sure that the retrieval is still in progress. Then see how long
     * it's been since any sort of response was received from the other side.
     */

    TkDND_SelectionNotifyEventProc(retrPtr, NULL);
    if (retrPtr->result != -1) {
	return;
    }
    XFlush(Tk_Display(retrPtr->tkwin));
    if (retrPtr->idleTime > 3){
      Tcl_ThreadAlert(Tcl_GetCurrentThread());
      XFlush(Tk_Display(retrPtr->tkwin));
    }
    retrPtr->idleTime++;
    if (retrPtr->idleTime >= 6) {
	/*
	 * Use a careful function to store the error message, because the
	 * result could already be partially filled in with a partial
	 * selection return.
	 */

	Tcl_SetResult(retrPtr->interp, "selection owner didn't respond",
		TCL_STATIC);
	retrPtr->result = TCL_ERROR;
        retrPtr->timeout = NULL;

    } else {
	retrPtr->timeout = Tcl_CreateTimerHandler(1000, TkDND_SelTimeoutProc,
		(ClientData) retrPtr);
    }
}

int TkDND_GetSelection(Tcl_Interp *interp, Tk_Window tkwin, Atom selection,
                       Atom target, Time time,
                       Tk_GetSelProc *proc, ClientData clientData) {
  /*
   * The selection is owned by some other process.
   */
  return TkDNDSelGetSelection(interp, tkwin, selection, target, time,
                              proc, clientData);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
