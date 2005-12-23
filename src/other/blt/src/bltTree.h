
/*
 * bltTree.h --
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

#ifndef _BLT_TREE_H
#define _BLT_TREE_H

#include <bltChain.h>
#include <bltHash.h>
#include <bltPool.h>

typedef struct Blt_TreeNodeStruct *Blt_TreeNode;
typedef struct Blt_TreeObjectStruct *Blt_TreeObject;
typedef struct Blt_TreeClientStruct *Blt_Tree;
typedef struct Blt_TreeTraceStruct *Blt_TreeTrace;
typedef struct Blt_TreeValueStruct *Blt_TreeValue;
typedef struct Blt_TreeTagEntryStruct Blt_TreeTagEntry;
typedef struct Blt_TreeTagTableStruct Blt_TreeTagTable;

typedef char *Blt_TreeKey;

#define TREE_PREORDER		(1<<0)
#define TREE_POSTORDER		(1<<1)
#define TREE_INORDER		(1<<2)
#define TREE_BREADTHFIRST	(1<<3)

#define TREE_TRACE_UNSET	(1<<3)
#define TREE_TRACE_WRITE	(1<<4)
#define TREE_TRACE_READ		(1<<5)
#define TREE_TRACE_CREATE	(1<<6)
#define TREE_TRACE_ALL		\
    (TREE_TRACE_UNSET | TREE_TRACE_WRITE | TREE_TRACE_READ | TREE_TRACE_CREATE)
#define TREE_TRACE_MASK		(TREE_TRACE_ALL)

#define TREE_TRACE_FOREIGN_ONLY	(1<<8)
#define TREE_TRACE_ACTIVE	(1<<9)

#define TREE_NOTIFY_CREATE	(1<<0)
#define TREE_NOTIFY_DELETE	(1<<1)
#define TREE_NOTIFY_MOVE	(1<<2)
#define TREE_NOTIFY_SORT	(1<<3)
#define TREE_NOTIFY_RELABEL	(1<<4)
#define TREE_NOTIFY_ALL		\
    (TREE_NOTIFY_CREATE | TREE_NOTIFY_DELETE | TREE_NOTIFY_MOVE | \
	TREE_NOTIFY_SORT | TREE_NOTIFY_RELABEL)
#define TREE_NOTIFY_MASK	(TREE_NOTIFY_ALL)

#define TREE_NOTIFY_WHENIDLE	 (1<<8)
#define TREE_NOTIFY_FOREIGN_ONLY (1<<9)
#define TREE_NOTIFY_ACTIVE	 (1<<10)

typedef struct {
    int type;
    Blt_Tree tree;
    int inode;			/* Node of event */
    Tcl_Interp *interp;
} Blt_TreeNotifyEvent;

typedef struct {
    Blt_TreeNode node;		/* Node being searched. */
    unsigned long nextIndex;	/* Index of next bucket to be
				 * enumerated after present one. */
    Blt_TreeValue nextValue;	/* Next entry to be enumerated in the
				 * the current bucket. */
} Blt_TreeKeySearch;

/*
 * Blt_TreeObject --
 *
 *	Structure providing the internal representation of the tree
 *	object.	A tree is uniquely identified by a combination of 
 *	its name and originating namespace.  Two trees in the same 
 *	interpreter can have the same names but reside in different 
 *	namespaces.
 *
 *	The tree object represents a general-ordered tree of nodes.
 *	Each node may contain a heterogeneous collection of data
 *	values. Each value is identified by a field name and nodes 
 *	do not need to contain the same data fields. Data field
 *	names are saved as reference counted strings and can be 
 *	shared among nodes.
 *
 *	The tree is threaded.  A node contains both a pointer to 
 *	back its parents and another to its siblings.  Therefore
 *	the tree maybe traversed non-recursively.
 * 
 *	A tree object can be shared by several clients.  When a
 *	client wants to use a tree object, it is given a token
 *	that represents the tree.  The tree object uses the tokens
 *	to keep track of its clients.  When all clients have 
 *	released their tokens the tree is automatically destroyed.
 */
struct Blt_TreeObjectStruct {
    Tcl_Interp *interp;		/* Interpreter associated with this tree. */

    char *name;

    Tcl_Namespace *nsPtr;

    Blt_HashEntry *hashPtr;	/* Pointer to this tree object in tree
				 * object hash table. */
    Blt_HashTable *tablePtr;

    Blt_TreeNode root;		/* Root of the entire tree. */

    char *sortNodesCmd;		/* Tcl command to invoke to sort entries */

    Blt_Chain *clients;		/* List of clients using this tree */

    Blt_Pool nodePool;
    Blt_Pool valuePool;

    Blt_HashTable nodeTable;	/* Table of node identifiers. Used to
				 * search for a node pointer given an inode.*/
    unsigned int nextInode;

    unsigned int nNodes;	/* Always counts root node. */

    unsigned int depth;		/* Maximum depth of the tree. */

    unsigned int flags;		/* Internal flags. See definitions
				 * below. */
    unsigned int notifyFlags;	/* Notification flags. See definitions
				 * below. */

};

/*
 * Blt_TreeNodeStruct --
 *
 *	Structure representing a node in a general ordered tree.
 *	Nodes are identified by their index, or inode.  Nodes also
 *	have names, but nodes names are not unique and can be 
 *	changed.  Inodes are valid even if the node is moved.
 *
 *	Each node can contain a list of data fields.  Fields are
 *	name-value pairs.  The values are represented by Tcl_Objs.
 *	
 */
struct Blt_TreeNodeStruct {
    Blt_TreeNode parent;	/* Parent node. If NULL, then this is
				   the root node. */
    Blt_TreeNode next;		/* Next sibling node. */
    Blt_TreeNode prev;		/* Previous sibling node. */
    Blt_TreeNode first;		/* First child node. */
    Blt_TreeNode last;		/* Last child node. */

    Blt_TreeKey label;		/* Node label (doesn't have to be
				 * unique). */

    Blt_TreeObject treeObject;

    Blt_TreeValue values;	/* Depending upon the number of values
				 * stored, this is either a chain or
				 * hash table of Blt_TreeValue
				 * structures.  (Note: if logSize is 
				 * 0, then this is a list).  Each
				 * value contains key/value data
				 * pair.  The data is a Tcl_Obj. */

    unsigned short nValues;	/* # of values for this node. */

    unsigned short logSize;	/* Size of hash table indicated as a
				 * power of 2 (e.g. if logSize=3, then
				 * table size is 8). If 0, this
				 * indicates that the node's values
				 * are stored as a list. */

    unsigned int nChildren;	/* # of children for this node. */
    unsigned int inode;		/* Serial number of the node. */

    unsigned short depth;	/* The depth of this node in the tree. */

    unsigned short flags;
};

struct Blt_TreeTagEntryStruct {
    char *tagName;
    Blt_HashEntry *hashPtr;
    Blt_HashTable nodeTable;
};

struct Blt_TreeTagTableStruct {
    Blt_HashTable tagTable;
    int refCount;
};

/*
 * Blt_TreeClientStruct --
 *
 *	A tree can be shared by several clients.  Each client allocates
 *	this structure which acts as a ticket for using the tree.  Clients
 *	can designate notifier routines that are automatically invoked
 *	by the tree object whenever the tree is changed is specific
 *	ways by other clients.
 */

struct Blt_TreeClientStruct {
    unsigned int magic;		/* Magic value indicating whether a
				 * generic pointer is really a tree
				 * token or not. */
    Blt_ChainLink *linkPtr;	/* Pointer into the server's chain of
				 * clients. */
    Blt_TreeObject treeObject;	/* Pointer to the structure containing
				 * the master information about the
				 * tree used by the client.  If NULL,
				 * this indicates that the tree has
				 * been destroyed (but as of yet, this
				 * client hasn't recognized it). */

    Blt_Chain *events;		/* Chain of node event handlers. */
    Blt_Chain *traces;		/* Chain of data field callbacks. */
    Blt_TreeNode root;		/* Designated root for this client */
    Blt_TreeTagTable *tagTablePtr;
};


typedef int (Blt_TreeNotifyEventProc) _ANSI_ARGS_((ClientData clientData, 
	Blt_TreeNotifyEvent *eventPtr));

typedef int (Blt_TreeTraceProc) _ANSI_ARGS_((ClientData clientData, 
	Tcl_Interp *interp, Blt_TreeNode node, Blt_TreeKey key, 
	unsigned int flags));

typedef int (Blt_TreeEnumProc) _ANSI_ARGS_((Blt_TreeNode node, Blt_TreeKey key,
	Tcl_Obj *valuePtr));

typedef int (Blt_TreeCompareNodesProc) _ANSI_ARGS_((Blt_TreeNode *n1Ptr, 
	Blt_TreeNode *n2Ptr));

typedef int (Blt_TreeApplyProc) _ANSI_ARGS_((Blt_TreeNode node, 
	ClientData clientData, int order));

struct Blt_TreeTraceStruct {
    ClientData clientData;
    Blt_TreeKey key;
    Blt_TreeNode node;
    unsigned int mask;
    Blt_TreeTraceProc *proc;
};

/*
 * Structure definition for information used to keep track of searches
 * through hash tables:p
 */
struct Blt_TreeKeySearchStruct {
    Blt_TreeNode node;		/* Table being searched. */
    unsigned long nextIndex;	/* Index of next bucket to be
				 * enumerated after present one. */
    Blt_TreeValue nextValue;	/* Next entry to be enumerated in the
				 * the current bucket. */
};

EXTERN Blt_TreeKey Blt_TreeGetKey _ANSI_ARGS_((CONST char *string));

EXTERN Blt_TreeNode Blt_TreeCreateNode _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode parent, CONST char *name, int position)); 
EXTERN Blt_TreeNode Blt_TreeCreateNodeWithId _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode parent, CONST char *name, int position, int inode)); 

EXTERN int Blt_TreeDeleteNode _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node));

EXTERN int Blt_TreeMoveNode _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	Blt_TreeNode parent, Blt_TreeNode before));

EXTERN Blt_TreeNode Blt_TreeGetNode _ANSI_ARGS_((Blt_Tree tree, 
	unsigned int inode));

EXTERN Blt_TreeNode Blt_TreeFindChild _ANSI_ARGS_((Blt_TreeNode parent, 
	CONST char *name));

EXTERN Blt_TreeNode Blt_TreeFirstChild _ANSI_ARGS_((Blt_TreeNode parent));

EXTERN Blt_TreeNode Blt_TreeNextSibling _ANSI_ARGS_((Blt_TreeNode node));

EXTERN Blt_TreeNode Blt_TreeLastChild _ANSI_ARGS_((Blt_TreeNode parent));

EXTERN Blt_TreeNode Blt_TreePrevSibling _ANSI_ARGS_((Blt_TreeNode node));

EXTERN Blt_TreeNode Blt_TreeNextNode _ANSI_ARGS_((Blt_TreeNode root, 
	Blt_TreeNode node));

EXTERN Blt_TreeNode Blt_TreePrevNode _ANSI_ARGS_((Blt_TreeNode root,
	Blt_TreeNode node));

EXTERN Blt_TreeNode Blt_TreeChangeRoot _ANSI_ARGS_((Blt_Tree tree,
	Blt_TreeNode node));

EXTERN Blt_TreeNode Blt_TreeEndNode _ANSI_ARGS_((Blt_TreeNode node,
	unsigned int nodeFlags));

EXTERN int Blt_TreeIsBefore _ANSI_ARGS_((Blt_TreeNode node1, 
	Blt_TreeNode node2));

EXTERN int Blt_TreeIsAncestor _ANSI_ARGS_((Blt_TreeNode node1, 
	Blt_TreeNode node2));

EXTERN int Blt_TreePrivateValue _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree,
	Blt_TreeNode node, Blt_TreeKey key));

EXTERN int Blt_TreePublicValue _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree,
	Blt_TreeNode node, Blt_TreeKey key));

EXTERN int Blt_TreeGetValue _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree, 
	Blt_TreeNode node, CONST char *string, Tcl_Obj **valuePtr));

EXTERN int Blt_TreeValueExists _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	CONST char *string));

EXTERN int Blt_TreeSetValue _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree, 
	Blt_TreeNode node, CONST char *string, Tcl_Obj *valuePtr));

EXTERN int Blt_TreeUnsetValue _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree, 
	Blt_TreeNode node, CONST char *string));

EXTERN int Blt_TreeGetArrayValue _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, CONST char *arrayName, 
	CONST char *elemName, Tcl_Obj **valueObjPtrPtr));

EXTERN int Blt_TreeSetArrayValue _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, CONST char *arrayName, 
	CONST char *elemName, Tcl_Obj *valueObjPtr));

EXTERN int Blt_TreeUnsetArrayValue _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, CONST char *arrayName, 
	CONST char *elemName));

EXTERN int Blt_TreeArrayValueExists _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode node, CONST char *arrayName, CONST char *elemName));

EXTERN int Blt_TreeArrayNames _ANSI_ARGS_((Tcl_Interp *interp, Blt_Tree tree, 
	Blt_TreeNode node, CONST char *arrayName, Tcl_Obj *listObjPtr));

EXTERN int Blt_TreeGetValueByKey _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, Blt_TreeKey key, 
	Tcl_Obj **valuePtr));

EXTERN int Blt_TreeSetValueByKey _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, Blt_TreeKey key, Tcl_Obj *valuePtr));

EXTERN int Blt_TreeUnsetValueByKey _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_Tree tree, Blt_TreeNode node, Blt_TreeKey key));

EXTERN int Blt_TreeValueExistsByKey _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode node, Blt_TreeKey key));

EXTERN Blt_TreeKey Blt_TreeFirstKey _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode node, Blt_TreeKeySearch *cursorPtr));

EXTERN Blt_TreeKey Blt_TreeNextKey _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeKeySearch *cursorPtr));

EXTERN int Blt_TreeApply _ANSI_ARGS_((Blt_TreeNode root, 
	Blt_TreeApplyProc *proc, ClientData clientData));

EXTERN int Blt_TreeApplyDFS _ANSI_ARGS_((Blt_TreeNode root, 
	Blt_TreeApplyProc *proc, ClientData clientData, int order));

EXTERN int Blt_TreeApplyBFS _ANSI_ARGS_((Blt_TreeNode root, 
	Blt_TreeApplyProc *proc, ClientData clientData));

EXTERN int Blt_TreeSortNode _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	Blt_TreeCompareNodesProc *proc));

EXTERN int Blt_TreeCreate _ANSI_ARGS_((Tcl_Interp *interp, CONST char *name,
	Blt_Tree *treePtr));

EXTERN int Blt_TreeExists _ANSI_ARGS_((Tcl_Interp *interp, CONST char *name));

EXTERN int Blt_TreeGetToken _ANSI_ARGS_((Tcl_Interp *interp, CONST char *name, 
	Blt_Tree *treePtr));

EXTERN void Blt_TreeReleaseToken _ANSI_ARGS_((Blt_Tree tree));

EXTERN int Blt_TreeSize _ANSI_ARGS_((Blt_TreeNode node));

EXTERN Blt_TreeTrace Blt_TreeCreateTrace _ANSI_ARGS_((Blt_Tree tree, 
	Blt_TreeNode node, CONST char *keyPattern, CONST char *tagName,
	unsigned int mask, Blt_TreeTraceProc *proc, ClientData clientData));

EXTERN void Blt_TreeDeleteTrace _ANSI_ARGS_((Blt_TreeTrace token));

EXTERN void Blt_TreeCreateEventHandler _ANSI_ARGS_((Blt_Tree tree, 
	unsigned int mask, Blt_TreeNotifyEventProc *proc, 
	ClientData clientData));

EXTERN void Blt_TreeDeleteEventHandler _ANSI_ARGS_((Blt_Tree tree, 
	unsigned int mask, Blt_TreeNotifyEventProc *proc, 
	ClientData clientData));

EXTERN void Blt_TreeRelabelNode _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	CONST char *string));
EXTERN void Blt_TreeRelabelNode2 _ANSI_ARGS_((Blt_TreeNode node, 
	CONST char *string));
EXTERN char *Blt_TreeNodePath _ANSI_ARGS_((Blt_TreeNode node, 
	Tcl_DString *resultPtr));	
EXTERN int Blt_TreeNodePosition _ANSI_ARGS_((Blt_TreeNode node));

EXTERN void Blt_TreeClearTags _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node));
EXTERN int Blt_TreeHasTag _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	CONST char *tagName));
EXTERN void Blt_TreeAddTag _ANSI_ARGS_((Blt_Tree tree, Blt_TreeNode node, 
	CONST char *tagName));
EXTERN void Blt_TreeForgetTag _ANSI_ARGS_((Blt_Tree tree, CONST char *tagName));
EXTERN Blt_HashTable *Blt_TreeTagHashTable _ANSI_ARGS_((Blt_Tree tree, 
	CONST char *tagName));
EXTERN int Blt_TreeTagTableIsShared _ANSI_ARGS_((Blt_Tree tree));
EXTERN int Blt_TreeShareTagTable _ANSI_ARGS_((Blt_Tree src, Blt_Tree target));
EXTERN Blt_HashEntry *Blt_TreeFirstTag _ANSI_ARGS_((Blt_Tree tree, 
	Blt_HashSearch *searchPtr));

#define Blt_TreeName(token)	((token)->treeObject->name)
#define Blt_TreeRootNode(token)	((token)->root)
#define Blt_TreeChangeRoot(token, node) ((token)->root = (node))

#define Blt_TreeNodeDepth(token, node)	((node)->depth - (token)->root->depth)
#define Blt_TreeNodeLabel(node)	 ((node)->label)
#define Blt_TreeNodeId(node)	 ((node)->inode)
#define Blt_TreeNodeParent(node) ((node)->parent)
#define Blt_TreeNodeDegree(node) ((node)->nChildren)

#define Blt_TreeIsLeaf(node)     ((node)->nChildren == 0)
#define Blt_TreeNextNodeId(token)     ((token)->treeObject->nextInode)

#define Blt_TreeFirstChild(node) ((node)->first)
#define Blt_TreeLastChild(node) ((node)->last)
#define Blt_TreeNextSibling(node) (((node) == NULL) ? NULL : (node)->next)
#define Blt_TreePrevSibling(node) (((node) == NULL) ? NULL : (node)->prev)

#endif /* _BLT_TREE_H */

