/*
 * tkTreeHeader.c --
 *
 *	This module implements treectrl widget's column headers.
 *
 * Copyright (c) 2011 Tim Baker
 */

#include "tkTreeCtrl.h"

typedef struct TreeHeader_ TreeHeader_;
typedef struct TreeHeaderColumn_ HeaderColumn;

#define IS_TAIL(C) (C == tree->columnTail)

/*
 * The following structure holds information about a single column header
 * of a TreeHeader.
 */
struct TreeHeaderColumn_
{
    TreeItemColumn itemColumn;

    Tcl_Obj *textObj;		/* -text */
    char *text;			/* -text */
    Tk_Font tkfont;		/* -font */
    Tk_Justify justify;		/* -justify */
    PerStateInfo border;	/* -background */
    Tcl_Obj *borderWidthObj;	/* -borderwidth */
    int borderWidth;		/* -borderwidth */
    PerStateInfo textColor;	/* -textcolor */
    char *imageString;		/* -image */
    PerStateInfo arrowBitmap;	/* -arrowbitmap */
    PerStateInfo arrowImage;	/* -arrowimage */
    Pixmap bitmap;		/* -bitmap */
    int button;			/* -button */
    Tcl_Obj *textPadXObj;	/* -textpadx */
    int *textPadX;		/* -textpadx */
    Tcl_Obj *textPadYObj;	/* -textpady */
    int *textPadY;		/* -textpady */
    Tcl_Obj *imagePadXObj;	/* -imagepadx */
    int *imagePadX;		/* -imagepadx */
    Tcl_Obj *imagePadYObj;	/* -imagepady */
    int *imagePadY;		/* -imagepady */
    Tcl_Obj *arrowPadXObj;	/* -arrowpadx */
    int *arrowPadX;		/* -arrowpadx */
    Tcl_Obj *arrowPadYObj;	/* -arrowpady */
    int *arrowPadY;		/* -arrowpady */

    int arrow;			/* -arrow */

#define SIDE_LEFT 0
#define SIDE_RIGHT 1
    int arrowSide;		/* -arrowside */
    int arrowGravity;		/* -arrowgravity */

    int state;			/* -state */

    int textLen;
    Tk_Image image;
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_WORD 1
    int textWrap;		/* -textwrap */
    int textLines;		/* -textlines */

    Tk_Image dragImage;		/* Used during drag-and-drop. */
    int imageEpoch;		/* If this value doesn't match
				 * tree->columnDrag.imageEpoch the drag image
				 * for this column is recreated. */
    Tk_Uid dragImageName;	/* Name needed to delete the drag image. */
};

/*
 * The following structure holds [dragconfigure] option info for a TreeHeader.
 */
struct TreeHeaderDrag
{
    int enable;			/* -enable */
    int draw;			/* -draw */
};

/*
 * The following structure holds information about a single TreeHeader.
 */
struct TreeHeader_
{
    TreeCtrl *tree;
    TreeItem item;
    struct TreeHeaderDrag columnDrag;
};

static CONST char *arrowST[] = { "none", "up", "down", (char *) NULL };
static CONST char *arrowSideST[] = { "left", "right", (char *) NULL };
static CONST char *stateST[] = { "normal", "active", "pressed", (char *) NULL };

#define COLU_CONF_IMAGE		0x0001
#define COLU_CONF_NWIDTH	0x0002	/* neededWidth */
#define COLU_CONF_NHEIGHT	0x0004	/* neededHeight */
#define COLU_CONF_STATE		0x0008	/* -arrow and -state */
#define COLU_CONF_DISPLAY	0x0040
#define COLU_CONF_JUSTIFY	0x0080
#define COLU_CONF_TEXT		0x0200
#define COLU_CONF_BITMAP	0x0400
#define COLU_CONF_TAIL		0x0800 /* options affecting tail's HeaderStyle */

#define ELEM_HEADER		0x00010000
#define ELEM_BITMAP		0x00020000
#define ELEM_IMAGE		0x00040000
#define ELEM_TEXT		0x00080000

static Tk_OptionSpec columnSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-arrow", (char *) NULL, (char *) NULL,
     "none", -1, Tk_Offset(HeaderColumn, arrow),
     0, (ClientData) arrowST, COLU_CONF_STATE | ELEM_HEADER},
    {TK_OPTION_CUSTOM, "-arrowbitmap", (char *) NULL, (char *) NULL,
     (char *) NULL,
     Tk_Offset(HeaderColumn, arrowBitmap.obj), Tk_Offset(HeaderColumn, arrowBitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_STRING_TABLE, "-arrowgravity", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(HeaderColumn, arrowGravity),
     0, (ClientData) arrowSideST, COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_CUSTOM, "-arrowimage", (char *) NULL, (char *) NULL,
     (char *) NULL,
     Tk_Offset(HeaderColumn, arrowImage.obj), Tk_Offset(HeaderColumn, arrowImage),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_CUSTOM, "-arrowpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(HeaderColumn, arrowPadXObj), Tk_Offset(HeaderColumn, arrowPadX),
     0, (ClientData) &TreeCtrlCO_pad, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_CUSTOM, "-arrowpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(HeaderColumn, arrowPadYObj), Tk_Offset(HeaderColumn, arrowPadY),
     0, (ClientData) &TreeCtrlCO_pad, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_STRING_TABLE, "-arrowside", (char *) NULL, (char *) NULL,
     "right", -1, Tk_Offset(HeaderColumn, arrowSide),
     0, (ClientData) arrowSideST, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY | ELEM_HEADER},
    {TK_OPTION_CUSTOM, "-background", (char *) NULL, (char *) NULL,
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     Tk_Offset(HeaderColumn, border.obj), Tk_Offset(HeaderColumn, border),
     0, (ClientData) NULL, COLU_CONF_DISPLAY | COLU_CONF_TAIL | ELEM_HEADER},
    {TK_OPTION_BITMAP, "-bitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(HeaderColumn, bitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_BITMAP | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | ELEM_BITMAP},
    {TK_OPTION_PIXELS, "-borderwidth", (char *) NULL, (char *) NULL,
     "2", Tk_Offset(HeaderColumn, borderWidthObj), Tk_Offset(HeaderColumn, borderWidth),
     0, (ClientData) NULL, COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT |
     COLU_CONF_DISPLAY | COLU_CONF_TAIL | ELEM_HEADER},
    {TK_OPTION_BOOLEAN, "-button", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(HeaderColumn, button),
     0, (ClientData) NULL, 0},
    {TK_OPTION_FONT, "-font", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(HeaderColumn, tkfont),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | COLU_CONF_TEXT | ELEM_TEXT},
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(HeaderColumn, imageString),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_IMAGE | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY
     | ELEM_IMAGE},
    {TK_OPTION_CUSTOM, "-imagepadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(HeaderColumn, imagePadXObj),
     Tk_Offset(HeaderColumn, imagePadX), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(HeaderColumn, imagePadYObj),
     Tk_Offset(HeaderColumn, imagePadY), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_JUSTIFY, "-justify", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(HeaderColumn, justify),
     0, (ClientData) NULL, COLU_CONF_DISPLAY | COLU_CONF_JUSTIFY},
    {TK_OPTION_STRING_TABLE, "-state", (char *) NULL, (char *) NULL,
     "normal", -1, Tk_Offset(HeaderColumn, state), 0, (ClientData) stateST,
     COLU_CONF_STATE | ELEM_HEADER},
    {TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(HeaderColumn, textObj), Tk_Offset(HeaderColumn, text),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_TEXT | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY |
     ELEM_TEXT},
    {TK_OPTION_CUSTOM, "-textcolor", (char *) NULL, (char *) NULL,
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     Tk_Offset(HeaderColumn, textColor.obj),
     Tk_Offset(HeaderColumn, textColor), 0, (ClientData) NULL,
     COLU_CONF_DISPLAY | ELEM_TEXT},
    {TK_OPTION_INT, "-textlines", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(HeaderColumn, textLines),
     0, (ClientData) NULL, COLU_CONF_TEXT | COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | ELEM_TEXT},
    {TK_OPTION_CUSTOM, "-textpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(HeaderColumn, textPadXObj),
     Tk_Offset(HeaderColumn, textPadX), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(HeaderColumn, textPadYObj),
     Tk_Offset(HeaderColumn, textPadY), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

static Tk_OptionSpec dragSpecs[] = {
    {TK_OPTION_BOOLEAN, "-draw", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeHeader_, columnDrag.draw),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeHeader_, columnDrag.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/* We can also configure -height, -tags and -visible item options */
static Tk_OptionSpec headerSpecs[] = {
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * HeaderCO_Set --
 *
 *	Tk_ObjCustomOption.setProc(). Converts a Tcl_Obj holding a
 *	header description into a TreeHeader.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May store a TreeHeader pointer into the internal representation
 *	pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *	that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
HeaderCO_Set(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
				 * We use a pointer to the pointer because
				 * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
				 * internal value is to be stored. */
    char *saveInternalPtr,	/* Pointer to storage for the old value. */
    int flags			/* Flags for the option, set Tk_SetOptions. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    TreeHeader new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (TreeHeader *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (TreeHeader_FromObj(tree, (*value), &new) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = NULL;
	*((TreeHeader *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * HeaderCO_Get --
 *
 *	Tk_ObjCustomOption.getProc(). Converts a TreeHeader into a
 *	Tcl_Obj string representation.
 *
 * Results:
 *	Tcl_Obj containing the string representation of the header.
 *	Returns NULL if the TreeHeader is NULL.
 *
 * Side effects:
 *	May create a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
HeaderCO_Get(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset		/* Offset within *recordPtr containing the
				 * sticky value. */
    )
{
    TreeHeader value = *(TreeHeader *) (recordPtr + internalOffset);
/*    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;*/
    if (value == NULL)
	return NULL;
#if 0
    if (value == COLUMN_ALL)
	return Tcl_NewStringObj("all", -1);
#endif
    return TreeHeader_ToObj(value);
}

/*
 *----------------------------------------------------------------------
 *
 * HeaderCO_Restore --
 *
 *	Tk_ObjCustomOption.restoreProc(). Restores a TreeHeader value
 *	from a saved value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
HeaderCO_Restore(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Not used. */
    char *internalPtr,		/* Where to store old value. */
    char *saveInternalPtr)	/* Pointer to old value. */
{
    *(TreeHeader *) internalPtr = *(TreeHeader *) saveInternalPtr;
}

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeHeader conversion.
 * A header description must refer to a single header.
 */
Tk_ObjCustomOption TreeCtrlCO_header =
{
    "header",
    HeaderCO_Set,
    HeaderCO_Get,
    HeaderCO_Restore,
    NULL,
    (ClientData) 0
};

/*
 *----------------------------------------------------------------------
 *
 * LookupOption --
 *
 *	Find the configuration option that matches the given
 *	possibly-truncated name.  This code was copied from the Tk
 *	sources, but doesn't handle chained option tables.
 *
 * Results:
 *	Pointer to the matching option record, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tk_OptionSpec*
LookupOption(
    Tk_OptionSpec *tablePtr,
    CONST char *name
    )
{
    Tk_OptionSpec *specPtr = tablePtr, *bestPtr = NULL;
    CONST char *p1, *p2;

    while (specPtr->type != TK_OPTION_END) {
	for (p1 = name, p2 = specPtr->optionName;
		*p1 == *p2; p1++, p2++) {
	    if (*p1 == 0) {
		/*
		* This is an exact match.  We're done.
		*/

		bestPtr = specPtr;
		return bestPtr;
	    }
	}
	if (*p1 == 0) {
	    /*
	    * The name is an abbreviation for this option.  Keep
	    * to make sure that the abbreviation only matches one
	    * option name.  If we've already found a match in the
	    * past, then it is an error unless the full names for
	    * the two options are identical; in this case, the first
	    * option overrides the second.
	    */

	    if (bestPtr == NULL) {
		bestPtr = specPtr;
	    } else {
		if (strcmp(bestPtr->optionName,
			specPtr->optionName) != 0) {
		    return NULL;
		}
	    }
	}
	specPtr++;
    }
    return bestPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_ConfigureHeaderStyle --
 *
 *	Passes configuration option/value pairs to the appropriate
 *	element in the underlying style for a column header.
 *
 *	If the column header is not using one of the default header
 *	styles, or if this is the tail column, then nothing is done.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May cause layout changes and a redraw of the widget.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderColumn_ConfigureHeaderStyle(
    TreeHeader header,		/* Header containing the column. */
    TreeHeaderColumn column,	/* Column that was configured. */
    TreeColumn treeColumn,	/* The tree-column. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Option/value pairs. */
    )
{
    TreeCtrl *tree = header->tree;
    Tcl_Interp *interp = tree->interp;
    Tk_OptionSpec *specPtr = columnSpecs;
    Tk_OptionSpec *staticSpecs[STATIC_SIZE], **specs = staticSpecs;
    int i, j, infoObjC = 0, elemObjC[4], eMask, iMask = 0;
    Tcl_Obj *staticInfoObjV[STATIC_SIZE], **infoObjV = staticInfoObjV;
    Tcl_Obj *staticElemObjV[4][STATIC_SIZE], **elemObjV[4];
    Tcl_Obj *textFillObj = NULL, *textLinesObj = NULL, *textFontObj = NULL;
    int allocSpecs = 0, allocInfoObjV = 0, allocElemObjV[4];
    HeaderStyleParams params;
    int result;

    /* TODO: If the column header uses a user-defined style, generate a
     * <Header-configure> event. */
    if (!TreeStyle_IsHeaderStyle(tree,
	    TreeItemColumn_GetStyle(tree, column->itemColumn)))
	return TCL_OK;

    params.text = column->textLen > 0;
    params.image = column->image != NULL;
    params.bitmap = !params.image && (column->bitmap != None);

    /* With no arguments, it means the style was (re)created, so gotta
     * configure all non-layout options. */
    if (objc == 0) {
	Tcl_Obj *optionNameObj = Tcl_NewObj();
	Tcl_IncrRefCount(optionNameObj);
	for (i = 0; i < 4; i++)
	    elemObjC[i] = 0;
	while (specPtr->type != TK_OPTION_END) {
	    if (IS_TAIL(treeColumn) && !(specPtr->typeMask & COLU_CONF_TAIL)) {
		specPtr++;
		continue;
	    }
	    if (specPtr->typeMask & ELEM_HEADER) {
		elemObjC[0] += 2;
		objc += 2;
	    }
	    if (specPtr->typeMask & ELEM_IMAGE) {
		if (params.image)
		    elemObjC[1] += 2;
		objc += 2;
	    }
	    if (specPtr->typeMask & ELEM_TEXT) {
		if (params.text)
		    elemObjC[2] += 2;
		objc += 2;
	    }
	    if (specPtr->typeMask & ELEM_BITMAP) {
		if (params.bitmap)
		    elemObjC[3] += 2;
		objc += 2;
	    }
	    specPtr++;
	}
	allocInfoObjV = objc / 2;
	STATIC_ALLOC(infoObjV, Tcl_Obj *, allocInfoObjV);
	for (i = 0; i < 4; i++) {
	    elemObjV[i] = staticElemObjV[i];
	    allocElemObjV[i] = elemObjC[i];
	    STATIC_ALLOC(elemObjV[i], Tcl_Obj *, allocElemObjV[i]);
	    elemObjC[i] = 0;
	}
	specPtr = columnSpecs;
	while (specPtr->type != TK_OPTION_END) {
	    if (IS_TAIL(treeColumn) && !(specPtr->typeMask & COLU_CONF_TAIL)) {
		specPtr++;
		continue;
	    }
	    if (specPtr->typeMask & (ELEM_HEADER | ELEM_IMAGE | ELEM_TEXT | ELEM_BITMAP)) {
		int listC;
		Tcl_Obj *infoObj, **listObjV;
		Tcl_SetStringObj(optionNameObj, specPtr->optionName, -1);
		infoObj = Tk_GetOptionInfo(interp, (char *) column,
		    tree->headerColumnOptionTable, optionNameObj,
		    tree->tkwin);
		Tcl_IncrRefCount(infoObj);
		Tcl_ListObjGetElements(interp, infoObj, &listC, &listObjV);
		if (specPtr->typeMask & ELEM_HEADER) {
		    elemObjV[0][elemObjC[0]++] = listObjV[0]; /* name */
		    elemObjV[0][elemObjC[0]++] = listObjV[4]; /* value */
		}
		if (specPtr->typeMask & ELEM_IMAGE) {
		    if (params.image) {
			elemObjV[1][elemObjC[1]++] = listObjV[0]; /* name */
			elemObjV[1][elemObjC[1]++] = listObjV[4]; /* value */
		    }
		}
		if (specPtr->typeMask & ELEM_TEXT) {
		    if (params.text) {
			if (!strcmp(specPtr->optionName, "-textcolor")) {
			    if (textFillObj == NULL) {
				textFillObj = Tcl_NewStringObj("-fill", -1);
				Tcl_IncrRefCount(textFillObj);
			    }
			    elemObjV[2][elemObjC[2]++] = textFillObj;
			} else if (!strcmp(specPtr->optionName, "-textlines")) {
			    if (textLinesObj == NULL) {
				textLinesObj = Tcl_NewStringObj("-lines", -1);
				Tcl_IncrRefCount(textLinesObj);
			    }
			    elemObjV[2][elemObjC[2]++] = textLinesObj;
			} else {
			    elemObjV[2][elemObjC[2]++] = listObjV[0]; /* name */
			}
			/* Text element -font is per-state. */
			if (!strcmp(specPtr->optionName, "-font")) {
			    textFontObj = Tcl_NewListObj(1, &listObjV[4]);
			    Tcl_IncrRefCount(textFontObj);
			    elemObjV[2][elemObjC[2]++] = textFontObj; /* value */
			} else {
			    elemObjV[2][elemObjC[2]++] = listObjV[4]; /* value */
			}
		    }
		}
		if (specPtr->typeMask & ELEM_BITMAP) {
		    if (params.bitmap) {
			elemObjV[3][elemObjC[3]++] = listObjV[0]; /* name */
			elemObjV[3][elemObjC[3]++] = listObjV[4]; /* value */
		    }
		}
		infoObjV[infoObjC++] = infoObj;
	    }
	    specPtr++;
	}
	Tcl_DecrRefCount(optionNameObj);
	objc = 0;

    /* Some option/value pairs were given. */
    } else {
	for (i = 0; i < 4; i++) {
	    elemObjV[i] = staticElemObjV[i];
	    allocElemObjV[i] = objc;
	    STATIC_ALLOC(elemObjV[i], Tcl_Obj *, allocElemObjV[i]);
	    elemObjC[i] = 0;
	}
	/* Remove duplicate options (see use of textFontObj). */
	allocSpecs = objc;
	STATIC_ALLOC(specs, Tk_OptionSpec *, allocSpecs);
	for (i = 0; i < objc; i += 2) {
	    specs[i] = LookupOption(columnSpecs, Tcl_GetString(objv[i]));
	    for (j = 0; j < i; j += 2) {
		if (specs[j] == specs[i]) {
		    specs[j] = NULL;
		    break;
		}
	    }
	}
	for (i = 0; i < objc; i += 2) {
	    specPtr = specs[i];
	    if (specPtr == NULL)
		continue;
	    if (IS_TAIL(treeColumn) && !(specPtr->typeMask & COLU_CONF_TAIL))
		continue;
	    if (specPtr->typeMask & ELEM_HEADER) {
		elemObjV[0][elemObjC[0]++] = objv[i]; /* name */
		elemObjV[0][elemObjC[0]++] = objv[i + 1]; /* value */
	    }
	    if (specPtr->typeMask & ELEM_IMAGE) {
		if (params.image) {
		    elemObjV[1][elemObjC[1]++] = objv[i]; /* name */
		    elemObjV[1][elemObjC[1]++] = objv[i + 1]; /* value */
		}
	    }
	    if (specPtr->typeMask & ELEM_TEXT) {
		if (params.text) {
		    if (!strcmp(specPtr->optionName, "-textcolor")) {
			if (textFillObj == NULL) {
			    textFillObj = Tcl_NewStringObj("-fill", -1);
			    Tcl_IncrRefCount(textFillObj);
			}
			elemObjV[2][elemObjC[2]++] = textFillObj;
		    } else if (!strcmp(specPtr->optionName, "-textlines")) {
			if (textLinesObj == NULL) {
			    textLinesObj = Tcl_NewStringObj("-lines", -1);
			    Tcl_IncrRefCount(textLinesObj);
			}
			elemObjV[2][elemObjC[2]++] = textLinesObj;
		    } else {
			elemObjV[2][elemObjC[2]++] = objv[i]; /* name */
		    }
		    /* Text element -font is per-state. */
		    if (!strcmp(specPtr->optionName, "-font")) {
			textFontObj = Tcl_NewListObj(1, &objv[i + 1]);
			Tcl_IncrRefCount(textFontObj);
			elemObjV[2][elemObjC[2]++] = textFontObj; /* value */
		    } else {
			elemObjV[2][elemObjC[2]++] = objv[i + 1]; /* value */
		    }
		}
	    }
	    if (specPtr->typeMask & ELEM_BITMAP) {
		if (params.bitmap) {
		    elemObjV[3][elemObjC[3]++] = objv[i]; /* name */
		    elemObjV[3][elemObjC[3]++] = objv[i + 1]; /* value */
		}
	    }
	}
    }

    if (elemObjC[0] > 0) {
	result = TreeStyle_ElementConfigure(tree, header->item,
	    column->itemColumn,
	    TreeItemColumn_GetStyle(tree, column->itemColumn),
	    tree->headerStyle.headerElem, elemObjC[0], elemObjV[0], &eMask);
	if (result != TCL_OK)
	    Tcl_BackgroundError(interp);
	iMask |= eMask;
    }
    if (elemObjC[1] > 0) {
	result = TreeStyle_ElementConfigure(tree, header->item,
	    column->itemColumn,
	    TreeItemColumn_GetStyle(tree, column->itemColumn),
	    tree->headerStyle.imageElem, elemObjC[1], elemObjV[1], &eMask);
	if (result != TCL_OK)
	    Tcl_BackgroundError(interp);
	iMask |= eMask;
    }
    if (elemObjC[2] > 0) {
	result = TreeStyle_ElementConfigure(tree, header->item,
	    column->itemColumn,
	    TreeItemColumn_GetStyle(tree, column->itemColumn),
	    tree->headerStyle.textElem, elemObjC[2], elemObjV[2], &eMask);
	if (result != TCL_OK)
	    Tcl_BackgroundError(interp);
	iMask |= eMask;
    }
    if (elemObjC[3] > 0) {
	result = TreeStyle_ElementConfigure(tree, header->item,
	    column->itemColumn,
	    TreeItemColumn_GetStyle(tree, column->itemColumn),
	    tree->headerStyle.bitmapElem, elemObjC[3], elemObjV[3], &eMask);
	if (result != TCL_OK)
	    Tcl_BackgroundError(interp);
	iMask |= eMask;
    }

    if (iMask & CS_LAYOUT) {
	TreeItem_InvalidateHeight(tree, header->item);
	TreeItemColumn_InvalidateSize(tree, column->itemColumn);
	Tree_FreeItemDInfo(tree, header->item, NULL);
	TreeColumns_InvalidateWidthOfItems(tree, treeColumn);
    } else if (iMask & CS_DISPLAY)
	Tree_InvalidateItemDInfo(tree, treeColumn, header->item, NULL);

    for (i = 0; i < infoObjC; i++)
	Tcl_DecrRefCount(infoObjV[i]);
    if (textFillObj != NULL)
	Tcl_DecrRefCount(textFillObj);
    if (textLinesObj != NULL)
	Tcl_DecrRefCount(textLinesObj);
    if (textFontObj != NULL)
	Tcl_DecrRefCount(textFontObj);

    for (i = 0; i < 4; i++)
	STATIC_FREE(elemObjV[i], Tcl_Obj *, allocElemObjV[i]);
    STATIC_FREE(infoObjV, Tcl_Obj *, allocInfoObjV);
    STATIC_FREE(specs, Tk_OptionSpec *, allocSpecs);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Configure --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a HeaderColumn.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for column;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Column_Configure(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    TreeColumn treeColumn,	/* Column token. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the Column is being created. */
    )
{
    TreeCtrl *tree = header->tree;
    HeaderColumn saved;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask, maskFree = 0;
    int state = column->state, arrow = column->arrow;

    /* Init these to prevent compiler warnings */
    saved.image = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tree_SetOptions(tree, STATE_DOMAIN_HEADER, column,
			tree->headerColumnOptionTable, objc, objv,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (column->imageString != NULL)
		    mask |= COLU_CONF_IMAGE;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    if (mask & COLU_CONF_IMAGE)
		saved.image = column->image;

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & COLU_CONF_IMAGE) {
		if (column->imageString == NULL) {
		    column->image = NULL;
		} else {
		    column->image = Tree_GetImage(tree, column->imageString);
		    if (column->image == NULL)
			continue;
		    maskFree |= COLU_CONF_IMAGE;
		}
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    if (mask & COLU_CONF_IMAGE) {
		if (saved.image != NULL)
		    Tree_FreeImage(tree, saved.image);
	    }
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /*
	     * Free new values.
	     */
	    if (maskFree & COLU_CONF_IMAGE)
		Tree_FreeImage(tree, column->image);

	    /*
	     * Restore old values.
	     */
	    if (mask & COLU_CONF_IMAGE)
		column->image = saved.image;

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    /* Wouldn't have to do this if Tk_InitOptions() would return
    * a mask of configured options like Tk_SetOptions() does. */
    if (createFlag) {
	if (column->textObj != NULL)
	    mask |= COLU_CONF_TEXT;
	if (column->bitmap != None)
	    mask |= COLU_CONF_BITMAP;
    }

    if (mask & COLU_CONF_TEXT) {
	if (column->textObj != NULL)
	    (void) Tcl_GetStringFromObj(column->textObj, &column->textLen);
	else
	    column->textLen = 0;
    }

    /* Keep the STATE_HEADER_XXX flags in sync. */
    /* FIXME: don't call TreeItemColumn_ChangeState twice here */
    if (treeColumn != tree->columnTail && state != column->state) {
	int stateOff = 0, stateOn = 0;
	switch (state) {
	    case COLUMN_STATE_ACTIVE: stateOff = STATE_HEADER_ACTIVE; break;
	    case COLUMN_STATE_NORMAL: stateOff = STATE_HEADER_NORMAL; break;
	    case COLUMN_STATE_PRESSED: stateOff = STATE_HEADER_PRESSED; break;
	}
	switch (column->state) {
	    case COLUMN_STATE_ACTIVE: stateOn = STATE_HEADER_ACTIVE; break;
	    case COLUMN_STATE_NORMAL: stateOn = STATE_HEADER_NORMAL; break;
	    case COLUMN_STATE_PRESSED: stateOn = STATE_HEADER_PRESSED; break;
	}
	TreeItemColumn_ChangeState(tree, header->item, column->itemColumn,
	    treeColumn, stateOff, stateOn);
    }
    if (treeColumn != tree->columnTail && arrow != column->arrow) {
	int stateOff = 0, stateOn = 0;
	switch (arrow) {
	    case COLUMN_ARROW_UP: stateOff = STATE_HEADER_SORT_UP; break;
	    case COLUMN_ARROW_DOWN: stateOff = STATE_HEADER_SORT_DOWN; break;
	}
	switch (column->arrow) {
	    case COLUMN_ARROW_UP: stateOn = STATE_HEADER_SORT_UP; break;
	    case COLUMN_ARROW_DOWN: stateOn = STATE_HEADER_SORT_DOWN; break;
	}
	TreeItemColumn_ChangeState(tree, header->item, column->itemColumn,
	    treeColumn, stateOff, stateOn);
    }

    if (!createFlag) {
	TreeHeaderColumn_EnsureStyleExists(header, column, treeColumn);
	TreeHeaderColumn_ConfigureHeaderStyle(header, column, treeColumn,
	    objc, objv);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_EnsureStyleExists --
 *
 *	This procedure maintains the style assigned to a column header.
 *	If the column header uses a custom user-defined style, it is left
 *	alone.  Otherwise, a new header style may be created if one
 *	doesn't already exist that satisfies the various configuration
 *	options of the column header.  If a new header style is created,
 *	the current instance style (if any) is freed and a new instance
 *	style is assigned and configured.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderColumn_EnsureStyleExists(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    TreeColumn treeColumn	/* Column token. */
    )
{
    TreeCtrl *tree = header->tree;
    TreeItemColumn itemColumn = column->itemColumn;
    TreeStyle styleOld, styleNew;
    HeaderStyleParams params;
    int i;

    styleOld = TreeItemColumn_GetStyle(tree, itemColumn);
    if (styleOld != NULL) {
	styleOld = TreeStyle_GetMaster(tree, styleOld);
	if (!TreeStyle_IsHeaderStyle(tree, styleOld))
	    return TCL_OK; /* it's a custom style */
    }

    params.justify = column->justify;
    params.text = column->textLen > 0;
    params.image = column->image != NULL;
    params.bitmap = !params.image && (column->bitmap != None);
    for (i = 0; i < 2; i++) {
	params.textPadX[i] = column->textPadX[i];
	params.textPadY[i] = column->textPadY[i];
	params.imagePadX[i] = column->imagePadX[i];
	params.imagePadY[i] = column->imagePadY[i];
    }
    if (treeColumn == tree->columnTail) {
	params.text = params.image = params.bitmap = FALSE;
    }
    styleNew = Tree_MakeHeaderStyle(tree, &params);
    if (styleOld != styleNew) {
	styleNew = TreeStyle_NewInstance(tree, styleNew);
	TreeItemColumn_SetStyle(tree, itemColumn, styleNew);
	TreeHeaderColumn_ConfigureHeaderStyle(header, column, treeColumn,
	    0, NULL);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ConsumeColumnCget --
 *
 *	Sets the interpreter result to the value of a single configuration
 *	option for a header-column.  This is called when an unknown
 *	option is passed to [column cget].  It operates on the first
 *	row of headers only.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_ConsumeColumnCget(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* Used to determine which header-column to
				 * get the option value from. */
    Tcl_Obj *objPtr		/* Object holding the name of the option. */
    )
{
    TreeItemColumn itemColumn;
    TreeHeaderColumn column;
    Tcl_Obj *resultObjPtr;

#ifdef TREECTRL_DEBUG
    if (tree->headerItems == NULL)
	panic("the default header was deleted!");
#endif
    itemColumn = TreeItem_FindColumn(tree, tree->headerItems,
	TreeColumn_Index(treeColumn));
#ifdef TREECTRL_DEBUG
    if (itemColumn == NULL)
	panic("the default header is missing column %s%d!",
	    tree->columnPrefix, TreeColumn_GetID(treeColumn));
#endif
    column = TreeItemColumn_GetHeaderColumn(tree, itemColumn);
    resultObjPtr = Tk_GetOptionValue(tree->interp, (char *) column,
	    tree->headerColumnOptionTable, objPtr, tree->tkwin);
    if (resultObjPtr == NULL)
	return TCL_ERROR;
    Tcl_SetObjResult(tree->interp, resultObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ConsumeColumnConfig --
 *
 *	Configures a header-column with option/value pairs.  This is
 *	called by [column configure] for any unknown options.  It
 *	operates on the first row of headers only.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_ConsumeColumnConfig(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* Used to determine which header-column to
				 * configure. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if column is being created. */
    )
{
    TreeItemColumn itemColumn;
    TreeHeaderColumn column;

#if 1
    if (objc <= 0)
#else
    if ((objc <= 0) || (treeColumn == tree->columnTail))
#endif
	return TCL_OK;
#ifdef TREECTRL_DEBUG
    if (tree->headerItems == NULL)
	panic("the default header was deleted!");
#endif
    itemColumn = TreeItem_FindColumn(tree, tree->headerItems,
	TreeColumn_Index(treeColumn));
#ifdef TREECTRL_DEBUG
    if (itemColumn == NULL)
	panic("the default header is missing column %s%d!",
	    tree->columnPrefix, TreeColumn_GetID(treeColumn));
#endif
    column = TreeItemColumn_GetHeaderColumn(tree, itemColumn);
    return Column_Configure(TreeItem_GetHeader(tree, tree->headerItems),
	column, treeColumn, objc, objv, createFlag);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ConsumeColumnOptionInfo --
 *
 *	Retrieves the info for one or all configuration options of a
 *	header-column.  This is called when an unknown option is passed
 *	to [column configure].   It operates on the first row of
 *	headers only.
 *
 * Results:
 *	A pointer to a list object containing the configuration info
 *	for the option(s), or NULL if an error occurs.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeHeader_ConsumeColumnOptionInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* Used to determine which header-column
				 * to get the option info from. */
    Tcl_Obj *objPtr		/* Option name or NULL for every option. */
    )
{
    TreeItemColumn itemColumn;
    TreeHeaderColumn column;

#ifdef TREECTRL_DEBUG
    if (tree->headerItems == NULL)
	panic("the default header was deleted!");
#endif
    itemColumn = TreeItem_FindColumn(tree, tree->headerItems,
	TreeColumn_Index(treeColumn));
#ifdef TREECTRL_DEBUG
    if (itemColumn == NULL)
	panic("the default header is missing column %s%d!",
	    tree->columnPrefix, TreeColumn_GetID(treeColumn));
#endif
    column = TreeItemColumn_GetHeaderColumn(tree, itemColumn);
    return Tk_GetOptionInfo(tree->interp, (char *) column,
	tree->headerColumnOptionTable, objPtr,  tree->tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_GetImageOrText --
 *
 *	Retrieves the value of the -image or -text option of a
 *	header-column.
 *	This is called by the [header image] and [header text] command
 *	when a header-column has no style or no style with a text
 *	element.
 *
 * Results:
 *	A point to an object containing the value of the -image or
 *	-text option, which may be NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeHeaderColumn_GetImageOrText(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    int isImage
    )
{
    TreeCtrl *tree = header->tree;

    return Tk_GetOptionValue(tree->interp, (char *) column,
	tree->headerColumnOptionTable,
	isImage ? tree->imageOptionNameObj : tree->textOptionNameObj,
	tree->tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_SetImageOrText --
 *
 *	Configures the -image or -text option of a header-column.
 *	This is called by the [header image] and [header text] commands
 *	when a header-column has no style or no style with a text
 *	element.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Header layout may change, a redraw may get scheduled.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderColumn_SetImageOrText(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    TreeColumn treeColumn,	/* Column token. */
    Tcl_Obj *valueObj,		/* New value of -image or -text option */
    int isImage			/* TRUE to configure the -image option,
				 * FALSE to configure the -text option. */
    )
{
    TreeCtrl *tree = header->tree;
    int objc = 2;
    Tcl_Obj *objv[2];

    objv[0] = isImage ? tree->imageOptionNameObj : tree->textOptionNameObj;
    objv[1] = valueObj;
    return Column_Configure(header, column, treeColumn, objc, objv, FALSE);
}

static TreeColumn
GetFollowingColumn(
    TreeColumn column,
    int n,
    TreeColumn stop
    )
{
    while (--n > 0) {
	TreeColumn next = TreeColumn_Next(column);
	if (next == NULL)
	    break;
	if (next == stop)
	    break;
	if (TreeColumn_Lock(next) != TreeColumn_Lock(column))
	    break;
	column = next;
    }
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ColumnDragOrder --
 *
 *	Calculates the display order of a column during drag-and-drop.
 *	If no column headers are being dragged, or there is no indicator
 *	column, or this header isn't displaying drag-and-drop feedback,
 *	then the column order is unchanged.
 *
 * Results:
 *	The possibly-updated display order for the column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_ColumnDragOrder(
    TreeHeader header,		/* Header token. */
    TreeColumn column,		/* Column to get updated index for. */
    int index			/* Zero-based index of this column in
				 * the list of all columns with the same
				 * -lock value. */
    )
{
    TreeCtrl *tree = header->tree;
    TreeColumn column1min, column1max;
    TreeColumn column2min, column2max;
    int index1min, index1max;
    int index2min, index2max;
    int index3;

    if (!header->columnDrag.draw)
	return index;

    if (tree->columnDrag.column == NULL)
	return index;

    if (tree->columnDrag.indColumn == NULL)
	return index;

    column1min = tree->columnDrag.column;
    column1max = GetFollowingColumn(column1min, tree->columnDrag.span, NULL);
    index1min = TreeColumn_Index(column1min);
    index1max = TreeColumn_Index(column1max);

    column2min = tree->columnDrag.indColumn;
    column2max = GetFollowingColumn(column2min, tree->columnDrag.indSpan,
	column1min);
    index2min = TreeColumn_Index(column2min);
    index2max = TreeColumn_Index(column2max);

    /* The library scripts shouldn't pass an indicator column that is
     * one of the columns being dragged. */
    if (index2min >= index1min && index2min <= index1max)
	return index;

    index3 = TreeColumn_Index(column);

    /* D D D D D C C C I I I I I I I I */
    if (index1min < index2min) {
	if (index3 > index1max && index3 <= index2max)
	    return index - (index1max - index1min + 1);
	if (index3 >= index1min && index3 <= index1max)
	    return index + (index2max - index1max);

    /* I I I I I I I I C C C D D D D D */
    } else {
	if (index3 >= index2min && index3 < index1min)
	    return index + (index1max - index1min + 1);
	if (index3 >= index1min && index3 <= index1max)
	    return index - (index1min - index2min);
    }

    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_GetDraggedColumns --
 *
 *	Returns the first and last columns in the range of columns
 *	whose headers are being dragged.
 *	If the header isn't displaying drag-and-drop feedback, the
 *	result is always zero.
 *
 * Results:
 *	The number of columns which may be zero.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_GetDraggedColumns(
    TreeHeader header,		/* Header token. */
    int lock,			/* COLUMN_LOCK_XXX. */
    TreeColumn *first,		/* Out: First dragged column, or NULL. */
    TreeColumn *last		/* Out: Last dragged column, or NULL */
    )
{
    TreeCtrl *tree = header->tree;
    TreeColumn column1min, column1max;
    int index1min, index1max;

    if (tree->columnDrag.column == NULL)
	return 0;

    if (TreeColumn_Lock(tree->columnDrag.column) != lock)
	return 0;

    if (!header->columnDrag.draw)
	return 0;

    column1min = tree->columnDrag.column;
    column1max = GetFollowingColumn(column1min, tree->columnDrag.span, NULL);
    index1min = TreeColumn_Index(column1min);
    index1max = TreeColumn_Index(column1max);

    *first = column1min;
    *last = column1max;

    return index1max - index1min + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_IsDraggedColumn --
 *
 *	Returns TRUE if the header for a column is being dragged.
 *	If the header isn't displaying drag-and-drop feedback, the
 *	result is always FALSE.
 *
 * Results:
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_IsDraggedColumn(
    TreeHeader header,		/* Header token. */
    TreeColumn treeColumn	/* Column to test. */
    )
{
    TreeCtrl *tree = header->tree;
    TreeColumn column1min, column1max;
    int index1min, index1max, index3;

    if (tree->columnDrag.column == NULL)
	return 0;

    if (!header->columnDrag.draw)
	return 0;

    column1min = tree->columnDrag.column;
    column1max = GetFollowingColumn(column1min, tree->columnDrag.span, NULL);
    index1min = TreeColumn_Index(column1min);
    index1max = TreeColumn_Index(column1max);

    index3 = TreeColumn_Index(treeColumn);

    return index3 >= index1min && index3 <= index1max;
}

static void
RequiredDummyChangedProc(
    ClientData clientData,		/* Widget info. */
    int x, int y,			/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, int height,		/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imageWidth, int imageHeight	/* New dimensions of image. */
    )
{
}

/*
 *----------------------------------------------------------------------
 *
 * SetImageForColumn --
 *
 *	Sets a photo image to contain a picture of the header of a
 *	column. This image is used when dragging and dropping a column
 *	header.
 *
 * Results:
 *	Token for a photo image, or NULL if the image could not be
 *	created.
 *
 * Side effects:
 *	A photo image called "::TreeCtrl::ImageColumn" will be created if
 *	it doesn't exist. The image is set to contain a picture of the
 *	column header.
 *
 *----------------------------------------------------------------------
 */

static Tk_Image
SetImageForColumn(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    TreeColumn treeColumn,	/* Column token. */
    int indent,			/* */
    int width,			/* Width of the header and image */
    int height			/* Height of the header and image */
    )
{
    TreeCtrl *tree = header->tree;
    TreeItem item = header->item;
    Tk_PhotoHandle photoH;
    TreeDrawable td;
    XImage *ximage;
    char imageName[128];

    if ((column->dragImage != NULL) &&
	    (column->imageEpoch == tree->columnDrag.imageEpoch))
	return column->dragImage;

    sprintf(imageName, "::TreeCtrl::ImageColumnH%dC%d",
	TreeItem_GetID(tree, header->item), TreeColumn_GetID(treeColumn));
    column->dragImageName = Tk_GetUid(imageName);

    photoH = Tk_FindPhoto(tree->interp, imageName);
    if (photoH == NULL) {
	char buf[256];
	sprintf(buf, "image create photo %s", imageName);
	Tcl_GlobalEval(tree->interp, buf);
	photoH = Tk_FindPhoto(tree->interp, imageName);
	if (photoH == NULL)
	    return NULL;
    }

    td.width = width;
    td.height = height;
    td.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tree->tkwin),
	    width, height, Tk_Depth(tree->tkwin));

    {
	GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	TreeRectangle tr;

	TreeRect_SetXYWH(tr, 0, 0, width, height);
	Tree_FillRectangle(tree, td, NULL, gc, tr);
    }

    if (TreeItemColumn_GetStyle(tree, column->itemColumn) != NULL) {
	StyleDrawArgs drawArgs;
	int area = TREE_AREA_HEADER_NONE;
	switch (TreeColumn_Lock(treeColumn)) {
	    case COLUMN_LOCK_LEFT: area = TREE_AREA_HEADER_LEFT; break;
	    case COLUMN_LOCK_RIGHT: area = TREE_AREA_HEADER_RIGHT; break;
	}
	if (!Tree_AreaBbox(tree, area, &drawArgs.bounds)) {
	    TreeRect_SetXYWH(drawArgs.bounds, 0, 0, 0, 0);
	}
	drawArgs.tree = tree;
	drawArgs.item = item; /* needed for gradients */
	drawArgs.td = td;
	drawArgs.state = TreeItem_GetState(tree, item) |
	    TreeItemColumn_GetState(tree, column->itemColumn);
	drawArgs.style = TreeItemColumn_GetStyle(tree, column->itemColumn);
	drawArgs.indent = indent;
	drawArgs.x = 0;
	drawArgs.y = 0;
	drawArgs.width = width,
	drawArgs.height = height;
	drawArgs.justify = column->justify;
	drawArgs.column = treeColumn;
	TreeStyle_Draw(&drawArgs);
    }

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, td.drawable, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("tkTreeColumn.c:SetImageForColumn() ximage is NULL");

    /* XImage -> Tk_Image */
    Tree_XImage2Photo(tree->interp, photoH, ximage, 0, tree->columnDrag.alpha);

    XDestroyImage(ximage);
    Tk_FreePixmap(tree->display, td.drawable);

    column->dragImage = Tk_GetImage(tree->interp, tree->tkwin, imageName,
	RequiredDummyChangedProc, (ClientData) NULL);
    column->imageEpoch = tree->columnDrag.imageEpoch;
    return column->dragImage;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_Draw --
 *
 *	Draws a single header-column.  If the column header is being
 *	drawn at its drag position, a transparent image of the header
 *	is rendered overtop whatever is in the drawable.  Otherwise,
 *	the background is erased to the treectrl's -background color,
 *	then the style is drawn if the column header isn't part of
 *	the drag image (or the hidden tail column).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeaderColumn_Draw(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column,	/* Column token. */
    int visIndex,		/* 0-based index in the list of spans. */
    StyleDrawArgs *drawArgs,	/* Various args. */
    int dragPosition		/* TRUE if this header is being drawn at
				 * its drag position (i.e., offset by
				 * -imageoffset). */
    )
{
    TreeCtrl *tree = header->tree;
    TreeDrawable td = drawArgs->td;
    TreeColumn column1min, column1max;
    int index1min, index1max, index3;
    int x = drawArgs->x, y = drawArgs->y,
	width = drawArgs->width, height = drawArgs->height;
    int isDragColumn = 0, isHiddenTail;
    GC gc;
    TreeRectangle tr;

    if (header->columnDrag.draw == TRUE && tree->columnDrag.column != NULL) {
	column1min = tree->columnDrag.column;
	column1max = GetFollowingColumn(column1min, tree->columnDrag.span, NULL);
	index1min = TreeColumn_Index(column1min);
	index1max = TreeColumn_Index(column1max);
	index3 = TreeColumn_Index(drawArgs->column);

	isDragColumn = index3 >= index1min && index3 <= index1max;
    }

    /* Don't draw the tail column if it isn't visible.
     * Currently a span is always created for the tail column. */
    isHiddenTail = (drawArgs->column == tree->columnTail) &&
	!TreeColumn_Visible(drawArgs->column);

    if (!isDragColumn || !dragPosition) {
	gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	TreeRect_SetXYWH(tr, x, y, width, height);
	Tree_FillRectangle(tree, td, NULL, gc, tr);
    }

    if ((drawArgs->style != NULL) && !isDragColumn && !isHiddenTail) {
	StyleDrawArgs drawArgsCopy = *drawArgs;
	TreeStyle_Draw(&drawArgsCopy);
    }

    if (isDragColumn && dragPosition) {
	Tk_Image image;
	image = SetImageForColumn(header, column, drawArgs->column, 0,
	    width, height);
	if (image != NULL) {
	    Tree_RedrawImage(image, 0, 0, width, height, td, x, y);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_Justify --
 *
 *	Returns the value of the -justify option for a header-column.
 *
 * Results:
 *	A Tk_Justify value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Justify
TreeHeaderColumn_Justify(
    TreeHeader header,		/* Header token. */
    TreeHeaderColumn column	/* Column token. */
    )
{
    return column->justify;
}

/*
 *----------------------------------------------------------------------
 *
 * Header_Configure --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a TreeHeader.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for 'header';  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Header_Configure(
    TreeHeader header,		/* Header token */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the TreeHeader is being created. */
    )
{
    TreeCtrl *tree = header->tree;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    int objC = 0, iObjC = 0;
    Tcl_Obj *staticObjV[STATIC_SIZE], **objV = staticObjV;
    Tcl_Obj *staticIObjV[STATIC_SIZE], **iObjV = staticIObjV;
    int i, oldVisible = TreeItem_ReallyVisible(tree, header->item);

    /* Hack -- Pass all unknown options to the underlying item. */
    STATIC_ALLOC(objV, Tcl_Obj *, objc);
    STATIC_ALLOC(iObjV, Tcl_Obj *, objc);
    for (i = 0; i < objc; i += 2) {
	Tk_OptionSpec *specPtr = headerSpecs;
	int length;
	CONST char *optionName = Tcl_GetStringFromObj(objv[i], &length);
	while (specPtr->type != TK_OPTION_END) {
	    if (strncmp(specPtr->optionName, optionName, length) == 0) {
		objV[objC++] = objv[i];
		if (i + 1 < objc)
		    objV[objC++] = objv[i + 1];
		break;
	    }
	    specPtr++;
	}
	if (specPtr->type == TK_OPTION_END) {
	    iObjV[iObjC++] = objv[i];
	    if (i + 1 < objc)
		iObjV[iObjC++] = objv[i + 1];
	}
    }
    if (TreeItem_ConsumeHeaderConfig(tree, header->item, iObjC, iObjV) != TCL_OK) {
	STATIC_FREE(objV, Tcl_Obj *, objc);
	STATIC_FREE(iObjV, Tcl_Obj *, objc);
	return TCL_ERROR;
    }

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) header,
			tree->headerOptionTable, objC, objV, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
	    }

	    /*
	     * Step 1: Save old values
	     */

	    /*
	     * Step 2: Process new values
	     */

	    /*
	     * Step 3: Free saved values
	     */

	    Tk_FreeSavedOptions(&savedOptions);

	    STATIC_FREE(objV, Tcl_Obj *, objc);
	    STATIC_FREE(iObjV, Tcl_Obj *, objc);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /*
	     * Free new values.
	     */

	    /*
	     * Restore old values.
	     */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);

	    STATIC_FREE(objV, Tcl_Obj *, objc);
	    STATIC_FREE(iObjV, Tcl_Obj *, objc);
	    return TCL_ERROR;
	}
    }

    if (oldVisible != TreeItem_ReallyVisible(tree, header->item)) {
	tree->headerHeight = -1;
	Tree_FreeItemDInfo(tree, header->item, NULL);
	TreeColumns_InvalidateWidth(tree);
	Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_CreateWithItemColumn --
 *
 *	Allocates and initializes a new header-column.
 *
 * Results:
 *	A token for the new header-column.
 *
 * Side effects:
 *	Memory allocation, option initialization.
 *
 *----------------------------------------------------------------------
 */

TreeHeaderColumn
TreeHeaderColumn_CreateWithItemColumn(
    TreeHeader header,			/* Header token. */
    TreeItemColumn itemColumn		/* Newly-created item-column to
					 * assocate the new header-column
					 * with. */
    )
{
    TreeCtrl *tree = header->tree;
    TreeHeaderColumn column;

    column = (TreeHeaderColumn) ckalloc(sizeof(HeaderColumn));
    memset(column, '\0', sizeof(HeaderColumn));
    if (Tree_InitOptions(tree, STATE_DOMAIN_HEADER, column,
	    tree->headerColumnOptionTable) != TCL_OK) {
	WFREE(column, HeaderColumn);
	return NULL;
    }
    /* FIXME: should call Column_Configure to handle any option-database tomfoolery */
    column->itemColumn = itemColumn;
    tree->headerHeight = -1;
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_CreateWithItem --
 *
 *	Allocates and initializes a new row of column headers.
 *
 * Results:
 *	A token for the new header.
 *
 * Side effects:
 *	Memory allocation, option initialization.
 *
 *----------------------------------------------------------------------
 */

TreeHeader
TreeHeader_CreateWithItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Newly-created item to associate the
				 * new header with. */
    )
{
    TreeHeader header;

    header = (TreeHeader) ckalloc(sizeof(TreeHeader_));
    memset(header, '\0', sizeof(TreeHeader_));
    if (Tk_InitOptions(tree->interp, (char *) header,
	    tree->headerOptionTable, tree->tkwin) != TCL_OK) {
	WFREE(header, TreeHeader_);
	return NULL;
    }
    if (Tk_InitOptions(tree->interp, (char *) header,
	    tree->headerDragOptionTable, tree->tkwin) != TCL_OK) {
	Tk_FreeConfigOptions((char *) header, tree->headerOptionTable,
	    tree->tkwin);
	WFREE(header, TreeHeader_);
	return NULL;
    }
    header->tree = tree;
    header->item = item;
    return header;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeDragImages --
 *
 *	Free the drag-and-drop images for all header-columns that have
 *	one.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Images may be freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeDragImages(
    TreeCtrl *tree
    )
{
    TreeItem item;
    TreeItemColumn itemColumn;

    for (item = tree->headerItems;
	    item != NULL;
	    item = TreeItem_GetNextSibling(tree, item)) {
	for (itemColumn = TreeItem_GetFirstColumn(tree, item);
		itemColumn != NULL;
		itemColumn = TreeItemColumn_GetNext(tree, itemColumn)) {
	    TreeHeaderColumn column = TreeItemColumn_GetHeaderColumn(tree,
		itemColumn);
	    if (column->dragImage != NULL) {
		Tk_FreeImage(column->dragImage);
		Tk_DeleteImage(tree->interp, column->dragImageName);
		column->dragImage = NULL;
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ColumnDeleted --
 *
 *	Called when a tree-column is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeader_ColumnDeleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn	/* Column being deleted. */
    )
{
    if (treeColumn == tree->columnDrag.column) {
	FreeDragImages(tree);
	tree->columnDrag.column = NULL;
    }
    if (treeColumn == tree->columnDrag.indColumn)
	tree->columnDrag.indColumn = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_FreeResources --
 *
 *	Frees any memory and options associated with a header-column.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeaderColumn_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    TreeHeaderColumn column	/* Column token. */
    )
{
    if (column->image != NULL)
	Tree_FreeImage(tree, column->image);
    if (column->dragImage != NULL) {
	Tk_FreeImage(column->dragImage);
	Tk_DeleteImage(tree->interp, column->dragImageName);
    }

    Tk_FreeConfigOptions((char *) column, tree->headerColumnOptionTable,
	tree->tkwin);
    WFREE(column, HeaderColumn);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_FreeResources --
 *
 *	Frees any memory and options associated with a header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeader_FreeResources(
    TreeHeader header		/* Header to free. */
    )
{
    TreeCtrl *tree = header->tree;

    Tk_FreeConfigOptions((char *) header, tree->headerOptionTable,
	tree->tkwin);
    Tk_FreeConfigOptions((char *) header, tree->headerDragOptionTable,
	tree->tkwin);
    WFREE(header, TreeHeader_);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaders_RequestWidthInColumns --
 *
 *	Calculates the width needed by styles in a range of columns
 *	for every visible header-row.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeaders_RequestWidthInColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn columnMin,
    TreeColumn columnMax
    )
{
    TreeItem item = tree->headerItems;

    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item)) {
	    TreeItem_RequestWidthInColumns(tree, item, columnMin, columnMax);
	}
	item = TreeItem_GetNextSibling(tree, item);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_HeaderHeight --
 *
 *	Return the total height of the column header area. The height
 *	is only recalculated if it is marked out-of-date.
 *
 * Results:
 *	Pixel height. Will be zero if there are no visible headers.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_HeaderHeight(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItem item = tree->headerItems;
    int totalHeight = 0;

    if (!tree->showHeader)
	return 0;

    if (tree->headerHeight >= 0)
	return tree->headerHeight;

    while (item != NULL) {
	totalHeight += TreeItem_Height(tree, item);
	item = TreeItem_GetNextSibling(tree, item);
    }

    return tree->headerHeight = totalHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_HeaderUnderPoint --
 *
 *	Returns the header containing the given coordinates.
 *
 * Results:
 *	TreeItem or NULL if the point is outside any header.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
Tree_HeaderUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_, int *y_,		/* In: window coordinates.
				 * Out: coordinates relative to top-left
				 * corner of the returned column. */
    int *lock			/* Returned COLUMN_LOCK_XXX. */
    )
{
    int y;
    TreeItem item;

    if (Tree_HitTest(tree, *x_, *y_) != TREE_AREA_HEADER)
	return NULL;

    y = Tree_BorderTop(tree);
    item = tree->headerItems;
    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextSiblingVisible(tree, item);
    while (item != NULL) {
	if (*y_ < y + TreeItem_Height(tree, item)) {
	    /* Right-locked columns are drawn over left-locked ones. */
	    /* Left-locked columns are drawn over unlocked ones. */
	    if (*x_ >= Tree_ContentRight(tree)) {
		(*x_) -= Tree_ContentRight(tree);
		(*lock) = COLUMN_LOCK_RIGHT;
	    } else if (*x_ < Tree_ContentLeft(tree)) {
		(*x_) -= Tree_BorderLeft(tree);
		(*lock) = COLUMN_LOCK_LEFT;
	    } else {
		(*x_) += tree->xOrigin /*- tree->canvasPadX[PAD_TOP_LEFT]*/;
		(*lock) = COLUMN_LOCK_NONE;
	    }
	    (*y_) = (*y_) - y;
	    return item;
	}
	y += TreeItem_Height(tree, item);
	item = TreeItem_NextSiblingVisible(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderColumn_FromObj --
 *
 *	Parses a column description into a header-column token.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderColumn_FromObj(
    TreeHeader header,		/* Header token. */
    Tcl_Obj *objPtr,		/* Object to parse to a column. */
    TreeHeaderColumn *columnPtr	/* Returned column. */
    )
{
    TreeCtrl *tree = header->tree;
    TreeColumn treeColumn;
    TreeItemColumn itemColumn;

    if (TreeColumn_FromObj(tree, objPtr, &treeColumn, CFO_NOT_NULL) != TCL_OK)
	return TCL_ERROR;
    itemColumn = TreeItem_FindColumn(tree, header->item,
	TreeColumn_Index(treeColumn));
    (*columnPtr) = TreeItemColumn_GetHeaderColumn(tree, itemColumn);
    return TCL_OK;
}

typedef struct Qualifiers {
    TreeCtrl *tree;
    int visible;		/* 1 for -visible TRUE,
				   0 for -visible FALSE,
				   -1 for unspecified. */
    TagExpr expr;		/* Tag expression. */
    int exprOK;			/* TRUE if expr is valid. */
    Tk_Uid tag;			/* Tag (without operators) or NULL. */
} Qualifiers;

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Init --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Init(
    TreeCtrl *tree,		/* Widget info. */
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    q->tree = tree;
    q->visible = -1;
    q->exprOK = FALSE;
    q->tag = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Scan --
 *
 *	Helper routine for TreeHeaderList_FromObj.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Qualifiers_Scan(
    Qualifiers *q,		/* Must call Qualifiers_Init first,
				 * and Qualifiers_Free if result is TCL_OK. */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Argument values. */
    int startIndex,		/* First objv[] index to look at. */
    int *argsUsed		/* Out: number of objv[] used. */
    )
{
    TreeCtrl *tree = q->tree;
    Tcl_Interp *interp = tree->interp;
    int qual, j = startIndex;

    static CONST char *qualifiers[] = {
	"tag", "visible", "!visible", NULL
    };
    enum qualEnum {
	QUAL_TAG, QUAL_VISIBLE, QUAL_NOT_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 1, 1
    };

    *argsUsed = 0;

    for (; j < objc; ) {
	if (Tcl_GetIndexFromObj(NULL, objv[j], qualifiers, NULL, 0,
		&qual) != TCL_OK)
	    break;
	if (objc - j < qualArgs[qual]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(objv[j]), "\" qualifier", NULL);
	    goto errorExit;
	}
	switch ((enum qualEnum) qual) {
	    case QUAL_TAG: {
		if (tree->columnTagExpr) {
		    if (q->exprOK)
			TagExpr_Free(&q->expr);
		    if (TagExpr_Init(tree, objv[j + 1], &q->expr) != TCL_OK)
			return TCL_ERROR;
		    q->exprOK = TRUE;
		} else {
		    q->tag = Tk_GetUid(Tcl_GetString(objv[j + 1]));
		}
		break;
	    }
	    case QUAL_VISIBLE: {
		q->visible = 1;
		break;
	    }
	    case QUAL_NOT_VISIBLE: {
		q->visible = 0;
		break;
	    }
	}
	*argsUsed += qualArgs[qual];
	j += qualArgs[qual];
    }
    return TCL_OK;
errorExit:
    if (q->exprOK)
	TagExpr_Free(&q->expr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifies --
 *
 *	Helper routine for TreeHeaderList_FromObj.
 *
 * Results:
 *	Returns TRUE if the header meets the given criteria.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Qualifies(
    Qualifiers *q,		/* Qualifiers to check. */
    TreeItem header		/* The header to test. May be NULL. */
    )
{
    TreeCtrl *tree = q->tree;
    /* Note: if the header is NULL it is a "match" because we have run
     * out of headers to check. */
    if (header == NULL)
	return 1;
    if ((q->visible == 1) && !TreeItem_ReallyVisible(tree, header))
	return 0;
    else if ((q->visible == 0) && TreeItem_ReallyVisible(tree, header))
	return 0;
    if (q->exprOK && !TagExpr_Eval(&q->expr, TreeItem_GetTagInfo(tree, header)))
	return 0;
    if ((q->tag != NULL) && !TreeItem_HasTag(header, q->tag))
	return 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Free --
 *
 *	Helper routine for TreeHeaderList_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Free(
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    if (q->exprOK)
	TagExpr_Free(&q->expr);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderList_FromObj --
 *
 *	Parses a header description into a list of zero or more header
 *	tokens.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderList_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to a header. */
    TreeItemList *items,	/* Uninitialized item list. Caller must free
				 * it with TreeItemList_Free unless the
				 * result of this function is TCL_ERROR. */
    int flags			/* IFO_xxx flags */
    )
{
    Tcl_Interp *interp = tree->interp;
    static CONST char *indexName[] = {
	"all", "end", "first", "last", (char *) NULL
    };
    enum indexEnum {
	INDEX_ALL, INDEX_END, INDEX_FIRST, INDEX_LAST
    };
    /* Number of arguments used by indexName[]. */
    static int indexArgs[] = {
	1, 1, 1, 1
    };
    /* Boolean: can indexName[] be followed by 1 or more qualifiers. */
    static int indexQual[] = {
	1, 1, 1, 1
    };
    int id, index, listIndex, objc;
    Tcl_Obj **objv, *elemPtr;
    TreeItem item = NULL;
    Qualifiers q;
    int qualArgsTotal;

    TreeItemList_Init(tree, items, 0);
    Qualifiers_Init(tree, &q);

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto baditem;
    if (objc == 0)
	goto baditem;

    listIndex = 0;
    elemPtr = objv[listIndex];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index)
	    == TCL_OK) {

	if (objc - listIndex < indexArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" keyword", NULL);
	    goto errorExit;
	}

	qualArgsTotal = 0;
	if (indexQual[index]) {
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + indexArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum indexEnum) index) {
	    case INDEX_ALL: {
		item = tree->headerItems;
		while (item != NULL) {
		    if (!qualArgsTotal || Qualifies(&q, item)) {
			TreeItemList_Append(items, item);
		    }
		    item = TreeItem_GetNextSibling(tree, item);
		}
		item = NULL;
		break;
	    }
	    case INDEX_FIRST: {
		item = tree->headerItems;
		while (!Qualifies(&q, item))
		    item = TreeItem_GetNextSibling(tree, item);
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST: {
		TreeItem walk = tree->headerItems;
		while (walk != NULL) {
		    if (Qualifies(&q, walk))
			item = walk;
		    walk = TreeItem_GetNextSibling(tree, walk);
		}
		break;
	    }
	}
	listIndex += indexArgs[index] + qualArgsTotal;

    /* No indexName[] was found. */
    } else {
	int gotId = FALSE;

	/* Try an item ID. */
	if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK)
	    gotId = TRUE;

	if (gotId) {
	    item = tree->headerItems;
	    while (item != NULL) {
		if (TreeItem_GetID(tree, item) == id)
		    break;
		item = TreeItem_GetNextSibling(tree, item);
	    }
	    goto gotFirstPart;
	}

	/* Try a list of qualifiers. This has the same effect as
	 * "all QUALIFIERS". */
	if (Qualifiers_Scan(&q, objc, objv, listIndex, &qualArgsTotal)
		!= TCL_OK) {
	    goto errorExit;
	}
	if (qualArgsTotal) {
	    item = tree->headerItems;
	    while (item != NULL) {
		if (Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		item = TreeItem_GetNextSibling(tree, item);
	    }
	    item = NULL;
	    listIndex += qualArgsTotal;
	    goto gotFirstPart;
	}

	/* Try a tag or tag expression followed by qualifiers. */
	if (objc > 1) {
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + 1,
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}
	if (tree->itemTagExpr) { /* FIXME: headerTagExpr */
	    TagExpr expr;
	    if (TagExpr_Init(tree, elemPtr, &expr) != TCL_OK)
		goto errorExit;
	    item = tree->headerItems;
	    while (item != NULL) {
		if (TagExpr_Eval(&expr, TreeItem_GetTagInfo(tree, item)) &&
			Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		item = TreeItem_GetNextSibling(tree, item);
	    }
	    TagExpr_Free(&expr);
	} else {
	    Tk_Uid tag = Tk_GetUid(Tcl_GetString(elemPtr));
	    item = tree->headerItems;
	    while (item != NULL) {
		if (TreeItem_HasTag(item, tag) && Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		item = TreeItem_GetNextSibling(tree, item);
	    }
	}
	item = NULL;
	listIndex += 1 + qualArgsTotal;
    }

gotFirstPart:

    /* This means a valid specification was given, but there is no such item */
    if ((TreeItemList_Count(items) == 0) && (item == NULL)) {
	if (flags & IFO_NOT_NULL)
	    goto noitem;
	/* Empty list returned */
	goto goodExit;
    }

    if ((flags & IFO_NOT_MANY) && (TreeItemList_Count(items) > 1)) {
	FormatResult(interp, "can't specify > 1 header for this command");
	goto errorExit;
    }

    if (item != NULL)
	TreeItemList_Append(items, item);

goodExit:
    Qualifiers_Free(&q);
    return TCL_OK;

baditem:
    Tcl_AppendResult(interp, "bad header description \"", Tcl_GetString(objPtr),
	    "\"", NULL);
    goto errorExit;

noitem:
    Tcl_AppendResult(interp, "header \"", Tcl_GetString(objPtr),
	    "\" doesn't exist", NULL);

errorExit:
    Qualifiers_Free(&q);
    TreeItemList_Free(items);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_FromObj --
 *
 *	Parses a header description into a single header token.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to a header. */
    TreeHeader *headerPtr	/* Out: returned header, always non-NULL
				 * unless an error occurs. */
    )
{
    TreeItemList items;
    TreeItem item;

    if (TreeHeaderList_FromObj(tree, objPtr, &items, IFO_NOT_MANY |
	    IFO_NOT_NULL) != TCL_OK)
	return TCL_ERROR;
    item = TreeItemList_Nth(&items, 0);
    (*headerPtr) = TreeItem_GetHeader(tree, item);
    TreeItemList_Free(&items);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_GetItem --
 *
 *	Get the underlying item for a header.
 *
 * Results:
 *	The TreeItem that is the back-end of a header.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeHeader_GetItem(
    TreeHeader header		/* Header token. */
    )
{
    return header->item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_ToObj --
 *
 *	Convert a TreeHeader to a Tcl_Obj.
 *
 * Results:
 *	A new Tcl_Obj representing the TreeHeader.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeHeader_ToObj(
    TreeHeader header		/* Header token. */
    )
{
    TreeCtrl *tree = header->tree;
#if 0
    if (tree->itemPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->itemPrefix, item->id);
	return Tcl_NewStringObj(buf, -1);
    }
#endif
    return Tcl_NewIntObj(TreeItem_GetID(tree, header->item));
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderCmd_Create --
 *
 *	This procedure is invoked to process the [header create] widget
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TreeHeaderCmd_Create(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeItem item;
    TreeHeader header;

    item = TreeItem_CreateHeader(tree);
    header = TreeItem_GetHeader(tree, item);
    if (Header_Configure(header, objc - 3, objv + 3, TRUE) != TCL_OK) {
	TreeItem_Delete(tree, item);
	return TCL_ERROR;
    }
    tree->headerHeight = -1;
    Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderCmd_Cget --
 *
 *	This procedure is invoked to process the [header cget] widget
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TreeHeaderCmd_Cget(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeHeader header;
    TreeHeaderColumn column;
    Tcl_Obj *resultObjPtr;

    if (objc < 5 || objc > 6) {
	Tcl_WrongNumArgs(interp, 3, objv, "header ?column? option");
	return TCL_ERROR;
    }

    if (TreeHeader_FromObj(tree, objv[3], &header) != TCL_OK)
	return TCL_ERROR;

    /* T header cget H option */
    if (objc == 5) {
	{
	    Tk_OptionSpec *specPtr = headerSpecs;
	    int length;
	    CONST char *optionName = Tcl_GetStringFromObj(objv[4], &length);
	    while (specPtr->type != TK_OPTION_END) {
		if (strncmp(specPtr->optionName, optionName, length) == 0) {
		    break;
		}
		specPtr++;
	    }
	    if (specPtr->type == TK_OPTION_END) {
		return TreeItem_ConsumeHeaderCget(tree, header->item, objv[4]);
	    }
	}
	resultObjPtr = Tk_GetOptionValue(interp, (char *) header,
		tree->headerOptionTable, objv[4], tree->tkwin);
	if (resultObjPtr == NULL)
	    return TCL_ERROR;
	Tcl_SetObjResult(interp, resultObjPtr);
    } else {
	/* T header cget H C option */
	if (TreeHeaderColumn_FromObj(header, objv[4], &column) != TCL_OK)
	    return TCL_ERROR;
	resultObjPtr = Tk_GetOptionValue(interp, (char *) column,
	    tree->headerColumnOptionTable, objv[5], tree->tkwin);
	if (resultObjPtr == NULL)
	    return TCL_ERROR;
	Tcl_SetObjResult(interp, resultObjPtr);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderCmd_Configure --
 *
 *	This procedure is invoked to process the [header configure]
 *	widget command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TreeHeaderCmd_Configure(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeHeader header;
    TreeHeaderColumn column;
    Tcl_Obj *resultObjPtr;
    CONST char *s;
    TreeItem item;
    TreeItemList items;
    ItemForEach iter;
    TreeColumnList columns;
    TreeColumn treeColumn;
    ColumnForEach citer;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 3, objv,
	    "header ?column? ?option? ?value? ?option value ...?");
	return TCL_ERROR;
    }

    /* T header configure H */
    if (objc == 4) {
	if (TreeHeader_FromObj(tree, objv[3], &header) != TCL_OK)
	    return TCL_ERROR;
	/* Return the combined [header configure] and [item configure] */
	resultObjPtr = Tk_GetOptionInfo(interp, (char *) header,
	    tree->headerOptionTable,(Tcl_Obj *) NULL, tree->tkwin);
	if (resultObjPtr == NULL)
	    return TCL_ERROR;
	if (TreeItem_GetHeaderOptionInfo(tree, header, NULL, resultObjPtr)
		!= TCL_OK)
	    return TCL_ERROR;
	Tcl_SetObjResult(interp, resultObjPtr);
	return TCL_OK;
    }

    s = Tcl_GetString(objv[4]);
    if (s[0] == '-') {

	/* T header configure H -option */
	if (objc == 5) {
	    if (TreeHeader_FromObj(tree, objv[3], &header) != TCL_OK)
		return TCL_ERROR;
	    if (TreeItem_GetHeaderOptionInfo(tree, header, objv[4], NULL)
		    == TCL_OK)
		return TCL_OK;
	    Tcl_ResetResult(interp);
	    resultObjPtr = Tk_GetOptionInfo(interp, (char *) header,
		tree->headerOptionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);

	/* T header configure H -option value ... */
	} else {
	    if (TreeHeaderList_FromObj(tree, objv[3], &items, 0) != TCL_OK)
		return TCL_ERROR;
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		header = TreeItem_GetHeader(tree, item);
		if (Header_Configure(header, objc - 4, objv + 4, FALSE)
			!= TCL_OK) {
		    TreeItemList_Free(&items);
		    return TCL_ERROR;
		}
	    }
	    TreeItemList_Free(&items);
	}
	return TCL_OK;
    }

    /* T header configure H C ?-option? */
    if (objc <= 6) {
	if (TreeHeader_FromObj(tree, objv[3], &header) != TCL_OK)
	    return TCL_ERROR;
	if (TreeHeaderColumn_FromObj(header, objv[4], &column) != TCL_OK)
	    return TCL_ERROR;
	resultObjPtr = Tk_GetOptionInfo(interp, (char *) column,
	    tree->headerColumnOptionTable,
	    (objc == 5) ? (Tcl_Obj *) NULL : objv[5], tree->tkwin);
	if (resultObjPtr == NULL)
	    return TCL_ERROR;
	Tcl_SetObjResult(interp, resultObjPtr);

    /* T header configure H C -option value ... */
    } else {
	if (TreeHeaderList_FromObj(tree, objv[3], &items, 0) != TCL_OK)
	    return TCL_ERROR;
	if (TreeColumnList_FromObj(tree, objv[4], &columns, 0) != TCL_OK) {
	    TreeItemList_Free(&items);
	    return TCL_ERROR;
	}
	ITEM_FOR_EACH(item, &items, NULL, &iter) {
	    header = TreeItem_GetHeader(tree, item);
	    COLUMN_FOR_EACH(treeColumn, &columns, NULL, &citer) {
		TreeItemColumn itemColumn = TreeItem_FindColumn(tree, item,
		    TreeColumn_Index(treeColumn));
		column = TreeItemColumn_GetHeaderColumn(tree, itemColumn);
		if (Column_Configure(header, column, treeColumn, objc - 5,
			objv + 5, FALSE) != TCL_OK) {
		    TreeItemList_Free(&items);
		    TreeColumnList_Free(&columns);
		    return TCL_ERROR;
		}
	    }
	}
	TreeItemList_Free(&items);
	TreeColumnList_Free(&columns);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeaderCmd --
 *
 *	This procedure is invoked to process the [header] widget
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeaderCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"bbox", "cget", "compare", "configure", "count", "create", "delete",
	"dragcget", "dragconfigure", "element", "id", "image", "span",
	"state", "style", "tag", "text", (char *) NULL
    };
    enum {
	COMMAND_BBOX, COMMAND_CGET, COMMAND_COMPARE, COMMAND_CONFIGURE,
	COMMAND_COUNT, COMMAND_CREATE, COMMAND_DELETE, COMMAND_DRAGCGET,
	COMMAND_DRAGCONF, COMMAND_ELEMENT, COMMAND_ID, COMMAND_IMAGE,
	COMMAND_SPAN, COMMAND_STATE, COMMAND_STYLE, COMMAND_TAG, COMMAND_TEXT
    };
    int index;
    TreeHeader header;
    TreeItemList items;
    TreeItem item;
    ItemForEach iter;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    /* FIXME: Tree_PreserveItems? */

    switch (index) {
	/* T header bbox I ?C? ?E? */
	case COMMAND_BBOX:
	    return TreeItemCmd_Bbox(tree, objc, objv, TRUE);

	case COMMAND_CREATE:
	    return TreeHeaderCmd_Create(clientData, interp, objc, objv);

	/* T header cget H ?C? option */
	case COMMAND_CGET:
	    return TreeHeaderCmd_Cget(clientData, interp, objc, objv);

	/* T header compare H op H */
	case COMMAND_COMPARE: {
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=",
		NULL };
	    enum { COP_LT, COP_LE, COP_EQ, COP_GE, COP_GT, COP_NE };
	    int op, compare = 0, index1 = 0, index2 = 0;
	    TreeHeader header1, header2;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 3, objv, "header1 op header2");
		return TCL_ERROR;
	    }

	    if (TreeHeader_FromObj(tree, objv[3], &header1) != TCL_OK)
		return TCL_ERROR;

	    if (Tcl_GetIndexFromObj(interp, objv[4], opName,
		    "comparison operator", 0, &op) != TCL_OK) {
		return TCL_ERROR;
	    }

	    if (TreeHeader_FromObj(tree, objv[5], &header2) != TCL_OK)
		return TCL_ERROR;

	    if (op != COP_EQ && op != COP_NE) {
		for (item = tree->headerItems; item != header1->item;
			item = TreeItem_GetNextSibling(tree, item)) {
		    index1++;
		}
		for (item = tree->headerItems; item != header2->item;
			item = TreeItem_GetNextSibling(tree, item)) {
		    index2++;
		}
	    }
	    switch (op) {
		case COP_LT:
		    compare = index1 < index2;
		    break;
		case COP_LE:
		    compare = index1 <= index2;
		    break;
		case COP_EQ:
		    compare = header1 == header2;
		    break;
		case COP_GE:
		    compare = index1 >= index2;
		    break;
		case COP_GT:
		    compare = index1 > index2;
		    break;
		case COP_NE:
		    compare = header1 != header2;
		    break;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(compare));
	    break;
	}

	/* T header configure H ?C? ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE:
	    return TreeHeaderCmd_Configure(clientData, interp, objc, objv);

	/* T header count ?H? */
	case COMMAND_COUNT: {
	    int count = tree->headerCount;

	    if (objc > 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?headerDesc?");
		return TCL_ERROR;
	    }

	    if (objc == 4) {
		if (TreeHeaderList_FromObj(tree, objv[3], &items, 0) != TCL_OK)
		    return TCL_ERROR;
		count = 0;
		ITEM_FOR_EACH(item, &items, NULL, &iter) {
		    count++;
		}
		TreeItemList_Free(&items);
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(count));
	    break;
	}

	case COMMAND_DELETE: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "header");
		return TCL_ERROR;
	    }
	    if (TreeHeaderList_FromObj(tree, objv[3], &items, 0) != TCL_OK)
		return TCL_ERROR;

	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		/* The default header can't be deleted */
		if (item == tree->headerItems)
		    continue;

		if (TreeItem_ReallyVisible(tree, item)) {
		    TreeColumns_InvalidateWidth(tree);
		    TreeColumns_InvalidateSpans(tree);
		    /* TreeItem_Delete will call TreeItem_FreeResources which
		     * will call Tree_FreeItemDInfo will will set tree->headerHeight=-1
		     * and set DINFO_DRAW_HEADER. */
		}

		/* FIXME: ITEM_FLAG_DELETED */
		TreeItem_Delete(tree, item);
	    }
	    TreeItemList_Free(&items);
	    break;
	}

	/* T header dragcget ?header? option */
	case COMMAND_DRAGCGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?header? option");
		return TCL_ERROR;
	    }
	    if (objc == 5) {
		if (TreeHeader_FromObj(tree, objv[3], &header) != TCL_OK)
		    return TCL_ERROR;
		resultObjPtr = Tk_GetOptionValue(interp, (char *) header,
			tree->headerDragOptionTable, objv[4], tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->columnDrag.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T header dragconfigure ?header? ?option? ?value? ?option value ...? */
	case COMMAND_DRAGCONF: {
	    Tcl_Obj *resultObjPtr;
	    Tk_SavedOptions savedOptions;
	    int mask, result, flags = 0;
	    CONST char *s;

	    if (objc == 3) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->columnDrag.optionTable,
			(Tcl_Obj *) NULL,
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    s = Tcl_GetString(objv[3]);
	    if (s[0] == '-') {
		int alpha = tree->columnDrag.alpha;
		TreeColumn dragColumn = tree->columnDrag.column;

		if (objc <= 4) {
		    resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			    tree->columnDrag.optionTable,
			    (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			    tree->tkwin);
		    if (resultObjPtr == NULL)
			return TCL_ERROR;
		    Tcl_SetObjResult(interp, resultObjPtr);
		    break;
		}
		result = Tk_SetOptions(interp, (char *) tree,
			tree->columnDrag.optionTable, objc - 3, objv + 3,
			tree->tkwin, &savedOptions, &mask);
		if (result != TCL_OK) {
		    Tk_RestoreSavedOptions(&savedOptions);
		    return TCL_ERROR;
		}
		Tk_FreeSavedOptions(&savedOptions);

		if (tree->columnDrag.alpha < 0)
		    tree->columnDrag.alpha = 0;
		if (tree->columnDrag.alpha > 255)
		    tree->columnDrag.alpha = 255;

		if (alpha != tree->columnDrag.alpha)
		    tree->columnDrag.imageEpoch++;

		/* Free header drag images if -imagecolumn changes to "" */
		if ((dragColumn != NULL) && (tree->columnDrag.column == NULL)) {
		    FreeDragImages(tree);
		}

		for (item = tree->headerItems;
			item != NULL;
			item = TreeItem_GetNextSibling(tree, item)) {
		    header = TreeItem_GetHeader(tree, item);
		    if (header->columnDrag.draw)
			Tree_InvalidateItemDInfo(tree, NULL, item, NULL);
		}
		break;
	    }
	    if (objc < 6)
		flags |= IFO_NOT_MANY | IFO_NOT_NULL;
	    if (TreeHeaderList_FromObj(tree, objv[3], &items, flags) != TCL_OK)
		return TCL_ERROR;
	    if (objc <= 5) {
		item = TreeItemList_Nth(&items, 0);
		header = TreeItem_GetHeader(tree, item);
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) header,
			tree->headerDragOptionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		header = TreeItem_GetHeader(tree, item);
		result = Tk_SetOptions(interp, (char *) header,
			tree->headerDragOptionTable, objc - 4, objv + 4,
			tree->tkwin, &savedOptions, &mask);
		if (result != TCL_OK) {
		    Tk_RestoreSavedOptions(&savedOptions);
		    TreeItemList_Free(&items);
		    return TCL_ERROR;
		}
		Tk_FreeSavedOptions(&savedOptions);
		Tree_InvalidateItemDInfo(tree, NULL, item, NULL);
	    }
/*	    Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);*/
	    TreeItemList_Free(&items);
	    break;
	}

	case COMMAND_ELEMENT:
	    return TreeItemCmd_Element(tree, objc, objv, TRUE);

	/* T header id H */
	case COMMAND_ID: {
	    Tcl_Obj *listObj;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "header");
		return TCL_ERROR;
	    }

	    if (TreeHeaderList_FromObj(tree, objv[3], &items, 0) != TCL_OK)
		return TCL_ERROR;

	    listObj = Tcl_NewListObj(0, NULL);
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
	    }
	    TreeItemList_Free(&items);
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_IMAGE:
	    return TreeItemCmd_ImageOrText(tree, objc, objv, TRUE, TRUE);

	case COMMAND_SPAN:
	    return TreeItemCmd_Span(tree, objc, objv, TRUE);

	case COMMAND_STATE:
	    return TreeItemCmd_State(tree, objc, objv, TRUE);

	case COMMAND_STYLE:
	    return TreeItemCmd_Style(tree, objc, objv, TRUE);

	case COMMAND_TAG:
	    return TreeItemCmd_Tag(tree, objc, objv, TRUE);

	case COMMAND_TEXT:
	    return TreeItemCmd_ImageOrText(tree, objc, objv, FALSE, TRUE);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_TreeChanged --
 *
 *	Called when a TreeCtrl is configured. Performs any relayout
 *	necessary on column headers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeader_TreeChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flagT			/* TREE_CONF_xxx flags. */
    )
{
    if (!(flagT & (TREE_CONF_FONT | TREE_CONF_RELAYOUT)))
	return;

    tree->headerHeight = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_InitWidget --
 *
 *	Perform header-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeHeader_InitWidget(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Tk_OptionSpec *specPtr;
    Tcl_DString dString;

    Tcl_InitHashTable(&tree->headerHash, TCL_ONE_WORD_KEYS);

    specPtr = Tree_FindOptionSpec(columnSpecs, "-background");
    if (specPtr->defValue == NULL) {
	Tcl_DStringInit(&dString);
	Tcl_DStringAppendElement(&dString, DEF_BUTTON_BG_COLOR);
	Tcl_DStringAppendElement(&dString, "normal");
	Tcl_DStringAppendElement(&dString, DEF_BUTTON_ACTIVE_BG_COLOR);
	Tcl_DStringAppendElement(&dString, "");
	specPtr->defValue = ckalloc(Tcl_DStringLength(&dString) + 1);
	strcpy((char *)specPtr->defValue, Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
    }

    PerStateCO_Init(columnSpecs, "-arrowbitmap", &pstBitmap, TreeStateFromObj);
    PerStateCO_Init(columnSpecs, "-arrowimage", &pstImage, TreeStateFromObj);
    PerStateCO_Init(columnSpecs, "-background", &pstBorder, TreeStateFromObj);
    PerStateCO_Init(columnSpecs, "-textcolor", &pstColor, TreeStateFromObj);

    tree->headerOptionTable = Tk_CreateOptionTable(tree->interp, headerSpecs);
    tree->headerColumnOptionTable = Tk_CreateOptionTable(tree->interp, columnSpecs);
    tree->headerDragOptionTable = Tk_CreateOptionTable(tree->interp, dragSpecs);

    tree->tailExtend = 20;

    /* Create the default/topmost header item.  It can't be deleted. */
    tree->headerItems = TreeItem_CreateHeader(tree);

    /* Create the style for the tail column. */
    TreeHeaderColumn_EnsureStyleExists(TreeItem_GetHeader(tree,
	tree->headerItems), TreeItemColumn_GetHeaderColumn(tree,
	TreeItem_GetFirstColumn(tree, tree->headerItems)), tree->columnTail);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeHeader_FreeWidget --
 *
 *	Free header-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TreeHeader_FreeWidget(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItem item;

    item = tree->headerItems;
    while (item != NULL) {
	TreeItem_FreeResources(tree, item);
	item = TreeItem_GetNextSibling(tree, item);
    }

    Tcl_DeleteHashTable(&tree->headerHash);
}
