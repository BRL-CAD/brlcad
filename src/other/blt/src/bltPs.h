/*
 * bltPs.h --
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

#ifndef _BLT_PS_H
#define _BLT_PS_H

#include "bltImage.h"

typedef enum {
    PS_MODE_MONOCHROME,
    PS_MODE_GREYSCALE,
    PS_MODE_COLOR
} PsColorMode;

typedef struct PsTokenStruct *PsToken;

struct PsTokenStruct {
    Tcl_Interp *interp;		/* Interpreter to report errors to. */

    Tk_Window tkwin;		/* Tk_Window used to get font and color
				 * information */

    Tcl_DString dString;	/* Dynamic string used to contain the
				 * PostScript generated. */

    char *fontVarName;		/* Name of a Tcl array variable to convert
				 * X font names to PostScript fonts. */

    char *colorVarName;		/* Name of a Tcl array variable to convert
				 * X color names to PostScript. */

    PsColorMode colorMode;	/* Mode: color or greyscale */

#define PSTOKEN_BUFSIZ	((BUFSIZ*2)-1)
    /*
     * Utility space for building strings.  Currently used to create
     * PostScript output for the "postscript" command.
     */
    char scratchArr[PSTOKEN_BUFSIZ+1];
};

extern PsToken Blt_GetPsToken _ANSI_ARGS_((Tcl_Interp *interp, 
			   Tk_Window tkwin));

extern void Blt_ReleasePsToken _ANSI_ARGS_((PsToken psToken));

extern char *Blt_PostScriptFromToken _ANSI_ARGS_((PsToken psToken));
extern char *Blt_ScratchBufferFromToken _ANSI_ARGS_((PsToken psToken));

extern void Blt_AppendToPostScript _ANSI_ARGS_(TCL_VARARGS(PsToken, psToken));

extern void Blt_FormatToPostScript _ANSI_ARGS_(TCL_VARARGS(PsToken, psToken));

extern void Blt_Draw3DRectangleToPostScript _ANSI_ARGS_((PsToken psToken,
	Tk_3DBorder border, double x, double y, int width, int height,
	int borderWidth, int relief));

extern void Blt_Fill3DRectangleToPostScript _ANSI_ARGS_((PsToken psToken,
	Tk_3DBorder border, double x, double y, int width, int height,
	int borderWidth, int relief));

extern void Blt_BackgroundToPostScript _ANSI_ARGS_((PsToken psToken,
	XColor *colorPtr));

extern void Blt_BitmapDataToPostScript _ANSI_ARGS_((PsToken psToken,
	Display *display, Pixmap bitmap, int width, int height));

extern void Blt_ClearBackgroundToPostScript _ANSI_ARGS_((PsToken psToken));

extern int Blt_ColorImageToPsData _ANSI_ARGS_((Blt_ColorImage image,
	int nComponents, Tcl_DString * resultPtr, char *prefix));

extern void Blt_ColorImageToPostScript _ANSI_ARGS_((PsToken psToken,
	Blt_ColorImage image, double x, double y));

extern void Blt_ForegroundToPostScript _ANSI_ARGS_((PsToken psToken,
	XColor *colorPtr));

extern void Blt_FontToPostScript _ANSI_ARGS_((PsToken psToken, Tk_Font font));

extern void Blt_WindowToPostScript _ANSI_ARGS_((PsToken psToken,
	Tk_Window tkwin, double x, double y));

extern void Blt_LineDashesToPostScript _ANSI_ARGS_((PsToken psToken,
	Blt_Dashes *dashesPtr));

extern void Blt_LineWidthToPostScript _ANSI_ARGS_((PsToken psToken,
	int lineWidth));

extern void Blt_PathToPostScript _ANSI_ARGS_((PsToken psToken,
	Point2D *screenPts, int nScreenPts));

extern void Blt_PhotoToPostScript _ANSI_ARGS_((PsToken psToken,
	Tk_PhotoHandle photoToken, double x, double y));

extern void Blt_PolygonToPostScript _ANSI_ARGS_((PsToken psToken,
	Point2D *screenPts, int nScreenPts));

extern void Blt_LineToPostScript _ANSI_ARGS_((PsToken psToken, 
	XPoint *pointArr, int nPoints));

extern void Blt_TextToPostScript _ANSI_ARGS_((PsToken psToken, char *string,
	TextStyle *attrPtr, double x, double y));

extern void Blt_RectangleToPostScript _ANSI_ARGS_((PsToken psToken, double x,
	double y, int width, int height));

extern void Blt_RegionToPostScript _ANSI_ARGS_((PsToken psToken, double x,
	double y, int width, int height));

extern void Blt_RectanglesToPostScript _ANSI_ARGS_((PsToken psToken,
	XRectangle *rectArr, int nRects));

extern void Blt_BitmapToPostScript _ANSI_ARGS_((PsToken psToken, 
	Display *display, Pixmap bitmap, double scaleX, double scaleY));

extern void Blt_SegmentsToPostScript _ANSI_ARGS_((PsToken psToken,
	XSegment *segArr, int nSegs));

extern void Blt_StippleToPostScript _ANSI_ARGS_((PsToken psToken,
	Display *display, Pixmap bitmap));

extern void Blt_LineAttributesToPostScript _ANSI_ARGS_((PsToken psToken,
	XColor *colorPtr, int lineWidth, Blt_Dashes *dashesPtr, int capStyle,
	int joinStyle));

extern int Blt_FileToPostScript _ANSI_ARGS_((PsToken psToken,
	char *fileName));

extern void Blt_2DSegmentsToPostScript _ANSI_ARGS_((PsToken psToken, 
	Segment2D *segments, int nSegments));

#endif /* BLT_PS_H */
