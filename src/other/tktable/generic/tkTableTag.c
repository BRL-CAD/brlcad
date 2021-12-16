/* 
 * tkTableTag.c --
 *
 *	This module implements tags for table widgets.
 *
 * Copyright (c) 1998-2002 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkTable.h"

static TableTag *TableTagGetEntry _ANSI_ARGS_((Table *tablePtr, char *name,
	int objc, CONST char **argv));
static unsigned int	TableTagGetPriority _ANSI_ARGS_((Table *tablePtr,
	TableTag *tagPtr));
static void	TableImageProc _ANSI_ARGS_((ClientData clientData, int x,
	int y, int width, int height, int imageWidth, int imageHeight));
static int	TableOptionReliefSet _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, Tk_Window tkwin,
			CONST84 char *value, char *widgRec, int offset));
static char *	TableOptionReliefGet _ANSI_ARGS_((ClientData clientData,
			Tk_Window tkwin, char *widgRec, int offset,
			Tcl_FreeProc **freeProcPtr));

static CONST84 char *tagCmdNames[] = {
    "celltag", "cget", "coltag", "configure", "delete", "exists",
    "includes", "lower", "names", "raise", "rowtag", (char *) NULL
};

enum tagCmd {
    TAG_CELLTAG, TAG_CGET, TAG_COLTAG, TAG_CONFIGURE, TAG_DELETE, TAG_EXISTS,
    TAG_INCLUDES, TAG_LOWER, TAG_NAMES, TAG_RAISE, TAG_ROWTAG
};

static Cmd_Struct tagState_vals[]= {
    {"unknown",	 STATE_UNKNOWN},
    {"normal",	 STATE_NORMAL},
    {"disabled", STATE_DISABLED},
    {"",	 0 }
};

static Tk_CustomOption tagStateOpt =
{ Cmd_OptionSet, Cmd_OptionGet, (ClientData) (&tagState_vals) };
static Tk_CustomOption tagBdOpt =
{ TableOptionBdSet, TableOptionBdGet, (ClientData) BD_TABLE_TAG };
static Tk_CustomOption tagReliefOpt =
{ TableOptionReliefSet, TableOptionReliefGet, (ClientData) NULL };

/*
 * The default specification for configuring tags
 * Done like this to make the command line parsing easy
 */

static Tk_ConfigSpec tagConfig[] = {
  {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor", "center",
   Tk_Offset(TableTag, anchor), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_BORDER, "-background", "background", "Background", NULL,
   Tk_Offset(TableTag, bg), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
  {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
  {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth", "",
   0 /* no offset */,
   TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK, &tagBdOpt },
  {TK_CONFIG_STRING, "-ellipsis", "ellipsis", "Ellipsis", "",
   Tk_Offset(TableTag, ellipsis), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_BORDER, "-foreground", "foreground", "Foreground", NULL,
   Tk_Offset(TableTag, fg), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
  {TK_CONFIG_FONT, "-font", "font", "Font", NULL,
   Tk_Offset(TableTag, tkfont), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_STRING, "-image", "image", "Image", NULL,
   Tk_Offset(TableTag, imageStr),
   TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify", "left",
   Tk_Offset(TableTag, justify), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK },
  {TK_CONFIG_INT, "-multiline", "multiline", "Multiline", "-1",
   Tk_Offset(TableTag, multiline), TK_CONFIG_DONT_SET_DEFAULT },
  {TK_CONFIG_CUSTOM, "-relief", "relief", "Relief", "flat",
   Tk_Offset(TableTag, relief), TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK,
   &tagReliefOpt },
  {TK_CONFIG_INT, "-showtext", "showText", "ShowText", "-1",
   Tk_Offset(TableTag, showtext), TK_CONFIG_DONT_SET_DEFAULT },
  {TK_CONFIG_CUSTOM, "-state", "state", "State", "unknown",
   Tk_Offset(TableTag, state), TK_CONFIG_DONT_SET_DEFAULT, &tagStateOpt },
  {TK_CONFIG_INT, "-wrap", "wrap", "Wrap", "-1",
   Tk_Offset(TableTag, wrap), TK_CONFIG_DONT_SET_DEFAULT },
  {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 0, 0}
};

/*
 * The join tag structure is used to create a combined tag, so it
 * keeps priority info.
 */
typedef struct {
    TableTag	tag;		/* must be first */
    unsigned int magic;
    unsigned int pbg, pfg, pborders, prelief, ptkfont, panchor, pimage;
    unsigned int pstate, pjustify, pmultiline, pwrap, pshowtext, pellipsis;
} TableJoinTag;

/* 
 *----------------------------------------------------------------------
 *
 * TableImageProc --
 *	Called when an image associated with a tag is changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invalidates the whole table.
 *	This should only invalidate affected cells, but that info
 *	is not managed...
 *
 *----------------------------------------------------------------------
 */
static void
TableImageProc(ClientData clientData, int x, int y, int width, int height,
	       int imageWidth, int imageHeight)
{
    TableInvalidateAll((Table *)clientData, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * TableNewTag --
 *	ckallocs space for a new tag structure and inits the structure.
 *
 * Results:
 *	Returns a pointer to the new structure.  Must be freed later.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
TableTag *
TableNewTag(Table *tablePtr)
{
    TableTag *tagPtr;

    /*
     * If tablePtr is NULL, make a regular tag, otherwise make a join tag.
     */
    if (tablePtr == NULL) {
	tagPtr = (TableTag *) ckalloc(sizeof(TableTag));
	memset((VOID *) tagPtr, 0, sizeof(TableTag));

	/*
	 * Set the values that aren't 0/NULL by default
	 */
	tagPtr->anchor		= (Tk_Anchor)-1;
	tagPtr->justify		= (Tk_Justify)-1;
	tagPtr->multiline	= -1;
	tagPtr->relief		= -1;
	tagPtr->showtext	= -1;
	tagPtr->state		= STATE_UNKNOWN;
	tagPtr->wrap		= -1;
    } else {
	TableJoinTag *jtagPtr = (TableJoinTag *) ckalloc(sizeof(TableJoinTag));
	memset((VOID *) jtagPtr, 0, sizeof(TableJoinTag));
	tagPtr = (TableTag *) jtagPtr;

	tagPtr->anchor		= (Tk_Anchor)-1;
	tagPtr->justify		= (Tk_Justify)-1;
	tagPtr->multiline	= -1;
	tagPtr->relief		= -1;
	tagPtr->showtext	= -1;
	tagPtr->state		= STATE_UNKNOWN;
	tagPtr->wrap		= -1;
	jtagPtr->magic		= 0x99ABCDEF;
	jtagPtr->pbg		= -1;
	jtagPtr->pfg		= -1;
	jtagPtr->pborders	= -1;
	jtagPtr->prelief	= -1;
	jtagPtr->ptkfont	= -1;
	jtagPtr->panchor	= -1;
	jtagPtr->pimage		= -1;
	jtagPtr->pstate		= -1;
	jtagPtr->pjustify	= -1;
	jtagPtr->pmultiline	= -1;
	jtagPtr->pwrap		= -1;
	jtagPtr->pshowtext	= -1;
	jtagPtr->pellipsis	= -1;
    }

    return (TableTag *) tagPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TableResetTag --
 *	This routine resets a given tag to the table defaults.
 *
 * Results:
 *	Tag will have values changed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
TableResetTag(Table *tablePtr, TableTag *tagPtr)
{
    TableJoinTag *jtagPtr = (TableJoinTag *) tagPtr;

    if (jtagPtr->magic != 0x99ABCDEF) {
	panic("bad mojo in TableResetTag");
    }

    memset((VOID *) jtagPtr, 0, sizeof(TableJoinTag));

    tagPtr->anchor	= (Tk_Anchor)-1;
    tagPtr->justify	= (Tk_Justify)-1;
    tagPtr->multiline	= -1;
    tagPtr->relief	= -1;
    tagPtr->showtext	= -1;
    tagPtr->state	= STATE_UNKNOWN;
    tagPtr->wrap	= -1;
    jtagPtr->magic	= 0x99ABCDEF;
    jtagPtr->pbg	= -1;
    jtagPtr->pfg	= -1;
    jtagPtr->pborders	= -1;
    jtagPtr->prelief	= -1;
    jtagPtr->ptkfont	= -1;
    jtagPtr->panchor	= -1;
    jtagPtr->pimage	= -1;
    jtagPtr->pstate	= -1;
    jtagPtr->pjustify	= -1;
    jtagPtr->pmultiline	= -1;
    jtagPtr->pwrap	= -1;
    jtagPtr->pshowtext	= -1;
    jtagPtr->pellipsis	= -1;

    /*
     * Merge in the default tag.
     */
    memcpy((VOID *) jtagPtr, (VOID *) &(tablePtr->defaultTag),
	    sizeof(TableTag));
}

/*
 *----------------------------------------------------------------------
 *
 * TableMergeTag --
 *	This routine merges two tags by adding any fields from the addTag
 *	that are set to the baseTag.
 *
 * Results:
 *	baseTag will inherit all set characteristics of addTag
 *	(addTag thus has the priority).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
TableMergeTag(Table *tablePtr, TableTag *baseTag, TableTag *addTag)
{
    TableJoinTag *jtagPtr = (TableJoinTag *) baseTag;
    unsigned int prio;

    if (jtagPtr->magic != 0x99ABCDEF) {
	panic("bad mojo in TableMergeTag");
    }

#ifndef NO_TAG_PRIORITIES
    /*
     * Find priority for the tag to merge
     */
    prio = TableTagGetPriority(tablePtr, addTag);

    if ((addTag->anchor != -1) && (prio < jtagPtr->panchor)) {
	baseTag->anchor		= addTag->anchor;
	jtagPtr->panchor	= prio;
    }
    if ((addTag->bg != NULL) && (prio < jtagPtr->pbg)) {
	baseTag->bg		= addTag->bg;
	jtagPtr->pbg		= prio;
    }
    if ((addTag->fg != NULL) && (prio < jtagPtr->pfg)) {
	baseTag->fg		= addTag->fg;
	jtagPtr->pfg		= prio;
    }
    if ((addTag->ellipsis != NULL) && (prio < jtagPtr->pellipsis)) {
	baseTag->ellipsis	= addTag->ellipsis;
	jtagPtr->pellipsis	= prio;
    }
    if ((addTag->tkfont != NULL) && (prio < jtagPtr->ptkfont)) {
	baseTag->tkfont		= addTag->tkfont;
	jtagPtr->ptkfont	= prio;
    }
    if ((addTag->imageStr != NULL) && (prio < jtagPtr->pimage)) {
	baseTag->imageStr	= addTag->imageStr;
	baseTag->image		= addTag->image;
	jtagPtr->pimage		= prio;
    }
    if ((addTag->multiline >= 0) && (prio < jtagPtr->pmultiline)) {
	baseTag->multiline	= addTag->multiline;
	jtagPtr->pmultiline	= prio;
    }
    if ((addTag->relief != -1) && (prio < jtagPtr->prelief)) {
	baseTag->relief		= addTag->relief;
	jtagPtr->prelief	= prio;
    }
    if ((addTag->showtext >= 0) && (prio < jtagPtr->pshowtext)) {
	baseTag->showtext	= addTag->showtext;
	jtagPtr->pshowtext	= prio;
    }
    if ((addTag->state != STATE_UNKNOWN) && (prio < jtagPtr->pstate)) {
	baseTag->state		= addTag->state;
	jtagPtr->pstate		= prio;
    }
    if ((addTag->justify != -1) && (prio < jtagPtr->pjustify)) {
	baseTag->justify	= addTag->justify;
	jtagPtr->pjustify	= prio;
    }
    if ((addTag->wrap >= 0) && (prio < jtagPtr->pwrap)) {
	baseTag->wrap		= addTag->wrap;
	jtagPtr->pwrap		= prio;
    }
    if ((addTag->borders) && (prio < jtagPtr->pborders)) {
	baseTag->borderStr	= addTag->borderStr;
	baseTag->borders	= addTag->borders;
	baseTag->bd[0]		= addTag->bd[0];
	baseTag->bd[1]		= addTag->bd[1];
	baseTag->bd[2]		= addTag->bd[2];
	baseTag->bd[3]		= addTag->bd[3];
	jtagPtr->pborders	= prio;
    }
#else
    if (addTag->anchor != -1)	baseTag->anchor = addTag->anchor;
    if (addTag->bg != NULL)	baseTag->bg	= addTag->bg;
    if (addTag->fg != NULL)	baseTag->fg	= addTag->fg;
    if (addTag->ellipsis != NULL) baseTag->ellipsis = addTag->ellipsis;
    if (addTag->tkfont != NULL)	baseTag->tkfont	= addTag->tkfont;
    if (addTag->imageStr != NULL) {
	baseTag->imageStr	= addTag->imageStr;
	baseTag->image		= addTag->image;
    }
    if (addTag->multiline >= 0)	baseTag->multiline	= addTag->multiline;
    if (addTag->relief != -1)	baseTag->relief		= addTag->relief;
    if (addTag->showtext >= 0)	baseTag->showtext	= addTag->showtext;
    if (addTag->state != STATE_UNKNOWN)	baseTag->state	= addTag->state;
    if (addTag->justify != -1)	baseTag->justify	= addTag->justify;
    if (addTag->wrap >= 0)	baseTag->wrap		= addTag->wrap;
    if (addTag->borders) {
	baseTag->borderStr	= addTag->borderStr;
	baseTag->borders	= addTag->borders;
	baseTag->bd[0]		= addTag->bd[0];
	baseTag->bd[1]		= addTag->bd[1];
	baseTag->bd[2]		= addTag->bd[2];
	baseTag->bd[3]		= addTag->bd[3];
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TableInvertTag --
 *	This routine swaps background and foreground for the selected tag.
 *
 * Results:
 *	Inverts fg and bg of tag.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
TableInvertTag(TableTag *baseTag)
{
    Tk_3DBorder tmpBg;

    tmpBg	= baseTag->fg;
    baseTag->fg	= baseTag->bg;
    baseTag->bg	= tmpBg;
}

/*
 *----------------------------------------------------------------------
 *
 * TableGetTagBorders --
 *	This routine gets the border values based on a tag.
 *
 * Results:
 *	It returns the values in the int*'s (if not NULL), and the
 *	total number of defined borders as a result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
TableGetTagBorders(TableTag *tagPtr,
	int *left, int *right, int *top, int *bottom)
{
    switch (tagPtr->borders) {
	case 0:
	    if (left)	{ *left		= 0; }
	    if (right)	{ *right	= 0; }
	    if (top)	{ *top		= 0; }
	    if (bottom)	{ *bottom	= 0; }
	    break;
	case 1:
	    if (left)	{ *left		= tagPtr->bd[0]; }
	    if (right)	{ *right	= tagPtr->bd[0]; }
	    if (top)	{ *top		= tagPtr->bd[0]; }
	    if (bottom)	{ *bottom	= tagPtr->bd[0]; }
	    break;
	case 2:
	    if (left)	{ *left		= tagPtr->bd[0]; }
	    if (right)	{ *right	= tagPtr->bd[1]; }
	    if (top)	{ *top		= 0; }
	    if (bottom)	{ *bottom	= 0; }
	    break;
	case 4:
	    if (left)	{ *left		= tagPtr->bd[0]; }
	    if (right)	{ *right	= tagPtr->bd[1]; }
	    if (top)	{ *top		= tagPtr->bd[2]; }
	    if (bottom)	{ *bottom	= tagPtr->bd[3]; }
	    break;
	default:
	    panic("invalid border value '%d'\n", tagPtr->borders);
	    break;
    }
    return tagPtr->borders;
}

/*
 *----------------------------------------------------------------------
 *
 * TableTagGetEntry --
 *	Takes a name and optional args and creates a tag entry in the
 *	table's tag table.
 *
 * Results:
 *	A new tag entry will be created and returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static TableTag *
TableTagGetEntry(Table *tablePtr, char *name, int objc, CONST char **argv)
{
    Tcl_HashEntry *entryPtr;
    TableTag *tagPtr = NULL;
    int new;

    entryPtr = Tcl_CreateHashEntry(tablePtr->tagTable, name, &new);
    if (new) {
	tagPtr = TableNewTag(NULL);
	Tcl_SetHashValue(entryPtr, (ClientData) tagPtr);
	if (tablePtr->tagPrioSize >= tablePtr->tagPrioMax) {
	    int i;
	    /*
	     * Increase the priority list size in blocks of 10
	     */
	    tablePtr->tagPrioMax += 10;
	    tablePtr->tagPrioNames = (char **) ckrealloc(
		(char *) tablePtr->tagPrioNames,
		sizeof(TableTag *) * tablePtr->tagPrioMax);
	    tablePtr->tagPrios = (TableTag **) ckrealloc(
		(char *) tablePtr->tagPrios,
		sizeof(TableTag *) * tablePtr->tagPrioMax);
	    for (i = tablePtr->tagPrioSize; i < tablePtr->tagPrioMax; i++) {
		tablePtr->tagPrioNames[i] = (char *) NULL;
		tablePtr->tagPrios[i] = (TableTag *) NULL;
	    }
	}
	tablePtr->tagPrioNames[tablePtr->tagPrioSize] =
	    (char *) Tcl_GetHashKey(tablePtr->tagTable, entryPtr);
	tablePtr->tagPrios[tablePtr->tagPrioSize] = tagPtr;
	tablePtr->tagPrioSize++;
    } else {
	tagPtr = (TableTag *) Tcl_GetHashValue(entryPtr);
    }
    if (objc) {
	Tk_ConfigureWidget(tablePtr->interp, tablePtr->tkwin, tagConfig,
		objc, (CONST84 char **) argv, (char *)tagPtr,
		TK_CONFIG_ARGV_ONLY);
    }
    return tagPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TableTagGetPriority --
 *	Get the priority value for a tag.
 *
 * Results:
 *	returns the priority.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static unsigned int
TableTagGetPriority(Table *tablePtr, TableTag *tagPtr)
{
    unsigned int prio = 0;
    while (tagPtr != tablePtr->tagPrios[prio]) { prio++; }
    return prio;
}

/*
 *----------------------------------------------------------------------
 *
 * TableInitTags --
 *	Creates the static table tags.
 *
 * Results:
 *	active, sel, title and flash are created as tags.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
TableInitTags(Table *tablePtr)
{
    static CONST char *activeArgs[] = {"-bg", ACTIVE_BG, "-relief", "flat" };
    static CONST char *selArgs[]    = {"-bg", SELECT_BG, "-fg", SELECT_FG,
				       "-relief", "sunken" };
    static CONST char *titleArgs[]  = {"-bg", DISABLED, "-fg", "white",
				       "-relief", "flat",
				       "-state", "disabled" };
    static CONST char *flashArgs[]  = {"-bg", "red" };
    /*
     * The order of creation is important to priority.
     */
    TableTagGetEntry(tablePtr, "flash", ARSIZE(flashArgs), flashArgs);
    TableTagGetEntry(tablePtr, "active", ARSIZE(activeArgs), activeArgs);
    TableTagGetEntry(tablePtr, "sel", ARSIZE(selArgs), selArgs);
    TableTagGetEntry(tablePtr, "title", ARSIZE(titleArgs), titleArgs);
}

/*
 *----------------------------------------------------------------------
 *
 * FindRowColTag --
 *	Finds a row/col tag based on the row/col styles and tagCommand.
 *
 * Results:
 *	Returns tag associated with row/col cell, if any.
 *
 * Side effects:
 *	Possible side effects from eval of tagCommand.
 *	IMPORTANT: This plays with the interp result object,
 *	so use of resultPtr in prior command may be invalid after
 *	calling this function.
 *
 *----------------------------------------------------------------------
 */
TableTag *
FindRowColTag(Table *tablePtr, int cell, int mode)
{
    Tcl_HashEntry *entryPtr;
    TableTag *tagPtr = NULL;

    entryPtr = Tcl_FindHashEntry((mode == ROW) ? tablePtr->rowStyles
				 : tablePtr->colStyles, (char *) cell);
    if (entryPtr == NULL) {
	char *cmd = (mode == ROW) ? tablePtr->rowTagCmd : tablePtr->colTagCmd;
	if (cmd) {
	    register Tcl_Interp *interp = tablePtr->interp;
	    char buf[INDEX_BUFSIZE];
	    /*
	     * Since no specific row/col tag exists, eval the given command
	     * with row/col appended
	     */
	    sprintf(buf, " %d", cell);
	    Tcl_Preserve((ClientData) interp);
	    if (Tcl_VarEval(interp, cmd, buf, (char *)NULL) == TCL_OK) {
		CONST char *name = Tcl_GetStringResult(interp);
		if (name && *name) {
		    /*
		     * If a result was returned, check to see if it is
		     * a valid tag.
		     */
		    entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, name);
		}
	    }
	    Tcl_Release((ClientData) interp);
	    Tcl_ResetResult(interp);
	}
    }
    if (entryPtr != NULL) {
	/*
	 * This can be either the one in row|colStyles,
	 * or that returned by eval'ing the row|colTagCmd
	 */
	tagPtr = (TableTag *) Tcl_GetHashValue(entryPtr);
    }
    return tagPtr;
}

/* 
 *----------------------------------------------------------------------
 *
 * TableCleanupTag --
 *	Releases the resources used by a tag before it is freed up.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The tag is no longer valid.
 *
 *----------------------------------------------------------------------
 */
void
TableCleanupTag(Table *tablePtr, TableTag *tagPtr)
{
    /*
     * Free resources that the optionSpec doesn't specifically know about
     */
    if (tagPtr->image) {
	Tk_FreeImage(tagPtr->image);
    }

    Tk_FreeOptions(tagConfig, (char *) tagPtr, tablePtr->display, 0);
}

/*
 *--------------------------------------------------------------
 *
 * Table_TagCmd --
 *	This procedure is invoked to process the tag method
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
int
Table_TagCmd(ClientData clientData, register Tcl_Interp *interp,
	    int objc, Tcl_Obj *CONST objv[])
{
    register Table *tablePtr = (Table *)clientData;
    int result = TCL_OK, cmdIndex, i, newEntry, value, len;
    int row, col, tagPrio, refresh = 0;
    TableTag *tagPtr, *tag2Ptr;
    Tcl_HashEntry *entryPtr, *scanPtr;
    Tcl_HashTable *hashTblPtr;
    Tcl_HashSearch search;
    Tk_Image image;
    Tcl_Obj *objPtr, *resultPtr;
    char buf[INDEX_BUFSIZE], *keybuf, *tagname;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], tagCmdNames,
				 "tag option", 0, &cmdIndex);
    if (result != TCL_OK) {
	return result;
    }
    /*
     * Before using this object, make sure there aren't any calls that
     * could have changed the interp result, thus freeing the object.
     */
    resultPtr = Tcl_GetObjResult(interp);

    switch ((enum tagCmd) cmdIndex) {
	case TAG_CELLTAG:	/* add named tag to a (group of) cell(s) */
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "tag ?arg arg ...?");
		return TCL_ERROR;
	    }
	    tagname = Tcl_GetStringFromObj(objv[3], &len);
	    if (len == 0) {
		/*
		 * An empty string was specified, so just delete the tag.
		 */
		tagPtr = NULL;
	    } else {
		/*
		 * Get the pointer to the tag structure.  If it doesn't
		 * exist, it will be created.
		 */
		tagPtr = TableTagGetEntry(tablePtr, tagname, 0, NULL);
	    }

	    if (objc == 4) {
		/*
		 * The user just wants the cells with this tag returned.
		 * Handle specially tags named: active, flash, sel, title
		 */

		if ((tablePtr->flags & HAS_ACTIVE) &&
			STREQ(tagname, "active")) {
		    TableMakeArrayIndex(
			tablePtr->activeRow+tablePtr->rowOffset,
			tablePtr->activeCol+tablePtr->colOffset, buf);
		    Tcl_SetStringObj(resultPtr, buf, -1);
		} else if ((tablePtr->flashMode && STREQ(tagname, "flash"))
			|| STREQ(tagname, "sel")) {
		    hashTblPtr = (*tagname == 's') ?
			tablePtr->selCells : tablePtr->flashCells;
		    for (scanPtr = Tcl_FirstHashEntry(hashTblPtr, &search);
			 scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			keybuf = (char *) Tcl_GetHashKey(hashTblPtr, scanPtr);
			Tcl_ListObjAppendElement(NULL, resultPtr,
				Tcl_NewStringObj(keybuf, -1));
		    }
		} else if (STREQ(tagname, "title") &&
			(tablePtr->titleRows || tablePtr->titleCols)) {
		    for (row = tablePtr->rowOffset;
			 row < tablePtr->rowOffset+tablePtr->rows; row++) {
			for (col = tablePtr->colOffset;
			     col < tablePtr->colOffset+tablePtr->titleCols;
			     col++) {
			    TableMakeArrayIndex(row, col, buf);
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewStringObj(buf, -1));
			}
		    }
		    for (row = tablePtr->rowOffset;
			 row < tablePtr->rowOffset+tablePtr->titleRows;
			 row++) {
			for (col = tablePtr->colOffset+tablePtr->titleCols;
			     col < tablePtr->colOffset+tablePtr->cols; col++) {
			    TableMakeArrayIndex(row, col, buf);
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewStringObj(buf, -1));
			}
		    }
		} else {
		    /*
		     * Check this tag pointer amongst all tagged cells
		     */
		    for (scanPtr = Tcl_FirstHashEntry(tablePtr->cellStyles,
			    &search);
			 scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			if ((TableTag *) Tcl_GetHashValue(scanPtr) == tagPtr) {
			    keybuf = (char *) Tcl_GetHashKey(
				tablePtr->cellStyles, scanPtr);
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewStringObj(keybuf, -1));
			}
		    }
		}
		return TCL_OK;
	    }

	    /*
	     * Loop through the arguments and fill in the hash table
	     */
	    for (i = 4; i < objc; i++) {
		/*
		 * Try and parse the index
		 */
		if (TableGetIndexObj(tablePtr, objv[i], &row, &col)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		/*
		 * Get the hash key ready
		 */
		TableMakeArrayIndex(row, col, buf);

		if (tagPtr == NULL) {
		    /*
		     * This is a deletion
		     */
		    entryPtr = Tcl_FindHashEntry(tablePtr->cellStyles, buf);
		    if (entryPtr != NULL) {
			Tcl_DeleteHashEntry(entryPtr);
			refresh = 1;
		    }
		} else {
		    /*
		     * Add a key to the hash table and set it to point to the
		     * Tag structure if it wasn't the same as an existing one
		     */
		    entryPtr = Tcl_CreateHashEntry(tablePtr->cellStyles,
			    buf, &newEntry);
		    if (newEntry || (tagPtr !=
			    (TableTag *) Tcl_GetHashValue(entryPtr))) {
			Tcl_SetHashValue(entryPtr, (ClientData) tagPtr);
			refresh = 1;
		    }
		}
		/*
		 * Now invalidate this cell for redraw
		 */
		if (refresh) {
		    TableRefresh(tablePtr, row-tablePtr->rowOffset,
			    col-tablePtr->colOffset, CELL);
		}
	    }
	    return TCL_OK;

	case TAG_COLTAG:
	case TAG_ROWTAG: {	    /* tag a row or a column */
	    int forRows = (cmdIndex == TAG_ROWTAG);

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "tag ?arg arg ..?");
		return TCL_ERROR;
	    }
	    tagname = Tcl_GetStringFromObj(objv[3], &len);
	    if (len == 0) {
		/*
		 * Empty string, so we want to delete this element
		 */
		tagPtr = NULL;
	    } else {
		/*
		 * Get the pointer to the tag structure.  If it doesn't
		 * exist, it will be created.
		 */
		tagPtr = TableTagGetEntry(tablePtr, tagname, 0, NULL);
	    }

	    /*
	     * Choose the correct hash table based on args
	     */
	    hashTblPtr = forRows ? tablePtr->rowStyles : tablePtr->colStyles;

	    if (objc == 4) {
		/* the user just wants the tagged cells to be returned */
		/* Special handling for tags: active, flash, sel, title */

		if ((tablePtr->flags & HAS_ACTIVE) &&
			strcmp(tagname, "active") == 0) {
		    Tcl_SetIntObj(resultPtr,
			    (forRows ?
				    tablePtr->activeRow+tablePtr->rowOffset :
				    tablePtr->activeCol+tablePtr->colOffset));
		} else if ((tablePtr->flashMode && STREQ(tagname, "flash"))
			|| STREQ(tagname, "sel")) {
		    Tcl_HashTable *cacheTblPtr;

		    cacheTblPtr = (Tcl_HashTable *)
			ckalloc(sizeof(Tcl_HashTable));
		    Tcl_InitHashTable(cacheTblPtr, TCL_ONE_WORD_KEYS);

		    hashTblPtr = (*tagname == 's') ?
			tablePtr->selCells : tablePtr->flashCells;
		    for (scanPtr = Tcl_FirstHashEntry(hashTblPtr, &search);
			 scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			TableParseArrayIndex(&row, &col,
				Tcl_GetHashKey(hashTblPtr, scanPtr));
			value = forRows ? row : col;
			Tcl_CreateHashEntry(cacheTblPtr,
				(char *)value, &newEntry);
			if (newEntry) {
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewIntObj(value));
			}
		    }

		    Tcl_DeleteHashTable(cacheTblPtr);
		    ckfree((char *) (cacheTblPtr));
		} else if (STREQ(tagname, "title") &&
			(forRows?tablePtr->titleRows:tablePtr->titleCols)) {
		    if (forRows) {
			for (row = tablePtr->rowOffset;
			     row < tablePtr->rowOffset+tablePtr->titleRows;
			     row++) {
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewIntObj(row));
			}
		    } else {
			for (col = tablePtr->colOffset;
			     col < tablePtr->colOffset+tablePtr->titleCols;
			     col++) {
			    Tcl_ListObjAppendElement(NULL, resultPtr,
				    Tcl_NewIntObj(col));
			}
		    }
		} else {
		    for (scanPtr = Tcl_FirstHashEntry(hashTblPtr, &search);
			 scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			/* is this the tag pointer on this row */
			if ((TableTag *) Tcl_GetHashValue(scanPtr) == tagPtr) {
			    objPtr = Tcl_NewIntObj(
				(int) Tcl_GetHashKey(hashTblPtr, scanPtr));
			    Tcl_ListObjAppendElement(NULL, resultPtr, objPtr);
			}
		    }
		}
		return TCL_OK;
	    }

	    /*
	     * Loop through the arguments and fill in the hash table
	     */
	    for (i = 4; i < objc; i++) {
		/*
		 * Try and parse the index
		 */
		if (Tcl_GetIntFromObj(interp, objv[i], &value) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tagPtr == NULL) {
		    /*
		     * This is a deletion
		     */
		    entryPtr = Tcl_FindHashEntry(hashTblPtr, (char *)value);
		    if (entryPtr != NULL) {
			Tcl_DeleteHashEntry(entryPtr);
			refresh = 1;
		    }
		} else {
		    /*
		     * Add a key to the hash table and set it to point to the
		     * Tag structure if it wasn't the same as an existing one
		     */
		    entryPtr = Tcl_CreateHashEntry(hashTblPtr,
			    (char *) value, &newEntry);
		    if (newEntry || (tagPtr !=
			    (TableTag *) Tcl_GetHashValue(entryPtr))) {
			Tcl_SetHashValue(entryPtr, (ClientData) tagPtr);
			refresh = 1;
		    }
		}
		/* and invalidate the row or column affected */
		if (refresh) {
		    if (cmdIndex == TAG_ROWTAG) {
			TableRefresh(tablePtr, value-tablePtr->rowOffset, 0,
				ROW);
		    } else {
			TableRefresh(tablePtr, 0, value-tablePtr->colOffset,
				COL);
		    }
		}
	    }
	    return TCL_OK;	/* COLTAG && ROWTAG */
	}

	case TAG_CGET:
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "tagName option");
		return TCL_ERROR;
	    }
	    tagname  = Tcl_GetString(objv[3]);
	    entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, tagname);
	    if (entryPtr == NULL) {
		goto invalidtag;
	    } else {
		tagPtr = (TableTag *) Tcl_GetHashValue (entryPtr);
		result = Tk_ConfigureValue(interp, tablePtr->tkwin, tagConfig,
			(char *) tagPtr, Tcl_GetString(objv[4]), 0);
	    }
	    return result;	/* CGET */

	case TAG_CONFIGURE:
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "tagName ?arg arg  ...?");
		return TCL_ERROR;
	    }

	    /*
	     * Get the pointer to the tag structure.  If it doesn't
	     * exist, it will be created.
	     */
	    tagPtr = TableTagGetEntry(tablePtr, Tcl_GetString(objv[3]),
		    0, NULL);

	    /* 
	     * If there were less than 6 args, we return the configuration
	     * (for all or just one option), even for new tags
	     */
	    if (objc < 6) {
		result = Tk_ConfigureInfo(interp, tablePtr->tkwin, tagConfig,
			(char *) tagPtr, (objc == 5) ?
			Tcl_GetString(objv[4]) : NULL, 0);
	    } else {
		CONST84 char **argv;

		/* Stringify */
		argv = (CONST84 char **) ckalloc((objc + 1) * sizeof(char *));
		for (i = 0; i < objc; i++)
		    argv[i] = Tcl_GetString(objv[i]);
		argv[objc] = NULL;

		result = Tk_ConfigureWidget(interp, tablePtr->tkwin,
			tagConfig, objc-4, argv+4, (char *) tagPtr,
			TK_CONFIG_ARGV_ONLY);
		ckfree((char *) argv);
		if (result == TCL_ERROR) {
		    return TCL_ERROR;
		}

		/*
		 * Handle change of image name
		 */
		if (tagPtr->imageStr) {
		    image = Tk_GetImage(interp, tablePtr->tkwin,
			    tagPtr->imageStr,
			    TableImageProc, (ClientData)tablePtr);
		    if (image == NULL) {
			result = TCL_ERROR;
		    }
		} else {
		    image = NULL;
		}
		if (tagPtr->image) {
		    Tk_FreeImage(tagPtr->image);
		}
		tagPtr->image = image;

		/*
		 * We reconfigured, so invalidate the table to redraw
		 */
		TableInvalidateAll(tablePtr, 0);
	    }
	    return result;

	case TAG_DELETE:
	    /* delete a tag */
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "tagName ?tagName ...?");
		return TCL_ERROR;
	    }
	    /* run through the remaining arguments */
	    for (i = 3; i < objc; i++) {
		tagname  = Tcl_GetString(objv[i]);
		/* cannot delete the title tag */
		if (STREQ(tagname, "title") ||
			STREQ(tagname, "sel") ||
			STREQ(tagname, "flash") ||
			STREQ(tagname, "active")) {
		    Tcl_AppendStringsToObj(resultPtr, "cannot delete ",
			    tagname, " tag", (char *) NULL);
		    return TCL_ERROR;
		}
		entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, tagname);
		if (entryPtr != NULL) {
		    /* get the tag pointer */
		    tagPtr = (TableTag *) Tcl_GetHashValue(entryPtr);

		    /* delete all references to this tag in rows */
		    scanPtr = Tcl_FirstHashEntry(tablePtr->rowStyles, &search);
		    for (; scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			if ((TableTag *)Tcl_GetHashValue(scanPtr) == tagPtr) {
			    Tcl_DeleteHashEntry(scanPtr);
			    refresh = 1;
			}
		    }

		    /* delete all references to this tag in cols */
		    scanPtr = Tcl_FirstHashEntry(tablePtr->colStyles, &search);
		    for (; scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			if ((TableTag *)Tcl_GetHashValue(scanPtr) == tagPtr) {
			    Tcl_DeleteHashEntry(scanPtr);
			    refresh = 1;
			}
		    }

		    /* delete all references to this tag in cells */
		    scanPtr = Tcl_FirstHashEntry(tablePtr->cellStyles,
			    &search);
		    for (; scanPtr != NULL;
			 scanPtr = Tcl_NextHashEntry(&search)) {
			if ((TableTag *)Tcl_GetHashValue(scanPtr) == tagPtr) {
			    Tcl_DeleteHashEntry(scanPtr);
			    refresh = 1;
			}
		    }

		    /*
		     * Remove the tag from the prio list and collapse
		     * the rest of the tags.  We could check for shrinking
		     * the prio list as well.
		     */
		    for (i = 0; i < tablePtr->tagPrioSize; i++) {
			if (tablePtr->tagPrios[i] == tagPtr) break;
		    }
		    for ( ; i < tablePtr->tagPrioSize; i++) {
			tablePtr->tagPrioNames[i] =
			    tablePtr->tagPrioNames[i+1];
			tablePtr->tagPrios[i] = tablePtr->tagPrios[i+1];
		    }
		    tablePtr->tagPrioSize--;

		    /* Release the tag structure */
		    TableCleanupTag(tablePtr, tagPtr);
		    ckfree((char *) tagPtr);

		    /* And free the hash table entry */
		    Tcl_DeleteHashEntry(entryPtr);
		}
	    }
	    /* since we deleted a tag, redraw the screen */
	    if (refresh) {
		TableInvalidateAll(tablePtr, 0);
	    }
	    return result;

	case TAG_EXISTS:
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "tagName");
		return TCL_ERROR;
	    }
	    Tcl_SetBooleanObj(resultPtr,
		    (Tcl_FindHashEntry(tablePtr->tagTable,
			    Tcl_GetString(objv[3])) != NULL));
	    return TCL_OK;

	case TAG_INCLUDES:
	    /* does a tag contain a index ? */
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "tag index");
		return TCL_ERROR;
	    }
	    tagname  = Tcl_GetString(objv[3]);
	    /* check to see if the tag actually exists */
	    entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, tagname);
	    if (entryPtr == NULL) {
		/* Unknown tag, just return 0 */
		Tcl_SetBooleanObj(resultPtr, 0);
		return TCL_OK;
	    }
	    /* parse index */
	    if (TableGetIndexObj(tablePtr, objv[4], &row, &col) != TCL_OK) {
		return TCL_ERROR;
	    }
	    /* create hash key */
	    TableMakeArrayIndex(row, col, buf);
    
	    if (STREQ(tagname, "active")) {
		result = (tablePtr->activeRow+tablePtr->rowOffset==row &&
			tablePtr->activeCol+tablePtr->colOffset==col);
	    } else if (STREQ(tagname, "flash")) {
		result = (tablePtr->flashMode &&
			(Tcl_FindHashEntry(tablePtr->flashCells, buf)
				!= NULL));
	    } else if (STREQ(tagname, "sel")) {
		result = (Tcl_FindHashEntry(tablePtr->selCells, buf) != NULL);
	    } else if (STREQ(tagname, "title")) {
		result = (row < tablePtr->titleRows+tablePtr->rowOffset ||
			col < tablePtr->titleCols+tablePtr->colOffset);
	    } else {
		/* get the pointer to the tag structure */
		tagPtr = (TableTag *) Tcl_GetHashValue(entryPtr);
		scanPtr = Tcl_FindHashEntry(tablePtr->cellStyles, buf);
		/*
		 * Look to see if there is a cell, row, or col tag
		 * for this cell
		 */
		result = ((scanPtr &&
			(tagPtr == (TableTag *) Tcl_GetHashValue(scanPtr))) ||
			(tagPtr == FindRowColTag(tablePtr, row, ROW)) ||
			(tagPtr == FindRowColTag(tablePtr, col, COL)));
	    }
	    /*
	     * Because we may call FindRowColTag above, we can't use
	     * the resultPtr, but this is almost equivalent, and is SAFE
	     */
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
	    return TCL_OK;

	case TAG_NAMES:
	    /*
	     * Print out the tag names in priority order
	     */
	    if (objc < 3 || objc > 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?pattern?");
		return TCL_ERROR;
	    }
	    tagname = (objc == 4) ? Tcl_GetString(objv[3]) : NULL;
	    for (i = 0; i < tablePtr->tagPrioSize; i++) {
		keybuf = tablePtr->tagPrioNames[i];
		if (objc == 3 || Tcl_StringMatch(keybuf, tagname)) {
		    objPtr = Tcl_NewStringObj(keybuf, -1);
		    Tcl_ListObjAppendElement(NULL, resultPtr, objPtr);
		}
	    }
	    return TCL_OK;

	case TAG_LOWER:
	case TAG_RAISE:
	    /*
	     * Change priority of the named tag
	     */
	    if (objc != 4 && objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, (cmdIndex == TAG_LOWER) ?
			"tagName ?belowThis?" : "tagName ?aboveThis?");
		return TCL_ERROR;
	    }
	    tagname  = Tcl_GetString(objv[3]);
	    /* check to see if the tag actually exists */
	    entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, tagname);
	    if (entryPtr == NULL) {
		goto invalidtag;
	    }
	    tagPtr  = (TableTag *) Tcl_GetHashValue(entryPtr);
	    tagPrio = TableTagGetPriority(tablePtr, tagPtr);
	    keybuf  = tablePtr->tagPrioNames[tagPrio];
	    /*
	     * In the RAISE case, the priority is one higher (-1) because
	     * we want the named tag to move above the other in priority.
	     */
	    if (objc == 5) {
		tagname  = Tcl_GetString(objv[4]);
		entryPtr = Tcl_FindHashEntry(tablePtr->tagTable, tagname);
		if (entryPtr == NULL) {
		    goto invalidtag;
		}
		tag2Ptr  = (TableTag *) Tcl_GetHashValue(entryPtr);
		if (cmdIndex == TAG_LOWER) {
		    value = TableTagGetPriority(tablePtr, tag2Ptr);
		} else {
		    value = TableTagGetPriority(tablePtr, tag2Ptr) - 1;
		}
	    } else {
		if (cmdIndex == TAG_LOWER) {
		    /*
		     * Lower this tag's priority to the bottom.
		     */
		    value = tablePtr->tagPrioSize - 1;
		} else {
		    /*
		     * Raise this tag's priority to the top.
		     */
		    value = -1;
		}
	    }
	    if (value < tagPrio) {
		/*
		 * Move tag up in priority.
		 */
		for (i = tagPrio; i > value; i--) {
		    tablePtr->tagPrioNames[i] = tablePtr->tagPrioNames[i-1];
		    tablePtr->tagPrios[i]     = tablePtr->tagPrios[i-1];
		}
		i++;
		tablePtr->tagPrioNames[i] = keybuf;
		tablePtr->tagPrios[i]     = tagPtr;
		refresh = 1;
	    } else if (value > tagPrio) {
		/*
		 * Move tag down in priority.
		 */
		for (i = tagPrio; i < value; i++) {
		    tablePtr->tagPrioNames[i] = tablePtr->tagPrioNames[i+1];
		    tablePtr->tagPrios[i]     = tablePtr->tagPrios[i+1];
		}
		tablePtr->tagPrioNames[i] = keybuf;
		tablePtr->tagPrios[i]     = tagPtr;
		refresh = 1;
	    }
	    /* since we deleted a tag, redraw the screen */
	    if (refresh) {
		TableInvalidateAll(tablePtr, 0);
	    }
	    return TCL_OK;

    }
    return TCL_OK;

    invalidtag:
    /*
     * When jumping here, ensure the invalid 'tagname' is set already.
     */
    Tcl_AppendStringsToObj(resultPtr, "invalid tag name \"",
	    tagname, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TableOptionReliefSet --
 *
 *	This routine configures the borderwidth value for a tag.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	It may adjust the tag struct values of relief[0..4] and borders.
 *
 *----------------------------------------------------------------------
 */

static int
TableOptionReliefSet(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Type of struct being set. */
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing table widget. */
    CONST84 char *value;		/* Value of option. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
{
    TableTag *tagPtr = (TableTag *) widgRec;

    if (*value == '\0') {
	tagPtr->relief = -1;
    } else {
	return Tk_GetRelief(interp, value, &(tagPtr->relief));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TableOptionReliefGet --
 *
 * Results:
 *	Value of the tag's -relief option.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
TableOptionReliefGet(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Type of struct being set. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    return (char *) Tk_NameOfRelief(((TableTag *) widgRec)->relief);
}
