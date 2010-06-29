/* 
 * tkTableUtil.c --
 *
 *	This module contains utility functions for table widgets.
 *
 * Copyright (c) 2000-2002 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkTable.h"

static char *	Cmd_GetName _ANSI_ARGS_((const Cmd_Struct *cmds, int val));
static int	Cmd_GetValue _ANSI_ARGS_((const Cmd_Struct *cmds,
			const char *arg));
static void	Cmd_GetError _ANSI_ARGS_((Tcl_Interp *interp,
			const Cmd_Struct *cmds, const char *arg));

/*
 *--------------------------------------------------------------
 *
 * Table_ClearHashTable --
 *	This procedure is invoked to clear a STRING_KEY hash table,
 *	freeing the string entries and then deleting the hash table.
 *	The hash table cannot be used after calling this, except to
 *	be freed or reinitialized.
 *
 * Results:
 *	Cached info will be lost.
 *
 * Side effects:
 *	Can cause redraw.
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
void
Table_ClearHashTable(Tcl_HashTable *hashTblPtr)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    char *value;

    for (entryPtr = Tcl_FirstHashEntry(hashTblPtr, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	value = (char *) Tcl_GetHashValue(entryPtr);
	if (value != NULL) ckfree(value);
    }

    Tcl_DeleteHashTable(hashTblPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TableOptionBdSet --
 *
 *	This routine configures the borderwidth value for a tag.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	It may adjust the tag struct values of bd[0..4] and borders.
 *
 *----------------------------------------------------------------------
 */

int
TableOptionBdSet(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Type of struct being set. */
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing table widget. */
    CONST84 char *value;		/* Value of option. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
{
    char **borderStr;
    int *bordersPtr, *bdPtr;
    int type	= (int) clientData;
    int result	= TCL_OK;
    int argc;
    CONST84 char **argv;

    if ((type == BD_TABLE) && (value[0] == '\0')) {
	/*
	 * NULL strings aren't allowed for the table global -bd
	 */
	Tcl_AppendResult(interp, "borderwidth value may not be empty",
		(char *) NULL);
	return TCL_ERROR;
    }

    if ((type == BD_TABLE) || (type == BD_TABLE_TAG)) {
	TableTag *tagPtr = (TableTag *) (widgRec + offset);
	borderStr	= &(tagPtr->borderStr);
	bordersPtr	= &(tagPtr->borders);
	bdPtr		= tagPtr->bd;
    } else if (type == BD_TABLE_WIN) {
	TableEmbWindow *tagPtr = (TableEmbWindow *) widgRec;
	borderStr	= &(tagPtr->borderStr);
	bordersPtr	= &(tagPtr->borders);
	bdPtr		= tagPtr->bd;
    } else {
	panic("invalid type given to TableOptionBdSet\n");
	return TCL_ERROR; /* lint */
    }

    result = Tcl_SplitList(interp, value, &argc, &argv);
    if (result == TCL_OK) {
	int i, bd[4];

	if (((type == BD_TABLE) && (argc == 0)) || (argc == 3) || (argc > 4)) {
	    Tcl_AppendResult(interp,
		    "1, 2 or 4 values must be specified for borderwidth",
		    (char *) NULL);
	    result = TCL_ERROR;
	} else {
	    /*
	     * We use the shadow bd array first, in case we have an error
	     * parsing arguments half way through.
	     */
	    for (i = 0; i < argc; i++) {
		if (Tk_GetPixels(interp, tkwin, argv[i], &(bd[i])) != TCL_OK) {
		    result = TCL_ERROR;
		    break;
		}
	    }
	    /*
	     * If everything is OK, store the parsed and given values for
	     * easy retrieval.
	     */
	    if (result == TCL_OK) {
		for (i = 0; i < argc; i++) {
		    bdPtr[i] = MAX(0, bd[i]);
		}
		if (*borderStr) {
		    ckfree(*borderStr);
		}
		if (value) {
		    *borderStr	= (char *) ckalloc(strlen(value) + 1);
		    strcpy(*borderStr, value);
		} else {
		    *borderStr	= NULL;
		}
		*bordersPtr	= argc;
	    }
	}
	ckfree ((char *) argv);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TableOptionBdGet --
 *
 * Results:
 *	Value of the -bd option.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TableOptionBdGet(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Type of struct being set. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register int type	= (int) clientData;

    if (type == BD_TABLE) {
	return ((TableTag *) (widgRec + offset))->borderStr;
    } else if (type == BD_TABLE_TAG) {
	return ((TableTag *) widgRec)->borderStr;
    } else if (type == BD_TABLE_WIN) {
	return ((TableEmbWindow *) widgRec)->borderStr;
    } else {
	panic("invalid type given to TableOptionBdSet\n");
	return NULL; /* lint */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableTagConfigureBd --
 *	This routine configures the border values based on a tag.
 *	The previous value of the bd string (oldValue) is assumed to
 *	be a valid value for this tag.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	It may adjust the value used by -bd.
 *
 *----------------------------------------------------------------------
 */

int
TableTagConfigureBd(Table *tablePtr, TableTag *tagPtr,
	char *oldValue, int nullOK)
{
    int i, argc, result = TCL_OK;
    CONST84 char **argv;

    /*
     * First check to see if the value really changed.
     */
    if (strcmp(tagPtr->borderStr ? tagPtr->borderStr : "",
	    oldValue ? oldValue : "") == 0) {
	return TCL_OK;
    }

    tagPtr->borders = 0;
    if (!nullOK && ((tagPtr->borderStr == NULL)
	    || (*(tagPtr->borderStr) == '\0'))) {
	/*
	 * NULL strings aren't allowed for this tag
	 */
	result = TCL_ERROR;
    } else if (tagPtr->borderStr) {
	result = Tcl_SplitList(tablePtr->interp, tagPtr->borderStr,
		&argc, &argv);
	if (result == TCL_OK) {
	    if ((!nullOK && (argc == 0)) || (argc == 3) || (argc > 4)) {
		Tcl_SetResult(tablePtr->interp,
			"1, 2 or 4 values must be specified to -borderwidth",
			TCL_STATIC);
		result = TCL_ERROR;
	    } else {
		for (i = 0; i < argc; i++) {
		    if (Tk_GetPixels(tablePtr->interp, tablePtr->tkwin,
			    argv[i], &(tagPtr->bd[i])) != TCL_OK) {
			result = TCL_ERROR;
			break;
		    }
		    tagPtr->bd[i] = MAX(0, tagPtr->bd[i]);
		}
		tagPtr->borders = argc;
	    }
	    ckfree ((char *) argv);
	}
    }

    if (result != TCL_OK) {
	if (tagPtr->borderStr) {
	    ckfree ((char *) tagPtr->borderStr);
	}
	if (oldValue != NULL) {
	    size_t length = strlen(oldValue) + 1;
	    /*
	     * We are making the assumption that oldValue is correct.
	     * We have to reparse in case the bad new value had a couple
	     * of correct args before failing on a bad pixel value.
	     */
	    Tcl_SplitList(tablePtr->interp, oldValue, &argc, &argv);
	    for (i = 0; i < argc; i++) {
		Tk_GetPixels(tablePtr->interp, tablePtr->tkwin,
			argv[i], &(tagPtr->bd[i]));
	    }
	    ckfree ((char *) argv);
	    tagPtr->borders	= argc;
	    tagPtr->borderStr	= (char *) ckalloc(length);
	    memcpy(tagPtr->borderStr, oldValue, length);
	} else {
	    tagPtr->borders	= 0;
	    tagPtr->borderStr	= (char *) NULL;
	}
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Cmd_OptionSet --
 *
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
Cmd_OptionSet(ClientData clientData, Tcl_Interp *interp,
	Tk_Window unused, CONST84 char *value, char *widgRec, int offset)
{
  Cmd_Struct *p = (Cmd_Struct *)clientData;
  int mode = Cmd_GetValue(p,value);
  if (!mode) {
    Cmd_GetError(interp,p,value);
    return TCL_ERROR;
  }
  *((int*)(widgRec+offset)) = mode;
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Cmd_OptionGet --
 *
 *
 * Results:
 *	Value of the option.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Cmd_OptionGet(ClientData clientData, Tk_Window unused,
	      char *widgRec, int offset, Tcl_FreeProc **freeProcPtr)
{
  Cmd_Struct *p = (Cmd_Struct *)clientData;
  int mode = *((int*)(widgRec+offset));
  return Cmd_GetName(p,mode);
}

/*
 * simple Cmd_Struct lookup functions
 */

char *
Cmd_GetName(const Cmd_Struct *cmds, int val)
{
  for(;cmds->name && cmds->name[0];cmds++) {
    if (cmds->value==val) return cmds->name;
  }
  return NULL;
}

int
Cmd_GetValue(const Cmd_Struct *cmds, const char *arg)
{
  unsigned int len = strlen(arg);
  for(;cmds->name && cmds->name[0];cmds++) {
    if (!strncmp(cmds->name, arg, len)) return cmds->value;
  }
  return 0;
}

void
Cmd_GetError(Tcl_Interp *interp, const Cmd_Struct *cmds, const char *arg)
{
  int i;
  Tcl_AppendResult(interp, "bad option \"", arg, "\" must be ", (char *) 0);
  for(i=0;cmds->name && cmds->name[0];cmds++,i++) {
    Tcl_AppendResult(interp, (i?", ":""), cmds->name, (char *) 0);
  }
}
