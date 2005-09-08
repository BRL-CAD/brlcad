
/*
 * bltWin.h --
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

#ifndef _BLT_WIN_H
#define _BLT_WIN_H

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef STRICT
#undef WIN32_LEAN_AND_MEAN
#include <windowsx.h>


#undef STD_NORMAL_BACKGROUND
#undef STD_NORMAL_FOREGROUND
#undef STD_SELECT_BACKGROUND
#undef STD_SELECT_FOREGROUND
#undef STD_TEXT_FOREGROUND
#undef STD_FONT
#undef STD_FONT_LARGE
#undef STD_FONT_SMALL

#define STD_NORMAL_BACKGROUND	"SystemButtonFace"
#define STD_NORMAL_FOREGROUND	"SystemButtonText"
#define STD_SELECT_BACKGROUND	"SystemHighlight"
#define STD_SELECT_FOREGROUND	"SystemHighlightText"
#define STD_TEXT_FOREGROUND	"SystemWindowText"
#define STD_FONT		"Arial 8"
#define STD_FONT_LARGE		"Arial 12"
#define STD_FONT_SMALL		"Arial 6"

#ifdef CHECK_UNICODE_CALLS
#define _UNICODE
#define UNICODE
#define __TCHAR_DEFINED
typedef float *_TCHAR;
#define _TCHAR_DEFINED
typedef float *TCHAR;
#endif /* CHECK_UNICODE_CALLS */

/* DOS Encapsulated PostScript File Header */
#pragma pack(2)
typedef struct {
    BYTE magic[4];		/* Magic number for a DOS EPS file
				 * C5,D0,D3,C6 */
    DWORD psStart;		/* Offset of PostScript section. */
    DWORD psLength;		/* Length of the PostScript section. */
    DWORD wmfStart;		/* Offset of Windows Meta File section. */
    DWORD wmfLength;		/* Length of Meta file section. */
    DWORD tiffStart;		/* Offset of TIFF section. */
    DWORD tiffLength;		/* Length of TIFF section. */
    WORD checksum;		/* Checksum of header. If FFFF, ignore. */
} DOSEPSHEADER;
#pragma pack()

/* Aldus Portable Metafile Header */
#pragma pack(2)
typedef struct {
    DWORD key;			/* Type of metafile */
    WORD hmf;			/* Unused. Must be NULL. */
    SMALL_RECT bbox;		/* Bounding rectangle */
    WORD inch;			/* Units per inch. */
    DWORD reserved;		/* Unused. */
    WORD checksum;		/* XOR of previous fields (10 32-bit words). */
} APMHEADER;
#pragma pack()

extern double hypot(double x, double y);
extern int Blt_AsyncRead(int fd, char *buffer, unsigned int size);
extern int Blt_AsyncWrite(int fd, char *buffer, unsigned int size);
extern void Blt_CreateFileHandler(int fd, int flags, Tcl_FileProc * proc,
    ClientData clientData);
extern void Blt_DeleteFileHandler(int fd);
extern int Blt_GetPlatformId(void);
extern char *Blt_LastError(void);
extern int Blt_GetOpenPrinter(Tcl_Interp *interp, const char *id,
    Drawable *drawablePtr);
extern int Blt_PrintDialog(Tcl_Interp *interp, Drawable *drawablePtr);
extern int Blt_OpenPrinterDoc(Tcl_Interp *interp, const char *id);
extern int Blt_ClosePrinterDoc(Tcl_Interp *interp, const char *id);
extern void Blt_GetPrinterScale(HDC dc, double *xRatio, double *yRatio);
extern int Blt_StartPrintJob(Tcl_Interp *interp, Drawable drawable);
extern int Blt_EndPrintJob(Tcl_Interp *interp, Drawable drawable);

#undef EXPORT
#define EXPORT __declspec(dllexport)

#ifdef _MSC_VER
#define strncasecmp(s1,s2,n)	_strnicmp(s1,s2,n)
#define strcasecmp(s1,s2)	_stricmp(s1,s2)
#define isnan(x)		_isnan(x)
#endif /* _MSC_VER */

#ifdef __BORLANDC__
#define isnan(x)		_isnan(x)
#endif

#if defined(__BORLANDC__) || defined(_MSC_VER)
#ifdef FINITE
#undef FINITE
#define FINITE(x)		_finite(x)
#endif
#endif /* __BORLANDC__ || _MSC_VER */

#ifdef __GNUC__ 
#include <wingdi.h>
#include <windowsx.h>
#undef Status
#include <winspool.h>
#define Status int
/*
 * Add definitions missing from windgi.h, windowsx.h, and winspool.h
 */
#include <missing.h> 
#endif /* __GNUC__ */

#define XCopyArea		Blt_EmulateXCopyArea
#define XCopyPlane		Blt_EmulateXCopyPlane
#define XDrawArcs		Blt_EmulateXDrawArcs
#define XDrawLine		Blt_EmulateXDrawLine
#define XDrawLines		Blt_EmulateXDrawLines
#define XDrawPoints		Blt_EmulateXDrawPoints
#define XDrawRectangle		Blt_EmulateXDrawRectangle
#define XDrawRectangles		Blt_EmulateXDrawRectangles
#define XDrawSegments		Blt_EmulateXDrawSegments
#define XDrawString		Blt_EmulateXDrawString
#define XFillArcs		Blt_EmulateXFillArcs
#define XFillPolygon		Blt_EmulateXFillPolygon
#define XFillRectangle		Blt_EmulateXFillRectangle
#define XFillRectangles		Blt_EmulateXFillRectangles
#define XFree			Blt_EmulateXFree
#define XGetWindowAttributes	Blt_EmulateXGetWindowAttributes
#define XLowerWindow		Blt_EmulateXLowerWindow
#define XMaxRequestSize		Blt_EmulateXMaxRequestSize
#define XRaiseWindow		Blt_EmulateXRaiseWindow
#define XReparentWindow		Blt_EmulateXReparentWindow
#define XSetDashes		Blt_EmulateXSetDashes
#define XUnmapWindow		Blt_EmulateXUnmapWindow
#define XWarpPointer		Blt_EmulateXWarpPointer

EXTERN GC Blt_EmulateXCreateGC(Display *display, Drawable drawable,
    unsigned long mask, XGCValues *valuesPtr);
EXTERN void Blt_EmulateXCopyArea(Display *display, Drawable src, Drawable dest,
    GC gc, int src_x, int src_y, unsigned int width, unsigned int height,
    int dest_x, int dest_y);
EXTERN void Blt_EmulateXCopyPlane(Display *display, Drawable src,
    Drawable dest, GC gc, int src_x, int src_y, unsigned int width,
    unsigned int height, int dest_x, int dest_y, unsigned long plane);
EXTERN void Blt_EmulateXDrawArcs(Display *display, Drawable drawable, GC gc,
    XArc *arcArr, int nArcs);
EXTERN void Blt_EmulateXDrawLine(Display *display, Drawable drawable, GC gc,
    int x1, int y1, int x2, int y2);
EXTERN void Blt_EmulateXDrawLines(Display *display, Drawable drawable, GC gc,
    XPoint *pointArr, int nPoints, int mode);
EXTERN void Blt_EmulateXDrawPoints(Display *display, Drawable drawable, GC gc,
    XPoint *pointArr, int nPoints, int mode);
EXTERN void Blt_EmulateXDrawRectangle(Display *display, Drawable drawable,
    GC gc, int x, int y, unsigned int width, unsigned int height);
EXTERN void Blt_EmulateXDrawRectangles(Display *display, Drawable drawable,
    GC gc, XRectangle *rectArr, int nRects);
EXTERN void Blt_EmulateXDrawSegments(Display *display, Drawable drawable,
    GC gc, XSegment *segArr, int nSegments);
EXTERN void Blt_EmulateXDrawSegments(Display *display, Drawable drawable,
    GC gc, XSegment *segArr, int nSegments);
EXTERN void Blt_EmulateXDrawString(Display *display, Drawable drawable, GC gc,
    int x, int y, _Xconst char *string, int length);
EXTERN void Blt_EmulateXFillArcs(Display *display, Drawable drawable, GC gc,
    XArc *arcArr, int nArcs);
EXTERN void Blt_EmulateXFillPolygon(Display *display, Drawable drawable,
    GC gc, XPoint *points, int nPoints,  int shape, int mode);
EXTERN void Blt_EmulateXFillRectangle(Display *display, Drawable drawable,
    GC gc, int x, int y, unsigned int width, unsigned int height);
EXTERN void Blt_EmulateXFillRectangles(Display *display, Drawable drawable,
    GC gc, XRectangle *rectArr, int nRects);
EXTERN void Blt_EmulateXFree(void *ptr);
EXTERN int Blt_EmulateXGetWindowAttributes(Display *display, Window window,
    XWindowAttributes * attrsPtr);
EXTERN void Blt_EmulateXLowerWindow(Display *display, Window window);
EXTERN void Blt_EmulateXMapWindow(Display *display, Window window);
EXTERN long Blt_EmulateXMaxRequestSize(Display *display);
EXTERN void Blt_EmulateXRaiseWindow(Display *display, Window window);
EXTERN void Blt_EmulateXReparentWindow(Display *display, Window window,
    Window parent, int x, int y);
EXTERN void Blt_EmulateXSetDashes(Display *display, GC gc, int dashOffset,
    _Xconst char *dashList, int n);
EXTERN void Blt_EmulateXUnmapWindow(Display *display, Window window);
EXTERN void Blt_EmulateXWarpPointer(Display *display, Window srcWindow,
    Window destWindow, int srcX, int srcY, unsigned int srcWidth,
    unsigned int srcHeight, int destX, int destY);

EXTERN void Blt_DrawLine2D(Display *display, Drawable drawable, GC gc,
    POINT *screenPts, int nScreenPts);

extern unsigned char *Blt_GetBitmapData _ANSI_ARGS_((Display *display,
	Pixmap bitmap, int width, int height, int *pitchPtr));

extern HFONT Blt_CreateRotatedFont _ANSI_ARGS_((Tk_Window tkwin,
	unsigned long font, double theta));

extern HPALETTE Blt_GetSystemPalette _ANSI_ARGS_((void));

extern HPEN Blt_GCToPen _ANSI_ARGS_((HDC dc, GC gc));

#endif /*_BLT_WIN_H*/
