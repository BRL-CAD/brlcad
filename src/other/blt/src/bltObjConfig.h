/* 
 * bltObjConfig.h --
 *
 *	This file contains the Tcl_Obj based versions of the old
 *	Tk_ConfigureWidget procedures. 
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef BLT_OBJCONFIG_H
#define BLT_OBJCONFIG_H

/*
 * This is a Tcl_Obj based replacement for the widget configuration
 * functions in Tk.  
 *
 * What not use the new Tk_Option interface?
 *
 *	There were design changes in the new Tk_Option interface that
 *	make it unwieldy.  
 *
 *	o You have to dynamically allocate, store, and deallocate
 *	  your option table.  
 *      o The Tk_FreeConfigOptions routine requires a tkwin argument.
 *	  Unfortunately, most widgets save the display pointer and 
 *	  deference their tkwin when the window is destroyed.  
 *	o There's no TK_CONFIG_CUSTOM functionality.  This means that
 *	  save special options must be saved as strings by 
 *	  Tk_ConfigureWidget and processed later, thus losing the 
 *	  benefits of Tcl_Objs.  It also make error handling 
 *	  problematic, since you don't pick up certain errors like 
 *	  
 *	    .widget configure -myoption bad -myoption good
 *        
 *	  You will never see the first "bad" value.
 *	o Especially compared to the former Tk_ConfigureWidget calls,
 *	  the new interface is overly complex.  If there was a big
 *	  performance win, it might be worth the effort.  But let's 
 *	  face it, this biggest wins are in processing custom options
 *	  values with thousands of elements.  Most common resources 
 *	  (font, color, etc) have string tokens anyways.
 *
 *	On the other hand, the replacement functions in this file fell
 *	into place quite easily both from the aspect of API writer and
 *	user.  The biggest benefit is that you don't need to change lots
 *	of working code just to get the benefits of Tcl_Objs.
 * 
 */
#define SIDE_LEFT		(0)
#define SIDE_TOP		(1)
#define SIDE_RIGHT		(2)
#define SIDE_BOTTOM		(3)

#define SIDE_HORIZONTAL(s)	(!((s) & 0x1))
#define SIDE_VERTICAL(s)	((s) & 0x1)

#ifndef Blt_Offset
#ifdef offsetof
#define Blt_Offset(type, field) ((int) offsetof(type, field))
#else
#define Blt_Offset(type, field) ((int) ((char *) &((type *) 0)->field))
#endif
#endif /* Blt_Offset */

typedef int (Blt_OptionParseProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr, char *widgRec, 
	int offset));					       
typedef Tcl_Obj *(Blt_OptionPrintProc) _ANSI_ARGS_((ClientData clientData, 
	Tcl_Interp *interp, Tk_Window tkwin, char *widgRec, int offset));
typedef void (Blt_OptionFreeProc) _ANSI_ARGS_((ClientData clientData,
	Display *display, char *widgRec, int offset));

typedef struct Blt_CustomOption {
    Blt_OptionParseProc *parseProc;	/* Procedure to call to parse an
					 * option and store it in converted
					 * form. */
    Blt_OptionPrintProc *printProc;	/* Procedure to return a printable
					 * string describing an existing
					 * option. */
    Blt_OptionFreeProc *freeProc;	/* Procedure to free the value. */

    ClientData clientData;		/* Arbitrary one-word value used by
					 * option parser:  passed to
					 * parseProc and printProc. */
} Blt_CustomOption;

/*
 * Structure used to specify information for Tk_ConfigureWidget.  Each
 * structure gives complete information for one option, including
 * how the option is specified on the command line, where it appears
 * in the option database, etc.
 */

typedef struct {
    int type;			/* Type of option, such as BLT_CONFIG_COLOR;
				 * see definitions below.  Last option in
				 * table must have type BLT_CONFIG_END. */
    char *switchName;		/* Switch used to specify option in argv.
				 * NULL means this spec is part of a group. */
    Tk_Uid dbName;		/* Name for option in option database. */
    Tk_Uid dbClass;		/* Class for option in database. */
    Tk_Uid defValue;		/* Default value for option if not
				 * specified in command line or database. */
    int offset;			/* Where in widget record to store value;
				 * use Tk_Offset macro to generate values
				 * for this. */
    int specFlags;		/* Any combination of the values defined
				 * below;  other bits are used internally
				 * by tkConfig.c. */
    Blt_CustomOption *customPtr; /* If type is BLT_CONFIG_CUSTOM then this is
				 * a pointer to info about how to parse and
				 * print the option.  Otherwise it is
				 * irrelevant. */
} Blt_ConfigSpec;

/*
 * Type values for Blt_ConfigSpec structures.  See the user
 * documentation for details.
 */

typedef enum {
    BLT_CONFIG_ACTIVE_CURSOR, 
    BLT_CONFIG_ANCHOR, 
    BLT_CONFIG_BITMAP,
    BLT_CONFIG_BOOLEAN, 
    BLT_CONFIG_BORDER, 
    BLT_CONFIG_CAP_STYLE, 
    BLT_CONFIG_COLOR, 
    BLT_CONFIG_CURSOR, 
    BLT_CONFIG_CUSTOM, 
    BLT_CONFIG_DOUBLE, 
    BLT_CONFIG_FONT, 
    BLT_CONFIG_INT, 
    BLT_CONFIG_JOIN_STYLE,
    BLT_CONFIG_JUSTIFY, 
    BLT_CONFIG_MM, 
    BLT_CONFIG_PIXELS, 
    BLT_CONFIG_RELIEF, 
    BLT_CONFIG_STRING,
    BLT_CONFIG_SYNONYM, 
    BLT_CONFIG_UID, 
    BLT_CONFIG_WINDOW, 

    BLT_CONFIG_BITFLAG,
    BLT_CONFIG_DASHES,
    BLT_CONFIG_DISTANCE,	/*  */
    BLT_CONFIG_FILL,
    BLT_CONFIG_FLOAT, 
    BLT_CONFIG_LIST,
    BLT_CONFIG_LISTOBJ,
    BLT_CONFIG_PAD,
    BLT_CONFIG_POS_DISTANCE,	/*  */
    BLT_CONFIG_SHADOW,		/*  */
    BLT_CONFIG_SIDE,
    BLT_CONFIG_STATE, 
    BLT_CONFIG_TILE,
    
    BLT_CONFIG_END
} Blt_ConfigTypes;

/*
 * Possible values for flags argument to Tk_ConfigureWidget:
 */

#define BLT_CONFIG_OBJV_ONLY	1
#define BLT_CONFIG_OBJS		0x80

/*
 * Possible flag values for Blt_ConfigSpec structures.  Any bits at
 * or above BLT_CONFIG_USER_BIT may be used by clients for selecting
 * certain entries.  Before changing any values here, coordinate with
 * tkOldConfig.c (internal-use-only flags are defined there).
 */

#define BLT_CONFIG_NULL_OK		1
#define BLT_CONFIG_COLOR_ONLY		2
#define BLT_CONFIG_MONO_ONLY		4
#define BLT_CONFIG_DONT_SET_DEFAULT	8
#define BLT_CONFIG_OPTION_SPECIFIED	0x10
#define BLT_CONFIG_USER_BIT		0x100

/*
 * Values for "flags" field of Blt_ConfigSpec structures.  Be sure
 * to coordinate these values with those defined in tk.h
 * (BLT_CONFIG_COLOR_ONLY, etc.).  There must not be overlap!
 *
 * INIT -		Non-zero means (char *) things have been
 *			converted to Tk_Uid's.
 */

#define INIT		0x20

EXTERN int Blt_ConfigureInfoFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Blt_ConfigSpec *specs, char *widgRec, 
	Tcl_Obj *objPtr, int flags));
EXTERN int Blt_ConfigureValueFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Blt_ConfigSpec *specs, char * widgRec, 
	Tcl_Obj *objPtr, int flags));
EXTERN int Blt_ConfigureWidgetFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Blt_ConfigSpec *specs, int objc, Tcl_Obj *CONST *objv,
	char *widgRec, int flags));
EXTERN int Blt_ConfigureComponentFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, char *name, char *className, Blt_ConfigSpec *specs,
	int objc, Tcl_Obj *CONST *objv, char *widgRec, int flags));

EXTERN int Blt_ObjConfigModified _ANSI_ARGS_(TCL_VARARGS(Blt_ConfigSpec *, specs));
EXTERN void Blt_FreeObjOptions _ANSI_ARGS_((Blt_ConfigSpec *specs, 
	char *widgRec, Display *display, int needFlags));

EXTERN int Blt_ObjIsOption _ANSI_ARGS_((Blt_ConfigSpec *specs, Tcl_Obj *objPtr,
	int flags));

EXTERN int Blt_GetPositionFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, int *indexPtr));

EXTERN int Blt_GetPixelsFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr, int flags, int *valuePtr));

EXTERN int Blt_GetPadFromObj  _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr, Blt_Pad *padPtr));

EXTERN int Blt_GetShadowFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr, Shadow *shadowPtr));

EXTERN int Blt_GetStateFromObj  _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, int *statePtr));

EXTERN int Blt_GetFillFromObj  _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, int *fillPtr));

EXTERN int Blt_GetDashesFromObj  _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, Blt_Dashes *dashesPtr));


#if ((TK_VERSION_NUMBER >= _VERSION(8,0,0)) && \
     (TK_VERSION_NUMBER < _VERSION(8,1,0)))
EXTERN int Tk_GetAnchorFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, Tk_Anchor *anchorPtr));
EXTERN int Tk_GetJustifyFromObj _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Obj *objPtr, Tk_Justify *justifyPtr));
EXTERN int Tk_GetReliefFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tcl_Obj *objPtr, int *reliefPtr));
EXTERN int Tk_GetMMFromObj _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin,
	Tcl_Obj *objPtr, double *doublePtr));
EXTERN int Tk_GetPixelsFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr, int *intPtr));
EXTERN Tk_3DBorder Tk_Alloc3DBorderFromObj _ANSI_ARGS_((Tcl_Interp *interp,
	Tk_Window tkwin, Tcl_Obj *objPtr));
EXTERN Pixmap Tk_AllocBitmapFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr));
EXTERN Tk_Font Tk_AllocFontFromObj _ANSI_ARGS_((Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr));
EXTERN Tk_Cursor Tk_AllocCursorFromObj _ANSI_ARGS_((Tcl_Interp *interp,
	Tk_Window tkwin, Tcl_Obj *objPtr));
EXTERN XColor *Tk_AllocColorFromObj _ANSI_ARGS_((Tcl_Interp *interp,
	Tk_Window tkwin, Tcl_Obj *objPtr));
#endif /* 8.0 */

#endif /* BLT_OBJCONFIG_H */
