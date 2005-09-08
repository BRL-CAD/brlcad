/*
 * bltList.c --
 *
 *	The module implements generic linked lists.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
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
 */

#include "bltInt.h"
#include "bltList.h"

static struct Blt_ListNodeStruct *
FindString(listPtr, key)
    struct Blt_ListStruct *listPtr; /* List to search */
    CONST char *key;		/* Key to match */
{
    register struct Blt_ListNodeStruct *nodePtr;
    char c;

    c = key[0];
    for (nodePtr = listPtr->headPtr; nodePtr != NULL; 
	 nodePtr = nodePtr->nextPtr) {
	if ((c == nodePtr->key.string[0]) &&
	    (strcmp(key, nodePtr->key.string) == 0)) {
	    return nodePtr;
	}
    }
    return NULL;
}

static Blt_ListNode
FindOneWord(listPtr, key)
    struct Blt_ListStruct *listPtr; /* List to search */
    CONST char *key;		/* Key to match */
{
    register struct Blt_ListNodeStruct *nodePtr;

    for (nodePtr = listPtr->headPtr; nodePtr != NULL; 
	 nodePtr = nodePtr->nextPtr) {
	if (key == nodePtr->key.oneWordValue) {
	    return nodePtr;
	}
    }
    return NULL;
}

static Blt_ListNode
FindArray(listPtr, key)
    struct Blt_ListStruct *listPtr; /* List to search */
    CONST char *key;		/* Key to match */
{
    register struct Blt_ListNodeStruct *nodePtr;
    int nBytes;

    nBytes = sizeof(int) * listPtr->type;
    for (nodePtr = listPtr->headPtr; nodePtr != NULL; 
	 nodePtr = nodePtr->nextPtr) {
	if (memcmp(key, nodePtr->key.words, nBytes) == 0) {
	    return nodePtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeNode --
 *
 *	Free the memory allocated for the node.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
FreeNode(nodePtr)
    struct Blt_ListNodeStruct *nodePtr;
{
    Blt_Free(nodePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListCreate --
 *
 *	Creates a new linked list structure and initializes its pointers
 *
 * Results:
 *	Returns a pointer to the newly created list structure.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
Blt_List 
Blt_ListCreate(type)
    int type;
{
    struct Blt_ListStruct *listPtr;

    listPtr = Blt_Malloc(sizeof(struct Blt_ListStruct));
    if (listPtr != NULL) {
	Blt_ListInit(listPtr, type);
    }
    return listPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListCreateNode --
 *
 *	Creates a list node holder.  This routine does not insert
 *	the node into the list, nor does it no attempt to maintain
 *	consistency of the keys.  For example, more than one node
 *	may use the same key.
 *
 * Results:
 *	The return value is the pointer to the newly created node.
 *
 * Side Effects:
 *	The key is not copied, only the Uid is kept.  It is assumed
 *	this key will not change in the life of the node.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
Blt_ListNode
Blt_ListCreateNode(listPtr, key)
    struct Blt_ListStruct *listPtr;
    CONST char *key;		/* Unique key to reference object */
{
    register struct Blt_ListNodeStruct *nodePtr;
    int keySize;

    if (listPtr->type == BLT_STRING_KEYS) {
	keySize = strlen(key) + 1;
    } else if (listPtr->type == BLT_ONE_WORD_KEYS) {
	keySize = sizeof(int);
    } else {
	keySize = sizeof(int) * listPtr->type;
    }
    nodePtr = Blt_Calloc(1, sizeof(struct Blt_ListNodeStruct) + keySize - 4);
    assert(nodePtr);
    nodePtr->clientData = NULL;
    nodePtr->nextPtr = nodePtr->prevPtr = NULL;
    nodePtr->listPtr = listPtr;
    switch (listPtr->type) {
    case BLT_STRING_KEYS:
	strcpy(nodePtr->key.string, key);
	break;
    case BLT_ONE_WORD_KEYS:
	nodePtr->key.oneWordValue = key;
	break;
    default:
	memcpy(nodePtr->key.words, key, keySize);
	break;
    }
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListReset --
 *
 *	Removes all the entries from a list, removing pointers to the
 *	objects and keys (not the objects or keys themselves).  The
 *	node counter is reset to zero.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListReset(listPtr)
    struct Blt_ListStruct *listPtr; /* List to clear */
{
    if (listPtr != NULL) {
	register struct Blt_ListNodeStruct *oldPtr;
	register struct Blt_ListNodeStruct *nodePtr = listPtr->headPtr;

	while (nodePtr != NULL) {
	    oldPtr = nodePtr;
	    nodePtr = nodePtr->nextPtr;
	    FreeNode(oldPtr);
	}
	Blt_ListInit(listPtr, listPtr->type);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListDestroy
 *
 *     Frees all list structures
 *
 * Results:
 *	Returns a pointer to the newly created list structure.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListDestroy(listPtr)
    struct Blt_ListStruct *listPtr;
{
    if (listPtr != NULL) {
	Blt_ListReset(listPtr);
	Blt_Free(listPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListInit --
 *
 *	Initializes a linked list.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListInit(listPtr, type)
    struct Blt_ListStruct *listPtr;
    int type;
{
    listPtr->nNodes = 0;
    listPtr->headPtr = listPtr->tailPtr = NULL;
    listPtr->type = type;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListLinkAfter --
 *
 *	Inserts an node following a given node.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListLinkAfter(listPtr, nodePtr, afterPtr)
    struct Blt_ListStruct *listPtr;
    struct Blt_ListNodeStruct *nodePtr;
    struct Blt_ListNodeStruct *afterPtr;
{
    if (listPtr->headPtr == NULL) {
	listPtr->tailPtr = listPtr->headPtr = nodePtr;
    } else {
	if (afterPtr == NULL) {
	    /* Prepend to the front of the list */
	    nodePtr->nextPtr = listPtr->headPtr;
	    nodePtr->prevPtr = NULL;
	    listPtr->headPtr->prevPtr = nodePtr;
	    listPtr->headPtr = nodePtr;
	} else {
	    nodePtr->nextPtr = afterPtr->nextPtr;
	    nodePtr->prevPtr = afterPtr;
	    if (afterPtr == listPtr->tailPtr) {
		listPtr->tailPtr = nodePtr;
	    } else {
		afterPtr->nextPtr->prevPtr = nodePtr;
	    }
	    afterPtr->nextPtr = nodePtr;
	}
    }
    nodePtr->listPtr = listPtr;
    listPtr->nNodes++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListLinkBefore --
 *
 *	Inserts an node preceding a given node.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListLinkBefore(listPtr, nodePtr, beforePtr)
    struct Blt_ListStruct *listPtr; /* List to contain new node */
    struct Blt_ListNodeStruct *nodePtr;	/* New node to be inserted */
    struct Blt_ListNodeStruct *beforePtr;	/* Node to link before */
{
    if (listPtr->headPtr == NULL) {
	listPtr->tailPtr = listPtr->headPtr = nodePtr;
    } else {
	if (beforePtr == NULL) {
	    /* Append onto the end of the list */
	    nodePtr->nextPtr = NULL;
	    nodePtr->prevPtr = listPtr->tailPtr;
	    listPtr->tailPtr->nextPtr = nodePtr;
	    listPtr->tailPtr = nodePtr;
	} else {
	    nodePtr->prevPtr = beforePtr->prevPtr;
	    nodePtr->nextPtr = beforePtr;
	    if (beforePtr == listPtr->headPtr) {
		listPtr->headPtr = nodePtr;
	    } else {
		beforePtr->prevPtr->nextPtr = nodePtr;
	    }
	    beforePtr->prevPtr = nodePtr;
	}
    }
    nodePtr->listPtr = listPtr;
    listPtr->nNodes++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListUnlinkNode --
 *
 *	Unlinks an node from the given list. The node itself is
 *	not deallocated, but only removed from the list.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListUnlinkNode(nodePtr)
    struct Blt_ListNodeStruct *nodePtr;
{
    struct Blt_ListStruct *listPtr;

    listPtr = nodePtr->listPtr;
    if (listPtr != NULL) {
	if (listPtr->headPtr == nodePtr) {
	    listPtr->headPtr = nodePtr->nextPtr;
	}
	if (listPtr->tailPtr == nodePtr) {
	    listPtr->tailPtr = nodePtr->prevPtr;
	}
	if (nodePtr->nextPtr != NULL) {
	    nodePtr->nextPtr->prevPtr = nodePtr->prevPtr;
	}
	if (nodePtr->prevPtr != NULL) {
	    nodePtr->prevPtr->nextPtr = nodePtr->nextPtr;
	}
	nodePtr->listPtr = NULL;
	listPtr->nNodes--;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListGetNode --
 *
 *	Find the first node matching the key given.
 *
 * Results:
 *	Returns the pointer to the node.  If no node matching
 *	the key given is found, then NULL is returned.
 *
 *----------------------------------------------------------------------
 */

/*LINTLIBRARY*/
Blt_ListNode
Blt_ListGetNode(listPtr, key)
    struct Blt_ListStruct *listPtr; /* List to search */
    CONST char *key;		/* Key to match */
{
    if (listPtr != NULL) {
	switch (listPtr->type) {
	case BLT_STRING_KEYS:
	    return FindString(listPtr, key);
	case BLT_ONE_WORD_KEYS:
	    return FindOneWord(listPtr, key);
	default:
	    return FindArray(listPtr, key);
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListDeleteNode --
 *
 *	Unlinks and deletes the given node.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListDeleteNode(nodePtr)
    struct Blt_ListNodeStruct *nodePtr;
{
    Blt_ListUnlinkNode(nodePtr);
    FreeNode(nodePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListDeleteNodeByKey --
 *
 *	Find the node and free the memory allocated for the node.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListDeleteNodeByKey(listPtr, key)
    struct Blt_ListStruct *listPtr;
    CONST char *key;
{
    struct Blt_ListNodeStruct *nodePtr;

    nodePtr = Blt_ListGetNode(listPtr, key);
    if (nodePtr != NULL) {
	Blt_ListDeleteNode(nodePtr);
    }
}

/*LINTLIBRARY*/
Blt_ListNode
Blt_ListAppend(listPtr, key, clientData)
    struct Blt_ListStruct *listPtr;
    CONST char *key;
    ClientData clientData;
{
    struct Blt_ListNodeStruct *nodePtr;

    nodePtr = Blt_ListCreateNode(listPtr, key);
    Blt_ListSetValue(nodePtr, clientData);
    Blt_ListAppendNode(listPtr, nodePtr);
    return nodePtr;
}

/*LINTLIBRARY*/
Blt_ListNode
Blt_ListPrepend(listPtr, key, clientData)
    struct Blt_ListStruct *listPtr;
    CONST char *key;
    ClientData clientData;
{
    struct Blt_ListNodeStruct *nodePtr;

    nodePtr = Blt_ListCreateNode(listPtr, key);
    Blt_ListSetValue(nodePtr, clientData);
    Blt_ListPrependNode(listPtr, nodePtr);
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListGetNthNode --
 *
 *	Find the node based upon a given position in list.
 *
 * Results:
 *	Returns the pointer to the node, if that numbered element
 *	exists. Otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
Blt_ListNode
Blt_ListGetNthNode(listPtr, position, direction)
    struct Blt_ListStruct *listPtr; /* List to traverse */
    int position;		/* Index of node to select from front
				 * or back of the list. */
    int direction;
{
    register struct Blt_ListNodeStruct *nodePtr;

    if (listPtr != NULL) {
	if (direction > 0) {
	    for (nodePtr = listPtr->headPtr; nodePtr != NULL; 
		 nodePtr = nodePtr->nextPtr) {
		if (position == 0) {
		    return nodePtr;
		}
		position--;
	    }
	} else {
	    for (nodePtr = listPtr->tailPtr; nodePtr != NULL; 
		 nodePtr = nodePtr->prevPtr) {
		if (position == 0) {
		    return nodePtr;
		}
		position--;
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ListSort --
 *
 *	Find the node based upon a given position in list.
 *
 * Results:
 *	Returns the pointer to the node, if that numbered element
 *	exists. Otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_ListSort(listPtr, proc)
    struct Blt_ListStruct *listPtr; /* List to traverse */
    Blt_ListCompareProc *proc;
{
    struct Blt_ListNodeStruct **nodeArr;
    register struct Blt_ListNodeStruct *nodePtr;
    register int i;

    if (listPtr->nNodes < 2) {
	return;
    }
    nodeArr = Blt_Malloc(sizeof(Blt_List) * (listPtr->nNodes + 1));
    if (nodeArr == NULL) {
	return;			/* Out of memory. */
    }
    i = 0;
    for (nodePtr = listPtr->headPtr; nodePtr != NULL; 
	 nodePtr = nodePtr->nextPtr) {
	nodeArr[i++] = nodePtr;
    }
    qsort((char *)nodeArr, listPtr->nNodes, 
		sizeof(struct Blt_ListNodeStruct *), (QSortCompareProc *)proc);

    /* Rethread the list. */
    nodePtr = nodeArr[0];
    listPtr->headPtr = nodePtr;
    nodePtr->prevPtr = NULL;
    for (i = 1; i < listPtr->nNodes; i++) {
	nodePtr->nextPtr = nodeArr[i];
	nodePtr->nextPtr->prevPtr = nodePtr;
	nodePtr = nodePtr->nextPtr;
    }
    listPtr->tailPtr = nodePtr;
    nodePtr->nextPtr = NULL;
    Blt_Free(nodeArr);
}
