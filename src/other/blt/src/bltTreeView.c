
/*
 * bltTreeView.c --
 *
 *	This module implements an hierarchy widget for the BLT toolkit.
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

/*
 * TODO:
 *
 * BUGS:
 *   1.  "open" operation should change scroll offset so that as many
 *	 new entries (up to half a screen) can be seen.
 *   2.  "open" needs to adjust the scrolloffset so that the same entry
 *	 is seen at the same place.
 */

#include "bltInt.h"

#ifndef NO_TREEVIEW

#include "bltTreeView.h"

#define BUTTON_PAD		2
#define BUTTON_IPAD		1
#define BUTTON_SIZE		7
#define COLUMN_PAD		2
#define FOCUS_WIDTH		1
#define ICON_PADX		2
#define ICON_PADY		1
#define INSET_PAD		0
#define LABEL_PADX		3
#define LABEL_PADY		0

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define DEF_ICON_WIDTH		16
#define DEF_ICON_HEIGHT		16

static Blt_TreeApplyProc DeleteApplyProc;
static Blt_TreeApplyProc CreateApplyProc;

extern Blt_CustomOption bltTreeViewDataOption;

static Blt_OptionParseProc ObjToTree;
static Blt_OptionPrintProc TreeToObj;
static Blt_OptionFreeProc FreeTree;
Blt_CustomOption bltTreeViewTreeOption =
{
    ObjToTree, TreeToObj, FreeTree, NULL,
};

static Blt_OptionParseProc ObjToIcons;
static Blt_OptionPrintProc IconsToObj;
static Blt_OptionFreeProc FreeIcons;
Blt_CustomOption bltTreeViewIconsOption =
{
    /* Contains a pointer to the widget that's currently being
     * configured.  This is used in the custom configuration parse
     * routine for icons.  */
    ObjToIcons, IconsToObj, FreeIcons, NULL,
};

static Blt_OptionParseProc ObjToButton;
static Blt_OptionPrintProc ButtonToObj;
static Blt_CustomOption buttonOption = {
    ObjToButton, ButtonToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToUid;
static Blt_OptionPrintProc UidToObj;
static Blt_OptionFreeProc FreeUid;
Blt_CustomOption bltTreeViewUidOption = {
    ObjToUid, UidToObj, FreeUid, NULL,
};

static Blt_OptionParseProc ObjToScrollmode;
static Blt_OptionPrintProc ScrollmodeToObj;
static Blt_CustomOption scrollmodeOption = {
    ObjToScrollmode, ScrollmodeToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToSelectmode;
static Blt_OptionPrintProc SelectmodeToObj;
static Blt_CustomOption selectmodeOption = {
    ObjToSelectmode, SelectmodeToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToSeparator;
static Blt_OptionPrintProc SeparatorToObj;
static Blt_OptionFreeProc FreeSeparator;
static Blt_CustomOption separatorOption = {
    ObjToSeparator, SeparatorToObj, FreeSeparator, NULL,
};

static Blt_OptionParseProc ObjToLabel;
static Blt_OptionPrintProc LabelToObj;
static Blt_OptionFreeProc FreeLabel;
static Blt_CustomOption labelOption =
{
    ObjToLabel, LabelToObj, FreeLabel, NULL,
};


#define DEF_BUTTON_ACTIVE_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_BUTTON_ACTIVE_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_BUTTON_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_BUTTON_BORDERWIDTH		"1"
#if (TK_MAJOR_VERSION == 4) 
#define DEF_BUTTON_CLOSE_RELIEF		"flat"
#define DEF_BUTTON_OPEN_RELIEF		"flat"
#else
#define DEF_BUTTON_CLOSE_RELIEF		"solid"
#define DEF_BUTTON_OPEN_RELIEF		"solid"
#endif
#define DEF_BUTTON_NORMAL_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_NORMAL_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_BUTTON_NORMAL_FOREGROUND	STD_NORMAL_FOREGROUND
#define DEF_BUTTON_NORMAL_FG_MONO	STD_NORMAL_FG_MONO
#define DEF_BUTTON_SIZE			"7"

/* RGB_LIGHTBLUE1 */

#define DEF_TV_ACTIVE_FOREGROUND	"black"
#define DEF_TV_ACTIVE_ICONS \
	"blt::tv::activeOpenFolder blt::tv::activeCloseFolder"
#define DEF_TV_ACTIVE_RELIEF	"flat"
#define DEF_TV_ACTIVE_STIPPLE	"gray25"
#define DEF_TV_ALLOW_DUPLICATES	"yes"
#define DEF_TV_BACKGROUND	"white"
#define DEF_TV_BORDERWIDTH	STD_BORDERWIDTH
#define DEF_TV_BUTTON		"auto"
#define DEF_TV_DASHES		"dot"
#define DEF_TV_EXPORT_SELECTION	"no"
#define DEF_TV_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_TV_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_TV_FLAT		"no"
#define DEF_TV_FOCUS_DASHES	"dot"
#define DEF_TV_FOCUS_EDIT	"no"
#define DEF_TV_FOCUS_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_TV_FOCUS_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TV_FONT		"Courier 12"
#define DEF_TV_HEIGHT		"400"
#define DEF_TV_HIDE_LEAVES	"no"
#define DEF_TV_HIDE_ROOT	"yes"
#define DEF_TV_FOCUS_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TV_FOCUS_HIGHLIGHT_COLOR		"black"
#define DEF_TV_FOCUS_HIGHLIGHT_WIDTH		"2"
#define DEF_TV_ICONS "blt::tv::normalOpenFolder blt::tv::normalCloseFolder"
#define DEF_TV_VLINE_COLOR	RGB_GREY50
#define DEF_TV_VLINE_MONO	STD_NORMAL_FG_MONO
#define DEF_TV_LINESPACING	"0"
#define DEF_TV_LINEWIDTH	"1"
#define DEF_TV_MAKE_PATH	"no"
#define DEF_TV_NEW_TAGS		"no"
#define DEF_TV_NORMAL_BACKGROUND 	STD_NORMAL_BACKGROUND
#define DEF_TV_NORMAL_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TV_RELIEF		"sunken"
#define DEF_TV_RESIZE_CURSOR	"arrow"
#define DEF_TV_SCROLL_INCREMENT "20"
#define DEF_TV_SCROLL_MODE	"hierbox"
#define DEF_TV_SELECT_BACKGROUND 	STD_SELECT_BACKGROUND /* RGB_LIGHTBLUE1 */
#define DEF_TV_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_TV_SELECT_BORDERWIDTH "1"
#define DEF_TV_SELECT_FOREGROUND 	STD_SELECT_FOREGROUND
#define DEF_TV_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_TV_SELECT_MODE	"single"
#define DEF_TV_SELECT_RELIEF	"flat"
#define DEF_TV_SHOW_ROOT	"yes"
#define DEF_TV_SHOW_TITLES	"yes"
#define DEF_TV_SORT_SELECTION	"no"
#define DEF_TV_TAKE_FOCUS	"1"
#define DEF_TV_TEXT_COLOR	STD_NORMAL_FOREGROUND
#define DEF_TV_TEXT_MONO	STD_NORMAL_FG_MONO
#define DEF_TV_TRIMLEFT		""
#define DEF_TV_WIDTH		"200"

Blt_ConfigSpec bltTreeViewButtonSpecs[] =
{
    {BLT_CONFIG_BORDER, "-activebackground", "activeBackground", "Background",
	DEF_BUTTON_ACTIVE_BACKGROUND, Blt_Offset(TreeView, button.activeBorder),
	0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_BUTTON_ACTIVE_FOREGROUND, 
	Blt_Offset(TreeView, button.activeFgColor), 0},
    {BLT_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_NORMAL_BACKGROUND, Blt_Offset(TreeView, button.border), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 
	0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_DISTANCE, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDERWIDTH, Blt_Offset(TreeView, button.borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-closerelief", "closeRelief", "Relief",
	DEF_BUTTON_CLOSE_RELIEF, Blt_Offset(TreeView, button.closeRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_NORMAL_FOREGROUND, Blt_Offset(TreeView, button.fgColor), 0},
    {BLT_CONFIG_CUSTOM, "-images", "images", "Icons",
	(char *)NULL, Blt_Offset(TreeView, button.icons), BLT_CONFIG_NULL_OK, 
	&bltTreeViewIconsOption},
    {BLT_CONFIG_RELIEF, "-openrelief", "openRelief", "Relief",
	DEF_BUTTON_OPEN_RELIEF, Blt_Offset(TreeView, button.openRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DISTANCE, "-size", "size", "Size", 
	DEF_BUTTON_SIZE, Blt_Offset(TreeView, button.reqSize), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

Blt_ConfigSpec bltTreeViewEntrySpecs[] =
{
    {BLT_CONFIG_CUSTOM, "-activeicons", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, activeIcons),
	BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_CUSTOM, "-bindtags", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, tagsUid),
	BLT_CONFIG_NULL_OK, &bltTreeViewUidOption},
    {BLT_CONFIG_CUSTOM, "-button", (char *)NULL, (char *)NULL,
	DEF_TV_BUTTON, Blt_Offset(TreeViewEntry, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, &buttonOption},
    {BLT_CONFIG_CUSTOM, "-closecommand", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, closeCmd),
	BLT_CONFIG_NULL_OK, &bltTreeViewUidOption},
    {BLT_CONFIG_CUSTOM, "-data", (char *)NULL, (char *)NULL,
	(char *)NULL, 0, BLT_CONFIG_NULL_OK, &bltTreeViewDataOption},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, color), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DISTANCE, "-height", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, reqHeight),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-icons", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, icons),
	BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_CUSTOM, "-label", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, labelUid), 0, 
	&labelOption},
    {BLT_CONFIG_CUSTOM, "-opencommand", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, openCmd),
	BLT_CONFIG_NULL_OK, &bltTreeViewUidOption},
    {BLT_CONFIG_SHADOW, "-shadow", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, shadow),
	BLT_CONFIG_NULL_OK | BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_SHADOW, "-shadow", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(TreeViewEntry, shadow),
	BLT_CONFIG_NULL_OK | BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

Blt_ConfigSpec bltTreeViewSpecs[] =
{
    {BLT_CONFIG_CUSTOM, "-activeicons", "activeIcons", "Icons",
	DEF_TV_ACTIVE_ICONS, Blt_Offset(TreeView, activeIcons),
	BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_BITFLAG, 
	"-allowduplicates", "allowDuplicates", "AllowDuplicates",
	DEF_TV_ALLOW_DUPLICATES, Blt_Offset(TreeView, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_ALLOW_DUPLICATES},
    {BLT_CONFIG_BITFLAG, "-autocreate", "autoCreate", "AutoCreate",
	DEF_TV_MAKE_PATH, Blt_Offset(TreeView, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_FILL_ANCESTORS},
    {BLT_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TV_BACKGROUND, Blt_Offset(TreeView, border), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_DISTANCE, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_TV_BORDERWIDTH, Blt_Offset(TreeView, borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-button", "button", "Button",
	DEF_TV_BUTTON, Blt_Offset(TreeView, buttonFlags),
	BLT_CONFIG_DONT_SET_DEFAULT, &buttonOption},
    {BLT_CONFIG_STRING, "-closecommand", "closeCommand", "CloseCommand",
	(char *)NULL, Blt_Offset(TreeView, closeCmd), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	(char *)NULL, Blt_Offset(TreeView, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DASHES, "-dashes", "dashes", "Dashes",
	DEF_TV_DASHES, Blt_Offset(TreeView, dashes),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITFLAG, "-exportselection", "exportSelection",
	"ExportSelection", DEF_TV_EXPORT_SELECTION, 
	Blt_Offset(TreeView, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)TV_SELECT_EXPORT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_BOOLEAN, "-flat", "flat", "Flat",
	DEF_TV_FLAT, Blt_Offset(TreeView, flatView),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DASHES, "-focusdashes", "focusDashes", "FocusDashes",
	DEF_TV_FOCUS_DASHES, Blt_Offset(TreeView, focusDashes),
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, 
	"-focusforeground", "focusForeground", "FocusForeground",
	DEF_TV_FOCUS_FOREGROUND, Blt_Offset(TreeView, focusColor),
	BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, 
	"-focusforeground", "focusForeground", "FocusForeground",
	DEF_TV_FOCUS_FG_MONO, Blt_Offset(TreeView, focusColor),
	BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_FONT, "-font", "font", "Font",
	DEF_TV_FONT, Blt_Offset(TreeView, font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TV_TEXT_COLOR, Blt_Offset(TreeView, fgColor),
	BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TV_TEXT_MONO, Blt_Offset(TreeView, fgColor), 
	BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_DISTANCE, "-height", "height", "Height",
	DEF_TV_HEIGHT, Blt_Offset(TreeView, reqHeight),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITFLAG, "-hideleaves", "hideLeaves", "HideLeaves",
	DEF_TV_HIDE_LEAVES, Blt_Offset(TreeView, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_HIDE_LEAVES},
    {BLT_CONFIG_BITFLAG, "-hideroot", "hideRoot", "HideRoot",
	DEF_TV_HIDE_ROOT, Blt_Offset(TreeView, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_HIDE_ROOT},
    {BLT_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_TV_FOCUS_HIGHLIGHT_BACKGROUND, 
        Blt_Offset(TreeView, highlightBgColor), 0},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_TV_FOCUS_HIGHLIGHT_COLOR, Blt_Offset(TreeView, highlightColor), 0},
    {BLT_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_TV_FOCUS_HIGHLIGHT_WIDTH, 
	Blt_Offset(TreeView, highlightWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-icons", "icons", "Icons",
	DEF_TV_ICONS, Blt_Offset(TreeView, icons), 
	BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_BORDER, "-nofocusselectbackground", "noFocusSelectBackground",
	"NoFocusSelectBackground", DEF_TV_SELECT_BACKGROUND, 
	Blt_Offset(TreeView, selOutFocusBorder), TK_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-nofocusselectforeground", "noFocusSelectForeground", 
	"NoFocusSelectForeground", DEF_TV_SELECT_FOREGROUND, 
	Blt_Offset(TreeView, selOutFocusFgColor), TK_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_TV_VLINE_COLOR, Blt_Offset(TreeView, lineColor),
	BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_TV_VLINE_MONO, Blt_Offset(TreeView, lineColor),
	BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_DISTANCE, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_TV_LINESPACING, Blt_Offset(TreeView, leader),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DISTANCE, "-linewidth", "lineWidth", "LineWidth",
	DEF_TV_LINEWIDTH, Blt_Offset(TreeView, lineWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITFLAG, "-newtags", "newTags", "NewTags",
	DEF_TV_NEW_TAGS, Blt_Offset(TreeView, flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_NEW_TAGS},
    {BLT_CONFIG_STRING, "-opencommand", "openCommand", "OpenCommand",
	(char *)NULL, Blt_Offset(TreeView, openCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_TV_RELIEF, Blt_Offset(TreeView, relief), 0},
    {BLT_CONFIG_CURSOR, "-resizecursor", "resizeCursor", "ResizeCursor",
	DEF_TV_RESIZE_CURSOR, Blt_Offset(TreeView, resizeCursor), 0},
    {BLT_CONFIG_CUSTOM, "-scrollmode", "scrollMode", "ScrollMode",
	DEF_TV_SCROLL_MODE, Blt_Offset(TreeView, scrollMode),
	BLT_CONFIG_DONT_SET_DEFAULT, &scrollmodeOption},
    {BLT_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_TV_SELECT_BACKGROUND, Blt_Offset(TreeView, selInFocusBorder), 0},
    {BLT_CONFIG_DISTANCE, 
	"-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_TV_SELECT_BORDERWIDTH, Blt_Offset(TreeView, selBorderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	(char *)NULL, Blt_Offset(TreeView, selectCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TV_SELECT_FOREGROUND, Blt_Offset(TreeView, selInFocusFgColor), 0},
    {BLT_CONFIG_CUSTOM, "-selectmode", "selectMode", "SelectMode",
	DEF_TV_SELECT_MODE, Blt_Offset(TreeView, selectMode),
	BLT_CONFIG_DONT_SET_DEFAULT, &selectmodeOption},
    {BLT_CONFIG_RELIEF, "-selectrelief", "selectRelief", "Relief",
	DEF_TV_SELECT_RELIEF, Blt_Offset(TreeView, selRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-separator", "separator", "Separator",
	(char *)NULL, Blt_Offset(TreeView, pathSep), BLT_CONFIG_NULL_OK, 
	&separatorOption},
    {BLT_CONFIG_BITFLAG, "-showtitles", "showTitles", "ShowTitles",
	DEF_TV_SHOW_TITLES, Blt_Offset(TreeView, flags), 0,
        (Blt_CustomOption *)TV_SHOW_COLUMN_TITLES},
    {BLT_CONFIG_BITFLAG, "-sortselection", "sortSelection", "SortSelection",
	DEF_TV_SORT_SELECTION, Blt_Offset(TreeView, flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_SELECT_SORTED},
    {BLT_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TV_TAKE_FOCUS, Blt_Offset(TreeView, takeFocus), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-tree", "tree", "Tree", 
	(char *)NULL, Blt_Offset(TreeView, tree), BLT_CONFIG_NULL_OK, 
	&bltTreeViewTreeOption},
    {BLT_CONFIG_STRING, "-trim", "trim", "Trim",
	DEF_TV_TRIMLEFT, Blt_Offset(TreeView, trimLeft), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DISTANCE, "-width", "width", "Width",
	DEF_TV_WIDTH, Blt_Offset(TreeView, reqWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, 
	"-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, Blt_Offset(TreeView, xScrollCmdPrefix), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DISTANCE, 
	"-xscrollincrement", "xScrollIncrement", "ScrollIncrement",
	DEF_TV_SCROLL_INCREMENT, Blt_Offset(TreeView, xScrollUnits),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, 
        "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, Blt_Offset(TreeView, yScrollCmdPrefix), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DISTANCE, 
	"-yscrollincrement", "yScrollIncrement", "ScrollIncrement",
	DEF_TV_SCROLL_INCREMENT, Blt_Offset(TreeView, yScrollUnits),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
static Blt_TreeNotifyEventProc TreeEventProc;
static Blt_TreeTraceProc TreeTraceProc;
static Tcl_CmdDeleteProc WidgetInstCmdDeleteProc;
static Tcl_FreeProc DestroyTreeView;
static Tcl_IdleProc DisplayTreeView;
static Tk_EventProc TreeViewEventProc;

static int ComputeVisibleEntries _ANSI_ARGS_((TreeView *tvPtr));

EXTERN int Blt_TreeCmdGetToken _ANSI_ARGS_((Tcl_Interp *interp, 
	CONST char *treeName, Blt_Tree *treePtr));

extern Blt_TreeApplyProc Blt_TreeViewSortApplyProc;
static Blt_BindPickProc PickItem;
static Blt_BindTagProc GetTags;
static Tcl_FreeProc DestroyEntry;
static Tcl_ObjCmdProc TreeViewObjCmd;
static Tk_ImageChangedProc IconChangedProc;
static Tk_SelectionProc SelectionProc;

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewEventuallyRedraw --
 *
 *	Queues a request to redraw the widget at the next idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TreeViewEventuallyRedraw(TreeView *tvPtr)
{
    if ((tvPtr->tkwin != NULL) && ((tvPtr->flags & TV_REDRAW) == 0)) {
	tvPtr->flags |= TV_REDRAW;
	Tcl_DoWhenIdle(DisplayTreeView, tvPtr);
    }
}

void
Blt_TreeViewTraceColumn(TreeView *tvPtr, TreeViewColumn *columnPtr)
{
    Blt_TreeCreateTrace(tvPtr->tree, NULL /* Node */, columnPtr->key, NULL,
	TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	TreeTraceProc, tvPtr);
}

static void
TraceColumns(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;

    for(linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	Blt_TreeCreateTrace(
		tvPtr->tree, 
		NULL /* Node */, 
		columnPtr->key /* Key pattern */, 
		NULL /* Tag */,
	        TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	        TreeTraceProc /* Callback routine */, 
		tvPtr /* Client data */);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToTree --
 *
 *	Convert the string representing the name of a tree object 
 *	into a tree token.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTree(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    Blt_Tree *treePtr = (Blt_Tree *)(widgRec + offset);
    Blt_Tree tree;
    char *string;

    tree = NULL;
    string = Tcl_GetString(objPtr);
    if ((string[0] != '\0') && 
	(Blt_TreeGetToken(interp, string, &tree) != TCL_OK)) {
	return TCL_ERROR;
    }
    *treePtr = tree;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TreeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    Blt_Tree tree = *(Blt_Tree *)(widgRec + offset);

    if (tree == NULL) {
	return bltEmptyStringObjPtr;
    } else {
	return Tcl_NewStringObj(Blt_TreeName(tree), -1);
    }
}

/*ARGSUSED*/
static void
FreeTree(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    Blt_Tree *treePtr = (Blt_Tree *)(widgRec + offset);

    if (*treePtr != NULL) {
	Blt_TreeNode root;
	TreeView *tvPtr = clientData;

	/* 
	 * Release the current tree, removing any entry fields. 
	 */
	root = Blt_TreeRootNode(*treePtr);
	Blt_TreeApply(root, DeleteApplyProc, tvPtr);
	Blt_TreeViewClearSelection(tvPtr);
	Blt_TreeReleaseToken(*treePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToScrollmode --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToScrollmode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* New legend position string */
    char *widgRec,
    int offset)
{
    char *string;
    char c;
    int *modePtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    c = string[0];
    if ((c == 'l') && (strcmp(string, "listbox") == 0)) {
	*modePtr = BLT_SCROLL_MODE_LISTBOX;
    } else if ((c == 't') && (strcmp(string, "treeview") == 0)) {
	*modePtr = BLT_SCROLL_MODE_HIERBOX;
    } else if ((c == 'h') && (strcmp(string, "hiertable") == 0)) {
	*modePtr = BLT_SCROLL_MODE_HIERBOX;
    } else if ((c == 'c') && (strcmp(string, "canvas") == 0)) {
	*modePtr = BLT_SCROLL_MODE_CANVAS;
    } else {
	Tcl_AppendResult(interp, "bad scroll mode \"", string,
	    "\": should be \"treeview\", \"listbox\", or \"canvas\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScrollmodeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ScrollmodeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    int mode = *(int *)(widgRec + offset);

    switch (mode) {
    case BLT_SCROLL_MODE_LISTBOX:
	return Tcl_NewStringObj("listbox", -1);
    case BLT_SCROLL_MODE_HIERBOX:
	return Tcl_NewStringObj("hierbox", -1);
    case BLT_SCROLL_MODE_CANVAS:
	return Tcl_NewStringObj("canvas", -1);
    default:
	return Tcl_NewStringObj("unknown scroll mode", -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToSelectmode --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToSelectmode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    char *string;
    char c;
    int *modePtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    c = string[0];
    if ((c == 's') && (strcmp(string, "single") == 0)) {
	*modePtr = SELECT_MODE_SINGLE;
    } else if ((c == 'm') && (strcmp(string, "multiple") == 0)) {
	*modePtr = SELECT_MODE_MULTIPLE;
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	*modePtr = SELECT_MODE_SINGLE;
    } else {
	Tcl_AppendResult(interp, "bad select mode \"", string,
	    "\": should be \"single\" or \"multiple\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectmodeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
SelectmodeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    int mode = *(int *)(widgRec + offset);

    switch (mode) {
    case SELECT_MODE_SINGLE:
	return Tcl_NewStringObj("single", -1);
    case SELECT_MODE_MULTIPLE:
	return Tcl_NewStringObj("multiple", -1);
    default:
	return Tcl_NewStringObj("unknown scroll mode", -1);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * ObjToButton --
 *
 *	Convert a string to one of three values.
 *		0 - false, no, off
 *		1 - true, yes, on
 *		2 - auto
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToButton(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    char *string;
    int *flagsPtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    if ((string[0] == 'a') && (strcmp(string, "auto") == 0)) {
	*flagsPtr &= ~BUTTON_MASK;
	*flagsPtr |= BUTTON_AUTO;
    } else {
	int bool;

	if (Tcl_GetBooleanFromObj(interp, objPtr, &bool) != TCL_OK) {
	    return TCL_ERROR;
	}
	*flagsPtr &= ~BUTTON_MASK;
	if (bool) {
	    *flagsPtr |= BUTTON_SHOW;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ButtonToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    int bool;
    unsigned int flags = *(int *)(widgRec + offset);

    bool = (flags & BUTTON_MASK);
    if (bool == BUTTON_AUTO) {
	return Tcl_NewStringObj("auto", 4);
    } else {
	return Tcl_NewBooleanObj(bool);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToScrollmode --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToSeparator(clientData, interp, tkwin, objPtr, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    Tcl_Obj *objPtr;		/* Tcl_Obj representing the new value. */
    char *widgRec;
    int offset;
{
    char **sepPtr = (char **)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (*string == '\0') {
	*sepPtr = SEPARATOR_LIST;
    } else if (strcmp(string, "none") == 0) {
	*sepPtr = SEPARATOR_NONE;
    } else {
	*sepPtr = Blt_Strdup(string);
    } 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SeparatorToObj --
 *
 * Results:
 *	The string representation of the separator is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
SeparatorToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    char *separator = *(char **)(widgRec + offset);

    if (separator == SEPARATOR_NONE) {
	return bltEmptyStringObjPtr;
    } else if (separator == SEPARATOR_LIST) {
	return Tcl_NewStringObj("list", -1);
    }  else {
	return Tcl_NewStringObj(separator, -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeSeparator --
 *
 *	Free the UID from the widget record, setting it to NULL.
 *
 * Results:
 *	The UID in the widget record is set to NULL.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreeSeparator(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    char *separator = *(char **)(widgRec + offset);

    if ((separator != SEPARATOR_LIST) && (separator != SEPARATOR_NONE)) {
	Blt_Free(separator);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToLabel --
 *
 *	Convert the string representing the label. 
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToLabel(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    UID *labelPtr = (UID *)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] != '\0') {
	TreeView *tvPtr = clientData;

	*labelPtr = Blt_TreeViewGetUid(tvPtr, string);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeToObj --
 *
 * Results:
 *	The string of the entry's label is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
LabelToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID labelUid = *(UID *)(widgRec + offset);
    char *string;

    if (labelUid == NULL) {
	TreeViewEntry *entryPtr  = (TreeViewEntry *)widgRec;

	string = Blt_TreeNodeLabel(entryPtr->node);
    } else {
	string = labelUid;
    }
    return Tcl_NewStringObj(string, -1);
}

/*ARGSUSED*/
static void
FreeLabel(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID *labelPtr = (UID *)(widgRec + offset);

    if (*labelPtr != NULL) {
	TreeView *tvPtr = clientData;

	Blt_TreeViewFreeUid(tvPtr, *labelPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewGetUid --
 *
 *	Gets or creates a unique string identifier.  Strings are
 *	reference counted.  The string is placed into a hashed table
 *	local to the treeview.
 *
 * Results:
 *	Returns the pointer to the hashed string.
 *
 *---------------------------------------------------------------------- 
 */
UID
Blt_TreeViewGetUid(TreeView *tvPtr, CONST char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;
    int refCount;

    hPtr = Blt_CreateHashEntry(&tvPtr->uidTable, string, &isNew);
    if (isNew) {
	refCount = 1;
    } else {
	refCount = (int)Blt_GetHashValue(hPtr);
	refCount++;
    }
    Blt_SetHashValue(hPtr, (ClientData)refCount);
    return Blt_GetHashKey(&tvPtr->uidTable, hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewFreeUid --
 *
 *	Releases the uid.  Uids are reference counted, so only when
 *	the reference count is zero (i.e. no one else is using the
 *	string) is the entry removed from the hash table.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TreeViewFreeUid(TreeView *tvPtr, UID uid)
{
    Blt_HashEntry *hPtr;
    int refCount;

    hPtr = Blt_FindHashEntry(&tvPtr->uidTable, uid);
    assert(hPtr != NULL);
    refCount = (int)Blt_GetHashValue(hPtr);
    refCount--;
    if (refCount > 0) {
	Blt_SetHashValue(hPtr, (ClientData)refCount);
    } else {
	Blt_DeleteHashEntry(&tvPtr->uidTable, hPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToUid --
 *
 *	Converts the string to a Uid. Uid's are hashed, reference
 *	counted strings.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToUid(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    TreeView *tvPtr = clientData;
    UID *uidPtr = (UID *)(widgRec + offset);
    UID newId;
    char *string;

    newId = NULL;
    string = Tcl_GetString(objPtr);
    if (*string != '\0') {
	newId = Blt_TreeViewGetUid(tvPtr, string);
    }
    *uidPtr = newId;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UidToObj --
 *
 *	Returns the uid as a string.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
UidToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID uid = *(UID *)(widgRec + offset);

    if (uid == NULL) {
	return bltEmptyStringObjPtr;
    }
    return Tcl_NewStringObj(uid, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeUid --
 *
 *	Free the UID from the widget record, setting it to NULL.
 *
 * Results:
 *	The UID in the widget record is set to NULL.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreeUid(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID *uidPtr = (UID *)(widgRec + offset);

    if (*uidPtr != NULL) {
	TreeView *tvPtr = clientData;

	Blt_TreeViewFreeUid(tvPtr, *uidPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * IconChangedProc
 *
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
IconChangedProc(
    ClientData clientData,
    int x,			/* Not used. */
    int y,			/* Not used. */
    int width,			/* Not used. */
    int height,			/* Not used. */
    int imageWidth, 		/* Not used. */
    int imageHeight)		/* Not used. */
{
    TreeView *tvPtr = clientData;

    tvPtr->flags |= (TV_DIRTY | TV_LAYOUT | TV_SCROLL);
    Blt_TreeViewEventuallyRedraw(tvPtr);
}

TreeViewIcon
Blt_TreeViewGetIcon(TreeView *tvPtr, CONST char *iconName)
{
    Blt_HashEntry *hPtr;
    int isNew;
    struct TreeViewIconStruct *iconPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->iconTable, iconName, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(tvPtr->interp, tvPtr->tkwin, (char *)iconName, 
		IconChangedProc, tvPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&tvPtr->iconTable, hPtr);
	    return NULL;
	}
	Tk_SizeOfImage(tkImage, &width, &height);
	iconPtr = Blt_Malloc(sizeof(struct TreeViewIconStruct));
	iconPtr->tkImage = tkImage;
	iconPtr->hashPtr = hPtr;
	iconPtr->refCount = 1;
	iconPtr->width = width;
	iconPtr->height = height;
	Blt_SetHashValue(hPtr, iconPtr);
    } else {
	iconPtr = Blt_GetHashValue(hPtr);
	iconPtr->refCount++;
    }
    return iconPtr;
}

void
Blt_TreeViewFreeIcon(
    TreeView *tvPtr,
    struct TreeViewIconStruct *iconPtr)
{
    iconPtr->refCount--;
    if (iconPtr->refCount == 0) {
	Blt_DeleteHashEntry(&tvPtr->iconTable, iconPtr->hashPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
}

static void
DumpIconTable(TreeView *tvPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    struct TreeViewIconStruct *iconPtr;

    for (hPtr = Blt_FirstHashEntry(&tvPtr->iconTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	iconPtr = Blt_GetHashValue(hPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
    Blt_DeleteHashTable(&tvPtr->iconTable);
}

/*
 *----------------------------------------------------------------------
 *
 * ObjToIcons --
 *
 *	Convert a list of image names into Tk images.
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
ObjToIcons(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset)
{
    Tcl_Obj **objv;
    TreeView *tvPtr = clientData;
    TreeViewIcon **iconPtrPtr = (TreeViewIcon **)(widgRec + offset);
    TreeViewIcon *icons;
    int objc;
    int result;

    result = TCL_OK;
    icons = NULL;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 0) {
	register int i;
	
	icons = Blt_Malloc(sizeof(TreeViewIcon *) * (objc + 1));
	assert(icons);
	for (i = 0; i < objc; i++) {
	    icons[i] = Blt_TreeViewGetIcon(tvPtr, Tcl_GetString(objv[i]));
	    if (icons[i] == NULL) {
		result = TCL_ERROR;
		break;
	    }
	}
	icons[i] = NULL;
    }
    *iconPtrPtr = icons;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * IconsToObj --
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
IconsToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset)
{
    TreeViewIcon *icons = *(TreeViewIcon **)(widgRec + offset);
    Tcl_Obj *listObjPtr;
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (icons != NULL) {
	register TreeViewIcon *iconPtr;
	Tcl_Obj *objPtr;

	for (iconPtr = icons; *iconPtr != NULL; iconPtr++) {
	    objPtr = Tcl_NewStringObj(Blt_NameOfImage((*iconPtr)->tkImage), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

	}
    }
    return listObjPtr;
}

/*ARGSUSED*/
static void
FreeIcons(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    TreeViewIcon *icons = *(TreeViewIcon **)(widgRec + offset);

    if (icons != NULL) {
	register TreeViewIcon *iconPtr;
	TreeView *tvPtr = clientData;

	for (iconPtr = icons; *iconPtr != NULL; iconPtr++) {
	    Blt_TreeViewFreeIcon(tvPtr, *iconPtr);
	}
	Blt_Free(icons);
    }
}

TreeViewEntry *
Blt_NodeToEntry(TreeView *tvPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	abort();
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

int
Blt_TreeViewApply(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,	/* Root entry of subtree. */
    TreeViewApplyProc *proc,	/* Procedure to call for each entry. */
    unsigned int flags)
{
    if ((flags & ENTRY_HIDDEN) && 
	(Blt_TreeViewEntryIsHidden(entryPtr))) {
	return TCL_OK;		/* Hidden node. */
    }
    if ((flags & ENTRY_HIDDEN) && (entryPtr->flags & ENTRY_HIDDEN)) {
	return TCL_OK;		/* Hidden node. */
    }
    if (((flags & ENTRY_CLOSED) == 0) || 
	((entryPtr->flags & ENTRY_CLOSED) == 0)) {
	TreeViewEntry *childPtr;
	Blt_TreeNode node, next;

	for (node = Blt_TreeFirstChild(entryPtr->node); node != NULL; 
	     node = next) {
	    next = Blt_TreeNextSibling(node);
	    /* 
	     * Get the next child before calling Blt_TreeViewApply
	     * recursively.  This is because the apply callback may
	     * delete the node and its link.
	     */
	    childPtr = Blt_NodeToEntry(tvPtr, node);
	    if (Blt_TreeViewApply(tvPtr, childPtr, proc, flags) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    if ((*proc) (tvPtr, entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_TreeViewEntryIsHidden(TreeViewEntry *entryPtr)
{
    TreeView *tvPtr = entryPtr->tvPtr; 

    if ((tvPtr->flags & TV_HIDE_LEAVES) && (Blt_TreeIsLeaf(entryPtr->node))) {
	return TRUE;
    }
    return (entryPtr->flags & ENTRY_HIDDEN) ? TRUE : FALSE;
}

#ifdef notdef
int
Blt_TreeViewEntryIsMapped(TreeViewEntry *entryPtr)
{
    TreeView *tvPtr = entryPtr->tvPtr; 

    /* Don't check if the entry itself is open, only that its
     * ancestors are. */
    if (Blt_TreeViewEntryIsHidden(entryPtr)) {
	return FALSE;
    }
    if (entryPtr == tvPtr->rootPtr) {
	return TRUE;
    }
    entryPtr = Blt_TreeViewParentEntry(entryPtr);
    while (entryPtr != tvPtr->rootPtr) {
	if (entryPtr->flags & (ENTRY_CLOSED | ENTRY_HIDDEN)) {
	    return FALSE;
	}
	entryPtr = Blt_TreeViewParentEntry(entryPtr);
    }
    return TRUE;
}
#endif

TreeViewEntry *
Blt_TreeViewFirstChild(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_TreeFirstChild(entryPtr->node); node != NULL; 
	 node = Blt_TreeNextSibling(node)) {
	entryPtr = Blt_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDDEN) == 0) || 
	    (!Blt_TreeViewEntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeViewLastChild(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_TreeLastChild(entryPtr->node); node != NULL; 
	 node = Blt_TreePrevSibling(node)) {
	entryPtr = Blt_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDDEN) == 0) ||
	    (!Blt_TreeViewEntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeViewNextSibling(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_TreeNextSibling(entryPtr->node); node != NULL; 
	 node = Blt_TreeNextSibling(node)) {
	entryPtr = Blt_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDDEN) == 0) ||
	    (!Blt_TreeViewEntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeViewPrevSibling(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_TreePrevSibling(entryPtr->node); node != NULL; 
	 node = Blt_TreePrevSibling(node)) {
	entryPtr = Blt_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDDEN) == 0) ||
	    (!Blt_TreeViewEntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewPrevEntry --
 *
 *	Returns the "previous" node in the tree.  This node (in 
 *	depth-first order) is its parent if the node has no siblings
 *	that are previous to it.  Otherwise it is the last descendant 
 *	of the last sibling.  In this case, descend the sibling's
 *	hierarchy, using the last child at any ancestor, until we
 *	we find a leaf.
 *
 *----------------------------------------------------------------------
 */
TreeViewEntry *
Blt_TreeViewPrevEntry(TreeViewEntry *entryPtr, unsigned int mask)
{
    TreeView *tvPtr = entryPtr->tvPtr; 
    TreeViewEntry *prevPtr;

    if (entryPtr->node == Blt_TreeRootNode(tvPtr->tree)) {
	return NULL;		/* The root is the first node. */
    }
    prevPtr = Blt_TreeViewPrevSibling(entryPtr, mask);
    if (prevPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	prevPtr = Blt_TreeViewParentEntry(entryPtr);
    } else {
	/*
	 * Traverse down the right-most thread in order to select the
	 * last entry.  Stop if we find a "closed" entry or reach a leaf.
	 */
	entryPtr = prevPtr;
	while ((entryPtr->flags & mask) == 0) {
	    entryPtr = Blt_TreeViewLastChild(entryPtr, mask);
	    if (entryPtr == NULL) {
		break;		/* Found a leaf. */
	    }
	    prevPtr = entryPtr;
	}
    }
    if (prevPtr == NULL) {
	return NULL;
    }
    return prevPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewNextNode --
 *
 *	Returns the "next" node in relation to the given node.  
 *	The next node (in depth-first order) is either the first 
 *	child of the given node the next sibling if the node has
 *	no children (the node is a leaf).  If the given node is the 
 *	last sibling, then try it's parent next sibling.  Continue
 *	until we either find a next sibling for some ancestor or 
 *	we reach the root node.  In this case the current node is 
 *	the last node in the tree.
 *
 *----------------------------------------------------------------------
 */
TreeViewEntry *
Blt_TreeViewNextEntry(TreeViewEntry *entryPtr, unsigned int mask)
{
    TreeView *tvPtr = entryPtr->tvPtr; 
    TreeViewEntry *nextPtr;
    int ignoreLeaf;

    ignoreLeaf = ((tvPtr->flags & TV_HIDE_LEAVES) && 
		  (Blt_TreeIsLeaf(entryPtr->node)));

    if ((!ignoreLeaf) && ((entryPtr->flags & mask) == 0)) {
	nextPtr = Blt_TreeViewFirstChild(entryPtr, mask); 
	if (nextPtr != NULL) {
	    return nextPtr;	/* Pick the first sub-node. */
	}
    }


    /* 
     * Back up until to a level where we can pick a "next sibling".  
     * For the last entry we'll thread our way back to the root.
     */

    while (entryPtr != tvPtr->rootPtr) {
	nextPtr = Blt_TreeViewNextSibling(entryPtr, mask);
	if (nextPtr != NULL) {
	    return nextPtr;
	}
	entryPtr = Blt_TreeViewParentEntry(entryPtr);
    }
    return NULL;		/* At root, no next node. */
}

void
Blt_TreeViewConfigureButtons(TreeView *tvPtr)
{
    GC newGC;
    TreeViewButton *buttonPtr = &tvPtr->button;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->fgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->normalGC);
    }
    buttonPtr->normalGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->activeGC);
    }
    buttonPtr->activeGC = newGC;

    buttonPtr->width = buttonPtr->height = ODD(buttonPtr->reqSize);
    if (buttonPtr->icons != NULL) {
	register int i;
	int width, height;

	for (i = 0; i < 2; i++) {
	    if (buttonPtr->icons[i] == NULL) {
		break;
	    }
	    width = TreeViewIconWidth(buttonPtr->icons[i]);
	    height = TreeViewIconWidth(buttonPtr->icons[i]);
	    if (buttonPtr->width < width) {
		buttonPtr->width = width;
	    }
	    if (buttonPtr->height < height) {
		buttonPtr->height = height;
	    }
	}
    }
    buttonPtr->width += 2 * buttonPtr->borderWidth;
    buttonPtr->height += 2 * buttonPtr->borderWidth;
}

int
Blt_TreeViewConfigureEntry(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,
    int objc,
    Tcl_Obj *CONST *objv,
    int flags)
{
    GC newGC;
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;

    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewUidOption.clientData = tvPtr;
    labelOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(tvPtr->interp, tvPtr->tkwin, 
	bltTreeViewEntrySpecs, objc, objv, (char *)entryPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /* 
     * Check if there are values that need to be added 
     */
    for(linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	Blt_TreeViewAddValue(entryPtr, columnPtr);
    }

    newGC = NULL;
    if ((entryPtr->font != NULL) || (entryPtr->color != NULL)) {
	Tk_Font font;
	XColor *colorPtr;
	XGCValues gcValues;
	unsigned long gcMask;

	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeViewGetStyleFont(tvPtr, tvPtr->treeColumn.stylePtr);
	}
	colorPtr = CHOOSE(tvPtr->fgColor, entryPtr->color);
	gcMask = GCForeground | GCFont;
	gcValues.foreground = colorPtr->pixel;
	gcValues.font = Tk_FontId(font);
	newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, entryPtr->gc);
    }
    /* Assume all changes require a new layout. */
    entryPtr->gc = newGC;
    entryPtr->flags |= ENTRY_LAYOUT_PENDING;
    if (Blt_ObjConfigModified(bltTreeViewEntrySpecs, "-font", (char *)NULL)) {
	tvPtr->flags |= TV_UPDATE;
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    return TCL_OK;
}

void
Blt_TreeViewDestroyValue(TreeView *tvPtr, TreeViewValue *valuePtr)
{
    if (valuePtr->stylePtr != NULL) {
	Blt_TreeViewFreeStyle(tvPtr, valuePtr->stylePtr);
    }
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
    }
}


static void
DestroyEntry(DestroyData data)
{
    TreeViewEntry *entryPtr = (TreeViewEntry *)data;
    TreeView *tvPtr;
    
    tvPtr = entryPtr->tvPtr;
    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewUidOption.clientData = tvPtr;
    labelOption.clientData = tvPtr;
    Blt_FreeObjOptions(bltTreeViewEntrySpecs, (char *)entryPtr, tvPtr->display,
	0);
    if (!Blt_TreeTagTableIsShared(tvPtr->tree)) {
	/* Don't clear tags unless this client is the only one using
	 * the tag table.*/
	Blt_TreeClearTags(tvPtr->tree, entryPtr->node);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, entryPtr->gc);
    }
    if (entryPtr->shadow.color != NULL) {
	Tk_FreeColor(entryPtr->shadow.color);
    }
    /* Delete the chain of data values from the entry. */
    if (entryPtr->values != NULL) {
	TreeViewValue *valuePtr, *nextPtr;
	
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    Blt_TreeViewDestroyValue(tvPtr, valuePtr);
	}
	entryPtr->values = NULL;
    }
    if (entryPtr->fullName != NULL) {
	Blt_Free(entryPtr->fullName);
    }
    if (entryPtr->textPtr != NULL) {
	Blt_Free(entryPtr->textPtr);
    }
    Blt_PoolFreeItem(tvPtr->entryPool, entryPtr);
}

TreeViewEntry *
Blt_TreeViewParentEntry(TreeViewEntry *entryPtr)
{
    TreeView *tvPtr = entryPtr->tvPtr; 
    Blt_TreeNode node;

    if (entryPtr->node == Blt_TreeRootNode(tvPtr->tree)) {
	return NULL;
    }
    node = Blt_TreeNodeParent(entryPtr->node);
    if (node == NULL) {
	return NULL;
    }
    return Blt_NodeToEntry(tvPtr, node);
}

static void
FreeEntry(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    if (entryPtr == tvPtr->activePtr) {
	tvPtr->activePtr = Blt_TreeViewParentEntry(entryPtr);
    }
    if (entryPtr == tvPtr->activeButtonPtr) {
	tvPtr->activeButtonPtr = NULL;
    }
    if (entryPtr == tvPtr->focusPtr) {
	tvPtr->focusPtr = Blt_TreeViewParentEntry(entryPtr);
	Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
    }
    if (entryPtr == tvPtr->selAnchorPtr) {
	tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
    }
    Blt_TreeViewDeselectEntry(tvPtr, entryPtr);
    Blt_TreeViewPruneSelection(tvPtr, entryPtr);
    Blt_DeleteBindings(tvPtr->bindTable, entryPtr);
    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, entryPtr->node);
    if (hPtr != NULL) {
	Blt_DeleteHashEntry(&tvPtr->entryTable, hPtr);
    }
    entryPtr->node = NULL;

    Tcl_EventuallyFree(entryPtr, DestroyEntry);
    /*
     * Indicate that the screen layout of the hierarchy may have changed
     * because this node was deleted.  The screen positions of the nodes
     * in tvPtr->visibleArr are invalidated.
     */
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
}

int
Blt_TreeViewEntryIsSelected(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->selectTable, (char *)entryPtr);
    return (hPtr != NULL);
}

void
Blt_TreeViewSelectEntry(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->selectTable, (char *)entryPtr, &isNew);
    if (isNew) {
	Blt_ChainLink *linkPtr;

	linkPtr = Blt_ChainAppend(tvPtr->selChainPtr, entryPtr);
	Blt_SetHashValue(hPtr, linkPtr);
    }
}

void
Blt_TreeViewDeselectEntry(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->selectTable, (char *)entryPtr);
    if (hPtr != NULL) {
	Blt_ChainLink *linkPtr;

	linkPtr = Blt_GetHashValue(hPtr);
	Blt_ChainDeleteLink(tvPtr->selChainPtr, linkPtr);
	Blt_DeleteHashEntry(&tvPtr->selectTable, hPtr);
    }
}

char *
Blt_TreeViewGetFullName(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,
    int checkEntryLabel,
    Tcl_DString *resultPtr)
{
    Blt_TreeNode node;
    char **names;		/* Used the stack the component names. */
    char *staticSpace[64];
    int level;
    register int i;

    level = Blt_TreeNodeDepth(tvPtr->tree, entryPtr->node);
    if (tvPtr->rootPtr->labelUid == NULL) {
	level--;
    }
    if (level > 64) {
	names = Blt_Malloc((level + 2) * sizeof(char *));
	assert(names);
    } else {
	names = staticSpace;
    }
    for (i = level; i >= 0; i--) {
	/* Save the name of each ancestor in the name array. */
	if (checkEntryLabel) {
	    names[i] = GETLABEL(entryPtr);
	} else {
	    names[i] = Blt_TreeNodeLabel(entryPtr->node);
	}
	node = Blt_TreeNodeParent(entryPtr->node);
	if (node != NULL) {
	    entryPtr = Blt_NodeToEntry(tvPtr, node);
	}
    }
    Tcl_DStringInit(resultPtr);
    if (level >= 0) {
	if ((tvPtr->pathSep == SEPARATOR_LIST) || 
	    (tvPtr->pathSep == SEPARATOR_NONE)) {
	    for (i = 0; i <= level; i++) {
		Tcl_DStringAppendElement(resultPtr, names[i]);
	    }
	} else {
	    Tcl_DStringAppend(resultPtr, names[0], -1);
	    for (i = 1; i <= level; i++) {
		Tcl_DStringAppend(resultPtr, tvPtr->pathSep, -1);
		Tcl_DStringAppend(resultPtr, names[i], -1);
	    }
	}
    } else {
	if ((tvPtr->pathSep != SEPARATOR_LIST) &&
	    (tvPtr->pathSep != SEPARATOR_NONE)) {
	    Tcl_DStringAppend(resultPtr, tvPtr->pathSep, -1);
	}
    }
    if (names != staticSpace) {
	Blt_Free(names);
    }
    return Tcl_DStringValue(resultPtr);
}


int
Blt_TreeViewCloseEntry(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    char *cmd;

    if (entryPtr->flags & ENTRY_CLOSED) {
	return TCL_OK;		/* Entry is already closed. */
    }
    entryPtr->flags |= ENTRY_CLOSED;

    /*
     * Invoke the entry's "close" command, if there is one. Otherwise
     * try the treeview's global "close" command.
     */
    cmd = CHOOSE(tvPtr->closeCmd, entryPtr->closeCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeViewPercentSubst(tvPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(tvPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    tvPtr->flags |= TV_LAYOUT;
    return TCL_OK;
}


int
Blt_TreeViewOpenEntry(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    char *cmd;

    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	return TCL_OK;		/* Entry is already open. */
    }
    entryPtr->flags &= ~ENTRY_CLOSED;
    /*
     * If there's a "open" command proc specified for the entry, use
     * that instead of the more general "open" proc for the entire 
     * treeview.
     */
    cmd = CHOOSE(tvPtr->openCmd, entryPtr->openCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeViewPercentSubst(tvPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(tvPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    tvPtr->flags |= TV_LAYOUT;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewCreateEntry --
 *
 *	This procedure is called by the Tree object when a node is 
 *	created and inserted into the tree.  It adds a new treeview 
 *	entry field to the node.
 *
 * Results:
 *	Returns the entry.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TreeViewCreateEntry(
    TreeView *tvPtr,
    Blt_TreeNode node,		/* Node that has just been created. */
    int objc,
    Tcl_Obj *CONST *objv,
    int flags)
{
    TreeViewEntry *entryPtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->entryTable, (char *)node, &isNew);
    if (isNew) {
	/* Create the entry structure */
	entryPtr = Blt_PoolAllocItem(tvPtr->entryPool, sizeof(TreeViewEntry));
	memset(entryPtr, 0, sizeof(TreeViewEntry));
	entryPtr->flags = tvPtr->buttonFlags | ENTRY_CLOSED;
	entryPtr->tvPtr = tvPtr;
	entryPtr->labelUid = NULL;
	entryPtr->node = node;
	Blt_SetHashValue(hPtr, entryPtr);

    } else {
	entryPtr = Blt_GetHashValue(hPtr);
    }
    if (Blt_TreeViewConfigureEntry(tvPtr, entryPtr, objc, objv, flags) 
	!= TCL_OK) {
	FreeEntry(tvPtr, entryPtr);
	return TCL_ERROR;	/* Error configuring the entry. */
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}


/*ARGSUSED*/
static int
CreateApplyProc(
    Blt_TreeNode node,		/* Node that has just been created. */
    ClientData clientData,
    int order)			/* Not used. */
{
    TreeView *tvPtr = clientData; 
    return Blt_TreeViewCreateEntry(tvPtr, node, 0, NULL, 0);
}

/*ARGSUSED*/
static int
DeleteApplyProc(
    Blt_TreeNode node,
    ClientData clientData,
    int order)			/* Not used. */
{
    TreeView *tvPtr = clientData;
    /* 
     * Unsetting the tree value triggers a call back to destroy the entry
     * and also releases the Tcl_Obj that contains it. 
     */
    return Blt_TreeUnsetValueByKey(tvPtr->interp, tvPtr->tree, node, 
	tvPtr->treeColumn.key);
}

static int
TreeEventProc(ClientData clientData, Blt_TreeNotifyEvent *eventPtr)
{
    Blt_TreeNode node;
    TreeView *tvPtr = clientData; 

    node = Blt_TreeGetNode(eventPtr->tree, eventPtr->inode);
    switch (eventPtr->type) {
    case TREE_NOTIFY_CREATE:
	return Blt_TreeViewCreateEntry(tvPtr, node, 0, NULL, 0);
    case TREE_NOTIFY_DELETE:
	/*  
	 * Deleting the tree node triggers a call back to free the
	 * treeview entry that is associated with it.
	 */
	if (node != NULL) {
	    FreeEntry(tvPtr, Blt_NodeToEntry(tvPtr, node));
	}
	break;
    case TREE_NOTIFY_RELABEL:
	if (node != NULL) {
	    TreeViewEntry *entryPtr;

	    entryPtr = Blt_NodeToEntry(tvPtr, node);
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	/*FALLTHRU*/
    case TREE_NOTIFY_MOVE:
    case TREE_NOTIFY_SORT:
	Blt_TreeViewEventuallyRedraw(tvPtr);
	tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
	break;
    default:
	/* empty */
	break;
    }	
    return TCL_OK;
}

TreeViewValue *
Blt_TreeViewFindValue(TreeViewEntry *entryPtr, TreeViewColumn *columnPtr)
{
    register TreeViewValue *valuePtr;

    for (valuePtr = entryPtr->values; valuePtr != NULL; 
	 valuePtr = valuePtr->nextPtr) {
	if (valuePtr->columnPtr == columnPtr) {
	    return valuePtr;
	}
    }
    return NULL;
}

void
Blt_TreeViewAddValue(TreeViewEntry *entryPtr, TreeViewColumn *columnPtr)
{
    if (Blt_TreeViewFindValue(entryPtr, columnPtr) == NULL) {
	Tcl_Obj *objPtr;

	if (Blt_TreeViewGetData(entryPtr, columnPtr->key, &objPtr) == TCL_OK) {
	    TreeViewValue *valuePtr;

	    /* Add a new value only if a data entry exists. */
	    valuePtr = Blt_PoolAllocItem(entryPtr->tvPtr->valuePool, 
			 sizeof(TreeViewValue));
	    valuePtr->columnPtr = columnPtr;
	    valuePtr->nextPtr = entryPtr->values;
	    valuePtr->textPtr = NULL;
	    valuePtr->width = valuePtr->height = 0;
	    valuePtr->stylePtr = NULL;
	    valuePtr->string = NULL;
	    entryPtr->values = valuePtr;
	}
    }
    entryPtr->tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    entryPtr->flags |= ENTRY_DIRTY;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTraceProc --
 *
 *	Mirrors the individual values of the tree object (they must
 *	also be listed in the widget's columns chain). This is because
 *	it must track and save the sizes of each individual data
 *	entry, rather than re-computing all the sizes each time the
 *	widget is redrawn.
 *
 *	This procedure is called by the Tree object when a node data
 *	value is set unset.
 *
 * Results:
 *	Returns TCL_OK.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TreeTraceProc(
    ClientData clientData,
    Tcl_Interp *interp,
    Blt_TreeNode node,		/* Node that has just been updated. */
    Blt_TreeKey key,		/* Key of value that's been updated. */
    unsigned int flags)
{
    Blt_HashEntry *hPtr;
    TreeView *tvPtr = clientData; 
    TreeViewColumn *columnPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr, *nextPtr, *lastPtr;
    
    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	return TCL_OK;		/* Not a node that we're interested in. */
    }
    entryPtr = Blt_GetHashValue(hPtr);
    flags &= TREE_TRACE_WRITE | TREE_TRACE_READ | TREE_TRACE_UNSET;
    switch (flags) {
    case TREE_TRACE_WRITE:
	hPtr = Blt_FindHashEntry(&tvPtr->columnTable, key);
	if (hPtr == NULL) {
	    return TCL_OK;	/* Data value isn't used by widget. */
	}
	columnPtr = Blt_GetHashValue(hPtr);
	if (columnPtr != &tvPtr->treeColumn) {
	    Blt_TreeViewAddValue(entryPtr, columnPtr);
	}
	entryPtr->flags |= ENTRY_DIRTY;
	Blt_TreeViewEventuallyRedraw(tvPtr);
	tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
	break;

    case TREE_TRACE_UNSET:
	lastPtr = NULL;
	for(valuePtr = entryPtr->values; valuePtr != NULL; 
	    valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    if (valuePtr->columnPtr->key == key) { 
		Blt_TreeViewDestroyValue(tvPtr, valuePtr);
		if (lastPtr == NULL) {
		    entryPtr->values = nextPtr;
		} else {
		    lastPtr->nextPtr = nextPtr;
		}
		entryPtr->flags |= ENTRY_DIRTY;
		Blt_TreeViewEventuallyRedraw(tvPtr);
		tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
		break;
	    }
	    lastPtr = valuePtr;
	}		
	break;

    default:
	break;
    }
    return TCL_OK;
}


static void
GetValueSize(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,
    TreeViewValue *valuePtr,
    TreeViewStyle *stylePtr)
{
    TreeViewColumn *columnPtr;

    columnPtr = valuePtr->columnPtr;
    valuePtr->width = valuePtr->height = 0;
    if (entryPtr->flags & ENTRY_DIRTY) { /* Reparse the data. */
	char *string;
	TreeViewIcon icon;

	Tcl_Obj *valueObjPtr;
	TreeViewStyle *newStylePtr;
    
	icon = NULL;
	newStylePtr = NULL;
	if (Blt_TreeViewGetData(entryPtr, valuePtr->columnPtr->key, 
		&valueObjPtr) != TCL_OK) {
	    return;			/* No data ??? */
	}
	string = Tcl_GetString(valueObjPtr);
	valuePtr->string = string;
	if (string[0] == '@') {	/* Name of style or Tk image. */
	    int objc;
	    Tcl_Obj **objv;
	    
	    if ((Tcl_ListObjGetElements(tvPtr->interp, valueObjPtr, &objc, 
		&objv) != TCL_OK) || (objc < 1) || (objc > 2)) {
		goto handleString;
	    }
	    if (objc > 0) {
		char *name;
		
		name = Tcl_GetString(objv[0]) + 1;
		if (Blt_TreeViewGetStyle((Tcl_Interp *)NULL, tvPtr, name, 
					 &newStylePtr) != TCL_OK) {
		    icon = Blt_TreeViewGetIcon(tvPtr, name);
		    if (icon == NULL) {
			goto handleString;
		    }
		    /* Create a new style by the name of the image. */
		    newStylePtr = Blt_TreeViewCreateStyle((Tcl_Interp *)NULL, 
			tvPtr, STYLE_TEXTBOX, name);
		    assert(newStylePtr);
		    Blt_TreeViewUpdateStyleGCs(tvPtr, newStylePtr);
		}
		
	    }
	    if (valuePtr->stylePtr != NULL) {
		Blt_TreeViewFreeStyle(tvPtr, valuePtr->stylePtr);
	    }
	    if (icon != NULL) {
		Blt_TreeViewSetStyleIcon(tvPtr, newStylePtr, icon);
	    }
	    valuePtr->stylePtr = newStylePtr;
	    valuePtr->string = (objc > 1) ? Tcl_GetString(objv[1]) : NULL;
	}
    }
 handleString:
    stylePtr = CHOOSE(columnPtr->stylePtr, valuePtr->stylePtr);
    /* Measure the text string. */
    (*stylePtr->classPtr->measProc)(tvPtr, stylePtr, valuePtr);
}

static void
GetRowExtents(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,
    int *widthPtr, 
    int *heightPtr)
{
    TreeViewValue *valuePtr;
    int valueWidth;		/* Width of individual value.  */
    int width, height;		/* Compute dimensions of row. */
    TreeViewStyle *stylePtr;

    width = height = 0;
    for (valuePtr = entryPtr->values; valuePtr != NULL; 
	valuePtr = valuePtr->nextPtr) {
	stylePtr = valuePtr->stylePtr;
	if (stylePtr == NULL) {
	    stylePtr = valuePtr->columnPtr->stylePtr;
	}
	if ((entryPtr->flags & ENTRY_DIRTY) || 
	    (stylePtr->flags & STYLE_DIRTY)) {
	    GetValueSize(tvPtr, entryPtr, valuePtr, stylePtr);
	}
	if (valuePtr->height > height) {
	    height = valuePtr->height;
	}
	valueWidth = valuePtr->width;
	width += valueWidth;
    }	    
    *widthPtr = width;
    *heightPtr = height;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewNearestEntry --
 *
 *	Finds the entry closest to the given screen X-Y coordinates
 *	in the viewport.
 *
 * Results:
 *	Returns the pointer to the closest node.  If no node is
 *	visible (nodes may be hidden), NULL is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
TreeViewEntry *
Blt_TreeViewNearestEntry(TreeView *tvPtr, int x, int y, int selectOne)
{
    TreeViewEntry *lastPtr, *entryPtr;
    register TreeViewEntry **p;

    /*
     * We implicitly can pick only visible entries.  So make sure that
     * the tree exists.
     */
    if (tvPtr->nVisible == 0) {
	return NULL;
    }
    if (y < tvPtr->titleHeight) {
	return (selectOne) ? tvPtr->visibleArr[0] : NULL;
    }
    /*
     * Since the entry positions were previously computed in world
     * coordinates, convert Y-coordinate from screen to world
     * coordinates too.
     */
    y = WORLDY(tvPtr, y);
    lastPtr = tvPtr->visibleArr[0];
    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	entryPtr = *p;
	/*
	 * If the start of the next entry starts beyond the point,
	 * use the last entry.
	 */
	if (entryPtr->worldY > y) {
	    return (selectOne) ? entryPtr : NULL;
	}
	if (y < (entryPtr->worldY + entryPtr->height)) {
	    return entryPtr;	/* Found it. */
	}
	lastPtr = entryPtr;
    }
    return (selectOne) ? lastPtr : NULL;
}


ClientData
Blt_TreeViewEntryTag(TreeView *tvPtr, CONST char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;			/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->entryTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->entryTagTable, hPtr);
}

ClientData
Blt_TreeViewButtonTag(TreeView *tvPtr, CONST char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;			/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->buttonTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->buttonTagTable, hPtr);
}

ClientData
Blt_TreeViewColumnTag(TreeView *tvPtr, CONST char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;			/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->columnTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->columnTagTable, hPtr);
}

ClientData
Blt_TreeViewStyleTag(TreeView *tvPtr, CONST char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;			/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->styleTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->styleTagTable, hPtr);
}

static void
GetTags(
    Blt_BindTable table,
    ClientData object,		/* Object picked. */
    ClientData context,		/* Context of object. */
    Blt_List ids)		/* (out) List of binding ids to be
				 * applied for this object. */
{
    TreeView *tvPtr;
    int nNames;
    char **names;
    register char **p;

    tvPtr = Blt_GetBindingData(table);
    if (context == (ClientData)ITEM_ENTRY_BUTTON) {
	TreeViewEntry *entryPtr = object;

	Blt_ListAppend(ids, Blt_TreeViewButtonTag(tvPtr, "Button"), 0);
	if (entryPtr->tagsUid != NULL) {
	    if (Tcl_SplitList((Tcl_Interp *)NULL, entryPtr->tagsUid, &nNames,
			      &names) == TCL_OK) {
		for (p = names; *p != NULL; p++) {
		    Blt_ListAppend(ids, Blt_TreeViewButtonTag(tvPtr, *p), 0);
		}
		Blt_Free(names);
	    }
	} else {
	    Blt_ListAppend(ids, Blt_TreeViewButtonTag(tvPtr, "Entry"), 0);
	    Blt_ListAppend(ids, Blt_TreeViewButtonTag(tvPtr, "all"), 0);
	}
    } else if (context == (ClientData)ITEM_COLUMN_TITLE) {
	TreeViewColumn *columnPtr = object;

	Blt_ListAppend(ids, (char *)columnPtr, 0);
	if (columnPtr->tagsUid != NULL) {
	    if (Tcl_SplitList((Tcl_Interp *)NULL, columnPtr->tagsUid, &nNames,
		      &names) == TCL_OK) {
		for (p = names; *p != NULL; p++) {
		    Blt_ListAppend(ids, Blt_TreeViewColumnTag(tvPtr, *p), 0);
		}
		Blt_Free(names);
	    }
	}
    } else if (context == ITEM_COLUMN_RULE) {
	Blt_ListAppend(ids, Blt_TreeViewColumnTag(tvPtr, "Rule"), 0);
    } else {
	TreeViewEntry *entryPtr = object;

	Blt_ListAppend(ids, (char *)entryPtr, 0);
	if (entryPtr->tagsUid != NULL) {
	    if (Tcl_SplitList((Tcl_Interp *)NULL, entryPtr->tagsUid, &nNames,
		      &names) == TCL_OK) {
		for (p = names; *p != NULL; p++) {
		    Blt_ListAppend(ids, Blt_TreeViewEntryTag(tvPtr, *p), 0);
		}
		Blt_Free(names);
	    }
	} else if (context == ITEM_ENTRY){
	    Blt_ListAppend(ids, Blt_TreeViewEntryTag(tvPtr, "Entry"), 0);
	    Blt_ListAppend(ids, Blt_TreeViewEntryTag(tvPtr, "all"), 0);
	} else {
	    TreeViewValue *valuePtr = context;

	    if (valuePtr != NULL) {
		TreeViewStyle *stylePtr = valuePtr->stylePtr;

		if (stylePtr == NULL) {
		    stylePtr = valuePtr->columnPtr->stylePtr;
		}
		Blt_ListAppend(ids, 
	            Blt_TreeViewEntryTag(tvPtr, stylePtr->name), 0);
		Blt_ListAppend(ids, 
		    Blt_TreeViewEntryTag(tvPtr, valuePtr->columnPtr->key), 0);
		Blt_ListAppend(ids, 
		    Blt_TreeViewEntryTag(tvPtr, stylePtr->classPtr->className),
		    0);
#ifndef notdef
		Blt_ListAppend(ids, Blt_TreeViewEntryTag(tvPtr, "Entry"), 0);
		Blt_ListAppend(ids, Blt_TreeViewEntryTag(tvPtr, "all"), 0);
#endif
	    }
	}
    }
}

/*ARGSUSED*/
static ClientData
PickItem(
    ClientData clientData,
    int x, 
    int y,			/* Screen coordinates of the test point. */
    ClientData *contextPtr)	/* (out) Context of item selected: should
				 * be ITEM_ENTRY, ITEM_ENTRY_BUTTON,
				 * ITEM_COLUMN_TITLE,
				 * ITEM_COLUMN_RULE, or
				 * ITEM_STYLE. */
{
    TreeView *tvPtr = clientData;
    TreeViewEntry *entryPtr;
    TreeViewColumn *columnPtr;

    if (contextPtr != NULL) {
	*contextPtr = NULL;
    }
    if (tvPtr->flags & TV_DIRTY) {
	/* Can't trust the selected entry if nodes have been added or
	 * deleted. So recompute the layout. */
	if (tvPtr->flags & TV_LAYOUT) {
	    Blt_TreeViewComputeLayout(tvPtr);
	} 
	ComputeVisibleEntries(tvPtr);
    }
    columnPtr = Blt_TreeViewNearestColumn(tvPtr, x, y, contextPtr);
    if ((*contextPtr != NULL) && (tvPtr->flags & TV_SHOW_COLUMN_TITLES)) {
	return columnPtr;
    }
    if (tvPtr->nVisible == 0) {
	return NULL;
    }
    entryPtr = Blt_TreeViewNearestEntry(tvPtr, x, y, FALSE);
    if (entryPtr == NULL) {
	return NULL;
    }
    x = WORLDX(tvPtr, x);
    y = WORLDY(tvPtr, y);
    if (contextPtr != NULL) {
	*contextPtr = ITEM_ENTRY;
	if (columnPtr != NULL) {
	    TreeViewValue *valuePtr;

	    valuePtr = Blt_TreeViewFindValue(entryPtr, columnPtr);
	    if (valuePtr != NULL) {
		TreeViewStyle *stylePtr;
		
		stylePtr = valuePtr->stylePtr;
		if (stylePtr == NULL) {
		    stylePtr = valuePtr->columnPtr->stylePtr;
		}
		if ((stylePtr->classPtr->pickProc == NULL) ||
		    ((*stylePtr->classPtr->pickProc)(entryPtr, valuePtr, 
			stylePtr, x, y))) {
		    *contextPtr = valuePtr;
		} 
	    }
	}
	if (entryPtr->flags & ENTRY_HAS_BUTTON) {
	    TreeViewButton *buttonPtr = &tvPtr->button;
	    int left, right, top, bottom;
	    
	    left = entryPtr->worldX + entryPtr->buttonX - BUTTON_PAD;
	    right = left + buttonPtr->width + 2 * BUTTON_PAD;
	    top = entryPtr->worldY + entryPtr->buttonY - BUTTON_PAD;
	    bottom = top + buttonPtr->height + 2 * BUTTON_PAD;
	    if ((x >= left) && (x < right) && (y >= top) && (y < bottom)) {
		*contextPtr = (ClientData)ITEM_ENTRY_BUTTON;
	    }
	}
    }
    return entryPtr;
}

static void
GetEntryExtents(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    Tk_Font font;
    TreeViewIcon *icons;
    char *label;
    int entryWidth, entryHeight;
    int width, height;

    /*
     * FIXME: Use of DIRTY flag inconsistent.  When does it
     *	      mean "dirty entry"? When does it mean "dirty column"?
     *	      Does it matter? probably
     */
    if ((entryPtr->flags & ENTRY_DIRTY) || (tvPtr->flags & TV_UPDATE)) {
	Tk_FontMetrics fontMetrics;

	entryPtr->iconWidth = entryPtr->iconHeight = 0;
	icons = CHOOSE(tvPtr->icons, entryPtr->icons);
	if (icons != NULL) {
	    register int i;
	    
	    for (i = 0; i < 2; i++) {
		if (icons[i] == NULL) {
		    break;
		}
		if (entryPtr->iconWidth < TreeViewIconWidth(icons[i])) {
		    entryPtr->iconWidth = TreeViewIconWidth(icons[i]);
		}
		if (entryPtr->iconHeight < TreeViewIconHeight(icons[i])) {
		    entryPtr->iconHeight = TreeViewIconHeight(icons[i]);
		}
	    }
	}
	if ((icons == NULL) || (icons[0] == NULL)) {
	    entryPtr->iconWidth = DEF_ICON_WIDTH;
	    entryPtr->iconHeight = DEF_ICON_HEIGHT;
	}
	entryPtr->iconWidth += 2 * ICON_PADX;
	entryPtr->iconHeight += 2 * ICON_PADY;
	entryHeight = MAX(entryPtr->iconHeight, tvPtr->button.height);
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeViewGetStyleFont(tvPtr, tvPtr->treeColumn.stylePtr);
	}
	if (entryPtr->fullName != NULL) {
	    Blt_Free(entryPtr->fullName);
	    entryPtr->fullName = NULL;
	}
	if (entryPtr->textPtr != NULL) {
	    Blt_Free(entryPtr->textPtr);
	    entryPtr->textPtr = NULL;
	}
	
	Tk_GetFontMetrics(font, &fontMetrics);
	entryPtr->lineHeight = fontMetrics.linespace;
	entryPtr->lineHeight += 2 * (FOCUS_WIDTH + LABEL_PADY + 
				    tvPtr->selBorderWidth) + tvPtr->leader;
	label = GETLABEL(entryPtr);
	if (label[0] == '\0') {
	    width = height = entryPtr->lineHeight;
	} else {
	    TextStyle ts;

	    Blt_InitTextStyle(&ts);
	    ts.shadow.offset = entryPtr->shadow.offset;
	    ts.font = font;
	    
	    if (tvPtr->flatView) {
		Tcl_DString dString;

		Blt_TreeViewGetFullName(tvPtr, entryPtr, TRUE, &dString);
		entryPtr->fullName = Blt_Strdup(Tcl_DStringValue(&dString));
		Tcl_DStringFree(&dString);
		entryPtr->textPtr = Blt_GetTextLayout(entryPtr->fullName, &ts);
	    } else {
		entryPtr->textPtr = Blt_GetTextLayout(label, &ts);
	    }
	    width = entryPtr->textPtr->width;
	    height = entryPtr->textPtr->height;
	}
	width += 2 * (FOCUS_WIDTH + LABEL_PADX + tvPtr->selBorderWidth);
	height += 2 * (FOCUS_WIDTH + LABEL_PADY + tvPtr->selBorderWidth);
	width = ODD(width);
	if (entryPtr->reqHeight > height) {
	    height = entryPtr->reqHeight;
	} 
	height = ODD(height);
	entryWidth = width;
	if (entryHeight < height) {
	    entryHeight = height;
	}
	entryPtr->labelWidth = width;
	entryPtr->labelHeight = height;
    } else {
	entryHeight = entryPtr->labelHeight;
	entryWidth = entryPtr->labelWidth;
    }
    /*  
     * Find the maximum height of the data value entries. This also has
     * the side effect of contributing the maximum width of the column. 
     */
    GetRowExtents(tvPtr, entryPtr, &width, &height);
    if (entryHeight < height) {
	entryHeight = height;
    }
    entryPtr->width = entryWidth + COLUMN_PAD;
    entryPtr->height = entryHeight + tvPtr->leader;
    /*
     * Force the height of the entry to an even number. This is to
     * make the dots or the vertical line segments coincide with the
     * start of the horizontal lines.
     */
    if (entryPtr->height & 0x01) {
	entryPtr->height++;
    }
    entryPtr->flags &= ~ENTRY_DIRTY;
}

/*
 * TreeView Procedures
 */

/*
 * ----------------------------------------------------------------------
 *
 * CreateTreeView --
 *
 * ----------------------------------------------------------------------
 */
static TreeView *
CreateTreeView(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,		/* Name of the new widget. */
    CONST char *className)
{
    Tk_Window tkwin;
    TreeView *tvPtr;
    char *name;
    Tcl_DString dString;
    int result;

    name = Tcl_GetString(objPtr);
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), name,
	(char *)NULL);
    if (tkwin == NULL) {
	return NULL;
    }
    Tk_SetClass(tkwin, (char *)className);

    tvPtr = Blt_Calloc(1, sizeof(TreeView));
    assert(tvPtr);
    tvPtr->tkwin = tkwin;
    tvPtr->display = Tk_Display(tkwin);
    tvPtr->interp = interp;
    tvPtr->flags = (TV_HIDE_ROOT | TV_SHOW_COLUMN_TITLES | 
		    TV_DIRTY | TV_LAYOUT | TV_RESORT);
    tvPtr->leader = 0;
    tvPtr->dashes = 1;
    tvPtr->highlightWidth = 2;
    tvPtr->selBorderWidth = 1;
    tvPtr->borderWidth = 2;
    tvPtr->relief = TK_RELIEF_SUNKEN;
    tvPtr->selRelief = TK_RELIEF_FLAT;
    tvPtr->scrollMode = BLT_SCROLL_MODE_HIERBOX;
    tvPtr->selectMode = SELECT_MODE_SINGLE;
    tvPtr->button.closeRelief = tvPtr->button.openRelief = TK_RELIEF_SOLID;
    tvPtr->reqWidth = 200;
    tvPtr->reqHeight = 400;
    tvPtr->xScrollUnits = tvPtr->yScrollUnits = 20;
    tvPtr->lineWidth = 1;
    tvPtr->button.borderWidth = 1;
    tvPtr->colChainPtr = Blt_ChainCreate();
    tvPtr->buttonFlags = BUTTON_AUTO;
    tvPtr->selChainPtr = Blt_ChainCreate();
    Blt_InitHashTableWithPool(&tvPtr->entryTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->columnTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->iconTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->selectTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->uidTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->styleTable, BLT_STRING_KEYS);
    tvPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, tvPtr, PickItem, 
	GetTags);
    Blt_InitHashTable(&tvPtr->entryTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->columnTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->buttonTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->styleTagTable, BLT_STRING_KEYS);

    tvPtr->entryPool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    tvPtr->valuePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, tvPtr);
#endif
    tvPtr->cmdToken = Tcl_CreateObjCommand(interp, Tk_PathName(tvPtr->tkwin), 
	Blt_TreeViewWidgetInstCmd, tvPtr, WidgetInstCmdDeleteProc);

#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(tvPtr->tkwin, tvPtr->cmdToken);
#endif
    Tk_CreateSelHandler(tvPtr->tkwin, XA_PRIMARY, XA_STRING, SelectionProc,
	tvPtr, XA_STRING);
    Tk_CreateEventHandler(tvPtr->tkwin, ExposureMask | StructureNotifyMask |
	FocusChangeMask, TreeViewEventProc, tvPtr);
    /* 
     * Create a default style. This must exist before we can create
     * the treeview column. 
     */  
    tvPtr->stylePtr = Blt_TreeViewCreateStyle(interp, tvPtr, STYLE_TEXTBOX,
	"text");
    if (tvPtr->stylePtr == NULL) {
	return NULL;
    }
    /* Create a default column to display the view of the tree. */
    Tcl_DStringInit(&dString);
    Tcl_DStringAppend(&dString, "BLT TreeView ", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(tvPtr->tkwin), -1);
    result = Blt_TreeViewCreateColumn(tvPtr, &tvPtr->treeColumn, 
				      Tcl_DStringValue(&dString), "");
    Tcl_DStringFree(&dString);
    if (result != TCL_OK) {
	return NULL;
    }
    Blt_ChainAppend(tvPtr->colChainPtr, &tvPtr->treeColumn);
    return tvPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyTreeView --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a TreeView at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyTreeView(DestroyData dataPtr)	/* Pointer to the widget record. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    TreeView *tvPtr = (TreeView *)dataPtr;
    TreeViewButton *buttonPtr;
    TreeViewEntry *entryPtr;
    TreeViewStyle *stylePtr;

    Blt_TreeDeleteEventHandler(tvPtr->tree, TREE_NOTIFY_ALL, TreeEventProc, 
	   tvPtr);
    for (hPtr = Blt_FirstHashEntry(&tvPtr->entryTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	entryPtr = Blt_GetHashValue(hPtr);
	DestroyEntry((ClientData)entryPtr);
    }
    bltTreeViewTreeOption.clientData = tvPtr;
    bltTreeViewIconsOption.clientData = tvPtr;
    Blt_FreeObjOptions(bltTreeViewSpecs, (char *)tvPtr, tvPtr->display, 0);
    if (tvPtr->tkwin != NULL) {
	Tk_DeleteSelHandler(tvPtr->tkwin, XA_PRIMARY, XA_STRING);
    }
    if (tvPtr->lineGC != NULL) {
	Tk_FreeGC(tvPtr->display, tvPtr->lineGC);
    }
    if (tvPtr->focusGC != NULL) {
	Blt_FreePrivateGC(tvPtr->display, tvPtr->focusGC);
    }
    if (tvPtr->visibleArr != NULL) {
	Blt_Free(tvPtr->visibleArr);
    }
    if (tvPtr->flatArr != NULL) {
	Blt_Free(tvPtr->flatArr);
    }
    if (tvPtr->levelInfo != NULL) {
	Blt_Free(tvPtr->levelInfo);
    }
    buttonPtr = &tvPtr->button;
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->activeGC);
    }
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->normalGC);
    }
    if (tvPtr->stylePtr != NULL) {
	Blt_TreeViewFreeStyle(tvPtr, tvPtr->stylePtr);
    }

    Blt_TreeViewDestroyColumns(tvPtr);
    Blt_DestroyBindingTable(tvPtr->bindTable);
    Blt_ChainDestroy(tvPtr->selChainPtr);
    Blt_DeleteHashTable(&tvPtr->entryTagTable);
    Blt_DeleteHashTable(&tvPtr->columnTagTable);
    Blt_DeleteHashTable(&tvPtr->buttonTagTable);
    Blt_DeleteHashTable(&tvPtr->styleTagTable);

    for (hPtr = Blt_FirstHashEntry(&tvPtr->styleTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	stylePtr = Blt_GetHashValue(hPtr);
	/* stylePtr->refCount = 0; */
	stylePtr->flags &= ~STYLE_USER;
	Blt_TreeViewFreeStyle(tvPtr, stylePtr);
    }
    if (tvPtr->comboWin != NULL) {
	Tk_DestroyWindow(tvPtr->comboWin);
    }
    Blt_DeleteHashTable(&tvPtr->styleTable);
    Blt_DeleteHashTable(&tvPtr->selectTable);
    Blt_DeleteHashTable(&tvPtr->uidTable);
    Blt_DeleteHashTable(&tvPtr->entryTable);

    Blt_PoolDestroy(tvPtr->entryPool);
    Blt_PoolDestroy(tvPtr->valuePool);
    DumpIconTable(tvPtr);
    Blt_Free(tvPtr);
}

/*
 * --------------------------------------------------------------
 *
 * TreeViewEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on treeview widgets.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
TreeViewEventProc(
    ClientData clientData,	/* Information about window. */
    XEvent *eventPtr)		/* Information about event. */
{
    TreeView *tvPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    Blt_TreeViewEventuallyRedraw(tvPtr);
	    Blt_PickCurrentItem(tvPtr->bindTable);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
	Blt_TreeViewEventuallyRedraw(tvPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		tvPtr->flags |= TV_FOCUS;
	    } else {
		tvPtr->flags &= ~TV_FOCUS;
	    }
	    Blt_TreeViewEventuallyRedraw(tvPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (tvPtr->tkwin != NULL) {
	    tvPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(tvPtr->interp, tvPtr->cmdToken);
	}
	if (tvPtr->flags & TV_REDRAW) {
	    Tcl_CancelIdleCall(DisplayTreeView, tvPtr);
	}
	if (tvPtr->flags & TV_SELECT_PENDING) {
	    Tcl_CancelIdleCall(Blt_TreeViewSelectCmdProc, tvPtr);
	}
	Tcl_EventuallyFree(tvPtr, DestroyTreeView);
    }
}

/* Selection Procedures */
/*
 *----------------------------------------------------------------------
 *
 * SelectionProc --
 *
 *	This procedure is called back by Tk when the selection is
 *	requested by someone.  It returns part or all of the selection
 *	in a buffer provided by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored at
 *	buffer.  Buffer is filled (or partially filled) with a
 *	NUL-terminated string containing part or all of the
 *	selection, as given by offset and maxBytes.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
SelectionProc(
    ClientData clientData,	/* Information about the widget. */
    int offset,			/* Offset within selection of first
				 * character to be returned. */
    char *buffer,		/* Location in which to place
				 * selection. */
    int maxBytes)		/* Maximum number of bytes to place
				 * at buffer, not including terminating
				 * NULL character. */
{
    Tcl_DString dString;
    TreeView *tvPtr = clientData;
    TreeViewEntry *entryPtr;
    int size;

    if ((tvPtr->flags & TV_SELECT_EXPORT) == 0) {
	return -1;
    }
    /*
     * Retrieve the names of the selected entries.
     */
    Tcl_DStringInit(&dString);
    if (tvPtr->flags & TV_SELECT_SORTED) {
	Blt_ChainLink *linkPtr;

	for (linkPtr = Blt_ChainFirstLink(tvPtr->selChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    Tcl_DStringAppend(&dString, GETLABEL(entryPtr), -1);
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    } else {
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
	    if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
		Tcl_DStringAppend(&dString, GETLABEL(entryPtr), -1);
		Tcl_DStringAppend(&dString, "\n", -1);
	    }
	}
    }
    size = Tcl_DStringLength(&dString) - offset;
    strncpy(buffer, Tcl_DStringValue(&dString) + offset, maxBytes);
    Tcl_DStringFree(&dString);
    buffer[maxBytes] = '\0';
    return (size > maxBytes) ? maxBytes : size;
}

/*
 *----------------------------------------------------------------------
 *
 * WidgetInstCmdDeleteProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */
static void
WidgetInstCmdDeleteProc(ClientData clientData)
{
    TreeView *tvPtr = clientData;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this
     * procedure destroys the widget.
     */
    if (tvPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = tvPtr->tkwin;
	tvPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef ITCL_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* ITCL_NAMESPACES */
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_TreeViewUpdateWidget --
 *
 *	Updates the GCs and other information associated with the
 *	treeview widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for tvPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_TreeViewUpdateWidget(Tcl_Interp *interp, TreeView *tvPtr)	
{
    GC newGC;
    XGCValues gcValues;
    int setupTree;
    unsigned long gcMask;

    /*
     * GC for dotted vertical line.
     */
    gcMask = (GCForeground | GCLineWidth);
    gcValues.foreground = tvPtr->lineColor->pixel;
    gcValues.line_width = tvPtr->lineWidth;
    if (tvPtr->dashes > 0) {
	gcMask |= (GCLineStyle | GCDashList);
	gcValues.line_style = LineOnOffDash;
	gcValues.dashes = tvPtr->dashes;
    }
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (tvPtr->lineGC != NULL) {
	Tk_FreeGC(tvPtr->display, tvPtr->lineGC);
    }
    tvPtr->lineGC = newGC;

    /*
     * GC for active label. Dashed outline.
     */
    gcMask = GCForeground | GCLineStyle;
    gcValues.foreground = tvPtr->focusColor->pixel;
    gcValues.line_style = (LineIsDashed(tvPtr->focusDashes))
	? LineOnOffDash : LineSolid;
    newGC = Blt_GetPrivateGC(tvPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(tvPtr->focusDashes)) {
	tvPtr->focusDashes.offset = 2;
	Blt_SetDashes(tvPtr->display, newGC, &tvPtr->focusDashes);
    }
    if (tvPtr->focusGC != NULL) {
	Blt_FreePrivateGC(tvPtr->display, tvPtr->focusGC);
    }
    tvPtr->focusGC = newGC;

    Blt_TreeViewConfigureButtons(tvPtr);
    tvPtr->inset = tvPtr->highlightWidth + tvPtr->borderWidth + INSET_PAD;

    setupTree = FALSE;

    /*
     * If no tree object was named, allocate a new one.  The name will
     * be the same as the widget pathname.
     */
    if (tvPtr->tree == NULL) {
	Blt_Tree token;
	char *string;

	string = Tk_PathName(tvPtr->tkwin);
	if (Blt_TreeCreate(interp, string, &token) != TCL_OK) {
	    return TCL_ERROR;
	}
	tvPtr->tree = token;
	setupTree = TRUE;
    } 

    /*
     * If the tree object was changed, we need to setup the new one.
     */
    if (Blt_ObjConfigModified(bltTreeViewSpecs, "-tree", (char *)NULL)) {
	setupTree = TRUE;
    }

    /*
     * These options change the layout of the box.  Mark the widget for update.
     */
    if (Blt_ObjConfigModified(bltTreeViewSpecs, "-font", 
	"-linespacing", "-*width", "-height", "-hide*", "-tree", "-flat", 
	(char *)NULL)) {
	tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
    }
    /*
     * If the tree view was changed, mark all the nodes dirty (we'll
     * be switching back to either the full path name or the label)
     * and free the array representing the flattened view of the tree.
     */
    if (Blt_ObjConfigModified(bltTreeViewSpecs, "-hideleaves", "-flat",
			      (char *)NULL)) {
	TreeViewEntry *entryPtr;
	
	tvPtr->flags |= (TV_DIRTY | TV_RESORT);
	/* Mark all entries dirty. */
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, 0)) {
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	if ((!tvPtr->flatView) && (tvPtr->flatArr != NULL)) {
	    Blt_Free(tvPtr->flatArr);
	    tvPtr->flatArr = NULL;
	}
    }
    if ((tvPtr->reqHeight != Tk_ReqHeight(tvPtr->tkwin)) ||
	(tvPtr->reqWidth != Tk_ReqWidth(tvPtr->tkwin))) {
	Tk_GeometryRequest(tvPtr->tkwin, tvPtr->reqWidth, tvPtr->reqHeight);
    }

    if (setupTree) {
	Blt_TreeNode root;

	Blt_TreeCreateEventHandler(tvPtr->tree, TREE_NOTIFY_ALL, TreeEventProc, 
		   tvPtr);
	TraceColumns(tvPtr);
	root = Blt_TreeRootNode(tvPtr->tree);

	/* Automatically add view-entry values to the new tree. */
	Blt_TreeApply(root, CreateApplyProc, tvPtr);
	tvPtr->focusPtr = tvPtr->rootPtr = Blt_NodeToEntry(tvPtr, root);
	tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
	Blt_SetFocusItem(tvPtr->bindTable, tvPtr->rootPtr, ITEM_ENTRY);

	/* Automatically open the root node. */
	if (Blt_TreeViewOpenEntry(tvPtr, tvPtr->rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!(tvPtr->flags & TV_NEW_TAGS)) {
	    Blt_Tree tree;

	    if (Blt_TreeCmdGetToken(interp, Blt_TreeName(tvPtr->tree), 
			&tree) == TCL_OK) {
		Blt_TreeShareTagTable(tree, tvPtr->tree);
	    }
	}
    }

    if (Blt_ObjConfigModified(bltTreeViewSpecs, "-font", "-color", 
	(char *)NULL)) {
	Blt_TreeViewUpdateColumnGCs(tvPtr, &tvPtr->treeColumn);
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ResetCoordinates --
 *
 *	Determines the maximum height of all visible entries.
 *
 *	1. Sets the worldY coordinate for all mapped/open entries.
 *	2. Determines if entry needs a button.
 *	3. Collects the minimum height of open/mapped entries. (Do for all
 *	   entries upon insert).
 *	4. Figures out horizontal extent of each entry (will be width of 
 *	   tree view column).
 *	5. Collects maximum icon size for each level.
 *	6. The height of its vertical line
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 * ----------------------------------------------------------------------
 */
static void
ResetCoordinates(
    TreeView *tvPtr,
    TreeViewEntry *entryPtr,
    int *yPtr)
{
    int depth;

    entryPtr->worldY = -1;
    entryPtr->vertLineLength = -1;
    if ((entryPtr != tvPtr->rootPtr) && 
	(Blt_TreeViewEntryIsHidden(entryPtr))) {
	return;     /* If the entry is hidden, then do nothing. */
    }
    entryPtr->worldY = *yPtr;
    entryPtr->vertLineLength = -(*yPtr);
    *yPtr += entryPtr->height;

    depth = DEPTH(tvPtr, entryPtr->node) + 1;
    if (tvPtr->levelInfo[depth].labelWidth < entryPtr->labelWidth) {
	tvPtr->levelInfo[depth].labelWidth = entryPtr->labelWidth;
    }
    if (tvPtr->levelInfo[depth].iconWidth < entryPtr->iconWidth) {
	tvPtr->levelInfo[depth].iconWidth = entryPtr->iconWidth;
    }
    tvPtr->levelInfo[depth].iconWidth |= 0x01;

    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	TreeViewEntry *bottomPtr, *childPtr;

	bottomPtr = entryPtr;
	for (childPtr = Blt_TreeViewFirstChild(entryPtr, ENTRY_HIDDEN); 
	     childPtr != NULL; 
	     childPtr = Blt_TreeViewNextSibling(childPtr, ENTRY_HIDDEN)){
	    ResetCoordinates(tvPtr, childPtr, yPtr);
	    bottomPtr = childPtr;
	}
	entryPtr->vertLineLength += bottomPtr->worldY;
    }
}

static void
AdjustColumns(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    double weight;
    int nOpen;
    int size, avail, ration, growth;

    growth = VPORTWIDTH(tvPtr) - tvPtr->worldWidth;
    nOpen = 0;
    weight = 0.0;
    /* Find out how many columns still have space available */
    for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	if ((columnPtr->hidden) || 
	    (columnPtr->weight == 0.0) || 
	    (columnPtr->width >= columnPtr->max) || 
	    (columnPtr->reqWidth > 0)) {
	    continue;
	}
	nOpen++;
	weight += columnPtr->weight;
    }

    while ((nOpen > 0) && (weight > 0.0) && (growth > 0)) {
	ration = (int)(growth / weight);
	if (ration == 0) {
	    ration = 1;
	}
	for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    columnPtr = Blt_ChainGetValue(linkPtr);
	    if ((columnPtr->hidden) || 
		(columnPtr->weight == 0.0) ||
		(columnPtr->width >= columnPtr->max) || 
		(columnPtr->reqWidth > 0)) {
		continue;
	    }
	    size = (int)(ration * columnPtr->weight);
	    if (size > growth) {
		size = growth; 
	    }
	    avail = columnPtr->max - columnPtr->width;
	    if (size > avail) {
		size = avail;
		nOpen--;
		weight -= columnPtr->weight;
	    }
	    growth -= size;
	    columnPtr->width += size;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeFlatLayout --
 *
 *	Recompute the layout when entries are opened/closed,
 *	inserted/deleted, or when text attributes change (such as
 *	font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeFlatLayout(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    TreeViewEntry **p;
    TreeViewEntry *entryPtr;
    int count;
    int maxX;
    int y;

    /* 
     * Pass 1:	Reinitialize column sizes and loop through all nodes. 
     *
     *		1. Recalculate the size of each entry as needed. 
     *		2. The maximum depth of the tree. 
     *		3. Minimum height of an entry.  Dividing this by the
     *		   height of the widget gives a rough estimate of the 
     *		   maximum number of visible entries.
     *		4. Build an array to hold level information to be filled
     *		   in on pass 2.
     */
    if (tvPtr->flags & (TV_DIRTY | TV_UPDATE)) {
	int position;

	/* Reset the positions of all the columns and initialize the
	 * column used to track the widest value. */
	position = 1;
	for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    columnPtr = Blt_ChainGetValue(linkPtr);
	    columnPtr->maxWidth = 0;
	    columnPtr->max = SHRT_MAX;
	    if (columnPtr->reqMax > 0) {
		columnPtr->max = columnPtr->reqMax;
	    }
	    columnPtr->position = position;
	    position++;
	}

	/* If the view needs to be resorted, free the old view. */
	if ((tvPtr->flags & TV_RESORT) && (tvPtr->flatArr != NULL)) {
	    Blt_Free(tvPtr->flatArr);
	    tvPtr->flatArr = NULL;
	}

	/* Recreate the flat view of all the open and not-hidden entries. */
	if (tvPtr->flatArr == NULL) {
	    count = 0;
	    /* Count the number of open entries to allocate for the array. */
	    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
		if ((tvPtr->flags & TV_HIDE_ROOT) && 
		    (entryPtr == tvPtr->rootPtr)) {
		    continue;
		}
		count++;
	    }
	    tvPtr->nEntries = count;

	    /* Allocate an array for the flat view. */
	    tvPtr->flatArr = Blt_Calloc((count + 1), sizeof(TreeViewEntry *));
	    assert(tvPtr->flatArr);

	    /* Fill the array with open and not-hidden entries */
	    p = tvPtr->flatArr;
	    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
		if ((tvPtr->flags & TV_HIDE_ROOT) && 
		    (entryPtr == tvPtr->rootPtr)) {
		    continue;
		}
		*p++ = entryPtr;
	    }
	    *p = NULL;
	    tvPtr->flags &= ~TV_SORTED;	/* Indicate the view isn't sorted. */
	}

	/* Collect the extents of the entries in the flat view. */
	tvPtr->depth = 0;
	tvPtr->minHeight = SHRT_MAX;
	for (p = tvPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    GetEntryExtents(tvPtr, entryPtr);
	    if (tvPtr->minHeight > entryPtr->height) {
		tvPtr->minHeight = entryPtr->height;
	    }
	    entryPtr->flags &= ~ENTRY_HAS_BUTTON;
	}
	if (tvPtr->levelInfo != NULL) {
	    Blt_Free(tvPtr->levelInfo);
	}
	tvPtr->levelInfo = Blt_Calloc(tvPtr->depth + 2, sizeof(LevelInfo));
	assert(tvPtr->levelInfo);
	tvPtr->flags &= ~(TV_DIRTY | TV_UPDATE | TV_RESORT);
	if (tvPtr->flags & TV_SORT_AUTO) {
	    /* If we're auto-sorting, schedule the view to be resorted. */
	    tvPtr->flags |= TV_SORT_PENDING;
	}
    } 

    if (tvPtr->flags & TV_SORT_PENDING) {
	Blt_TreeViewSortFlatView(tvPtr);
    }

    tvPtr->levelInfo[0].labelWidth = tvPtr->levelInfo[0].x = 
	    tvPtr->levelInfo[0].iconWidth = 0;
    /* 
     * Pass 2:	Loop through all open/mapped nodes. 
     *
     *		1. Set world y-coordinates for entries. We must defer
     *		   setting the x-coordinates until we know the maximum 
     *		   icon sizes at each level.
     *		2. Compute the maximum depth of the tree. 
     *		3. Build an array to hold level information.
     */
    y = 0;			
    count = 0;
    for(p = tvPtr->flatArr; *p != NULL; p++) {
	entryPtr = *p;
	entryPtr->flatIndex = count++;
	entryPtr->worldY = y;
	entryPtr->vertLineLength = 0;
	y += entryPtr->height;
	if (tvPtr->levelInfo[0].labelWidth < entryPtr->labelWidth) {
	    tvPtr->levelInfo[0].labelWidth = entryPtr->labelWidth;
	}
	if (tvPtr->levelInfo[0].iconWidth < entryPtr->iconWidth) {
	    tvPtr->levelInfo[0].iconWidth = entryPtr->iconWidth;
	}
    }
    tvPtr->levelInfo[0].iconWidth |= 0x01;
    tvPtr->worldHeight = y;	/* Set the scroll height of the hierarchy. */
    if (tvPtr->worldHeight < 1) {
	tvPtr->worldHeight = 1;
    }
    maxX = tvPtr->levelInfo[0].iconWidth + tvPtr->levelInfo[0].labelWidth;
    tvPtr->treeColumn.maxWidth = maxX;
    tvPtr->treeWidth = maxX;
    tvPtr->flags |= TV_VIEWPORT;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeTreeLayout --
 *
 *	Recompute the layout when entries are opened/closed,
 *	inserted/deleted, or when text attributes change (such as
 *	font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeTreeLayout(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    TreeViewEntry *entryPtr;
    int maxX, x, y;
    int sum;
    register int i;

    /* 
     * Pass 1:	Reinitialize column sizes and loop through all nodes. 
     *
     *		1. Recalculate the size of each entry as needed. 
     *		2. The maximum depth of the tree. 
     *		3. Minimum height of an entry.  Dividing this by the
     *		   height of the widget gives a rough estimate of the 
     *		   maximum number of visible entries.
     *		4. Build an array to hold level information to be filled
     *		   in on pass 2.
     */
    if (tvPtr->flags & TV_DIRTY) {
	int position;

	position = 1;
	for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    columnPtr = Blt_ChainGetValue(linkPtr);
	    columnPtr->maxWidth = 0;
	    columnPtr->max = SHRT_MAX;
	    if (columnPtr->reqMax > 0) {
		columnPtr->max = columnPtr->reqMax;
	    }
	    columnPtr->position = position;
	    position++;
	}
	tvPtr->minHeight = SHRT_MAX;
	tvPtr->depth = 0;
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, 0)) {
	    GetEntryExtents(tvPtr, entryPtr);
	    if (tvPtr->minHeight > entryPtr->height) {
		tvPtr->minHeight = entryPtr->height;
	    }
	    /* 
	     * Determine if the entry should display a button
	     * (indicating that it has children) and mark the
	     * entry accordingly. 
	     */
	    entryPtr->flags &= ~ENTRY_HAS_BUTTON;
	    if (entryPtr->flags & BUTTON_SHOW) {
		entryPtr->flags |= ENTRY_HAS_BUTTON;
	    } else if (entryPtr->flags & BUTTON_AUTO) {
		if (tvPtr->flags & TV_HIDE_LEAVES) {
		    /* Check that a non-leaf child exists */
		    if (Blt_TreeViewFirstChild(entryPtr, ENTRY_HIDDEN) 
			!= NULL) {
			entryPtr->flags |= ENTRY_HAS_BUTTON;
		    }
		} else if (!Blt_TreeIsLeaf(entryPtr->node)) {
		    entryPtr->flags |= ENTRY_HAS_BUTTON;
		}
	    }
	    /* Determine the depth of the tree. */
	    if (tvPtr->depth < DEPTH(tvPtr, entryPtr->node)) {
		tvPtr->depth = DEPTH(tvPtr, entryPtr->node);
	    }
	}
	if (tvPtr->flags & TV_SORT_PENDING) {
	    Blt_TreeViewSortTreeView(tvPtr);
	}
	if (tvPtr->levelInfo != NULL) {
	    Blt_Free(tvPtr->levelInfo);
	}
	tvPtr->levelInfo = Blt_Calloc(tvPtr->depth + 2, sizeof(LevelInfo));
	assert(tvPtr->levelInfo);
	tvPtr->flags &= ~(TV_DIRTY | TV_RESORT);
    }
    for (i = 0; i <= (tvPtr->depth + 1); i++) {
	tvPtr->levelInfo[i].labelWidth = tvPtr->levelInfo[i].x = 
	    tvPtr->levelInfo[i].iconWidth = 0;
    }
    /* 
     * Pass 2:	Loop through all open/mapped nodes. 
     *
     *		1. Set world y-coordinates for entries. We must defer
     *		   setting the x-coordinates until we know the maximum 
     *		   icon sizes at each level.
     *		2. Compute the maximum depth of the tree. 
     *		3. Build an array to hold level information.
     */
    y = 0;
    if (tvPtr->flags & TV_HIDE_ROOT) {
	/* If the root entry is to be hidden, cheat by offsetting
	 * the y-coordinates by the height of the entry. */
	y = -(tvPtr->rootPtr->height);
    } 
    ResetCoordinates(tvPtr, tvPtr->rootPtr, &y);
    tvPtr->worldHeight = y;	/* Set the scroll height of the hierarchy. */
    if (tvPtr->worldHeight < 1) {
	tvPtr->worldHeight = 1;
    }
    sum = maxX = 0;
    for (i = 0; i <= (tvPtr->depth + 1); i++) {
	sum += tvPtr->levelInfo[i].iconWidth;
	if (i <= tvPtr->depth) {
	    tvPtr->levelInfo[i + 1].x = sum;
	}
	x = sum + tvPtr->levelInfo[i].labelWidth;
	if (x > maxX) {
	    maxX = x;
	}
    }
    tvPtr->treeColumn.maxWidth = maxX;
    tvPtr->treeWidth = maxX;
}


static void
LayoutColumns(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    int sum;

    /* The width of the widget (in world coordinates) is the sum 
     * of the column widths. */

    tvPtr->worldWidth = tvPtr->titleHeight = 0;
    sum = 0;
    for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	columnPtr->width = 0;
	if (!columnPtr->hidden) {
	    if ((tvPtr->flags & TV_SHOW_COLUMN_TITLES) &&
		(tvPtr->titleHeight < columnPtr->titleHeight)) {
		tvPtr->titleHeight = columnPtr->titleHeight;
	    }
	    if (columnPtr->reqWidth > 0) {
		columnPtr->width = columnPtr->reqWidth;
	    } else {
		/* The computed width of a column is the maximum of
		 * the title width and the widest entry. */
		columnPtr->width = MAX(columnPtr->titleWidth, 
				       columnPtr->maxWidth);
		/* Check that the width stays within any constraints that
		 * have been set. */
		if ((columnPtr->reqMin > 0) && 
		    (columnPtr->reqMin > columnPtr->width)) {
		    columnPtr->width = columnPtr->reqMin;
		}
		if ((columnPtr->reqMax > 0) && 
		    (columnPtr->reqMax < columnPtr->width)) {
		    columnPtr->width = columnPtr->reqMax;
		}
	    }
	    columnPtr->width += 
		PADDING(columnPtr->pad) + 2 * columnPtr->borderWidth;
	}
	columnPtr->worldX = sum;
	sum += columnPtr->width;
    }
    tvPtr->worldWidth = sum;
    if (VPORTWIDTH(tvPtr) > sum) {
	AdjustColumns(tvPtr);
    }
    sum = 0;
    for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	columnPtr->worldX = sum;
	sum += columnPtr->width;
    }
    if (tvPtr->titleHeight > 0) {
	/* If any headings are displayed, add some extra padding to
	 * the height. */
	tvPtr->titleHeight += 4;
    }
    /* tvPtr->worldWidth += 10; */
    if (tvPtr->yScrollUnits < 1) {
	tvPtr->yScrollUnits = 1;
    }
    if (tvPtr->xScrollUnits < 1) {
	tvPtr->xScrollUnits = 1;
    }
    if (tvPtr->worldWidth < 1) {
	tvPtr->worldWidth = 1;
    }
    tvPtr->flags &= ~TV_LAYOUT;
    tvPtr->flags |= TV_SCROLL;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_TreeViewComputeLayout --
 *
 *	Recompute the layout when entries are opened/closed,
 *	inserted/deleted, or when text attributes change (such as
 *	font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_TreeViewComputeLayout(TreeView *tvPtr)
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;

    if (tvPtr->flatView) {
	ComputeFlatLayout(tvPtr);
    } else {
	ComputeTreeLayout(tvPtr);
    }
    /*
     * Determine the width of each column based upon the entries
     * that as open (not hidden).  The widest entry in a column
     * determines the width of that column.
     */

    /* Initialize the columns. */
    for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	 linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	columnPtr->maxWidth = 0;
	columnPtr->max = SHRT_MAX;
	if (columnPtr->reqMax > 0) {
	    columnPtr->max = columnPtr->reqMax;
	}
    }
    /* The treeview column width was computed earlier. */
    tvPtr->treeColumn.maxWidth = tvPtr->treeWidth;

    /* 
     * Look at all open entries and their values.  Determine the column
     * widths by tracking the maximum width value in each column.
     */
    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	 entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = valuePtr->nextPtr) {
	    if (valuePtr->columnPtr->maxWidth < valuePtr->width) {
		valuePtr->columnPtr->maxWidth = valuePtr->width;
	    }
	}	    
    }
    /* Now layout the columns with the proper sizes. */
    LayoutColumns(tvPtr);
}
    

/*
 * ----------------------------------------------------------------------
 *
 * ComputeVisibleEntries --
 *
 *	The entries visible in the viewport (the widget's window) are
 *	inserted into the array of visible nodes.
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 * ----------------------------------------------------------------------
 */
static int
ComputeVisibleEntries(TreeView *tvPtr)
{
    int height;
    int level;
    int nSlots;
    int x, maxX;
    int xOffset, yOffset;

    xOffset = Blt_AdjustViewport(tvPtr->xOffset, tvPtr->worldWidth,
	VPORTWIDTH(tvPtr), tvPtr->xScrollUnits, tvPtr->scrollMode);
    yOffset = Blt_AdjustViewport(tvPtr->yOffset, 
	tvPtr->worldHeight, VPORTHEIGHT(tvPtr), tvPtr->yScrollUnits, 
	tvPtr->scrollMode);

    if ((xOffset != tvPtr->xOffset) || (yOffset != tvPtr->yOffset)) {
	tvPtr->yOffset = yOffset;
	tvPtr->xOffset = xOffset;
	tvPtr->flags |= TV_VIEWPORT;
    }
    height = VPORTHEIGHT(tvPtr);

    /* Allocate worst case number of slots for entry array. */
    nSlots = (height / tvPtr->minHeight) + 3;
    if (nSlots != tvPtr->nVisible) {
	if (tvPtr->visibleArr != NULL) {
	    Blt_Free(tvPtr->visibleArr);
	}
	tvPtr->visibleArr = Blt_Calloc(nSlots, sizeof(TreeViewEntry *));
	assert(tvPtr->visibleArr);
    }
    tvPtr->nVisible = 0;

    if (tvPtr->rootPtr->flags & ENTRY_HIDDEN) {
	return TCL_OK;		/* Root node is hidden. */
    }
    /* Find the node where the view port starts. */
    if (tvPtr->flatView) {
	register TreeViewEntry **p, *entryPtr;

	/* Find the starting entry visible in the viewport. It can't
	 * be hidden or any of it's ancestors closed. */
    again:
	for (p = tvPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    if ((entryPtr->worldY + entryPtr->height) > tvPtr->yOffset) {
		break;
	    }
	}	    
	/*
	 * If we can't find the starting node, then the view must be
	 * scrolled down, but some nodes were deleted.  Reset the view
	 * back to the top and try again.
	 */
	if (*p == NULL) {
	    if (tvPtr->yOffset == 0) {
		return TCL_OK;	/* All entries are hidden. */
	    }
	    tvPtr->yOffset = 0;
	    goto again;
	}

	maxX = 0;
	height += tvPtr->yOffset;
	for (/* empty */; *p != NULL; p++) {
	    entryPtr = *p;
	    entryPtr->worldX = LEVELX(0) + tvPtr->treeColumn.worldX;
	    x = entryPtr->worldX + ICONWIDTH(0) + entryPtr->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if (entryPtr->worldY >= height) {
		break;
	    }
	    tvPtr->visibleArr[tvPtr->nVisible] = *p;
	    tvPtr->nVisible++;
	}
	tvPtr->visibleArr[tvPtr->nVisible] = NULL;
    } else {
	TreeViewEntry *entryPtr;

	entryPtr = tvPtr->rootPtr;
	while ((entryPtr->worldY + entryPtr->height) <= tvPtr->yOffset) {
	    for (entryPtr = Blt_TreeViewLastChild(entryPtr, ENTRY_HIDDEN);
		 entryPtr != NULL; 
		 entryPtr = Blt_TreeViewPrevSibling(entryPtr, ENTRY_HIDDEN)) {
		if (entryPtr->worldY <= tvPtr->yOffset) {
		    break;
		}
	    }
	    /*
	     * If we can't find the starting node, then the view must be
	     * scrolled down, but some nodes were deleted.  Reset the view
	     * back to the top and try again.
	     */
	    if (entryPtr == NULL) {
		if (tvPtr->yOffset == 0) {
		    return TCL_OK;	/* All entries are hidden. */
		}
		tvPtr->yOffset = 0;
		continue;
	    }
	}
	

	height += tvPtr->yOffset;
	maxX = 0;
	tvPtr->treeColumn.maxWidth = tvPtr->treeWidth;

	for (/* empty */; entryPtr != NULL; 
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
	    /*
	     * Compute and save the entry's X-coordinate now that we know
	     * the maximum level offset for the entire widget.
	     */
	    level = DEPTH(tvPtr, entryPtr->node);
	    entryPtr->worldX = LEVELX(level) + tvPtr->treeColumn.worldX;
	    
	    x = entryPtr->worldX + ICONWIDTH(level) + ICONWIDTH(level + 1) + 
		entryPtr->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if (entryPtr->worldY >= height) {
		break;
	    }
	    tvPtr->visibleArr[tvPtr->nVisible] = entryPtr;
	    tvPtr->nVisible++;
	}
	tvPtr->visibleArr[tvPtr->nVisible] = NULL;
    }
    /*
     * -------------------------------------------------------------------
     *
     * Note:	It's assumed that the view port always starts at or
     *		over an entry.  Check that a change in the hierarchy
     *		(e.g. closing a node) hasn't left the viewport beyond
     *		the last entry.  If so, adjust the viewport to start
     *		on the last entry.
     *
     * -------------------------------------------------------------------
     */
    if (tvPtr->xOffset > (tvPtr->worldWidth - tvPtr->xScrollUnits)) {
	tvPtr->xOffset = tvPtr->worldWidth - tvPtr->xScrollUnits;
    }
    if (tvPtr->yOffset > (tvPtr->worldHeight - tvPtr->yScrollUnits)) {
	tvPtr->yOffset = tvPtr->worldHeight - tvPtr->yScrollUnits;
    }
    tvPtr->xOffset = Blt_AdjustViewport(tvPtr->xOffset, 
	tvPtr->worldWidth, VPORTWIDTH(tvPtr), tvPtr->xScrollUnits, 
	tvPtr->scrollMode);
    tvPtr->yOffset = Blt_AdjustViewport(tvPtr->yOffset,
	tvPtr->worldHeight, VPORTHEIGHT(tvPtr), tvPtr->yScrollUnits,
	tvPtr->scrollMode);

    Blt_PickCurrentItem(tvPtr->bindTable);
    tvPtr->flags &= ~TV_DIRTY;
    return TCL_OK;
}


/*
 * ---------------------------------------------------------------------------
 *
 * DrawVerticals --
 *
 * 	Draws vertical lines for the ancestor nodes.  While the entry
 *	of the ancestor may not be visible, its vertical line segment
 *	does extent into the viewport.  So walk back up the hierarchy
 *	drawing lines until we get to the root.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Vertical lines are drawn for the ancestor nodes.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawVerticals(
    TreeView *tvPtr,		/* Widget record containing the attribute
				 * information for buttons. */
    TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)		/* Pixmap or window to draw into. */
{
    int height, level;
    int x, y;
    int x1, y1, x2, y2;

    while (entryPtr != tvPtr->rootPtr) {
	entryPtr = Blt_TreeViewParentEntry(entryPtr);
	if (entryPtr == NULL) {
	    break;
	}
	level = DEPTH(tvPtr, entryPtr->node);
	/*
	 * World X-coordinates aren't computed only for entries that are
	 * outside the view port.  So for each off-screen ancestor node
	 * compute it here too.
	 */
	entryPtr->worldX = LEVELX(level) + tvPtr->treeColumn.worldX;
	x = SCREENX(tvPtr, entryPtr->worldX);
	y = SCREENY(tvPtr, entryPtr->worldY);
	height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
		tvPtr->button.height);
	y += (height - tvPtr->button.height) / 2;
	x1 = x2 = x + ICONWIDTH(level) + ICONWIDTH(level + 1) / 2;
	y1 = y + tvPtr->button.height / 2;
	y2 = y1 + entryPtr->vertLineLength;
	if ((entryPtr == tvPtr->rootPtr) && (tvPtr->flags & TV_HIDE_ROOT)) {
	    y1 += entryPtr->height;
	}
	/*
	 * Clip the line's Y-coordinates at the viewport borders.
	 */
	if (y1 < 0) {
	    y1 = (y1 & 0x1);	/* Make sure the dotted line starts on 
				 * the same even/odd pixel. */
	}
	if (y2 > Tk_Height(tvPtr->tkwin)) {
	    y2 = Tk_Height(tvPtr->tkwin);
	}
	if ((y1 < Tk_Height(tvPtr->tkwin)) && (y2 > 0)) {
	    XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, 
	      x1, y1, x2, y2);
	}
    }
}

void
Blt_TreeViewDrawRule(
    TreeView *tvPtr,		/* Widget record containing the
				 * attribute information for rules. */
    TreeViewColumn *columnPtr,
    Drawable drawable)		/* Pixmap or window to draw into. */
{
    int x, y1, y2;

    x = SCREENX(tvPtr, columnPtr->worldX) + 
	columnPtr->width + tvPtr->ruleMark - tvPtr->ruleAnchor - 1;

    y1 = tvPtr->titleHeight + tvPtr->inset;
    y2 = Tk_Height(tvPtr->tkwin) - tvPtr->inset;
    XDrawLine(tvPtr->display, drawable, columnPtr->ruleGC, x, y1, x, y2);
    tvPtr->flags = TOGGLE(tvPtr->flags, TV_RULE_ACTIVE);
}

/*
 * ---------------------------------------------------------------------------
 *
 * Blt_TreeViewDrawButton --
 *
 * 	Draws a button for the given entry. The button is drawn
 * 	centered in the region immediately to the left of the origin
 * 	of the entry (computed in the layout routines). The height
 * 	and width of the button were previously calculated from the
 * 	average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button may have a border.  The symbol (either a plus or
 *	minus) is slight smaller than the width or height minus the
 *	border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
void
Blt_TreeViewDrawButton(
    TreeView *tvPtr,		/* Widget record containing the
				 * attribute information for
				 * buttons. */
    TreeViewEntry *entryPtr,	/* Entry. */
    Drawable drawable,		/* Pixmap or window to draw into. */
    int x, 
    int y)
{
    Tk_3DBorder border;
    TreeViewButton *buttonPtr = &tvPtr->button;
    TreeViewIcon icon;
    int relief;
    int width, height;

    if (entryPtr == tvPtr->activeButtonPtr) {
	border = buttonPtr->activeBorder;
    } else {
	border = buttonPtr->border;
    }
    if (entryPtr->flags & ENTRY_CLOSED) {
	relief = buttonPtr->closeRelief;
    } else {
	relief = buttonPtr->openRelief;
    }
    if (relief == TK_RELIEF_SOLID) {
	relief = TK_RELIEF_FLAT;
    }
    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, x, y,
	buttonPtr->width, buttonPtr->height, buttonPtr->borderWidth, relief);

    x += buttonPtr->borderWidth;
    y += buttonPtr->borderWidth;
    width = buttonPtr->width - (2 * buttonPtr->borderWidth);
    height = buttonPtr->height - (2 * buttonPtr->borderWidth);

    icon = NULL;
    if (buttonPtr->icons != NULL) {  /* Open or close button icon? */
	icon = buttonPtr->icons[0];
	if (((entryPtr->flags & ENTRY_CLOSED) == 0) && 
	    (buttonPtr->icons[1] != NULL)) {
	    icon = buttonPtr->icons[1];
	}
    }
    if (icon != NULL) {	/* Icon or rectangle? */
	Tk_RedrawImage(TreeViewIconBits(icon), 0, 0, width, height, drawable,
		       x, y);
    } else {
	int top, bottom, left, right;
	XSegment segments[6];
	int count;
	GC gc;

	gc = (entryPtr == tvPtr->activeButtonPtr)
	    ? buttonPtr->activeGC : buttonPtr->normalGC;
	if (relief == TK_RELIEF_FLAT) {
	    /* Draw the box outline */

	    left = x - buttonPtr->borderWidth;
	    top = y - buttonPtr->borderWidth;
	    right = left + buttonPtr->width - 1;
	    bottom = top + buttonPtr->height - 1;

	    segments[0].x1 = left;
	    segments[0].x2 = right;
	    segments[0].y2 = segments[0].y1 = top;
	    segments[1].x2 = segments[1].x1 = right;
	    segments[1].y1 = top;
	    segments[1].y2 = bottom;
	    segments[2].x2 = segments[2].x1 = left;
	    segments[2].y1 = top;
	    segments[2].y2 = bottom;
#ifdef WIN32
	    segments[2].y2++;
#endif
	    segments[3].x1 = left;
	    segments[3].x2 = right;
	    segments[3].y2 = segments[3].y1 = bottom;
#ifdef WIN32
	    segments[3].x2++;
#endif
	}
	top = y + height / 2;
	left = x + BUTTON_IPAD;
	right = x + width - BUTTON_IPAD;

	segments[4].y1 = segments[4].y2 = top;
	segments[4].x1 = left;
	segments[4].x2 = right - 1;
#ifdef WIN32
	segments[4].x2++;
#endif

	count = 5;
	if (entryPtr->flags & ENTRY_CLOSED) { /* Draw the vertical
					       * line for the plus. */
	    top = y + BUTTON_IPAD;
	    bottom = y + height - BUTTON_IPAD;
	    segments[5].y1 = top;
	    segments[5].y2 = bottom - 1;
	    segments[5].x1 = segments[5].x2 = x + width / 2;
#ifdef WIN32
	    segments[5].y2++;
#endif
	    count = 6;
	}
	XDrawSegments(tvPtr->display, drawable, gc, segments, count);
    }
}


/*
 * ---------------------------------------------------------------------------
 *
 * Blt_TreeViewGetEntryIcon --
 *
 * 	Selects the correct image for the entry's icon depending upon
 *	the current state of the entry: active/inactive normal/selected.  
 *
 *		active - normal
 *		active - selected
 *		inactive - normal
 *		inactive - selected
 *
 * Results:
 *	Returns the image for the icon.
 *
 * ---------------------------------------------------------------------------
 */
TreeViewIcon
Blt_TreeViewGetEntryIcon(TreeView *tvPtr, TreeViewEntry *entryPtr)
{
    TreeViewIcon *icons;
    TreeViewIcon icon;

    int isActive, hasFocus;

    isActive = (entryPtr == tvPtr->activePtr);
    hasFocus = (entryPtr == tvPtr->focusPtr);
    icons = NULL;
    if (isActive) {
	icons = CHOOSE(tvPtr->activeIcons, entryPtr->activeIcons);
    }
    if (icons == NULL) {
	icons = CHOOSE(tvPtr->icons, entryPtr->icons);

    }
    icon = NULL;
    if (icons != NULL) {	/* Selected or normal icon? */
	icon = icons[0];
	if ((hasFocus) && (icons[1] != NULL)) {
	    icon = icons[1];
	}
    }
    return icon;
}


int
Blt_TreeViewDrawIcon(
    TreeView *tvPtr,		/* Widget record containing the attribute
				 * information for buttons. */
    TreeViewEntry *entryPtr,	/* Entry to display. */
    Drawable drawable,		/* Pixmap or window to draw into. */
    int x, 
    int y)
{
    TreeViewIcon icon;

    icon = Blt_TreeViewGetEntryIcon(tvPtr, entryPtr);

    if (icon != NULL) {	/* Icon or default icon bitmap? */
	int entryHeight;
	int level;
	int maxY;
	int top, bottom;
	int topInset, botInset;
	int width, height;

	level = DEPTH(tvPtr, entryPtr->node);
	entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
		tvPtr->button.height);
	height = TreeViewIconHeight(icon);
	width = TreeViewIconWidth(icon);
	if (tvPtr->flatView) {
	    x += (ICONWIDTH(0) - width) / 2;
	} else {
	    x += (ICONWIDTH(level + 1) - width) / 2;
	}	    
	y += (entryHeight - height) / 2;
	botInset = tvPtr->inset - INSET_PAD;
	topInset = tvPtr->titleHeight + tvPtr->inset;
	maxY = Tk_Height(tvPtr->tkwin) - botInset;
	top = 0;
	bottom = y + height;
	if (y < topInset) {
	    height += y - topInset;
	    top = -y + topInset;
	    y = topInset;
	} else if (bottom >= maxY) {
	    height = maxY - y;
	}
	Tk_RedrawImage(TreeViewIconBits(icon), 0, top, width, height, 
		drawable, x, y);
    } 
    return (icon != NULL);
}

static int
DrawLabel(
    TreeView *tvPtr,		/* Widget record. */
    TreeViewEntry *entryPtr,	/* Entry attribute information. */
    Drawable drawable,		/* Pixmap or window to draw into. */
    int x, 
    int y)			
{
    char *label;
    int entryHeight;
    int isFocused;
    int width, height;		/* Width and height of label. */
    int selected;

    entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
       tvPtr->button.height);
    isFocused = ((entryPtr == tvPtr->focusPtr) && 
		 (tvPtr->flags & TV_FOCUS));
    selected = Blt_TreeViewEntryIsSelected(tvPtr, entryPtr);

    /* Includes padding, selection 3-D border, and focus outline. */
    width = entryPtr->labelWidth;
    height = entryPtr->labelHeight;

    /* Center the label, if necessary, vertically along the entry row. */
    if (height < entryHeight) {
	y += (entryHeight - height) / 2;
    }
    if (isFocused) {		/* Focus outline */
	if (selected) {
	    XColor *color;

	    color = SELECT_FG(tvPtr);
	    XSetForeground(tvPtr->display, tvPtr->focusGC, color->pixel);
	}
	XDrawRectangle(tvPtr->display, drawable, tvPtr->focusGC, x, y, 
		       width - 1, height - 1);
	if (selected) {
	    XSetForeground(tvPtr->display, tvPtr->focusGC, 
		tvPtr->focusColor->pixel);
	}
    }
    x += FOCUS_WIDTH + LABEL_PADX + tvPtr->selBorderWidth;
    y += FOCUS_WIDTH + LABEL_PADY + tvPtr->selBorderWidth;

    label = GETLABEL(entryPtr);
    if (label[0] != '\0') {
	GC gc;
	TreeViewStyle *stylePtr;
	TextStyle ts;
	Tk_Font font;
	XColor *normalColor, *activeColor;

	stylePtr = tvPtr->treeColumn.stylePtr;
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeViewGetStyleFont(tvPtr, stylePtr);
	}
	normalColor = entryPtr->color;
	if (normalColor == NULL) {
	    normalColor = Blt_TreeViewGetStyleFg(tvPtr, stylePtr);
	}
	activeColor = (selected) ? SELECT_FG(tvPtr) : normalColor;
	gc = entryPtr->gc;
	if (gc == NULL) {
	    gc = Blt_TreeViewGetStyleGC(stylePtr);
	}
	Blt_SetDrawTextStyle(&ts, font, gc, normalColor, activeColor, 
		entryPtr->shadow.color, 0.0, TK_ANCHOR_NW, TK_JUSTIFY_LEFT, 0, 
		entryPtr->shadow.offset);
	ts.state = (selected || (entryPtr->gc == NULL)) ? STATE_ACTIVE : 0;
	Blt_DrawTextLayout(tvPtr->tkwin, drawable, entryPtr->textPtr, 
		&ts, x, y);
    }
    return entryHeight;
}

/*
 * ---------------------------------------------------------------------------
 *
 * DrawFlatEntry --
 *
 * 	Draws a button for the given entry.  Note that buttons should only
 *	be drawn if the entry has sub-entries to be opened or closed.  It's
 *	the responsibility of the calling routine to ensure this.
 *
 *	The button is drawn centered in the region immediately to the left
 *	of the origin of the entry (computed in the layout routines). The
 *	height and width of the button were previously calculated from the
 *	average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button has a border.  The symbol (either a plus or minus) is
 *	slight smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawFlatEntry(
    TreeView *tvPtr,		/* Widget record containing the attribute
				 * information for buttons. */
    TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)		/* Pixmap or window to draw into. */
{
    int level;
    int x, y;

    entryPtr->flags &= ~ENTRY_REDRAW;

    x = SCREENX(tvPtr, entryPtr->worldX);
    y = SCREENY(tvPtr, entryPtr->worldY);
    if (!Blt_TreeViewDrawIcon(tvPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    level = 0;
    x += ICONWIDTH(level);
    /* Entry label. */
    DrawLabel(tvPtr, entryPtr, drawable, x, y);
}

/*
 * ---------------------------------------------------------------------------
 *
 * DrawTreeEntry --
 *
 * 	Draws a button for the given entry.  Note that buttons should only
 *	be drawn if the entry has sub-entries to be opened or closed.  It's
 *	the responsibility of the calling routine to ensure this.
 *
 *	The button is drawn centered in the region immediately to the left
 *	of the origin of the entry (computed in the layout routines). The
 *	height and width of the button were previously calculated from the
 *	average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button has a border.  The symbol (either a plus or minus) is
 *	slight smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawTreeEntry(
    TreeView *tvPtr,		/* Widget record. */
    TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)		/* Pixmap or window to draw into. */
{
    TreeViewButton *buttonPtr = &tvPtr->button;
    int buttonY;
    int level;
    int width, height;
    int x, y;
    int x1, y1, x2, y2;

    entryPtr->flags &= ~ENTRY_REDRAW;

    x = SCREENX(tvPtr, entryPtr->worldX);
    y = SCREENY(tvPtr, entryPtr->worldY);

    level = DEPTH(tvPtr, entryPtr->node);
    width = ICONWIDTH(level);
    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
	buttonPtr->height);

    entryPtr->buttonX = (width - buttonPtr->width) / 2;
    entryPtr->buttonY = (height - buttonPtr->height) / 2;

    buttonY = y + entryPtr->buttonY;

    x1 = x + (width / 2);
    y1 = y2 = buttonY + (buttonPtr->height / 2);
    x2 = x1 + (ICONWIDTH(level) + ICONWIDTH(level + 1)) / 2;

    if ((Blt_TreeNodeParent(entryPtr->node) != NULL) && 
	(tvPtr->lineWidth > 0)) {
	/*
	 * For every node except root, draw a horizontal line from
	 * the vertical bar to the middle of the icon.
	 */
	XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, x1, y1, x2, y2);
    }
    if (((entryPtr->flags & ENTRY_CLOSED) == 0) && (tvPtr->lineWidth > 0)) {
	/*
	 * Entry is open, draw vertical line.
	 */
	y2 = y1 + entryPtr->vertLineLength;
	if (y2 > Tk_Height(tvPtr->tkwin)) {
	    y2 = Tk_Height(tvPtr->tkwin); /* Clip line at window border. */
	}
	XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, x2, y1, x2, y2);
    }
    if ((entryPtr->flags & ENTRY_HAS_BUTTON) && (entryPtr != tvPtr->rootPtr)) {
	/*
	 * Except for the root, draw a button for every entry that
	 * needs one.  The displayed button can be either an icon (Tk
	 * image) or a line drawing (rectangle with plus or minus
	 * sign).
	 */
	Blt_TreeViewDrawButton(tvPtr, entryPtr, drawable, x + entryPtr->buttonX,
		y + entryPtr->buttonY);
    }
    x += ICONWIDTH(level);

    if (!Blt_TreeViewDrawIcon(tvPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    x += ICONWIDTH(level + 1) + 4;

    /* Entry label. */
    DrawLabel(tvPtr, entryPtr, drawable, x, y);
}

/*
 * ---------------------------------------------------------------------------
 *
 * Blt_TreeViewDrawValue --
 *
 * 	Draws a column value for the given entry.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
void
Blt_TreeViewDrawValue(
    TreeView *tvPtr,		/* Widget record. */
    TreeViewEntry *entryPtr,	/* Node of entry to be drawn. */
    TreeViewValue *valuePtr,
    Drawable drawable,		/* Pixmap or window to draw into. */
    int x, 
    int y)
{
    TreeViewStyle *stylePtr;

    stylePtr = CHOOSE(valuePtr->columnPtr->stylePtr, valuePtr->stylePtr);
    (*stylePtr->classPtr->drawProc)(tvPtr, drawable, entryPtr, valuePtr, 
		stylePtr, x, y);
}

static void
DrawTitle(
    TreeView *tvPtr,
    TreeViewColumn *columnPtr,
    Drawable drawable,
    int x)
{
    GC gc;
    Tk_3DBorder border;
    XColor *fgColor;
    int columnWidth;
    int width;
    int x0, cx, xOffset;

    columnWidth = columnPtr->width;
    cx = x;
    if (columnPtr->position == Blt_ChainGetLength(tvPtr->colChainPtr)) {
	/* If there's any room left over, let the last column take it. */
	columnWidth = Tk_Width(tvPtr->tkwin) - x;
    } else if (columnPtr->position == 1) {
	columnWidth += x;
	cx = 0;
    }
    x0 = x + columnPtr->borderWidth;

    if (columnPtr == tvPtr->activeTitleColumnPtr) {
	border = columnPtr->activeTitleBorder;
	gc = columnPtr->activeTitleGC;
	fgColor = columnPtr->activeTitleFgColor;
    } else {
	border = columnPtr->titleBorder;
	gc = columnPtr->titleGC;
	fgColor = columnPtr->titleFgColor;
    }
    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, cx + 1, 
	tvPtr->inset + 1, columnWidth - 2, tvPtr->titleHeight - 2, 0, 
	TK_RELIEF_FLAT);
    width = columnPtr->width;
    xOffset = x0 + columnPtr->pad.side1 + 1;
    
    if (width > columnPtr->titleWidth) {
	x += (width - columnPtr->titleWidth) / 2;
    }
    if (columnPtr == tvPtr->sortColumnPtr) {
	/* Make sure there's room for the sorting-direction triangle. */
	if ((x - xOffset) <= (STD_ARROW_WIDTH + 4)) {
	    x = xOffset + STD_ARROW_WIDTH + 4;
	}
    }
    if (columnPtr->titleIcon != NULL) {
	int iconX, iconY, iconWidth, iconHeight;

	iconHeight = TreeViewIconHeight(columnPtr->titleIcon);
	iconWidth = TreeViewIconWidth(columnPtr->titleIcon);
	iconX = x;
	if (columnPtr->titleTextPtr != NULL) {
	    iconX += 2;
	}
	iconY = tvPtr->inset + (tvPtr->titleHeight - iconHeight) / 2;
	Tk_RedrawImage(TreeViewIconBits(columnPtr->titleIcon), 0, 0, 
	   iconWidth, iconHeight, drawable, iconX, iconY);
	x += iconWidth + 6;
    }
    if (columnPtr->titleTextPtr != NULL) {
	TextStyle ts;

	Blt_SetDrawTextStyle(&ts, columnPtr->titleFont, gc, fgColor,
	    SELECT_FG(tvPtr), columnPtr->titleShadow.color, 0.0, TK_ANCHOR_NW,
		TK_JUSTIFY_LEFT, 0, columnPtr->titleShadow.offset);
	Blt_DrawTextLayout(tvPtr->tkwin, drawable, columnPtr->titleTextPtr, &ts,
		x, tvPtr->inset + 1);
    }
    if ((columnPtr == tvPtr->sortColumnPtr) && (tvPtr->flatView)) {
	Blt_DrawArrow(tvPtr->display, drawable, gc, 
		xOffset + ARROW_OFFSET, 
		tvPtr->inset + tvPtr->titleHeight / 2, STD_ARROW_HEIGHT, 
		(tvPtr->sortDecreasing) ? ARROW_UP : ARROW_DOWN);
    }
    Blt_Draw3DRectangle(tvPtr->tkwin, drawable, border, cx, tvPtr->inset, 
	columnWidth, tvPtr->titleHeight, columnPtr->titleBorderWidth, 
	columnPtr->titleRelief);
}

void
Blt_TreeViewDrawHeadings(tvPtr, drawable)
    TreeView *tvPtr;
    Drawable drawable;
{
    Blt_ChainLink *linkPtr;
    TreeViewColumn *columnPtr;
    int x;

    for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	if (columnPtr->hidden) {
	    continue;
	}
	x = SCREENX(tvPtr, columnPtr->worldX);
	if ((x + columnPtr->width) < 0) {
	    continue;	/* Don't draw columns before the left edge. */
	}
	if (x > Tk_Width(tvPtr->tkwin)) {
	    break;		/* Discontinue when a column starts beyond
				 * the right edge. */
	}
	DrawTitle(tvPtr, columnPtr, drawable, x);
    }
}

static void
DrawTreeView(tvPtr, drawable, x)
    TreeView *tvPtr;
    Drawable drawable;
    int x;
{
    register TreeViewEntry **p;
    Tk_3DBorder selBorder;

    /* 
     * Draw the backgrounds of selected entries first.  The vertical
     * lines connecting child entries will be draw on top.
     */
    selBorder = SELECT_BORDER(tvPtr);
    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	if (Blt_TreeViewEntryIsSelected(tvPtr, *p)) {
	    int y;

	    y = SCREENY(tvPtr, (*p)->worldY) - 1;
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, selBorder, x, y, 
		tvPtr->treeColumn.width, (*p)->height + 1, 
		tvPtr->selBorderWidth, tvPtr->selRelief);
	}
    }
    if ((tvPtr->lineWidth > 0) && (tvPtr->nVisible > 0)) { 
	/* Draw all the vertical lines from topmost node. */
	DrawVerticals(tvPtr, tvPtr->visibleArr[0], drawable);
    }

    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	DrawTreeEntry(tvPtr, *p, drawable);
    }
}

static void
DrawFlatView(
    TreeView *tvPtr,
    Drawable drawable,
    int x)
{
    register TreeViewEntry **p;
    Tk_3DBorder selBorder;
    /* 
     * Draw the backgrounds of selected entries first.  The vertical
     * lines connecting child entries will be draw on top. 
     */
    selBorder = SELECT_BORDER(tvPtr);
    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	if (Blt_TreeViewEntryIsSelected(tvPtr, *p)) {
	    int y;

	    y = SCREENY(tvPtr, (*p)->worldY) - 1;
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, selBorder, x, y, 
		tvPtr->treeColumn.width, (*p)->height + 1, 
		tvPtr->selBorderWidth, tvPtr->selRelief);
	}
    }
    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	DrawFlatEntry(tvPtr, *p, drawable);
    }
}

void
Blt_TreeViewDrawOuterBorders(tvPtr, drawable)
    TreeView *tvPtr;
    Drawable drawable;
{
    /* Draw 3D border just inside of the focus highlight ring. */
    if ((tvPtr->borderWidth > 0) && (tvPtr->relief != TK_RELIEF_FLAT)) {
	Blt_Draw3DRectangle(tvPtr->tkwin, drawable, tvPtr->border,
	    tvPtr->highlightWidth, tvPtr->highlightWidth,
	    Tk_Width(tvPtr->tkwin) - 2 * tvPtr->highlightWidth,
	    Tk_Height(tvPtr->tkwin) - 2 * tvPtr->highlightWidth,
	    tvPtr->borderWidth, tvPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (tvPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (tvPtr->flags & TV_FOCUS)
	    ? tvPtr->highlightColor : tvPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(tvPtr->tkwin, gc, tvPtr->highlightWidth,
	    drawable);
    }
    tvPtr->flags &= ~TV_BORDERS;
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayTreeView --
 *
 * 	This procedure is invoked to display the widget.
 *
 *      Recompute the layout of the text if necessary. This is
 *	necessary if the world coordinate system has changed.
 *	Specifically, the following may have occurred:
 *
 *	  1.  a text attribute has changed (font, linespacing, etc.).
 *	  2.  an entry's option changed, possibly resizing the entry.
 *
 *      This is deferred to the display routine since potentially
 *      many of these may occur.
 *
 *	Set the vertical and horizontal scrollbars.  This is done
 *	here since the window width and height are needed for the
 *	scrollbar calculations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static void
DisplayTreeView(ClientData clientData)	/* Information about widget. */
{
    Blt_ChainLink *linkPtr;
    TreeView *tvPtr = clientData;
    TreeViewColumn *columnPtr;
    Pixmap drawable; 
    int width, height;
    int x;

    tvPtr->flags &= ~TV_REDRAW;
    if (tvPtr->tkwin == NULL) {
	return;			/* Window has been destroyed. */
    }
    if (tvPtr->flags & TV_LAYOUT) {
	/*
	 * Recompute the layout when entries are opened/closed,
	 * inserted/deleted, or when text attributes change (such as
	 * font, linespacing).
	 */
	Blt_TreeViewComputeLayout(tvPtr);
    }
    if (tvPtr->flags & TV_SCROLL) {
	/* 
	 * Scrolling means that the view port has changed and that the
	 * visible entries need to be recomputed.
	 */
	ComputeVisibleEntries(tvPtr);

	width = VPORTWIDTH(tvPtr);
	height = VPORTHEIGHT(tvPtr);
	if (tvPtr->flags & TV_XSCROLL) {
	    if (tvPtr->xScrollCmdPrefix != NULL) {
		Blt_UpdateScrollbar(tvPtr->interp, tvPtr->xScrollCmdPrefix,
		    (double)tvPtr->xOffset / tvPtr->worldWidth,
		    (double)(tvPtr->xOffset + width) / tvPtr->worldWidth);
	    }
	}
	if (tvPtr->flags & TV_YSCROLL) {
	    if (tvPtr->yScrollCmdPrefix != NULL) {
		Blt_UpdateScrollbar(tvPtr->interp, tvPtr->yScrollCmdPrefix,
		    (double)tvPtr->yOffset / tvPtr->worldHeight,
		    (double)(tvPtr->yOffset + height) / tvPtr->worldHeight);
	    }
	}
	tvPtr->flags &= ~TV_SCROLL;
    }
    if (tvPtr->reqWidth == 0) {

	/* 
	 * The first time through this routine, set the requested
	 * width to the computed width.  All we want is to
	 * automatically set the width of the widget, not dynamically
	 * grow/shrink it as attributes change.
	 */

	tvPtr->reqWidth = tvPtr->worldWidth + 2 * tvPtr->inset;
	Tk_GeometryRequest(tvPtr->tkwin, tvPtr->reqWidth, tvPtr->reqHeight);
    }
    if (!Tk_IsMapped(tvPtr->tkwin)) {
	return;
    }

    drawable = Tk_GetPixmap(tvPtr->display, Tk_WindowId(tvPtr->tkwin), 
	Tk_Width(tvPtr->tkwin), Tk_Height(tvPtr->tkwin), 
	Tk_Depth(tvPtr->tkwin));
    tvPtr->flags |= TV_VIEWPORT;

    if ((tvPtr->flags & TV_RULE_ACTIVE) &&
	(tvPtr->resizeColumnPtr != NULL)) {
	Blt_TreeViewDrawRule(tvPtr, tvPtr->resizeColumnPtr, drawable);
    }
    {
	register TreeViewEntry **p;
	Tk_3DBorder border, selBorder;
	int y;

	selBorder = SELECT_BORDER(tvPtr);
	for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    columnPtr = Blt_ChainGetValue(linkPtr);
	    columnPtr->flags &= ~COLUMN_DIRTY;
	    if (columnPtr->hidden) {
		continue;
	    }
	    x = SCREENX(tvPtr, columnPtr->worldX);
	    if ((x + columnPtr->width) < 0) {
		continue;	/* Don't draw columns before the left edge. */
	    }
	    if (x > Tk_Width(tvPtr->tkwin)) {
		break;		/* Discontinue when a column starts beyond
				 * the right edge. */
	    }
	    /* Clear the column background. */
	    border = Blt_TreeViewGetStyleBorder(tvPtr, tvPtr->stylePtr);
	    Blt_Fill3DRectangle(tvPtr->tkwin, drawable, border, x, 0,
	       columnPtr->width, Tk_Height(tvPtr->tkwin), 0, TK_RELIEF_FLAT);

	    if (columnPtr != &tvPtr->treeColumn) {
		TreeViewValue *valuePtr;
		TreeViewEntry *entryPtr;

		for (p = tvPtr->visibleArr; *p != NULL; p++) {
		    entryPtr = *p;
		    y = SCREENY(tvPtr, entryPtr->worldY);

		    /* Draw the background of the value. */
		    if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
			Blt_Fill3DRectangle(tvPtr->tkwin, drawable, selBorder, 
				x, y - 1, columnPtr->width, 
				entryPtr->height + 1, tvPtr->selBorderWidth, 
				tvPtr->selRelief);
		    }
		    /* Check if there's a corresponding value in the entry. */
		    valuePtr = Blt_TreeViewFindValue(entryPtr, columnPtr);
		    if (valuePtr != NULL) {
			Blt_TreeViewDrawValue(tvPtr, entryPtr, valuePtr, 
				drawable, x + columnPtr->pad.side1, y);
		    }
		}
	    } else {
		if (tvPtr->flatView) {
		    DrawFlatView(tvPtr, drawable, x);
		} else {
		    DrawTreeView(tvPtr, drawable, x);
		}
	    }
	    if (columnPtr->relief != TK_RELIEF_FLAT) {
		Blt_Draw3DRectangle(tvPtr->tkwin, drawable, border, x, 0, 
			columnPtr->width, Tk_Height(tvPtr->tkwin), 
		   columnPtr->borderWidth, columnPtr->relief);
	    }
	}
    }
    if (tvPtr->flags & TV_SHOW_COLUMN_TITLES) {
	Blt_TreeViewDrawHeadings(tvPtr, drawable);
    }
    Blt_TreeViewDrawOuterBorders(tvPtr, drawable);
    if ((tvPtr->flags & TV_RULE_NEEDED) &&
	(tvPtr->resizeColumnPtr != NULL)) {
	Blt_TreeViewDrawRule(tvPtr, tvPtr->resizeColumnPtr, drawable);
    }
    /* Now copy the new view to the window. */
    XCopyArea(tvPtr->display, drawable, Tk_WindowId(tvPtr->tkwin), 
	tvPtr->lineGC, 0, 0, Tk_Width(tvPtr->tkwin), 
	Tk_Height(tvPtr->tkwin), 0, 0);
    Tk_FreePixmap(tvPtr->display, drawable);
    tvPtr->flags &= ~TV_VIEWPORT;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewSelectCmdProc --
 *
 *      Invoked at the next idle point whenever the current
 *      selection changes.  Executes some application-specific code
 *      in the -selectcommand option.  This provides a way for
 *      applications to handle selection changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tcl code gets executed for some application-specific task.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TreeViewSelectCmdProc(ClientData clientData) 
{
    TreeView *tvPtr = clientData;

    Tcl_Preserve(tvPtr);
    if (tvPtr->selectCmd != NULL) {
	tvPtr->flags &= ~TV_SELECT_PENDING;
	if (Tcl_GlobalEval(tvPtr->interp, tvPtr->selectCmd) != TCL_OK) {
	    Tcl_BackgroundError(tvPtr->interp);
	}
    }
    Tcl_Release(tvPtr);
}

/*
 * --------------------------------------------------------------
 *
 * TreeViewObjCmd --
 *
 * 	This procedure is invoked to process the Tcl command that
 * 	corresponds to a widget managed by this module. See the user
 * 	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */
/* ARGSUSED */
static int
TreeViewObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST *objv;	/* Argument strings. */
{
    Tcl_CmdInfo cmdInfo;
    Tcl_Obj *initObjv[2];
    TreeView *tvPtr;
    char *className;
    char *string;

    string = Tcl_GetString(objv[0]);
    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", string, 
		" pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    className = (string[0] == 'h') ? "Hiertable" : "TreeView";
    tvPtr = CreateTreeView(interp, objv[1], className);
    if (tvPtr == NULL) {
	goto error;
    }
    /*
     * Invoke a procedure to initialize various bindings on treeview
     * entries.  If the procedure doesn't already exist, source it
     * from "$blt_library/treeview.tcl".  We deferred sourcing the
     * file until now so that the variable $blt_library could be set
     * within a script.
     */
    if (!Tcl_GetCommandInfo(interp, "blt::tv::Initialize", &cmdInfo)) {
	char cmd[200];
	sprintf(cmd, "set className %s\n\
source [file join $blt_library treeview.tcl]\n\
unset className\n", className);
	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    char info[200];

	    sprintf(info, "\n    (while loading bindings for %.50s)", 
		    Tcl_GetString(objv[0]));
	    Tcl_AddErrorInfo(interp, info);
	    goto error;
	}
    }
    /* 
     * Initialize the widget's configuration options here. The options
     * need to be set first, so that entry, column, and style
     * components can use them for their own GCs.
     */
    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewTreeOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(interp, tvPtr->tkwin, bltTreeViewSpecs, 
	objc - 2, objv + 2, (char *)tvPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigureComponentFromObj(interp, tvPtr->tkwin, "button", "Button",
	 bltTreeViewButtonSpecs, 0, (Tcl_Obj **)NULL, (char *)tvPtr, 0) 
	!= TCL_OK) {
	goto error;
    }

    /* 
     * Rebuild the widget's GC and other resources that are predicated
     * by the widget's configuration options.  Do the same for the
     * default column.
     */
    if (Blt_TreeViewUpdateWidget(interp, tvPtr) != TCL_OK) {
	goto error;
    }
    Blt_TreeViewUpdateColumnGCs(tvPtr, &tvPtr->treeColumn);
    Blt_TreeViewUpdateStyleGCs(tvPtr, tvPtr->stylePtr);

    /*
     * Invoke a procedure to initialize various bindings on treeview
     * entries.  If the procedure doesn't already exist, source it
     * from "$blt_library/treeview.tcl".  We deferred sourcing the
     * file until now so that the variable $blt_library could be set
     * within a script.
     */
    initObjv[0] = Tcl_NewStringObj("blt::tv::Initialize", -1);
    initObjv[1] = objv[1];
    if (Tcl_EvalObjv(interp, 2, initObjv, TCL_EVAL_GLOBAL) != TCL_OK) {
	goto error;
    }
    Tcl_DecrRefCount(initObjv[0]);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tk_PathName(tvPtr->tkwin), -1));
    return TCL_OK;
  error:
    Tk_DestroyWindow(tvPtr->tkwin);
    return TCL_ERROR;
}

int
Blt_TreeViewInit(Tcl_Interp *interp)
{
    static Blt_ObjCmdSpec cmdSpec[] = { 
	{ "treeview", TreeViewObjCmd, },
	{ "hiertable", TreeViewObjCmd, }
    };

    if (Blt_InitObjCmd(interp, "blt", cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    if (Blt_InitObjCmd(interp, "blt", cmdSpec + 1) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_TREEVIEW */
