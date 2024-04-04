/* 
 * tkTableCmds.c --
 *
 *	This module implements general commands of a table widget,
 *	based on the major/minor command structure.
 *
 * Copyright (c) 1998-2002 Jeffrey Hobbs
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tkTable.h"

/*
 *--------------------------------------------------------------
 *
 * Table_ActivateCmd --
 *	This procedure is invoked to process the activate method
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
Table_ActivateCmd(ClientData clientData, register Tcl_Interp *interp,
	      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int result = TCL_OK;
    int row, col, templen;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "index");
	return TCL_ERROR;
    } else if (Tcl_GetStringFromObj(objv[2], &templen), templen == 0) {
	/*
	 * Test implementation to clear active cell (becroft)
	 */
	tablePtr->flags &= ~HAS_ACTIVE;
	tablePtr->flags |= ACTIVE_DISABLED;
	tablePtr->activeRow = -1;
	tablePtr->activeCol = -1;
	TableAdjustActive(tablePtr);
	TableConfigCursor(tablePtr);
    } else if (TableGetIndexObj(tablePtr, objv[2], &row, &col) != TCL_OK) {
	return TCL_ERROR;
    } else {
	int x, y, w, dummy;
	char buf1[INDEX_BUFSIZE], buf2[INDEX_BUFSIZE];

	/* convert to valid active index in real coords */
	row -= tablePtr->rowOffset;
	col -= tablePtr->colOffset;
	/* we do this regardless, to avoid cell commit problems */
	if ((tablePtr->flags & HAS_ACTIVE) &&
	    (tablePtr->flags & TEXT_CHANGED)) {
	    tablePtr->flags &= ~TEXT_CHANGED;
	    TableSetCellValue(tablePtr,
			      tablePtr->activeRow+tablePtr->rowOffset,
			      tablePtr->activeCol+tablePtr->colOffset,
			      tablePtr->activeBuf);
	}
	if (row != tablePtr->activeRow || col != tablePtr->activeCol) {
	    if (tablePtr->flags & HAS_ACTIVE) {
		TableMakeArrayIndex(tablePtr->activeRow+tablePtr->rowOffset,
				    tablePtr->activeCol+tablePtr->colOffset,
				    buf1);
	    } else {
		buf1[0] = '\0';
	    }
	    tablePtr->flags |= HAS_ACTIVE;
	    tablePtr->flags &= ~ACTIVE_DISABLED;
	    tablePtr->activeRow = row;
	    tablePtr->activeCol = col;
	    if (tablePtr->activeTagPtr != NULL) {
		ckfree((char *) (tablePtr->activeTagPtr));
		tablePtr->activeTagPtr = NULL;
	    }
	    TableAdjustActive(tablePtr);
	    TableConfigCursor(tablePtr);
	    if (!(tablePtr->flags & BROWSE_CMD) &&
		tablePtr->browseCmd != NULL) {
		Tcl_DString script;
		tablePtr->flags |= BROWSE_CMD;
		row = tablePtr->activeRow+tablePtr->rowOffset;
		col = tablePtr->activeCol+tablePtr->colOffset;
		TableMakeArrayIndex(row, col, buf2);
		Tcl_DStringInit(&script);
		ExpandPercents(tablePtr, tablePtr->browseCmd, row, col,
			       buf1, buf2, tablePtr->icursor, &script, 0);
		result = Tcl_GlobalEval(interp, Tcl_DStringValue(&script));
		if (result == TCL_OK || result == TCL_RETURN) {
		    Tcl_ResetResult(interp);
		}
		Tcl_DStringFree(&script);
		tablePtr->flags &= ~BROWSE_CMD;
	    }
	} else {
	    char *p = Tcl_GetString(objv[2]);

	    if ((tablePtr->activeTagPtr != NULL) && *p == '@' &&
		!(tablePtr->flags & ACTIVE_DISABLED) &&
		TableCellVCoords(tablePtr, row, col, &x, &y, &w, &dummy, 0)) {
		/* we are clicking into the same cell
		 * If it was activated with @x,y indexing,
		 * find the closest char */
		Tk_TextLayout textLayout;
		TableTag *tagPtr = tablePtr->activeTagPtr;

		/* no error checking because GetIndex did it for us */
		p++;
		x = strtol(p, &p, 0) - x - tablePtr->activeX;
		p++;
		y = strtol(p, &p, 0) - y - tablePtr->activeY;

		textLayout = Tk_ComputeTextLayout(tagPtr->tkfont,
					tablePtr->activeBuf, -1,
					(tagPtr->wrap) ? w : 0,
					tagPtr->justify, 0, &dummy, &dummy);

		tablePtr->icursor = Tk_PointToChar(textLayout, x, y);
		Tk_FreeTextLayout(textLayout);
		TableRefresh(tablePtr, row, col, CELL|INV_FORCE);
	    }
	}
	tablePtr->flags |= HAS_ACTIVE;
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Table_AdjustCmd --
 *	This procedure is invoked to process the width/height method
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
Table_AdjustCmd(ClientData clientData, register Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_HashTable *hashTablePtr;
    int i, widthType, dummy, value, posn, offset;
    char buf1[INDEX_BUFSIZE];

    widthType = (*(Tcl_GetString(objv[1])) == 'w');
    /* changes the width/height of certain selected columns */
    if (objc != 3 && (objc & 1)) {
	Tcl_WrongNumArgs(interp, 2, objv, widthType ?
			 "?col? ?width col width ...?" :
			 "?row? ?height row height ...?");
	return TCL_ERROR;
    }
    if (widthType) {
	hashTablePtr = tablePtr->colWidths;
	offset = tablePtr->colOffset;
    } else { 
	hashTablePtr = tablePtr->rowHeights;
	offset = tablePtr->rowOffset;
    }

    if (objc == 2) {
	/* print out all the preset column widths or row heights */
	entryPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
	while (entryPtr != NULL) {
	    posn = ((int) Tcl_GetHashKey(hashTablePtr, entryPtr)) + offset;
	    value = (int) Tcl_GetHashValue(entryPtr);
	    sprintf(buf1, "%d %d", posn, value);
	    /* OBJECTIFY */
	    Tcl_AppendElement(interp, buf1);
	    entryPtr = Tcl_NextHashEntry(&search);
	}
    } else if (objc == 3) {
	/* get the width/height of a particular row/col */
	if (Tcl_GetIntFromObj(interp, objv[2], &posn) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* no range check is done, why bother? */
	posn -= offset;
	entryPtr = Tcl_FindHashEntry(hashTablePtr, (char *) posn);
	if (entryPtr != NULL) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp),
			  (int) Tcl_GetHashValue(entryPtr));
	} else {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), widthType ?
			  tablePtr->defColWidth : tablePtr->defRowHeight);
	}
    } else {
	for (i=2; i<objc; i++) {
	    /* set new width|height here */
	    value = -999999;
	    if (Tcl_GetIntFromObj(interp, objv[i++], &posn) != TCL_OK ||
		(strcmp(Tcl_GetString(objv[i]), "default") &&
		 Tcl_GetIntFromObj(interp, objv[i], &value) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    posn -= offset;
	    if (value == -999999) {
		/* reset that field */
		entryPtr = Tcl_FindHashEntry(hashTablePtr, (char *) posn);
		if (entryPtr != NULL) {
		    Tcl_DeleteHashEntry(entryPtr);
		}
	    } else {
		entryPtr = Tcl_CreateHashEntry(hashTablePtr,
					       (char *) posn, &dummy);
		Tcl_SetHashValue(entryPtr, (ClientData) value);
	    }
	}
	TableAdjustParams(tablePtr);
	/* rerequest geometry */
	TableGeometryRequest(tablePtr);
	/*
	 * Invalidate the whole window as TableAdjustParams
	 * will only check to see if the top left cell has moved
	 * FIX: should just move from lowest order visible cell
	 * to edge of window
	 */
	TableInvalidateAll(tablePtr, 0);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_BboxCmd --
 *	This procedure is invoked to process the bbox method
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
Table_BboxCmd(ClientData clientData, register Tcl_Interp *interp,
	      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int x, y, w, h, row, col, key;
    Tcl_Obj *resultPtr;

    /* Returns bounding box of cell(s) */
    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "first ?last?");
	return TCL_ERROR;
    } else if (TableGetIndexObj(tablePtr, objv[2], &row, &col) == TCL_ERROR ||
	       (objc == 4 &&
		TableGetIndexObj(tablePtr, objv[3], &x, &y) == TCL_ERROR)) {
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);
    if (objc == 3) {
	row -= tablePtr->rowOffset; col -= tablePtr->colOffset;
	if (TableCellVCoords(tablePtr, row, col, &x, &y, &w, &h, 0)) {
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(x));
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(y));
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(w));
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(h));
	}
	return TCL_OK;
    } else {
	int r1, c1, r2, c2, minX = 99999, minY = 99999, maxX = 0, maxY = 0;

	row -= tablePtr->rowOffset; col -= tablePtr->colOffset;
	x -= tablePtr->rowOffset; y -= tablePtr->colOffset;
	r1 = MIN(row,x); r2 = MAX(row,x);
	c1 = MIN(col,y); c2 = MAX(col,y);
	key = 0;
	for (row = r1; row <= r2; row++) {
	    for (col = c1; col <= c2; col++) {
		if (TableCellVCoords(tablePtr, row, col, &x, &y, &w, &h, 0)) {
		    /* Get max bounding box */
		    if (x < minX) minX = x;
		    if (y < minY) minY = y;
		    if (x+w > maxX) maxX = x+w;
		    if (y+h > maxY) maxY = y+h;
		    key++;
		}
	    }
	}
	if (key) {
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(minX));
	    Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewIntObj(minY));
	    Tcl_ListObjAppendElement(NULL, resultPtr,
				     Tcl_NewIntObj(maxX-minX));
	    Tcl_ListObjAppendElement(NULL, resultPtr,
				     Tcl_NewIntObj(maxY-minY));
	}
    }
    return TCL_OK;
}

static CONST84 char *bdCmdNames[] = {
    "mark", "dragto", (char *)NULL
};
enum bdCmd {
    BD_MARK, BD_DRAGTO
};

/*
 *--------------------------------------------------------------
 *
 * Table_BorderCmd --
 *	This procedure is invoked to process the bbox method
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
Table_BorderCmd(ClientData clientData, register Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    Tcl_HashEntry *entryPtr;
    int x, y, w, h, row, col, key, dummy, value, cmdIndex;
    char *rc = NULL;
    Tcl_Obj *objPtr, *resultPtr;

    if (objc < 5 || objc > 6) {
	Tcl_WrongNumArgs(interp, 2, objv, "mark|dragto x y ?row|col?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], bdCmdNames,
			    "option", 0, &cmdIndex) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 6) {
	rc = Tcl_GetStringFromObj(objv[5], &w);
	if ((w < 1) || (strncmp(rc, "row", w) && strncmp(rc, "col", w))) {
	    Tcl_WrongNumArgs(interp, 2, objv, "mark|dragto x y ?row|col?");
	    return TCL_ERROR;
	}
    }

    resultPtr = Tcl_GetObjResult(interp);
    switch ((enum bdCmd) cmdIndex) {
    case BD_MARK:
	/* Use x && y to determine if we are over a border */
	value = TableAtBorder(tablePtr, x, y, &row, &col);
	/* Cache the row && col for use in DRAGTO */
	tablePtr->scanMarkRow = row;
	tablePtr->scanMarkCol = col;
	if (!value) {
	    return TCL_OK;
	}
	TableCellCoords(tablePtr, row, col, &x, &y, &dummy, &dummy);
	tablePtr->scanMarkX = x;
	tablePtr->scanMarkY = y;
	if (objc == 5 || *rc == 'r') {
	    if (row < 0) {
		objPtr = Tcl_NewStringObj("", 0);
	    } else {
		objPtr = Tcl_NewIntObj(row+tablePtr->rowOffset);
	    }
	    Tcl_ListObjAppendElement(NULL, resultPtr, objPtr);
	}
	if (objc == 5 || *rc == 'c') {
	    if (col < 0) {
		objPtr = Tcl_NewStringObj("", 0);
	    } else {
		objPtr = Tcl_NewIntObj(col+tablePtr->colOffset);
	    }
	    Tcl_ListObjAppendElement(NULL, resultPtr, objPtr);
	}
	return TCL_OK;	/* BORDER MARK */

    case BD_DRAGTO:
	/* check to see if we want to resize any borders */
	if (tablePtr->resize == SEL_NONE) { return TCL_OK; }
	row = tablePtr->scanMarkRow;
	col = tablePtr->scanMarkCol;
	TableCellCoords(tablePtr, row, col, &w, &h, &dummy, &dummy);
	key = 0;
	if (row >= 0 && (tablePtr->resize & SEL_ROW)) {
	    /* row border was active, move it */
	    value = y-h;
	    if (value < -1) value = -1;
	    if (value != tablePtr->scanMarkY) {
		entryPtr = Tcl_CreateHashEntry(tablePtr->rowHeights,
					       (char *) row, &dummy);
		/* -value means rowHeight will be interp'd as pixels, not
                   lines */
		Tcl_SetHashValue(entryPtr, (ClientData) MIN(0,-value));
		tablePtr->scanMarkY = value;
		key++;
	    }
	}
	if (col >= 0 && (tablePtr->resize & SEL_COL)) {
	    /* col border was active, move it */
	    value = x-w;
	    if (value < -1) value = -1;
	    if (value != tablePtr->scanMarkX) {
		entryPtr = Tcl_CreateHashEntry(tablePtr->colWidths,
					       (char *) col, &dummy);
		/* -value means colWidth will be interp'd as pixels, not
                   chars */
		Tcl_SetHashValue(entryPtr, (ClientData) MIN(0,-value));
		tablePtr->scanMarkX = value;
		key++;
	    }
	}
	/* Only if something changed do we want to update */
	if (key) {
	    TableAdjustParams(tablePtr);
	    /* Only rerequest geometry if the basis is the #rows &| #cols */
	    if (tablePtr->maxReqCols || tablePtr->maxReqRows)
		TableGeometryRequest(tablePtr);
	    TableInvalidateAll(tablePtr, 0);
	}
	return TCL_OK;	/* BORDER DRAGTO */
    }
    return TCL_OK;
}

/* clear subcommands */
static CONST84 char *clearNames[] = {
    "all", "cache", "sizes", "tags", (char *)NULL
};
enum clearCommand {
    CLEAR_ALL, CLEAR_CACHE, CLEAR_SIZES, CLEAR_TAGS
};

/*
 *--------------------------------------------------------------
 *
 * Table_ClearCmd --
 *	This procedure is invoked to process the clear method
 *	that corresponds to a table widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	Cached info can be lost.  Returns valid Tcl result.
 *
 * Side effects:
 *	Can cause redraw.
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
int
Table_ClearCmd(ClientData clientData, register Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int cmdIndex, redraw = 0;

    if (objc < 3 || objc > 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "option ?first? ?last?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], clearNames,
			    "clear option", 0, &cmdIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    if (objc == 3) {
	if (cmdIndex == CLEAR_TAGS || cmdIndex == CLEAR_ALL) {
	    Tcl_DeleteHashTable(tablePtr->rowStyles);
	    Tcl_DeleteHashTable(tablePtr->colStyles);
	    Tcl_DeleteHashTable(tablePtr->cellStyles);
	    Tcl_DeleteHashTable(tablePtr->flashCells);
	    Tcl_DeleteHashTable(tablePtr->selCells);

	    /* style hash tables */
	    Tcl_InitHashTable(tablePtr->rowStyles, TCL_ONE_WORD_KEYS);
	    Tcl_InitHashTable(tablePtr->colStyles, TCL_ONE_WORD_KEYS);
	    Tcl_InitHashTable(tablePtr->cellStyles, TCL_STRING_KEYS);

	    /* special style hash tables */
	    Tcl_InitHashTable(tablePtr->flashCells, TCL_STRING_KEYS);
	    Tcl_InitHashTable(tablePtr->selCells, TCL_STRING_KEYS);
	}

	if (cmdIndex == CLEAR_SIZES || cmdIndex == CLEAR_ALL) {
	    Tcl_DeleteHashTable(tablePtr->colWidths);
	    Tcl_DeleteHashTable(tablePtr->rowHeights);

	    /* style hash tables */
	    Tcl_InitHashTable(tablePtr->colWidths, TCL_ONE_WORD_KEYS);
	    Tcl_InitHashTable(tablePtr->rowHeights, TCL_ONE_WORD_KEYS);
	}

	if (cmdIndex == CLEAR_CACHE || cmdIndex == CLEAR_ALL) {
	    Table_ClearHashTable(tablePtr->cache);
	    Tcl_InitHashTable(tablePtr->cache, TCL_STRING_KEYS);
	    /* If we were caching and we have no other data source,
	     * invalidate all the cells */
	    if (tablePtr->dataSource == DATA_CACHE) {
		TableGetActiveBuf(tablePtr);
	    }
	}
	redraw = 1;
    } else {
	int row, col, r1, r2, c1, c2;
	Tcl_HashEntry *entryPtr;
	char buf[INDEX_BUFSIZE], *value;

	if (TableGetIndexObj(tablePtr, objv[3], &row, &col) != TCL_OK ||
	    ((objc == 5) &&
	     TableGetIndexObj(tablePtr, objv[4], &r2, &c2) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    r1 = r2 = row;
	    c1 = c2 = col;
	} else {
	    r1 = MIN(row,r2); r2 = MAX(row,r2);
	    c1 = MIN(col,c2); c2 = MAX(col,c2);
	}
	for (row = r1; row <= r2; row++) {
	    /* Note that *Styles entries are user based (no offset)
	     * while size entries are 0-based (real) */
	    if ((cmdIndex == CLEAR_TAGS || cmdIndex == CLEAR_ALL) &&
		(entryPtr = Tcl_FindHashEntry(tablePtr->rowStyles,
					      (char *) row))) {
		Tcl_DeleteHashEntry(entryPtr);
		redraw = 1;
	    }

	    if ((cmdIndex == CLEAR_SIZES || cmdIndex == CLEAR_ALL) &&
		(entryPtr = Tcl_FindHashEntry(tablePtr->rowHeights,
					      (char *) row-tablePtr->rowOffset))) {
		Tcl_DeleteHashEntry(entryPtr);
		redraw = 1;
	    }

	    for (col = c1; col <= c2; col++) {
		TableMakeArrayIndex(row, col, buf);

		if (cmdIndex == CLEAR_TAGS || cmdIndex == CLEAR_ALL) {
		    if ((row == r1) &&
			(entryPtr = Tcl_FindHashEntry(tablePtr->colStyles,
						      (char *) col))) {
			Tcl_DeleteHashEntry(entryPtr);
			redraw = 1;
		    }
		    if ((entryPtr = Tcl_FindHashEntry(tablePtr->cellStyles,
						      buf))) {
			Tcl_DeleteHashEntry(entryPtr);
			redraw = 1;
		    }
		    if ((entryPtr = Tcl_FindHashEntry(tablePtr->flashCells,
						      buf))) {
			Tcl_DeleteHashEntry(entryPtr);
			redraw = 1;
		    }
		    if ((entryPtr = Tcl_FindHashEntry(tablePtr->selCells,
						      buf))) {
			Tcl_DeleteHashEntry(entryPtr);
			redraw = 1;
		    }
		}

		if ((cmdIndex == CLEAR_SIZES || cmdIndex == CLEAR_ALL) &&
		    row == r1 &&
		    (entryPtr = Tcl_FindHashEntry(tablePtr->colWidths, (char *)
						  col-tablePtr->colOffset))) {
		    Tcl_DeleteHashEntry(entryPtr);
		    redraw = 1;
		}

		if ((cmdIndex == CLEAR_CACHE || cmdIndex == CLEAR_ALL) &&
		    (entryPtr = Tcl_FindHashEntry(tablePtr->cache, buf))) {
		    value = (char *) Tcl_GetHashValue(entryPtr);
		    if (value) { ckfree(value); }
		    Tcl_DeleteHashEntry(entryPtr);
		    /* if the cache is our data source,
		     * we need to invalidate the cells changed */
		    if ((tablePtr->dataSource == DATA_CACHE) &&
			(row-tablePtr->rowOffset == tablePtr->activeRow &&
			 col-tablePtr->colOffset == tablePtr->activeCol))
			TableGetActiveBuf(tablePtr);
		    redraw = 1;
		}
	    }
	}
    }
    /* This could be more sensitive about what it updates,
     * but that can actually be a lot more costly in some cases */
    if (redraw) {
	if (cmdIndex == CLEAR_SIZES || cmdIndex == CLEAR_ALL) {
	    TableAdjustParams(tablePtr);
	    /* rerequest geometry */
	    TableGeometryRequest(tablePtr);
	}
	TableInvalidateAll(tablePtr, 0);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_CurselectionCmd --
 *	This procedure is invoked to process the bbox method
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
Table_CurselectionCmd(ClientData clientData, register Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    char *value = NULL;
    int row, col;

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "?value?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	/* make sure there is a data source to accept a set value */
	if ((tablePtr->state == STATE_DISABLED) ||
	    (tablePtr->dataSource == DATA_NONE)) {
	    return TCL_OK;
	}
	value = Tcl_GetString(objv[2]);
	for (entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
	     entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	    TableParseArrayIndex(&row, &col,
				 Tcl_GetHashKey(tablePtr->selCells, entryPtr));
	    TableSetCellValue(tablePtr, row, col, value);
	    row -= tablePtr->rowOffset;
	    col -= tablePtr->colOffset;
	    if (row == tablePtr->activeRow && col == tablePtr->activeCol) {
		TableGetActiveBuf(tablePtr);
	    }
	    TableRefresh(tablePtr, row, col, CELL);
	}
    } else {
	Tcl_Obj *objPtr = Tcl_NewObj();

	for (entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
	     entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	    value = Tcl_GetHashKey(tablePtr->selCells, entryPtr);
	    Tcl_ListObjAppendElement(NULL, objPtr,
				     Tcl_NewStringObj(value, -1));
	}
	Tcl_SetObjResult(interp, TableCellSortObj(interp, objPtr));
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_CurvalueCmd --
 *	This procedure is invoked to process the curvalue method
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
Table_CurvalueCmd(ClientData clientData, register Tcl_Interp *interp,
		  int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "?<value>?");
	return TCL_ERROR;
    } else if (!(tablePtr->flags & HAS_ACTIVE)) {
	return TCL_OK;
    }

    if (objc == 3) {
	char *value;
	int len;

	value = Tcl_GetStringFromObj(objv[2], &len);
	if (STREQ(value, tablePtr->activeBuf)) {
	    Tcl_SetObjResult(interp, objv[2]);
	    return TCL_OK;
	}
	/* validate potential new active buffer contents
	 * only accept if validation returns acceptance. */
	if (tablePtr->validate &&
	    TableValidateChange(tablePtr,
				tablePtr->activeRow+tablePtr->rowOffset,
				tablePtr->activeCol+tablePtr->colOffset,
				tablePtr->activeBuf,
				value, tablePtr->icursor) != TCL_OK) {
	    return TCL_OK;
	}
	tablePtr->activeBuf = (char *)ckrealloc(tablePtr->activeBuf, len+1);
	strcpy(tablePtr->activeBuf, value);
	/* mark the text as changed */
	tablePtr->flags |= TEXT_CHANGED;
	TableSetActiveIndex(tablePtr);
	/* check for possible adjustment of icursor */
	TableGetIcursor(tablePtr, "insert", (int *)0);
	TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol, CELL);
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(tablePtr->activeBuf, -1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_GetCmd --
 *	This procedure is invoked to process the bbox method
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
Table_GetCmd(ClientData clientData, register Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int result = TCL_OK;
    int r1, c1, r2, c2, row, col;

    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "first ?last?");
	result = TCL_ERROR;
    } else if (TableGetIndexObj(tablePtr, objv[2], &row, &col) == TCL_ERROR) {
	result = TCL_ERROR;
    } else if (objc == 3) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj(TableGetCellValue(tablePtr, row, col), -1));
    } else if (TableGetIndexObj(tablePtr, objv[3], &r2, &c2) == TCL_ERROR) {
	result = TCL_ERROR;
    } else {
	Tcl_Obj *objPtr = Tcl_NewObj();

	r1 = MIN(row,r2); r2 = MAX(row,r2);
	c1 = MIN(col,c2); c2 = MAX(col,c2);
	for ( row = r1; row <= r2; row++ ) {
	    for ( col = c1; col <= c2; col++ ) {
		Tcl_ListObjAppendElement(NULL, objPtr,
			Tcl_NewStringObj(TableGetCellValue(tablePtr,
				row, col), -1));
	    }
	}
	Tcl_SetObjResult(interp, objPtr);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Table_ScanCmd --
 *	This procedure is invoked to process the scan method
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
Table_ScanCmd(ClientData clientData, register Tcl_Interp *interp,
	      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int x, y, row, col, cmdIndex;

    if (objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "mark|dragto x y");
	return TCL_ERROR;
    } else if (Tcl_GetIndexFromObj(interp, objv[2], bdCmdNames,
	    "option", 0, &cmdIndex) != TCL_OK ||
	    Tcl_GetIntFromObj(interp, objv[3], &x) == TCL_ERROR ||
	    Tcl_GetIntFromObj(interp, objv[4], &y) == TCL_ERROR) {
	return TCL_ERROR;
    }
    switch ((enum bdCmd) cmdIndex) {
	case BD_MARK:
	    TableWhatCell(tablePtr, x, y, &row, &col);
	    tablePtr->scanMarkRow = row-tablePtr->topRow;
	    tablePtr->scanMarkCol = col-tablePtr->leftCol;
	    tablePtr->scanMarkX = x;
	    tablePtr->scanMarkY = y;
	    break;

	case BD_DRAGTO: {
	    int oldTop = tablePtr->topRow, oldLeft = tablePtr->leftCol;
	    y += (5*(y-tablePtr->scanMarkY));
	    x += (5*(x-tablePtr->scanMarkX));

	    TableWhatCell(tablePtr, x, y, &row, &col);

	    /* maintain appropriate real index */
	    tablePtr->topRow  = BETWEEN(row-tablePtr->scanMarkRow,
		    tablePtr->titleRows, tablePtr->rows-1);
	    tablePtr->leftCol = BETWEEN(col-tablePtr->scanMarkCol,
		    tablePtr->titleCols, tablePtr->cols-1);

	    /* Adjust the table if new top left */
	    if (oldTop != tablePtr->topRow || oldLeft != tablePtr->leftCol) {
		TableAdjustParams(tablePtr);
	    }
	    break;
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_SelAnchorCmd --
 *	This procedure is invoked to process the selection anchor method
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
Table_SelAnchorCmd(ClientData clientData, register Tcl_Interp *interp,
		   int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int row, col;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "index");
	return TCL_ERROR;
    } else if (TableGetIndexObj(tablePtr, objv[3], &row, &col) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr->flags |= HAS_ANCHOR;
    /* maintain appropriate real index */
    if (tablePtr->selectTitles) {
	tablePtr->anchorRow = BETWEEN(row-tablePtr->rowOffset,
		0, tablePtr->rows-1);
	tablePtr->anchorCol = BETWEEN(col-tablePtr->colOffset,
		0, tablePtr->cols-1);
    } else {
	tablePtr->anchorRow = BETWEEN(row-tablePtr->rowOffset,
		tablePtr->titleRows, tablePtr->rows-1);
	tablePtr->anchorCol = BETWEEN(col-tablePtr->colOffset,
		tablePtr->titleCols, tablePtr->cols-1);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_SelClearCmd --
 *	This procedure is invoked to process the selection clear method
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
Table_SelClearCmd(ClientData clientData, register Tcl_Interp *interp,
		  int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int result = TCL_OK;
    char buf1[INDEX_BUFSIZE];
    int row, col, key, clo=0,chi=0,r1,c1,r2,c2;
    Tcl_HashEntry *entryPtr;

    if (objc < 4 || objc > 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "all|<first> ?<last>?");
	return TCL_ERROR;
    }
    if (STREQ(Tcl_GetString(objv[3]), "all")) {
	Tcl_HashSearch search;
	for(entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
	    entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	    TableParseArrayIndex(&row, &col,
				 Tcl_GetHashKey(tablePtr->selCells,entryPtr));
	    Tcl_DeleteHashEntry(entryPtr);
	    TableRefresh(tablePtr, row-tablePtr->rowOffset,
			 col-tablePtr->colOffset, CELL);
	}
	return TCL_OK;
    }
    if (TableGetIndexObj(tablePtr, objv[3], &row, &col) == TCL_ERROR ||
	(objc==5 &&
	 TableGetIndexObj(tablePtr, objv[4], &r2, &c2) == TCL_ERROR)) {
	return TCL_ERROR;
    }
    key = 0;
    if (objc == 4) {
	r1 = r2 = row;
	c1 = c2 = col;
    } else {
	r1 = MIN(row,r2); r2 = MAX(row,r2);
	c1 = MIN(col,c2); c2 = MAX(col,c2);
    }
    switch (tablePtr->selectType) {
    case SEL_BOTH:
	clo = c1; chi = c2;
	c1 = tablePtr->colOffset;
	c2 = tablePtr->cols-1+c1;
	key = 1;
	goto CLEAR_CELLS;
    CLEAR_BOTH:
	key = 0;
	c1 = clo; c2 = chi;
    case SEL_COL:
	r1 = tablePtr->rowOffset;
	r2 = tablePtr->rows-1+r1;
	break;
    case SEL_ROW:
	c1 = tablePtr->colOffset;
	c2 = tablePtr->cols-1+c1;
	break;
    }
    /* row/col are in user index coords */
CLEAR_CELLS:
    for ( row = r1; row <= r2; row++ ) {
	for ( col = c1; col <= c2; col++ ) {
	    TableMakeArrayIndex(row, col, buf1);
	    entryPtr = Tcl_FindHashEntry(tablePtr->selCells, buf1);
	    if (entryPtr != NULL) {
		Tcl_DeleteHashEntry(entryPtr);
		TableRefresh(tablePtr, row-tablePtr->rowOffset,
			     col-tablePtr->colOffset, CELL);
	    }
	}
    }
    if (key) goto CLEAR_BOTH;
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Table_SelIncludesCmd --
 *	This procedure is invoked to process the selection includes method
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
Table_SelIncludesCmd(ClientData clientData, register Tcl_Interp *interp,
		     int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int row, col;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "index");
	return TCL_ERROR;
    } else if (TableGetIndexObj(tablePtr, objv[3], &row, &col) == TCL_ERROR) {
	return TCL_ERROR;
    } else {
	char buf[INDEX_BUFSIZE];
	TableMakeArrayIndex(row, col, buf);
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
			  (Tcl_FindHashEntry(tablePtr->selCells, buf)!=NULL));
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_SelSetCmd --
 *	This procedure is invoked to process the selection set method
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
Table_SelSetCmd(ClientData clientData, register Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int row, col, dummy, key;
    char buf1[INDEX_BUFSIZE];
    Tcl_HashSearch search;
    Tcl_HashEntry *entryPtr;

    int clo=0, chi=0, r1, c1, r2, c2, firstRow, firstCol, lastRow, lastCol;
    if (objc < 4 || objc > 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
	return TCL_ERROR;
    }
    if (TableGetIndexObj(tablePtr, objv[3], &row, &col) == TCL_ERROR ||
	(objc==5 &&
	 TableGetIndexObj(tablePtr, objv[4], &r2, &c2) == TCL_ERROR)) {
	return TCL_ERROR;
    }
    key = 0;
    lastRow = tablePtr->rows-1+tablePtr->rowOffset;
    lastCol = tablePtr->cols-1+tablePtr->colOffset;
    if (tablePtr->selectTitles) {
	firstRow = tablePtr->rowOffset;
	firstCol = tablePtr->colOffset;
    } else {
	firstRow = tablePtr->titleRows+tablePtr->rowOffset;
	firstCol = tablePtr->titleCols+tablePtr->colOffset;
    }
    /* maintain appropriate user index */
    CONSTRAIN(row, firstRow, lastRow);
    CONSTRAIN(col, firstCol, lastCol);
    if (objc == 4) {
	r1 = r2 = row;
	c1 = c2 = col;
    } else {
	CONSTRAIN(r2, firstRow, lastRow);
	CONSTRAIN(c2, firstCol, lastCol);
	r1 = MIN(row,r2); r2 = MAX(row,r2);
	c1 = MIN(col,c2); c2 = MAX(col,c2);
    }
    switch (tablePtr->selectType) {
    case SEL_BOTH:
	if (firstCol > lastCol) c2--; /* No selectable columns in table */
	if (firstRow > lastRow) r2--; /* No selectable rows in table */
	clo = c1; chi = c2;
	c1 = firstCol;
	c2 = lastCol;
	key = 1;
	goto SET_CELLS;
    SET_BOTH:
	key = 0;
	c1 = clo; c2 = chi;
    case SEL_COL:
	r1 = firstRow;
	r2 = lastRow;
	if (firstCol > lastCol) c2--; /* No selectable columns in table */
	break;
    case SEL_ROW:
	c1 = firstCol;
	c2 = lastCol;
	if (firstRow>lastRow) r2--; /* No selectable rows in table */
	break;
    }
SET_CELLS:
    entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
    for ( row = r1; row <= r2; row++ ) {
	for ( col = c1; col <= c2; col++ ) {
	    TableMakeArrayIndex(row, col, buf1);
	    if (Tcl_FindHashEntry(tablePtr->selCells, buf1) == NULL) {
		Tcl_CreateHashEntry(tablePtr->selCells, buf1, &dummy);
		TableRefresh(tablePtr, row-tablePtr->rowOffset,
			     col-tablePtr->colOffset, CELL);
	    }
	}
    }
    if (key) goto SET_BOTH;

    /* Adjust the table for top left, selection on screen etc */
    TableAdjustParams(tablePtr);

    /* If the table was previously empty and we want to export the
     * selection, we should grab it now */
    if (entryPtr == NULL && tablePtr->exportSelection) {
	Tk_OwnSelection(tablePtr->tkwin, XA_PRIMARY, TableLostSelection,
			(ClientData) tablePtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Table_ViewCmd --
 *	This procedure is invoked to process the x|yview method
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
Table_ViewCmd(ClientData clientData, register Tcl_Interp *interp,
	      int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int row, col, value;
    char *xy;

    /* Check xview or yview */
    if (objc > 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "?args?");
	return TCL_ERROR;
    }
    xy = Tcl_GetString(objv[1]);

    if (objc == 2) {
	Tcl_Obj *resultPtr;
	int diff, x, y, w, h;
	double first, last;

	resultPtr = Tcl_GetObjResult(interp);
	TableGetLastCell(tablePtr, &row, &col);
	TableCellVCoords(tablePtr, row, col, &x, &y, &w, &h, 0);
	if (*xy == 'y') {
	    if (row < tablePtr->titleRows) {
		first = 0;
		last  = 1;
	    } else {
		diff = tablePtr->rowStarts[tablePtr->titleRows];
		last = (double) (tablePtr->rowStarts[tablePtr->rows]-diff);
		first = (tablePtr->rowStarts[tablePtr->topRow]-diff) / last;
		last  = (h+tablePtr->rowStarts[row]-diff) / last;
	    }
	} else {
	    if (col < tablePtr->titleCols) {
		first = 0;
		last  = 1;
	    } else {
		diff = tablePtr->colStarts[tablePtr->titleCols];
		last = (double) (tablePtr->colStarts[tablePtr->cols]-diff);
		first = (tablePtr->colStarts[tablePtr->leftCol]-diff) / last;
		last  = (w+tablePtr->colStarts[col]-diff) / last;
	    }
	}
	Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewDoubleObj(first));
	Tcl_ListObjAppendElement(NULL, resultPtr, Tcl_NewDoubleObj(last));
    } else {
	/* cache old topleft to see if it changes */
	int oldTop = tablePtr->topRow, oldLeft = tablePtr->leftCol;

	if (objc == 3) {
	    if (Tcl_GetIntFromObj(interp, objv[2], &value) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (*xy == 'y') {
		tablePtr->topRow  = value + tablePtr->titleRows;
	    } else {
		tablePtr->leftCol = value + tablePtr->titleCols;
	    }
	} else {
	    int result;
	    double frac;
#if (TK_MINOR_VERSION > 0) /* 8.1+ */
	    result = Tk_GetScrollInfoObj(interp, objc, objv, &frac, &value);
#else
	    int i;
	    char **argv = (char **) ckalloc((objc + 1) * sizeof(char *));
	    for (i = 0; i < objc; i++) {
		argv[i] = Tcl_GetString(objv[i]);
	    }
	    argv[i] = NULL;
	    result = Tk_GetScrollInfo(interp, objc, argv, &frac, &value);
	    ckfree ((char *) argv);
#endif
	    switch (result) {
	    case TK_SCROLL_ERROR:
		return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
		if (frac < 0) frac = 0;
		if (*xy == 'y') {
		    tablePtr->topRow = (int)(frac*tablePtr->rows)
			+tablePtr->titleRows;
		} else {
		    tablePtr->leftCol = (int)(frac*tablePtr->cols)
			+tablePtr->titleCols;
		}
		break;
	    case TK_SCROLL_PAGES:
		TableGetLastCell(tablePtr, &row, &col);
		if (*xy == 'y') {
		    tablePtr->topRow  += value * (row-tablePtr->topRow+1);
		} else {
		    tablePtr->leftCol += value * (col-tablePtr->leftCol+1);
		}
		break;
	    case TK_SCROLL_UNITS:
		if (*xy == 'y') {
		    tablePtr->topRow  += value;
		} else {
		    tablePtr->leftCol += value;
		}
		break;
	    }
	}
	/* maintain appropriate real index */
	CONSTRAIN(tablePtr->topRow, tablePtr->titleRows, tablePtr->rows-1);
	CONSTRAIN(tablePtr->leftCol, tablePtr->titleCols, tablePtr->cols-1);
	/* Do the table adjustment if topRow || leftCol changed */	
	if (oldTop != tablePtr->topRow || oldLeft != tablePtr->leftCol) {
	    TableAdjustParams(tablePtr);
	}
    }

    return TCL_OK;
}

#if 0
/*
 *--------------------------------------------------------------
 *
 * Table_Cmd --
 *	This procedure is invoked to process the CMD method
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
Table_Cmd(ClientData clientData, register Tcl_Interp *interp,
	  int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *) clientData;
    int result = TCL_OK;

    return result;
}
#endif
