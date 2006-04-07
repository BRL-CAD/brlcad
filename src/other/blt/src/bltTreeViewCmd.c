
/*
 * bltTreeViewCmd.c --
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
#include "bltList.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <ctype.h>

#define CLAMP(val,low,hi)	\
	(((val) < (low)) ? (low) : ((val) > (hi)) ? (hi) : (val))

static TreeViewCompareProc ExactCompare, GlobCompare, RegexpCompare;
static TreeViewApplyProc ShowEntryApplyProc, HideEntryApplyProc, 
	MapAncestorsApplyProc, FixSelectionsApplyProc;
static Tk_LostSelProc LostSelection;
static TreeViewApplyProc SelectEntryApplyProc;

extern Blt_CustomOption bltTreeViewIconsOption;
extern Blt_CustomOption bltTreeViewUidOption;
extern Blt_CustomOption bltTreeViewTreeOption;

extern Blt_ConfigSpec bltTreeViewButtonSpecs[];
extern Blt_ConfigSpec bltTreeViewSpecs[];
extern Blt_ConfigSpec bltTreeViewEntrySpecs[];

#define TAG_UNKNOWN	 (1<<0)
#define TAG_RESERVED	 (1<<1)
#define TAG_USER_DEFINED (1<<2)

#define TAG_SINGLE	(1<<3)
#define TAG_MULTIPLE	(1<<4)
#define TAG_ALL		(1<<5)

/*
 *----------------------------------------------------------------------
 *
 * SkipSeparators --
 *
 *	Moves the character pointer past one of more separators.
 *
 * Results:
 *	Returns the updates character pointer.
 *
 *----------------------------------------------------------------------
 */
static char *
SkipSeparators(path, separator, length)
    char *path, *separator;
    int length;
{
    while ((path[0] == separator[0]) && 
	   (strncmp(path, separator, length) == 0)) {
	path += length;
    }
    return path;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteNode --
 *
 *	Delete the node and its descendants.  Don't remove the root
 *	node, though.  If the root node is specified, simply remove
 *	all its children.
 *
 *---------------------------------------------------------------------- 
 */
static void
DeleteNode(tvPtr, node)
    TreeView *tvPtr;
    Blt_TreeNode node;
{
    Blt_TreeNode root;

    if (!Blt_TreeTagTableIsShared(tvPtr->tree)) {
	Blt_TreeClearTags(tvPtr->tree, node);
    }
    root = Blt_TreeRootNode(tvPtr->tree);
    if (node == root) {
	Blt_TreeNode next;
	/* Don't delete the root node. Simply clean out the tree. */
	for (node = Blt_TreeFirstChild(node); node != NULL; node = next) {
	    next = Blt_TreeNextSibling(node);
	    Blt_TreeDeleteNode(tvPtr->tree, node);
	}	    
    } else if (Blt_TreeIsAncestor(root, node)) {
	Blt_TreeDeleteNode(tvPtr->tree, node);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SplitPath --
 *
 *	Returns the trailing component of the given path.  Trailing
 *	separators are ignored.
 *
 * Results:
 *	Returns the string of the tail component.
 *
 *----------------------------------------------------------------------
 */
static int
SplitPath(tvPtr, path, depthPtr, compPtrPtr)
    TreeView *tvPtr;
    char *path;
    int *depthPtr;
    char ***compPtrPtr;
{
    int skipLen, pathLen;
    int depth, listSize;
    char **components;
    register char *p;
    char *sep;

    if (tvPtr->pathSep == SEPARATOR_LIST) {
	if (Tcl_SplitList(tvPtr->interp, path, depthPtr, compPtrPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    pathLen = strlen(path);

    skipLen = strlen(tvPtr->pathSep);
    path = SkipSeparators(path, tvPtr->pathSep, skipLen);
    depth = pathLen / skipLen;

    listSize = (depth + 1) * sizeof(char *);
    components = Blt_Malloc(listSize + (pathLen + 1));
    assert(components);
    p = (char *)components + listSize;
    strcpy(p, path);

    sep = strstr(p, tvPtr->pathSep);
    depth = 0;
    while ((*p != '\0') && (sep != NULL)) {
	*sep = '\0';
	components[depth++] = p;
	p = SkipSeparators(sep + skipLen, tvPtr->pathSep, skipLen);
	sep = strstr(p, tvPtr->pathSep);
    }
    if (*p != '\0') {
	components[depth++] = p;
    }
    components[depth] = NULL;
    *depthPtr = depth;
    *compPtrPtr = components;
    return TCL_OK;
}


static TreeViewEntry *
LastEntry(tvPtr, entryPtr, mask)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    unsigned int mask;
{
    Blt_TreeNode next;
    TreeViewEntry *nextPtr;

    next = Blt_TreeLastChild(entryPtr->node);
    while (next != NULL) {
	nextPtr = Blt_NodeToEntry(tvPtr, next);
	if ((nextPtr->flags & mask) != mask) {
	    break;
	}
	entryPtr = nextPtr;
	next = Blt_TreeLastChild(next);
    }
    return entryPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * ShowEntryApplyProc --
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ShowEntryApplyProc(tvPtr, entryPtr)
    TreeView *tvPtr;		/* Not used. */
    TreeViewEntry *entryPtr;
{
    entryPtr->flags &= ~ENTRY_HIDDEN;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * HideEntryApplyProc --
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
HideEntryApplyProc(tvPtr, entryPtr)
    TreeView *tvPtr;		/* Not used. */
    TreeViewEntry *entryPtr;
{
    entryPtr->flags |= ENTRY_HIDDEN;
    return TCL_OK;
}

static void
MapAncestors(tvPtr, entryPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
{
    while (entryPtr != tvPtr->rootPtr) {
	entryPtr = Blt_TreeViewParentEntry(entryPtr);
	if (entryPtr->flags & (ENTRY_CLOSED | ENTRY_HIDDEN)) {
	    tvPtr->flags |= TV_LAYOUT;
	    entryPtr->flags &= ~(ENTRY_CLOSED | ENTRY_HIDDEN);
	} 
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MapAncestorsApplyProc --
 *
 *	If a node in mapped, then all its ancestors must be mapped also.
 *	This routine traverses upwards and maps each unmapped ancestor.
 *	It's assumed that for any mapped ancestor, all it's ancestors
 *	will already be mapped too.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
MapAncestorsApplyProc(tvPtr, entryPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
{
    /*
     * Make sure that all the ancestors of this entry are mapped too.
     */
    while (entryPtr != tvPtr->rootPtr) {
	entryPtr = Blt_TreeViewParentEntry(entryPtr);
	if ((entryPtr->flags & (ENTRY_HIDDEN | ENTRY_CLOSED)) == 0) {
	    break;		/* Assume ancestors are also mapped. */
	}
	entryPtr->flags &= ~(ENTRY_HIDDEN | ENTRY_CLOSED);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindPath --
 *
 *	Finds the node designated by the given path.  Each path
 *	component is searched for as the tree is traversed.
 *
 *	A leading character string is trimmed off the path if it
 *	matches the one designated (see the -trimleft option).
 *
 *	If no separator is designated (see the -separator
 *	configuration option), the path is considered a Tcl list.
 *	Otherwise the each component of the path is separated by a
 *	character string.  Leading and trailing separators are
 *	ignored.  Multiple separators are treated as one.
 *
 * Results:
 *	Returns the pointer to the designated node.  If any component
 *	can't be found, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
static TreeViewEntry *
FindPath(tvPtr, rootPtr, path)
    TreeView *tvPtr;
    TreeViewEntry *rootPtr;
    char *path;
{
    Blt_TreeNode child;
    char **compArr;
    char *name;
    int nComp;
    register char **p;
    TreeViewEntry *entryPtr;

    /* Trim off characters that we don't want */
    if (tvPtr->trimLeft != NULL) {
	register char *s1, *s2;
	
	/* Trim off leading character string if one exists. */
	for (s1 = path, s2 = tvPtr->trimLeft; *s2 != '\0'; s2++, s1++) {
	    if (*s1 != *s2) {
		break;
	    }
	}
	if (*s2 == '\0') {
	    path = s1;
	}
    }
    if (*path == '\0') {
	return rootPtr;
    }
    name = path;
    entryPtr = rootPtr;
    if (tvPtr->pathSep == SEPARATOR_NONE) {
	child = Blt_TreeFindChild(entryPtr->node, name);
	if (child == NULL) {
	    goto error;
	}
	return Blt_NodeToEntry(tvPtr, child);
    }

    if (SplitPath(tvPtr, path, &nComp, &compArr) != TCL_OK) {
	return NULL;
    }
    for (p = compArr; *p != NULL; p++) {
	name = *p;
	child = Blt_TreeFindChild(entryPtr->node, name);
	if (child == NULL) {
	    Blt_Free(compArr);
	    goto error;
	}
	entryPtr = Blt_NodeToEntry(tvPtr, child);
    }
    Blt_Free(compArr);
    return entryPtr;
 error:
    {
	Tcl_DString dString;

	Blt_TreeViewGetFullName(tvPtr, entryPtr, FALSE, &dString);
	Tcl_AppendResult(tvPtr->interp, "can't find node \"", name,
		 "\" in parent node \"", Tcl_DStringValue(&dString), "\"", 
		(char *)NULL);
	Tcl_DStringFree(&dString);
    }
    return NULL;

}

/*
 *----------------------------------------------------------------------
 *
 * NodeToObj --
 *
 *	Converts a node pointer to a string representation.
 *	The string contains the node's index which is unique.
 *
 * Results:
 *	The string representation of the node is returned.  Note that
 *	the string is stored statically, so that callers must save the
 *	string before the next call to this routine overwrites the
 *	static array again.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *
NodeToObj(node)
    Blt_TreeNode node;
{
    char string[200];

    sprintf(string, "%d", Blt_TreeNodeId(node));
    return Tcl_NewStringObj(string, -1);
}


static int
GetEntryFromSpecialId(tvPtr, string, entryPtrPtr)
    TreeView *tvPtr;
    char *string;
    TreeViewEntry **entryPtrPtr;
{
    Blt_TreeNode node;
    TreeViewEntry *fromPtr, *entryPtr;
    char c;

    entryPtr = NULL;
    fromPtr = tvPtr->fromPtr;
    if (fromPtr == NULL) {
	fromPtr = tvPtr->focusPtr;
    } 
    if (fromPtr == NULL) {
	fromPtr = tvPtr->rootPtr;
    }
    c = string[0];
    if (c == '@') {
	int x, y;

	if (Blt_GetXY(tvPtr->interp, tvPtr->tkwin, string, &x, &y) == TCL_OK) {
	    *entryPtrPtr = Blt_TreeViewNearestEntry(tvPtr, x, y, TRUE);
	}
    } else if ((c == 'b') && (strcmp(string, "bottom") == 0)) {
	if (tvPtr->flatView) {
	    entryPtr = tvPtr->flatArr[tvPtr->nEntries - 1];
	} else {
	    entryPtr = LastEntry(tvPtr, tvPtr->rootPtr, ENTRY_MASK);
	}
    } else if ((c == 't') && (strcmp(string, "top") == 0)) {
	if (tvPtr->flatView) {
	    entryPtr = tvPtr->flatArr[0];
	} else {
	    entryPtr = tvPtr->rootPtr;
	    if (tvPtr->flags & TV_HIDE_ROOT) {
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK);
	    }
	}
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	entryPtr = LastEntry(tvPtr, tvPtr->rootPtr, ENTRY_MASK);
    } else if ((c == 'a') && (strcmp(string, "anchor") == 0)) {
	entryPtr = tvPtr->selAnchorPtr;
    } else if ((c == 'f') && (strcmp(string, "focus") == 0)) {
	entryPtr = tvPtr->focusPtr;
	if ((entryPtr == tvPtr->rootPtr) && (tvPtr->flags & TV_HIDE_ROOT)) {
	    entryPtr = Blt_TreeViewNextEntry(tvPtr->rootPtr, ENTRY_MASK);
	}
    } else if ((c == 'r') && (strcmp(string, "root") == 0)) {
	entryPtr = tvPtr->rootPtr;
    } else if ((c == 'p') && (strcmp(string, "parent") == 0)) {
	if (fromPtr != tvPtr->rootPtr) {
	    entryPtr = Blt_TreeViewParentEntry(fromPtr);
	}
    } else if ((c == 'c') && (strcmp(string, "current") == 0)) {
	/* Can't trust picked item, if entries have been 
	 * added or deleted. */
	if (!(tvPtr->flags & TV_DIRTY)) {
	    ClientData context;

	    context = Blt_GetCurrentContext(tvPtr->bindTable);
	    if ((context == ITEM_ENTRY) || 
		(context == ITEM_ENTRY_BUTTON) ||
		(context >= ITEM_STYLE)) {
		entryPtr = Blt_GetCurrentItem(tvPtr->bindTable);
	    }
	}
    } else if ((c == 'u') && (strcmp(string, "up") == 0)) {
	entryPtr = fromPtr;
	if (tvPtr->flatView) {
	    int i;

	    i = entryPtr->flatIndex - 1;
	    if (i >= 0) {
		entryPtr = tvPtr->flatArr[i];
	    }
	} else {
	    entryPtr = Blt_TreeViewPrevEntry(fromPtr, ENTRY_MASK);
	    if (entryPtr == NULL) {
		entryPtr = fromPtr;
	    }
	    if ((entryPtr == tvPtr->rootPtr) && 
		(tvPtr->flags & TV_HIDE_ROOT)) {
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK);
	    }
	}
    } else if ((c == 'd') && (strcmp(string, "down") == 0)) {
	entryPtr = fromPtr;
	if (tvPtr->flatView) {
	    int i;

	    i = entryPtr->flatIndex + 1;
	    if (i < tvPtr->nEntries) {
		entryPtr = tvPtr->flatArr[i];
	    }
	} else {
	    entryPtr = Blt_TreeViewNextEntry(fromPtr, ENTRY_MASK);
	    if (entryPtr == NULL) {
		entryPtr = fromPtr;
	    }
	    if ((entryPtr == tvPtr->rootPtr) && 
		(tvPtr->flags & TV_HIDE_ROOT)) {
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK);
	    }
	}
    } else if (((c == 'l') && (strcmp(string, "last") == 0)) ||
	       ((c == 'p') && (strcmp(string, "prev") == 0))) {
	entryPtr = fromPtr;
	if (tvPtr->flatView) {
	    int i;

	    i = entryPtr->flatIndex - 1;
	    if (i < 0) {
		i = tvPtr->nEntries - 1;
	    }
	    entryPtr = tvPtr->flatArr[i];
	} else {
	    entryPtr = Blt_TreeViewPrevEntry(fromPtr, ENTRY_MASK);
	    if (entryPtr == NULL) {
		entryPtr = LastEntry(tvPtr, tvPtr->rootPtr, ENTRY_MASK);
	    }
	    if ((entryPtr == tvPtr->rootPtr) && 
		(tvPtr->flags & TV_HIDE_ROOT)) {
		entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK);
	    }
	}
    } else if ((c == 'n') && (strcmp(string, "next") == 0)) {
	entryPtr = fromPtr;
	if (tvPtr->flatView) {
	    int i;

	    i = entryPtr->flatIndex + 1; 
	    if (i >= tvPtr->nEntries) {
		i = 0;
	    }
	    entryPtr = tvPtr->flatArr[i];
	} else {
	    entryPtr = Blt_TreeViewNextEntry(fromPtr, ENTRY_MASK);
	    if (entryPtr == NULL) {
		if (tvPtr->flags & TV_HIDE_ROOT) {
		    entryPtr = Blt_TreeViewNextEntry(tvPtr->rootPtr,ENTRY_MASK);
		} else {
		    entryPtr = tvPtr->rootPtr;
		}
	    }
	}
    } else if ((c == 'n') && (strcmp(string, "nextsibling") == 0)) {
	node = Blt_TreeNextSibling(fromPtr->node);
	if (node != NULL) {
	    entryPtr = Blt_NodeToEntry(tvPtr, node);
	}
    } else if ((c == 'p') && (strcmp(string, "prevsibling") == 0)) {
	node = Blt_TreePrevSibling(fromPtr->node);
	if (node != NULL) {
	    entryPtr = Blt_NodeToEntry(tvPtr, node);
	}
    } else if ((c == 'v') && (strcmp(string, "view.top") == 0)) {
	if (tvPtr->nVisible > 0) {
	    entryPtr = tvPtr->visibleArr[0];
	}
    } else if ((c == 'v') && (strcmp(string, "view.bottom") == 0)) {
	if (tvPtr->nVisible > 0) {
	    entryPtr = tvPtr->visibleArr[tvPtr->nVisible - 1];
	} 
    } else {
	return TCL_ERROR;
    }
    *entryPtrPtr = entryPtr;
    return TCL_OK;
}

static int
GetTagInfo(tvPtr, tagName, infoPtr)
    TreeView *tvPtr;
    char *tagName;
    TreeViewTagInfo *infoPtr;
{
    
    infoPtr->tagType = TAG_RESERVED | TAG_SINGLE;
    infoPtr->entryPtr = NULL;

    if (strcmp(tagName, "all") == 0) {
	infoPtr->entryPtr = tvPtr->rootPtr;
	infoPtr->tagType |= TAG_ALL;
    } else {
	Blt_HashTable *tablePtr;

	tablePtr = Blt_TreeTagHashTable(tvPtr->tree, tagName);
	if (tablePtr != NULL) {
	    Blt_HashEntry *hPtr;
	    
	    infoPtr->tagType = TAG_USER_DEFINED; /* Empty tags are not
						  * an error. */
	    hPtr = Blt_FirstHashEntry(tablePtr, &infoPtr->cursor); 
	    if (hPtr != NULL) {
		Blt_TreeNode node;

		node = Blt_GetHashValue(hPtr);
		infoPtr->entryPtr = Blt_NodeToEntry(tvPtr, node);
		if (tablePtr->numEntries > 1) {
		    infoPtr->tagType |= TAG_MULTIPLE;
		}
	    }
	}  else {
	    infoPtr->tagType = TAG_UNKNOWN;
	    Tcl_AppendResult(tvPtr->interp, "can't find tag or id \"", tagName, 
		"\" in \"", Tk_PathName(tvPtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*ARGSUSED*/
void
Blt_TreeViewGetTags(interp, tvPtr, entryPtr, list)
    Tcl_Interp *interp;		/* Not used. */
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    Blt_List list;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_TreeTagEntry *tPtr;

    for (hPtr = Blt_TreeFirstTag(tvPtr->tree, &cursor); hPtr != NULL; 
	hPtr = Blt_NextHashEntry(&cursor)) {
	tPtr = Blt_GetHashValue(hPtr);
	hPtr = Blt_FindHashEntry(&tPtr->nodeTable, (char *)entryPtr->node);
	if (hPtr != NULL) {
	    Blt_ListAppend(list, Blt_TreeViewGetUid(tvPtr, tPtr->tagName),0);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AddTag --
 *
 *---------------------------------------------------------------------- 
 */
static int
AddTag(tvPtr, node, tagName)
    TreeView *tvPtr;
    Blt_TreeNode node;
    char *tagName;
{
    TreeViewEntry *entryPtr;

    if (strcmp(tagName, "root") == 0) {
	Tcl_AppendResult(tvPtr->interp, "can't add reserved tag \"",
			 tagName, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (isdigit(UCHAR(tagName[0]))) {
	Tcl_AppendResult(tvPtr->interp, "invalid tag \"", tagName, 
		"\": can't start with digit", (char *)NULL);
	return TCL_ERROR;
    } 
    if (isdigit(UCHAR(tagName[0]))) {
	Tcl_AppendResult(tvPtr->interp, "invalid tag \"", tagName, 
		"\": can't start with digit", (char *)NULL);
	return TCL_ERROR;
    } 
    if (tagName[0] == '@') {
	Tcl_AppendResult(tvPtr->interp, "invalid tag \"", tagName, 
		"\": can't start with \"@\"", (char *)NULL);
	return TCL_ERROR;
    } 
    tvPtr->fromPtr = NULL;
    if (GetEntryFromSpecialId(tvPtr, tagName, &entryPtr) == TCL_OK) {
	Tcl_AppendResult(tvPtr->interp, "invalid tag \"", tagName, 
		"\": is a special id", (char *)NULL);
	return TCL_ERROR;
    }
    /* Add the tag to the node. */
    Blt_TreeAddTag(tvPtr->tree, node, tagName);
    return TCL_OK;
}
    
TreeViewEntry *
Blt_TreeViewFirstTaggedEntry(infoPtr)	
    TreeViewTagInfo *infoPtr;
{
    return infoPtr->entryPtr;
}

int
Blt_TreeViewFindTaggedEntries(tvPtr, objPtr, infoPtr)	
    TreeView *tvPtr;
    Tcl_Obj *objPtr;
    TreeViewTagInfo *infoPtr;
{
    char *tagName;
    TreeViewEntry *entryPtr;

    tagName = Tcl_GetString(objPtr); 
    tvPtr->fromPtr = NULL;
    if (isdigit(UCHAR(tagName[0]))) {
	int inode;
	Blt_TreeNode node;

	if (Tcl_GetIntFromObj(tvPtr->interp, objPtr, &inode) != TCL_OK) {
	    return TCL_ERROR;
	}
	node = Blt_TreeGetNode(tvPtr->tree, inode);
	infoPtr->entryPtr = Blt_NodeToEntry(tvPtr, node);
	infoPtr->tagType = (TAG_RESERVED | TAG_SINGLE);
    } else if (GetEntryFromSpecialId(tvPtr, tagName, &entryPtr) == TCL_OK) {
	infoPtr->entryPtr = entryPtr;
	infoPtr->tagType = (TAG_RESERVED | TAG_SINGLE);
    } else {
	if (GetTagInfo(tvPtr, tagName, infoPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

TreeViewEntry *
Blt_TreeViewNextTaggedEntry(infoPtr) 
    TreeViewTagInfo *infoPtr;
{
    TreeViewEntry *entryPtr;

    entryPtr = NULL;
    if (infoPtr->entryPtr != NULL) {
	TreeView *tvPtr = infoPtr->entryPtr->tvPtr;

	if (infoPtr->tagType & TAG_ALL) {
	    entryPtr = Blt_TreeViewNextEntry(infoPtr->entryPtr, 0);
	} else if (infoPtr->tagType & TAG_MULTIPLE) {
	    Blt_HashEntry *hPtr;
	    
	    hPtr = Blt_NextHashEntry(&infoPtr->cursor);
	    if (hPtr != NULL) {
		Blt_TreeNode node;

		node = Blt_GetHashValue(hPtr);
		entryPtr = Blt_NodeToEntry(tvPtr, node);
	    }
	} 
	infoPtr->entryPtr = entryPtr;
    }
    return entryPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetEntryFromObj2 --
 *
 *	Converts a string into node pointer.  The string may be in one
 *	of the following forms:
 *
 *	    NNN			- inode.
 *	    "active"		- Currently active node.
 *	    "anchor"		- anchor of selected region.
 *	    "current"		- Currently picked node in bindtable.
 *	    "focus"		- The node currently with focus.
 *	    "root"		- Root node.
 *	    "end"		- Last open node in the entire hierarchy.
 *	    "next"		- Next open node from the currently active
 *				  node. Wraps around back to top.
 *	    "last"		- Previous open node from the currently active
 *				  node. Wraps around back to bottom.
 *	    "up"		- Next open node from the currently active
 *				  node. Does not wrap around.
 *	    "down"		- Previous open node from the currently active
 *				  node. Does not wrap around.
 *	    "nextsibling"	- Next sibling of the current node.
 *	    "prevsibling"	- Previous sibling of the current node.
 *	    "parent"		- Parent of the current node.
 *	    "view.top"		- Top of viewport.
 *	    "view.bottom"	- Bottom of viewport.
 *	    @x,y		- Closest node to the specified X-Y position.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	The pointer to the node is returned via nodePtr.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
static int
GetEntryFromObj2(tvPtr, objPtr, entryPtrPtr)
    TreeView *tvPtr;
    Tcl_Obj *objPtr;
    TreeViewEntry **entryPtrPtr;
{
    Tcl_Interp *interp;
    char *string;
    TreeViewTagInfo info;

    interp = tvPtr->interp;

    string = Tcl_GetString(objPtr);
    *entryPtrPtr = NULL;
    if (isdigit(UCHAR(string[0]))) {    
	Blt_TreeNode node;
	int inode;

	if (Tcl_GetIntFromObj(interp, objPtr, &inode) != TCL_OK) {
	    return TCL_ERROR;
	}
	node = Blt_TreeGetNode(tvPtr->tree, inode);
	if (node != NULL) {
	    *entryPtrPtr = Blt_NodeToEntry(tvPtr, node);
	}
	return TCL_OK;		/* Node Id. */
    }
    if (GetEntryFromSpecialId(tvPtr, string, entryPtrPtr) == TCL_OK) {
	return TCL_OK;		/* Special Id. */
    }
    if (GetTagInfo(tvPtr, string, &info) != TCL_OK) {
	return TCL_ERROR;
    }
    if (info.tagType & TAG_MULTIPLE) {
	Tcl_AppendResult(interp, "more than one entry tagged as \"", string, 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    *entryPtrPtr = info.entryPtr;
    return TCL_OK;		/* Singleton tag. */
}

static int
GetEntryFromObj(tvPtr, objPtr, entryPtrPtr)
    TreeView *tvPtr;
    Tcl_Obj *objPtr;
    TreeViewEntry **entryPtrPtr;
{
    tvPtr->fromPtr = NULL;
    return GetEntryFromObj2(tvPtr, objPtr, entryPtrPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewGetEntry --
 *
 *	Returns an entry based upon its index.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	The pointer to the node is returned via nodePtr.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TreeViewGetEntry(tvPtr, objPtr, entryPtrPtr)
    TreeView *tvPtr;
    Tcl_Obj *objPtr;
    TreeViewEntry **entryPtrPtr;
{
    TreeViewEntry *entryPtr;

    if (GetEntryFromObj(tvPtr, objPtr, &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (entryPtr == NULL) {
	Tcl_ResetResult(tvPtr->interp);
	Tcl_AppendResult(tvPtr->interp, "can't find entry \"", 
		Tcl_GetString(objPtr), "\" in \"", Tk_PathName(tvPtr->tkwin), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    *entryPtrPtr = entryPtr;
    return TCL_OK;
}

static Blt_TreeNode 
GetNthNode(parent, position)
    Blt_TreeNode parent;
    int position;
{
    Blt_TreeNode node;
    int count;

    count = 0;
    for(node = Blt_TreeFirstChild(parent); node != NULL; 
	node = Blt_TreeNextSibling(node)) {
	if (count == position) {
	    return node;
	}
    }
    return Blt_TreeLastChild(parent);
}

static TreeViewEntry *
GetNthEntry(
    TreeViewEntry *parentPtr,
    int position,
    unsigned int mask)
{
    TreeViewEntry *entryPtr;
    int count;

    count = 0;
    for(entryPtr = Blt_TreeViewFirstChild(parentPtr, mask); entryPtr != NULL; 
	entryPtr = Blt_TreeViewNextSibling(entryPtr, mask)) {
	if (count == position) {
	    return entryPtr;
	}
    }
    return Blt_TreeViewLastChild(parentPtr, mask);
}

/*
 * Preprocess the command string for percent substitution.
 */
void
Blt_TreeViewPercentSubst(tvPtr, entryPtr, command, resultPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
    char *command;
    Tcl_DString *resultPtr;
{
    register char *last, *p;
    char *fullName;
    Tcl_DString dString;

    /*
     * Get the full path name of the node, in case we need to
     * substitute for it.
     */
    fullName = Blt_TreeViewGetFullName(tvPtr, entryPtr, TRUE, &dString);
    Tcl_DStringInit(resultPtr);
    /* Append the widget name and the node .t 0 */
    for (last = p = command; *p != '\0'; p++) {
	if (*p == '%') {
	    char *string;
	    char buf[3];

	    if (p > last) {
		*p = '\0';
		Tcl_DStringAppend(resultPtr, last, -1);
		*p = '%';
	    }
	    switch (*(p + 1)) {
	    case '%':		/* Percent sign */
		string = "%";
		break;
	    case 'W':		/* Widget name */
		string = Tk_PathName(tvPtr->tkwin);
		break;
	    case 'P':		/* Full pathname */
		string = fullName;
		break;
	    case 'p':		/* Name of the node */
		string = GETLABEL(entryPtr);
		break;
	    case '#':		/* Node identifier */
		string = Blt_Itoa(Blt_TreeNodeId(entryPtr->node));
		break;
	    default:
		if (*(p + 1) == '\0') {
		    p--;
		}
		buf[0] = *p, buf[1] = *(p + 1), buf[2] = '\0';
		string = buf;
		break;
	    }
	    Tcl_DStringAppend(resultPtr, string, -1);
	    p++;
	    last = p + 1;
	}
    }
    if (p > last) {
	*p = '\0';
	Tcl_DStringAppend(resultPtr, last, -1);
    }
    Tcl_DStringFree(&dString);
}

/*
 *----------------------------------------------------------------------
 *
 * SelectEntryApplyProc --
 *
 *	Sets the selection flag for a node.  The selection flag is
 *	set/cleared/toggled based upon the flag set in the treeview
 *	widget.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SelectEntryApplyProc(tvPtr, entryPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
{
    Blt_HashEntry *hPtr;

    switch (tvPtr->flags & TV_SELECT_MASK) {
    case TV_SELECT_CLEAR:
	Blt_TreeViewDeselectEntry(tvPtr, entryPtr);
	break;

    case TV_SELECT_SET:
	Blt_TreeViewSelectEntry(tvPtr, entryPtr);
	break;

    case TV_SELECT_TOGGLE:
	hPtr = Blt_FindHashEntry(&tvPtr->selectTable, (char *)entryPtr);
	if (hPtr != NULL) {
	    Blt_TreeViewDeselectEntry(tvPtr, entryPtr);
	} else {
	    Blt_TreeViewSelectEntry(tvPtr, entryPtr);
	}
	break;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyInvokeSelectCmd --
 *
 *      Queues a request to execute the -selectcommand code associated
 *      with the widget at the next idle point.  Invoked whenever the
 *      selection changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tcl code gets executed for some application-specific task.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyInvokeSelectCmd(tvPtr)
    TreeView *tvPtr;
{
    if (!(tvPtr->flags & TV_SELECT_PENDING)) {
	tvPtr->flags |= TV_SELECT_PENDING;
	Tcl_DoWhenIdle(Blt_TreeViewSelectCmdProc, tvPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeViewPruneSelection --
 *
 *	The root entry being deleted or closed.  Deselect any of its
 *	descendants that are currently selected. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If any of the entry's descendants are deselected the widget
 *	is redrawn and the a selection command callback is invoked
 *	(if there's one configured).
 *
 *----------------------------------------------------------------------
 */
void
Blt_TreeViewPruneSelection(tvPtr, rootPtr)
    TreeView *tvPtr;
    TreeViewEntry *rootPtr;
{
    Blt_ChainLink *linkPtr, *nextPtr;
    TreeViewEntry *entryPtr;
    int selectionChanged;

    /* 
     * Check if any of the currently selected entries are a descendant
     * of of the current root entry.  Deselect the entry and indicate
     * that the treeview widget needs to be redrawn.
     */
    selectionChanged = FALSE;
    for (linkPtr = Blt_ChainFirstLink(tvPtr->selChainPtr); linkPtr != NULL; 
	 linkPtr = nextPtr) {
	nextPtr = Blt_ChainNextLink(linkPtr);
	entryPtr = Blt_ChainGetValue(linkPtr);
	if (Blt_TreeIsAncestor(rootPtr->node, entryPtr->node)) {
	    Blt_TreeViewDeselectEntry(tvPtr, entryPtr);
	    selectionChanged = TRUE;
	}
    }
    if (selectionChanged) {
	Blt_TreeViewEventuallyRedraw(tvPtr);
	if (tvPtr->selectCmd != NULL) {
	    EventuallyInvokeSelectCmd(tvPtr);
	}
    }
}


/*
 * --------------------------------------------------------------
 *
 * TreeView operations
 *
 * --------------------------------------------------------------
 */

/*ARGSUSED*/
static int
FocusOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (objc == 3) {
	TreeViewEntry *entryPtr;

	if (GetEntryFromObj(tvPtr, objv[2], &entryPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((entryPtr != NULL) && (entryPtr != tvPtr->focusPtr)) {
	    if (entryPtr->flags & ENTRY_HIDDEN) {
		/* Doesn't make sense to set focus to a node you can't see. */
		MapAncestors(tvPtr, entryPtr);
	    }
	    /* Changing focus can only affect the visible entries.  The
	     * entry layout stays the same. */
	    if (tvPtr->focusPtr != NULL) {
		tvPtr->focusPtr->flags |= ENTRY_REDRAW;
	    } 
	    entryPtr->flags |= ENTRY_REDRAW;
	    tvPtr->flags |= TV_SCROLL;
	    tvPtr->focusPtr = entryPtr;
	}
	Blt_TreeViewEventuallyRedraw(tvPtr);
    }
    Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
    if (tvPtr->focusPtr != NULL) {
	Tcl_SetObjResult(interp, NodeToObj(tvPtr->focusPtr->node));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BboxOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BboxOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    register int i;
    TreeViewEntry *entryPtr;
    int width, height, yBot;
    int left, top, right, bottom;
    int screen;
    int lWidth;
    char *string;

    if (tvPtr->flags & TV_LAYOUT) {
	/*
	 * The layout is dirty.  Recompute it now, before we use the
	 * world dimensions.  But remember, the "bbox" operation isn't
	 * valid for hidden entries (since they're not visible, they
	 * don't have world coordinates).
	 */
	Blt_TreeViewComputeLayout(tvPtr);
    }
    left = tvPtr->worldWidth;
    top = tvPtr->worldHeight;
    right = bottom = 0;

    screen = FALSE;
    string = Tcl_GetString(objv[2]);
    if ((string[0] == '-') && (strcmp(string, "-screen") == 0)) {
	screen = TRUE;
	objc--, objv++;
    }
    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if ((string[0] == 'a') && (strcmp(string, "all") == 0)) {
	    left = top = 0;
	    right = tvPtr->worldWidth;
	    bottom = tvPtr->worldHeight;
	    break;
	}
	if (GetEntryFromObj(tvPtr, objv[i], &entryPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (entryPtr == NULL) {
	    continue;
	}
	if (entryPtr->flags & ENTRY_HIDDEN) {
	    continue;
	}
	yBot = entryPtr->worldY + entryPtr->height;
	height = VPORTHEIGHT(tvPtr);
	if ((yBot <= tvPtr->yOffset) &&
	    (entryPtr->worldY >= (tvPtr->yOffset + height))) {
	    continue;
	}
	if (bottom < yBot) {
	    bottom = yBot;
	}
	if (top > entryPtr->worldY) {
	    top = entryPtr->worldY;
	}
	lWidth = ICONWIDTH(DEPTH(tvPtr, entryPtr->node));
	if (right < (entryPtr->worldX + entryPtr->width + lWidth)) {
	    right = (entryPtr->worldX + entryPtr->width + lWidth);
	}
	if (left > entryPtr->worldX) {
	    left = entryPtr->worldX;
	}
    }

    if (screen) {
	width = VPORTWIDTH(tvPtr);
	height = VPORTHEIGHT(tvPtr);
	/*
	 * Do a min-max text for the intersection of the viewport and
	 * the computed bounding box.  If there is no intersection, return
	 * the empty string.
	 */
	if ((right < tvPtr->xOffset) || (bottom < tvPtr->yOffset) ||
	    (left >= (tvPtr->xOffset + width)) ||
	    (top >= (tvPtr->yOffset + height))) {
	    return TCL_OK;
	}
	/* Otherwise clip the coordinates at the view port boundaries. */
	if (left < tvPtr->xOffset) {
	    left = tvPtr->xOffset;
	} else if (right > (tvPtr->xOffset + width)) {
	    right = tvPtr->xOffset + width;
	}
	if (top < tvPtr->yOffset) {
	    top = tvPtr->yOffset;
	} else if (bottom > (tvPtr->yOffset + height)) {
	    bottom = tvPtr->yOffset + height;
	}
	left = SCREENX(tvPtr, left), top = SCREENY(tvPtr, top);
	right = SCREENX(tvPtr, right), bottom = SCREENY(tvPtr, bottom);
    }
    if ((left < right) && (top < bottom)) {
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(left));
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(top));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewIntObj(right - left));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewIntObj(bottom - top));
	Tcl_SetObjResult(interp, listObjPtr);
    }
    return TCL_OK;
}

static void
DrawButton(tvPtr, entryPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
{
    Drawable drawable;
    int sx, sy, dx, dy;
    int width, height;
    int left, right, top, bottom;

    dx = SCREENX(tvPtr, entryPtr->worldX) + entryPtr->buttonX;
    dy = SCREENY(tvPtr, entryPtr->worldY) + entryPtr->buttonY;
    width = tvPtr->button.width;
    height = tvPtr->button.height;

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
    Blt_TreeViewDrawButton(tvPtr, entryPtr, drawable, 0, 0);

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
 * ButtonActivateOp --
 *
 *	Selects the button to appear active.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonActivateOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *oldPtr, *newPtr;
    char *string;

    string = Tcl_GetString(objv[3]);
    if (string[0] == '\0') {
	newPtr = NULL;
    } else if (GetEntryFromObj(tvPtr, objv[3], &newPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tvPtr->treeColumn.hidden) {
	return TCL_OK;
    }
    if ((newPtr != NULL) && !(newPtr->flags & ENTRY_HAS_BUTTON)) {
	newPtr = NULL;
    }
    oldPtr = tvPtr->activeButtonPtr;
    tvPtr->activeButtonPtr = newPtr;
    if (!(tvPtr->flags & TV_REDRAW) && (newPtr != oldPtr)) {
	if ((oldPtr != NULL) && (oldPtr != tvPtr->rootPtr)) {
	    DrawButton(tvPtr, oldPtr);
	}
	if ((newPtr != NULL) && (newPtr != tvPtr->rootPtr)) {
	    DrawButton(tvPtr, newPtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonBindOp --
 *
 *	  .t bind tag sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonBindOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    ClientData object;
    char *string;

    string = Tcl_GetString(objv[3]);
    /* Assume that this is a binding tag. */
    object = Blt_TreeViewButtonTag(tvPtr, string);
    return Blt_ConfigureBindingsFromObj(interp, tvPtr->bindTable, object, 
	objc - 4, objv + 4);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonCgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonCgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    return Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
	bltTreeViewButtonSpecs, (char *)tvPtr, objv[3], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h button configure option value
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for tvPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ButtonConfigureOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
	    bltTreeViewButtonSpecs, (char *)tvPtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
		bltTreeViewButtonSpecs, (char *)tvPtr, objv[3], 0);
    }
    bltTreeViewIconsOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(tvPtr->interp, tvPtr->tkwin, 
		bltTreeViewButtonSpecs, objc - 3, objv + 3, (char *)tvPtr, 
		BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_TreeViewConfigureButtons(tvPtr);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonOp --
 *
 *	This procedure handles button operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec buttonOps[] =
{
    {"activate", 1, (Blt_Op)ButtonActivateOp, 4, 4, "tagOrId",},
    {"bind", 1, (Blt_Op)ButtonBindOp, 4, 6, "tagName ?sequence command?",},
    {"cget", 2, (Blt_Op)ButtonCgetOp, 4, 4, "option",},
    {"configure", 2, (Blt_Op)ButtonConfigureOp, 3, 0, "?option value?...",},
    {"highlight", 1, (Blt_Op)ButtonActivateOp, 4, 4, "tagOrId",},
};

static int nButtonOps = sizeof(buttonOps) / sizeof(Blt_OpSpec);

static int
ButtonOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nButtonOps, buttonOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (tvPtr, interp, objc, objv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    return Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, bltTreeViewSpecs,
	(char *)tvPtr, objv[2], 0);
}

/*ARGSUSED*/
static int
CloseOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    TreeViewTagInfo info;
    int recurse, result;
    register int i;

    recurse = FALSE;

    if (objc > 2) {
	char *string;
	int length;

	string = Tcl_GetStringFromObj(objv[2], &length);
	if ((string[0] == '-') && (length > 1) && 
	    (strncmp(string, "-recurse", length) == 0)) {
	    objv++, objc--;
	    recurse = TRUE;
	}
    }
    for (i = 2; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    /* 
	     * Clear the selections for any entries that may have become
	     * hidden by closing the node.  
	     */
	    Blt_TreeViewPruneSelection(tvPtr, entryPtr);
	    
	    /*
	     * -----------------------------------------------------------
	     *
	     *  Check if either the "focus" entry or selection anchor
	     *  is in this hierarchy.  Must move it or disable it before
	     *  we close the node.  Otherwise it may be deleted by a Tcl
	     *  "close" script, and we'll be left pointing to a bogus
	     *  memory location.
	     *
	     * -----------------------------------------------------------
	     */
	    if ((tvPtr->focusPtr != NULL) && 
		(Blt_TreeIsAncestor(entryPtr->node, tvPtr->focusPtr->node))) {
		tvPtr->focusPtr = entryPtr;
		Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
	    }
	    if ((tvPtr->selAnchorPtr != NULL) && 
		(Blt_TreeIsAncestor(entryPtr->node, 
				    tvPtr->selAnchorPtr->node))) {
		tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
	    }
	    if ((tvPtr->activePtr != NULL) && 
		(Blt_TreeIsAncestor(entryPtr->node, tvPtr->activePtr->node))) {
		tvPtr->activePtr = entryPtr;
	    }
	    if (recurse) {
		result = Blt_TreeViewApply(tvPtr, entryPtr, 
					   Blt_TreeViewCloseEntry, 0);
	    } else {
		result = Blt_TreeViewCloseEntry(tvPtr, entryPtr);
	    }
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }	
	}
    }
    /* Closing a node may affect the visible entries and the 
     * the world layout of the entries. */
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 * 	This procedure is called to process an objv/objc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	the widget.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for tvPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, bltTreeViewSpecs,
		(char *)tvPtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
		bltTreeViewSpecs, (char *)tvPtr, objv[2], 0);
    } 
    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewTreeOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(interp, tvPtr->tkwin, bltTreeViewSpecs, 
	objc - 2, objv + 2, (char *)tvPtr, BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_TreeViewUpdateWidget(interp, tvPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
CurselectionOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;	/* Not used. */
{
    TreeViewEntry *entryPtr;
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (tvPtr->flags & TV_SELECT_SORTED) {
	Blt_ChainLink *linkPtr;

	for (linkPtr = Blt_ChainFirstLink(tvPtr->selChainPtr); linkPtr != NULL;
	     linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    objPtr = NodeToObj(entryPtr->node);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
			
	}
    } else {
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, ENTRY_MASK)) {
	    if (Blt_TreeViewEntryIsSelected(tvPtr, entryPtr)) {
		objPtr = NodeToObj(entryPtr->node);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BindOp --
 *
 *	  .t bind tagOrId sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BindOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    ClientData object;
    TreeViewEntry *entryPtr;
    char *string;

    /*
     * Entries are selected by id only.  All other strings are
     * interpreted as a binding tag.
     */
    string = Tcl_GetString(objv[2]);
    if (isdigit(UCHAR(string[0]))) {
	Blt_TreeNode node;
	int inode;

	if (Tcl_GetIntFromObj(tvPtr->interp, objv[2], &inode) != TCL_OK) {
	    return TCL_ERROR;
	}
	node = Blt_TreeGetNode(tvPtr->tree, inode);
	object = Blt_NodeToEntry(tvPtr, node);
    } else if (GetEntryFromSpecialId(tvPtr, string, &entryPtr) == TCL_OK) {
	if (entryPtr != NULL) {
	    return TCL_OK;	/* Special id doesn't currently exist. */
	}
	object = entryPtr;
    } else {
	/* Assume that this is a binding tag. */
	object = Blt_TreeViewEntryTag(tvPtr, string);
    } 
    return Blt_ConfigureBindingsFromObj(interp, tvPtr->bindTable, object, 
	 objc - 3, objv + 3);
}


/*ARGSUSED*/
static int
EditOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    char *string;
    int isRoot, isTest;
    int x, y;

    isRoot = isTest = FALSE;
    string = Tcl_GetString(objv[2]);
    if (strcmp("-root", string) == 0) {
	isRoot = TRUE;
	objv++, objc--;
    }
    string = Tcl_GetString(objv[2]);
    if (strcmp("-test", string) == 0) {
	isTest = TRUE;
	objv++, objc--;
    }
    if (objc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " ", Tcl_GetString(objv[1]), 
			" ?-root? x y\"", (char *)NULL);
	return TCL_ERROR;
			 
    }
    if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (isRoot) {
	int rootX, rootY;

	Tk_GetRootCoords(tvPtr->tkwin, &rootX, &rootY);
	x -= rootX;
	y -= rootY;
    }
    entryPtr = Blt_TreeViewNearestEntry(tvPtr, x, y, FALSE);
    if (entryPtr != NULL) {
	Blt_ChainLink *linkPtr;
	TreeViewColumn *columnPtr;
	int worldX;

	worldX = WORLDX(tvPtr, x);
	for (linkPtr = Blt_ChainFirstLink(tvPtr->colChainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    columnPtr = Blt_ChainGetValue(linkPtr);
	    if (!columnPtr->editable) {
		continue;		/* Column isn't editable. */
	    }
	    if ((worldX >= columnPtr->worldX) && 
		(worldX < (columnPtr->worldX + columnPtr->width))) {
		TreeViewValue *valuePtr;
		
		valuePtr = Blt_TreeViewFindValue(entryPtr, columnPtr);
		if (valuePtr != NULL) {
		    TreeViewStyle *stylePtr;
		    
		    stylePtr = valuePtr->stylePtr;
		    if (stylePtr == NULL) {
			stylePtr = columnPtr->stylePtr;
		    }
		    if ((stylePtr->classPtr->editProc != NULL) && (!isTest)) {
			if ((*stylePtr->classPtr->editProc)(tvPtr, entryPtr, 
				    valuePtr, stylePtr) != TCL_OK) {
			    return TCL_ERROR;
			}
			Blt_TreeViewEventuallyRedraw(tvPtr);
		    }
		    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
		    return TCL_OK;
		}
	    }
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryActivateOp --
 *
 *	Selects the entry to appear active.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryActivateOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *newPtr, *oldPtr;
    char *string;

    string = Tcl_GetString(objv[3]);
    if (string[0] == '\0') {
	newPtr = NULL;
    } else if (GetEntryFromObj(tvPtr, objv[3], &newPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tvPtr->treeColumn.hidden) {
	return TCL_OK;
    }
    oldPtr = tvPtr->activePtr;
    tvPtr->activePtr = newPtr;
    if (!(tvPtr->flags & TV_REDRAW) && (newPtr != oldPtr)) {
	Drawable drawable;
	int x, y;
	
	drawable = Tk_WindowId(tvPtr->tkwin);
	if (oldPtr != NULL) {
	    x = SCREENX(tvPtr, oldPtr->worldX);
	    if (!tvPtr->flatView) {
		x += ICONWIDTH(DEPTH(tvPtr, oldPtr->node));
	    }
	    y = SCREENY(tvPtr, oldPtr->worldY);
	    oldPtr->flags |= ENTRY_ICON;
	    Blt_TreeViewDrawIcon(tvPtr, oldPtr, drawable, x, y);
	}
	if (newPtr != NULL) {
	    x = SCREENX(tvPtr, newPtr->worldX);
	    if (!tvPtr->flatView) {
		x += ICONWIDTH(DEPTH(tvPtr, newPtr->node));
	    }
	    y = SCREENY(tvPtr, newPtr->worldY);
	    newPtr->flags |= ENTRY_ICON;
	    Blt_TreeViewDrawIcon(tvPtr, newPtr, drawable, x, y);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryCgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryCgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;

    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
		bltTreeViewEntrySpecs, (char *)entryPtr, objv[4], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * EntryConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h entryconfigure node node node node option value
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for tvPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
EntryConfigureOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int nIds, configObjc;
    Tcl_Obj *CONST *configObjv;
    register int i;
    TreeViewEntry *entryPtr;
    TreeViewTagInfo info;
    char *string;

    /* Figure out where the option value pairs begin */
    objc -= 3, objv += 3;
    for (i = 0; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (string[0] == '-') {
	    break;
	}
    }
    nIds = i;			/* # of tags or ids specified */
    configObjc = objc - i;	/* # of options specified */
    configObjv = objv + i;	/* Start of options in objv  */

    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewUidOption.clientData = tvPtr;

    for (i = 0; i < nIds; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    if (configObjc == 0) {
		return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
			bltTreeViewEntrySpecs, (char *)entryPtr, 
			(Tcl_Obj *)NULL, 0);
	    } else if (configObjc == 1) {
		return Blt_ConfigureInfoFromObj(interp, tvPtr->tkwin, 
			bltTreeViewEntrySpecs, (char *)entryPtr, 
			configObjv[0], 0);
	    }
	    if (Blt_TreeViewConfigureEntry(tvPtr, entryPtr, configObjc, 
		configObjv, BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    tvPtr->flags |= (TV_DIRTY | TV_LAYOUT | TV_SCROLL | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryIsOpenOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryIsBeforeOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *e1Ptr, *e2Ptr;
    int bool;

    if ((Blt_TreeViewGetEntry(tvPtr, objv[3], &e1Ptr) != TCL_OK) ||
	(Blt_TreeViewGetEntry(tvPtr, objv[4], &e2Ptr) != TCL_OK)) {
	return TCL_ERROR;
    }
    bool = Blt_TreeIsBefore(e1Ptr->node, e2Ptr->node);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryIsHiddenOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryIsHiddenOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    int bool;

    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    bool = (entryPtr->flags & ENTRY_HIDDEN);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * EntryIsOpenOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryIsOpenOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    int bool;

    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    bool = ((entryPtr->flags & ENTRY_CLOSED) == 0);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*ARGSUSED*/
static int
EntryChildrenOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *parentPtr;
    Tcl_Obj *listObjPtr, *objPtr;
    unsigned int mask;

    mask = 0;
    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &parentPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (objc == 4) {
	TreeViewEntry *entryPtr;

	for (entryPtr = Blt_TreeViewFirstChild(parentPtr, mask); 
	     entryPtr != NULL;
	     entryPtr = Blt_TreeViewNextSibling(entryPtr, mask)) {
	    objPtr = NodeToObj(entryPtr->node);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else if (objc == 6) {
	TreeViewEntry *entryPtr, *lastPtr, *firstPtr;
	int firstPos, lastPos;
	int nNodes;

	if ((Blt_GetPositionFromObj(interp, objv[4], &firstPos) != TCL_OK) ||
	    (Blt_GetPositionFromObj(interp, objv[5], &lastPos) != TCL_OK)) {
	    return TCL_ERROR;
	}
	nNodes = Blt_TreeNodeDegree(parentPtr->node);
	if (nNodes == 0) {
	    return TCL_OK;
	}
	if ((lastPos == END) || (lastPos >= nNodes)) {
	    lastPtr = Blt_TreeViewLastChild(parentPtr, mask);
	} else {
	    lastPtr = GetNthEntry(parentPtr, lastPos, mask);
	}
	if ((firstPos == END) || (firstPos >= nNodes)) {
	    firstPtr = Blt_TreeViewLastChild(parentPtr, mask);
	} else {
	    firstPtr = GetNthEntry(parentPtr, firstPos, mask);
	}
	if ((lastPos != END) && (firstPos > lastPos)) {
	    for (entryPtr = lastPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeViewPrevEntry(entryPtr, mask)) {
		objPtr = NodeToObj(entryPtr->node);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		if (entryPtr == firstPtr) {
		    break;
		}
	    }
	} else {
	    for (entryPtr = firstPtr; entryPtr != NULL; 
		 entryPtr = Blt_TreeViewNextEntry(entryPtr, mask)) {
		objPtr = NodeToObj(entryPtr->node);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		if (entryPtr == lastPtr) {
		    break;
		}
	    }
	}
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
			 Tcl_GetString(objv[0]), " ",
			 Tcl_GetString(objv[1]), " ", 
			 Tcl_GetString(objv[2]), " tagOrId ?first last?", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * EntryDeleteOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EntryDeleteOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;

    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 5) {
	int entryPos;
	Blt_TreeNode node;
	/*
	 * Delete a single child node from a hierarchy specified 
	 * by its numeric position.
	 */
	if (Blt_GetPositionFromObj(interp, objv[3], &entryPos) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (entryPos >= (int)Blt_TreeNodeDegree(entryPtr->node)) {
	    return TCL_OK;	/* Bad first index */
	}
	if (entryPos == END) {
	    node = Blt_TreeLastChild(entryPtr->node);
	} else {
	    node = GetNthNode(entryPtr->node, entryPos);
	}
	DeleteNode(tvPtr, node);
    } else {
	int firstPos, lastPos;
	Blt_TreeNode node, first, last, next;
	int nEntries;
	/*
	 * Delete range of nodes in hierarchy specified by first/last
	 * positions.
	 */
	if ((Blt_GetPositionFromObj(interp, objv[4], &firstPos) != TCL_OK) ||
	    (Blt_GetPositionFromObj(interp, objv[5], &lastPos) != TCL_OK)) {
	    return TCL_ERROR;
	}
	nEntries = Blt_TreeNodeDegree(entryPtr->node);
	if (nEntries == 0) {
	    return TCL_OK;
	}
	if (firstPos == END) {
	    firstPos = nEntries - 1;
	}
	if (firstPos >= nEntries) {
	    Tcl_AppendResult(interp, "first position \"", 
		Tcl_GetString(objv[4]), " is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	if ((lastPos == END) || (lastPos >= nEntries)) {
	    lastPos = nEntries - 1;
	}
	if (firstPos > lastPos) {
	    Tcl_AppendResult(interp, "bad range: \"", Tcl_GetString(objv[4]), 
		" > ", Tcl_GetString(objv[5]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	first = GetNthNode(entryPtr->node, firstPos);
	last = GetNthNode(entryPtr->node, lastPos);
	for (node = first; node != NULL; node = next) {
	    next = Blt_TreeNextSibling(node);
	    DeleteNode(tvPtr, node);
	    if (node == last) {
		break;
	    }
	}
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntrySizeOp --
 *
 *	Counts the number of entries at this node.
 *
 * Results:
 *	A standard Tcl result.  If an error occurred TCL_ERROR is
 *	returned and interp->result will contain an error message.
 *	Otherwise, TCL_OK is returned and interp->result contains
 *	the number of entries.
 *
 *----------------------------------------------------------------------
 */
static int
EntrySizeOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    int length, sum, recurse;
    char *string;

    recurse = FALSE;
    string = Tcl_GetStringFromObj(objv[3], &length);
    if ((string[0] == '-') && (length > 1) &&
	(strncmp(string, "-recurse", length) == 0)) {
	objv++, objc--;
	recurse = TRUE;
    }
    if (objc == 3) {
	Tcl_AppendResult(interp, "missing node argument: should be \"",
	    Tcl_GetString(objv[0]), " entry open node\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (recurse) {
	sum = Blt_TreeSize(entryPtr->node);
    } else {
	sum = Blt_TreeNodeDegree(entryPtr->node);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(sum));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryOp --
 *
 *	This procedure handles entry operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

static Blt_OpSpec entryOps[] =
{
    {"activate", 1, (Blt_Op)EntryActivateOp, 4, 4, "tagOrId",},
    /*bbox*/
    /*bind*/
    {"cget", 2, (Blt_Op)EntryCgetOp, 5, 5, "tagOrId option",},
    {"children", 2, (Blt_Op)EntryChildrenOp, 4, 6, 
	"tagOrId firstPos lastPos",},
    /*close*/
    {"configure", 2, (Blt_Op)EntryConfigureOp, 4, 0,
	"tagOrId ?tagOrId...? ?option value?...",},
    {"delete", 2, (Blt_Op)EntryDeleteOp, 5, 6, "tagOrId firstPos ?lastPos?",},
    /*focus*/
    /*hide*/
    {"highlight", 1, (Blt_Op)EntryActivateOp, 4, 4, "tagOrId",},
    /*index*/
    {"isbefore", 3, (Blt_Op)EntryIsBeforeOp, 5, 5, "tagOrId tagOrId",},
    {"ishidden", 3, (Blt_Op)EntryIsHiddenOp, 4, 4, "tagOrId",},
    {"isopen", 3, (Blt_Op)EntryIsOpenOp, 4, 4, "tagOrId",},
    /*move*/
    /*nearest*/
    /*open*/
    /*see*/
    /*show*/
    {"size", 1, (Blt_Op)EntrySizeOp, 4, 5, "?-recurse? tagOrId",},
    /*toggle*/
};
static int nEntryOps = sizeof(entryOps) / sizeof(Blt_OpSpec);

static int
EntryOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nEntryOps, entryOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (tvPtr, interp, objc, objv);
    return result;
}

/*ARGSUSED*/
static int
ExactCompare(interp, name, pattern)
    Tcl_Interp *interp;		/* Not used. */
    char *name;
    char *pattern;
{
    return (strcmp(name, pattern) == 0);
}

/*ARGSUSED*/
static int
GlobCompare(interp, name, pattern)
    Tcl_Interp *interp;		/* Not used. */
    char *name;
    char *pattern;
{
    return Tcl_StringMatch(name, pattern);
}

static int
RegexpCompare(interp, name, pattern)
    Tcl_Interp *interp;
    char *name;
    char *pattern;
{
    return Tcl_RegExpMatch(interp, name, pattern);
}

/*
 *----------------------------------------------------------------------
 *
 * FindOp --
 *
 *	Find one or more nodes based upon the pattern provided.
 *
 * Results:
 *	A standard Tcl result.  The interpreter result will contain a
 *	list of the node serial identifiers.
 *
 *----------------------------------------------------------------------
 */
static int
FindOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *firstPtr, *lastPtr;
    int nMatches, maxMatches;
    char c;
    int length;
    TreeViewCompareProc *compareProc;
    TreeViewIterProc *nextProc;
    int invertMatch;		/* normal search mode (matching entries) */
    char *namePattern, *fullPattern;
    char *execCmd;
    register int i;
    int result;
    char *pattern, *option;
    Tcl_DString dString;
    Blt_List options;
    Blt_ListNode node;
    char *addTag, *withTag;
    register TreeViewEntry *entryPtr;
    char *string;
    Tcl_Obj *listObjPtr, *objPtr;

    invertMatch = FALSE;
    maxMatches = 0;
    execCmd = namePattern = fullPattern = NULL;
    compareProc = ExactCompare;
    nextProc = Blt_TreeViewNextEntry;
    options = Blt_ListCreate(BLT_ONE_WORD_KEYS);
    withTag = addTag = NULL;

    entryPtr = tvPtr->rootPtr;
    /*
     * Step 1:  Process flags for find operation.
     */
    for (i = 2; i < objc; i++) {
	string = Tcl_GetStringFromObj(objv[i], &length);
	if (string[0] != '-') {
	    break;
	}
	option = string + 1;
	length--;
	c = option[0];
	if ((c == 'e') && (length > 2) &&
	    (strncmp(option, "exact", length) == 0)) {
	    compareProc = ExactCompare;
	} else if ((c == 'g') && (strncmp(option, "glob", length) == 0)) {
	    compareProc = GlobCompare;
	} else if ((c == 'r') && (strncmp(option, "regexp", length) == 0)) {
	    compareProc = RegexpCompare;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "nonmatching", length) == 0)) {
	    invertMatch = TRUE;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "name", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    namePattern = Tcl_GetString(objv[i]);
	} else if ((c == 'f') && (strncmp(option, "full", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    fullPattern = Tcl_GetString(objv[i]);
	} else if ((c == 'e') && (length > 2) &&
	    (strncmp(option, "exec", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    execCmd = Tcl_GetString(objv[i]);
	} else if ((c == 'a') && (length > 1) &&
		   (strncmp(option, "addtag", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    addTag = Tcl_GetString(objv[i]);
	} else if ((c == 't') && (length > 1) && 
		   (strncmp(option, "tag", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    withTag = Tcl_GetString(objv[i]);
	} else if ((c == 'c') && (strncmp(option, "count", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    if (Tcl_GetIntFromObj(interp, objv[i], &maxMatches) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (maxMatches < 0) {
		Tcl_AppendResult(interp, "bad match count \"", objv[i],
		    "\": should be a positive number", (char *)NULL);
		Blt_ListDestroy(options);
		return TCL_ERROR;
	    }
	} else if ((option[0] == '-') && (option[1] == '\0')) {
	    break;
	} else {
	    /*
	     * Verify that the switch is actually an entry configuration
	     * option.
	     */
	    if (Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
		  bltTreeViewEntrySpecs, (char *)entryPtr, objv[i], 0) 
		!= TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad find switch \"", string, "\"",
		    (char *)NULL);
		Blt_ListDestroy(options);
		return TCL_ERROR;
	    }
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    /* Save the option in the list of configuration options */
	    node = Blt_ListGetNode(options, (char *)objv[i]);
	    if (node == NULL) {
		node = Blt_ListCreateNode(options, (char *)objv[i]);
		Blt_ListAppendNode(options, node);
	    }
	    i++;
	    Blt_ListSetValue(node, Tcl_GetString(objv[i]));
	}
    }

    if ((objc - i) > 2) {
	Blt_ListDestroy(options);
	Tcl_AppendResult(interp, "too many args", (char *)NULL);
	return TCL_ERROR;
    }
    /*
     * Step 2:  Find the range of the search.  Check the order of two
     *		nodes and arrange the search accordingly.
     *
     *	Note:	Be careful to treat "end" as the end of all nodes, instead
     *		of the end of visible nodes.  That way, we can search the
     *		entire tree, even if the last folder is closed.
     */
    firstPtr = tvPtr->rootPtr;	/* Default to root node */
    lastPtr = LastEntry(tvPtr, firstPtr, 0);

    if (i < objc) {
	string = Tcl_GetString(objv[i]);
	if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
	    firstPtr = LastEntry(tvPtr, tvPtr->rootPtr, 0);
	} else if (Blt_TreeViewGetEntry(tvPtr, objv[i], &firstPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	i++;
    }
    if (i < objc) {
	string = Tcl_GetString(objv[i]);
	if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
	    lastPtr = LastEntry(tvPtr, tvPtr->rootPtr, 0);
	} else if (Blt_TreeViewGetEntry(tvPtr, objv[i], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (Blt_TreeIsBefore(lastPtr->node, firstPtr->node)) {
	nextProc = Blt_TreeViewPrevEntry;
    }
    nMatches = 0;

    /*
     * Step 3:	Search through the tree and look for nodes that match the
     *		current pattern specifications.  Save the name of each of
     *		the matching nodes.
     */
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (entryPtr = firstPtr; entryPtr != NULL; 
	 entryPtr = (*nextProc) (entryPtr, 0)) {
	if (namePattern != NULL) {
	    result = (*compareProc)(interp, Blt_TreeNodeLabel(entryPtr->node),
		     namePattern);
	    if (result == invertMatch) {
		goto nextEntry;	/* Failed to match */
	    }
	}
	if (fullPattern != NULL) {
	    Tcl_DString fullName;

	    Blt_TreeViewGetFullName(tvPtr, entryPtr, FALSE, &fullName);
	    result = (*compareProc) (interp, Tcl_DStringValue(&fullName), 
		fullPattern);
	    Tcl_DStringFree(&fullName);
	    if (result == invertMatch) {
		goto nextEntry;	/* Failed to match */
	    }
	}
	if (withTag != NULL) {
	    result = Blt_TreeHasTag(tvPtr->tree, entryPtr->node, withTag);
	    if (result == invertMatch) {
		goto nextEntry;	/* Failed to match */
	    }
	}
	for (node = Blt_ListFirstNode(options); node != NULL;
	    node = Blt_ListNextNode(node)) {
	    objPtr = (Tcl_Obj *)Blt_ListGetKey(node);
	    Tcl_ResetResult(interp);
	    Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
		bltTreeViewEntrySpecs, (char *)entryPtr, objPtr, 0);
	    pattern = Blt_ListGetValue(node);
	    objPtr = Tcl_GetObjResult(interp);
	    result = (*compareProc) (interp, Tcl_GetString(objPtr), pattern);
	    if (result == invertMatch) {
		goto nextEntry;	/* Failed to match */
	    }
	}
	/* 
	 * Someone may actually delete the current node in the "exec"
	 * callback.  Preserve the entry.
	 */
	Tcl_Preserve(entryPtr);
	if (execCmd != NULL) {
	    Tcl_DString cmdString;

	    Blt_TreeViewPercentSubst(tvPtr, entryPtr, execCmd, &cmdString);
	    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&cmdString));
	    Tcl_DStringFree(&cmdString);
	    if (result != TCL_OK) {
		Tcl_Release(entryPtr);
		goto error;
	    }
	}
	/* A NULL node reference in an entry indicates that the entry
	 * was deleted, but its memory not released yet. */
	if (entryPtr->node != NULL) {
	    /* Finally, save the matching node name. */
	    objPtr = NodeToObj(entryPtr->node);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    if (addTag != NULL) {
		if (AddTag(tvPtr, entryPtr->node, addTag) != TCL_OK) {
		    goto error;
		}
	    }
	}
	    
	Tcl_Release(entryPtr);
	nMatches++;
	if ((nMatches == maxMatches) && (maxMatches > 0)) {
	    break;
	}
      nextEntry:
	if (entryPtr == lastPtr) {
	    break;
	}
    }
    Tcl_ResetResult(interp);
    Blt_ListDestroy(options);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;

  missingArg:
    Tcl_AppendResult(interp, "missing argument for find option \"", objv[i],
	"\"", (char *)NULL);
  error:
    Tcl_DStringFree(&dString);
    Blt_ListDestroy(options);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 *	Converts one or more node identifiers to its path component.
 *	The path may be either the single entry name or the full path
 *	of the entry.
 *
 * Results:
 *	A standard Tcl result.  The interpreter result will contain a
 *	list of the convert names.
 *
 *----------------------------------------------------------------------
 */
static int
GetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewTagInfo info;
    TreeViewEntry *entryPtr;
    int useFullName;
    register int i;
    Tcl_DString dString1, dString2;
    int count;

    useFullName = FALSE;
    if (objc > 2) {
	char *string;

	string = Tcl_GetString(objv[2]);
	if ((string[0] == '-') && (strcmp(string, "-full") == 0)) {
	    useFullName = TRUE;
	    objv++, objc--;
	}
    }
    Tcl_DStringInit(&dString1);
    Tcl_DStringInit(&dString2);
    count = 0;
    for (i = 2; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    Tcl_DStringSetLength(&dString2, 0);
	    count++;
	    if (entryPtr->node == NULL) {
		Tcl_DStringAppendElement(&dString1, "");
		continue;
	    }
	    if (useFullName) {
		Blt_TreeViewGetFullName(tvPtr, entryPtr, FALSE, &dString2);
		Tcl_DStringAppendElement(&dString1, 
			 Tcl_DStringValue(&dString2));
	    } else {
		Tcl_DStringAppendElement(&dString1, 
			 Blt_TreeNodeLabel(entryPtr->node));
	    }
	}
    }
    /* This handles the single element list problem. */
    if (count == 1) {
	Tcl_DStringResult(interp, &dString2);
	Tcl_DStringFree(&dString1);
    } else {
	Tcl_DStringResult(interp, &dString1);
	Tcl_DStringFree(&dString2);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SearchAndApplyToTree --
 *
 *	Searches through the current tree and applies a procedure
 *	to matching nodes.  The search specification is taken from
 *	the following command-line arguments:
 *
 *      ?-exact? ?-glob? ?-regexp? ?-nonmatching?
 *      ?-data string?
 *      ?-name string?
 *      ?-full string?
 *      ?--?
 *      ?inode...?
 *
 * Results:
 *	A standard Tcl result.  If the result is valid, and if the
 *      nonmatchPtr is specified, it returns a boolean value
 *      indicating whether or not the search was inverted.  This
 *      is needed to fix things properly for the "hide nonmatching"
 *      case.
 *
 *----------------------------------------------------------------------
 */
static int
SearchAndApplyToTree(tvPtr, interp, objc, objv, proc, nonMatchPtr)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
    TreeViewApplyProc *proc;
    int *nonMatchPtr;		/* returns: inverted search indicator */
{
    TreeViewCompareProc *compareProc;
    int invertMatch;		/* normal search mode (matching entries) */
    char *namePattern, *fullPattern;
    register int i;
    int length;
    int result;
    char *option, *pattern;
    char c;
    Blt_List options;
    TreeViewEntry *entryPtr;
    register Blt_ListNode node;
    char *string;
    char *withTag;
    Tcl_Obj *objPtr;
    TreeViewTagInfo info;

    options = Blt_ListCreate(BLT_ONE_WORD_KEYS);
    invertMatch = FALSE;
    namePattern = fullPattern = NULL;
    compareProc = ExactCompare;
    withTag = NULL;

    entryPtr = tvPtr->rootPtr;
    for (i = 2; i < objc; i++) {
	string = Tcl_GetStringFromObj(objv[i], &length);
	if (string[0] != '-') {
	    break;
	}
	option = string + 1;
	length--;
	c = option[0];
	if ((c == 'e') && (strncmp(option, "exact", length) == 0)) {
	    compareProc = ExactCompare;
	} else if ((c == 'g') && (strncmp(option, "glob", length) == 0)) {
	    compareProc = GlobCompare;
	} else if ((c == 'r') && (strncmp(option, "regexp", length) == 0)) {
	    compareProc = RegexpCompare;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "nonmatching", length) == 0)) {
	    invertMatch = TRUE;
	} else if ((c == 'f') && (strncmp(option, "full", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    fullPattern = Tcl_GetString(objv[i]);
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "name", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    namePattern = Tcl_GetString(objv[i]);
	} else if ((c == 't') && (length > 1) && 
		   (strncmp(option, "tag", length) == 0)) {
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    i++;
	    withTag = Tcl_GetString(objv[i]);
	} else if ((option[0] == '-') && (option[1] == '\0')) {
	    break;
	} else {
	    /*
	     * Verify that the switch is actually an entry configuration option.
	     */
	    if (Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
		bltTreeViewEntrySpecs, (char *)entryPtr, objv[i], 0) 
		!= TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad switch \"", string,
	    "\": must be -exact, -glob, -regexp, -name, -full, or -nonmatching",
		    (char *)NULL);
		return TCL_ERROR;
	    }
	    if ((i + 1) == objc) {
		goto missingArg;
	    }
	    /* Save the option in the list of configuration options */
	    node = Blt_ListGetNode(options, (char *)objv[i]);
	    if (node == NULL) {
		node = Blt_ListCreateNode(options, (char *)objv[i]);
		Blt_ListAppendNode(options, node);
	    }
	    i++;
	    Blt_ListSetValue(node, Tcl_GetString(objv[i]));
	}
    }

    if ((namePattern != NULL) || (fullPattern != NULL) ||
	(Blt_ListGetLength(options) > 0)) {
	/*
	 * Search through the tree and look for nodes that match the
	 * current spec.  Apply the input procedure to each of the
	 * matching nodes.
	 */
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, 0)) {
	    if (namePattern != NULL) {
		result = (*compareProc) (interp, 
			Blt_TreeNodeLabel(entryPtr->node), namePattern);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    if (fullPattern != NULL) {
		Tcl_DString dString;

		Blt_TreeViewGetFullName(tvPtr, entryPtr, FALSE, &dString);
		result = (*compareProc) (interp, Tcl_DStringValue(&dString), 
			fullPattern);
		Tcl_DStringFree(&dString);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    if (withTag != NULL) {
		result = Blt_TreeHasTag(tvPtr->tree, entryPtr->node, withTag);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    for (node = Blt_ListFirstNode(options); node != NULL;
		node = Blt_ListNextNode(node)) {
		objPtr = (Tcl_Obj *)Blt_ListGetKey(node);
		Tcl_ResetResult(interp);
		if (Blt_ConfigureValueFromObj(interp, tvPtr->tkwin, 
			bltTreeViewEntrySpecs, (char *)entryPtr, objPtr, 0) 
		    != TCL_OK) {
		    return TCL_ERROR;	/* This shouldn't happen. */
		}
		pattern = Blt_ListGetValue(node);
		objPtr = Tcl_GetObjResult(interp);
		result = (*compareProc)(interp, Tcl_GetString(objPtr), pattern);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    /* Finally, apply the procedure to the node */
	    (*proc) (tvPtr, entryPtr);
	}
	Tcl_ResetResult(interp);
	Blt_ListDestroy(options);
    }
    /*
     * Apply the procedure to nodes that have been specified
     * individually.
     */
    for ( /*empty*/ ; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    if ((*proc) (tvPtr, entryPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    if (nonMatchPtr != NULL) {
	*nonMatchPtr = invertMatch;	/* return "inverted search" status */
    }
    return TCL_OK;

  missingArg:
    Blt_ListDestroy(options);
    Tcl_AppendResult(interp, "missing pattern for search option \"", objv[i],
	"\"", (char *)NULL);
    return TCL_ERROR;

}

static int
FixSelectionsApplyProc(tvPtr, entryPtr)
    TreeView *tvPtr;
    TreeViewEntry *entryPtr;
{
    if (entryPtr->flags & ENTRY_HIDDEN) {
	Blt_TreeViewDeselectEntry(tvPtr, entryPtr);
	if ((tvPtr->focusPtr != NULL) &&
	    (Blt_TreeIsAncestor(entryPtr->node, tvPtr->focusPtr->node))) {
	    if (entryPtr != tvPtr->rootPtr) {
		entryPtr = Blt_TreeViewParentEntry(entryPtr);
		tvPtr->focusPtr = (entryPtr == NULL) 
		    ? tvPtr->focusPtr : entryPtr;
		Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
	    }
	}
	if ((tvPtr->selAnchorPtr != NULL) &&
	    (Blt_TreeIsAncestor(entryPtr->node, tvPtr->selAnchorPtr->node))) {
	    tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
	}
	if ((tvPtr->activePtr != NULL) &&
	    (Blt_TreeIsAncestor(entryPtr->node, tvPtr->activePtr->node))) {
	    tvPtr->activePtr = NULL;
	}
	Blt_TreeViewPruneSelection(tvPtr, entryPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * HideOp --
 *
 *	Hides one or more nodes.  Nodes can be specified by their
 *      inode, or by matching a name or data value pattern.  By
 *      default, the patterns are matched exactly.  They can also
 *      be matched using glob-style and regular expression rules.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
HideOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int status, nonmatching;

    status = SearchAndApplyToTree(tvPtr, interp, objc, objv, 
	HideEntryApplyProc, &nonmatching);

    if (status != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * If this was an inverted search, scan back through the
     * tree and make sure that the parents for all visible
     * nodes are also visible.  After all, if a node is supposed
     * to be visible, its parent can't be hidden.
     */
    if (nonmatching) {
	Blt_TreeViewApply(tvPtr, tvPtr->rootPtr, MapAncestorsApplyProc, 0);
    }
    /*
     * Make sure that selections are cleared from any hidden
     * nodes.  This wasn't done earlier--we had to delay it until
     * we fixed the visibility status for the parents.
     */
    Blt_TreeViewApply(tvPtr, tvPtr->rootPtr, FixSelectionsApplyProc, 0);

    /* Hiding an entry only effects the visible nodes. */
    tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ShowOp --
 *
 *	Mark one or more nodes to be exposed.  Nodes can be specified
 *	by their inode, or by matching a name or data value pattern.  By
 *      default, the patterns are matched exactly.  They can also
 *      be matched using glob-style and regular expression rules.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
ShowOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (SearchAndApplyToTree(tvPtr, interp, objc, objv, ShowEntryApplyProc,
	    (int *)NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *	Converts one of more words representing indices of the entries
 *	in the treeview widget to their respective serial identifiers.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain the
 *	identifier of each inode found. If an inode could not be found,
 *	then the serial identifier will be the empty string.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    char *string;
    TreeViewEntry *fromPtr;
    int usePath;

    usePath = FALSE;
    fromPtr = NULL;
    string = Tcl_GetString(objv[2]);
    if ((string[0] == '-') && (strcmp(string, "-path") == 0)) {
	usePath = TRUE;
	objv++, objc--;
    }
    if ((string[0] == '-') && (strcmp(string, "-at") == 0)) {
	if (Blt_TreeViewGetEntry(tvPtr, objv[3], &fromPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	objv += 2, objc -= 2;
    }
    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), 
		" index ?-at tagOrId? ?-path? tagOrId\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    tvPtr->fromPtr = fromPtr;
    if (tvPtr->fromPtr == NULL) {
	tvPtr->fromPtr = tvPtr->focusPtr;
    }
    if (tvPtr->fromPtr == NULL) {
	tvPtr->fromPtr = tvPtr->rootPtr;
    }
    if (usePath) {
	if (fromPtr == NULL) {
	    fromPtr = tvPtr->rootPtr;
	}
	string = Tcl_GetString(objv[2]);
	entryPtr = FindPath(tvPtr, fromPtr, string);
	if (entryPtr != NULL) {
	    Tcl_SetObjResult(interp, NodeToObj(entryPtr->node));
	}
    } else {
	if ((GetEntryFromObj2(tvPtr, objv[2], &entryPtr) == TCL_OK) && 
	    (entryPtr != NULL)) {
	    Tcl_SetObjResult(interp, NodeToObj(entryPtr->node));
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Add new entries into a hierarchy.  If no node is specified,
 *	new entries will be added to the root of the hierarchy.
 *
 *----------------------------------------------------------------------
 */
static int
InsertOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode node, parent;
    int insertPos;
    int depth, count;
    char *path;
    Tcl_Obj *CONST *options;
    Tcl_Obj *listObjPtr;
    char **compArr;
    register char **p;
    register int n;
    TreeViewEntry *rootPtr;
    char *string;

    rootPtr = tvPtr->rootPtr;
    string = Tcl_GetString(objv[2]);
    if ((string[0] == '-') && (strcmp(string, "-at") == 0)) {
	if (objc > 2) {
	    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &rootPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    objv += 2, objc -= 2;
	} else {
	    Tcl_AppendResult(interp, "missing argument for \"-at\" flag",
		     (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (objc == 2) {
	Tcl_AppendResult(interp, "missing position argument", (char *)NULL);
	return TCL_ERROR;
    }
    if (Blt_GetPositionFromObj(interp, objv[2], &insertPos) != TCL_OK) {
	return TCL_ERROR;
    }
    node = NULL;
    objc -= 3, objv += 3;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    while (objc > 0) {
	path = Tcl_GetString(objv[0]);
	objv++, objc--;

	/*
	 * Count the option-value pairs that follow.  Count until we
	 * spot one that looks like an entry name (i.e. doesn't start
	 * with a minus "-").
	 */
	for (count = 0; count < objc; count += 2) {
	    string = Tcl_GetString(objv[count]);
	    if (string[0] != '-') {
		break;
	    }
	}
	if (count > objc) {
	    count = objc;
	}
	options = objv;
	objc -= count, objv += count;

	if (tvPtr->trimLeft != NULL) {
	    register char *s1, *s2;

	    /* Trim off leading character string if one exists. */
	    for (s1 = path, s2 = tvPtr->trimLeft; *s2 != '\0'; s2++, s1++) {
		if (*s1 != *s2) {
		    break;
		}
	    }
	    if (*s2 == '\0') {
		path = s1;
	    }
	}
	/*
	 * Split the path and find the parent node of the path.
	 */
	compArr = &path;
	depth = 1;
	if (tvPtr->pathSep != SEPARATOR_NONE) {
	    if (SplitPath(tvPtr, path, &depth, &compArr) != TCL_OK) {
		goto error;
	    }
	    if (depth == 0) {
		Blt_Free(compArr);
		continue;		/* Root already exists. */
	    }
	}
	parent = rootPtr->node;
	depth--;		

	/* Verify each component in the path preceding the tail.  */
	for (n = 0, p = compArr; n < depth; n++, p++) {
	    node = Blt_TreeFindChild(parent, *p);
	    if (node == NULL) {
		if ((tvPtr->flags & TV_FILL_ANCESTORS) == 0) {
		    Tcl_AppendResult(interp, "can't find path component \"",
		         *p, "\" in \"", path, "\"", (char *)NULL);
		    goto error;
		}
		node = Blt_TreeCreateNode(tvPtr->tree, parent, *p, END);
		if (node == NULL) {
		    goto error;
		}
	    }
	    parent = node;
	}
	node = NULL;
	if (((tvPtr->flags & TV_ALLOW_DUPLICATES) == 0) && 
	    (Blt_TreeFindChild(parent, *p) != NULL)) {
	    Tcl_AppendResult(interp, "entry \"", *p, "\" already exists in \"",
		 path, "\"", (char *)NULL);
	    goto error;
	}
	node = Blt_TreeCreateNode(tvPtr->tree, parent, *p, insertPos);
	if (node == NULL) {
	    goto error;
	}
	if (Blt_TreeViewCreateEntry(tvPtr, node, count, options, 0) != TCL_OK) {
	    goto error;
	}
	if (compArr != &path) {
	    Blt_Free(compArr);
	}
	Tcl_ListObjAppendElement(interp, listObjPtr, NodeToObj(node));
    }
    tvPtr->flags |= (TV_LAYOUT | TV_SCROLL | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;

  error:
    if (compArr != &path) {
	Blt_Free(compArr);
    }
    Tcl_DecrRefCount(listObjPtr);
    if (node != NULL) {
	DeleteNode(tvPtr, node);
    }
    return TCL_ERROR;
}

#ifdef notdef
/*
 *----------------------------------------------------------------------
 *
 * AddOp --
 *
 *	Add new entries into a hierarchy.  If no node is specified,
 *	new entries will be added to the root of the hierarchy.
 *
 *----------------------------------------------------------------------
 */

static Blt_SwitchParseProc StringToChild;
#define INSERT_BEFORE	(ClientData)0
#define INSERT_AFTER	(ClientData)1
static Blt_SwitchCustom beforeSwitch =
{
    StringToChild, (Blt_SwitchFreeProc *)NULL, INSERT_BEFORE,
};
static Blt_SwitchCustom afterSwitch =
{
    StringToChild, (Blt_SwitchFreeProc *)NULL, INSERT_AFTER,
};

typedef struct {
    int insertPos;
    Blt_TreeNode parent;
} InsertData;

static Blt_SwitchSpec insertSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-after", Blt_Offset(InsertData, insertPos), 0, 
	&afterSwitch},
    {BLT_SWITCH_INT_NONNEGATIVE, "-at", Blt_Offset(InsertData, insertPos), 0},
    {BLT_SWITCH_CUSTOM, "-before", Blt_Offset(InsertData, insertPos), 0,
	&beforeSwitch},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static int
AddOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode node, parent;
    int insertPos;
    int depth, count;
    char *path;
    Tcl_Obj *CONST *options;
    Tcl_Obj *listObjPtr;
    char **compArr;
    register char **p;
    register int n;
    TreeViewEntry *rootPtr;
    char *string;

    memset(&data, 0, sizeof(data));
    data.maxDepth = -1;
    data.cmdPtr = cmdPtr;

    /* Process any leading switches  */
    i = Blt_ProcessObjSwitches(interp, addSwitches, objc - 2, objv + 2, 
	     (char *)&data, BLT_CONFIG_OBJV_PARTIAL);
    if (i < 0) {
	return TCL_ERROR;
    }
    i += 2;
    /* Should have at the starting node */
    if (i >= objc) {
	Tcl_AppendResult(interp, "starting node argument is missing", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (Blt_TreeViewGetEntry(tvPtr, objv[i], &rootPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    objv += i, objc -= i;
    node = NULL;

    /* Process sections of path ?options? */
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    while (objc > 0) {
	path = Tcl_GetString(objv[0]);
	objv++, objc--;
	/*
	 * Count the option-value pairs that follow.  Count until we
	 * spot one that looks like an entry name (i.e. doesn't start
	 * with a minus "-").
	 */
	for (count = 0; count < objc; count += 2) {
	    if (!Blt_ObjIsOption(bltTreeViewEntrySpecs, objv[count], 0)) {
		break;
	    }
	}
	if (count > objc) {
	    count = objc;
	}
	options = objv;
	objc -= count, objv += count;

	if (tvPtr->trimLeft != NULL) {
	    register char *s1, *s2;

	    /* Trim off leading character string if one exists. */
	    for (s1 = path, s2 = tvPtr->trimLeft; *s2 != '\0'; s2++, s1++) {
		if (*s1 != *s2) {
		    break;
		}
	    }
	    if (*s2 == '\0') {
		path = s1;
	    }
	}
	/*
	 * Split the path and find the parent node of the path.
	 */
	compArr = &path;
	depth = 1;
	if (tvPtr->pathSep != SEPARATOR_NONE) {
	    if (SplitPath(tvPtr, path, &depth, &compArr) != TCL_OK) {
		goto error;
	    }
	    if (depth == 0) {
		Blt_Free(compArr);
		continue;		/* Root already exists. */
	    }
	}
	parent = rootPtr->node;
	depth--;		

	/* Verify each component in the path preceding the tail.  */
	for (n = 0, p = compArr; n < depth; n++, p++) {
	    node = Blt_TreeFindChild(parent, *p);
	    if (node == NULL) {
		if ((tvPtr->flags & TV_FILL_ANCESTORS) == 0) {
		    Tcl_AppendResult(interp, "can't find path component \"",
		         *p, "\" in \"", path, "\"", (char *)NULL);
		    goto error;
		}
		node = Blt_TreeCreateNode(tvPtr->tree, parent, *p, END);
		if (node == NULL) {
		    goto error;
		}
	    }
	    parent = node;
	}
	node = NULL;
	if (((tvPtr->flags & TV_ALLOW_DUPLICATES) == 0) && 
	    (Blt_TreeFindChild(parent, *p) != NULL)) {
	    Tcl_AppendResult(interp, "entry \"", *p, "\" already exists in \"",
		 path, "\"", (char *)NULL);
	    goto error;
	}
	node = Blt_TreeCreateNode(tvPtr->tree, parent, *p, insertPos);
	if (node == NULL) {
	    goto error;
	}
	if (Blt_TreeViewCreateEntry(tvPtr, node, count, options, 0) != TCL_OK) {
	    goto error;
	}
	if (compArr != &path) {
	    Blt_Free(compArr);
	}
	Tcl_ListObjAppendElement(interp, listObjPtr, NodeToObj(node));
    }
    tvPtr->flags |= (TV_LAYOUT | TV_SCROLL | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;

  error:
    if (compArr != &path) {
	Blt_Free(compArr);
    }
    Tcl_DecrRefCount(listObjPtr);
    if (node != NULL) {
	DeleteNode(tvPtr, node);
    }
    return TCL_ERROR;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes nodes from the hierarchy. Deletes one or more entries 
 *	(except root). In all cases, nodes are removed recursively.
 *
 *	Note: There's no need to explicitly clean up Entry structures 
 *	      or request a redraw of the widget. When a node is 
 *	      deleted in the tree, all of the Tcl_Objs representing
 *	      the various data fields are also removed.  The treeview 
 *	      widget store the Entry structure in a data field. So it's
 *	      automatically cleaned up when FreeEntryInternalRep is
 *	      called.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
DeleteOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewTagInfo info;
    TreeViewEntry *entryPtr;
    register int i;

    for (i = 2; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    if (entryPtr == tvPtr->rootPtr) {
		Blt_TreeNode next, node;

		/* 
		 *   Don't delete the root node.  We implicitly assume
		 *   that even an empty tree has at a root.  Instead
		 *   delete all the children regardless if they're closed
		 *   or hidden.
		 */
		for (node = Blt_TreeFirstChild(entryPtr->node); node != NULL; 
		     node = next) {
		    next = Blt_TreeNextSibling(node);
		    DeleteNode(tvPtr, node);
		}
	    } else {
		DeleteNode(tvPtr, entryPtr->node);
	    }
	}
    } 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	Move an entry into a new location in the hierarchy.
 *
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MoveOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode parent;
    TreeViewEntry *srcPtr, *destPtr;
    char c;
    int action;
    char *string;
    TreeViewTagInfo info;

#define MOVE_INTO	(1<<0)
#define MOVE_BEFORE	(1<<1)
#define MOVE_AFTER	(1<<2)
    if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[2], &info) != TCL_OK) {
	return TCL_ERROR;
    }
    string = Tcl_GetString(objv[3]);
    c = string[0];
    if ((c == 'i') && (strcmp(string, "into") == 0)) {
	action = MOVE_INTO;
    } else if ((c == 'b') && (strcmp(string, "before") == 0)) {
	action = MOVE_BEFORE;
    } else if ((c == 'a') && (strcmp(string, "after") == 0)) {
	action = MOVE_AFTER;
    } else {
	Tcl_AppendResult(interp, "bad position \"", string,
	    "\": should be into, before, or after", (char *)NULL);
	return TCL_ERROR;
    }
    if (Blt_TreeViewGetEntry(tvPtr, objv[4], &destPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    for (srcPtr = Blt_TreeViewFirstTaggedEntry(&info); srcPtr != NULL; 
	 srcPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	/* Verify they aren't ancestors. */
	if (Blt_TreeIsAncestor(srcPtr->node, destPtr->node)) {
	    Tcl_DString dString;
	    char *path;

	    path = Blt_TreeViewGetFullName(tvPtr, srcPtr, 1, &dString);
	    Tcl_AppendResult(interp, "can't move node: \"", path, 
			"\" is an ancestor of \"", Tcl_GetString(objv[4]), 
			"\"", (char *)NULL);
	    Tcl_DStringFree(&dString);
	    return TCL_ERROR;
	}
	parent = Blt_TreeNodeParent(destPtr->node);
	if (parent == NULL) {
	    action = MOVE_INTO;
	}
	switch (action) {
	case MOVE_INTO:
	    Blt_TreeMoveNode(tvPtr->tree, srcPtr->node, destPtr->node, 
			     (Blt_TreeNode)NULL);
	    break;
	    
	case MOVE_BEFORE:
	    Blt_TreeMoveNode(tvPtr->tree, srcPtr->node, parent, destPtr->node);
	    break;
	    
	case MOVE_AFTER:
	    Blt_TreeMoveNode(tvPtr->tree, srcPtr->node, parent, 
			     Blt_TreeNextSibling(destPtr->node));
	    break;
	}
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewButton *buttonPtr = &tvPtr->button;
    int x, y;			/* Screen coordinates of the test point. */
    register TreeViewEntry *entryPtr;
    int isRoot;
    char *string;

    isRoot = FALSE;
    string = Tcl_GetString(objv[2]);
    if (strcmp("-root", string) == 0) {
	isRoot = TRUE;
	objv++, objc--;
    } 
    if (objc < 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " ", Tcl_GetString(objv[1]), 
		" ?-root? x y\"", (char *)NULL);
	return TCL_ERROR;
			 
    }
    if ((Tk_GetPixelsFromObj(interp, tvPtr->tkwin, objv[2], &x) != TCL_OK) ||
	(Tk_GetPixelsFromObj(interp, tvPtr->tkwin, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (tvPtr->nVisible == 0) {
	return TCL_OK;
    }
    if (isRoot) {
	int rootX, rootY;

	Tk_GetRootCoords(tvPtr->tkwin, &rootX, &rootY);
	x -= rootX;
	y -= rootY;
    }
    entryPtr = Blt_TreeViewNearestEntry(tvPtr, x, y, TRUE);
    if (entryPtr == NULL) {
	return TCL_OK;
    }
    x = WORLDX(tvPtr, x);
    y = WORLDY(tvPtr, y);
    if (objc > 4) {
	char *where;
	int labelX, labelY, depth;
	TreeViewIcon icon;

	where = "";
	if (entryPtr->flags & ENTRY_HAS_BUTTON) {
	    int buttonX, buttonY;

	    buttonX = entryPtr->worldX + entryPtr->buttonX;
	    buttonY = entryPtr->worldY + entryPtr->buttonY;
	    if ((x >= buttonX) && (x < (buttonX + buttonPtr->width)) &&
		(y >= buttonY) && (y < (buttonY + buttonPtr->height))) {
		where = "button";
		goto done;
	    }
	} 
	depth = DEPTH(tvPtr, entryPtr->node);

	icon = Blt_TreeViewGetEntryIcon(tvPtr, entryPtr);
	if (icon != NULL) {
	    int iconWidth, iconHeight, entryHeight;
	    int iconX, iconY;
	    
	    entryHeight = MAX(entryPtr->iconHeight, tvPtr->button.height);
	    iconHeight = TreeViewIconHeight(icon);
	    iconWidth = TreeViewIconWidth(icon);
	    iconX = entryPtr->worldX + ICONWIDTH(depth);
	    iconY = entryPtr->worldY;
	    if (tvPtr->flatView) {
		iconX += (ICONWIDTH(0) - iconWidth) / 2;
	    } else {
		iconX += (ICONWIDTH(depth + 1) - iconWidth) / 2;
	    }	    
	    iconY += (entryHeight - iconHeight) / 2;
	    if ((x >= iconX) && (x <= (iconX + iconWidth)) &&
		(y >= iconY) && (y < (iconY + iconHeight))) {
		where = "icon";
		goto done;
	    }
	}
	labelX = entryPtr->worldX + ICONWIDTH(depth);
	labelY = entryPtr->worldY;
	if (!tvPtr->flatView) {
	    labelX += ICONWIDTH(depth + 1) + 4;
	}	    
	if ((x >= labelX) && (x < (labelX + entryPtr->labelWidth)) &&
	    (y >= labelY) && (y < (labelY + entryPtr->labelHeight))) {
	    where = "label";
	}
    done:
	if (Tcl_SetVar(interp, Tcl_GetString(objv[4]), where, 
		TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_SetObjResult(interp, NodeToObj(entryPtr->node));
    return TCL_OK;
}


/*ARGSUSED*/
static int
OpenOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    TreeViewTagInfo info;
    int recurse, result;
    register int i;

    recurse = FALSE;
    if (objc > 2) {
	int length;
	char *string;

	string = Tcl_GetStringFromObj(objv[2], &length);
	if ((string[0] == '-') && (length > 1) && 
	    (strncmp(string, "-recurse", length) == 0)) {
	    objv++, objc--;
	    recurse = TRUE;
	}
    }
    for (i = 2; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    if (recurse) {
		result = Blt_TreeViewApply(tvPtr, entryPtr, 
					   Blt_TreeViewOpenEntry, 0);
	    } else {
		result = Blt_TreeViewOpenEntry(tvPtr, entryPtr);
	    }
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	    /* Make sure ancestors of this node aren't hidden. */
	    MapAncestors(tvPtr, entryPtr);
	}
    }
    /*FIXME: This is only for flattened entries.  */
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY | TV_RESORT);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RangeOp --
 *
 *	Returns the node identifiers in a given range.
 *
 *----------------------------------------------------------------------
 */
static int
RangeOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr, *firstPtr, *lastPtr;
    unsigned int mask;
    int length;
    Tcl_Obj *listObjPtr, *objPtr;
    char *string;

    mask = 0;
    string = Tcl_GetStringFromObj(objv[2], &length);
    if ((string[0] == '-') && (length > 1) && 
	(strncmp(string, "-open", length) == 0)) {
	objv++, objc--;
	mask |= ENTRY_CLOSED;
    }
    if (Blt_TreeViewGetEntry(tvPtr, objv[2], &firstPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 3) {
	if (Blt_TreeViewGetEntry(tvPtr, objv[3], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	lastPtr = LastEntry(tvPtr, firstPtr, mask);
    }    
    if (mask & ENTRY_CLOSED) {
	if (firstPtr->flags & ENTRY_HIDDEN) {
	    Tcl_AppendResult(interp, "first node \"", Tcl_GetString(objv[2]), 
		"\" is hidden.", (char *)NULL);
	    return TCL_ERROR;
	}
	if (lastPtr->flags & ENTRY_HIDDEN) {
	    Tcl_AppendResult(interp, "last node \"", Tcl_GetString(objv[3]), 
		"\" is hidden.", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * The relative order of the first/last markers determines the
     * direction.
     */
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (Blt_TreeIsBefore(lastPtr->node, firstPtr->node)) {
	for (entryPtr = lastPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewPrevEntry(entryPtr, mask)) {
	    objPtr = NodeToObj(entryPtr->node);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    if (entryPtr == firstPtr) {
		break;
	    }
	}
    } else {
	for (entryPtr = firstPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextEntry(entryPtr, mask)) {
	    objPtr = NodeToObj(entryPtr->node);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    if (entryPtr == lastPtr) {
		break;
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanOp --
 *
 *	Implements the quick scan.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ScanOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    int x, y;
    char c;
    int length;
    int oper;
    char *string;
    Tk_Window tkwin;

#define SCAN_MARK	1
#define SCAN_DRAGTO	2
    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    tkwin = tvPtr->tkwin;
    if ((c == 'm') && (strncmp(string, "mark", length) == 0)) {
	oper = SCAN_MARK;
    } else if ((c == 'd') && (strncmp(string, "dragto", length) == 0)) {
	oper = SCAN_DRAGTO;
    } else {
	Tcl_AppendResult(interp, "bad scan operation \"", string,
	    "\": should be either \"mark\" or \"dragto\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((Blt_GetPixelsFromObj(interp, tkwin, objv[3], 0, &x) != TCL_OK) ||
	(Blt_GetPixelsFromObj(interp, tkwin, objv[4], 0, &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (oper == SCAN_MARK) {
	tvPtr->scanAnchorX = x;
	tvPtr->scanAnchorY = y;
	tvPtr->scanX = tvPtr->xOffset;
	tvPtr->scanY = tvPtr->yOffset;
    } else {
	int worldX, worldY;
	int dx, dy;

	dx = tvPtr->scanAnchorX - x;
	dy = tvPtr->scanAnchorY - y;
	worldX = tvPtr->scanX + (10 * dx);
	worldY = tvPtr->scanY + (10 * dy);

	if (worldX < 0) {
	    worldX = 0;
	} else if (worldX >= tvPtr->worldWidth) {
	    worldX = tvPtr->worldWidth - tvPtr->xScrollUnits;
	}
	if (worldY < 0) {
	    worldY = 0;
	} else if (worldY >= tvPtr->worldHeight) {
	    worldY = tvPtr->worldHeight - tvPtr->yScrollUnits;
	}
	tvPtr->xOffset = worldX;
	tvPtr->yOffset = worldY;
	tvPtr->flags |= TV_SCROLL;
	Blt_TreeViewEventuallyRedraw(tvPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SeeOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    int width, height;
    int x, y;
    Tk_Anchor anchor;
    int left, right, top, bottom;
    char *string;

    string = Tcl_GetString(objv[2]);
    anchor = TK_ANCHOR_W;	/* Default anchor is West */
    if ((string[0] == '-') && (strcmp(string, "-anchor") == 0)) {
	if (objc == 3) {
	    Tcl_AppendResult(interp, "missing \"-anchor\" argument",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (Tk_GetAnchorFromObj(interp, objv[3], &anchor) != TCL_OK) {
	    return TCL_ERROR;
	}
	objc -= 2, objv += 2;
    }
    if (objc == 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", objv[0],
	    "see ?-anchor anchor? tagOrId\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetEntryFromObj(tvPtr, objv[2], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (entryPtr == NULL) {
	return TCL_OK;
    }
    if (entryPtr->flags & ENTRY_HIDDEN) {
	MapAncestors(tvPtr, entryPtr);
	tvPtr->flags |= TV_SCROLL;
	/*
	 * If the entry wasn't previously exposed, its world coordinates
	 * aren't likely to be valid.  So re-compute the layout before
	 * we try to see the viewport to the entry's location.
	 */
	Blt_TreeViewComputeLayout(tvPtr);
    }
    width = VPORTWIDTH(tvPtr);
    height = VPORTHEIGHT(tvPtr);

    /*
     * XVIEW:	If the entry is left or right of the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    left = tvPtr->xOffset;
    right = tvPtr->xOffset + width;

    switch (anchor) {
    case TK_ANCHOR_W:
    case TK_ANCHOR_NW:
    case TK_ANCHOR_SW:
	x = 0;
	break;
    case TK_ANCHOR_E:
    case TK_ANCHOR_NE:
    case TK_ANCHOR_SE:
	x = entryPtr->worldX + entryPtr->width + 
	    ICONWIDTH(DEPTH(tvPtr, entryPtr->node)) - width;
	break;
    default:
	if (entryPtr->worldX < left) {
	    x = entryPtr->worldX;
	} else if ((entryPtr->worldX + entryPtr->width) > right) {
	    x = entryPtr->worldX + entryPtr->width - width;
	} else {
	    x = tvPtr->xOffset;
	}
	break;
    }
    /*
     * YVIEW:	If the entry is above or below the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    top = tvPtr->yOffset;
    bottom = tvPtr->yOffset + height;

    switch (anchor) {
    case TK_ANCHOR_N:
	y = tvPtr->yOffset;
	break;
    case TK_ANCHOR_NE:
    case TK_ANCHOR_NW:
	y = entryPtr->worldY - (height / 2);
	break;
    case TK_ANCHOR_S:
    case TK_ANCHOR_SE:
    case TK_ANCHOR_SW:
	y = entryPtr->worldY + entryPtr->height - height;
	break;
    default:
	if (entryPtr->worldY < top) {
	    y = entryPtr->worldY;
	} else if ((entryPtr->worldY + entryPtr->height) > bottom) {
	    y = entryPtr->worldY + entryPtr->height - height;
	} else {
	    y = tvPtr->yOffset;
	}
	break;
    }
    if ((y != tvPtr->yOffset) || (x != tvPtr->xOffset)) {
	/* tvPtr->xOffset = x; */
	tvPtr->yOffset = y;
	tvPtr->flags |= TV_SCROLL;
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

void
Blt_TreeViewClearSelection(tvPtr)
    TreeView *tvPtr;
{
    Blt_DeleteHashTable(&tvPtr->selectTable);
    Blt_InitHashTable(&tvPtr->selectTable, BLT_ONE_WORD_KEYS);
    Blt_ChainReset(tvPtr->selChainPtr);
    Blt_TreeViewEventuallyRedraw(tvPtr);
    if (tvPtr->selectCmd != NULL) {
	EventuallyInvokeSelectCmd(tvPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LostSelection --
 *
 *	This procedure is called back by Tk when the selection is grabbed
 *	away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The existing selection is unhighlighted, and the window is
 *	marked as not containing a selection.
 *
 *----------------------------------------------------------------------
 */
static void
LostSelection(clientData)
    ClientData clientData;	/* Information about the widget. */
{
    TreeView *tvPtr = clientData;

    if ((tvPtr->flags & TV_SELECT_EXPORT) == 0) {
	return;
    }
    Blt_TreeViewClearSelection(tvPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SelectRange --
 *
 *	Sets the selection flag for a range of nodes.  The range is
 *	determined by two pointers which designate the first/last
 *	nodes of the range.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SelectRange(tvPtr, fromPtr, toPtr)
    TreeView *tvPtr;
    TreeViewEntry *fromPtr, *toPtr;
{
    if (tvPtr->flatView) {
	register int i;

	if (fromPtr->flatIndex > toPtr->flatIndex) {
	    for (i = fromPtr->flatIndex; i >= toPtr->flatIndex; i--) {
		SelectEntryApplyProc(tvPtr, tvPtr->flatArr[i]);
	    }
	} else {
	    for (i = fromPtr->flatIndex; i <= toPtr->flatIndex; i++) {
		SelectEntryApplyProc(tvPtr, tvPtr->flatArr[i]);
	    }
	}	    
    } else {
	TreeViewEntry *entryPtr;
	TreeViewIterProc *proc;
	/* From the range determine the direction to select entries. */

	proc = (Blt_TreeIsBefore(toPtr->node, fromPtr->node)) 
	    ? Blt_TreeViewPrevEntry : Blt_TreeViewNextEntry;
	for (entryPtr = fromPtr; entryPtr != NULL;
	     entryPtr = (*proc)(entryPtr, ENTRY_MASK)) {
	    SelectEntryApplyProc(tvPtr, entryPtr);
	    if (entryPtr == toPtr) {
		break;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionAnchorOp --
 *
 *	Sets the selection anchor to the element given by a index.
 *	The selection anchor is the end of the selection that is fixed
 *	while dragging out a selection with the mouse.  The index
 *	"anchor" may be used to refer to the anchor element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionAnchorOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;

    if (GetEntryFromObj(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Set both the anchor and the mark. Indicates that a single entry
     * is selected. */
    tvPtr->selAnchorPtr = entryPtr;
    tvPtr->selMarkPtr = NULL;
    if (entryPtr != NULL) {
	Tcl_SetObjResult(interp, NodeToObj(entryPtr->node));
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SelectionClearallOp
 *
 *	Clears the entire selection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionClearallOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;	/* Not used. */
{
    Blt_TreeViewClearSelection(tvPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionIncludesOp
 *
 *	Returns 1 if the element indicated by index is currently
 *	selected, 0 if it isn't.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionIncludesOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    int bool;

    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    bool = Blt_TreeViewEntryIsSelected(tvPtr, entryPtr);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionMarkOp --
 *
 *	Sets the selection mark to the element given by a index.
 *	The selection anchor is the end of the selection that is movable
 *	while dragging out a selection with the mouse.  The index
 *	"mark" may be used to refer to the anchor element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionMarkOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;

    if (GetEntryFromObj(tvPtr, objv[3], &entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tvPtr->selAnchorPtr == NULL) {
	Tcl_AppendResult(interp, "selection anchor must be set first", 
		 (char *)NULL);
	return TCL_ERROR;
    }
    if (tvPtr->selMarkPtr != entryPtr) {
	Blt_ChainLink *linkPtr, *nextPtr;
	TreeViewEntry *selectPtr;

	/* Deselect entry from the list all the way back to the anchor. */
	for (linkPtr = Blt_ChainLastLink(tvPtr->selChainPtr); linkPtr != NULL; 
	     linkPtr = nextPtr) {
	    nextPtr = Blt_ChainPrevLink(linkPtr);
	    selectPtr = Blt_ChainGetValue(linkPtr);
	    if (selectPtr == tvPtr->selAnchorPtr) {
		break;
	    }
	    Blt_TreeViewDeselectEntry(tvPtr, selectPtr);
	}
	tvPtr->flags &= ~TV_SELECT_MASK;
	tvPtr->flags |= TV_SELECT_SET;
	SelectRange(tvPtr, tvPtr->selAnchorPtr, entryPtr);
	Tcl_SetObjResult(interp, NodeToObj(entryPtr->node));
	tvPtr->selMarkPtr = entryPtr;

	Blt_TreeViewEventuallyRedraw(tvPtr);
	if (tvPtr->selectCmd != NULL) {
	    EventuallyInvokeSelectCmd(tvPtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionPresentOp
 *
 *	Returns 1 if there is a selection and 0 if it isn't.
 *
 * Results:
 *	A standard Tcl result.  interp->result will contain a
 *	boolean string indicating if there is a selection.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionPresentOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    int bool;

    bool = (Blt_ChainGetLength(tvPtr->selChainPtr) > 0);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionSetOp
 *
 *	Selects, deselects, or toggles all of the elements in the
 *	range between first and last, inclusive, without affecting the
 *	selection state of elements outside that range.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectionSetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *firstPtr, *lastPtr;
    char *string;

    tvPtr->flags &= ~TV_SELECT_MASK;
    string = Tcl_GetString(objv[2]);
    switch (string[0]) {
    case 's':
	tvPtr->flags |= TV_SELECT_SET;
	break;
    case 'c':
	tvPtr->flags |= TV_SELECT_CLEAR;
	break;
    case 't':
	tvPtr->flags |= TV_SELECT_TOGGLE;
	break;
    }
    if (Blt_TreeViewGetEntry(tvPtr, objv[3], &firstPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((firstPtr->flags & ENTRY_HIDDEN) && 
	(!(tvPtr->flags & TV_SELECT_CLEAR))) {
	Tcl_AppendResult(interp, "can't select hidden node \"", 
		Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    lastPtr = firstPtr;
    if (objc > 4) {
	if (Blt_TreeViewGetEntry(tvPtr, objv[4], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((lastPtr->flags & ENTRY_HIDDEN) && 
		(!(tvPtr->flags & TV_SELECT_CLEAR))) {
	    Tcl_AppendResult(interp, "can't select hidden node \"", 
		     Tcl_GetString(objv[4]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (firstPtr == lastPtr) {
	SelectEntryApplyProc(tvPtr, firstPtr);
    } else {
	SelectRange(tvPtr, firstPtr, lastPtr);
    }
    /* Set both the anchor and the mark. Indicates that a single entry
     * is selected. */
    if (tvPtr->selAnchorPtr == NULL) {
	tvPtr->selAnchorPtr = firstPtr;
    }
    if (tvPtr->flags & TV_SELECT_EXPORT) {
	Tk_OwnSelection(tvPtr->tkwin, XA_PRIMARY, LostSelection, tvPtr);
    }
    Blt_TreeViewEventuallyRedraw(tvPtr);
    if (tvPtr->selectCmd != NULL) {
	EventuallyInvokeSelectCmd(tvPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionOp --
 *
 *	This procedure handles the individual options for text
 *	selections.  The selected text is designated by start and end
 *	indices into the text pool.  The selected segment has both a
 *	anchored and unanchored ends.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec selectionOps[] =
{
    {"anchor", 1, (Blt_Op)SelectionAnchorOp, 4, 4, "tagOrId",},
    {"clear", 5, (Blt_Op)SelectionSetOp, 4, 5, "first ?last?",},
    {"clearall", 6, (Blt_Op)SelectionClearallOp, 3, 3, "",},
    {"includes", 1, (Blt_Op)SelectionIncludesOp, 4, 4, "tagOrId",},
    {"mark", 1, (Blt_Op)SelectionMarkOp, 4, 4, "tagOrId",},
    {"present", 1, (Blt_Op)SelectionPresentOp, 3, 3, "",},
    {"set", 1, (Blt_Op)SelectionSetOp, 4, 5, "first ?last?",},
    {"toggle", 1, (Blt_Op)SelectionSetOp, 4, 5, "first ?last?",},
};
static int nSelectionOps = sizeof(selectionOps) / sizeof(Blt_OpSpec);

static int
SelectionOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nSelectionOps, selectionOps, BLT_OP_ARG2, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (tvPtr, interp, objc, objv);
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * TagForgetOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TagForgetOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    register int i;

    for (i = 3; i < objc; i++) {
	Blt_TreeForgetTag(tvPtr->tree, Tcl_GetString(objv[i]));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagNamesOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagNamesOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    objPtr = Tcl_NewStringObj("all", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (objc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	Blt_TreeTagEntry *tPtr;

	objPtr = Tcl_NewStringObj("root", -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	for (hPtr = Blt_TreeFirstTag(tvPtr->tree, &cursor); hPtr != NULL;
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    objPtr = Tcl_NewStringObj(tPtr->tagName, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	register int i;
	TreeViewEntry *entryPtr;
	Blt_List list;
	Blt_ListNode listNode;

	for (i = 3; i < objc; i++) {
	    if (Blt_TreeViewGetEntry(tvPtr, objv[i], &entryPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    list = Blt_ListCreate(BLT_ONE_WORD_KEYS);
	    Blt_TreeViewGetTags(interp, tvPtr, entryPtr, list);
	    for (listNode = Blt_ListFirstNode(list); listNode != NULL; 
		 listNode = Blt_ListNextNode(listNode)) {
		objPtr = Tcl_NewStringObj(Blt_ListGetKey(listNode), -1);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	    Blt_ListDestroy(list);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagNodesOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagNodesOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable nodeTable;
    Blt_TreeNode node;
    TreeViewTagInfo info;
    Tcl_Obj *listObjPtr;
    Tcl_Obj *objPtr;
    TreeViewEntry *entryPtr;
    int isNew;
    register int i;

    Blt_InitHashTable(&nodeTable, BLT_ONE_WORD_KEYS);
    for (i = 3; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    Blt_CreateHashEntry(&nodeTable, (char *)entryPtr->node, &isNew);
	}
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&nodeTable, &cursor); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&cursor)) {
	node = (Blt_TreeNode)Blt_GetHashKey(&nodeTable, hPtr);
	objPtr = Tcl_NewIntObj(Blt_TreeNodeId(node));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_DeleteHashTable(&nodeTable);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagAddOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagAddOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    register int i;
    char *tagName;
    TreeViewTagInfo info;

    tagName = Tcl_GetString(objv[3]);
    tvPtr->fromPtr = NULL;
    if (strcmp(tagName, "root") == 0) {
	Tcl_AppendResult(interp, "can't add reserved tag \"", tagName, "\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (isdigit(UCHAR(tagName[0]))) {
	Tcl_AppendResult(interp, "invalid tag \"", tagName, 
		 "\": can't start with digit", (char *)NULL);
	return TCL_ERROR;
    }
    if (tagName[0] == '@') {
	Tcl_AppendResult(tvPtr->interp, "invalid tag \"", tagName, 
		"\": can't start with \"@\"", (char *)NULL);
	return TCL_ERROR;
    } 
    if (GetEntryFromSpecialId(tvPtr, tagName, &entryPtr) == TCL_OK) {
	Tcl_AppendResult(interp, "invalid tag \"", tagName, 
		 "\": is a special id", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
	if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	     entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	    if (AddTag(tvPtr, entryPtr->node, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * TagDeleteOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TagDeleteOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    char *tagName;
    Blt_HashTable *tablePtr;
    TreeViewTagInfo info;

    tagName = Tcl_GetString(objv[3]);
    tablePtr = Blt_TreeTagHashTable(tvPtr->tree, tagName);
    if (tablePtr != NULL) {
        register int i;
        Blt_HashEntry *hPtr;
	TreeViewEntry *entryPtr;

        for (i = 4; i < objc; i++) {
	    if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[i], &info)!= TCL_OK) {
		return TCL_ERROR;
	    }
	    for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); 
		entryPtr != NULL; 
		entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	        hPtr = Blt_FindHashEntry(tablePtr, (char *)entryPtr->node);
	        if (hPtr != NULL) {
		    Blt_DeleteHashEntry(tablePtr, hPtr);
	        }
	   }
       }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagOp --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec tagOps[] = {
    {"add", 1, (Blt_Op)TagAddOp, 5, 0, "tag id...",},
    {"delete", 2, (Blt_Op)TagDeleteOp, 5, 0, "tag id...",},
    {"forget", 1, (Blt_Op)TagForgetOp, 4, 0, "tag...",},
    {"names", 2, (Blt_Op)TagNamesOp, 3, 0, "?id...?",}, 
    {"nodes", 2, (Blt_Op)TagNodesOp, 4, 0, "tag ?tag...?",},
};

static int nTagOps = sizeof(tagOps) / sizeof(Blt_OpSpec);

static int
TagOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nTagOps, tagOps, BLT_OP_ARG2, objc, objv, 
	0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(tvPtr, interp, objc, objv);
    return result;
}

/*ARGSUSED*/
static int
ToggleOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    TreeViewEntry *entryPtr;
    TreeViewTagInfo info;

    if (Blt_TreeViewFindTaggedEntries(tvPtr, objv[2], &info) != TCL_OK) {
	return TCL_ERROR;
    }
    for (entryPtr = Blt_TreeViewFirstTaggedEntry(&info); entryPtr != NULL; 
	 entryPtr = Blt_TreeViewNextTaggedEntry(&info)) {
	if (entryPtr == NULL) {
	    return TCL_OK;
	}
	if (entryPtr->flags & ENTRY_CLOSED) {
	    Blt_TreeViewOpenEntry(tvPtr, entryPtr);
	} else {
	    Blt_TreeViewPruneSelection(tvPtr, entryPtr);
	    if ((tvPtr->focusPtr != NULL) && 
		(Blt_TreeIsAncestor(entryPtr->node, tvPtr->focusPtr->node))) {
		tvPtr->focusPtr = entryPtr;
		Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
	    }
	    if ((tvPtr->selAnchorPtr != NULL) &&
		(Blt_TreeIsAncestor(entryPtr->node, 
				    tvPtr->selAnchorPtr->node))) {
		tvPtr->selAnchorPtr = NULL;
	    }
	    Blt_TreeViewCloseEntry(tvPtr, entryPtr);
	}
    }
    tvPtr->flags |= TV_SCROLL;
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

static int
XViewOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int width, worldWidth;

    width = VPORTWIDTH(tvPtr);
    worldWidth = tvPtr->worldWidth;
    if (objc == 2) {
	double fract;
	Tcl_Obj *listObjPtr;

	/*
	 * Note that we are bounding the fractions between 0.0 and 1.0
	 * to support the "canvas"-style of scrolling.
	 */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	fract = (double)tvPtr->xOffset / worldWidth;
	fract = CLAMP(fract, 0.0, 1.0);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(fract));
	fract = (double)(tvPtr->xOffset + width) / worldWidth;
	fract = CLAMP(fract, 0.0, 1.0);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(fract));
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    if (Blt_GetScrollInfoFromObj(interp, objc - 2, objv + 2, &tvPtr->xOffset,
	    worldWidth, width, tvPtr->xScrollUnits, tvPtr->scrollMode) 
	    != TCL_OK) {
	return TCL_ERROR;
    }
    tvPtr->flags |= TV_XSCROLL;
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

static int
YViewOp(tvPtr, interp, objc, objv)
    TreeView *tvPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int height, worldHeight;

    height = VPORTHEIGHT(tvPtr);
    worldHeight = tvPtr->worldHeight;
    if (objc == 2) {
	double fract;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	/* Report first and last fractions */
	fract = (double)tvPtr->yOffset / worldHeight;
	fract = CLAMP(fract, 0.0, 1.0);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(fract));
	fract = (double)(tvPtr->yOffset + height) / worldHeight;
	fract = CLAMP(fract, 0.0, 1.0);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(fract));
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    if (Blt_GetScrollInfoFromObj(interp, objc - 2, objv + 2, &tvPtr->yOffset,
	    worldHeight, height, tvPtr->yScrollUnits, tvPtr->scrollMode)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    tvPtr->flags |= TV_SCROLL;
    Blt_TreeViewEventuallyRedraw(tvPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * Blt_TreeViewWidgetInstCmd --
 *
 * 	This procedure is invoked to process commands on behalf of
 *	the treeview widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */
static Blt_OpSpec treeViewOps[] =
{
    {"bbox", 2, (Blt_Op)BboxOp, 3, 0, "tagOrId...",}, 
    {"bind", 2, (Blt_Op)BindOp, 3, 5, "tagName ?sequence command?",}, 
    {"button", 2, (Blt_Op)ButtonOp, 2, 0, "args",},
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "option",}, 
    {"close", 2, (Blt_Op)CloseOp, 2, 0, "?-recurse? tagOrId...",}, 
    {"column", 3, (Blt_Op)Blt_TreeViewColumnOp, 2, 0, "oper args",}, 
    {"configure", 3, (Blt_Op)ConfigureOp, 2, 0, "?option value?...",},
    {"curselection", 2, (Blt_Op)CurselectionOp, 2, 2, "",},
    {"delete", 1, (Blt_Op)DeleteOp, 2, 0, "tagOrId ?tagOrId...?",}, 
    {"edit", 2, (Blt_Op)EditOp, 4, 6, "?-root|-test? x y",},
    {"entry", 2, (Blt_Op)EntryOp, 2, 0, "oper args",},
    {"find", 2, (Blt_Op)FindOp, 2, 0, "?flags...? ?first last?",}, 
    {"focus", 2, (Blt_Op)FocusOp, 3, 3, "tagOrId",}, 
    {"get", 1, (Blt_Op)GetOp, 2, 0, "?-full? tagOrId ?tagOrId...?",}, 
    {"hide", 1, (Blt_Op)HideOp, 2, 0, "?-exact? ?-glob? ?-regexp? ?-nonmatching? ?-name string? ?-full string? ?-data string? ?--? ?tagOrId...?",},
    {"index", 3, (Blt_Op)IndexOp, 3, 6, "?-at tagOrId? ?-path? string",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0, "?-at tagOrId? position label ?label...? ?option value?",},
    {"move", 1, (Blt_Op)MoveOp, 5, 5, "tagOrId into|before|after tagOrId",},
    {"nearest", 1, (Blt_Op)NearestOp, 4, 5, "x y ?varName?",}, 
    {"open", 1, (Blt_Op)OpenOp, 2, 0, "?-recurse? tagOrId...",}, 
    {"range", 1, (Blt_Op)RangeOp, 4, 5, "?-open? tagOrId tagOrId",},
    {"scan", 2, (Blt_Op)ScanOp, 5, 5, "dragto|mark x y",},
    {"see", 3, (Blt_Op)SeeOp, 3, 0, "?-anchor anchor? tagOrId",},
    {"selection", 3, (Blt_Op)SelectionOp, 2, 0, "oper args",},
    {"show", 2, (Blt_Op)ShowOp, 2, 0, "?-exact? ?-glob? ?-regexp? ?-nonmatching? ?-name string? ?-full string? ?-data string? ?--? ?tagOrId...?",},
    {"sort", 2, (Blt_Op)Blt_TreeViewSortOp, 2, 0, "args",},
    {"style", 2, (Blt_Op)Blt_TreeViewStyleOp, 2, 0, "args",},
    {"tag", 2, (Blt_Op)TagOp, 2, 0, "oper args",},
    {"toggle", 2, (Blt_Op)ToggleOp, 3, 3, "tagOrId",},
    {"xview", 1, (Blt_Op)XViewOp, 2, 5, "?moveto fract? ?scroll number what?",},
    {"yview", 1, (Blt_Op)YViewOp, 2, 5, "?moveto fract? ?scroll number what?",},
};

static int nTreeViewOps = sizeof(treeViewOps) / sizeof(Blt_OpSpec);

int
Blt_TreeViewWidgetInstCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Information about the widget. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST *objv;	/* Vector of argument strings. */
{
    Blt_Op proc;
    TreeView *tvPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nTreeViewOps, treeViewOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(tvPtr);
    result = (*proc) (tvPtr, interp, objc, objv);
    Tcl_Release(tvPtr);
    return result;
}

#endif /* NO_TREEVIEW */
