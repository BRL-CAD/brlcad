/*
 * bltList.h --
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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
#ifndef _BLT_LIST_H
#define _BLT_LIST_H

typedef struct Blt_ListStruct *Blt_List;
typedef struct Blt_ListNodeStruct *Blt_ListNode;

/*
 * A Blt_ListNode is the container structure for the Blt_List.
 */
struct Blt_ListNodeStruct {
    struct Blt_ListNodeStruct *prevPtr; /* Link to the previous node */
    struct Blt_ListNodeStruct *nextPtr; /* Link to the next node */
    ClientData clientData;	/* Pointer to the data object */
    struct Blt_ListStruct *listPtr; /* List to eventually insert node */
    union {			/* Key has one of these forms: */
	CONST char *oneWordValue; /* One-word value for key. */
	int *words[1];		/* Multiple integer words for key.
				 * The actual size will be as large
				 * as necessary for this table's
				 * keys. */
	char string[4];		/* String for key.  The actual size
				 * will be as large as needed to hold
				 * the key. */
    } key;			/* MUST BE LAST FIELD IN RECORD!! */
};

typedef int (Blt_ListCompareProc) _ANSI_ARGS_((Blt_ListNode *node1Ptr, 
	Blt_ListNode *node2Ptr));

/*
 * A Blt_List is a doubly chained list structure.
 */
struct Blt_ListStruct {
    struct Blt_ListNodeStruct *headPtr;	/* Pointer to first element in list */
    struct Blt_ListNodeStruct *tailPtr;	/* Pointer to last element in list */
    int nNodes;			/* Number of node currently in the list. */
    int type;			/* Type of keys in list. */
};

EXTERN void Blt_ListInit _ANSI_ARGS_((Blt_List list, int type));
EXTERN void Blt_ListReset _ANSI_ARGS_((Blt_List list));
EXTERN Blt_List Blt_ListCreate _ANSI_ARGS_((int type));
EXTERN void Blt_ListDestroy _ANSI_ARGS_((Blt_List list));
EXTERN Blt_ListNode Blt_ListCreateNode _ANSI_ARGS_((Blt_List list, 
	CONST char *key));
EXTERN void Blt_ListDeleteNode _ANSI_ARGS_((Blt_ListNode node));

EXTERN Blt_ListNode Blt_ListAppend _ANSI_ARGS_((Blt_List list, CONST char *key,
	ClientData clientData));
EXTERN Blt_ListNode Blt_ListPrepend _ANSI_ARGS_((Blt_List list, CONST char *key,
	ClientData clientData));
EXTERN void Blt_ListLinkAfter _ANSI_ARGS_((Blt_List list, Blt_ListNode node, 
	Blt_ListNode afterNode));
EXTERN void Blt_ListLinkBefore _ANSI_ARGS_((Blt_List list, Blt_ListNode node, 
	Blt_ListNode beforeNode));
EXTERN void Blt_ListUnlinkNode _ANSI_ARGS_((Blt_ListNode node));
EXTERN Blt_ListNode Blt_ListGetNode _ANSI_ARGS_((Blt_List list, 
	CONST char *key));
EXTERN void Blt_ListDeleteNodeByKey _ANSI_ARGS_((Blt_List list, 
	CONST char *key));
EXTERN Blt_ListNode Blt_ListGetNthNode _ANSI_ARGS_((Blt_List list,
	int position, int direction));
EXTERN void Blt_ListSort _ANSI_ARGS_((Blt_List list,
	Blt_ListCompareProc * proc));

#define Blt_ListGetLength(list) \
	(((list) == NULL) ? 0 : ((struct Blt_ListStruct *)list)->nNodes)
#define Blt_ListFirstNode(list) \
	(((list) == NULL) ? NULL : ((struct Blt_ListStruct *)list)->headPtr)
#define Blt_ListLastNode(list)	\
	(((list) == NULL) ? NULL : ((struct Blt_ListStruct *)list)->tailPtr)
#define Blt_ListPrevNode(node)	((node)->prevPtr)
#define Blt_ListNextNode(node) 	((node)->nextPtr)
#define Blt_ListGetKey(node)	\
	(((node)->listPtr->type == BLT_STRING_KEYS) \
		 ? (node)->key.string : (node)->key.oneWordValue)
#define Blt_ListGetValue(node)  	((node)->clientData)
#define Blt_ListSetValue(node, value) \
	((node)->clientData = (ClientData)(value))
#define Blt_ListAppendNode(list, node) \
	(Blt_ListLinkBefore((list), (node), (Blt_ListNode)NULL))
#define Blt_ListPrependNode(list, node) \
	(Blt_ListLinkAfter((list), (node), (Blt_ListNode)NULL))

#endif /* _BLT_LIST_H */
