/* 
 * tkColor.c --
 *
 *	This file maintains a database of color values for the Tk
 *	toolkit, in order to avoid round-trips to the server to
 *	map color names to pixel values.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkColor.c 1.44 96/11/04 13:55:25
 */

#include <tkColor.h>

/*
 * A two-level data structure is used to manage the color database.
 * The top level consists of one entry for each color name that is
 * currently active, and the bottom level contains one entry for each
 * pixel value that is still in use.  The distinction between
 * levels is necessary because the same pixel may have several
 * different names.  There are two hash tables, one used to index into
 * each of the data structures.  The name hash table is used when
 * allocating colors, and the pixel hash table is used when freeing
 * colors.
 */


/*
 * Hash table for name -> TkColor mapping, and key structure used to
 * index into that table:
 */

static Tcl_HashTable nameTable;
typedef struct {
    Tk_Uid name;		/* Name of desired color. */
    Colormap colormap;		/* Colormap from which color will be
				 * allocated. */
    Display *display;		/* Display for colormap. */
} NameKey;

/*
 * Hash table for value -> TkColor mapping, and key structure used to
 * index into that table:
 */

static Tcl_HashTable valueTable;
typedef struct {
    int red, green, blue;	/* Values for desired color. */
    Colormap colormap;		/* Colormap from which color will be
				 * allocated. */
    Display *display;		/* Display for colormap. */
} ValueKey;

static int initialized = 0;	/* 0 means static structures haven't been
				 * initialized yet. */

/*
 * Forward declarations for procedures defined in this file:
 */

static void		ColorInit _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetColor --
 *
 *	Given a string name for a color, map the name to a corresponding
 *	XColor structure.
 *
 * Results:
 *	The return value is a pointer to an XColor structure that
 *	indicates the red, blue, and green intensities for the color
 *	given by "name", and also specifies a pixel value to use to
 *	draw in that color.  If an error occurs, NULL is returned and
 *	an error message will be left in interp->result.
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColor so that the database is cleaned up when colors
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */

XColor *
Tk_GetColor(interp, tkwin, name)
    Tcl_Interp *interp;		/* Place to leave error message if
				 * color can't be found. */
    Tk_Window tkwin;		/* Window in which color will be used. */
    Tk_Uid name;		/* Name of color to allocated (in form
				 * suitable for passing to XParseColor). */
{
    NameKey nameKey;
    Tcl_HashEntry *nameHashPtr;
    int new;
    TkColor *tkColPtr;
    Display *display = Tk_Display(tkwin);

    if (!initialized) {
	ColorInit();
    }

    /*
     * First, check to see if there's already a mapping for this color
     * name.
     */

    nameKey.name = name;
    nameKey.colormap = Tk_Colormap(tkwin);
    nameKey.display = display;
    nameHashPtr = Tcl_CreateHashEntry(&nameTable, (char *) &nameKey, &new);
    if (!new) {
	tkColPtr = (TkColor *) Tcl_GetHashValue(nameHashPtr);
	tkColPtr->refCount++;
	return &tkColPtr->color;
    }

    /*
     * The name isn't currently known.  Map from the name to a pixel
     * value.
     */

    tkColPtr = TkpGetColor(tkwin, name);
    if (tkColPtr == NULL) {
	if (interp != NULL) {
	    if (*name == '#') {
		Tcl_AppendResult(interp, "invalid color name \"", name,
			"\"", (char *) NULL);
	    } else {
		Tcl_AppendResult(interp, "unknown color name \"", name,
			"\"", (char *) NULL);
	    }
	}
	Tcl_DeleteHashEntry(nameHashPtr);
	return (XColor *) NULL;
    }

    /*
     * Now create a new TkColor structure and add it to nameTable.
     */

    tkColPtr->magic = COLOR_MAGIC;
    tkColPtr->gc = None;
    tkColPtr->screen = Tk_Screen(tkwin);
    tkColPtr->colormap = nameKey.colormap;
    tkColPtr->visual  = Tk_Visual(tkwin);
    tkColPtr->refCount = 1;
    tkColPtr->tablePtr = &nameTable;
    tkColPtr->hashPtr = nameHashPtr;
    Tcl_SetHashValue(nameHashPtr, tkColPtr);

    return &tkColPtr->color;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetColorByValue --
 *
 *	Given a desired set of red-green-blue intensities for a color,
 *	locate a pixel value to use to draw that color in a given
 *	window.
 *
 * Results:
 *	The return value is a pointer to an XColor structure that
 *	indicates the closest red, blue, and green intensities available
 *	to those specified in colorPtr, and also specifies a pixel
 *	value to use to draw in that color.
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColor, so that the database is cleaned up when colors
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */

XColor *
Tk_GetColorByValue(tkwin, colorPtr)
    Tk_Window tkwin;		/* Window where color will be used. */
    XColor *colorPtr;		/* Red, green, and blue fields indicate
				 * desired color. */
{
    ValueKey valueKey;
    Tcl_HashEntry *valueHashPtr;
    int new;
    TkColor *tkColPtr;
    Display *display = Tk_Display(tkwin);

    if (!initialized) {
	ColorInit();
    }

    /*
     * First, check to see if there's already a mapping for this color
     * name.
     */

    valueKey.red = colorPtr->red;
    valueKey.green = colorPtr->green;
    valueKey.blue = colorPtr->blue;
    valueKey.colormap = Tk_Colormap(tkwin);
    valueKey.display = display;
    valueHashPtr = Tcl_CreateHashEntry(&valueTable, (char *) &valueKey, &new);
    if (!new) {
	tkColPtr = (TkColor *) Tcl_GetHashValue(valueHashPtr);
	tkColPtr->refCount++;
	return &tkColPtr->color;
    }

    /*
     * The name isn't currently known.  Find a pixel value for this
     * color and add a new structure to valueTable.
     */

    tkColPtr = TkpGetColorByValue(tkwin, colorPtr);
    tkColPtr->magic = COLOR_MAGIC;
    tkColPtr->gc = None;
    tkColPtr->screen = Tk_Screen(tkwin);
    tkColPtr->colormap = valueKey.colormap;
    tkColPtr->visual  = Tk_Visual(tkwin);
    tkColPtr->refCount = 1;
    tkColPtr->tablePtr = &valueTable;
    tkColPtr->hashPtr = valueHashPtr;
    Tcl_SetHashValue(valueHashPtr, tkColPtr);
    return &tkColPtr->color;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfColor --
 *
 *	Given a color, return a textual string identifying
 *	the color.
 *
 * Results:
 *	If colorPtr was created by Tk_GetColor, then the return
 *	value is the "string" that was used to create it.
 *	Otherwise the return value is a string that could have
 *	been passed to Tk_GetColor to allocate that color.  The
 *	storage for the returned string is only guaranteed to
 *	persist up until the next call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfColor(colorPtr)
    XColor *colorPtr;		/* Color whose name is desired. */
{
    register TkColor *tkColPtr = (TkColor *) colorPtr;
    static char string[20];

    if ((tkColPtr->magic == COLOR_MAGIC)
	    && (tkColPtr->tablePtr == &nameTable)) {
	return ((NameKey *) tkColPtr->hashPtr->key.words)->name;
    }
    sprintf(string, "#%04x%04x%04x", colorPtr->red, colorPtr->green,
	    colorPtr->blue);
    return string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GCForColor --
 *
 *	Given a color allocated from this module, this procedure
 *	returns a GC that can be used for simple drawing with that
 *	color.
 *
 * Results:
 *	The return value is a GC with color set as its foreground
 *	color and all other fields defaulted.  This GC is only valid
 *	as long as the color exists;  it is freed automatically when
 *	the last reference to the color is freed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GC
Tk_GCForColor(colorPtr, drawable)
    XColor *colorPtr;		/* Color for which a GC is desired. Must
				 * have been allocated by Tk_GetColor or
				 * Tk_GetColorByName. */
    Drawable drawable;		/* Drawable in which the color will be
				 * used (must have same screen and depth
				 * as the one for which the color was
				 * allocated). */
{
    TkColor *tkColPtr = (TkColor *) colorPtr;
    XGCValues gcValues;

    /*
     * Do a quick sanity check to make sure this color was really
     * allocated by Tk_GetColor.
     */

    if (tkColPtr->magic != COLOR_MAGIC) {
	panic("Tk_GCForColor called with bogus color");
    }

    if (tkColPtr->gc == None) {
	gcValues.foreground = tkColPtr->color.pixel;
	tkColPtr->gc = XCreateGC(DisplayOfScreen(tkColPtr->screen),
		drawable, GCForeground, &gcValues);
    }
    return tkColPtr->gc;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeColor --
 *
 *	This procedure is called to release a color allocated by
 *	Tk_GetColor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference count associated with colorPtr is deleted, and
 *	the color is released to X if there are no remaining uses
 *	for it.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeColor(colorPtr)
    XColor *colorPtr;		/* Color to be released.  Must have been
				 * allocated by Tk_GetColor or
				 * Tk_GetColorByValue. */
{
    register TkColor *tkColPtr = (TkColor *) colorPtr;
    Screen *screen = tkColPtr->screen;

    /*
     * Do a quick sanity check to make sure this color was really
     * allocated by Tk_GetColor.
     */

    if (tkColPtr->magic != COLOR_MAGIC) {
	panic("Tk_FreeColor called with bogus color");
    }

    tkColPtr->refCount--;
    if (tkColPtr->refCount == 0) {
	if (tkColPtr->gc != None) {
	    XFreeGC(DisplayOfScreen(screen), tkColPtr->gc);
	    tkColPtr->gc = None;
	}
	TkpFreeColor(tkColPtr);
	Tcl_DeleteHashEntry(tkColPtr->hashPtr);
	tkColPtr->magic = 0;
	ckfree((char *) tkColPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ColorInit --
 *
 *	Initialize the structure used for color management.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Read the code.
 *
 *----------------------------------------------------------------------
 */

static void
ColorInit()
{
    initialized = 1;
    Tcl_InitHashTable(&nameTable, sizeof(NameKey)/sizeof(int));
    Tcl_InitHashTable(&valueTable, sizeof(ValueKey)/sizeof(int));
}
