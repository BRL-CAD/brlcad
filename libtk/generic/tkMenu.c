/* 
 * tkMenu.c --
 *
 * This file contains most of the code for implementing menus in Tk. It takes
 * care of all of the generic (platform-independent) parts of menus, and
 * is supplemented by platform-specific files. The geometry calculation
 * and drawing code for menus is in the file tkMenuDraw.c
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMenu.c 1.148 97/10/29 09:22:00
 */

/*
 * Notes on implementation of menus:
 *
 * Menus can be used in three ways:
 * - as a popup menu, either as part of a menubutton or standalone.
 * - as a menubar. The menu's cascade items are arranged according to
 * the specific platform to provide the user access to the menus at all
 * times
 * - as a tearoff palette. This is a window with the menu's items in it.
 *
 * The goal is to provide the Tk developer with a way to use a common
 * set of menus for all of these tasks.
 *
 * In order to make the bindings for cascade menus work properly under Unix,
 * the cascade menus' pathnames must be proper children of the menu that
 * they are cascade from. So if there is a menu .m, and it has two
 * cascades labelled "File" and "Edit", the cascade menus might have
 * the pathnames .m.file and .m.edit. Another constraint is that the menus
 * used for menubars must be children of the toplevel widget that they
 * are attached to. And on the Macintosh, the platform specific menu handle
 * for cascades attached to a menu bar must have a title that matches the
 * label for the cascade menu.
 *
 * To handle all of the constraints, Tk menubars and tearoff menus are
 * implemented using menu clones. Menu clones are full menus in their own
 * right; they have a Tk window and pathname associated with them; they have
 * a TkMenu structure and array of entries. However, they are linked with the
 * original menu that they were cloned from. The reflect the attributes of
 * the original, or "master", menu. So if an item is added to a menu, and
 * that menu has clones, then the item must be added to all of its clones
 * also. Menus are cloned when a menu is torn-off or when a menu is assigned
 * as a menubar using the "-menu" option of the toplevel's pathname configure
 * subcommand. When a clone is destroyed, only the clone is destroyed, but
 * when the master menu is destroyed, all clones are also destroyed. This
 * allows the developer to just deal with one set of menus when creating
 * and destroying.
 *
 * Clones are rather tricky when a menu with cascade entries is cloned (such
 * as a menubar). Not only does the menu have to be cloned, but each cascade
 * entry's corresponding menu must also be cloned. This maintains the pathname
 * parent-child hierarchy necessary for menubars and toplevels to work.
 * This leads to several special cases:
 *
 * 1. When a new menu is created, and it is pointed to by cascade entries in
 * cloned menus, the new menu has to be cloned to parallel the cascade
 * structure.
 * 2. When a cascade item is added to a menu that has been cloned, and the
 * menu that the cascade item points to exists, that menu has to be cloned.
 * 3. When the menu that a cascade entry points to is changed, the old
 * cloned cascade menu has to be discarded, and the new one has to be cloned.
 *
 */

#include "tkPort.h"
#include "tkMenu.h"

#define MENU_HASH_KEY "tkMenus"

static int menusInitialized;	/* Whether or not the hash tables, etc., have
				 * been setup */

/*
 * Configuration specs for individual menu entries. If this changes, be sure
 * to update code in TkpMenuInit that changes the font string entry.
 */

Tk_ConfigSpec tkMenuEntryConfigSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACTIVE_BG, Tk_Offset(TkMenuEntry, activeBorder),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-activeforeground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACTIVE_FG, Tk_Offset(TkMenuEntry, activeFg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-accelerator", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACCELERATOR, Tk_Offset(TkMenuEntry, accel),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-background", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_BG, Tk_Offset(TkMenuEntry, border),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|SEPARATOR_MASK|TEAROFF_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-bitmap", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_BITMAP, Tk_Offset(TkMenuEntry, bitmap),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-columnbreak", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_COLUMN_BREAK, Tk_Offset(TkMenuEntry, columnBreak),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK},
    {TK_CONFIG_STRING, "-command", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_COMMAND, Tk_Offset(TkMenuEntry, command),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_FONT, Tk_Offset(TkMenuEntry, tkfont),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-foreground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_FG, Tk_Offset(TkMenuEntry, fg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-hidemargin", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_HIDE_MARGIN, Tk_Offset(TkMenuEntry, hideMargin),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|SEPARATOR_MASK|TEAROFF_MASK},
    {TK_CONFIG_STRING, "-image", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_IMAGE, Tk_Offset(TkMenuEntry, imageString),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-indicatoron", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_INDICATOR, Tk_Offset(TkMenuEntry, indicatorOn),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-label", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_LABEL, Tk_Offset(TkMenuEntry, label),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK},
    {TK_CONFIG_STRING, "-menu", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_MENU, Tk_Offset(TkMenuEntry, name),
	CASCADE_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-offvalue", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_OFF_VALUE, Tk_Offset(TkMenuEntry, offValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_STRING, "-onvalue", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ON_VALUE, Tk_Offset(TkMenuEntry, onValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_COLOR, "-selectcolor", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_SELECT, Tk_Offset(TkMenuEntry, indicatorFg),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-selectimage", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_SELECT_IMAGE, Tk_Offset(TkMenuEntry, selectImageString),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-state", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_STATE, Tk_Offset(TkMenuEntry, state),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TEAROFF_MASK|TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-value", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_VALUE, Tk_Offset(TkMenuEntry, onValue),
	RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-variable", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_CHECK_VARIABLE, Tk_Offset(TkMenuEntry, name),
	CHECK_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-variable", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_RADIO_VARIABLE, Tk_Offset(TkMenuEntry, name),
	RADIO_BUTTON_MASK},
    {TK_CONFIG_INT, "-underline", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_UNDERLINE, Tk_Offset(TkMenuEntry, underline),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Configuration specs valid for the menu as a whole. If this changes, be sure
 * to update code in TkpMenuInit that changes the font string entry.
 */

Tk_ConfigSpec tkMenuConfigSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_MENU_ACTIVE_BG_COLOR, Tk_Offset(TkMenu, activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_MENU_ACTIVE_BG_MONO, Tk_Offset(TkMenu, activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-activeborderwidth", "activeBorderWidth",
        "BorderWidth", DEF_MENU_ACTIVE_BORDER_WIDTH,
        Tk_Offset(TkMenu, activeBorderWidth), 0},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENU_ACTIVE_FG_COLOR, Tk_Offset(TkMenu, activeFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENU_ACTIVE_FG_MONO, Tk_Offset(TkMenu, activeFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MENU_BG_COLOR, Tk_Offset(TkMenu, border), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MENU_BG_MONO, Tk_Offset(TkMenu, border), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_MENU_BORDER_WIDTH, Tk_Offset(TkMenu, borderWidth), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_MENU_CURSOR, Tk_Offset(TkMenu, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENU_DISABLED_FG_COLOR,
	Tk_Offset(TkMenu, disabledFg), TK_CONFIG_COLOR_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENU_DISABLED_FG_MONO,
	Tk_Offset(TkMenu, disabledFg), TK_CONFIG_MONO_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_MENU_FONT, Tk_Offset(TkMenu, tkfont), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MENU_FG, Tk_Offset(TkMenu, fg), 0},
    {TK_CONFIG_STRING, "-postcommand", "postCommand", "Command",
	DEF_MENU_POST_COMMAND, Tk_Offset(TkMenu, postCommand),
        TK_CONFIG_NULL_OK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_MENU_RELIEF, Tk_Offset(TkMenu, relief), 0},
    {TK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_MENU_SELECT_COLOR, Tk_Offset(TkMenu, indicatorFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_MENU_SELECT_MONO, Tk_Offset(TkMenu, indicatorFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MENU_TAKE_FOCUS, Tk_Offset(TkMenu, takeFocus), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-tearoff", "tearOff", "TearOff",
	DEF_MENU_TEAROFF, Tk_Offset(TkMenu, tearOff), 0},
    {TK_CONFIG_STRING, "-tearoffcommand", "tearOffCommand", "TearOffCommand",
	DEF_MENU_TEAROFF_CMD, Tk_Offset(TkMenu, tearOffCommand),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-title", "title", "Title",
    	DEF_MENU_TITLE, Tk_Offset(TkMenu, title), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-type", "type", "Type",
	DEF_MENU_TYPE, Tk_Offset(TkMenu, menuTypeName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Prototypes for static procedures in this file:
 */

static int		CloneMenu _ANSI_ARGS_((TkMenu *menuPtr,
			    char *newMenuName, char *newMenuTypeString));
static int		ConfigureMenu _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, int argc, char **argv,
			    int flags));
static int		ConfigureMenuCloneEntries _ANSI_ARGS_((
			    Tcl_Interp *interp, TkMenu *menuPtr, int index,
			    int argc, char **argv, int flags));
static int		ConfigureMenuEntry _ANSI_ARGS_((TkMenuEntry *mePtr,
			    int argc, char **argv, int flags));
static void		DeleteMenuCloneEntries _ANSI_ARGS_((TkMenu *menuPtr,
			    int first, int last));
static void		DestroyMenuHashTable _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static void		DestroyMenuInstance _ANSI_ARGS_((TkMenu *menuPtr));
static void		DestroyMenuEntry _ANSI_ARGS_((char *memPtr));
static int		GetIndexFromCoords
			    _ANSI_ARGS_((Tcl_Interp *interp, TkMenu *menuPtr,
			    char *string, int *indexPtr));
static int		MenuDoYPosition _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, char *arg));
static int		MenuAddOrInsert _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, char *indexString, int argc,
			    char **argv));
static void		MenuCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static TkMenuEntry *	MenuNewEntry _ANSI_ARGS_((TkMenu *menuPtr, int index,
			    int type));
static char *		MenuVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		MenuWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		MenuWorldChanged _ANSI_ARGS_((
			    ClientData instanceData));
static void		RecursivelyDeleteMenu _ANSI_ARGS_((TkMenu *menuPtr));
static void		UnhookCascadeEntry _ANSI_ARGS_((TkMenuEntry *mePtr));

/*
 * The structure below is a list of procs that respond to certain window
 * manager events. One of these includes a font change, which forces
 * the geometry proc to be called.
 */

static TkClassProcs menuClass = {
    NULL,			/* createProc. */
    MenuWorldChanged		/* geometryProc. */
};



/*
 *--------------------------------------------------------------
 *
 * Tk_MenuCmd --
 *
 *	This procedure is invoked to process the "menu" Tcl
 *	command.  See the user documentation for details on
 *	what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_MenuCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    Tk_Window new;
    register TkMenu *menuPtr;
    TkMenuReferences *menuRefPtr;
    int i, len;
    char *arg, c;
    int toplevel;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    TkMenuInit();

    toplevel = 1;
    for (i = 2; i < argc; i += 2) {
	arg = argv[i];
	len = strlen(arg);
	if (len < 2) {
	    continue;
	}
	c = arg[1];
	if ((c == 't') && (strncmp(arg, "-type", strlen(arg)) == 0)
		&& (len >= 3)) {
	    if (strcmp(argv[i + 1], "menubar") == 0) {
		toplevel = 0;
	    }
	    break;
	}
    }

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], toplevel ? ""
	    : NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the data structure for the menu.
     */

    menuPtr = (TkMenu *) ckalloc(sizeof(TkMenu));
    menuPtr->tkwin = new;
    menuPtr->display = Tk_Display(new);
    menuPtr->interp = interp;
    menuPtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(menuPtr->tkwin), MenuWidgetCmd,
	    (ClientData) menuPtr, MenuCmdDeletedProc);
    menuPtr->entries = NULL;
    menuPtr->numEntries = 0;
    menuPtr->active = -1;
    menuPtr->border = NULL;
    menuPtr->borderWidth = 0;
    menuPtr->relief = TK_RELIEF_FLAT;
    menuPtr->activeBorder = NULL;
    menuPtr->activeBorderWidth = 0;
    menuPtr->tkfont = NULL;
    menuPtr->fg = NULL;
    menuPtr->disabledFg = NULL;
    menuPtr->activeFg = NULL;
    menuPtr->indicatorFg = NULL;
    menuPtr->tearOff = 1;
    menuPtr->tearOffCommand = NULL;
    menuPtr->cursor = None;
    menuPtr->takeFocus = NULL;
    menuPtr->postCommand = NULL;
    menuPtr->postCommandGeneration = 0;
    menuPtr->postedCascade = NULL;
    menuPtr->nextInstancePtr = NULL;
    menuPtr->masterMenuPtr = menuPtr;
    menuPtr->menuType = UNKNOWN_TYPE;
    menuPtr->menuFlags = 0;
    menuPtr->parentTopLevelPtr = NULL;
    menuPtr->menuTypeName = NULL;
    menuPtr->title = NULL;
    TkMenuInitializeDrawingFields(menuPtr);

    menuRefPtr = TkCreateMenuReferences(menuPtr->interp,
	    Tk_PathName(menuPtr->tkwin));
    menuRefPtr->menuPtr = menuPtr;
    menuPtr->menuRefPtr = menuRefPtr;
    if (TCL_OK != TkpNewMenu(menuPtr)) {
    	goto error;
    }

    Tk_SetClass(menuPtr->tkwin, "Menu");
    TkSetClassProcs(menuPtr->tkwin, &menuClass, (ClientData) menuPtr);
    Tk_CreateEventHandler(new, ExposureMask|StructureNotifyMask|ActivateMask,
	    TkMenuEventProc, (ClientData) menuPtr);
    if (ConfigureMenu(interp, menuPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    /*
     * If a menu has a parent menu pointing to it as a cascade entry, the
     * parent menu needs to be told that this menu now exists so that
     * the platform-part of the menu is correctly updated.
     *
     * If a menu has an instance and has cascade entries, then each cascade
     * menu must also have a parallel instance. This is especially true on
     * the Mac, where each menu has to have a separate title everytime it is in
     * a menubar. For instance, say you have a menu .m1 with a cascade entry
     * for .m2, where .m2 does not exist yet. You then put .m1 into a menubar.
     * This creates a menubar instance for .m1, but since .m2 is not there,
     * nothing else happens. When we go to create .m2, we hook it up properly
     * with .m1. However, we now need to clone .m2 and assign the clone of .m2
     * to be the cascade entry for the clone of .m1. This is special case
     * #1 listed in the introductory comment.
     */
    
    if (menuRefPtr->parentEntryPtr != NULL) {
        TkMenuEntry *cascadeListPtr = menuRefPtr->parentEntryPtr;
        TkMenuEntry *nextCascadePtr;
        char *newMenuName;
        char *newArgv[2];

        while (cascadeListPtr != NULL) {

	    nextCascadePtr = cascadeListPtr->nextCascadePtr;
     
     	    /*
     	     * If we have a new master menu, and an existing cloned menu
	     * points to this menu in a cascade entry, we have to clone
	     * the new menu and point the entry to the clone instead
	     * of the menu we are creating. Otherwise, ConfigureMenuEntry
	     * will hook up the platform-specific cascade linkages now
	     * that the menu we are creating exists.
     	     */
     	     
     	    if ((menuPtr->masterMenuPtr != menuPtr)
     	    	    || ((menuPtr->masterMenuPtr == menuPtr)
     	    	    && ((cascadeListPtr->menuPtr->masterMenuPtr
		    == cascadeListPtr->menuPtr)))) {
		newArgv[0] = "-menu";
		newArgv[1] = Tk_PathName(menuPtr->tkwin);
     	    	ConfigureMenuEntry(cascadeListPtr, 2, newArgv,
     	    	    TK_CONFIG_ARGV_ONLY);
     	    } else {
      	    	newMenuName = TkNewMenuName(menuPtr->interp,
     	    		Tk_PathName(cascadeListPtr->menuPtr->tkwin),
     	    		menuPtr);
            	CloneMenu(menuPtr, newMenuName, "normal");
    	            
                /*
                 * Now we can set the new menu instance to be the cascade entry
                 * of the parent's instance.
                 */

		newArgv[0] = "-menu";
                newArgv[1] = newMenuName;
                ConfigureMenuEntry(cascadeListPtr, 2, newArgv, 
                	TK_CONFIG_ARGV_ONLY);
	        if (newMenuName != NULL) {
	            ckfree(newMenuName);
	        }
            }
            cascadeListPtr = nextCascadePtr;
        }
    }
    
    /*
     * If there already exist toplevel widgets that refer to this menu,
     * find them and notify them so that they can reconfigure their
     * geometry to reflect the menu.
     */
 
    if (menuRefPtr->topLevelListPtr != NULL) {
    	TkMenuTopLevelList *topLevelListPtr = menuRefPtr->topLevelListPtr;
    	TkMenuTopLevelList *nextPtr;
    	Tk_Window listtkwin;
   	while (topLevelListPtr != NULL) {
    	
    	    /*
    	     * Need to get the next pointer first. TkSetWindowMenuBar
    	     * changes the list, so that the next pointer is different
    	     * after calling it.
    	     */
    	
    	    nextPtr = topLevelListPtr->nextPtr;
    	    listtkwin = topLevelListPtr->tkwin;
    	    TkSetWindowMenuBar(menuPtr->interp, listtkwin, 
    	    	    Tk_PathName(menuPtr->tkwin), Tk_PathName(menuPtr->tkwin));
    	    topLevelListPtr = nextPtr;
    	}
    }

    interp->result = Tk_PathName(menuPtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(menuPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * MenuWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
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
MenuWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about menu widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register TkMenu *menuPtr = (TkMenu *) clientData;
    register TkMenuEntry *mePtr;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) menuPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)
	    && (length >= 2)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate index\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (menuPtr->active == index) {
	    goto done;
	}
	if (index >= 0) {
	    if ((menuPtr->entries[index]->type == SEPARATOR_ENTRY)
		    || (menuPtr->entries[index]->state == tkDisabledUid)) {
		index = -1;
	    }
	}
	result = TkActivateMenuEntry(menuPtr, index);
    } else if ((c == 'a') && (strncmp(argv[1], "add", length) == 0)
	    && (length >= 2)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " add type ?options?\"", (char *) NULL);
	    goto error;
	}
	if (MenuAddOrInsert(interp, menuPtr, (char *) NULL,
		argc-2, argv+2) != TCL_OK) {
	    goto error;
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, menuPtr->tkwin, tkMenuConfigSpecs,
		(char *) menuPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "clone", length) == 0)
    	    && (length >=2)) {
    	if ((argc < 3) || (argc > 4)) {
    	    Tcl_AppendResult(interp, "wrong # args: should be \"",
    	    	    argv[0], " clone newMenuName ?menuType?\"",
    	    	    (char *) NULL);
    	    goto error;
    	}
    	result = CloneMenu(menuPtr, argv[2], (argc == 3) ? NULL : argv[3]);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, menuPtr->tkwin,
		    tkMenuConfigSpecs, (char *) menuPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, menuPtr->tkwin,
		    tkMenuConfigSpecs, (char *) menuPtr, argv[2], 0);
	} else {
	    result = ConfigureMenu(interp, menuPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	int first, last;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete first ?last?\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 3) {
	    last = first;
	} else {
	    if (TkGetMenuIndex(interp, menuPtr, argv[3], 0, &last) != TCL_OK) {
	        goto error;
	    }
	}
	if (menuPtr->tearOff && (first == 0)) {

	    /*
	     * Sorry, can't delete the tearoff entry;  must reconfigure
	     * the menu.
	     */
	    
	    first = 1;
	}
	if ((first < 0) || (last < first)) {
	    goto done;
	}
	DeleteMenuCloneEntries(menuPtr, first, last);
    } else if ((c == 'e') && (length >= 7)
	    && (strncmp(argv[1], "entrycget", length) == 0)) {
	int index;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " entrycget index option\"",
		    (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	Tcl_Preserve((ClientData) mePtr);
	result = Tk_ConfigureValue(interp, menuPtr->tkwin,
		tkMenuEntryConfigSpecs, (char *) mePtr, argv[3],
		COMMAND_MASK << mePtr->type);
	Tcl_Release((ClientData) mePtr);
    } else if ((c == 'e') && (length >= 7)
	    && (strncmp(argv[1], "entryconfigure", length) == 0)) {
	int index;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " entryconfigure index ?option value ...?\"",
		    (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	Tcl_Preserve((ClientData) mePtr);
	if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, menuPtr->tkwin,
		    tkMenuEntryConfigSpecs, (char *) mePtr, (char *) NULL,
		    COMMAND_MASK << mePtr->type);
	} else if (argc == 4) {
	    result = Tk_ConfigureInfo(interp, menuPtr->tkwin,
		    tkMenuEntryConfigSpecs, (char *) mePtr, argv[3],
		    COMMAND_MASK << mePtr->type);
	} else {
	    result = ConfigureMenuCloneEntries(interp, menuPtr, index, 
	    	    argc-3, argv+3, 
	    	    TK_CONFIG_ARGV_ONLY | COMMAND_MASK << mePtr->type);
	}
	Tcl_Release((ClientData) mePtr);
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index string\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    interp->result = "none";
	} else {
	    sprintf(interp->result, "%d", index);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert index type ?options?\"", (char *) NULL);
	    goto error;
	}
	if (MenuAddOrInsert(interp, menuPtr, argv[2],
		argc-3, argv+3) != TCL_OK) {
	    goto error;
	}
    } else if ((c == 'i') && (strncmp(argv[1], "invoke", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " invoke index\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	result = TkInvokeMenu(interp, menuPtr, index);
    } else if ((c == 'p') && (strncmp(argv[1], "post", length) == 0)
	    && (length == 4)) {
	int x, y;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " post x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}

	/*
	 * Tearoff menus are posted differently on Mac and Windows than
	 * non-tearoffs. TkpPostMenu does not actually map the menu's
	 * window on those platforms, and popup menus have to be
	 * handled specially.
	 */
	
    	if (menuPtr->menuType != TEAROFF_MENU) {
    	    result = TkpPostMenu(interp, menuPtr, x, y);
    	} else {
    	    result = TkPostTearoffMenu(interp, menuPtr, x, y);
    	}
    } else if ((c == 'p') && (strncmp(argv[1], "postcascade", length) == 0)
	    && (length > 4)) {
	int index;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " postcascade index\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if ((index < 0) || (menuPtr->entries[index]->type != CASCADE_ENTRY)) {
	    result = TkPostSubmenu(interp, menuPtr, (TkMenuEntry *) NULL);
	} else {
	    result = TkPostSubmenu(interp, menuPtr, menuPtr->entries[index]);
	}
    } else if ((c == 't') && (strncmp(argv[1], "type", length) == 0)) {
	int index;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " type index\"", (char *) NULL);
	    goto error;
	}
	if (TkGetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	switch (mePtr->type) {
	    case COMMAND_ENTRY:
		interp->result = "command";
		break;
	    case SEPARATOR_ENTRY:
		interp->result = "separator";
		break;
	    case CHECK_BUTTON_ENTRY:
		interp->result = "checkbutton";
		break;
	    case RADIO_BUTTON_ENTRY:
		interp->result = "radiobutton";
		break;
	    case CASCADE_ENTRY:
		interp->result = "cascade";
		break;
	    case TEAROFF_ENTRY:
		interp->result = "tearoff";
		break;
	}
    } else if ((c == 'u') && (strncmp(argv[1], "unpost", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " unpost\"", (char *) NULL);
	    goto error;
	}
	Tk_UnmapWindow(menuPtr->tkwin);
	result = TkPostSubmenu(interp, menuPtr, (TkMenuEntry *) NULL);
    } else if ((c == 'y') && (strncmp(argv[1], "yposition", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " yposition index\"", (char *) NULL);
	    goto error;
	}
	result = MenuDoYPosition(interp, menuPtr, argv[2]);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, add, cget, clone, configure, delete, ",
		"entrycget, entryconfigure, index, insert, invoke, ",
		"post, postcascade, type, unpost, or yposition",
		(char *) NULL);
	goto error;
    }
    done:
    Tcl_Release((ClientData) menuPtr);
    return result;

    error:
    Tcl_Release((ClientData) menuPtr);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * TkInvokeMenu --
 *
 *	Given a menu and an index, takes the appropriate action for the
 *	entry associated with that index.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *----------------------------------------------------------------------
 */

int
TkInvokeMenu(interp, menuPtr, index)
    Tcl_Interp *interp;		/* The interp that the menu lives in. */
    TkMenu *menuPtr;		/* The menu we are invoking. */
    int index;			/* The zero based index of the item we
    				 * are invoking */
{
    int result = TCL_OK;
    TkMenuEntry *mePtr;
    
    if (index < 0) {
    	goto done;
    }
    mePtr = menuPtr->entries[index];
    if (mePtr->state == tkDisabledUid) {
	goto done;
    }
    Tcl_Preserve((ClientData) mePtr);
    if (mePtr->type == TEAROFF_ENTRY) {
    	Tcl_DString commandDString;
    	
    	Tcl_DStringInit(&commandDString);
    	Tcl_DStringAppendElement(&commandDString, "tkTearOffMenu");
    	Tcl_DStringAppendElement(&commandDString, Tk_PathName(menuPtr->tkwin));
    	result = Tcl_Eval(interp, Tcl_DStringValue(&commandDString));
    	Tcl_DStringFree(&commandDString);
    } else if (mePtr->type == CHECK_BUTTON_ENTRY) {
	if (mePtr->entryFlags & ENTRY_SELECTED) {
	    if (Tcl_SetVar(interp, mePtr->name, mePtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	} else {
	    if (Tcl_SetVar(interp, mePtr->name, mePtr->onValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
    } else if (mePtr->type == RADIO_BUTTON_ENTRY) {
	if (Tcl_SetVar(interp, mePtr->name, mePtr->onValue,
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    result = TCL_ERROR;
	}
    }
    if ((result == TCL_OK) && (mePtr->command != NULL)) {
	result = TkCopyAndGlobalEval(interp, mePtr->command);
    }
    Tcl_Release((ClientData) mePtr);
    done:
    return result; 
}



/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuInstance --
 *
 *	This procedure is invoked by TkDestroyMenu
 *	to clean up the internal structure of a menu at a safe time
 *	(when no-one is using it anymore). Only takes care of one instance
 *	of the menu.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the menu is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuInstance(menuPtr)
    TkMenu *menuPtr;	/* Info about menu widget. */
{
    int i, numEntries = menuPtr->numEntries;
    TkMenu *menuInstancePtr;
    TkMenuEntry *cascadePtr, *nextCascadePtr;
    char *newArgv[2];
    TkMenu *parentMasterMenuPtr;
    TkMenuEntry *parentMasterEntryPtr;
    TkMenu *parentMenuPtr;
    
    /*
     * If the menu has any cascade menu entries pointing to it, the cascade
     * entries need to be told that the menu is going away. We need to clear
     * the menu ptr field in the menu reference at this point in the code
     * so that everything else can forget about this menu properly. We also
     * need to reset -menu field of all entries that are not master menus
     * back to this entry name if this is a master menu pointed to by another
     * master menu. If there is a clone menu that points to this menu,
     * then this menu is itself a clone, so when this menu goes away,
     * the -menu field of the pointing entry must be set back to this
     * menu's master menu name so that later if another menu is created
     * the cascade hierarchy can be maintained.
     */

    TkpDestroyMenu(menuPtr);
    cascadePtr = menuPtr->menuRefPtr->parentEntryPtr;
    menuPtr->menuRefPtr->menuPtr = NULL;
    TkFreeMenuReferences(menuPtr->menuRefPtr);

    for (; cascadePtr != NULL; cascadePtr = nextCascadePtr) {
    	parentMenuPtr = cascadePtr->menuPtr;
    	nextCascadePtr = cascadePtr->nextCascadePtr;
    	
    	if (menuPtr->masterMenuPtr != menuPtr) {
	    parentMasterMenuPtr = cascadePtr->menuPtr->masterMenuPtr;
	    parentMasterEntryPtr =
		    parentMasterMenuPtr->entries[cascadePtr->index];
	    newArgv[0] = "-menu";
	    newArgv[1] = parentMasterEntryPtr->name;
    	    ConfigureMenuEntry(cascadePtr, 2, newArgv, TK_CONFIG_ARGV_ONLY);
    	} else {
    	    ConfigureMenuEntry(cascadePtr, 0, (char **) NULL, 0);
    	}
    }
    
    if (menuPtr->masterMenuPtr != menuPtr) {
        for (menuInstancePtr = menuPtr->masterMenuPtr; 
        	menuInstancePtr != NULL;
        	menuInstancePtr = menuInstancePtr->nextInstancePtr) {
            if (menuInstancePtr->nextInstancePtr == menuPtr) {
                menuInstancePtr->nextInstancePtr = 
                	menuInstancePtr->nextInstancePtr->nextInstancePtr;
                break;
            }
        }
   } else if (menuPtr->nextInstancePtr != NULL) {
       panic("Attempting to delete master menu when there are still clones.");
   }

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    for (i = numEntries - 1; i >= 0; i--) {
	DestroyMenuEntry((char *) menuPtr->entries[i]);
    }
    if (menuPtr->entries != NULL) {
	ckfree((char *) menuPtr->entries);
    }
    TkMenuFreeDrawOptions(menuPtr);
    Tk_FreeOptions(tkMenuConfigSpecs, (char *) menuPtr, menuPtr->display, 0);

    Tcl_EventuallyFree((ClientData) menuPtr, TCL_DYNAMIC);
}

/*
 *----------------------------------------------------------------------
 *
 * TkDestroyMenu --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a menu at a safe time
 *	(when no-one is using it anymore).  If called on a master instance,
 *	destroys all of the slave instances. If called on a non-master
 *	instance, just destroys that instance.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the menu is freed up.
 *
 *----------------------------------------------------------------------
 */

void
TkDestroyMenu(menuPtr)
    TkMenu *menuPtr;	/* Info about menu widget. */
{
    TkMenu *menuInstancePtr;
    TkMenuTopLevelList *topLevelListPtr, *nextTopLevelPtr;

    if (menuPtr->menuFlags & MENU_DELETION_PENDING) {
    	return;
    }
    
    /*
     * Now destroy all non-tearoff instances of this menu if this is a 
     * parent menu. Is this loop safe enough? Are there going to be
     * destroy bindings on child menus which kill the parent? If not,
     * we have to do a slightly more complex scheme.
     */
    
    if (menuPtr->masterMenuPtr == menuPtr) {
    	menuPtr->menuFlags |= MENU_DELETION_PENDING;
	while (menuPtr->nextInstancePtr != NULL) {
	    menuInstancePtr = menuPtr->nextInstancePtr;
	    menuPtr->nextInstancePtr = menuInstancePtr->nextInstancePtr;
    	    if (menuInstancePtr->tkwin != NULL) {
	     	Tk_DestroyWindow(menuInstancePtr->tkwin);
	    }
	}
    	menuPtr->menuFlags &= ~MENU_DELETION_PENDING;
    }

    /*
     * If any toplevel widgets have this menu as their menubar,
     * the geometry of the window may have to be recalculated.
     */
    
    topLevelListPtr = menuPtr->menuRefPtr->topLevelListPtr;
    while (topLevelListPtr != NULL) {
         nextTopLevelPtr = topLevelListPtr->nextPtr;
         TkpSetWindowMenuBar(topLevelListPtr->tkwin, NULL);
    	 topLevelListPtr = nextTopLevelPtr;
    }   
    DestroyMenuInstance(menuPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * UnhookCascadeEntry --
 *
 *	This entry is removed from the list of entries that point to the
 *	cascade menu. This is done in preparation for changing the menu
 *	that this entry points to.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The appropriate lists are modified.
 *
 *----------------------------------------------------------------------
 */

static void
UnhookCascadeEntry(mePtr)
    TkMenuEntry *mePtr;			/* The cascade entry we are removing
					 * from the cascade list. */
{
    TkMenuEntry *cascadeEntryPtr;
    TkMenuEntry *prevCascadePtr;
    TkMenuReferences *menuRefPtr;

    menuRefPtr = mePtr->childMenuRefPtr;
    if (menuRefPtr == NULL) {
        return;
    }
    
    cascadeEntryPtr = menuRefPtr->parentEntryPtr;
    if (cascadeEntryPtr == NULL) {
    	return;
    }
    
    /*
     * Singularly linked list deletion. The two special cases are
     * 1. one element; 2. The first element is the one we want.
     */
 
    if (cascadeEntryPtr == mePtr) {
    	if (cascadeEntryPtr->nextCascadePtr == NULL) {

	    /*
	     * This is the last menu entry which points to this
	     * menu, so we need to clear out the list pointer in the
	     * cascade itself.
	     */
	
	    menuRefPtr->parentEntryPtr = NULL;
	    TkFreeMenuReferences(menuRefPtr);
    	} else {
    	    menuRefPtr->parentEntryPtr = cascadeEntryPtr->nextCascadePtr;
    	}
    	mePtr->nextCascadePtr = NULL;
    } else {
	for (prevCascadePtr = cascadeEntryPtr,
		cascadeEntryPtr = cascadeEntryPtr->nextCascadePtr;
		cascadeEntryPtr != NULL;
	        prevCascadePtr = cascadeEntryPtr,
		cascadeEntryPtr = cascadeEntryPtr->nextCascadePtr) {
    	    if (cascadeEntryPtr == mePtr){
    	    	prevCascadePtr->nextCascadePtr =
            	    	cascadeEntryPtr->nextCascadePtr;
    	    	cascadeEntryPtr->nextCascadePtr = NULL;
    	    	break;
    	    }
        }
    }
    mePtr->childMenuRefPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuEntry --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a menu entry at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the menu entry is freed.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuEntry(memPtr)
    char *memPtr;		/* Pointer to entry to be freed. */
{
    register TkMenuEntry *mePtr = (TkMenuEntry *) memPtr;
    TkMenu *menuPtr = mePtr->menuPtr;

    if (menuPtr->postedCascade == mePtr) {
	
    	/*
	 * Ignore errors while unposting the menu, since it's possible
	 * that the menu has already been deleted and the unpost will
	 * generate an error.
	 */

	TkPostSubmenu(menuPtr->interp, menuPtr, (TkMenuEntry *) NULL);
    }

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (mePtr->type == CASCADE_ENTRY) {
        UnhookCascadeEntry(mePtr);
    }
    if (mePtr->image != NULL) {
	Tk_FreeImage(mePtr->image);
    }
    if (mePtr->selectImage != NULL) {
	Tk_FreeImage(mePtr->selectImage);
    }
    if (mePtr->name != NULL) {
	Tcl_UntraceVar(menuPtr->interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }
    TkpDestroyMenuEntry(mePtr);
    TkMenuEntryFreeDrawOptions(mePtr);
    Tk_FreeOptions(tkMenuEntryConfigSpecs, (char *) mePtr, menuPtr->display, 
	    (COMMAND_MASK << mePtr->type));
    ckfree((char *) mePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * MenuWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way (such as the fonts in the system changing) and the widget needs
 *	to recompute all its graphics contexts and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Menu will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
static void
MenuWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    TkMenu *menuPtr = (TkMenu *) instanceData;
    int i;
    
    TkMenuConfigureDrawOptions(menuPtr);
    for (i = 0; i < menuPtr->numEntries; i++) {
    	TkMenuConfigureEntryDrawOptions(menuPtr->entries[i],
		menuPtr->entries[i]->index);
	TkpConfigureMenuEntry(menuPtr->entries[i]);	
    }
}


/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenu --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a menu widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, font, etc. get set
 *	for menuPtr;  old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenu(interp, menuPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register TkMenu *menuPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    int i;
    TkMenu* menuListPtr;
    
    for (menuListPtr = menuPtr->masterMenuPtr; menuListPtr != NULL;
	    menuListPtr = menuListPtr->nextInstancePtr) {
    
	if (Tk_ConfigureWidget(interp, menuListPtr->tkwin,
		tkMenuConfigSpecs, argc, argv, (char *) menuListPtr,
		flags) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * When a menu is created, the type is in all of the arguments
	 * to the menu command. Let Tk_ConfigureWidget take care of
	 * parsing them, and then set the type after we can look at
	 * the type string. Once set, a menu's type cannot be changed
	 */
	
	if (menuListPtr->menuType == UNKNOWN_TYPE) {
	    if (strcmp(menuListPtr->menuTypeName, "menubar") == 0) {
	    	menuListPtr->menuType = MENUBAR;
	    } else if (strcmp(menuListPtr->menuTypeName, "tearoff") == 0) {
	    	menuListPtr->menuType = TEAROFF_MENU;
	    } else {
	    	menuListPtr->menuType = MASTER_MENU;
	    }
	}
	
	/*
	 * Depending on the -tearOff option, make sure that there is or
	 * isn't an initial tear-off entry at the beginning of the menu.
	 */
	
	if (menuListPtr->tearOff) {
	    if ((menuListPtr->numEntries == 0)
		    || (menuListPtr->entries[0]->type != TEAROFF_ENTRY)) {
		if (MenuNewEntry(menuListPtr, 0, TEAROFF_ENTRY) == NULL) {
		    return TCL_ERROR;
		}
	    }
	} else if ((menuListPtr->numEntries > 0)
		&& (menuListPtr->entries[0]->type == TEAROFF_ENTRY)) {
	    int i;

	    Tcl_EventuallyFree((ClientData) menuListPtr->entries[0],
	    	    DestroyMenuEntry);
	    for (i = 0; i < menuListPtr->numEntries - 1; i++) {
		menuListPtr->entries[i] = menuListPtr->entries[i + 1];
		menuListPtr->entries[i]->index = i;
	    }
	    menuListPtr->numEntries--;
	    if (menuListPtr->numEntries == 0) {
		ckfree((char *) menuListPtr->entries);
		menuListPtr->entries = NULL;
	    }
	}

	TkMenuConfigureDrawOptions(menuListPtr);

	/*
	 * Configure the new window to be either a pop-up menu
	 * or a tear-off menu.
	 * We don't do this for menubars since they are not toplevel
	 * windows. Also, since this gets called before CloneMenu has
	 * a chance to set the menuType field, we have to look at the
	 * menuTypeName field to tell that this is a menu bar.
	 */
	
	if (strcmp(menuListPtr->menuTypeName, "normal") == 0) {
	    TkpMakeMenuWindow(menuListPtr->tkwin, 1);
	} else if (strcmp(menuListPtr->menuTypeName, "tearoff") == 0) {
	    TkpMakeMenuWindow(menuListPtr->tkwin, 0);
	}
	
	/*
	 * After reconfiguring a menu, we need to reconfigure all of the
	 * entries in the menu, since some of the things in the children
	 * (such as graphics contexts) may have to change to reflect changes
	 * in the parent.
	 */
	
	for (i = 0; i < menuListPtr->numEntries; i++) {
	    TkMenuEntry *mePtr;
	
	    mePtr = menuListPtr->entries[i];
	    ConfigureMenuEntry(mePtr, 0,
	    	    (char **) NULL, TK_CONFIG_ARGV_ONLY 
	    	    | COMMAND_MASK << mePtr->type);
	}
	
	TkEventuallyRecomputeMenu(menuListPtr);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenuEntry --
 *
 *	This procedure is called to process an argv/argc list in order
 *	to configure (or reconfigure) one entry in a menu.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information such as label and accelerator get
 *	set for mePtr;  old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenuEntry(mePtr, argc, argv, flags)
    register TkMenuEntry *mePtr;		/* Information about menu entry;  may
					 * or may not already have values for
					 * some fields. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Additional flags to pass to
					 * Tk_ConfigureWidget. */
{
    TkMenu *menuPtr = mePtr->menuPtr;
    int index = mePtr->index;
    Tk_Image image;

    /*
     * If this entry is a check button or radio button, then remove
     * its old trace procedure.
     */

    if ((mePtr->name != NULL)
    	    && ((mePtr->type == CHECK_BUTTON_ENTRY)
	    || (mePtr->type == RADIO_BUTTON_ENTRY))) {
	Tcl_UntraceVar(menuPtr->interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }
    
    if (menuPtr->tkwin != NULL) {
	if (Tk_ConfigureWidget(menuPtr->interp, menuPtr->tkwin, 
		tkMenuEntryConfigSpecs, argc, argv, (char *) mePtr,
		flags | (COMMAND_MASK << mePtr->type)) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * The code below handles special configuration stuff not taken
     * care of by Tk_ConfigureWidget, such as special processing for
     * defaults, sizing strings, graphics contexts, etc.
     */

    if (mePtr->label == NULL) {
	mePtr->labelLength = 0;
    } else {
	mePtr->labelLength = strlen(mePtr->label);
    }
    if (mePtr->accel == NULL) {
	mePtr->accelLength = 0;
    } else {
	mePtr->accelLength = strlen(mePtr->accel);
    }

    /*
     * If this is a cascade entry, the platform-specific data of the child
     * menu has to be updated. Also, the links that point to parents and
     * cascades have to be updated.
     */

    if ((mePtr->type == CASCADE_ENTRY) && (mePtr->name != NULL)) {
 	TkMenuEntry *cascadeEntryPtr;
 	TkMenu *cascadeMenuPtr;
	int alreadyThere;
	TkMenuReferences *menuRefPtr;
	char *oldHashKey = NULL;	/* Initialization only needed to
					 * prevent compiler warning. */

	/*
	 * This is a cascade entry. If the menu that the cascade entry
	 * is pointing to has changed, we need to remove this entry
	 * from the list of entries pointing to the old menu, and add a
	 * cascade reference to the list of entries pointing to the
	 * new menu.
	 *
	 * BUG: We are not recloning for special case #3 yet.
	 */
	
	if (mePtr->childMenuRefPtr != NULL) {
	    oldHashKey = Tcl_GetHashKey(TkGetMenuHashTable(menuPtr->interp),
		    mePtr->childMenuRefPtr->hashEntryPtr);
	    if (strcmp(oldHashKey, mePtr->name) != 0) {
		UnhookCascadeEntry(mePtr);
	    }
	}

	if ((mePtr->childMenuRefPtr == NULL) 
		|| (strcmp(oldHashKey, mePtr->name) != 0)) {
	    menuRefPtr = TkCreateMenuReferences(menuPtr->interp,
		    mePtr->name);
	    cascadeMenuPtr = menuRefPtr->menuPtr;
	    mePtr->childMenuRefPtr = menuRefPtr;

	    if (menuRefPtr->parentEntryPtr == NULL) {
		menuRefPtr->parentEntryPtr = mePtr;
	    } else {
		alreadyThere = 0;
		for (cascadeEntryPtr = menuRefPtr->parentEntryPtr;
			cascadeEntryPtr != NULL;
			cascadeEntryPtr =
			cascadeEntryPtr->nextCascadePtr) {
		    if (cascadeEntryPtr == mePtr) {
			alreadyThere = 1;
			break;
		    }
		}
    
		/*
		 * Put the item at the front of the list.
		 */
	    
		if (!alreadyThere) {
		    mePtr->nextCascadePtr = menuRefPtr->parentEntryPtr;
		    menuRefPtr->parentEntryPtr = mePtr;
		}
	    }
	}
    }
    
    if (TkMenuConfigureEntryDrawOptions(mePtr, index) != TCL_OK) {
    	return TCL_ERROR;
    }

    if (TkpConfigureMenuEntry(mePtr) != TCL_OK) {
    	return TCL_ERROR;
    }
    
    if ((mePtr->type == CHECK_BUTTON_ENTRY)
	    || (mePtr->type == RADIO_BUTTON_ENTRY)) {
	char *value;

	if (mePtr->name == NULL) {
	    mePtr->name =
		    (char *) ckalloc((unsigned) (mePtr->labelLength + 1));
	    strcpy(mePtr->name, (mePtr->label == NULL) ? "" : mePtr->label);
	}
	if (mePtr->onValue == NULL) {
	    mePtr->onValue = (char *) ckalloc((unsigned)
		    (mePtr->labelLength + 1));
	    strcpy(mePtr->onValue, (mePtr->label == NULL) ? "" : mePtr->label);
	}

	/*
	 * Select the entry if the associated variable has the
	 * appropriate value, initialize the variable if it doesn't
	 * exist, then set a trace on the variable to monitor future
	 * changes to its value.
	 */

	value = Tcl_GetVar(menuPtr->interp, mePtr->name, TCL_GLOBAL_ONLY);
	mePtr->entryFlags &= ~ENTRY_SELECTED;
	if (value != NULL) {
	    if (strcmp(value, mePtr->onValue) == 0) {
		mePtr->entryFlags |= ENTRY_SELECTED;
	    }
	} else {
	    Tcl_SetVar(menuPtr->interp, mePtr->name,
		    (mePtr->type == CHECK_BUTTON_ENTRY) ? mePtr->offValue : "",
		    TCL_GLOBAL_ONLY);
	}
	Tcl_TraceVar(menuPtr->interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }

    /*
     * Get the images for the entry, if there are any.  Allocate the
     * new images before freeing the old ones, so that the reference
     * counts don't go to zero and cause image data to be discarded.
     */

    if (mePtr->imageString != NULL) {
	image = Tk_GetImage(menuPtr->interp, menuPtr->tkwin, mePtr->imageString,
		TkMenuImageProc, (ClientData) mePtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (mePtr->image != NULL) {
	Tk_FreeImage(mePtr->image);
    }
    mePtr->image = image;
    if (mePtr->selectImageString != NULL) {
	image = Tk_GetImage(menuPtr->interp, menuPtr->tkwin, mePtr->selectImageString,
		TkMenuSelectImageProc, (ClientData) mePtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (mePtr->selectImage != NULL) {
	Tk_FreeImage(mePtr->selectImage);
    }
    mePtr->selectImage = image;

    TkEventuallyRecomputeMenu(menuPtr);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenuCloneEntries --
 *
 *	Calls ConfigureMenuEntry for each menu in the clone chain.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information such as label and accelerator get
 *	set for mePtr;  old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenuCloneEntries(interp, menuPtr, index, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    TkMenu *menuPtr;			/* Information about whole menu. */
    int index;				/* Index of mePtr within menuPtr's
					 * entries. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Additional flags to pass to
					 * Tk_ConfigureWidget. */
{
    TkMenuEntry *mePtr;
    TkMenu *menuListPtr;
    char *oldCascadeName = NULL, *newMenuName = NULL;
    int cascadeEntryChanged;
    TkMenuReferences *oldCascadeMenuRefPtr, *cascadeMenuRefPtr = NULL; 
    
    /*
     * Cascades are kind of tricky here. This is special case #3 in the comment
     * at the top of this file. Basically, if a menu is the master menu of a
     * clone chain, and has an entry with a cascade menu, the clones of
     * the menu will point to clones of the cascade menu. We have
     * to destroy the clones of the cascades, clone the new cascade
     * menu, and configure the entry to point to the new clone.
     */

    mePtr = menuPtr->masterMenuPtr->entries[index];
    if (mePtr->type == CASCADE_ENTRY) {
	oldCascadeName = mePtr->name;
    }

    if (ConfigureMenuEntry(mePtr, argc, argv, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    cascadeEntryChanged = (mePtr->type == CASCADE_ENTRY)
	    && (oldCascadeName != mePtr->name);

    if (cascadeEntryChanged) {
	newMenuName = mePtr->name;
	if (newMenuName != NULL) {
	    cascadeMenuRefPtr = TkFindMenuReferences(menuPtr->interp,
		    mePtr->name);
	}
    }

    for (menuListPtr = menuPtr->masterMenuPtr->nextInstancePtr; 
    	    menuListPtr != NULL;
	    menuListPtr = menuListPtr->nextInstancePtr) {
  	
    	mePtr = menuListPtr->entries[index];

	if (cascadeEntryChanged && (mePtr->name != NULL)) {
	    oldCascadeMenuRefPtr = TkFindMenuReferences(menuPtr->interp, 
		    mePtr->name);

	    if ((oldCascadeMenuRefPtr != NULL)
		    && (oldCascadeMenuRefPtr->menuPtr != NULL)) {
		RecursivelyDeleteMenu(oldCascadeMenuRefPtr->menuPtr);
	    }
	}

    	if (ConfigureMenuEntry(mePtr, argc, argv, flags) != TCL_OK) {
    	    return TCL_ERROR;
    	}
	
	if (cascadeEntryChanged && (newMenuName != NULL)) {
	    if (cascadeMenuRefPtr->menuPtr != NULL) {
		char *newArgV[2];
		char *newCloneName;

		newCloneName = TkNewMenuName(menuPtr->interp,
			Tk_PathName(menuListPtr->tkwin), 
			cascadeMenuRefPtr->menuPtr);
		CloneMenu(cascadeMenuRefPtr->menuPtr, newCloneName,
			"normal");

		newArgV[0] = "-menu";
		newArgV[1] = newCloneName;
		ConfigureMenuEntry(mePtr, 2, newArgV, flags);
		ckfree(newCloneName);
	    }
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkGetMenuIndex --
 *
 *	Parse a textual index into a menu and return the numerical
 *	index of the indicated entry.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the entry index corresponding to string
 *	(ranges from -1 to the number of entries in the menu minus
 *	one).  Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkGetMenuIndex(interp, menuPtr, string, lastOK, indexPtr)
    Tcl_Interp *interp;		/* For error messages. */
    TkMenu *menuPtr;		/* Menu for which the index is being
				 * specified. */
    char *string;		/* Specification of an entry in menu.  See
				 * manual entry for valid .*/
    int lastOK;			/* Non-zero means its OK to return index
				 * just *after* last entry. */
    int *indexPtr;		/* Where to store converted relief. */
{
    int i;

    if ((string[0] == 'a') && (strcmp(string, "active") == 0)) {
	*indexPtr = menuPtr->active;
	return TCL_OK;
    }

    if (((string[0] == 'l') && (strcmp(string, "last") == 0))
	    || ((string[0] == 'e') && (strcmp(string, "end") == 0))) {
	*indexPtr = menuPtr->numEntries - ((lastOK) ? 0 : 1);
	return TCL_OK;
    }

    if ((string[0] == 'n') && (strcmp(string, "none") == 0)) {
	*indexPtr = -1;
	return TCL_OK;
    }

    if (string[0] == '@') {
	if (GetIndexFromCoords(interp, menuPtr, string, indexPtr)
		== TCL_OK) {
	    return TCL_OK;
	}
    }

    if (isdigit(UCHAR(string[0]))) {
	if (Tcl_GetInt(interp, string,  &i) == TCL_OK) {
	    if (i >= menuPtr->numEntries) {
		if (lastOK) {
		    i = menuPtr->numEntries;
		} else {
		    i = menuPtr->numEntries-1;
		}
	    } else if (i < 0) {
		i = -1;
	    }
	    *indexPtr = i;
	    return TCL_OK;
	}
	Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
    }

    for (i = 0; i < menuPtr->numEntries; i++) {
	char *label;

	label = menuPtr->entries[i]->label;
	if ((label != NULL)
		&& (Tcl_StringMatch(menuPtr->entries[i]->label, string))) {
	    *indexPtr = i;
	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interp, "bad menu entry index \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MenuCmdDeletedProc --
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
MenuCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    TkMenu *menuPtr = (TkMenu *) clientData;
    Tk_Window tkwin = menuPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	menuPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MenuNewEntry --
 *
 *	This procedure allocates and initializes a new menu entry.
 *
 * Results:
 *	The return value is a pointer to a new menu entry structure,
 *	which has been malloc-ed, initialized, and entered into the
 *	entry array for the  menu.
 *
 * Side effects:
 *	Storage gets allocated.
 *
 *----------------------------------------------------------------------
 */

static TkMenuEntry *
MenuNewEntry(menuPtr, index, type)
    TkMenu *menuPtr;		/* Menu that will hold the new entry. */
    int index;			/* Where in the menu the new entry is to
				 * go. */
    int type;			/* The type of the new entry. */
{
    TkMenuEntry *mePtr;
    TkMenuEntry **newEntries;
    int i;

    /*
     * Create a new array of entries with an empty slot for the
     * new entry.
     */

    newEntries = (TkMenuEntry **) ckalloc((unsigned)
	    ((menuPtr->numEntries+1)*sizeof(TkMenuEntry *)));
    for (i = 0; i < index; i++) {
	newEntries[i] = menuPtr->entries[i];
    }
    for (  ; i < menuPtr->numEntries; i++) {
	newEntries[i+1] = menuPtr->entries[i];
	newEntries[i+1]->index = i + 1;
    }
    if (menuPtr->numEntries != 0) {
	ckfree((char *) menuPtr->entries);
    }
    menuPtr->entries = newEntries;
    menuPtr->numEntries++;
    mePtr = (TkMenuEntry *) ckalloc(sizeof(TkMenuEntry));
    menuPtr->entries[index] = mePtr;
    mePtr->type = type;
    mePtr->menuPtr = menuPtr;
    mePtr->label = NULL;
    mePtr->labelLength = 0;
    mePtr->underline = -1;
    mePtr->bitmap = None;
    mePtr->imageString = NULL;
    mePtr->image = NULL;
    mePtr->selectImageString  = NULL;
    mePtr->selectImage = NULL;
    mePtr->accel = NULL;
    mePtr->accelLength = 0;
    mePtr->state = tkNormalUid;
    mePtr->border = NULL;
    mePtr->fg = NULL;
    mePtr->activeBorder = NULL;
    mePtr->activeFg = NULL;
    mePtr->tkfont = NULL;
    mePtr->indicatorOn = 1;
    mePtr->indicatorFg = NULL;
    mePtr->columnBreak = 0;
    mePtr->hideMargin = 0;
    mePtr->command = NULL;
    mePtr->name = NULL;
    mePtr->childMenuRefPtr = NULL;
    mePtr->onValue = NULL;
    mePtr->offValue = NULL;
    mePtr->entryFlags = 0;
    mePtr->index = index;
    mePtr->nextCascadePtr = NULL;
    TkMenuInitializeEntryDrawingFields(mePtr);
    if (TkpMenuNewEntry(mePtr) != TCL_OK) {
    	ckfree((char *) mePtr);
    	return NULL;
    }
    
    return mePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MenuAddOrInsert --
 *
 *	This procedure does all of the work of the "add" and "insert"
 *	widget commands, allowing the code for these to be shared.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A new menu entry is created in menuPtr.
 *
 *----------------------------------------------------------------------
 */

static int
MenuAddOrInsert(interp, menuPtr, indexString, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    TkMenu *menuPtr;			/* Widget in which to create new
					 * entry. */
    char *indexString;			/* String describing index at which
					 * to insert.  NULL means insert at
					 * end. */
    int argc;				/* Number of elements in argv. */
    char **argv;			/* Arguments to command:  first arg
					 * is type of entry, others are
					 * config options. */
{
    int c, type, index;
    size_t length;
    TkMenuEntry *mePtr;
    TkMenu *menuListPtr;

    if (indexString != NULL) {
	if (TkGetMenuIndex(interp, menuPtr, indexString, 1, &index)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	index = menuPtr->numEntries;
    }
    if (index < 0) {
	Tcl_AppendResult(interp, "bad index \"", indexString, "\"",
		 (char *) NULL);
	return TCL_ERROR;
    }
    if (menuPtr->tearOff && (index == 0)) {
	index = 1;
    }

    /*
     * Figure out the type of the new entry.
     */

    c = argv[0][0];
    length = strlen(argv[0]);
    if ((c == 'c') && (strncmp(argv[0], "cascade", length) == 0)
	    && (length >= 2)) {
	type = CASCADE_ENTRY;
    } else if ((c == 'c') && (strncmp(argv[0], "checkbutton", length) == 0)
	    && (length >= 2)) {
	type = CHECK_BUTTON_ENTRY;
    } else if ((c == 'c') && (strncmp(argv[0], "command", length) == 0)
	    && (length >= 2)) {
	type = COMMAND_ENTRY;
    } else if ((c == 'r')
	    && (strncmp(argv[0], "radiobutton", length) == 0)) {
	type = RADIO_BUTTON_ENTRY;
    } else if ((c == 's')
	    && (strncmp(argv[0], "separator", length) == 0)) {
	type = SEPARATOR_ENTRY;
    } else {
	Tcl_AppendResult(interp, "bad menu entry type \"",
		argv[0], "\": must be cascade, checkbutton, ",
		"command, radiobutton, or separator", (char *) NULL);
	return TCL_ERROR;
    }
    
    /*
     * Now we have to add an entry for every instance related to this menu.
     */

    for (menuListPtr = menuPtr->masterMenuPtr; menuListPtr != NULL; 
    	    menuListPtr = menuListPtr->nextInstancePtr) {
    	
    	mePtr = MenuNewEntry(menuListPtr, index, type);
    	if (mePtr == NULL) {
    	    return TCL_ERROR;
    	}
    	if (ConfigureMenuEntry(mePtr, argc-1, argv+1, 0) != TCL_OK) {
	    TkMenu *errorMenuPtr;
	    int i; 

	    for (errorMenuPtr = menuPtr->masterMenuPtr;
		    errorMenuPtr != NULL;
		    errorMenuPtr = errorMenuPtr->nextInstancePtr) {
    		Tcl_EventuallyFree((ClientData) errorMenuPtr->entries[index],
    	    		DestroyMenuEntry);
		for (i = index; i < errorMenuPtr->numEntries - 1; i++) {
		    errorMenuPtr->entries[i] = errorMenuPtr->entries[i + 1];
		    errorMenuPtr->entries[i]->index = i;
		}
		errorMenuPtr->numEntries--;
		if (errorMenuPtr->numEntries == 0) {
		    ckfree((char *) errorMenuPtr->entries);
		    errorMenuPtr->entries = NULL;
		}
		if (errorMenuPtr == menuListPtr) {
		    break;
		}
	    }
    	    return TCL_ERROR;
    	}
    	
    	/*
    	 * If a menu has cascades, then every instance of the menu has
    	 * to have its own parallel cascade structure. So adding an
	 * entry to a menu with clones means that the menu that the
	 * entry points to has to be cloned for every clone the
	 * master menu has. This is special case #2 in the comment
	 * at the top of this file.
    	 */
 
    	if ((menuPtr != menuListPtr) && (type == CASCADE_ENTRY)) {    	    
    	    if ((mePtr->name != NULL)  && (mePtr->childMenuRefPtr != NULL)
    	    	    && (mePtr->childMenuRefPtr->menuPtr != NULL)) {
    	        TkMenu *cascadeMenuPtr =
			mePtr->childMenuRefPtr->menuPtr->masterMenuPtr;
    	        char *newCascadeName;
  		char *newArgv[2];
		TkMenuReferences *menuRefPtr;
    	            
		newCascadeName = TkNewMenuName(menuListPtr->interp,
			Tk_PathName(menuListPtr->tkwin),
			cascadeMenuPtr);
		CloneMenu(cascadeMenuPtr, newCascadeName, "normal");
		
		menuRefPtr = TkFindMenuReferences(menuListPtr->interp,
			newCascadeName);
		if (menuRefPtr == NULL) {
		    panic("CloneMenu failed inside of MenuAddOrInsert.");
		}
		newArgv[0] = "-menu";
		newArgv[1] = newCascadeName;
    	        ConfigureMenuEntry(mePtr, 2, newArgv, 0);
    	        ckfree(newCascadeName);
    	    }
    	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MenuVarProc --
 *
 *	This procedure is invoked when someone changes the
 *	state variable associated with a radiobutton or checkbutton
 *	menu entry.  The entry's selected state is set to match
 *	the value of the variable.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The menu entry may become selected or deselected.
 *
 *--------------------------------------------------------------
 */

static char *
MenuVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about menu entry. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* First part of variable's name. */
    char *name2;		/* Second part of variable's name. */
    int flags;			/* Describes what just happened. */
{
    TkMenuEntry *mePtr = (TkMenuEntry *) clientData;
    TkMenu *menuPtr;
    char *value;

    menuPtr = mePtr->menuPtr;

    /*
     * If the variable is being unset, then re-establish the
     * trace unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	mePtr->entryFlags &= ~ENTRY_SELECTED;
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, mePtr->name,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MenuVarProc, clientData);
	}
	TkpConfigureMenuEntry(mePtr);
	TkEventuallyRedrawMenu(menuPtr, (TkMenuEntry *) NULL);
	return (char *) NULL;
    }

    /*
     * Use the value of the variable to update the selected status of
     * the menu entry.
     */

    value = Tcl_GetVar(interp, mePtr->name, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (strcmp(value, mePtr->onValue) == 0) {
	if (mePtr->entryFlags & ENTRY_SELECTED) {
	    return (char *) NULL;
	}
	mePtr->entryFlags |= ENTRY_SELECTED;
    } else if (mePtr->entryFlags & ENTRY_SELECTED) {
	mePtr->entryFlags &= ~ENTRY_SELECTED;
    } else {
	return (char *) NULL;
    }
    TkpConfigureMenuEntry(mePtr);
    TkEventuallyRedrawMenu(menuPtr, mePtr);
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkActivateMenuEntry --
 *
 *	This procedure is invoked to make a particular menu entry
 *	the active one, deactivating any other entry that might
 *	currently be active.
 *
 * Results:
 *	The return value is a standard Tcl result (errors can occur
 *	while posting and unposting submenus).
 *
 * Side effects:
 *	Menu entries get redisplayed, and the active entry changes.
 *	Submenus may get posted and unposted.
 *
 *----------------------------------------------------------------------
 */

int
TkActivateMenuEntry(menuPtr, index)
    register TkMenu *menuPtr;		/* Menu in which to activate. */
    int index;				/* Index of entry to activate, or
					 * -1 to deactivate all entries. */
{
    register TkMenuEntry *mePtr;
    int result = TCL_OK;

    if (menuPtr->active >= 0) {
	mePtr = menuPtr->entries[menuPtr->active];

	/*
	 * Don't change the state unless it's currently active (state
	 * might already have been changed to disabled).
	 */

	if (mePtr->state == tkActiveUid) {
	    mePtr->state = tkNormalUid;
	}
	TkEventuallyRedrawMenu(menuPtr, menuPtr->entries[menuPtr->active]);
    }
    menuPtr->active = index;
    if (index >= 0) {
	mePtr = menuPtr->entries[index];
	mePtr->state = tkActiveUid;
	TkEventuallyRedrawMenu(menuPtr, mePtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkPostCommand --
 *
 *	Execute the postcommand for the given menu.
 *
 * Results:
 *	The return value is a standard Tcl result (errors can occur
 *	while the postcommands are being processed).
 *
 * Side effects:
 *	Since commands can get executed while this routine is being executed,
 *	the entire world can change.
 *
 *----------------------------------------------------------------------
 */
 
int
TkPostCommand(menuPtr)
    TkMenu *menuPtr;
{
    int result;

    /*
     * If there is a command for the menu, execute it.  This
     * may change the size of the menu, so be sure to recompute
     * the menu's geometry if needed.
     */

    if (menuPtr->postCommand != NULL) {
    	result = TkCopyAndGlobalEval(menuPtr->interp,
	        menuPtr->postCommand);
	if (result != TCL_OK) {
	    return result;
	}
	TkRecomputeMenu(menuPtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * CloneMenu --
 *
 *	Creates a child copy of the menu. It will be inserted into
 *	the menu's instance chain. All attributes and entry
 *	attributes will be duplicated.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Allocates storage. After the menu is created, any 
 *	configuration done with this menu or any related one
 *	will be reflected in all of them.
 *
 *--------------------------------------------------------------
 */

static int
CloneMenu(menuPtr, newMenuName, newMenuTypeString)
    TkMenu *menuPtr;		/* The menu we are going to clone */
    char *newMenuName;		/* The name to give the new menu */
    char *newMenuTypeString;	/* What kind of menu is this, a normal menu
    				 * a menubar, or a tearoff? */
{
    int returnResult;
    int menuType;
    size_t length;
    TkMenuReferences *menuRefPtr;
    Tcl_Obj *commandObjPtr;
    
    if (newMenuTypeString == NULL) {
    	menuType = MASTER_MENU;
    } else {
    	length = strlen(newMenuTypeString);
    	if (strncmp(newMenuTypeString, "normal", length) == 0) {
            menuType = MASTER_MENU;
    	} else if (strncmp(newMenuTypeString, "tearoff", length) == 0) {
            menuType = TEAROFF_MENU;
    	} else if (strncmp(newMenuTypeString, "menubar", length) == 0) {
            menuType = MENUBAR;
    	} else {
            Tcl_AppendResult(menuPtr->interp, 
            	    "bad menu type - must be normal, tearoff, or menubar",
        	    (char *) NULL);
            return TCL_ERROR;
    	}
    }

    commandObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement(menuPtr->interp, commandObjPtr,
    	    Tcl_NewStringObj("tkMenuDup", -1));
    Tcl_ListObjAppendElement(menuPtr->interp, commandObjPtr,
    	    Tcl_NewStringObj(Tk_PathName(menuPtr->tkwin), -1));
    Tcl_ListObjAppendElement(menuPtr->interp, commandObjPtr,
    	    Tcl_NewStringObj(newMenuName, -1));
    if ((newMenuTypeString == NULL) || (newMenuTypeString[0] == '\0')) {
    	Tcl_ListObjAppendElement(menuPtr->interp, commandObjPtr,
    		Tcl_NewStringObj("normal", -1));
    } else {
    	Tcl_ListObjAppendElement(menuPtr->interp, commandObjPtr,
    		Tcl_NewStringObj(newMenuTypeString, -1));
    }
    Tcl_IncrRefCount(commandObjPtr);
    Tcl_Preserve((ClientData) menuPtr);
    returnResult = Tcl_EvalObj(menuPtr->interp, commandObjPtr);
    Tcl_DecrRefCount(commandObjPtr);

    /*
     * Make sure the tcl command actually created the clone.
     */
    
    if ((returnResult == TCL_OK) &&
    	    ((menuRefPtr = TkFindMenuReferences(menuPtr->interp, newMenuName))
	    != (TkMenuReferences *) NULL)
	    && (menuPtr->numEntries == menuRefPtr->menuPtr->numEntries)) {
    	TkMenu *newMenuPtr = menuRefPtr->menuPtr;
	char *newArgv[3];
	int i, numElements;

	/*
	 * Now put this newly created menu into the parent menu's instance
	 * chain.
	 */

	if (menuPtr->nextInstancePtr == NULL) {
	    menuPtr->nextInstancePtr = newMenuPtr;
	    newMenuPtr->masterMenuPtr = menuPtr->masterMenuPtr;
	} else {
	    TkMenu *masterMenuPtr;
	    
	    masterMenuPtr = menuPtr->masterMenuPtr;
	    newMenuPtr->nextInstancePtr = masterMenuPtr->nextInstancePtr;
	    masterMenuPtr->nextInstancePtr = newMenuPtr;
	    newMenuPtr->masterMenuPtr = masterMenuPtr;
	}
   	
   	/*
   	 * Add the master menu's window to the bind tags for this window
   	 * after this window's tag. This is so the user can bind to either
   	 * this clone (which may not be easy to do) or the entire menu
   	 * clone structure.
   	 */
   	
   	newArgv[0] = "bindtags";
   	newArgv[1] = Tk_PathName(newMenuPtr->tkwin);
   	if (Tk_BindtagsCmd((ClientData)newMenuPtr->tkwin, 
   		newMenuPtr->interp, 2, newArgv) == TCL_OK) {
   	    char *windowName;
   	    Tcl_Obj *bindingsPtr = 
   	    		Tcl_NewStringObj(newMenuPtr->interp->result, -1);
   	    Tcl_Obj *elementPtr;
     
   	    Tcl_ListObjLength(newMenuPtr->interp, bindingsPtr, &numElements);
   	    for (i = 0; i < numElements; i++) {
   	    	Tcl_ListObjIndex(newMenuPtr->interp, bindingsPtr, i,
			&elementPtr);
   	    	windowName = Tcl_GetStringFromObj(elementPtr, NULL);
   	    	if (strcmp(windowName, Tk_PathName(newMenuPtr->tkwin))
   	    		== 0) {
   	    	    Tcl_Obj *newElementPtr = Tcl_NewStringObj(
   	    	    	    Tk_PathName(newMenuPtr->masterMenuPtr->tkwin), -1);
   	    	    Tcl_ListObjReplace(menuPtr->interp, bindingsPtr,
   	    	    	    i + 1, 0, 1, &newElementPtr);
   	    	    newArgv[2] = Tcl_GetStringFromObj(bindingsPtr, NULL);
   	    	    Tk_BindtagsCmd((ClientData)newMenuPtr->tkwin,
   	    	    	    menuPtr->interp, 3, newArgv);
   	    	    break;
   	    	}
   	    }
   	    Tcl_DecrRefCount(bindingsPtr);   	    
   	}
   	Tcl_ResetResult(menuPtr->interp);
      	
   	/*
   	 * Clone all of the cascade menus that this menu points to.
   	 */
   	
   	for (i = 0; i < menuPtr->numEntries; i++) {
   	    char *newCascadeName;
   	    TkMenuReferences *cascadeRefPtr;
   	    TkMenu *oldCascadePtr;
   	    
   	    if ((menuPtr->entries[i]->type == CASCADE_ENTRY)
		&& (menuPtr->entries[i]->name != NULL)) {
   	    	cascadeRefPtr =
			TkFindMenuReferences(menuPtr->interp,
			menuPtr->entries[i]->name);
   	    	if ((cascadeRefPtr != NULL) && (cascadeRefPtr->menuPtr)) {
   	    	    char *nameString;
		    
   	    	    oldCascadePtr = cascadeRefPtr->menuPtr;

		    nameString = Tk_PathName(newMenuPtr->tkwin);
   	    	    newCascadeName = TkNewMenuName(menuPtr->interp,
   	    	     	    nameString, oldCascadePtr);
		    CloneMenu(oldCascadePtr, newCascadeName, NULL);

		    newArgv[0] = "-menu";
		    newArgv[1] = newCascadeName;
		    ConfigureMenuEntry(newMenuPtr->entries[i], 2, newArgv, 
		    	    TK_CONFIG_ARGV_ONLY);
		    ckfree(newCascadeName);
   	    	}
   	    }
   	}
   	
    	returnResult = TCL_OK;
    } else {
    	returnResult = TCL_ERROR;
    }
    Tcl_Release((ClientData) menuPtr);
    return returnResult;
}

/*
 *----------------------------------------------------------------------
 *
 * MenuDoYPosition --
 *
 *	Given arguments from an option command line, returns the Y position.
 *
 * Results:
 *	Returns TCL_OK or TCL_Error
 *
 * Side effects:
 *	yPosition is set to the Y-position of the menu entry.
 *
 *----------------------------------------------------------------------
 */
    
static int
MenuDoYPosition(interp, menuPtr, arg)
    Tcl_Interp *interp;
    TkMenu *menuPtr;
    char *arg;
{
    int index;
    
    TkRecomputeMenu(menuPtr);
    if (TkGetMenuIndex(interp, menuPtr, arg, 0, &index) != TCL_OK) {
    	goto error;
    }
    if (index < 0) {
        interp->result = "0";
    } else {
    	sprintf(interp->result, "%d", menuPtr->entries[index]->y);
    }
    return TCL_OK;
    
error:
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetIndexFromCoords --
 *
 *	Given a string of the form "@int", return the menu item corresponding
 *	to int.
 *
 * Results:
 *	If int is a valid number, *indexPtr will be the number of the menuentry
 *	that is the correct height. If int is invaled, *indexPtr will be
 *	unchanged. Returns appropriate Tcl error number.
 *
 * Side effects:
 *	If int is invalid, interp's result will set to NULL.
 *
 *----------------------------------------------------------------------
 */

static int
GetIndexFromCoords(interp, menuPtr, string, indexPtr)
    Tcl_Interp *interp;		/* interp of menu */
    TkMenu *menuPtr;		/* the menu we are searching */
    char *string;		/* The @string we are parsing */
    int *indexPtr;		/* The index of the item that matches */
{
    int x, y, i;
    char *p, *end;
    
    TkRecomputeMenu(menuPtr);
    p = string + 1;
    y = strtol(p, &end, 0);
    if (end == p) {
	goto error;
    }
    if (*end == ',') {
	x = y;
	p = end + 1;
	y = strtol(p, &end, 0);
	if (end == p) {
	    goto error;
	}
    } else {
	x = menuPtr->borderWidth;
    }
    
    for (i = 0; i < menuPtr->numEntries; i++) {
	if ((x >= menuPtr->entries[i]->x) && (y >= menuPtr->entries[i]->y)
		&& (x < (menuPtr->entries[i]->x + menuPtr->entries[i]->width))
		&& (y < (menuPtr->entries[i]->y
		+ menuPtr->entries[i]->height))) {
	    break;
	}
    }
    if (i >= menuPtr->numEntries) {
	/* i = menuPtr->numEntries - 1; */
	i = -1;
    }
    *indexPtr = i;
    return TCL_OK;

    error:
    Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RecursivelyDeleteMenu --
 *
 *	Deletes a menu and any cascades underneath it. Used for deleting
 *	instances when a menu is no longer being used as a menubar,
 *	for instance.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the menu and all cascade menus underneath it.
 *
 *----------------------------------------------------------------------
 */

static void
RecursivelyDeleteMenu(menuPtr)
    TkMenu *menuPtr;		/* The menubar instance we are deleting */
{
    int i;
    TkMenuEntry *mePtr;
    
    for (i = 0; i < menuPtr->numEntries; i++) {
    	mePtr = menuPtr->entries[i];
    	if ((mePtr->type == CASCADE_ENTRY)
    		&& (mePtr->childMenuRefPtr != NULL)
    		&& (mePtr->childMenuRefPtr->menuPtr != NULL)) {
    	    RecursivelyDeleteMenu(mePtr->childMenuRefPtr->menuPtr);
    	}
    }
    Tk_DestroyWindow(menuPtr->tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TkNewMenuName --
 *
 *	Makes a new unique name for a cloned menu. Will be a child
 *	of oldName.
 *
 * Results:
 *	Returns a char * which has been allocated; caller must free.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

char *
TkNewMenuName(interp, parentName, menuPtr)
    Tcl_Interp *interp;		/* The interp the new name has to live in.*/
    char *parentName;		/* The prefix path of the new name. */
    TkMenu *menuPtr;		/* The menu we are cloning. */
{
    Tcl_DString resultDString;
    Tcl_DString childDString;
    char *destString;
    int offset, i;
    int doDot = parentName[strlen(parentName) - 1] != '.';
    Tcl_CmdInfo cmdInfo;
    char *returnString;
    Tcl_HashTable *nameTablePtr = NULL;
    TkWindow *winPtr = (TkWindow *) menuPtr->tkwin;
    if (winPtr->mainPtr != NULL) {
	nameTablePtr = &(winPtr->mainPtr->nameTable);
    }
    
    Tcl_DStringInit(&childDString);
    Tcl_DStringAppend(&childDString, Tk_PathName(menuPtr->tkwin), -1);
    for (destString = Tcl_DStringValue(&childDString);
    	    *destString != '\0'; destString++) {
    	if (*destString == '.') {
    	    *destString = '#';
    	}
    }
    
    offset = 0;
    
    for (i = 0; ; i++) {
    	if (i == 0) {
    	    Tcl_DStringInit(&resultDString);
    	    Tcl_DStringAppend(&resultDString, parentName, -1);
    	    if (doDot) {
    	    	Tcl_DStringAppend(&resultDString, ".", -1);
    	    }
    	    Tcl_DStringAppend(&resultDString,
    	    	    Tcl_DStringValue(&childDString), -1);
    	    destString = Tcl_DStringValue(&resultDString);
    	} else {
    	    if (i == 1) {
    	    	offset = Tcl_DStringLength(&resultDString);
    	    	Tcl_DStringSetLength(&resultDString, offset + 10);
    	    	destString = Tcl_DStringValue(&resultDString);
    	    }
    	    sprintf(destString + offset, "%d", i);
    	}
    	if ((Tcl_GetCommandInfo(interp, destString, &cmdInfo) == 0)
		&& ((nameTablePtr == NULL)
		|| (Tcl_FindHashEntry(nameTablePtr, destString) == NULL))) {
    	    break;
    	}
    }
    returnString = ckalloc(strlen(destString) + 1);
    strcpy(returnString, destString);
    Tcl_DStringFree(&resultDString);
    Tcl_DStringFree(&childDString);
    return returnString;    	   
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetWindowMenuBar --
 *
 *	Associates a menu with a window. Called by ConfigureFrame in
 *	in response to a "-menu .foo" configuration option for a top
 *	level.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The old menu clones for the menubar are thrown away, and a
 *	handler is set up to allocate the new ones.
 *
 *----------------------------------------------------------------------
 */
void
TkSetWindowMenuBar(interp, tkwin, oldMenuName, menuName)
    Tcl_Interp *interp;		/* The interpreter the toplevel lives in. */
    Tk_Window tkwin;		/* The toplevel window */
    char *oldMenuName;		/* The name of the menubar previously set in
    				 * this toplevel. NULL means no menu was
				 * set previously. */
    char *menuName;		/* The name of the new menubar that the
				 * toplevel needs to be set to. NULL means
				 * that their is no menu now. */
{
    TkMenuTopLevelList *topLevelListPtr, *prevTopLevelPtr;
    TkMenu *menuPtr;
    TkMenuReferences *menuRefPtr;
    
    TkMenuInit();

    /*
     * Destroy the menubar instances of the old menu. Take this window
     * out of the old menu's top level reference list.
     */
    
    if (oldMenuName != NULL) {
        menuRefPtr = TkFindMenuReferences(interp, oldMenuName);
    	if (menuRefPtr != NULL) {

	    /*
	     * Find the menubar instance that is to be removed. Destroy
	     * it and all of the cascades underneath it.
	     */

	    if (menuRefPtr->menuPtr != NULL) {    	    
    	    	TkMenu *instancePtr;

    	    	menuPtr = menuRefPtr->menuPtr;
    	        	    
    	    	for (instancePtr = menuPtr->masterMenuPtr;
		        instancePtr != NULL; 
    	    	    	instancePtr = instancePtr->nextInstancePtr) {
    	    	    if (instancePtr->menuType == MENUBAR 
    	    		    && instancePtr->parentTopLevelPtr == tkwin) {
    	    	    	RecursivelyDeleteMenu(instancePtr);
    	    	    	break;
    	    	    }
    	    	}
    	    }
 
 	    /*
 	     * Now we need to remove this toplevel from the list of toplevels
	     * that reference this menu.
 	     */
 
            for (topLevelListPtr = menuRefPtr->topLevelListPtr,
		    prevTopLevelPtr = NULL;
		    (topLevelListPtr != NULL) 
            	    && (topLevelListPtr->tkwin != tkwin);
		    prevTopLevelPtr = topLevelListPtr,
		    topLevelListPtr = topLevelListPtr->nextPtr) {

		/*
		 * Empty loop body.
		 */
		
            }

	    /*
	     * Now we have found the toplevel reference that matches the
	     * tkwin; remove this reference from the list.
	     */

	    if (topLevelListPtr != NULL) {
            	if (prevTopLevelPtr == NULL) {
		    menuRefPtr->topLevelListPtr =
			    menuRefPtr->topLevelListPtr->nextPtr;
		} else {
            	    prevTopLevelPtr->nextPtr = topLevelListPtr->nextPtr;
            	}
            	ckfree((char *) topLevelListPtr);
            	TkFreeMenuReferences(menuRefPtr);
            }
        }
    }

    /*
     * Now, add the clone references for the new menu.
     */
    
    if (menuName != NULL && menuName[0] != 0) {
    	TkMenu *menuBarPtr = NULL;

	menuRefPtr = TkCreateMenuReferences(interp, menuName);    	
    	
    	menuPtr = menuRefPtr->menuPtr;
    	if (menuPtr != NULL) {
   	    char *cloneMenuName;
   	    TkMenuReferences *cloneMenuRefPtr;
	    char *newArgv[4];
    	
            /*
             * Clone the menu and all of the cascades underneath it.
             */

    	    cloneMenuName = TkNewMenuName(interp, Tk_PathName(tkwin),
    	    	    menuPtr);
            CloneMenu(menuPtr, cloneMenuName, "menubar");
	    
            cloneMenuRefPtr = TkFindMenuReferences(interp, cloneMenuName);
            if ((cloneMenuRefPtr != NULL)
		    && (cloneMenuRefPtr->menuPtr != NULL)) {
            	cloneMenuRefPtr->menuPtr->parentTopLevelPtr = tkwin;
            	menuBarPtr = cloneMenuRefPtr->menuPtr;
		newArgv[0] = "-cursor";
		newArgv[1] = "";
		ConfigureMenu(menuPtr->interp, cloneMenuRefPtr->menuPtr,
			2, newArgv, TK_CONFIG_ARGV_ONLY);
            }

	    TkpSetWindowMenuBar(tkwin, menuBarPtr);
		        
            ckfree(cloneMenuName);
        } else {
    	    TkpSetWindowMenuBar(tkwin, NULL);
	}

        
        /*
         * Add this window to the menu's list of windows that refer
         * to this menu.
         */

        topLevelListPtr = (TkMenuTopLevelList *)
		ckalloc(sizeof(TkMenuTopLevelList));
        topLevelListPtr->tkwin = tkwin;
        topLevelListPtr->nextPtr = menuRefPtr->topLevelListPtr;
        menuRefPtr->topLevelListPtr = topLevelListPtr;
    } else {
	TkpSetWindowMenuBar(tkwin, NULL);
    }
    TkpSetMainMenubar(interp, tkwin, menuName);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuHashTable --
 *
 *	Called when an interp is deleted and a menu hash table has
 *	been set in it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The hash table is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuHashTable(clientData, interp)
    ClientData clientData;	/* The menu hash table we are destroying */
    Tcl_Interp *interp;		/* The interpreter we are destroying */
{
    Tcl_DeleteHashTable((Tcl_HashTable *) clientData);
    ckfree((char *) clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetMenuHashTable --
 *
 *	For a given interp, give back the menu hash table that goes with
 *	it. If the hash table does not exist, it is created.
 *
 * Results:
 *	Returns a hash table pointer.
 *
 * Side effects:
 *	A new hash table is created if there were no table in the interp
 *	originally.
 *
 *----------------------------------------------------------------------
 */

Tcl_HashTable *
TkGetMenuHashTable(interp)
    Tcl_Interp *interp;		/* The interp we need the hash table in.*/
{
    Tcl_HashTable *menuTablePtr;

    menuTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, MENU_HASH_KEY,
	    NULL);
    if (menuTablePtr == NULL) {
	menuTablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(menuTablePtr, TCL_STRING_KEYS);
	Tcl_SetAssocData(interp, MENU_HASH_KEY, DestroyMenuHashTable,
		(ClientData) menuTablePtr);
    }
    return menuTablePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateMenuReferences --
 *
 *	Given a pathname, gives back a pointer to a TkMenuReferences structure.
 *	If a reference is not already in the hash table, one is created.
 *
 * Results:
 *	Returns a pointer to a menu reference structure. Should not
 *	be freed by calller; when a field of the reference is cleared,
 *	TkFreeMenuReferences should be called.
 *
 * Side effects:
 *	A new hash table entry is created if there were no references
 *	to the menu originally.
 *
 *----------------------------------------------------------------------
 */

TkMenuReferences *
TkCreateMenuReferences(interp, pathName)
    Tcl_Interp *interp;
    char *pathName;		/* The path of the menu widget */
{
    Tcl_HashEntry *hashEntryPtr;
    TkMenuReferences *menuRefPtr;
    int newEntry;
    Tcl_HashTable *menuTablePtr = TkGetMenuHashTable(interp);

    hashEntryPtr = Tcl_CreateHashEntry(menuTablePtr, pathName, &newEntry);
    if (newEntry) {
    	menuRefPtr = (TkMenuReferences *) ckalloc(sizeof(TkMenuReferences));
    	menuRefPtr->menuPtr = NULL;
    	menuRefPtr->topLevelListPtr = NULL;
    	menuRefPtr->parentEntryPtr = NULL;
    	menuRefPtr->hashEntryPtr = hashEntryPtr;
    	Tcl_SetHashValue(hashEntryPtr, (char *) menuRefPtr);
    } else {
    	menuRefPtr = (TkMenuReferences *) Tcl_GetHashValue(hashEntryPtr);
    }
    return menuRefPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFindMenuReferences --
 *
 *	Given a pathname, gives back a pointer to the TkMenuReferences
 *	structure.
 *
 * Results:
 *	Returns a pointer to a menu reference structure. Should not
 *	be freed by calller; when a field of the reference is cleared,
 *	TkFreeMenuReferences should be called. Returns NULL if no reference
 *	with this pathname exists.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkMenuReferences *
TkFindMenuReferences(interp, pathName)
    Tcl_Interp *interp;		/* The interp the menu is living in. */
    char *pathName;		/* The path of the menu widget */
{
    Tcl_HashEntry *hashEntryPtr;
    TkMenuReferences *menuRefPtr = NULL;
    Tcl_HashTable *menuTablePtr;

    menuTablePtr = TkGetMenuHashTable(interp);
    hashEntryPtr = Tcl_FindHashEntry(menuTablePtr, pathName);
    if (hashEntryPtr != NULL) {
    	menuRefPtr = (TkMenuReferences *) Tcl_GetHashValue(hashEntryPtr);
    }
    return menuRefPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeMenuReferences --
 *
 *	This is called after one of the fields in a menu reference
 *	is cleared. It cleans up the ref if it is now empty.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If this is the last field to be cleared, the menu ref is
 *	taken out of the hash table.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeMenuReferences(menuRefPtr)
    TkMenuReferences *menuRefPtr;		/* The menu reference to
						 * free */
{
    if ((menuRefPtr->menuPtr == NULL) 
    	    && (menuRefPtr->parentEntryPtr == NULL)
    	    && (menuRefPtr->topLevelListPtr == NULL)) {
    	Tcl_DeleteHashEntry(menuRefPtr->hashEntryPtr);
    	ckfree((char *) menuRefPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteMenuCloneEntries --
 *
 *	For every clone in this clone chain, delete the menu entries
 *	given by the parameters.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The appropriate entries are deleted from all clones of this menu.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteMenuCloneEntries(menuPtr, first, last)
    TkMenu *menuPtr;		    /* the menu the command was issued with */
    int	first;			    /* the zero-based first entry in the set
				     * of entries to delete. */
    int last;			    /* the zero-based last entry */
{

    TkMenu *menuListPtr;
    int numDeleted, i;

    numDeleted = last + 1 - first;
    for (menuListPtr = menuPtr->masterMenuPtr; menuListPtr != NULL;
	    menuListPtr = menuListPtr->nextInstancePtr) {
	for (i = last; i >= first; i--) {
	    Tcl_EventuallyFree((ClientData) menuListPtr->entries[i],
		    DestroyMenuEntry);
	}
	for (i = last + 1; i < menuListPtr->numEntries; i++) {
	    menuListPtr->entries[i - numDeleted] = menuListPtr->entries[i];
	    menuListPtr->entries[i - numDeleted]->index = i;
	}
	menuListPtr->numEntries -= numDeleted;
	if (menuListPtr->numEntries == 0) {
	    ckfree((char *) menuListPtr->entries);
	    menuListPtr->entries = NULL;
	}
	if ((menuListPtr->active >= first) 
		&& (menuListPtr->active <= last)) {
	    menuListPtr->active = -1;
	} else if (menuListPtr->active > last) {
	    menuListPtr->active -= numDeleted;
	}
	TkEventuallyRecomputeMenu(menuListPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMenuInit --
 *
 *	Sets up the hash tables and the variables used by the menu package.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	lastMenuID gets initialized, and the parent hash and the command hash
 *	are allocated.
 *
 *----------------------------------------------------------------------
 */

void
TkMenuInit()
{
    if (!menusInitialized) {
    	TkpMenuInit();
    	menusInitialized = 1;
    }
}
