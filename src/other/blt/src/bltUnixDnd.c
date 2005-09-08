
/*
 *  bltUnixDnd.c --
 *
 *	This module implements a drag-and-drop manager for the BLT
 *	Toolkit.  Allows widgets to be registered as drag&drop sources
 *	and targets for handling "drag-and-drop" operations between
 *	Tcl/Tk applications.
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
 *
 *	The "drag&drop" command was created by Michael J. McLennan.
 */

#include "bltInt.h"

#ifndef NO_DRAGDROP

#include <bltHash.h>
#include <bltChain.h>


#include <X11/Xatom.h>
#include <X11/Xproto.h>

#define DND_THREAD_KEY	"BLT Dnd Data"

#define DND_SELECTED	(1<<0)
#define DND_INITIATED	(1<<1)
#define DND_ACTIVE	(DND_SELECTED | DND_INITIATED)
#define DND_IN_PACKAGE  (1<<2)	/* Indicates if a token package command is
				 * currently active. The user may invoke
				 * "update" or "tkwait" commands from within
				 * the package command script. This allows the
				 * "drag" operation to preempt itself. */
#define DND_VOIDED	(1<<3)
#define DND_DELETED	(1<<4)

#define PACK(lo,hi)	(((hi) << 16) | ((lo) & 0x0000FFFF))
#define UNPACK(x,lo,hi) ((lo) = (x & 0x0000FFFF), (hi) = (x >> 16))

#define WATCH_ENTER	(1<<0)
#define WATCH_LEAVE	(1<<1)
#define WATCH_MOTION	(1<<2)
#define WATCH_MASK	(WATCH_ENTER | WATCH_LEAVE | WATCH_MOTION)

/* Source-to-Target Message Types */

#define ST_DRAG_ENTER	0x1001
#define ST_DRAG_LEAVE	0x1002
#define ST_DRAG_MOTION	0x1003
#define ST_DROP		0x1004

/* Target-to-Source Message Types */

#define TS_DRAG_STATUS	0x1005
#define TS_START_DROP	0x1006
#define TS_DROP_RESULT	0x1007

/* Indices of information fields in ClientMessage array. */

#define MESG_TYPE	0	/* Message type. */
#define MESG_WINDOW	1	/* Window id of remote. */
#define MESG_TIMESTAMP	2	/* Transaction timestamp. */
#define MESG_POINT	3	/* Root X-Y coordinate. */
#define MESG_STATE	4	/* Button and key state. */
#define MESG_RESPONSE	3	/* Response to drag/drop message. */
#define MESG_FORMAT	3	/* Format atom. */
#define MESG_PROPERTY	4	/* Index of button #/key state. */

/* Drop Status Values (actions included) */

#define DROP_CONTINUE	-2
#define DROP_FAIL	-1
#define DROP_CANCEL	0
#define DROP_OK		1
#define DROP_COPY	1
#define DROP_LINK	2
#define DROP_MOVE	3

#define PROP_WATCH_FLAGS	0
#define PROP_DATA_FORMATS	1
#define PROP_MAX_SIZE		1000	/* Maximum size of property. */

#define PROTO_BLT	0
#define PROTO_XDND	1

#define TOKEN_OFFSET	0
#define TOKEN_REDRAW	(1<<0)

#define TOKEN_NORMAL	0
#define TOKEN_REJECT	-1
#define TOKEN_ACTIVE	1
#define TOKEN_TIMEOUT	5000	/* 5 second timeout for drop requests. */

/*
 *   Each widget representing a drag & drop target is tagged with 
 *   a "BltDndTarget" property in XA_STRING format.  This property 
 *   identifies the window as a target.  It's formated as a Tcl list 
 *   and contains the following information:
 *
 *	"flags DATA_TYPE DATA_TYPE ..."
 *
 *	"INTERP_NAME TARGET_NAME WINDOW_ID DATA_TYPE DATA_TYPE ..."
 *
 *	INTERP_NAME	Name of the target application's interpreter.
 *	TARGET_NAME	Path name of widget registered as the drop target.  
 *	WINDOW_ID	Window Id of the target's communication window. 
 *			Used to forward Enter/Leave/Motion event information 
 *			to the target.
 *	DATA_TYPE	One or more "types" handled by the target.
 *
 *   When the user invokes the "drag" operation, the window hierarchy
 *   is progressively examined.  Window information is cached during
 *   the operation, to minimize X server traffic. Windows carrying a
 *   "BltDndTarget" property are identified.  When the token is dropped 
 *   over a valid site, the drop information is sent to the application 
 *   via the usual "send" command.  If communication fails, the drag&drop 
 *   facility automatically posts a rejection symbol on the token window.  
 */

/* 
 * Drop Protocol:
 *
 *		Source				Target
 *              ------                          ------
 *   ButtonRelease-? event.
 *   Invokes blt::dnd drop
 *		   +
 *   Send "drop" message to target (via 
 *   ClientMessage). Contains X-Y, key/ --> Gets "drop" message. 
 *   button state, source window XID.       Invokes LeaveCmd proc.
 *					    Gets property from source of 
 *					    ordered matching formats.  
 *					            +
 *					    Invokes DropCmd proc. Arguments
 *					    are X-Y coordinate, key/button 
 *					    state, transaction timestamp, 
 *					    list of matching formats.  
 *						    +
 *					    Target selects format and invokes
 *					    blt::dnd pull to transfer the data
 *					    in the selected format.
 *						    +
 *					    Sends "drop start" message to 
 *					    source.  Contains selected format
 *   Gets "drop start" message.		<-- (as atom), ?action?, target window
 *   Invokes data handler for the     	    ID, transaction timestamp. 
 *   selected format.				    +
 *                +			    Waits for property to change on
 *   Places first packet of data in         its window.  Time out set for
 *   property on target window.         --> no response.
 *                +                                 +
 *   Waits for response property            After each packet, sets zero-length
 *   change. Time out set for no resp.  <-- property on source window.
 *   If non-zero length packet, error               +
 *   occurred, packet is error message.     Sends "drop finished" message.
 *					    Contains transaction timestamp, 
 *   Gets "drop finished" message.      <-- status, ?action?.
 *   Invokes FinishCmd proc. 
 */

/* Configuration Parameters */

#define DEF_DND_BUTTON_BACKGROUND		RGB_YELLOW
#define DEF_DND_BUTTON_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_DND_BUTTON_NUMBER		"3"
#define DEF_DND_ENTER_COMMAND		(char *)NULL
#define DEF_DND_LEAVE_COMMAND		(char *)NULL
#define DEF_DND_MOTION_COMMAND		(char *)NULL
#define DEF_DND_DROP_COMMAND		(char *)NULL
#define DEF_DND_RESULT_COMMAND		(char *)NULL
#define DEF_DND_PACKAGE_COMMAND		(char *)NULL
#define DEF_DND_SELF_TARGET		"no"
#define DEF_DND_SEND			(char *)NULL
#define DEF_DND_IS_TARGET		"no"
#define DEF_DND_IS_SOURCE		"no"
#define DEF_DND_SITE_COMMAND		(char *)NULL

#define DEF_DND_DRAG_THRESHOLD		"0"
#define DEF_TOKEN_ACTIVE_BACKGROUND	STD_ACTIVE_BACKGROUND
#define DEF_TOKEN_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_TOKEN_ACTIVE_BORDERWIDTH	"3"
#define DEF_TOKEN_ACTIVE_RELIEF		"sunken"
#define DEF_TOKEN_ANCHOR		"se"
#define DEF_TOKEN_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_TOKEN_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_TOKEN_BORDERWIDTH		"3"
#define DEF_TOKEN_CURSOR		"top_left_arrow"
#define DEF_TOKEN_REJECT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TOKEN_REJECT_BG_MONO	RGB_WHITE
#define DEF_TOKEN_REJECT_FOREGROUND	RGB_RED
#define DEF_TOKEN_REJECT_FG_MONO	RGB_BLACK
#define DEF_TOKEN_REJECT_STIPPLE_COLOR	(char *)NULL
#define DEF_TOKEN_REJECT_STIPPLE_MONO	RGB_GREY50
#define DEF_TOKEN_RELIEF		"raised"

static int StringToCursors _ANSI_ARGS_((ClientData clientData, 
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec, 
	int offset));
static char *CursorsToString _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption cursorsOption =
{
    StringToCursors, CursorsToString, (ClientData)0
};

typedef struct {
    Blt_HashTable dndTable;	/* Hash table of dnd structures keyed by 
				 * the address of the reference Tk window */
    Tk_Window mainWindow;
    Display *display;
    Atom mesgAtom;		/* Atom signifying a drag-and-drop message. */
    Atom formatsAtom;		/* Source formats property atom.  */
    Atom targetAtom;		/* Target property atom. */
    Atom commAtom;		/* Communication property atom. */

#ifdef HAVE_XDND
    Blt_HashTable handlerTable; /* Table of toplevel windows with XdndAware 
				 * properties attached to them. */
    Atom XdndActionListAtom;
    Atom XdndAwareAtom;
    Atom XdndEnterAtom;
    Atom XdndFinishedAtom;
    Atom XdndLeaveAtom;
    Atom XdndPositionAtom;
    Atom XdndSelectionAtom;
    Atom XdndStatusAtom;
    Atom XdndTypeListAtom;

    Atom XdndActionCopyAtom;
    Atom XdndActionMoveAtom;
    Atom XdndActionLinkAtom;
    Atom XdndActionAskAtom;
    Atom XdndActionPrivateAtom;
    Atom XdndActionDescriptionAtom;
#endif
} DndInterpData;


typedef struct {
    Tcl_DString dString;
    Window window;		/* Source/Target window */
    Display *display;
    Atom commAtom;		/* Data communication property atom. */
    int packetSize;
    Tcl_TimerToken timerToken;
    int status;			/* Status of transaction:  CONTINUE, OK, FAIL,
				 * or TIMEOUT. */
    int timestamp;		/* Timestamp of the transaction. */
    int offset;
    int protocol;		/* Drag-and-drop protocol used by the source:
				 * either PROTO_BLT or PROTO_XDND. */
} DropPending;

/* 
 * SubstDescriptors --
 *
 *	Structure to hold letter-value pairs for percent substitutions.
 */
typedef struct {
    char letter;		/* character like 'x' in "%x" */
    char *value;		/* value to be substituted in place of "%x" */
} SubstDescriptors;

/*
 *  Drag&Drop Registration Data
 */
typedef struct {
    Tk_Window tkwin;		/* Window that embodies the token.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up. */

    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with widget.  Used
				 * to delete widget command. */
    Tk_3DBorder border;		/* Structure used to draw 3-D border and
				 * background.  NULL means no background
				 * or border. */
    int borderWidth;		/* Width of 3-D border (if any). */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED etc. */

    int flags;			/* Various flags;  see below for
				 * definitions. */

    /* Token specific fields */
    int x, y;			/* Last position of token window */
    int startX, startY;

    int status;			/* Indicates the current status of the token:
				 * 0 is normal, 1 is active. */
    int lastStatus;		/* Indicates the last status of the token. */
    Tcl_TimerToken timerToken;	/* Token for routine to hide tokenwin */
    GC fillGC;			/* GC used to draw rejection fg: (\) */
    GC outlineGC;		/* GC used to draw rejection bg: (\) */
    int width, height;

    /* User-configurable fields */

    Tk_Anchor anchor;		/* Position of token win relative to mouse */
    Tk_3DBorder normalBorder;	/* Border/background for token window */
    Tk_3DBorder activeBorder;	/* Border/background for token window */
    int activeRelief;
    int activeBorderWidth;	/* Border width in pixels */
    XColor *fillColor;		/* Color used to draw rejection fg: (\) */
    XColor *outlineColor;	/* Color used to draw rejection bg: (\) */
    Pixmap rejectStipple;	/* Stipple used to draw rejection: (\) */
    int reqWidth, reqHeight;

    int nSteps;

} Token;

/*
 *  Winfo --
 *
 *	This structure represents a window hierarchy examined during a single
 *	"drag" operation.  It's used to cache information to reduce the round
 *	trip calls to the server needed to query window geometry information
 *	and grab the target property.  
 */
typedef struct WinfoStruct {
    Window window;		/* Window in hierarchy. */
    
    int initialized;		/* If zero, the rest of this structure's
				 * information hasn't been set. */
    
    int x1, y1, x2, y2;		/* Extents of the window (upper-left and
				 * lower-right corners). */
    
    struct WinfoStruct *parentPtr;	/* Parent node. NULL if root. Used to
				 * compute offset for X11 windows. */
    
    Blt_Chain *chainPtr;	/* List of this window's children. If NULL,
				 * there are no children. */
    
    int isTarget;		/* Indicates if this window is a drag&drop
				 * target. */
    int lookedForProperty;	/* Indicates if this window  */
    
    int eventFlags;		/* Retrieved from the target's drag&drop 
				 * property, indicates what kinds of pointer
				 * events should be relayed to the target via
				 * ClientMessages. Possible values are OR-ed 
				 * combinations of the following bits: 
				 *	001 Enter events.  
				 *	010 Motion events.
				 *	100 Leave events.  
				 */
    char *matches;
    
} Winfo;

/*
 *  Dnd --
 *
 *	This structure represents the drag&drop manager.  It is associated
 *	with a widget as a drag&drop source, target, or both.  It contains
 *	both the source and target components, since a widget can be both 
 *	a drag source and a drop target.  
 */
typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with the drag&drop
				 * manager. */
    
    Tk_Window tkwin;		/* Tk window representing the drag&drop 
				 * manager (can be source and/or target). */

    Display *display;		/* Display for drag&drop widget. Saved to free
				 * resources after window has been destroyed. */

    int isSource;		/* Indicates if this drag&drop manager can act
				 * as a drag source. */
    int isTarget;		/* Indicates if this drag&drop manager can act
				 * as a drop target. */

    int targetPropertyExists;	/* Indicates is the drop target property has 
				 * been set. */

    unsigned int flags;		/* Various flags;  see below for
				 * definitions. */
    int timestamp;		/* Id of the current drag&drop transaction. */

    int x, y;			/* Last known location of the mouse pointer. */

    Blt_HashEntry *hashPtr;

    DndInterpData *dataPtr;	

    /* Source component. */
    
    Blt_HashTable getDataTable;	/* Table of data handlers (converters)
				 * registered for this source. */

    int reqButton;		/* Button used to invoke drag operation. */

    int button;			/* Last button press detected. */
    int keyState;		/* Last key state.  */

    Tk_Cursor cursor;		/* Cursor restored after dragging */

    int selfTarget;		/* Indicated if the source should drop onto 
				 * itself. */

    char **reqFormats;		/* List of requested data formats. The
				 * list should be ordered with the more 
				 * desireable formats first. You can also
				 * temporarily turn off a source by setting 
				 * the value to the empty string. */

    Winfo *rootPtr;		/* Cached window information: Gathered
				 * and used during the "drag" operation 
				 * to see if the mouse pointer is over a 
				 * valid target. */

    Winfo *windowPtr;		/* Points to information about the last 
				 * target the pointer was over. If NULL, 
				 * the pointer was not over a valid target. */

    char **packageCmd;		/* Tcl command executed at start of the drag
				 * operation to initialize token. */

    char **resultCmd;		/* Tcl command executed at the end of the
				 * "drop" operation to indicate its status. */

    char **siteCmd;		/* Tcl command executed to update token 
				 * window. */

    Token *tokenPtr;		/* Token used to provide special cursor. */
    

    Tcl_TimerToken timerToken;

    Tk_Cursor *cursors;		/* Array of drag-and-drop cursors. */
    int cursorPos;

    int dragStart;		/* Minimum number of pixels movement
				 * before B1-Motion is considered to
				 * start dragging. */

    /* Target component. */

    Blt_HashTable setDataTable;	/* Table of data handlers (converters)
				 * registered for this target. */
    char **enterCmd;		/* Tcl proc called when the mouse enters the
				 * target. */
    char **leaveCmd;		/* Tcl proc called when the mouse leaves the
				 * target. */
    char **motionCmd;		/* Tcl proc called when the mouse is moved
				 * over the target. */
    char **dropCmd;		/* Tcl proc called when the mouse button
				 * is released over the target. */

    char *matchingFormats;
    int lastId;			/* The last transaction id used. This is used
				 * to cache the above formats string. */

    DropPending *pendingPtr;	/* Points to structure containing information
				 * about a current drop in progress. If NULL,
				 * no drop is in progress. */

    short int dropX, dropY;	/* Location of the current drop. */
    short int dragX, dragY;	/* Starting position of token window */
} Dnd;


typedef struct {
    Tk_Window tkwin;		/* Toplevel window of the drop target. */
    int refCount;		/* # of targets referencing this structure. */
    Dnd *dndPtr;		/* Last drop target selected.  Used the 
				 * implement Enter/Leave events for targets. 
				 * If NULL, indicates that no drop target was 
				 * previously selected. */
    int lastRepsonse;		/* Indicates what the last response was. */
    Window window;		/* Window id of the top-level window (ie.
				 * the wrapper). */
    char **formatArr;		/* List of formats available from source. 
				 * Must be pruned down to matching list. */
    DndInterpData *dataPtr;
    int x, y;
    
} XDndHandler;

extern Tk_CustomOption bltListOption;
extern Tk_CustomOption bltDistanceOption;

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-allowformats", "allowFormats", "AllowFormats", 
	DEF_DND_SEND, Tk_Offset(Dnd, reqFormats), 
        TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_INT, "-button", "buttonNumber", "ButtonNumber",
	DEF_DND_BUTTON_NUMBER, Tk_Offset(Dnd, reqButton), 0},
    {TK_CONFIG_CUSTOM, "-dragthreshold", "dragThreshold", "DragThreshold",
	DEF_DND_DRAG_THRESHOLD, Tk_Offset(Dnd, dragStart), 0, 
        &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-cursors", "cursors", "cursors",
	DEF_TOKEN_CURSOR, Tk_Offset(Dnd, cursors), 
	TK_CONFIG_NULL_OK, &cursorsOption },
    {TK_CONFIG_CUSTOM, "-onenter", "onEnter", "OnEnter",
	DEF_DND_ENTER_COMMAND, Tk_Offset(Dnd, enterCmd), 
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-onmotion", "onMotion", "OnMotion",
	DEF_DND_MOTION_COMMAND, Tk_Offset(Dnd, motionCmd), 
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-onleave", "onLeave", "OnLeave",
	DEF_DND_LEAVE_COMMAND, Tk_Offset(Dnd, leaveCmd), 
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-ondrop", "onDrop", "OnDrop",
	DEF_DND_DROP_COMMAND, Tk_Offset(Dnd, dropCmd), 
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-package", "packageCommand", "PackageCommand",
	DEF_DND_PACKAGE_COMMAND, Tk_Offset(Dnd, packageCmd), 
        TK_CONFIG_NULL_OK, &bltListOption },
    {TK_CONFIG_CUSTOM, "-result", "result", "Result",
	DEF_DND_RESULT_COMMAND, Tk_Offset(Dnd, resultCmd), 
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_BOOLEAN, "-selftarget", "selfTarget", "SelfTarget",
	DEF_DND_SELF_TARGET, Tk_Offset(Dnd, selfTarget), 0},
    {TK_CONFIG_CUSTOM, "-site", "siteCommand", "Command",
	DEF_DND_SITE_COMMAND, Tk_Offset(Dnd, siteCmd), 
        TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_BOOLEAN, "-source", "source", "Source",
	DEF_DND_IS_SOURCE, Tk_Offset(Dnd, isSource), 0},
    {TK_CONFIG_BOOLEAN, "-target", "target", "Target",
	DEF_DND_IS_TARGET, Tk_Offset(Dnd, isTarget), 0},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0},
};

static Tk_ConfigSpec tokenConfigSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_TOKEN_ACTIVE_BACKGROUND,
	Tk_Offset(Token, activeBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_TOKEN_ACTIVE_BG_MONO, 
	Tk_Offset(Token, activeBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-activerelief", "activeRelief", "activeRelief",
	DEF_TOKEN_ACTIVE_RELIEF, Tk_Offset(Token, activeRelief), 0},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_TOKEN_ANCHOR, Tk_Offset(Token, anchor), 0},
    {TK_CONFIG_PIXELS, "-activeborderwidth", "activeBorderWidth",
	"ActiveBorderWidth", DEF_TOKEN_ACTIVE_BORDERWIDTH, 
	Tk_Offset(Token, activeBorderWidth), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TOKEN_BACKGROUND, Tk_Offset(Token, normalBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TOKEN_BG_MONO, Tk_Offset(Token, normalBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_TOKEN_BORDERWIDTH, Tk_Offset(Token, borderWidth), 0},
    {TK_CONFIG_COLOR, "-outline", "outline", "Outline",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Token, outlineColor), 
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-outline", "outline", "Outline",
	DEF_TOKEN_REJECT_BG_MONO, Tk_Offset(Token, outlineColor), 
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-fill", "fill", "Fill",
	DEF_TOKEN_REJECT_FOREGROUND, Tk_Offset(Token, fillColor), 
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-fill", "fill", "Fill",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Token, fillColor), 
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_COLOR, Tk_Offset(Token, rejectStipple),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_MONO, Tk_Offset(Token, rejectStipple),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_TOKEN_RELIEF, Tk_Offset(Token, relief), 0},
    {TK_CONFIG_INT, "-width", "width", "Width",
	(char *)NULL, Tk_Offset(Token, reqWidth), 0},
    {TK_CONFIG_INT, "-height", "height", "Height",
	(char *)NULL, Tk_Offset(Token, reqHeight), 0},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0},
};


/*
 *  Forward Declarations
 */
static int DndCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	int argc, char **argv));
static void TokenEventProc _ANSI_ARGS_((ClientData clientData, 
	XEvent *eventPtr));
static void MoveToken _ANSI_ARGS_((Dnd *dndPtr));
static void DisplayToken _ANSI_ARGS_((ClientData clientData));
static void HideToken _ANSI_ARGS_((Dnd *dndPtr));
static void DrawRejectSymbol _ANSI_ARGS_((Dnd *dndPtr));

static int GetDnd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp, 
	char *name, Dnd **dndPtrPtr));
static Dnd *CreateDnd _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin));
static void DestroyDnd _ANSI_ARGS_((DestroyData data));
static int DndEventProc _ANSI_ARGS_((ClientData clientData, XEvent *eventPtr));
static int ConfigureToken _ANSI_ARGS_((Tcl_Interp *interp, Dnd *dndPtr,
	int argc, char **argv, int flags));

static Winfo *OverTarget _ANSI_ARGS_((Dnd *dndPtr));
static void AddTargetProperty _ANSI_ARGS_((Dnd *dndPtr));

static Winfo *InitRoot _ANSI_ARGS_((Dnd *dndPtr));
static void FreeWinfo _ANSI_ARGS_((Winfo *wr));
static void GetWinfo _ANSI_ARGS_((Display *display, Winfo * windowPtr));

static void CancelDrag _ANSI_ARGS_((Dnd *dndPtr));

/*
 * ----------------------------------------------------------------------------
 *
 * StringToCursors --
 *
 *	Converts the resize mode into its numeric representation.  Valid
 *	mode strings are "none", "expand", "shrink", or "both".
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToCursors(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representing cursors. */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of field in record. */
{
    Tk_Cursor **cursorPtrPtr = (Tk_Cursor **)(widgRec + offset);
    int result = TCL_OK;
    int nElems;
    char **elemArr;

    if (*cursorPtrPtr != NULL) {
	Blt_Free(*cursorPtrPtr);
	*cursorPtrPtr = NULL;
    }
    if (string == NULL) {
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, string, &nElems, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nElems > 0) {
	Tk_Cursor *cursorArr;
	register int i;

	cursorArr = Blt_Calloc(nElems + 1, sizeof(Tk_Cursor));
	for (i = 0; i < nElems; i++) {
	    cursorArr[i] = Tk_GetCursor(interp, tkwin, Tk_GetUid(elemArr[i]));
	    if (cursorArr[i] == None) {
		*cursorPtrPtr = cursorArr;
		result = TCL_ERROR;
		break;
	    }
	}    
	Blt_Free(elemArr);
	*cursorPtrPtr = cursorArr;
    }
    return result;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CursorsToString --
 *
 *	Returns resize mode string based upon the resize flags.
 *
 * Results:
 *	The resize mode string is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
CursorsToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Cursor record */
    int offset;			/* Offset of record. */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Tk_Cursor *cursorArr = *(Tk_Cursor **)(widgRec + offset);
    Tk_Cursor *cursorPtr;
    Tcl_DString dString;
    char *result;

    if (cursorArr == NULL) {
	return "";
    }
    Tcl_DStringInit(&dString);
    for (cursorPtr = cursorArr; *cursorPtr != NULL; cursorPtr++) {
	Tcl_DStringAppendElement(&dString, 
		 Tk_NameOfCursor(Tk_Display(tkwin), *cursorPtr));
    }
    result = Blt_Strdup(Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}


static char *
PrintList(list)
    char **list;
{
    int count;
    char **p;

    count = 0;
    for(p = list; *p != NULL; p++) {
	count++;
    }
    return Tcl_Merge(count, list);
}


/* ARGSUSED */
static int
XSendEventErrorProc(clientData, errEventPtr)
    ClientData clientData;
    XErrorEvent *errEventPtr;
{
    int *errorPtr = clientData;

    *errorPtr = TCL_ERROR;
    return 0;
}

static void
SendClientMsg(display, window, mesgAtom, data0, data1, data2, data3, data4)
    Display *display;
    Window window;
    Atom mesgAtom;
    int data0, data1, data2, data3, data4;
{
    XEvent event;
    Tk_ErrorHandler handler;
    int result;
    int any = -1;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = mesgAtom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    result = TCL_OK;
    handler = Tk_CreateErrorHandler(display, any, X_SendEvent, any,
	XSendEventErrorProc, &result);
    if (!XSendEvent(display, window, False, ClientMessage, &event)) {
	result = TCL_ERROR;
    }
    Tk_DeleteErrorHandler(handler);
    XSync(display, False);
    if (result != TCL_OK) {
	fprintf(stderr, "XSendEvent response to drop: Protocol failed\n");
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWindowZOrder --
 *
 *	Returns a chain of the child windows according to their stacking
 *	order. The window ids are ordered from top to bottom.
 *
 * ------------------------------------------------------------------------ 
 */
static Blt_Chain *
GetWindowZOrder(display, window)
    Display *display;
    Window window;
{
    Blt_Chain *chainPtr;
    Window *childArr;
    unsigned int nChildren;
    Window dummy;

    chainPtr = NULL;
    if ((XQueryTree(display, window, &dummy, &dummy, &childArr, &nChildren)) &&
	(nChildren > 0)) {
	register int i;

	chainPtr = Blt_ChainCreate();
	for (i = 0; i < nChildren; i++) {
	    /* 
	     * XQuery returns windows in bottom to top order.  We only care
	     * about the top window.  
	     */
	    Blt_ChainPrepend(chainPtr, (ClientData)childArr[i]);
	}
	if (childArr != NULL) {
	    XFree((char *)childArr);	/* done with list of kids */
	}
    }
    return chainPtr;
}

static int
GetMaxPropertySize(display)
    Display *display;
{
    int size;

    size = Blt_MaxRequestSize(display, sizeof(char));
    size -= 32;
    return size;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetProperty --
 *
 *	Returns the data associated with the named property on the
 *	given window.  All data is assumed to be 8-bit string data.
 *
 * ------------------------------------------------------------------------ 
 */
static char *
GetProperty(display, window, atom)
    Display *display;
    Window window;
    Atom atom;
{
    char *data;
    int result, format;
    Atom typeAtom;
    unsigned long nItems, bytesAfter;

    if (window == None) {
	return NULL;
    }
    data = NULL;
    result = XGetWindowProperty(
        display,		/* Display of window. */
	window,			/* Window holding the property. */
        atom,			/* Name of property. */
        0,			/* Offset of data (for multiple reads). */
	GetMaxPropertySize(display), /* Maximum number of items to read. */
	False,			/* If true, delete the property. */
        XA_STRING,		/* Desired type of property. */
        &typeAtom,		/* (out) Actual type of the property. */
        &format,		/* (out) Actual format of the property. */
        &nItems,		/* (out) # of items in specified format. */
        &bytesAfter,		/* (out) # of bytes remaining to be read. */
	(unsigned char **)&data);
    if ((result != Success) || (format != 8) || (typeAtom != XA_STRING)) {
	if (data != NULL) {
	    XFree((char *)data);
	    data = NULL;
	}
    }
    return data;
}

/*
 * ------------------------------------------------------------------------
 *
 *  SetProperty --
 *
 *	Associates the given data with the a property on a given window.
 *	All data is assumed to be 8-bit string data.
 *
 * ------------------------------------------------------------------------ 
 */
static void
SetProperty(tkwin, atom, data)
    Tk_Window tkwin;
    Atom atom;
    char *data;
{
    XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), atom, XA_STRING,
	8, PropModeReplace, (unsigned char *)data, strlen(data) + 1);
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWindowRegion --
 *
 *	Queries for the upper-left and lower-right corners of the 
 *	given window.  
 *
 *  Results:
 *	Returns if the window is currently viewable.  The coordinates
 *	of the window are returned via parameters.
 *
 * ------------------------------------------------------------------------ 
 */
static int
GetWindowRegion(display, windowPtr)
    Display *display;
    Winfo *windowPtr;
{
    XWindowAttributes winAttrs;

    if (XGetWindowAttributes(display, windowPtr->window, &winAttrs)) {
	windowPtr->x1 = winAttrs.x;
	windowPtr->y1 = winAttrs.y;
	windowPtr->x2 = winAttrs.x + winAttrs.width - 1;
	windowPtr->y2 = winAttrs.y + winAttrs.height - 1;
    }
    return (winAttrs.map_state == IsViewable);
}

/*
 * ------------------------------------------------------------------------
 *
 *  FindTopWindow --
 *
 *	Searches for the topmost window at a given pair of X-Y coordinates.
 *
 *  Results:
 *	Returns a pointer to the node representing the window containing
 *	the point.  If one can't be found, NULL is returned.
 *
 * ------------------------------------------------------------------------
 */
static Winfo *
FindTopWindow(dndPtr, x, y)
    Dnd *dndPtr;
    int x, y;
{
    Winfo *rootPtr;
    register Blt_ChainLink *linkPtr;
    register Winfo *windowPtr;

    rootPtr = dndPtr->rootPtr;
    if (!rootPtr->initialized) {
	GetWinfo(dndPtr->display, rootPtr);
    }
    if ((x < rootPtr->x1) || (x > rootPtr->x2) ||
	(y < rootPtr->y1) || (y > rootPtr->y2)) {
	return NULL;		/* Point is not over window  */
    }
    windowPtr = rootPtr;

    /*  
     * The window list is ordered top to bottom, so stop when we find the
     * first child that contains the X-Y coordinate. It will be the topmost
     * window in that hierarchy.  If none exists, then we already have the
     * topmost window.  
     */
  descend:
    for (linkPtr = Blt_ChainFirstLink(rootPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rootPtr = Blt_ChainGetValue(linkPtr);
	if (!rootPtr->initialized) {
	    GetWinfo(dndPtr->display, rootPtr);
	}
	if (rootPtr->window == Blt_GetRealWindowId(dndPtr->tokenPtr->tkwin)) {
	    continue;		/* Don't examine the token window. */
	}
	if ((x >= rootPtr->x1) && (x <= rootPtr->x2) &&
	    (y >= rootPtr->y1) && (y <= rootPtr->y2)) {
	    /*   
	     * Remember the last window containing the pointer and descend
	     * into its window hierarchy. We'll look for a child that also
	     * contains the pointer.  
	     */
	    windowPtr = rootPtr;
	    goto descend;
	}
    }
    return windowPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWidgetCursor --
 *
 *	Queries a widget for its current cursor.   The given window
 *	may or may not be a Tk widget that has a -cursor option. 
 *
 *  Results:
 *	Returns the current cursor of the widget.
 *
 * ------------------------------------------------------------------------ 
 */
static Tk_Cursor
GetWidgetCursor(interp, tkwin)
    Tcl_Interp *interp;		/* Interpreter to evaluate widget command. */
    Tk_Window tkwin;		/* Window of drag&drop source. */
{
    Tk_Cursor cursor;
    Tcl_DString dString, savedResult;

    cursor = None;
    Tcl_DStringInit(&dString);
    Blt_DStringAppendElements(&dString, Tk_PathName(tkwin), "cget", "-cursor",
	      (char *)NULL);
    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    if (Tcl_GlobalEval(interp, Tcl_DStringValue(&dString)) == TCL_OK) {
	char *name;

	name = Tcl_GetStringResult(interp);
	if ((name != NULL) && (name[0] != '\0')) {
	    cursor = Tk_GetCursor(interp, tkwin, Tk_GetUid(name));
	}
    }
    Tcl_DStringResult(interp, &savedResult);
    Tcl_DStringFree(&dString);
    return cursor;
}

/*
 * ------------------------------------------------------------------------
 *
 *  NameOfStatus --
 *
 *	Converts a numeric drop result into its string representation.
 *
 *  Results:
 *	Returns a static string representing the drop result.
 *
 * ------------------------------------------------------------------------
 */
static char *
NameOfStatus(status)
    int status;
{
    switch (status) {
    case DROP_OK:
	return "active";
    case DROP_CONTINUE:
	return "normal";
    case DROP_FAIL:
	return "reject";
    case DROP_CANCEL:
	return "cancel";
    default:
	return "unknown status value";
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  NameOfAction --
 *
 *	Converts a numeric drop result into its string representation.
 *
 *  Results:
 *	Returns a static string representing the drop result.
 *
 * ------------------------------------------------------------------------
 */
static char *
NameOfAction(action)
    int action;
{
    switch (action) {
    case DROP_COPY:
	return "copy";
    case DROP_CANCEL:
	return "cancel";
    case DROP_MOVE:
	return "move";
	break;
    case DROP_LINK:
	return "link";
    case DROP_FAIL:
	return "fail";
    default:
	return "unknown action";
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetAction --
 *
 *	Converts a string to its numeric drop result value.
 *
 *  Results:
 *	Returns the drop result.
 *
 * ------------------------------------------------------------------------
 */
static int
GetAction(string)
    char *string;
{
    char c;

    c = string[0];
    if ((c == 'c') && (strcmp(string, "cancel") == 0)) {
	return DROP_CANCEL;
    } else if ((c == 'f') && (strcmp(string, "fail") == 0)) {
	return DROP_FAIL;
    } else if ((c == 'm') && (strcmp(string, "move") == 0)) {
	return DROP_MOVE;
    } else if ((c == 'l') && (strcmp(string, "link") == 0)) {
	return DROP_LINK;
    } else if ((c == 'c') && (strcmp(string, "copy") == 0)) {
	return DROP_COPY;
    } else {
	return DROP_COPY;
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetDragResult --
 *
 *	Converts a string to its numeric drag result value.
 *
 *  Results:
 *	Returns the drag result.
 *
 * ------------------------------------------------------------------------
 */
static int
GetDragResult(interp, string)
    Tcl_Interp *interp;
    char *string;
{
    char c;
    int bool;

    c = string[0];
    if ((c == 'c') && (strcmp(string, "cancel") == 0)) {
	return DROP_CANCEL;
    } else if (Tcl_GetBoolean(interp, string, &bool) != TCL_OK) {
	Tcl_BackgroundError(interp);
	return DROP_CANCEL;
    }
    return bool;
}

static void
AnimateActiveCursor(clientData)
    ClientData clientData;
{
    Dnd *dndPtr = clientData;    
    Tk_Cursor cursor;

    dndPtr->cursorPos++;
    cursor = dndPtr->cursors[dndPtr->cursorPos];
    if (cursor == None) {
	cursor = dndPtr->cursors[1];
	dndPtr->cursorPos = 1;
    }
    Tk_DefineCursor(dndPtr->tkwin, cursor);
    dndPtr->timerToken = Tcl_CreateTimerHandler(100, AnimateActiveCursor,
	    dndPtr);
}

static void
StartActiveCursor(dndPtr)
    Dnd *dndPtr;    
{
    if (dndPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(dndPtr->timerToken);
    }
    if (dndPtr->cursors != NULL) {
	Tk_Cursor cursor;

	dndPtr->cursorPos = 1;
	cursor = dndPtr->cursors[1];
	if (cursor != None) {
	    Tk_DefineCursor(dndPtr->tkwin, cursor);
	    dndPtr->timerToken = Tcl_CreateTimerHandler(125, 
		AnimateActiveCursor, dndPtr);
	}
    }
}

static void
StopActiveCursor(dndPtr)
    Dnd *dndPtr;    
{
    if (dndPtr->cursorPos > 0) {
	dndPtr->cursorPos = 0;
    }
    if (dndPtr->cursors != NULL) {
	Tk_DefineCursor(dndPtr->tkwin, dndPtr->cursors[0]);
    }
    if (dndPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(dndPtr->timerToken);
	dndPtr->timerToken = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedrawToken --
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
static void
EventuallyRedrawToken(dndPtr)
    Dnd *dndPtr;
{
    Token *tokenPtr;

    if (dndPtr->tokenPtr == NULL) {
	return;
    }
    tokenPtr = dndPtr->tokenPtr;
    if ((tokenPtr->tkwin != NULL) && (tokenPtr->tkwin != NULL) && 
	!(tokenPtr->flags & TOKEN_REDRAW)) {
	tokenPtr->flags |= TOKEN_REDRAW;
	Tcl_DoWhenIdle(DisplayToken, dndPtr);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  RaiseToken --
 *
 * ------------------------------------------------------------------------
 */
static void
RaiseToken(dndPtr)
    Dnd *dndPtr;
{
    Token *tokenPtr = dndPtr->tokenPtr;

    if (dndPtr->flags & DND_INITIATED) {
	if ((Tk_Width(tokenPtr->tkwin) != Tk_ReqWidth(tokenPtr->tkwin)) ||
	    (Tk_Height(tokenPtr->tkwin) != Tk_ReqHeight(tokenPtr->tkwin))) {
	    Blt_ResizeToplevel(tokenPtr->tkwin, 
		Tk_ReqWidth(tokenPtr->tkwin),
		Tk_ReqHeight(tokenPtr->tkwin));
	}
	Blt_MapToplevel(tokenPtr->tkwin);
	Blt_RaiseToplevel(tokenPtr->tkwin);
    }
}



/*
 * ------------------------------------------------------------------------
 *
 *  DisplayToken --
 *
 * ------------------------------------------------------------------------
 */
static void
DisplayToken(clientData)
    ClientData clientData;
{
    Dnd *dndPtr = clientData;
    Token *tokenPtr = dndPtr->tokenPtr;
    int relief;
    Tk_3DBorder border;
    int borderWidth;

    tokenPtr->flags &= ~TOKEN_REDRAW;
    if (tokenPtr->status == DROP_OK) {
	relief = tokenPtr->activeRelief;
	border = tokenPtr->activeBorder;
	borderWidth = tokenPtr->activeBorderWidth;
	if ((dndPtr->cursors != NULL) && (dndPtr->cursorPos == 0)) {
	    StartActiveCursor(dndPtr);
	}
    } else {
	relief = tokenPtr->relief;
	border = tokenPtr->normalBorder;
	borderWidth = tokenPtr->borderWidth;
	StopActiveCursor(dndPtr);
    } 
    Blt_Fill3DRectangle(tokenPtr->tkwin, Tk_WindowId(tokenPtr->tkwin), border, 
	0, 0, Tk_Width(tokenPtr->tkwin), Tk_Height(tokenPtr->tkwin), 
	borderWidth, relief);
    tokenPtr->lastStatus = tokenPtr->status;
    if (tokenPtr->status == DROP_FAIL) {
	DrawRejectSymbol(dndPtr);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  FadeToken --
 *
 *	Fades the token into the target.
 *
 * ------------------------------------------------------------------------
 */
static void
FadeToken(dndPtr)
    Dnd *dndPtr;		/* Drag-and-drop manager (source). */
{ 
    Token *tokenPtr = dndPtr->tokenPtr;
    int w, h;
    int dx, dy;
    Window window;

    if (tokenPtr->status == DROP_FAIL) {
	tokenPtr->nSteps = 1;
	return;
    }
    if (tokenPtr->nSteps == 1) {
	HideToken(dndPtr);
	dndPtr->flags &= ~(DND_ACTIVE | DND_VOIDED);
	return;
    }
    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    tokenPtr->timerToken = Tcl_CreateTimerHandler(10, 
		  (Tcl_TimerProc *)FadeToken, dndPtr);
    tokenPtr->nSteps--;

    w = Tk_ReqWidth(tokenPtr->tkwin) * tokenPtr->nSteps / 10;
    h = Tk_ReqHeight(tokenPtr->tkwin) * tokenPtr->nSteps / 10;
    if (w < 1) {
	w = 1;
    } 
    if (h < 1) {
	h = 1;
    }
    dx = (Tk_ReqWidth(tokenPtr->tkwin) - w) / 2;
    dy = (Tk_ReqHeight(tokenPtr->tkwin) - h) / 2;
    window = Blt_GetRealWindowId(tokenPtr->tkwin);
    XMoveResizeWindow(dndPtr->display, window, tokenPtr->x + dx, 
	     tokenPtr->y + dy, (unsigned int)w, (unsigned int)h);
    tokenPtr->width = w, tokenPtr->height = h;
}

/*
 * ------------------------------------------------------------------------
 *
 *  SnapToken --
 *
 *	Snaps the token back to the source.
 *
 * ------------------------------------------------------------------------
 */
static void
SnapToken(dndPtr)
    Dnd *dndPtr;		/* drag&drop source window data */
{ 
    Token *tokenPtr = dndPtr->tokenPtr;

    if (tokenPtr->nSteps == 1) {
	HideToken(dndPtr);
	return;
    }
    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    tokenPtr->timerToken = Tcl_CreateTimerHandler(10, 
	(Tcl_TimerProc *)SnapToken, dndPtr);
    tokenPtr->nSteps--;
    tokenPtr->x -= (tokenPtr->x - tokenPtr->startX) / tokenPtr->nSteps;
    tokenPtr->y -= (tokenPtr->y - tokenPtr->startY) / tokenPtr->nSteps;
    if ((tokenPtr->x != Tk_X(tokenPtr->tkwin)) || 
	(tokenPtr->y != Tk_Y(tokenPtr->tkwin))) {
	Tk_MoveToplevelWindow(tokenPtr->tkwin, tokenPtr->x, tokenPtr->y);
    }
    RaiseToken(dndPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  HideToken --
 *
 *	Unmaps the drag&drop token.  Invoked directly at the end of a
 *	successful communication, or after a delay if the communication
 *	fails (allowing the user to see a graphical picture of failure).
 *
 * ------------------------------------------------------------------------
 */
static void
HideToken(dndPtr)
    Dnd *dndPtr;
{
    Token *tokenPtr = dndPtr->tokenPtr;

    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
	tokenPtr->timerToken = NULL;
    }
    if (dndPtr->flags & DND_INITIATED) {
	/* Reset the cursor back to its normal state.  */
	StopActiveCursor(dndPtr);
	if (dndPtr->cursor == None) {
	    Tk_UndefineCursor(dndPtr->tkwin);
	} else {
	    Tk_DefineCursor(dndPtr->tkwin, dndPtr->cursor);
	}
	if (tokenPtr->tkwin != NULL) {
	    Tk_UnmapWindow(tokenPtr->tkwin); 
	    Blt_ResizeToplevel(tokenPtr->tkwin, 
			Tk_ReqWidth(tokenPtr->tkwin), 
			Tk_ReqHeight(tokenPtr->tkwin));
	}
    }
    if (dndPtr->rootPtr != NULL) {
	FreeWinfo(dndPtr->rootPtr);
	dndPtr->rootPtr = NULL;
    }
    dndPtr->flags &= ~(DND_ACTIVE | DND_VOIDED);
    tokenPtr->status = DROP_CONTINUE;
}

/*
 * ------------------------------------------------------------------------
 *
 *  MorphToken --
 *
 *	Fades the token into the target.
 *
 * ------------------------------------------------------------------------
 */
static void
MorphToken(dndPtr)
    Dnd *dndPtr;		/* Drag-and-drop manager (source). */
{ 
    Token *tokenPtr = dndPtr->tokenPtr;

    if (tokenPtr->status == DROP_FAIL) {
	tokenPtr->nSteps = 1;
	return;
    }
    if (tokenPtr->nSteps == 1) {
	HideToken(dndPtr);
	dndPtr->flags &= ~(DND_ACTIVE | DND_VOIDED);
	return;
    }
    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    tokenPtr->timerToken = Tcl_CreateTimerHandler(10, 
	(Tcl_TimerProc *)MorphToken, dndPtr);
    tokenPtr->nSteps--;

    if (dndPtr->flags & DROP_CANCEL) {
	tokenPtr->nSteps--;
	tokenPtr->x -= (tokenPtr->x - tokenPtr->startX) / tokenPtr->nSteps;
	tokenPtr->y -= (tokenPtr->y - tokenPtr->startY) / tokenPtr->nSteps;
	if ((tokenPtr->x != Tk_X(tokenPtr->tkwin)) || 
	    (tokenPtr->y != Tk_Y(tokenPtr->tkwin))) {
	    Tk_MoveToplevelWindow(tokenPtr->tkwin, tokenPtr->x, tokenPtr->y);
	}
    } else {
	int w, h;
	int dx, dy;
	Window window;

	w = Tk_ReqWidth(tokenPtr->tkwin) * tokenPtr->nSteps / 10;
	h = Tk_ReqHeight(tokenPtr->tkwin) * tokenPtr->nSteps / 10;
	if (w < 1) {
	    w = 1;
	} 
	if (h < 1) {
	    h = 1;
	}
	dx = (Tk_ReqWidth(tokenPtr->tkwin) - w) / 2;
	dy = (Tk_ReqHeight(tokenPtr->tkwin) - h) / 2;
	window = Blt_GetRealWindowId(tokenPtr->tkwin);
	XMoveResizeWindow(dndPtr->display, window, tokenPtr->x + dx, 
			  tokenPtr->y + dy, (unsigned int)w, (unsigned int)h);
	tokenPtr->width = w, tokenPtr->height = h;
    }
    RaiseToken(dndPtr);
}

static void
GetTokenPosition(dndPtr, x, y)
    Dnd *dndPtr;
    int x, y;
{ 
    Token *tokenPtr = dndPtr->tokenPtr;
    int maxX, maxY;
    int vx, vy, dummy;
    Screen *screenPtr;

    /* Adjust current location for virtual root windows.  */
    Tk_GetVRootGeometry(dndPtr->tkwin, &vx, &vy, &dummy, &dummy);
    x += vx - TOKEN_OFFSET;
    y += vy - TOKEN_OFFSET;

    screenPtr = Tk_Screen(tokenPtr->tkwin);
    maxX = WidthOfScreen(screenPtr) - Tk_Width(tokenPtr->tkwin);
    maxY = HeightOfScreen(screenPtr) - Tk_Height(tokenPtr->tkwin);
    Blt_TranslateAnchor(x, y, Tk_Width(tokenPtr->tkwin),
	Tk_Height(tokenPtr->tkwin), tokenPtr->anchor, &x, &y);
    if (x > maxX) {
	x = maxX;
    } else if (x < 0) {
	x = 0;
    }
    if (y > maxY) {
	y = maxY;
    } else if (y < 0) {
	y = 0;
    }
    tokenPtr->x = x, tokenPtr->y  = y;
}

/*
 * ------------------------------------------------------------------------
 *
 *  MoveToken --
 *
 *	Invoked during "drag" operations to move a token window to its
 *	current "drag" coordinate.
 *
 * ------------------------------------------------------------------------
 */
static void
MoveToken(dndPtr)
    Dnd *dndPtr;		/* drag&drop source window data */
{ 
    Token *tokenPtr = dndPtr->tokenPtr;

    GetTokenPosition(dndPtr, dndPtr->x, dndPtr->y);
    if ((tokenPtr->x != Tk_X(tokenPtr->tkwin)) || 
	(tokenPtr->y != Tk_Y(tokenPtr->tkwin))) {
	Tk_MoveToplevelWindow(tokenPtr->tkwin, tokenPtr->x, tokenPtr->y);
    }
}


/*
 * ------------------------------------------------------------------------
 *
 *  ChangeToken --
 *
 *	Invoked when the event loop is idle to determine whether or not
 *	the current drag&drop token position is over another drag&drop
 *	target.
 *
 * ------------------------------------------------------------------------
 */
static void
ChangeToken(dndPtr, status)
    Dnd *dndPtr;
    int status;
{
    Token *tokenPtr = dndPtr->tokenPtr;

    tokenPtr->status = status;
    EventuallyRedrawToken(dndPtr);

    /*
     *  If the source has a site command, then invoke it to
     *  modify the appearance of the token window.  Pass any
     *  errors onto the drag&drop error handler.
     */
    if (dndPtr->siteCmd) {
	Tcl_Interp *interp = dndPtr->interp;
	Tcl_DString dString, savedResult;
	char **p;
	
	Tcl_DStringInit(&dString);
	for (p = dndPtr->siteCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&dString, *p);
	}
	Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
	Tcl_DStringAppendElement(&dString, "timestamp");
	Tcl_DStringAppendElement(&dString, Blt_Utoa(dndPtr->timestamp));
	Tcl_DStringAppendElement(&dString, "status");
	Tcl_DStringAppendElement(&dString, NameOfStatus(status));
	Tcl_DStringInit(&savedResult);
	Tcl_DStringGetResult(interp, &savedResult);
	if (Tcl_GlobalEval(interp, Tcl_DStringValue(&dString)) != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	Tcl_DStringFree(&dString);
	Tcl_DStringResult(interp, &savedResult);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  DrawRejectSymbol --
 *
 *	Draws a rejection mark on the current drag&drop token, and arranges
 *	for the token to be unmapped after a small delay.
 *
 * ------------------------------------------------------------------------
 */
static void
DrawRejectSymbol(dndPtr)
    Dnd *dndPtr;
{
    Token *tokenPtr = dndPtr->tokenPtr;
    int divisor = 6;		/* controls size of rejection symbol */
    int w, h, lineWidth, x, y, margin;

    margin = 2 * tokenPtr->borderWidth;
    w = Tk_Width(tokenPtr->tkwin) - 2 * margin;
    h = Tk_Height(tokenPtr->tkwin) - 2 * margin;
    lineWidth = (w < h) ? w / divisor : h / divisor;
    lineWidth = (lineWidth < 1) ? 1 : lineWidth;

    w = h = lineWidth * (divisor - 1);
    x = (Tk_Width(tokenPtr->tkwin) - w) / 2;
    y = (Tk_Height(tokenPtr->tkwin) - h) / 2;

    /*
     *  Draw the rejection symbol background (\) on the token window...
     */
    XSetLineAttributes(Tk_Display(tokenPtr->tkwin), tokenPtr->outlineGC,
	lineWidth + 2, LineSolid, CapButt, JoinBevel);

    XDrawArc(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->outlineGC, x, y, w, h, 0, 23040);

    XDrawLine(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->outlineGC, x + lineWidth, y + lineWidth, x + w - lineWidth,
	y + h - lineWidth);

    /*
     *  Draw the rejection symbol foreground (\) on the token window...
     */
    XSetLineAttributes(Tk_Display(tokenPtr->tkwin), tokenPtr->fillGC,
	lineWidth, LineSolid, CapButt, JoinBevel);

    XDrawArc(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->fillGC, x, y, w, h, 0, 23040);

    XDrawLine(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->fillGC, x + lineWidth, y + lineWidth, x + w - lineWidth,
	y + h - lineWidth);

    tokenPtr->status = DROP_FAIL;
    /*
     *  Arrange for token window to disappear eventually.
     */
    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    tokenPtr->timerToken = Tcl_CreateTimerHandler(1000, 
	(Tcl_TimerProc *)HideToken, dndPtr);
    RaiseToken(dndPtr);
    dndPtr->flags &= ~(DND_ACTIVE | DND_VOIDED);
}

/*
 * ------------------------------------------------------------------------
 *
 *  CreateToken --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Creates a new record if the widget name is not already
 *	registered.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static void
DestroyToken(data)
    DestroyData data;
{
    Dnd *dndPtr = (Dnd *)data;
    Token *tokenPtr = dndPtr->tokenPtr;

    dndPtr->tokenPtr = NULL;
    if (tokenPtr == NULL) {
	return;
    }
    if (tokenPtr->flags & TOKEN_REDRAW) {
	Tcl_CancelIdleCall(DisplayToken, dndPtr);
    }
    Tk_FreeOptions(tokenConfigSpecs, (char *)tokenPtr, dndPtr->display, 0);
    if (tokenPtr->timerToken) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    if (tokenPtr->fillGC != NULL) {
	Tk_FreeGC(dndPtr->display, tokenPtr->fillGC);
    }
    if (tokenPtr->outlineGC != NULL) {
	Tk_FreeGC(dndPtr->display, tokenPtr->outlineGC);
    }
    if (tokenPtr->tkwin != NULL) {
	Tk_DeleteEventHandler(tokenPtr->tkwin, 
	      ExposureMask | StructureNotifyMask, TokenEventProc, dndPtr);
	Tk_DestroyWindow(tokenPtr->tkwin);
    }
    Blt_Free(tokenPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  TokenEventProc --
 *
 *	Invoked by the Tk dispatcher to handle widget events.
 *	Manages redraws for the drag&drop token window.
 *
 * ------------------------------------------------------------------------
 */
static void
TokenEventProc(clientData, eventPtr)
    ClientData clientData;	/* data associated with widget */
    XEvent *eventPtr;		/* information about event */
{
    Dnd *dndPtr = clientData;
    Token *tokenPtr = dndPtr->tokenPtr;

    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	if (tokenPtr->tkwin != NULL) {
	    EventuallyRedrawToken(dndPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	tokenPtr->tkwin = NULL;
	if (tokenPtr->flags & TOKEN_REDRAW) {
	    tokenPtr->flags &= ~TOKEN_REDRAW;
	    Tcl_CancelIdleCall(DisplayToken, dndPtr);
	}
	Tcl_EventuallyFree(dndPtr, DestroyToken);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  CreateToken --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Creates a new record if the widget name is not already
 *	registered.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static int
CreateToken(interp, dndPtr)
    Tcl_Interp *interp;
    Dnd *dndPtr;
{
    XSetWindowAttributes attrs;
    Tk_Window tkwin;
    unsigned int mask;
    Token *tokenPtr;

    tokenPtr = Blt_Calloc(1, sizeof(Token));
    assert(tokenPtr);
    tokenPtr->anchor = TK_ANCHOR_SE;
    tokenPtr->relief = TK_RELIEF_RAISED;
    tokenPtr->activeRelief = TK_RELIEF_SUNKEN;
    tokenPtr->borderWidth = tokenPtr->activeBorderWidth = 3;

    /* Create toplevel on parent's screen. */
    tkwin = Tk_CreateWindow(interp, dndPtr->tkwin, "dndtoken", "");
    if (tkwin == NULL) {
	Blt_Free(tokenPtr);
	return TCL_ERROR;
    }
    tokenPtr->tkwin = tkwin;
    Tk_SetClass(tkwin, "DndToken"); 
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	TokenEventProc, dndPtr);
    attrs.override_redirect = True;
    attrs.backing_store = WhenMapped;
    attrs.save_under = True;
    mask = CWOverrideRedirect | CWSaveUnder | CWBackingStore;
    Tk_ChangeWindowAttributes(tkwin, mask, &attrs);
    Tk_SetInternalBorder(tkwin, tokenPtr->borderWidth + 2);
    Tk_MakeWindowExist(tkwin);
    dndPtr->tokenPtr = tokenPtr;
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  ConfigureToken --
 *
 *	Called to process an (argc,argv) list to configure (or
 *	reconfigure) a drag&drop source widget.
 *
 * ------------------------------------------------------------------------
 */
static int
ConfigureToken(interp, dndPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* current interpreter */
    Dnd *dndPtr;		/* Drag&drop source widget record */
    int argc;			/* number of arguments */
    char **argv;		/* argument strings */
    int flags;			/* flags controlling interpretation */
{
    GC newGC;
    Token *tokenPtr = dndPtr->tokenPtr;
    XGCValues gcValues;
    unsigned long gcMask;

    Tk_MakeWindowExist(tokenPtr->tkwin);
    if (Tk_ConfigureWidget(interp, tokenPtr->tkwin, tokenConfigSpecs, argc, 
		argv, (char *)tokenPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     *  Set up the rejection outline GC for the token window...
     */
    gcValues.foreground = tokenPtr->outlineColor->pixel;
    gcValues.subwindow_mode = IncludeInferiors;
    gcValues.graphics_exposures = False;
    gcValues.line_style = LineSolid;
    gcValues.cap_style = CapButt;
    gcValues.join_style = JoinBevel;

    gcMask = GCForeground | GCSubwindowMode | GCLineStyle |
	GCCapStyle | GCJoinStyle | GCGraphicsExposures;
    newGC = Tk_GetGC(dndPtr->tkwin, gcMask, &gcValues);

    if (tokenPtr->outlineGC != NULL) {
	Tk_FreeGC(dndPtr->display, tokenPtr->outlineGC);
    }
    tokenPtr->outlineGC = newGC;

    /*
     *  Set up the rejection fill GC for the token window...
     */
    gcValues.foreground = tokenPtr->fillColor->pixel;
    if (tokenPtr->rejectStipple != None) {
	gcValues.stipple = tokenPtr->rejectStipple;
	gcValues.fill_style = FillStippled;
	gcMask |= GCStipple | GCFillStyle;
    }
    newGC = Tk_GetGC(dndPtr->tkwin, gcMask, &gcValues);

    if (tokenPtr->fillGC != NULL) {
	Tk_FreeGC(dndPtr->display, tokenPtr->fillGC);
    }
    tokenPtr->fillGC = newGC;

    if ((tokenPtr->reqWidth > 0) && (tokenPtr->reqHeight > 0)) {
	Tk_GeometryRequest(tokenPtr->tkwin, tokenPtr->reqWidth, 
		tokenPtr->reqHeight);
    }
    /*
     *  Reset the border width in case it has changed...
     */
    Tk_SetInternalBorder(tokenPtr->tkwin, tokenPtr->borderWidth + 2);
    return TCL_OK;
}

static int
GetFormattedData(dndPtr, format, timestamp, resultPtr)
    Dnd *dndPtr;
    char *format;
    int timestamp;
    Tcl_DString *resultPtr;
{
    Tcl_Interp *interp = dndPtr->interp;
    Blt_HashEntry *hPtr;
    char **formatCmd;
    Tcl_DString savedResult;
    Tcl_DString dString;
    char **p;
    int x, y;

    /* Find the data converter for the prescribed format. */
    hPtr = Blt_FindHashEntry(&(dndPtr->getDataTable), format);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find format \"", format, 
	 "\" in source \"", Tk_PathName(dndPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    formatCmd = (char **)Blt_GetHashValue(hPtr);
    Tcl_DStringInit(&dString);
    for (p = formatCmd; *p != NULL; p++) {
	Tcl_DStringAppendElement(&dString, *p);
    }
    x = dndPtr->dragX - Blt_RootX(dndPtr->tkwin);
    y = dndPtr->dragY - Blt_RootY(dndPtr->tkwin);
    Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
    Tcl_DStringAppendElement(&dString, "x");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(x));
    Tcl_DStringAppendElement(&dString, "y");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(y));
    Tcl_DStringAppendElement(&dString, "timestamp");
    Tcl_DStringAppendElement(&dString, Blt_Utoa(timestamp));
    Tcl_DStringAppendElement(&dString, "format");
    Tcl_DStringAppendElement(&dString, format);
    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    if (Tcl_GlobalEval(interp, Tcl_DStringValue(&dString)) != TCL_OK) {
	Tcl_BackgroundError(interp);
    }
    Tcl_DStringFree(&dString);
    Tcl_DStringInit(resultPtr);
    Tcl_DStringGetResult(interp, resultPtr);

    /* Restore the interpreter result. */
    Tcl_DStringResult(interp, &savedResult);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *
 *  DestroyDnd --
 *
 *	Free resources allocated for the drag&drop window.
 *
 * ------------------------------------------------------------------------
 */
static void
DestroyDnd(data)
    DestroyData data;
{
    Dnd *dndPtr = (Dnd *)data;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char **cmd;

    Tk_FreeOptions(configSpecs, (char *)dndPtr, dndPtr->display, 0);
    Tk_DeleteGenericHandler(DndEventProc, dndPtr);
    for (hPtr = Blt_FirstHashEntry(&(dndPtr->getDataTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	cmd = (char **)Blt_GetHashValue(hPtr);
	if (cmd != NULL) {
	    Blt_Free(cmd);
	}
    }
    Blt_DeleteHashTable(&(dndPtr->getDataTable));

    for (hPtr = Blt_FirstHashEntry(&(dndPtr->setDataTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	cmd = (char **)Blt_GetHashValue(hPtr);
	if (cmd != NULL) {
	    Blt_Free(cmd);
	}
    }
    Blt_DeleteHashTable(&(dndPtr->setDataTable));
    if (dndPtr->rootPtr != NULL) {
	FreeWinfo(dndPtr->rootPtr);
    }
    if (dndPtr->cursor != None) {
	Tk_FreeCursor(dndPtr->display, dndPtr->cursor);
    }
    if (dndPtr->reqFormats != NULL) {
	Blt_Free(dndPtr->reqFormats);
    }
    if (dndPtr->matchingFormats != NULL) {
	Blt_Free(dndPtr->matchingFormats);
    }

    /* Now that the various commands are custom list options, we need
     * to explicitly free them. */
    if (dndPtr->motionCmd != NULL) {
	Blt_Free(dndPtr->motionCmd);
    }
    if (dndPtr->leaveCmd != NULL) {
	Blt_Free(dndPtr->leaveCmd);
    }
    if (dndPtr->enterCmd != NULL) {
	Blt_Free(dndPtr->enterCmd);
    }
    if (dndPtr->dropCmd != NULL) {
	Blt_Free(dndPtr->dropCmd);
    }
    if (dndPtr->resultCmd != NULL) {
	Blt_Free(dndPtr->resultCmd);
    }
    if (dndPtr->packageCmd != NULL) {
	Blt_Free(dndPtr->packageCmd);
    }
    if (dndPtr->siteCmd != NULL) {
	Blt_Free(dndPtr->siteCmd);
    }

    if (dndPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&(dndPtr->dataPtr->dndTable), dndPtr->hashPtr);
    }
    if (dndPtr->tokenPtr != NULL) {
	DestroyToken((DestroyData)dndPtr);
    }
    if (dndPtr->tkwin != NULL) {
	XDeleteProperty(dndPtr->display, Tk_WindowId(dndPtr->tkwin),
			dndPtr->dataPtr->targetAtom);
	XDeleteProperty(dndPtr->display, Tk_WindowId(dndPtr->tkwin),
			dndPtr->dataPtr->commAtom);
    }
    Blt_Free(dndPtr);
}


/*
 * ------------------------------------------------------------------------
 *
 *  GetDnd --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static int
GetDnd(clientData, interp, pathName, dndPtrPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    char *pathName;		/* widget pathname for desired record */
    Dnd **dndPtrPtr;
{
    DndInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, dataPtr->mainWindow);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&(dataPtr->dndTable), (char *)tkwin);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "window \"", pathName,
	     "\" is not a drag&drop source/target", (char *)NULL);
	return TCL_ERROR;
    }
    *dndPtrPtr = (Dnd *)Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  CreateDnd --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Creates a new record if the widget name is not already
 *	registered.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static Dnd *
CreateDnd(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* widget for desired record */
{
    Dnd *dndPtr;

    dndPtr = Blt_Calloc(1, sizeof(Dnd));
    assert(dndPtr);
    dndPtr->interp = interp;
    dndPtr->display = Tk_Display(tkwin);
    dndPtr->tkwin = tkwin;
    Tk_MakeWindowExist(tkwin);
    Blt_InitHashTable(&(dndPtr->setDataTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(dndPtr->getDataTable), BLT_STRING_KEYS);
    Tk_CreateGenericHandler(DndEventProc, dndPtr);
    return dndPtr;
}

static int
ConfigureDnd(interp, dndPtr)
    Tcl_Interp *interp;
    Dnd *dndPtr;
{
    Tcl_CmdInfo cmdInfo;
    Tcl_DString dString;
    int button, result;

    if (!Tcl_GetCommandInfo(interp, "blt::DndInit", &cmdInfo)) {
	static char cmd[] = "source [file join $blt_library dnd.tcl]";
	/* 
	 * If the "DndInit" routine hasn't been sourced, do it now.
	 */
	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		     "\n    (while loading bindings for blt::drag&drop)");
	    return TCL_ERROR;
	}
    }
    /*  
     * Reset the target property if it's changed state or
     * added/subtracted one of its callback procedures.  
     */
    if (Blt_ConfigModified(configSpecs, "-target", "-onenter",  "-onmotion",
	   "-onleave", (char *)NULL)) {
	if (dndPtr->targetPropertyExists) {
	    XDeleteProperty(dndPtr->display, Tk_WindowId(dndPtr->tkwin),
			    dndPtr->dataPtr->targetAtom);
	    dndPtr->targetPropertyExists = FALSE;
	}
	if (dndPtr->isTarget) {
	    AddTargetProperty(dndPtr);
	    dndPtr->targetPropertyExists = TRUE;
	}
    }
    if (dndPtr->isSource) {
	/* Check the button binding for valid range (0 or 1-5) */
	if ((dndPtr->reqButton < 0) || (dndPtr->reqButton > 5)) {
	    Tcl_AppendResult(interp, 
			     "button must be 1-5, or 0 for no bindings",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	button = dndPtr->reqButton;
    } else {
	button = 0;
    }
    Tcl_DStringInit(&dString);
    Blt_DStringAppendElements(&dString, "blt::DndInit", 
	Tk_PathName(dndPtr->tkwin), Blt_Itoa(button), (char *)NULL);
    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SendRestrictProc --
 *
 *	This procedure filters incoming events when a "send" command
 *	is outstanding.  It defers all events except those containing
 *	send commands and results.
 *
 * Results:
 *	False is returned except for property-change events on a
 *	commWindow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static Tk_RestrictAction
SendRestrictProc(clientData, eventPtr)
    ClientData clientData;		/* Drag-and-drop manager. */
    register XEvent *eventPtr;		/* Event that just arrived. */
{
    Dnd *dndPtr = clientData;

    if (eventPtr->xproperty.window != Tk_WindowId(dndPtr->tkwin)) {
	return TK_PROCESS_EVENT; /* Event not in our window. */
    }
    if ((eventPtr->type == PropertyNotify) &&
	(eventPtr->xproperty.state == PropertyNewValue)) {
	return TK_PROCESS_EVENT; /* This is the one we want to process. */
    }
    if (eventPtr->type == Expose) {
	return TK_PROCESS_EVENT; /* Let expose events also get
				  * handled. */
    }
    return TK_DEFER_EVENT;	/* Defer everything else. */
}

/*
 *----------------------------------------------------------------------
 *
 * SendTimerProc --
 *
 *	Procedure called when the timer event elapses.  Used to wait
 *	between attempts checking for the designated window.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Sets a flag, indicating the timeout occurred.
 *
 *----------------------------------------------------------------------
 */
static void
SendTimerProc(clientData)
    ClientData clientData;
{
    int *statusPtr = clientData;

    /* 
     * An unusually long amount of time has elapsed since the drag
     * start message was sent.  Assume that the other party has died
     * and abort the operation.  
     */
    *statusPtr = DROP_FAIL;
}

#define WAIT_INTERVAL	2000	/* Twenty seconds. */

/*
 * ------------------------------------------------------------------------
 *
 *  TargetPropertyEventProc --
 *
 *	Invoked by the Tk dispatcher to handle widget events.
 *	Manages redraws for the drag&drop token window.
 *
 * ------------------------------------------------------------------------
 */
static void
TargetPropertyEventProc(clientData, eventPtr)
    ClientData clientData;	/* Data associated with transaction. */
    XEvent *eventPtr;		/* information about event */
{
    DropPending *pendingPtr = clientData;
    char *data;
    int result, format;
    Atom typeAtom;
    unsigned long nItems, bytesAfter;

#ifdef notdef
    fprintf(stderr, "TargetPropertyEventProc\n");
#endif
    if ((eventPtr->type != PropertyNotify) ||
	(eventPtr->xproperty.atom != pendingPtr->commAtom) || 
	(eventPtr->xproperty.state != PropertyNewValue)) {
	return;
    }
    Tcl_DeleteTimerHandler(pendingPtr->timerToken);
    data = NULL;
    result = XGetWindowProperty(
        eventPtr->xproperty.display,	/* Display of window. */
	eventPtr->xproperty.window,     /* Window holding the property. */
        eventPtr->xproperty.atom,	/* Name of property. */
        0,			/* Offset of data (for multiple reads). */
	pendingPtr->packetSize,	/* Maximum number of items to read. */
	False,			/* If true, delete the property. */
        XA_STRING,		/* Desired type of property. */
        &typeAtom,		/* (out) Actual type of the property. */
        &format,		/* (out) Actual format of the property. */
        &nItems,		/* (out) # of items in specified format. */
        &bytesAfter,		/* (out) # of bytes remaining to be read. */
	(unsigned char **)&data);
#ifdef notdef
    fprintf(stderr, 
	"TargetPropertyEventProc: result=%d, typeAtom=%d, format=%d, nItems=%d\n",
	    result, typeAtom, format, nItems);
#endif
    pendingPtr->status = DROP_FAIL;
    if ((result == Success) && (typeAtom == XA_STRING) && (format == 8)) {
	pendingPtr->status = DROP_OK;
#ifdef notdef
	fprintf(stderr, "data found is (%s)\n", data);
#endif
	Tcl_DStringAppend(&(pendingPtr->dString), data, -1);
	XFree(data);
	if (nItems == pendingPtr->packetSize) {
	    /* Normally, we'll receive the data in one chunk. But if
	     * more are required, reset the timer and go back into the 
	     * wait loop again. */
	    pendingPtr->timerToken = Tcl_CreateTimerHandler(WAIT_INTERVAL, 
		SendTimerProc, &pendingPtr->status);
	    pendingPtr->status = DROP_CONTINUE;
	}
    } 
    /* Set an empty, zero-length value on the source's property. This
     * acts as a handshake, indicating that the target received the
     * latest chunk. */
#ifdef notdef
    fprintf(stderr, "TargetPropertyEventProc: set response property\n");
#endif
    XChangeProperty(pendingPtr->display, pendingPtr->window, 
	pendingPtr->commAtom, XA_STRING, 8, PropModeReplace, 
	(unsigned char *)"", 0);
}

#ifdef HAVE_XDND

static int 
XDndSelectionProc(clientData, interp, portion)
    ClientData clientData;
    Tcl_Interp *interp;
    char *portion;
{
    DropPending *pendingPtr = clientData;

    Tcl_DStringAppend(&(pendingPtr->dString), portion, -1);
#ifdef notdef
    fprintf(stderr, "-> XDndGetSelectionProc\n");
#endif
    return TCL_OK;
}

#endif /* HAVE_XDND */

static void
CompleteDataTransaction(dndPtr, format, pendingPtr)
    Dnd *dndPtr;
    char *format;
    DropPending *pendingPtr;
{
    DndInterpData *dataPtr = dndPtr->dataPtr;
    Tk_Window tkwin;
    Atom formatAtom;

#ifdef notdef
    fprintf(stderr, "-> CompleteDataTransaction\n");
#endif
    /* Check if the source is local to the application. */
    tkwin = Tk_IdToWindow(dndPtr->display, pendingPtr->window);
    if (tkwin != NULL) {
	Blt_HashEntry *hPtr;

	hPtr = Blt_FindHashEntry(&(dndPtr->dataPtr->dndTable), (char *)tkwin);
	if (hPtr != NULL) {
	    Dnd *srcPtr;

	    srcPtr = (Dnd *)Blt_GetHashValue(hPtr);
	    GetFormattedData(srcPtr, format, pendingPtr->timestamp, 
		&(pendingPtr->dString));
	}
	return;
    }

    formatAtom = XInternAtom(pendingPtr->display, format, False);

    if (pendingPtr->protocol == PROTO_XDND) { 
#ifdef HAVE_XDND
	if (Tk_GetSelection(dndPtr->interp, dndPtr->tkwin, 
		dataPtr->XdndSelectionAtom, formatAtom, XDndSelectionProc, 
		pendingPtr) != TCL_OK) {
	    pendingPtr->status = DROP_FAIL;
	}
#endif
	pendingPtr->status = DROP_OK;
    } else {
	Tk_RestrictProc *proc;
	ClientData arg;

	SendClientMsg(pendingPtr->display, pendingPtr->window, 
		dataPtr->mesgAtom, 
		TS_START_DROP, 
		(int)Tk_WindowId(dndPtr->tkwin),
		pendingPtr->timestamp, 
		(int)formatAtom, 
		(int)pendingPtr->commAtom);

	pendingPtr->commAtom = dndPtr->dataPtr->commAtom;
	pendingPtr->status = DROP_CONTINUE;
	pendingPtr->display = dndPtr->display;
	proc = Tk_RestrictEvents(SendRestrictProc, dndPtr, &arg);
	Tk_CreateEventHandler(dndPtr->tkwin, PropertyChangeMask, 
		TargetPropertyEventProc, pendingPtr);
	pendingPtr->timerToken = Tcl_CreateTimerHandler(WAIT_INTERVAL, 
	       SendTimerProc, &pendingPtr->status);
	/*  
	 * ----------------------------------------------------------
	 *
	 * Enter a loop processing X events until the all the data is
	 * received or the source is declared to be dead (i.e. we
	 * timeout).  While waiting for a result, restrict handling to
	 * just property-related events so that the transfer is
	 * synchronous with respect to other events in the widget.
	 *
	 * ---------------------------------------------------------- 
	 */
	while (pendingPtr->status == DROP_CONTINUE) { 
	    /* Wait for property event. */
	    Tcl_DoOneEvent(TCL_ALL_EVENTS);
	}
	Tk_RestrictEvents(proc, arg, &arg);
	Tcl_DeleteTimerHandler(pendingPtr->timerToken);
	Tk_DeleteEventHandler(dndPtr->tkwin, PropertyChangeMask, 
	      TargetPropertyEventProc, pendingPtr);
    }
#ifdef notdef
    fprintf(stderr, "<- CompleteDataTransaction\n");
#endif
}

/*
 * ------------------------------------------------------------------------
 *
 *  SourcePropertyEventProc --
 *
 *	Invoked by the Tk dispatcher when a PropertyNotify event occurs
 *	on the source window.  The event acts as a handshake between the
 *	target and the source.  The source acknowledges the target has 
 *	received the last packet of data and sends the next packet.  
 *
 *	Note a special case.  If the data is divisible by the packetsize,
 *	then an extra zero-length packet is sent to mark the end of the
 *	data.  A packetsize length packet indicates more is to follow.
 *
 *	Normally the property is empty (zero-length).  But if an
 *	errored occurred on the target, it will contain the error
 *	message.
 *
 * ------------------------------------------------------------------------ 
 */
static void
SourcePropertyEventProc(clientData, eventPtr)
    ClientData clientData;	/* data associated with widget */
    XEvent *eventPtr;		/* information about event */
{
    DropPending *pendingPtr = clientData;
    char *data;
    int result, format;
    Atom typeAtom;
    unsigned long nItems, bytesAfter;
    int size, bytesLeft;
    unsigned char *p;

#ifdef notdef
    fprintf(stderr, "-> SourcePropertyEventProc\n");
#endif
    if ((eventPtr->xproperty.atom != pendingPtr->commAtom)
	|| (eventPtr->xproperty.state != PropertyNewValue)) {
	return;
    }
    Tcl_DeleteTimerHandler(pendingPtr->timerToken);
    data = NULL;
    result = XGetWindowProperty(
        eventPtr->xproperty.display,	/* Display of window. */
	eventPtr->xproperty.window,     /* Window holding the property. */
        eventPtr->xproperty.atom,	/* Name of property. */
        0,			/* Offset of data (for multiple reads). */
	pendingPtr->packetSize,	/* Maximum number of items to read. */
	True,			/* If true, delete the property. */
        XA_STRING,		/* Desired type of property. */
        &typeAtom,		/* (out) Actual type of the property. */
        &format,		/* (out) Actual format of the property. */
        &nItems,		/* (out) # of items in specified format. */
        &bytesAfter,		/* (out) # of bytes remaining to be read. */
	(unsigned char **)&data);

    if ((result != Success) || (typeAtom != XA_STRING) || (format != 8)) {
	pendingPtr->status = DROP_FAIL;
#ifdef notdef
    fprintf(stderr, "<- SourcePropertyEventProc: wrong format\n");
#endif
	return;			/* Wrong data format. */
    }
    if (nItems > 0) {
	pendingPtr->status = DROP_FAIL;
	Tcl_DStringFree(&(pendingPtr->dString));
	Tcl_DStringAppend(&(pendingPtr->dString), data, -1);
	XFree(data);
#ifdef notdef
    fprintf(stderr, "<- SourcePropertyEventProc: error\n");
#endif
	return;			/* Error occurred on target. */
    }    
    bytesLeft = Tcl_DStringLength(&(pendingPtr->dString)) - pendingPtr->offset;
    if (bytesLeft <= 0) {
#ifdef notdef
	fprintf(stderr, "<- SourcePropertyEventProc: done\n");
#endif
	pendingPtr->status = DROP_OK;
	size = 0;
    } else {
	size = MIN(bytesLeft, pendingPtr->packetSize);
	pendingPtr->status = DROP_CONTINUE;
    }
    p = (unsigned char *)Tcl_DStringValue(&(pendingPtr->dString)) + 
	pendingPtr->offset;
    XChangeProperty(pendingPtr->display, pendingPtr->window, 
	pendingPtr->commAtom, XA_STRING, 8, PropModeReplace, p, size);
    pendingPtr->offset += size;
    pendingPtr->timerToken = Tcl_CreateTimerHandler(WAIT_INTERVAL, 
	   SendTimerProc, &pendingPtr->status);
#ifdef notdef
    fprintf(stderr, "<- SourcePropertyEventProc\n");
#endif
}


static void
SendDataToTarget(dndPtr, pendingPtr)
    Dnd *dndPtr;
    DropPending *pendingPtr;
{
    int size;
    Tk_RestrictProc *proc;
    ClientData arg;

#ifdef notdef
    fprintf(stderr, "-> SendDataToTarget\n");
#endif
    Tk_CreateEventHandler(dndPtr->tkwin, PropertyChangeMask, 
	SourcePropertyEventProc, pendingPtr);
    pendingPtr->timerToken = Tcl_CreateTimerHandler(WAIT_INTERVAL, 
	SendTimerProc, &pendingPtr->status);
    size = MIN(Tcl_DStringLength(&pendingPtr->dString), pendingPtr->packetSize);

    proc = Tk_RestrictEvents(SendRestrictProc, dndPtr, &arg);

    /* 
     * Setting the property starts the process.  The target will
     * see the PropertyChange event and respond accordingly.  
     */
    XChangeProperty(dndPtr->display, pendingPtr->window, 
	pendingPtr->commAtom, XA_STRING, 8, PropModeReplace, 
	(unsigned char *)Tcl_DStringValue(&(pendingPtr->dString)), size);
    pendingPtr->offset += size;

    /*
     * Enter a loop processing X events until the result comes
     * in or the target is declared to be dead.  While waiting
     * for a result, look only at property-related events so that
     * the handshake is synchronous with respect to other events in
     * the application.
     */
    pendingPtr->status = DROP_CONTINUE;
    while (pendingPtr->status == DROP_CONTINUE) {
	/* Wait for the property change event. */
	Tcl_DoOneEvent(TCL_ALL_EVENTS);
    }
    Tk_RestrictEvents(proc, arg, &arg);
    Tcl_DeleteTimerHandler(pendingPtr->timerToken);
    Tk_DeleteEventHandler(dndPtr->tkwin, PropertyChangeMask, 
	  SourcePropertyEventProc, pendingPtr);
#ifdef notdef
    fprintf(stderr, "<- SendDataToTarget\n");
#endif
}

static void
DoDrop(dndPtr, eventPtr)
    Dnd *dndPtr;
    XEvent *eventPtr;
{
    DndInterpData *dataPtr = dndPtr->dataPtr;
    Token *tokenPtr = dndPtr->tokenPtr;
    Tcl_Interp *interp = dndPtr->interp;
    struct DropRequest {
	int mesg;		/* TS_DROP_RESULT message. */
	Window window;		/* Target window. */
	int timestamp;		/* Transaction timestamp. */
	Atom formatAtom;	/* Format requested. */
    } *dropPtr;
    char *format;
    DropPending pending;

    if (tokenPtr->timerToken != NULL) {
	Tcl_DeleteTimerHandler(tokenPtr->timerToken);
    }
    dropPtr = (struct DropRequest *)eventPtr->xclient.data.l;
    format = XGetAtomName(dndPtr->display, dropPtr->formatAtom);
#ifdef notdef
    fprintf(stderr, "DoDrop %s 0x%x\n", Tk_PathName(dndPtr->tkwin), 
	    dropPtr->window);
#endif
    if (GetFormattedData(dndPtr, format, dropPtr->timestamp, &(pending.dString))
	!= TCL_OK) {
	Tcl_BackgroundError(interp);
	/* Send empty string to break target's wait loop. */
	XChangeProperty(dndPtr->display, dropPtr->window, dataPtr->commAtom, 
		XA_STRING, 8, PropModeReplace, (unsigned char *)"", 0);
	return;
    } 
    pending.window = dropPtr->window;
    pending.display = dndPtr->display;
    pending.commAtom = dataPtr->commAtom;
    pending.offset = 0;
    pending.packetSize = GetMaxPropertySize(dndPtr->display);
    SendDataToTarget(dndPtr, &pending);
    Tcl_DStringFree(&(pending.dString));
#ifdef notdef
    fprintf(stderr, "<- DoDrop\n");
#endif
}

static void
DropFinished(dndPtr, eventPtr)
    Dnd *dndPtr;
    XEvent *eventPtr;
{
    Tcl_Interp *interp = dndPtr->interp;
    Tcl_DString dString, savedResult;
    int result;
    char **p;
    struct DropResult {
	int mesg;		/* TS_DROP_RESULT message. */
	Window window;		/* Target window. */
	int timestamp;		/* Transaction timestamp. */
	int result;		/* Result of transaction. */
    } *dropPtr;

#ifdef notdef
    fprintf(stderr, "DropFinished\n");
#endif
    dropPtr = (struct DropResult *)eventPtr->xclient.data.l;

    Tcl_DStringInit(&dString);
    for (p = dndPtr->resultCmd; *p != NULL; p++) {
	Tcl_DStringAppendElement(&dString, *p);
    }
    Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
    Tcl_DStringAppendElement(&dString, "action");
    Tcl_DStringAppendElement(&dString, NameOfAction(dropPtr->result));
    Tcl_DStringAppendElement(&dString, "timestamp");
    Tcl_DStringAppendElement(&dString, Blt_Utoa(dropPtr->timestamp));

    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    if (result != TCL_OK) {
	Tcl_BackgroundError(interp);
    }
    Tcl_DStringResult(interp, &savedResult);
}


static void
FreeFormats(dndPtr) 
    Dnd *dndPtr;
{
    if (dndPtr->matchingFormats != NULL) {
	Blt_Free(dndPtr->matchingFormats);
	dndPtr->matchingFormats = NULL;
    }
    dndPtr->lastId = None;
}

static char *
GetSourceFormats(dndPtr, window, timestamp)
    Dnd *dndPtr;
    Window window;
    int timestamp;
{
    if (dndPtr->lastId != timestamp) {
	char *data;
	
	FreeFormats(dndPtr);
	data = GetProperty(dndPtr->display, window, 
		   dndPtr->dataPtr->formatsAtom);
	if (data != NULL) {
	    dndPtr->matchingFormats = Blt_Strdup(data);
	    XFree(data);
	}
	dndPtr->lastId = timestamp;
    }
    if (dndPtr->matchingFormats == NULL) {
	return "";
    }
    return dndPtr->matchingFormats;
}
 

static int
InvokeCallback(dndPtr, cmd, x, y, formats, button, keyState, timestamp)
    Dnd *dndPtr;
    char **cmd;
    int x, y;
    char *formats;
    int button, keyState, timestamp;
{
    Tcl_DString dString, savedResult;
    Tcl_Interp *interp = dndPtr->interp;
    int result;
    char **p;

    Tcl_DStringInit(&dString);
    for (p = cmd; *p != NULL; p++) {
	Tcl_DStringAppendElement(&dString, *p);
    }
    Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
    x -= Blt_RootX(dndPtr->tkwin); /* Send coordinates relative to target. */
    y -= Blt_RootY(dndPtr->tkwin);
    Tcl_DStringAppendElement(&dString, "x");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(x));
    Tcl_DStringAppendElement(&dString, "y");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(y));
    Tcl_DStringAppendElement(&dString, "formats");
    if (formats == NULL) {
	formats = "";
    }
    Tcl_DStringAppendElement(&dString, formats);
    Tcl_DStringAppendElement(&dString, "button");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(button));
    Tcl_DStringAppendElement(&dString, "state");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(keyState));
    Tcl_DStringAppendElement(&dString, "timestamp");
    Tcl_DStringAppendElement(&dString, Blt_Utoa(timestamp));
    Tcl_Preserve(interp);
    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    if (result == TCL_OK) {
	result = GetDragResult(interp, Tcl_GetStringResult(interp));
    } else {
	result = DROP_CANCEL;
	Tcl_BackgroundError(interp);
    }
    Tcl_DStringResult(interp, &savedResult);
    Tcl_Release(interp);
    return result;
}

/* 
 * ------------------------------------------------------------------------
 *
 *  AcceptDrop --
 *	
 *	Invokes a Tcl procedure to handle the target's side of the
 *	drop.  A Tcl procedure is invoked, either one designated for 
 *	this target by the user (-ondrop) or a default Tcl procedure. 
 *	It is passed the following arguments:
 *
 *		widget		The path name of the target. 
 *		x		X-coordinate of the mouse relative to the 
 *				widget.
 *		y		Y-coordinate of the mouse relative to the 
 *				widget.
 *		formats		A list of data formats acceptable to both
 *				the source and target.
 *		button		Button pressed.
 *		state		Key state.
 *		timestamp	Timestamp of transaction.
 *		action		Requested action from source.
 *
 *	If the Tcl procedure returns "cancel", this indicates that the drop was
 *	not accepted by the target and the reject symbol should be displayed.
 *	Otherwise one of the following strings may be recognized:
 *
 *		"cancel"	Drop was canceled.
 *		"copy"		Source data has been successfully copied.
 *		"link"		Target has made a link to the data. It's 
 *				Ok for the source to remove it's association
 *				with the data, but not to delete the data
 *				itself.
 *		"move"		Source data has been successfully copied,
 *				it's Ok for the source to delete its
 *				association with the data and the data itself.
 *
 *	The result is relayed back to the source via another client message.
 *	The source may or may not be waiting for the result.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A Tcl procedure is invoked in the target to handle the drop event.
 *	The result of the drop is sent (via another ClientMessage) to the
 *	source.
 *
 * ------------------------------------------------------------------------
 */
static int
AcceptDrop(dndPtr, x, y, formats, button, keyState, timestamp) 
    Dnd *dndPtr;		/* Target where the drop event occurred. */
    int x, y;
    char *formats;
    int button, keyState, timestamp;
{
    Tcl_Interp *interp = dndPtr->interp;
    char **cmd;
    Tcl_DString dString, savedResult;
    int result;
    
    if (dndPtr->motionCmd != NULL) {
	result = InvokeCallback(dndPtr, dndPtr->motionCmd, x, y, formats, 
		button, keyState, timestamp);
	if (result != DROP_OK) {
	    return result;
	}
    }
    if (dndPtr->leaveCmd != NULL) {
	InvokeCallback(dndPtr, dndPtr->leaveCmd, x, y, formats, button, 
	       keyState, timestamp);
    }
    Tcl_DStringInit(&dString);
    cmd = dndPtr->dropCmd;
    if (cmd != NULL) {
	char **p;

	for (p = cmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&dString, *p);
	}
    } else {
	Tcl_DStringAppendElement(&dString, "blt::DndStdDrop");
    }
    Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
    dndPtr->dropX = x - Blt_RootX(dndPtr->tkwin);
    dndPtr->dropY = y - Blt_RootY(dndPtr->tkwin);
    Tcl_DStringAppendElement(&dString, "x");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->dropX));
    Tcl_DStringAppendElement(&dString, "y");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->dropY));
    Tcl_DStringAppendElement(&dString, "formats");
    Tcl_DStringAppendElement(&dString, formats);
    Tcl_DStringAppendElement(&dString, "button");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(button));
    Tcl_DStringAppendElement(&dString, "state");
    Tcl_DStringAppendElement(&dString, Blt_Itoa(keyState));
    Tcl_DStringAppendElement(&dString, "timestamp");
    Tcl_DStringAppendElement(&dString, Blt_Utoa(timestamp));
    Tcl_Preserve(interp);
    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    if (result == TCL_OK) {
	result = GetAction(Tcl_GetStringResult(interp));
    } else {
	result = DROP_CANCEL;
	Tcl_BackgroundError(interp);
    }
    Tcl_DStringResult(interp, &savedResult);
    Tcl_Release(interp);
    return result;
}

/* 
 * ------------------------------------------------------------------------
 *
 *  HandleDropEvent --
 *	
 *	Invokes a Tcl procedure to handle the target's side of the
 *	drop.  This routine is triggered via a client message from the
 *	drag source indicating that the token was dropped over this
 *	target. The fields of the incoming message are:
 *
 *		data.l[0]	Message type.
 *		data.l[1]	Window Id of the source.
 *		data.l[2]	Screen X-coordinate of the pointer.
 *		data.l[3]	Screen Y-coordinate of the pointer.
 *		data.l[4]	Id of the drag&drop transaction.
 *
 *	A Tcl procedure is invoked, either one designated for this 
 *	target by the user (-ondrop) or a default Tcl procedure. It
 *	is passed the following arguments:
 *
 *		widget		The path name of the target. 
 *		x		X-coordinate of the mouse relative to the 
 *				widget.
 *		y		Y-coordinate of the mouse relative to the 
 *				widget.
 *		formats		A list of data formats acceptable to both
 *				the source and target.
 *
 *	If the Tcl procedure returns "cancel", this indicates that the drop was
 *	not accepted by the target and the reject symbol should be displayed.
 *	Otherwise one of the following strings may be recognized:
 *
 *		"cancel"	Drop was canceled.
 *		"copy"		Source data has been successfully copied.
 *		"link"		Target has made a link to the data. It's 
 *				Ok for the source to remove it's association
 *				with the data, but not to delete the data
 *				itself.
 *		"move"		Source data has been successfully copied,
 *				it's Ok for the source to delete its
 *				association with the data and the data itself.
 *
 *	The result is relayed back to the source via another client message.
 *	The source may or may not be waiting for the result.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A Tcl procedure is invoked in the target to handle the drop event.
 *	The result of the drop is sent (via another ClientMessage) to the
 *	source.
 *
 * ------------------------------------------------------------------------
 */
static void
HandleDropEvent(dndPtr, eventPtr) 
    Dnd *dndPtr;		/* Target where the drop event occurred. */
    XEvent *eventPtr;		/* Message sent from the drag source. */
{
    int button, keyState;
    int x, y;
    char *formats;
    int result;
    struct DropInfo {
	int mesg;		/* TS_DROP message. */
	Window window;		/* Source window. */
	int timestamp;		/* Transaction timestamp. */
	int point;		/* Root X-Y coordinate of pointer. */
	int flags;		/* Button/keystate information. */
    } *dropPtr;
    DropPending pending;

    dropPtr = (struct DropInfo *)eventPtr->xclient.data.l;
    UNPACK(dropPtr->point, x, y);
    UNPACK(dropPtr->flags, button, keyState);

    /* Set up temporary bookkeeping for the drop transaction */
    memset (&pending, 0, sizeof(pending));
    pending.window = dropPtr->window;
    pending.display = eventPtr->xclient.display;
    pending.timestamp = dropPtr->timestamp;
    pending.protocol = PROTO_BLT;
    pending.packetSize = GetMaxPropertySize(pending.display);
    Tcl_DStringInit(&(pending.dString));

    formats = GetSourceFormats(dndPtr, dropPtr->window, dropPtr->timestamp);

    dndPtr->pendingPtr = &pending;
    result = AcceptDrop(dndPtr, x, y, formats, button, keyState, 
	dropPtr->timestamp);
    dndPtr->pendingPtr = NULL;

    /* Target-to-Source: Drop result message. */
    SendClientMsg(dndPtr->display, dropPtr->window, dndPtr->dataPtr->mesgAtom,
	TS_DROP_RESULT, (int)Tk_WindowId(dndPtr->tkwin), dropPtr->timestamp, 
	result, 0);
    FreeFormats(dndPtr);
}

/* 
 * ------------------------------------------------------------------------
 *
 *  HandleDragEvent --
 *	
 *	Invokes one of 3 Tcl procedures to handle the target's side of 
 *	the drag operation.  This routine is triggered via a ClientMessage 
 *	from the drag source indicating that the token as either entered,
 *	moved, or left this target.  The source sends messages only if
 *	Tcl procedures on the target have been defined to watch the 
 *	events. The message_type field can be either 
 *
 *	  ST_DRAG_ENTER		The mouse has entered the target.
 *	  ST_DRAG_MOTION	The mouse has moved within the target.
 *	  ST_DRAG_LEAVE		The mouse has left the target.
 *
 *	The data fields are as follows:
 *	  data.l[0]		Message type.
 *	  data.l[1]		Window Id of the source.
 *	  data.l[2]		Timestamp of the drag&drop transaction.
 *	  data.l[3]		Root X-Y coordinate of the pointer.
 *	  data.l[4]		Button and key state information.
 *
 *	For any of the 3 Tcl procedures, the following arguments
 *	are passed:
 *
 *	  widget		The path name of the target. 
 *	  x			X-coordinate of the mouse in the widget.
 *	  y			Y-coordinate of the mouse in the widget.
 *	  formats		A list of data formats acceptable to both
 *				the source and target.
 *
 *	If the Tcl procedure returns "cancel", this indicates that the drag 
 *	operation has been canceled and the reject symbol should be displayed.
 *	Otherwise it should return a boolean:
 *
 *	  true			Target will accept drop.
 *	  false			Target will not accept the drop.
 *
 *	The purpose of the Enter and Leave procedure is to allow the
 *	target to provide visual feedback that the drop can occur or not.
 *	The Motion procedure is for cases where the drop area is a smaller
 *	area within the target, such as a canvas item on a canvas. The 
 *	procedure can determine (based upon the X-Y coordinates) whether 
 *	the pointer is over the canvas item and return a value accordingly.
 *
 *	The result of the Tcl procedure is then relayed back to the 
 *	source by a ClientMessage.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A Tcl procedure is invoked in the target to handle the drag event.
 *	The result of the drag is sent (via another ClientMessage) to the
 *	source.
 *
 * ------------------------------------------------------------------------
 */
static void
HandleDragEvent(dndPtr, eventPtr) 
    Dnd *dndPtr;		/* Target where the drag event occurred. */
    XEvent *eventPtr;		/* Message sent from the drag source. */
{
    char **cmd;
    int resp;
    int x, y;
    int button, keyState;
    char *formats;
    struct DragInfo {
	int mesg;		/* Drag-and-drop message type. */
	Window window;		/* Source window. */
	int timestamp;		/* Transaction timestamp. */
	int point;		/* Root X-Y coordinate of pointer. */
	int flags;		/* Button/keystate information. */
    } *dragPtr;

    dragPtr = (struct DragInfo *)eventPtr->xclient.data.l;

    cmd = NULL;
    switch (dragPtr->mesg) {
    case ST_DRAG_ENTER:
	cmd = dndPtr->enterCmd;
	break;
    case ST_DRAG_MOTION:
	cmd = dndPtr->motionCmd;
	break;
    case ST_DRAG_LEAVE:
	cmd = dndPtr->leaveCmd;
	break;
    } 
    if (cmd == NULL) {
	return;			/* Nothing to do. */
    }
    UNPACK(dragPtr->point, x, y);
    UNPACK(dragPtr->flags, button, keyState);
    formats = GetSourceFormats(dndPtr, dragPtr->window, dragPtr->timestamp);
    resp = InvokeCallback(dndPtr, cmd, x, y, formats, button, keyState,
		  dragPtr->timestamp);

    /* Target-to-Source: Drag result message. */
    SendClientMsg(dndPtr->display, dragPtr->window, dndPtr->dataPtr->mesgAtom,
	TS_DRAG_STATUS, (int)Tk_WindowId(dndPtr->tkwin), dragPtr->timestamp, 
	resp, 0);
}

/*
 * ------------------------------------------------------------------------
 *
 *  DndEventProc --
 *
 *	Invoked by Tk_HandleEvent whenever a DestroyNotify event is received
 *	on a registered drag&drop source widget.
 *
 * ------------------------------------------------------------------------
 */
static int
DndEventProc(clientData, eventPtr)
    ClientData clientData;	/* Drag&drop record. */
    XEvent *eventPtr;		/* Event description. */
{
    Dnd *dndPtr = clientData;

    if (eventPtr->xany.window != Tk_WindowId(dndPtr->tkwin)) {
	return 0;
    }
    if (eventPtr->type == DestroyNotify) {
	dndPtr->tkwin = NULL;
	dndPtr->flags |= DND_DELETED;
	Tcl_EventuallyFree(dndPtr, DestroyDnd);
	return 0;		/* Other handlers have to see this event too.*/
    } else if (eventPtr->type == ButtonPress) {
	dndPtr->keyState = eventPtr->xbutton.state;
	dndPtr->button =  eventPtr->xbutton.button;
	return 0;
    } else if (eventPtr->type == ButtonRelease) {
	dndPtr->keyState = eventPtr->xbutton.state;
	dndPtr->button =  eventPtr->xbutton.button;
	return 0;
    } else if (eventPtr->type == MotionNotify) {
	dndPtr->keyState = eventPtr->xmotion.state;
	return 0;
    } else if ((eventPtr->type == ClientMessage) &&
	(eventPtr->xclient.message_type == dndPtr->dataPtr->mesgAtom)) {
	int result;

	switch((unsigned int)eventPtr->xclient.data.l[0]) {
	case TS_START_DROP:
	    DoDrop(dndPtr, eventPtr);
	    return 1;
	    
	case TS_DROP_RESULT:
	    result = eventPtr->xclient.data.l[MESG_RESPONSE];
	    dndPtr->tokenPtr->status = result;
	    if (result == DROP_CANCEL) {
		CancelDrag(dndPtr);
	    } else if (result == DROP_FAIL) {
		EventuallyRedrawToken(dndPtr);
	    } else {
		dndPtr->tokenPtr->nSteps = 10;
		FadeToken(dndPtr);
	    }
	    if (dndPtr->resultCmd != NULL) {
		DropFinished(dndPtr, eventPtr);
	    }
	    return 1;

	case TS_DRAG_STATUS:
	    result = eventPtr->xclient.data.l[MESG_RESPONSE];
	    ChangeToken(dndPtr, result);
	    return 1;

	case ST_DROP:
	    HandleDropEvent(dndPtr, eventPtr);
	    return 1;

	case ST_DRAG_ENTER:
	case ST_DRAG_MOTION:
	case ST_DRAG_LEAVE:
	    HandleDragEvent(dndPtr, eventPtr);
	    return 1;
	}
    }
    return 0;
}

static void
SendPointerMessage(dndPtr, eventType, windowPtr, x, y)
    Dnd *dndPtr;		/* Source drag&drop manager. */
    int eventType;		/* Type of event to relay. */
    Winfo *windowPtr;		/* Generic window information. */
    int x, y;			/* Root coordinates of mouse. */
{
    /* Source-to-Target: Pointer event messages. */
    SendClientMsg(
	dndPtr->display,	/* Display of recipient window. */
	windowPtr->window,	/* Recipient window. */
	dndPtr->dataPtr->mesgAtom, /* Message type. */
	eventType,		/* Data 1 */
	(int)Tk_WindowId(dndPtr->tkwin), /* Data 2 */
	dndPtr->timestamp,	/* Data 3  */
	PACK(x, y),		/* Data 4 */
	PACK(dndPtr->button, dndPtr->keyState)); /* Data 5 */
    /* Don't wait the response. */
}

static void
RelayEnterEvent(dndPtr, windowPtr, x, y)
    Dnd *dndPtr;
    Winfo *windowPtr;
    int x, y;
{
    if ((windowPtr != NULL) && (windowPtr->eventFlags & WATCH_ENTER)) {
	SendPointerMessage(dndPtr, ST_DRAG_ENTER, windowPtr, x, y);
    }
}

static void
RelayLeaveEvent(dndPtr, windowPtr, x, y)
    Dnd *dndPtr;
    Winfo *windowPtr;
    int x, y;
{ 
    if ((windowPtr != NULL) && (windowPtr->eventFlags & WATCH_LEAVE)) {
	SendPointerMessage(dndPtr, ST_DRAG_LEAVE, windowPtr, x, y);
    }
}

static void
RelayMotionEvent(dndPtr, windowPtr, x, y)
    Dnd *dndPtr;
    Winfo *windowPtr;
    int x, y;
{
    if ((windowPtr != NULL) && (windowPtr->eventFlags & WATCH_MOTION)) {
	SendPointerMessage(dndPtr, ST_DRAG_MOTION, windowPtr, x, y);
    }
}

static void
RelayDropEvent(dndPtr, windowPtr, x, y)
    Dnd *dndPtr;
    Winfo *windowPtr;
    int x, y;
{
    SendPointerMessage(dndPtr, ST_DROP, windowPtr, x, y);
}

/*
 * ------------------------------------------------------------------------
 *
 *  FreeWinfo --
 *
 * ------------------------------------------------------------------------
 */
static void
FreeWinfo(windowPtr)
    Winfo *windowPtr;		/* window rep to be freed */
{
    Winfo *childPtr;
    Blt_ChainLink *linkPtr;

    for (linkPtr = Blt_ChainFirstLink(windowPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	childPtr = Blt_ChainGetValue(linkPtr);
	FreeWinfo(childPtr);	/* Recursively free children. */
    }
    if (windowPtr->matches != NULL) {
	Blt_Free(windowPtr->matches);
    }
    Blt_ChainDestroy(windowPtr->chainPtr);
    Blt_Free(windowPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWinfo --
 *
 *	Invoked during "drag" operations. Digs into the root window
 *	hierarchy and caches the window-related information.
 *	If the current point lies over an uninitialized window (i.e.
 *	one that already has an allocated Winfo structure, but has
 *	not been filled in yet), this routine is called to query 
 *	window coordinates.  If the window has any children, more 
 *	uninitialized Winfo structures are allocated.  Further queries 
 *	will cause these structures to be initialized in turn.
 *
 * ------------------------------------------------------------------------
 */
static void
GetWinfo(display, windowPtr)
    Display *display;
    Winfo *windowPtr;		/* window rep to be initialized */
{
    int visible;

    if (windowPtr->initialized) {
	return;
    }
    /* Query for the window coordinates.  */
    visible = GetWindowRegion(display, windowPtr);
    if (visible) {
	Blt_ChainLink *linkPtr;
	Blt_Chain *chainPtr;
	Winfo *childPtr;

	/* Add offset from parent's origin to coordinates */
	if (windowPtr->parentPtr != NULL) {
	    windowPtr->x1 += windowPtr->parentPtr->x1;
	    windowPtr->y1 += windowPtr->parentPtr->y1;
	    windowPtr->x2 += windowPtr->parentPtr->x1;
	    windowPtr->y2 += windowPtr->parentPtr->y1;
	}
	/*
	 * Collect a list of child windows, sorted in z-order.  The
	 * topmost window will be first in the list.
	 */
	chainPtr = GetWindowZOrder(display, windowPtr->window);

	/* Add and initialize extra slots if needed. */
	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    childPtr = Blt_Calloc(1, sizeof(Winfo));
	    assert(childPtr);
	    childPtr->initialized = FALSE;
	    childPtr->window = (Window)Blt_ChainGetValue(linkPtr);
	    childPtr->parentPtr = windowPtr;
	    Blt_ChainSetValue(linkPtr, childPtr);
	}
	windowPtr->chainPtr = chainPtr;
    } else {
	/* If it's not viewable don't bother doing anything else. */
	windowPtr->x1 = windowPtr->y1 = windowPtr->x2 = windowPtr->y2 = -1;
	windowPtr->chainPtr = NULL;
    }
    windowPtr->initialized = TRUE;
}

/*
 * ------------------------------------------------------------------------
 *
 *  InitRoot --
 *
 *	Invoked at the start of a "drag" operation to capture the
 *	positions of all windows on the current root.  Queries the
 *	entire window hierarchy and determines the placement of each
 *	window.  Queries the "BltDndTarget" property info where
 *	appropriate.  This information is used during the drag
 *	operation to determine when the drag&drop token is over a
 *	valid drag&drop target.
 *
 *  Results:
 *	Returns the record for the root window, which contains records
 *	for all other windows as children.
 *
 * ------------------------------------------------------------------------
 */
static Winfo *
InitRoot(dndPtr)
    Dnd *dndPtr;
{
    Winfo *rootPtr;

    rootPtr = Blt_Calloc(1, sizeof(Winfo));
    assert(rootPtr);
    rootPtr->window = DefaultRootWindow(dndPtr->display);
    dndPtr->windowPtr = NULL;
    GetWinfo(dndPtr->display, rootPtr);
    return rootPtr;
}


static int
ParseProperty(interp, dndPtr, windowPtr, data)
    Tcl_Interp *interp;
    Dnd *dndPtr;
    Winfo *windowPtr;
    char *data;
{
    int nElems;
    char **elemArr;
    int eventFlags;
    Tcl_DString dString;
    int count;
    register int i;
    
    if (Tcl_SplitList(interp, data, &nElems, &elemArr) != TCL_OK) {
	return TCL_ERROR;	/* Malformed property list. */
    }
    if (nElems < 1) {
	Tcl_AppendResult(interp, "Malformed property \"", data, "\"", 
			 (char *)NULL);
	goto error;
    }
    if (Tcl_GetInt(interp, elemArr[PROP_WATCH_FLAGS], &eventFlags) != TCL_OK) {
	goto error;
    }

    /* target flags, type1, type2, ... */
    /*
     * The target property contains a list of possible formats.
     * Compare this with what formats the source is willing to
     * convert and compress the list down to just the matching
     * formats.  It's up to the target to request the specific 
     * type (or types) that it wants.
     */
    count = 0;
    Tcl_DStringInit(&dString);
    if (dndPtr->reqFormats == NULL) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	char *fmt;

	for (i = 1; i < nElems; i++) {
	    for(hPtr = Blt_FirstHashEntry(&(dndPtr->getDataTable), &cursor);
		hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		fmt = Blt_GetHashKey(&(dndPtr->getDataTable), hPtr);
		if ((*fmt == elemArr[i][0]) && (strcmp(fmt, elemArr[i]) == 0)) {
		    Tcl_DStringAppendElement(&dString, elemArr[i]);
		    count++;
		    break;
		}
	    }
	}
    } else {
	register char **s;

	for (i = 1; i < nElems; i++) {
	    for (s = dndPtr->reqFormats; *s != NULL; s++) {
		if ((**s == elemArr[i][0]) && (strcmp(*s, elemArr[i]) == 0)) {
		    Tcl_DStringAppendElement(&dString, elemArr[i]);
		    count++;
		}
	    }
	}
    }
    if (count == 0) {
#ifdef notdef
	fprintf(stderr, "source/target mismatch: No matching types\n");
#endif
	return TCL_BREAK;
    } 
    if (eventFlags != 0) {
	SetProperty(dndPtr->tkwin, dndPtr->dataPtr->formatsAtom, 
		Tcl_DStringValue(&dString));
	windowPtr->matches = NULL;
    } else {
	windowPtr->matches = Blt_Strdup(Tcl_DStringValue(&dString));
    }
    Tcl_DStringFree(&dString);	
    windowPtr->eventFlags = eventFlags;
    return TCL_OK;
 error:
    Blt_Free(elemArr);
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *
 *  OverTarget --
 *
 *      Checks to see if a compatible drag&drop target exists at the
 *      given position.  A target is "compatible" if it is a drag&drop
 *      window, and if it has a handler that is compatible with the
 *      current source window.
 *
 *  Results:
 *	Returns a pointer to a structure describing the target, or NULL
 *	if no compatible target is found.
 *
 * ------------------------------------------------------------------------
 */
static Winfo *
OverTarget(dndPtr)
    Dnd *dndPtr;		/* drag&drop source window */
{
    Tcl_Interp *interp = dndPtr->interp;
    int x, y;
    int vx, vy;
    int dummy;
    Winfo *windowPtr;

    /* 
     * If no window info has been been gathered yet for this target,
     * then abort this call.  This probably means that the token is
     * moved before it has been properly built.  
     */
    if (dndPtr->rootPtr == NULL) {
	fprintf(stderr, "rootPtr not initialized\n");
	return NULL;
    }
    /* Adjust current location for virtual root windows.  */
    Tk_GetVRootGeometry(dndPtr->tkwin, &vx, &vy, &dummy, &dummy);
    x = dndPtr->x + vx;
    y = dndPtr->y + vy;

    windowPtr = FindTopWindow(dndPtr, x, y);
    if (windowPtr == NULL) {
	return NULL;		/* Not over a window. */
    }
    if ((!dndPtr->selfTarget) && 
	(Tk_WindowId(dndPtr->tkwin) == windowPtr->window)) {
	return NULL;		/* If the self-target flag isn't set,
				 *  don't allow the source window to
				 *  drop onto itself.  */
    }
    if (!windowPtr->lookedForProperty) {
	char *data;
	int result;

	windowPtr->lookedForProperty = TRUE;
	/* See if this window has a "BltDndTarget" property. */
	data = GetProperty(dndPtr->display, windowPtr->window, 
		dndPtr->dataPtr->targetAtom);
	if (data == NULL) {
#ifdef notdef
	    fprintf(stderr, "No property on 0x%x\n", windowPtr->window);
#endif
	    return NULL;		/* No such property on window. */
	}
	result = ParseProperty(interp, dndPtr, windowPtr, data);
	XFree(data);
	if (result == TCL_BREAK) {
#ifdef notdef
	    fprintf(stderr, "No matching formats\n");
#endif
	    return NULL;
	}
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	    return NULL;		/* Malformed property list. */
	} 
	windowPtr->isTarget = TRUE;
    }
    if (!windowPtr->isTarget) {
	return NULL;
    }
    return windowPtr;

}

/*
 * ------------------------------------------------------------------------
 *
 *  AddTargetProperty --
 *
 *	Attaches a drag&drop property to the given target window.
 *	This property allows us to recognize the window later as a
 *	valid target. It also stores important information including
 *	the interpreter managing the target and the pathname of the
 *	target window.  Usually this routine is called when the target
 *	is first registered or first exposed (so that the X-window
 *	really exists).
 *
 * ------------------------------------------------------------------------
 */
static void
AddTargetProperty(dndPtr)
    Dnd *dndPtr;		/* drag&drop target window data */
{
    Tcl_DString dString;
    Blt_HashEntry *hPtr;
    unsigned int eventFlags;
    Blt_HashSearch cursor;
    char *fmt;
    char string[200];

    Tcl_DStringInit(&dString);
    /*
     * Each target window's dnd property contains
     *
     *	1. Mouse event flags.
     *  2. List of all the data types that can be handled. If none
     *     are listed, then all can be handled.
     */
    eventFlags = 0;
    if (dndPtr->enterCmd != NULL) {
	eventFlags |= WATCH_ENTER;
    }
    if (dndPtr->leaveCmd != NULL) {
	eventFlags |= WATCH_LEAVE;
    }
    if (dndPtr->motionCmd != NULL) {
	eventFlags |= WATCH_MOTION;
    }
    sprintf(string, "0x%x", eventFlags);
    Tcl_DStringAppendElement(&dString, string);
    for (hPtr = Blt_FirstHashEntry(&(dndPtr->setDataTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	fmt = Blt_GetHashKey(&(dndPtr->setDataTable), hPtr);
	Tcl_DStringAppendElement(&dString, fmt);
    }
    SetProperty(dndPtr->tkwin, dndPtr->dataPtr->targetAtom, 
	Tcl_DStringValue(&dString));
    dndPtr->targetPropertyExists = TRUE;
    Tcl_DStringFree(&dString);
}

static void
CancelDrag(dndPtr)
    Dnd *dndPtr;
{
    if (dndPtr->flags & DND_INITIATED) {
	dndPtr->tokenPtr->nSteps = 10;
	SnapToken(dndPtr);
	StopActiveCursor(dndPtr);
	if (dndPtr->cursor == None) {
	    Tk_UndefineCursor(dndPtr->tkwin);
	} else {
	    Tk_DefineCursor(dndPtr->tkwin, dndPtr->cursor);
	}
    }
    if (dndPtr->rootPtr != NULL) {
	FreeWinfo(dndPtr->rootPtr);
	dndPtr->rootPtr = NULL;
    }
}

static int
DragInit(dndPtr, x, y)
    Dnd *dndPtr;
    int x, y;
{
    Token *tokenPtr = dndPtr->tokenPtr;
    int result;
    Winfo *newPtr;

    assert((dndPtr->flags & DND_ACTIVE) == DND_SELECTED);
    
    if (dndPtr->rootPtr != NULL) {
	FreeWinfo(dndPtr->rootPtr);
    }
    dndPtr->rootPtr = InitRoot(dndPtr); /* Reset information cache. */ 
    dndPtr->flags &= ~DND_VOIDED;

    dndPtr->x = x;	/* Save current location. */
    dndPtr->y = y;
    result = TRUE;
    Tcl_Preserve(dndPtr);
    if (dndPtr->packageCmd != NULL) {
	Tcl_DString dString, savedResult;
	Tcl_Interp *interp = dndPtr->interp;
	int status;
	char **p;
	int rx, ry;

	Tcl_DStringInit(&dString);
	for (p = dndPtr->packageCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&dString, *p);
	}
	Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
	rx = dndPtr->dragX - Blt_RootX(dndPtr->tkwin);
	ry = dndPtr->dragY - Blt_RootY(dndPtr->tkwin);
	Tcl_DStringAppendElement(&dString, "x");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(rx));
	Tcl_DStringAppendElement(&dString, "y");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(ry));
	Tcl_DStringAppendElement(&dString, "button");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->button));
	Tcl_DStringAppendElement(&dString, "state");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->keyState));
	Tcl_DStringAppendElement(&dString, "timestamp");
	Tcl_DStringAppendElement(&dString, Blt_Utoa(dndPtr->timestamp));
	Tcl_DStringAppendElement(&dString, "token");
	Tcl_DStringAppendElement(&dString, Tk_PathName(tokenPtr->tkwin));

	Tcl_DStringInit(&savedResult);
	Tcl_DStringGetResult(interp, &savedResult);
	dndPtr->flags |= DND_IN_PACKAGE;
	status = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
	dndPtr->flags &= ~DND_IN_PACKAGE;
	if (status == TCL_OK) {
	    result = GetDragResult(interp, Tcl_GetStringResult(interp));
	} else {
	    Tcl_BackgroundError(interp);
	}
	Tcl_DStringFree(&dString);
	Tcl_DStringResult(interp, &savedResult);
	Tcl_DStringFree(&dString);
	if (status != TCL_OK) {
	    HideToken(dndPtr);
	    Tcl_Release(dndPtr);
	    return TCL_ERROR;
	}
    }
    if (dndPtr->flags & DND_VOIDED) {
	HideToken(dndPtr);
	Tcl_Release(dndPtr);
	return TCL_RETURN;
    }
    if ((!result) || (dndPtr->flags & DND_DELETED)) {
	HideToken(dndPtr);
	Tcl_Release(dndPtr);
	return TCL_RETURN;
    }
    Tcl_Release(dndPtr);

    if (dndPtr->cursor != None) {
	Tk_Cursor cursor;
	
	/* Save the old cursor */
	cursor = GetWidgetCursor(dndPtr->interp, dndPtr->tkwin);
	if (dndPtr->cursor != None) {
	    Tk_FreeCursor(dndPtr->display, dndPtr->cursor);
	}
	dndPtr->cursor = cursor;
	if (dndPtr->cursors != NULL) {
	    /* Temporarily install the drag-and-drop cursor. */
	    Tk_DefineCursor(dndPtr->tkwin, dndPtr->cursors[0]);
	}
    }
    if (Tk_WindowId(tokenPtr->tkwin) == None) {
	Tk_MakeWindowExist(tokenPtr->tkwin);
    }
    if (!Tk_IsMapped(tokenPtr->tkwin)) {
	Tk_MapWindow(tokenPtr->tkwin);
    }
    dndPtr->flags |= DND_INITIATED;
    newPtr = OverTarget(dndPtr);
    RelayEnterEvent(dndPtr, newPtr, x, y);
    dndPtr->windowPtr = newPtr;
    tokenPtr->status = (newPtr != NULL) ? DROP_OK : DROP_CONTINUE;
    if (tokenPtr->lastStatus != tokenPtr->status) {
	EventuallyRedrawToken(dndPtr);
    }
    MoveToken(dndPtr);		/* Move token to current drag point. */ 
    RaiseToken(dndPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  CancelOp --
 *
 *	Cancels the current drag&drop operation for the source.  Calling
 *	this operation does not affect the transfer of data from the 
 *	source to the target, once the drop has been made.  From the 
 *	source's point of view, the drag&drop operation is already over. 
 *
 *	Example: dnd cancel .widget
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Hides the token and sets a flag indicating that further "drag"
 *	and "drop" operations should be ignored.
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CancelOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Dnd *dndPtr;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!dndPtr->isSource) {
	Tcl_AppendResult(interp, "widget \"", Tk_PathName(dndPtr->tkwin), 
	 "\" is not a registered drag&drop source.", (char *)NULL);
	return TCL_ERROR;
    }
    /* Send the target a Leave message so it can change back. */
    RelayLeaveEvent(dndPtr, dndPtr->windowPtr, 0, 0);
    CancelDrag(dndPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  CgetOp --
 *
 *	Called to process an (argc,argv) list to configure (or
 *	reconfigure) a drag&drop widget.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED*/
static int
CgetOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Dnd *dndPtr;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tk_ConfigureValue(interp, dndPtr->tkwin, configSpecs, (char *)dndPtr,
	argv[3], 0);
}

/*
 * ------------------------------------------------------------------------
 *
 *  ConfigureOp --
 *
 *	Called to process an (argc,argv) list to configure (or
 *	reconfigure) a drag&drop widget.
 *
 * ------------------------------------------------------------------------
 */
static int
ConfigureOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;		/* current interpreter */
    int argc;			/* number of arguments */
    char **argv;		/* argument strings */
{
    Dnd *dndPtr;
    int flags;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = TK_CONFIG_ARGV_ONLY;
    if (argc == 3) {
	return Tk_ConfigureInfo(interp, dndPtr->tkwin, configSpecs, 
		(char *)dndPtr, (char *)NULL, flags);
    } else if (argc == 4) {
	return Tk_ConfigureInfo(interp, dndPtr->tkwin, configSpecs,
	    (char *)dndPtr, argv[3], flags);
    } 
    if (Tk_ConfigureWidget(interp, dndPtr->tkwin, configSpecs, argc - 3,
		   argv + 3, (char *)dndPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (ConfigureDnd(interp, dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  DeleteOp --
 *
 *	Deletes the drag&drop manager from the widget.  If a "-source"
 *	or "-target" switch is present, only that component of the
 *	drag&drop manager is shutdown.  The manager is not deleted
 *	unless both the target and source components are shutdown.
 *
 *	Example: dnd delete .widget
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Deletes the drag&drop manager.  Also the source and target window 
 *	properties are removed from the widget.
 *
 * ------------------------------------------------------------------------ 
 */
static int
DeleteOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Dnd *dndPtr;
    register int i;

    for(i = 3; i < argc; i++) {
	if (GetDnd(clientData, interp, argv[i], &dndPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	dndPtr->flags |= DND_DELETED;
	Tcl_EventuallyFree(dndPtr, DestroyDnd);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  SelectOp --
 *
 *	Initializes a drag&drop transaction.  Typically this operation
 *	is called from a ButtonPress event on a source widget.  The
 *	window information cache is initialized, and the token is 
 *	initialized and displayed.  
 *
 *	Example: dnd pickup .widget x y 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	The token is initialized and displayed.  This may require invoking
 *	a user-defined package command.  The window information cache is
 *	also initialized.
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Dnd *dndPtr;
    int x, y, timestamp;
    Token *tokenPtr;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!dndPtr->isSource) {
	Tcl_AppendResult(interp, "widget \"", Tk_PathName(dndPtr->tkwin), 
	 "\" is not a registered drag&drop source.", (char *)NULL);
	return TCL_ERROR;
    }
    tokenPtr = dndPtr->tokenPtr;
    if (tokenPtr == NULL) {
	Tcl_AppendResult(interp, "no drag&drop token created for \"", 
		 argv[2], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[5], &timestamp) != TCL_OK) {
	return TCL_ERROR;
    }
    if (dndPtr->flags & (DND_IN_PACKAGE | DND_ACTIVE | DND_VOIDED)) {
	return TCL_OK;
    }

    if (tokenPtr->timerToken != NULL) {
	HideToken(dndPtr);	/* If the user selected again before the
				 * token snap/melt has completed, first 
				 * disable the token timer callback. */
    }
    /* At this point, simply save the starting pointer location. */
    dndPtr->dragX = x, dndPtr->dragY = y;
    GetTokenPosition(dndPtr, x, y);
    tokenPtr->startX = tokenPtr->x;
    tokenPtr->startY = tokenPtr->y;
    dndPtr->timestamp = timestamp;
    dndPtr->flags |= DND_SELECTED;
    
    if (dndPtr->dragStart == 0) {
	if (DragInit(dndPtr, x, y) == TCL_ERROR) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  DragOp --
 *
 *	Continues the drag&drop transaction.  Typically this operation
 *	is called from a button Motion event on a source widget.  Pointer
 *	event messages are possibly sent to the target, indicating Enter,
 *	Leave, and Motion events.
 *
 *	Example: dnd drag .widget x y 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Pointer events are relayed to the target (if the mouse is over
 *	one).
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DragOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Winfo *newPtr, *oldPtr;
    Dnd *dndPtr;
    int x, y;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!dndPtr->isSource) {
	Tcl_AppendResult(interp, "widget \"", Tk_PathName(dndPtr->tkwin), 
	 "\" is not a registered drag&drop source.", (char *)NULL);
	return TCL_ERROR;
    }
    if (dndPtr->tokenPtr == NULL) {
	Tcl_AppendResult(interp, "no drag&drop token created for \"", 
		 argv[2], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }

    if ((dndPtr->flags & DND_SELECTED) == 0) {
	return TCL_OK;		/* Re-entered this routine. */
    }	
    /* 
     * The following code gets tricky because the package command may 
     * call "update" or "tkwait".  A motion event may then trigger 
     * this routine, before the token has been initialized. Until the 
     * package command finishes, no target messages are sent and drops 
     * are silently ignored.  Note that we do still track mouse 
     * movements, so that when the package command completes, it will 
     * have the latest pointer position.  
     */
    dndPtr->x = x;	/* Save current location. */
    dndPtr->y = y;

    if (dndPtr->flags & DND_IN_PACKAGE) {
	return TCL_OK;		/* Re-entered this routine. */
    }
    if ((dndPtr->flags & DND_INITIATED) == 0) {
	int dx, dy;
	int result;

	dx = dndPtr->dragX - x;
	dy = dndPtr->dragY - y;
	if ((ABS(dx) < dndPtr->dragStart) && (ABS(dy) < dndPtr->dragStart)) {
	    return TCL_OK;
	}
	result = DragInit(dndPtr, x, y);
	if (result == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (result == TCL_RETURN) {
	    return TCL_OK;
	}
    }
    if (dndPtr->flags & DND_VOIDED) {
	return TCL_OK;
    }
    oldPtr = dndPtr->windowPtr;
    newPtr = OverTarget(dndPtr);
    if (newPtr == oldPtr) {
	RelayMotionEvent(dndPtr, oldPtr, x, y); 
	dndPtr->windowPtr = oldPtr;
    } else {
	RelayLeaveEvent(dndPtr, oldPtr, x, y);
	RelayEnterEvent(dndPtr, newPtr, x, y);
	dndPtr->windowPtr = newPtr;
    } 
    dndPtr->tokenPtr->status = (newPtr != NULL) ? DROP_OK : DROP_CONTINUE;
    if (dndPtr->tokenPtr->lastStatus != dndPtr->tokenPtr->status) {
	EventuallyRedrawToken(dndPtr);
    }
    MoveToken(dndPtr);		/* Move token to current drag point. */ 
    RaiseToken(dndPtr);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *
 *  DropOp --
 *
 *	Finishes the drag&drop transaction by dropping the data on the
 *	selected target.  Typically this operation is called from a 
 *	ButtonRelease event on a source widget.  Note that a Leave message
 *	is always sent to the target so that is can un-highlight itself.
 *	The token is hidden and a drop message is sent to the target.
 *
 *	Example: dnd drop .widget x y 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	The token is hidden and a drop message is sent to the target.
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DropOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Winfo *windowPtr;
    Dnd *dndPtr;
    int x, y;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!dndPtr->isSource) {
	Tcl_AppendResult(interp, "widget \"", Tk_PathName(dndPtr->tkwin), 
	 "\" is not a registered drag&drop source.", (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    dndPtr->x = x;	/* Save drag&drop location */
    dndPtr->y = y;
    if ((dndPtr->flags & DND_INITIATED) == 0) {
	return TCL_OK;		/* Never initiated any drag operation. */
    }
    if (dndPtr->flags & DND_VOIDED) {
	HideToken(dndPtr);
	return TCL_OK;
    }
    windowPtr = OverTarget(dndPtr);
    if (windowPtr != NULL) {
	if (windowPtr->matches != NULL) {
	    SetProperty(dndPtr->tkwin, dndPtr->dataPtr->formatsAtom, 
			windowPtr->matches);
	}
	MoveToken(dndPtr);	/* Move token to current drag point. */ 
	RaiseToken(dndPtr);
	RelayDropEvent(dndPtr, windowPtr, x, y);
#ifdef notdef
	tokenPtr->nSteps = 10;
	FadeToken(dndPtr);
#endif
    } else {
	CancelDrag(dndPtr);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetdataOp --
 *
 *	Registers one or more data formats with a drag&drop source.  
 *	Each format has a Tcl command associated with it.  This command
 *	is automatically invoked whenever data is pulled from the source
 *	to a target requesting the data in that particular format.  The 
 *	purpose of the Tcl command is to get the data from in the 
 *	application. When the Tcl command is invoked, special percent 
 *	substitutions are made:
 *
 *		%#		Drag&drop transaction timestamp.
 *		%W		Source widget.
 *
 *	If a converter (command) already exists for a format, it 
 *	overwrites the existing command.
 *
 *	Example: dnd getdata .widget "color" { %W cget -bg }
 *
 * Results:
 *	A standard Tcl result.  
 *
 * ------------------------------------------------------------------------
 */
static int
GetdataOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Dnd *dndPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    int isNew, nElem;
    char **cmd;
    register int i;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 3) {
	/* Return list of source data formats */
	for (hPtr = Blt_FirstHashEntry(&(dndPtr->getDataTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Tcl_AppendElement(interp,
		Blt_GetHashKey(&(dndPtr->getDataTable), hPtr));
	}
	return TCL_OK;
    }

    if (argc == 4) {
	hPtr = Blt_FindHashEntry(&(dndPtr->getDataTable), argv[3]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find handler for format \"", 
	     argv[3], "\" for source \"", Tk_PathName(dndPtr->tkwin), "\"",
	     (char *)NULL);
	    return TCL_ERROR;
	}
	cmd = (char **)Blt_GetHashValue(hPtr);
	if (cmd == NULL) {
	    Tcl_SetResult(interp, "", TCL_STATIC);
	} else {
	    Tcl_SetResult(interp, PrintList(cmd), TCL_DYNAMIC);
	}
	return TCL_OK;
    }

    for (i = 3; i < argc; i += 2) {
	hPtr = Blt_CreateHashEntry(&(dndPtr->getDataTable), argv[i], &isNew);
	if (!isNew) {
	    cmd = (char **)Blt_GetHashValue(hPtr);
	    Blt_Free(cmd);
	}
	if (Tcl_SplitList(interp, argv[i + 1], &nElem, &cmd) != TCL_OK) {
	    Blt_DeleteHashEntry(&(dndPtr->getDataTable), hPtr);
	    return TCL_ERROR;
	}
	Blt_SetHashValue(hPtr, cmd);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  NamesOp --
 *
 *	Returns the names of all the drag&drop managers.  If either
 *	a "-source" or "-target" switch is present, only the names of
 *	managers acting as sources or targets respectively are returned.
 *	A pattern argument may also be given.  Only those managers
 *	matching the pattern are returned.
 *
 *	Examples: dnd names
 *		  dnd names -source
 *		  dnd names -target
 *		  dnd names .*label
 * Results:
 *	A standard Tcl result.  A Tcl list of drag&drop manager
 *	names is returned.
 *
 * ------------------------------------------------------------------------
 */
static int
NamesOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    DndInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Dnd *dndPtr;
    int findSources, findTargets;

    findSources = findTargets = TRUE;
    if (argc > 2) {
	if ((argv[2][0] == '-') && (strcmp(argv[2], "-source") == 0)) {
	    findTargets = FALSE;
	    argc--, argv++;
	} else if ((argv[2][0] == '-') && (strcmp(argv[2], "-target") == 0)) {
	    findSources = FALSE;
	    argc--, argv++;
	}
    }
    for (hPtr = Blt_FirstHashEntry(&(dataPtr->dndTable), &cursor); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	dndPtr = (Dnd *)Blt_GetHashValue(hPtr);
	if ((argc > 3) && 
	    (!Tcl_StringMatch(Tk_PathName(dndPtr->tkwin), argv[3]))) {
	    continue;
	}
	if (((findSources) && (dndPtr->isSource)) ||
	    ((findTargets) && (dndPtr->isTarget))) {
	    Tcl_AppendElement(interp, Tk_PathName(dndPtr->tkwin));
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  PullOp --
 *
 *	Pulls the current data from the source in the given format.
 *	application.
 *
 *	dnd pull .widget format data
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Invokes the target's data converter to store the data. 
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PullOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Dnd *dndPtr;		/* drag&drop source record */
    int result;
    char **formatCmd;
    Blt_HashEntry *hPtr;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!dndPtr->isTarget) {
	Tcl_AppendResult(interp, "widget \"", Tk_PathName(dndPtr->tkwin), 
	 "\" is not a registered drag&drop target.", (char *)NULL);
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&(dndPtr->setDataTable), argv[3]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find format \"", argv[3], 
	 "\" in target \"", Tk_PathName(dndPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    formatCmd = (char **)Blt_GetHashValue(hPtr);
    if (dndPtr->pendingPtr == NULL) {
	Tcl_AppendResult(interp, "no drop in progress", (char *)NULL);
	return TCL_ERROR;
    }

    CompleteDataTransaction(dndPtr, argv[3], dndPtr->pendingPtr);
    result = TCL_OK;
    if (Tcl_DStringLength(&(dndPtr->pendingPtr->dString)) > 0) {
	Tcl_DString dString, savedResult;
	char **p;

	Tcl_DStringInit(&dString);
	for (p = formatCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&dString, *p);
	}
	Tcl_DStringAppendElement(&dString, Tk_PathName(dndPtr->tkwin));
	Tcl_DStringAppendElement(&dString, "x");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->dropX));
	Tcl_DStringAppendElement(&dString, "y");
	Tcl_DStringAppendElement(&dString, Blt_Itoa(dndPtr->dropY));
	Tcl_DStringAppendElement(&dString, "timestamp");
	Tcl_DStringAppendElement(&dString, 
		Blt_Utoa(dndPtr->pendingPtr->timestamp));
	Tcl_DStringAppendElement(&dString, "format");
	Tcl_DStringAppendElement(&dString, argv[3]);
	Tcl_DStringAppendElement(&dString, "value");
	Tcl_DStringAppendElement(&dString, 
		Tcl_DStringValue(&(dndPtr->pendingPtr->dString)));
	Tcl_DStringInit(&savedResult);
	Tcl_DStringGetResult(interp, &savedResult);
	if (Tcl_GlobalEval(interp, Tcl_DStringValue(&dString)) != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	Tcl_DStringResult(interp, &savedResult);
	Tcl_DStringFree(&dString);
    }
    return result;
}

/*
 * ------------------------------------------------------------------------
 *
 *  SetdataOp --
 *
 *	Registers one or more data formats with a drag&drop target.  
 *	Each format has a Tcl command associated with it.  This command
 *	is automatically invoked whenever data arrives from a source
 *	to be converted to that particular format.  The purpose of the 
 *	command is to set the data somewhere in the application (either 
 *	using a Tcl command or variable).   When the Tcl command is invoked, 
 *	special percent substitutions are made:
 *
 *		%#		Drag&drop transaction timestamp.
 *		%W		Target widget.
 *		%v		Data value transfered from the source to
 *				be converted into the correct format.
 *
 *	If a converter (command) already exists for a format, it 
 *	overwrites the existing command.
 *
 *	Example: dnd setdata .widget color { . configure -bg %v }
 *
 * Results:
 *	A standard Tcl result.
 *
 * ------------------------------------------------------------------------
 */
static int
SetdataOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Dnd *dndPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    int isNew, nElem;
    char **cmd;
    int i;

    if (GetDnd(clientData, interp, argv[2], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 3) {
	/* Show target handler data formats */
	for (hPtr = Blt_FirstHashEntry(&(dndPtr->setDataTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Tcl_AppendElement(interp,
		Blt_GetHashKey(&(dndPtr->setDataTable), hPtr));
	}
	return TCL_OK;
    }
    if (argc == 4) {
	hPtr = Blt_FindHashEntry(&(dndPtr->setDataTable), argv[3]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find handler for format \"", 
	     argv[3], "\" for target \"", Tk_PathName(dndPtr->tkwin), "\"",
	     (char *)NULL);
	    return TCL_ERROR;
	}
	cmd = (char **)Blt_GetHashValue(hPtr);
	if (cmd == NULL) {
	    Tcl_SetResult(interp, "", TCL_STATIC);
	} else {
	    Tcl_SetResult(interp, PrintList(cmd), TCL_DYNAMIC);
	}
	return TCL_OK;
    }
    for (i = 3; i < argc; i += 2) {
	hPtr = Blt_CreateHashEntry(&(dndPtr->setDataTable), argv[i], &isNew);
	if (!isNew) {
	    cmd = (char **)Blt_GetHashValue(hPtr);
	    Blt_Free(cmd);
	}
	if (Tcl_SplitList(interp, argv[i + 1], &nElem, &cmd) != TCL_OK) {
	    Blt_DeleteHashEntry(&(dndPtr->setDataTable), hPtr);
	    return TCL_ERROR;
	}
	Blt_SetHashValue(hPtr, cmd);
    }
    AddTargetProperty(dndPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  RegisterOp --
 *
 *  dnd register .window 
 * ------------------------------------------------------------------------
 */
static int
RegisterOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    DndInterpData *dataPtr = clientData;
    Tk_Window tkwin;
    Blt_HashEntry *hPtr;
    Dnd *dndPtr;
    int isNew;

    tkwin = Tk_NameToWindow(interp, argv[2], dataPtr->mainWindow);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_CreateHashEntry(&(dataPtr->dndTable), (char *)tkwin, &isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "\"", Tk_PathName(tkwin), 
	     "\" is already registered as a drag&drop manager", (char *)NULL);
	return TCL_ERROR;
    }
    dndPtr = CreateDnd(interp, tkwin);
    dndPtr->hashPtr = hPtr;
    dndPtr->dataPtr = dataPtr;
    Blt_SetHashValue(hPtr, dndPtr);
    if (Tk_ConfigureWidget(interp, dndPtr->tkwin, configSpecs, argc - 3,
	   argv + 3, (char *)dndPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    if (ConfigureDnd(interp, dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  TokenWindowOp --
 *
 * ------------------------------------------------------------------------
 */
static int
TokenWindowOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Dnd *dndPtr;
    int flags;

    if (GetDnd(clientData, interp, argv[3], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = 0;
    if (dndPtr->tokenPtr == NULL) {
	if (CreateToken(interp, dndPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	flags = TK_CONFIG_ARGV_ONLY;
    }
    if (ConfigureToken(interp, dndPtr, argc - 4, argv + 4, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(dndPtr->tokenPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  TokenCgetOp --
 *
 *	Called to process an (argc,argv) list to configure (or
 *	reconfigure) a drag&drop widget.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED*/
static int
TokenCgetOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Dnd *dndPtr;

    if (GetDnd(clientData, interp, argv[3], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (dndPtr->tokenPtr == NULL) {
	Tcl_AppendResult(interp, "no token created for \"", argv[3], "\"",
		 (char *)NULL);
	return TCL_ERROR;
    }
    return Tk_ConfigureValue(interp, dndPtr->tokenPtr->tkwin, tokenConfigSpecs, 
	     (char *)dndPtr->tokenPtr, argv[4], TK_CONFIG_ARGV_ONLY);
}

/*
 * ------------------------------------------------------------------------
 *
 *  TokenConfigureOp --
 *
 * ------------------------------------------------------------------------
 */
static int
TokenConfigureOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Token *tokenPtr;
    Dnd *dndPtr;
    int flags;

    if (GetDnd(clientData, interp, argv[3], &dndPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = TK_CONFIG_ARGV_ONLY;
    if (dndPtr->tokenPtr == NULL) {
	Tcl_AppendResult(interp, "no token created for \"", argv[3], "\"",
		 (char *)NULL);
	return TCL_ERROR;
    }
    tokenPtr = dndPtr->tokenPtr;
    if (argc == 3) {
	return Tk_ConfigureInfo(interp, tokenPtr->tkwin, tokenConfigSpecs,
	    (char *)tokenPtr, (char *)NULL, flags);
    } else if (argc == 4) {
	return Tk_ConfigureInfo(interp, tokenPtr->tkwin, tokenConfigSpecs,
	    (char *)tokenPtr, argv[3], flags);
    } 
    return ConfigureToken(interp, dndPtr, argc - 4, argv + 4, flags);
}

static Blt_OpSpec tokenOps[] =
{
    {"cget", 2, (Blt_Op)TokenCgetOp, 5, 5, "widget option",},
    {"configure", 2, (Blt_Op)TokenConfigureOp, 4, 0, 
	"widget ?option value?...",},
    {"window", 5, (Blt_Op)TokenWindowOp, 4, 0, 
	"widget ?option value?...",},
};

static int nTokenOps = sizeof(tokenOps) / sizeof(Blt_OpSpec);

/*
 * ------------------------------------------------------------------------
 *
 *  TokenOp --
 *
 * ------------------------------------------------------------------------
 */
static int
TokenOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nTokenOps, tokenOps, BLT_OP_ARG2, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

static Blt_OpSpec dndOps[] =
{
    {"cancel", 2, (Blt_Op)CancelOp, 3, 3, "widget",},
    {"cget", 2, (Blt_Op)CgetOp, 4, 4, "widget option",},
    {"configure", 4, (Blt_Op)ConfigureOp, 3, 0, 
	"widget ?option value?...",},
#ifdef notdef
    {"convert", 4, (Blt_Op)ConvertOp, 5, 5, "widget data format",},
#endif
    {"delete", 2, (Blt_Op)DeleteOp, 3, 0,"?-source|-target? widget...",},
    {"drag", 3, (Blt_Op)DragOp, 3, 0, "widget x y ?token?",},
    {"drop", 3, (Blt_Op)DropOp, 3, 0, "widget x y ?token?",},
    {"getdata", 1, (Blt_Op)GetdataOp, 3, 0, "widget ?format command?",},
    {"names", 1, (Blt_Op)NamesOp, 2, 4, "?-source|-target? ?pattern?",},
    {"pull", 1, (Blt_Op)PullOp, 4, 4, "widget format",},
    {"register", 1, (Blt_Op)RegisterOp, 3, 0, "widget ?option value?...",},
    {"select", 3, (Blt_Op)SelectOp, 6, 6, "widget x y timestamp",},
    {"setdata", 3, (Blt_Op)SetdataOp, 3, 0, "widget ?format command?",},
    {"token", 5, (Blt_Op)TokenOp, 3, 0, "args...",},
};

static int nDndOps = sizeof(dndOps) / sizeof(Blt_OpSpec);

/*
 * ------------------------------------------------------------------------
 *
 *  DndCmd --
 *
 *	Invoked by TCL whenever the user issues a drag&drop command.
 *
 * ------------------------------------------------------------------------
 */
static int
DndCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;		/* current interpreter */
    int argc;			/* number of arguments */
    char **argv;		/* argument strings */
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nDndOps, dndOps, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * DndInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the "dnd" command is 
 *	destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the hash table containing the drag&drop managers.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
DndInterpDeleteProc(clientData, interp)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
{
    DndInterpData *dataPtr = clientData;
    Dnd *dndPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    for (hPtr = Blt_FirstHashEntry(&(dataPtr->dndTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	dndPtr = (Dnd *)Blt_GetHashValue(hPtr);
	dndPtr->hashPtr = NULL;
	DestroyDnd((DestroyData)dndPtr);
    }
    Blt_DeleteHashTable(&(dataPtr->dndTable));
    Tcl_DeleteAssocData(interp, DND_THREAD_KEY);
    Blt_Free(dataPtr);
}

static DndInterpData *
GetDndInterpData(interp)
     Tcl_Interp *interp;
{
    DndInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (DndInterpData *)Tcl_GetAssocData(interp, DND_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	Display *display;
	Tk_Window tkwin;

	dataPtr = Blt_Malloc(sizeof(DndInterpData));
	assert(dataPtr);
	tkwin = Tk_MainWindow(interp);
	display = Tk_Display(tkwin);
	dataPtr->mainWindow = tkwin;
	dataPtr->display = display;
	Tcl_SetAssocData(interp, DND_THREAD_KEY, DndInterpDeleteProc,
		dataPtr);
	Blt_InitHashTable(&(dataPtr->dndTable), BLT_ONE_WORD_KEYS);
	dataPtr->mesgAtom = XInternAtom(display, "BLT Dnd Message", False);
	dataPtr->targetAtom = XInternAtom(display, "BLT Dnd Target", False);
	dataPtr->formatsAtom = XInternAtom(display, "BLT Dnd Formats",False);
	dataPtr->commAtom = XInternAtom(display, "BLT Dnd CommData", False);

#ifdef HAVE_XDND
	dataPtr->XdndActionListAtom = XInternAtom(display, "XdndActionList", 
		False);
	dataPtr->XdndAwareAtom = XInternAtom(display, "XdndAware", False);
	dataPtr->XdndEnterAtom = XInternAtom(display, "XdndEnter", False);
	dataPtr->XdndFinishedAtom = XInternAtom(display, "XdndFinished", False);
	dataPtr->XdndLeaveAtom = XInternAtom(display, "XdndLeave", False);
	dataPtr->XdndPositionAtom = XInternAtom(display, "XdndPosition", False);
	dataPtr->XdndSelectionAtom = XInternAtom(display, "XdndSelection", 
						 False);
	dataPtr->XdndStatusAtom = XInternAtom(display, "XdndStatus", False);
	dataPtr->XdndTypeListAtom = XInternAtom(display, "XdndTypeList", False);
#endif /* HAVE_XDND */
    }
    return dataPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  Blt_DndInit --
 *
 *	Adds the drag&drop command to the given interpreter.  Should
 *	be invoked to properly install the command whenever a new
 *	interpreter is created.
 *
 * ------------------------------------------------------------------------
 */
int
Blt_DndInit(interp)
    Tcl_Interp *interp;		/* interpreter to be updated */
{
    static Blt_CmdSpec cmdSpec =
    {
	"dnd", DndCmd
    };
    DndInterpData *dataPtr;

    dataPtr = GetDndInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#ifdef notdef
/*
 * Registers bitmap outline of dragged data, used to indicate
 * what is being dragged by source.  Bitmap is XOR-ed as cursor/token
 * is moved around the screen.
 */
static void
Blt_DndSetOutlineBitmap(tkwin, bitmap, x, y)
    Tk_Window tkwin;
    Pixmap bitmap;
    int x, y;
{
    
}
#endif

#ifdef HAVE_XDND

static void
XDndFreeFormats(handlerPtr) 
    XDndHandler *handlerPtr;
{
    if (handlerPtr->formatArr != NULL) {
	char **p;

	for (p = handlerPtr->formatArr; *p != NULL; p++) {
	    XFree(*p);
	}
	Blt_Free(handlerPtr->formatArr);
	handlerPtr->formatArr = NULL;
    }
}

static char **
XDndGetFormats(handlerPtr, eventPtr)
    XDndHandler *handlerPtr;
    XEvent *eventPtr;
{
    int flags;
    Window window;
    unsigned char *data;
    Atom typeAtom;
    Atom format;
    int nItems, bytesAfter;
    Atom *atomArr;
    char *nameArr[XDND_MAX_TYPES + 1];
    Display *display;

    XDndFreeFormats(handlerPtr);
    display = eventPtr->xclient.display;
    window = eventPtr->xclient.data.l[0];
    flags = eventPtr->xclient.data.l[1];
    data = NULL;
    if (flags & 0x01) {
	result = XGetWindowProperty(
	    display,		/* Display of window. */
	    window,		/* Window holding the property. */
	    handlerPtr->dataPtr->XdndTypeListAtom, /* Name of property. */
	    0,			/* Offset of data (for multiple reads). */
	    XDND_MAX_TYPES,	/* Maximum number of items to read. */
	    False,		/* If true, delete the property. */
	    XA_ATOM,		/* Desired type of property. */
	    &typeAtom,		/* (out) Actual type of the property. */
	    &format,		/* (out) Actual format of the property. */
	    &nItems,		/* (out) # of items in specified format. */
	    &bytesAfter,	/* (out) # of bytes remaining to be read. */
	    (unsigned char **)&data);
	if ((result != Success) || (format != 32) || (typeAtom != XA_ATOM)) {
	    if (data != NULL) {
		XFree((char *)data);
		return (char **)NULL;
	    }
	}
	atomArr = (Atom *)data;
	nAtoms = nItems;
    } else {
	atomArr = &(eventPtr->xclient.data.l[2]);
	nAtoms = 3;
    }
    formatArr = Blt_Calloc(nAtoms + 1, sizeof(char *));
    for (i = 0; (i < nAtoms) && (atomArr[i] != None); i++) {
	formatArr[i] = XGetAtomName(display, atomArr[i]);
    }
    if (data != NULL) {
	XFree((char *)data);
    }
    handlerPtr->formatArr = formatArr;
}

static char *
GetMatchingFormats(dndPtr, formatArr)
    Dnd *dndPtr;
    char **formatArr;
{
    int nMatches;

    nMatches = 0;
    Tcl_DStringInit(&dString);
    for (p = formatArr; *p != NULL; p++) {
	for(hPtr = Blt_FirstHashEntry(&(dndPtr->setDataTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    fmt = Blt_GetHashKey(&(dndPtr->setDataTable), hPtr);
	    if ((*fmt == **p) && (strcmp(fmt, *p) == 0)) {
		Tcl_DStringAppendElement(&dString, *p);
		nMatches++;
		break;
	    }
	}
    }
    if (nMatches > 0) {
	char *string;

	string = Blt_Strdup(Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
	return string;
    }
    return NULL;
}

static void
XDndPointerEvent(handlerPtr, eventPtr)
    XDndHandler *handlerPtr;
    XEvent *eventPtr;
{
    Tk_Window tkwin;
    int flags;
    Atom action;
    Window window;
    
    flags = 0;
    action = None;
    window = eventPtr->xclient.data.l[MESG_XDND_WINDOW];
    /* 
     * If the XDND source has no formats specified, don't process any further.
     * Simply send a "no accept" action with the message.
     */
    if (handlerPtr->formatArr != NULL) { 
	Dnd *newPtr, *oldPtr;
	int point;
	int button, keyState;
	int x, y;
	char *formats;

	point = (int)eventPtr->xclient.data.l[MESG_XDND_POINT];
	UNPACK(point, x, y);

	/*  
	 * See if the mouse pointer currently over a drop target. We first
	 * determine what Tk window is under the mouse, and then check if 
	 * that window is registered as a drop target.  
	 */
	newPtr = NULL;
	tkwin = Tk_CoordsToWindow(x, y, handlerPtr->tkwin);
	if (tkwin != NULL) {
	    Blt_HashEntry *hPtr;
	    
	    hPtr = Blt_FindHashEntry(&(handlerPtr->dataPtr->dndTable), 
		(char *)tkwin);
	    if (hPtr != NULL) {
		newPtr = (Dnd *)Blt_GetHashValue(hPtr);
		if (!newPtr->isTarget) {
		    newPtr = NULL; /* Not a DND target. */
		}
		formats = GetMatchingFormats(newPtr, handlerPtr->formatArr);
		if (formats == NULL) {
		    newPtr = NULL; /* Source has no matching formats. */
		} 
	    }
	}
	button = keyState = 0;
	oldPtr = handlerPtr->dndPtr;
	resp = DROP_CANCEL;
	if (newPtr == oldPtr) {
	    if ((oldPtr != NULL) && (oldPtr->motionCmd != NULL)) {
		resp = InvokeCallback(oldPtr, oldPtr->motionCmd, x, y, formats,
			 button, keyState, dndPtr->timestamp);
	    }
	} else {
	    if ((oldPtr != NULL) && (oldPtr->leaveCmd != NULL)) {
		InvokeCallback(oldPtr, oldPtr->leaveCmd, x, y, formats, button,
			 keyState, dndPtr->timestamp);
	    }
	    if ((newPtr != NULL) && (newPtr->enterCmd != NULL)) {
		resp = InvokeCallback(newPtr, newPtr->enterCmd, x, y, formats, 
			button, keyState, dndPtr->timestamp);
	    }
	    handlerPtr->dndPtr = newPtr;
	    /* 
	     * Save the current mouse position, since we get them from the
	     * drop message. 
	     */
	    newPtr->x = x;	
	    newPtr->y = y;
	} 
	if (formats != NULL) {
	    Blt_Free(formats);
	}
	flags = XDND_FLAGS_WANT_POSITION_MSGS;
	if (resp) {
	    flags |= XDND_FLAGS_ACCEPT_DROP;
	    action = handlerPtr->dataPtr->XdndActionCopyAtom;
	}
    }
    /* Target-to-Source: Drag result message. */
    SendClientMsg(handlerPtr->display, window, 
	handlerPtr->dataPtr->XdndStatusAtom, handlerPtr->window, 
	flags, 0, 0, action);
}

static void
XDndDropEvent(handlerPtr, eventPtr)
    XDndHandler *handlerPtr;
    XEvent *eventPtr;
{
    Tk_Window tkwin;
    int flags;
    Atom action;
    Window window;
    int timestamp;

    flags = 0;
    action = None;
    window = eventPtr->xclient.data.l[MESG_XDND_WINDOW];
    timestamp = eventPtr->xclient.data.l[MESG_XDND_TIMESTAMP];

    /* 
     * If no formats were specified for the XDND source or if the last 
     * motion event did not place the mouse over a valid drop target, 
     * don't process any further. Simply send a "no accept" action with 
     * the message.
     */
    if ((handlerPtr->formatArr != NULL) && (handlerPtr->dndPtr != NULL)) { 
	int button, keyState;
	Dnd *dndPtr = handlerPtr->dndPtr;
	DropPending pending;
	int resp;

	button = keyState = 0;	/* Protocol doesn't supply this information. */

	/* Set up temporary bookkeeping for the drop transaction */
	memset (&pending, 0, sizeof(pending));
	pending.window = window;
	pending.display = eventPtr->xclient.display;
	pending.timestamp = timestamp;
	pending.protocol = PROTO_XDND;
	pending.packetSize = GetMaxPropertySize(pending.display);
	Tcl_DStringInit(&(pending.dString));
	
	formats = GetMatchingFormats(handlerPtr->dndPtr, handlerPtr->formatArr);
	if (formats == NULL) {
	}
	dndPtr->pendingPtr = &pending;
	resp = AcceptDrop(dndPtr, dndPtr->x, dndPtr->y, formats, button,
	     keyState, action, timestamp);
	dndPtr->pendingPtr = NULL;
	if (resp) {
	    flags |= XDND_FLAGS_ACCEPT_DROP;
	    action = handlerPtr->dataPtr->XdndActionCopyAtom;
	}
    }
    /* Target-to-Source: Drag result message. */
    SendClientMsg(handlerPtr->display, window, 
	handlerPtr->dataPtr->XdndStatusAtom, handlerPtr->window, 
	flags, 0, 0, action);
}

/*
 * ------------------------------------------------------------------------
 *
 *  XDndProtoEventProc --
 *
 *	Invoked by Tk_HandleEvent whenever a DestroyNotify event is received
 *	on a registered drag&drop source widget.
 *
 * ------------------------------------------------------------------------
 */
static int
XDndProtoEventProc(clientData, eventPtr)
    ClientData clientData;	/* Drag&drop record. */
    XEvent *eventPtr;		/* Event description. */
{
    DndInterpData *dataPtr = clientData;
    Tk_Window tkwin;
    Blt_HashEntry *hPtr;
    XDndHandler *handlerPtr;
    int point;
    int x, y;
    Atom mesg;

    if (eventPtr->type != ClientMessage) {
	return 0;		/* Not a ClientMessage event. */
    }
    /* Was the recipient a registered toplevel window? */
    hPtr = Blt_FindHashEntry(&(dataPtr->handlerTable), 
	     (char *)eventPtr->xclient.window);
    if (hPtr == NULL) {
	return 0;		/* No handler registered with window. */
    }
    handlerPtr = (XDndHandler *)Blt_GetHashValue(hPtr);
    mesg = eventPtr->xclient.message_type;
    if (mesg == dataPtr->XdndEnterAtom) {
	XDndGetFormats(handlerPtr, eventPtr);
	handlerPtr->dndPtr = NULL;
    } else if (mesg == dataPtr->XdndPositionAtom) {
	XDndPointerEvent(handlerPtr, eventPtr);
    } else if (mesg == dataPtr->XdndLeaveAtom) {
	XDndFreeFormats(handlerPtr);	/* Free up any collected formats. */
	if (handlerPtr->dndPtr != NULL) {
	    InvokeCallback(handlerPtr->dndPtr, handlerPtr->dndPtr->leaveCmd, 
		-1, -1, NULL, 0, 0);
	    /* Send leave event to drop target. */
	}
    } else if (mesg == dataPtr->XdndDropAtom) {
	XDndDropEvent(handlerPtr, eventPtr);
    } else {
	fprintf(stderr, "Unknown client message type = 0x%x\n", mesg);
	return 0;		/* Unknown message type.  */
    }
    return 1;
}

static XDndHandler *
XDndCreateHandler(dndPtr) 
    Dnd *dndPtr;
{
    Tk_Window tkwin;
    Window window;
    Blt_HashEntry *hPtr;
    int isNew;
    XDndHandler *handlerPtr;

    /* 
     * Find the containing toplevel of this window. See if an XDND 
     * handler is already registered for it.  
     */
    tkwin = Blt_GetToplevel(dndPtr->tkwin);
    window = Blt_GetRealWindowId(tkwin); /* Use the wrapper window as
					  * the real toplevel window. */
    hPtr = Blt_CreateHashEntry(&(dataPtr->XDndHandlerTable), (char *)window, 
	&isNew);
    if (!isNew) {
	handlerPtr = (XDndHandler *)Blt_GetHashEntry(hPtr);
	handlerPtr->refCount++;
    } else {
	handlerPtr = Blt_Malloc(sizeof(XDndHandler));
	handlerPtr->tkwin = tkwin;
	handlerPtr->dndPtr = NULL;
	handlerPtr->refCount = 1;
	handlerPtr->dataPtr = dataPtr;
	/* FIXME */
	SetProperty(window, dataPtr->XdndAwareAtom, "3");
	Blt_SetHashValue(hPtr, handlerPtr);
    }
    return handlerPtr;
}

static void
XDndDeleteHandler(dndPtr) 
    Dnd *dndPtr;
{
    Tk_Window tkwin;
    Window window;
    Blt_HashEntry *hPtr;

    tkwin = Blt_GetToplevel(dndPtr->tkwin);
    window = Blt_GetRealWindowId(tkwin); /* Use the wrapper window as the real 
					  * toplevel window. */
    hPtr = Blt_FindHashEntry(&(dataPtr->XDndHandlerTable), (char *)window);
    if (hPtr != NULL) {
	XDndHandler *handlerPtr;

	handlerPtr = (XDndHandler *)Blt_GetHashEntry(hPtr);
	handlerPtr->refCount--;
	if (handlerPtr->refCount == 0) {
	    XDndFreeFormats(handlerPtr); 
	    XDeleteProperty(dndPtr->display, window, 
		dndPtr->dataPtr->XdndAwareAtom);
	    Blt_DeleteHashEntry(&(dataPtr->XDndHandlerTable), hPtr);
	    Blt_Free(handlerPtr);
	}
    }
}

#endif /* HAVE_XDND */

#endif /* NO_DRAGDROP */
