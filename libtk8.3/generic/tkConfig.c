/* 
 * tkConfig.c --
 *
 *	This file contains procedures that manage configuration options
 *	for widgets and other things.
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

/*
 * Temporary flag for working on new config package.
 */

#if 0

/*
 * used only for removing the old config code
 */

#define __NO_OLD_CONFIG
#endif

#include "tk.h"
#include "tkInt.h"
#include "tkPort.h"
#include "tkFont.h"

/*
 * The following definition is an AssocData key used to keep track of
 * all of the option tables that have been created for an interpreter.
 */

#define OPTION_HASH_KEY "TkOptionTable"

/*
 * The following two structures are used along with Tk_OptionSpec
 * structures to manage configuration options.  Tk_OptionSpec is
 * static templates that are compiled into the code of a widget
 * or other object manager.  However, to look up options efficiently
 * we need to supplement the static information with additional
 * dynamic information, and this dynamic information may be different
 * for each application.  Thus we create structures of the following
 * two types to hold all of the dynamic information; this is done
 * by Tk_CreateOptionTable.
 * 
 * One of the following structures corresponds to each Tk_OptionSpec.
 * These structures exist as arrays inside TkOptionTable structures.
 */

typedef struct TkOption {
    const Tk_OptionSpec *specPtr;	/* The original spec from the template
					 * passed to Tk_CreateOptionTable.*/
    Tk_Uid dbNameUID;		 	/* The Uid form of the option database 
					 * name. */
    Tk_Uid dbClassUID;			/* The Uid form of the option database
					 * class name. */
    Tcl_Obj *defaultPtr;		/* Default value for this option. */
    union {
	Tcl_Obj *monoColorPtr;		/* For color and border options, this
					 * is an alternate default value to
					 * use on monochrome displays. */
	struct TkOption *synonymPtr;	/* For synonym options, this points to
					 * the master entry. */
    } extra;
    int flags;				/* Miscellaneous flag values; see
					 * below for definitions. */
} Option;

/*
 * Flag bits defined for Option structures:
 *
 * OPTION_NEEDS_FREEING -	1 means that FreeResources must be
 *				invoke to free resources associated with
 *				the option when it is no longer needed.
 */

#define OPTION_NEEDS_FREEING		1

/*
 * One of the following exists for each Tk_OptionSpec array that has
 * been passed to Tk_CreateOptionTable.
 */

typedef struct OptionTable {
    int refCount;			/* Counts the number of uses of this
					 * table (the number of times
					 * Tk_CreateOptionTable has returned
					 * it).  This can be greater than 1 if
					 * it is shared along several option
					 * table  chains, or if the same table
					 * is used for multiple purposes. */
    Tcl_HashEntry *hashEntryPtr;	/* Hash table entry that refers to this
					 * table; used to delete the entry. */
    struct OptionTable *nextPtr;	/* If templatePtr was part of a chain
					 * of templates, this points to the
					 * table corresponding to the next
					 * template in the chain. */
    int numOptions;			/* The number of items in the options
					 * array below. */
    Option options[1];			/* Information about the individual
					 * options in the table.  This must be
					 * the last field in the structure:
					 * the actual size of the array will
					 * be numOptions, not 1. */
} OptionTable;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		DoObjConfig _ANSI_ARGS_((Tcl_Interp *interp,
			    char *recordPtr, Option *optionPtr,
			    Tcl_Obj *valuePtr, Tk_Window tkwin, 
			    Tk_SavedOption *savePtr));
static void		DestroyOptionHashTable _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static void		FreeResources _ANSI_ARGS_((Option *optionPtr, 
			    Tcl_Obj *objPtr, char *internalPtr,
			    Tk_Window tkwin));
static Tcl_Obj *	GetConfigList _ANSI_ARGS_((char *recordPtr,
			    Option *optionPtr, Tk_Window tkwin));
static Tcl_Obj *	GetObjectForOption _ANSI_ARGS_((char *recordPtr,
			    Option *optionPtr, Tk_Window tkwin));
static Option *		GetOptionFromObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, OptionTable *tablePtr));
static int		ObjectIsEmpty _ANSI_ARGS_((Tcl_Obj *objPtr));
static int		SetOptionFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));

/*
 * The structure below defines an object type that is used to cache the
 * result of looking up an option name.  If an object has this type, then
 * its internalPtr1 field points to the OptionTable in which it was looked up,
 * and the internalPtr2 field points to the entry that matched.
 */

Tcl_ObjType optionType = {
    "option",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,	/* freeIntRepProc */
    (Tcl_DupInternalRepProc *) NULL,	/* dupIntRepProc */
    (Tcl_UpdateStringProc *) NULL,	/* updateStringProc */
    SetOptionFromAny			/* setFromAnyProc */
};

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateOptionTable --
 *
 *	Given a template for configuration options, this procedure
 *	creates a table that may be used to look up options efficiently.
 *
 * Results:
 *	Returns a token to a structure that can be passed to procedures
 *	such as Tk_InitOptions, Tk_SetOptions, and Tk_FreeConfigOptions.
 *
 * Side effects:
 *	Storage is allocated.
 *
 *--------------------------------------------------------------
 */

Tk_OptionTable
Tk_CreateOptionTable(interp, templatePtr)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application in which this table
					 * will be used. */
    const Tk_OptionSpec *templatePtr;	/* Static information about the
					 * configuration options. */
{
    Tcl_HashTable *hashTablePtr;
    Tcl_HashEntry *hashEntryPtr;
    int newEntry;
    OptionTable *tablePtr;
    const Tk_OptionSpec *specPtr, *specPtr2;
    Option *optionPtr;
    int numOptions, i;

    /*
     * We use an AssocData value in the interpreter to keep a hash
     * table of all the option tables we've created for this application.
     * This is used for two purposes.  First, it allows us to share the
     * tables (e.g. in several chains) and second, we use the deletion
     * callback for the AssocData to delete all the option tables when
     * the interpreter is deleted.  The code below finds the hash table
     * or creates a new one if it doesn't already exist.
     */

    hashTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, OPTION_HASH_KEY,
	    NULL);
    if (hashTablePtr == NULL) {
	hashTablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(hashTablePtr, TCL_ONE_WORD_KEYS);
	Tcl_SetAssocData(interp, OPTION_HASH_KEY, DestroyOptionHashTable,
		(ClientData) hashTablePtr);
    }

    /*
     * See if a table has already been created for this template.  If
     * so, just reuse the existing table.
     */

    hashEntryPtr = Tcl_CreateHashEntry(hashTablePtr, (char *) templatePtr,
	    &newEntry);
    if (!newEntry) {
	tablePtr = (OptionTable *) Tcl_GetHashValue(hashEntryPtr);
	tablePtr->refCount++;
	return (Tk_OptionTable) tablePtr;
    }

    /*
     * Count the number of options in the template, then create the
     * table structure.
     */

    numOptions = 0;
    for (specPtr = templatePtr; specPtr->type != TK_OPTION_END; specPtr++) {
	numOptions++;
    }
    tablePtr = (OptionTable *) (ckalloc(sizeof(OptionTable)
	    + ((numOptions - 1)  * sizeof(Option))));
    tablePtr->refCount = 1;
    tablePtr->hashEntryPtr = hashEntryPtr;
    tablePtr->nextPtr = NULL;
    tablePtr->numOptions = numOptions;

    /*
     * Initialize all of the Option structures in the table.
     */

    for (specPtr = templatePtr, optionPtr = tablePtr->options;
	    specPtr->type != TK_OPTION_END; specPtr++, optionPtr++) {
	optionPtr->specPtr = specPtr;
	optionPtr->dbNameUID = NULL;
	optionPtr->dbClassUID = NULL;
	optionPtr->defaultPtr = NULL;
	optionPtr->extra.monoColorPtr = NULL;
	optionPtr->flags = 0;

	if (specPtr->type == TK_OPTION_SYNONYM) {
	    /*
	     * This is a synonym option; find the master option that it
	     * refers to and create a pointer from the synonym to the
	     * master.
	     */

	    for (specPtr2 = templatePtr, i = 0; ; specPtr2++, i++) {
		if (specPtr2->type == TK_OPTION_END) {
		    panic("Tk_CreateOptionTable couldn't find synonym");
		}
		if (strcmp(specPtr2->optionName,
			(char *) specPtr->clientData) == 0) {
		    optionPtr->extra.synonymPtr = tablePtr->options + i;
		    break;
		}
	    }
	} else {
	    if (specPtr->dbName != NULL) {
		optionPtr->dbNameUID = Tk_GetUid(specPtr->dbName);
	    }
	    if (specPtr->dbClass != NULL) {
		optionPtr->dbClassUID = 
			Tk_GetUid(specPtr->dbClass);
	    }
	    if (specPtr->defValue != NULL) {
		optionPtr->defaultPtr =
			Tcl_NewStringObj(specPtr->defValue, -1);
		Tcl_IncrRefCount(optionPtr->defaultPtr);
	    }
	    if (((specPtr->type == TK_OPTION_COLOR)
		    || (specPtr->type == TK_OPTION_BORDER))
		    && (specPtr->clientData != NULL)) {
		optionPtr->extra.monoColorPtr =
			Tcl_NewStringObj((char *) specPtr->clientData, -1);
		Tcl_IncrRefCount(optionPtr->extra.monoColorPtr);
	    }
	}
	if (((specPtr->type == TK_OPTION_STRING)
		&& (specPtr->internalOffset >= 0))
		|| (specPtr->type == TK_OPTION_COLOR)
		|| (specPtr->type == TK_OPTION_FONT)
		|| (specPtr->type == TK_OPTION_BITMAP)
		|| (specPtr->type == TK_OPTION_BORDER)
		|| (specPtr->type == TK_OPTION_CURSOR)) {
	    optionPtr->flags |= OPTION_NEEDS_FREEING;
	}
    }
    tablePtr->hashEntryPtr = hashEntryPtr;
    Tcl_SetHashValue(hashEntryPtr, tablePtr);

    /*
     * Finally, check to see if this template chains to another template
     * with additional options.  If so, call ourselves recursively to
     * create the next table(s).
     */

    if (specPtr->clientData != NULL) {
	tablePtr->nextPtr = (OptionTable *) Tk_CreateOptionTable(interp,
		(Tk_OptionSpec *) specPtr->clientData);
    }

    return (Tk_OptionTable) tablePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DeleteOptionTable --
 *
 *	Called to release resources used by an option table when 
 *	the table is no longer needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The option table and associated resources (such as additional
 *	option tables chained off it) are destroyed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_DeleteOptionTable(optionTable)
    Tk_OptionTable optionTable;		/* The option table to delete. */
{
    OptionTable *tablePtr = (OptionTable *) optionTable;
    Option *optionPtr;
    int count;

    tablePtr->refCount--;
    if (tablePtr->refCount > 0) {
	return;
    }

    if (tablePtr->nextPtr != NULL) {
	Tk_DeleteOptionTable((Tk_OptionTable) tablePtr->nextPtr);
    }

    for (count = tablePtr->numOptions - 1, optionPtr = tablePtr->options;
	    count > 0;  count--, optionPtr++) {
	if (optionPtr->defaultPtr != NULL) {
	    Tcl_DecrRefCount(optionPtr->defaultPtr);
	}
	if (((optionPtr->specPtr->type == TK_OPTION_COLOR)
		|| (optionPtr->specPtr->type == TK_OPTION_BORDER))
		&& (optionPtr->extra.monoColorPtr != NULL)) {
	    Tcl_DecrRefCount(optionPtr->extra.monoColorPtr);
	}
    }
    Tcl_DeleteHashEntry(tablePtr->hashEntryPtr);
    ckfree((char *) tablePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyOptionHashTable --
 *
 *	This procedure is the deletion callback associated with the
 *	AssocData entry created by Tk_CreateOptionTable.  It is
 *	invoked when an interpreter is deleted, and deletes all of
 *	the option tables associated with that interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The option hash table is destroyed along with all of the
 *	OptionTable structures that it refers to.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyOptionHashTable(clientData, interp)
    ClientData clientData;	/* The hash table we are destroying */
    Tcl_Interp *interp;		/* The interpreter we are destroying */
{
    Tcl_HashTable *hashTablePtr = (Tcl_HashTable *) clientData;
    Tcl_HashSearch search;
    Tcl_HashEntry *hashEntryPtr;
    OptionTable *tablePtr;

    for (hashEntryPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
	    hashEntryPtr != NULL;
	    hashEntryPtr = Tcl_NextHashEntry(&search)) {
	tablePtr = (OptionTable *) Tcl_GetHashValue(hashEntryPtr);

	/*
	 * The following statements do two tricky things:
	 * 1. They ensure that the option table is deleted, even if
	 *    there are outstanding references to it.
	 * 2. They ensure that Tk_DeleteOptionTable doesn't delete
	 *    other tables chained from this one; we'll do it when
	 *    we come across the hash table entry for the chained
	 *    table (in fact, the chained table may already have
	 *    been deleted).
	 */

	tablePtr->refCount = 1;
	tablePtr->nextPtr = NULL;
	Tk_DeleteOptionTable((Tk_OptionTable) tablePtr);
    }
    Tcl_DeleteHashTable(hashTablePtr);
    ckfree((char *) hashTablePtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_InitOptions --
 *
 *	This procedure is invoked when an object such as a widget
 *	is created.  It supplies an initial value for each configuration
 *	option (the value may come from the option database, a system
 *	default, or the default in the option table).
 *
 * Results:
 *	The return value is TCL_OK if the procedure completed
 *	successfully, and TCL_ERROR if one of the initial values was
 *	bogus.  If an error occurs and interp isn't NULL, then an
 *	error message will be left in its result.
 *
 * Side effects:
 *	Fields of recordPtr are filled in with initial values.
 *
 *--------------------------------------------------------------
 */

int
Tk_InitOptions(interp, recordPtr, optionTable, tkwin)
    Tcl_Interp *interp;		/* Interpreter for error reporting.    NULL
				 * means don't leave an error message. */
    char *recordPtr;		/* Pointer to the record to configure.
				 * Note: the caller should have properly
				 * initialized the record with NULL
				 * pointers for each option value. */
    Tk_OptionTable optionTable;	/* The token which matches the config
				 * specs for the widget in question. */
    Tk_Window tkwin;		/* Certain options types (such as
				 * TK_OPTION_COLOR) need fields out
				 * of the window they are used in to
				 * be able to calculate their values.
				 * Not needed unless one of these
				 * options is in the configSpecs record. */
{
    OptionTable *tablePtr = (OptionTable *) optionTable;
    Option *optionPtr;
    int count;
    char *value;
    Tcl_Obj *valuePtr;
    enum {
	OPTION_DATABASE, SYSTEM_DEFAULT, TABLE_DEFAULT
    } source;

    /*
     * If this table chains to other tables, handle their initialization
     * first.  That way, if both tables refer to the same field of the
     * record, the value in the first table will win.
     */

    if (tablePtr->nextPtr != NULL) {
	if (Tk_InitOptions(interp, recordPtr,
		(Tk_OptionTable) tablePtr->nextPtr, tkwin) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * Iterate over all of the options in the table, initializing each in
     * turn.
     */

    for (optionPtr = tablePtr->options, count = tablePtr->numOptions;
	    count > 0; optionPtr++, count--) {

	/*
	 * If we specify TK_OPTION_DONT_SET_DEFAULT, then the user has
	 * processed and set a default for this already.
	 */
	if ((optionPtr->specPtr->type == TK_OPTION_SYNONYM) ||
		(optionPtr->specPtr->flags & TK_OPTION_DONT_SET_DEFAULT)) {
	    continue;
	}
	source = TABLE_DEFAULT;

	/*
	 * We look in three places for the initial value, using the first
	 * non-NULL value that we find.  First, check the option database.
	 */

	valuePtr = NULL;
	if (optionPtr->dbNameUID != NULL) {
	    value = Tk_GetOption(tkwin, optionPtr->dbNameUID, 
		    optionPtr->dbClassUID);
	    if (value != NULL) {
		valuePtr = Tcl_NewStringObj(value, -1);
		source = OPTION_DATABASE;
	    }
	}

	/*
	 * Second, check for a system-specific default value.
	 */
	if ((valuePtr == NULL)
		&& (optionPtr->dbNameUID != NULL)) {
	    valuePtr = TkpGetSystemDefault(tkwin, optionPtr->dbNameUID,
		    optionPtr->dbClassUID);
	    if (valuePtr != NULL) {
		source = SYSTEM_DEFAULT;
	    }
	}

	/*
	 * Third and last, use the default value supplied by the option
	 * table.  In the case of color objects, we pick one of two
	 * values depending on whether the screen is mono or color.
	 */

	if (valuePtr == NULL) {
	    if ((tkwin != NULL) 
		    && ((optionPtr->specPtr->type == TK_OPTION_COLOR)
		    || (optionPtr->specPtr->type == TK_OPTION_BORDER))
		    && (Tk_Depth(tkwin) <= 1) 
		    && (optionPtr->extra.monoColorPtr != NULL)) {
		valuePtr = optionPtr->extra.monoColorPtr;
	    } else {
		valuePtr = optionPtr->defaultPtr;
	    }
	}

	if (valuePtr == NULL) {
	    continue;
	}

	if (DoObjConfig(interp, recordPtr, optionPtr, valuePtr, tkwin,
		(Tk_SavedOption *) NULL) != TCL_OK) {
	    if (interp != NULL) {
		char msg[200];
    
		switch (source) {
		    case OPTION_DATABASE:
			sprintf(msg, "\n    (database entry for \"%.50s\")",
				optionPtr->specPtr->optionName);
			break;
		    case SYSTEM_DEFAULT:
			sprintf(msg, "\n    (system default for \"%.50s\")",
				optionPtr->specPtr->optionName);
			break;
		    case TABLE_DEFAULT:
			sprintf(msg, "\n    (default value for \"%.50s\")",
				optionPtr->specPtr->optionName);
		}
		if (tkwin != NULL) {
		    sprintf(msg + strlen(msg) - 1, " in widget \"%.50s\")",
			    Tk_PathName(tkwin));
		}
		Tcl_AddErrorInfo(interp, msg);
	    }
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DoObjConfig --
 *
 *	This procedure applies a new value for a configuration option
 *	to the record being configured.
 *
 * Results:
 *	The return value is TCL_OK if the procedure completed
 *	successfully.  If an error occurred then TCL_ERROR is
 *	returned and an error message is left in interp's result, if
 *	interp isn't NULL.  In addition, if oldValuePtrPtr isn't
 *	NULL then it *oldValuePtrPtr is filled in with a pointer
 *	to the option's old value.
 *
 * Side effects:
 *	RecordPtr gets modified to hold the new value in the form of
 *	a Tcl_Obj, an internal representation, or both.  The old
 *	value is freed if oldValuePtrPtr is NULL.
 *
 *--------------------------------------------------------------
 */

static int
DoObjConfig(interp, recordPtr, optionPtr, valuePtr, tkwin, savedOptionPtr)
    Tcl_Interp *interp;		/* Interpreter for error reporting.  If
				 * NULL, then no message is left if an error
				 * occurs. */
    char *recordPtr;		/* The record to modify to hold the new
				 * option value. */
    Option *optionPtr;		/* Pointer to information about the
				 * option. */
    Tcl_Obj *valuePtr;		/* New value for option. */
    Tk_Window tkwin;		/* Window in which option will be used (needed
				 * to allocate resources for some options).
				 * May be NULL if the option doesn't
				 * require window-related resources. */
    Tk_SavedOption *savedOptionPtr;
				/* If NULL, the old value for the option will
				 * be freed. If non-NULL, the old value will
				 * be stored here, and it becomes the property
				 * of the caller (the caller must eventually
				 * free the old value). */
{
    Tcl_Obj **slotPtrPtr, *oldPtr;
    char *internalPtr;		/* Points to location in record where
				 * internal representation of value should
				 * be stored, or NULL. */
    char *oldInternalPtr;	/* Points to location in which to save old
				 * internal representation of value. */
    Tk_SavedOption internal;	/* Used to save the old internal representation
				 * of the value if savedOptionPtr is NULL. */
    const Tk_OptionSpec *specPtr;
    int nullOK;

    /*
     * Save the old object form for the value, if there is one.
     */

    specPtr = optionPtr->specPtr;
    if (specPtr->objOffset >= 0) {
	slotPtrPtr = (Tcl_Obj **) (recordPtr + specPtr->objOffset);
	oldPtr = *slotPtrPtr;
    } else {
	slotPtrPtr = NULL;
	oldPtr = NULL;
    }

    /*
     * Apply the new value in a type-specific way.  Also remember the
     * old object and internal forms, if they exist.
     */

    if (specPtr->internalOffset >= 0) {
	internalPtr = recordPtr + specPtr->internalOffset;
    } else {
	internalPtr = NULL;
    }
    if (savedOptionPtr != NULL) {
	savedOptionPtr->optionPtr = optionPtr;
	savedOptionPtr->valuePtr = oldPtr;
	oldInternalPtr = (char *) &savedOptionPtr->internalForm;
    } else {
	oldInternalPtr = (char *) &internal.internalForm;
    }
    nullOK = (optionPtr->specPtr->flags & TK_OPTION_NULL_OK);
    switch (optionPtr->specPtr->type) {
	case TK_OPTION_BOOLEAN: {
	    int new;

	    if (Tcl_GetBooleanFromObj(interp, valuePtr, &new)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((int *) oldInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_INT: {
	    int new;
	    
	    if (Tcl_GetIntFromObj(interp, valuePtr, &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((int *) oldInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_DOUBLE: {
	    double new;
	    
	    if (Tcl_GetDoubleFromObj(interp, valuePtr, &new) 
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((double *) oldInternalPtr) = *((double *) internalPtr);
		*((double *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_STRING: {
	    char *new, *value;
	    int length;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
	    }
	    if (internalPtr != NULL) {
		if (valuePtr != NULL) {
		    value = Tcl_GetStringFromObj(valuePtr, &length);
		    new = ckalloc((unsigned) (length + 1));
		    strcpy(new, value);
		} else {
		    new = NULL;
		}
		*((char **) oldInternalPtr) = *((char **) internalPtr);
		*((char **) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_STRING_TABLE: {
	    int new;

	    if (Tcl_GetIndexFromObj(interp, valuePtr,
		    (char **) optionPtr->specPtr->clientData,
		    optionPtr->specPtr->optionName+1, 0, &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((int *) oldInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_COLOR: {
	    XColor *newPtr;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
		newPtr = NULL;
	    } else {
		newPtr = Tk_AllocColorFromObj(interp, tkwin, valuePtr);
		if (newPtr == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((XColor **) oldInternalPtr) = *((XColor **) internalPtr);
		*((XColor **) internalPtr) = newPtr;
	    }
	    break;
	}
	case TK_OPTION_FONT: {
	    Tk_Font new;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
		new = NULL;
	    } else {
		new = Tk_AllocFontFromObj(interp, tkwin, valuePtr);
		if (new == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((Tk_Font *) oldInternalPtr) = *((Tk_Font *) internalPtr);
		*((Tk_Font *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_BITMAP: {
	    Pixmap new;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
		new = None;
	    } else {
		new = Tk_AllocBitmapFromObj(interp, tkwin, valuePtr);
		if (new == None) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((Pixmap *) oldInternalPtr) = *((Pixmap *) internalPtr);
		*((Pixmap *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_BORDER: {
	    Tk_3DBorder new;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
		new = NULL;
	    } else {
		new = Tk_Alloc3DBorderFromObj(interp, tkwin, valuePtr);
		if (new == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((Tk_3DBorder *) oldInternalPtr) =
			*((Tk_3DBorder *) internalPtr);
		*((Tk_3DBorder *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_RELIEF: {
	    int new;

	    if (Tk_GetReliefFromObj(interp, valuePtr, &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((int *) oldInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_CURSOR: {
	    Tk_Cursor new;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		new = None;
		valuePtr = NULL;
	    } else {
		new = Tk_AllocCursorFromObj(interp, tkwin, valuePtr);
		if (new == None) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((Tk_Cursor *) oldInternalPtr) = *((Tk_Cursor *) internalPtr);
		*((Tk_Cursor *) internalPtr) = new;
	    }
	    Tk_DefineCursor(tkwin, new);
	    break;
	}
	case TK_OPTION_JUSTIFY: {
	    Tk_Justify new;

	    if (Tk_GetJustifyFromObj(interp, valuePtr, &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((Tk_Justify *) oldInternalPtr)
			= *((Tk_Justify *) internalPtr);
		*((Tk_Justify *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_ANCHOR: {
	    Tk_Anchor new;

	    if (Tk_GetAnchorFromObj(interp, valuePtr, &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((Tk_Anchor *) oldInternalPtr)
			= *((Tk_Anchor *) internalPtr);
		*((Tk_Anchor *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_PIXELS: {
	    int new;
	    
	    if (Tk_GetPixelsFromObj(interp, tkwin, valuePtr,
		    &new) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (internalPtr != NULL) {
		*((int *) oldInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	    }
	    break;
	}
	case TK_OPTION_WINDOW: {
	    Tk_Window new;

	    if (nullOK && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
		new = None;
	    } else {
		if (TkGetWindowFromObj(interp, tkwin, valuePtr, &new)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (internalPtr != NULL) {
		*((Tk_Window *) oldInternalPtr) = *((Tk_Window *) internalPtr);
		*((Tk_Window *) internalPtr) = new;
	    }
	    break;
	}
	default: {
	    char buf[40+TCL_INTEGER_SPACE];
	    sprintf(buf, "bad config table: unknown type %d",
		    optionPtr->specPtr->type);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    return TCL_ERROR;
	}
    }

    /*
     * Release resources associated with the old value, if we're not
     * returning it to the caller, then install the new object value into
     * the record.
     */

    if (savedOptionPtr == NULL) {
	if (optionPtr->flags & OPTION_NEEDS_FREEING) {
	    FreeResources(optionPtr, oldPtr, oldInternalPtr, tkwin);
	}
	if (oldPtr != NULL) {
	    Tcl_DecrRefCount(oldPtr);
	}
    }
    if (slotPtrPtr != NULL) {
	*slotPtrPtr = valuePtr;
	if (valuePtr != NULL) {
	    Tcl_IncrRefCount(valuePtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ObjectIsEmpty --
 *
 *	This procedure tests whether the string value of an object is
 *	empty.
 *
 * Results:
 *	The return value is 1 if the string value of objPtr has length
 *	zero, and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ObjectIsEmpty(objPtr)
    Tcl_Obj *objPtr;		/* Object to test.  May be NULL. */
{
    int length;

    if (objPtr == NULL) {
	return 1;
    }
    if (objPtr->bytes != NULL) {
	return (objPtr->length == 0);
    }
    Tcl_GetStringFromObj(objPtr, &length);
    return (length == 0);
}

/*
 *----------------------------------------------------------------------
 *
 * GetOptionFromObj --
 *
 *	This procedure searches through a chained option table to find
 *	the entry for a particular option name.
 *
 * Results:
 *	The return value is a pointer to the matching entry, or NULL
 *	if no matching entry could be found.  If NULL is returned and
 *	interp is not NULL than an error message is left in its result.
 *	Note: if the matching entry is a synonym then this procedure
 *	returns a pointer to the synonym entry, *not* the "real" entry
 *	that the synonym refers to.
 *
 * Side effects:
 *	Information about the matching entry is cached in the object
 *	containing the name, so that future lookups can proceed more
 *	quickly.
 *
 *----------------------------------------------------------------------
 */

static Option *
GetOptionFromObj(interp, objPtr, tablePtr)
    Tcl_Interp *interp;		/* Used only for error reporting; if NULL
				 * no message is left after an error. */
    Tcl_Obj *objPtr;		/* Object whose string value is to be
				 * looked up in the option table. */
    OptionTable *tablePtr;	/* Table in which to look up objPtr. */
{
    Option *bestPtr, *optionPtr;
    OptionTable *tablePtr2;
    char *p1, *p2, *name;
    int count;

    /*
     * First, check to see if the object already has the answer cached.
     */

    if (objPtr->typePtr == &optionType) {
	if (objPtr->internalRep.twoPtrValue.ptr1 == (VOID *) tablePtr) {
	    return (Option *) objPtr->internalRep.twoPtrValue.ptr2;
	}
    }

    /*
     * The answer isn't cached.  Search through all of the option tables
     * in the chain to find the best match.  Some tricky aspects:
     *
     * 1. We have to accept unique abbreviations.
     * 2. The same name could appear in different tables in the chain.
     *    If this happens, we use the entry from the first table. We
     *    have to be careful to distinguish this case from an ambiguous
     *    abbreviation.
     */

    bestPtr = NULL;
    name = Tcl_GetStringFromObj(objPtr, (int *) NULL);
    for (tablePtr2 = tablePtr; tablePtr2 != NULL;
	    tablePtr2 = tablePtr2->nextPtr) {
	for (optionPtr = tablePtr2->options, count = tablePtr2->numOptions;
		count > 0; optionPtr++, count--) {
	    for (p1 = name, p2 = optionPtr->specPtr->optionName;
		    *p1 == *p2; p1++, p2++) {
		if (*p1 == 0) {
		    /*
		     * This is an exact match.  We're done.
		     */

		    bestPtr = optionPtr;
		    goto done;
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
		    bestPtr = optionPtr;
		} else {
		    if (strcmp(bestPtr->specPtr->optionName,
			    optionPtr->specPtr->optionName) != 0) {
			goto error;
		    }
		}
	    }
	}
    }
    if (bestPtr == NULL) {
	goto error;
    }

    done:
    if ((objPtr->typePtr != NULL)
	    && (objPtr->typePtr->freeIntRepProc != NULL)) {
	objPtr->typePtr->freeIntRepProc(objPtr);
    }
    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) tablePtr;
    objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) bestPtr;
    objPtr->typePtr = &optionType;
    return bestPtr;

    error:
    if (interp != NULL) {
	Tcl_AppendResult(interp, "unknown option \"", name,
		"\"", (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOptionFromAny --
 *
 *	This procedure is called to convert a Tcl object to option
 *	internal form. However, this doesn't make sense (need to have a
 *	table of options in order to do the conversion) so the
 *	procedure always generates an error.
 *
 * Results:
 *	The return value is always TCL_ERROR, and an error message is
 *	left in interp's result if interp isn't NULL. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SetOptionFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	    "can't convert value to option except via GetOptionFromObj API",
	    -1);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_SetOptions --
 *
 *	Process one or more name-value pairs for configuration options
 *	and fill in fields of a record with new values.
 *
 * Results:
 *	If all goes well then TCL_OK is returned and the old values of
 *	any modified objects are saved in *savePtr, if it isn't NULL (the
 *	caller must eventually call Tk_RestoreSavedOptions or
 *	Tk_FreeSavedOptions to free the contents of *savePtr).  In
 *	addition, if maskPtr isn't NULL then *maskPtr is filled in with
 *	the OR of the typeMask bits from all modified options.  If an
 *	error occurs then TCL_ERROR is returned and a message 
 *	is left in interp's result unless interp is NULL; nothing is
 *	saved in *savePtr or *maskPtr in this case.
 *
 * Side effects:
 *	The fields of recordPtr get filled in with object pointers
 *	from objc/objv.  Old information in widgRec's fields gets 
 * 	recycled.  Information may be left at *savePtr.
 *
 *--------------------------------------------------------------
 */

int
Tk_SetOptions(interp, recordPtr, optionTable, objc, objv, tkwin, savePtr,
	maskPtr)
    Tcl_Interp *interp;			/* Interpreter for error reporting.
					 * If NULL, then no error message is
					 * returned.*/
    char *recordPtr;	    		/* The record to configure. */
    Tk_OptionTable optionTable;		/* Describes valid options. */
    int objc;				/* The number of elements in objv. */
    Tcl_Obj *const objv[];		/* Contains one or more name-value
					 * pairs. */
    Tk_Window tkwin;			/* Window associated with the thing
					 * being configured; needed for some
					 * options (such as colors). */
    Tk_SavedOptions *savePtr;		/* If non-NULL, the old values of
					 * modified options are saved here
					 * so that they can be restored
					 * after an error. */
    int *maskPtr;			/* It non-NULL, this word is modified
					 * on a successful return to hold the
					 * bit-wise OR of the typeMask fields
					 * of all options that were modified
					 * by this call.  Used by the caller
					 * to figure out which options
					 * actually changed. */
{
    OptionTable *tablePtr = (OptionTable *) optionTable;
    Option *optionPtr;
    Tk_SavedOptions *lastSavePtr, *newSavePtr;
    int mask;

    if (savePtr != NULL) {
	savePtr->recordPtr = recordPtr;
	savePtr->tkwin = tkwin;
	savePtr->numItems = 0;
	savePtr->nextPtr = NULL;
    }
    lastSavePtr = savePtr;

    /*
     * Scan through all of the arguments, processing those
     * that match entries in the option table.
     */

    mask = 0;
    for ( ; objc > 0; objc -= 2, objv += 2) {
	optionPtr = GetOptionFromObj(interp, objv[0], tablePtr);
	if (optionPtr == NULL) {
	    goto error;
	}
	if (optionPtr->specPtr->type == TK_OPTION_SYNONYM) {
	    optionPtr = optionPtr->extra.synonymPtr;
	}

	if (objc < 2) {
	    if (interp != NULL) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"value for \"", Tcl_GetStringFromObj(*objv, NULL),
			"\" missing", (char *) NULL);
		goto error;
	    }
	}
	if ((savePtr != NULL)
		&& (lastSavePtr->numItems >= TK_NUM_SAVED_OPTIONS)) {
	    /*
	     * We've run out of space for saving old option values.  Allocate
	     * more space.
	     */

	    newSavePtr = (Tk_SavedOptions *) ckalloc(sizeof(
		    Tk_SavedOptions));
	    newSavePtr->recordPtr = recordPtr;
	    newSavePtr->tkwin = tkwin;
	    newSavePtr->numItems = 0;
	    newSavePtr->nextPtr = NULL;
	    lastSavePtr->nextPtr = newSavePtr;
	    lastSavePtr = newSavePtr;
	}
	if (DoObjConfig(interp, recordPtr, optionPtr, objv[1], tkwin,
		(savePtr != NULL) ? &lastSavePtr->items[lastSavePtr->numItems]
		: (Tk_SavedOption *) NULL) != TCL_OK) {
	    char msg[100];

	    sprintf(msg, "\n    (processing \"%.40s\" option)",
		    Tcl_GetStringFromObj(*objv, NULL));
	    Tcl_AddErrorInfo(interp, msg);
	    goto error;
	}
	if (savePtr != NULL) {
	    lastSavePtr->numItems++;
	}
	mask |= optionPtr->specPtr->typeMask;
    }
    if (maskPtr != NULL) {
	*maskPtr = mask;
    }
    return TCL_OK;

    error:
    if (savePtr != NULL) {
	Tk_RestoreSavedOptions(savePtr);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RestoreSavedOptions --
 *
 *	This procedure undoes the effect of a previous call to
 *	Tk_SetOptions by restoring all of the options to their value
 *	before the call to Tk_SetOptions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The configutation record is restored and all the information
 *	stored in savePtr is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_RestoreSavedOptions(savePtr)
    Tk_SavedOptions *savePtr;	/* Holds saved option information; must
				 * have been passed to Tk_SetOptions. */
{
    int i;
    Option *optionPtr;
    Tcl_Obj *newPtr;		/* New object value of option, which we
				 * replace with old value and free.  Taken
				 * from record. */
    char *internalPtr;		/* Points to internal value of option in
				 * record. */
    const Tk_OptionSpec *specPtr;

    /*
     * Be sure to restore the options in the opposite order they were
     * set.  This is important because it's possible that the same
     * option name was used twice in a single call to Tk_SetOptions.
     */

    if (savePtr->nextPtr != NULL) {
	Tk_RestoreSavedOptions(savePtr->nextPtr);
	ckfree((char *) savePtr->nextPtr);
	savePtr->nextPtr = NULL;
    }
    for (i = savePtr->numItems - 1; i >= 0; i--) {
	optionPtr = savePtr->items[i].optionPtr;
	specPtr = optionPtr->specPtr;

	/*
	 * First free the new value of the option, which is currently
	 * in the record.
	 */

	if (specPtr->objOffset >= 0) {
	    newPtr = *((Tcl_Obj **) (savePtr->recordPtr + specPtr->objOffset));
	} else {
	    newPtr = NULL;
	}
	if (specPtr->internalOffset >= 0) {
	    internalPtr = savePtr->recordPtr + specPtr->internalOffset;
	} else {
	    internalPtr = NULL;
	}
	if (optionPtr->flags & OPTION_NEEDS_FREEING) {
	    FreeResources(optionPtr, newPtr, internalPtr, savePtr->tkwin);
	}
	if (newPtr != NULL) {
	    Tcl_DecrRefCount(newPtr);
	}

	/*
	 * Now restore the old value of the option.
	 */

	if (specPtr->objOffset >= 0) {
	    *((Tcl_Obj **) (savePtr->recordPtr + specPtr->objOffset))
		    = savePtr->items[i].valuePtr;
	}
	if (specPtr->internalOffset >= 0) {
	    switch (specPtr->type) {
		case TK_OPTION_BOOLEAN: {
		    *((int *) internalPtr)
			    = *((int *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_INT: {
		    *((int *) internalPtr)
			    = *((int *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_DOUBLE: {
		    *((double *) internalPtr)
			    = *((double *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_STRING: {
		    *((char **) internalPtr)
			    = *((char **) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_STRING_TABLE: {
		    *((int *) internalPtr)
			    = *((int *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_COLOR: {
		    *((XColor **) internalPtr)
			    = *((XColor **) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_FONT: {
		    *((Tk_Font *) internalPtr)
			    = *((Tk_Font *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_BITMAP: {
		    *((Pixmap *) internalPtr)
			    = *((Pixmap *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_BORDER: {
		    *((Tk_3DBorder *) internalPtr)
			    = *((Tk_3DBorder *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_RELIEF: {
		    *((int *) internalPtr)
			    = *((int *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_CURSOR: {
		    *((Tk_Cursor *) internalPtr)
			    = *((Tk_Cursor *) &savePtr->items[i].internalForm);
		    Tk_DefineCursor(savePtr->tkwin,
			    *((Tk_Cursor *) internalPtr));
		    break;
		}
		case TK_OPTION_JUSTIFY: {
		    *((Tk_Justify *) internalPtr)
			    = *((Tk_Justify *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_ANCHOR: {
		    *((Tk_Anchor *) internalPtr)
			    = *((Tk_Anchor *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_PIXELS: {
		    *((int *) internalPtr)
			    = *((int *) &savePtr->items[i].internalForm);
		    break;
		}
		case TK_OPTION_WINDOW: {
		    *((Tk_Window *) internalPtr)
			    = *((Tk_Window *) &savePtr->items[i].internalForm);
		    break;
		}
		default: {
		    panic("bad option type in Tk_RestoreSavedOptions");
		}
	    }
	}
    }
    savePtr->numItems = 0;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_FreeSavedOptions --
 *
 *	Free all of the saved configuration option values from a
 *	previous call to Tk_SetOptions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage and system resources are freed.
 *
 *--------------------------------------------------------------
 */

void
Tk_FreeSavedOptions(savePtr)
    Tk_SavedOptions *savePtr;	/* Contains options saved in a previous
				 * call to Tk_SetOptions. */
{
    int count;
    Tk_SavedOption *savedOptionPtr;

    if (savePtr->nextPtr != NULL) {
	Tk_FreeSavedOptions(savePtr->nextPtr);
	ckfree((char *) savePtr->nextPtr);
    }
    for (count = savePtr->numItems,
	    savedOptionPtr = &savePtr->items[savePtr->numItems-1];
	    count > 0;  count--, savedOptionPtr--) {
	if (savedOptionPtr->optionPtr->flags & OPTION_NEEDS_FREEING) {
	    FreeResources(savedOptionPtr->optionPtr, savedOptionPtr->valuePtr,
		    (char *) &savedOptionPtr->internalForm, savePtr->tkwin);
	}
	if (savedOptionPtr->valuePtr != NULL) {
	    Tcl_DecrRefCount(savedOptionPtr->valuePtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeConfigOptions --
 *
 *	Free all resources associated with configuration options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All of the Tcl_Obj's in recordPtr that are controlled by
 *	configuration options in optionTable are freed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
void
Tk_FreeConfigOptions(recordPtr, optionTable, tkwin)
    char *recordPtr;		/* Record whose fields contain current
				 * values for options. */
    Tk_OptionTable optionTable;	/* Describes legal options. */
    Tk_Window tkwin;		/* Window associated with recordPtr; needed
				 * for freeing some options. */
{
    OptionTable *tablePtr;
    Option *optionPtr;
    int count;
    Tcl_Obj **oldPtrPtr, *oldPtr; 
    char *oldInternalPtr;
    const Tk_OptionSpec *specPtr;

    for (tablePtr = (OptionTable *) optionTable; tablePtr != NULL;
	    tablePtr = tablePtr->nextPtr) {
	for (optionPtr = tablePtr->options, count = tablePtr->numOptions;
		count > 0; optionPtr++, count--) {
	    specPtr = optionPtr->specPtr;
	    if (specPtr->type == TK_OPTION_SYNONYM) {
		continue;
	    }
	    if (specPtr->objOffset >= 0) {
		oldPtrPtr = (Tcl_Obj **) (recordPtr + specPtr->objOffset);
		oldPtr = *oldPtrPtr;
		*oldPtrPtr = NULL;
	    } else {
		oldPtr = NULL;
	    }
	    if (specPtr->internalOffset >= 0) {
		oldInternalPtr = recordPtr + specPtr->internalOffset;
	    } else {
		oldInternalPtr = NULL;
	    }
	    if (optionPtr->flags & OPTION_NEEDS_FREEING) {
		FreeResources(optionPtr, oldPtr, oldInternalPtr, tkwin);
	    }
	    if (oldPtr != NULL) {
		Tcl_DecrRefCount(oldPtr);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeResources --
 *
 *	Free system resources associated with a configuration option,
 *	such as colors or fonts.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any system resources associated with objPtr are released.  However,
 *	objPtr itself is not freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeResources(optionPtr, objPtr, internalPtr, tkwin)
    Option *optionPtr;		/* Description of the configuration option. */
    Tcl_Obj *objPtr;		/* The current value of the option, specified
				 * as an object. */
    char *internalPtr;		/* A pointer to an internal representation for
				 * the option's value, such as an int or
				 * (XColor *).  Only valid if
				 * optionPtr->specPtr->internalOffset >= 0. */
    Tk_Window tkwin;		/* The window in which this option is used. */
{
    int internalFormExists;

    /*
     * If there exists an internal form for the value, use it to free
     * resources (also zero out the internal form).  If there is no
     * internal form, then use the object form.
     */

    internalFormExists = optionPtr->specPtr->internalOffset >= 0;
    switch (optionPtr->specPtr->type) {
	case TK_OPTION_STRING:
	    if (internalFormExists) {
		if (*((char **) internalPtr) != NULL) {
		    ckfree(*((char **) internalPtr));
		    *((char **) internalPtr) = NULL;
		}
	    }
	    break;
	case TK_OPTION_COLOR:
	    if (internalFormExists) {
		if (*((XColor **) internalPtr) != NULL) {
		    Tk_FreeColor(*((XColor **) internalPtr));
		    *((XColor **) internalPtr) = NULL;
		}
	    } else if (objPtr != NULL) {
		Tk_FreeColorFromObj(tkwin, objPtr);
	    }
	    break;
	case TK_OPTION_FONT:
	    if (internalFormExists) {
		Tk_FreeFont(*((Tk_Font *) internalPtr));
		*((Tk_Font *) internalPtr) = NULL;
	    } else if (objPtr != NULL) {
		Tk_FreeFontFromObj(tkwin, objPtr);
	    }
	    break;
	case TK_OPTION_BITMAP:
	    if (internalFormExists) {
		if (*((Pixmap *) internalPtr) != None) {
		    Tk_FreeBitmap(Tk_Display(tkwin), *((Pixmap *) internalPtr));
		    *((Pixmap *) internalPtr) = None;
		}
	    } else if (objPtr != NULL) {
		Tk_FreeBitmapFromObj(tkwin, objPtr);
	    }
	    break;
	case TK_OPTION_BORDER:
	    if (internalFormExists) {
		if (*((Tk_3DBorder *) internalPtr) != NULL) {
		    Tk_Free3DBorder(*((Tk_3DBorder *) internalPtr));
		    *((Tk_3DBorder *) internalPtr) = NULL;
		}
	    } else if (objPtr != NULL) {
		Tk_Free3DBorderFromObj(tkwin, objPtr);
	    }
	    break;
	case TK_OPTION_CURSOR:
	    if (internalFormExists) {
		if (*((Tk_Cursor *) internalPtr) != None) {
		    Tk_FreeCursor(Tk_Display(tkwin),
			    *((Tk_Cursor *) internalPtr));
		    *((Tk_Cursor *) internalPtr) = None;
		}
	    } else if (objPtr != NULL) {
		Tk_FreeCursorFromObj(tkwin, objPtr);
	    }
	    break;
	default:
	    break;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetOptionInfo --
 *
 *	Returns a list object containing complete information about
 *	either a single option or all the configuration options in a
 *	table.
 *
 * Results:
 *	This procedure normally returns a pointer to an object.
 *	If namePtr isn't NULL, then the result object is a list with
 *	five elements: the option's name, its database name, database
 *	class, default value, and current value.  If the option is a
 *	synonym then the list will contain only two values: the option
 *	name and the name of the option it refers to.  If namePtr is
 *	NULL, then information is returned for every option in the
 *	option table: the result will have one sub-list (in the form
 *	described above) for each option in the table.  If an error
 *	occurs (e.g. because namePtr isn't valid) then NULL is returned
 *	and an error message will be left in interp's result unless
 *	interp is NULL.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

Tcl_Obj *
Tk_GetOptionInfo(interp, recordPtr, optionTable, namePtr, tkwin)
    Tcl_Interp *interp;		/* Interpreter for error reporting.  If	
				 * NULL, then no error message is created. */
    char *recordPtr;		/* Record whose fields contain current
				 * values for options. */
    Tk_OptionTable optionTable;	/* Describes all the legal options. */
    Tcl_Obj *namePtr;		/* If non-NULL, the string value selects
				 * a single option whose info is to be
				 * returned.  Otherwise info is returned for
				 * all options in optionTable. */
    Tk_Window tkwin;		/* Window associated with recordPtr; needed
				 * to compute correct default value for some
				 * options. */
{
    Tcl_Obj *resultPtr;
    OptionTable *tablePtr = (OptionTable *) optionTable;
    Option *optionPtr;
    int count;

    /*
     * If information is only wanted for a single configuration
     * spec, then handle that one spec specially.
     */

    if (namePtr != NULL) {
	optionPtr = GetOptionFromObj(interp, namePtr, tablePtr);
	if (optionPtr == NULL) {
	    return (Tcl_Obj *) NULL;
	}
	if (optionPtr->specPtr->type == TK_OPTION_SYNONYM) {
	    optionPtr = optionPtr->extra.synonymPtr;
	}
	return GetConfigList(recordPtr, optionPtr, tkwin);
    }

    /*
     * Loop through all the specs, creating a big list with all
     * their information.
     */

    resultPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (; tablePtr != NULL; tablePtr = tablePtr->nextPtr) {
	for (optionPtr = tablePtr->options, count = tablePtr->numOptions;
		count > 0; optionPtr++, count--) {
	    Tcl_ListObjAppendElement(interp, resultPtr,
		    GetConfigList(recordPtr, optionPtr, tkwin));
	}
    }
    return resultPtr;
}

/*
 *--------------------------------------------------------------
 *
 * GetConfigList --
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
GetConfigList(recordPtr, optionPtr, tkwin)
    char *recordPtr;		/* Pointer to record holding current
				 * values of configuration options. */
    Option *optionPtr;		/* Pointer to information describing a
				 * particular option. */
    Tk_Window tkwin;		/* Window corresponding to recordPtr. */
{
    Tcl_Obj *listPtr, *elementPtr;

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, 
	    Tcl_NewStringObj(optionPtr->specPtr->optionName, -1));

    if (optionPtr->specPtr->type == TK_OPTION_SYNONYM) {
	elementPtr = Tcl_NewStringObj(
		optionPtr->extra.synonymPtr->specPtr->optionName, -1);
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, elementPtr);
    } else {
	if (optionPtr->dbNameUID == NULL) {
	    elementPtr = Tcl_NewObj();
	} else {
	    elementPtr = Tcl_NewStringObj(optionPtr->dbNameUID, -1);
	}
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, elementPtr);

	if (optionPtr->dbClassUID == NULL) {
	    elementPtr = Tcl_NewObj();
	} else {
	    elementPtr = Tcl_NewStringObj(optionPtr->dbClassUID, -1);
	}
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, elementPtr);

	if ((tkwin != NULL) && ((optionPtr->specPtr->type == TK_OPTION_COLOR)
		|| (optionPtr->specPtr->type == TK_OPTION_BORDER))
		&& (Tk_Depth(tkwin) <= 1)
		&& (optionPtr->extra.monoColorPtr != NULL)) {
	    elementPtr = optionPtr->extra.monoColorPtr;
	} else if (optionPtr->defaultPtr != NULL) {
	    elementPtr = optionPtr->defaultPtr;
	} else {
	    elementPtr = Tcl_NewObj();
	}
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, elementPtr);

	if (optionPtr->specPtr->objOffset >= 0) {
	    elementPtr = *((Tcl_Obj **) (recordPtr
		    + optionPtr->specPtr->objOffset));
	    if (elementPtr == NULL) {
		elementPtr = Tcl_NewObj();
	    }
	} else {
	    elementPtr = GetObjectForOption(recordPtr, optionPtr, tkwin);
	}
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, elementPtr);
    }
    return listPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetObjectForOption --
 *
 *	This procedure is called to create an object that contains the
 *	value for an option.  It is invoked by GetConfigList and
 *	Tk_GetOptionValue when only the internal form of an option is
 *	stored in the record.
 *
 * Results:
 *	The return value is a pointer to a Tcl object.  The caller
 *	must call Tcl_IncrRefCount on this object to preserve it.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
GetObjectForOption(recordPtr, optionPtr, tkwin)
    char *recordPtr;		/* Pointer to record holding current
				 * values of configuration options. */
    Option *optionPtr;		/* Pointer to information describing an
				 * option whose internal value is stored
				 * in *recordPtr. */
    Tk_Window tkwin;		/* Window corresponding to recordPtr. */
{
    Tcl_Obj *objPtr;
    char *internalPtr;		/* Points to internal value of option in
				 * record. */

    internalPtr = recordPtr + optionPtr->specPtr->internalOffset;
    objPtr = NULL;
    switch (optionPtr->specPtr->type) {
	case TK_OPTION_BOOLEAN: {
	    objPtr = Tcl_NewIntObj(*((int *) internalPtr));
	    break;
	}
	case TK_OPTION_INT: {
	    objPtr = Tcl_NewIntObj(*((int *) internalPtr));
	    break;
	}
	case TK_OPTION_DOUBLE: {
	    objPtr = Tcl_NewDoubleObj(*((double *) internalPtr));
	    break;
	}
	case TK_OPTION_STRING: {
	    objPtr = Tcl_NewStringObj(*((char **) internalPtr), -1);
	    break;
	}
	case TK_OPTION_STRING_TABLE: {
	    objPtr = Tcl_NewStringObj(
		    ((char **) optionPtr->specPtr->clientData)[
		    *((int *) internalPtr)], -1);
	    break;
	}
	case TK_OPTION_COLOR: { 
	    XColor *colorPtr = *((XColor **) internalPtr);
	    if (colorPtr != NULL) {
		objPtr = Tcl_NewStringObj(Tk_NameOfColor(colorPtr), -1);
	    }
	    break;
	}
	case TK_OPTION_FONT: {
	    Tk_Font tkfont = *((Tk_Font *) internalPtr);
	    if (tkfont != NULL) {
		objPtr = Tcl_NewStringObj(Tk_NameOfFont(tkfont), -1);
	    }
	    break;
	}
	case TK_OPTION_BITMAP: {
	    Pixmap pixmap = *((Pixmap *) internalPtr);
	    if (pixmap != None) {
		objPtr = Tcl_NewStringObj(Tk_NameOfBitmap(Tk_Display(tkwin),
			pixmap), -1);
	    }
	    break;
	}
	case TK_OPTION_BORDER: {
	    Tk_3DBorder border = *((Tk_3DBorder *) internalPtr);
	    if (border != NULL) {
		objPtr = Tcl_NewStringObj(Tk_NameOf3DBorder(border), -1);
	    }
	    break;
	}
	case TK_OPTION_RELIEF: {
	    objPtr = Tcl_NewStringObj(Tk_NameOfRelief(
		    *((int *) internalPtr)), -1);
	    break;
	}
	case TK_OPTION_CURSOR: {
	    Tk_Cursor cursor = *((Tk_Cursor *) internalPtr);
	    if (cursor != None) {
		objPtr = Tcl_NewStringObj(
			Tk_NameOfCursor(Tk_Display(tkwin), cursor), -1);
	    }
	    break;
	}
	case TK_OPTION_JUSTIFY: {
	    objPtr = Tcl_NewStringObj(Tk_NameOfJustify(
		    *((Tk_Justify *) internalPtr)), -1);
	    break;
	}
	case TK_OPTION_ANCHOR: {
	    objPtr = Tcl_NewStringObj(Tk_NameOfAnchor(
		    *((Tk_Anchor *) internalPtr)), -1);
	    break;
	}
	case TK_OPTION_PIXELS: {
	    objPtr = Tcl_NewIntObj(*((int *) internalPtr));
	    break;
	}
	case TK_OPTION_WINDOW: {
	    Tk_Window tkwin = *((Tk_Window *) internalPtr);
	    if (tkwin != NULL) {
		objPtr = Tcl_NewStringObj(Tk_PathName(tkwin), -1);
	    }
	    break;
	}
	default: {
	    panic("bad option type in GetObjectForOption");
	}
    }
    if (objPtr == NULL) {
	objPtr = Tcl_NewObj();
    }
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetOptionValue --
 *
 *	This procedure returns the current value of a configuration
 *	option.
 *
 * Results:
 *	The return value is the object holding the current value of
 *	the option given by namePtr.  If no such option exists, then
 *	the return value is NULL and an error message is left in
 *	interp's result (if interp isn't NULL).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tk_GetOptionValue(interp, recordPtr, optionTable, namePtr, tkwin)
    Tcl_Interp *interp;		/* Interpreter for error reporting.  If
				 * NULL then no messages are provided for
				 * errors. */
    char *recordPtr;		/* Record whose fields contain current
				 * values for options. */
    Tk_OptionTable optionTable;	/* Describes legal options. */
    Tcl_Obj *namePtr;		/* Gives the command-line name for the
				 * option whose value is to be returned. */
    Tk_Window tkwin;		/* Window corresponding to recordPtr. */
{
    OptionTable *tablePtr = (OptionTable *) optionTable;
    Option *optionPtr;
    Tcl_Obj *resultPtr;

    optionPtr = GetOptionFromObj(interp, namePtr, tablePtr);
    if (optionPtr == NULL) {
	return NULL;
    }
    if (optionPtr->specPtr->type == TK_OPTION_SYNONYM) {
	optionPtr = optionPtr->extra.synonymPtr;
    }
    if (optionPtr->specPtr->objOffset >= 0) {
	resultPtr = *((Tcl_Obj **) (recordPtr + optionPtr->specPtr->objOffset));
	if (resultPtr == NULL) {
	    /*
	     * This option has a null value and is represented by a null
	     * object pointer.  We can't return the null pointer, since that
	     * would indicate an error.  Instead, return a new empty object.
	     */
    
	    resultPtr = Tcl_NewObj();
	} 
    } else {
	resultPtr = GetObjectForOption(recordPtr, optionPtr, tkwin);
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkDebugConfig --
 *
 *	This is a debugging procedure that returns information about
 *	one of the configuration tables that currently exists for an
 *	interpreter.
 *
 * Results:
 *	If the specified table exists in the given interpreter, then a
 *	list is returned describing the table and any other tables that
 *	it chains to: for each table there will be three list elements
 *	giving the reference count for the table, the number of elements
 *	in the table, and the command-line name for the first option
 *	in the table.  If the table doesn't exist in the interpreter
 *	then an empty object is returned.  The reference count for the
 *	returned object is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TkDebugConfig(interp, table)
    Tcl_Interp *interp;			/* Interpreter in which the table is
					 * defined. */
    Tk_OptionTable table;		/* Table about which information is to
					 * be returned.  May not necessarily
					 * exist in the interpreter anymore. */
{
    OptionTable *tablePtr = (OptionTable *) table;
    Tcl_HashTable *hashTablePtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_HashSearch search;
    Tcl_Obj *objPtr;

    objPtr = Tcl_NewObj();
    hashTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, OPTION_HASH_KEY,
	    NULL);
    if (hashTablePtr == NULL) {
	return objPtr;
    }

    /*
     * Scan all the tables for this interpreter to make sure that the
     * one we want still is valid.
     */

    for (hashEntryPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
	    hashEntryPtr != NULL;
	    hashEntryPtr = Tcl_NextHashEntry(&search)) {
	if (tablePtr == (OptionTable *) Tcl_GetHashValue(hashEntryPtr)) {
	    for ( ; tablePtr != NULL; tablePtr = tablePtr->nextPtr) {
		Tcl_ListObjAppendElement((Tcl_Interp *) NULL, objPtr,
			Tcl_NewIntObj(tablePtr->refCount));
		Tcl_ListObjAppendElement((Tcl_Interp *) NULL, objPtr,
			Tcl_NewIntObj(tablePtr->numOptions));
		Tcl_ListObjAppendElement((Tcl_Interp *) NULL, objPtr,
			Tcl_NewStringObj(
				tablePtr->options[0].specPtr->optionName,
			-1));
	    }
	    break;
	}
    }
    return objPtr;
}
