/* $Id$ */

/* vi:set sw=4 expandtab: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

/* 
 * Currently we support X11, Win32 and Mac OS X only
 */

#include "brlcad_config.h"

#define USE_TOGL_STUB_PROCS
#include "togl.h"
#ifdef HAVE_GL_GLEXT_H
# include <GL/glext.h>
#else
# include "./glext.h"
#endif
#include <tclInt.h>
#include <tkInt.h>
#include <limits.h>

#ifndef TOGL_USE_FONTS
#  define TOGL_USE_FONTS 1
#endif
#if (TK_MAJOR_VERSION > 8 || TK_MINOR_VERSION > 4) && !defined(TOGL_WGL)
/* X11 and Aqua font technology changed in 8.5 */
#  undef TOGL_USE_FONTS
#endif
#ifndef TOGL_USE_OVERLAY
#  if defined(TOGL_X11) || defined(TOGL_WGL)
#    define TOGL_USE_OVERLAY 1
#  endif
#endif

/* Use TCL_STUPID to cast (const char *) to (char *) where the Tcl function
 * prototype argument should really be const */
#define TCL_STUPID (char *)

/* Use WIDGREC to cast widgRec or recordPtr arguments */
#define WIDGREC	(char *)

/*** Windows headers ***/
#if defined(TOGL_WGL)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
#  include <winnt.h>
#  ifndef PFD_SUPPORT_COMPOSITION
// for Vista -- not strictly needed because we don't use PFD_SUPPORT_GDI/BITMAP
#    define PFD_SUPPORT_COMPOSITION 0x00008000
#  endif
#  include <objbase.h>
#  include "GL/wglew.h"
#  ifdef _MSC_VER
#    include <strsafe.h>
#  else
#    ifdef UNICODE
#      define StringCchPrintf snwprintf
#    else
#      define StringCchPrintf snprintf
#    endif
#  endif

/*** X Window System headers ***/
#elif defined(TOGL_X11)
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>        /* for XA_RGB_DEFAULT_MAP atom */
#  if !defined(USE_SYSTEM_XMU)
#    include "Xmu/StdCmap.h"
#  else
#    if defined(__vms)
#      include <X11/StdCmap.h>  /* for XmuLookupStandardColormap */
#    else
#      include <X11/Xmu/StdCmap.h>      /* for XmuLookupStandardColormap */
#    endif
#  endif
#  include "GL/glxew.h"
#  ifdef __sgi
#    include <X11/extensions/SGIStereo.h>
#  endif
#  ifdef HAVE_AUTOSTEREO
#    include <autostereo.h>
#  endif

/*** Mac Carbon headers ***/
#elif defined(TOGL_AGL)
#  define Cursor QDCursor
#  include <AGL/agl.h>
#  undef Cursor
#  include <tkMacOSXInt.h>      /* usa MacDrawable */
#  include <ApplicationServices/ApplicationServices.h>
#  define TOGL_COCOA 1
#  if !defined(TOGL_COCOA)
#    define Togl_MacOSXGetDrawablePort(togl) TkMacOSXGetDrawablePort((Drawable) ((TkWindow *) togl->TkWin)->privatePtr)
#  endif

/*** Mac Cocoa headers ***/
#elif defined(TOGL_NSOPENGL)
#  include <OpenGL/OpenGL.h>
#  include <AppKit/NSOpenGL.h>	/* Use NSOpenGLContext */
#  include <AppKit/NSView.h>	/* Use NSView */
#  include <Foundation/Foundation.h>	/* Use NSRect */
#  include <tkMacOSXInt.h>      /* Use MacDrawable */
#  include <ApplicationServices/ApplicationServices.h>
#  define Togl_MacOSXGetDrawablePort(togl) TkMacOSXGetDrawablePort((Drawable) ((TkWindow *) togl->TkWin)->privatePtr)

#else /* make sure only one platform defined */
#  error Unsupported platform, or confused platform defines...
#endif

#define NC3D "NVidia Consumer 3D Stereo"

#ifndef STEREO_BUFFER_NONE
/* From <X11/extensions/SGIStereo.h>, but we use this constants elsewhere */
#  define STEREO_BUFFER_NONE 0
#  define STEREO_BUFFER_LEFT 1
#  define STEREO_BUFFER_RIGHT 2
#endif

/*** Standard C headers ***/
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef TOGL_WGL
#  include <tkPlatDecls.h>
#endif

#if TK_MAJOR_VERSION < 8
#  error Sorry Togl requires Tcl/Tk ver 8.0 or higher.
#endif

#ifdef USE_TCL_STUBS
#  if TK_MAJOR_VERSION < 8 || (TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 1)
#    error Sorry stub support requires Tcl/Tk ver 8.1 or higher.
#  endif
#endif

#if defined(TOGL_AGL)
#  if TK_MAJOR_VERSION < 8 || (TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 4)
#    error Sorry Mac Aqua version requires Tcl/Tk ver 8.4.0 or higher.
#  endif
#endif /* TOGL_AGL */

#if defined(TOGL_NSOPENGL)
#  if TK_MAJOR_VERSION < 8 || (TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 6)
#    error Sorry Mac Cocoa version requires Tcl/Tk ver 8.6.0 or higher.
#  endif
#endif /* TOGL_NSOPENGL */

#if defined(TOGL_WGL) && defined(_MSC_VER)
#  define snprintf _snprintf
#  pragma warning(disable:4995)
#endif

/* workaround for bug #123153 in tcl ver8.4a2 (tcl.h) */
#if defined(Tcl_InitHashTable) && defined(USE_TCL_STUBS)
#  undef Tcl_InitHashTable
#  define Tcl_InitHashTable (tclStubsPtr->tcl_InitHashTable)
#endif
#if TK_MAJOR_VERSION > 8 || (TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION >= 4)
#  define HAVE_TK_SETCLASSPROCS
/* pointer to Tk_SetClassProcs function in the stub table */

#if TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 6
static void (*SetClassProcsPtr)
        _ANSI_ARGS_((Tk_Window, Tk_ClassProcs *, ClientData));
#else
static void (*SetClassProcsPtr)
        _ANSI_ARGS_((Tk_Window, const Tk_ClassProcs *, ClientData));
#endif
#endif

/* 
 * Copy of TkClassProcs declarations from tkInt.h
 * (this is needed for Tcl ver =< 8.4a3)
 */

typedef Window (TkClassCreateProc) _ANSI_ARGS_((Tk_Window tkwin,
                Window parent, ClientData instanceData));
typedef void (TkClassGeometryProc) _ANSI_ARGS_((ClientData instanceData));
typedef void (TkClassModalProc) _ANSI_ARGS_((Tk_Window tkwin,
                XEvent *eventPtr));
typedef struct TkClassProcs
{
    TkClassCreateProc *createProc;
    TkClassGeometryProc *geometryProc;
    TkClassModalProc *modalProc;
} TkClassProcs;


/* Defaults */
#define DEFAULT_WIDTH		"400"
#define DEFAULT_HEIGHT		"400"
#define DEFAULT_IDENT		""
#define DEFAULT_FONTNAME	"Courier"
#define DEFAULT_TIME		"1"


#ifdef TOGL_WGL
/* Maximum size of a logical palette corresponding to a colormap in color index 
 * mode. */
#  define MAX_CI_COLORMAP_SIZE 4096
#  define MAX_CI_COLORMAP_BITS 12

#  if TOGL_USE_FONTS != 1
/* 
 * copy of TkWinColormap from tkWinInt.h
 */

typedef struct
{
    HPALETTE palette;           /* Palette handle used when drawing. */
    UINT    size;               /* Number of entries in the palette. */
    int     stale;              /* 1 if palette needs to be realized, otherwise 
                                 * 0.  If the palette is stale, then an idle
                                 * handler is scheduled to realize the palette. 
                                 */
    Tcl_HashTable refCounts;    /* Hash table of palette entry reference counts
                                 * indexed by pixel value. */
} TkWinColormap;
#  else
#    include <tkWinInt.h>
#  endif

static  LRESULT(CALLBACK *tkWinChildProc) (HWND hwnd, UINT message,
        WPARAM wParam, LPARAM lParam) = NULL;

#  ifndef TK_WIN_CHILD_CLASS_NAME 
#    define TK_WIN_CHILD_CLASS_NAME "TkChild"
#  endif

#endif /* TOGL_WGL */


#define MAX(a,b)	(((a)>(b))?(a):(b))

#define TCL_ERR(interp, string)			\
   do {						\
      Tcl_ResetResult(interp);			\
      Tcl_AppendResult(interp, string, NULL);	\
      return TCL_ERROR;				\
   } while (0)

#define ALL_EVENTS_MASK 	\
   (KeyPressMask		\
   |KeyReleaseMask		\
   |ButtonPressMask		\
   |ButtonReleaseMask		\
   |EnterWindowMask		\
   |LeaveWindowMask		\
   |PointerMotionMask		\
   |ExposureMask		\
   |VisibilityChangeMask	\
   |FocusChangeMask		\
   |PropertyChangeMask		\
   |ColormapChangeMask)

/* 
 * The following structure contains pointers to functions used for
 * processing the custom "-stereo" option.  Copied from tkPanedWindow.c.
 */
static int SetStereo(ClientData clientData, Tcl_Interp *interp,
        Tk_Window tkwin, Tcl_Obj **value, char *recordPtr,
        int internalOffset, char *oldInternalPtr, int flags);
static Tcl_Obj *GetStereo(ClientData clientData, Tk_Window tkwin,
        char *recordPtr, int internalOffset);
static void RestoreStereo(ClientData clientData, Tk_Window tkwin,
        char *internalPtr, char *oldInternalPtr);

static Tk_ObjCustomOption stereoOption = {
    "stereo",                   /* name */
    SetStereo,                  /* setProc */
    GetStereo,                  /* getProc */
    RestoreStereo,              /* restoreProc */
    NULL,                       /* freeProc */
    0
};

/* 
 * The following structure contains pointers to functions used for
 * processing the custom "-pixelformat" option.  Copied from tkPanedWindow.c.
 */
static int SetWideInt(ClientData clientData, Tcl_Interp *interp,
        Tk_Window tkwin, Tcl_Obj **value, char *recordPtr,
        int internalOffset, char *oldInternalPtr, int flags);
static Tcl_Obj *GetWideInt(ClientData clientData, Tk_Window tkwin,
        char *recordPtr, int internalOffset);
static void RestoreWideInt(ClientData clientData, Tk_Window tkwin,
        char *internalPtr, char *oldInternalPtr);

static Tk_ObjCustomOption wideIntOption = {
    "wide int",                 /* name */
    SetWideInt,                 /* setProc */
    GetWideInt,                 /* getProc */
    RestoreWideInt,             /* restoreProc */
    NULL,                       /* freeProc */
    0
};

/* 
 * Stuff we initialize on a per package (Togl_Init) basis.
 * Since Tcl uses one interpreter per thread, any per-thread
 * data goes here.
 */
struct Togl_PackageGlobals
{
    Tk_OptionTable optionTable; /* Used to parse options */
    Togl   *toglHead;           /* Head of linked list of all Togl widgets */
    int     nextContextTag;     /* Used to assign similar context tags */
};
typedef struct Togl_PackageGlobals Togl_PackageGlobals;

extern ToglStubs toglStubs;     /* should be only non-const global */

struct Togl
{
    Togl   *Next;               /* next in linked list */

#if defined(TOGL_WGL)
    HGLRC   Ctx;                /* OpenGL rendering context to be made current */
    HDC     tglGLHdc;           /* Device context of device that OpenGL calls
                                 * will be drawn on */
    int     CiColormapSize;     /* (Maximum) size of colormap in color index
                                 * mode */
#elif defined(TOGL_X11)
    GLXContext Ctx;             /* Normal planes GLX context */
#elif defined(TOGL_AGL)
    AGLContext Ctx;
#elif defined(TOGL_NSOPENGL)
    NSOpenGLContext *Ctx;
    NSView *nsview;
#endif
    int     contextTag;         /* all contexts with same tag share display
                                 * lists */

    XVisualInfo *VisInfo;       /* Visual info of the current */

    Display *display;           /* X's token for the window's display. */
    Tk_Window TkWin;            /* Tk window structure */
    Tcl_Interp *Interp;         /* Tcl interpreter */
    Tcl_Command widgetCmd;      /* Token for togl's widget command */
    Togl_PackageGlobals *tpg;   /* Used to access globals */
#ifndef NO_TK_CURSOR
    Tk_Cursor Cursor;           /* The widget's cursor */
#endif
    int     Width, Height;      /* Dimensions of window */
    int     SetGrid;            /* positive is grid size for window manager */
    int     TimerInterval;      /* Time interval for timer in milliseconds */
    Tcl_TimerToken timerHandler;        /* Token for togl's timer handler */
    Bool    RgbaFlag;           /* configuration flags (ala GLX parameters) */
    int     RgbaRed;
    int     RgbaGreen;
    int     RgbaBlue;
    Bool    DoubleFlag;
    Bool    DepthFlag;
    int     DepthSize;
    Bool    AccumFlag;
    int     AccumRed;
    int     AccumGreen;
    int     AccumBlue;
    int     AccumAlpha;
    Bool    AlphaFlag;
    int     AlphaSize;
    Bool    StencilFlag;
    int     StencilSize;
    Bool    PrivateCmapFlag;
    Bool    OverlayFlag;
    int     Stereo;
    double  EyeSeparation;
    double  Convergence;
    GLuint  riStencilBit;       /* row interleaved stencil bit */
    int     AuxNumber;
    Bool    Indirect;
#if defined(TOGL_NSOPENGL)
    NSOpenGLPixelFormat *PixelFormat;
#else
    Tcl_WideInt PixelFormat;
#endif
    int     SwapInterval;
    Bool    MultisampleFlag;
    Bool    FullscreenFlag;
    Bool    PbufferFlag;
    Bool    LargestPbufferFlag;
#if defined(TOGL_X11)
    GLXFBConfig fbcfg;          /* cache FBConfig for pbuffer creation */
    GLXPbuffer pbuf;
#elif defined(TOGL_WGL)
    HPBUFFERARB pbuf;
    int     pbufferLost;
#elif defined(TOGL_AGL)
    AGLPbuffer pbuf;
#elif defined(TOGL_NSOPENGL)
    NSOpenGLPixelBuffer *pbuf;
#endif
    const char *ShareList;      /* name (ident) of Togl to share dlists with */
    const char *ShareContext;   /* name (ident) to share OpenGL context with */

    const char *Ident;          /* User's identification string */
    ClientData Client_Data;     /* Pointer to user data */

    Bool    UpdatePending;      /* Should normal planes be redrawn? */

    Tcl_Obj *CreateProc;        /* Callback when widget is realized */
    Tcl_Obj *DisplayProc;       /* Callback when widget is redrawn */
    Tcl_Obj *ReshapeProc;       /* Callback when window size changes */
    Tcl_Obj *DestroyProc;       /* Callback when widget is destroyed */
    Tcl_Obj *TimerProc;         /* Callback when widget is idle */

    /* Overlay stuff */
#if defined(TOGL_X11)
    GLXContext OverlayCtx;      /* Overlay planes OpenGL context */
#elif defined(TOGL_WGL)
    HGLRC   tglGLOverlayHglrc;
#endif

    Window  OverlayWindow;      /* The overlay window, or 0 */
    Tcl_Obj *OverlayDisplayProc;        /* Overlay redraw proc */
    Bool    OverlayUpdatePending;       /* Should overlay be redrawn? */
    Colormap OverlayCmap;       /* colormap for overlay is created */
    int     OverlayTransparentPixel;    /* transparent pixel */
    Bool    OverlayIsMapped;

    GLfloat *RedMap;            /* Index2RGB Maps for Color index modes */
    GLfloat *GreenMap;
    GLfloat *BlueMap;
    GLint   MapSize;            /* = Number of indices in our Togl */
    int     currentStereoBuffer;
#ifdef HAVE_AUTOSTEREO
    int     as_initialized;     /* for autostereo package */
    ASHandle ash;               /* for autostereo package */
#endif
    int     badWindow;          /* true when Togl_MakeWindow fails or should
                                 * create a dummy window */
};


/* 
 * Prototypes for functions local to this file
 */
static int Togl_ObjCmd(ClientData clientData, Tcl_Interp *interp,
        int objc, Tcl_Obj *const *objv);
static void Togl_ObjCmdDelete(ClientData clientData);
static void Togl_EventProc(ClientData clientData, XEvent *eventPtr);
static void Togl_RedisplayProc(ClientData clientData, XEvent *eventPtr);
static Window Togl_MakeWindow(Tk_Window, Window, ClientData);
static void Togl_WorldChanged(ClientData);

#ifdef MESA_COLOR_HACK
static int get_free_color_cells(Display *display, int screen,
        Colormap colormap);
static void free_default_color_cells(Display *display, Colormap colormap);
#endif
static void ToglCmdDeletedProc(ClientData);

#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
static void SetMacBufRect(Togl *togl);
#if defined(TOGL_COCOA)
static WindowRef Togl_MacOSXGetDrawableWindow(Tk_Window w);
#endif
#endif

#if defined(TOGL_WGL)
#  include "toglWGL.c"
#elif defined(TOGL_AGL)
#  include "toglAGL.c"
#elif defined(TOGL_NSOPENGL)
#  include "toglNSOpenGL.c"
#elif defined(TOGL_X11)
#  include "toglGLX.c"
#endif


/* 
 * Setup Togl widget configuration options:
 */

#define GEOMETRY_MASK 0x1       /* widget geometry */
#define FORMAT_MASK 0x2         /* pixel format */
#define CURSOR_MASK 0x4
#define TIMER_MASK 0x8
#define OVERLAY_MASK 0x10
#define SWAP_MASK 0x20
#define STEREO_MASK 0x40
#define STEREO_FORMAT_MASK 0x80

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_PIXELS, TCL_STUPID "-height", "height", "Height",
                DEFAULT_HEIGHT, -1, Tk_Offset(Togl, Height), 0, NULL,
            GEOMETRY_MASK},
    {TK_OPTION_PIXELS, TCL_STUPID "-width", "width", "Width",
                DEFAULT_WIDTH, -1, Tk_Offset(Togl, Width), 0, NULL,
            GEOMETRY_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-rgba", "rgba", "Rgba",
            "true", -1, Tk_Offset(Togl, RgbaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-redsize", "redsize", "RedSize",
            "1", -1, Tk_Offset(Togl, RgbaRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-greensize", "greensize", "GreenSize",
            "1", -1, Tk_Offset(Togl, RgbaGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-bluesize", "bluesize", "BlueSize",
            "1", -1, Tk_Offset(Togl, RgbaBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-double", "double", "Double",
            "false", -1, Tk_Offset(Togl, DoubleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-depth", "depth", "Depth",
            "false", -1, Tk_Offset(Togl, DepthFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-depthsize", "depthsize", "DepthSize",
            "1", -1, Tk_Offset(Togl, DepthSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-accum", "accum", "Accum",
            "false", -1, Tk_Offset(Togl, AccumFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-accumredsize", "accumredsize", "AccumRedSize",
            "1", -1, Tk_Offset(Togl, AccumRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-accumgreensize", "accumgreensize",
                "AccumGreenSize",
            "1", -1, Tk_Offset(Togl, AccumGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-accumbluesize", "accumbluesize",
                "AccumBlueSize",
            "1", -1, Tk_Offset(Togl, AccumBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-accumalphasize", "accumalphasize",
                "AccumAlphaSize",
            "1", -1, Tk_Offset(Togl, AccumAlpha), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-alpha", "alpha", "Alpha",
            "false", -1, Tk_Offset(Togl, AlphaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-alphasize", "alphasize", "AlphaSize",
            "1", -1, Tk_Offset(Togl, AlphaSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-stencil", "stencil", "Stencil",
            "false", -1, Tk_Offset(Togl, StencilFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-stencilsize", "stencilsize", "StencilSize",
            "1", -1, Tk_Offset(Togl, StencilSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-auxbuffers", "auxbuffers", "AuxBuffers",
            "0", -1, Tk_Offset(Togl, AuxNumber), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-privatecmap", "privateCmap", "PrivateCmap",
                "false", -1, Tk_Offset(Togl, PrivateCmapFlag), 0, NULL,
            FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-overlay", "overlay", "Overlay",
            "false", -1, Tk_Offset(Togl, OverlayFlag), 0, NULL, OVERLAY_MASK},
    {TK_OPTION_CUSTOM, TCL_STUPID "-stereo", "stereo", "Stereo",
                "", -1, Tk_Offset(Togl, Stereo), 0,
            (ClientData) &stereoOption, STEREO_FORMAT_MASK},
    {TK_OPTION_DOUBLE, TCL_STUPID "-eyeseparation", "eyeseparation",
                "EyeSeparation",
            "2.0", -1, Tk_Offset(Togl, EyeSeparation), 0, NULL, STEREO_MASK},
    {TK_OPTION_DOUBLE, TCL_STUPID "-convergence", "convergence", "Convergence",
            "35.0", -1, Tk_Offset(Togl, Convergence), 0, NULL, STEREO_MASK},
#ifndef NO_TK_CURSOR
    {TK_OPTION_CURSOR, TCL_STUPID "-cursor", "cursor", "Cursor",
                "", -1, Tk_Offset(Togl, Cursor), TK_OPTION_NULL_OK, NULL,
            CURSOR_MASK},
#endif
    {TK_OPTION_INT, TCL_STUPID "-setgrid", "setGrid", "SetGrid",
            "0", -1, Tk_Offset(Togl, SetGrid), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, TCL_STUPID "-time", "time", "Time",
                DEFAULT_TIME, -1, Tk_Offset(Togl, TimerInterval), 0, NULL,
            TIMER_MASK},
    {TK_OPTION_STRING, TCL_STUPID "-sharelist", "sharelist", "ShareList",
            NULL, -1, Tk_Offset(Togl, ShareList), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, TCL_STUPID "-sharecontext", "sharecontext",
                "ShareContext", NULL,
            -1, Tk_Offset(Togl, ShareContext), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, TCL_STUPID "-ident", "ident", "Ident",
            DEFAULT_IDENT, -1, Tk_Offset(Togl, Ident), 0, NULL, 0},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-indirect", "indirect", "Indirect",
            "false", -1, Tk_Offset(Togl, Indirect), 0, NULL, FORMAT_MASK},
    {TK_OPTION_CUSTOM, TCL_STUPID "-pixelformat", "pixelFormat", "PixelFormat",
                "0", -1, Tk_Offset(Togl, PixelFormat), 0,
            (ClientData) &wideIntOption, FORMAT_MASK},
    {TK_OPTION_INT, TCL_STUPID "-swapinterval", "swapInterval", "SwapInterval",
            "1", -1, Tk_Offset(Togl, SwapInterval), 0, NULL, SWAP_MASK},
#if 0
    {TK_OPTION_BOOLEAN, TCL_STUPID "-fullscreen", "fullscreen", "Fullscreen",
                "false", -1, Tk_Offset(Togl, FullscreenFlag), 0, NULL,
            GEOMETRY_MASK|FORMAT_MASK},
#endif
    {TK_OPTION_BOOLEAN, TCL_STUPID "-multisample", "multisample", "Multisample",
                "false", -1, Tk_Offset(Togl, MultisampleFlag), 0, NULL,
            FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-pbuffer", "pbuffer", "Pbuffer",
            "false", -1, Tk_Offset(Togl, PbufferFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, TCL_STUPID "-largestpbuffer", "largestpbuffer",
                "LargestPbuffer",
            "false", -1, Tk_Offset(Togl, LargestPbufferFlag), 0, NULL, 0},
    {TK_OPTION_STRING, TCL_STUPID "-createcommand", "createCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, CreateProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-create", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-createcommand", 0},
    {TK_OPTION_STRING, TCL_STUPID "-displaycommand", "displayCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, DisplayProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-display", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-displaycommand", 0},
    {TK_OPTION_STRING, TCL_STUPID "-reshapecommand", "reshapeCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, ReshapeProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-reshape", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-reshapecommand", 0},
    {TK_OPTION_STRING, TCL_STUPID "-destroycommand", "destroyCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, DestroyProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-destroy", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-destroycommand", 0},
    {TK_OPTION_STRING, TCL_STUPID "-timercommand", "timerCommand",
                "CallabckCommand", NULL,
            Tk_Offset(Togl, TimerProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-timer", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-timercommand", 0},
    {TK_OPTION_STRING, TCL_STUPID "-overlaydisplaycommand",
                "overlaydisplayCommand", "CallbackCommand", NULL,
                Tk_Offset(Togl, OverlayDisplayProc), -1,
            TK_OPTION_NULL_OK, NULL, OVERLAY_MASK},
    {TK_OPTION_SYNONYM, TCL_STUPID "-overlaydisplay", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-overlaydisplaycommand", 0},
    /* Tcl3D backwards compatibility */
    {TK_OPTION_SYNONYM, TCL_STUPID "-createproc", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-createcommand", 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-displayproc", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-displaycommand", 0},
    {TK_OPTION_SYNONYM, TCL_STUPID "-reshapeproc", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-reshapecommand", 0},
    /* end Tcl3D compatibility */
    {TK_OPTION_END, NULL, NULL, NULL, NULL, -1, -1, 0, NULL, 0}
};

/* 
 * Add given togl widget to linked list.
 */
static void
AddToList(Togl *t)
{
    t->Next = t->tpg->toglHead;
    t->tpg->toglHead = t;
}

/* 
 * Remove given togl widget from linked list.
 */
static void
RemoveFromList(Togl *t)
{
    Togl   *prev;
    Togl   *cur;

    for (cur = t->tpg->toglHead, prev = NULL; cur; prev = cur, cur = cur->Next) {
        if (t != cur)
            continue;
        if (prev) {
            prev->Next = cur->Next;
        } else {
            t->tpg->toglHead = cur->Next;
        }
        break;
    }
    if (cur)
        cur->Next = NULL;
}

/* 
 * Return pointer to togl widget given a user identifier string.
 */
static Togl *
FindTogl(Togl *togl, const char *ident)
{
    Togl   *t;

    if (ident[0] != '.') {
        for (t = togl->tpg->toglHead; t; t = t->Next) {
            if (strcmp(t->Ident, ident) == 0)
                break;
        }
    } else {
        for (t = togl->tpg->toglHead; t; t = t->Next) {
            const char *pathname = Tk_PathName(t->TkWin);

            if (strcmp(pathname, ident) == 0)
                break;
        }
    }
    return t;
}


/* 
 * Return pointer to another togl widget with same OpenGL context.
 */
static Togl *
FindToglWithSameContext(const Togl *togl)
{
    Togl   *t;

    for (t = togl->tpg->toglHead; t != NULL; t = t->Next) {
        if (t == togl)
            continue;
        if (t->Ctx == togl->Ctx) {
            return t;
        }
    }
    return NULL;
}

#if TOGL_USE_OVERLAY
/* 
 * Return pointer to another togl widget with same OpenGL overlay context.
 */
static Togl *
FindToglWithSameOverlayContext(const Togl *togl)
{
    Togl   *t;

    for (t = togl->tpg->toglHead; t != NULL; t = t->Next) {
        if (t == togl)
            continue;
#  if defined(TOGL_X11)
        if (t->OverlayCtx == togl->OverlayCtx)
#  elif defined(TOGL_WGL)
        if (t->tglGLOverlayHglrc == togl->tglGLOverlayHglrc)
#  endif
        {
            return t;
        }
    }
    return NULL;
}
#endif

#if defined(TOGL_X11)
/* 
 * Return an X colormap to use for OpenGL RGB-mode rendering.
 * Input:  dpy - the X display
 *         scrnum - the X screen number
 *         visinfo - the XVisualInfo as returned by glXChooseVisual()
 * Return:  an X Colormap or 0 if there's a _serious_ error.
 */
static Colormap
get_rgb_colormap(Display *dpy,
        int scrnum, const XVisualInfo *visinfo, Tk_Window tkwin)
{
    Atom    hp_cr_maps;
    Status  status;
    int     numCmaps;
    int     i;
    XStandardColormap *standardCmaps;
    Window  root = XRootWindow(dpy, scrnum);
    Bool    using_mesa;

    /* 
     * First check if visinfo's visual matches the default/root visual.
     */
    if (visinfo->visual == Tk_Visual(tkwin)) {
        /* use the default/root colormap */
        Colormap cmap;

        cmap = Tk_Colormap(tkwin);
#  ifdef MESA_COLOR_HACK
        (void) get_free_color_cells(dpy, scrnum, cmap);
#  endif
        return cmap;
    }

    /* 
     * Check if we're using Mesa.
     */
    if (strstr(glXQueryServerString(dpy, scrnum, GLX_VERSION), "Mesa")) {
        using_mesa = True;
    } else {
        using_mesa = False;
    }

    /* 
     * Next, if we're using Mesa and displaying on an HP with the "Color
     * Recovery" feature and the visual is 8-bit TrueColor, search for a
     * special colormap initialized for dithering.  Mesa will know how to
     * dither using this colormap.
     */
    if (using_mesa) {
        hp_cr_maps = XInternAtom(dpy, "_HP_RGB_SMOOTH_MAP_LIST", True);
        if (hp_cr_maps
#  ifdef __cplusplus
                && visinfo->visual->c_class == TrueColor
#  else
                && visinfo->visual->class == TrueColor
#  endif
                && visinfo->depth == 8) {
            status = XGetRGBColormaps(dpy, root, &standardCmaps,
                    &numCmaps, hp_cr_maps);
            if (status) {
                for (i = 0; i < numCmaps; i++) {
                    if (standardCmaps[i].visualid == visinfo->visual->visualid) {
                        Colormap cmap = standardCmaps[i].colormap;

                        (void) XFree(standardCmaps);
                        return cmap;
                    }
                }
                (void) XFree(standardCmaps);
            }
        }
    }

    /* 
     * Next, try to find a standard X colormap.
     */
#  if !HP && !SUN
#    ifndef SOLARIS_BUG
    status = XmuLookupStandardColormap(dpy, visinfo->screen,
            visinfo->visualid, visinfo->depth, XA_RGB_DEFAULT_MAP,
            /* replace */ False, /* retain */ True);
    if (status == 1) {
        status = XGetRGBColormaps(dpy, root, &standardCmaps,
                &numCmaps, XA_RGB_DEFAULT_MAP);
        if (status == 1) {
            for (i = 0; i < numCmaps; i++) {
                if (standardCmaps[i].visualid == visinfo->visualid) {
                    Colormap cmap = standardCmaps[i].colormap;

                    (void) XFree(standardCmaps);
                    return cmap;
                }
            }
            (void) XFree(standardCmaps);
        }
    }
#    endif
#  endif

    /* 
     * If we get here, give up and just allocate a new colormap.
     */
    return XCreateColormap(dpy, root, visinfo->visual, AllocNone);
}
#elif defined(TOGL_WGL)

/* Code to create RGB palette is taken from the GENGL sample program of Win32
 * SDK */

static const unsigned char threeto8[8] = {
    0, 0111 >> 1, 0222 >> 1, 0333 >> 1, 0444 >> 1, 0555 >> 1, 0666 >> 1, 0377
};

static const unsigned char twoto8[4] = {
    0, 0x55, 0xaa, 0xff
};

static const unsigned char oneto8[2] = {
    0, 255
};

static const int defaultOverride[13] = {
    0, 3, 24, 27, 64, 67, 88, 173, 181, 236, 247, 164, 91
};

static const PALETTEENTRY defaultPalEntry[20] = {
    {0, 0, 0, 0},
    {0x80, 0, 0, 0},
    {0, 0x80, 0, 0},
    {0x80, 0x80, 0, 0},
    {0, 0, 0x80, 0},
    {0x80, 0, 0x80, 0},
    {0, 0x80, 0x80, 0},
    {0xC0, 0xC0, 0xC0, 0},

    {192, 220, 192, 0},
    {166, 202, 240, 0},
    {255, 251, 240, 0},
    {160, 160, 164, 0},

    {0x80, 0x80, 0x80, 0},
    {0xFF, 0, 0, 0},
    {0, 0xFF, 0, 0},
    {0xFF, 0xFF, 0, 0},
    {0, 0, 0xFF, 0},
    {0xFF, 0, 0xFF, 0},
    {0, 0xFF, 0xFF, 0},
    {0xFF, 0xFF, 0xFF, 0}
};

static unsigned char
ComponentFromIndex(int i, UINT nbits, UINT shift)
{
    unsigned char val;

    val = (unsigned char) (i >> shift);
    switch (nbits) {

      case 1:
          val &= 0x1;
          return oneto8[val];

      case 2:
          val &= 0x3;
          return twoto8[val];

      case 3:
          val &= 0x7;
          return threeto8[val];

      default:
          return 0;
    }
}

static Colormap
Win32CreateRgbColormap(PIXELFORMATDESCRIPTOR pfd)
{
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));
    LOGPALETTE *pPal;
    int     n, i;

    n = 1 << pfd.cColorBits;
    pPal = (PLOGPALETTE) LocalAlloc(LMEM_FIXED, sizeof (LOGPALETTE)
            + n * sizeof (PALETTEENTRY));
    pPal->palVersion = 0x300;
    pPal->palNumEntries = n;
    for (i = 0; i < n; i++) {
        pPal->palPalEntry[i].peRed =
                ComponentFromIndex(i, pfd.cRedBits, pfd.cRedShift);
        pPal->palPalEntry[i].peGreen =
                ComponentFromIndex(i, pfd.cGreenBits, pfd.cGreenShift);
        pPal->palPalEntry[i].peBlue =
                ComponentFromIndex(i, pfd.cBlueBits, pfd.cBlueShift);
        pPal->palPalEntry[i].peFlags = 0;
    }

    /* fix up the palette to include the default GDI palette */
    if ((pfd.cColorBits == 8)
            && (pfd.cRedBits == 3) && (pfd.cRedShift == 0)
            && (pfd.cGreenBits == 3) && (pfd.cGreenShift == 3)
            && (pfd.cBlueBits == 2) && (pfd.cBlueShift == 6)) {
        for (i = 1; i <= 12; i++)
            pPal->palPalEntry[defaultOverride[i]] = defaultPalEntry[i];
    }

    cmap->palette = CreatePalette(pPal);
    LocalFree(pPal);
    cmap->size = n;
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}

static Colormap
Win32CreateCiColormap(Togl *togl)
{
    /* Create a colormap with size of togl->CiColormapSize and set all entries
     * to black */

    LOGPALETTE logPalette;
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));

    logPalette.palVersion = 0x300;
    logPalette.palNumEntries = 1;
    logPalette.palPalEntry[0].peRed = 0;
    logPalette.palPalEntry[0].peGreen = 0;
    logPalette.palPalEntry[0].peBlue = 0;
    logPalette.palPalEntry[0].peFlags = 0;

    cmap->palette = CreatePalette(&logPalette);
    cmap->size = togl->CiColormapSize;
    ResizePalette(cmap->palette, cmap->size);   /* sets new entries to black */
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}

/* ErrorExit is from <http://msdn2.microsoft.com/en-us/library/ms680582.aspx> */
static void
ErrorExit(LPTSTR lpszFunction)
{
    /* Retrieve the system error message for the last-error code */
    LPTSTR  lpMsgBuf;
    LPTSTR  lpDisplayBuf;
    DWORD   err = GetLastError();

    if (err == 0) {
        /* The function said it failed, but GetLastError says it didn't, so
         * pretend it didn't. */
        return;
    }

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf, 0, NULL);

    /* Display the error message and exit the process */

    lpDisplayBuf = (LPTSTR) LocalAlloc(LMEM_ZEROINIT,
            (lstrlen(lpMsgBuf) + lstrlen(lpszFunction) + 40) * sizeof (TCHAR));
    StringCchPrintf(lpDisplayBuf, LocalSize(lpDisplayBuf),
            TEXT("%s failed with error %ld: %s"), lpszFunction, err, lpMsgBuf);
    MessageBox(NULL, lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(err);
}
#endif

/* 
 * Togl_Init
 *
 *   Called upon system startup to create togl command.
 */
int
Togl_Init(Tcl_Interp *interp)
{
    int     major, minor, patchLevel, releaseType;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, TCL_STUPID "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    Tcl_GetVersion(&major, &minor, &patchLevel, &releaseType);
#ifdef HAVE_TK_SETCLASSPROCS
    if (major > 8
            || (major == 8
                    && (minor > 4
                            || (minor == 4 && (releaseType > 0
                                            || patchLevel >= 2))))) {
#  ifdef USE_TK_STUBS
        SetClassProcsPtr = tkStubsPtr->tk_SetClassProcs;
#  else
        SetClassProcsPtr = Tk_SetClassProcs;
#  endif
    } else {
        SetClassProcsPtr = NULL;
    }
#else
    if (major > 8
            || (major == 8
                    && (minor > 4
                            || (minor == 4 && (releaseType > 0
                                            || patchLevel >= 2))))) {
        TCL_ERR(interp,
                "Sorry, this instance of Togl was not compiled to work with Tcl/Tk 8.4a2 or higher.");
    }
#endif

    if (Tcl_CreateObjCommand(interp, "togl", Togl_ObjCmd, NULL,
                    Togl_ObjCmdDelete) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvideEx(interp, "Togl", TOGL_VERSION, &toglStubs) != TCL_OK) {
        return TCL_ERROR;
    }

    return TCL_OK;
}


/* 
 * Togl_CallCallback
 *
 * Call command with togl widget as only argument
 */

int
Togl_CallCallback(Togl *togl, Tcl_Obj *cmd)
{
    int     result;
    Tcl_Obj *objv[3];

    if (cmd == NULL || togl->widgetCmd == NULL)
        return TCL_OK;

    objv[0] = cmd;
    Tcl_IncrRefCount(objv[0]);
    objv[1] =
            Tcl_NewStringObj(Tcl_GetCommandName(togl->Interp, togl->widgetCmd),
            -1);
    Tcl_IncrRefCount(objv[1]);
    objv[2] = NULL;
    result = Tcl_EvalObjv(togl->Interp, 2, objv, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(objv[1]);
    Tcl_DecrRefCount(objv[0]);
    if (result != TCL_OK)
        Tcl_BackgroundError(togl->Interp);
    return result;
}


/* 
 * Togl_Timer
 *
 * Gets called from Tk_CreateTimerHandler.
 */
static void
Togl_Timer(ClientData clientData)
{
    Togl   *togl = (Togl *) clientData;

    if (togl->TimerProc) {
        if (Togl_CallCallback(togl, togl->TimerProc) != TCL_OK) {
            togl->timerHandler = NULL;
            return;
        }
        /* 
         * Re-register this callback since Tcl/Tk timers are "one-shot".
         * That is, after the timer callback is called it not normally
         * called again.  That's not the behavior we want for Togl.
         */
        togl->timerHandler =
                Tcl_CreateTimerHandler(togl->TimerInterval, Togl_Timer,
                (ClientData) togl);
    }
}


/* 
 * Togl_MakeCurrent
 *
 *   Bind the OpenGL rendering context to the specified
 *   Togl widget.  If given a NULL argument, then the
 *   OpenGL context is released without assigning a new one.
 */
void
Togl_MakeCurrent(const Togl *togl)
{
#if defined(TOGL_WGL)
    int     res = TRUE;

    if (togl == NULL) {
        HDC     hdc = wglGetCurrentDC();

        if (hdc != NULL)
            res = wglMakeCurrent(hdc, NULL);
    } else {
        if (togl->pbufferLost) {
            Bool    keepContext = FindToglWithSameContext(togl) != NULL;
            Togl   *t = (Togl *) togl;  /* conceptually const */

            if (!keepContext) {
                wglDeleteContext(t->Ctx);
            }
            togl_destroyPbuffer(t);
            t->pbuf = togl_createPbuffer(t);
            if (!keepContext) {
                t->Ctx = wglCreateContext(t->tglGLHdc);
            }
        }
        res = wglMakeCurrent(togl->tglGLHdc, togl->Ctx);
    }
    if (!res) {
        ErrorExit(TEXT("wglMakeCurrent"));
    }
#elif defined(TOGL_X11)
    Display *display = togl ? togl->display : glXGetCurrentDisplay();

    if (display) {
        GLXDrawable drawable;

        if (!togl)
            drawable = None;
        else if (togl->PbufferFlag)
            drawable = togl->pbuf;
        else if (togl->TkWin)
            drawable = Tk_WindowId(togl->TkWin);
        else
            drawable = None;
        (void) glXMakeCurrent(display, drawable, drawable ? togl->Ctx : NULL);
    }
#elif defined(TOGL_AGL)
    if (togl == NULL || togl->Ctx == NULL) {
        (void) aglSetCurrentContext(NULL);
    } else {
        (void) aglSetCurrentContext(togl->Ctx);
        if (FindToglWithSameContext(togl) != NULL) {
            if (!togl->PbufferFlag) {
#if defined(TOGL_COCOA)
	        WindowRef w = Togl_MacOSXGetDrawableWindow(togl->TkWin);
	        aglSetWindowRef(togl->Ctx, w);
#else
                AGLDrawable d = Togl_MacOSXGetDrawablePort(togl);
                aglSetDrawable(togl->Ctx, d);
#endif
            } else {
                GLint   virtualScreen = aglGetVirtualScreen(togl->Ctx);

                aglSetPBuffer(togl->Ctx, togl->pbuf, 0, 0, virtualScreen);
            }
        }
    }
#elif defined(TOGL_NSOPENGL)
    if (togl != NULL && togl->Ctx != NULL) {
        [togl->Ctx makeCurrentContext];
        if (FindToglWithSameContext(togl) != NULL) {
            if (!togl->PbufferFlag) {
	        [togl->Ctx setView:togl->nsview];
            } else {
	        GLint   virtualScreen =	[togl->Ctx currentVirtualScreen];
                [togl->Ctx setPixelBuffer:togl->pbuf cubeMapFace:0
		 mipMapLevel:0 currentVirtualScreen:virtualScreen];
            }
        }
    }
#endif
}

/* 
 * Togl_TakePhoto
 *
 *   Take a photo image of the current OpenGL window.  May have problems
 *   if window is partially obscured, either by other windows or by the
 *   edges of the display.
 */
int
Togl_TakePhoto(Togl *togl, Tk_PhotoHandle photo)
{
    GLubyte *buffer;
    Tk_PhotoImageBlock photoBlock;
    int     y, midy;
    unsigned char *cp;
    int     width = togl->Width, height = togl->Height;

    /* 
     * TIP #116 altered Tk_PhotoPutBlock API to add interp arg that 8.4
     * doesn't have.
     * We need to remove that for compiling with 8.4.
     */
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 5)
#  define TK_PHOTOPUTBLOCK(interp, hdl, blk, x, y, w, h, cr) \
   		Tk_PhotoPutBlock(hdl, blk, x, y, w, h, cr)
#else
#  define TK_PHOTOPUTBLOCK	Tk_PhotoPutBlock
#endif
    buffer = (GLubyte *) ckalloc(width * height * 4);
    photoBlock.pixelPtr = buffer;
    photoBlock.width = width;
    photoBlock.height = height;
    photoBlock.pitch = width * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    if (!togl->RgbaFlag) {
#if defined(TOGL_WGL)
        /* Due to the lack of a unique inverse mapping from the frame buffer to
         * the logical palette we need a translation map from the complete
         * logical palette. */
        int     n, i;
        TkWinColormap *cmap = (TkWinColormap *) Tk_Colormap(togl->TkWin);
        LPPALETTEENTRY entry = (LPPALETTEENTRY) malloc(togl->MapSize *
                sizeof (PALETTEENTRY));

        n = GetPaletteEntries(cmap->palette, 0, togl->MapSize, entry);
        for (i = 0; i < n; i++) {
            togl->RedMap[i] = (GLfloat) (entry[i].peRed / 255.0);
            togl->GreenMap[i] = (GLfloat) (entry[i].peGreen / 255.0);
            togl->BlueMap[i] = (GLfloat) (entry[i].peBlue / 255.0);
        }
        free(entry);
#endif /* TOGL_WGL */

        glPixelMapfv(GL_PIXEL_MAP_I_TO_R, togl->MapSize, togl->RedMap);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_G, togl->MapSize, togl->GreenMap);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_B, togl->MapSize, togl->BlueMap);
    }

    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);        /* guarantee performance */
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

#if 1
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    /* Some OpenGL drivers are buggy and return zero for Alpha instead of one
     * for RGB pixel formats.  If that is happening to you, upgrade your
     * graphics driver. */

    /* OpenGL's origin is bottom-left, Tk Photo image's is top-left, so mirror
     * the rows around the middle row. */
    midy = height / 2;
    cp = buffer;
    for (y = 0; y < midy; ++y) {
        int     m_y = height - 1 - y;   /* mirror y */
        unsigned char *m_cp = buffer + m_y * photoBlock.pitch;
        int     x;

        for (x = 0; x < photoBlock.pitch; ++x) {
            unsigned char c = *cp;

            *cp++ = *m_cp;
            *m_cp++ = c;
        }
    }
#else
    /* OpenGL's origin is bottom-left, Tk Photo image's is top-left, so save
     * rows in reverse order. */
    glPixelStorei(GL_PACK_ROW_LENGTH, width);
    glPixelStorei(GL_PACK_SKIP_ROWS, -1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            buffer + width * (height - 1) * 4);
#endif

    TK_PHOTOPUTBLOCK(togl->Interp, photo, &photoBlock, 0, 0, width, height,
            TK_PHOTO_COMPOSITE_SET);

    glPopClientAttrib();        /* restore PACK_ALIGNMENT */
    ckfree((char *) buffer);
    return TCL_OK;
}

Bool
Togl_SwapInterval(const Togl *togl, int interval)
{
#ifdef TOGL_AGL
    GLint   swapInterval = interval;

    return aglSetInteger(togl->Ctx, AGL_SWAP_INTERVAL, &swapInterval);
#endif
#ifdef TOGL_NSOPENGL
    GLint   swapInterval = interval;
    [togl->Ctx setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
    return True;
#endif
#ifdef TOGL_WGL
    typedef BOOL (WINAPI *BOOLFuncInt) (int);
    typedef const char *(WINAPI *StrFuncHDC) (HDC);
    static BOOLFuncInt swapInterval = NULL;
    static BOOL initialized = False;

    if (!initialized) {
        const char *extensions;
        StrFuncHDC getExtensionsString;

        getExtensionsString = (StrFuncHDC)
                wglGetProcAddress("wglGetExtensionsStringARB");
        if (getExtensionsString == NULL)
            getExtensionsString = (StrFuncHDC)
                    wglGetProcAddress("wglGetExtensionsStringEXT");
        if (getExtensionsString) {
            extensions = getExtensionsString(togl->tglGLHdc);
            if (strstr(extensions, "WGL_EXT_swap_control") != NULL) {
                swapInterval =
                        (BOOLFuncInt) wglGetProcAddress("wglSwapIntervalEXT");
            }
        }
        initialized = True;
    }
    if (swapInterval)
        return swapInterval(interval);
    return False;
#endif
#ifdef TOGL_X11
    typedef int (*IntFuncInt) (int);
    static IntFuncInt swapInterval = NULL;
    static int initialized = False;

    if (!initialized) {
        const char *extensions = glXQueryExtensionsString(togl->display,
                Tk_ScreenNumber(togl->TkWin));

        if (strstr(extensions, "GLX_SGI_swap_control") != NULL) {
            swapInterval = (IntFuncInt) Togl_GetProcAddr("glXSwapIntervalSGI");
        } else if (strstr(extensions, "GLX_MESA_swap_control") != NULL) {
            swapInterval = (IntFuncInt) Togl_GetProcAddr("glXSwapIntervalMESA");
        }
        initialized = True;
    }
    if (swapInterval)
        return swapInterval(interval) == 0;
    return False;
#endif
}

#if defined(TOGL_AGL)

#if defined(TOGL_COCOA)
static WindowRef
Togl_MacOSXGetDrawableWindow(Tk_Window win)
{
  TkWindow *w = (TkWindow *) win;
  Drawable d = (Drawable) w->window;
  WindowRef r = TkMacOSXDrawableWindowRef(d);
  return r;
}
#endif

/* tell OpenGL which part of the Mac window to render to */
static void
SetMacBufRect(Togl *togl)
{
    GLint   wrect[4];
    Rect    r;
    MacDrawable *d = ((TkWindow *) togl->TkWin)->privatePtr;

    /* set wrect[0,1] to lower left corner of widget */
    wrect[2] = Tk_Width(togl->TkWin);
    wrect[3] = Tk_Height(togl->TkWin);
    wrect[0] = d->xOff;

#if defined(TOGL_COCOA)
    /*
      d->xOff, d->yOff seems to be the offset of the togl window upper left
      corner from its parent or from the containing top level.
      The wrect[1] value we need is the offset from the bottom of the
      containing top level.  Seems we may just need the height of the toplevel
      to make the calculation work.
    */
    /* Get bounds for containing top level. */
    TkWindow *t = (TkWindow *) togl->TkWin;
    for ( ; !(t->flags & TK_TOP_HIERARCHY) && t->parentPtr ; t = t->parentPtr) ;
    TkMacOSXWinBounds(t, &r);
#else
    GetPortBounds(Togl_MacOSXGetDrawablePort(togl), &r);
#endif

    wrect[1] = r.bottom - wrect[3] - d->yOff;

    if (togl->FullscreenFlag) {
        aglEnable(togl->Ctx, AGL_FS_CAPTURE_SINGLE);
        aglSetFullScreen(togl->Ctx, 0, 0, 0, 0);
    } else {
        aglUpdateContext(togl->Ctx);
    }
    aglSetInteger(togl->Ctx, AGL_BUFFER_RECT, wrect);
    aglEnable(togl->Ctx, AGL_BUFFER_RECT);
}

static void
ReconfigureCB(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags,
        void *closure)
{
    /* Display reconfiguration callback. Documented as needed by Apple QA1209.
     * Updated for 10.3 (and later) to use
     * CGDisplayRegisterReconfigurationCallback. */
    Togl   *togl = (Togl *) closure;

    if (0 != (flags & kCGDisplayBeginConfigurationFlag))
        return;                 /* wait until display is reconfigured */

    SetMacBufRect(togl);
    Togl_MakeCurrent(togl);
    if (togl->Ctx) {
        if (togl->ReshapeProc) {
            Togl_CallCallback(togl, togl->ReshapeProc);
        } else {
            glViewport(0, 0, togl->Width, togl->Height);
        }
    }
}
#endif

#if defined(TOGL_NSOPENGL)
/*
TODO: It appears that Tk only makes an NSView for toplevel windows.
Also it looks like NSOpenGL does not have the equivalent of AGL_BUFFER_RECT
that allows opengl drawing to just part of an NSView.  So we might need to
create our own NSView for controlling the opengl bounds.
Look at TkMacOSXMakeRealWindowExist() in tkMacOSXWm.c.
*/

/* tell OpenGL which part of the Mac window to render to */
static void
SetMacBufRect(Togl *togl)
{
    Rect r, rt;
    NSRect    rect;
    TkWindow *w = (TkWindow *) togl->TkWin;
    TkWindow *t = w->privatePtr->toplevel->winPtr;

    TkMacOSXWinBounds(w, &r);
    TkMacOSXWinBounds(t, &rt);

    rect.origin.x = r.left - rt.left;
    rect.origin.y = rt.bottom - r.bottom;
    rect.size.width = r.right - r.left;
    rect.size.height = r.bottom - r.top;

    [togl->nsview setFrame:rect];
    [togl->Ctx update];
    
    /* TODO: Support full screen. */
}

static void
ReconfigureCB(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags,
        void *closure)
{
    /* Display reconfiguration callback. Documented as needed by Apple QA1209.
     * Updated for 10.3 (and later) to use
     * CGDisplayRegisterReconfigurationCallback. */
    Togl   *togl = (Togl *) closure;

    if (0 != (flags & kCGDisplayBeginConfigurationFlag))
        return;                 /* wait until display is reconfigured */

    SetMacBufRect(togl);
    Togl_MakeCurrent(togl);
    if (togl->Ctx) {
        if (togl->ReshapeProc) {
            Togl_CallCallback(togl, togl->ReshapeProc);
        } else {
            glViewport(0, 0, togl->Width, togl->Height);
        }
    }
}
#endif

/* 
 * Called when the widget's contents must be redrawn.  Basically, we
 * just call the user's render callback function.
 *
 * Note that the parameter type is ClientData so this function can be
 * passed to Tk_DoWhenIdle().
 */
static void
Togl_Render(ClientData clientData)
{
    Togl   *togl = (Togl *) clientData;

    if (togl->DisplayProc) {
        Togl_MakeCurrent(togl);
        Togl_CallCallback(togl, togl->DisplayProc);
    }
    togl->UpdatePending = False;
}


static void
Togl_RenderOverlay(ClientData clientData)
{
    Togl   *togl = (Togl *) clientData;

    if (togl->OverlayFlag && togl->OverlayDisplayProc) {
#if defined(TOGL_WGL)
        int     res = wglMakeCurrent(togl->tglGLHdc, togl->tglGLOverlayHglrc);

        if (!res) {
            ErrorExit(TEXT("wglMakeCurrent overlay"));
        }
#elif defined(TOGL_X11)
        (void) glXMakeCurrent(Tk_Display(togl->TkWin),
                togl->OverlayWindow, togl->OverlayCtx);
#endif /* TOGL_WGL */

        Togl_CallCallback(togl, togl->OverlayDisplayProc);
    }
    togl->OverlayUpdatePending = False;
}


static int
Togl_EnterStereo(Togl *togl)
{
    if (togl->Stereo == TOGL_STEREO_ROW_INTERLEAVED) {
        GLint   stencil_bits;
        Tk_Window top;

        Togl_MakeCurrent(togl);
        glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
        if (stencil_bits == 0) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "need stencil buffer for row interleaved stereo",
                    TCL_STATIC);
            return False;
        }
        togl->riStencilBit = 1u << (stencil_bits - 1);
        glEnable(GL_STENCIL_TEST);

        /* Need to redraw window when moved between odd and even scanlines, so
         * bind to top level window so we're notified when that happens. */
        top = togl->TkWin;
        while (!Tk_IsTopLevel(top)) {
            top = Tk_Parent(top);
            if (top == NULL)
                break;
        }
        if (top) {
            Tk_CreateEventHandler(top, StructureNotifyMask, Togl_RedisplayProc,
                    (ClientData) togl);
        }
    }
    return True;
}


static void
Togl_LeaveStereo(Togl *togl, int oldStereo)
{
    switch (oldStereo) {
      default:
          break;
#ifdef HAVE_AUTOSTEREO
      case TOGL_STEREO_NATIVE:
          if (togl->ash != -1) {
              ASClosedStereoWindow(togl->ash);
              togl->ash = -1;
          }
          break;
#endif
#ifdef __sgi
      case TOGL_STEREO_SGIOLDSTYLE:
          togl->currentStereoBuffer = STEREO_BUFFER_NONE;
          glXWaitGL();          /* sync with GL command stream before calling X 
                                 */
          XSGISetStereoBuffer(togl->display, Tk_WindowId(togl->TkWin),
                  togl->currentStereoBuffer);
          glXWaitX();           /* sync with X command stream before calling GL 
                                 */
          break;
#endif
      case TOGL_STEREO_ROW_INTERLEAVED:
          if (togl->riStencilBit) {
              Tk_Window top;

              glDisable(GL_STENCIL_TEST);

              /* need to remove previously added top level event handler */
              top = togl->TkWin;
              while (!Tk_IsTopLevel(top)) {
                  top = Tk_Parent(top);
                  if (top == NULL)
                      break;
              }
              if (top) {
                  Tk_DeleteEventHandler(top, StructureNotifyMask,
                          Togl_RedisplayProc, (ClientData) togl);
              }
          }
          break;
    }
}


/* 
 * See domentation about what can't be changed
 */
static int
Togl_ObjConfigure(Tcl_Interp *interp, Togl *togl,
        int objc, Tcl_Obj *const *objv)
{
    Tk_SavedOptions savedOptions;
    int     error, mask;
    int     undoMask = 0;
    Tcl_Obj *errorResult = NULL;
    int     oldStereo = togl->Stereo;
    int     oldWidth = togl->Width;
    int     oldHeight = togl->Height;

    for (error = 0; error <= 1; ++error, mask = undoMask) {
        if (error == 0) {
            /* 
             * Tk_SetOptions parses the command arguments
             * and looks for defaults in the resource database.
             */
            if (Tk_SetOptions(interp, WIDGREC togl, togl->tpg->optionTable,
                            objc, objv, togl->TkWin, &savedOptions, &mask)
                    != TCL_OK) {
                /* previous values are restored, so nothing to do */
                return TCL_ERROR;
            }
        } else {
            /* 
             * Restore options from saved values
             */
            errorResult = Tcl_GetObjResult(interp);
            Tcl_IncrRefCount(errorResult);
            Tk_RestoreSavedOptions(&savedOptions);
        }

        if (togl->Ident && togl->Ident[0] == '.') {
            Tcl_AppendResult(interp, "Can not set ident to a window path name",
                    NULL);
            continue;
        }

        if (togl->FullscreenFlag) {
            /* override width and height */
            togl->Width = WidthOfScreen(Tk_Screen(togl->TkWin));
            togl->Height = HeightOfScreen(Tk_Screen(togl->TkWin));
            undoMask |= GEOMETRY_MASK;
        }

        if (mask & GEOMETRY_MASK) {
            if (!togl->PbufferFlag) {
                Togl_WorldChanged((ClientData) togl);
                /* Reset width and height so ConfigureNotify
                 * event will call reshape callback */
                togl->Width = oldWidth;
                togl->Height = oldHeight;
                undoMask |= GEOMETRY_MASK;
            }
        }

        if (mask & OVERLAY_MASK) {
#if !TOGL_USE_OVERLAY
            if (togl->OverlayFlag) {
                Tcl_AppendResult(interp, "Sorry, overlay was disabled", NULL);
                continue;
            }
#else
#  if defined(TOGL_X11)
            if (togl->OverlayCtx)
#  elif defined(TOGL_WGL)
            if (togl->tglGLOverlayHglrc)
#  endif
            {
                /* 
                 * Trying to change existing pixel format/graphics context
                 */
                Tcl_AppendResult(interp,
                        "Unable to change overlay pixel format", NULL);
                continue;
            }
#endif
        }

        if (mask & SWAP_MASK) {
            if (togl->Ctx) {
                /* 
                 * Change existing swap interval
                 */
                Togl_MakeCurrent(togl); /* TODO: needed? */
                Togl_SwapInterval(togl, togl->SwapInterval);
                undoMask |= SWAP_MASK;
            }
        }

        if (error == 0 && (mask & STEREO_FORMAT_MASK) != 0) {
            if (oldStereo == TOGL_STEREO_NATIVE
                    || togl->Stereo == TOGL_STEREO_NATIVE) {
                /* only native stereo affects the visual format */
                mask |= FORMAT_MASK;
            }
            if (togl->Stereo == TOGL_STEREO_SGIOLDSTYLE) {
#ifndef __sgi
                Tcl_AppendResult(interp,
                        "sgioldstyle: only available on SGI computers", NULL);
                continue;
#else
                int     event, error;

                /* Make sure Display supports SGIStereo */
                if (XSGIStereoQueryExtension(Tk_Display(togl->TkWin), &event,
                                &error) == False) {
                    Tcl_AppendResult(interp,
                            "sgioldstyle: SGIStereo X extension is missing",
                            NULL);
                    continue;
                }
                /* Make sure Window (Screen) supports SGIStereo */
                if (XSGIQueryStereoMode(Tk_Display(togl->TkWin),
                                Tk_WindowId(Tk_Parent(togl->TkWin))) ==
                        X_STEREO_UNSUPPORTED) {
                    Tcl_AppendResult(interp,
                            "sgioldstyle: unsupported by screen", NULL);
                    continue;
                }
#endif
            }
        }

        if (mask & FORMAT_MASK) {
            if (togl->Ctx) {
                /* 
                 * Trying to change existing pixel format/graphics context
                 * TODO: (re)create graphics context
                 *
                 * save old graphics context
                 * try to create new one and share display lists
                 * if failure, then restore old one
                 */
                Tcl_AppendResult(interp, "Unable to change pixel format", NULL);
                continue;
            }
            if (togl->ShareContext && togl->ShareList) {
                Tcl_AppendResult(interp,
                        "only one of -sharelist and -sharecontext allowed",
                        NULL);
                continue;
            }
            if (togl->PbufferFlag && togl->Stereo) {
                Tcl_AppendResult(interp, "pbuffer not supported with stereo",
                        NULL);
                continue;
            }
            if (togl->PbufferFlag && togl->OverlayFlag) {
                Tcl_AppendResult(interp, "pbuffer not supported with overlay",
                        NULL);
                continue;
            }
            if (togl->FullscreenFlag) {
#if defined(TOGL_NSOPENGL)
	        Tcl_AppendResult(interp,
                       "Fullscreen not supported with Cocoa Tk", NULL);
                continue;
#endif
#ifndef TOGL_AGL
#  if TK_MAJOR_VERSION < 8 || (TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 5)
                Tcl_AppendResult(interp,
                        "Need Tk 8.5 or later for fullscreen support", NULL);
                continue;
#  endif
#endif
            }
            /* Whether or not the format is okay is figured out when togl tries 
             * to create the window. */
#ifdef MESA_COLOR_HACK
            free_default_color_cells(Tk_Display(togl->TkWin),
                    Tk_Colormap(togl->TkWin));
#endif
            undoMask |= FORMAT_MASK;
        }

        if (togl->Ctx) {
            if (oldStereo != togl->Stereo) {
                /* leaving stereo */
                Togl_LeaveStereo(togl, oldStereo);
                if (togl->Stereo && !Togl_EnterStereo(togl))
                    continue;
            }
        }

        if (mask & TIMER_MASK) {
            if (togl->timerHandler != NULL) {
                Tcl_DeleteTimerHandler(togl->timerHandler);
            }
            if (togl->TimerProc) {
                togl->timerHandler =
                        Tcl_CreateTimerHandler(togl->TimerInterval, Togl_Timer,
                        (ClientData) togl);
            }
            undoMask |= TIMER_MASK;
        }
        break;
    }

    if (error == 0) {
        Tk_FreeSavedOptions(&savedOptions);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, errorResult);
        Tcl_DecrRefCount(errorResult);
        return TCL_ERROR;
    }
}


static int
Togl_ObjWidget(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl   *togl = (Togl *) clientData;
    const char *commands[] = {
        "cget", "configure", "extensions",
        "postredisplay", "render",
        "swapbuffers", "makecurrent", "takephoto",
        "loadbitmapfont", "unloadbitmapfont", "write",
        "uselayer", "showoverlay", "hideoverlay",
        "postredisplayoverlay", "renderoverlay",
        "existsoverlay", "ismappedoverlay",
        "getoverlaytransparentvalue",
        "drawbuffer", "clear", "frustum", "ortho",
        "numeyes", "contexttag", "copycontextto",
        NULL
    };
    enum command
    {
        TOGL_CGET, TOGL_CONFIGURE, TOGL_EXTENSIONS,
        TOGL_POSTREDISPLAY, TOGL_RENDER,
        TOGL_SWAPBUFFERS, TOGL_MAKECURRENT, TOGL_TAKEPHOTO,
        TOGL_LOADBITMAPFONT, TOGL_UNLOADBITMAPFONT, TOGL_WRITE,
        TOGL_USELAYER, TOGL_SHOWOVERLAY, TOGL_HIDEOVERLAY,
        TOGL_POSTREDISPLAYOVERLAY, TOGL_RENDEROVERLAY,
        TOGL_EXISTSOVERLAY, TOGL_ISMAPPEDOVERLAY,
        TOGL_GETOVERLAYTRANSPARENTVALUE,
        TOGL_DRAWBUFFER, TOGL_CLEAR, TOGL_FRUSTUM, TOGL_ORTHO,
        TOGL_NUMEYES, TOGL_CONTEXTTAG, TOGL_COPYCONTEXTTO
    };
    int     result = TCL_OK;
    Tcl_Obj *objPtr;
    int     index;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg ...?");
        return TCL_ERROR;
    }

    Tk_Preserve((ClientData) togl);

    result = Tcl_GetIndexFromObj(interp, objv[1], commands, "option", 0,
            &index);

    switch (index) {
      case TOGL_CGET:
          if (objc != 3) {
              Tcl_WrongNumArgs(interp, 2, objv, "option");
              result = TCL_ERROR;
              break;
          }
          objPtr = Tk_GetOptionValue(interp, WIDGREC togl,
                  togl->tpg->optionTable, (objc == 3) ? objv[2] : NULL,
                  togl->TkWin);
          if (objPtr == NULL) {
              result = TCL_ERROR;
              break;
          }
          Tcl_SetObjResult(interp, objPtr);
          break;

      case TOGL_CONFIGURE:
          if (objc <= 3) {
              /* 
               * Return one item if the option is given,
               * or return all configuration information
               */
              objPtr = Tk_GetOptionInfo(interp, WIDGREC togl,
                      togl->tpg->optionTable, (objc == 3) ? objv[2] : NULL,
                      togl->TkWin);
              if (objPtr == NULL) {
                  result = TCL_ERROR;
              } else {
                  Tcl_SetObjResult(interp, objPtr);
              }
          } else {
              /* Execute a configuration change */
              result = Togl_ObjConfigure(interp, togl, objc - 2, objv + 2);
          }
          break;

      case TOGL_EXTENSIONS:
          /* Return a list of OpenGL extensions available */
          /* TODO: -glu for glu extensions, -platform for glx/wgl extensions */
          if (objc == 2) {
              const char *extensions;
              Tcl_Obj *objPtr;
              int     length = -1;

              extensions = (const char *) glGetString(GL_EXTENSIONS);
              objPtr = Tcl_NewStringObj(extensions, -1);
              /* convert to list by asking for its length */
              (void) Tcl_ListObjLength(interp, objPtr, &length);
              Tcl_SetObjResult(interp, objPtr);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_POSTREDISPLAY:
          /* schedule the widget to be redrawn */
          if (objc == 2) {
              Togl_PostRedisplay(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_RENDER:
          /* force the widget to be redrawn */
          if (objc == 2) {
              Togl_Render((ClientData) togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_SWAPBUFFERS:
          /* force the widget to be redrawn */
          if (objc == 2) {
              Togl_SwapBuffers(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_MAKECURRENT:
          /* force the widget to be redrawn */
          if (objc == 2) {
              Togl_MakeCurrent(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_TAKEPHOTO:
      {
          /* force the widget to be redrawn */
          if (objc != 3) {
              Tcl_WrongNumArgs(interp, 2, objv, "name");
              result = TCL_ERROR;
          } else {
              const char *name;
              Tk_PhotoHandle photo;

              name = Tcl_GetStringFromObj(objv[2], NULL);
              photo = Tk_FindPhoto(interp, name);
              if (photo == NULL) {
                  Tcl_AppendResult(interp, "image \"", name,
                          "\" doesn't exist or is not a photo image", NULL);
                  result = TCL_ERROR;
                  break;
              }
              glPushAttrib(GL_PIXEL_MODE_BIT);
              if (togl->DoubleFlag) {
                  glReadBuffer(GL_FRONT);
              }
              Togl_TakePhoto(togl, photo);
              glPopAttrib();    /* restore glReadBuffer */
          }
          break;
      }

      case TOGL_LOADBITMAPFONT:
          Tcl_AppendResult(interp, "unsupported", NULL);
          result = TCL_ERROR;
          break;

      case TOGL_UNLOADBITMAPFONT:
          Tcl_AppendResult(interp, "unsupported", NULL);
          result = TCL_ERROR;
          break;

      case TOGL_WRITE:{
          Tcl_AppendResult(interp, "unsupported", NULL);
          result = TCL_ERROR;
          break;
      }

      case TOGL_USELAYER:
          if (objc == 3) {
              int     layer;

              result = Tcl_GetIntFromObj(interp, objv[2], &layer);
              if (result == TCL_OK) {
                  Togl_UseLayer(togl, layer);
              }
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, "layer");
              result = TCL_ERROR;
          }
          break;

      case TOGL_SHOWOVERLAY:
          if (objc == 2) {
              Togl_ShowOverlay(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_HIDEOVERLAY:
          if (objc == 2) {
              Togl_HideOverlay(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_POSTREDISPLAYOVERLAY:
          if (objc == 2) {
              Togl_PostOverlayRedisplay(togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_RENDEROVERLAY:
          /* force the overlay to be redrawn */
          if (objc == 2) {
              Togl_RenderOverlay((ClientData) togl);
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_EXISTSOVERLAY:
          if (objc == 2) {
              Tcl_SetObjResult(interp, Tcl_NewIntObj(Togl_ExistsOverlay(togl)));
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_ISMAPPEDOVERLAY:
          if (objc == 2) {
              Tcl_SetObjResult(interp,
                      Tcl_NewIntObj(Togl_IsMappedOverlay(togl)));
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_GETOVERLAYTRANSPARENTVALUE:
          if (objc == 2) {
              Tcl_SetObjResult(interp,
                      Tcl_NewIntObj(Togl_GetOverlayTransparentValue(togl)));
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_DRAWBUFFER:
          if (objc != 3) {
              Tcl_WrongNumArgs(interp, 2, objv, "mode");
              result = TCL_ERROR;
          } else {
              int     mask;

              result = Tcl_GetIntFromObj(interp, objv[2], &mask);
              if (result == TCL_ERROR)
                  break;
              Togl_DrawBuffer(togl, (GLenum) mask);
          }
          break;

      case TOGL_CLEAR:
          if (objc != 3) {
              Tcl_WrongNumArgs(interp, 2, objv, "mask");
              result = TCL_ERROR;
          } else {
              int     mask;

              result = Tcl_GetIntFromObj(interp, objv[2], &mask);
              if (result == TCL_ERROR)
                  break;
              Togl_Clear(togl, (GLbitfield) mask);
          }
          break;

      case TOGL_FRUSTUM:
          if (objc != 8) {
              Tcl_WrongNumArgs(interp, 2, objv,
                      "left right bottom top near far");
              result = TCL_ERROR;
          } else {
              double  left, right, bottom, top, zNear, zFar;

              if (Tcl_GetDoubleFromObj(interp, objv[2], &left) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[3],
                              &right) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[4],
                              &bottom) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[5],
                              &top) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[6],
                              &zNear) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[7],
                              &zFar) == TCL_ERROR) {
                  result = TCL_ERROR;
                  break;
              }
              Togl_Frustum(togl, left, right, bottom, top, zNear, zFar);
          }
          break;

      case TOGL_ORTHO:
          if (objc != 8) {
              Tcl_WrongNumArgs(interp, 2, objv,
                      "left right bottom top near far");
              result = TCL_ERROR;
          } else {
              double  left, right, bottom, top, zNear, zFar;

              if (Tcl_GetDoubleFromObj(interp, objv[2], &left) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[3],
                              &right) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[4],
                              &bottom) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[5],
                              &top) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[6],
                              &zNear) == TCL_ERROR
                      || Tcl_GetDoubleFromObj(interp, objv[7],
                              &zFar) == TCL_ERROR) {
                  result = TCL_ERROR;
                  break;
              }
              Togl_Ortho(togl, left, right, bottom, top, zNear, zFar);
          }
          break;

      case TOGL_NUMEYES:
          if (objc == 2) {
              Tcl_SetObjResult(interp, Tcl_NewIntObj(Togl_NumEyes(togl)));
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_CONTEXTTAG:
          if (objc == 2) {
              Tcl_SetObjResult(interp, Tcl_NewIntObj(Togl_ContextTag(togl)));
          } else {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          }
          break;

      case TOGL_COPYCONTEXTTO:
          if (objc != 4) {
              Tcl_WrongNumArgs(interp, 2, objv, NULL);
              result = TCL_ERROR;
          } else {
              Togl   *to;
              unsigned int mask;

              if (Togl_GetToglFromObj(togl->Interp, objv[2], &to) == TCL_ERROR
                      || Tcl_GetIntFromObj(togl->Interp, objv[3],
                              (int *) &mask) == TCL_ERROR) {
                  result = TCL_ERROR;
                  break;
              }
              result = Togl_CopyContext(togl, to, mask);
          }
    }

    Tk_Release((ClientData) togl);
    return result;
}

/* 
 * Togl_ObjCmdDelete
 *
 * Called when togl command is removed from interpreter.
 */

static void
Togl_ObjCmdDelete(ClientData clientData)
{
    if (clientData != NULL) {
        Togl_PackageGlobals *tpg = (Togl_PackageGlobals *) clientData;

        Tk_DeleteOptionTable(tpg->optionTable);
        ckfree((char *) clientData);
    }
}


/* 
 * Togl_ObjCmd
 *
 *   Called when Togl is executed - creation of a Togl widget.
 *     * Creates a new window
 *     * Creates an 'Togl' data structure
 *     * Creates an event handler for this window
 *     * Creates a command that handles this object
 *     * Configures this Togl for the given arguments
 */
int
Togl_ObjCmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const *objv)
{
    Togl_PackageGlobals *tpg;
    Togl   *togl;
    Tk_Window tkwin;
    Tcl_SavedResult saveError;

    if (objc <= 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
        return TCL_ERROR;
    }
    tpg = (Togl_PackageGlobals *) clientData;
    if (tpg == NULL) {
        Tcl_CmdInfo info;
        const char *name;

        /* 
         * Initialize the Togl_PackageGlobals for this widget the
         * first time a Togl widget is created.  The globals are
         * saved as our client data.
         */

        tpg = (Togl_PackageGlobals *) ckalloc(sizeof (Togl_PackageGlobals));
        if (tpg == NULL) {
            return TCL_ERROR;
        }
        tpg->nextContextTag = 0;
        tpg->optionTable = Tk_CreateOptionTable(interp, optionSpecs);
        tpg->toglHead = NULL;

        name = Tcl_GetString(objv[0]);
        Tcl_GetCommandInfo(interp, name, &info);
        info.objClientData = (ClientData) tpg;
        Tcl_SetCommandInfo(interp, name, &info);
    }

    /* Create the window. */
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp),
            Tcl_GetString(objv[1]), NULL);
    if (tkwin == NULL) {
        return TCL_ERROR;
    }

    Tk_SetClass(tkwin, "Togl");

    /* Create Togl data structure */
    togl = (Togl *) ckalloc(sizeof (Togl));
    if (togl == NULL) {
        return TCL_ERROR;
    }

    /* initialize Togl data structures values */
    togl->Next = NULL;
    togl->Ctx = NULL;
#if defined(TOGL_WGL)
    togl->tglGLHdc = NULL;
    togl->tglGLOverlayHglrc = NULL;
#elif defined(TOGL_X11)
    togl->OverlayCtx = NULL;
#endif
    togl->contextTag = 0;
    togl->display = Tk_Display(tkwin);
    togl->TkWin = tkwin;
    togl->Interp = interp;
    togl->VisInfo = NULL;
    togl->OverlayWindow = None;
    togl->OverlayCmap = None;
    togl->OverlayTransparentPixel = 0;
    togl->OverlayIsMapped = False;

    togl->UpdatePending = False;
    togl->OverlayUpdatePending = False;
    togl->tpg = tpg;
    togl->Client_Data = NULL;

    /* for color index mode photos */
    togl->RedMap = togl->GreenMap = togl->BlueMap = NULL;
    togl->MapSize = 0;

#ifndef NO_TK_CURSOR
    togl->Cursor = None;
#endif
    togl->Width = 0;
    togl->Height = 0;
    togl->SetGrid = 0;
    togl->TimerInterval = 0;
    togl->RgbaFlag = True;
    togl->RgbaRed = 1;
    togl->RgbaGreen = 1;
    togl->RgbaBlue = 1;
    togl->DoubleFlag = False;
    togl->DepthFlag = False;
    togl->DepthSize = 1;
    togl->AccumFlag = False;
    togl->AccumRed = 1;
    togl->AccumGreen = 1;
    togl->AccumBlue = 1;
    togl->AccumAlpha = 1;
    togl->AlphaFlag = False;
    togl->AlphaSize = 1;
    togl->StencilFlag = False;
    togl->StencilSize = 1;
    togl->OverlayFlag = False;
    togl->Stereo = TOGL_STEREO_NONE;
    togl->EyeSeparation = 0;
    togl->Convergence = 0;
    togl->riStencilBit = 0;
    togl->AuxNumber = 0;
    togl->Indirect = False;
    togl->PixelFormat = 0;
    togl->SwapInterval = 1;
    togl->MultisampleFlag = False;
    togl->FullscreenFlag = False;
    togl->PbufferFlag = False;
    togl->LargestPbufferFlag = False;
#if defined(TOGL_X11)
    togl->fbcfg = None;
    togl->pbuf = None;
#elif defined(TOGL_WGL)
    togl->pbuf = None;
    togl->pbufferLost = 0;
#elif defined(TOGL_AGL)
    togl->pbuf = NULL;
#elif defined(TOGL_NSOPENGL)
    togl->pbuf = NULL;
#endif

    togl->CreateProc = NULL;
    togl->DisplayProc = NULL;
    togl->ReshapeProc = NULL;
    togl->DestroyProc = NULL;
    togl->TimerProc = NULL;
    togl->timerHandler = NULL;
    togl->OverlayDisplayProc = NULL;
    togl->ShareList = NULL;
    togl->ShareContext = NULL;
    togl->Ident = NULL;
    togl->PrivateCmapFlag = False;
    togl->currentStereoBuffer = STEREO_BUFFER_NONE;
#ifdef HAVE_AUTOSTEREO
    togl->as_initialized = False;
    togl->ash = -1;
#endif
    togl->badWindow = False;

    /* Create command event handler */
    togl->widgetCmd = Tcl_CreateObjCommand(interp, Tk_PathName(tkwin),
            Togl_ObjWidget, (ClientData) togl, ToglCmdDeletedProc);

    /* 
     * Setup the Tk_ClassProcs callbacks to point at our own window creation
     * function
     *
     * We need to check at runtime if we should use the new Tk_SetClassProcs()
     * API or if we need to modify the window structure directly
     */
#ifdef HAVE_TK_SETCLASSPROCS

    if (SetClassProcsPtr != NULL) {     /* use public API (Tk 8.4+) */
        Tk_ClassProcs *procsPtr;

        procsPtr = (Tk_ClassProcs *) ckalloc(sizeof (Tk_ClassProcs));
        procsPtr->size = sizeof (Tk_ClassProcs);
        procsPtr->createProc = Togl_MakeWindow;
        procsPtr->worldChangedProc = Togl_WorldChanged;
        procsPtr->modalProc = NULL;
        /* Tk_SetClassProcs(togl->TkWin, procsPtr, (ClientData) togl); */
        (SetClassProcsPtr) (togl->TkWin, procsPtr, (ClientData) togl);
    } else
#endif
    {                           /* use private API */
        /* 
         * We need to set these fields in the Tk_FakeWin structure: dummy17 =
         * classProcsPtr dummy18 = instanceData */
        TkClassProcs *procsPtr;
        Tk_FakeWin *winPtr = (Tk_FakeWin *) (togl->TkWin);

        procsPtr = (TkClassProcs *) ckalloc(sizeof (TkClassProcs));
        procsPtr->createProc = Togl_MakeWindow;
        procsPtr->geometryProc = Togl_WorldChanged;
        procsPtr->modalProc = NULL;
        winPtr->dummy17 = (char *) procsPtr;
        winPtr->dummy18 = (ClientData) togl;
    }

    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
            Togl_EventProc, (ClientData) togl);

    /* Configure Togl widget */
    if (Tk_InitOptions(interp, WIDGREC togl, tpg->optionTable, tkwin) != TCL_OK
            || Togl_ObjConfigure(interp, togl, objc - 2, objv + 2) != TCL_OK) {
        goto error;
    }

    /* 
     * If OpenGL window wasn't already created by Togl_ObjConfigure() we
     * create it now.  We can tell by checking if the OpenGL context has
     * been initialized.
     */
    if (!togl->Ctx) {
        Tk_MakeWindowExist(togl->TkWin);
        if (togl->badWindow) {
            goto error;
        }
    }
    Togl_MakeCurrent(togl);
    if (togl->contextTag == 0)
        togl->contextTag = ++tpg->nextContextTag;

    (void) Togl_SwapInterval(togl, togl->SwapInterval);

    /* If defined, call create callback */
    if (togl->CreateProc) {
        if (Togl_CallCallback(togl, togl->CreateProc) != TCL_OK) {
            goto error;
        }
    }
#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    if (!togl->PbufferFlag)
        SetMacBufRect(togl);
#endif
    /* If defined, call reshape proc */
    if (togl->ReshapeProc) {
        if (Togl_CallCallback(togl, togl->ReshapeProc) != TCL_OK) {
            goto error;
        }
    } else {
        glViewport(0, 0, togl->Width, togl->Height);
#if defined(TOGL_X11)
        if (togl->OverlayFlag) {
            Togl_UseLayer(togl, TOGL_OVERLAY);
            glViewport(0, 0, togl->Width, togl->Height);
            Togl_UseLayer(togl, TOGL_NORMAL);
        }
#endif
    }

    if (togl->Stereo && !Togl_EnterStereo(togl))
        goto error;

    Tcl_AppendResult(interp, Tk_PathName(tkwin), NULL);

    /* Add to linked list */
    AddToList(togl);

    return TCL_OK;

  error:
    Tcl_SaveResult(interp, &saveError);
    togl->badWindow = True;
    (void) Tcl_DeleteCommandFromToken(interp, togl->widgetCmd);
    Tcl_RestoreResult(interp, &saveError);
    Tcl_AppendResult(interp, "\nCouldn't configure togl widget", NULL);
    return TCL_ERROR;
}


#if TOGL_USE_OVERLAY

/* 
 * Do all the setup for overlay planes
 * Return:   TCL_OK or TCL_ERROR
 */
static int
SetupOverlay(Togl *togl)
{
#  if defined(TOGL_X11)

#    ifdef GLX_TRANSPARENT_TYPE_EXT
    static int ovAttributeList[] = {
        GLX_BUFFER_SIZE, 2,
        GLX_LEVEL, 1,
        GLX_TRANSPARENT_TYPE_EXT, GLX_TRANSPARENT_INDEX_EXT,
        None
    };
#    else
    static int ovAttributeList[] = {
        GLX_BUFFER_SIZE, 2,
        GLX_LEVEL, 1,
        None
    };
#    endif

    XVisualInfo *visinfo;
    TkWindow *winPtr = (TkWindow *) togl->TkWin;

    XSetWindowAttributes swa;
    Tcl_HashEntry *hPtr;
    int     new_flag;

    visinfo =
            glXChooseVisual(togl->display, Tk_ScreenNumber(winPtr),
            ovAttributeList);
    if (!visinfo) {
        Tcl_AppendResult(togl->Interp, Tk_PathName(winPtr),
                ": No suitable overlay index visual available", NULL);
        togl->OverlayCtx = NULL;
        togl->OverlayWindow = 0;
        togl->OverlayCmap = 0;
        return TCL_ERROR;
    }
#    ifdef GLX_TRANSPARENT_INDEX_EXT
    {
        int     fail = glXGetConfig(togl->display, visinfo,
                GLX_TRANSPARENT_INDEX_VALUE_EXT,
                &togl->OverlayTransparentPixel);

        if (fail)
            togl->OverlayTransparentPixel = 0;  /* maybe, maybe ... */
    }
#    else
    togl->OverlayTransparentPixel = 0;  /* maybe, maybe ... */
#    endif

    /* share display lists with normal layer context */
    togl->OverlayCtx = glXCreateContext(togl->display, visinfo, togl->Ctx,
            !togl->Indirect);

    swa.colormap = XCreateColormap(togl->display,
            XRootWindow(togl->display, visinfo->screen),
            visinfo->visual, AllocNone);
    togl->OverlayCmap = swa.colormap;

    swa.border_pixel = 0;
    swa.event_mask = ALL_EVENTS_MASK;
    togl->OverlayWindow = XCreateWindow(togl->display,
            Tk_WindowId(togl->TkWin), 0, 0,
            togl->Width, togl->Height, 0,
            visinfo->depth, InputOutput,
            visinfo->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

    hPtr = Tcl_CreateHashEntry(&winPtr->dispPtr->winTable,
            (const char *) togl->OverlayWindow, &new_flag);
    Tcl_SetHashValue(hPtr, winPtr);

    /* XMapWindow(togl->display, togl->OverlayWindow); */
    togl->OverlayIsMapped = False;

    /* Make sure window manager installs our colormap */
    XSetWMColormapWindows(togl->display, togl->OverlayWindow,
            &togl->OverlayWindow, 1);

    return TCL_OK;

#  elif defined(TOGL_WGL) || defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    /* not yet implemented on these */
    return TCL_ERROR;
#  endif
}

#endif /* TOGL_USE_OVERLAY */



#ifdef TOGL_WGL
#  define TOGL_CLASS_NAME "Togl Class"
static Bool ToglClassInitialized = False;

static LRESULT CALLBACK
Win32WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG    result;
    Togl   *togl = (Togl *) GetWindowLongPtr(hwnd, 0);
    WNDCLASS childClass;

    switch (message) {

      case WM_WINDOWPOSCHANGED:
          /* Should be processed by DefWindowProc, otherwise a double buffered
           * context is not properly resized when the corresponding window is
           * resized. */
          break;

      case WM_DESTROY:
          if (togl && togl->TkWin != NULL) {
              if (togl->SetGrid > 0) {
                  Tk_UnsetGrid(togl->TkWin);
              }
              (void) Tcl_DeleteCommandFromToken(togl->Interp, togl->widgetCmd);
          }
          break;

      case WM_ERASEBKGND:
          /* We clear our own window */
          return 1;

      case WM_DISPLAYCHANGE:
          if (togl->PbufferFlag && hasARBPbuffer && !togl->pbufferLost) {
              queryPbuffer(togl->pbuf, WGL_PBUFFER_LOST_ARB,
                      &togl->pbufferLost);
          }
          /* FALLTHROUGH */

      default:
#  if USE_STATIC_LIB
          return TkWinChildProc(hwnd, message, wParam, lParam);
#  else
          /* 
           * OK, since TkWinChildProc is not explicitly exported in the
           * dynamic libraries, we have to retrieve it from the class info
           * registered with windows.
           *
           */
          if (tkWinChildProc == NULL) {
              GetClassInfo(Tk_GetHINSTANCE(), TK_WIN_CHILD_CLASS_NAME,
                      &childClass);
              tkWinChildProc = childClass.lpfnWndProc;
          }
          return tkWinChildProc(hwnd, message, wParam, lParam);
#  endif
    }
    result = DefWindowProc(hwnd, message, wParam, lParam);
    Tcl_ServiceAll();
    return result;
}
#endif /* TOGL_WGL */


/* 
 * Togl_MakeWindow
 *
 *   Window creation function, invoked as a callback from Tk_MakeWindowExist.
 *   This is called instead of TkpMakeWindow and must always succeed.
 */
static Window
Togl_MakeWindow(Tk_Window tkwin, Window parent, ClientData instanceData)
{
    Togl   *togl = (Togl *) instanceData;
    Display *dpy;
    Colormap cmap;
    int     scrnum;
    Window  window = None;

#if defined(TOGL_X11)
    Bool    directCtx = True;
    XSetWindowAttributes swa;
    int     width, height;
#elif defined(TOGL_WGL)
    HWND    hwnd, parentWin;
    DWORD   style;
    HINSTANCE hInstance;
    PIXELFORMATDESCRIPTOR pfd;
    int     width, height;
    Bool    createdPbufferDC = False;
#elif defined(TOGL_AGL)
#endif

    if (togl->badWindow) {
        TkWindow *winPtr = (TkWindow *) tkwin;

        return TkpMakeWindow(winPtr, parent);
    }

    /* for color index mode photos */
    if (togl->RedMap)
        free(togl->RedMap);
    if (togl->GreenMap)
        free(togl->GreenMap);
    if (togl->BlueMap)
        free(togl->BlueMap);
    togl->RedMap = togl->GreenMap = togl->BlueMap = NULL;
    togl->MapSize = 0;

    dpy = Tk_Display(tkwin);
    scrnum = Tk_ScreenNumber(tkwin);

    /* 
     * Windows and Mac OS X need the window created before OpenGL context
     * is created.  So do that now and set the window variable. 
     */
#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    {
        TkWindow *winPtr = (TkWindow *) tkwin;

        window = TkpMakeWindow(winPtr, parent);
        if (!togl->PbufferFlag)
            (void) XMapWindow(dpy, window);
    }
#elif defined(TOGL_WGL)
    hInstance = Tk_GetHINSTANCE();
    if (!ToglClassInitialized) {
        WNDCLASS ToglClass;

        ToglClassInitialized = True;
        ToglClass.style = CS_HREDRAW | CS_VREDRAW;
        ToglClass.cbClsExtra = 0;
        ToglClass.cbWndExtra = sizeof (LONG_PTR);       /* to save Togl* */
        ToglClass.hInstance = hInstance;
        ToglClass.hbrBackground = NULL;
        ToglClass.lpszMenuName = NULL;
        ToglClass.lpszClassName = TOGL_CLASS_NAME;
        ToglClass.lpfnWndProc = Win32WinProc;
        ToglClass.hIcon = NULL;
        ToglClass.hCursor = NULL;
        if (!RegisterClass(&ToglClass)) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable register Togl window class", TCL_STATIC);
            goto error;
        }
    }

    /* duplicate tkpMakeWindow logic from tk8.[45]/win/tkWinWindow.c */
    if (parent != None) {
        parentWin = Tk_GetHWND(parent);
        style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    } else {
        parentWin = NULL;
        style = WS_POPUP | WS_CLIPCHILDREN;
    }
    if (togl->PbufferFlag) {
        width = height = 1;     /* TODO: demo code mishaves when set to 1000 */
    } else {
        width = togl->Width;
        height = togl->Height;
    }
    hwnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, TOGL_CLASS_NAME, NULL, style,
            0, 0, width, height, parentWin, NULL, hInstance, NULL);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    window = Tk_AttachHWND(tkwin, hwnd);
    SetWindowLongPtr(hwnd, 0, (LONG_PTR) togl);
    if (togl->PbufferFlag) {
        ShowWindow(hwnd, SW_HIDE);      /* make sure it's hidden */
    }
#endif

    /* 
     * Figure out which OpenGL context to use
     */

#ifdef TOGL_WGL
    togl->tglGLHdc = GetDC(hwnd);
#endif
    if (togl->PixelFormat) {
#if defined(TOGL_X11)
        XVisualInfo template;
        int     count = 0;

        template.visualid = togl->PixelFormat;
        togl->VisInfo = XGetVisualInfo(dpy, VisualIDMask, &template, &count);
        if (togl->VisInfo == NULL) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "missing visual information", TCL_STATIC);
            goto error;
        }
#endif
        if (!togl_describePixelFormat(togl)) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "couldn't choose pixel format", TCL_STATIC);
            goto error;
        }
    } else {
#if defined(TOGL_X11)
        togl->VisInfo = togl_pixelFormat(togl, scrnum);
        if (togl->VisInfo == NULL)
#elif defined(TOGL_WGL) || defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
#  ifdef TOGL_WGL
        togl->PixelFormat = togl_pixelFormat(togl, hwnd);
#  elif defined(TOGL_NSOPENGL)
        togl->PixelFormat = (void *)togl_pixelFormat(togl);
#  else
        togl->PixelFormat = (Tcl_WideInt)togl_pixelFormat(togl);
#  endif
        if (togl->PixelFormat == 0)
#endif
        {
            goto error;
        }
    }
#ifdef TOGL_WGL
    if (togl->PbufferFlag) {
        togl->pbuf = togl_createPbuffer(togl);
        if (togl->pbuf == NULL) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "couldn't create pbuffer", TCL_STATIC);
            goto error;
        }
        ReleaseDC(hwnd, togl->tglGLHdc);
        togl->tglGLHdc = getPbufferDC(togl->pbuf);
        createdPbufferDC = True;
    } else if (SetPixelFormat(togl->tglGLHdc, (int) togl->PixelFormat,
                    NULL) == FALSE) {
        Tcl_SetResult(togl->Interp, TCL_STUPID "couldn't set pixel format",
                TCL_STATIC);
        goto error;
    }
#endif
#if defined(TOGL_WGL) || defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    if (togl->VisInfo == NULL) {
        /* 
         * Create a new OpenGL rendering context. And check to share lists.
         */
        Visual *visual;

        /* Just for portability, define the simplest visinfo */
        visual = DefaultVisual(dpy, scrnum);
        togl->VisInfo = (XVisualInfo *) calloc(1, sizeof (XVisualInfo));
        togl->VisInfo->screen = scrnum;
        togl->VisInfo->visual = visual;
        togl->VisInfo->visualid = visual->visualid;
#  if defined(__cplusplus) || defined(c_plusplus)
        togl->VisInfo->c_class = visual->c_class;
#  else
        togl->VisInfo->class = visual->class;
#  endif
        togl->VisInfo->depth = visual->bits_per_rgb;
    }
#endif

#if defined(TOGL_X11)
    if (togl->Indirect) {
        directCtx = False;
    }
#endif

    /* 
     * Create a new OpenGL rendering context.
     */
#if defined(TOGL_X11)
    if (togl->ShareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareList);
        GLXContext shareCtx;
        int     error_code;

        if (shareWith) {
            shareCtx = shareWith->Ctx;
            togl->contextTag = shareWith->contextTag;
        } else {
            shareCtx = None;
        }
        if (shareCtx) {
            togl_SetupXErrorHandler();
        }
        togl->Ctx = glXCreateContext(dpy, togl->VisInfo, shareCtx, directCtx);
        if (shareCtx && (error_code = togl_CheckForXError(togl))) {
            char    buf[256];

            togl->Ctx = NULL;
            XGetErrorText(dpy, error_code, buf, sizeof buf);
            Tcl_AppendResult(togl->Interp,
                    "unable to share display lists: ", buf, NULL);
            goto error;
        }
    } else {
        if (togl->ShareContext && FindTogl(togl, togl->ShareContext)) {
            /* share OpenGL context with existing Togl widget */
            Togl   *shareWith = FindTogl(togl, togl->ShareContext);

            if (togl->VisInfo->visualid != shareWith->VisInfo->visualid) {
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "unable to share OpenGL context",
                        TCL_STATIC);
                goto error;
            }
            togl->Ctx = shareWith->Ctx;
        } else {
            /* don't share display lists */
            togl->ShareContext = False;
            togl->Ctx = glXCreateContext(dpy, togl->VisInfo, None, directCtx);
        }
    }
#elif defined(TOGL_WGL)
    if (togl->ShareContext && FindTogl(togl, togl->ShareContext)) {
        /* share OpenGL context with existing Togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareContext);

        if (togl->PixelFormat != shareWith->PixelFormat) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share OpenGL context", TCL_STATIC);
            goto error;
        }
        togl->Ctx = shareWith->Ctx;
    } else {
        togl->Ctx = wglCreateContext(togl->tglGLHdc);
    }

    if (togl->ShareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareList);

        if (shareWith) {
            if (!wglShareLists(shareWith->Ctx, togl->Ctx)) {
#  if 0
                LPVOID  lpMsgBuf;
                DWORD   err = GetLastError();

                FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM,
                        NULL, err,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPTSTR) &lpMsgBuf, 0, NULL);
                fprintf(stderr, "unable to share display lists: %d: %s\n",
                        err, lpMsgBuf);
                LocalFree(lpMsgBuf);
#  endif
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "unable to share display lists", TCL_STATIC);
                goto error;
            }
            togl->contextTag = shareWith->contextTag;
        }
    }
#elif defined(TOGL_AGL)
    AGLContext shareCtx = NULL;

    if (togl->ShareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareList);

        if (shareWith) {
            shareCtx = shareWith->Ctx;
            togl->contextTag = shareWith->contextTag;
        }
    }
    if (togl->ShareContext && FindTogl(togl, togl->ShareContext)) {
        /* share OpenGL context with existing Togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareContext);

        if (togl->PixelFormat != shareWith->PixelFormat) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share OpenGL context", TCL_STATIC);
            goto error;
        }
        togl->Ctx = shareWith->Ctx;
    } else if ((togl->Ctx = aglCreateContext((AGLPixelFormat) togl->PixelFormat,
                            shareCtx)) == NULL) {
        GLenum  err = aglGetError();

        aglDestroyPixelFormat((AGLPixelFormat) togl->PixelFormat);
        togl->PixelFormat = 0;
        if (err == AGL_BAD_MATCH)
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share display lists"
                    ": shared context doesn't match", TCL_STATIC);
        else if (err == AGL_BAD_CONTEXT)
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share display lists"
                    ": bad shared context", TCL_STATIC);
        else if (err == AGL_BAD_PIXELFMT)
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "could not create rendering context"
                    ": bad pixel format", TCL_STATIC);
        else
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "could not create rendering context"
                    ": unknown reason", TCL_STATIC);
        goto error;
    }
    if (!togl->PbufferFlag) {
#if defined(TOGL_COCOA)
      /*
       * Can't use
           WindowRef r = Togl_MacOSXGetDrawableWindow(togl->TkWin);
       * because Tk window drawable is not set until Togl_MakeWindow()
       * routine returns.
       */
      WindowRef r = TkMacOSXDrawableWindowRef(window);
      GLboolean set = aglSetWindowRef(togl->Ctx, r);
#else
      GLboolean set = aglSetDrawable(togl->Ctx, Togl_MacOSXGetDrawablePort(togl));
#endif
      if (!set) {
        /* aglSetDrawable is deprecated in OS X 10.5 */
        aglDestroyContext(togl->Ctx);
        togl->Ctx = NULL;
        aglDestroyPixelFormat((AGLPixelFormat) togl->PixelFormat);
        togl->PixelFormat = 0;
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "couldn't set drawable", TCL_STATIC);
        goto error;
      }
    }
#elif defined(TOGL_NSOPENGL)
    NSOpenGLContext *shareCtx = NULL;
    if (togl->ShareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(togl, togl->ShareList);

        if (shareWith) {
            shareCtx = shareWith->Ctx;
            togl->contextTag = shareWith->contextTag;
        }
    }
    if (togl->ShareContext && FindTogl(togl, togl->ShareContext)) {
        /* share OpenGL context with existing Togl widget */
      Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share NSOpenGL context", TCL_STATIC);
      goto error;
      /*
        Togl   *shareWith = FindTogl(togl, togl->ShareContext);

        if (togl->PixelFormat != shareWith->PixelFormat) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to share OpenGL context", TCL_STATIC);
            goto error;
        }
	togl->Ctx = [[NSOpenGLContext alloc] initWithCGLContextObj:shareWith->Ctx];
      */
	/* initWithCGLContextObj requires Mac OS 10.6 */
    } else {
      togl->Ctx = [NSOpenGLContext alloc];
      if ([togl->Ctx initWithFormat:togl->PixelFormat shareContext:shareCtx]
	  == nil)
	{
	  [togl->PixelFormat release];
	  togl->PixelFormat = 0;
	  Tcl_SetResult(togl->Interp,
			TCL_STUPID "Could not obtain OpenGL context",
			TCL_STATIC);
	  goto error;
	}
    }

    if (!togl->PbufferFlag) {
      togl->nsview = [[NSView alloc] initWithFrame:NSZeroRect];
      MacDrawable *d = ((TkWindow *) togl->TkWin)->privatePtr;
      NSView *topview = d->toplevel->view;
      [topview addSubview:togl->nsview];
      /* TODO: Appears setView has to be deferred until window mapped.
       * or it gives "invalid drawable" error.  But MapNotify doesn't happen.
       * I think toplevel is already mapped.  Iconifying and uniconifying
       * main window makes the graphics work.
       */
      /*      [togl->Ctx setView:togl->nsview];*/
    }
#endif

    if (togl->Ctx == NULL) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "could not create rendering context", TCL_STATIC);
        goto error;
    }
#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    CGDisplayRegisterReconfigurationCallback(ReconfigureCB, togl);
#endif

    if (togl->PbufferFlag) {
        /* Don't need a colormap, nor overlay, nor be displayed */
#if defined(TOGL_X11) || defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
        togl->pbuf = togl_createPbuffer(togl);
        if (!togl->pbuf) {
            /* tcl result set in togl_createPbuffer */
#  ifdef TOGL_AGL
            if (!togl->ShareContext) {
                aglDestroyContext(togl->Ctx);
                aglDestroyPixelFormat((AGLPixelFormat) togl->PixelFormat);
            }
            togl->Ctx = NULL;
            togl->PixelFormat = 0;
#  endif
#  ifdef TOGL_NSOPENGL
            if (!togl->ShareContext) {
	        [togl->Ctx release];
		[togl->PixelFormat release];
            }
            togl->Ctx = NULL;
            togl->PixelFormat = 0;
#  endif
            goto error;
        }
#  ifdef TOGL_X11
        window = TkpMakeWindow((TkWindow *) tkwin, parent);
#  endif
#endif
        return window;
    }
#ifdef TOGL_WGL
    DescribePixelFormat(togl->tglGLHdc, (int) togl->PixelFormat, sizeof (pfd),
            &pfd);
#endif

    /* 
     * find a colormap
     */
    if (togl->RgbaFlag) {
        /* Colormap for RGB mode */
#if defined(TOGL_X11)
        cmap = get_rgb_colormap(dpy, scrnum, togl->VisInfo, tkwin);
#elif defined(TOGL_WGL)
        if (pfd.dwFlags & PFD_NEED_PALETTE) {
            cmap = Win32CreateRgbColormap(pfd);
        } else {
            cmap = DefaultColormap(dpy, scrnum);
        }
#elif defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
        cmap = DefaultColormap(dpy, scrnum);
#endif
    } else {
        /* Colormap for CI mode */
#ifdef TOGL_WGL
        /* this logic is to overcome a combination driver/compiler bug: (1)
         * cColorBits may be unusally large (e.g., 32 instead of 8 or 12) and
         * (2) 1 << 32 might be 1 instead of zero (gcc for ia32) */
        if (pfd.cColorBits >= MAX_CI_COLORMAP_BITS) {
            togl->CiColormapSize = MAX_CI_COLORMAP_SIZE;
        } else {
            togl->CiColormapSize = 1 << pfd.cColorBits;
            if (togl->CiColormapSize >= MAX_CI_COLORMAP_SIZE)
                togl->CiColormapSize = MAX_CI_COLORMAP_SIZE;
        }

#endif
        if (togl->PrivateCmapFlag) {
            /* need read/write colormap so user can store own color entries */
#if defined(TOGL_X11)
            cmap = XCreateColormap(dpy, XRootWindow(dpy, togl->VisInfo->screen),
                    togl->VisInfo->visual, AllocAll);
#elif defined(TOGL_WGL)
            cmap = Win32CreateCiColormap(togl);
#elif defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
            /* need to figure out how to do this correctly on Mac... */
            cmap = DefaultColormap(dpy, scrnum);
#endif
        } else {
            if (togl->VisInfo->visual == DefaultVisual(dpy, scrnum)) {
                /* share default/root colormap */
                cmap = Tk_Colormap(tkwin);
            } else {
                /* make a new read-only colormap */
                cmap = XCreateColormap(dpy,
                        XRootWindow(dpy, togl->VisInfo->screen),
                        togl->VisInfo->visual, AllocNone);
            }
        }
    }

    /* Make sure Tk knows to switch to the new colormap when the cursor is over
     * this window when running in color index mode. */
    (void) Tk_SetWindowVisual(tkwin, togl->VisInfo->visual,
            togl->VisInfo->depth, cmap);

#ifdef TOGL_WGL
    /* Install the colormap */
    SelectPalette(togl->tglGLHdc, ((TkWinColormap *) cmap)->palette, TRUE);
    RealizePalette(togl->tglGLHdc);
#endif

#if defined(TOGL_X11)
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.colormap = cmap;
    swa.event_mask = ALL_EVENTS_MASK;
    if (togl->PbufferFlag) {
        width = height = 1;
    } else {
        width = togl->Width;
        height = togl->Height;
    }
    window = XCreateWindow(dpy, parent,
            0, 0, width, height,
            0, togl->VisInfo->depth, InputOutput, togl->VisInfo->visual,
            CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask, &swa);
    /* Make sure window manager installs our colormap */
    (void) XSetWMColormapWindows(dpy, window, &window, 1);

    if (!togl->DoubleFlag) {
        int     dbl_flag;

        /* See if we requested single buffering but had to accept a double
         * buffered visual.  If so, set the GL draw buffer to be the front
         * buffer to simulate single buffering. */
        if (glXGetConfig(dpy, togl->VisInfo, GLX_DOUBLEBUFFER, &dbl_flag)) {
            if (dbl_flag) {
                glXMakeCurrent(dpy, window, togl->Ctx);
                glDrawBuffer(GL_FRONT);
                glReadBuffer(GL_FRONT);
            }
        }
    }
#elif defined(TOGL_WGL)
    if (!togl->DoubleFlag) {
        /* See if we requested single buffering but had to accept a double
         * buffered visual.  If so, set the GL draw buffer to be the front
         * buffer to simulate single buffering. */
        if (getPixelFormatAttribiv == NULL) {
            /* pfd is already set */
            if ((pfd.dwFlags & PFD_DOUBLEBUFFER) != 0) {
                wglMakeCurrent(togl->tglGLHdc, togl->Ctx);
                glDrawBuffer(GL_FRONT);
                glReadBuffer(GL_FRONT);
            }
        } else {
            static int attribs[] = {
                WGL_DOUBLE_BUFFER_ARB,
            };
#  define NUM_ATTRIBS (sizeof attribs / sizeof attribs[0])
            int     info[NUM_ATTRIBS];

            getPixelFormatAttribiv(togl->tglGLHdc, (int) togl->PixelFormat, 0,
                    NUM_ATTRIBS, attribs, info);
#  undef NUM_ATTRIBS
            if (info[0]) {
                wglMakeCurrent(togl->tglGLHdc, togl->Ctx);
                glDrawBuffer(GL_FRONT);
                glReadBuffer(GL_FRONT);
            }
        }
    }
#endif

#if TOGL_USE_OVERLAY
    if (togl->OverlayFlag) {
        if (SetupOverlay(togl) == TCL_ERROR) {
            fprintf(stderr, "Warning: couldn't setup overlay.\n");
            togl->OverlayFlag = False;
        }
    }
#endif

#if !defined(TOGL_AGL)
    /* Request the X window to be displayed */
    (void) XMapWindow(dpy, window);
#endif

    if (!togl->RgbaFlag) {
        int     index_size;

#if defined(TOGL_X11) || defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
        GLint   index_bits;

        glGetIntegerv(GL_INDEX_BITS, &index_bits);
        index_size = 1 << index_bits;
#elif defined(TOGL_WGL)
        index_size = togl->CiColormapSize;
#endif
        if (togl->MapSize != index_size) {
            if (togl->RedMap)
                free(togl->RedMap);
            if (togl->GreenMap)
                free(togl->GreenMap);
            if (togl->BlueMap)
                free(togl->BlueMap);
            togl->MapSize = index_size;
            togl->RedMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
            togl->GreenMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
            togl->BlueMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
        }
    }
#ifdef HAVE_AUTOSTEREO
    if (togl->Stereo == TOGL_STEREO_NATIVE) {
        if (!togl->as_initialized) {
            const char *autostereod;

            togl->as_initialized = True;
            if ((autostereod = getenv("AUTOSTEREOD")) == NULL)
                autostereod = AUTOSTEREOD;
            if (autostereod && *autostereod) {
                if (ASInitialize(togl->display, autostereod) == Success) {
                    togl->ash = ASCreatedStereoWindow(dpy);
                }
            }
        } else {
            togl->ash = ASCreatedStereoWindow(dpy);
        }
    }
#endif

    return window;

  error:

    togl->badWindow = True;

#if defined(TOGL_X11)
    if (window == None) {
        TkWindow *winPtr = (TkWindow *) tkwin;

        window = TkpMakeWindow(winPtr, parent);
    }
#elif defined(TOGL_WGL)
    if (togl->tglGLHdc) {
        if (createdPbufferDC)
            releasePbufferDC(togl->pbuf, togl->tglGLHdc);
        else
            ReleaseDC(hwnd, togl->tglGLHdc);
        togl->tglGLHdc = NULL;
    }
#endif
    return window;
}

/* 
 * Togl_WorldChanged
 *
 *    Add support for setgrid option.
 */
static void
Togl_WorldChanged(ClientData instanceData)
{
    Togl   *togl = (Togl *) instanceData;
    int     width;
    int     height;

    if (togl->PbufferFlag)
        width = height = 1;
    else {
        width = togl->Width;
        height = togl->Height;
    }
    Tk_GeometryRequest(togl->TkWin, width, height);
    Tk_SetInternalBorder(togl->TkWin, 0);
    if (togl->SetGrid > 0) {
        Tk_SetGrid(togl->TkWin, width / togl->SetGrid,
                height / togl->SetGrid, togl->SetGrid, togl->SetGrid);
    } else {
        Tk_UnsetGrid(togl->TkWin);
    }
}

/* 
 * ToglFree
 *
 *      Wrap the ckfree macro.
 */
static void
ToglFree(char *clientData)
{
    ckfree(clientData);
}

/* 
 * ToglCmdDeletedProc
 *
 *      This procedure is invoked when a widget command is deleted.  If
 *      the widget isn't already in the process of being destroyed,
 *      this command destroys it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */
static void
ToglCmdDeletedProc(ClientData clientData)
{
    Togl   *togl = (Togl *) clientData;
    Tk_Window tkwin = togl->TkWin;

    /* 
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin) {
        Tk_DeleteEventHandler(tkwin, ExposureMask | StructureNotifyMask,
                Togl_EventProc, (ClientData) togl);
    }

    Tk_Preserve((ClientData) togl);
    Tcl_EventuallyFree((ClientData) togl, ToglFree);

    Togl_LeaveStereo(togl, togl->Stereo);

    if (togl->DestroyProc) {
        /* call user's cleanup code */
        Togl_CallCallback(togl, togl->DestroyProc);
    }

    if (togl->TimerProc != NULL) {
        Tcl_DeleteTimerHandler(togl->timerHandler);
        togl->timerHandler = NULL;
    }
    if (togl->UpdatePending) {
        Tcl_CancelIdleCall(Togl_Render, (ClientData) togl);
        togl->UpdatePending = False;
    }
#ifndef NO_TK_CURSOR
    if (togl->Cursor != None) {
        Tk_FreeCursor(togl->display, togl->Cursor);
        togl->Cursor = None;
    }
#endif

    /* remove from linked list */
    RemoveFromList(togl);

    togl->TkWin = NULL;
    if (tkwin != NULL) {

        if (togl->Ctx) {
            if (FindToglWithSameContext(togl) == NULL) {
#if defined(TOGL_X11)
                glXDestroyContext(togl->display, togl->Ctx);
#elif defined(TOGL_WGL)
                wglDeleteContext(togl->Ctx);
#elif defined(TOGL_AGL)
                aglDestroyContext(togl->Ctx);
                CGDisplayRemoveReconfigurationCallback(ReconfigureCB, togl);
#elif defined(TOGL_NSOPENGL)
		[togl->Ctx release];
		togl->Ctx = nil;
		[togl->nsview release];
		togl->nsview = nil;
                CGDisplayRemoveReconfigurationCallback(ReconfigureCB, togl);
#endif
#if defined(TOGL_X11)
                XFree(togl->VisInfo);
#else
                free(togl->VisInfo);
#endif
            }
#if defined(TOGL_WGL)
            if (togl->tglGLHdc) {
                if (togl->PbufferFlag) {
                    releasePbufferDC(togl->pbuf, togl->tglGLHdc);
                } else {
                    HWND    hwnd = Tk_GetHWND(Tk_WindowId(tkwin));

                    ReleaseDC(hwnd, togl->tglGLHdc);
                }
                togl->tglGLHdc = NULL;
            }
#endif
            if (togl->PbufferFlag && togl->pbuf) {
                togl_destroyPbuffer(togl);
                togl->pbuf = 0;
            }
            togl->Ctx = NULL;
            togl->VisInfo = NULL;
        }
#if defined(TOGL_X11)
#  if TOGL_USE_OVERLAY
        if (togl->OverlayCtx) {
            Tcl_HashEntry *entryPtr;
            TkWindow *winPtr = (TkWindow *) tkwin;

            if (winPtr) {
                entryPtr = Tcl_FindHashEntry(&winPtr->dispPtr->winTable,
                        (const char *) togl->OverlayWindow);
                Tcl_DeleteHashEntry(entryPtr);
            }
            if (FindToglWithSameOverlayContext(togl) == NULL)
                glXDestroyContext(togl->display, togl->OverlayCtx);
            togl->OverlayCtx = NULL;
        }
#  endif /* TOGL_USE_OVERLAY */
#endif

        if (togl->SetGrid > 0) {
            Tk_UnsetGrid(tkwin);
        }
        Tk_DestroyWindow(tkwin);
    }

    Tk_Release((ClientData) togl);
}


/* 
 * This gets called to track top level position changes for
 * row interleaved stereo.
 */
static void
Togl_RedisplayProc(ClientData clientData, XEvent *eventPtr)
{
    Togl   *togl = (Togl *) clientData;

    switch (eventPtr->type) {
      case ConfigureNotify:
          Togl_PostRedisplay(togl);
          break;
    }
}


/* 
 * This gets called to handle Togl window configuration events
 */
static void
Togl_EventProc(ClientData clientData, XEvent *eventPtr)
{
    Togl   *togl = (Togl *) clientData;

    switch (eventPtr->type) {
      case Expose:
          if (eventPtr->xexpose.count == 0) {
              if (!togl->UpdatePending
                      && eventPtr->xexpose.window == Tk_WindowId(togl->TkWin)) {
                  Togl_PostRedisplay(togl);
              }
#if defined(TOGL_X11)
              if (!togl->OverlayUpdatePending && togl->OverlayFlag
                      && togl->OverlayIsMapped
                      && eventPtr->xexpose.window == togl->OverlayWindow) {
                  Togl_PostOverlayRedisplay(togl);
              }
#endif
#if defined(TOGL_NSOPENGL)
	      [togl->Ctx setView:togl->nsview];
              SetMacBufRect(togl);
#endif
          }
          break;
      case ConfigureNotify:
          if (togl->PbufferFlag)
              break;
          if (togl->Width == Tk_Width(togl->TkWin)
                          && togl->Height == Tk_Height(togl->TkWin)) {

#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
              // Even though the size hasn't changed,
              // it's position on the screen may have.
              SetMacBufRect(togl);
#endif
              break;
          }
          togl->Width = Tk_Width(togl->TkWin);
          togl->Height = Tk_Height(togl->TkWin);
          (void) XResizeWindow(Tk_Display(togl->TkWin),
                  Tk_WindowId(togl->TkWin), togl->Width, togl->Height);
#if defined(TOGL_X11)
          if (togl->OverlayFlag) {
              (void) XResizeWindow(Tk_Display(togl->TkWin),
                      togl->OverlayWindow, togl->Width, togl->Height);
              (void) XRaiseWindow(Tk_Display(togl->TkWin), togl->OverlayWindow);
          }
#endif

#if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
          SetMacBufRect(togl);
#endif

          Togl_MakeCurrent(togl);
          if (togl->ReshapeProc) {
              (void) Togl_CallCallback(togl, togl->ReshapeProc);
          } else {
              glViewport(0, 0, togl->Width, togl->Height);
#if defined(TOGL_X11)
              if (togl->OverlayFlag) {
                  Togl_UseLayer(togl, TOGL_OVERLAY);
                  glViewport(0, 0, togl->Width, togl->Height);
                  Togl_UseLayer(togl, TOGL_NORMAL);
              }
#endif
          }
          break;
      case MapNotify:
#if defined(TOGL_AGL)
          if (!togl->PbufferFlag) {
              /* 
               * See comment for the UnmapNotify case below.
               */

#if defined(TOGL_COCOA)
	      WindowRef w = Togl_MacOSXGetDrawableWindow(togl->TkWin);
              aglSetWindowRef(togl->Ctx, w);
#else
              AGLDrawable d = Togl_MacOSXGetDrawablePort(togl);
              /* aglSetDrawable is deprecated in OS X 10.5 */
              aglSetDrawable(togl->Ctx, d);
#endif
              SetMacBufRect(togl);
          }
#endif
#if defined(TOGL_NSOPENGL)
          if (!togl->PbufferFlag) {
              /* 
               * See comment for the UnmapNotify case below.
               */
	      [togl->Ctx setView:togl->nsview];
              SetMacBufRect(togl);
          }
#endif
          break;
      case UnmapNotify:
#if defined(TOGL_AGL)
          if (!togl->PbufferFlag) {
              /* 
               * For Mac OS X Aqua, Tk subwindows are not implemented as
               * separate Aqua windows.  They are just different regions of
               * a single Aqua window.  To unmap them they are just not drawn.
               * Have to disconnect the AGL context otherwise they will continue
               * to be displayed directly by Aqua.
               */
#if defined(TOGL_COCOA)
              aglSetWindowRef(togl->Ctx, NULL);
#else
              /* aglSetDrawable is deprecated in OS X 10.5 */
              aglSetDrawable(togl->Ctx, NULL);
#endif
          }
#endif
#if defined(TOGL_NSOPENGL)
          if (!togl->PbufferFlag) {
              /* 
               * For Mac OS X Aqua, Tk subwindows are not implemented as
               * separate Aqua windows.  They are just different regions of
               * a single Aqua window.  To unmap them they are just not drawn.
               * Have to disconnect the NSView otherwise they will continue
               * to be displayed directly by Aqua.
               */
              [togl->Ctx clearDrawable];
          }
#endif
          break;
      case DestroyNotify:
          if (togl->TkWin != NULL) {
#ifdef TOGL_WGL
              HWND    hwnd = Tk_GetHWND(Tk_WindowId(togl->TkWin));

              /* Prevent Win32WinProc from calling Tcl_DeleteCommandFromToken
               * a second time */
              SetWindowLongPtr(hwnd, 0, (LONG_PTR) 0);
#endif
              if (togl->SetGrid > 0) {
                  Tk_UnsetGrid(togl->TkWin);
              }
              (void) Tcl_DeleteCommandFromToken(togl->Interp, togl->widgetCmd);
          }
          break;
      default:
          /* nothing */
          ;
    }
}


void
Togl_PostRedisplay(Togl *togl)
{
    if (!togl->UpdatePending) {
        togl->UpdatePending = True;
        Tk_DoWhenIdle(Togl_Render, (ClientData) togl);
    }
}


Bool
Togl_UpdatePending(const Togl *togl)
{
    return togl->UpdatePending;
}


void
Togl_SwapBuffers(const Togl *togl)
{
    if (togl->DoubleFlag) {
#if defined(TOGL_WGL)
        int     res = SwapBuffers(togl->tglGLHdc);

        if (!res) {
            ErrorExit(TEXT("SwapBuffers"));
        }
#elif defined(TOGL_X11)
        glXSwapBuffers(Tk_Display(togl->TkWin), Tk_WindowId(togl->TkWin));
#elif defined(TOGL_AGL)
        aglSwapBuffers(togl->Ctx);
#elif defined(TOGL_NSOPENGL)
        [togl->Ctx flushBuffer];
#endif
    } else {
        glFlush();
    }
}



const char *
Togl_Ident(const Togl *togl)
{
    return togl->Ident;
}


int
Togl_Width(const Togl *togl)
{
    return togl->Width;
}


int
Togl_Height(const Togl *togl)
{
    return togl->Height;
}


Tcl_Interp *
Togl_Interp(const Togl *togl)
{
    return togl->Interp;
}


Tk_Window
Togl_TkWin(const Togl *togl)
{
    return togl->TkWin;
}


const char *
Togl_CommandName(const Togl *togl)
{
    return Tcl_GetCommandName(togl->Interp, togl->widgetCmd);
}

int
Togl_ContextTag(const Togl *togl)
{
    return togl->contextTag;
}

Bool
Togl_HasRGBA(const Togl *togl)
{
    return togl->RgbaFlag;
}

Bool
Togl_IsDoubleBuffered(const Togl *togl)
{
    return togl->DoubleFlag;
}

Bool
Togl_HasDepthBuffer(const Togl *togl)
{
    return togl->DepthFlag;
}

Bool
Togl_HasAccumulationBuffer(const Togl *togl)
{
    return togl->AccumFlag;
}

Bool
Togl_HasDestinationAlpha(const Togl *togl)
{
    return togl->AlphaFlag;
}

Bool
Togl_HasStencilBuffer(const Togl *togl)
{
    return togl->StencilFlag;
}

int
Togl_StereoMode(const Togl *togl)
{
    return togl->Stereo;
}

Bool
Togl_HasMultisample(const Togl *togl)
{
    return togl->MultisampleFlag;
}


#if defined(TOGL_X11)
/* 
 * A replacement for XAllocColor.  This function should never
 * fail to allocate a color.  When XAllocColor fails, we return
 * the nearest matching color.  If we have to allocate many colors
 * this function isn't too efficient; the XQueryColors() could be
 * done just once.
 * Written by Michael Pichler, Brian Paul, Mark Kilgard
 * Input:  dpy - X display
 *         cmap - X colormap
 *         cmapSize - size of colormap
 * In/Out: color - the XColor struct
 * Output:  exact - 1=exact color match, 0=closest match
 */
static void
noFaultXAllocColor(Display *dpy, Colormap cmap, int cmapSize,
        XColor *color, int *exact)
{
    XColor *ctable, subColor;
    int     i, bestmatch;
    double  mindist;            /* 3*2^16^2 exceeds long int precision. */

    /* First try just using XAllocColor. */
    if (XAllocColor(dpy, cmap, color)) {
        *exact = 1;
        return;
    }

    /* Retrieve color table entries. */
    /* XXX alloca candidate. */
    ctable = (XColor *) ckalloc(cmapSize * sizeof (XColor));
    for (i = 0; i < cmapSize; i++) {
        ctable[i].pixel = i;
    }
    (void) XQueryColors(dpy, cmap, ctable, cmapSize);

    /* Find best match. */
    bestmatch = -1;
    mindist = 0;
    for (i = 0; i < cmapSize; i++) {
        double  dr = (double) color->red - (double) ctable[i].red;
        double  dg = (double) color->green - (double) ctable[i].green;
        double  db = (double) color->blue - (double) ctable[i].blue;
        double  dist = dr * dr + dg * dg + db * db;

        if (bestmatch < 0 || dist < mindist) {
            bestmatch = i;
            mindist = dist;
        }
    }

    /* Return result. */
    subColor.red = ctable[bestmatch].red;
    subColor.green = ctable[bestmatch].green;
    subColor.blue = ctable[bestmatch].blue;
    ckfree((char *) ctable);
    /* Try to allocate the closest match color.  This should only fail if the
     * cell is read/write.  Otherwise, we're incrementing the cell's reference
     * count. */
    if (!XAllocColor(dpy, cmap, &subColor)) {
        /* do this to work around a problem reported by Frank Ortega */
        subColor.pixel = (unsigned long) bestmatch;
        subColor.red = ctable[bestmatch].red;
        subColor.green = ctable[bestmatch].green;
        subColor.blue = ctable[bestmatch].blue;
        subColor.flags = DoRed | DoGreen | DoBlue;
    }
    *color = subColor;
}

#elif defined(TOGL_WGL)

static UINT
Win32AllocColor(const Togl *togl, float red, float green, float blue)
{
    /* Modified version of XAllocColor emulation of Tk. - returns index,
     * instead of color itself - allocates logical palette entry even for
     * non-palette devices */

    TkWinColormap *cmap = (TkWinColormap *) Tk_Colormap(togl->TkWin);
    UINT    index;
    COLORREF newColor, closeColor;
    PALETTEENTRY entry, closeEntry;
    int     isNew, refCount;
    Tcl_HashEntry *entryPtr;

    entry.peRed = (unsigned char) (red * 255 + .5);
    entry.peGreen = (unsigned char) (green * 255 + .5);
    entry.peBlue = (unsigned char) (blue * 255 + .5);
    entry.peFlags = 0;

    /* 
     * Find the nearest existing palette entry.
     */

    newColor = RGB(entry.peRed, entry.peGreen, entry.peBlue);
    index = GetNearestPaletteIndex(cmap->palette, newColor);
    GetPaletteEntries(cmap->palette, index, 1, &closeEntry);
    closeColor = RGB(closeEntry.peRed, closeEntry.peGreen, closeEntry.peBlue);

    /* 
     * If this is not a duplicate and colormap is not full, allocate a new entry.
     */

    if (newColor != closeColor) {
        if (cmap->size == (unsigned int) togl->CiColormapSize) {
            entry = closeEntry;
        } else {
            cmap->size++;
            ResizePalette(cmap->palette, cmap->size);
            index = cmap->size - 1;
            SetPaletteEntries(cmap->palette, index, 1, &entry);
            SelectPalette(togl->tglGLHdc, cmap->palette, TRUE);
            RealizePalette(togl->tglGLHdc);
        }
    }
    newColor = PALETTERGB(entry.peRed, entry.peGreen, entry.peBlue);
    entryPtr = Tcl_CreateHashEntry(&cmap->refCounts,
            (CONST char *) newColor, &isNew);
    if (isNew) {
        refCount = 1;
    } else {
        refCount = ((int) Tcl_GetHashValue(entryPtr)) + 1;
    }
    Tcl_SetHashValue(entryPtr, (ClientData) refCount);

    /* for color index mode photos */
    togl->RedMap[index] = (GLfloat) (entry.peRed / 255.0);
    togl->GreenMap[index] = (GLfloat) (entry.peGreen / 255.0);
    togl->BlueMap[index] = (GLfloat) (entry.peBlue / 255.0);
    return index;
}

static void
Win32FreeColor(const Togl *togl, unsigned long index)
{
    TkWinColormap *cmap = (TkWinColormap *) Tk_Colormap(togl->TkWin);
    COLORREF cref;
    UINT    count, refCount;
    PALETTEENTRY entry, *entries;
    Tcl_HashEntry *entryPtr;

    if (index >= cmap->size) {
        panic("Tried to free a color that isn't allocated.");
    }
    GetPaletteEntries(cmap->palette, index, 1, &entry);

    cref = PALETTERGB(entry.peRed, entry.peGreen, entry.peBlue);
    entryPtr = Tcl_FindHashEntry(&cmap->refCounts, (CONST char *) cref);
    if (!entryPtr) {
        panic("Tried to free a color that isn't allocated.");
    }
    refCount = (int) Tcl_GetHashValue(entryPtr) - 1;
    if (refCount == 0) {
        count = cmap->size - index;
        entries = (PALETTEENTRY *) ckalloc(sizeof (PALETTEENTRY) * count);
        GetPaletteEntries(cmap->palette, index + 1, count, entries);
        SetPaletteEntries(cmap->palette, index, count, entries);
        SelectPalette(togl->tglGLHdc, cmap->palette, TRUE);
        RealizePalette(togl->tglGLHdc);
        ckfree((char *) entries);
        cmap->size--;
        Tcl_DeleteHashEntry(entryPtr);
    } else {
        Tcl_SetHashValue(entryPtr, (ClientData) refCount);
    }
}

static void
Win32SetColor(const Togl *togl,
        unsigned long index, float red, float green, float blue)
{
    TkWinColormap *cmap = (TkWinColormap *) Tk_Colormap(togl->TkWin);
    PALETTEENTRY entry;

    entry.peRed = (unsigned char) (red * 255 + .5);
    entry.peGreen = (unsigned char) (green * 255 + .5);
    entry.peBlue = (unsigned char) (blue * 255 + .5);
    entry.peFlags = 0;
    SetPaletteEntries(cmap->palette, index, 1, &entry);
    SelectPalette(togl->tglGLHdc, cmap->palette, TRUE);
    RealizePalette(togl->tglGLHdc);

    /* for color index mode photos */
    togl->RedMap[index] = (GLfloat) (entry.peRed / 255.0);
    togl->GreenMap[index] = (GLfloat) (entry.peGreen / 255.0);
    togl->BlueMap[index] = (GLfloat) (entry.peBlue / 255.0);
}
#endif /* TOGL_X11 */


unsigned long
Togl_AllocColor(const Togl *togl, float red, float green, float blue)
{
    if (togl->RgbaFlag) {
        (void) fprintf(stderr,
                "Error: Togl_AllocColor illegal in RGBA mode.\n");
        return 0;
    }
    /* TODO: maybe not... */
    if (togl->PrivateCmapFlag) {
        (void) fprintf(stderr,
                "Error: Togl_AllocColor illegal with private colormap\n");
        return 0;
    }
#if defined(TOGL_X11)
    {
        XColor  xcol;
        int     exact;

        xcol.red = (short) (red * 65535.0);
        xcol.green = (short) (green * 65535.0);
        xcol.blue = (short) (blue * 65535.0);

        noFaultXAllocColor(Tk_Display(togl->TkWin), Tk_Colormap(togl->TkWin),
                Tk_Visual(togl->TkWin)->map_entries, &xcol, &exact);
        /* for color index mode photos */
        togl->RedMap[xcol.pixel] = (float) xcol.red / 65535.0;
        togl->GreenMap[xcol.pixel] = (float) xcol.green / 65535.0;
        togl->BlueMap[xcol.pixel] = (float) xcol.blue / 65535.0;

        return xcol.pixel;
    }

#elif defined(TOGL_WGL)
    return Win32AllocColor(togl, red, green, blue);

#elif defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
    /* still need to implement this on Mac... */
    return 0;

#endif
}



void
Togl_FreeColor(const Togl *togl, unsigned long pixel)
{
    if (togl->RgbaFlag) {
        (void) fprintf(stderr, "Error: Togl_FreeColor illegal in RGBA mode.\n");
        return;
    }
    /* TODO: maybe not... */
    if (togl->PrivateCmapFlag) {
        (void) fprintf(stderr,
                "Error: Togl_FreeColor illegal with private colormap\n");
        return;
    }
#if defined(TOGL_X11)
    (void) XFreeColors(Tk_Display(togl->TkWin), Tk_Colormap(togl->TkWin),
            &pixel, 1, 0);
#elif defined(TOGL_WGL)
    Win32FreeColor(togl, pixel);
#endif
}



void
Togl_SetColor(const Togl *togl,
        unsigned long index, float red, float green, float blue)
{

    if (togl->RgbaFlag) {
        (void) fprintf(stderr, "Error: Togl_SetColor illegal in RGBA mode.\n");
        return;
    }
    if (!togl->PrivateCmapFlag) {
        (void) fprintf(stderr,
                "Error: Togl_SetColor requires a private colormap\n");
        return;
    }
#if defined(TOGL_X11)
    {
        XColor  xcol;

        xcol.pixel = index;
        xcol.red = (short) (red * 65535.0);
        xcol.green = (short) (green * 65535.0);
        xcol.blue = (short) (blue * 65535.0);
        xcol.flags = DoRed | DoGreen | DoBlue;

        (void) XStoreColor(Tk_Display(togl->TkWin), Tk_Colormap(togl->TkWin),
                &xcol);

        /* for color index mode photos */
        togl->RedMap[xcol.pixel] = (float) xcol.red / 65535.0;
        togl->GreenMap[xcol.pixel] = (float) xcol.green / 65535.0;
        togl->BlueMap[xcol.pixel] = (float) xcol.blue / 65535.0;
    }
#elif defined(TOGL_WGL)
    Win32SetColor(togl, index, red, green, blue);
#endif
}


Tcl_Obj *
Togl_LoadBitmapFont(const Togl *togl, const char *fontname)
{
    return NULL;
}

int
Togl_UnloadBitmapFont(const Togl *togl, Tcl_Obj *bitmapfont)
{
    return TCL_OK;
}

int
Togl_WriteObj(const Togl *togl, const Tcl_Obj *toglfont, Tcl_Obj *obj)
{
    return -1;
}

int
Togl_WriteChars(const Togl *togl, const Tcl_Obj *toglfont, const char *str,
        int len)
{
    return -1;
}


/* 
 * Overlay functions
 */


void
Togl_UseLayer(Togl *togl, int layer)
{
    if (layer == TOGL_NORMAL) {
#if defined(TOGL_WGL)
        int     res = wglMakeCurrent(togl->tglGLHdc, togl->Ctx);

        if (!res) {
            ErrorExit(TEXT("wglMakeCurrent"));
        }
#elif defined(TOGL_X11)
        (void) glXMakeCurrent(Tk_Display(togl->TkWin),
                Tk_WindowId(togl->TkWin), togl->Ctx);
#elif defined(TOGL_AGL)
        (void) aglSetCurrentContext(togl->Ctx);
#elif defined(TOGL_NSOPENGL)
        [togl->Ctx makeCurrentContext];
#endif
    } else if (layer == TOGL_OVERLAY && togl->OverlayWindow) {
#if defined(TOGL_WGL)
        int     res = wglMakeCurrent(togl->tglGLHdc, togl->tglGLOverlayHglrc);

        if (!res) {
            ErrorExit(TEXT("wglMakeCurrent overlay"));
        }
#elif defined(TOGL_X11)
        (void) glXMakeCurrent(Tk_Display(togl->TkWin),
                togl->OverlayWindow, togl->OverlayCtx);
#elif defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
#endif
    } else {
        /* error */
    }
}


void
Togl_ShowOverlay(Togl *togl)
{
#if defined(TOGL_X11)           /* not yet implemented on Windows */
    if (togl->OverlayWindow) {
        (void) XMapWindow(Tk_Display(togl->TkWin), togl->OverlayWindow);
        (void) XInstallColormap(Tk_Display(togl->TkWin), togl->OverlayCmap);
        togl->OverlayIsMapped = True;
    }
#endif
}


void
Togl_HideOverlay(Togl *togl)
{
    if (togl->OverlayWindow && togl->OverlayIsMapped) {
        (void) XUnmapWindow(Tk_Display(togl->TkWin), togl->OverlayWindow);
        togl->OverlayIsMapped = False;
    }
}


void
Togl_PostOverlayRedisplay(Togl *togl)
{
    if (!togl->OverlayUpdatePending
            && togl->OverlayWindow && togl->OverlayDisplayProc) {
        Tk_DoWhenIdle(Togl_RenderOverlay, (ClientData) togl);
        togl->OverlayUpdatePending = True;
    }
}


int
Togl_ExistsOverlay(const Togl *togl)
{
    return togl->OverlayFlag;
}


int
Togl_GetOverlayTransparentValue(const Togl *togl)
{
    return togl->OverlayTransparentPixel;
}


int
Togl_IsMappedOverlay(const Togl *togl)
{
    return togl->OverlayFlag && togl->OverlayIsMapped;
}


unsigned long
Togl_AllocColorOverlay(const Togl *togl, float red, float green, float blue)
{
#if defined(TOGL_X11)           /* not yet implemented on Windows */
    if (togl->OverlayFlag && togl->OverlayCmap) {
        XColor  xcol;

        xcol.red = (short) (red * 65535.0);
        xcol.green = (short) (green * 65535.0);
        xcol.blue = (short) (blue * 65535.0);
        if (!XAllocColor(Tk_Display(togl->TkWin), togl->OverlayCmap, &xcol))
            return (unsigned long) -1;
        return xcol.pixel;
    }
#endif /* TOGL_X11 */
    return (unsigned long) -1;
}


void
Togl_FreeColorOverlay(const Togl *togl, unsigned long pixel)
{
#if defined(TOGL_X11)           /* not yet implemented on Windows */
    if (togl->OverlayFlag && togl->OverlayCmap) {
        (void) XFreeColors(Tk_Display(togl->TkWin), togl->OverlayCmap, &pixel,
                1, 0);
    }
#endif /* TOGL_X11 */
}


/* 
 * User client data
 */

ClientData
Togl_GetClientData(const Togl *togl)
{
    return togl->Client_Data;
}


void
Togl_SetClientData(Togl *togl, ClientData clientData)
{
    togl->Client_Data = clientData;
}

int
Togl_CopyContext(const Togl *from, const Togl *to, unsigned mask)
{
#ifdef TOGL_X11
    int     error_code;
    int     same = (glXGetCurrentContext() == to->Ctx);

    if (same)
        (void) glXMakeCurrent(to->display, None, NULL);
    togl_SetupXErrorHandler();
    glXCopyContext(from->display, from->Ctx, to->Ctx, mask);
    if (error_code = togl_CheckForXError(from)) {
        char    buf[256];

        XGetErrorText(from->display, error_code, buf, sizeof buf);
        Tcl_AppendResult(from->Interp, "unable to copy context: ", buf, NULL);
        return TCL_ERROR;
    }
#elif defined(TOGL_WGL)
    int     same = (wglGetCurrentContext() == to->Ctx);

    if (same)
        (void) wglMakeCurrent(to->tglGLHdc, NULL);
    if (!wglCopyContext(from->Ctx, to->Ctx, mask)) {
        char buf[256];

        snprintf(buf, sizeof buf, "unable to copy context: %d", GetLastError());
        Tcl_AppendElement(from->Interp, buf);
        return TCL_ERROR;
    }
#elif defined(TOGL_AGL)
    int     same = (aglGetCurrentContext() == to->Ctx);

    if (same)
        (void) aglSetCurrentContext(NULL);
    if (!aglCopyContext(from->Ctx, to->Ctx, mask)) {
        Tcl_AppendResult(from->Interp, "unable to copy context: ",
                aglErrorString(aglGetError()), NULL);
        return TCL_ERROR;
    }
#elif defined(TOGL_NSOPENGL)
    int     same = (from->Ctx == to->Ctx);

    if (same) {
      [NSOpenGLContext clearCurrentContext];
    }
    [to->Ctx copyAttributesFromContext:from->Ctx withMask:mask];
#endif
    if (same)
        Togl_MakeCurrent(to);
    return TCL_OK;
}


#ifdef MESA_COLOR_HACK
/* 
 * Let's know how many free colors do we have
 */
#  define RLEVELS     5
#  define GLEVELS     9
#  define BLEVELS     5

/* to free dithered_rgb_colormap pixels allocated by Mesa */
static unsigned long *ToglMesaUsedPixelCells = NULL;
static int ToglMesaUsedFreeCells = 0;

static int
get_free_color_cells(Display *display, int screen, Colormap colormap)
{
    if (!ToglMesaUsedPixelCells) {
        XColor  xcol;
        int     i;
        int     colorsfailed, ncolors = XDisplayCells(display, screen);

        long    r, g, b;

        ToglMesaUsedPixelCells =
                (unsigned long *) ckalloc(ncolors * sizeof (unsigned long));

        /* Allocate X colors and initialize color_table[], red_table[], etc */
        /* de Mesa 2.1: xmesa1.c setup_dithered_(...) */
        i = colorsfailed = 0;
        for (r = 0; r < RLEVELS; r++)
            for (g = 0; g < GLEVELS; g++)
                for (b = 0; b < BLEVELS; b++) {
                    int     exact;

                    xcol.red = (r * 65535) / (RLEVELS - 1);
                    xcol.green = (g * 65535) / (GLEVELS - 1);
                    xcol.blue = (b * 65535) / (BLEVELS - 1);
                    noFaultXAllocColor(display, colormap, ncolors,
                            &xcol, &exact);
                    ToglMesaUsedPixelCells[i++] = xcol.pixel;
                    if (!exact) {
                        colorsfailed++;
                    }
                }
        ToglMesaUsedFreeCells = i;

        XFreeColors(display, colormap, ToglMesaUsedPixelCells,
                ToglMesaUsedFreeCells, 0x00000000);
    }
    return ToglMesaUsedFreeCells;
}


static void
free_default_color_cells(Display *display, Colormap colormap)
{
    if (ToglMesaUsedPixelCells) {
        XFreeColors(display, colormap, ToglMesaUsedPixelCells,
                ToglMesaUsedFreeCells, 0x00000000);
        ckfree((char *) ToglMesaUsedPixelCells);
        ToglMesaUsedPixelCells = NULL;
        ToglMesaUsedFreeCells = 0;
    }
}
#endif

/* 
 * Original stereo code contributed by Ben Evans (Ben.Evans@anusf.anu.edu.au)
 * and was based on SGI's /usr/share/src/OpenGL/teach/stereo/glwstereo.c,
 * which is identical to the 1997/12/1 glwstereo.c code in the CrystalEyes
 * Software Development Kit.
 */

int
Togl_NumEyes(const Togl *togl)
{
    if (togl->Stereo > TOGL_STEREO_ONE_EYE_MAX)
        return 2;
    return 1;
}

/* call instead of glDrawBuffer */
void
Togl_DrawBuffer(Togl *togl, GLenum mode)
{
    if (togl->Stereo <= TOGL_STEREO_ONE_EYE_MAX) {
        /* Only drawing a single eye */
        if (togl->currentStereoBuffer != STEREO_BUFFER_NONE) {
            glViewport(0, 0, togl->Width, togl->Height);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            togl->currentStereoBuffer = STEREO_BUFFER_NONE;
        }
        switch (mode) {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
              break;
          case GL_LEFT:
          case GL_FRONT_LEFT:
          case GL_RIGHT:
          case GL_FRONT_RIGHT:
              mode = GL_FRONT;
              break;
          case GL_BACK_LEFT:
          case GL_BACK_RIGHT:
              mode = GL_BACK;
              break;
          default:
              break;
        }
        glDrawBuffer(mode);
        return;
    }
    /* called once for each eye */
    switch (mode) {
      case GL_FRONT:
      case GL_BACK:
      case GL_FRONT_AND_BACK:
          /* 
           ** Simultaneous drawing to both left and right buffers isn't
           ** really possible if we don't have a stereo capable visual.
           ** For now just fall through and use the left buffer.
           */
      case GL_LEFT:
      case GL_FRONT_LEFT:
      case GL_BACK_LEFT:
          togl->currentStereoBuffer = STEREO_BUFFER_LEFT;
          break;
      case GL_RIGHT:
      case GL_FRONT_RIGHT:
      case GL_BACK_RIGHT:
          togl->currentStereoBuffer = STEREO_BUFFER_RIGHT;
          break;
      default:
          break;
    }
    if (togl->Stereo != TOGL_STEREO_NATIVE) {
        switch (mode) {
          default:
              mode = GL_FRONT;
              break;
          case GL_BACK:
          case GL_BACK_LEFT:
          case GL_BACK_RIGHT:
              mode = GL_BACK;
              break;
        }
    }
    switch (togl->Stereo) {
      default:
          break;
#ifdef __sgi
      case TOGL_STEREO_SGIOLDSTYLE:
          glXWaitGL();          /* sync with GL command stream before calling X 
                                 */
          XSGISetStereoBuffer(togl->display, Tk_WindowId(togl->TkWin),
                  togl->currentStereoBuffer);
          glXWaitX();           /* sync with X command stream before calling GL 
                                 */
          break;
#endif
      case TOGL_STEREO_ANAGLYPH:
          if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
              glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
          else
              glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
          glViewport(0, 0, togl->Width, togl->Height);
          break;
      case TOGL_STEREO_CROSS_EYE:
          if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
              glViewport(togl->Width / 2 + 1, 0, togl->Width / 2, togl->Height);
          else
              glViewport(0, 0, togl->Width / 2, togl->Height);
          break;
      case TOGL_STEREO_WALL_EYE:
          if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
              glViewport(0, 0, togl->Width / 2, togl->Height);
          else
              glViewport(togl->Width / 2 + 1, 0, togl->Width / 2, togl->Height);
          break;
      case TOGL_STEREO_DTI:
          if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
              glViewport(0, 0, togl->Width / 2, togl->Height);
          else
              glViewport(togl->Width / 2 + 1, 0, togl->Width / 2, togl->Height);
          break;
      case TOGL_STEREO_ROW_INTERLEAVED:
          glViewport(0, 0, togl->Width, togl->Height);
          break;
    }
    glDrawBuffer(mode);
}

/* call instead of glClear */
void
Togl_Clear(const Togl *togl, GLbitfield mask)
{
    GLint   stencil_write_mask = 0;
    GLint   stencil_clear_value = 0;

    switch (togl->Stereo) {
      default:
          break;
      case TOGL_STEREO_CROSS_EYE:
      case TOGL_STEREO_WALL_EYE:
      case TOGL_STEREO_DTI:
          if (togl->currentStereoBuffer != STEREO_BUFFER_LEFT) {
              /* Since glViewport does not affect what is cleared (unlike
               * glScissor), only clear in left eye */
              return;
          }
          break;
      case TOGL_STEREO_ROW_INTERLEAVED:
          if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT) {
              if ((mask & GL_STENCIL_BUFFER_BIT) == 0) {
                  mask |= GL_STENCIL_BUFFER_BIT;
                  glStencilMask(~0u);
                  glClearStencil(0);
              } else {
                  glGetIntegerv(GL_STENCIL_WRITEMASK, &stencil_write_mask);
                  glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil_clear_value);
                  glStencilMask(stencil_write_mask | togl->riStencilBit);
                  glClearStencil(stencil_clear_value & ~togl->riStencilBit);
              }
          } else {
              mask &= ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          }
          break;
    }
#if 0
    /* only needed if we wish to support multi-eye clears */
    if (togl->Stereo > TOGL_STEREO_ONE_EYE_MAX) {
        GLenum  drawBuffer = togl->currentDrawBuffer;

        switch (drawBuffer) {
          case GL_FRONT:
              Togl_DrawBuffer(togl, GL_FRONT_RIGHT);
              glClear(mask);
              Togl_DrawBuffer(togl, drawBuffer);
              break;
          case GL_BACK:
              Togl_DrawBuffer(togl, GL_BACK_RIGHT);
              glClear(mask);
              Togl_DrawBuffer(togl, drawBuffer);
              break;
          case GL_FRONT_AND_BACK:
              Togl_DrawBuffer(togl, GL_RIGHT);
              glClear(mask);
              Togl_DrawBuffer(togl, drawBuffer);
              break;
          case GL_LEFT:
          case GL_FRONT_LEFT:
          case GL_BACK_LEFT:
          case GL_RIGHT:
          case GL_FRONT_RIGHT:
          case GL_BACK_RIGHT:
          default:
              break;
        }
    }
#endif
    if (mask != 0)
        glClear(mask);
    if (togl->Stereo == TOGL_STEREO_ROW_INTERLEAVED) {
        int     x, y;

        if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT) {
            int     i;

            /* initialize stencil buffer mask */
            glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT
                    | GL_LINE_BIT | GL_VIEWPORT_BIT);
            // 2d projection
            glViewport(0, 0, togl->Width, togl->Height);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, togl->Width, 0, togl->Height, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glTranslatef(0.375f, 0.375f, 0);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_COLOR_LOGIC_OP);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_DITHER);
            glDisable(GL_INDEX_LOGIC_OP);
            glDisable(GL_LIGHTING);
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_MULTISAMPLE);
            glLineWidth(1);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glStencilFunc(GL_ALWAYS, togl->riStencilBit, togl->riStencilBit);
            glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
            glBegin(GL_LINES);
            for (i = 0; i < togl->Height; i += 2) {
                glVertex2i(0, i);
                glVertex2i(togl->Width, i);
            }
            glEnd();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glPopAttrib();

            if (stencil_write_mask) {
                glStencilMask(stencil_write_mask & ~togl->riStencilBit);
            } else {
                glStencilMask(~togl->riStencilBit);
            }

            Tk_GetRootCoords(togl->TkWin, &x, &y);
            if ((y + togl->Height) % 2) {
                glStencilFunc(GL_NOTEQUAL, togl->riStencilBit,
                        togl->riStencilBit);
            } else {
                glStencilFunc(GL_EQUAL, togl->riStencilBit, togl->riStencilBit);
            }
        } else {
            Tk_GetRootCoords(togl->TkWin, &x, &y);
            if ((y + togl->Height) % 2) {
                glStencilFunc(GL_EQUAL, togl->riStencilBit, togl->riStencilBit);
            } else {
                glStencilFunc(GL_NOTEQUAL, togl->riStencilBit,
                        togl->riStencilBit);
            }
        }
    }
}

/*
 * Togl_Frustum and Togl_Ortho:
 *
 *     eyeOffset is the distance from the center line
 *     and is negative for the left eye and positive for right eye.
 *     eyeDist and eyeOffset need to be in the same units as your model space.
 *     In physical space, eyeDist might be 30 inches from the screen
 *     and eyeDist would be +/- 1.25 inch (for a total interocular distance
 *     of 2.5 inches).
 */

void
Togl_Frustum(const Togl *togl, GLdouble left, GLdouble right,
        GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    GLdouble eyeOffset = 0, eyeShift = 0;

    if (togl->Stereo == TOGL_STEREO_LEFT_EYE
            || togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
        eyeOffset = -togl->EyeSeparation / 2;   /* for left eye */
    else if (togl->Stereo == TOGL_STEREO_RIGHT_EYE
            || togl->currentStereoBuffer == STEREO_BUFFER_RIGHT)
        eyeOffset = togl->EyeSeparation / 2;    /* for right eye */
    eyeShift = (togl->Convergence - zNear) * (eyeOffset / togl->Convergence);

    /* compenstate for altered viewports */
    switch (togl->Stereo) {
      default:
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
      case TOGL_STEREO_DTI:
          /* squished image is expanded, nothing needed */
          break;
      case TOGL_STEREO_CROSS_EYE:
      case TOGL_STEREO_WALL_EYE:{
          GLdouble delta = (top - bottom) / 2;

          top += delta;
          bottom -= delta;
          break;
      }
    }

    glFrustum(left + eyeShift, right + eyeShift, bottom, top, zNear, zFar);
    glTranslated(-eyeShift, 0, 0);
}

void
Togl_Ortho(const Togl *togl, GLdouble left, GLdouble right,
        GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    /* TODO: debug this */
    GLdouble eyeOffset = 0, eyeShift = 0;

    if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
        eyeOffset = -togl->EyeSeparation / 2;   /* for left eye */
    else if (togl->currentStereoBuffer == STEREO_BUFFER_RIGHT)
        eyeOffset = togl->EyeSeparation / 2;    /* for right eye */
    eyeShift = (togl->Convergence - zNear) * (eyeOffset / togl->Convergence);

    /* compenstate for altered viewports */
    switch (togl->Stereo) {
      default:
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
      case TOGL_STEREO_DTI:
          /* squished image is expanded, nothing needed */
          break;
      case TOGL_STEREO_CROSS_EYE:
      case TOGL_STEREO_WALL_EYE:{
          GLdouble delta = (top - bottom) / 2;

          top += delta;
          bottom -= delta;
          break;
      }
    }

    glOrtho(left + eyeShift, right + eyeShift, bottom, top, zNear, zFar);
    glTranslated(-eyeShift, 0, 0);
}

int
Togl_GetToglFromObj(Tcl_Interp *interp, Tcl_Obj *obj, Togl **toglPtr)
{
    Tcl_Command toglCmd;
    Tcl_CmdInfo info;

    toglCmd = Tcl_GetCommandFromObj(interp, obj);
    if (Tcl_GetCommandInfoFromToken(toglCmd, &info) == 0
            || info.objProc != Togl_ObjWidget) {
        Tcl_AppendResult(interp, "expected togl command argument", NULL);
        return TCL_ERROR;
    }
    *toglPtr = (Togl *) info.objClientData;
    return TCL_OK;
}

int
Togl_GetToglFromName(Tcl_Interp *interp, const char *cmdName, Togl **toglPtr)
{
    Tcl_CmdInfo info;

    if (Tcl_GetCommandInfo(interp, cmdName, &info) == 0
            || info.objProc != Togl_ObjWidget) {
        Tcl_AppendResult(interp, "expected togl command argument", NULL);
        return TCL_ERROR;
    }
    *toglPtr = (Togl *) info.objClientData;
    return TCL_OK;
}

static int ObjectIsEmpty(Tcl_Obj *objPtr);

/* 
 *----------------------------------------------------------------------
 *
 * GetStereo -
 *
 *      Converts an internal int into a a Tcl string obj.
 *
 * Results:
 *      Tcl_Obj containing the string representation of the stereo value.
 *
 * Side effects:
 *      Creates a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
GetStereo(ClientData clientData, Tk_Window tkwin, char *recordPtr,
        int internalOffset)
    /* recordPtr is a pointer to widget record. */
    /* internalOffset is the offset within *recordPtr containing the stereo
     * value. */
{
    int     stereo = *(int *) (recordPtr + internalOffset);
    const char *name = "unknown";

    switch (stereo) {
      case TOGL_STEREO_NONE:
          name = "";
          break;
      case TOGL_STEREO_LEFT_EYE:
          name = "left eye";
          break;
      case TOGL_STEREO_RIGHT_EYE:
          name = "right eye";
          break;
      case TOGL_STEREO_NATIVE:
          name = "native";
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
          name = "sgioldstyle";
          break;
      case TOGL_STEREO_ANAGLYPH:
          name = "anaglyph";
          break;
      case TOGL_STEREO_CROSS_EYE:
          name = "cross-eye";
          break;
      case TOGL_STEREO_WALL_EYE:
          name = "wall-eye";
          break;
      case TOGL_STEREO_DTI:
          name = "dti";
          break;
      case TOGL_STEREO_ROW_INTERLEAVED:
          name = "row interleaved";
          break;
    }
    return Tcl_NewStringObj(name, -1);
}

/* 
 *----------------------------------------------------------------------
 *
 * SetStereo --
 *
 *      Converts a Tcl_Obj representing a widgets stereo into an
 *      integer value.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      May store the integer value into the internal representation
 *      pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *      that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
SetStereo(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
        Tcl_Obj **value, char *recordPtr, int internalOffset,
        char *oldInternalPtr, int flags)
    /* interp is the current interp; may be used for errors. */
    /* tkwin is the Window for which option is being set. */
    /* value is a pointer to the pointer to the value object. We use a pointer
     * to the pointer because we may need to return a value (NULL). */
    /* recordPtr is a pointer to storage for the widget record. */
    /* internalOffset is the offset within *recordPtr at which the internal
     * value is to be stored. */
    /* oldInternalPtr is a pointer to storage for the old value. */
    /* flags are the flags for the option, set Tk_SetOptions. */
{
    int     stereo = 0;
    char   *string, *internalPtr;

    internalPtr = (internalOffset > 0) ? recordPtr + internalOffset : NULL;

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(*value)) {
        *value = NULL;
    } else {
        /* 
         * Convert the stereo specifier into an integer value.
         */

        if (Tcl_GetBooleanFromObj(NULL, *value, &stereo) == TCL_OK) {
            stereo = stereo ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
        } else {
            string = Tcl_GetString(*value);

            if (strcmp(string, "") == 0 || strcasecmp(string, "none") == 0) {
                stereo = TOGL_STEREO_NONE;
            } else if (strcasecmp(string, "native") == 0) {
                stereo = TOGL_STEREO_NATIVE;
                /* check if available when creating visual */
            } else if (strcasecmp(string, "left eye") == 0) {
                stereo = TOGL_STEREO_LEFT_EYE;
            } else if (strcasecmp(string, "right eye") == 0) {
                stereo = TOGL_STEREO_RIGHT_EYE;
            } else if (strcasecmp(string, "sgioldstyle") == 0) {
                stereo = TOGL_STEREO_SGIOLDSTYLE;
            } else if (strcasecmp(string, "anaglyph") == 0) {
                stereo = TOGL_STEREO_ANAGLYPH;
            } else if (strcasecmp(string, "cross-eye") == 0) {
                stereo = TOGL_STEREO_CROSS_EYE;
            } else if (strcasecmp(string, "wall-eye") == 0) {
                stereo = TOGL_STEREO_WALL_EYE;
            } else if (strcasecmp(string, "dti") == 0) {
                stereo = TOGL_STEREO_DTI;
            } else if (strcasecmp(string, "row interleaved") == 0) {
                stereo = TOGL_STEREO_ROW_INTERLEAVED;
            } else {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, "bad stereo value \"",
                        Tcl_GetString(*value), "\"", NULL);
                return TCL_ERROR;
            }
        }
    }

    if (internalPtr != NULL) {
        *((int *) oldInternalPtr) = *((int *) internalPtr);
        *((int *) internalPtr) = stereo;
    }
    return TCL_OK;
}

/* 
 *----------------------------------------------------------------------
 * RestoreStereo --
 *
 *      Restore a stereo option value from a saved value.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
RestoreStereo(ClientData clientData, Tk_Window tkwin, char *internalPtr,
        char *oldInternalPtr)
{
    *(int *) internalPtr = *(int *) oldInternalPtr;
}

/* 
 *----------------------------------------------------------------------
 *
 * GetWideInt -
 *
 *      Converts an internal wide integer into a a Tcl WideInt obj.
 *
 * Results:
 *      Tcl_Obj containing the wide int value.
 *
 * Side effects:
 *      Creates a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
GetWideInt(ClientData clientData, Tk_Window tkwin, char *recordPtr,
        int internalOffset)
    /* recordPtr is a pointer to widget record. */
    /* internalOffset is the offset within *recordPtr containing the wide int
     * value. */
{
    Tcl_WideInt wi = *(Tcl_WideInt *) (recordPtr + internalOffset);

    return Tcl_NewWideIntObj(wi);
}

/* 
 *----------------------------------------------------------------------
 *
 * SetWideInt --
 *
 *      Converts a Tcl_Obj representing a Tcl_WideInt.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      May store the wide int value into the internal representation
 *      pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *      that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
SetWideInt(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
        Tcl_Obj **value, char *recordPtr, int internalOffset,
        char *oldInternalPtr, int flags)
    /* interp is the current interp; may be used for errors. */
    /* tkwin is the Window for which option is being set. */
    /* value is a pointer to the pointer to the value object. We use a pointer
     * to the pointer because we may need to return a value (NULL). */
    /* recordPtr is a pointer to storage for the widget record. */
    /* internalOffset is the offset within *recordPtr at which the internal
     * value is to be stored. */
    /* oldInternalPtr is a pointer to storage for the old value. */
    /* flags are the flags for the option, set Tk_SetOptions. */
{
    char   *internalPtr;
    Tcl_WideInt w;

    internalPtr = (internalOffset > 0) ? recordPtr + internalOffset : NULL;

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(*value)) {
        *value = NULL;
        w = 0;
    } else {
        if (Tcl_GetWideIntFromObj(interp, *value, &w) != TCL_OK) {
            return TCL_ERROR;
        }
    }

    if (internalPtr != NULL) {
        *((Tcl_WideInt *) oldInternalPtr) = *((Tcl_WideInt *) internalPtr);
        *((Tcl_WideInt *) internalPtr) = w;
    }
    return TCL_OK;
}

/* 
 *----------------------------------------------------------------------
 * RestoreWideInt --
 *
 *      Restore a wide int option value from a saved value.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
RestoreWideInt(ClientData clientData, Tk_Window tkwin, char *internalPtr,
        char *oldInternalPtr)
{
    *(Tcl_WideInt *) internalPtr = *(Tcl_WideInt *) oldInternalPtr;
}

/* 
 *----------------------------------------------------------------------
 *
 * ObjectIsEmpty --
 *
 *      This procedure tests whether the string value of an object is
 *      empty.
 *
 * Results:
 *      The return value is 1 if the string value of objPtr has length
 *      zero, and 0 otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ObjectIsEmpty(Tcl_Obj *objPtr)
/* objPtr = Object to test.  May be NULL. */
{
    int     length;

    if (objPtr == NULL) {
        return 1;
    }
    if (objPtr->bytes != NULL) {
        return (objPtr->length == 0);
    }
    Tcl_GetStringFromObj(objPtr, &length);
    return (length == 0);
}
