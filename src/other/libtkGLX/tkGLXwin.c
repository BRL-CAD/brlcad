/* 
 * tkGLXwin.c --
 *
 *	This module implements "GLXwin" widgets for the Tk
 *	toolkit.  GLXwins are windows with a background color
 *	and possibly a 3-D effect, but no other attributes.
 *
 * 
 * GLXwin copyright:
 * Copyright 1993, MIT Media Laboratory.
 * Author: Michael Halle  (mhalle@media.mit.edu)
 * Version: 1.3
 * Release date: 10/12/93
 *
 * 1.2:   added refs
 * 1.3:   added overlay et. al. support, glx_viewport and glx_getgdesc commands
 * 1.3.1: fixed bug where core dump results if WM_COLORMAP_WINDOWS does not
 *        exist and storage for it is freed.
 * 1.3.2: fixed spurious free of glxconfig.
 *
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The Massachusetts Institute 
 * of Technology makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without  express or implied warranty.
 *
 * Tk Copyright:
 * Copyright 1990 Regents of the University of California.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "tk.h"
#include <X11/Xutil.h>

#include <gl/gl.h>
#include <gl/glws.h>
#include "tkGLX.h"

#define DEF_GLXWIN_REF ""

static Tcl_HashTable RefTable;
static int initted = 0;
static int NumRefs = 0;


/*
 * A data structure of the following type is kept for each
 * GLXwin that currently exists for this process: */

typedef struct {
  int buffer;                   /* buffer type (GLX_NORMAL, ... ) */

  int exists;                   /* does this buffer exist ?
				 * if not, the rest of the structure
				 * is undefined. */

  int rgbmode;                  /* rgb mode or not? (else cmap mode) */
  int doublebuffer;             /* doublebuffer or not ? (else single) */
  int bufsize;                  /* bits in buffer */

  int stencilsize;              /* sizes of various buffers */
  int accumsize;
  int zbuffersize;

  Window window;                /* (sub-)window of this buffer */
  Colormap colormap;
  XVisualInfo *visual_info;
} GLXBufferInfo;
  

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the GLXwin.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with
				 * widget.  Used to delete widget
				 * command.  */


    int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
    int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    int flags;			/* Various flags;  see below for
				 * definitions. */


    GLXBufferInfo normal_info;
    GLXBufferInfo overlay_info;
    GLXBufferInfo underlay_info;
    GLXBufferInfo popup_info;

#define NORMAL_IDX   0
#define OVERLAY_IDX  1
#define UNDERLAY_IDX 2
#define POPUP_IDX    3
#define BUF_COUNT    4

    GLXBufferInfo *info_table[BUF_COUNT];
   
    GLXconfig *glxconfig;    /* current configuration (or NULL if not set) */
    ClientData clientData;
    Tk_Uid ref;
    int current_winset;
} GLXwin;

#define GLX_INST_CMAPS 1    /* use the code to install and uninstall 
			       colormaps by modifying the 
			       WM_COLORMAP_WINDOWS property on the
			       toplevel window */


#define DEF_GLXWIN_CURSOR     ((char *) NULL)
#define DEF_GLXWIN_WIDTH        "0"
#define DEF_GLXWIN_HEIGHT       "0"
#define DEF_GLXWIN_RGBMODE      "0"
#define DEF_GLXWIN_DOUBLEBUFFER "0"
/*  #define DEF_GLXWIN_  */
#define DEF_GLXWIN_STENCILSIZE  "-1"
#define DEF_GLXWIN_ACCUMSIZE    "-1"
#define DEF_GLXWIN_ZBUFFERSIZE  "-1"


/*
 * Flag bits for GLXwins:
 *
 * 
 *
 */

#define GLXWIN_BOUND 1
#define GLXWIN_CMAPINSTALLED 2

static Tk_ConfigSpec configSpecs[] = {
  
  {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
     DEF_GLXWIN_CURSOR, Tk_Offset(GLXwin, cursor), TK_CONFIG_NULL_OK},
  
  {TK_CONFIG_PIXELS, "-height", "height", "Height",
     DEF_GLXWIN_HEIGHT, Tk_Offset(GLXwin, height), 0},
  
  {TK_CONFIG_PIXELS, "-width", "width", "Width",
     DEF_GLXWIN_WIDTH, Tk_Offset(GLXwin, width), 0},
  
  {TK_CONFIG_BOOLEAN, "-rgbmode", "rgbMode", "RgbMode",
     DEF_GLXWIN_RGBMODE, Tk_Offset(GLXwin, normal_info.rgbmode), 0},
  {TK_CONFIG_SYNONYM, "-rgb", "rgbMode", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_BOOLEAN, "-doublebuffer", "doubleBuffer", "DoubleBuffer",
     DEF_GLXWIN_DOUBLEBUFFER, 
     Tk_Offset(GLXwin, normal_info.doublebuffer), 0},
  
  {TK_CONFIG_SYNONYM, "-db", "doubleBuffer", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-stencilsize", "stencilSize", "StencilSize",
     DEF_GLXWIN_STENCILSIZE, 
     Tk_Offset(GLXwin, normal_info.stencilsize), 0},
  
  {TK_CONFIG_SYNONYM, "-stensize", "stencilSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-accumsize", "accumSize", "AccumSize",
     DEF_GLXWIN_ACCUMSIZE, Tk_Offset(GLXwin, normal_info.accumsize), 0},
  {TK_CONFIG_SYNONYM, "-acsize", "accumSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-zbuffersize", "zbufferSize", "ZbufferSize",
     DEF_GLXWIN_ACCUMSIZE, Tk_Offset(GLXwin, normal_info.zbuffersize), 0},
  {TK_CONFIG_SYNONYM, "-zsize", "zbufferSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-buffersize", "bufferSize", "BufferSize",
     DEF_GLXWIN_STENCILSIZE, Tk_Offset(GLXwin, normal_info.bufsize),  0},
  {TK_CONFIG_SYNONYM, "-bufsize", "bufferSize", (char *) NULL,
     (char *) NULL, 0, 0},

  {TK_CONFIG_UID, "-ref", "ref", "Ref",
     DEF_GLXWIN_REF, Tk_Offset(GLXwin, ref), TK_CONFIG_NULL_OK},
  
  {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, 0}
};

static Tk_ConfigSpec bufConfigSpecs[] = {
  
  {TK_CONFIG_BOOLEAN, "-rgbmode", "rgbMode", "RgbMode",
     DEF_GLXWIN_RGBMODE, Tk_Offset(GLXBufferInfo, rgbmode), 0},
  {TK_CONFIG_SYNONYM, "-rgb", "rgbMode", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_BOOLEAN, "-doublebuffer", "doubleBuffer", "DoubleBuffer",
     DEF_GLXWIN_DOUBLEBUFFER, 
     Tk_Offset(GLXBufferInfo, doublebuffer), 0},
  {TK_CONFIG_SYNONYM, "-db", "doubleBuffer", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-buffersize", "bufferSize", "BufferSize",
     DEF_GLXWIN_STENCILSIZE, Tk_Offset(GLXBufferInfo, bufsize), 0},
  {TK_CONFIG_SYNONYM, "-bufsize", "bufferSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-stencilsize", "stencilSize", "StencilSize",
     DEF_GLXWIN_STENCILSIZE, Tk_Offset(GLXBufferInfo, stencilsize), 0},
  {TK_CONFIG_SYNONYM, "-stensize", "stencilSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-accumsize", "accumSize", "AccumSize",
     DEF_GLXWIN_ACCUMSIZE, Tk_Offset(GLXBufferInfo, accumsize), 0},
  {TK_CONFIG_SYNONYM, "-acsize", "accumSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_INT, "-zbuffersize", "zbufferSize", "ZbufferSize",
     DEF_GLXWIN_ACCUMSIZE, Tk_Offset(GLXBufferInfo, zbuffersize), 0},
  {TK_CONFIG_SYNONYM, "-zsize", "zbufferSize", (char *) NULL,
     (char *) NULL, 0, 0},
  
  {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigureGLXwin _ANSI_ARGS_((Tcl_Interp *interp,
		    GLXwin *GLXwinPtr, int argc, char **argv, int flags));
static void	DestroyGLXwin _ANSI_ARGS_((ClientData clientData));
static void	GLXwinEventProc _ANSI_ARGS_((ClientData clientData,
		    XEvent *eventPtr));
static int	GLXwinWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

static int	GLXwinCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

static unsigned long ExtractValue _ANSI_ARGS_((int buffer, int mode, 
					       GLXconfig *conf));

static int      BufnameToIdx _ANSI_ARGS_((char *name));
static int SetWindow      _ANSI_ARGS_((int buffer, Window W, GLXconfig *conf));
static int LinkGLXwin     _ANSI_ARGS_((GLXwin *gp));
static int UnlinkGLXwin   _ANSI_ARGS_((GLXwin *gp));
static int GetGLXwinInfo  _ANSI_ARGS_((GLXwin *gp));
static int FillGLXconfig  _ANSI_ARGS_((GLXwin *gp));
static int GLXwinWinset   _ANSI_ARGS_((GLXwin *gp, int idx));
static int ConfigGLXconfig _ANSI_ARGS_((Tcl_Interp *interp, GLXwin *gp));

static void MakeChildren   _ANSI_ARGS_((GLXwin *gp));
static void ResizeChildren _ANSI_ARGS_((GLXwin *gp));

static int	GLXwinAddRef    _ANSI_ARGS_((Tk_Uid uid, GLXwin *win));
static int	GLXwinDeleteRef _ANSI_ARGS_((Tk_Uid uid));
static int	GLXwinDeleteRef _ANSI_ARGS_((Tk_Uid uid));
static int      GLXwinRefGetGLXwin _ANSI_ARGS_((char *ref, GLXwin **x));
static Tk_Window      GetTopLevelWindow  _ANSI_ARGS_((GLXwin *x));
static int      InstallColormaps _ANSI_ARGS_((GLXwin *x));
static int      UninstallColormaps _ANSI_ARGS_((GLXwin *x));


int TkGLXwin_RefWinset(ref, buf)
     char *ref;
     int buf;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(1);

  switch(buf){
  case GLX_NORMAL: idx = NORMAL_IDX; break;
  case GLX_POPUP: idx = POPUP_IDX; break;
  case GLX_UNDERLAY: idx = UNDERLAY_IDX; break;
  case GLX_OVERLAY: idx = OVERLAY_IDX; break;
  default: return(1);
  }
   
  return(GLXwinWinset(u, idx));
}

Window TkGLXwin_RefGetWindow(ref, buf)
     char *ref;
     int buf;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((Window)None);

  switch(buf){
  case GLX_NORMAL:   idx = NORMAL_IDX;   break;
  case GLX_POPUP:    idx = POPUP_IDX;    break;
  case GLX_UNDERLAY: idx = UNDERLAY_IDX; break;
  case GLX_OVERLAY:  idx = OVERLAY_IDX;  break;
  default: return((Window)None);
  }
   
  return(u->info_table[idx]->window);
}

int TkGLXwin_RefWindowExists(ref, buf)
     char *ref;
     int buf;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(0);

  switch(buf){
  case GLX_NORMAL:   idx = NORMAL_IDX;   break;
  case GLX_POPUP:    idx = POPUP_IDX;    break;
  case GLX_UNDERLAY: idx = UNDERLAY_IDX; break;
  case GLX_OVERLAY:  idx = OVERLAY_IDX;  break;
  default: return(0);
  }
   
  return(u->info_table[idx]->exists);
}

int TkGLXwin_RefIsLinked(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(0);

  return(u->flags & GLXWIN_BOUND);
}

Colormap TkGLXwin_RefGetColormap(ref, buf)
     char *ref;
     int buf;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((Colormap)None);

  switch(buf){
  case GLX_NORMAL:   idx = NORMAL_IDX;   break;
  case GLX_POPUP:    idx = POPUP_IDX;    break;
  case GLX_UNDERLAY: idx = UNDERLAY_IDX; break;
  case GLX_OVERLAY:  idx = OVERLAY_IDX;  break;
  default: return((Colormap)None);
  }
   
  return(u->info_table[idx]->colormap);
}

XVisualInfo *TkGLXwin_RefGetVisualInfo(ref, buf)
     char *ref;
     int buf;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((XVisualInfo *)NULL);

  switch(buf){
  case GLX_NORMAL:   idx = NORMAL_IDX;   break;
  case GLX_POPUP:    idx = POPUP_IDX;    break;
  case GLX_UNDERLAY: idx = UNDERLAY_IDX; break;
  case GLX_OVERLAY:  idx = OVERLAY_IDX;  break;
  default: return((XVisualInfo *)NULL);
  }
   
  return(u->info_table[idx]->visual_info);
}

unsigned long TkGLXwin_RefGetParam(ref, buf, param)
     char *ref;
     int buf, param;
{
  GLXwin *u;
  int idx;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(GLWS_NOWINDOW);

  return(ExtractValue(buf, param, u->glxconfig));
}

Tk_Window TkGLXwin_RefGetTkwin(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((Tk_Window)NULL);

  return(u->tkwin);
}

Tcl_Interp *TkGLXwin_RefGetInterp(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((Tcl_Interp *)NULL);

  return(u->interp);
}

int TkGLXwin_GetRefs(refs, count)
     char ***refs;
     int *count;
{
  Tcl_HashSearch search;
  Tcl_HashEntry *ent;
  char **rp;
  GLXwin *u;
  int tmp;

  if(count == NULL)
    count = &tmp;

  if(!initted){
    *refs = (char **)ckalloc(sizeof(char *));
    (*refs)[0] = NULL;
    *count = 0;
    return(*count);
  }
  
  *refs = (char **)ckalloc((NumRefs + 1) * sizeof(char *));
  *count = NumRefs;

  for(ent = Tcl_FirstHashEntry(&RefTable, &search), rp = *refs; 
      ent != NULL;
      ent = Tcl_NextHashEntry(&search), rp++){
    u = (GLXwin *)Tcl_GetHashValue(ent);
    *rp = (char *)u->ref;
  }
  *rp = NULL;

  return(*count);
}

int TkGLXwin_RefGetDisplayWindow(ref, d, w)
     char *ref;
     Display **d;
     Window *w;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(1);

  if(d != (Display **)NULL)
    *d = u->display;

  if(u->tkwin == NULL)
    return(2);

  if(w != (Window *)NULL)
    *w = Tk_WindowId(u->tkwin);

  return(0);
}

Display *TkGLXwin_RefGetDisplay(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((Display *)NULL);

  return(u->display);
}

int TkGLXwin_RefExists(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return(0);
  
  return(1);
}  

GLXconfig *TkGLXwin_RefGetConfig(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((GLXconfig *)NULL);

  return(u->glxconfig);
}

void *TkGLXwin_RefGetClientData(ref)
     char *ref;
{
  GLXwin *u;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((ClientData)NULL);

  return((void *)u->clientData);
}

void *TkGLXwin_RefSetClientData(ref, clientData)
     char *ref;
     void *clientData;
{
  GLXwin *u;
  ClientData tmp;

  if(GLXwinRefGetGLXwin(ref, &u) || u == NULL)
    return((ClientData) NULL);

  tmp = u->clientData;

  u->clientData = (ClientData)clientData;
  return((void *)tmp);
}

static int GLXwinAddRef(uid, win)
     Tk_Uid uid;
     GLXwin *win;
{
  int isnew;
  Tcl_HashEntry *ent;

  if(!initted){
    Tcl_InitHashTable(&RefTable, TCL_ONE_WORD_KEYS);
    initted = 1;
  }

  ent = Tcl_CreateHashEntry(&RefTable, uid, &isnew);
  if(!isnew)
    return(win == (GLXwin *)Tcl_GetHashValue(ent) ? 0 : 1);

  NumRefs++;
  Tcl_SetHashValue(ent, (ClientData)win);
  return(0);
}

static int GLXwinDeleteRef(uid)
     Tk_Uid uid;
{
  Tcl_HashEntry *ent;

  if(!initted)
    return(1);

  ent = Tcl_FindHashEntry(&RefTable, uid);
  if(ent == NULL)
    return(1);

  NumRefs--;
  Tcl_DeleteHashEntry(ent);
  return(0);
}

static int GLXwinRefGetGLXwin(ref, x)
     char *ref;
     GLXwin **x;
{
  Tcl_HashEntry *ent;

  if(!initted)
    return(1);

  ent = Tcl_FindHashEntry(&RefTable, Tk_GetUid(ref));
  if(ent == NULL)
    return(1);

  *x = (GLXwin *)Tcl_GetHashValue(ent);
  return(0);
}

TkGLX_WidgetInit(interp, w)
     Tcl_Interp *interp;
     Tk_Window w;
{
  Tcl_CreateCommand(interp, "glxwin", GLXwinCmd,
		    (ClientData)w, (void (*)()) NULL);
}
/*
 *--------------------------------------------------------------
 *
 * GLXwinCmd --
 *
 *	This procedure is invoked to process the "GLXwin" and
 *	"toplevel" Tcl commands.  See the user documentation for
 *	details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
static int
GLXwinCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    Tk_Window new;
    register GLXwin *GLXwinPtr;
    int i;
    GLXBufferInfo *info[BUF_COUNT];

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the window.
     */

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    Tk_SetClass(new, "Glxwin");
    GLXwinPtr = (GLXwin *) ckalloc(sizeof(GLXwin));
    GLXwinPtr->tkwin = new;
    GLXwinPtr->display = Tk_Display(new);
    GLXwinPtr->interp = interp;
    GLXwinPtr->cursor = None;
    GLXwinPtr->flags = 0;

    GLXwinPtr->glxconfig = NULL;

    info[0] = &GLXwinPtr->normal_info;
    info[1] = &GLXwinPtr->overlay_info;
    info[2] = &GLXwinPtr->underlay_info;
    info[3] = &GLXwinPtr->popup_info;
    
    info[0]->buffer = GLX_NORMAL;
    info[1]->buffer = GLX_OVERLAY;
    info[2]->buffer = GLX_UNDERLAY;
    info[3]->buffer = GLX_POPUP;
    
    for(i = 0; i < BUF_COUNT; i++){
      GLXwinPtr->info_table[i] = info[i];
      info[i]->exists = 0;
      info[i]->doublebuffer = 0;
      info[i]->rgbmode = 0;

      info[i]->bufsize     = -1;
      info[i]->stencilsize = 0;
      info[i]->accumsize   = 0;
      info[i]->zbuffersize = 0;
      info[i]->window      = (Window)        None;
      info[i]->colormap    = (Colormap)      None;
      info[i]->visual_info = (XVisualInfo *) NULL;
    }

    GLXwinPtr->normal_info.exists = 1;
    GLXwinPtr->current_winset = NORMAL_IDX;

    Tk_CreateEventHandler(GLXwinPtr->tkwin, StructureNotifyMask,
	    GLXwinEventProc, (ClientData) GLXwinPtr);

    Tcl_CreateCommand(interp, Tk_PathName(GLXwinPtr->tkwin),
	    GLXwinWidgetCmd, (ClientData) GLXwinPtr, (void (*)()) NULL);

    if (ConfigureGLXwin(interp, GLXwinPtr, argc-2, argv+2, 0) != TCL_OK) {
	Tk_DestroyWindow(GLXwinPtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(GLXwinPtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GLXwinWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a GLXwin widget.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
GLXwinWidgetCmd(clientData, interp, argc, argv)
     ClientData clientData;	/* Information about GLXwin widget. */
     Tcl_Interp *interp;		/* Current interpreter. */
     int argc;			/* Number of arguments. */
     char **argv;		/* Argument strings. */
{
  register GLXwin *GLXwinPtr = (GLXwin *) clientData;
  int result = TCL_OK;
  int length, bufidx;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], " option ?arg arg ...?\"", (char *) NULL);
    return TCL_ERROR;
  }
  Tk_Preserve((ClientData) GLXwinPtr);
  length = strlen(argv[1]);

  if (strncmp(argv[1], "configure", length) == 0) {
    if (argc == 2) {
      result = Tk_ConfigureInfo(interp, GLXwinPtr->tkwin, configSpecs,
				(char *) GLXwinPtr, (char *) NULL, 0);
    } 
    else if (argc == 3) {
      result = Tk_ConfigureInfo(interp, GLXwinPtr->tkwin, configSpecs,
				(char *) GLXwinPtr, argv[2], 0);
    } 
    else {
      result = ConfigureGLXwin(interp, GLXwinPtr, argc-2, argv+2,
			       TK_CONFIG_ARGV_ONLY);
    }
  } 

  else if (strncmp(argv[1], "winset", length) == 0) {
    GLXwinWinset(GLXwinPtr, NORMAL_IDX);
  } 
  
  else if (strncmp(argv[1], "islinked", length) == 0) {
    sprintf(interp->result, "%d", (GLXwinPtr->flags & GLXWIN_BOUND) ? 1 : 0);
  }

  else if(strncmp(argv[1], "installcolormaps", length) == 0){
    InstallColormaps(GLXwinPtr);
  }

  else if(strncmp(argv[1], "uninstallcolormaps", length) == 0){
    UninstallColormaps(GLXwinPtr);
  }
  
  else if (strncmp(argv[1], "ref", length) == 0) {
    if (argc == 2) {
      if(GLXwinPtr->ref != NULL){
	interp->result = (char *)GLXwinPtr->ref;
      }
    }
    else if (argc == 3) {
      if(GLXwinPtr->ref != NULL)
	(void)GLXwinDeleteRef(GLXwinPtr->ref);
      if(strcmp(argv[2], "") == 0){
	GLXwinPtr->ref = NULL;
	interp->result = "";
      }
      else{
	GLXwinPtr->ref = Tk_GetUid(argv[2]);
	GLXwinAddRef(GLXwinPtr->ref, GLXwinPtr);
	interp->result = (char *)GLXwinPtr->ref;
      }
    }

    else {
      Tcl_AppendResult(interp, "ref: wrong number of args", (char *) NULL);
      result = TCL_ERROR;
    }
  }
  
  else if ((bufidx = BufnameToIdx(argv[1])) != -1){
    /* one of overlay, underlay, normal, popup */
    result = handle_buffer_cmd(interp, bufidx, GLXwinPtr, argc, argv);
  }

  else {
    Tcl_AppendResult(interp, "bad option \"", argv[1],
		     "\":  must be configure, winset, islinked, installcolormaps, uninstallcolormaps, or ref", (char *) NULL);
    result = TCL_ERROR;
  }
  
  Tk_Release((ClientData) GLXwinPtr);
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyGLXwin --
 *
 *	This procedure is invoked by Tk_EventuallyFree or Tk_Release
 *	to clean up the internal structure of a GLXwin at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the GLXwin is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyGLXwin(clientData)
    ClientData clientData;	/* Info about GLXwin widget. */
{
  int i;
  GLXwin *GLXwinPtr = (GLXwin *) clientData;
  
  if (GLXwinPtr->cursor != None) {
    Tk_FreeCursor(GLXwinPtr->display, GLXwinPtr->cursor);
  }
  
  if (GLXwinPtr->glxconfig != NULL) {
    ckfree(GLXwinPtr->glxconfig);
  }
  
  for(i = 0; i < BUF_COUNT; i++){
    if(GLXwinPtr->info_table[i]->visual_info != (XVisualInfo *)NULL)
      XFree((char *)GLXwinPtr->info_table[i]->visual_info);
  }
  
  ckfree((char *) GLXwinPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureGLXwin --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a GLXwin widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for GLXwinPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureGLXwin(interp, GLXwinPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register GLXwin *GLXwinPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{

  if(GLXwinPtr->ref != NULL)
    (void)GLXwinDeleteRef(GLXwinPtr->ref);
  
  
  if (Tk_ConfigureWidget(interp, GLXwinPtr->tkwin, configSpecs,
			 argc, argv, (char *) GLXwinPtr, flags) != TCL_OK) {
    return TCL_ERROR;
  }

  if(GLXwinPtr->ref != NULL && 
     GLXwinAddRef(GLXwinPtr->ref, GLXwinPtr) != 0){
    Tcl_AppendResult(interp, "ref \"", GLXwinPtr->ref,
		     "\" already in use", (char *) NULL);
    return TCL_ERROR;
  }
  
  if ((GLXwinPtr->width > 0) || (GLXwinPtr->height > 0)) {
    Tk_GeometryRequest(GLXwinPtr->tkwin, GLXwinPtr->width,
		       GLXwinPtr->height);
  }

  return(ConfigGLXconfig(interp, GLXwinPtr));
}

/*
 *--------------------------------------------------------------
 *
 * GLXwinEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher on
 *	structure changes to a GLXwin.  For GLXwins with 3D
 *	borders, this procedure is also invoked for exposures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
GLXwinEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    register XEvent *eventPtr;	/* Information about event. */
{
  register GLXwin *GLXwinPtr = (GLXwin *) clientData;
  
  if (eventPtr->type == DestroyNotify) {
    Tcl_DeleteCommand(GLXwinPtr->interp, Tk_PathName(GLXwinPtr->tkwin));
    (void)UninstallColormaps(GLXwinPtr);
    (void)UnlinkGLXwin(GLXwinPtr);

    if(GLXwinPtr->ref != NULL)
      GLXwinDeleteRef(GLXwinPtr->ref);
    GLXwinPtr->ref = NULL;
    
    GLXwinPtr->tkwin = NULL;
    
    Tk_EventuallyFree((ClientData) GLXwinPtr, DestroyGLXwin);
     }
  else if (eventPtr->type == MapNotify) {
    (void)LinkGLXwin(GLXwinPtr);
    (void)InstallColormaps(GLXwinPtr);
  }
  else if (eventPtr->type == ConfigureNotify) {
    ResizeChildren(GLXwinPtr);
  }
}

static int FillGLXconfig(gp)
     GLXwin *gp;
{
  
  GLXconfig *confp, inconf[32];
  GLXBufferInfo *info[BUF_COUNT];
  int i, buf;

  for(i = 0; i < BUF_COUNT; i++)
    info[i] = gp->info_table[i];
  
  confp = inconf;
  
  for(i = 0; i < BUF_COUNT; i++){
    buf = info[i]->buffer;
    
    if(! info[i]->exists) 
      continue;
    
    confp->buffer = buf;
    confp->mode = GLX_BUFSIZE;
    confp->arg  = 
      info[i]->bufsize == -1 ? GLX_NOCONFIG : info[i]->bufsize;
    confp++;
    
    confp->buffer = buf;
    confp->mode = GLX_RGB;
    confp->arg  = info[i]->rgbmode ? True : False;
    confp++;
    
    confp->buffer = buf;
    confp->mode = GLX_DOUBLE;
    confp->arg  = info[i]->doublebuffer ? True : False;
    confp++;

    
    confp->buffer = buf;
    confp->mode   = GLX_ZSIZE;
    confp->arg  = 
      info[i]->zbuffersize == -1 ? GLX_NOCONFIG : info[i]->zbuffersize;
    confp++;
    
    confp->buffer = buf;
    confp->mode   = GLX_ACSIZE;
    confp->arg  = 
      info[i]->accumsize == -1 ? GLX_NOCONFIG : info[i]->accumsize;
    confp++;
    
    confp->buffer = buf;
    confp->mode   = GLX_STENSIZE;
    confp->arg  = 
      info[i]->stencilsize == -1 ? GLX_NOCONFIG : info[i]->stencilsize;
    confp++;
  }

  /* terminate buffer list */
  confp->buffer = 0;
  confp->mode =   0;
  confp->arg  =   0;

  /* set/get configuration */
  gp->glxconfig =
    GLXgetconfig(gp->display, Tk_ScreenNumber(gp->tkwin), inconf);

  if(gp->glxconfig == NULL)
    return(1);

  /* return actual values to structure */
  for(i = 0; i < BUF_COUNT; i++){
    if(!info[i]->exists) continue;
    info[i]->bufsize = 
      ExtractValue(info[i]->buffer, GLX_BUFSIZE, gp->glxconfig);
    
    info[i]->stencilsize = 
      ExtractValue(info[i]->buffer, GLX_STENSIZE, gp->glxconfig);

    info[i]->zbuffersize = 
      ExtractValue(info[i]->buffer, GLX_ZSIZE, gp->glxconfig);

    info[i]->accumsize = 
      ExtractValue(info[i]->buffer, GLX_ACSIZE, gp->glxconfig);
  }
  return(0);
}

static int LinkGLXwin(gp)
     GLXwin *gp;
{
  int i;

  if(!(gp->flags & GLXWIN_BOUND)){
    Tk_MakeWindowExist(gp->tkwin);
    gp->normal_info.window = Tk_WindowId(gp->tkwin);
    MakeChildren(gp);

    for(i = 0; i < BUF_COUNT; i++){
      if(gp->info_table[i]->exists)
	SetWindow(gp->info_table[i]->buffer, 
		   gp->info_table[i]->window, gp->glxconfig);
    }

    if(GLXlink(gp->display, gp->glxconfig) != GLWS_NOERROR)
      return(1);
    
    gp->flags |= GLXWIN_BOUND;
    GLXwinWinset(gp, gp->current_winset);
  }
  return(0);
}

static int UnlinkGLXwin(gp)
     GLXwin *gp;
{
  if(gp == NULL)
    return(0);

  if(!(gp->flags & GLXWIN_BOUND)){
    return(0); /* not linked */
  }

  if(gp->tkwin != NULL)
    if(GLXunlink(gp->display, Tk_WindowId(gp->tkwin)) != GLWS_NOERROR)
      return(1);
  
  gp->flags &= ~GLXWIN_BOUND;
  return(0);
}


/* must be called before the window exists */
static int GetGLXwinInfo(gp)
     GLXwin *gp;
{
  XVisualInfo templ;
  int n, i;

  templ.screen = Tk_ScreenNumber(gp->tkwin);

  for(i = 0; i < BUF_COUNT; i++){
    if(! gp->info_table[i]->exists) continue;
    templ.visualid = 
      ExtractValue(gp->info_table[i]->buffer, GLX_VISUAL, gp->glxconfig);

    if(gp->info_table[i]->visual_info != (XVisualInfo *)NULL)
      XFree((char *)gp->info_table[i]->visual_info);
    
    gp->info_table[i]->visual_info = NULL;
    
    gp->info_table[i]->visual_info = 
      XGetVisualInfo(gp->display, VisualScreenMask|VisualIDMask, &templ, &n);
    
    gp->info_table[i]->colormap = 
      ExtractValue(gp->info_table[i]->buffer, GLX_COLORMAP, gp->glxconfig);
  }
  Tk_SetWindowVisual(gp->tkwin, 
		     gp->normal_info.visual_info->visual, 
		     gp->normal_info.visual_info->depth, 
		     gp->normal_info.colormap);
  return(0);
}

static unsigned long
ExtractValue(buffer, mode, conf)
     int buffer;
     int mode;
     GLXconfig *conf;
{
  int	i;
  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == mode)
      return conf[i].arg;
  return 0;
}

static int SetWindow(buffer, W, conf)
     int buffer;
     Window W;
     GLXconfig *conf;

{
  int	i;
  
  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == GLX_WINDOW)
      conf[i].arg = W;
  return(0);
}

static int GLXwinWinset(gp, idx)
     GLXwin *gp;
     int idx;
{
  gp->current_winset = idx;

  if(!(gp->flags & GLXWIN_BOUND))
    return(0);

  if(! gp->info_table[idx]->exists) 
    return(1);

  if(GLXwinset(gp->display, gp->info_table[idx]->window) != GLWS_NOERROR)
    return(2);

  return(0);
}

static int BufnameToIdx(name)
     char *name;
{
  if(strcmp(name, "normal"  ) == 0) return (NORMAL_IDX   );
  if(strcmp(name, "overlay" ) == 0) return (OVERLAY_IDX  );
  if(strcmp(name, "underlay") == 0) return (UNDERLAY_IDX );
  if(strcmp(name, "popup"   ) == 0) return (POPUP_IDX    );
  return(-1);
}

static void MakeChildren(gp)
     GLXwin *gp;
{
  int i;
  Window parent;
  XSetWindowAttributes attrs;

  parent = gp->normal_info.window;

  attrs.border_pixel = 0;
  for(i = 1; i < BUF_COUNT; i++){
    if(! gp->info_table[i]->exists) 
      continue;

    if((gp->info_table[i]->visual_info == (XVisualInfo *)NULL) ||
       (gp->info_table[i]->colormap == (Colormap)None)){
      gp->info_table[i]->exists = 0;
      continue;
    }

    attrs.colormap = gp->info_table[i]->colormap;
    gp->info_table[i]->window = XCreateWindow(gp->display, parent,
				    0, 0, 
				    Tk_Width(gp->tkwin),
				    Tk_Height(gp->tkwin),
				    0, 
				    gp->info_table[i]->visual_info->depth,
				    InputOutput,
				    gp->info_table[i]->visual_info->visual,
				    CWColormap|CWBorderPixel, &attrs);
    /* make this window eligible to be mapped */
    XMapWindow(gp->display, gp->info_table[i]->window);
  }
}

static void ResizeChildren(gp)
     GLXwin *gp;
{
  int i;

  for(i = 1; i < BUF_COUNT; i++){
    if(gp->info_table[i]->exists && 
       gp->info_table[i]->window != (Window)None) {
      XResizeWindow(gp->display, gp->info_table[i]->window, 
		    Tk_Width(gp->tkwin), Tk_Height(gp->tkwin)); 
    }
  }
  /* viewport may fail on Indigo if this isn't here */
  XSync(gp->display, FALSE); 
}

static int ConfigGLXconfig(interp, gp)
     Tcl_Interp *interp;
     GLXwin *gp;
{
  /* don't even try to reconfigure if window is already linked */
  if(! gp->flags & GLXWIN_BOUND) {
    if (gp->glxconfig != NULL) {
      ckfree(gp->glxconfig);
      gp->glxconfig = (GLXconfig *)NULL;
    }

    if(FillGLXconfig(gp) != 0){
      Tcl_AppendResult(interp, "GLXgetconfig failed", (char *) NULL);
      return TCL_ERROR;
    }

    if(gp->glxconfig == (GLXconfig *)NULL) {
      Tcl_AppendResult(interp, "configuration isn't filled, must be invalid", 
		       (char *) NULL);
      return TCL_ERROR;
    }
      
    GetGLXwinInfo(gp);
  }

  return(TCL_OK);
}

handle_buffer_cmd(interp, bufidx, gp, argc, argv)
     Tcl_Interp *interp;
     int bufidx;
     GLXwin *gp;
     int argc;
     char **argv;
{
  int length;
  GLXBufferInfo *info;
  int exists, result;

  info = gp->info_table[bufidx];
  exists = info->exists;
  
  if(argc < 3){
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		       argv[0], " ", argv[1], " ", "args ...\"", 
		     (char *) NULL);
    return(TCL_ERROR);
  }
  

  length = strlen(argv[2]);

  if(strncmp(argv[2], "exists", length) == 0){
    interp->result = exists ? "1" : "0";
    return(TCL_OK);
  }

  else if(strncmp(argv[2], "id", length) == 0){
    if(!exists){
      Tcl_AppendResult(interp, 
		       "buffer named \"",
		       argv[1], 
		       "\" does not exist in window \"",
		       argv[0], "\"", (char *)0);
      return(TCL_ERROR);
    }
    
    sprintf(interp->result, 
	    info->window == (Window)None ? "" : "0x%x",
	    (unsigned int)info->window);

    return(TCL_OK);
  }

  else if(strncmp(argv[2], "create", length) == 0){
    result = Tk_ConfigureWidget(interp, gp->tkwin, bufConfigSpecs,
				argc - 3, argv + 3,
				(char *) gp->info_table[bufidx],
				TK_CONFIG_ARGV_ONLY);
    if(result == TCL_OK)
      info->exists = 1;

    result = ConfigGLXconfig(interp, gp);
    return(result);
  }

  else if(strncmp(argv[2], "configure", length) == 0){
    if(!exists){
      Tcl_AppendResult(interp, 
		       "buffer named \"",
		       argv[1], 
		       "\" does not exist in window \"",
		       argv[0], "\"", (char *)0);
      return(TCL_ERROR);
    }
    
    if(argc == 3){
      result = Tk_ConfigureInfo(interp, gp->tkwin, bufConfigSpecs,
				(char *) gp->info_table[bufidx],
				(char *) NULL, 0);
    }
    else if (argc == 4) {
      result = Tk_ConfigureInfo(interp, gp->tkwin, bufConfigSpecs,
				(char *) gp->info_table[bufidx],
				argv[3], 0);
    } 
    else {
      result = Tk_ConfigureWidget(interp, gp->tkwin, bufConfigSpecs,
				  argc - 3, argv + 3,
				  (char *) gp->info_table[bufidx],
				  TK_CONFIG_ARGV_ONLY);
      if(result == TCL_OK)
	result = ConfigGLXconfig(interp, gp);
    }
    return(result);
  }

  else if(strncmp(argv[2], "winset", length) == 0){
    if(!exists){
      Tcl_AppendResult(interp, 
		       "buffer named \"",
		       argv[1], 
		       "\" does not exist in window \"",
		       argv[0], "\"", (char *)0);
      return(TCL_ERROR);
    }
    GLXwinWinset(gp, bufidx);
    return(TCL_OK);
  }

  else {
      Tcl_AppendResult(interp, 
		       "usage: \"",
		       argv[0], " ", argv[1], " ",
		       "exists, winset, create, configure, id\"",
		       (char *)0);
      return(TCL_ERROR);
    }
}

static Tk_Window GetTopLevelWindow(gp)
     GLXwin *gp;
{
  Tk_Window win;

  for (win = gp->tkwin; win != (Tk_Window)NULL && !Tk_IsTopLevel(win);
       win = Tk_Parent(win)){
    /* Empty loop body. */
  }
  
  return(win);
}

static int InstallColormaps(gp)
     GLXwin *gp;
{
#ifdef GLX_INST_CMAPS

  Window *cmapwins, *newwins,  *np, tl;
  int mycnt, cnt, i, j, f, match, tot;
  Status got_prop;


/* this code removed so that colormap can be installed *for sure* when
   needed */

#if 0
  if(gp->flags & GLXWIN_CMAPINSTALLED)
    return(1);
#endif


  if(gp->tkwin == NULL) 
    return(0);

  mycnt = 0;
  for(i = 0; i < 4; i++)
    if(gp->info_table[i]->exists)
      mycnt++;
    
  tl = Tk_WindowId(GetTopLevelWindow(gp));

  if(tl == (Window)None)
    return(0);

  got_prop = XGetWMColormapWindows(gp->display, tl,
				   &cmapwins, &cnt);
  if(!got_prop) cnt = 0;

  newwins = (Window *)ckalloc((cnt + mycnt + 1) * sizeof(Window));

  np = newwins;

  tot = 0;
  for(i = 0; i < 4; i++)
    if(gp->info_table[i]->exists){
      *np++ = gp->info_table[i]->window;
      tot++;
    }
  
  f = 1;
  for(i = 0; i < cnt; i++){

    /* look to see if "our" windows are already in the list.  If
       they are, remove all further occurences by not copying them.
     */
    for(j = 0, match = 0; j < 4; j++)
      if(gp->info_table[j]->exists && cmapwins[i] == gp->info_table[j]->window)
	match = 1;

    if(!match){
      *np++ = cmapwins[i];
      tot++;
    }
	
    /* look to see if the toplevel window is in the list;  if it is, don't
       add it again, but make sure it is at the end of the list */
    if(cmapwins[i] == tl)
      f = 0;
  }

  if(f){
    *np++ = tl;
    tot++;
  }

  XSetWMColormapWindows(gp->display, tl, newwins, tot);
  ckfree((char *)newwins);

  if(got_prop)
    XFree((char *)cmapwins);
      
  gp->flags |= GLXWIN_CMAPINSTALLED;
#endif /*  GLX_INST_CMAPS */
  return(1);
}

static int UninstallColormaps(gp)
     GLXwin *gp;
{
#ifdef GLX_INST_CMAPS

  Window *cmapwins, *newwins, *np, tl;
  int mycnt, cnt, i, j, match;
  Status got_prop;

  if(!(gp->flags & GLXWIN_CMAPINSTALLED))
    return(1);

  if(gp->tkwin == NULL) 
    return(0);

  mycnt = 0;
  for(i = 0; i < 4; i++)
    if(gp->info_table[i]->exists)
      mycnt++;
    
  tl = Tk_WindowId(GetTopLevelWindow(gp));

  if(tl == (Window)None)
    return(0);

  got_prop = XGetWMColormapWindows(gp->display, tl, &cmapwins, &cnt);
  if(!got_prop) {
    return(1);
  }

  newwins = (Window *)ckalloc(cnt * sizeof(Window));
  np = newwins;
  
  for(j = 0, mycnt = 0; j < cnt; j++){
    for(i = 0, match = 0; i < 4; i++){
      if((gp->info_table[i]->exists &&
	  gp->info_table[i]->window == cmapwins[j]))
	match = 1;
    }
    if(!match){
      *np++ = cmapwins[j];
      mycnt++;
    }
  }

  XSetWMColormapWindows(gp->display, tl, newwins, mycnt);
  ckfree((char *)newwins);
  if(got_prop)
    XFree((char *)cmapwins);
  
  gp->flags &= ~GLXWIN_CMAPINSTALLED;

#endif /* GLX_INST_CMAPS */
  return(1);
}
