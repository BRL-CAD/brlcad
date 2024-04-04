/* 
 * tkTableEdit.c --
 *
 *	This module implements editing functions of a table widget.
 *
 * Copyright (c) 1998-2000 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkTable.h"

static void	TableModifyRC _ANSI_ARGS_((register Table *tablePtr,
			int doRows, int movetag,
			Tcl_HashTable *tagTblPtr, Tcl_HashTable *dimTblPtr,
			int offset, int from, int to, int lo, int hi,
			int outOfBounds));

/* insert/delete subcommands */
static CONST84 char *modCmdNames[] = {
    "active", "cols", "rows", (char *)NULL
};
enum modCmd {
    MOD_ACTIVE, MOD_COLS, MOD_ROWS
};

/* insert/delete row/col switches */
static CONST84 char *rcCmdNames[] = {
    "-keeptitles",	"-holddimensions",	"-holdselection",
    "-holdtags",	"-holdwindows",	"--",
    (char *) NULL
};
enum rcCmd {
    OPT_TITLES,	OPT_DIMS,	OPT_SEL,
    OPT_TAGS,	OPT_WINS,	OPT_LAST
};

#define HOLD_TITLES	1<<0
#define HOLD_DIMS	1<<1
#define HOLD_TAGS	1<<2
#define HOLD_WINS	1<<3
#define HOLD_SEL	1<<4


/*
 *--------------------------------------------------------------
 *
 * Table_EditCmd --
 *	This procedure is invoked to process the insert/delete method
 *	that corresponds to a table widget managed by this module.
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
int
Table_EditCmd(ClientData clientData, register Tcl_Interp *interp,
	      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int doInsert, cmdIndex, first, last;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 2, objv,
			 "option ?switches? arg ?arg?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], modCmdNames,
			    "option", 0, &cmdIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    doInsert = (*(Tcl_GetString(objv[1])) == 'i');
    switch ((enum modCmd) cmdIndex) {
    case MOD_ACTIVE:
	if (doInsert) {
	    /* INSERT */
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "index string");
		return TCL_ERROR;
	    }
	    if (TableGetIcursorObj(tablePtr, objv[3], &first) != TCL_OK) {
		return TCL_ERROR;
	    } else if ((tablePtr->flags & HAS_ACTIVE) &&
		       !(tablePtr->flags & ACTIVE_DISABLED) &&
		       tablePtr->state == STATE_NORMAL) {
		TableInsertChars(tablePtr, first, Tcl_GetString(objv[4]));
	    }
	} else {
	    /* DELETE */
	    if (objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
		return TCL_ERROR;
	    }
	    if (TableGetIcursorObj(tablePtr, objv[3], &first) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		last = first+1;
	    } else if (TableGetIcursorObj(tablePtr, objv[4],
					  &last) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((last >= first) && (tablePtr->flags & HAS_ACTIVE) &&
		!(tablePtr->flags & ACTIVE_DISABLED) &&
		tablePtr->state == STATE_NORMAL) {
		TableDeleteChars(tablePtr, first, last-first);
	    }
	}
	break;	/* EDIT ACTIVE */

    case MOD_COLS:
    case MOD_ROWS: {
	/*
	 * ROW/COL INSERTION/DELETION
	 * FIX: This doesn't handle spans
	 */
	int i, lo, hi, argsLeft, offset, minkeyoff, doRows;
	int maxrow, maxcol, maxkey, minkey, flags, count, *dimPtr;
	Tcl_HashTable *tagTblPtr, *dimTblPtr;
	Tcl_HashSearch search;

	doRows	= (cmdIndex == MOD_ROWS);
	flags	= 0;
	for (i = 3; i < objc; i++) {
	    if (*(Tcl_GetString(objv[i])) != '-') {
		break;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[i], rcCmdNames,
				    "switch", 0, &cmdIndex) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (cmdIndex == OPT_LAST) {
		i++;
		break;
	    }
	    switch (cmdIndex) {
	    case OPT_TITLES:
		flags |= HOLD_TITLES;
		break;
	    case OPT_DIMS:
		flags |= HOLD_DIMS;
		break;
 	    case OPT_SEL:
 		flags |= HOLD_SEL;
 		break;
	    case OPT_TAGS:
		flags |= HOLD_TAGS;
		break;
	    case OPT_WINS:
		flags |= HOLD_WINS;
		break;
	    }
	}
	argsLeft = objc - i;
	if (argsLeft < 1 || argsLeft > 2) {
	    Tcl_WrongNumArgs(interp, 3, objv, "?switches? index ?count?");
	    return TCL_ERROR;
	}

	count	= 1;
	maxcol	= tablePtr->cols-1+tablePtr->colOffset;
	maxrow	= tablePtr->rows-1+tablePtr->rowOffset;
	if (strcmp(Tcl_GetString(objv[i]), "end") == 0) {
	    /* allow "end" to be specified as an index */
	    first = (doRows) ? maxrow : maxcol;
	} else if (Tcl_GetIntFromObj(interp, objv[i], &first) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (argsLeft == 2 &&
	    Tcl_GetIntFromObj(interp, objv[++i], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (count == 0 || (tablePtr->state == STATE_DISABLED)) {
	    return TCL_OK;
	}

	if (doRows) {
	    maxkey	= maxrow;
	    minkey	= tablePtr->rowOffset;
	    minkeyoff	= tablePtr->rowOffset+tablePtr->titleRows;
	    offset	= tablePtr->rowOffset;
	    tagTblPtr	= tablePtr->rowStyles;
	    dimTblPtr	= tablePtr->rowHeights;
	    dimPtr	= &(tablePtr->rows);
	    lo		= tablePtr->colOffset
		+ ((flags & HOLD_TITLES) ? tablePtr->titleCols : 0);
	    hi		= maxcol;
	} else {
	    maxkey	= maxcol;
	    minkey	= tablePtr->colOffset;
	    minkeyoff	= tablePtr->colOffset+tablePtr->titleCols;
	    offset	= tablePtr->colOffset;
	    tagTblPtr	= tablePtr->colStyles;
	    dimTblPtr	= tablePtr->colWidths;
	    dimPtr	= &(tablePtr->cols);
	    lo		= tablePtr->rowOffset
		+ ((flags & HOLD_TITLES) ? tablePtr->titleRows : 0);
	    hi		= maxrow;
	}

	/* constrain the starting index */
	if (first > maxkey) {
	    first = maxkey;
	} else if (first < minkey) {
	    first = minkey;
	}
	if (doInsert) {
	    /* +count means insert after index,
	     * -count means insert before index */
	    if (count < 0) {
		count = -count;
	    } else {
		first++;
	    }
	    if ((flags & HOLD_TITLES) && (first < minkeyoff)) {
		count -= minkeyoff-first;
		if (count <= 0) {
		    return TCL_OK;
		}
		first = minkeyoff;
	    }
	    if (!(flags & HOLD_DIMS)) {
		maxkey += count;
		*dimPtr += count;
	    }
	    /*
	     * We need to call TableAdjustParams before TableModifyRC to
	     * ensure that side effect code like var traces that might get
	     * called will access the correct new dimensions.
	     */
	    if (*dimPtr < 1) {
		*dimPtr = 1;
	    }
	    TableAdjustParams(tablePtr);
	    for (i = maxkey; i >= first; i--) {
		/* move row/col style && width/height here */
		TableModifyRC(tablePtr, doRows, flags, tagTblPtr, dimTblPtr,
			offset, i, i-count, lo, hi, ((i-count) < first));
	    }
	    if (!(flags & HOLD_WINS)) {
		/*
		 * This may be a little severe, but it does unmap the
		 * windows that need to be unmapped, and those that should
		 * stay do remap correctly. [Bug #551325]
		 */
		if (doRows) {
		    EmbWinUnmap(tablePtr,
			    first - tablePtr->rowOffset,
			    maxkey - tablePtr->rowOffset,
			    lo - tablePtr->colOffset,
			    hi - tablePtr->colOffset);
		} else {
		    EmbWinUnmap(tablePtr,
			    lo - tablePtr->rowOffset,
			    hi - tablePtr->rowOffset,
			    first - tablePtr->colOffset,
			    maxkey - tablePtr->colOffset);
		}
	    }
	} else {
	    /* (index = i && count = 1) == (index = i && count = -1) */
	    if (count < 0) {
		/* if the count is negative, make sure that the col count will
		 * delete no greater than the original index */
		if (first+count < minkey) {
		    if (first-minkey < abs(count)) {
			/*
			 * In this case, the user is asking to delete more rows
			 * than exist before the minkey, so we have to shrink
			 * the count down to the existing rows up to index.
			 */
			count = first-minkey;
		    } else {
			count += first-minkey;
		    }
		    first = minkey;
		} else {
		    first += count;
		    count = -count;
		}
	    }
	    if ((flags & HOLD_TITLES) && (first <= minkeyoff)) {
		count -= minkeyoff-first;
		if (count <= 0) {
		    return TCL_OK;
		}
		first = minkeyoff;
	    }
	    if (count > maxkey-first+1) {
		count = maxkey-first+1;
	    }
	    if (!(flags & HOLD_DIMS)) {
		*dimPtr -= count;
	    }
	    /*
	     * We need to call TableAdjustParams before TableModifyRC to
	     * ensure that side effect code like var traces that might get
	     * called will access the correct new dimensions.
	     */
	    if (*dimPtr < 1) {
		*dimPtr = 1;
	    }
	    TableAdjustParams(tablePtr);
	    for (i = first; i <= maxkey; i++) {
		TableModifyRC(tablePtr, doRows, flags, tagTblPtr, dimTblPtr,
			offset, i, i+count, lo, hi, ((i+count) > maxkey));
	    }
	}
	if (!(flags & HOLD_SEL) &&
		Tcl_FirstHashEntry(tablePtr->selCells, &search) != NULL) {
	    /* clear selection - forceful, but effective */
	    Tcl_DeleteHashTable(tablePtr->selCells);
	    Tcl_InitHashTable(tablePtr->selCells, TCL_STRING_KEYS);
	}

	/*
	 * Make sure that the modified dimension is actually legal
	 * after removing all that stuff.
	 */
	if (*dimPtr < 1) {
	    *dimPtr = 1;
	    TableAdjustParams(tablePtr);
	}

	/* change the geometry */
	TableGeometryRequest(tablePtr);
	/* FIX:
	 * This has to handle when the previous rows/cols resize because
	 * of the *stretchmode.  InvalidateAll does that, but could be
	 * more efficient.
	 */
	TableInvalidateAll(tablePtr, 0);
	break;
    }

    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TableDeleteChars --
 *	Remove one or more characters from an table widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, the table gets modified and (eventually)
 *	redisplayed.
 *
 *----------------------------------------------------------------------
 */
void
TableDeleteChars(tablePtr, index, count)
    register Table *tablePtr;	/* Table widget to modify. */
    int index;			/* Index of first character to delete. */
    int count;			/* How many characters to delete. */
{
#ifdef TCL_UTF_MAX
    int byteIndex, byteCount, newByteCount, numBytes, numChars;
    char *new, *string;

    string = tablePtr->activeBuf;
    numBytes = strlen(string);
    numChars = Tcl_NumUtfChars(string, numBytes);
    if ((index + count) > numChars) {
	count = numChars - index;
    }
    if (count <= 0) {
	return;
    }

    byteIndex = Tcl_UtfAtIndex(string, index) - string;
    byteCount = Tcl_UtfAtIndex(string + byteIndex, count)
	- (string + byteIndex);

    newByteCount = numBytes + 1 - byteCount;
    new = (char *) ckalloc((unsigned) newByteCount);
    memcpy(new, string, (size_t) byteIndex);
    strcpy(new + byteIndex, string + byteIndex + byteCount);
#else
    int oldlen;
    char *new;

    /* this gets the length of the string, as well as ensuring that
     * the cursor isn't beyond the end char */
    TableGetIcursor(tablePtr, "end", &oldlen);

    if ((index+count) > oldlen)
	count = oldlen-index;
    if (count <= 0)
	return;

    new = (char *) ckalloc((unsigned)(oldlen-count+1));
    strncpy(new, tablePtr->activeBuf, (size_t) index);
    strcpy(new+index, tablePtr->activeBuf+index+count);
    /* make sure this string is null terminated */
    new[oldlen-count] = '\0';
#endif
    /* This prevents deletes on BREAK or validation error. */
    if (tablePtr->validate &&
	TableValidateChange(tablePtr, tablePtr->activeRow+tablePtr->rowOffset,
			    tablePtr->activeCol+tablePtr->colOffset,
			    tablePtr->activeBuf, new, index) != TCL_OK) {
	ckfree(new);
	return;
    }

    ckfree(tablePtr->activeBuf);
    tablePtr->activeBuf = new;

    /* mark the text as changed */
    tablePtr->flags |= TEXT_CHANGED;

    if (tablePtr->icursor >= index) {
	if (tablePtr->icursor >= (index+count)) {
	    tablePtr->icursor -= count;
	} else {
	    tablePtr->icursor = index;
	}
    }

    TableSetActiveIndex(tablePtr);

    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol, CELL);
}

/*
 *----------------------------------------------------------------------
 *
 * TableInsertChars --
 *	Add new characters to the active cell of a table widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New information gets added to tablePtr; it will be redisplayed
 *	soon, but not necessarily immediately.
 *
 *----------------------------------------------------------------------
 */
void
TableInsertChars(tablePtr, index, value)
    register Table *tablePtr;	/* Table that is to get the new elements. */
    int index;			/* Add the new elements before this element. */
    char *value;		/* New characters to add (NULL-terminated
				 * string). */
{
#ifdef TCL_UTF_MAX
    int oldlen, byteIndex, byteCount;
    char *new, *string;

    byteCount = strlen(value);
    if (byteCount == 0) {
	return;
    }

    /* Is this an autoclear and this is the first update */
    /* Note that this clears without validating */
    if (tablePtr->autoClear && !(tablePtr->flags & TEXT_CHANGED)) {
	/* set the buffer to be empty */
	tablePtr->activeBuf = (char *)ckrealloc(tablePtr->activeBuf, 1);
	tablePtr->activeBuf[0] = '\0';
	/* the insert position now has to be 0 */
	index = 0;
	tablePtr->icursor = 0;
    }

    string = tablePtr->activeBuf;
    byteIndex = Tcl_UtfAtIndex(string, index) - string;

    oldlen = strlen(string);
    new = (char *) ckalloc((unsigned)(oldlen + byteCount + 1));
    memcpy(new, string, (size_t) byteIndex);
    strcpy(new + byteIndex, value);
    strcpy(new + byteIndex + byteCount, string + byteIndex);

    /* validate potential new active buffer */
    /* This prevents inserts on either BREAK or validation error. */
    if (tablePtr->validate &&
	TableValidateChange(tablePtr, tablePtr->activeRow+tablePtr->rowOffset,
			    tablePtr->activeCol+tablePtr->colOffset,
			    tablePtr->activeBuf, new, byteIndex) != TCL_OK) {
	ckfree(new);
	return;
    }

    /*
     * The following construction is used because inserting improperly
     * formed UTF-8 sequences between other improperly formed UTF-8
     * sequences could result in actually forming valid UTF-8 sequences;
     * the number of characters added may not be Tcl_NumUtfChars(string, -1),
     * because of context.  The actual number of characters added is how
     * many characters were are in the string now minus the number that
     * used to be there.
     */

    if (tablePtr->icursor >= index) {
	tablePtr->icursor += Tcl_NumUtfChars(new, oldlen+byteCount)
	    - Tcl_NumUtfChars(tablePtr->activeBuf, oldlen);
    }

    ckfree(string);
    tablePtr->activeBuf = new;

#else
    int oldlen, newlen;
    char *new;

    newlen = strlen(value);
    if (newlen == 0) return;

    /* Is this an autoclear and this is the first update */
    /* Note that this clears without validating */
    if (tablePtr->autoClear && !(tablePtr->flags & TEXT_CHANGED)) {
	/* set the buffer to be empty */
	tablePtr->activeBuf = (char *)ckrealloc(tablePtr->activeBuf, 1);
	tablePtr->activeBuf[0] = '\0';
	/* the insert position now has to be 0 */
	index = 0;
    }
    oldlen = strlen(tablePtr->activeBuf);
    /* get the buffer to at least the right length */
    new = (char *) ckalloc((unsigned)(oldlen+newlen+1));
    strncpy(new, tablePtr->activeBuf, (size_t) index);
    strcpy(new+index, value);
    strcpy(new+index+newlen, (tablePtr->activeBuf)+index);
    /* make sure this string is null terminated */
    new[oldlen+newlen] = '\0';

    /* validate potential new active buffer */
    /* This prevents inserts on either BREAK or validation error. */
    if (tablePtr->validate &&
	TableValidateChange(tablePtr, tablePtr->activeRow+tablePtr->rowOffset,
			    tablePtr->activeCol+tablePtr->colOffset,
			    tablePtr->activeBuf, new, index) != TCL_OK) {
	ckfree(new);
	return;
    }
    ckfree(tablePtr->activeBuf);
    tablePtr->activeBuf = new;

    if (tablePtr->icursor >= index) {
	tablePtr->icursor += newlen;
    }
#endif

    /* mark the text as changed */
    tablePtr->flags |= TEXT_CHANGED;

    TableSetActiveIndex(tablePtr);

    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol, CELL);
}

/*
 *----------------------------------------------------------------------
 *
 * TableModifyRC --
 *	Helper function that does the core work of moving rows/cols
 *	and associated tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Moves cell data and possibly tag data
 *
 *----------------------------------------------------------------------
 */
static void
TableModifyRC(tablePtr, doRows, flags, tagTblPtr, dimTblPtr,
	      offset, from, to, lo, hi, outOfBounds)
    Table *tablePtr;	/* Information about text widget. */
    int doRows;		/* rows (1) or cols (0) */
    int flags;		/* flags indicating what to move */
    Tcl_HashTable *tagTblPtr, *dimTblPtr; /* Pointers to the row/col tags
					   * and width/height tags */
    int offset;		/* appropriate offset */
    int from, to;	/* the from and to row/col */
    int lo, hi;		/* the lo and hi col/row */
    int outOfBounds;	/* the boundary check for shifting items */
{
    int j, new;
    char buf[INDEX_BUFSIZE], buf1[INDEX_BUFSIZE];
    Tcl_HashEntry *entryPtr, *newPtr;
    TableEmbWindow *ewPtr;

    /*
     * move row/col style && width/height here
     * If -holdtags is specified, we don't move the user-set widths/heights
     * of the absolute rows/columns, otherwise we enter here to move the
     * dimensions appropriately
     */
    if (!(flags & HOLD_TAGS)) {
	entryPtr = Tcl_FindHashEntry(tagTblPtr, (char *)from);
	if (entryPtr != NULL) {
	    Tcl_DeleteHashEntry(entryPtr);
	}
	entryPtr = Tcl_FindHashEntry(dimTblPtr, (char *)from-offset);
	if (entryPtr != NULL) {
	    Tcl_DeleteHashEntry(entryPtr);
	}
	if (!outOfBounds) {
	    entryPtr = Tcl_FindHashEntry(tagTblPtr, (char *)to);
	    if (entryPtr != NULL) {
		newPtr = Tcl_CreateHashEntry(tagTblPtr, (char *)from, &new);
		Tcl_SetHashValue(newPtr, Tcl_GetHashValue(entryPtr));
		Tcl_DeleteHashEntry(entryPtr);
	    }
	    entryPtr = Tcl_FindHashEntry(dimTblPtr, (char *)to-offset);
	    if (entryPtr != NULL) {
		newPtr = Tcl_CreateHashEntry(dimTblPtr, (char *)from-offset,
			&new);
		Tcl_SetHashValue(newPtr, Tcl_GetHashValue(entryPtr));
		Tcl_DeleteHashEntry(entryPtr);
	    }
	}
    }
    for (j = lo; j <= hi; j++) {
	if (doRows /* rows */) {
	    TableMakeArrayIndex(from, j, buf);
	    TableMakeArrayIndex(to, j, buf1);
	    TableMoveCellValue(tablePtr, to, j, buf1, from, j, buf,
		    outOfBounds);
	} else {
	    TableMakeArrayIndex(j, from, buf);
	    TableMakeArrayIndex(j, to, buf1);
	    TableMoveCellValue(tablePtr, j, to, buf1, j, from, buf,
		    outOfBounds);
	}
	/*
	 * If -holdselection is specified, we leave the selected cells in the
	 * absolute cell values, otherwise we enter here to move the
	 * selection appropriately
	 */
	if (!(flags & HOLD_SEL)) {
	    entryPtr = Tcl_FindHashEntry(tablePtr->selCells, buf);
	    if (entryPtr != NULL) {
		Tcl_DeleteHashEntry(entryPtr);
	    }
	    if (!outOfBounds) {
		entryPtr = Tcl_FindHashEntry(tablePtr->selCells, buf1);
		if (entryPtr != NULL) {
		    Tcl_CreateHashEntry(tablePtr->selCells, buf, &new);
		    Tcl_DeleteHashEntry(entryPtr);
		}
	    }
	}
	/*
	 * If -holdtags is specified, we leave the tags in the
	 * absolute cell values, otherwise we enter here to move the
	 * tags appropriately
	 */
	if (!(flags & HOLD_TAGS)) {
	    entryPtr = Tcl_FindHashEntry(tablePtr->cellStyles, buf);
	    if (entryPtr != NULL) {
		Tcl_DeleteHashEntry(entryPtr);
	    }
	    if (!outOfBounds) {
		entryPtr = Tcl_FindHashEntry(tablePtr->cellStyles, buf1);
		if (entryPtr != NULL) {
		    newPtr = Tcl_CreateHashEntry(tablePtr->cellStyles, buf,
			    &new);
		    Tcl_SetHashValue(newPtr, Tcl_GetHashValue(entryPtr));
		    Tcl_DeleteHashEntry(entryPtr);
		}
	    }
	}
	/*
	 * If -holdwindows is specified, we leave the windows in the
	 * absolute cell values, otherwise we enter here to move the
	 * windows appropriately
	 */
	if (!(flags & HOLD_WINS)) {
	    /*
	     * Delete whatever window might be in our destination
	     */
	    Table_WinDelete(tablePtr, buf);
	    if (!outOfBounds) {
		/*
		 * buf1 is where the window is
		 * buf is where we want it to be
		 *
		 * This is an adaptation of Table_WinMove, which we can't
		 * use because we are intermediately fiddling with boundaries
		 */
		entryPtr = Tcl_FindHashEntry(tablePtr->winTable, buf1);
		if (entryPtr != NULL) {
		    /*
		     * If there was a window in our source,
		     * get the window pointer to move it
		     */
		    ewPtr = (TableEmbWindow *) Tcl_GetHashValue(entryPtr);
		    /* and free the old hash table entry */
		    Tcl_DeleteHashEntry(entryPtr);

		    entryPtr = Tcl_CreateHashEntry(tablePtr->winTable, buf,
			    &new);
		    /*
		     * We needn't check if a window was in buf, since the
		     * Table_WinDelete above should guarantee that no window
		     * is there.  Just set the new entry's value.
		     */
		    Tcl_SetHashValue(entryPtr, (ClientData) ewPtr);
		    ewPtr->hPtr = entryPtr;
		}
	    }
	}
    }
}
