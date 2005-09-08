/*
 * bltChain.h --
 *
 * Copyright 1993-2000 Lucent Technologies, Inc.
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
#ifndef _BLT_CHAIN_H
#define _BLT_CHAIN_H

typedef struct Blt_ChainLinkStruct Blt_ChainLink;

/*
 * A Blt_ChainLink is the container structure for the Blt_Chain.
 */

struct Blt_ChainLinkStruct {
    Blt_ChainLink *prevPtr;	/* Link to the previous link */
    Blt_ChainLink *nextPtr;	/* Link to the next link */
    ClientData clientData;	/* Pointer to the data object */
};

typedef int (Blt_ChainCompareProc) _ANSI_ARGS_((Blt_ChainLink **l1PtrPtr, 
	Blt_ChainLink **l2PtrPtr));

/*
 * A Blt_Chain is a doubly chained list structure.
 */
typedef struct {
    Blt_ChainLink *headPtr;	/* Pointer to first element in chain */
    Blt_ChainLink *tailPtr;	/* Pointer to last element in chain */
    int nLinks;			/* Number of elements in chain */
} Blt_Chain;

extern void Blt_ChainInit _ANSI_ARGS_((Blt_Chain * chainPtr));
extern Blt_Chain *Blt_ChainCreate _ANSI_ARGS_(());
extern void Blt_ChainDestroy _ANSI_ARGS_((Blt_Chain * chainPtr));
extern Blt_ChainLink *Blt_ChainNewLink _ANSI_ARGS_((void));
extern Blt_ChainLink *Blt_ChainAllocLink _ANSI_ARGS_((unsigned int size));
extern Blt_ChainLink *Blt_ChainAppend _ANSI_ARGS_((Blt_Chain * chainPtr,
	ClientData clientData));
extern Blt_ChainLink *Blt_ChainPrepend _ANSI_ARGS_((Blt_Chain * chainPtr,
	ClientData clientData));
extern void Blt_ChainReset _ANSI_ARGS_((Blt_Chain * chainPtr));
extern void Blt_ChainLinkAfter _ANSI_ARGS_((Blt_Chain * chainPtr,
	Blt_ChainLink * linkPtr, Blt_ChainLink * afterLinkPtr));
extern void Blt_ChainLinkBefore _ANSI_ARGS_((Blt_Chain * chainPtr,
	Blt_ChainLink * linkPtr, Blt_ChainLink * beforeLinkPtr));
extern void Blt_ChainUnlinkLink _ANSI_ARGS_((Blt_Chain * chainPtr,
	Blt_ChainLink * linkPtr));
extern void Blt_ChainDeleteLink _ANSI_ARGS_((Blt_Chain * chainPtr,
	Blt_ChainLink * linkPtr));
extern Blt_ChainLink *Blt_ChainGetNthLink _ANSI_ARGS_((Blt_Chain * chainPtr, int n));
extern void Blt_ChainSort _ANSI_ARGS_((Blt_Chain * chainPtr,
	Blt_ChainCompareProc * proc));

#define Blt_ChainGetLength(c)	(((c) == NULL) ? 0 : (c)->nLinks)
#define Blt_ChainFirstLink(c)	(((c) == NULL) ? NULL : (c)->headPtr)
#define Blt_ChainLastLink(c)	(((c) == NULL) ? NULL : (c)->tailPtr)
#define Blt_ChainPrevLink(l)	((l)->prevPtr)
#define Blt_ChainNextLink(l) 	((l)->nextPtr)
#define Blt_ChainGetValue(l)  	((l)->clientData)
#define Blt_ChainSetValue(l, value) ((l)->clientData = (ClientData)(value))
#define Blt_ChainAppendLink(c, l) \
	(Blt_ChainLinkBefore((c), (l), (Blt_ChainLink *)NULL))
#define Blt_ChainPrependLink(c, l) \
	(Blt_ChainLinkAfter((c), (l), (Blt_ChainLink *)NULL))

#endif /* _BLT_CHAIN_H */
