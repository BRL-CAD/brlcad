
/*
 *
 * bltTreeCmd.c --
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

/*
  tree create t0 t1 t2
  tree names
  t0 destroy
     -or-
  tree destroy t0
  tree copy tree@node tree@node -recurse -tags

  tree move node after|before|into t2@node

  $t apply -recurse $root command arg arg			

  $t attach treename				

  $t children $n
  t0 copy node1 node2 node3 node4 node5 destName 
  $t delete $n...				
  $t depth $n
  $t dump $root
  $t dumpfile $root fileName
  $t dup $t2		
  $t find $root -name pat -name pattern
  $t firstchild $n
  $t get $n $key
  $t get $n $key(abc)
  $t index $n
  $t insert $parent $switches?
  $t isancestor $n1 $n2
  $t isbefore $n1 $n2
  $t isleaf $n
  $t lastchild $n
  $t move $n1 after|before|into $n2
  $t next $n
  $t nextsibling $n
  $t path $n1 $n2 $n3...
  $t parent $n
  $t previous $n
  $t prevsibling $n
  $t restore $root data -overwrite
  $t root ?$n?

  $t set $n $key $value ?$key $value?
  $t size $n
  $t slink $n $t2@$node				???
  $t sort -recurse $root		

  $t tag delete tag1 tag2 tag3...
  $t tag names
  $t tag nodes $tag
  $t tag set $n tag1 tag2 tag3...
  $t tag unset $n tag1 tag2 tag3...

  $t trace create $n $key how command		
  $t trace delete id1 id2 id3...
  $t trace names
  $t trace info $id

  $t unset $n key1 key2 key3...
  
  $t notify create -oncreate -ondelete -onmove command 
  $t notify create -oncreate -ondelete -onmove -onsort command arg arg arg 
  $t notify delete id1 id2 id3
  $t notify names
  $t notify info id

  for { set n [$t firstchild $node] } { $n >= 0 } { 
        set n [$t nextsibling $n] } {
  }
  foreach n [$t children $node] { 
	  
  }
  set n [$t next $node]
  set n [$t previous $node]

*/

#include <bltInt.h>

#ifndef NO_TREE

#include <bltTree.h>
#include <bltHash.h>
#include <bltList.h>
#include "bltSwitch.h"
#include <ctype.h>

#define TREE_THREAD_KEY "BLT Tree Command Data"
#define TREE_MAGIC ((unsigned int) 0x46170277)

enum TagTypes { TAG_TYPE_NONE, TAG_TYPE_ALL, TAG_TYPE_TAG };

typedef struct {
    Blt_HashTable treeTable;	/* Hash table of trees keyed by address. */
    Tcl_Interp *interp;
} TreeCmdInterpData;

typedef struct {
    Tcl_Interp *interp;
    Tcl_Command cmdToken;	/* Token for tree's Tcl command. */
    Blt_Tree tree;		/* Token holding internal tree. */

    Blt_HashEntry *hashPtr;
    Blt_HashTable *tablePtr;

    TreeCmdInterpData *dataPtr;	/*  */

    int traceCounter;		/* Used to generate trace id strings.  */
    Blt_HashTable traceTable;	/* Table of active traces. Maps trace ids
				 * back to their TraceInfo records. */

    int notifyCounter;		/* Used to generate notify id strings. */
    Blt_HashTable notifyTable;	/* Table of event handlers. Maps notify ids
				 * back to their NotifyInfo records. */
} TreeCmd;

typedef struct {
    TreeCmd *cmdPtr;
    Blt_TreeNode node;

    Blt_TreeTrace traceToken;

    char *withTag;		/* If non-NULL, the event or trace was
				 * specified with this tag.  This
				 * value is saved for informational
				 * purposes.  The tree's trace
				 * matching routines do the real
				 * checking, not the client's
				 * callback.  */

    char command[1];		/* Command prefix for the trace or notify
				 * Tcl callback.  Extra arguments will be
				 * appended to the end. Extra space will
				 * be allocated for the length of the string
				 */
} TraceInfo;

typedef struct {
    TreeCmd *cmdPtr;
    int mask;
    Tcl_Obj **objv;
    int objc;
    Blt_TreeNode node;		/* Node affected by event. */
    Blt_TreeTrace notifyToken;
} NotifyInfo;


typedef struct {
    int mask;
} NotifyData;

static Blt_SwitchSpec notifySwitches[] = 
{
    {BLT_SWITCH_FLAG, "-create", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_CREATE},
    {BLT_SWITCH_FLAG, "-delete", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_DELETE},
    {BLT_SWITCH_FLAG, "-move", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_MOVE},
    {BLT_SWITCH_FLAG, "-sort", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_SORT},
    {BLT_SWITCH_FLAG, "-relabel", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_RELABEL},
    {BLT_SWITCH_FLAG, "-allevents", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_ALL},
    {BLT_SWITCH_FLAG, "-whenidle", Blt_Offset(NotifyData, mask), 0, 0, 
	TREE_NOTIFY_WHENIDLE},
    {BLT_SWITCH_END, NULL, 0, 0}
};

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
    char *label;
    int insertPos;
    int inode;
    char **tags;
    char **dataPairs;
    Blt_TreeNode parent;
} InsertData;

static Blt_SwitchSpec insertSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-after", Blt_Offset(InsertData, insertPos), 0, 
	&afterSwitch},
    {BLT_SWITCH_INT_NONNEGATIVE, "-at", Blt_Offset(InsertData, insertPos), 0},
    {BLT_SWITCH_CUSTOM, "-before", Blt_Offset(InsertData, insertPos), 0,
	&beforeSwitch},
    {BLT_SWITCH_LIST, "-data", Blt_Offset(InsertData, dataPairs), 0},
    {BLT_SWITCH_STRING, "-label", Blt_Offset(InsertData, label), 0},
    {BLT_SWITCH_INT_NONNEGATIVE, "-node", Blt_Offset(InsertData, inode), 0},
    {BLT_SWITCH_LIST, "-tags", Blt_Offset(InsertData, tags), 0},
    {BLT_SWITCH_END, NULL, 0, 0}
};

#define PATTERN_EXACT	(1)
#define PATTERN_GLOB	(2)
#define PATTERN_MASK	(0x3)
#define PATTERN_NONE	(0)
#define PATTERN_REGEXP	(3)
#define MATCH_INVERT		(1<<8)
#define MATCH_LEAFONLY		(1<<4)
#define MATCH_NOCASE		(1<<5)
#define MATCH_PATHNAME		(1<<6)

typedef struct {
    TreeCmd *cmdPtr;		/* Tree to examine. */
    Tcl_Obj *listObjPtr;	/* List to accumulate the indices of 
				 * matching nodes. */
    Tcl_Obj **objv;		/* Command converted into an array of 
				 * Tcl_Obj's. */
    int objc;			/* Number of Tcl_Objs in above array. */

    int nMatches;		/* Current number of matches. */

    unsigned int flags;		/* See flags definitions above. */

    /* Integer options. */
    int maxMatches;		/* If > 0, stop after this many matches. */
    int maxDepth;		/* If > 0, don't descend more than this
				 * many levels. */
    int order;			/* Order of search: Can be either
				 * TREE_PREORDER, TREE_POSTORDER,
				 * TREE_INORDER, TREE_BREADTHFIRST. */
    /* String options. */
    Blt_List patternList;	/* List of patterns to compare with labels
				 * or values.  */
    char *addTag;		/* If non-NULL, tag added to selected nodes. */

    char **command;		/* Command split into a Tcl list. */

    Blt_List keyList;		/* List of key name patterns. */
    char *withTag;

} FindData;

static Blt_SwitchParseProc StringToOrder;
static Blt_SwitchCustom orderSwitch =
{
    StringToOrder, (Blt_SwitchFreeProc *)NULL, (ClientData)0,
};

static Blt_SwitchParseProc StringToPattern;
static Blt_SwitchFreeProc FreePatterns;

static Blt_SwitchCustom regexpSwitch =
{
    StringToPattern, FreePatterns, (ClientData)PATTERN_REGEXP,
};
static Blt_SwitchCustom globSwitch =
{
    StringToPattern, FreePatterns, (ClientData)PATTERN_GLOB,
};
static Blt_SwitchCustom exactSwitch =
{
    StringToPattern, FreePatterns, (ClientData)PATTERN_EXACT,
};


static Blt_SwitchSpec findSwitches[] = 
{
    {BLT_SWITCH_STRING, "-addtag", Blt_Offset(FindData, addTag), 0},
    {BLT_SWITCH_INT_NONNEGATIVE, "-count", Blt_Offset(FindData, maxMatches), 0},
    {BLT_SWITCH_INT_NONNEGATIVE, "-depth", Blt_Offset(FindData, maxDepth), 0},
    {BLT_SWITCH_CUSTOM, "-exact", Blt_Offset(FindData, patternList), 0,
        &exactSwitch},
    {BLT_SWITCH_LIST, "-exec", Blt_Offset(FindData, command), 0},
    {BLT_SWITCH_CUSTOM, "-glob", Blt_Offset(FindData, patternList), 0, 
	&globSwitch},
    {BLT_SWITCH_FLAG, "-invert", Blt_Offset(FindData, flags), 0, 0, 
	MATCH_INVERT},
    {BLT_SWITCH_CUSTOM, "-key", Blt_Offset(FindData, keyList), 0, &exactSwitch},
    {BLT_SWITCH_CUSTOM, "-keyexact", Blt_Offset(FindData, keyList), 0, 
	&exactSwitch},
    {BLT_SWITCH_CUSTOM, "-keyglob", Blt_Offset(FindData, keyList), 0, 
	&globSwitch},
    {BLT_SWITCH_CUSTOM, "-keyregexp", Blt_Offset(FindData, keyList), 0, 
	&regexpSwitch},
    {BLT_SWITCH_FLAG, "-leafonly", Blt_Offset(FindData, flags), 0, 0, 
	MATCH_LEAFONLY},
    {BLT_SWITCH_FLAG, "-nocase", Blt_Offset(FindData, flags), 0, 0, 
	MATCH_NOCASE},
    {BLT_SWITCH_CUSTOM, "-order", Blt_Offset(FindData, order), 0, &orderSwitch},
    {BLT_SWITCH_FLAG, "-path", Blt_Offset(FindData, flags), 0, 0, 
	MATCH_PATHNAME},
    {BLT_SWITCH_CUSTOM, "-regexp", Blt_Offset(FindData, patternList), 0, 
	&regexpSwitch},
    {BLT_SWITCH_STRING, "-tag", Blt_Offset(FindData, withTag), 0},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static Blt_SwitchParseProc StringToNode;
static Blt_SwitchCustom nodeSwitch =
{
    StringToNode, (Blt_SwitchFreeProc *)NULL, (ClientData)0,
};

typedef struct {
    TreeCmd *cmdPtr;		/* Tree to move nodes. */
    Blt_TreeNode node;
    int movePos;
} MoveData;

static Blt_SwitchSpec moveSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-after", Blt_Offset(MoveData, node), 0, &nodeSwitch},
    {BLT_SWITCH_INT_NONNEGATIVE, "-at", Blt_Offset(MoveData, movePos), 0},
    {BLT_SWITCH_CUSTOM, "-before", Blt_Offset(MoveData, node), 0, &nodeSwitch},
    {BLT_SWITCH_END, NULL, 0, 0}
};

typedef struct {
    Blt_TreeNode srcNode, destNode;
    Blt_Tree srcTree, destTree;
    TreeCmd *srcPtr, *destPtr;
    unsigned int flags;
    char *label;
} CopyData;

#define COPY_RECURSE	(1<<0)
#define COPY_TAGS	(1<<1)
#define COPY_OVERWRITE	(1<<2)

static Blt_SwitchSpec copySwitches[] = 
{
    {BLT_SWITCH_STRING, "-label", Blt_Offset(CopyData, label), 0},
    {BLT_SWITCH_FLAG, "-recurse", Blt_Offset(CopyData, flags), 0, 0, 
	COPY_RECURSE},
    {BLT_SWITCH_FLAG, "-tags", Blt_Offset(CopyData, flags), 0, 0, 
	COPY_TAGS},
    {BLT_SWITCH_FLAG, "-overwrite", Blt_Offset(CopyData, flags), 0, 0, 
	COPY_OVERWRITE},
    {BLT_SWITCH_END, NULL, 0, 0}
};

typedef struct {
    TreeCmd *cmdPtr;		/* Tree to examine. */
    Tcl_Obj **preObjv;		/* Command converted into an array of 
				 * Tcl_Obj's. */
    int preObjc;		/* Number of Tcl_Objs in above array. */

    Tcl_Obj **postObjv;		/* Command converted into an array of 
				 * Tcl_Obj's. */
    int postObjc;		/* Number of Tcl_Objs in above array. */

    unsigned int flags;		/* See flags definitions above. */

    int maxDepth;		/* If > 0, don't descend more than this
				 * many levels. */
    /* String options. */
    Blt_List patternList;	/* List of label or value patterns. */
    char **preCmd;		/* Pre-command split into a Tcl list. */
    char **postCmd;		/* Post-command split into a Tcl list. */

    Blt_List keyList;		/* List of key-name patterns. */
    char *withTag;
} ApplyData;

static Blt_SwitchSpec applySwitches[] = 
{
    {BLT_SWITCH_LIST, "-precommand", Blt_Offset(ApplyData, preCmd), 0},
    {BLT_SWITCH_LIST, "-postcommand", Blt_Offset(ApplyData, postCmd), 0},
    {BLT_SWITCH_INT_NONNEGATIVE, "-depth", Blt_Offset(ApplyData, maxDepth), 0},
    {BLT_SWITCH_CUSTOM, "-exact", Blt_Offset(ApplyData, patternList), 0,
	&exactSwitch},
    {BLT_SWITCH_CUSTOM, "-glob", Blt_Offset(ApplyData, patternList), 0, 
	&globSwitch},
    {BLT_SWITCH_FLAG, "-invert", Blt_Offset(ApplyData, flags), 0, 0, 
	MATCH_INVERT},
    {BLT_SWITCH_CUSTOM, "-key", Blt_Offset(ApplyData, keyList), 0, 
	&exactSwitch},
    {BLT_SWITCH_CUSTOM, "-keyexact", Blt_Offset(ApplyData, keyList), 0, 
	&exactSwitch},
    {BLT_SWITCH_CUSTOM, "-keyglob", Blt_Offset(ApplyData, keyList), 0, 
	&globSwitch},
    {BLT_SWITCH_CUSTOM, "-keyregexp", Blt_Offset(ApplyData, keyList), 0, 
	&regexpSwitch},
    {BLT_SWITCH_FLAG, "-leafonly", Blt_Offset(ApplyData, flags), 0, 0, 
	MATCH_LEAFONLY},
    {BLT_SWITCH_FLAG, "-nocase", Blt_Offset(ApplyData, flags), 0, 0, 
	MATCH_NOCASE},
    {BLT_SWITCH_FLAG, "-path", Blt_Offset(ApplyData, flags), 0, 0, 
	MATCH_PATHNAME},
    {BLT_SWITCH_CUSTOM, "-regexp", Blt_Offset(ApplyData, patternList), 0,
	&regexpSwitch},
    {BLT_SWITCH_STRING, "-tag", Blt_Offset(ApplyData, withTag), 0},
    {BLT_SWITCH_END, NULL, 0, 0}
};

typedef struct {
    unsigned int flags;
    Blt_HashTable idTable;
    Blt_TreeNode root;
} RestoreData;

#define RESTORE_NO_TAGS		(1<<0)
#define RESTORE_OVERWRITE	(1<<1)

static Blt_SwitchSpec restoreSwitches[] = 
{
    {BLT_SWITCH_FLAG, "-notags", Blt_Offset(RestoreData, flags), 0, 0, 
	RESTORE_NO_TAGS},
    {BLT_SWITCH_FLAG, "-overwrite", Blt_Offset(RestoreData, flags), 0, 0, 
	RESTORE_OVERWRITE},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static Blt_SwitchParseProc StringToFormat;
static Blt_SwitchCustom formatSwitch =
{
    StringToFormat, (Blt_SwitchFreeProc *)NULL, (ClientData)0,
};

typedef struct {
    int sort;			/* If non-zero, sort the nodes.  */
    int withParent;		/* If non-zero, add the parent node id 
				 * to the output of the command.*/
    int withId;			/* If non-zero, echo the node id in the
				 * output of the command. */
} PositionData;

#define POSITION_SORTED		(1<<0)

static Blt_SwitchSpec positionSwitches[] = 
{
    {BLT_SWITCH_FLAG, "-sort", Blt_Offset(PositionData, sort), 0, 0,
       POSITION_SORTED},
    {BLT_SWITCH_CUSTOM, "-format", 0, 0, &formatSwitch},
    {BLT_SWITCH_END, NULL, 0, 0}
};


static Tcl_InterpDeleteProc TreeInterpDeleteProc;
static Blt_TreeApplyProc MatchNodeProc, SortApplyProc;
static Blt_TreeApplyProc ApplyNodeProc;
static Blt_TreeTraceProc TreeTraceProc;
static Tcl_CmdDeleteProc TreeInstDeleteProc;
static Blt_TreeCompareNodesProc CompareNodes;

static Tcl_ObjCmdProc TreeObjCmd;
static Tcl_ObjCmdProc CompareDictionaryCmd;
static Tcl_ObjCmdProc ExitCmd;
static Blt_TreeNotifyEventProc TreeEventProc;

static int GetNode _ANSI_ARGS_((TreeCmd *cmdPtr, Tcl_Obj *objPtr, 
	Blt_TreeNode *nodePtr));

static int nLines;



/*
 *----------------------------------------------------------------------
 *
 * StringToChild --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToChild(
    ClientData clientData,	/* Flag indicating if the node is
				 * considered before or after the
				 * insertion position. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    char *switchName,		/* Not used. */
    char *string,		/* String representation */
    char *record,		/* Structure record */
    int offset)			/* Offset to field in structure */
{
    InsertData *dataPtr = (InsertData *)record;
    Blt_TreeNode node;
    
    node = Blt_TreeFindChild(dataPtr->parent, string);
    if (node == NULL) {
	Tcl_AppendResult(interp, "can't find a child named \"", string, 
		 "\" in \"", Blt_TreeNodeLabel(dataPtr->parent), "\"",
		 (char *)NULL);	 
	return TCL_ERROR;
    }			  
    dataPtr->insertPos = Blt_TreeNodeDegree(node);
    if (clientData == INSERT_AFTER) {
	dataPtr->insertPos++;
    } 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToNode --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToNode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    char *switchName,		/* Not used. */
    char *string,		/* String representation */
    char *record,		/* Structure record */
    int offset)			/* Offset to field in structure */
{
    MoveData *dataPtr = (MoveData *)record;
    Blt_TreeNode node;
    Tcl_Obj *objPtr;
    TreeCmd *cmdPtr = dataPtr->cmdPtr;
    int result;

    objPtr = Tcl_NewStringObj(string, -1);
    result = GetNode(cmdPtr, objPtr, &node);
    Tcl_DecrRefCount(objPtr);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    dataPtr->node = node;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * StringToOrder --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToOrder(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    char *switchName,		/* Not used. */
    char *string,		/* String representation */
    char *record,		/* Structure record */
    int offset)			/* Offset to field in structure */
{
    int *orderPtr = (int *)(record + offset);
    char c;

    c = string[0];
    if ((c == 'b') && (strcmp(string, "breadthfirst") == 0)) {
	*orderPtr = TREE_BREADTHFIRST;
    } else if ((c == 'i') && (strcmp(string, "inorder") == 0)) {
	*orderPtr = TREE_INORDER;
    } else if ((c == 'p') && (strcmp(string, "preorder") == 0)) {
	*orderPtr = TREE_PREORDER;
    } else if ((c == 'p') && (strcmp(string, "postorder") == 0)) {
	*orderPtr = TREE_POSTORDER;
    } else {
	Tcl_AppendResult(interp, "bad order \"", string, 
		 "\": should be breadthfirst, inorder, preorder, or postorder",
		 (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToPattern --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToPattern(
    ClientData clientData,	/* Flag indicating type of pattern. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    char *switchName,		/* Not used. */
    char *string,		/* String representation */
    char *record,		/* Structure record */
    int offset)			/* Offset to field in structure */
{
    Blt_List *listPtr = (Blt_List *)(record + offset);

    if (*listPtr == NULL) {
	*listPtr = Blt_ListCreate(BLT_STRING_KEYS);
    }
    Blt_ListAppend(*listPtr, string, clientData);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreePatterns --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreePatterns(char *object)
{
    Blt_List list = (Blt_List)object;

    if (list != NULL) {
	Blt_ListDestroy(list);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * StringToFormat --
 *
 *	Convert a string represent a node number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToFormat(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    char *switchName,		/* Not used. */
    char *string,		/* String representation */
    char *record,		/* Structure record */
    int offset)			/* Offset to field in structure */
{
    PositionData *dataPtr = (PositionData *)record;

    if (strcmp(string, "position") == 0) {
	dataPtr->withParent = FALSE;
	dataPtr->withId = FALSE;
    } else if (strcmp(string, "id+position") == 0) {
	dataPtr->withParent = FALSE;
	dataPtr->withId = TRUE;
    } else if (strcmp(string, "parent-at-position") == 0) {
	dataPtr->withParent = TRUE;
	dataPtr->withId = FALSE;
    } else if (strcmp(string, "id+parent-at-position") == 0) {
	dataPtr->withParent = TRUE;
	dataPtr->withId  = TRUE;
    } else {
	Tcl_AppendResult(interp, "bad format \"", string, 
 "\": should be position, parent-at-position, id+position, or id+parent-at-position",
		 (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTreeCmdInterpData --
 *
 *---------------------------------------------------------------------- 
 */
static TreeCmdInterpData *
GetTreeCmdInterpData(Tcl_Interp *interp)
{
    TreeCmdInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TreeCmdInterpData *)
	Tcl_GetAssocData(interp, TREE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TreeCmdInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TREE_THREAD_KEY, TreeInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&dataPtr->treeTable, BLT_ONE_WORD_KEYS);
    }
    return dataPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTreeCmd --
 *
 *	Find the tree command associated with the Tcl command "string".
 *	
 *	We have to do multiple lookups to get this right.  
 *
 *	The first step is to generate a canonical command name.  If an
 *	unqualified command name (i.e. no namespace qualifier) is
 *	given, we should search first the current namespace and then
 *	the global one.  Most Tcl commands (like Tcl_GetCmdInfo) look
 *	only at the global namespace.
 *
 *	Next check if the string is 
 *		a) a Tcl command and 
 *		b) really is a command for a tree object.  
 *	Tcl_GetCommandInfo will get us the objClientData field that 
 *	should be a cmdPtr.  We can verify that by searching our hashtable 
 *	of cmdPtr addresses.
 *
 * Results:
 *	A pointer to the tree command.  If no associated tree command
 *	can be found, NULL is returned.  It's up to the calling routines
 *	to generate an error message.
 *
 *---------------------------------------------------------------------- 
 */
static TreeCmd *
GetTreeCmd(
    TreeCmdInterpData *dataPtr, 
    Tcl_Interp *interp, 
    CONST char *string)
{
    CONST char *name;
    Tcl_Namespace *nsPtr;
    Tcl_CmdInfo cmdInfo;
    Blt_HashEntry *hPtr;
    Tcl_DString dString;
    char *treeName;
    int result;

    /* Put apart the tree name and put is back together in a standard
     * format. */
    if (Blt_ParseQualifiedName(interp, string, &nsPtr, &name) != TCL_OK) {
	return NULL;		/* No such parent namespace. */
    }
    if (nsPtr == NULL) {
	nsPtr = Tcl_GetCurrentNamespace(interp);
    }
    /* Rebuild the fully qualified name. */
    treeName = Blt_GetQualifiedName(nsPtr, name, &dString);
    result = Tcl_GetCommandInfo(interp, treeName, &cmdInfo);
    Tcl_DStringFree(&dString);

    if (!result) {
	return NULL;
    }
    hPtr = Blt_FindHashEntry(&dataPtr->treeTable, 
			     (char *)(cmdInfo.objClientData));
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

static Blt_TreeNode 
ParseModifiers(
    Tcl_Interp *interp,
    Blt_Tree tree,
    Blt_TreeNode node,
    char *modifiers)
{
    char *p, *np;

    p = modifiers;
    do {
	p += 2;			/* Skip the initial "->" */
	np = strstr(p, "->");
	if (np != NULL) {
	    *np = '\0';
	}
	if ((*p == 'p') && (strcmp(p, "parent") == 0)) {
	    node = Blt_TreeNodeParent(node);
	} else if ((*p == 'f') && (strcmp(p, "firstchild") == 0)) {
	    node = Blt_TreeFirstChild(node);
	} else if ((*p == 'l') && (strcmp(p, "lastchild") == 0)) {
	    node = Blt_TreeLastChild(node);
	} else if ((*p == 'n') && (strcmp(p, "next") == 0)) {
	    node = Blt_TreeNextNode(Blt_TreeRootNode(tree), node);
	} else if ((*p == 'n') && (strcmp(p, "nextsibling") == 0)) {
	    node = Blt_TreeNextSibling(node);
	} else if ((*p == 'p') && (strcmp(p, "previous") == 0)) {
	    node = Blt_TreePrevNode(Blt_TreeRootNode(tree), node);
	} else if ((*p == 'p') && (strcmp(p, "prevsibling") == 0)) {
	    node = Blt_TreePrevSibling(node);
	} else if (isdigit(UCHAR(*p))) {
	    int inode;
	    
	    if (Tcl_GetInt(interp, p, &inode) != TCL_OK) {
		node = NULL;
	    } else {
		node = Blt_TreeGetNode(tree, inode);
	    }
	} else {
	    char *endp;

	    if (np != NULL) {
		endp = np - 1;
	    } else {
		endp = p + strlen(p) - 1;
	    }
	    if ((*p == '"') && (*endp == '"')) {
		*endp = '\0';
		node = Blt_TreeFindChild(node, p + 1);
		*endp = '"';
	    } else {
		node = Blt_TreeFindChild(node, p);
	    }		
	}
	if (node == NULL) {
	    goto error;
	}
	if (np != NULL) {
	    *np = '-';		/* Repair the string */
	}
	p = np;
    } while (np != NULL);
    return node;
 error:
    if (np != NULL) {
	*np = '-';		/* Repair the string */
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetForeignNode --
 *
 *---------------------------------------------------------------------- 
 */
static int 
GetForeignNode(
    Tcl_Interp *interp,
    Blt_Tree tree,
    Tcl_Obj *objPtr,
    Blt_TreeNode *nodePtr)
{
    char c;
    Blt_TreeNode node;
    char *string;
    char *p;

    string = Tcl_GetString(objPtr);
    c = string[0];

    /* 
     * Check if modifiers are present.
     */
    p = strstr(string, "->");
    if (isdigit(UCHAR(c))) {
	int inode;

	if (p != NULL) {
	    char save;
	    int result;

	    save = *p;
	    *p = '\0';
	    result = Tcl_GetInt(interp, string, &inode);
	    *p = save;
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	} else {
	    if (Tcl_GetIntFromObj(interp, objPtr, &inode) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	node = Blt_TreeGetNode(tree, inode);
	if (p != NULL) {
	    node = ParseModifiers(interp, tree, node, p);
	}
	if (node != NULL) {
	    *nodePtr = node;
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "can't find tag or id \"", string, "\" in ",
	 Blt_TreeName(tree), (char *)NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetNode --
 *
 *---------------------------------------------------------------------- 
 */
static int 
GetNode(TreeCmd *cmdPtr, Tcl_Obj *objPtr, Blt_TreeNode *nodePtr)
{
    Tcl_Interp *interp = cmdPtr->interp;
    Blt_Tree tree = cmdPtr->tree;
    char c;
    Blt_TreeNode node;
    char *string;
    char *p;

    string = Tcl_GetString(objPtr);
    c = string[0];

    /* 
     * Check if modifiers are present.
     */
    p = strstr(string, "->");
    if (isdigit(UCHAR(c))) {
	int inode;

	if (p != NULL) {
	    char save;
	    int result;

	    save = *p;
	    *p = '\0';
	    result = Tcl_GetInt(interp, string, &inode);
	    *p = save;
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	} else {
	    if (Tcl_GetIntFromObj(interp, objPtr, &inode) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	node = Blt_TreeGetNode(tree, inode);
    }  else if (cmdPtr != NULL) {
	char save;

	save = '\0';		/* Suppress compiler warning. */
	if (p != NULL) {
	    save = *p;
	    *p = '\0';
	}
	if (strcmp(string, "all") == 0) {
	    if (Blt_TreeSize(Blt_TreeRootNode(tree)) > 1) {
		Tcl_AppendResult(interp, "more than one node tagged as \"", 
				 string, "\"", (char *)NULL);
		if (p != NULL) {
		    *p = save;
		}
		return TCL_ERROR;
	    }
	    node = Blt_TreeRootNode(tree);
	} else if (strcmp(string, "root") == 0) {
	    node = Blt_TreeRootNode(tree);
	} else {
	    Blt_HashTable *tablePtr;
	    Blt_HashSearch cursor;
	    Blt_HashEntry *hPtr;
	    int result;

	    node = NULL;
	    result = TCL_ERROR;
	    tablePtr = Blt_TreeTagHashTable(cmdPtr->tree, string);
	    if (tablePtr == NULL) {
		Tcl_AppendResult(interp, "can't find tag or id \"", string, 
			"\" in ", Blt_TreeName(cmdPtr->tree), (char *)NULL);
	    } else if (tablePtr->numEntries > 1) {
		Tcl_AppendResult(interp, "more than one node tagged as \"", 
			 string, "\"", (char *)NULL);
	    } else if (tablePtr->numEntries > 0) {
		hPtr = Blt_FirstHashEntry(tablePtr, &cursor);
		node = Blt_GetHashValue(hPtr);
		result = TCL_OK;
	    }
	    if (result == TCL_ERROR) {
		if (p != NULL) {
		    *p = save;
		}
		return TCL_ERROR;
	    }
	}
	if (p != NULL) {
	    *p = save;
	}
    }
    if (node != NULL) {
	if (p != NULL) {
	    node = ParseModifiers(interp, tree, node, p);
	    if (node == NULL) {
		goto error;
	    }
	}
	*nodePtr = node;
	return TCL_OK;
    }
 error:
    Tcl_AppendResult(interp, "can't find tag or id \"", string, "\" in ",
		 Blt_TreeName(tree), (char *)NULL);
    return TCL_ERROR;
}

typedef struct {
    int tagType;
    Blt_TreeNode root;
    Blt_HashSearch cursor;
} TagSearch;

/*
 *----------------------------------------------------------------------
 *
 * FirstTaggedNode --
 *
 *	Returns the id of the first node tagged by the given tag in
 *	objPtr.  It basically hides much of the cumbersome special
 *	case details.  For example, the special tags "root" and "all"
 *	always exist, so they don't have entries in the tag hashtable.
 *	If it's a hashed tag (not "root" or "all"), we have to save
 *	the place of where we are in the table for the next call to
 *	NextTaggedNode.
 *
 *---------------------------------------------------------------------- 
 */
static Blt_TreeNode
FirstTaggedNode(
    Tcl_Interp *interp,
    TreeCmd *cmdPtr,
    Tcl_Obj *objPtr,
    TagSearch *cursorPtr)
{
    Blt_TreeNode node, root;
    char *string;

    node = NULL;

    root = Blt_TreeRootNode(cmdPtr->tree);
    string = Tcl_GetString(objPtr);
    cursorPtr->tagType = TAG_TYPE_NONE;
    cursorPtr->root = root;

    /* Process strings with modifiers or digits as simple ids, not
     * tags. */
    if ((strstr(string, "->") != NULL) || (isdigit(UCHAR(*string)))) {
	if (GetNode(cmdPtr, objPtr, &node) != TCL_OK) {
	    return NULL;
	}
	return node;
    }
    if (strcmp(string, "all") == 0) {
	cursorPtr->tagType = TAG_TYPE_ALL;
	return root;
    } else if (strcmp(string, "root") == 0)  {
	return root;
    } else {
	Blt_HashTable *tablePtr;
	
	tablePtr = Blt_TreeTagHashTable(cmdPtr->tree, string);
	if (tablePtr != NULL) {
	    Blt_HashEntry *hPtr;
	    
	    cursorPtr->tagType = TAG_TYPE_TAG;
	    hPtr = Blt_FirstHashEntry(tablePtr, &(cursorPtr->cursor)); 
	    if (hPtr == NULL) {
		return NULL;
	    }
	    node = Blt_GetHashValue(hPtr);
	    return node;
	}
    }
    Tcl_AppendResult(interp, "can't find tag or id \"", string, "\" in ", 
	Blt_TreeName(cmdPtr->tree), (char *)NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * NextTaggedNode --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_TreeNode
NextTaggedNode(Blt_TreeNode node, TagSearch *cursorPtr)
{
    if (cursorPtr->tagType == TAG_TYPE_ALL) {
	return Blt_TreeNextNode(cursorPtr->root, node);
    }
    if (cursorPtr->tagType == TAG_TYPE_TAG) {
	Blt_HashEntry *hPtr;

	hPtr = Blt_NextHashEntry(&(cursorPtr->cursor));
	if (hPtr == NULL) {
	    return NULL;
	}
	return Blt_GetHashValue(hPtr);
    }
    return NULL;
}

static int
AddTag(TreeCmd *cmdPtr, Blt_TreeNode node, CONST char *tagName)
{
    if (strcmp(tagName, "root") == 0) {
	Tcl_AppendResult(cmdPtr->interp, "can't add reserved tag \"",
			 tagName, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    Blt_TreeAddTag(cmdPtr->tree, node, tagName);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * DeleteNode --
 *
 *---------------------------------------------------------------------- 
 */
static void
DeleteNode(TreeCmd *cmdPtr, Blt_TreeNode node)
{
    Blt_TreeNode root;

    if (!Blt_TreeTagTableIsShared(cmdPtr->tree)) {
	Blt_TreeClearTags(cmdPtr->tree, node);
    }
    root = Blt_TreeRootNode(cmdPtr->tree);
    if (node == root) {
	Blt_TreeNode next;
	/* Don't delete the root node. Simply clean out the tree. */
	for (node = Blt_TreeFirstChild(node); node != NULL; node = next) {
	    next = Blt_TreeNextSibling(node);
	    Blt_TreeDeleteNode(cmdPtr->tree, node);
	}	    
    } else if (Blt_TreeIsAncestor(root, node)) {
	Blt_TreeDeleteNode(cmdPtr->tree, node);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetNodePath --
 *
 *---------------------------------------------------------------------- 
 */
static char *
GetNodePath(
    TreeCmd *cmdPtr,
    Blt_TreeNode root, 
    Blt_TreeNode node,
    int rootFlag,		/* If non-zero, indicates to include
				 * the root name in the path */
    Tcl_DString *resultPtr)
{
    char **nameArr;		/* Used to stack the component names. */
    char *staticSpace[64];
    register int i;
    int nLevels;

    nLevels = Blt_TreeNodeDepth(cmdPtr->tree, node) -
	Blt_TreeNodeDepth(cmdPtr->tree, root);
    if (rootFlag) {
	nLevels++;
    }
    if (nLevels > 64) {
	nameArr = Blt_Malloc(nLevels * sizeof(char *));
	assert(nameArr);
    } else {
	nameArr = staticSpace;
    }
    for (i = nLevels; i > 0; i--) {
	/* Save the name of each ancestor in the name array. 
	 * Note that we ignore the root. */
	nameArr[i - 1] = Blt_TreeNodeLabel(node);
	node = Blt_TreeNodeParent(node);
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

/*
 *----------------------------------------------------------------------
 *
 * ParseNode5 --
 *
 *	Parses and creates a node based upon the first 3 fields of
 *	a five field entry.  This is the new restore file format.
 *
 *		parentId nodeId pathList dataList tagList
 *
 *	The purpose is to attempt to save and restore the node ids
 *	embedded in the restore file information.  The old format
 *	could not distinquish between two sibling nodes with the same
 *	label unless they were both leaves.  I'm trying to avoid
 *	dependencies upon labels.  
 *
 *	If you're starting from an empty tree, this obviously should
 *	work without a hitch.  We only need to map the file's root id
 *	to 0.  It's a little more complicated when adding node to an
 *	already full tree.  
 *
 *	First see if the node id isn't already in use.  Otherwise, map
 *	the node id (via a hashtable) to the real node. We'll need it
 *	later when subsequent entries refer to their parent id.
 *
 *	If a parent id is unknown (the restore file may be out of
 *	order), then follow plan B and use its path.
 *	
 *---------------------------------------------------------------------- 
 */
static Blt_TreeNode
ParseNode5(TreeCmd *cmdPtr, char **argv, RestoreData *dataPtr)
{
    Blt_HashEntry *hPtr;
    Blt_TreeNode node, parent;
    char **names;
    int nNames, isNew;
    int parentId, nodeId;

    if ((Tcl_GetInt(cmdPtr->interp, argv[0], &parentId) != TCL_OK) ||
	(Tcl_GetInt(cmdPtr->interp, argv[1], &nodeId) != TCL_OK) ||
	(Tcl_SplitList(cmdPtr->interp, argv[2], &nNames, &names) != TCL_OK)) {
	return NULL;
    }    

    if (parentId == -1) {	/* Dump marks root's parent as -1. */
	node = dataPtr->root;
	/* Create a mapping between the old id and the new node */
	hPtr = Blt_CreateHashEntry(&dataPtr->idTable, (char *)nodeId, 
		   &isNew);
	Blt_SetHashValue(hPtr, node);
	Blt_TreeRelabelNode(cmdPtr->tree, node, names[0]);
    } else {
	/* 
	 * Check if the parent has been translated to another id.
	 * This can happen when there's a id collision with an
	 * existing node. 
	 */
	hPtr = Blt_FindHashEntry(&dataPtr->idTable, (char *)parentId);
	if (hPtr != NULL) {
	    parent = Blt_GetHashValue(hPtr);
	} else {
	    /* Check if the id already exists in the tree. */
	    parent = Blt_TreeGetNode(cmdPtr->tree, parentId);
	    if (parent == NULL) {
		/* Parent id doesn't exist (partial restore?). 
		 * Plan B: Use the path to create/find the parent with 
		 *	   the requested parent id. */
		if (nNames > 1) {
		    int i;

		    for (i = 1; i < (nNames - 2); i++) {
			node = Blt_TreeFindChild(parent, names[i]);
			if (node == NULL) {
			    node = Blt_TreeCreateNode(cmdPtr->tree, parent, 
			      names[i], -1);
			}
			parent = node;
		    }
		    node = Blt_TreeFindChild(parent, names[nNames - 2]);
		    if (node == NULL) {
			node = Blt_TreeCreateNodeWithId(cmdPtr->tree, parent, 
				names[nNames - 2], parentId, -1);
		    }
		    parent = node;
		} else {
		    parent = dataPtr->root;
		}
	    }
	} 
	/* Check if old node id already in use. */
	node = NULL;
	if (dataPtr->flags & RESTORE_OVERWRITE) {
	    node = Blt_TreeFindChild(parent, names[nNames - 1]);
	    /* Create a mapping between the old id and the new node */
	    hPtr = Blt_CreateHashEntry(&dataPtr->idTable, (char *)nodeId, 
				       &isNew);
	    Blt_SetHashValue(hPtr, node);
	}
	if (node == NULL) {
	    node = Blt_TreeGetNode(cmdPtr->tree, nodeId);
	    if (node != NULL) {
		node = Blt_TreeCreateNode(cmdPtr->tree, parent, 
					  names[nNames - 1], -1);
		/* Create a mapping between the old id and the new node */
		hPtr = Blt_CreateHashEntry(&dataPtr->idTable, (char *)nodeId,
					   &isNew);
		Blt_SetHashValue(hPtr, node);
	    } else {
		/* Otherwise create a new node with the requested id. */
		node = Blt_TreeCreateNodeWithId(cmdPtr->tree, parent, 
						names[nNames - 1], nodeId, -1);
	    }
	}
    }
    Blt_Free(names);
    return node;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseNode3 --
 *
 *	Parses and creates a node based upon the first field of
 *	a three field entry.  This is the old restore file format.
 *
 *		pathList dataList tagList
 *
 *----------------------------------------------------------------------
 */
static Blt_TreeNode
ParseNode3(TreeCmd *cmdPtr, char **argv, RestoreData *dataPtr)
{
    Blt_TreeNode node, parent;
    char **names;
    int i;
    int nNames;
    
    if (Tcl_SplitList(cmdPtr->interp, argv[0], &nNames, &names) != TCL_OK) {
	return NULL;
    }
    node = parent = dataPtr->root;
    /*  Automatically create nodes as needed except for the last node.  */
    for (i = 0; i < (nNames - 1); i++) {
	node = Blt_TreeFindChild(parent, names[i]);
	if (node == NULL) {
	    node = Blt_TreeCreateNode(cmdPtr->tree, parent, names[i], -1);
	}
	parent = node;
    }
    if (nNames > 0) {
	/* 
	 * By default, create duplicate nodes (two sibling nodes with
	 * the same label), unless the -overwrite flag was set.
	 */
	node = NULL;
	if (dataPtr->flags & RESTORE_OVERWRITE) {
	    node = Blt_TreeFindChild(parent, names[i]);
	}
	if (node == NULL) {
	    node = Blt_TreeCreateNode(cmdPtr->tree, parent, names[i], -1);
	}
    }
    Blt_Free(names);
    return node;
}

static int
RestoreNode(TreeCmd *cmdPtr, int argc, char **argv, RestoreData *dataPtr)
{
    Blt_TreeNode node;
    Tcl_Obj *valueObjPtr;
    char **elemArr;
    int nElem, result;
    register int i;

    if ((argc != 3) && (argc != 5)) {
	Tcl_AppendResult(cmdPtr->interp, "line #", Blt_Itoa(nLines), 
		": wrong # elements in restore entry", (char *)NULL);
	return TCL_ERROR;
    }
    /* Parse the path name. */
    if (argc == 3) {
	node = ParseNode3(cmdPtr, argv, dataPtr);
	argv++;
    } else if (argc == 5) {
	node = ParseNode5(cmdPtr, argv, dataPtr);
	argv += 3;
    } else {
	Tcl_AppendResult(cmdPtr->interp, "line #", Blt_Itoa(nLines), 
		": wrong # elements in restore entry", (char *)NULL);
	return TCL_ERROR;
    }
    if (node == NULL) {
	return TCL_ERROR;
    }
    /* Parse the key-value list. */
    if (Tcl_SplitList(cmdPtr->interp, argv[0], &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < nElem; i += 2) {
	if ((i + 1) < nElem) {
	    valueObjPtr = Tcl_NewStringObj(elemArr[i + 1], -1);
	} else {
	    valueObjPtr = bltEmptyStringObjPtr;
	}
	Tcl_IncrRefCount(valueObjPtr);
	result = Blt_TreeSetValue(cmdPtr->interp, cmdPtr->tree, node, 
			  elemArr[i], valueObjPtr);
	Tcl_DecrRefCount(valueObjPtr);
	if (result != TCL_OK) {
	    Blt_Free(elemArr);
	    return TCL_ERROR;
	}
    }
    Blt_Free(elemArr);
    if (!(dataPtr->flags & RESTORE_NO_TAGS)) {
	/* Parse the tag list. */
	if (Tcl_SplitList(cmdPtr->interp, argv[1], &nElem, &elemArr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	for (i = 0; i < nElem; i++) {
	    if (AddTag(cmdPtr, node, elemArr[i]) != TCL_OK) {
		Blt_Free(elemArr);
		return TCL_ERROR;
	    }
	}
	Blt_Free(elemArr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintNode --
 *
 *---------------------------------------------------------------------- 
 */
static void
PrintNode(
    TreeCmd *cmdPtr, 
    Blt_TreeNode root, 
    Blt_TreeNode node, 
    Tcl_DString *resultPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char *pathName;
    Tcl_DString dString;
    Tcl_Obj *valueObjPtr;
    register Blt_TreeKey key;
    Blt_TreeTagEntry *tPtr;
    Blt_TreeKeySearch keyIter;

    if (node == root) {
	Tcl_DStringAppendElement(resultPtr, "-1");
    } else {
	Blt_TreeNode parent;

	parent = Blt_TreeNodeParent(node);
	Tcl_DStringAppendElement(resultPtr, Blt_Itoa(Blt_TreeNodeId(parent)));
    }	
    Tcl_DStringAppendElement(resultPtr, Blt_Itoa(Blt_TreeNodeId(node)));

    pathName = GetNodePath(cmdPtr, root, node, TRUE, &dString);
    Tcl_DStringAppendElement(resultPtr, pathName);
    Tcl_DStringStartSublist(resultPtr);
    for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &keyIter); key != NULL; 
	 key = Blt_TreeNextKey(cmdPtr->tree, &keyIter)) {
	if (Blt_TreeGetValueByKey((Tcl_Interp *)NULL, cmdPtr->tree, node, 
		key, &valueObjPtr) == TCL_OK) {
	    Tcl_DStringAppendElement(resultPtr, key);
	    Tcl_DStringAppendElement(resultPtr, Tcl_GetString(valueObjPtr));
	}
    }	    
    Tcl_DStringEndSublist(resultPtr);
    Tcl_DStringStartSublist(resultPtr);
    for (hPtr = Blt_TreeFirstTag(cmdPtr->tree, &cursor); hPtr != NULL; 
	hPtr = Blt_NextHashEntry(&cursor)) {
	tPtr = Blt_GetHashValue(hPtr);
	if (Blt_FindHashEntry(&tPtr->nodeTable, (char *)node) != NULL) {
	    Tcl_DStringAppendElement(resultPtr, tPtr->tagName);
	}
    }
    Tcl_DStringEndSublist(resultPtr);
    Tcl_DStringAppend(resultPtr, "\n", -1);
    Tcl_DStringFree(&dString);
}

/*
 *----------------------------------------------------------------------
 *
 * PrintTraceFlags --
 *
 *---------------------------------------------------------------------- 
 */
static void
PrintTraceFlags(unsigned int flags, char *string)
{
    register char *p;

    p = string;
    if (flags & TREE_TRACE_READ) {
	*p++ = 'r';
    } 
    if (flags & TREE_TRACE_WRITE) {
	*p++ = 'w';
    } 
    if (flags & TREE_TRACE_UNSET) {
	*p++ = 'u';
    } 
    if (flags & TREE_TRACE_CREATE) {
	*p++ = 'c';
    } 
    *p = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * GetTraceFlags --
 *
 *---------------------------------------------------------------------- 
 */
static int
GetTraceFlags(char *string)
{
    register char *p;
    unsigned int flags;

    flags = 0;
    for (p = string; *p != '\0'; p++) {
	switch (toupper(*p)) {
	case 'R':
	    flags |= TREE_TRACE_READ;
	    break;
	case 'W':
	    flags |= TREE_TRACE_WRITE;
	    break;
	case 'U':
	    flags |= TREE_TRACE_UNSET;
	    break;
	case 'C':
	    flags |= TREE_TRACE_CREATE;
	    break;
	default:
	    return -1;
	}
    }
    return flags;
}

/*
 *----------------------------------------------------------------------
 *
 * SetValues --
 *
 *---------------------------------------------------------------------- 
 */
static int
SetValues(TreeCmd *cmdPtr, Blt_TreeNode node, int objc, Tcl_Obj *CONST *objv)
{
    register int i;
    char *string;

    for (i = 0; i < objc; i += 2) {
	string = Tcl_GetString(objv[i]);
	if ((i + 1) == objc) {
	    Tcl_AppendResult(cmdPtr->interp, "missing value for field \"", 
		string, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (Blt_TreeSetValue(cmdPtr->interp, cmdPtr->tree, node, string, 
			     objv[i + 1]) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UnsetValues --
 *
 *---------------------------------------------------------------------- 
 */
static int
UnsetValues(TreeCmd *cmdPtr, Blt_TreeNode node, int objc, Tcl_Obj *CONST *objv)
{
    if (objc == 0) {
	Blt_TreeKey key;
	Blt_TreeKeySearch cursor;

	for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &cursor); key != NULL;
	     key = Blt_TreeNextKey(cmdPtr->tree, &cursor)) {
	    if (Blt_TreeUnsetValueByKey(cmdPtr->interp, cmdPtr->tree, node, 
			key) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    } else {
	register int i;

	for (i = 0; i < objc; i ++) {
	    if (Blt_TreeUnsetValue(cmdPtr->interp, cmdPtr->tree, node, 
		Tcl_GetString(objv[i])) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

static int
ComparePatternList(Blt_List patternList, char *string, int nocase)
{
    Blt_ListNode node;
    int result, type;
    char *pattern;

    if (nocase) {
	string = Blt_Strdup(string);
	strtolower(string);
    }
    result = FALSE;
    for (node = Blt_ListFirstNode(patternList); node != NULL; 
	node = Blt_ListNextNode(node)) {
		
	type = (int)Blt_ListGetValue(node);
	pattern = (char *)Blt_ListGetKey(node);
	switch (type) {
	case PATTERN_EXACT:
	    result = (strcmp(string, pattern) == 0);
	    break;
	    
	case PATTERN_GLOB:
	    result = Tcl_StringMatch(string, pattern);
	    break;
		    
	case PATTERN_REGEXP:
	    result = Tcl_RegExpMatch((Tcl_Interp *)NULL, string, pattern); 
	    break;
	}
    }
    if (nocase) {
	Blt_Free(string);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchNodeProc --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
MatchNodeProc(Blt_TreeNode node, ClientData clientData, int order)
{
    FindData *dataPtr = clientData;
    Tcl_DString dString;
    TreeCmd *cmdPtr = dataPtr->cmdPtr;
    Tcl_Interp *interp = dataPtr->cmdPtr->interp;
    int result, invert;

    if ((dataPtr->flags & MATCH_LEAFONLY) && (!Blt_TreeIsLeaf(node))) {
	return TCL_OK;
    }
    if ((dataPtr->maxDepth >= 0) &&
	(dataPtr->maxDepth < Blt_TreeNodeDepth(cmdPtr->tree, node))) {
	return TCL_OK;
    }
    result = TRUE;
    Tcl_DStringInit(&dString);
    if (dataPtr->keyList != NULL) {
	Blt_TreeKey key;
	Blt_TreeKeySearch cursor;

	result = FALSE;		/* It's false if no keys match. */
	for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &cursor);
	     key != NULL; key = Blt_TreeNextKey(cmdPtr->tree, &cursor)) {
	    
	    result = ComparePatternList(dataPtr->keyList, key, 0);
	    if (!result) {
		continue;
	    }
	    if (dataPtr->patternList != NULL) {
		char *string;
		Tcl_Obj *objPtr;

		Blt_TreeGetValue(interp, cmdPtr->tree, node, key, &objPtr);
		string = (objPtr == NULL) ? "" : Tcl_GetString(objPtr);
		result = ComparePatternList(dataPtr->patternList, string, 
			 dataPtr->flags & MATCH_NOCASE);
		if (!result) {
		    continue;
		}
	    }
	    break;
	}
    } else if (dataPtr->patternList != NULL) {	    
	char *string;

	if (dataPtr->flags & MATCH_PATHNAME) {
	    string = GetNodePath(cmdPtr, Blt_TreeRootNode(cmdPtr->tree),
		 node, FALSE, &dString);
	} else {
	    string = Blt_TreeNodeLabel(node);
	}
	result = ComparePatternList(dataPtr->patternList, string, 
		dataPtr->flags & MATCH_NOCASE);		     
    }
    if ((dataPtr->withTag != NULL) && 
	(!Blt_TreeHasTag(cmdPtr->tree, node, dataPtr->withTag))) {
	result = FALSE;
    }
    Tcl_DStringFree(&dString);
    invert = (dataPtr->flags & MATCH_INVERT) ? TRUE : FALSE;
    if (result != invert) {
	Tcl_Obj *objPtr;

	if (dataPtr->addTag != NULL) {
	    if (AddTag(cmdPtr, node, dataPtr->addTag) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	objPtr = Tcl_NewIntObj(Blt_TreeNodeId(node));
	Tcl_ListObjAppendElement(interp, dataPtr->listObjPtr, objPtr);
	if (dataPtr->objv != NULL) {
	    dataPtr->objv[dataPtr->objc - 1] = objPtr;
	    Tcl_IncrRefCount(objPtr);
	    result = Tcl_EvalObjv(interp, dataPtr->objc, dataPtr->objv, 0);
	    Tcl_DecrRefCount(objPtr);
	    dataPtr->objv[dataPtr->objc - 1] = NULL;
	    if (result != TCL_OK) {
		return result;
	    }
	}
	dataPtr->nMatches++;
	if ((dataPtr->maxMatches > 0) && 
	    (dataPtr->nMatches >= dataPtr->maxMatches)) {
	    return TCL_BREAK;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ApplyNodeProc --
 *
 *---------------------------------------------------------------------- 
 */
static int
ApplyNodeProc(Blt_TreeNode node, ClientData clientData, int order)
{
    ApplyData *dataPtr = clientData;
    TreeCmd *cmdPtr = dataPtr->cmdPtr;
    Tcl_Interp *interp = cmdPtr->interp;
    int invert, result;
    Tcl_DString dString;

    if ((dataPtr->flags & MATCH_LEAFONLY) && (!Blt_TreeIsLeaf(node))) {
	return TCL_OK;
    }
    if ((dataPtr->maxDepth >= 0) &&
	(dataPtr->maxDepth < Blt_TreeNodeDepth(cmdPtr->tree, node))) {
	return TCL_OK;
    }
    Tcl_DStringInit(&dString);
    result = TRUE;
    if (dataPtr->keyList != NULL) {
	Blt_TreeKey key;
	Blt_TreeKeySearch cursor;

	result = FALSE;		/* It's false if no keys match. */
	for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &cursor);
	     key != NULL; key = Blt_TreeNextKey(cmdPtr->tree, &cursor)) {
	    
	    result = ComparePatternList(dataPtr->keyList, key, 0);
	    if (!result) {
		continue;
	    }
	    if (dataPtr->patternList != NULL) {
		char *string;
		Tcl_Obj *objPtr;

		Blt_TreeGetValue(interp, cmdPtr->tree, node, key, &objPtr);
		string = (objPtr == NULL) ? "" : Tcl_GetString(objPtr);
		result = ComparePatternList(dataPtr->patternList, string, 
			 dataPtr->flags & MATCH_NOCASE);
		if (!result) {
		    continue;
		}
	    }
	    break;
	}
    } else if (dataPtr->patternList != NULL) {	    
	char *string;

	if (dataPtr->flags & MATCH_PATHNAME) {
	    string = GetNodePath(cmdPtr, Blt_TreeRootNode(cmdPtr->tree),
		 node, FALSE, &dString);
	} else {
	    string = Blt_TreeNodeLabel(node);
	}
	result = ComparePatternList(dataPtr->patternList, string, 
		dataPtr->flags & MATCH_NOCASE);		     
    }
    Tcl_DStringFree(&dString);
    if ((dataPtr->withTag != NULL) && 
	(!Blt_TreeHasTag(cmdPtr->tree, node, dataPtr->withTag))) {
	result = FALSE;
    }
    invert = (dataPtr->flags & MATCH_INVERT) ? 1 : 0;
    if (result != invert) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewIntObj(Blt_TreeNodeId(node));
	if (order == TREE_PREORDER) {
	    dataPtr->preObjv[dataPtr->preObjc - 1] = objPtr;
	    return Tcl_EvalObjv(interp, dataPtr->preObjc, dataPtr->preObjv, 0);
	} else if (order == TREE_POSTORDER) {
	    dataPtr->postObjv[dataPtr->postObjc - 1] = objPtr;
	    return Tcl_EvalObjv(interp, dataPtr->postObjc, dataPtr->postObjv,0);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ReleaseTreeObject --
 *
 *---------------------------------------------------------------------- 
 */
static void
ReleaseTreeObject(TreeCmd *cmdPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    TraceInfo *tracePtr;
    NotifyInfo *notifyPtr;
    int i;

    Blt_TreeReleaseToken(cmdPtr->tree);
    /* 
     * When the tree token is released, all the traces and
     * notification events are automatically removed.  But we still
     * need to clean up the bookkeeping kept for traces. Clear all
     * the tags and trace information.  
     */
    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->traceTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tracePtr = Blt_GetHashValue(hPtr);
	Blt_Free(tracePtr);
    }
    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->notifyTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	notifyPtr = Blt_GetHashValue(hPtr);
	for (i = 0; i < notifyPtr->objc - 2; i++) {
	    Tcl_DecrRefCount(notifyPtr->objv[i]);
	}
	Blt_Free(notifyPtr->objv);
	Blt_Free(notifyPtr);
    }
    cmdPtr->tree = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTraceProc --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TreeTraceProc(
    ClientData clientData,
    Tcl_Interp *interp,
    Blt_TreeNode node,		/* Node that has just been updated. */
    Blt_TreeKey key,		/* Field that's updated. */
    unsigned int flags)
{
    TraceInfo *tracePtr = clientData; 
    Tcl_DString dsCmd, dsName;
    char string[5];
    char *qualName;
    int result;

    Tcl_DStringInit(&dsCmd);
    Tcl_DStringAppend(&dsCmd, tracePtr->command, -1);
    Tcl_DStringInit(&dsName);
    qualName = Blt_GetQualifiedName(
	Blt_GetCommandNamespace(interp, tracePtr->cmdPtr->cmdToken), 
	Tcl_GetCommandName(interp, tracePtr->cmdPtr->cmdToken), &dsName);
    Tcl_DStringAppendElement(&dsCmd, qualName);
    Tcl_DStringFree(&dsName);
    if (node != NULL) {
	Tcl_DStringAppendElement(&dsCmd, Blt_Itoa(Blt_TreeNodeId(node)));
    } else {
	Tcl_DStringAppendElement(&dsCmd, "");
    }
    Tcl_DStringAppendElement(&dsCmd, key);
    PrintTraceFlags(flags, string);
    Tcl_DStringAppendElement(&dsCmd, string);
    result = Tcl_Eval(interp, Tcl_DStringValue(&dsCmd));
    Tcl_DStringFree(&dsCmd);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeEventProc --
 *
 *---------------------------------------------------------------------- 
 */
static int
TreeEventProc(ClientData clientData, Blt_TreeNotifyEvent *eventPtr)
{
    TreeCmd *cmdPtr = clientData; 
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    NotifyInfo *notifyPtr;
    Blt_TreeNode node;
    char *string;

    switch (eventPtr->type) {
    case TREE_NOTIFY_CREATE:
	string = "-create";
	break;

    case TREE_NOTIFY_DELETE:
	node = Blt_TreeGetNode(cmdPtr->tree, eventPtr->inode);
	if (node != NULL) {
	    Blt_TreeClearTags(cmdPtr->tree, node);
	}
	string = "-delete";
	break;

    case TREE_NOTIFY_MOVE:
	string = "-move";
	break;

    case TREE_NOTIFY_SORT:
	string = "-sort";
	break;

    case TREE_NOTIFY_RELABEL:
	string = "-relabel";
	break;

    default:
	/* empty */
	string = "???";
	break;
    }	

    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->notifyTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	notifyPtr = Blt_GetHashValue(hPtr);
	if (notifyPtr->mask & eventPtr->type) {
	    int result;
	    Tcl_Obj *flagObjPtr, *nodeObjPtr;

	    flagObjPtr = Tcl_NewStringObj(string, -1);
	    nodeObjPtr = Tcl_NewIntObj(eventPtr->inode);
	    Tcl_IncrRefCount(flagObjPtr);
	    Tcl_IncrRefCount(nodeObjPtr);
	    notifyPtr->objv[notifyPtr->objc - 2] = flagObjPtr;
	    notifyPtr->objv[notifyPtr->objc - 1] = nodeObjPtr;
	    result = Tcl_EvalObjv(cmdPtr->interp, notifyPtr->objc, 
		notifyPtr->objv, 0);
	    Tcl_DecrRefCount(nodeObjPtr);
	    Tcl_DecrRefCount(flagObjPtr);
	    if (result != TCL_OK) {
		Tcl_BackgroundError(cmdPtr->interp);
		return TCL_ERROR;
	    }
	    Tcl_ResetResult(cmdPtr->interp);
	}
    }
    return TCL_OK;
}


/* Tree command operations. */

/*
 *----------------------------------------------------------------------
 *
 * ApplyOp --
 *
 * t0 apply root -precommand {command} -postcommand {command}
 *
 *---------------------------------------------------------------------- 
 */
static int
ApplyOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    int result;
    Blt_TreeNode node;
    int i;
    Tcl_Obj **objArr;
    int count;
    ApplyData data;
    int order;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    memset(&data, 0, sizeof(data));
    data.maxDepth = -1;
    data.cmdPtr = cmdPtr;
    
    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, applySwitches, objc - 3, objv + 3, 
	     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    order = 0;
    if (data.flags & MATCH_NOCASE) {
	Blt_ListNode listNode;

	for (listNode = Blt_ListFirstNode(data.patternList); listNode != NULL;
	     listNode = Blt_ListNextNode(listNode)) {
	    strtolower((char *)Blt_ListGetKey(listNode));
	}
    }
    if (data.preCmd != NULL) {
	char **p;

	count = 0;
	for (p = data.preCmd; *p != NULL; p++) {
	    count++;
	}
	objArr = Blt_Malloc((count + 1) * sizeof(Tcl_Obj *));
	for (i = 0; i < count; i++) {
	    objArr[i] = Tcl_NewStringObj(data.preCmd[i], -1);
	    Tcl_IncrRefCount(objArr[i]);
	}
	data.preObjv = objArr;
	data.preObjc = count + 1;
	order |= TREE_PREORDER;
    }
    if (data.postCmd != NULL) {
	char **p;

	count = 0;
	for (p = data.postCmd; *p != NULL; p++) {
	    count++;
	}
	objArr = Blt_Malloc((count + 1) * sizeof(Tcl_Obj *));
	for (i = 0; i < count; i++) {
	    objArr[i] = Tcl_NewStringObj(data.postCmd[i], -1);
	    Tcl_IncrRefCount(objArr[i]);
	}
	data.postObjv = objArr;
	data.postObjc = count + 1;
	order |= TREE_POSTORDER;
    }
    result = Blt_TreeApplyDFS(node, ApplyNodeProc, &data, order);
    if (data.preObjv != NULL) {
	for (i = 0; i < (data.preObjc - 1); i++) {
	    Tcl_DecrRefCount(data.preObjv[i]);
	}
	Blt_Free(data.preObjv);
    }
    if (data.postObjv != NULL) {
	for (i = 0; i < (data.postObjc - 1); i++) {
	    Tcl_DecrRefCount(data.postObjv[i]);
	}
	Blt_Free(data.postObjv);
    }
    Blt_FreeSwitches(applySwitches, (char *)&data, 0);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*ARGSUSED*/
static int
AncestorOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    int d1, d2, minDepth;
    register int i;
    Blt_TreeNode ancestor, node1, node2;

    if ((GetNode(cmdPtr, objv[2], &node1) != TCL_OK) ||
	(GetNode(cmdPtr, objv[3], &node2) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (node1 == node2) {
	ancestor = node1;
	goto done;
    }
    d1 = Blt_TreeNodeDepth(cmdPtr->tree, node1);
    d2 = Blt_TreeNodeDepth(cmdPtr->tree, node2);
    minDepth = MIN(d1, d2);
    if (minDepth == 0) {	/* One of the nodes is root. */
	ancestor = Blt_TreeRootNode(cmdPtr->tree);
	goto done;
    }
    /* 
     * Traverse back from the deepest node, until the both nodes are
     * at the same depth.  Check if the ancestor node found is the
     * other node.  
     */
    for (i = d1; i > minDepth; i--) {
	node1 = Blt_TreeNodeParent(node1);
    }
    if (node1 == node2) {
	ancestor = node2;
	goto done;
    }
    for (i = d2; i > minDepth; i--) {
	node2 = Blt_TreeNodeParent(node2);
    }
    if (node2 == node1) {
	ancestor = node1;
	goto done;
    }

    /* 
     * First find the mutual ancestor of both nodes.  Look at each
     * preceding ancestor level-by-level for both nodes.  Eventually
     * we'll find a node that's the parent of both ancestors.  Then
     * find the first ancestor in the parent's list of subnodes.  
     */
    for (i = minDepth; i > 0; i--) {
	node1 = Blt_TreeNodeParent(node1);
	node2 = Blt_TreeNodeParent(node2);
	if (node1 == node2) {
	    ancestor = node2;
	    goto done;
	}
    }
    Tcl_AppendResult(interp, "unknown ancestor", (char *)NULL);
    return TCL_ERROR;
 done:
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_TreeNodeId(ancestor));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AttachOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
AttachOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    if (objc == 3) {
	CONST char *treeName;
	CONST char *name;
	Blt_Tree token;
	Tcl_Namespace *nsPtr;
	Tcl_DString dString;
	int result;

	treeName = Tcl_GetString(objv[2]);
	if (Blt_ParseQualifiedName(interp, treeName, &nsPtr, &name) 
	    != TCL_OK) {
	    Tcl_AppendResult(interp, "can't find namespace in \"", treeName, 
			     "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (nsPtr == NULL) {
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	}
	treeName = Blt_GetQualifiedName(nsPtr, name, &dString);
	result = Blt_TreeGetToken(interp, treeName, &token);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	ReleaseTreeObject(cmdPtr);
	cmdPtr->tree = token;
    }
    Tcl_SetResult(interp, Blt_TreeName(cmdPtr->tree), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildrenOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
ChildrenOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    
    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	Tcl_Obj *objPtr, *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (node = Blt_TreeFirstChild(node); node != NULL;
	     node = Blt_TreeNextSibling(node)) {
	    objPtr = Tcl_NewIntObj(Blt_TreeNodeId(node));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else if (objc == 4) {
	int childPos;
	int inode, count;
	
	/* Get the node at  */
	if (Tcl_GetIntFromObj(interp, objv[3], &childPos) != TCL_OK) {
		return TCL_ERROR;
	}
	count = 0;
	inode = -1;
	for (node = Blt_TreeFirstChild(node); node != NULL;
	     node = Blt_TreeNextSibling(node)) {
	    if (count == childPos) {
		inode = Blt_TreeNodeId(node);
		break;
	    }
	    count++;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
	return TCL_OK;
    } else if (objc == 5) {
	int firstPos, lastPos, count;
	Tcl_Obj *objPtr, *listObjPtr;
	char *string;

	firstPos = lastPos = Blt_TreeNodeDegree(node) - 1;
	string = Tcl_GetString(objv[3]);
	if ((strcmp(string, "end") != 0) &&
	    (Tcl_GetIntFromObj(interp, objv[3], &firstPos) != TCL_OK)) {
	    return TCL_ERROR;
	}
	string = Tcl_GetString(objv[4]);
	if ((strcmp(string, "end") != 0) &&
	    (Tcl_GetIntFromObj(interp, objv[4], &lastPos) != TCL_OK)) {
	    return TCL_ERROR;
	}

	count = 0;
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (node = Blt_TreeFirstChild(node); node != NULL;
	     node = Blt_TreeNextSibling(node)) {
	    if ((count >= firstPos) && (count <= lastPos)) {
		objPtr = Tcl_NewIntObj(Blt_TreeNodeId(node));
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	    count++;
	}
	Tcl_SetObjResult(interp, listObjPtr);
    }
    return TCL_OK;
}


static Blt_TreeNode 
CopyNodes(
    CopyData *dataPtr,
    Blt_TreeNode node,		/* Node to be copied. */
    Blt_TreeNode parent)	/* New parent for the copied node. */
{
    Blt_TreeNode newNode;	/* Newly created copy. */
    char *label;

    newNode = NULL;
    label = Blt_TreeNodeLabel(node);
    if (dataPtr->flags & COPY_OVERWRITE) {
	newNode = Blt_TreeFindChild(parent, label);
    }
    if (newNode == NULL) {	/* Create node in new parent. */
	newNode = Blt_TreeCreateNode(dataPtr->destTree, parent, label, -1);
    }
    /* Copy the data fields. */
    {
	Blt_TreeKey key;
	Tcl_Obj *objPtr;
	Blt_TreeKeySearch cursor;

	for (key = Blt_TreeFirstKey(dataPtr->srcTree, node, &cursor); 
	     key != NULL; key = Blt_TreeNextKey(dataPtr->srcTree, &cursor)) {
	    if (Blt_TreeGetValueByKey((Tcl_Interp *)NULL, dataPtr->srcTree, 
			node, key, &objPtr) == TCL_OK) {
		Blt_TreeSetValueByKey((Tcl_Interp *)NULL, dataPtr->destTree, 
			newNode, key, objPtr);
	    } 
	}
    }
    /* Add tags to destination tree command. */
    if ((dataPtr->destPtr != NULL) && (dataPtr->flags & COPY_TAGS)) {
	Blt_TreeTagEntry *tPtr;
	Blt_HashEntry *hPtr, *h2Ptr;
	Blt_HashSearch cursor;

	for (hPtr = Blt_TreeFirstTag(dataPtr->srcPtr->tree, &cursor); 
		hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    h2Ptr = Blt_FindHashEntry(&tPtr->nodeTable, (char *)node);
	    if (h2Ptr != NULL) {
		if (AddTag(dataPtr->destPtr, newNode, tPtr->tagName)!= TCL_OK) {
		    return NULL;
		}
	    }
	}
    }
    if (dataPtr->flags & COPY_RECURSE) {
	Blt_TreeNode child;

	for (child = Blt_TreeFirstChild(node); child != NULL;
	     child = Blt_TreeNextSibling(child)) {
	    if (CopyNodes(dataPtr, child, newNode) == NULL) {
		return NULL;
	    }
	}
    }
    return newNode;
}

/*
 *----------------------------------------------------------------------
 *
 * CopyOp --
 * 
 *	t0 copy node tree node 
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
CopyOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TreeCmd *srcPtr, *destPtr;
    Blt_Tree srcTree, destTree;
    Blt_TreeNode srcNode, destNode;
    CopyData data;
    int nArgs, nSwitches;
    char *string;
    Blt_TreeNode root;
    register int i;

    if (GetNode(cmdPtr, objv[2], &srcNode) != TCL_OK) {
	return TCL_ERROR;
    }
    srcTree = cmdPtr->tree;
    srcPtr = cmdPtr;

    /* Find the first switch. */
    for(i = 3; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (string[0] == '-') {
	    break;
	}
    }
    nArgs = i - 2;
    nSwitches = objc - i;
    if (nArgs < 2) {
	string = Tcl_GetString(objv[0]);
	Tcl_AppendResult(interp, "must specify source and destination nodes: ",
			 "should be \"", string, 
			 " copy srcNode ?destTree? destNode ?switches?", 
			 (char *)NULL);
	return TCL_ERROR;
	
    }
    if (nArgs == 3) {
	/* 
	 * The tree name is either the name of a tree command (first choice)
	 * or an internal tree object.  
	 */
	string = Tcl_GetString(objv[3]);
	destPtr = GetTreeCmd(cmdPtr->dataPtr, interp, string);
	if (destPtr != NULL) {
	    destTree = destPtr->tree;
	} else {
	    /* Try to get the tree as an internal tree data object. */
	    if (Blt_TreeGetToken(interp, string, &destTree) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	objv++;
    } else {
	destPtr = cmdPtr;
	destTree = destPtr->tree;
    }

    root = NULL;
    if (destPtr == NULL) {
	if (GetForeignNode(interp, destTree, objv[3], &destNode) != TCL_OK) {
	    goto error;
	}
    } else {
	if (GetNode(destPtr, objv[3], &destNode) != TCL_OK) {
	    goto error;
	}
    }
    if (srcNode == destNode) {
	Tcl_AppendResult(interp, "source and destination nodes are the same",
		 (char *)NULL);	     
	goto error;
    }
    memset((char *)&data, 0, sizeof(data));
    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, copySwitches, nSwitches, objv + 4, 
	     (char *)&data, 0) < 0) {
	goto error;
    }
    data.destPtr = destPtr;
    data.destTree = destTree;
    data.srcPtr = srcPtr;
    data.srcTree = srcTree;

    if ((srcTree == destTree) && (data.flags & COPY_RECURSE) &&
	(Blt_TreeIsAncestor(srcNode, destNode))) {    
	Tcl_AppendResult(interp, "can't make cyclic copy: ",
			 "source node is an ancestor of the destination",
			 (char *)NULL);	     
	goto error;
    }

    /* Copy nodes to destination. */
    root = CopyNodes(&data, srcNode, destNode);
    if (root != NULL) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewIntObj(Blt_TreeNodeId(root));
	if (data.label != NULL) {
	    Blt_TreeRelabelNode(data.destTree, root, data.label);
	}
	Tcl_SetObjResult(interp, objPtr);
    }
 error:
    if (destPtr == NULL) {
	Blt_TreeReleaseToken(destTree);
    }
    return (root == NULL) ? TCL_ERROR : TCL_OK;

}

/*
 *----------------------------------------------------------------------
 *
 * DepthOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
DegreeOp(cmdPtr, interp, objc, objv)
    TreeCmd *cmdPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode node;
    int degree;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    degree = Blt_TreeNodeDegree(node);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), degree);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes one or more nodes from the tree.  Nodes may be
 *	specified by their id (a number) or a tag.
 *	
 *	Tags have to be handled carefully here.  We can't use the
 *	normal GetTaggedNode, NextTaggedNode, etc. routines because
 *	they walk hashtables while we're deleting nodes.  Also,
 *	remember that deleting a node recursively deletes all its
 *	children. If a parent and its children have the same tag, its
 *	possible that the tag list may contain nodes than no longer
 *	exist. So save the node indices in a list and then delete 
 *	then in a second pass.
 *
 *---------------------------------------------------------------------- 
 */
static int
DeleteOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int i;
    char *string;

    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (isdigit(UCHAR(string[0]))) {
	    if (GetNode(cmdPtr, objv[i], &node) != TCL_OK) {
		return TCL_ERROR;
	    }
	    DeleteNode(cmdPtr, node);
	} else {
	    Blt_HashEntry *hPtr;
	    Blt_HashTable *tablePtr;
	    Blt_HashSearch cursor;
	    Blt_Chain *chainPtr;
	    Blt_ChainLink *linkPtr, *nextPtr;
	    int inode;

	    if ((strcmp(string, "all") == 0) ||
		(strcmp(string, "root") == 0)) {
		node = Blt_TreeRootNode(cmdPtr->tree);
		DeleteNode(cmdPtr, node);
		continue;
	    }
	    tablePtr = Blt_TreeTagHashTable(cmdPtr->tree, string);
	    if (tablePtr == NULL) {
		goto error;
	    }
	    /* 
	     * Generate a list of tagged nodes. Save the inode instead
	     * of the node itself since a pruned branch may contain
	     * more tagged nodes.  
	     */
	    chainPtr = Blt_ChainCreate();
	    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); 
		hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		node = Blt_GetHashValue(hPtr);
		Blt_ChainAppend(chainPtr, (ClientData)Blt_TreeNodeId(node));
	    }   
	    /*  
	     * Iterate through this list to delete the nodes.  By
	     * side-effect the tag table is deleted and Uids are
	     * released.  
	     */
	    for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
		 linkPtr = nextPtr) {
		nextPtr = Blt_ChainNextLink(linkPtr);
		inode = (int)Blt_ChainGetValue(linkPtr);
		node = Blt_TreeGetNode(cmdPtr->tree, inode);
		if (node != NULL) {
		    DeleteNode(cmdPtr, node);
		}
	    }
	    Blt_ChainDestroy(chainPtr);
	}
    }
    return TCL_OK;
 error:
    Tcl_AppendResult(interp, "can't find tag or id \"", string, "\" in ", 
		     Blt_TreeName(cmdPtr->tree), (char *)NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DepthOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
DepthOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int depth;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    depth = Blt_TreeNodeDepth(cmdPtr->tree, node);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), depth);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DumpOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
DumpOp(cmdPtr, interp, objc, objv)
    TreeCmd *cmdPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode top;
    Tcl_DString dString;
    register Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[2], &top) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    for (node = top; node != NULL; node = Blt_TreeNextNode(top, node)) {
	PrintNode(cmdPtr, top, node, &dString);
    }
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DumpfileOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
DumpfileOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode top;
    Tcl_Channel channel;
    Tcl_DString dString;
    char *fileName;
    int result;
    register Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[2], &top) != TCL_OK) {
	return TCL_ERROR;
    }
    fileName = Tcl_GetString(objv[3]);
    channel = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    for (node = top; node != NULL; node = Blt_TreeNextNode(top, node)) {
	PrintNode(cmdPtr, top, node, &dString);
    }
    result = Tcl_Write(channel, Tcl_DStringValue(&dString), -1);
    Tcl_Close(interp, channel);
    Tcl_DStringFree(&dString);
    if (result <= 0) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExistsOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
ExistsOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int bool;
    
    bool = TRUE;
    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	bool = FALSE;
    } else if (objc == 4) { 
	Tcl_Obj *valueObjPtr;
	char *string;
	
	string = Tcl_GetString(objv[3]);
	if (Blt_TreeGetValue((Tcl_Interp *)NULL, cmdPtr->tree, node, 
			     string, &valueObjPtr) != TCL_OK) {
	    bool = FALSE;
	}
    } 
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
FindOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    FindData data;
    int result;
    Tcl_Obj **objArr;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    memset(&data, 0, sizeof(data));
    data.maxDepth = -1;
    data.order = TREE_POSTORDER;
    objArr = NULL;

    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, findSwitches, objc - 3, objv + 3, 
		     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    if (data.maxDepth >= 0) {
	data.maxDepth += Blt_TreeNodeDepth(cmdPtr->tree, node);
    }
    if (data.flags & MATCH_NOCASE) {
	Blt_ListNode listNode;

	for (listNode = Blt_ListFirstNode(data.patternList); listNode != NULL;
	     listNode = Blt_ListNextNode(listNode)) {
	    strtolower((char *)Blt_ListGetKey(listNode));
	}
    }
    if (data.command != NULL) {
	int count;
	char **p;
	register int i;

	count = 0;
	for (p = data.command; *p != NULL; p++) {
	    count++;
	}
	/* Leave room for node Id argument to be appended */
	objArr = Blt_Calloc(count + 2, sizeof(Tcl_Obj *));
	for (i = 0; i < count; i++) {
	    objArr[i] = Tcl_NewStringObj(data.command[i], -1);
	    Tcl_IncrRefCount(objArr[i]);
	}
	data.objv = objArr;
	data.objc = count + 1;
    }
    data.listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    data.cmdPtr = cmdPtr;
    if (data.order == TREE_BREADTHFIRST) {
	result = Blt_TreeApplyBFS(node, MatchNodeProc, &data);
    } else {
	result = Blt_TreeApplyDFS(node, MatchNodeProc, &data, data.order);
    }
    if (data.command != NULL) {
	Tcl_Obj **objPtrPtr;

	for (objPtrPtr = objArr; *objPtrPtr != NULL; objPtrPtr++) {
	    Tcl_DecrRefCount(*objPtrPtr);
	}
	Blt_Free(objArr);
    }
    Blt_FreeSwitches(findSwitches, (char *)&data, 0);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, data.listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindChildOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
FindChildOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node, child;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    child = Blt_TreeFindChild(node, Tcl_GetString(objv[3]));
    if (child != NULL) {
	inode = Blt_TreeNodeId(child);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * FirstChildOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
FirstChildOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreeFirstChild(node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
GetOp(
    TreeCmd *cmdPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	Blt_TreeKey key;
	Tcl_Obj *valueObjPtr, *listObjPtr;
	Blt_TreeKeySearch cursor;

	/* Add the key-value pairs to a new Tcl_Obj */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &cursor); key != NULL; 
	     key = Blt_TreeNextKey(cmdPtr->tree, &cursor)) {
	    if (Blt_TreeGetValue((Tcl_Interp *)NULL, cmdPtr->tree, node, key,
				 &valueObjPtr) == TCL_OK) {
		Tcl_Obj *objPtr;

		objPtr = Tcl_NewStringObj(key, -1);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		Tcl_ListObjAppendElement(interp, listObjPtr, valueObjPtr);
	    }
	}	    
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *valueObjPtr;
	char *string;

	string = Tcl_GetString(objv[3]); 
	if (Blt_TreeGetValue((Tcl_Interp *)NULL, cmdPtr->tree, node, string,
		     &valueObjPtr) != TCL_OK) {
	    if (objc == 4) {
		Tcl_DString dString;
		char *path;

		path = GetNodePath(cmdPtr, Blt_TreeRootNode(cmdPtr->tree), 
		   node, FALSE, &dString);		
		Tcl_AppendResult(interp, "can't find field \"", string, 
			"\" in \"", path, "\"", (char *)NULL);
		Tcl_DStringFree(&dString);
		return TCL_ERROR;
	    } 
	    /* Default to given value */
	    valueObjPtr = objv[4];
	} 
	Tcl_SetObjResult(interp, valueObjPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
IndexOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    inode = -1;
    if (GetNode(cmdPtr, objv[2], &node) == TCL_OK) {
	inode = Blt_TreeNodeId(node);
    } else {
	register int i;
	int nObjs;
	Tcl_Obj **objArr;
	Blt_TreeNode parent;
	char *string;

	if (Tcl_ListObjGetElements(interp, objv[2], &nObjs, &objArr) 
	    != TCL_OK) {
	    goto done;		/* Can't split object. */
	}
	parent = Blt_TreeRootNode(cmdPtr->tree);
	for (i = 0; i < nObjs; i++) {
	    string = Tcl_GetString(objArr[i]);
	    if (string[0] == '\0') {
		continue;
	    }
	    node = Blt_TreeFindChild(parent, string);
	    if (node == NULL) {
		goto done;	/* Can't find component */
	    }
	    parent = node;
	}
	inode = Blt_TreeNodeId(node);
    }
 done:
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InsertOp --
 *
 *---------------------------------------------------------------------- 
 */

static int
InsertOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode parent, child;
    InsertData data;

    child = NULL;
    if (GetNode(cmdPtr, objv[2], &parent) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Initialize switch flags */
    memset(&data, 0, sizeof(data));
    data.insertPos = -1;	/* Default to append node. */
    data.parent = parent;
    data.inode = -1;

    if (Blt_ProcessObjSwitches(interp, insertSwitches, objc - 3, objv + 3, 
	     (char *)&data, 0) < 0) {
	goto error;
    }
    if (data.inode > 0) {
	Blt_TreeNode node;

	node = Blt_TreeGetNode(cmdPtr->tree, data.inode);
	if (node != NULL) {
	    Tcl_AppendResult(interp, "can't reissue node id \"", 
		Blt_Itoa(data.inode), "\": already exists.", (char *)NULL);
	    goto error;
	}
	child = Blt_TreeCreateNodeWithId(cmdPtr->tree, parent, data.label, 
		data.inode, data.insertPos);
    } else {
	child = Blt_TreeCreateNode(cmdPtr->tree, parent, data.label, 
		data.insertPos);
    }
    if (child == NULL) {
	Tcl_AppendResult(interp, "can't allocate new node", (char *)NULL);
	goto error;
    }
    if (data.label == NULL) {
	char string[200];

	sprintf(string, "node%d", Blt_TreeNodeId(child));
	Blt_TreeRelabelNode2(child, string);
    } 
    if (data.tags != NULL) {
	register char **p;

	for (p = data.tags; *p != NULL; p++) {
	    if (AddTag(cmdPtr, child, *p) != TCL_OK) {
		goto error;
	    }
	}
    }
    if (data.dataPairs != NULL) {
	register char **p;
	char *key;
	Tcl_Obj *objPtr;

	for (p = data.dataPairs; *p != NULL; p++) {
	    key = *p;
	    p++;
	    if (*p == NULL) {
		Tcl_AppendResult(interp, "missing value for \"", key, "\"",
				 (char *)NULL);
		goto error;
	    }
	    objPtr = Tcl_NewStringObj(*p, -1);
	    if (Blt_TreeSetValue(interp, cmdPtr->tree, child, key, objPtr) 
		!= TCL_OK) {
		Tcl_DecrRefCount(objPtr);
		goto error;
	    }
	}
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_TreeNodeId(child));
    Blt_FreeSwitches(insertSwitches, (char *)&data, 0);
    return TCL_OK;

 error:
    if (child != NULL) {
	Blt_TreeDeleteNode(cmdPtr->tree, child);
    }
    Blt_FreeSwitches(insertSwitches, (char *)&data, 0);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * IsAncestorOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
IsAncestorOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node1, node2;
    int bool;

    if ((GetNode(cmdPtr, objv[3], &node1) != TCL_OK) ||
	(GetNode(cmdPtr, objv[4], &node2) != TCL_OK)) {
	return TCL_ERROR;
    }
    bool = Blt_TreeIsAncestor(node1, node2);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsBeforeOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
IsBeforeOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node1, node2;
    int bool;

    if ((GetNode(cmdPtr, objv[3], &node1) != TCL_OK) ||
	(GetNode(cmdPtr, objv[4], &node2) != TCL_OK)) {
	return TCL_ERROR;
    }
    bool = Blt_TreeIsBefore(node1, node2);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsLeafOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
IsLeafOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[3], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_TreeIsLeaf(node));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsRootOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
IsRootOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int bool;

    if (GetNode(cmdPtr, objv[3], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    bool = (node == Blt_TreeRootNode(cmdPtr->tree));
    Tcl_SetIntObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsOp --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec isOps[] =
{
    {"ancestor", 1, (Blt_Op)IsAncestorOp, 5, 5, "node1 node2",},
    {"before", 1, (Blt_Op)IsBeforeOp, 5, 5, "node1 node2",},
    {"leaf", 1, (Blt_Op)IsLeafOp, 4, 4, "node",},
    {"root", 1, (Blt_Op)IsRootOp, 4, 4, "node",},
};

static int nIsOps = sizeof(isOps) / sizeof(Blt_OpSpec);

static int
IsOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nIsOps, isOps, BLT_OP_ARG2, objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * KeysOp --
 *
 *	Returns the key names of values for a node or array value.
 *
 *---------------------------------------------------------------------- 
 */
static int
KeysOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch tagSearch;
    Blt_HashTable keyTable;
    Blt_TreeKey key;
    Blt_TreeKeySearch keyIter;
    Blt_TreeNode node;
    TagSearch tagIter;
    Tcl_Obj *listObjPtr, *objPtr;
    register int i;
    int isNew;

    Blt_InitHashTableWithPool(&keyTable, BLT_ONE_WORD_KEYS);
    for (i = 2; i < objc; i++) {
	node = FirstTaggedNode(interp, cmdPtr, objv[i], &tagIter);
	if (node == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; node != NULL; node = NextTaggedNode(node, &tagIter)) {
	    for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &keyIter); 
		 key != NULL; key = Blt_TreeNextKey(cmdPtr->tree, &keyIter)) {
		Blt_CreateHashEntry(&keyTable, key, &isNew);
	    }
	}
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&keyTable, &tagSearch); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&tagSearch)) {
	objPtr = Tcl_NewStringObj(Blt_GetHashKey(&keyTable, hPtr), -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_DeleteHashTable(&keyTable);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LabelOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
LabelOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	Blt_TreeRelabelNode(cmdPtr->tree, node, Tcl_GetString(objv[3]));
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), Blt_TreeNodeLabel(node), -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LastChildOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
LastChildOp(cmdPtr, interp, objc, objv)
    TreeCmd *cmdPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreeLastChild(node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	The big trick here is to not consider the node to be moved in
 *	determining it's new location.  Ideally, you would temporarily
 *	pull from the tree and replace it (back in its old location if
 *	something went wrong), but you could still pick the node by 
 *	its serial number.  So here we make lots of checks for the 
 *	node to be moved.
 * 
 *
 *---------------------------------------------------------------------- 
 */
static int
MoveOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode root, parent, node;
    Blt_TreeNode before;
    MoveData data;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetNode(cmdPtr, objv[3], &parent) != TCL_OK) {
	return TCL_ERROR;
    }
    root = Blt_TreeRootNode(cmdPtr->tree);
    if (node == root) {
	Tcl_AppendResult(interp, "can't move root node", (char *)NULL);
	return TCL_ERROR;
    }
    if (parent == node) {
	Tcl_AppendResult(interp, "can't move node to self", (char *)NULL);
	return TCL_ERROR;
    }
    data.node = NULL;
    data.cmdPtr = cmdPtr;
    data.movePos = -1;
    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, moveSwitches, objc - 4, objv + 4, 
	     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    /* Verify they aren't ancestors. */
    if (Blt_TreeIsAncestor(node, parent)) {
	Tcl_AppendResult(interp, "can't move node: \"", 
		 Tcl_GetString(objv[2]), (char *)NULL);
	Tcl_AppendResult(interp, "\" is an ancestor of \"", 
		 Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    before = NULL;		/* If before is NULL, this appends the
				 * node to the parent's child list.  */

    if (data.node != NULL) {	/* -before or -after */
	if (Blt_TreeNodeParent(data.node) != parent) {
	    Tcl_AppendResult(interp, Tcl_GetString(objv[2]), 
		     " isn't the parent of ", Blt_TreeNodeLabel(data.node),
		     (char *)NULL);
	    return TCL_ERROR;
	}
	if (Blt_SwitchChanged(moveSwitches, "-before", (char *)NULL)) {
	    before = data.node;
	    if (before == node) {
		Tcl_AppendResult(interp, "can't move node before itself", 
				 (char *)NULL);
		return TCL_ERROR;
	    }
	} else {
	    before = Blt_TreeNextSibling(data.node);
	    if (before == node) {
		Tcl_AppendResult(interp, "can't move node after itself", 
				 (char *)NULL);
		return TCL_ERROR;
	    }
	}
    } else if (data.movePos >= 0) { /* -at */
	int count;		/* Tracks the current list index. */
	Blt_TreeNode child;

	/* 
	 * If the node is in the list, ignore it when determining the
	 * "before" node using the -at index.  An index of -1 means to
	 * append the node to the list.
	 */
	count = 0;
	for(child = Blt_TreeFirstChild(parent); child != NULL; 
	    child = Blt_TreeNextSibling(child)) {
	    if (child == node) {
		continue;	/* Ignore the node to be moved. */
	    }
	    if (count == data.movePos) {
		before = child;
		break;		
	    }
	    count++;	
	}
    }
    if (Blt_TreeMoveNode(cmdPtr->tree, node, parent, before) != TCL_OK) {
	Tcl_AppendResult(interp, "can't move node ", Tcl_GetString(objv[2]), 
		 " to ", Tcl_GetString(objv[3]), (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NextOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NextOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreeNextNode(Blt_TreeRootNode(cmdPtr->tree), node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NextSiblingOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NextSiblingOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreeNextSibling(node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyCreateOp --
 *
 *	tree0 notify create ?flags? command arg
 *---------------------------------------------------------------------- 
 */
static int
NotifyCreateOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    NotifyInfo *notifyPtr;
    NotifyData data;
    char *string;
    char idString[200];
    int isNew, nArgs;
    Blt_HashEntry *hPtr;
    int count;
    register int i;

    count = 0;
    for (i = 3; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (string[0] != '-') {
	    break;
	}
	count++;
    }
    data.mask = 0;
    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, notifySwitches, count, objv + 3, 
	     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    notifyPtr = Blt_Malloc(sizeof(NotifyInfo));

    nArgs = objc - i;

    /* Stash away the command in structure and pass that to the notifier. */
    notifyPtr->objv = Blt_Malloc((nArgs + 2) * sizeof(Tcl_Obj *));
    for (count = 0; i < objc; i++, count++) {
	Tcl_IncrRefCount(objv[i]);
	notifyPtr->objv[count] = objv[i];
    }
    notifyPtr->objc = nArgs + 2;
    notifyPtr->cmdPtr = cmdPtr;
    if (data.mask == 0) {
	data.mask = TREE_NOTIFY_ALL;
    }
    notifyPtr->mask = data.mask;

    sprintf(idString, "notify%d", cmdPtr->notifyCounter++);
    hPtr = Blt_CreateHashEntry(&(cmdPtr->notifyTable), idString, &isNew);
    Blt_SetHashValue(hPtr, notifyPtr);

    Tcl_SetStringObj(Tcl_GetObjResult(interp), idString, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyDeleteOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
NotifyDeleteOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    NotifyInfo *notifyPtr;
    Blt_HashEntry *hPtr;
    register int i, j;
    char *string;

    for (i = 3; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	hPtr = Blt_FindHashEntry(&(cmdPtr->notifyTable), string);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "unknown notify name \"", string, "\"", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	notifyPtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashEntry(&(cmdPtr->notifyTable), hPtr);
	for (j = 0; j < (notifyPtr->objc - 2); j++) {
	    Tcl_DecrRefCount(notifyPtr->objv[j]);
	}
	Blt_Free(notifyPtr->objv);
	Blt_Free(notifyPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyInfoOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NotifyInfoOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    NotifyInfo *notifyPtr;
    Blt_HashEntry *hPtr;
    Tcl_DString dString;
    char *string;
    int i;

    string = Tcl_GetString(objv[3]);
    hPtr = Blt_FindHashEntry(&(cmdPtr->notifyTable), string);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown notify name \"", string, "\"", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    notifyPtr = Blt_GetHashValue(hPtr);

    Tcl_DStringInit(&dString);
    Tcl_DStringAppendElement(&dString, string);	/* Copy notify Id */
    Tcl_DStringStartSublist(&dString);
    if (notifyPtr->mask & TREE_NOTIFY_CREATE) {
	Tcl_DStringAppendElement(&dString, "-create");
    }
    if (notifyPtr->mask & TREE_NOTIFY_DELETE) {
	Tcl_DStringAppendElement(&dString, "-delete");
    }
    if (notifyPtr->mask & TREE_NOTIFY_MOVE) {
	Tcl_DStringAppendElement(&dString, "-move");
    }
    if (notifyPtr->mask & TREE_NOTIFY_SORT) {
	Tcl_DStringAppendElement(&dString, "-sort");
    }
    if (notifyPtr->mask & TREE_NOTIFY_RELABEL) {
	Tcl_DStringAppendElement(&dString, "-relabel");
    }
    if (notifyPtr->mask & TREE_NOTIFY_WHENIDLE) {
	Tcl_DStringAppendElement(&dString, "-whenidle");
    }
    Tcl_DStringEndSublist(&dString);
    Tcl_DStringStartSublist(&dString);
    for (i = 0; i < (notifyPtr->objc - 2); i++) {
	Tcl_DStringAppendElement(&dString, Tcl_GetString(notifyPtr->objv[i]));
    }
    Tcl_DStringEndSublist(&dString);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyNamesOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NotifyNamesOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *objPtr, *listObjPtr;
    char *notifyId;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->notifyTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	notifyId = Blt_GetHashKey(&(cmdPtr->notifyTable), hPtr);
	objPtr = Tcl_NewStringObj(notifyId, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyOp --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec notifyOps[] =
{
    {"create", 1, (Blt_Op)NotifyCreateOp, 4, 0, "?flags? command",},
    {"delete", 1, (Blt_Op)NotifyDeleteOp, 3, 0, "notifyId...",},
    {"info", 1, (Blt_Op)NotifyInfoOp, 4, 4, "notifyId",},
    {"names", 1, (Blt_Op)NotifyNamesOp, 3, 3, "",},
};

static int nNotifyOps = sizeof(notifyOps) / sizeof(Blt_OpSpec);

static int
NotifyOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nNotifyOps, notifyOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}


/*ARGSUSED*/
static int
ParentOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreeNodeParent(node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PathOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
PathOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    Tcl_DString dString;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    GetNodePath(cmdPtr, Blt_TreeRootNode(cmdPtr->tree), node, FALSE, &dString);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}


static int
ComparePositions(Blt_TreeNode *n1Ptr, Blt_TreeNode *n2Ptr)
{
    if (*n1Ptr == *n2Ptr) {
        return 0;
    }
    if (Blt_TreeIsBefore(*n1Ptr, *n2Ptr)) {
        return -1;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * PositionOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
PositionOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    PositionData data;
    Blt_TreeNode *nodeArr, *nodePtr;
    Blt_TreeNode node;
    Blt_TreeNode parent, lastParent;
    Tcl_Obj *listObjPtr, *objPtr;
    int i;
    int position;
    Tcl_DString dString;
    int n;

    memset((char *)&data, 0, sizeof(data));
    /* Process switches  */
    n = Blt_ProcessObjSwitches(interp, positionSwitches, objc - 2, objv + 2, 
	     (char *)&data, BLT_SWITCH_OBJV_PARTIAL);
    if (n < 0) {
	return TCL_ERROR;
    }
    objc -= n + 2, objv += n + 2;

    /* Collect the node ids into an array */
    nodeArr = Blt_Malloc((objc + 1) * sizeof(Blt_TreeNode));
    for (i = 0; i < objc; i++) {
	if (GetNode(cmdPtr, objv[i], &node) != TCL_OK) {
	    Blt_Free(nodeArr);
	    return TCL_ERROR;
	}
	nodeArr[i] = node;
    }
    nodeArr[i] = NULL;

    if (data.sort) {		/* Sort the nodes by depth-first order 
				 * if requested. */
	qsort((char *)nodeArr, objc, sizeof(Blt_TreeNode), 
	      (QSortCompareProc *)ComparePositions);
    }

    position = 0;		/* Suppress compiler warning. */
    lastParent = NULL;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    Tcl_DStringInit(&dString);
    for (nodePtr = nodeArr; *nodePtr != NULL; nodePtr++) {
	parent = Blt_TreeNodeParent(*nodePtr);
	if ((parent != NULL) && (parent == lastParent)) {
	    /* 
	     * Since we've sorted the nodes already, we can safely
	     * assume that if two consecutive nodes have the same
	     * parent, the first node came before the second. If
	     * this is the case, use the last node as a starting
	     * point.  
	     */
	    
	    /*
	     * Note that we start comparing from the last node,
	     * not its successor.  Some one may give us the same
	     * node more than once.  
	     */
	    node = *(nodePtr - 1); /* Can't get here unless there's
				    * more than one node. */
	    for(/*empty*/; node != NULL; node = Blt_TreeNextSibling(node)) {
		if (node == *nodePtr) {
		    break;
		}
		position++;
	    }
	} else {
	    /* The fallback is to linearly search through the
	     * parent's list of children, counting the number of
	     * preceding siblings. Except for nodes with many
	     * siblings (100+), this should be okay. */
	    position = Blt_TreeNodePosition(*nodePtr);
	}
	if (data.sort) {
	    lastParent = parent; /* Update the last parent. */
	}	    
	/* 
	 * Add an element in the form "parent -at position" to the
	 * list that we're generating.
	 */
	if (data.withId) {
	    objPtr = Tcl_NewIntObj(Blt_TreeNodeId(*nodePtr));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	if (data.withParent) {
	    char *string;

	    Tcl_DStringSetLength(&dString, 0); /* Clear the string. */
	    string = (parent == NULL) ? "" : Blt_Itoa(Blt_TreeNodeId(parent));
	    Tcl_DStringAppendElement(&dString, string);
	    Tcl_DStringAppendElement(&dString, "-at");
	    Tcl_DStringAppendElement(&dString, Blt_Itoa(position));
	    objPtr = Tcl_NewStringObj(Tcl_DStringValue(&dString), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	} else {
	    objPtr = Tcl_NewIntObj(position);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_DStringFree(&dString);
    Blt_Free(nodeArr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PreviousOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
PreviousOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreePrevNode(Blt_TreeRootNode(cmdPtr->tree), node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}

/*ARGSUSED*/
static int
PrevSiblingOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    int inode;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    inode = -1;
    node = Blt_TreePrevSibling(node);
    if (node != NULL) {
	inode = Blt_TreeNodeId(node);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), inode);
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * RestoreOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
RestoreOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode root;
    RestoreData data;
    Tcl_DString dString;
    char *entry, *eol, *next;
    char saved;
    int result;

    if (GetNode(cmdPtr, objv[2], &root) != TCL_OK) {
	return TCL_ERROR;
    }
    memset((char *)&data, 0, sizeof(data));
    Blt_InitHashTable(&data.idTable, BLT_ONE_WORD_KEYS);
    data.root = root;

    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, restoreSwitches, objc - 4, objv + 4, 
	     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    result = TCL_OK;
    nLines = 0;
    Tcl_DStringInit(&dString);
    entry = eol = Tcl_GetString(objv[3]);
    next = entry;
    while (*eol != '\0') {
	/* Find the next end of line. */
	for (eol = next; (*eol != '\n') && (*eol != '\0'); eol++) {
	    /*empty*/
	}
	/* 
	 * Since we don't own the string (the Tcl_Obj could be shared),
	 * save the current end-of-line character (it's either a NUL
	 * or NL) so we can NUL-terminate the line for the call to
	 * Tcl_SplitList and repair it when we're done.
	 */
	saved = *eol;
	*eol = '\0';
	next = eol + 1;
	nLines++;
	if (Tcl_CommandComplete(entry)) {
	    char **elemArr;
	    int nElem;
	    
	    if (Tcl_SplitList(interp, entry, &nElem, &elemArr) != TCL_OK) {
		*eol = saved;
		return TCL_ERROR;
	    }
	    if (nElem > 0) {
		result = RestoreNode(cmdPtr, nElem, elemArr, &data);
		Blt_Free(elemArr);
		if (result != TCL_OK) {
		    *eol = saved;
		    break;
		}
	    }
	    entry = next;
	}
	*eol = saved;
    }
    Blt_DeleteHashTable(&data.idTable);
    return result;
}

static int
ReadEntry(
    Tcl_Interp *interp,
    Tcl_Channel channel,
    int *argcPtr,
    char ***argvPtr)
{
    Tcl_DString dString;
    int result;
    char *entry;

    if (*argvPtr != NULL) {
	Blt_Free(*argvPtr);
	*argvPtr = NULL;
    }
    Tcl_DStringInit(&dString);
    entry = NULL;
    while (Tcl_Gets(channel, &dString) >= 0) {
	nLines++;
	Tcl_DStringAppend(&dString, "\n", 1);
	entry = Tcl_DStringValue(&dString);
	if (Tcl_CommandComplete(entry)) {
	    result = Tcl_SplitList(interp, entry, argcPtr, argvPtr);
	    Tcl_DStringFree(&dString);
	    return result;
	}
    }
    Tcl_DStringFree(&dString);
    if (entry == NULL) {
	*argcPtr = 0;		/* EOF */
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "error reading file: ", 
		     Tcl_PosixError(interp), (char *)NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RestorefileOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
RestorefileOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode root;
    int nElem;
    char **elemArr;
    char *fileName;
    int result;
    Tcl_Channel channel;
    RestoreData data;

    if (GetNode(cmdPtr, objv[2], &root) != TCL_OK) {
	return TCL_ERROR;
    }
    fileName = Tcl_GetString(objv[3]);
    channel = Tcl_OpenFileChannel(interp, fileName, "r", 0);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    memset((char *)&data, 0, sizeof(data));
    Blt_InitHashTable(&data.idTable, BLT_ONE_WORD_KEYS);
    data.root = root;

    /* Process switches  */
    if (Blt_ProcessObjSwitches(interp, restoreSwitches, objc - 4, objv + 4, 
	     (char *)&data, 0) < 0) {
	Tcl_Close(interp, channel);
	return TCL_ERROR;
    }
    elemArr = NULL;
    nLines = 0;
    for (;;) {
	result = ReadEntry(interp, channel, &nElem, &elemArr);
	if ((result != TCL_OK) || (nElem == 0)) {
	    break;
	}
	result = RestoreNode(cmdPtr, nElem, elemArr, &data);
	if (result != TCL_OK) {
	    break;
	}
    } 
    if (elemArr != NULL) {
	Blt_Free(elemArr);
    }
    Tcl_Close(interp, channel);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RootOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
RootOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode root;

    if (objc == 3) {
	Blt_TreeNode node;

	if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_TreeChangeRoot(cmdPtr->tree, node);
    }
    root = Blt_TreeRootNode(cmdPtr->tree);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_TreeNodeId(root));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
SetOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    char *string;
    TagSearch cursor;
	
    string = Tcl_GetString(objv[2]);
    if (isdigit(UCHAR(*string))) {
	if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (SetValues(cmdPtr, node, objc - 3, objv + 3) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	node = FirstTaggedNode(interp, cmdPtr, objv[2], &cursor);
	if (node == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; node != NULL; node = NextTaggedNode(node, &cursor)) {
	    if (SetValues(cmdPtr, node, objc - 3, objv + 3) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SizeOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
SizeOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_TreeSize(node));
    return TCL_OK;
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
TagForgetOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    register int i;

    for (i = 3; i < objc; i++) {
	Blt_TreeForgetTag(cmdPtr->tree, Tcl_GetString(objv[i]));
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
TagNamesOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_HashSearch cursor;
    Blt_TreeTagEntry *tPtr;
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    objPtr = Tcl_NewStringObj("all", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (objc == 3) {
	Blt_HashEntry *hPtr;

	objPtr = Tcl_NewStringObj("root", -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	for (hPtr = Blt_TreeFirstTag(cmdPtr->tree, &cursor); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    objPtr = Tcl_NewStringObj(tPtr->tagName, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	register int i;
	Blt_TreeNode node;
	Blt_HashEntry *hPtr, *h2Ptr;
	Blt_HashTable uniqTable;
	int isNew;

	Blt_InitHashTable(&uniqTable, BLT_STRING_KEYS);
	for (i = 3; i < objc; i++) {
	    if (GetNode(cmdPtr, objv[i], &node) != TCL_OK) {
		goto error;
	    }
	    if (node == Blt_TreeRootNode(cmdPtr->tree)) {
		Blt_CreateHashEntry(&uniqTable, "root", &isNew);
	    }
	    for (hPtr = Blt_TreeFirstTag(cmdPtr->tree, &cursor); hPtr != NULL;
		 hPtr = Blt_NextHashEntry(&cursor)) {
		tPtr = Blt_GetHashValue(hPtr);
		h2Ptr = Blt_FindHashEntry(&tPtr->nodeTable, (char *)node);
		if (h2Ptr != NULL) {
		    Blt_CreateHashEntry(&uniqTable, tPtr->tagName, &isNew);
		}
	    }
	}
	for (hPtr = Blt_FirstHashEntry(&uniqTable, &cursor); hPtr != NULL;
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    objPtr = Tcl_NewStringObj(Blt_GetHashKey(&uniqTable, hPtr), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Blt_DeleteHashTable(&uniqTable);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
 error:
    Tcl_DecrRefCount(listObjPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TagNodesOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagNodesOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable nodeTable;
    Blt_TreeNode node;
    Tcl_Obj *listObjPtr;
    Tcl_Obj *objPtr;
    char *string;
    int isNew;
    register int i;
	
    Blt_InitHashTable(&nodeTable, BLT_ONE_WORD_KEYS);
    for (i = 3; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (strcmp(string, "all") == 0) {
	    break;
	} else if (strcmp(string, "root") == 0) {
	    Blt_CreateHashEntry(&nodeTable, 
		(char *)Blt_TreeRootNode(cmdPtr->tree), &isNew);
	    continue;
	} else {
	    Blt_HashTable *tablePtr;
	    
	    tablePtr = Blt_TreeTagHashTable(cmdPtr->tree, string);
	    if (tablePtr != NULL) {
		for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); 
		     hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		    node = Blt_GetHashValue(hPtr);
		    Blt_CreateHashEntry(&nodeTable, (char *)node, &isNew);
		}
		continue;
	    }
	}
	Tcl_AppendResult(interp, "can't find a tag \"", string, "\"",
			 (char *)NULL);
	goto error;
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

 error:
    Blt_DeleteHashTable(&nodeTable);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TagAddOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagAddOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    register int i;
    char *string;
    TagSearch cursor;

    string = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(string[0]))) {
	Tcl_AppendResult(interp, "bad tag \"", string, 
		 "\": can't start with a digit", (char *)NULL);
	return TCL_ERROR;
    }
    if ((strcmp(string, "all") == 0) || (strcmp(string, "root") == 0)) {
	Tcl_AppendResult(cmdPtr->interp, "can't add reserved tag \"",
			 string, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
	node = FirstTaggedNode(interp, cmdPtr, objv[i], &cursor);
	if (node == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; node != NULL; node = NextTaggedNode(node, &cursor)) {
	    if (AddTag(cmdPtr, node, string) != TCL_OK) {
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
static int
TagDeleteOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    char *string;
    Blt_HashTable *tablePtr;

    string = Tcl_GetString(objv[3]);
    if ((strcmp(string, "all") == 0) || (strcmp(string, "root") == 0)) {
	Tcl_AppendResult(interp, "can't delete reserved tag \"", string, "\"", 
			 (char *)NULL);
        return TCL_ERROR;
    }
    tablePtr = Blt_TreeTagHashTable(cmdPtr->tree, string);
    if (tablePtr != NULL) {
        register int i;
        Blt_TreeNode node;
        TagSearch cursor;
        Blt_HashEntry *hPtr;
      
        for (i = 4; i < objc; i++) {
	    node = FirstTaggedNode(interp, cmdPtr, objv[i], &cursor);
	    if (node == NULL) {
	        return TCL_ERROR;
	    }
	    for (/* empty */; node != NULL; 	
		node = NextTaggedNode(node, &cursor)) {
	        hPtr = Blt_FindHashEntry(tablePtr, (char *)node);
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
 * TagDumpOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TagDumpOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    register Blt_TreeNode root, node;
    Tcl_DString dString;
    TagSearch cursor;
    register int i;

    Tcl_DStringInit(&dString);
    root = Blt_TreeRootNode(cmdPtr->tree);
    for (i = 3; i < objc; i++) {
	node = FirstTaggedNode(interp, cmdPtr, objv[i], &cursor);
	if (node == NULL) {
	    Tcl_DStringFree(&dString);
	    return TCL_ERROR;
	}
	for (/* empty */; node != NULL; node = NextTaggedNode(node, &cursor)) {
	    PrintNode(cmdPtr, root, node, &dString);
	}
    }
    Tcl_DStringResult(interp, &dString);
    Tcl_DStringFree(&dString);
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
    {"add", 1, (Blt_Op)TagAddOp, 5, 0, "tag node...",},
    {"delete", 2, (Blt_Op)TagDeleteOp, 5, 0, "tag node...",},
    {"dump", 2, (Blt_Op)TagDumpOp, 4, 0, "tag...",},
    {"forget", 1, (Blt_Op)TagForgetOp, 4, 0, "tag...",},
    {"names", 2, (Blt_Op)TagNamesOp, 3, 0, "?node...?",},
    {"nodes", 2, (Blt_Op)TagNodesOp, 4, 0, "tag ?tag...?",},
};

static int nTagOps = sizeof(tagOps) / sizeof(Blt_OpSpec);

static int
TagOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nTagOps, tagOps, BLT_OP_ARG2, objc, objv, 
	0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceCreateOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceCreateOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_TreeNode node;
    TraceInfo *tracePtr;
    char *string, *key, *command;
    char *tagName;
    char idString[200];
    int flags, isNew;
    int length;

    string = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(*string))) {
	if (GetNode(cmdPtr, objv[3], &node) != TCL_OK) {
	    return TCL_ERROR;
	}
	tagName = NULL;
    } else {
	tagName = Blt_Strdup(string);
	node = NULL;
    }
    key = Tcl_GetString(objv[4]);
    string = Tcl_GetString(objv[5]);
    flags = GetTraceFlags(string);
    if (flags < 0) {
	Tcl_AppendResult(interp, "unknown flag in \"", string, "\"", 
		     (char *)NULL);
	return TCL_ERROR;
    }
    command = Tcl_GetStringFromObj(objv[6], &length);
    /* Stash away the command in structure and pass that to the trace. */
    tracePtr = Blt_Malloc(length + sizeof(TraceInfo));
    strcpy(tracePtr->command, command);
    tracePtr->cmdPtr = cmdPtr;
    tracePtr->withTag = tagName;
    tracePtr->node = node;
    tracePtr->traceToken = Blt_TreeCreateTrace(cmdPtr->tree, node, key, tagName,
	flags, TreeTraceProc, tracePtr);

    sprintf(idString, "trace%d", cmdPtr->traceCounter++);
    hPtr = Blt_CreateHashEntry(&(cmdPtr->traceTable), idString, &isNew);
    Blt_SetHashValue(hPtr, tracePtr);

    Tcl_SetStringObj(Tcl_GetObjResult(interp), idString, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceDeleteOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
TraceDeleteOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TraceInfo *tracePtr;
    Blt_HashEntry *hPtr;
    register int i;
    char *key;

    for (i = 3; i < objc; i++) {
	key = Tcl_GetString(objv[i]);
	hPtr = Blt_FindHashEntry(&(cmdPtr->traceTable), key);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "unknown trace \"", key, "\"", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	tracePtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashEntry(&(cmdPtr->traceTable), hPtr); 
	Blt_TreeDeleteTrace(tracePtr->traceToken);
	if (tracePtr->withTag != NULL) {
	    Blt_Free(tracePtr->withTag);
	}
	Blt_Free(tracePtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceNamesOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceNamesOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->traceTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Tcl_AppendElement(interp, Blt_GetHashKey(&(cmdPtr->traceTable), hPtr));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceInfoOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceInfoOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    TraceInfo *tracePtr;
    struct Blt_TreeTraceStruct *tokenPtr;
    Blt_HashEntry *hPtr;
    Tcl_DString dString;
    char string[5];
    char *key;

    key = Tcl_GetString(objv[3]);
    hPtr = Blt_FindHashEntry(&(cmdPtr->traceTable), key);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown trace \"", key, "\"", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    tracePtr = Blt_GetHashValue(hPtr);
    if (tracePtr->withTag != NULL) {
	Tcl_DStringAppendElement(&dString, tracePtr->withTag);
    } else {
	int inode;

	inode = Blt_TreeNodeId(tracePtr->node);
	Tcl_DStringAppendElement(&dString, Blt_Itoa(inode));
    }
    tokenPtr = (struct Blt_TreeTraceStruct *)tracePtr->traceToken;
    Tcl_DStringAppendElement(&dString, tokenPtr->key);
    PrintTraceFlags(tokenPtr->mask, string);
    Tcl_DStringAppendElement(&dString, string);
    Tcl_DStringAppendElement(&dString, tracePtr->command);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceOp --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec traceOps[] =
{
    {"create", 1, (Blt_Op)TraceCreateOp, 7, 7, "node key how command",},
    {"delete", 1, (Blt_Op)TraceDeleteOp, 3, 0, "id...",},
    {"info", 1, (Blt_Op)TraceInfoOp, 4, 4, "id",},
    {"names", 1, (Blt_Op)TraceNamesOp, 3, 3, "",},
};

static int nTraceOps = sizeof(traceOps) / sizeof(Blt_OpSpec);

static int
TraceOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nTraceOps, traceOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TypeOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    Tcl_Obj *valueObjPtr;
    char *string;

    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }

    string = Tcl_GetString(objv[3]);
    if (Blt_TreeGetValue(interp, cmdPtr->tree, node, string, &valueObjPtr) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (valueObjPtr->typePtr != NULL) {
	Tcl_SetResult(interp, valueObjPtr->typePtr->name, TCL_VOLATILE);
    } else {
	Tcl_SetResult(interp, "string", TCL_STATIC);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * UnsetOp --
 *
 *---------------------------------------------------------------------- 
 */
static int
UnsetOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    char *string;
	
    string = Tcl_GetString(objv[2]);
    if (isdigit(UCHAR(*string))) {
	if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (UnsetValues(cmdPtr, node, objc - 3, objv + 3) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	TagSearch cursor;

	node = FirstTaggedNode(interp, cmdPtr, objv[2], &cursor);
	if (node == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; node != NULL; node = NextTaggedNode(node, &cursor)) {
	    if (UnsetValues(cmdPtr, node, objc - 3, objv + 3) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


typedef struct {
    TreeCmd *cmdPtr;
    unsigned int flags;
    int type;
    int mode;
    char *key;
    char *command;
} SortData;

#define SORT_RECURSE		(1<<2)
#define SORT_DECREASING		(1<<3)
#define SORT_PATHNAME		(1<<4)

enum SortTypes { SORT_DICTIONARY, SORT_REAL, SORT_INTEGER, SORT_ASCII, 
	SORT_COMMAND };

enum SortModes { SORT_FLAT, SORT_REORDER };

static Blt_SwitchSpec sortSwitches[] = 
{
    {BLT_SWITCH_VALUE, "-ascii", Blt_Offset(SortData, type), 0, 0, 
	SORT_ASCII},
    {BLT_SWITCH_STRING, "-command", Blt_Offset(SortData, command), 0},
    {BLT_SWITCH_FLAG, "-decreasing", Blt_Offset(SortData, flags), 0, 0, 
	SORT_DECREASING},
    {BLT_SWITCH_VALUE, "-dictionary", Blt_Offset(SortData, type), 0, 0, 
	SORT_DICTIONARY},
    {BLT_SWITCH_VALUE, "-integer", Blt_Offset(SortData, type), 0, 0, 
	SORT_INTEGER},
    {BLT_SWITCH_STRING, "-key", Blt_Offset(SortData, key), 0},
    {BLT_SWITCH_FLAG, "-path", Blt_Offset(SortData, flags), 0, 0, 
	SORT_PATHNAME},
    {BLT_SWITCH_VALUE, "-real", Blt_Offset(SortData, type), 0, 0, 
	SORT_REAL},
    {BLT_SWITCH_VALUE, "-recurse", Blt_Offset(SortData, flags), 0, 0, 
	SORT_RECURSE},
    {BLT_SWITCH_VALUE, "-reorder", Blt_Offset(SortData, mode), 0, 0, 
	SORT_REORDER},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static SortData sortData;

static int
CompareNodes(Blt_TreeNode *n1Ptr, Blt_TreeNode *n2Ptr)
{
    TreeCmd *cmdPtr = sortData.cmdPtr;
    char *s1, *s2;
    int result;
    Tcl_DString dString1, dString2;

    s1 = s2 = "";
    result = 0;

    if (sortData.flags & SORT_PATHNAME) {
	Tcl_DStringInit(&dString1);
	Tcl_DStringInit(&dString2);
    }
    if (sortData.key != NULL) {
	Tcl_Obj *valueObjPtr;

	if (Blt_TreeGetValue((Tcl_Interp *)NULL, cmdPtr->tree, *n1Ptr, 
	     sortData.key, &valueObjPtr) == TCL_OK) {
	    s1 = Tcl_GetString(valueObjPtr);
	}
	if (Blt_TreeGetValue((Tcl_Interp *)NULL, cmdPtr->tree, *n2Ptr, 
	     sortData.key, &valueObjPtr) == TCL_OK) {
	    s2 = Tcl_GetString(valueObjPtr);
	}
    } else if (sortData.flags & SORT_PATHNAME)  {
	Blt_TreeNode root;
	
	root = Blt_TreeRootNode(cmdPtr->tree);
	s1 = GetNodePath(cmdPtr, root, *n1Ptr, FALSE, &dString1);
	s2 = GetNodePath(cmdPtr, root, *n2Ptr, FALSE, &dString2);
    } else {
	s1 = Blt_TreeNodeLabel(*n1Ptr);
	s2 = Blt_TreeNodeLabel(*n2Ptr);
    }
    switch (sortData.type) {
    case SORT_ASCII:
	result = strcmp(s1, s2);
	break;

    case SORT_COMMAND:
	if (sortData.command == NULL) {
	    result = Blt_DictionaryCompare(s1, s2);
	} else {
	    Tcl_DString dsCmd, dsName;
	    char *qualName;

	    result = 0;	/* Hopefully this will be Ok even if the
			 * Tcl command fails to return the correct
			 * result. */
	    Tcl_DStringInit(&dsCmd);
	    Tcl_DStringAppend(&dsCmd, sortData.command, -1);
	    Tcl_DStringInit(&dsName);
	    qualName = Blt_GetQualifiedName(
		Blt_GetCommandNamespace(cmdPtr->interp, cmdPtr->cmdToken), 
		Tcl_GetCommandName(cmdPtr->interp, cmdPtr->cmdToken), &dsName);
	    Tcl_DStringAppendElement(&dsCmd, qualName);
	    Tcl_DStringFree(&dsName);
	    Tcl_DStringAppendElement(&dsCmd, Blt_Itoa(Blt_TreeNodeId(*n1Ptr)));
	    Tcl_DStringAppendElement(&dsCmd, Blt_Itoa(Blt_TreeNodeId(*n2Ptr)));
	    Tcl_DStringAppendElement(&dsCmd, s1);
	    Tcl_DStringAppendElement(&dsCmd, s2);
	    result = Tcl_GlobalEval(cmdPtr->interp, Tcl_DStringValue(&dsCmd));
	    Tcl_DStringFree(&dsCmd);
	    
	    if ((result != TCL_OK) ||
		(Tcl_GetInt(cmdPtr->interp, 
		    Tcl_GetStringResult(cmdPtr->interp), &result) != TCL_OK)) {
		Tcl_BackgroundError(cmdPtr->interp);
	    }
	    Tcl_ResetResult(cmdPtr->interp);
	}
	break;

    case SORT_DICTIONARY:
	result = Blt_DictionaryCompare(s1, s2);
	break;

    case SORT_INTEGER:
	{
	    int i1, i2;

	    if (Tcl_GetInt(NULL, s1, &i1) == TCL_OK) {
		if (Tcl_GetInt(NULL, s2, &i2) == TCL_OK) {
		    result = i1 - i2;
		} else {
		    result = -1;
		} 
	    } else if (Tcl_GetInt(NULL, s2, &i2) == TCL_OK) {
		result = 1;
	    } else {
		result = Blt_DictionaryCompare(s1, s2);
	    }
	}
	break;

    case SORT_REAL:
	{
	    double r1, r2;

	    if (Tcl_GetDouble(NULL, s1, &r1) == TCL_OK) {
		if (Tcl_GetDouble(NULL, s2, &r2) == TCL_OK) {
		    result = (r1 < r2) ? -1 : (r1 > r2) ? 1 : 0;
		} else {
		    result = -1;
		} 
	    } else if (Tcl_GetDouble(NULL, s2, &r2) == TCL_OK) {
		result = 1;
	    } else {
		result = Blt_DictionaryCompare(s1, s2);
	    }
	}
	break;
    }
    if (result == 0) {
	result = Blt_TreeNodeId(*n1Ptr) - Blt_TreeNodeId(*n2Ptr);
    }
    if (sortData.flags & SORT_DECREASING) {
	result = -result;
    } 
    if (sortData.flags & SORT_PATHNAME) {
	Tcl_DStringFree(&dString1);
	Tcl_DStringFree(&dString2);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SortApplyProc --
 *
 *	Sorts the subnodes at a given node.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SortApplyProc(
    Blt_TreeNode node,
    ClientData clientData,
    int order)			/* Not used. */
{
    TreeCmd *cmdPtr = clientData;

    if (!Blt_TreeIsLeaf(node)) {
	Blt_TreeSortNode(cmdPtr->tree, node, CompareNodes);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SortOp --
 *  
 *---------------------------------------------------------------------- 
 */
static int
SortOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode top;
    SortData data;
    int result;

    if (GetNode(cmdPtr, objv[2], &top) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Process switches  */
    memset(&data, 0, sizeof(data));
    data.cmdPtr = cmdPtr;
    if (Blt_ProcessObjSwitches(interp, sortSwitches, objc - 3, objv + 3, 
	     (char *)&data, 0) < 0) {
	return TCL_ERROR;
    }
    if (data.command != NULL) {
	data.type = SORT_COMMAND;
    }
    data.cmdPtr = cmdPtr;
    sortData = data;
    if (data.mode == SORT_FLAT) {
	Blt_TreeNode *p, *nodeArr, node;
	int nNodes;
	Tcl_Obj *objPtr, *listObjPtr;
	int i;

	if (data.flags & SORT_RECURSE) {
	    nNodes = Blt_TreeSize(top);
	} else {
	    nNodes = Blt_TreeNodeDegree(top);
	}
	nodeArr = Blt_Malloc(nNodes * sizeof(Blt_TreeNode));
	assert(nodeArr);
	p = nodeArr;
	if (data.flags & SORT_RECURSE) {
	    for(node = top; node != NULL; node = Blt_TreeNextNode(top, node)) {
		*p++ = node;
	    }
	} else {
	    for (node = Blt_TreeFirstChild(top); node != NULL;
		 node = Blt_TreeNextSibling(node)) {
		*p++ = node;
	    }
	}
	qsort((char *)nodeArr, nNodes, sizeof(Blt_TreeNode),
	      (QSortCompareProc *)CompareNodes);
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (p = nodeArr, i = 0; i < nNodes; i++, p++) {
	    objPtr = Tcl_NewIntObj(Blt_TreeNodeId(*p));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
	Blt_Free(nodeArr);
	result = TCL_OK;
    } else {
	if (data.flags & SORT_RECURSE) {
	    result = Blt_TreeApply(top, SortApplyProc, cmdPtr);
	} else {
	    result = SortApplyProc(top, cmdPtr, TREE_PREORDER);
	}
    }
    Blt_FreeSwitches(sortSwitches, (char *)&data, 0);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ValuesOp --
 *
 *	Returns the names of values for a node or array value.
 *
 *---------------------------------------------------------------------- 
 */
static int
ValuesOp(
    TreeCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_TreeNode node;
    Tcl_Obj *listObjPtr;
    
    if (GetNode(cmdPtr, objv[2], &node) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (objc == 4) { 
	char *string;

	string = Tcl_GetString(objv[3]);
	if (Blt_TreeArrayNames(interp, cmdPtr->tree, node, string, listObjPtr)
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Blt_TreeKey key;
	Tcl_Obj *objPtr;
	Blt_TreeKeySearch keyIter;

	for (key = Blt_TreeFirstKey(cmdPtr->tree, node, &keyIter); key != NULL; 
	     key = Blt_TreeNextKey(cmdPtr->tree, &keyIter)) {
	    objPtr = Tcl_NewStringObj(key, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}	    
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


/*
 * --------------------------------------------------------------
 *
 * TreeInstObjCmd --
 *
 * 	This procedure is invoked to process commands on behalf of
 *	the tree object.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */
static Blt_OpSpec treeOps[] =
{
    {"ancestor", 2, (Blt_Op)AncestorOp, 4, 4, "node1 node2",},
    {"apply", 1, (Blt_Op)ApplyOp, 3, 0, "node ?switches?",},
    {"attach", 2, (Blt_Op)AttachOp, 2, 3, "?tree?",},
    {"children", 2, (Blt_Op)ChildrenOp, 3, 5, "node ?first? ?last?",},
    {"copy", 2, (Blt_Op)CopyOp, 4, 0, 
	"srcNode ?destTree? destNode ?switches?",},
    {"degree", 2, (Blt_Op)DegreeOp, 3, 0, "node",},
    {"delete", 2, (Blt_Op)DeleteOp, 3, 0, "node ?node...?",},
    {"depth", 3, (Blt_Op)DepthOp, 3, 3, "node",},
    {"dump", 4, (Blt_Op)DumpOp, 3, 3, "node",},
    {"dumpfile", 5, (Blt_Op)DumpfileOp, 4, 4, "node fileName",},
    {"exists", 1, (Blt_Op)ExistsOp, 3, 4, "node ?key?",},
    {"find", 4, (Blt_Op)FindOp, 3, 0, "node ?switches?",},
    {"findchild", 5, (Blt_Op)FindChildOp, 4, 4, "node name",},
    {"firstchild", 3, (Blt_Op)FirstChildOp, 3, 3, "node",},
    {"get", 1, (Blt_Op)GetOp, 3, 5, "node ?key? ?defaultValue?",},
    {"index", 3, (Blt_Op)IndexOp, 3, 3, "name",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0, "parent ?switches?",},
    {"is", 2, (Blt_Op)IsOp, 2, 0, "oper args...",},
    {"keys", 1, (Blt_Op)KeysOp, 3, 0, "node ?node...?",},
    {"label", 3, (Blt_Op)LabelOp, 3, 4, "node ?newLabel?",},
    {"lastchild", 3, (Blt_Op)LastChildOp, 3, 3, "node",},
    {"move", 1, (Blt_Op)MoveOp, 4, 0, "node newParent ?switches?",},
    {"next", 4, (Blt_Op)NextOp, 3, 3, "node",},
    {"nextsibling", 5, (Blt_Op)NextSiblingOp, 3, 3, "node",},
    {"notify", 2, (Blt_Op)NotifyOp, 2, 0, "args...",},
    {"parent", 3, (Blt_Op)ParentOp, 3, 3, "node",},
    {"path", 3, (Blt_Op)PathOp, 3, 3, "node",},
    {"position", 2, (Blt_Op)PositionOp, 3, 0, "?switches? node...",},
    {"previous", 5, (Blt_Op)PreviousOp, 3, 3, "node",},
    {"prevsibling", 5, (Blt_Op)PrevSiblingOp, 3, 3, "node",},
    {"restore", 7, (Blt_Op)RestoreOp, 4, 4, "node dataString",},
    {"restorefile", 8, (Blt_Op)RestorefileOp, 4, 4, "node fileName",},
    {"root", 2, (Blt_Op)RootOp, 2, 3, "?node?",},
    {"set", 3, (Blt_Op)SetOp, 3, 0, "node ?key value...?",},
    {"size", 2, (Blt_Op)SizeOp, 3, 3, "node",},
    {"sort", 2, (Blt_Op)SortOp, 3, 0, "node ?flags...?",},
    {"tag", 2, (Blt_Op)TagOp, 3, 0, "args...",},
    {"trace", 2, (Blt_Op)TraceOp, 2, 0, "args...",},
    {"type", 2, (Blt_Op)TypeOp, 4, 4, "node key",},
    {"unset", 3, (Blt_Op)UnsetOp, 3, 0, "node ?key...?",},
    {"values", 1, (Blt_Op)ValuesOp, 3, 4, "node ?key?",},
};

static int nTreeOps = sizeof(treeOps) / sizeof(Blt_OpSpec);

static int
TreeInstObjCmd(
    ClientData clientData,	/* Information about the widget. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST *objv)	/* Vector of argument strings. */
{
    Blt_Op proc;
    TreeCmd *cmdPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nTreeOps, treeOps, BLT_OP_ARG1, objc, objv,
 	BLT_OP_LINEAR_SEARCH);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(cmdPtr);
    result = (*proc) (cmdPtr, interp, objc, objv);
    Tcl_Release(cmdPtr);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TreeInstDeleteProc --
 *
 *	Deletes the command associated with the tree.  This is
 *	called only when the command associated with the tree is
 *	destroyed.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
TreeInstDeleteProc(ClientData clientData)
{
    TreeCmd *cmdPtr = clientData;

    ReleaseTreeObject(cmdPtr);
    if (cmdPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(cmdPtr->tablePtr, cmdPtr->hashPtr);
    }
    Blt_DeleteHashTable(&(cmdPtr->traceTable));
    Blt_Free(cmdPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * GenerateName --
 *
 *	Generates an unique tree command name.  Tree names are in
 *	the form "treeN", where N is a non-negative integer. Check 
 *	each name generated to see if it is already a tree. We want
 *	to recycle names if possible.
 *	
 * Results:
 *	Returns the unique name.  The string itself is stored in the
 *	dynamic string passed into the routine.
 *
 * ----------------------------------------------------------------------
 */
static CONST char *
GenerateName(
    Tcl_Interp *interp,
    CONST char *prefix, 
    CONST char *suffix,
    Tcl_DString *resultPtr)
{

    int n;
    Tcl_Namespace *nsPtr;
    char string[200];
    Tcl_CmdInfo cmdInfo;
    Tcl_DString dString;
    CONST char *treeName, *name;

    /* 
     * Parse the command and put back so that it's in a consistent
     * format.  
     *
     *	t1         <current namespace>::t1
     *	n1::t1     <current namespace>::n1::t1
     *	::t1	   ::t1
     *  ::n1::t1   ::n1::t1
     */
    treeName = NULL;		/* Suppress compiler warning. */
    for (n = 0; n < INT_MAX; n++) {
	Tcl_DStringInit(&dString);
	Tcl_DStringAppend(&dString, prefix, -1);
	sprintf(string, "tree%d", n);
	Tcl_DStringAppend(&dString, string, -1);
	Tcl_DStringAppend(&dString, suffix, -1);
	treeName = Tcl_DStringValue(&dString);
	if (Blt_ParseQualifiedName(interp, treeName, &nsPtr, &name) != TCL_OK) {
	    Tcl_AppendResult(interp, "can't find namespace in \"", treeName, 
		"\"", (char *)NULL);
	    return NULL;
	}
	if (nsPtr == NULL) {
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	}
	treeName = Blt_GetQualifiedName(nsPtr, name, resultPtr);
	/* 
	 * Check if the command already exists. 
	 */
	if (Tcl_GetCommandInfo(interp, (char *)treeName, &cmdInfo)) {
	    continue;
	}
	if (!Blt_TreeExists(interp, treeName)) {
	    /* 
	     * We want the name of the tree command and the underlying
	     * tree object to be the same. Check that the free command
	     * name isn't an already a tree object name.  
	     */
	    break;
	}
    }
    return treeName;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCreateOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TreeCreateOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TreeCmdInterpData *dataPtr = clientData;
    CONST char *treeName;
    Tcl_DString dString;
    Blt_Tree token;

    treeName = NULL;
    if (objc == 3) {
	treeName = Tcl_GetString(objv[2]);
    }
    Tcl_DStringInit(&dString);
    if (treeName == NULL) {
	treeName = GenerateName(interp, "", "", &dString);
    } else {
	char *p;

	p = strstr(treeName, "#auto");
	if (p != NULL) {
	    *p = '\0';
	    treeName = GenerateName(interp, treeName, p + 5, &dString);
	    *p = '#';
	} else {
	    CONST char *name;
	    Tcl_CmdInfo cmdInfo;
	    Tcl_Namespace *nsPtr;

	    nsPtr = NULL;
	    /* 
	     * Parse the command and put back so that it's in a consistent
	     * format.  
	     *
	     *	t1         <current namespace>::t1
	     *	n1::t1     <current namespace>::n1::t1
	     *	::t1	   ::t1
	     *  ::n1::t1   ::n1::t1
	     */
	    if (Blt_ParseQualifiedName(interp, treeName, &nsPtr, &name) 
		!= TCL_OK) {
		Tcl_AppendResult(interp, "can't find namespace in \"", treeName,
			 "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    if (nsPtr == NULL) {
		nsPtr = Tcl_GetCurrentNamespace(interp);
	    }
	    treeName = Blt_GetQualifiedName(nsPtr, name, &dString);
	    /* 
	     * Check if the command already exists. 
	     */
	    if (Tcl_GetCommandInfo(interp, (char *)treeName, &cmdInfo)) {
		Tcl_AppendResult(interp, "a command \"", treeName,
				 "\" already exists", (char *)NULL);
		goto error;
	    }
	    if (Blt_TreeExists(interp, treeName)) {
		Tcl_AppendResult(interp, "a tree \"", treeName, 
			"\" already exists", (char *)NULL);
		goto error;
	    }
	} 
    } 
    if (treeName == NULL) {
	goto error;
    }
    if (Blt_TreeCreate(interp, treeName, &token) == TCL_OK) {
	int isNew;
	TreeCmd *cmdPtr;

	cmdPtr = Blt_Calloc(1, sizeof(TreeCmd));
	assert(cmdPtr);
	cmdPtr->dataPtr = dataPtr;
	cmdPtr->tree = token;
	cmdPtr->interp = interp;
	Blt_InitHashTable(&(cmdPtr->traceTable), BLT_STRING_KEYS);
	Blt_InitHashTable(&(cmdPtr->notifyTable), BLT_STRING_KEYS);
	cmdPtr->cmdToken = Tcl_CreateObjCommand(interp, (char *)treeName, 
		(Tcl_ObjCmdProc *)TreeInstObjCmd, cmdPtr, TreeInstDeleteProc);
	cmdPtr->tablePtr = &dataPtr->treeTable;
	cmdPtr->hashPtr = Blt_CreateHashEntry(cmdPtr->tablePtr, (char *)cmdPtr,
	      &isNew);
	Blt_SetHashValue(cmdPtr->hashPtr, cmdPtr);
	Tcl_SetResult(interp, (char *)treeName, TCL_VOLATILE);
	Tcl_DStringFree(&dString);
	Blt_TreeCreateEventHandler(cmdPtr->tree, TREE_NOTIFY_ALL, 
	     TreeEventProc, cmdPtr);
	return TCL_OK;
    }
 error:
    Tcl_DStringFree(&dString);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDestroyOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TreeDestroyOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TreeCmdInterpData *dataPtr = clientData;
    TreeCmd *cmdPtr;
    char *string;
    register int i;

    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	cmdPtr = GetTreeCmd(dataPtr, interp, string);
	if (cmdPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a tree named \"", string,
			     "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, cmdPtr->cmdToken);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNamesOp --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TreeNamesOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TreeCmdInterpData *dataPtr = clientData;
    TreeCmd *cmdPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *objPtr, *listObjPtr;
    Tcl_DString dString;
    char *qualName;

    Tcl_DStringInit(&dString);
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&dataPtr->treeTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	cmdPtr = Blt_GetHashValue(hPtr);
	qualName = Blt_GetQualifiedName(
		Blt_GetCommandNamespace(interp, cmdPtr->cmdToken), 
		Tcl_GetCommandName(interp, cmdPtr->cmdToken), &dString);
	if (objc == 3) {
	    if (!Tcl_StringMatch(qualName, Tcl_GetString(objv[2]))) {
		continue;
	    }
	}
	objPtr = Tcl_NewStringObj(qualName, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Tcl_DStringFree(&dString);
    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * TreeObjCmd --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec treeCmdOps[] =
{
    {"create", 1, (Blt_Op)TreeCreateOp, 2, 3, "?name?",},
    {"destroy", 1, (Blt_Op)TreeDestroyOp, 3, 0, "name...",},
    {"names", 1, (Blt_Op)TreeNamesOp, 2, 3, "?pattern?...",},
};

static int nCmdOps = sizeof(treeCmdOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
TreeObjCmd(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;

    proc = Blt_GetOpFromObj(interp, nCmdOps, treeCmdOps, BLT_OP_ARG1, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, objc, objv);
}

/*
 * -----------------------------------------------------------------------
 *
 * TreeInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the "tree" command
 *	is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the hash table managing all tree names.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TreeInterpDeleteProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp)
{
    TreeCmdInterpData *dataPtr = clientData;

    /* All tree instances should already have been destroyed when
     * their respective Tcl commands were deleted. */
    Blt_DeleteHashTable(&dataPtr->treeTable);
    Tcl_DeleteAssocData(interp, TREE_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*ARGSUSED*/
static int
CompareDictionaryCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    int result;
    char *s1, *s2;

    s1 = Tcl_GetString(objv[1]);
    s2 = Tcl_GetString(objv[2]);
    result = Blt_DictionaryCompare(s1, s2);
    result = (result > 0) ? -1 : (result < 0) ? 1 : 0;
    Tcl_SetIntObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*ARGSUSED*/
static int
ExitCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    int code;

    if (Tcl_GetIntFromObj(interp, objv[1], &code) != TCL_OK) {
	return TCL_ERROR;
    }
#ifdef TCL_THREADS
    Tcl_Exit(code);
#else 
    exit(code);
#endif
    /*NOTREACHED*/
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_TreeInit --
 *
 *	This procedure is invoked to initialize the "tree" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds a new entry into a global Tcl
 *	associative array.
 *
 * ------------------------------------------------------------------------
 */
int
Blt_TreeInit(Tcl_Interp *interp)
{
    TreeCmdInterpData *dataPtr;	/* Interpreter-specific data. */
    static Blt_ObjCmdSpec cmdSpec = { 
	"tree", TreeObjCmd, 
    };
    static Blt_ObjCmdSpec compareSpec = { 
	"compare", CompareDictionaryCmd, 
    };
    static Blt_ObjCmdSpec exitSpec = { 
	"exit", ExitCmd, 
    };
    if (Blt_InitObjCmd(interp, "blt::util", &compareSpec) == NULL) {
	return TCL_ERROR;
    }
    if (Blt_InitObjCmd(interp, "blt::util", &exitSpec) == NULL) {
	return TCL_ERROR;
    }

    dataPtr = GetTreeCmdInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitObjCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_TreeCmdGetToken(
    Tcl_Interp *interp,
    CONST char *string,
    Blt_Tree  *treePtr)
{
    TreeCmdInterpData *dataPtr;
    TreeCmd *cmdPtr;

    dataPtr = GetTreeCmdInterpData(interp);
    cmdPtr = GetTreeCmd(dataPtr, interp, string);
    if (cmdPtr == NULL) {
	Tcl_AppendResult(interp, "can't find a tree associated with \"",
		 string, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *treePtr = cmdPtr->tree;
    return TCL_OK;
}

/* Dump tree to dbm */
/* Convert node data to datablock */

#endif /* NO_TREE */

