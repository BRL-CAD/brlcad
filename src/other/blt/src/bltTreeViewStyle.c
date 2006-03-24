
/*
 * bltTreeViewStyle.c --
 *
 *	This module implements styles for treeview widget cells.
 *
 * Copyright 1998-1999 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies or any of their entities not be used in
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
 *
 *	The "treeview" widget was created by George A. Howlett.
 */

#include "bltInt.h"

#ifndef NO_TREEVIEW

#include "bltTreeView.h"
#include "bltList.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <ctype.h>

#define STYLE_GAP		2

static Blt_OptionParseProc ObjToIcon;
static Blt_OptionPrintProc IconToObj;
static Blt_OptionFreeProc FreeIcon;
Blt_CustomOption bltTreeViewIconOption =
{
    /* Contains a pointer to the widget that's currently being
     * configured.  This is used in the custom configuration parse
     * routine for icons.  */
    ObjToIcon, IconToObj, FreeIcon, NULL,
};

#define DEF_STYLE_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_STYLE_HIGHLIGHT_FOREGROUND	STD_NORMAL_FOREGROUND
#ifdef WIN32
#define DEF_STYLE_ACTIVE_BACKGROUND	RGB_GREY85
#else
#define DEF_STYLE_ACTIVE_BACKGROUND	RGB_GREY95
#endif
#define DEF_STYLE_ACTIVE_FOREGROUND 	STD_ACTIVE_FOREGROUND
#define DEF_STYLE_GAP			"2"

typedef struct {
    int refCount;		/* Usage reference count. */
    unsigned int flags;
    char *name;			
    TreeViewStyleClass *classPtr; /* Class-specific routines to manage style. */
    Blt_HashEntry *hashPtr;

    /* General style fields. */
    Tk_Cursor cursor;		/* X Cursor */
    TreeViewIcon icon;		/* If non-NULL, is a Tk_Image to be drawn
				 * in the cell. */
    int gap;			/* # pixels gap between icon and text. */
    Tk_Font font;
    XColor *fgColor;		/* Normal foreground color of cell. */
    Tk_3DBorder border;		/* Normal background color of cell. */
    XColor *highlightFgColor;	/* Foreground color of cell when
				 * highlighted. */
    Tk_3DBorder highlightBorder;/* Background color of cell when
				 * highlighted. */
    XColor *activeFgColor;	/* Foreground color of cell when active. */
    Tk_3DBorder activeBorder;	/* Background color of cell when active. */

    /* TextBox-specific fields */
    GC gc;
    GC highlightGC;
    GC activeGC;

    int side;			/* Position of the text in relation to
				 * the icon.  */
    Blt_TreeKey key;		/* Actual data resides in this tree
				   value. */

} TreeViewTextBox;

#ifdef WIN32
#define DEF_TEXTBOX_CURSOR		"arrow"
#else
#define DEF_TEXTBOX_CURSOR		"hand2"
#endif /*WIN32*/
#define DEF_TEXTBOX_SIDE		"left"

static Blt_ConfigSpec textBoxSpecs[] =
{
    {BLT_CONFIG_BORDER, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	Blt_Offset(TreeViewTextBox, activeBorder), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	Blt_Offset(TreeViewTextBox, activeFgColor), 0},
    {BLT_CONFIG_BORDER, "-background", "background", "Background",
	(char *)NULL, Blt_Offset(TreeViewTextBox, border), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_TEXTBOX_CURSOR, Blt_Offset(TreeViewTextBox, cursor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font",
	(char *)NULL, Blt_Offset(TreeViewTextBox, font), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	(char *)NULL, Blt_Offset(TreeViewTextBox, fgColor),BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_DISTANCE, "-gap", "gap", "Gap",
	DEF_STYLE_GAP, Blt_Offset(TreeViewTextBox, gap), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BORDER, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        Blt_Offset(TreeViewTextBox, highlightBorder), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	 Blt_Offset(TreeViewTextBox, highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon",
	(char *)NULL, Blt_Offset(TreeViewTextBox, icon), 
	BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key",
	(char *)NULL, Blt_Offset(TreeViewTextBox, key), 
	BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_SIDE, "-side", "side", "side",
	DEF_TEXTBOX_SIDE, Tk_Offset(TreeViewTextBox, side),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef struct {
    int refCount;		/* Usage reference count. */
    unsigned int flags;		/* Contains style type and update flags. */
    char *name;			/* Instance name. */
    TreeViewStyleClass *classPtr; /* Class-specific routines to manage style. */
    Blt_HashEntry *hashPtr;	

    /* General style fields. */
    Tk_Cursor cursor;		/* X Cursor */
    TreeViewIcon icon;		/* If non-NULL, is a Tk_Image to be drawn
				 * in the cell. */
    int gap;			/* # pixels gap between icon and text. */
    Tk_Font font;
    XColor *fgColor;		/* Normal foreground color of cell. */
    Tk_3DBorder border;		/* Normal background color of cell. */
    XColor *highlightFgColor;	/* Foreground color of cell when
				 * highlighted. */
    Tk_3DBorder highlightBorder;/* Background color of cell when
				 * highlighted. */
    XColor *activeFgColor;	/* Foreground color of cell when active. */
    Tk_3DBorder activeBorder;	/* Background color of cell when active. */

    /* Checkbox specific fields. */
    GC gc;
    GC highlightGC;
    GC activeGC;

    Blt_TreeKey key;		/* Actual data resides in this tree
				   value. */
    int size;			/* Size of the checkbox. */
    int showValue;		/* If non-zero, display the on/off value.  */
    char *onValue;
    char *offValue;
    int lineWidth;		/* Linewidth of the surrounding box. */

    XColor *boxColor;		/* Rectangle (box) color (grey). */
    XColor *fillColor;		/* Fill color (white) */
    XColor *checkColor;		/* Check color (red). */

    GC boxGC;
    GC fillGC;			/* Box fill GC */
    GC checkGC;

    TextLayout *onPtr, *offPtr;
    
} TreeViewCheckBox;

#define DEF_CHECKBOX_BOX_COLOR		"black"
#define DEF_CHECKBOX_CHECK_COLOR	"red"
#define DEF_CHECKBOX_FILL_COLOR		"white"
#define DEF_CHECKBOX_OFFVALUE		"0"
#define DEF_CHECKBOX_ONVALUE		"1"
#define DEF_CHECKBOX_SHOWVALUE		"yes"
#define DEF_CHECKBOX_SIZE		"11"
#define DEF_CHECKBOX_LINEWIDTH		"2"
#define DEF_CHECKBOX_GAP		"4"
#ifdef WIN32
#define DEF_CHECKBOX_CURSOR		"arrow"
#else
#define DEF_CHECKBOX_CURSOR		"hand2"
#endif /*WIN32*/

static Blt_ConfigSpec checkBoxSpecs[] =
{
    {BLT_CONFIG_BORDER, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	Blt_Offset(TreeViewCheckBox, activeBorder), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	Blt_Offset(TreeViewCheckBox, activeFgColor), 0},
    {BLT_CONFIG_BORDER, "-background", "background", "Background",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, border), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_POS_DISTANCE, "-boxsize", "boxSize", "BoxSize",
	DEF_CHECKBOX_SIZE, Blt_Offset(TreeViewCheckBox, size), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_CHECKBOX_CURSOR, Blt_Offset(TreeViewTextBox, cursor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, font), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, fgColor), 
        BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_DISTANCE, "-gap", "gap", "Gap",
	DEF_CHECKBOX_GAP, Blt_Offset(TreeViewCheckBox, gap), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BORDER, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        Blt_Offset(TreeViewCheckBox, highlightBorder), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	 Blt_Offset(TreeViewCheckBox, highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, icon), 
	BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, key), 
	BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_DISTANCE, "-linewidth", "lineWidth", "LineWidth",
	DEF_CHECKBOX_LINEWIDTH, 
	Blt_Offset(TreeViewCheckBox, lineWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-checkcolor", "checkColor", "CheckColor", 
	DEF_CHECKBOX_CHECK_COLOR, Blt_Offset(TreeViewCheckBox, checkColor), 0},
    {BLT_CONFIG_COLOR, "-boxcolor", "boxColor", "BoxColor", 
	DEF_CHECKBOX_BOX_COLOR, Blt_Offset(TreeViewCheckBox, boxColor), 0},
    {BLT_CONFIG_COLOR, "-fillcolor", "fillColor", "FillColor", 
	DEF_CHECKBOX_FILL_COLOR, Blt_Offset(TreeViewCheckBox, fillColor), 0},
    {BLT_CONFIG_STRING, "-offvalue", "offValue", "OffValue",
	DEF_CHECKBOX_OFFVALUE, Blt_Offset(TreeViewCheckBox, offValue), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-onvalue", "onValue", "OnValue",
	DEF_CHECKBOX_ONVALUE, Blt_Offset(TreeViewCheckBox, onValue), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-key", "key", "key",
	(char *)NULL, Blt_Offset(TreeViewCheckBox, key), BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_BOOLEAN, "-showvalue", "showValue", "ShowValue",
	DEF_CHECKBOX_SHOWVALUE, Blt_Offset(TreeViewCheckBox, showValue), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef struct {
    int refCount;		/* Usage reference count. */
    unsigned int flags;
    char *name;			
    TreeViewStyleClass *classPtr;/* Class-specific style routines. */
    Blt_HashEntry *hashPtr;

    /* General style fields. */
    Tk_Cursor cursor;		/* X Cursor */
    TreeViewIcon icon;		/* If non-NULL, is a Tk_Image to be drawn
				 * in the cell. */
    int gap;			/* # pixels gap between icon and text. */
    Tk_Font font;
    XColor *fgColor;		/* Normal foreground color of cell. */
    Tk_3DBorder border;		/* Normal background color of cell. */
    XColor *highlightFgColor;	/* Foreground color of cell when
				 * highlighted. */
    Tk_3DBorder highlightBorder;/* Background color of cell when
				 * highlighted. */
    XColor *activeFgColor;	/* Foreground color of cell when active. */
    Tk_3DBorder activeBorder;	/* Background color of cell when active. */

    /* ComboBox-specific fields */
    GC gc;
    GC highlightGC;
    GC activeGC;

    int borderWidth;		/* Width of outer border surrounding
				 * the entire box. */
    int relief;			/* Relief of outer border. */

    Blt_TreeKey key;		/* Actual data resides in this tree
				   value. */

    char *choices;		/* List of available choices. */
    char *choiceIcons;		/* List of icons associated with choices. */
    int scrollWidth;
    int button;
    int buttonWidth;
    int buttonBorderWidth;	/* Border width of button. */
    int buttonRelief;		/* Normal relief of button. */

} TreeViewComboBox;

#define DEF_COMBOBOX_BORDERWIDTH	"1"
#define DEF_COMBOBOX_BUTTON_BORDERWIDTH	"1"
#define DEF_COMBOBOX_BUTTON_RELIEF	"raised"
#define DEF_COMBOBOX_RELIEF		"flat"
#ifdef WIN32
#define DEF_COMBOBOX_CURSOR		"arrow"
#else
#define DEF_COMBOBOX_CURSOR		"hand2"
#endif /*WIN32*/


static Blt_ConfigSpec comboBoxSpecs[] =
{
    {BLT_CONFIG_BORDER, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	Blt_Offset(TreeViewComboBox, activeBorder), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	Blt_Offset(TreeViewComboBox, activeFgColor), 0},
    {BLT_CONFIG_BORDER, "-background", "background", "Background",
	(char *)NULL, Blt_Offset(TreeViewComboBox, border), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 
	0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_DISTANCE, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_COMBOBOX_BORDERWIDTH, Blt_Offset(TreeViewComboBox, borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-buttonrelief", "buttonRelief", "ButtonRelief",
	DEF_COMBOBOX_BUTTON_RELIEF, Blt_Offset(TreeViewComboBox, buttonRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DISTANCE, "-buttonborderwidth", "buttonBorderWidth", 
	"ButtonBorderWidth", DEF_COMBOBOX_BUTTON_BORDERWIDTH, 
	Blt_Offset(TreeViewComboBox, buttonBorderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_COMBOBOX_CURSOR, Blt_Offset(TreeViewTextBox, cursor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font",
	(char *)NULL, Blt_Offset(TreeViewComboBox, font), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	(char *)NULL, Blt_Offset(TreeViewComboBox, fgColor), 
	BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_DISTANCE, "-gap", "gap", "Gap",
	DEF_STYLE_GAP, Blt_Offset(TreeViewComboBox, gap), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BORDER, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        Blt_Offset(TreeViewComboBox, highlightBorder), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	 Blt_Offset(TreeViewComboBox, highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon",
	(char *)NULL, Blt_Offset(TreeViewComboBox, icon), 
	BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key",
        (char *)NULL, Blt_Offset(TreeViewComboBox, key), 
	BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_COMBOBOX_RELIEF, Blt_Offset(TreeViewComboBox, relief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef TreeViewStyle *(StyleCreateProc) _ANSI_ARGS_((TreeView *tvPtr, 
	Blt_HashEntry *hPtr));

static StyleConfigProc ConfigureTextBox, ConfigureCheckBox, ConfigureComboBox;
static StyleCreateProc CreateTextBox, CreateCheckBox, CreateComboBox;
static StyleDrawProc DrawTextBox, DrawCheckBox, DrawComboBox;
static StyleEditProc EditTextBox, EditCheckBox, EditComboBox;
static StyleFreeProc FreeTextBox, FreeCheckBox, FreeComboBox;
static StyleMeasureProc MeasureTextBox, MeasureCheckBox, MeasureComboBox;
static StylePickProc PickCheckBox, PickComboBox;

/*
 *----------------------------------------------------------------------
 *
 * ObjToIcon --
 *
 *	Convert the name of an icon into a Tk image.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToIcon(clientData, interp, tkwin, objPtr, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    Tcl_Obj *objPtr;		/* Tcl_Obj representing the new value. */
    char *widgRec;
    int offset;
{
    TreeView *tvPtr = clientData;
    TreeViewIcon *iconPtr = (TreeViewIcon *)(widgRec + offset);
    TreeViewIcon icon;

    icon = Blt_TreeViewGetIcon(tvPtr, Tcl_GetString(objPtr));
    if (icon == NULL) {
	return TCL_ERROR;
    }
    *iconPtr = icon;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IconToObj --
 *
 *	Converts the icon into its string representation (its name).
 *
 * Results:
 *	The name of the icon is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
IconToObj(clientData, interp, tkwin, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;
    int offset;
{
    TreeViewIcon icon = *(TreeViewIcon *)(widgRec + offset);

    if (icon == NULL) {
	return bltEmptyStringObjPtr;
    }
    return Tcl_NewStringObj(Blt_NameOfImage((icon)->tkImage), -1);
}

/*ARGSUSED*/
static void
FreeIcon(clientData, display, widgRec, offset)
    ClientData clientData;
    Display *display;		/* Not used. */
    char *widgRec;
    int offset;
{
    TreeViewIcon icon = *(TreeViewIcon *)(widgRec + offset);
    TreeView *tvPtr = clientData;

    Blt_TreeViewFreeIcon(tvPtr, icon);
}

static TreeViewStyleClass textBoxClass = {
    "TextBoxStyle",
    textBoxSpecs,
    ConfigureTextBox,
    MeasureTextBox,
    DrawTextBox,
    NULL,
    EditTextBox,
    FreeTextBox,
};

static TreeViewStyleClass checkBoxClass = {
    "CheckBoxStyle",
    checkBoxSpecs,
    ConfigureCheckBox,
    MeasureCheckBox,
    DrawCheckBox,
    NULL,
    EditCheckBox,
    FreeCheckBox,
};

static TreeViewStyleClass comboBoxClass = {
    "ComboBoxStyle", 
    comboBoxSpecs,
    ConfigureComboBox,
    MeasureComboBox,
    DrawComboBox,
    PickComboBox,
    EditComboBox,
    FreeComboBox,
};

/*
 *----------------------------------------------------------------------
 *
 * CreateTextBox --
 *
 *	Creates a "textbox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *----------------------------------------------------------------------
 */
static TreeViewStyle *
CreateTextBox(tvPtr, hPtr)
    TreeView *tvPtr;
    Blt_HashEntry *hPtr;
{
    TreeViewTextBox *tbPtr;

    tbPtr = Blt_Calloc(1, sizeof(TreeViewTextBox));
    assert(tbPtr);
    tbPtr->classPtr = &textBoxClass;
    tbPtr->side = SIDE_LEFT;
    tbPtr->gap = STYLE_GAP;
    tbPtr->name = Blt_Strdup(Blt_GetHashKey(&tvPtr->styleTable, hPtr));
    tbPtr->hashPtr = hPtr;
    tbPtr->flags = STYLE_TEXTBOX;
    tbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, tbPtr);
    return (TreeViewStyle *)tbPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureTextBox --
 *
 *	Configures a "textbox" style.  This routine performs 
 *	generates the GCs required for a textbox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *----------------------------------------------------------------------
 */
static void
ConfigureTextBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    GC newGC;
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    XColor *bgColor;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground | GCBackground | GCFont;
    gcValues.font = Tk_FontId(CHOOSE(tvPtr->font, tbPtr->font));
    bgColor = Tk_3DBorderColor(CHOOSE(tvPtr->border, tbPtr->border));

    gcValues.background = bgColor->pixel;
    gcValues.foreground = CHOOSE(tvPtr->fgColor, tbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->gc);
    }
    tbPtr->gc = newGC;
    gcValues.background = Tk_3DBorderColor(tbPtr->highlightBorder)->pixel;
    gcValues.foreground = tbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->highlightGC);
    }
    tbPtr->highlightGC = newGC;

    gcValues.background = Tk_3DBorderColor(tbPtr->activeBorder)->pixel;
    gcValues.foreground = tbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->activeGC);
    }
    tbPtr->activeGC = newGC;
    tbPtr->flags |= STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * MeasureTextBox --
 *
 *	Determines the space requirements for the "textbox" given
 *	the value to be displayed.  Depending upon whether an icon
 *	or text is displayed and their relative placements, this
 *	routine computes the space needed for the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the
 *	computed dimensions.
 *
 *----------------------------------------------------------------------
 */
static void
MeasureTextBox(tvPtr, stylePtr, valuePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
    TreeViewValue *valuePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;

    textWidth = textHeight = 0;
    iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;

    if (tbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(tbPtr->icon);
	iconHeight = TreeViewIconHeight(tbPtr->icon);
    } 
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
	valuePtr->textPtr = NULL;
    }
    if (valuePtr->string != NULL) {	/* New string defined. */
	TextStyle ts;

	Blt_InitTextStyle(&ts);
	ts.font = CHOOSE(tvPtr->font, tbPtr->font);
	ts.anchor = TK_ANCHOR_NW;
	ts.justify = TK_JUSTIFY_LEFT;
	valuePtr->textPtr = Blt_GetTextLayout(valuePtr->string, &ts);
    } 
    gap = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
	if (tbPtr->icon != NULL) {
	    gap = tbPtr->gap;
	}
    }
    if (SIDE_VERTICAL(tbPtr->side)) {
	valuePtr->height = iconHeight + gap + textHeight;
	valuePtr->width = MAX(textWidth, iconWidth);
    } else {
	valuePtr->width = iconWidth + gap + textWidth;
	valuePtr->height = MAX(textHeight, iconHeight);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DrawTextBox --
 *
 *	Draws the "textbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The textbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static void
DrawTextBox(tvPtr, drawable, entryPtr, valuePtr, stylePtr, x, y)
    TreeView *tvPtr;
    Drawable drawable;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
    int x, y;
{
    GC gc;
    TreeViewColumn *columnPtr;
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textWidth, textHeight;
    int gap, columnWidth;
    Tk_3DBorder border;
    XColor *fgColor;

    columnPtr = valuePtr->columnPtr;

    if (stylePtr->flags & STYLE_HIGHLIGHT) {
	gc = tbPtr->highlightGC;
	border = tbPtr->highlightBorder;
	fgColor = tbPtr->highlightFgColor;
    } else {
	gc = tbPtr->gc;
	border = CHOOSE(tvPtr->border, tbPtr->border);
	fgColor = CHOOSE(tvPtr->fgColor, tbPtr->fgColor);
    }
    if (!Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	/*
	 * Draw the active or normal background color over the entire
	 * label area.  This includes both the tab's text and image.
	 * The rectangle should be 2 pixels wider/taller than this
	 * area. So if the label consists of just an image, we get an
	 * halo around the image when the tab is active.
	 */
	if (border != NULL) {
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, x, y, 
	       columnPtr->width, entryPtr->height, 0, TK_RELIEF_FLAT);
	}
    }    
    columnWidth = columnPtr->width - 
	(2 * columnPtr->borderWidth + PADDING(columnPtr->pad));
    if (columnWidth > valuePtr->width) {
	switch(columnPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

    textX = textY = iconX = iconY = 0;	/* Suppress compiler warning. */
    
    iconWidth = iconHeight = 0;
    if (tbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(tbPtr->icon);
	iconHeight = TreeViewIconHeight(tbPtr->icon);
    }
    textWidth = textHeight = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
    }
    gap = 0;
    if ((tbPtr->icon != NULL) && (valuePtr->textPtr != NULL)) {
	gap = tbPtr->gap;
    }
    switch (tbPtr->side) {
    case SIDE_RIGHT:
	textX = x;
	textY = y + (entryPtr->height - textHeight) / 2;
	iconX = textX + textWidth + gap;
	iconY = y + (entryPtr->height - iconHeight) / 2;
	break;
    case SIDE_LEFT:
	iconX = x;
	iconY = y + (entryPtr->height - iconHeight) / 2;
	textX = iconX + iconWidth + gap;
	textY = y + (entryPtr->height - textHeight) / 2;
	break;
    case SIDE_TOP:
	iconY = y;
	iconX = x + (columnWidth - iconWidth) / 2;
	textY = iconY + iconHeight + gap;
	textX = x + (columnWidth - textWidth) / 2;
	break;
    case SIDE_BOTTOM:
	textY = y;
	textX = x + (columnWidth - textWidth) / 2;
	iconY = textY + textHeight + gap;
	iconX = x + (columnWidth - iconWidth) / 2;
	break;
    }
    if (tbPtr->icon != NULL) {
	Tk_RedrawImage(TreeViewIconBits(tbPtr->icon), 0, 0, iconWidth, 
		       iconHeight, drawable, iconX, iconY);
    }
    if (valuePtr->textPtr != NULL) {
	TextStyle ts;
	XColor *color;
	Tk_Font font;
	
	font = CHOOSE(tvPtr->font, tbPtr->font);
	if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	    color = SELECT_FG(tvPtr);
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else {
	    color = fgColor;
	}
	Blt_SetDrawTextStyle(&ts, font, gc, color, fgColor, NULL, 0.0, 
		TK_ANCHOR_NW, TK_JUSTIFY_LEFT, 0, 0);
	Blt_DrawTextLayout(tvPtr->tkwin, drawable, valuePtr->textPtr, 
		&ts, textX, textY);
	if (color != fgColor) {
	    XSetForeground(tvPtr->display, gc, fgColor->pixel);
	}
    }
    stylePtr->flags &= ~STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * EditCombobox --
 *
 *	Edits the "combobox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EditTextBox(tvPtr, entryPtr, valuePtr, stylePtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;	/* Not used. */
{
    return Blt_TreeViewTextbox(tvPtr, entryPtr, valuePtr->columnPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * FreeTextBox --
 *
 *	Releases resources allocated for the textbox. The resources
 *	freed by this routine are specific only to the "textbox".   
 *	Other resources (common to all styles) are freed in the 
 *	Blt_TreeViewFreeStyle routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the textbox are freed.
 *
 *----------------------------------------------------------------------
 */
static void
FreeTextBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->highlightGC);
    }
    if (tbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->activeGC);
    }
    if (tbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, tbPtr->gc);
    }
    if (tbPtr->icon != NULL) {
	Blt_TreeViewFreeIcon(tvPtr, tbPtr->icon);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * CreateCheckbox --
 *
 *	Creates a "checkbox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *----------------------------------------------------------------------
 */
static TreeViewStyle *
CreateCheckBox(tvPtr, hPtr)
    TreeView *tvPtr;
    Blt_HashEntry *hPtr;
{
    TreeViewCheckBox *cbPtr;

    cbPtr = Blt_Calloc(1, sizeof(TreeViewCheckBox));
    assert(cbPtr);
    cbPtr->classPtr = &checkBoxClass;
    cbPtr->gap = 4;
    cbPtr->size = 11;
    cbPtr->lineWidth = 2;
    cbPtr->showValue = TRUE;
    cbPtr->name = Blt_Strdup(Blt_GetHashKey(&tvPtr->styleTable, hPtr));
    cbPtr->hashPtr = hPtr;
    cbPtr->flags = STYLE_CHECKBOX;
    cbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, cbPtr);
    return (TreeViewStyle *)cbPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureCheckbox --
 *
 *	Configures a "checkbox" style.  This routine performs 
 *	generates the GCs required for a checkbox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *----------------------------------------------------------------------
 */
static void
ConfigureCheckBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    GC newGC;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    XColor *bgColor;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground | GCBackground | GCFont;
    gcValues.font = Tk_FontId(CHOOSE(tvPtr->font, cbPtr->font));
    bgColor = Tk_3DBorderColor(CHOOSE(tvPtr->border, cbPtr->border));

    gcValues.background = bgColor->pixel;
    gcValues.foreground = CHOOSE(tvPtr->fgColor, cbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->gc);
    }
    cbPtr->gc = newGC;
    gcValues.background = Tk_3DBorderColor(cbPtr->highlightBorder)->pixel;
    gcValues.foreground = cbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->highlightGC);
    }
    cbPtr->highlightGC = newGC;

    gcValues.background = Tk_3DBorderColor(cbPtr->activeBorder)->pixel;
    gcValues.foreground = cbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->activeGC);
    }
    cbPtr->activeGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = cbPtr->fillColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->fillGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->fillGC);
    }
    cbPtr->fillGC = newGC;

    gcMask = GCForeground | GCLineWidth;
    gcValues.line_width = cbPtr->lineWidth;
    gcValues.foreground = cbPtr->boxColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->boxGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->boxGC);
    }
    cbPtr->boxGC = newGC;

    gcMask = GCForeground | GCLineWidth;
    gcValues.line_width = 1;
    gcValues.foreground = cbPtr->checkColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->checkGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->checkGC);
    }
    cbPtr->checkGC = newGC;
    cbPtr->flags |= STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * MeasureCheckbox --
 *
 *	Determines the space requirements for the "checkbox" given
 *	the value to be displayed.  Depending upon whether an icon
 *	or text is displayed and their relative placements, this
 *	routine computes the space needed for the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the
 *	computed dimensions.
 *
 *----------------------------------------------------------------------
 */
static void
MeasureCheckBox(tvPtr, stylePtr, valuePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
    TreeViewValue *valuePtr;
{
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;
    int boxWidth, boxHeight;

    boxWidth = boxHeight = ODD(cbPtr->size);

    textWidth = textHeight = iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(cbPtr->icon);
	iconHeight = TreeViewIconHeight(cbPtr->icon);
    } 
    if (cbPtr->onPtr != NULL) {
	Blt_Free(cbPtr->onPtr);
	cbPtr->onPtr = NULL;
    }
    if (cbPtr->offPtr != NULL) {
	Blt_Free(cbPtr->offPtr);
	cbPtr->offPtr = NULL;
    }
    gap = 0;
    if (cbPtr->showValue) {
	TextStyle ts;
	char *string;

	Blt_InitTextStyle(&ts);
	ts.font = CHOOSE(tvPtr->font, cbPtr->font);
	ts.anchor = TK_ANCHOR_NW;
	ts.justify = TK_JUSTIFY_LEFT;
	string = (cbPtr->onValue != NULL) ? cbPtr->onValue : valuePtr->string;
	cbPtr->onPtr = Blt_GetTextLayout(string, &ts);
	string = (cbPtr->offValue != NULL) ? cbPtr->offValue : valuePtr->string;
	cbPtr->offPtr = Blt_GetTextLayout(string, &ts);
	textWidth = MAX(cbPtr->offPtr->width, cbPtr->onPtr->width);
	textHeight = MAX(cbPtr->offPtr->height, cbPtr->onPtr->height);
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    valuePtr->width = cbPtr->gap * 2 + boxWidth + iconWidth + gap + textWidth;
    valuePtr->height = MAX3(boxHeight, textHeight, iconHeight);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawCheckbox --
 *
 *	Draws the "checkbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static void
DrawCheckBox(tvPtr, drawable, entryPtr, valuePtr, stylePtr, x, y)
    TreeView *tvPtr;
    Drawable drawable;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
    int x, y;
{
    GC gc;
    TreeViewColumn *columnPtr;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textHeight;
    int gap, columnWidth;
    Tk_3DBorder border;
    XColor *fgColor;
    Tk_Font font;
    int bool;
    int borderWidth, relief;
    TextLayout *textPtr;
    int boxX, boxY, boxWidth, boxHeight;

    font = CHOOSE(tvPtr->font, cbPtr->font);
    columnPtr = valuePtr->columnPtr;
    borderWidth = 0;
    relief = TK_RELIEF_FLAT;
    if (valuePtr == tvPtr->activeValuePtr) {
	gc = cbPtr->activeGC;
	border = cbPtr->activeBorder;
	fgColor = cbPtr->activeFgColor;
	borderWidth = 1;
	relief = TK_RELIEF_RAISED;
    } else if (stylePtr->flags & STYLE_HIGHLIGHT) {
	gc = cbPtr->highlightGC;
	border = cbPtr->highlightBorder;
	fgColor = cbPtr->highlightFgColor;
    } else {
	gc = cbPtr->gc;
	border = CHOOSE(tvPtr->border, cbPtr->border);
	fgColor = CHOOSE(tvPtr->fgColor, cbPtr->fgColor);
    }
    columnWidth = columnPtr->width - PADDING(columnPtr->pad);
    if (valuePtr == tvPtr->activeValuePtr) {
	/*
	 * Draw the active or normal background color over the entire
	 * label area.  This includes both the tab's text and image.
	 * The rectangle should be 2 pixels wider/taller than this
	 * area. So if the label consists of just an image, we get an
	 * halo around the image when the tab is active.
	 */
	if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, SELECT_BORDER(tvPtr),
		x, y, columnWidth, entryPtr->height - 1, borderWidth, relief);
	} else {
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, x, y, 
		columnWidth, entryPtr->height - 1, borderWidth, relief);
	}
    }    

    if (columnWidth > valuePtr->width) {
	switch(columnPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

    bool = (strcmp(valuePtr->string, cbPtr->onValue) == 0);
    textPtr = (bool) ? cbPtr->onPtr : cbPtr->offPtr;

    /*
     * Draw the box and check. 
     *
     *		+-----------+
     *		|           |
     *		|         * |
     *          |        *  |
     *          | *     *   |
     *          |  *   *    |
     *          |   * *     |
     *		|    *      |
     *		+-----------+
     */
    boxWidth = boxHeight = ODD(cbPtr->size);
    boxX = x + cbPtr->gap;
    boxY = y + (entryPtr->height - boxHeight) / 2;
    XFillRectangle(tvPtr->display, drawable, cbPtr->fillGC, boxX, boxY, 
		       boxWidth, boxHeight);
    XDrawRectangle(tvPtr->display, drawable, cbPtr->boxGC, boxX, boxY, 
	boxWidth, boxHeight);

    if (bool) {
	int midX, midY;
	int i;

	for (i = 0; i < 3; i++) {
	    midX = boxX + 2 * boxWidth / 5;
	    midY = boxY + boxHeight - 5 + i;
	    XDrawLine(tvPtr->display, drawable, cbPtr->checkGC, 
		      boxX + 2, boxY + boxHeight / 3 + 1 + i, midX, midY);
	    XDrawLine(tvPtr->display, drawable, cbPtr->checkGC, 
		      midX, midY, boxX + boxWidth - 2, boxY + i + 1);
	}
    }
#ifdef notdef
    textX = textY = iconX = iconY = 0;	/* Suppress compiler warning. */
#endif
    iconWidth = iconHeight = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(cbPtr->icon);
	iconHeight = TreeViewIconHeight(cbPtr->icon);
    }
    textHeight = 0;
    gap = 0;
    if (cbPtr->showValue) {
	textHeight = textPtr->height;
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    x = boxX + boxWidth + cbPtr->gap;

    /* The icon sits to the left of the text. */
    iconX = x;
    iconY = y + (entryPtr->height - iconHeight) / 2;
    textX = iconX + iconWidth + gap;
    textY = y + (entryPtr->height - textHeight) / 2;

    if (cbPtr->icon != NULL) {
	Tk_RedrawImage(TreeViewIconBits(cbPtr->icon), 0, 0, iconWidth, 
		       iconHeight, drawable, iconX, iconY);
    }
    if ((cbPtr->showValue) && (textPtr != NULL)) {
	TextStyle ts;
	XColor *color;
	
	if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	    color = SELECT_FG(tvPtr);
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else {
	    color = fgColor;
	}
	Blt_SetDrawTextStyle(&ts, font, gc, color, fgColor, NULL, 0.0, 
		TK_ANCHOR_NW, TK_JUSTIFY_LEFT, 0, 0);
	Blt_DrawTextLayout(tvPtr->tkwin, drawable, textPtr, &ts, textX, textY);
	if (color != fgColor) {
	    XSetForeground(tvPtr->display, gc, fgColor->pixel);
	}
    }
    stylePtr->flags &= ~STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * PickCheckbox --
 *
 *	Draws the "checkbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static int
PickCheckBox(entryPtr, valuePtr, stylePtr, worldX, worldY)
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
    int worldX, worldY;
{
    TreeViewColumn *columnPtr;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    int columnWidth;
    int x, y, width, height;

    columnPtr = valuePtr->columnPtr;
    columnWidth = columnPtr->width - 
	(2 * columnPtr->borderWidth + PADDING(columnPtr->pad));
    if (columnWidth > valuePtr->width) {
	switch(columnPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    worldX += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    worldX += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }
    width = height = ODD(cbPtr->size) + 2 * cbPtr->lineWidth;
    x = columnPtr->worldX + columnPtr->pad.side1 + cbPtr->gap - 
	cbPtr->lineWidth;
    y = entryPtr->worldY + (entryPtr->height - height) / 2;
    if ((worldX >= x) && (worldX < (x + width)) && 
	(worldY >= y) && (worldY < (y + height))) {
	return TRUE;
    }
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * EditCheckbox --
 *
 *	Edits the "checkbox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static int
EditCheckBox(tvPtr, entryPtr, valuePtr, stylePtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
{
    TreeViewColumn *columnPtr;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    Tcl_Obj *objPtr;

    columnPtr = valuePtr->columnPtr;
    if (Blt_TreeGetValueByKey(tvPtr->interp, tvPtr->tree, 
	      entryPtr->node, columnPtr->key, &objPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(objPtr), cbPtr->onValue) == 0) {
	objPtr = Tcl_NewStringObj(cbPtr->offValue, -1);
    } else {
	objPtr = Tcl_NewStringObj(cbPtr->onValue, -1);
    }
    entryPtr->flags |= ENTRY_DIRTY;
    tvPtr->flags |= (TV_DIRTY | TV_LAYOUT | TV_SCROLL | TV_RESORT);
    if (Blt_TreeSetValueByKey(tvPtr->interp, tvPtr->tree, 
	      entryPtr->node, columnPtr->key, objPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeCheckbox --
 *
 *	Releases resources allocated for the checkbox. The resources
 *	freed by this routine are specific only to the "checkbox".   
 *	Other resources (common to all styles) are freed in the 
 *	Blt_TreeViewFreeStyle routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the checkbox are freed.
 *
 *----------------------------------------------------------------------
 */
static void
FreeCheckBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;

    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->highlightGC);
    }
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->activeGC);
    }
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->gc);
    }
    if (cbPtr->fillGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->fillGC);
    }
    if (cbPtr->boxGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->boxGC);
    }
    if (cbPtr->checkGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->checkGC);
    }
    if (cbPtr->icon != NULL) {
	Blt_TreeViewFreeIcon(tvPtr, cbPtr->icon);
    }
    if (cbPtr->offPtr != NULL) {
	Blt_Free(cbPtr->offPtr);
    }
    if (cbPtr->onPtr != NULL) {
	Blt_Free(cbPtr->onPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CreateComboBox --
 *
 *	Creates a "combobox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *----------------------------------------------------------------------
 */
static TreeViewStyle *
CreateComboBox(tvPtr, hPtr)
    TreeView *tvPtr;
    Blt_HashEntry *hPtr;
{
    TreeViewComboBox *cbPtr;

    cbPtr = Blt_Calloc(1, sizeof(TreeViewComboBox));
    assert(cbPtr);
    cbPtr->classPtr = &comboBoxClass;
    cbPtr->gap = STYLE_GAP;
    cbPtr->buttonRelief = TK_RELIEF_RAISED;
    cbPtr->buttonBorderWidth = 1;
    cbPtr->borderWidth = 1;
    cbPtr->relief = TK_RELIEF_FLAT;
    cbPtr->name = Blt_Strdup(Blt_GetHashKey(&tvPtr->styleTable, hPtr));
    cbPtr->hashPtr = hPtr;
    cbPtr->flags = STYLE_COMBOBOX;
    cbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, cbPtr);
    return (TreeViewStyle *)cbPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureComboBox --
 *
 *	Configures a "combobox" style.  This routine performs 
 *	generates the GCs required for a combobox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *----------------------------------------------------------------------
 */
static void
ConfigureComboBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    GC newGC;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    XColor *bgColor;
    XGCValues gcValues;
    unsigned long gcMask;

    gcValues.font = Tk_FontId(CHOOSE(tvPtr->font, cbPtr->font));
    bgColor = Tk_3DBorderColor(CHOOSE(tvPtr->border, cbPtr->border));
    gcMask = GCForeground | GCBackground | GCFont;

    /* Normal foreground */
    gcValues.background = bgColor->pixel;
    gcValues.foreground = CHOOSE(tvPtr->fgColor, cbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->gc);
    }
    cbPtr->gc = newGC;

    /* Highlight foreground */
    gcValues.background = Tk_3DBorderColor(cbPtr->highlightBorder)->pixel;
    gcValues.foreground = cbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->highlightGC);
    }
    cbPtr->highlightGC = newGC;

    /* Active foreground */
    gcValues.background = Tk_3DBorderColor(cbPtr->activeBorder)->pixel;
    gcValues.foreground = cbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->activeGC);
    }
    cbPtr->activeGC = newGC;
    cbPtr->flags |= STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * MeasureComboBox --
 *
 *	Determines the space requirements for the "combobox" given
 *	the value to be displayed.  Depending upon whether an icon
 *	or text is displayed and their relative placements, this
 *	routine computes the space needed for the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the
 *	computed dimensions.
 *
 *----------------------------------------------------------------------
 */
static void
MeasureComboBox(tvPtr, stylePtr, valuePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
    TreeViewValue *valuePtr;
{
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;
    Tk_Font font;

    textWidth = textHeight = 0;
    iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;

    if (cbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(cbPtr->icon);
	iconHeight = TreeViewIconHeight(cbPtr->icon);
    } 
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
	valuePtr->textPtr = NULL;
    }
    font = CHOOSE(tvPtr->font, cbPtr->font);
    if (valuePtr->string != NULL) {	/* New string defined. */
	TextStyle ts;

	Blt_InitTextStyle(&ts);
	ts.font = font;
	ts.anchor = TK_ANCHOR_NW;
	ts.justify = TK_JUSTIFY_LEFT;
	valuePtr->textPtr = Blt_GetTextLayout(valuePtr->string, &ts);
    } 
    gap = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    cbPtr->buttonWidth = STD_ARROW_WIDTH + 6 + 2 * cbPtr->buttonBorderWidth;
    valuePtr->width = 2 * cbPtr->borderWidth + iconWidth + 4 * gap + 
	cbPtr->buttonWidth + textWidth;
    valuePtr->height = MAX(textHeight, iconHeight) + 2 * cbPtr->borderWidth;
}


/*
 *----------------------------------------------------------------------
 *
 * DrawComboBox --
 *
 *	Draws the "combobox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The combobox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static void
DrawComboBox(tvPtr, drawable, entryPtr, valuePtr, stylePtr, x, y)
    TreeView *tvPtr;
    Drawable drawable;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
    int x, y;
{
    GC gc;
    TreeViewColumn *columnPtr;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textHeight;
    int buttonX, buttonY;
    int gap, columnWidth;
    Tk_3DBorder border;
    XColor *fgColor;

    columnPtr = valuePtr->columnPtr;
    if (stylePtr->flags & STYLE_HIGHLIGHT) {
	gc = cbPtr->highlightGC;
	border = cbPtr->highlightBorder;
	fgColor = cbPtr->highlightFgColor;
    } else {
	gc = cbPtr->gc;
	border = CHOOSE(tvPtr->border, cbPtr->border);
	fgColor = CHOOSE(tvPtr->fgColor, cbPtr->fgColor);
    }
    if (!Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	/*
	 * Draw the active or normal background color over the entire
	 * label area.  This includes both the tab's text and image.
	 * The rectangle should be 2 pixels wider/taller than this
	 * area. So if the label consists of just an image, we get an
	 * halo around the image when the tab is active.
	 */
	if (border != NULL) {
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, x, y, 
		columnPtr->width, entryPtr->height, cbPtr->borderWidth, 
		cbPtr->relief);
	}
    }    
    buttonX = x + columnPtr->width;
    buttonX -= columnPtr->pad.side2 + cbPtr->borderWidth  +
	cbPtr->buttonWidth + cbPtr->gap;
    buttonY = y;

    columnWidth = columnPtr->width - 
	(2 * columnPtr->borderWidth + PADDING(columnPtr->pad));
    if (columnWidth > valuePtr->width) {
	switch(columnPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

#ifdef notdef
    textX = textY = iconX = iconY = 0;	/* Suppress compiler warning. */
#endif
    
    iconWidth = iconHeight = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeViewIconWidth(cbPtr->icon);
	iconHeight = TreeViewIconHeight(cbPtr->icon);
    }
    textHeight = 0;
    if (valuePtr->textPtr != NULL) {
	textHeight = valuePtr->textPtr->height;
    }
    gap = 0;
    if ((cbPtr->icon != NULL) && (valuePtr->textPtr != NULL)) {
	gap = cbPtr->gap;
    }

    iconX = x + gap;
    iconY = y + (entryPtr->height - iconHeight) / 2;
    textX = iconX + iconWidth + gap;
    textY = y + (entryPtr->height - textHeight) / 2;

    if (cbPtr->icon != NULL) {
	Tk_RedrawImage(TreeViewIconBits(cbPtr->icon), 0, 0, iconWidth, 
	       iconHeight, drawable, iconX, iconY);
    }
    if (valuePtr->textPtr != NULL) {
	TextStyle ts;
	XColor *color;
	Tk_Font font;
	
	font = CHOOSE(tvPtr->font, cbPtr->font);
	if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
	    color = SELECT_FG(tvPtr);
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	    XSetForeground(tvPtr->display, gc, color->pixel);
	} else {
	    color = fgColor;
	}
	Blt_SetDrawTextStyle(&ts, font, gc, color, fgColor, NULL, 0.0, 
		TK_ANCHOR_NW, TK_JUSTIFY_LEFT, 0, 0);
	Blt_DrawTextLayout(tvPtr->tkwin, drawable, valuePtr->textPtr, 
		&ts, textX, textY);
	if (color != fgColor) {
	    XSetForeground(tvPtr->display, gc, fgColor->pixel);
	}
    }
    if (valuePtr == tvPtr->activeValuePtr) {
	Blt_Fill3DRectangle(tvPtr->tkwin, drawable, stylePtr->activeBorder, 
	   buttonX, buttonY + cbPtr->borderWidth, cbPtr->buttonWidth, 
	   entryPtr->height - 2 * cbPtr->borderWidth, 
	cbPtr->buttonBorderWidth, cbPtr->buttonRelief); 
    } else {
	Blt_Fill3DRectangle(tvPtr->tkwin, drawable, columnPtr->titleBorder, 
		buttonX, buttonY + cbPtr->borderWidth, cbPtr->buttonWidth, 
		entryPtr->height - 2 * cbPtr->borderWidth, 
		cbPtr->buttonBorderWidth, cbPtr->buttonRelief); 
    }
    buttonX += cbPtr->buttonWidth / 2;
    buttonY += entryPtr->height / 2;
    Blt_DrawArrow(tvPtr->display, drawable, gc, buttonX, buttonY, 
		  STD_ARROW_HEIGHT, ARROW_DOWN);
    stylePtr->flags &= ~STYLE_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * PickCombobox --
 *
 *	Draws the "checkbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
static int
PickComboBox(entryPtr, valuePtr, stylePtr, worldX, worldY)
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;
    int worldX, worldY;
{
    TreeViewColumn *columnPtr;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    int x, y, width, height;

    columnPtr = valuePtr->columnPtr;
    width = cbPtr->buttonWidth;
    height = entryPtr->height - 4;
    x = columnPtr->worldX + columnPtr->width - columnPtr->pad.side2 - 
	cbPtr->borderWidth - columnPtr->borderWidth - width;
    y = entryPtr->worldY + cbPtr->borderWidth;
    if ((worldX >= x) && (worldX < (x + width)) && 
	(worldY >= y) && (worldY < (y + height))) {
	return TRUE;
    }
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * EditCombobox --
 *
 *	Edits the "combobox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EditComboBox(tvPtr, entryPtr, valuePtr, stylePtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
    TreeViewStyle *stylePtr;	/* Not used. */
{
    return Blt_TreeViewTextbox(tvPtr, entryPtr, valuePtr->columnPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeComboBox --
 *
 *	Releases resources allocated for the combobox. The resources
 *	freed by this routine are specific only to the "combobox".   
 *	Other resources (common to all styles) are freed in the 
 *	Blt_TreeViewFreeStyle routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the combobox are freed.
 *
 *----------------------------------------------------------------------
 */
static void
FreeComboBox(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;

    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->highlightGC);
    }
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->activeGC);
    }
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, cbPtr->gc);
    }
    if (cbPtr->icon != NULL) {
	Blt_TreeViewFreeIcon(tvPtr, cbPtr->icon);
    }
}

static TreeViewStyle *
GetStyle(interp, tvPtr, styleName)
    Tcl_Interp *interp;
    TreeView *tvPtr;
    char *styleName;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->styleTable, styleName);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find cell style \"", styleName, 
		"\"", (char *)NULL);
	}
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

int
Blt_TreeViewGetStyle(interp, tvPtr, styleName, stylePtrPtr)
    Tcl_Interp *interp;
    TreeView *tvPtr;
    char *styleName;
    TreeViewStyle **stylePtrPtr;
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, tvPtr, styleName);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->refCount++;
    *stylePtrPtr = stylePtr;
    return TCL_OK;
}

static TreeViewStyle *
CreateStyle(interp, tvPtr, type, styleName, objc, objv)
     Tcl_Interp *interp;
     TreeView *tvPtr;		/* TreeView widget. */
     int type;			/* Type of style: either
				 * STYLE_TEXTBOX,
				 * STYLE_COMBOBOX, or
				 * STYLE_CHECKBOX */
    char *styleName;		/* Name of the new style. */
    int objc;
    Tcl_Obj *CONST *objv;
{    
    Blt_HashEntry *hPtr;
    int isNew;
    TreeViewStyle *stylePtr;
    
    hPtr = Blt_CreateHashEntry(&tvPtr->styleTable, styleName, &isNew);
    if (!isNew) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "cell style \"", styleName, 
			     "\" already exists", (char *)NULL);
	}
	return NULL;
    }
    /* Create the new marker based upon the given type */
    switch (type) {
    case STYLE_TEXTBOX:
	stylePtr = CreateTextBox(tvPtr, hPtr);
	break;
    case STYLE_COMBOBOX:
	stylePtr = CreateComboBox(tvPtr, hPtr);
	break;
    case STYLE_CHECKBOX:
	stylePtr = CreateCheckBox(tvPtr, hPtr);
	break;
    default:
	return NULL;
    }
    bltTreeViewIconOption.clientData = tvPtr;
    if (Blt_ConfigureComponentFromObj(interp, tvPtr->tkwin, styleName, 
	stylePtr->classPtr->className, stylePtr->classPtr->specsPtr, 
	objc, objv, (char *)stylePtr, 0) != TCL_OK) {
	Blt_TreeViewFreeStyle(tvPtr, stylePtr);
	return NULL;
    }
    return stylePtr;
}

void
Blt_TreeViewUpdateStyleGCs(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    (*stylePtr->classPtr->configProc)(tvPtr, stylePtr);
    stylePtr->flags |= STYLE_DIRTY;
    Blt_TreeViewEventuallyRedraw(tvPtr);
}

TreeViewStyle *
Blt_TreeViewCreateStyle(interp, tvPtr, type, styleName)
     Tcl_Interp *interp;
     TreeView *tvPtr;		/* TreeView widget. */
     int type;			/* Type of style: either
				 * STYLE_TEXTBOX,
				 * STYLE_COMBOBOX, or
				 * STYLE_CHECKBOX */
    char *styleName;		/* Name of the new style. */
{    
    return CreateStyle(interp, tvPtr, type, styleName, 0, (Tcl_Obj **)NULL);
}

void
Blt_TreeViewFreeStyle(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    stylePtr->refCount--;
#ifdef notdef
    fprint(f(stderr, "Blt_TreeViewFreeStyle %s count=%d\n", stylePtr->name,
	    stylePtr->refCount);
#endif
    /* Remove the style from the hash table so that it's name can be used.*/
    /* If no cell is using the style, remove it.*/
    if ((stylePtr->refCount <= 0) && !(stylePtr->flags & STYLE_USER)){
#ifdef notdef
	fprintf(stderr, "freeing %s\n", stylePtr->name);
#endif
	bltTreeViewIconOption.clientData = tvPtr;
	Blt_FreeObjOptions(stylePtr->classPtr->specsPtr, (char *)stylePtr, 
		   tvPtr->display, 0);
	(*stylePtr->classPtr->freeProc)(tvPtr, stylePtr); 
	if (stylePtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(&tvPtr->styleTable, stylePtr->hashPtr);
	} 
	if (stylePtr->name != NULL) {
	    Blt_Free(stylePtr->name);
	}
	Blt_Free(stylePtr);
    } 
}

void
Blt_TreeViewSetStyleIcon(tvPtr, stylePtr, icon)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
    TreeViewIcon icon;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->icon != NULL) {
	Blt_TreeViewFreeIcon(tvPtr, tbPtr->icon);
    }
    tbPtr->icon = icon;
}

GC
Blt_TreeViewGetStyleGC(stylePtr)
    TreeViewStyle *stylePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    return tbPtr->gc;
}

Tk_3DBorder
Blt_TreeViewGetStyleBorder(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    Tk_3DBorder border;

    border = (tbPtr->flags & STYLE_HIGHLIGHT) 
	? tbPtr->highlightBorder : tbPtr->border;
    return (border != NULL) ? border : tvPtr->border;
}

Tk_Font
Blt_TreeViewGetStyleFont(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->font != NULL) {
	return tbPtr->font;
    }
    return tvPtr->font;
}

XColor *
Blt_TreeViewGetStyleFg(tvPtr, stylePtr)
    TreeView *tvPtr;
    TreeViewStyle *stylePtr;
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->fgColor != NULL) {
	return tbPtr->fgColor;
    }
    return tvPtr->fgColor;
}

static void
DrawValue(tvPtr, entryPtr, valuePtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;
{
    Drawable drawable;
    int sx, sy, dx, dy;
    int width, height;
    int left, right, top, bottom;
    TreeViewColumn *columnPtr;
    TreeViewStyle *stylePtr;

    stylePtr = valuePtr->stylePtr;
    if (stylePtr == NULL) {
	stylePtr = valuePtr->columnPtr->stylePtr;
    }
    if (stylePtr->cursor != None) {
	if (valuePtr == tvPtr->activeValuePtr) {
	    Tk_DefineCursor(tvPtr->tkwin, stylePtr->cursor);
	} else {
	    if (tvPtr->cursor != None) {
		Tk_DefineCursor(tvPtr->tkwin, tvPtr->cursor);
	    } else {
		Tk_UndefineCursor(tvPtr->tkwin);
	    }
	}
    }
    columnPtr = valuePtr->columnPtr;
    dx = SCREENX(tvPtr, columnPtr->worldX) + columnPtr->pad.side1;
    dy = SCREENY(tvPtr, entryPtr->worldY);
    height = entryPtr->height - 1;
    width = valuePtr->columnPtr->width - PADDING(columnPtr->pad);

    top = tvPtr->titleHeight + tvPtr->inset;
    bottom = Tk_Height(tvPtr->tkwin) - tvPtr->inset;
    left = tvPtr->inset;
    right = Tk_Width(tvPtr->tkwin) - tvPtr->inset;

    if (((dx + width) < left) || (dx > right) ||
	((dy + height) < top) || (dy > bottom)) {
	return;			/* Value is clipped. */
    }

    drawable = Tk_GetPixmap(tvPtr->display, Tk_WindowId(tvPtr->tkwin), 
	width, height, Tk_Depth(tvPtr->tkwin));
    /* Draw the background of the value. */
    if ((valuePtr == tvPtr->activeValuePtr) ||
	(!Blt_TreeViewEntryIsSelected(tvPtr, entryPtr))) {
	Tk_3DBorder border;

	border = Blt_TreeViewGetStyleBorder(tvPtr, tvPtr->stylePtr);
	Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, 0, 0, width, height,
		0, TK_RELIEF_FLAT);
    } else {
	Blt_Fill3DRectangle(tvPtr->tkwin, drawable, SELECT_BORDER(tvPtr), 0, 0, 
		width, height, tvPtr->selBorderWidth, tvPtr->selRelief);
    }
    Blt_TreeViewDrawValue(tvPtr, entryPtr, valuePtr, drawable, 0, 0);
    
    /* Clip the drawable if necessary */
    sx = sy = 0;
    if (dx < left) {
	width -= left - dx;
	sx += left - dx;
	dx = left;
    }
    if ((dx + width) >= right) {
	width -= (dx + width) - right;
    }
    if (dy < top) {
	height -= top - dy;
	sy += top - dy;
	dy = top;
    }
    if ((dy + height) >= bottom) {
	height -= (dy + height) - bottom;
    }
    XCopyArea(tvPtr->display, drawable, Tk_WindowId(tvPtr->tkwin), 
      tvPtr->lineGC, sx, sy, width,  height, dx, dy);
    Tk_FreePixmap(tvPtr->display, drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * StyleActivateOp --
 *
 * 	Turns on/off highlighting for a particular style.
 *
 *	  .t style activate entry column
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleActivateOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;

    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr, *oldPtr;

    oldPtr = tvPtr->activeValuePtr;
    if (objc == 3) {
	Tcl_Obj *listObjPtr; 

	valuePtr = tvPtr->activeValuePtr;
	entryPtr = tvPtr->activePtr;
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	if ((entryPtr != NULL) && (valuePtr != NULL)) {
	    Tcl_Obj *objPtr; 
	    objPtr = Tcl_NewIntObj(Blt_TreeNodeId(entryPtr->node));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Tcl_NewStringObj(valuePtr->columnPtr->key, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	} 
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    } else if (objc == 4) {
	tvPtr->activeValuePtr = NULL;
	if ((oldPtr != NULL)  && (tvPtr->activePtr != NULL)) {
	    DrawValue(tvPtr, tvPtr->activePtr, oldPtr);
	}
    } else {
	TreeViewColumn *columnPtr;

	if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	    return TCL_ERROR;
	}

	if (Blt_TreeViewGetColumn(interp, tvPtr, objv[4], &columnPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	valuePtr = Blt_TreeViewFindValue(entryPtr, columnPtr);
	if (valuePtr == NULL) {
	    return TCL_OK;
	}
	tvPtr->activePtr = entryPtr;
	tvPtr->activeColumnPtr = columnPtr;
	oldPtr = tvPtr->activeValuePtr;
	tvPtr->activeValuePtr = valuePtr;
	if (valuePtr != oldPtr) {
	    if (oldPtr != NULL) {
		DrawValue(tvPtr, entryPtr, oldPtr);
	    }
	    if (valuePtr != NULL) {
		DrawValue(tvPtr, entryPtr, valuePtr);
	    }
	}
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * StyleCgetOp --
 *
 *	  .t style cget "styleName" -background
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleCgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
	stylePtr->classPtr->specsPtr, (char *)tvPtr, objv[4], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * StyleCheckBoxOp --
 *
 *	  .t style checkbox "styleName" -background blue
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleCheckBoxOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, tvPtr, STYLE_CHECKBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->refCount = 0;
    stylePtr->flags |= STYLE_USER;
    Blt_TreeViewUpdateStyleGCs(tvPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleComboBoxOp --
 *
 *	  .t style combobox "styleName" -background blue
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleComboBoxOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, tvPtr, STYLE_COMBOBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->refCount = 0;
    stylePtr->flags |= STYLE_USER;
    Blt_TreeViewUpdateStyleGCs(tvPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure a style.
 *
 *	  .t style configure "styleName" option value
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for stylePtr; old resources get freed, if there
 *	were any.  
 *
 *----------------------------------------------------------------------
 */
static int
StyleConfigureOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
	    stylePtr->classPtr->specsPtr, (char *)stylePtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 5) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
		stylePtr->classPtr->specsPtr, (char *)stylePtr, objv[5], 0);
    }
    bltTreeViewIconOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(interp, tvPtr->tkwin, 
	stylePtr->classPtr->specsPtr, objc - 4, objv + 4, (char *)stylePtr, 
	BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    (*stylePtr->classPtr->configProc)(tvPtr, stylePtr);
    stylePtr->flags |= STYLE_DIRTY;
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleForgetOp --
 *
 * 	Eliminates one or more style names.  A style still may be in
 * 	use after its name has been officially removed.  Only its hash
 * 	table entry is removed.  The style itself remains until its
 * 	reference count returns to zero (i.e. no one else is using it).
 *
 *	  .t style forget "styleName"...
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
static int
StyleForgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;

    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;
    int i;

    for (i = 3; i < objc; i++) {
	stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[i]));
	if (stylePtr == NULL) {
	    return TCL_ERROR;
	}
	if (stylePtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(&tvPtr->styleTable, stylePtr->hashPtr);
	    stylePtr->hashPtr = NULL;
	} 
	stylePtr->flags &= ~STYLE_USER;
	if (stylePtr->refCount <= 0) {
	    Blt_TreeViewFreeStyle(tvPtr, stylePtr);
	}
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleHighlightOp --
 *
 * 	Turns on/off highlighting for a particular style.
 *
 *	  .t style highlight styleName on|off
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleHighlightOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;

    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;
    int bool, oldBool;

    stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[4], &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    oldBool = ((stylePtr->flags & STYLE_HIGHLIGHT) != 0);
    if (oldBool != bool) {
	if (bool) {
	    stylePtr->flags |= STYLE_HIGHLIGHT;
	} else {
	    stylePtr->flags &= ~STYLE_HIGHLIGHT;
	}
	Blt_TreeViewEventuallyRedraw(tvPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleNamesOp --
 *
 * 	Lists the names of all the current styles in the treeview widget.
 *
 *	  .t style names
 *
 * Results:
 *	Always TCL_OK.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleNamesOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;

    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;	/* Not used. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *listObjPtr, *objPtr;
    TreeViewStyle *stylePtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (hPtr = Blt_FirstHashEntry(&tvPtr->styleTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	stylePtr = Blt_GetHashValue(hPtr);
	objPtr = Tcl_NewStringObj(stylePtr->name, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleSetOp --
 *
 * 	Sets a style for a given key for all the ids given.
 *
 *	  .t style set styleName key node...
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
static int
StyleSetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;

    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_TreeKey key;
    TreeViewEntry *entryPtr;
    TreeViewStyle *stylePtr, *oldStylePtr;
    TreeViewTagInfo info;
    int i;

    stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    key = Blt_TreeGetKey(Tcl_GetString(objv[4]));
    stylePtr->flags |= STYLE_LAYOUT;
    for (i = 5; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    register TreeViewValue *valuePtr;

	    for (valuePtr = entryPtr->values; valuePtr != NULL; 
		 valuePtr = valuePtr->nextPtr) {
		if (valuePtr->columnPtr->key == key) {
		    stylePtr->refCount++;
		    oldStylePtr = valuePtr->stylePtr;
		    valuePtr->stylePtr = stylePtr;
		    if (oldStylePtr != NULL) {
			Blt_TreeViewFreeStyle(tvPtr, oldStylePtr);
		    }
		    break;
		}
	    }
	}
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleTextBoxOp --
 *
 *	  .t style text "styleName" -background blue
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleTextBoxOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, tvPtr, STYLE_TEXTBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->refCount = 0;
    stylePtr->flags |= STYLE_USER;
    Blt_TreeViewUpdateStyleGCs(tvPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleUnsetOp --
 *
 * 	Removes a style for a given key for all the ids given.
 *	The cell's style is returned to its default state.
 *
 *	  .t style unset styleName key node...
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
static int
StyleUnsetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_TreeKey key;
    TreeViewEntry *entryPtr;
    TreeViewStyle *stylePtr;
    TreeViewTagInfo info;
    int i;

    stylePtr = GetStyle(interp, tvPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    key = Blt_TreeGetKey(Tcl_GetString(objv[4]));
    stylePtr->flags |= STYLE_LAYOUT;
    for (i = 5; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    register TreeViewValue *valuePtr;

	    for (valuePtr = entryPtr->values; valuePtr != NULL; 
		 valuePtr = valuePtr->nextPtr) {
		if (valuePtr->columnPtr->key == key) {
		    if (valuePtr->stylePtr != NULL) {
			Blt_TreeViewFreeStyle(tvPtr, valuePtr->stylePtr);
			valuePtr->stylePtr = NULL;
		    }
		    break;
		}
	    }
	}
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleOp --
 *
 *	.t style activate $node $column
 *	.t style activate 
 *	.t style cget "highlight" -foreground
 *	.t style configure "highlight" -fg blue -bg green
 *	.t style checkbox "highlight"
 *	.t style highlight "highlight" on|off
 *	.t style combobox "highlight"
 *	.t style text "highlight"
 *	.t style forget "highlight"
 *	.t style get "mtime" $node
 *	.t style names
 *	.t style set "mtime" "highlight" all
 *	.t style unset "mtime" all
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec styleOps[] = {
    {"activate", 1, (Blt_Op)StyleActivateOp, 3, 5,"entry column",},
    {"cget", 2, (Blt_Op)StyleCgetOp, 5, 5, "styleName option",},
    {"checkbox", 2, (Blt_Op)StyleCheckBoxOp, 4, 0, "styleName options...",},
    {"combobox", 3, (Blt_Op)StyleComboBoxOp, 4, 0, "styleName options...",},
    {"configure", 3, (Blt_Op)StyleConfigureOp, 4, 0, "styleName options...",},
    {"forget", 1, (Blt_Op)StyleForgetOp, 3, 0, "styleName...",},
    {"highlight", 1, (Blt_Op)StyleHighlightOp, 5, 5, "styleName boolean",},
    {"names", 1, (Blt_Op)StyleNamesOp, 3, 3, "",}, 
    {"set", 1, (Blt_Op)StyleSetOp, 6, 6, "key styleName tagOrId...",},
    {"textbox", 1, (Blt_Op)StyleTextBoxOp, 4, 0, "styleName options...",},
    {"unset", 1, (Blt_Op)StyleUnsetOp, 5, 5, "key tagOrId",},
};

static int nStyleOps = sizeof(styleOps) / sizeof(Blt_OpSpec);

int
Blt_TreeViewStyleOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nStyleOps, styleOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(tvPtr, interp, objc, objv);
    return result;
}
#endif /* NO_TREEVIEW */
