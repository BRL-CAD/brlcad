
/*
 * bltTree.c --
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
 *	The "tree" data object was created by George A. Howlett.
 */

#include "bltInt.h"

/* TODO:
 *	traces and notifiers should be in one list in tree object.
 *	notifier is always fired.
 *	incorporate first/next tag routines ?
 */


#ifndef NO_TREE

#include "bltTree.h"

static Tcl_InterpDeleteProc TreeInterpDeleteProc;
static Blt_TreeApplyProc SizeApplyProc;
static Tcl_IdleProc NotifyIdleProc;

#define TREE_THREAD_KEY		"BLT Tree Data"
#define TREE_MAGIC		((unsigned int) 0x46170277)
#define TREE_DESTROYED		(1<<0)

typedef struct Blt_TreeNodeStruct Node;
typedef struct Blt_TreeClientStruct TreeClient;
typedef struct Blt_TreeObjectStruct TreeObject;
typedef struct Blt_TreeValueStruct Value;

/*
 * Blt_TreeValue --
 *
 *	Tree nodes contain heterogeneous data fields, represented as a
 *	chain of these structures.  Each field contains the key of the
 *	field (Blt_TreeKey) and the value (Tcl_Obj) containing the
 *	actual data representations.
 * 
 */
struct Blt_TreeValueStruct {
    Blt_TreeKey key;		/* String identifying the data field */
    Tcl_Obj *objPtr;		/* Data representation. */
    Blt_Tree owner;		/* Non-NULL if privately owned. */
    Blt_TreeValue next;		/* Next value in the chain. */
};

#include <stdio.h>
#include <string.h>
/* The following header is required for LP64 compilation */
#include <stdlib.h>

#include "bltHash.h"

static void TreeDestroyValues _ANSI_ARGS_((Blt_TreeNode node));

static Value *TreeFindValue _ANSI_ARGS_((Blt_TreeNode node,
	Blt_TreeKey key));
static Value *TreeCreateValue _ANSI_ARGS_((Blt_TreeNode node,
	Blt_TreeKey key, int *newPtr));

static int TreeDeleteValue _ANSI_ARGS_((Blt_TreeNode node, 
	Blt_TreeValue value));

static Value *TreeFirstValue _ANSI_ARGS_((Blt_TreeNode, 
	Blt_TreeKeySearch *searchPtr));

static Value *TreeNextValue _ANSI_ARGS_((Blt_TreeKeySearch *srchPtr));

/*
 * When there are this many entries per bucket, on average, rebuild
 * the hash table to make it larger.
 */

#define REBUILD_MULTIPLIER	3

#if (SIZEOF_VOID_P == 8)
#define RANDOM_INDEX(i)		HashOneWord(mask, downshift, i)
#define BITSPERWORD		64
#else 

#define START_LOGSIZE		5 /* Initial hash table size is 32. */
#define MAX_LIST_VALUES		20 /* Convert to hash table when node
				    * value list gets bigger than this
				    * many values. */

/*
 * The following macro takes a preliminary integer hash value and
 * produces an index into a hash tables bucket list.  The idea is
 * to make it so that preliminary values that are arbitrarily similar
 * will end up in different buckets.  The hash function was taken
 * from a random-number generator.
 */
#define RANDOM_INDEX(i) \
    (((((long) (i))*1103515245) >> downshift) & mask)
#define BITSPERWORD		32
#endif

#define DOWNSHIFT_START		(BITSPERWORD - 2) 

/*
 * Procedure prototypes for static procedures in this file:
 */


#if (SIZEOF_VOID_P == 8)
static Blt_Hash HashOneWord _ANSI_ARGS_((uint64_t mask, unsigned int downshift,
	CONST void *key));

#endif /* SIZEOF_VOID_P == 8 */

/*
 * The hash table below is used to keep track of all the Blt_TreeKeys
 * created so far.
 */
static Blt_HashTable keyTable;
static int keyTableInitialized = 0;

typedef struct {
    Blt_HashTable treeTable;	/* Table of trees. */
    unsigned int nextId;
    Tcl_Interp *interp;
} TreeInterpData;

typedef struct {
    Tcl_Interp *interp;
    ClientData clientData;
    Blt_TreeKey key;
    unsigned int mask;
    Blt_TreeNotifyEventProc *proc;
    Blt_TreeNotifyEvent event;
    int notifyPending;
} EventHandler;

typedef struct {
    ClientData clientData;
    char *keyPattern;
    char *withTag;
    Node *nodePtr;
    unsigned int mask;
    Blt_TreeTraceProc *proc;
    TreeClient *clientPtr;
    Blt_ChainLink *linkPtr;
} TraceHandler;

/*
 * --------------------------------------------------------------
 *
 * GetTreeInterpData --
 *
 *	Creates or retrieves data associated with tree data objects
 *	for a particular thread.  We're using Tcl_GetAssocData rather
 *	than the Tcl thread routines so BLT can work with pre-8.0 
 *	Tcl versions that don't have thread support.
 *
 * Results:
 *	Returns a pointer to the tree interpreter data.
 *
 * -------------------------------------------------------------- 
 */
static TreeInterpData *
GetTreeInterpData(Tcl_Interp *interp)
{
    Tcl_InterpDeleteProc *proc;
    TreeInterpData *dataPtr;

    dataPtr = (TreeInterpData *)
	Tcl_GetAssocData(interp, TREE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TreeInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TREE_THREAD_KEY, TreeInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&dataPtr->treeTable, BLT_STRING_KEYS);
    }
    return dataPtr;
}

/*
 * --------------------------------------------------------------
 *
 * NewNode --
 *
 *	Creates a new node in the tree without installing it.  The
 *	number of nodes in the tree is incremented and a unique serial
 *	number is generated for the node. 
 *
 *	Also, all nodes have a label.  If no label was provided (name
 *	is NULL) then automatically generate one in the form "nodeN"
 *	where N is the serial number of the node.
 *
 * Results:
 *	Returns a pointer to the new node.
 *
 * -------------------------------------------------------------- 
 */
static Node *
NewNode(TreeObject *treeObjPtr, CONST char *name, int inode)
{
    Node *nodePtr;

    /* Create the node structure */
    nodePtr = Blt_PoolAllocItem(treeObjPtr->nodePool, sizeof(Node));
    nodePtr->inode = inode;
    nodePtr->treeObject = treeObjPtr;
    nodePtr->parent = NULL;
    nodePtr->depth = 0;
    nodePtr->flags = 0;
    nodePtr->next = nodePtr->prev = NULL;
    nodePtr->first = nodePtr->last = NULL;
    nodePtr->nChildren = 0;
    nodePtr->values = NULL;     
    nodePtr->logSize = 0;
    nodePtr->nValues = 0;
    nodePtr->label = NULL;
    if (name != NULL) {
	nodePtr->label = Blt_TreeGetKey(name);
    }
    treeObjPtr->nNodes++;
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ReleaseTagTable --
 *
 *---------------------------------------------------------------------- 
 */
static void
ReleaseTagTable(Blt_TreeTagTable *tablePtr)
{
    tablePtr->refCount--;
    if (tablePtr->refCount <= 0) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;

	for(hPtr = Blt_FirstHashEntry(&tablePtr->tagTable, &cursor); 
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Blt_TreeTagEntry *tPtr;

	    tPtr = Blt_GetHashValue(hPtr);
	    Blt_DeleteHashTable(&tPtr->nodeTable);
	    Blt_Free(tPtr);
	}
	Blt_DeleteHashTable(&tablePtr->tagTable);
	Blt_Free(tablePtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ResetDepths --
 *
 *	Called after moving a node, resets the depths of each node
 *	for the entire branch (node and it's decendants).  
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
ResetDepths(
    Node *nodePtr,		/* Root node. */
    int depth)			/* Depth of the node. */
{
    nodePtr->depth = depth;
    /* Also reset the depth for each descendant node. */
    for (nodePtr = nodePtr->first; nodePtr != NULL; nodePtr = nodePtr->next) {
	ResetDepths(nodePtr, depth + 1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LinkBefore --
 *
 *	Inserts a link preceding a given link.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
LinkBefore(
    Node *parentPtr,	/* Parent to hold the new entry. */
    Node *nodePtr,	/* New node to be inserted. */
    Node *beforePtr)	/* Node to link before. */
{
    if (parentPtr->first == NULL) {
	parentPtr->last = parentPtr->first = nodePtr;
    } else if (beforePtr == NULL) { /* Append onto the end of the chain */
	nodePtr->next = NULL;
	nodePtr->prev = parentPtr->last;
	parentPtr->last->next = nodePtr;
	parentPtr->last = nodePtr;
    } else {
	nodePtr->prev = beforePtr->prev;
	nodePtr->next = beforePtr;
	if (beforePtr == parentPtr->first) {
	    parentPtr->first = nodePtr;
	} else {
	    beforePtr->prev->next = nodePtr;
	}
	beforePtr->prev = nodePtr;
    }
    parentPtr->nChildren++;
    nodePtr->parent = parentPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * UnlinkNode --
 *
 *	Unlinks a link from the chain. The link is not deallocated, 
 *	but only removed from the chain.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
UnlinkNode(Node *nodePtr)
{
    Node *parentPtr;
    int unlinked;		/* Indicates if the link is actually
				 * removed from the chain. */
    parentPtr = nodePtr->parent;
    unlinked = FALSE;
    if (parentPtr->first == nodePtr) {
	parentPtr->first = nodePtr->next;
	unlinked = TRUE;
    }
    if (parentPtr->last == nodePtr) {
	parentPtr->last = nodePtr->prev;
	unlinked = TRUE;
    }
    if (nodePtr->next != NULL) {
	nodePtr->next->prev = nodePtr->prev;
	unlinked = TRUE;
    }
    if (nodePtr->prev != NULL) {
	nodePtr->prev->next = nodePtr->next;
	unlinked = TRUE;
    }
    if (unlinked) {
	parentPtr->nChildren--;
    }
    nodePtr->prev = nodePtr->next = NULL;
}

/*
 * --------------------------------------------------------------
 *
 * FreeNode --
 *
 *	Unlinks a given node from the tree, removes its data, and
 *	frees memory allocated to the node.
 *
 * Results:
 *	None.
 *
 * -------------------------------------------------------------- 
 */
static void
FreeNode(TreeObject *treeObjPtr, Node *nodePtr)
{
    Blt_HashEntry *hPtr;

    /*
     * Destroy any data fields associated with this node.
     */
    TreeDestroyValues(nodePtr);
    UnlinkNode(nodePtr);
    treeObjPtr->nNodes--;
    hPtr = Blt_FindHashEntry(&treeObjPtr->nodeTable, (char *)nodePtr->inode);
    assert(hPtr);
    Blt_DeleteHashEntry(&treeObjPtr->nodeTable, hPtr);
    Blt_PoolFreeItem(treeObjPtr->nodePool, (char *)nodePtr);
}

/*
 * --------------------------------------------------------------
 *
 * NewTreeObject --
 *
 *	Creates and initializes a new tree object. Trees always
 *	contain a root node, so one is allocated here.
 *
 * Results:
 *	Returns a pointer to the new tree object is successful, NULL
 *	otherwise.  If a tree can't be generated, interp->result will
 *	contain an error message.
 *
 * -------------------------------------------------------------- */
static TreeObject *
NewTreeObject(TreeInterpData *dataPtr, Tcl_Interp *interp, CONST char *treeName)
{
    TreeObject *treeObjPtr;
    int isNew;
    Blt_HashEntry *hPtr;

    treeObjPtr = Blt_Calloc(1, sizeof(TreeObject));
    if (treeObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate tree", (char *)NULL);
	return NULL;
    }
    treeObjPtr->name = Blt_Strdup(treeName);
    treeObjPtr->interp = interp;
    treeObjPtr->valuePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    treeObjPtr->nodePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    treeObjPtr->clients = Blt_ChainCreate();
    treeObjPtr->depth = 1;
    treeObjPtr->notifyFlags = 0;
    Blt_InitHashTableWithPool(&treeObjPtr->nodeTable, BLT_ONE_WORD_KEYS);

    hPtr = Blt_CreateHashEntry(&treeObjPtr->nodeTable, (char *)0, &isNew);
    treeObjPtr->root = NewNode(treeObjPtr, treeName, 0);
    Blt_SetHashValue(hPtr, treeObjPtr->root);

    treeObjPtr->tablePtr = &dataPtr->treeTable;
    treeObjPtr->hashPtr = Blt_CreateHashEntry(treeObjPtr->tablePtr, treeName, 
	&isNew);
    Blt_SetHashValue(treeObjPtr->hashPtr, treeObjPtr);

    return treeObjPtr;
}

static TreeObject *
FindTreeInNamespace(
    TreeInterpData *dataPtr,	/* Interpreter-specific data. */
    Tcl_Namespace *nsPtr,
    CONST char *treeName)
{
    Tcl_DString dString;
    char *name;
    Blt_HashEntry *hPtr;

    name = Blt_GetQualifiedName(nsPtr, treeName, &dString);
    hPtr = Blt_FindHashEntry(&dataPtr->treeTable, name);
    Tcl_DStringFree(&dString);
    if (hPtr != NULL) {
	return Blt_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetTreeObject --
 *
 *	Searches for the tree object associated by the name given.
 *
 * Results:
 *	Returns a pointer to the tree if found, otherwise NULL.
 *
 * ----------------------------------------------------------------------
 */
static TreeObject *
GetTreeObject(Tcl_Interp *interp, CONST char *name, int flags)
{
    CONST char *treeName;
    Tcl_Namespace *nsPtr;	/* Namespace associated with the tree object.
				 * If NULL, indicates to look in first the
				 * current namespace and then the global
				 * for the tree. */
    TreeInterpData *dataPtr;	/* Interpreter-specific data. */
    TreeObject *treeObjPtr;

    treeObjPtr = NULL;
    if (Blt_ParseQualifiedName(interp, name, &nsPtr, &treeName) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", name, "\"", 
		(char *)NULL);
	return NULL;
    }
    dataPtr = GetTreeInterpData(interp);
    if (nsPtr != NULL) { 
	treeObjPtr = FindTreeInNamespace(dataPtr, nsPtr, treeName);
    } else { 
	if (flags & NS_SEARCH_CURRENT) {
	    /* Look first in the current namespace. */
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	    treeObjPtr = FindTreeInNamespace(dataPtr, nsPtr, treeName);
	}
	if ((treeObjPtr == NULL) && (flags & NS_SEARCH_GLOBAL)) {
	    nsPtr = Tcl_GetGlobalNamespace(interp);
	    treeObjPtr = FindTreeInNamespace(dataPtr, nsPtr, treeName);
	}
    }
    return treeObjPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * TeardownTree --
 *
 *	Destroys an entire branch.  This is a special purpose routine
 *	used to speed up the final clean up of the tree.
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
TeardownTree(TreeObject *treeObjPtr, Node *nodePtr)
{
    if (nodePtr->first != NULL) {
	Node *childPtr, *nextPtr;
	
	for (childPtr = nodePtr->first; childPtr != NULL; childPtr = nextPtr) {
	    nextPtr = childPtr->next;
	    TeardownTree(treeObjPtr, childPtr);
	}
    }
    if (nodePtr->values != NULL) {
	TreeDestroyValues(nodePtr);
    }
    Blt_PoolFreeItem(treeObjPtr->nodePool, (char *)nodePtr);
}

static void
DestroyTreeObject(TreeObject *treeObjPtr)
{
    Blt_ChainLink *linkPtr;
    TreeClient *clientPtr;

    treeObjPtr->flags |= TREE_DESTROYED;
    treeObjPtr->nNodes = 0;

    /* Remove the remaining clients. */
    for (linkPtr = Blt_ChainFirstLink(treeObjPtr->clients); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	Blt_ChainDestroy(clientPtr->events);
	Blt_ChainDestroy(clientPtr->traces);
	Blt_Free(clientPtr);
    }
    Blt_ChainDestroy(treeObjPtr->clients);

    TeardownTree(treeObjPtr, treeObjPtr->root);
    Blt_PoolDestroy(treeObjPtr->nodePool);
    Blt_PoolDestroy(treeObjPtr->valuePool);
    Blt_DeleteHashTable(&treeObjPtr->nodeTable);

    if (treeObjPtr->hashPtr != NULL) {
	/* Remove the entry from the global tree table. */
	Blt_DeleteHashEntry(treeObjPtr->tablePtr, treeObjPtr->hashPtr); 
	if ((treeObjPtr->tablePtr->numEntries == 0) && (keyTableInitialized)) {
	    keyTableInitialized = FALSE;
	    Blt_DeleteHashTable(&keyTable);
	}
    }
    if (treeObjPtr->name != NULL) {
	Blt_Free(treeObjPtr->name);
    }
    Blt_Free(treeObjPtr);
}

/*
 * -----------------------------------------------------------------------
 *
 * TreeInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the tree object
 *	is deleted from the interpreter.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys all remaining trees and removes the hash table
 *	used to register tree names.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TreeInterpDeleteProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp)
{
    TreeInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    TreeObject *treeObjPtr;
    
    for (hPtr = Blt_FirstHashEntry(&dataPtr->treeTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	treeObjPtr = (TreeObject *)Blt_GetHashValue(hPtr);
	treeObjPtr->hashPtr = NULL;
	DestroyTreeObject(treeObjPtr);
    }
    if (keyTableInitialized) {
	keyTableInitialized = FALSE;
	Blt_DeleteHashTable(&keyTable);
    }
    Blt_DeleteHashTable(&dataPtr->treeTable);
    Tcl_DeleteAssocData(interp, TREE_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyIdleProc --
 *
 *	Used to invoke event handler routines at some idle point.
 *	This routine is called from the Tcl event loop.  Errors
 *	generated by the event handler routines are backgrounded.
 *	
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */
static void
NotifyIdleProc(ClientData clientData)
{
    EventHandler *notifyPtr = clientData;
    int result;

    notifyPtr->notifyPending = FALSE;
    notifyPtr->mask |= TREE_NOTIFY_ACTIVE;
    result = (*notifyPtr->proc)(notifyPtr->clientData, &notifyPtr->event);
    notifyPtr->mask &= ~TREE_NOTIFY_ACTIVE;
    if (result != TCL_OK) {
	Tcl_BackgroundError(notifyPtr->interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CheckEventHandlers --
 *
 *	Traverses the list of client event callbacks and checks
 *	if one matches the given event.  A client may trigger an
 *	action that causes the tree to notify it.  The can be
 *	prevented by setting the TREE_NOTIFY_FOREIGN_ONLY bit in
 *	the event handler.
 *
 *	If a matching handler is found, a callback may be called either
 *	immediately or at the next idle time depending upon the
 *	TREE_NOTIFY_WHENIDLE bit.  
 *
 *	Since a handler routine may trigger yet another call to
 *	itself, callbacks are ignored while the event handler is
 *	executing.
 *	
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */
static void
CheckEventHandlers(
    TreeClient *clientPtr,
    int isSource,		/* Indicates if the client is the source
				 * of the event. */
    Blt_TreeNotifyEvent *eventPtr)
{
    Blt_ChainLink *linkPtr, *nextPtr;
    EventHandler *notifyPtr;

    eventPtr->tree = clientPtr;
    for (linkPtr = Blt_ChainFirstLink(clientPtr->events); 
	linkPtr != NULL; linkPtr = nextPtr) {
	nextPtr = Blt_ChainNextLink(linkPtr);
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if ((notifyPtr->mask & TREE_NOTIFY_ACTIVE) ||
	    (notifyPtr->mask & eventPtr->type) == 0) {
	    continue;		/* Ignore callbacks that are generated
				 * inside of a notify handler routine. */
	}
	if ((isSource) && (notifyPtr->mask & TREE_NOTIFY_FOREIGN_ONLY)) {
	    continue;		/* Don't notify yourself. */
	}
	if (notifyPtr->mask & TREE_NOTIFY_WHENIDLE) {
	    if (!notifyPtr->notifyPending) {
		notifyPtr->notifyPending = TRUE;
		notifyPtr->event = *eventPtr;
		Tcl_DoWhenIdle(NotifyIdleProc, notifyPtr);
	    }
	} else {
	    int result;

	    notifyPtr->mask |= TREE_NOTIFY_ACTIVE;
	    result = (*notifyPtr->proc) (notifyPtr->clientData, eventPtr);
	    notifyPtr->mask &= ~TREE_NOTIFY_ACTIVE;
	    if (result != TCL_OK) {
		Tcl_BackgroundError(notifyPtr->interp);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyClients --
 *
 *	Traverses the list of clients for a particular tree and
 *	notifies each client that an event occurred.  Clients 
 *	indicate interest in a particular event through a bit
 *	flag.  
 *
 *---------------------------------------------------------------------- 
 */
static void
NotifyClients(
    TreeClient *sourcePtr,
    TreeObject *treeObjPtr,
    Node *nodePtr,
    int eventFlag)
{
    Blt_ChainLink *linkPtr;
    Blt_TreeNotifyEvent event;
    TreeClient *clientPtr;
    int isSource;

    event.type = eventFlag;
    event.inode = nodePtr->inode;

    /* 
     * Issue callbacks to each client indicating that a new node has
     * been created.
     */
    for (linkPtr = Blt_ChainFirstLink(treeObjPtr->clients);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	isSource = (clientPtr == sourcePtr);
	CheckEventHandlers(clientPtr, isSource, &event);
    }
}

static void
FreeValue(Node *nodePtr, Value *valuePtr)
{
    if (valuePtr->objPtr != NULL) {
	Tcl_DecrRefCount(valuePtr->objPtr);
    }
    Blt_PoolFreeItem(nodePtr->treeObject->valuePool, valuePtr);
}


/* Public Routines */
/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeGetKey --
 *
 *	Given a string, returns a unique identifier for the string.
 *
 *----------------------------------------------------------------------
 */
Blt_TreeKey
Blt_TreeGetKey(string)
    CONST char *string;		/* String to convert. */
{
    Blt_HashEntry *hPtr;
    int isNew;

    if (!keyTableInitialized) {
	Blt_InitHashTable(&keyTable, BLT_STRING_KEYS);
	keyTableInitialized = 1;
    }
    hPtr = Blt_CreateHashEntry(&keyTable, string, &isNew);
    return (Blt_TreeKey)Blt_GetHashKey(&keyTable, hPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeCreateNode --
 *
 *	Creates a new node in the given parent node.  The name and
 *	position in the parent are also provided.
 *
 *----------------------------------------------------------------------
 */
Blt_TreeNode
Blt_TreeCreateNode(
    TreeClient *clientPtr,	/* The tree client that is creating
				 * this node.  If NULL, indicates to
				 * trigger notify events on behalf of
				 * the initiating client also. */
    Node *parentPtr,		/* Parent node where the new node will
				 * be inserted. */
    CONST char *name,		/* Name of node. */
    int pos)			/* Position in the parent's list of children
				 * where to insert the new node. */
{
    Blt_HashEntry *hPtr;
    Node *beforePtr;
    Node *nodePtr;	/* Node to be inserted. */
    TreeObject *treeObjPtr;
    int inode;
    int isNew;

    treeObjPtr = parentPtr->treeObject;

    /* Generate an unique serial number for this node.  */
    do {
	inode = treeObjPtr->nextInode++;
	hPtr = Blt_CreateHashEntry(&treeObjPtr->nodeTable,(char *)inode, 
		   &isNew);
    } while (!isNew);
    nodePtr = NewNode(treeObjPtr, name, inode);
    Blt_SetHashValue(hPtr, nodePtr);

    if ((pos == -1) || (pos >= (int)parentPtr->nChildren)) {
	beforePtr = NULL;
    } else {
	beforePtr = parentPtr->first;
	while ((pos > 0) && (beforePtr != NULL)) {
	    pos--;
	    beforePtr = beforePtr->next;
	}
    }
    LinkBefore(parentPtr, nodePtr, beforePtr);
    nodePtr->depth = parentPtr->depth + 1;
    /* 
     * Issue callbacks to each client indicating that a new node has
     * been created.
     */
    NotifyClients(clientPtr, treeObjPtr, nodePtr, TREE_NOTIFY_CREATE);
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeCreateNodeWithId --
 *
 *	Like Blt_TreeCreateNode, but provides a specific id to use
 *	for the node.  If the tree already contains a node by that
 *	id, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_TreeNode
Blt_TreeCreateNodeWithId(
    TreeClient *clientPtr,
    Node *parentPtr,		/* Parent node where the new node will
				 * be inserted. */
    CONST char *name,		/* Name of node. */
    int inode,			/* Requested id of the new node. If a
				 * node by this id already exists in the
				 * tree, no node is created. */
    int position)		/* Position in the parent's list of children
				 * where to insert the new node. */
{
    Blt_HashEntry *hPtr;
    Node *beforePtr;
    Node *nodePtr;	/* Node to be inserted. */
    TreeObject *treeObjPtr;
    int isNew;

    treeObjPtr = parentPtr->treeObject;
    hPtr = Blt_CreateHashEntry(&treeObjPtr->nodeTable,(char *)inode, &isNew);
    if (!isNew) {
	return NULL;
    }
    nodePtr = NewNode(treeObjPtr, name, inode);
    Blt_SetHashValue(hPtr, nodePtr);

    if ((position == -1) || (position >= (int)parentPtr->nChildren)) {
	beforePtr = NULL;
    } else {
	beforePtr = parentPtr->first;
	while ((position > 0) && (beforePtr != NULL)) {
	    position--;
	    beforePtr = beforePtr->next;
	}
    }
    LinkBefore(parentPtr, nodePtr, beforePtr);
    nodePtr->depth = parentPtr->depth + 1;
    /* 
     * Issue callbacks to each client indicating that a new node has
     * been created.
     */
    NotifyClients(clientPtr, treeObjPtr, nodePtr, TREE_NOTIFY_CREATE);
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeMoveNode --
 *
 *	Move an entry into a new location in the hierarchy.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_TreeMoveNode(
    TreeClient *clientPtr,
    Node *nodePtr, 
    Node *parentPtr, 
    Node *beforePtr)
{
    TreeObject *treeObjPtr = nodePtr->treeObject;
    int newDepth;

    if (nodePtr == beforePtr) {
	return TCL_ERROR;
    }
    if ((beforePtr != NULL) && (beforePtr->parent != parentPtr)) {
	return TCL_ERROR;
    }
    if (nodePtr->parent == NULL) {
	return TCL_ERROR;	/* Can't move root. */
    }
    /* Verify that the node isn't an ancestor of the new parent. */
    if (Blt_TreeIsAncestor(nodePtr, parentPtr)) {
	return TCL_ERROR;
    }
    UnlinkNode(nodePtr);
    /* 
     * Relink the node as a child of the new parent.
     */
    LinkBefore(parentPtr, nodePtr, beforePtr);
    newDepth = parentPtr->depth + 1;
    if (nodePtr->depth != newDepth) { 
	/* Reset the depths of all descendant nodes. */
	ResetDepths(nodePtr, newDepth);
    }

    /* 
     * Issue callbacks to each client indicating that a node has
     * been moved.
     */
    NotifyClients(clientPtr, treeObjPtr, nodePtr, TREE_NOTIFY_MOVE);
    return TCL_OK;
}

int
Blt_TreeDeleteNode(TreeClient *clientPtr, Node *nodePtr)
{
    TreeObject *treeObjPtr = nodePtr->treeObject;
    Node *childPtr, *nextPtr;

    /* In depth-first order, delete each descendant node. */
    for (childPtr = nodePtr->first; childPtr != NULL; childPtr = nextPtr) {
	nextPtr = childPtr->next;
	Blt_TreeDeleteNode(clientPtr, childPtr);
    }
    /* 
     * Issue callbacks to each client indicating that the node can
     * no longer be used.  
     */
    NotifyClients(clientPtr, treeObjPtr, nodePtr, TREE_NOTIFY_DELETE);

    /* Now remove the actual node. */
    FreeNode(treeObjPtr, nodePtr);
    return TCL_OK;
}

Blt_TreeNode
Blt_TreeGetNode(TreeClient *clientPtr, unsigned int inode)
{
    TreeObject *treeObjPtr = clientPtr->treeObject;
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&treeObjPtr->nodeTable, (char *)inode);
    if (hPtr != NULL) {
	return (Blt_TreeNode)Blt_GetHashValue(hPtr);
    }
    return NULL;
}

Blt_TreeTrace
Blt_TreeCreateTrace(
    TreeClient *clientPtr,
    Node *nodePtr,
    CONST char *keyPattern,
    CONST char *tagName,
    unsigned int mask,
    Blt_TreeTraceProc *proc,
    ClientData clientData)
{
    TraceHandler *tracePtr;

    tracePtr = Blt_Calloc(1, sizeof (TraceHandler));
    assert(tracePtr);
    tracePtr->linkPtr = Blt_ChainAppend(clientPtr->traces, tracePtr);
    if (keyPattern != NULL) {
	tracePtr->keyPattern = Blt_Strdup(keyPattern);
    }
    if (tagName != NULL) {
	tracePtr->withTag = Blt_Strdup(tagName);
    }
    tracePtr->clientPtr = clientPtr;
    tracePtr->proc = proc;
    tracePtr->clientData = clientData;
    tracePtr->mask = mask;
    tracePtr->nodePtr = nodePtr;
    return (Blt_TreeTrace)tracePtr;
}

void
Blt_TreeDeleteTrace(Blt_TreeTrace trace)
{
    TraceHandler *tracePtr = (TraceHandler *)trace;

    Blt_ChainDeleteLink(tracePtr->clientPtr->traces, tracePtr->linkPtr);
    if (tracePtr->keyPattern != NULL) {
	Blt_Free(tracePtr->keyPattern);
    }
    if (tracePtr->withTag != NULL) {
	Blt_Free(tracePtr->withTag);
    }
    Blt_Free(tracePtr);
}

void
Blt_TreeRelabelNode(TreeClient *clientPtr, Node *nodePtr, CONST char *string)
{
    nodePtr->label = Blt_TreeGetKey(string);
    /* 
     * Issue callbacks to each client indicating that a new node has
     * been created.
     */
    NotifyClients(clientPtr, clientPtr->treeObject, nodePtr, 
		  TREE_NOTIFY_RELABEL);
}

void
Blt_TreeRelabelNode2(Node *nodePtr, CONST char *string)
{
    nodePtr->label = Blt_TreeGetKey(string);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeFindChild --
 *
 *	Searches for the named node in a parent's chain of siblings.  
 *
 *
 * Results:
 *	If found, the child node is returned, otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
Blt_TreeNode
Blt_TreeFindChild(Node *parentPtr, CONST char *string)
{
    Blt_TreeKey label;
    register Node *nodePtr;
    
    label = Blt_TreeGetKey(string);
    for (nodePtr = parentPtr->first; nodePtr != NULL; nodePtr = nodePtr->next) {
	if (label == nodePtr->label) {
	    return nodePtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeNodePosition --
 *
 *	Returns the position of the node in its parent's list of
 *	children.  The root's position is 0.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TreeNodePosition(Node *nodePtr)
{
    Node *parentPtr;
    int count;

    count = 0;
    parentPtr = nodePtr->parent;
    if (parentPtr != NULL) {
	Node *childPtr;

	for (childPtr = parentPtr->first; childPtr != NULL; 
	     childPtr = childPtr->next) {
	    if (nodePtr == childPtr) {
		break;
	    }
	    count++;
	}
    }
    return count;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TreePrevNode --
 *
 *	Returns the "previous" node in the tree.  This node (in 
 *	depth-first order) is its parent, if the node has no siblings
 *	that are previous to it.  Otherwise it is the last descendant 
 *	of the last sibling.  In this case, descend the sibling's
 *	hierarchy, using the last child at any ancestor, with we
 *	we find a leaf.
 *
 *----------------------------------------------------------------------
 */
Blt_TreeNode
Blt_TreePrevNode(Node *rootPtr, Node *nodePtr)
{
    Node *prevPtr;

    if (nodePtr == rootPtr) {
	return NULL;		/* The root is the first node. */
    }
    prevPtr = nodePtr->prev;
    if (prevPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	return nodePtr->parent;
    }
    /*
     * Traverse down the right-most thread, in order to select the
     * next entry.  Stop when we reach a leaf.
     */
    nodePtr = prevPtr;
    while ((prevPtr = nodePtr->last) != NULL) {
	nodePtr = prevPtr;
    }
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeNextNode --
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
Blt_TreeNode
Blt_TreeNextNode(Node *rootPtr, Node *nodePtr)
{
    Node *nextPtr;

    /* Pick the first sub-node. */
    nextPtr = nodePtr->first;
    if (nextPtr != NULL) {
	return nextPtr;
    }
    /* 
     * Back up until we can find a level where we can pick a 
     * "next sibling".  For the last entry we'll thread our 
     * way back to the root.  
     */
    while (nodePtr != rootPtr) {
	nextPtr = nodePtr->next;
	if (nextPtr != NULL) {
	    return nextPtr;
	}
	nodePtr = nodePtr->parent;
    }
    return NULL;		/* At root, no next node. */
}


int
Blt_TreeIsBefore(Node *n1Ptr, Node *n2Ptr)
{
    int depth;
    register int i;
    Node *nodePtr;

    if (n1Ptr == n2Ptr) {
	return FALSE;
    }
    depth = MIN(n1Ptr->depth, n2Ptr->depth);
    if (depth == 0) {		/* One of the nodes is root. */
	return (n1Ptr->parent == NULL);
    }
    /* 
     * Traverse back from the deepest node, until both nodes are at
     * the same depth.  Check if this ancestor node is the same for
     * both nodes.
     */
    for (i = n1Ptr->depth; i > depth; i--) {
	n1Ptr = n1Ptr->parent;
    }
    if (n1Ptr == n2Ptr) {
	return FALSE;
    }
    for (i = n2Ptr->depth; i > depth; i--) {
	n2Ptr = n2Ptr->parent;
    }
    if (n2Ptr == n1Ptr) {
	return TRUE;
    }

    /* 
     * First find the mutual ancestor of both nodes.  Look at each
     * preceding ancestor level-by-level for both nodes.  Eventually
     * we'll find a node that's the parent of both ancestors.  Then
     * find the first ancestor in the parent's list of subnodes.  
     */
    for (i = depth; i > 0; i--) {
	if (n1Ptr->parent == n2Ptr->parent) {
	    break;
	}
	n1Ptr = n1Ptr->parent;
	n2Ptr = n2Ptr->parent;
    }
    for (nodePtr = n1Ptr->parent->first; nodePtr != NULL; 
	 nodePtr = nodePtr->next) {
	if (nodePtr == n1Ptr) {
	    return TRUE;
	} else if (nodePtr == n2Ptr) {
	    return FALSE;
	}
    }
    return FALSE;
}

static void
CallTraces(
    Tcl_Interp *interp,
    TreeClient *sourcePtr,	/* Client holding a reference to the
				 * tree.  If NULL, indicates to
				 * execute all handlers, including
				 * those of the caller. */
    TreeObject *treeObjPtr,	/* Tree that was changed. */
    Node *nodePtr,		/* Node that received the event. */
    Blt_TreeKey key,
    unsigned int flags)
{
    Blt_ChainLink *l1Ptr, *l2Ptr;
    TreeClient *clientPtr;
    TraceHandler *tracePtr;	

    for(l1Ptr = Blt_ChainFirstLink(treeObjPtr->clients); 
	l1Ptr != NULL; l1Ptr = Blt_ChainNextLink(l1Ptr)) {
	clientPtr = Blt_ChainGetValue(l1Ptr);
	for(l2Ptr = Blt_ChainFirstLink(clientPtr->traces); 
	    l2Ptr != NULL; l2Ptr = Blt_ChainNextLink(l2Ptr)) {
	    tracePtr = Blt_ChainGetValue(l2Ptr);
	    if ((tracePtr->keyPattern != NULL) && 
		(!Tcl_StringMatch(key, tracePtr->keyPattern))) {
		continue;	/* Key pattern doesn't match. */
	    }
	    if ((tracePtr->withTag != NULL) && 
		(!Blt_TreeHasTag(clientPtr, nodePtr, tracePtr->withTag))) {
		continue;	/* Doesn't have the tag. */
	    }
	    if ((tracePtr->mask & flags) == 0) {
		continue;	/* Flags don't match. */
	    }
	    if ((clientPtr == sourcePtr) && 
		(tracePtr->mask & TREE_TRACE_FOREIGN_ONLY)) {
		continue;	/* This client initiated the trace. */
	    }
	    if ((tracePtr->nodePtr != NULL) && (tracePtr->nodePtr != nodePtr)) {
		continue;	/* Nodes don't match. */
	    }
	    nodePtr->flags |= TREE_TRACE_ACTIVE;
	    if ((*tracePtr->proc) (tracePtr->clientData, treeObjPtr->interp, 
		nodePtr, key, flags) != TCL_OK) {
		if (interp != NULL) {
		    Tcl_BackgroundError(interp);
		}
	    }
	    nodePtr->flags &= ~TREE_TRACE_ACTIVE;
	}
    }
}

static Value *
GetTreeValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    Blt_TreeKey key)
{
    register Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\"", 
			     (char *)NULL);
	}
	return NULL;
    }	
    if ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't access private field \"", 
			     key, "\"", (char *)NULL);
	}
	return NULL;
    }
    return valuePtr;
}

int
Blt_TreePrivateValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\"", 
			     (char *)NULL);
	}
	return TCL_ERROR;
    }
    valuePtr->owner = clientPtr;
    return TCL_OK;
}

int
Blt_TreePublicValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\"", 
			     (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (valuePtr->owner != clientPtr) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "not the owner of \"", key, "\"", 
		     (char *)NULL);
	}
	return TCL_ERROR;
    }
    valuePtr->owner = NULL;
    return TCL_OK;
}

int
Blt_TreeValueExistsByKey(clientPtr, nodePtr, key)
    TreeClient *clientPtr;
    Node *nodePtr;
    Blt_TreeKey key;
{
    register Value *valuePtr;

    valuePtr = GetTreeValue((Tcl_Interp *)NULL, clientPtr, nodePtr, key);
    if (valuePtr == NULL) {
	return FALSE;
    }
    return TRUE;
}

int
Blt_TreeGetValueByKey(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    Blt_TreeKey key,
    Tcl_Obj **objPtrPtr)
{
    register Value *valuePtr;
    TreeObject *treeObjPtr = nodePtr->treeObject;

    valuePtr = GetTreeValue(interp, clientPtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    *objPtrPtr = valuePtr->objPtr;
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, clientPtr, treeObjPtr, nodePtr, key, 
		   TREE_TRACE_READ);
    }
    return TCL_OK;
}

int
Blt_TreeSetValueByKey(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    Blt_TreeKey key,		/* Identifies the field key. */
    Tcl_Obj *objPtr)		/* New value of field. */
{
    TreeObject *treeObjPtr = nodePtr->treeObject;
    Value *valuePtr;
    unsigned int flags;
    int isNew;

    assert(objPtr != NULL);
    valuePtr = TreeCreateValue(nodePtr, key, &isNew);
    if ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't set private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (objPtr != valuePtr->objPtr) {
	Tcl_IncrRefCount(objPtr);
	if (valuePtr->objPtr != NULL) {
	    Tcl_DecrRefCount(valuePtr->objPtr);
	}
	valuePtr->objPtr = objPtr;
    }
    flags = TREE_TRACE_WRITE;
    if (isNew) {
	flags |= TREE_TRACE_CREATE;
    }
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, clientPtr, treeObjPtr, nodePtr, valuePtr->key, 
		flags);
    }
    return TCL_OK;
}

int
Blt_TreeUnsetValueByKey(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    Blt_TreeKey key)		/* Name of field in node. */
{
    TreeObject *treeObjPtr = nodePtr->treeObject;
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_OK;		/* It's okay to unset values that don't
				 * exist in the node. */
    }
    if ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't unset private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    TreeDeleteValue(nodePtr, valuePtr);
    CallTraces(interp, clientPtr, treeObjPtr, nodePtr, key, TREE_TRACE_UNSET);
    return TCL_OK;
}

static int
ParseParentheses(
    Tcl_Interp *interp,
    CONST char *string,
    char **leftPtr, 
    char **rightPtr)
{
    register char *p;
    char *left, *right;

    left = right = NULL;
    for (p = (char *)string; *p != '\0'; p++) {
	if (*p == '(') {
	    left = p;
	} else if (*p == ')') {
	    right = p;
	}
    }
    if (left != right) {
	if (((left != NULL) && (right == NULL)) ||
	    ((left == NULL) && (right != NULL)) ||
	    (left > right) || (right != (p - 1))) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "bad array specification \"", string,
			     "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    *leftPtr = left;
    *rightPtr = right;
    return TCL_OK;
}

int
Blt_TreeGetValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    CONST char *string,		/* String identifying the field in node. */
    Tcl_Obj **objPtrPtr)
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_TreeGetArrayValue(interp, clientPtr, nodePtr, string, 
		left + 1, objPtrPtr);
	*left = '(', *right = ')';
    } else {
	result = Blt_TreeGetValueByKey(interp, clientPtr, nodePtr, 
		Blt_TreeGetKey(string), objPtrPtr);
    }
    return result;
}

int
Blt_TreeSetValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    CONST char *string,		/* String identifying the field in node. */
    Tcl_Obj *valueObjPtr)	/* New value of field. If NULL, field
				 * is deleted. */
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_TreeSetArrayValue(interp, clientPtr, nodePtr, string, 
		left + 1, valueObjPtr);
	*left = '(', *right = ')';
    } else {
	result = Blt_TreeSetValueByKey(interp, clientPtr, nodePtr, 
			       Blt_TreeGetKey(string), valueObjPtr);
    }
    return result;
}

int
Blt_TreeUnsetValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    CONST char *string)		/* String identifying the field in node. */
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_TreeUnsetArrayValue(interp, clientPtr, nodePtr, string, 
		left + 1);
	*left = '(', *right = ')';
    } else {
	result = Blt_TreeUnsetValueByKey(interp, clientPtr, nodePtr, 
		Blt_TreeGetKey(string));
    }
    return result;
}

int
Blt_TreeValueExists(TreeClient *clientPtr, Node *nodePtr, CONST char *string)
{
    char *left, *right;
    int result;

    if (ParseParentheses((Tcl_Interp *)NULL, string, &left, &right) != TCL_OK) {
	return FALSE;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_TreeArrayValueExists(clientPtr, nodePtr, string, left + 1);
	*left = '(', *right = ')';
    } else {
	result = Blt_TreeValueExistsByKey(clientPtr, nodePtr, 
		Blt_TreeGetKey(string));
    }
    return result;
}

Blt_TreeKey
Blt_TreeFirstKey(
    TreeClient *clientPtr, 
    Node *nodePtr, 
    Blt_TreeKeySearch *iterPtr)
{
    Value *valuePtr;
    
    valuePtr = TreeFirstValue(nodePtr, iterPtr);
    if (valuePtr == NULL) {
	return NULL;
    }
    while ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	valuePtr = TreeNextValue(iterPtr);
	if (valuePtr == NULL) {
	    return NULL;
	}
    }
    return valuePtr->key;
}

Blt_TreeKey
Blt_TreeNextKey(TreeClient *clientPtr, Blt_TreeKeySearch *iterPtr)
{
    Value *valuePtr;

    valuePtr = TreeNextValue(iterPtr);
    if (valuePtr == NULL) {
	return NULL;
    }
    while ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	valuePtr = TreeNextValue(iterPtr);
	if (valuePtr == NULL) {
	    return NULL;
	}
    }
    return valuePtr->key;
}

int
Blt_TreeIsAncestor(Node *n1Ptr, Node *n2Ptr)
{
    if (n2Ptr != NULL) {
	n2Ptr = n2Ptr->parent;
	while (n2Ptr != NULL) {
	    if (n2Ptr == n1Ptr) {
		return TRUE;
	    }
	    n2Ptr = n2Ptr->parent;
	}
    }
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeSortNode --
 *
 *	Sorts the subnodes at a given node.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TreeSortNode(
    TreeClient *clientPtr,
    Node *nodePtr,
    Blt_TreeCompareNodesProc *proc)
{
    Node **nodeArr;
    Node *childPtr;
    int nNodes;
    register Node **p;

    nNodes = nodePtr->nChildren;
    if (nNodes < 2) {
	return TCL_OK;
    }
    nodeArr = Blt_Malloc((nNodes + 1) * sizeof(Node *));
    if (nodeArr == NULL) {
	return TCL_ERROR;	/* Out of memory. */
    }
    for (p = nodeArr, childPtr = nodePtr->first; childPtr != NULL; 
	 childPtr = childPtr->next, p++) {
	*p = childPtr;
    }
    *p = NULL;

    qsort((char *)nodeArr, nNodes, sizeof(Node *), (QSortCompareProc *)proc);
    for (p = nodeArr; *p != NULL; p++) {
	UnlinkNode(*p);
	LinkBefore(nodePtr, *p, (Blt_TreeNode)NULL);
    }
    Blt_Free(nodeArr);
    NotifyClients(clientPtr, nodePtr->treeObject, nodePtr, TREE_NOTIFY_SORT);
    return TCL_OK;
}

#define TEST_RESULT(result) \
	switch (result) { \
	case TCL_CONTINUE: \
	    return TCL_OK; \
	case TCL_OK: \
	    break; \
	default: \
	    return (result); \
	}

int
Blt_TreeApply(
    Node *nodePtr,		/* Root node of subtree. */
    Blt_TreeApplyProc *proc,	/* Procedure to call for each node. */
    ClientData clientData)	/* One-word of data passed when calling
				 * proc. */
{
    Node *childPtr, *nextPtr;
    int result;

    for (childPtr = nodePtr->first; childPtr != NULL; childPtr = nextPtr) {

	/* 
	 * Get the next link in the chain before calling Blt_TreeApply
	 * recursively.  This is because the apply callback may delete
	 * the node and its link.  
	 */

	nextPtr = childPtr->next;
	result = Blt_TreeApply(childPtr, proc, clientData);
	TEST_RESULT(result);
    }
    return (*proc) (nodePtr, clientData, TREE_POSTORDER);
}

int
Blt_TreeApplyDFS(
    Node *nodePtr,		/* Root node of subtree. */
    Blt_TreeApplyProc *proc,	/* Procedure to call for each node. */
    ClientData clientData,	/* One-word of data passed when calling
				 * proc. */
    int order)			/* Order of traversal. */
{
    Node *childPtr, *nextPtr;
    int result;

    if (order & TREE_PREORDER) {
	result = (*proc) (nodePtr, clientData, TREE_PREORDER);
	TEST_RESULT(result);
    }
    childPtr = nodePtr->first;
    if (order & TREE_INORDER) {
	if (childPtr != NULL) {
	    result = Blt_TreeApplyDFS(childPtr, proc, clientData, order);
	    TEST_RESULT(result);
	    childPtr = childPtr->next;
	}
	result = (*proc) (nodePtr, clientData, TREE_INORDER);
	TEST_RESULT(result);
    }
    for (/* empty */; childPtr != NULL; childPtr = nextPtr) {
	/* 
	 * Get the next link in the chain before calling
	 * Blt_TreeApply recursively.  This is because the 
	 * apply callback may delete the node and its link.  
	 */
	nextPtr = childPtr->next;
	result = Blt_TreeApplyDFS(childPtr, proc, clientData, order);
	TEST_RESULT(result);
    }
    if (order & TREE_POSTORDER) {
	return (*proc) (nodePtr, clientData, TREE_POSTORDER);
    }
    return TCL_OK;
}

int
Blt_TreeApplyBFS(nodePtr, proc, clientData)
    Node *nodePtr;		/* Root node of subtree. */
    Blt_TreeApplyProc *proc;	/* Procedure to call for each node. */
    ClientData clientData;	/* One-word of data passed when calling
				 * proc. */
{
    Blt_Chain *queuePtr;
    Blt_ChainLink *linkPtr, *nextPtr;
    Node *childPtr;
    int result;

    queuePtr = Blt_ChainCreate();
    linkPtr = Blt_ChainAppend(queuePtr, nodePtr);
    while (linkPtr != NULL) {
	nodePtr = Blt_ChainGetValue(linkPtr);
	/* Add the children to the queue. */
	for (childPtr = nodePtr->first; childPtr != NULL; 
	     childPtr = childPtr->next) {
	    Blt_ChainAppend(queuePtr, childPtr);
	}
	/* Process the node. */
	result = (*proc) (nodePtr, clientData, TREE_BREADTHFIRST);
	switch (result) { 
	case TCL_CONTINUE: 
	    Blt_ChainDestroy(queuePtr);
	    return TCL_OK; 
	case TCL_OK: 
	    break; 
	default: 
	    Blt_ChainDestroy(queuePtr);
	    return result; 
	}
	/* Remove the node from the queue. */
	nextPtr = Blt_ChainNextLink(linkPtr);
	Blt_ChainDeleteLink(queuePtr, linkPtr);
	linkPtr = nextPtr;
    }
    Blt_ChainDestroy(queuePtr);
    return TCL_OK;
}

static TreeClient *
NewTreeClient(TreeObject *treeObjPtr)
{
    TreeClient *clientPtr;

    clientPtr = Blt_Calloc(1, sizeof(TreeClient));
    if (clientPtr != NULL) {
	Blt_TreeTagTable *tablePtr;

	clientPtr->magic = TREE_MAGIC;
	clientPtr->linkPtr = Blt_ChainAppend(treeObjPtr->clients, clientPtr);
	clientPtr->events = Blt_ChainCreate();
	clientPtr->traces = Blt_ChainCreate();
	clientPtr->treeObject = treeObjPtr;
	clientPtr->root = treeObjPtr->root;
	tablePtr = Blt_Malloc(sizeof(Blt_TreeTagTable));
	Blt_InitHashTable(&tablePtr->tagTable, BLT_STRING_KEYS);
	tablePtr->refCount = 1;
	clientPtr->tagTablePtr = tablePtr;
    }
    return clientPtr;
}

int
Blt_TreeCreate(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    CONST char *name,		/* Name of tree in namespace.  Tree
				 * must not already exist. */
    TreeClient **clientPtrPtr)	/* (out) Client token of newly created
				 * tree.  Releasing the token will
				 * free the tree.  If NULL, no token
				 * is generated. */
{
    Tcl_DString dString;
    Tcl_Namespace *nsPtr;
    TreeInterpData *dataPtr;
    TreeObject *treeObjPtr;
    CONST char *treeName;
    char string[200];

    dataPtr = GetTreeInterpData(interp);
    if (name != NULL) {
	/* Check if this tree already exists the current namespace. */
	treeObjPtr = GetTreeObject(interp, name, NS_SEARCH_CURRENT);
	if (treeObjPtr != NULL) {
	    Tcl_AppendResult(interp, "a tree object \"", name,
			     "\" already exists", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	/* Generate a unique tree name in the current namespace. */
	do  {
	    sprintf(string, "tree%d", dataPtr->nextId++);
	} while (GetTreeObject(interp, name, NS_SEARCH_CURRENT) != NULL);
	name = string;
    } 
    /* 
     * Tear apart and put back together the namespace-qualified name 
     * of the tree. This is to ensure that naming is consistent.
     */ 
    treeName = name;
    if (Blt_ParseQualifiedName(interp, name, &nsPtr, &treeName) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", name, "\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (nsPtr == NULL) {
	/* 
	 * Note: Unlike Tcl_CreateCommand, an unqualified name 
	 * doesn't imply the global namespace, but the current one.
	 */
	nsPtr = Tcl_GetCurrentNamespace(interp);
    }
    name = Blt_GetQualifiedName(nsPtr, treeName, &dString);
    treeObjPtr = NewTreeObject(dataPtr, interp, name);
    if (treeObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate tree \"", name, "\"", 
		(char *)NULL);
	Tcl_DStringFree(&dString);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&dString);
    if (clientPtrPtr != NULL) {
	TreeClient *clientPtr;
	
	clientPtr = NewTreeClient(treeObjPtr);
	if (clientPtr == NULL) {
	    Tcl_AppendResult(interp, "can't allocate tree token",(char *)NULL);
	    return TCL_ERROR;
	}
	*clientPtrPtr = clientPtr;
    }
    return TCL_OK;
}

int
Blt_TreeGetToken(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    CONST char *name,		/* Name of tree in namespace. */
    TreeClient **clientPtrPtr)
{
    TreeClient *clientPtr;
    TreeObject *treeObjPtr;

    treeObjPtr = GetTreeObject(interp, name, NS_SEARCH_BOTH);
    if (treeObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't find a tree object \"", name, "\"", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    clientPtr = NewTreeClient(treeObjPtr);
    if (clientPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate token for tree \"", 
			 name, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *clientPtrPtr = clientPtr;
    return TCL_OK;
}

void
Blt_TreeReleaseToken(TreeClient *clientPtr)
{
    TreeObject *treeObjPtr;
    Blt_ChainLink *linkPtr;
    EventHandler *notifyPtr;
    TraceHandler *tracePtr;

    if (clientPtr->magic != TREE_MAGIC) {
	fprintf(stderr, "invalid tree object token 0x%lx\n", 
		(unsigned long)clientPtr);
	return;
    }
    /* Remove any traces that may be set. */
    for (linkPtr = Blt_ChainFirstLink(clientPtr->traces); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	tracePtr = Blt_ChainGetValue(linkPtr);
	if (tracePtr->keyPattern != NULL) {
	    Blt_Free(tracePtr->keyPattern);
	}
	Blt_Free(tracePtr);
    }
    Blt_ChainDestroy(clientPtr->traces);
    /* And any event handlers. */
    for(linkPtr = Blt_ChainFirstLink(clientPtr->events); 
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if (notifyPtr->notifyPending) {
	    Tcl_CancelIdleCall(NotifyIdleProc, notifyPtr);
	}
	Blt_Free(notifyPtr);
    }
    if (clientPtr->tagTablePtr != NULL) {
	ReleaseTagTable(clientPtr->tagTablePtr);
    }
    Blt_ChainDestroy(clientPtr->events);
    treeObjPtr = clientPtr->treeObject;
    if (treeObjPtr != NULL) {
	/* Remove the client from the server's list */
	Blt_ChainDeleteLink(treeObjPtr->clients, clientPtr->linkPtr);
	if (Blt_ChainGetLength(treeObjPtr->clients) == 0) {
	    DestroyTreeObject(treeObjPtr);
	}
    }
    clientPtr->magic = 0;
    Blt_Free(clientPtr);
}

int
Blt_TreeExists(interp, name)
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    CONST char *name;		/* Name of tree in designated namespace. */
{
    TreeObject *treeObjPtr;

    treeObjPtr = GetTreeObject(interp, name, NS_SEARCH_BOTH);
    if (treeObjPtr == NULL) {
	Tcl_ResetResult(interp);
	return 0;
    }
    return 1;
}

/*ARGSUSED*/
static int
SizeApplyProc(
    Node *nodePtr,		/* Not used. */
    ClientData clientData,
    int order)			/* Not used. */
{
    int *sumPtr = clientData;
    *sumPtr = *sumPtr + 1;
    return TCL_OK;
}

int
Blt_TreeSize(Node *nodePtr)
{
    int sum;

    sum = 0;
    Blt_TreeApply(nodePtr, SizeApplyProc, &sum);
    return sum;
}


void
Blt_TreeCreateEventHandler(
    TreeClient *clientPtr,
    unsigned int mask,
    Blt_TreeNotifyEventProc *proc,
    ClientData clientData)
{
    Blt_ChainLink *linkPtr;
    EventHandler *notifyPtr;

    notifyPtr = NULL;		/* Suppress compiler warning. */

    /* Check if the event is already handled. */
    for(linkPtr = Blt_ChainFirstLink(clientPtr->events); 
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if ((notifyPtr->proc == proc) && 
	    (notifyPtr->mask == mask) &&
	    (notifyPtr->clientData == clientData)) {
	    break;
	}
    }
    if (linkPtr == NULL) {
	notifyPtr = Blt_Malloc(sizeof (EventHandler));
	assert(notifyPtr);
	linkPtr = Blt_ChainAppend(clientPtr->events, notifyPtr);
    }
    if (proc == NULL) {
	Blt_ChainDeleteLink(clientPtr->events, linkPtr);
	Blt_Free(notifyPtr);
    } else {
	notifyPtr->proc = proc;
	notifyPtr->clientData = clientData;
	notifyPtr->mask = mask;
	notifyPtr->notifyPending = FALSE;
	notifyPtr->interp = clientPtr->treeObject->interp;
    }
}

void
Blt_TreeDeleteEventHandler(
    TreeClient *clientPtr,
    unsigned int mask,
    Blt_TreeNotifyEventProc *proc,
    ClientData clientData)
{
    Blt_ChainLink *linkPtr;
    EventHandler *notifyPtr;

    for(linkPtr = Blt_ChainFirstLink(clientPtr->events); 
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if ((notifyPtr->proc == proc) && (notifyPtr->mask == mask) &&
	    (notifyPtr->clientData == clientData)) {
	    if (notifyPtr->notifyPending) {
		Tcl_CancelIdleCall(NotifyIdleProc, notifyPtr);
	    }
	    Blt_ChainDeleteLink(clientPtr->events, linkPtr);
	    Blt_Free(notifyPtr);
	    return;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeNodePath --
 *
 *---------------------------------------------------------------------- 
 */
char *
Blt_TreeNodePath(Node *nodePtr, Tcl_DString *resultPtr)
{
    char **nameArr;		/* Used to stack the component names. */
    char *staticSpace[64];
    int nLevels;
    register int i;

    nLevels = nodePtr->depth;
    if (nLevels > 64) {
	nameArr = Blt_Malloc(nLevels * sizeof(char *));
	assert(nameArr);
    } else {
	nameArr = staticSpace;
    }
    for (i = nLevels - 1; i >= 0; i--) {
	/* Save the name of each ancestor in the name array. 
	 * Note that we ignore the root. */
	nameArr[i] = nodePtr->label;
	nodePtr = nodePtr->parent;
    }
    /* Append each the names in the array. */
    Tcl_DStringInit(resultPtr);
    for (i = 0; i < nLevels; i++) {
	Tcl_DStringAppendElement(resultPtr, nameArr[i]);
    }
    if (nameArr != staticSpace) {
	Blt_Free(nameArr);
    }
    return Tcl_DStringValue(resultPtr);
}

int
Blt_TreeArrayValueExists(
    TreeClient *clientPtr,
    Node *nodePtr,
    CONST char *arrayName, 
    CONST char *elemName)
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    register Value *valuePtr;

    key = Blt_TreeGetKey(arrayName);
    valuePtr = GetTreeValue((Tcl_Interp *)NULL, clientPtr, nodePtr, key);
    if (valuePtr == NULL) {
	return FALSE;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj((Tcl_Interp *)NULL, valuePtr->objPtr, &tablePtr) 
	!= TCL_OK) {
	return FALSE;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    return (hPtr != NULL);
}

int
Blt_TreeGetArrayValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    CONST char *arrayName,
    CONST char *elemName,
    Tcl_Obj **valueObjPtrPtr)
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    register Value *valuePtr;

    key = Blt_TreeGetKey(arrayName);
    valuePtr = GetTreeValue(interp, clientPtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find \"", arrayName, "(",
			     elemName, ")\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    *valueObjPtrPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);

    /* Reading any element of the array can cause a trace to fire. */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, clientPtr, nodePtr->treeObject, nodePtr, key, 
		   TREE_TRACE_READ);
    }
    return TCL_OK;
}

int
Blt_TreeSetArrayValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    CONST char *arrayName,
    CONST char *elemName,
    Tcl_Obj *valueObjPtr)	/* New value of element. */
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    register Value *valuePtr;
    unsigned int flags;
    int isNew;

    assert(valueObjPtr != NULL);

    /* 
     * Search for the array in the list of data fields.  If one
     * doesn't exist, create it.
     */
    key = Blt_TreeGetKey(arrayName);
    valuePtr = TreeCreateValue(nodePtr, key, &isNew);
    if ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't set private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    flags = TREE_TRACE_WRITE;
    if (isNew) {
	valuePtr->objPtr = Blt_NewArrayObj(0, (Tcl_Obj **)NULL);
	Tcl_IncrRefCount(valuePtr->objPtr);
	flags |= TREE_TRACE_CREATE;
    } else if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_InvalidateStringRep(valuePtr->objPtr);
    hPtr = Blt_CreateHashEntry(tablePtr, elemName, &isNew);
    assert(hPtr);

    Tcl_IncrRefCount(valueObjPtr);
    if (!isNew) {
	Tcl_Obj *oldValueObjPtr;

	/* An element by the same name already exists. Decrement the
	 * reference count of the old value. */

	oldValueObjPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);
	if (oldValueObjPtr != NULL) {
	    Tcl_DecrRefCount(oldValueObjPtr);
	}
    }
    Blt_SetHashValue(hPtr, valueObjPtr);

    /*
     * We don't handle traces on a per array element basis.  Setting
     * any element can fire traces for the value.
     */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, clientPtr, nodePtr->treeObject, nodePtr, 
		valuePtr->key, flags);
    }
    return TCL_OK;
}

int
Blt_TreeUnsetArrayValue(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,		/* Node to be updated. */
    CONST char *arrayName,
    CONST char *elemName)
{
    Blt_TreeKey key;		/* Name of field in node. */
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Tcl_Obj *valueObjPtr;
    Value *valuePtr;

    key = Blt_TreeGetKey(arrayName);
    valuePtr = TreeFindValue(nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_OK;
    }
    if ((valuePtr->owner != NULL) && (valuePtr->owner != clientPtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't unset private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    if (hPtr == NULL) {
	return TCL_OK;		/* Element doesn't exist, Ok. */
    }
    valueObjPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);
    Tcl_DecrRefCount(valueObjPtr);
    Blt_DeleteHashEntry(tablePtr, hPtr);

    /*
     * Un-setting any element in the array can cause the trace on the value
     * to fire.
     */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, clientPtr, nodePtr->treeObject, nodePtr, 
		valuePtr->key, TREE_TRACE_WRITE);
    }
    return TCL_OK;
}

int
Blt_TreeArrayNames(
    Tcl_Interp *interp,
    TreeClient *clientPtr,
    Node *nodePtr,
    CONST char *arrayName,
    Tcl_Obj *listObjPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable *tablePtr;
    Tcl_Obj *objPtr;
    Value *valuePtr;
    char *key;

    key = Blt_TreeGetKey(arrayName);
    valuePtr = GetTreeValue(interp, clientPtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr = (Blt_HashTable *)valuePtr->objPtr;
    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&cursor)) {
	objPtr = Tcl_NewStringObj(Blt_GetHashKey(tablePtr, hPtr), -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeShareTagTable --
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TreeShareTagTable(
    TreeClient *sourcePtr,
    TreeClient *targetPtr)
{
    sourcePtr->tagTablePtr->refCount++;
    if (targetPtr->tagTablePtr != NULL) {
	ReleaseTagTable(targetPtr->tagTablePtr);
    }
    targetPtr->tagTablePtr = sourcePtr->tagTablePtr;
    return TCL_OK;
}

int
Blt_TreeTagTableIsShared(TreeClient *clientPtr)
{
    return (clientPtr->tagTablePtr->refCount > 1);
}   

void
Blt_TreeClearTags(TreeClient *clientPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr, *h2Ptr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&clientPtr->tagTablePtr->tagTable, &cursor); 
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Blt_TreeTagEntry *tPtr;

	tPtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(&tPtr->nodeTable, (char *)node);
	if (h2Ptr != NULL) {
	    Blt_DeleteHashEntry(&tPtr->nodeTable, h2Ptr);
	}
    }
}

int
Blt_TreeHasTag(
    TreeClient *clientPtr,
    Blt_TreeNode node, 
    CONST char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_TreeTagEntry *tPtr;

    if (strcmp(tagName, "all") == 0) {
	return TRUE;
    }
    if ((strcmp(tagName, "root") == 0) && 
	(node == Blt_TreeRootNode(clientPtr))) {
	return TRUE;
    }
    hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
    if (hPtr == NULL) {
	return FALSE;
    }
    tPtr = Blt_GetHashValue(hPtr);
    hPtr = Blt_FindHashEntry(&tPtr->nodeTable, (char *)node);
    if (hPtr == NULL) {
	return FALSE;
    }
    return TRUE;
}

void
Blt_TreeAddTag(
    TreeClient *clientPtr,
    Blt_TreeNode node,
    CONST char *tagName)
{
    int isNew;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Blt_TreeTagEntry *tPtr;

    if ((strcmp(tagName, "all") == 0) || (strcmp(tagName, "root") == 0)) {
	return;
    }
    tablePtr = &clientPtr->tagTablePtr->tagTable;
    hPtr = Blt_CreateHashEntry(tablePtr, tagName, &isNew);
    assert(hPtr);
    if (isNew) {

	tPtr = Blt_Malloc(sizeof(Blt_TreeTagEntry));
	Blt_InitHashTable(&tPtr->nodeTable, BLT_ONE_WORD_KEYS);
	Blt_SetHashValue(hPtr, tPtr);
	tPtr->hashPtr = hPtr;
	tPtr->tagName = Blt_GetHashKey(tablePtr, hPtr);
    } else {
	tPtr = Blt_GetHashValue(hPtr);
    }
    hPtr = Blt_CreateHashEntry(&tPtr->nodeTable, (char *)node, &isNew);
    assert(hPtr);
    if (isNew) {
	Blt_SetHashValue(hPtr, node);
    }
}

void
Blt_TreeForgetTag(TreeClient *clientPtr, CONST char *tagName)
{
    if ((strcmp(tagName, "all") != 0) && (strcmp(tagName, "root") != 0)) {
	Blt_HashEntry *hPtr;
	
	hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
	if (hPtr != NULL) {
	    Blt_TreeTagEntry *tPtr;
	    
	    Blt_DeleteHashEntry(&clientPtr->tagTablePtr->tagTable, hPtr);
	    tPtr = Blt_GetHashValue(hPtr);
	    Blt_DeleteHashTable(&tPtr->nodeTable);
	    Blt_Free(tPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeTagHashTable --
 *
 *---------------------------------------------------------------------- 
 */
Blt_HashTable *
Blt_TreeTagHashTable(TreeClient *clientPtr, CONST char *tagName)
{
    Blt_HashEntry *hPtr;
   
    hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
    if (hPtr != NULL) {
	Blt_TreeTagEntry *tPtr;
	
	tPtr = Blt_GetHashValue(hPtr);
	return &tPtr->nodeTable;
    }
    return NULL;
}

Blt_HashEntry *
Blt_TreeFirstTag(TreeClient *clientPtr, Blt_HashSearch *cursorPtr)
{
    return Blt_FirstHashEntry(&clientPtr->tagTablePtr->tagTable, cursorPtr);
}

#if (SIZEOF_VOID_P == 8)
/*
 *----------------------------------------------------------------------
 *
 * HashOneWord --
 *
 *	Compute a one-word hash value of a 64-bit word, which then can
 *	be used to generate a hash index.
 *
 *	From Knuth, it's a multiplicative hash.  Multiplies an unsigned
 *	64-bit value with the golden ratio (sqrt(5) - 1) / 2.  The
 *	downshift value is 64 - n, when n is the log2 of the size of
 *	the hash table.
 *		
 * Results:
 *	The return value is a one-word summary of the information in
 *	64 bit word.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Blt_Hash
HashOneWord(
    uint64_t mask,
    unsigned int downshift,	        
    CONST void *key)
{
    uint64_t a0, a1;
    uint64_t y0, y1;
    uint64_t y2, y3; 
    uint64_t p1, p2;
    uint64_t result;

    /* Compute key * GOLDEN_RATIO in 128-bit arithmetic */
    a0 = (uint64_t)key & 0x00000000FFFFFFFF; 
    a1 = (uint64_t)key >> 32;
    
    y0 = a0 * 0x000000007f4a7c13;
    y1 = a0 * 0x000000009e3779b9; 
    y2 = a1 * 0x000000007f4a7c13;
    y3 = a1 * 0x000000009e3779b9; 
    y1 += y0 >> 32;		/* Can't carry */ 
    y1 += y2;			/* Might carry */
    if (y1 < y2) {
	y3 += (1LL << 32);	/* Propagate */ 
    }

    /* 128-bit product: p1 = loword, p2 = hiword */
    p1 = ((y1 & 0x00000000FFFFFFFF) << 32) + (y0 & 0x00000000FFFFFFFF);
    p2 = y3 + (y1 >> 32);
    
    /* Left shift the value downward by the size of the table */
    if (downshift > 0) { 
	if (downshift < 64) { 
	    result = ((p2 << (64 - downshift)) | (p1 >> (downshift & 63))); 
	} else { 
	    result = p2 >> (downshift & 63); 
	} 
    } else { 
	result = p1;
    } 
    /* Finally mask off the high bits */
    return (Blt_Hash)(result & mask);
}

#endif /* SIZEOF_VOID_P == 8 */

/*
 *----------------------------------------------------------------------
 *
 * RebuildTable --
 *
 *	This procedure is invoked when the ratio of entries to hash
 *	buckets becomes too large.  It creates a new table with a
 *	larger bucket array and moves all of the entries into the
 *	new table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets reallocated and entries get re-hashed to new
 *	buckets.
 *
 *----------------------------------------------------------------------
 */
static void
RebuildTable(Node *nodePtr)		/* Table to enlarge. */
{
    Value **newBucketPtr, **oldBuckets;
    register Value **bucketPtr, **endPtr;
    register Value *valuePtr, *nextPtr;
    unsigned int downshift;
    unsigned long mask;
    Value **buckets;
    size_t nBuckets;

    oldBuckets = (Value **)nodePtr->values;
    nBuckets = (1 << nodePtr->logSize);
    endPtr = oldBuckets + nBuckets;

    /*
     * Allocate and initialize the new bucket array, and set up
     * hashing constants for new array size.
     */
    nodePtr->logSize += 2;
    nBuckets = (1 << nodePtr->logSize);
    buckets = Blt_Calloc(nBuckets, sizeof(Value *));

    /*
     * Move all of the existing entries into the new bucket array,
     * based on their hash values.  
     */
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - nodePtr->logSize;
    for (bucketPtr = oldBuckets; bucketPtr < endPtr; bucketPtr++) {
	for (valuePtr = *bucketPtr; valuePtr != NULL; valuePtr = nextPtr) {
	    nextPtr = valuePtr->next;
	    newBucketPtr = buckets + RANDOM_INDEX(valuePtr->key);
	    valuePtr->next = *newBucketPtr;
	    *newBucketPtr = valuePtr;
	}
    }
    nodePtr->values = (Value *)buckets;
    Blt_Free(oldBuckets);
}

static void
ConvertValues(Node *nodePtr)
{
    unsigned int nBuckets;
    Value **buckets;
    unsigned int mask;
    int downshift;
    Value *valuePtr, *nextPtr, **bucketPtr;

    /*
     * Convert list of values into a hash table.
     */
    nodePtr->logSize = START_LOGSIZE;
    nBuckets = 1 << nodePtr->logSize;
    buckets = Blt_Calloc(nBuckets, sizeof(Value *));
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - nodePtr->logSize;
    for (valuePtr = nodePtr->values; valuePtr != NULL; valuePtr = nextPtr) {
	nextPtr = valuePtr->next;
	bucketPtr = buckets + RANDOM_INDEX(valuePtr->key);
	valuePtr->next = *bucketPtr;
	*bucketPtr = valuePtr;
    }
    nodePtr->values = (Value *)buckets;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDeleteValue --
 *
 *	Remove a single entry from a hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The entry given by entryPtr is deleted from its table and
 *	should never again be used by the caller.  It is up to the
 *	caller to free the clientData field of the entry, if that
 *	is relevant.
 *
 *----------------------------------------------------------------------
 */
static int
TreeDeleteValue(Node *nodePtr, Blt_TreeValue value)
{
    Value *valuePtr = value;
    register Value *prevPtr;
    
    if (nodePtr->logSize > 0) {
	Value **bucketPtr;
	unsigned int downshift;
	unsigned long mask;

	mask = (1 << nodePtr->logSize) - 1;
	downshift = DOWNSHIFT_START - nodePtr->logSize;
	
	bucketPtr = (Value **)nodePtr->values + RANDOM_INDEX(valuePtr->key);
	if (*bucketPtr == valuePtr) {
	    *bucketPtr = valuePtr->next;
	} else {
	    for (prevPtr = *bucketPtr; /*empty*/; prevPtr = prevPtr->next) {
		if (prevPtr == NULL) {
		    return TCL_ERROR; /* Can't find value in hash bucket. */
		}
		if (prevPtr->next == valuePtr) {
		    prevPtr->next = valuePtr->next;
		    break;
		}
	    }
	}
    } else {
	prevPtr = NULL;
	for (valuePtr = nodePtr->values; valuePtr != NULL; 
	     valuePtr = valuePtr->next) {
	    if (valuePtr == value) {
		break;
	    }
	    prevPtr = valuePtr;
	}
	if (valuePtr == NULL) {
	    return TCL_ERROR;	/* Can't find value in list. */
	}
	if (prevPtr == NULL) {
	    nodePtr->values = valuePtr->next;
	} else {
	    prevPtr->next = valuePtr->next;
	}
    }
    nodePtr->nValues--;
    FreeValue(nodePtr, valuePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDestroyValues --
 *
 *	Free up everything associated with a hash table except for
 *	the record for the table itself.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The hash table is no longer useable.
 *
 *----------------------------------------------------------------------
 */
static void
TreeDestroyValues(Node *nodePtr)
{
    register Value *valuePtr;
    Value *nextPtr;

    /*
     * Free up all the entries in the table.
     */
    if (nodePtr->values != NULL) { 
	return;
    }
    if (nodePtr->logSize > 0) {
	Value **buckets;
	int nBuckets;
	int i;

	buckets = (Value **)nodePtr->values;
	nBuckets = (1 << nodePtr->logSize);
	for (i = 0; i < nBuckets; i++) {
	    for (valuePtr = buckets[i]; valuePtr != NULL; valuePtr = nextPtr) {
		nextPtr = valuePtr->next;
		FreeValue(nodePtr, valuePtr);
	    }
	}
	Blt_Free(buckets);
    } else {
	for (valuePtr = nodePtr->values; valuePtr != NULL; valuePtr = nextPtr) {
	    nextPtr = valuePtr->next;
	    FreeValue(nodePtr, valuePtr);
	}
    }
    nodePtr->values = NULL;
    nodePtr->nValues = 0;
    nodePtr->logSize = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeFirstValue --
 *
 *	Locate the first entry in a hash table and set up a record
 *	that can be used to step through all the remaining entries
 *	of the table.
 *
 * Results:
 *	The return value is a pointer to the first value in tablePtr,
 *	or NULL if tablePtr has no entries in it.  The memory at
 *	*searchPtr is initialized so that subsequent calls to
 *	Blt_TreeNextValue will return all of the values in the table,
 *	one at a time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Value *
TreeFirstValue(
    Node *nodePtr,
    Blt_TreeKeySearch *searchPtr) /* Place to store information about
				   * progress through the table. */
{
    searchPtr->node = nodePtr;
    searchPtr->nextIndex = 0;
    if (nodePtr->logSize > 0) {
	searchPtr->nextValue = NULL;
    } else {
	searchPtr->nextValue = nodePtr->values;
    }
    return TreeNextValue(searchPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNextValue --
 *
 *	Once a hash table enumeration has been initiated by calling
 *	Blt_TreeFirstValue, this procedure may be called to return
 *	successive elements of the table.
 *
 * Results:
 *	The return value is the next entry in the hash table being
 *	enumerated, or NULL if the end of the table is reached.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Value *
TreeNextValue(
    Blt_TreeKeySearch *searchPtr) /* Place to store information about
				   * progress through the table.  Must
				   * have been initialized by calling
				   * Blt_TreeFirstValue. */
{
    Value *valuePtr;

    if (searchPtr->node->logSize > 0) {
	size_t nBuckets;
	Value **buckets;

	nBuckets = (1 << searchPtr->node->logSize);
	buckets = (Value **)searchPtr->node->values;
	while (searchPtr->nextValue == NULL) {
	    if (searchPtr->nextIndex >= nBuckets) {
		return NULL;
	    }
	    searchPtr->nextValue = buckets[searchPtr->nextIndex];
	    searchPtr->nextIndex++;
	}
    }
    valuePtr = searchPtr->nextValue;
    if (valuePtr != NULL) {
	searchPtr->nextValue = valuePtr->next;
    }
    return valuePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeFindValue --
 *
 *	Given a hash table with one-word keys, and a one-word key, find
 *	the entry with a matching key.
 *
 * Results:
 *	The return value is a token for the matching entry in the
 *	hash table, or NULL if there was no matching entry.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Value *
TreeFindValue(
    Node *nodePtr,
    Blt_TreeKey key)		/* Key to use to find matching entry. */
{
    register Value *valuePtr;
    Value *bucket;

    if (nodePtr->logSize > 0) {
	unsigned int downshift;
	unsigned long mask;

	mask = (1 << nodePtr->logSize) - 1;
	downshift = DOWNSHIFT_START - nodePtr->logSize;
	bucket = ((Value **)(nodePtr->values))[RANDOM_INDEX((void *)key)];
    } else {
	bucket = nodePtr->values; /* Single chain list. */
    }
    /*
     * Search all of the entries in the appropriate bucket.
     */
    for (valuePtr = bucket; valuePtr != NULL; valuePtr = valuePtr->next) {
	if (valuePtr->key == key) {
	    return valuePtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TreeCreateValue --
 *
 *	Find the value with a matching key.  If there is no matching 
 *	value, then create a new one.
 *
 * Results:
 *	The return value is a pointer to the matching value.  If this
 *	is a newly-created value, then *newPtr will be set to a non-zero
 *	value;  otherwise *newPtr will be set to 0.  
 *
 * Side effects:
 *	A new value may be added to the hash table.
 *
 *----------------------------------------------------------------------
 */
static Value *
TreeCreateValue(
    Node *nodePtr,
    Blt_TreeKey key,		/* Key to use to find or create matching
				 * entry. */
    int *newPtr)		/* Store info here telling whether a new
				 * entry was created. */
{
    register Value *valuePtr;
    
    /* 
     * Check if there as so many values that storage should be
     * converted from a hash table instead of a list. 
     */
    if ((nodePtr->logSize == 0) && (nodePtr->nValues > MAX_LIST_VALUES)) {
	ConvertValues(nodePtr);
    }
    if (nodePtr->logSize > 0) {
	Value **bucketPtr;
	size_t nBuckets;
	unsigned int downshift;
	unsigned long mask;

	nBuckets = (1 << nodePtr->logSize);
	mask = nBuckets - 1;
	downshift = DOWNSHIFT_START - nodePtr->logSize;
	bucketPtr = (Value **)nodePtr->values + RANDOM_INDEX((void *)key);
	
	*newPtr = FALSE;
	for (valuePtr = *bucketPtr; valuePtr != NULL; 
	     valuePtr = valuePtr->next) {
	    if (valuePtr->key == key) {
		return valuePtr;
	    }
	}
	
	/* Value not found. Add a new value to the bucket. */
	*newPtr = TRUE;
	valuePtr = Blt_PoolAllocItem(nodePtr->treeObject->valuePool, 
		sizeof(Value));
	valuePtr->key = key;
	valuePtr->owner = NULL;
	valuePtr->next = *bucketPtr;
	valuePtr->objPtr = NULL;
	*bucketPtr = valuePtr;
	nodePtr->nValues++;
	
	/*
	 * If the table has exceeded a decent size, rebuild it with many
	 * more buckets.
	 */
	if ((unsigned int)nodePtr->nValues >= (nBuckets * 3)) {
	    RebuildTable(nodePtr);
	}
    } else {
	Value *prevPtr;

	prevPtr = NULL;
	*newPtr = FALSE;
	for (valuePtr = nodePtr->values; valuePtr != NULL; 
	     valuePtr = valuePtr->next) {
	    if (valuePtr->key == key) {
		return valuePtr;
	    }
	    prevPtr = valuePtr;
	}
	/* Value not found. Add a new value to the list. */
	*newPtr = TRUE;
	valuePtr = Blt_PoolAllocItem(nodePtr->treeObject->valuePool, 
		sizeof(Value));
	valuePtr->key = key;
	valuePtr->owner = NULL;
	valuePtr->next = NULL;
	valuePtr->objPtr = NULL;
	if (prevPtr == NULL) {
	    nodePtr->values = valuePtr;
	} else {
	    prevPtr->next = valuePtr;
	}
	nodePtr->nValues++;
    }
    return valuePtr;
}

#endif /* NO_TREE */
