/*
 * bltChain.c --
 *
 *	The module implements a generic linked list package.
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
#include "bltChain.h"

#ifndef ALIGN
#define ALIGN(a) \
	(((size_t)a + (sizeof(double) - 1)) & (~(sizeof(double) - 1)))
#endif /* ALIGN */

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainCreate --
 *
 *	Creates a new linked list (chain) structure and initializes 
 *	its pointers;
 *
 * Results:
 *	Returns a pointer to the newly created chain structure.
 *
 *----------------------------------------------------------------------
 */
Blt_Chain *
Blt_ChainCreate()
{
    Blt_Chain *chainPtr;

    chainPtr = Blt_Malloc(sizeof(Blt_Chain));
    if (chainPtr != NULL) {
	Blt_ChainInit(chainPtr);
    }
    return chainPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainAllocLink --
 *
 *	Creates a new chain link.  Unlink Blt_ChainNewLink, this 
 *	routine also allocates extra memory in the node for data.
 *
 * Results:
 *	The return value is the pointer to the newly created entry.
 *
 *----------------------------------------------------------------------
 */
Blt_ChainLink *
Blt_ChainAllocLink(extraSize)
    unsigned int extraSize;
{
    Blt_ChainLink *linkPtr;
    unsigned int linkSize;

    linkSize = ALIGN(sizeof(Blt_ChainLink));
    linkPtr = Blt_Calloc(1, linkSize + extraSize);
    assert(linkPtr);
    if (extraSize > 0) {
	/* Point clientData at the memory beyond the normal structure. */
	linkPtr->clientData = (ClientData)((char *)linkPtr + linkSize);
    }
    return linkPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainNewLink --
 *
 *	Creates a new link.
 *
 * Results:
 *	The return value is the pointer to the newly created link.
 *
 *----------------------------------------------------------------------
 */
Blt_ChainLink *
Blt_ChainNewLink()
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_Malloc(sizeof(Blt_ChainLink));
    assert(linkPtr);
    linkPtr->clientData = NULL;
    linkPtr->nextPtr = linkPtr->prevPtr = NULL;
    return linkPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainReset --
 *
 *	Removes all the links from the chain, freeing the memory for
 *	each link.  Memory pointed to by the link (clientData) is not
 *	freed.  It's the caller's responsibility to deallocate it.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_ChainReset(chainPtr)
    Blt_Chain *chainPtr;	/* Chain to clear */
{
    if (chainPtr != NULL) {
	Blt_ChainLink *oldPtr;
	Blt_ChainLink *linkPtr = chainPtr->headPtr;

	while (linkPtr != NULL) {
	    oldPtr = linkPtr;
	    linkPtr = linkPtr->nextPtr;
	    Blt_Free(oldPtr);
	}
	Blt_ChainInit(chainPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainDestroy
 *
 *     Frees all the nodes from the chain and deallocates the memory
 *     allocated for the chain structure itself.  It's assumed that
 *     the chain was previous allocated by Blt_ChainCreate.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainDestroy(chainPtr)
    Blt_Chain *chainPtr;
{
    if (chainPtr != NULL) {
	Blt_ChainReset(chainPtr);
	Blt_Free(chainPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainInit --
 *
 *	Initializes a linked list.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainInit(chainPtr)
    Blt_Chain *chainPtr;
{
    chainPtr->nLinks = 0;
    chainPtr->headPtr = chainPtr->tailPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainLinkAfter --
 *
 *	Inserts an entry following a given entry.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainLinkAfter(chainPtr, linkPtr, afterPtr)
    Blt_Chain *chainPtr;
    Blt_ChainLink *linkPtr, *afterPtr;
{
    if (chainPtr->headPtr == NULL) {
	chainPtr->tailPtr = chainPtr->headPtr = linkPtr;
    } else {
	if (afterPtr == NULL) {
	    /* Prepend to the front of the chain */
	    linkPtr->nextPtr = chainPtr->headPtr;
	    linkPtr->prevPtr = NULL;
	    chainPtr->headPtr->prevPtr = linkPtr;
	    chainPtr->headPtr = linkPtr;
	} else {
	    linkPtr->nextPtr = afterPtr->nextPtr;
	    linkPtr->prevPtr = afterPtr;
	    if (afterPtr == chainPtr->tailPtr) {
		chainPtr->tailPtr = linkPtr;
	    } else {
		afterPtr->nextPtr->prevPtr = linkPtr;
	    }
	    afterPtr->nextPtr = linkPtr;
	}
    }
    chainPtr->nLinks++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainLinkBefore --
 *
 *	Inserts a link preceding a given link.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainLinkBefore(chainPtr, linkPtr, beforePtr)
    Blt_Chain *chainPtr;	/* Chain to contain new entry */
    Blt_ChainLink *linkPtr;	/* New entry to be inserted */
    Blt_ChainLink *beforePtr;	/* Entry to link before */
{
    if (chainPtr->headPtr == NULL) {
	chainPtr->tailPtr = chainPtr->headPtr = linkPtr;
    } else {
	if (beforePtr == NULL) {
	    /* Append to the end of the chain. */
	    linkPtr->nextPtr = NULL;
	    linkPtr->prevPtr = chainPtr->tailPtr;
	    chainPtr->tailPtr->nextPtr = linkPtr;
	    chainPtr->tailPtr = linkPtr;
	} else {
	    linkPtr->prevPtr = beforePtr->prevPtr;
	    linkPtr->nextPtr = beforePtr;
	    if (beforePtr == chainPtr->headPtr) {
		chainPtr->headPtr = linkPtr;
	    } else {
		beforePtr->prevPtr->nextPtr = linkPtr;
	    }
	    beforePtr->prevPtr = linkPtr;
	}
    }
    chainPtr->nLinks++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainUnlinkLink --
 *
 *	Unlinks a link from the chain. The link is not deallocated, 
 *	but only removed from the chain.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainUnlinkLink(chainPtr, linkPtr)
    Blt_Chain *chainPtr;
    Blt_ChainLink *linkPtr;
{
    int unlinked;		/* Indicates if the link is actually
				 * removed from the chain. */

    unlinked = FALSE;
    if (chainPtr->headPtr == linkPtr) {
	chainPtr->headPtr = linkPtr->nextPtr;
	unlinked = TRUE;
    }
    if (chainPtr->tailPtr == linkPtr) {
	chainPtr->tailPtr = linkPtr->prevPtr;
	unlinked = TRUE;
    }
    if (linkPtr->nextPtr != NULL) {
	linkPtr->nextPtr->prevPtr = linkPtr->prevPtr;
	unlinked = TRUE;
    }
    if (linkPtr->prevPtr != NULL) {
	linkPtr->prevPtr->nextPtr = linkPtr->nextPtr;
	unlinked = TRUE;
    }
    if (unlinked) {
	chainPtr->nLinks--;
    }
    linkPtr->prevPtr = linkPtr->nextPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainDeleteLink --
 *
 *	Unlinks and also frees the given link.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainDeleteLink(chainPtr, linkPtr)
    Blt_Chain *chainPtr;
    Blt_ChainLink *linkPtr;
{
    Blt_ChainUnlinkLink(chainPtr, linkPtr);
    Blt_Free(linkPtr);
}

Blt_ChainLink *
Blt_ChainAppend(chainPtr, clientData)
    Blt_Chain *chainPtr;
    ClientData clientData;
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_ChainNewLink();
    Blt_ChainLinkBefore(chainPtr, linkPtr, (Blt_ChainLink *)NULL);
    Blt_ChainSetValue(linkPtr, clientData);
    return linkPtr;
}

Blt_ChainLink *
Blt_ChainPrepend(chainPtr, clientData)
    Blt_Chain *chainPtr;
    ClientData clientData;
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_ChainNewLink();
    Blt_ChainLinkAfter(chainPtr, linkPtr, (Blt_ChainLink *)NULL);
    Blt_ChainSetValue(linkPtr, clientData);
    return linkPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainGetNthLink --
 *
 *	Find the link at the given position in the chain.
 *
 * Results:
 *	Returns the pointer to the link, if that numbered link
 *	exists. Otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
Blt_ChainLink *
Blt_ChainGetNthLink(chainPtr, position)
    Blt_Chain *chainPtr;	/* Chain to traverse */
    int position;		/* Index of link to select from front
				 * or back of the chain. */
{
    Blt_ChainLink *linkPtr;

    if (chainPtr != NULL) {
	for (linkPtr = chainPtr->headPtr; linkPtr != NULL;
	    linkPtr = linkPtr->nextPtr) {
	    if (position == 0) {
		return linkPtr;
	    }
	    position--;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ChainSort --
 *
 *	Sorts the chain according to the given comparison routine.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The chain is reordered.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ChainSort(chainPtr, proc)
    Blt_Chain *chainPtr;	/* Chain to traverse */
    Blt_ChainCompareProc *proc;
{
    Blt_ChainLink **linkArr;
    register Blt_ChainLink *linkPtr;
    register int i;

    if (chainPtr->nLinks < 2) {
	return;
    }
    linkArr = Blt_Malloc(sizeof(Blt_ChainLink *) * (chainPtr->nLinks + 1));
    if (linkArr == NULL) {
	return;			/* Out of memory. */
    }
    i = 0;
    for (linkPtr = chainPtr->headPtr; linkPtr != NULL; 
	 linkPtr = linkPtr->nextPtr) { 
	linkArr[i++] = linkPtr;
    }
    qsort((char *)linkArr, chainPtr->nLinks, sizeof(Blt_ChainLink *),
	(QSortCompareProc *)proc);

    /* Rethread the chain. */
    linkPtr = linkArr[0];
    chainPtr->headPtr = linkPtr;
    linkPtr->prevPtr = NULL;
    for (i = 1; i < chainPtr->nLinks; i++) {
	linkPtr->nextPtr = linkArr[i];
	linkPtr->nextPtr->prevPtr = linkPtr;
	linkPtr = linkPtr->nextPtr;
    }
    chainPtr->tailPtr = linkPtr;
    linkPtr->nextPtr = NULL;
    Blt_Free(linkArr);
}
