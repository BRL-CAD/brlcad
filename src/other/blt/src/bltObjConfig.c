/* 
 * bltObjConfig.c --
 *
 *	This file contains the Tk_ConfigureWidget procedure. THIS FILE
 *	IS HERE FOR BACKWARD COMPATIBILITY; THE NEW CONFIGURATION
 *	PACKAGE SHOULD BE USED FOR NEW PROJECTS.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "bltInt.h"
#if (TK_VERSION_NUMBER >= _VERSION(8,0,0))
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <ctype.h>
#include "bltObjConfig.h"
#include "bltTile.h"

#if (TK_VERSION_NUMBER < _VERSION(8,1,0))
/*
 *----------------------------------------------------------------------
 *
 * Tk_GetAnchorFromObj --
 *
 *	Return a Tk_Anchor value based on the value of the objPtr.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	The object gets converted by Tcl_GetIndexFromObj.
 *
 *----------------------------------------------------------------------
 */
int
Tk_GetAnchorFromObj(interp, objPtr, anchorPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *objPtr;		/* The object we are trying to get the 
				 * value from. */
    Tk_Anchor *anchorPtr;	/* Where to place the Tk_Anchor that
				 * corresponds to the string value of
				 * objPtr. */
{
    return Tk_GetAnchor(interp, Tcl_GetString(objPtr), anchorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetJustifyFromObj --
 *
 *	Return a Tk_Justify value based on the value of the objPtr.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	The object gets converted by Tcl_GetIndexFromObj.
 *
 *----------------------------------------------------------------------
 */
int
Tk_GetJustifyFromObj(interp, objPtr, justifyPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *objPtr;		/* The object we are trying to get the 
				 * value from. */
    Tk_Justify *justifyPtr;	/* Where to place the Tk_Justify that
				 * corresponds to the string value of
				 * objPtr. */
{
    return Tk_GetJustify(interp, Tcl_GetString(objPtr), justifyPtr);
}
/*
 *----------------------------------------------------------------------
 *
 * Tk_GetReliefFromObj --
 *
 *	Return an integer value based on the value of the objPtr.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	The object gets converted by Tcl_GetIndexFromObj.
 *
 *----------------------------------------------------------------------
 */
int
Tk_GetReliefFromObj(interp, objPtr, reliefPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *objPtr;		/* The object we are trying to get the 
				 * value from. */
    int *reliefPtr;		/* Where to place the answer. */
{
    return Tk_GetRelief(interp, Tcl_GetString(objPtr), reliefPtr);
}
/*
 *----------------------------------------------------------------------
 *
 * Tk_GetMMFromObj --
 *
 *	Attempt to return an mm value from the Tcl object "objPtr". If the
 *	object is not already an mm value, an attempt will be made to convert
 *	it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already a pixel, the conversion will free
 *	any old internal representation. 
 *
 *----------------------------------------------------------------------
 */
int
Tk_GetMMFromObj(interp, tkwin, objPtr, doublePtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    Tk_Window tkwin;
    Tcl_Obj *objPtr;		/* The object from which to get mms. */
    double *doublePtr;		/* Place to store resulting millimeters. */
{
    return Tk_GetScreenMM(interp, tkwin, Tcl_GetString(objPtr), doublePtr);
}
/*
 *----------------------------------------------------------------------
 *
 * Tk_GetPixelsFromObj --
 *
 *	Attempt to return a pixel value from the Tcl object "objPtr". If the
 *	object is not already a pixel value, an attempt will be made to convert
 *	it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already a pixel, the conversion will free
 *	any old internal representation. 
 *
 *----------------------------------------------------------------------
 */
int
Tk_GetPixelsFromObj(interp, tkwin, objPtr, intPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    Tk_Window tkwin;
    Tcl_Obj *objPtr;		/* The object from which to get pixels. */
    int *intPtr;		/* Place to store resulting pixels. */
{
    return Tk_GetPixels(interp, tkwin, Tcl_GetString(objPtr), intPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_Alloc3DBorderFromObj --
 *
 *	Given a Tcl_Obj *, map the value to a corresponding
 *	Tk_3DBorder structure based on the tkwin given.
 *
 * Results:
 *	The return value is a token for a data structure describing a
 *	3-D border.  This token may be passed to procedures such as
 *	Blt_Draw3DRectangle and Tk_Free3DBorder.  If an error prevented
 *	the border from being created then NULL is returned and an error
 *	message will be left in the interp's result.
 *
 * Side effects:
 *	The border is added to an internal database with a reference
 *	count. For each call to this procedure, there should eventually
 *	be a call to FreeBorderObjProc so that the database is
 *	cleaned up when borders aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */
Tk_3DBorder
Tk_Alloc3DBorderFromObj(interp, tkwin, objPtr)
    Tcl_Interp *interp;		/* Interp for error results. */
    Tk_Window tkwin;		/* Need the screen the border is used on.*/
    Tcl_Obj *objPtr;		/* Object giving name of color for window
				 * background. */
{
    return Tk_Get3DBorder(interp, tkwin, Tcl_GetString(objPtr));
}
/*
 *----------------------------------------------------------------------
 *
 * Tk_AllocBitmapFromObj --
 *
 *	Given a Tcl_Obj *, map the value to a corresponding
 *	Pixmap structure based on the tkwin given.
 *
 * Results:
 *	The return value is the X identifer for the desired bitmap
 *	(i.e. a Pixmap with a single plane), unless string couldn't be
 *	parsed correctly.  In this case, None is returned and an error
 *	message is left in the interp's result.  The caller should never
 *	modify the bitmap that is returned, and should eventually call
 *	Tk_FreeBitmapFromObj when the bitmap is no longer needed.
 *
 * Side effects:
 *	The bitmap is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeBitmapFromObj, so that the database can be cleaned up 
 *	when bitmaps aren't needed anymore.
 *
 *----------------------------------------------------------------------
 */
Pixmap
Tk_AllocBitmapFromObj(interp, tkwin, objPtr)
    Tcl_Interp *interp;		/* Interp for error results. This may 
				 * be NULL. */
    Tk_Window tkwin;		/* Need the screen the bitmap is used on.*/
    Tcl_Obj *objPtr;		/* Object describing bitmap; see manual
				 * entry for legal syntax of string value. */
{
    return Tk_GetBitmap(interp, tkwin, Tcl_GetString(objPtr));
}

/*
 *---------------------------------------------------------------------------
 *
 * Tk_AllocFontFromObj -- 
 *
 *	Given a string description of a font, map the description to a
 *	corresponding Tk_Font that represents the font.
 *
 * Results:
 *	The return value is token for the font, or NULL if an error
 *	prevented the font from being created.  If NULL is returned, an
 *	error message will be left in interp's result object.
 *
 * Side effects:
 * 	The font is added to an internal database with a reference
 *	count.  For each call to this procedure, there should eventually
 *	be a call to Tk_FreeFont() or Tk_FreeFontFromObj() so that the
 *	database is cleaned up when fonts aren't in use anymore.
 *
 *---------------------------------------------------------------------------
 */
Tk_Font
Tk_AllocFontFromObj(interp, tkwin, objPtr)
    Tcl_Interp *interp;		/* Interp for database and error return. */
    Tk_Window tkwin;		/* For screen on which font will be used. */
    Tcl_Obj *objPtr;		/* Object describing font, as: named font,
				 * native format, or parseable string. */
{
    return Tk_GetFont(interp, tkwin, Tcl_GetString(objPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_AllocCursorFromObj --
 *
 *	Given a Tcl_Obj *, map the value to a corresponding
 *	Tk_Cursor structure based on the tkwin given.
 *
 * Results:
 *	The return value is the X identifer for the desired cursor,
 *	unless objPtr couldn't be parsed correctly.  In this case,
 *	None is returned and an error message is left in the interp's result.
 *	The caller should never modify the cursor that is returned, and
 *	should eventually call Tk_FreeCursorFromObj when the cursor is no 
 *	longer needed.
 *
 * Side effects:
 *	The cursor is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeCursorFromObj, so that the database can be cleaned up 
 *	when cursors aren't needed anymore.
 *
 *----------------------------------------------------------------------
 */
Tk_Cursor
Tk_AllocCursorFromObj(interp, tkwin, objPtr)
    Tcl_Interp *interp;		/* Interp for error results. */
    Tk_Window tkwin;		/* Window in which the cursor will be used.*/
    Tcl_Obj *objPtr;		/* Object describing cursor; see manual
				 * entry for description of legal
				 * syntax of this obj's string rep. */
{
    return Tk_GetCursor(interp, tkwin, Tcl_GetString(objPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_AllocColorFromObj --
 *
 *	Given a Tcl_Obj *, map the value to a corresponding
 *	XColor structure based on the tkwin given.
 *
 * Results:
 *	The return value is a pointer to an XColor structure that
 *	indicates the red, blue, and green intensities for the color
 *	given by the string in objPtr, and also specifies a pixel value 
 *	to use to draw in that color.  If an error occurs, NULL is 
 *	returned and an error message will be left in interp's result
 *	(unless interp is NULL).
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColorFromObj so that the database is cleaned up when colors
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */
XColor *
Tk_AllocColorFromObj(interp, tkwin, objPtr)
    Tcl_Interp *interp;		/* Used only for error reporting.  If NULL,
				 * then no messages are provided. */
    Tk_Window tkwin;		/* Window in which the color will be used.*/
    Tcl_Obj *objPtr;		/* Object that describes the color; string
				 * value is a color name such as "red" or
				 * "#ff0000".*/
{
    char *string;

    string = Tcl_GetString(objPtr);
    return Tk_GetColor(interp, tkwin, Tk_GetUid(string));
}

#endif /* 8.0 */

/*
 *--------------------------------------------------------------
 *
 * Blt_GetPosition --
 *
 *	Convert a string representing a numeric position.
 *	A position can be in one of the following forms.
 *
 * 	  number	- number of the item in the hierarchy, indexed
 *			  from zero.
 *	  "end"		- last position in the hierarchy.
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the corresponding numeric index.
 *	If "end" was selected then *indexPtr is set to -1.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
int
Blt_GetPositionFromObj(interp, objPtr, indexPtr)
    Tcl_Interp *interp;		/* Interpreter to report results back
				 * to. */
    Tcl_Obj *objPtr;		/* Tcl_Obj representation of the index.
				 * Can be an integer or "end" to refer
				 * to the last index. */
    int *indexPtr;		/* Holds the converted index. */
{
    char *string;

    string = Tcl_GetString(objPtr);
    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
	*indexPtr = -1;		/* Indicates last position in hierarchy. */
    } else {
	int position;

	if (Tcl_GetIntFromObj(interp, objPtr, &position) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (position < 0) {
	    Tcl_AppendResult(interp, "bad position \"", string, "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	*indexPtr = position;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetPixelsFromObj --
 *
 *	Like Tk_GetPixelsFromObj, but checks for negative, zero.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetPixelsFromObj(interp, tkwin, objPtr, check, valuePtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Tcl_Obj *objPtr;
    int check;			/* Can be PIXELS_POSITIVE, PIXELS_NONNEGATIVE,
				 * or PIXELS_ANY, */
    int *valuePtr;
{
    int length;

    if (Tk_GetPixelsFromObj(interp, tkwin, objPtr, &length) != TCL_OK) {
	return TCL_ERROR;
    }
    if (length >= SHRT_MAX) {
	Tcl_AppendResult(interp, "bad distance \"", Tcl_GetString(objPtr), 
		 "\": too big to represent", (char *)NULL);
	return TCL_ERROR;
    }
    switch (check) {
    case PIXELS_NONNEGATIVE:
	if (length < 0) {
	    Tcl_AppendResult(interp, "bad distance \"", Tcl_GetString(objPtr), 
		     "\": can't be negative", (char *)NULL);
	    return TCL_ERROR;
	}
	break;

    case PIXELS_POSITIVE:
	if (length <= 0) {
	    Tcl_AppendResult(interp, "bad distance \"", Tcl_GetString(objPtr), 
		     "\": must be positive", (char *)NULL);
	    return TCL_ERROR;
	}
	break;

    case PIXELS_ANY:
	break;
    }
    *valuePtr = length;
    return TCL_OK;
}

int
Blt_GetPadFromObj(interp, tkwin, objPtr, padPtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    Tcl_Obj *objPtr;		/* Pixel value string */
    Blt_Pad *padPtr;
{
    int side1, side2;
    int objc;
    Tcl_Obj **objv;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((objc < 1) || (objc > 2)) {
	Tcl_AppendResult(interp, "wrong # elements in padding list",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (Blt_GetPixelsFromObj(interp, tkwin, objv[0], PIXELS_NONNEGATIVE, 
	     &side1) != TCL_OK) {
	return TCL_ERROR;
    }
    side2 = side1;
    if ((objc > 1) && 
	(Blt_GetPixelsFromObj(interp, tkwin, objv[1], PIXELS_NONNEGATIVE, 
	      &side2) != TCL_OK)) {
	return TCL_ERROR;
    }
    /* Don't update the pad structure until we know both values are okay. */
    padPtr->side1 = side1;
    padPtr->side2 = side2;
    return TCL_OK;
}

int
Blt_GetShadowFromObj(interp, tkwin, objPtr, shadowPtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    Tcl_Obj *objPtr;		/* Pixel value string */
    Shadow *shadowPtr;
{
    XColor *colorPtr;
    int dropOffset;
    int objc;
    Tcl_Obj **objv;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # elements in drop shadow value",
			 (char *)NULL);
	return TCL_ERROR;
    }
    dropOffset = 0;
    colorPtr = NULL;
    if (objc > 0) {
	colorPtr = Tk_AllocColorFromObj(interp, tkwin, objv[0]);
	if (colorPtr == NULL) {
	    return TCL_ERROR;
	}
	dropOffset = 1;
	if (objc == 2) {
	    if (Blt_GetPixelsFromObj(interp, tkwin, objv[1], PIXELS_NONNEGATIVE,
				     &dropOffset) != TCL_OK) {
		Tk_FreeColor(colorPtr);
		return TCL_ERROR;
	    }
	}
    }
    if (shadowPtr->color != NULL) {
	Tk_FreeColor(shadowPtr->color);
    }
    shadowPtr->color = colorPtr;
    shadowPtr->offset = dropOffset;
    return TCL_OK;
}

int
Blt_GetStateFromObj(interp, objPtr, statePtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tcl_Obj *objPtr;		/* Pixel value string */
    int *statePtr;
{
    char *string;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "normal") == 0) {
	*statePtr = STATE_NORMAL;
    } else if (strcmp(string, "disabled") == 0) {
	*statePtr = STATE_DISABLED;
    } else if (strcmp(string, "active") == 0) {
	*statePtr = STATE_ACTIVE;
    } else {
	Tcl_AppendResult(interp, "bad state \"", string,
	    "\": should be normal, active, or disabled", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

char *
Blt_NameOfState(state)
    int state;
{
    switch (state) {
    case STATE_ACTIVE:
	return "active";
    case STATE_DISABLED:
	return "disabled";
    case STATE_NORMAL:
	return "normal";
    default:
	return "???";
    }
}

#ifdef notdef			/* Replace this routine when Tcl_Obj-based
				 * configuration comes on-line */

/*
 *----------------------------------------------------------------------
 *
 * Blt_NameOfFill --
 *
 *	Converts the integer representing the fill style into a string.
 *
 *----------------------------------------------------------------------
 */
char *
Blt_NameOfFill(fill)
    int fill;
{
    switch (fill) {
    case FILL_X:
	return "x";
    case FILL_Y:
	return "y";
    case FILL_NONE:
	return "none";
    case FILL_BOTH:
	return "both";
    default:
	return "unknown value";
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetFillFromObj --
 *
 *	Converts the fill style string into its numeric representation.
 *
 *	Valid style strings are:
 *
 *	  "none"   Use neither plane.
 * 	  "x"	   X-coordinate plane.
 *	  "y"	   Y-coordinate plane.
 *	  "both"   Use both coordinate planes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_GetFillFromObj(interp, objPtr, fillPtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tcl_Obj *objPtr;		/* Fill style string */
    int *fillPtr;
{
    int length;
    char c;
    char *string;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	*fillPtr = FILL_NONE;
    } else if ((c == 'x') && (strncmp(string, "x", length) == 0)) {
	*fillPtr = FILL_X;
    } else if ((c == 'y') && (strncmp(string, "y", length) == 0)) {
	*fillPtr = FILL_Y;
    } else if ((c == 'b') && (strncmp(string, "both", length) == 0)) {
	*fillPtr = FILL_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad argument \"", string,
	    "\": should be \"none\", \"x\", \"y\", or \"both\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetDashesFromObj --
 *
 *	Converts a Tcl list of dash values into a dash list ready for
 *	use with XSetDashes.
 *
 * 	A valid list dash values can have zero through 11 elements
 *	(PostScript limit).  Values must be between 1 and 255. Although
 *	a list of 0 (like the empty string) means no dashes.
 *
 * Results:
 *	A standard Tcl result. If the list represented a valid dash
 *	list TCL_OK is returned and *dashesPtr* will contain the
 *	valid dash list. Otherwise, TCL_ERROR is returned and
 *	interp->result will contain an error message.
 *
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetDashesFromObj(interp, objPtr, dashesPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    Blt_Dashes *dashesPtr;
{
    char *string;

    string = Tcl_GetString(objPtr);
    if ((string == NULL) || (*string == '\0')) {
	dashesPtr->values[0] = 0;
    } else if (strcmp(string, "dash") == 0) {	/* 5 2 */
	dashesPtr->values[0] = 5;
	dashesPtr->values[1] = 2;
	dashesPtr->values[2] = 0;
    } else if (strcmp(string, "dot") == 0) {	/* 1 */
	dashesPtr->values[0] = 1;
	dashesPtr->values[1] = 0;
    } else if (strcmp(string, "dashdot") == 0) {	/* 2 4 2 */
	dashesPtr->values[0] = 2;
	dashesPtr->values[1] = 4;
	dashesPtr->values[2] = 2;
	dashesPtr->values[3] = 0;
    } else if (strcmp(string, "dashdotdot") == 0) {	/* 2 4 2 2 */
	dashesPtr->values[0] = 2;
	dashesPtr->values[1] = 4;
	dashesPtr->values[2] = 2;
	dashesPtr->values[3] = 2;
	dashesPtr->values[4] = 0;
    } else {
	int objc;
	Tcl_Obj **objv;
	int value;
	register int i;

	if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc > 11) {	/* This is the postscript limit */
	    Tcl_AppendResult(interp, "too many values in dash list \"", 
			     string, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	for (i = 0; i < objc; i++) {
	    if (Tcl_GetIntFromObj(interp, objv[i], &value) != TCL_OK) {
		return TCL_ERROR;
	    }
	    /*
	     * Backward compatibility:
	     * Allow list of 0 to turn off dashes
	     */
	    if ((value == 0) && (objc == 1)) {
		break;
	    }
	    if ((value < 1) || (value > 255)) {
		Tcl_AppendResult(interp, "dash value \"", 
			 Tcl_GetString(objv[i]), "\" is out of range", 
			 (char *)NULL);
		return TCL_ERROR;
	    }
	    dashesPtr->values[i] = (unsigned char)value;
	}
	/* Make sure the array ends with a NUL byte  */
	dashesPtr->values[i] = 0;
    }
    return TCL_OK;
}

char *
Blt_NameOfSide(side)
    int side;
{
    switch (side) {
    case SIDE_LEFT:
	return "left";
    case SIDE_RIGHT:
	return "right";
    case SIDE_BOTTOM:
	return "bottom";
    case SIDE_TOP:
	return "top";
    }
    return "unknown side value";
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetSideFromObj --
 *
 *	Converts the fill style string into its numeric representation.
 *
 *	Valid style strings are "left", "right", "top", or  "bottom".
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED */
int
Blt_GetSideFromObj(interp, objPtr, sidePtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tcl_Obj *objPtr;		/* Value string */
    int *sidePtr;		/* (out) Token representing side:
				 * either SIDE_LEFT, SIDE_RIGHT,
				 * SIDE_TOP, or SIDE_BOTTOM. */
{
    char c;
    int length;
    char *string;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*sidePtr = SIDE_LEFT;
    } else if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*sidePtr = SIDE_RIGHT;
    } else if ((c == 't') && (strncmp(string, "top", length) == 0)) {
	*sidePtr = SIDE_TOP;
    } else if ((c == 'b') && (strncmp(string, "bottom", length) == 0)) {
	*sidePtr = SIDE_BOTTOM;
    } else {
	Tcl_AppendResult(interp, "bad side \"", string,
	    "\": should be left, right, top, or bottom", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_StringToEnum --
 *
 *	Converts the string into its enumerated type.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_ObjToEnum(clientData, interp, tkwin, objPtr, widgRec, offset)
    ClientData clientData;	/* Vectors of valid strings. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    Tcl_Obj *objPtr;
    char *widgRec;		/* Widget record. */
    int offset;			/* Offset of field in record */
{
    int *enumPtr = (int *)(widgRec + offset);
    char c;
    register char **p;
    register int i;
    int count;
    char *string;

    string = Tcl_GetString(objPtr);
    c = string[0];
    count = 0;
    for (p = (char **)clientData; *p != NULL; p++) {
	if ((c == p[0][0]) && (strcmp(string, *p) == 0)) {
	    *enumPtr = count;
	    return TCL_OK;
	}
	count++;
    }
    *enumPtr = -1;

    Tcl_AppendResult(interp, "bad value \"", string, "\": should be ", 
	(char *)NULL);
    p = (char **)clientData; 
    if (count > 0) {
	Tcl_AppendResult(interp, p[0], (char *)NULL);
    }
    for (i = 1; i < (count - 1); i++) {
	Tcl_AppendResult(interp, " ", p[i], ", ", (char *)NULL);
    }
    if (count > 1) {
	Tcl_AppendResult(interp, " or ", p[count - 1], ".", (char *)NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_EnumToObj --
 *
 *	Returns the string associated with the enumerated type.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
Tcl_Obj *
Blt_EnumToObj(clientData, interp, tkwin, widgRec, offset)
    ClientData clientData;	/* List of strings. */
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in widget record */
{
    int value = *(int *)(widgRec + offset);
    char **strings = (char **)clientData;
    char **p;
    int count;

    count = 0;
    for (p = strings; *p != NULL; p++) {
	if (value == count) {
	    return Tcl_NewStringObj(*p, -1);
	}
	count++;
    }
    return Tcl_NewStringObj("unknown value", -1);
}

/* Configuration option helper routines */

/*
 *--------------------------------------------------------------
 *
 * DoConfig --
 *
 *	This procedure applies a single configuration option
 *	to a widget record.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	WidgRec is modified as indicated by specPtr and value.
 *	The old value is recycled, if that is appropriate for
 *	the value type.
 *
 *--------------------------------------------------------------
 */
static int
DoConfig(interp, tkwin, specPtr, objPtr, widgRec)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window containing widget (needed to
				 * set up X resources). */
    Blt_ConfigSpec *specPtr;	/* Specifier to apply. */
    Tcl_Obj *objPtr;		/* Value to use to fill in widgRec. */
    char *widgRec;		/* Record whose fields are to be
				 * modified.  Values must be properly
				 * initialized. */
{
    char *ptr;
    int objIsEmpty;

    objIsEmpty = FALSE;
    if (objPtr == NULL) {
	objIsEmpty = TRUE;
    } else if (specPtr->specFlags & BLT_CONFIG_NULL_OK) {
	int length;

	if (objPtr->bytes != NULL) {
	    length = objPtr->length;
	} else {
	    Tcl_GetStringFromObj(objPtr, &length);
	}
	objIsEmpty = (length == 0);
    }
    do {
	ptr = widgRec + specPtr->offset;
	switch (specPtr->type) {
	case BLT_CONFIG_ANCHOR: 
	    {
		Tk_Anchor anchor;
		
		if (Tk_GetAnchorFromObj(interp, objPtr, &anchor) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(Tk_Anchor *)ptr = anchor;
	    }
	    break;

	case BLT_CONFIG_BITMAP: 
	    {
		Pixmap newBitmap, oldBitmap;
		
		if (objIsEmpty) {
		    newBitmap = None;
		} else {
		    newBitmap = Tk_AllocBitmapFromObj(interp, tkwin, objPtr);
		    if (newBitmap == None) {
			return TCL_ERROR;
		    }
		}
		oldBitmap = *(Pixmap *)ptr;
		if (oldBitmap != None) {
		    Tk_FreeBitmap(Tk_Display(tkwin), oldBitmap);
		}
		*(Pixmap *)ptr = newBitmap;
	    }
	    break;

	case BLT_CONFIG_BOOLEAN: 
	    {
		int newBool;
		
		if (Tcl_GetBooleanFromObj(interp, objPtr, &newBool) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = newBool;
	    }
	    break;

	case BLT_CONFIG_BORDER: 
	    {
		Tk_3DBorder newBorder, oldBorder;
		
		if (objIsEmpty) {
		    newBorder = NULL;
		} else {
		    newBorder = Tk_Alloc3DBorderFromObj(interp, tkwin, objPtr);
		    if (newBorder == NULL) {
			return TCL_ERROR;
		    }
		}
		oldBorder = *(Tk_3DBorder *)ptr;
		if (oldBorder != NULL) {
		    Tk_Free3DBorder(oldBorder);
		}
		*(Tk_3DBorder *)ptr = newBorder;
	    }
	    break;

	case BLT_CONFIG_CAP_STYLE: 
	    {
		int cap;
		Tk_Uid value;
		
		value = Tk_GetUid(Tcl_GetString(objPtr));
		if (Tk_GetCapStyle(interp, value, &cap) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = cap;
	    }
	    break;

	case BLT_CONFIG_COLOR: 
	    {
		XColor *newColor, *oldColor;
		
		if (objIsEmpty) {
		    newColor = NULL;
		} else {
		    newColor = Tk_AllocColorFromObj(interp, tkwin, objPtr);
		    if (newColor == NULL) {
			return TCL_ERROR;
		    }
		}
		oldColor = *(XColor **)ptr;
		if (oldColor != NULL) {
		    Tk_FreeColor(oldColor);
		}
		*(XColor **)ptr = newColor;
	    }
	    break;

	case BLT_CONFIG_CURSOR:
	case BLT_CONFIG_ACTIVE_CURSOR: 
	    {
		Tk_Cursor newCursor, oldCursor;
		
		if (objIsEmpty) {
		    newCursor = None;
		} else {
		    newCursor = Tk_AllocCursorFromObj(interp, tkwin, objPtr);
		    if (newCursor == None) {
			return TCL_ERROR;
		    }
		}
		oldCursor = *(Tk_Cursor *)ptr;
		if (oldCursor != None) {
		    Tk_FreeCursor(Tk_Display(tkwin), oldCursor);
		}
		*(Tk_Cursor *)ptr = newCursor;
		if (specPtr->type == BLT_CONFIG_ACTIVE_CURSOR) {
		    Tk_DefineCursor(tkwin, newCursor);
		}
	    }
	    break;

	case BLT_CONFIG_CUSTOM: 
	    {
		if ((*(char **)ptr != NULL) && 
		    (specPtr->customPtr->freeProc != NULL)) {
		    (*specPtr->customPtr->freeProc)
			(specPtr->customPtr->clientData, Tk_Display(tkwin), 
			 widgRec, specPtr->offset);
		    *(char **)ptr = NULL;
		}
		if (objIsEmpty) {
		    *(char **)ptr = NULL;
		} else {
		    int result;
		
		    result = (*specPtr->customPtr->parseProc)
			(specPtr->customPtr->clientData, interp, tkwin, objPtr,
			 widgRec, specPtr->offset);
		    if (result != TCL_OK) {
			return TCL_ERROR;
		    }
		}
	    }
	    break;

	case BLT_CONFIG_DOUBLE: 
	    {
		double newDouble;
		
		if (Tcl_GetDoubleFromObj(interp, objPtr, &newDouble) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		*(double *)ptr = newDouble;
	    }
	    break;

	case BLT_CONFIG_FONT: 
	    {
		Tk_Font newFont, oldFont;
		
		if (objIsEmpty) {
		    newFont = NULL;
		} else {
		    newFont = Tk_AllocFontFromObj(interp, tkwin, objPtr);
		    if (newFont == NULL) {
			return TCL_ERROR;
		    }
		}
		oldFont = *(Tk_Font *)ptr;
		if (oldFont != NULL) {
		    Tk_FreeFont(oldFont);
		}
		*(Tk_Font *)ptr = newFont;
	    }
	    break;

	case BLT_CONFIG_INT: 
	    {
		int newInt;
		
		if (Tcl_GetIntFromObj(interp, objPtr, &newInt) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = newInt;
	    }
	    break;

	case BLT_CONFIG_JOIN_STYLE: 
	    {
		int join;
		Tk_Uid value;

		value = Tk_GetUid(Tcl_GetString(objPtr));
		if (Tk_GetJoinStyle(interp, value, &join) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = join;
	    }
	    break;

	case BLT_CONFIG_JUSTIFY: 
	    {
		Tk_Justify justify;
		
		if (Tk_GetJustifyFromObj(interp, objPtr, &justify) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(Tk_Justify *)ptr = justify;
	    }
	    break;

	case BLT_CONFIG_MM: 
	    {
		double mm;

		if (Tk_GetMMFromObj(interp, tkwin, objPtr, &mm) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(double *)ptr = mm;
	    }
	    break;

	case BLT_CONFIG_PIXELS: 
	    {
		int pixels;
		
		if (Tk_GetPixelsFromObj(interp, tkwin, objPtr, &pixels) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = pixels;
	    }
	    break;

	case BLT_CONFIG_RELIEF: 
	    {
		int relief;
		
		if (Tk_GetReliefFromObj(interp, objPtr, &relief) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = relief;
	    }
	    break;

	case BLT_CONFIG_STRING: 
	    {
		char *oldString, *newString;
		
		if (objIsEmpty) {
		    newString = NULL;
		} else {
		    newString = (char *)Blt_Strdup(Tcl_GetString(objPtr));
		}
		oldString = *(char **)ptr;
		if (oldString != NULL) {
		    Blt_Free(oldString);
		}
		*(char **)ptr = newString;
	    }
	    break;

	case BLT_CONFIG_UID: 
	    if (objIsEmpty) {
		*(Tk_Uid *)ptr = NULL;
	    } else {
		*(Tk_Uid *)ptr = Tk_GetUid(Tcl_GetString(objPtr));
	    }
	    break;

	case BLT_CONFIG_WINDOW: 
	    {
		Tk_Window tkwin2;

		if (objIsEmpty) {
		    tkwin2 = None;
		} else {
		    char *path;

		    path = Tcl_GetString(objPtr);
		    tkwin2 = Tk_NameToWindow(interp, path, tkwin);
		    if (tkwin2 == NULL) {
			return TCL_ERROR;
		    }
		}
		*(Tk_Window *)ptr = tkwin2;
	    }
	    break;

	case BLT_CONFIG_BITFLAG: 
	    {
		int bool;
		unsigned int flag;

		
		if (Tcl_GetBooleanFromObj(interp, objPtr, &bool) != TCL_OK) {
		    return TCL_ERROR;
		}
		flag = (unsigned int)specPtr->customPtr;
		*(int *)ptr &= ~flag;
		if (bool) {
		    *(int *)ptr |= flag;
		}
	    }
	    break;

	case BLT_CONFIG_DASHES:
	    if (Blt_GetDashesFromObj(interp, objPtr, (Blt_Dashes *)ptr) 
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    break;

	case BLT_CONFIG_DISTANCE: 
	    {
		int newPixels;
		
		if (Blt_GetPixelsFromObj(interp, tkwin, objPtr, 
			PIXELS_NONNEGATIVE, &newPixels) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = newPixels;
	    }
	    break;

	case BLT_CONFIG_FILL:
	    if (Blt_GetFillFromObj(interp, objPtr, (int *)ptr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;

	case BLT_CONFIG_FLOAT: 
	    {
		double newDouble;
		
		if (Tcl_GetDoubleFromObj(interp, objPtr, &newDouble) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		*(float *)ptr = (float)newDouble;
	    }
	    break;

	case BLT_CONFIG_LIST: 
	    {
		char **argv;
		int argc;
		
		if (Tcl_SplitList(interp, Tcl_GetString(objPtr), &argc, &argv) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		*(char ***)ptr = argv;
	    }
	    break;

	case BLT_CONFIG_LISTOBJ: 
	    {
		Tcl_Obj **objv;
		Tcl_Obj *listObjPtr;
		int objc;
		
		if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
		listObjPtr = Tcl_NewListObj(objc, objv);
		Tcl_IncrRefCount(listObjPtr);
		*(Tcl_Obj **)ptr = listObjPtr;
	    }
	    break;

	case BLT_CONFIG_PAD:
	    if (Blt_GetPadFromObj(interp, tkwin, objPtr, (Blt_Pad *)ptr) 
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    break;

	case BLT_CONFIG_POS_DISTANCE: 
	    {
		int newPixels;
		
		if (Blt_GetPixelsFromObj(interp, tkwin, objPtr, 
			PIXELS_POSITIVE, &newPixels) != TCL_OK) {
		    return TCL_ERROR;
		}
		*(int *)ptr = newPixels;
	    }
	    break;

	case BLT_CONFIG_SHADOW: 
	    {
		Shadow *shadowPtr = (Shadow *)ptr;
		
		if ((shadowPtr != NULL) && (shadowPtr->color != NULL)) {
		    Tk_FreeColor(shadowPtr->color);
		}
		if (Blt_GetShadowFromObj(interp, tkwin, objPtr, shadowPtr) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    break;

	case BLT_CONFIG_STATE: 
	    {
		if (Blt_GetStateFromObj(interp, objPtr, (int *)ptr) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    break;

	case BLT_CONFIG_TILE: 
	    {
		Blt_Tile newTile, oldTile;
		
		if (objIsEmpty) {
		    newTile = None;
		} else {
		    if (Blt_GetTile(interp, tkwin, Tcl_GetString(objPtr), 
				    &newTile) != TCL_OK) {
			return TCL_ERROR;
		    }
		}
		oldTile = *(Blt_Tile *)ptr;
		if (oldTile != NULL) {
		    Blt_FreeTile(oldTile);
		}
		*(Blt_Tile *)ptr = newTile;
	    }
	    break;

	case BLT_CONFIG_SIDE:
	    if (Blt_GetSideFromObj(interp, objPtr, (int *)ptr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;

	default: 
	    {
		char buf[200];
		
		sprintf(buf, "bad config table: unknown type %d", 
			specPtr->type);
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
		return TCL_ERROR;
	    }
	}
	specPtr++;
    } while ((specPtr->switchName == NULL) && 
	     (specPtr->type != BLT_CONFIG_END));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatConfigValue --
 *
 *	This procedure formats the current value of a configuration
 *	option.
 *
 * Results:
 *	The return value is the formatted value of the option given
 *	by specPtr and widgRec.  If the value is static, so that it
 *	need not be freed, *freeProcPtr will be set to NULL;  otherwise
 *	*freeProcPtr will be set to the address of a procedure to
 *	free the result, and the caller must invoke this procedure
 *	when it is finished with the result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *
FormatConfigValue(interp, tkwin, specPtr, widgRec)
    Tcl_Interp *interp;		/* Interpreter for use in real conversions. */
    Tk_Window tkwin;		/* Window corresponding to widget. */
    Blt_ConfigSpec *specPtr;	/* Pointer to information describing option.
				 * Must not point to a synonym option. */
    char *widgRec;		/* Pointer to record holding current
				 * values of info for widget. */
{
    char *ptr;
    char *string;

    ptr = widgRec + specPtr->offset;
    string = "";
    switch (specPtr->type) {
    case BLT_CONFIG_ANCHOR:
	string = Tk_NameOfAnchor(*(Tk_Anchor *)ptr);
	break;

    case BLT_CONFIG_BITMAP: 
	if (*(Pixmap *)ptr != None) {
	    string = Tk_NameOfBitmap(Tk_Display(tkwin), *(Pixmap *)ptr);
	}
	break;

    case BLT_CONFIG_BOOLEAN: 
	return Tcl_NewBooleanObj(*(int *)ptr);

    case BLT_CONFIG_BORDER: 
	if (*(Tk_3DBorder *)ptr != NULL) {
	    string = Tk_NameOf3DBorder(*(Tk_3DBorder *)ptr);
	}
	break;

    case BLT_CONFIG_CAP_STYLE:
	string = Tk_NameOfCapStyle(*(int *)ptr);
	break;

    case BLT_CONFIG_COLOR: 
	if (*(XColor **)ptr != NULL) {
	    string = Tk_NameOfColor(*(XColor **)ptr);
	}
	break;

    case BLT_CONFIG_CURSOR:
    case BLT_CONFIG_ACTIVE_CURSOR:
	if (*(Tk_Cursor *)ptr != None) {
	    string = Tk_NameOfCursor(Tk_Display(tkwin), *(Tk_Cursor *)ptr);
	}
	break;

    case BLT_CONFIG_CUSTOM:
	return (*specPtr->customPtr->printProc)(specPtr->customPtr->clientData,
		interp, tkwin, widgRec, specPtr->offset);

    case BLT_CONFIG_DOUBLE: 
	return Tcl_NewDoubleObj(*(double *)ptr);

    case BLT_CONFIG_FONT: 
	if (*(Tk_Font *)ptr != NULL) {
	    string = Tk_NameOfFont(*(Tk_Font *)ptr);
	}
	break;

    case BLT_CONFIG_INT: 
	return Tcl_NewIntObj(*(int *)ptr);

    case BLT_CONFIG_JOIN_STYLE:
	string = Tk_NameOfJoinStyle(*(int *)ptr);
	break;

    case BLT_CONFIG_JUSTIFY:
	string = Tk_NameOfJustify(*(Tk_Justify *)ptr);
	break;

    case BLT_CONFIG_MM:
	return Tcl_NewDoubleObj(*(double *)ptr);

    case BLT_CONFIG_PIXELS: 
	return Tcl_NewIntObj(*(int *)ptr);

    case BLT_CONFIG_RELIEF: 
	string = Tk_NameOfRelief(*(int *)ptr);
	break;

    case BLT_CONFIG_STRING: 
    case BLT_CONFIG_UID:
	if (*(char **)ptr != NULL) {
	    string = *(char **)ptr;
	}
	break;

    case BLT_CONFIG_BITFLAG:
	{
	    unsigned int flag;

	    flag = (*(int *)ptr) & (unsigned int)specPtr->customPtr;
	    return Tcl_NewBooleanObj((flag != 0));
	}

    case BLT_CONFIG_DASHES: 
	{
	    unsigned char *p;
	    Tcl_Obj *listObjPtr;
	    Blt_Dashes *dashesPtr = (Blt_Dashes *)ptr;
	    
	    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	    for(p = dashesPtr->values; *p != 0; p++) {
		Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(*p));
	    }
	    return listObjPtr;
	}

    case BLT_CONFIG_DISTANCE:
    case BLT_CONFIG_POS_DISTANCE:
	return Tcl_NewIntObj(*(int *)ptr);

    case BLT_CONFIG_FILL: 
	string = Blt_NameOfFill(*(int *)ptr);
	break;

    case BLT_CONFIG_FLOAT: 
	{
	    double x = *(double *)ptr;
	    return Tcl_NewDoubleObj(x);
	}

    case BLT_CONFIG_LIST: 
	{
	    Tcl_Obj *objPtr, *listObjPtr;
	    char **p;
	    
	    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	    for (p = *(char ***)ptr; *p != NULL; p++) {
		objPtr = Tcl_NewStringObj(*p, -1);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	    return listObjPtr;
	}

    case BLT_CONFIG_LISTOBJ: 
	return *(Tcl_Obj **)ptr;

    case BLT_CONFIG_PAD: 
	{
	    Blt_Pad *padPtr = (Blt_Pad *)ptr;
	    Tcl_Obj *objPtr, *listObjPtr;
	    
	    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	    objPtr = Tcl_NewIntObj(padPtr->side1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Tcl_NewIntObj(padPtr->side2);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    return listObjPtr;
	}

    case BLT_CONFIG_SHADOW:
	{
	    Shadow *shadowPtr = (Shadow *)ptr;
	    Tcl_Obj *objPtr, *listObjPtr;

	    if (shadowPtr->color != NULL) {
		listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
		objPtr = Tcl_NewStringObj(Tk_NameOfColor(shadowPtr->color), -1);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		objPtr = Tcl_NewIntObj(shadowPtr->offset);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		return listObjPtr;
	    }
	}

    case BLT_CONFIG_STATE: 
	string = Blt_NameOfState(*(int *)ptr);
	break;

    case BLT_CONFIG_TILE:
	string = Blt_NameOfTile((Blt_Tile)ptr);
	break;
	
    case BLT_CONFIG_SIDE: 
	string = Blt_NameOfSide(*(int *)ptr);
	break;

    default: 
	string = "?? unknown type ??";
    }
    return Tcl_NewStringObj(string, -1);
}

/*
 *--------------------------------------------------------------
 *
 * FormatConfigInfo --
 *
 *	Create a valid Tcl list holding the configuration information
 *	for a single configuration option.
 *
 * Results:
 *	A Tcl list, dynamically allocated.  The caller is expected to
 *	arrange for this list to be freed eventually.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */
static Tcl_Obj *
FormatConfigInfo(interp, tkwin, specPtr, widgRec)
    Tcl_Interp *interp;			/* Interpreter to use for things
					 * like floating-point precision. */
    Tk_Window tkwin;			/* Window corresponding to widget. */
    register Blt_ConfigSpec *specPtr;	/* Pointer to information describing
					 * option. */
    char *widgRec;			/* Pointer to record holding current
					 * values of info for widget. */
{
    Tcl_Obj *objv[5];
    Tcl_Obj *listObjPtr;
    register int i;

    for (i = 0; i < 5; i++) {
	objv[i] = bltEmptyStringObjPtr;
    }
    if (specPtr->switchName != NULL) {
	objv[0] = Tcl_NewStringObj(specPtr->switchName, -1);
    } 
    if (specPtr->dbName != NULL) {
	objv[1] = Tcl_NewStringObj(specPtr->dbName, -1);
    }
    if (specPtr->type == BLT_CONFIG_SYNONYM) {
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	Tcl_ListObjAppendElement(interp, listObjPtr, objv[0]);
	Tcl_ListObjAppendElement(interp, listObjPtr, objv[1]);
	return listObjPtr;
    } 
    if (specPtr->dbClass != NULL) {
	objv[2] = Tcl_NewStringObj(specPtr->dbClass, -1);
    }
    if (specPtr->defValue != NULL) {
	objv[3] = Tcl_NewStringObj(specPtr->defValue, -1);
    }
    objv[4] = FormatConfigValue(interp, tkwin, specPtr, widgRec);
    return Tcl_NewListObj(5, objv);
}

/*
 *--------------------------------------------------------------
 *
 * FindConfigSpec --
 *
 *	Search through a table of configuration specs, looking for
 *	one that matches a given switchName.
 *
 * Results:
 *	The return value is a pointer to the matching entry, or NULL
 *	if nothing matched.  In that case an error message is left
 *	in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
static Blt_ConfigSpec *
FindConfigSpec(interp, specs, objPtr, needFlags, hateFlags)
    Tcl_Interp *interp;		/* Used for reporting errors. */
    Blt_ConfigSpec *specs;	/* Pointer to table of configuration
				 * specifications for a widget. */
    Tcl_Obj *objPtr;		/* Name (suitable for use in a "config"
				 * command) identifying particular option. */
    int needFlags;		/* Flags that must be present in matching
				 * entry. */
    int hateFlags;		/* Flags that must NOT be present in
				 * matching entry. */
{
    register Blt_ConfigSpec *specPtr;
    register char c;		/* First character of current argument. */
    Blt_ConfigSpec *matchPtr;	/* Matching spec, or NULL. */
    int length;
    char *string;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[1];
    matchPtr = NULL;
    for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	if (specPtr->switchName == NULL) {
	    continue;
	}
	if ((specPtr->switchName[1] != c) || 
	    (strncmp(specPtr->switchName, string, length) != 0)) {
	    continue;
	}
	if (((specPtr->specFlags & needFlags) != needFlags) || 
	    (specPtr->specFlags & hateFlags)) {
	    continue;
	}
	if (specPtr->switchName[length] == 0) {
	    matchPtr = specPtr;
	    goto gotMatch;
	}
	if (matchPtr != NULL) {
	    if (interp != NULL) {
	        Tcl_AppendResult(interp, "ambiguous option \"", string, "\"", 
			     (char *)NULL);
            }
	    return (Blt_ConfigSpec *)NULL;
	}
	matchPtr = specPtr;
    }

    if (matchPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "unknown option \"", string, "\"", 
		(char *)NULL);
	}
	return (Blt_ConfigSpec *)NULL;
    }

    /*
     * Found a matching entry.  If it's a synonym, then find the
     * entry that it's a synonym for.
     */

 gotMatch:
    specPtr = matchPtr;
    if (specPtr->type == BLT_CONFIG_SYNONYM) {
	for (specPtr = specs; ; specPtr++) {
	    if (specPtr->type == BLT_CONFIG_END) {
		if (interp != NULL) {
   		    Tcl_AppendResult(interp, 
			"couldn't find synonym for option \"", string, 
			"\"", (char *) NULL);
		}
		return (Blt_ConfigSpec *) NULL;
	    }
	    if ((specPtr->dbName == matchPtr->dbName) && 
		(specPtr->type != BLT_CONFIG_SYNONYM) && 
		((specPtr->specFlags & needFlags) == needFlags) && 
		!(specPtr->specFlags & hateFlags)) {
		break;
	    }
	}
    }
    return specPtr;
}

/* Public routines */

/*
 *--------------------------------------------------------------
 *
 * Blt_ConfigureWidgetFromObj --
 *
 *	Process command-line options and database options to
 *	fill in fields of a widget record with resources and
 *	other parameters.
 *
 * Results:
 *	A standard Tcl return value.  In case of an error,
 *	the interp's result will hold an error message.
 *
 * Side effects:
 *	The fields of widgRec get filled in with information
 *	from argc/argv and the option database.  Old information
 *	in widgRec's fields gets recycled.
 *
 *--------------------------------------------------------------
 */
int
Blt_ConfigureWidgetFromObj(interp, tkwin, specs, objc, objv, widgRec, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window containing widget (needed to
				 * set up X resources). */
    Blt_ConfigSpec *specs;	/* Describes legal options. */
    int objc;			/* Number of elements in argv. */
    Tcl_Obj *CONST *objv;	/* Command-line options. */
    char *widgRec;		/* Record whose fields are to be
				 * modified.  Values must be properly
				 * initialized. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered.  Also,
				 * may have BLT_CONFIG_ARGV_ONLY set. */
{
    register Blt_ConfigSpec *specPtr;
    int needFlags;		/* Specs must contain this set of flags
				 * or else they are not considered. */
    int hateFlags;		/* If a spec contains any bits here, it's
				 * not considered. */

    if (tkwin == NULL) {
	/*
	 * Either we're not really in Tk, or the main window was destroyed and
	 * we're on our way out of the application
	 */
	Tcl_AppendResult(interp, "NULL main window", (char *)NULL);
	return TCL_ERROR;
    }

    needFlags = flags & ~(BLT_CONFIG_USER_BIT - 1);
    if (Tk_Depth(tkwin) <= 1) {
	hateFlags = BLT_CONFIG_COLOR_ONLY;
    } else {
	hateFlags = BLT_CONFIG_MONO_ONLY;
    }

    /*
     * Pass one:  scan through all the option specs, replacing strings
     * with Tk_Uid structs (if this hasn't been done already) and
     * clearing the BLT_CONFIG_OPTION_SPECIFIED flags.
     */

    for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	if (!(specPtr->specFlags & INIT) && (specPtr->switchName != NULL)) {
	    if (specPtr->dbName != NULL) {
		specPtr->dbName = Tk_GetUid(specPtr->dbName);
	    }
	    if (specPtr->dbClass != NULL) {
		specPtr->dbClass = Tk_GetUid(specPtr->dbClass);
	    }
	    if (specPtr->defValue != NULL) {
		specPtr->defValue = Tk_GetUid(specPtr->defValue);
	    }
	}
	specPtr->specFlags = 
	    (specPtr->specFlags & ~BLT_CONFIG_OPTION_SPECIFIED) | INIT;
    }

    /*
     * Pass two:  scan through all of the arguments, processing those
     * that match entries in the specs.
     */
    while (objc > 0) {
	specPtr = FindConfigSpec(interp, specs, objv[0], needFlags, hateFlags);
	if (specPtr == NULL) {
	    return TCL_ERROR;
	}

	/* Process the entry.  */
	if (objc < 2) {
	    Tcl_AppendResult(interp, "value for \"", Tcl_GetString(objv[0]),
		    "\" missing", (char *) NULL);
	    return TCL_ERROR;
	}
	if (DoConfig(interp, tkwin, specPtr, objv[1], widgRec) != TCL_OK) {
	    char msg[100];

	    sprintf(msg, "\n    (processing \"%.40s\" option)",
		    specPtr->switchName);
	    Tcl_AddErrorInfo(interp, msg);
	    return TCL_ERROR;
	}
	specPtr->specFlags |= BLT_CONFIG_OPTION_SPECIFIED;
	objc -= 2, objv += 2;
    }

    /*
     * Pass three:  scan through all of the specs again;  if no
     * command-line argument matched a spec, then check for info
     * in the option database.  If there was nothing in the
     * database, then use the default.
     */

    if (!(flags & BLT_CONFIG_OBJV_ONLY)) {
	Tcl_Obj *objPtr;

	for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	    if ((specPtr->specFlags & BLT_CONFIG_OPTION_SPECIFIED) || 
		(specPtr->switchName == NULL) || 
		(specPtr->type == BLT_CONFIG_SYNONYM)) {
		continue;
	    }
	    if (((specPtr->specFlags & needFlags) != needFlags) || 
		(specPtr->specFlags & hateFlags)) {
		continue;
	    }
	    objPtr = NULL;
	    if (specPtr->dbName != NULL) {
		Tk_Uid value;

		value = Tk_GetOption(tkwin, specPtr->dbName, specPtr->dbClass);
		if (value != NULL) {
		    objPtr = Tcl_NewStringObj(value, -1);
		}
	    }
	    if (objPtr != NULL) {
		if (DoConfig(interp, tkwin, specPtr, objPtr, widgRec) 
		    != TCL_OK) {
		    char msg[200];
    
		    sprintf(msg, "\n    (%s \"%.50s\" in widget \"%.50s\")",
			    "database entry for",
			    specPtr->dbName, Tk_PathName(tkwin));
		    Tcl_AddErrorInfo(interp, msg);
		    return TCL_ERROR;
		}
	    } else {
		if (specPtr->defValue != NULL) {
		    objPtr = Tcl_NewStringObj(specPtr->defValue, -1);
		} else {
		    objPtr = NULL;
		}
		if ((objPtr != NULL) && !(specPtr->specFlags
			& BLT_CONFIG_DONT_SET_DEFAULT)) {
		    if (DoConfig(interp, tkwin, specPtr, objPtr, widgRec) 
			!= TCL_OK) {
			char msg[200];
	
			sprintf(msg,
				"\n    (%s \"%.50s\" in widget \"%.50s\")",
				"default value for",
				specPtr->dbName, Tk_PathName(tkwin));
			Tcl_AddErrorInfo(interp, msg);
			return TCL_ERROR;
		    }
		}
	    }
	}
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_ConfigureInfoFromObj --
 *
 *	Return information about the configuration options
 *	for a window, and their current values.
 *
 * Results:
 *	Always returns TCL_OK.  The interp's result will be modified
 *	hold a description of either a single configuration option
 *	available for "widgRec" via "specs", or all the configuration
 *	options available.  In the "all" case, the result will
 *	available for "widgRec" via "specs".  The result will
 *	be a list, each of whose entries describes one option.
 *	Each entry will itself be a list containing the option's
 *	name for use on command lines, database name, database
 *	class, default value, and current value (empty string
 *	if none).  For options that are synonyms, the list will
 *	contain only two values:  name and synonym name.  If the
 *	"name" argument is non-NULL, then the only information
 *	returned is that for the named argument (i.e. the corresponding
 *	entry in the overall list is returned).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Blt_ConfigureInfoFromObj(interp, tkwin, specs, widgRec, objPtr, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window corresponding to widgRec. */
    Blt_ConfigSpec *specs;	/* Describes legal options. */
    char *widgRec;		/* Record whose fields contain current
				 * values for options. */
    Tcl_Obj *objPtr;		/* If non-NULL, indicates a single option
				 * whose info is to be returned.  Otherwise
				 * info is returned for all options. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered. */
{
    register Blt_ConfigSpec *specPtr;
    int needFlags, hateFlags;
    char *string;
    Tcl_Obj *listObjPtr, *valueObjPtr;

    needFlags = flags & ~(BLT_CONFIG_USER_BIT - 1);
    if (Tk_Depth(tkwin) <= 1) {
	hateFlags = BLT_CONFIG_COLOR_ONLY;
    } else {
	hateFlags = BLT_CONFIG_MONO_ONLY;
    }

    /*
     * If information is only wanted for a single configuration
     * spec, then handle that one spec specially.
     */

    Tcl_SetResult(interp, (char *)NULL, TCL_STATIC);
    if (objPtr != NULL) {
	specPtr = FindConfigSpec(interp, specs, objPtr, needFlags, hateFlags);
	if (specPtr == NULL) {
	    return TCL_ERROR;
	}
	valueObjPtr =  FormatConfigInfo(interp, tkwin, specPtr, widgRec);
	Tcl_SetObjResult(interp, valueObjPtr);
	return TCL_OK;
    }

    /*
     * Loop through all the specs, creating a big list with all
     * their information.
     */
    string = NULL;		/* Suppress compiler warning. */
    if (objPtr != NULL) {
	string = Tcl_GetString(objPtr);
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	if ((objPtr != NULL) && (specPtr->switchName != string)) {
	    continue;
	}
	if (((specPtr->specFlags & needFlags) != needFlags) || 
	    (specPtr->specFlags & hateFlags)) {
	    continue;
	}
	if (specPtr->switchName == NULL) {
	    continue;
	}
	valueObjPtr = FormatConfigInfo(interp, tkwin, specPtr, widgRec);
	Tcl_ListObjAppendElement(interp, listObjPtr, valueObjPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ConfigureValueFromObj --
 *
 *	This procedure returns the current value of a configuration
 *	option for a widget.
 *
 * Results:
 *	The return value is a standard Tcl completion code (TCL_OK or
 *	TCL_ERROR).  The interp's result will be set to hold either the value
 *	of the option given by objPtr (if TCL_OK is returned) or
 *	an error message (if TCL_ERROR is returned).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Blt_ConfigureValueFromObj(interp, tkwin, specs, widgRec, objPtr, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window corresponding to widgRec. */
    Blt_ConfigSpec *specs;	/* Describes legal options. */
    char *widgRec;		/* Record whose fields contain current
				 * values for options. */
    Tcl_Obj *objPtr;		/* Gives the command-line name for the
				 * option whose value is to be returned. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered. */
{
    Blt_ConfigSpec *specPtr;
    int needFlags, hateFlags;

    needFlags = flags & ~(BLT_CONFIG_USER_BIT - 1);
    if (Tk_Depth(tkwin) <= 1) {
	hateFlags = BLT_CONFIG_COLOR_ONLY;
    } else {
	hateFlags = BLT_CONFIG_MONO_ONLY;
    }
    specPtr = FindConfigSpec(interp, specs, objPtr, needFlags, hateFlags);
    if (specPtr == NULL) {
	return TCL_ERROR;
    }
    objPtr = FormatConfigValue(interp, tkwin, specPtr, widgRec);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FreeObjOptions --
 *
 *	Free up all resources associated with configuration options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any resource in widgRec that is controlled by a configuration
 *	option (e.g. a Tk_3DBorder or XColor) is freed in the appropriate
 *	fashion.
 *
 *----------------------------------------------------------------------
 */
void
Blt_FreeObjOptions(specs, widgRec, display, needFlags)
    Blt_ConfigSpec *specs;	/* Describes legal options. */
    char *widgRec;		/* Record whose fields contain current
				 * values for options. */
    Display *display;		/* X display; needed for freeing some
				 * resources. */
    int needFlags;		/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered. */
{
    register Blt_ConfigSpec *specPtr;
    char *ptr;

    for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	if ((specPtr->specFlags & needFlags) != needFlags) {
	    continue;
	}
	ptr = widgRec + specPtr->offset;
	switch (specPtr->type) {
	case BLT_CONFIG_STRING:
	    if (*((char **) ptr) != NULL) {
		Blt_Free(*((char **) ptr));
		*((char **) ptr) = NULL;
	    }
	    break;

	case BLT_CONFIG_COLOR:
	    if (*((XColor **) ptr) != NULL) {
		Tk_FreeColor(*((XColor **) ptr));
		*((XColor **) ptr) = NULL;
	    }
	    break;

	case BLT_CONFIG_FONT:
	    Tk_FreeFont(*((Tk_Font *) ptr));
	    *((Tk_Font *) ptr) = NULL;
	    break;

	case BLT_CONFIG_BITMAP:
	    if (*((Pixmap *) ptr) != None) {
		Tk_FreeBitmap(display, *((Pixmap *) ptr));
		*((Pixmap *) ptr) = None;
	    }
	    break;

	case BLT_CONFIG_BORDER:
	    if (*((Tk_3DBorder *) ptr) != NULL) {
		Tk_Free3DBorder(*((Tk_3DBorder *) ptr));
		*((Tk_3DBorder *) ptr) = NULL;
	    }
	    break;

	case BLT_CONFIG_CURSOR:
	case BLT_CONFIG_ACTIVE_CURSOR:
	    if (*((Tk_Cursor *) ptr) != None) {
		Tk_FreeCursor(display, *((Tk_Cursor *) ptr));
		*((Tk_Cursor *) ptr) = None;
	    }
	    break;

	case BLT_CONFIG_LISTOBJ:
	    Tcl_DecrRefCount(*(Tcl_Obj **)ptr);
	    break;

	case BLT_CONFIG_LIST:
	    {
		char **argv;
		
		argv = *(char ***)ptr;
		if (argv != NULL) {
		    Blt_Free(argv);
		    *(char ***)ptr = NULL;
		}
	    }
	    break;

	case BLT_CONFIG_TILE: 
	    if ((Blt_Tile)ptr != NULL) {
		Blt_FreeTile((Blt_Tile)ptr);
		*(Blt_Tile *)ptr = NULL;
	    }
	    break;
		
	case BLT_CONFIG_CUSTOM:
	    if ((*(char **)ptr != NULL) && 
		(specPtr->customPtr->freeProc != NULL)) {
		(*specPtr->customPtr->freeProc)(specPtr->customPtr->clientData,
			display, widgRec, specPtr->offset);
		*(char **)ptr = NULL;
	    }
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ObjConfigModified --
 *
 *      Given the configuration specifications and one or more option
 *	patterns (terminated by a NULL), indicate if any of the matching
 *	configuration options has been reset.
 *
 * Results:
 *      Returns 1 if one of the options has changed, 0 otherwise.
 *
 *----------------------------------------------------------------------
 */
int 
Blt_ObjConfigModified TCL_VARARGS_DEF(Blt_ConfigSpec *, arg1)
{
    va_list argList;
    Blt_ConfigSpec *specs;
    register Blt_ConfigSpec *specPtr;
    register char *option;

    specs = TCL_VARARGS_START(Blt_ConfigSpec *, arg1, argList);
    while ((option = va_arg(argList, char *)) != NULL) {
	for (specPtr = specs; specPtr->type != BLT_CONFIG_END; specPtr++) {
	    if ((Tcl_StringMatch(specPtr->switchName, option)) &&
		(specPtr->specFlags & BLT_CONFIG_OPTION_SPECIFIED)) {
		va_end(argList);
		return 1;
	    }
	}
    }
    va_end(argList);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ConfigureComponentFromObj --
 *
 *	Configures a component of a widget.  This is useful for
 *	widgets that have multiple components which aren't uniquely
 *	identified by a Tk_Window. It allows us, for example, set
 *	resources for axes of the graph widget. The graph really has
 *	only one window, but its convenient to specify components in a
 *	hierarchy of options.
 *
 *		*graph.x.logScale yes
 *		*graph.Axis.logScale yes
 *		*graph.temperature.scaleSymbols yes
 *		*graph.Element.scaleSymbols yes
 *
 *	This is really a hack to work around the limitations of the Tk
 *	resource database.  It creates a temporary window, needed to
 *	call Tk_ConfigureWidget, using the name of the component.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side Effects:
 *	A temporary window is created merely to pass to Tk_ConfigureWidget.
 *
 *----------------------------------------------------------------------
 */
int
Blt_ConfigureComponentFromObj(interp, parent, name, className, specsPtr,
	objc, objv, widgRec, flags)
    Tcl_Interp *interp;
    Tk_Window parent;		/* Window to associate with component */
    char *name;			/* Name of component */
    char *className;
    Blt_ConfigSpec *specsPtr;
    int objc;
    Tcl_Obj *CONST *objv;
    char *widgRec;
    int flags;
{
    Tk_Window tkwin;
    int result;
    char *tmpName;
    int isTemporary = FALSE;

    tmpName = Blt_Strdup(name);

    /* Window name can't start with an upper case letter */
    tmpName[0] = tolower(name[0]);

    /*
     * Create component if a child window by the component's name
     * doesn't already exist.
     */
    tkwin = Blt_FindChild(parent, tmpName);
    if (tkwin == NULL) {
	tkwin = Tk_CreateWindow(interp, parent, tmpName, (char *)NULL);
	isTemporary = TRUE;
    }
    if (tkwin == NULL) {
	Tcl_AppendResult(interp, "can't find window in \"", 
			 Tk_PathName(parent), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    assert(Tk_Depth(tkwin) == Tk_Depth(parent));
    Blt_Free(tmpName);

    Tk_SetClass(tkwin, className);
    result = Blt_ConfigureWidgetFromObj(interp, tkwin, specsPtr, objc, objv, 
	widgRec, flags);
    if (isTemporary) {
	Tk_DestroyWindow(tkwin);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_ObjIsOption --
 *
 *	Indicates whether objPtr is a valid configuration option 
 *	such as -background.
 *
 * Results:
 *	Returns 1 is a matching option is found and 0 otherwise.
 *
 *--------------------------------------------------------------
 */
int
Blt_ObjIsOption(specs, objPtr, flags)
    Blt_ConfigSpec *specs;	/* Describes legal options. */
    Tcl_Obj *objPtr;		/* Command-line option name. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered.  Also,
				 * may have BLT_CONFIG_ARGV_ONLY set. */
{
    register Blt_ConfigSpec *specPtr;
    int needFlags;		/* Specs must contain this set of flags
				 * or else they are not considered. */

    needFlags = flags & ~(BLT_CONFIG_USER_BIT - 1);
    specPtr = FindConfigSpec((Tcl_Interp *)NULL, specs, objPtr, needFlags, 0);
    return (specPtr != NULL);
}


#endif /* TK_VERSION_NUMBER >= 8.1.0 */
