
/*
 *
 * bltTupleCmd.c --
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
 *	The "tuple" data object was created by George A. Howlett.
 */

/*
  tuple create t0 t1 t2
  tuple names
  t0 destroy
     -or-
  tuple destroy t0
  tuple copy tuple@node tuple@node -recurse -tags

  tuple move node after|before|into t2@node

  $t apply -recurse $root command arg arg			

  $t attach tuplename				

  $t children $n
  t0 copy node1 node2 node3 node4 node5 destName 
  $t delete $n...				
  $t delete 0 
  $t delete 0 10
  $t delete all
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

/*
 * Rows as tokens.  
 * 
 */

#include <bltInt.h>

#ifndef NO_TUPLE

#include <bltTuple.h>
#include <bltHash.h>
#include <bltList.h>
#include "bltSwitch.h"
#include <ctype.h>

#define TUPLE_THREAD_KEY "BLT Tuple Command Data"
#define TUPLE_MAGIC ((unsigned int) 0x46170277)

enum TagTypes { TAG_TYPE_NONE, TAG_TYPE_ALL, TAG_TYPE_TAG };

typedef struct {
    Blt_HashTable instTable;	/* Hash table of tuples keyed by address. */
    Tcl_Interp *interp;
} TupleCmdInterpData;

typedef struct {
    Tcl_Interp *interp;
    Blt_TupleTable table;	/* Token representing the tuple
				 * table. */
    Tcl_Command cmdToken;	/* Token for tuple object's Tcl
				 * command. */

    Tcl_Obj *emptyObjPtr;	/* String representing an empty table
				 * value. */
    Blt_HashEntry *hashPtr;
    Blt_HashTable *instTablePtr;

    int traceCounter;		/* Used to generate trace id strings.  */
    Blt_HashTable traceTable;	/* Table of active traces. Maps trace ids
				 * back to their TraceInfo records. */

    int notifyCounter;		/* Used to generate notify id strings. */
    Blt_HashTable notifyTable;	/* Table of event handlers. Maps notify ids
				 * back to their NotifyInfo records. */
} TupleCmd;

typedef struct {
    Blt_TupleTrace trace;
    TupleCmd *cmdPtr;
    Tcl_Obj **objv;
    int objc;
    Blt_HashEntry *hashPtr;
} TraceInfo;

typedef struct {
    Blt_TupleNotifier notifier;
    TupleCmd *cmdPtr;
    Tcl_Obj **objv;
    int objc;
    Blt_HashEntry *hashPtr;
} NotifyInfo;

typedef struct {
    unsigned int column;
    Tcl_Obj *objPtr;
} KeyValue;

typedef struct {
    int mask;
} NotifyData;

static Blt_SwitchSpec notifySwitches[] = 
{
    {BLT_SWITCH_FLAG, "-create", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_CREATE},
    {BLT_SWITCH_FLAG, "-createrow", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_CREATE_ROW},
    {BLT_SWITCH_FLAG, "-createcolumn", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_CREATE_COLUMN},
    {BLT_SWITCH_FLAG, "-delete", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_DELETE},
    {BLT_SWITCH_FLAG, "-deleterow", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_DELETE_ROW},
    {BLT_SWITCH_FLAG, "-deletecolumn", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_DELETE_COLUMN},
    {BLT_SWITCH_FLAG, "-move", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_MOVE},
    {BLT_SWITCH_FLAG, "-sort", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_SORT},
    {BLT_SWITCH_FLAG, "-relabel", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_RELABEL},
    {BLT_SWITCH_FLAG, "-allevents", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_ALL},
    {BLT_SWITCH_FLAG, "-whenidle", Blt_Offset(NotifyData, mask), 0, 0, 
	TUPLE_NOTIFY_WHENIDLE},
    {BLT_SWITCH_END, NULL, 0, 0}
};

#ifdef notdef

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
    Blt_TupleNode parent;
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
    TupleCmd *cmdPtr;		/* Tuple to examine. */
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
				 * TUPLE_PREORDER, TUPLE_POSTORDER,
				 * TUPLE_INORDER, TUPLE_BREADTHFIRST. */
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
    TupleCmd *cmdPtr;		/* Tuple to move nodes. */
    Blt_TupleNode node;
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
    Blt_TupleNode srcNode, destNode;
    Blt_Tuple srcTuple, destTuple;
    TupleCmd *srcPtr, *destPtr;
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
    TupleCmd *cmdPtr;		/* Tuple to examine. */
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
    int sort;			/* If non-zero, sort the tuples.  */
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

static Blt_TupleApplyProc MatchTupleProc, SortTupleProc;
static Blt_TupleApplyProc ApplyTupleProc;
static Blt_TupleTraceProc TupleTraceProc;
static Blt_TupleCompareTuplesProc CompareTuples;

static Tcl_ObjCmdProc CompareDictionaryCmd;
static Tcl_ObjCmdProc ExitCmd;
#endif

static Blt_TupleNotifyEventProc TupleEventProc;
static Tcl_CmdDeleteProc TupleInstDeleteProc;
static Tcl_InterpDeleteProc TupleInterpDeleteProc;
static Tcl_ObjCmdProc TupleInstObjCmd;
static Tcl_ObjCmdProc TupleObjCmd;

static int GetTuple(TupleCmd *cmdPtr, Tcl_Obj *objPtr, Blt_Tuple *tuplePtr);


/*
 *----------------------------------------------------------------------
 *
 * NewTupleCmd --
 *
 *	This is a helper routine used by TupleCreateOp.  It create a
 *	new instance of a tuple command.  Memory is allocated for the
 *	command structure and a new Tcl command is created (same as
 *	the instance name).  All tuple commands have hash table
 *	entries in a global (interpreter-specific) registry.
 *	
 * Results:
 *	Returns a pointer to the newly allocated tuple command structure.
 *
 * Side Effects:
 *	Memory is allocated for the structure and a hash table entry is
 *	added.  
 *
 *---------------------------------------------------------------------- 
 */
static TupleCmd *
NewTupleCmd(
    Tcl_Interp *interp, 
    TupleCmdInterpData *interpDataPtr,
    Blt_TupleTable table, 
    CONST char *instName)
{
    int isNew;
    TupleCmd *cmdPtr;
    
    cmdPtr = Blt_Calloc(1, sizeof(TupleCmd));
    assert(cmdPtr);
    cmdPtr->table = table;
    cmdPtr->interp = interp;

    Blt_InitHashTable(&(cmdPtr->traceTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(cmdPtr->notifyTable), BLT_STRING_KEYS);
    cmdPtr->cmdToken = Tcl_CreateObjCommand(interp, (char *)instName, 
	(Tcl_ObjCmdProc *)TupleInstObjCmd, cmdPtr, TupleInstDeleteProc);
    cmdPtr->instTablePtr = &interpDataPtr->instTable;
    cmdPtr->hashPtr = Blt_CreateHashEntry(cmdPtr->instTablePtr, (char *)cmdPtr,
	&isNew);
    cmdPtr->emptyObjPtr = Tcl_NewStringObj("NA", -1);
    Tcl_IncrRefCount(cmdPtr->emptyObjPtr);
    Blt_SetHashValue(cmdPtr->hashPtr, cmdPtr);
    return cmdPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * GetTupleCmdInterpData --
 *
 *---------------------------------------------------------------------- 
 */
static TupleCmdInterpData *
GetTupleCmdInterpData(Tcl_Interp *interp)
{
    TupleCmdInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TupleCmdInterpData *)
	Tcl_GetAssocData(interp, TUPLE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TupleCmdInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TUPLE_THREAD_KEY, TupleInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&dataPtr->instTable, BLT_ONE_WORD_KEYS);
    }
    return dataPtr;
}


/*
 * ----------------------------------------------------------------------
 *
 * GenerateName --
 *
 *	Generates an unique tuple command name.  Tuple names are in
 *	the form "tupleN", where N is a non-negative integer. Check 
 *	each name generated to see if it is already a tuple. We want
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
    CONST char *instName, *name;

    /* 
     * Parse the command and put back so that it's in a consistent
     * format.  
     *
     *	t1         <current namespace>::t1
     *	n1::t1     <current namespace>::n1::t1
     *	::t1	   ::t1
     *  ::n1::t1   ::n1::t1
     */
    instName = NULL;		/* Suppress compiler warning. */
    for (n = 0; n < INT_MAX; n++) {
	Tcl_DStringInit(&dString);
	Tcl_DStringAppend(&dString, prefix, -1);
	sprintf(string, "tuple%d", n);
	Tcl_DStringAppend(&dString, string, -1);
	Tcl_DStringAppend(&dString, suffix, -1);
	instName = Tcl_DStringValue(&dString);
	if (Blt_ParseQualifiedName(interp, instName, &nsPtr, &name) 
	    != TCL_OK) {
	    Tcl_AppendResult(interp, "can't find namespace in \"", instName, 
		"\"", (char *)NULL);
	    return NULL;
	}
	if (nsPtr == NULL) {
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	}
	instName = Blt_GetQualifiedName(nsPtr, name, resultPtr);
	/* 
	 * Check if the command already exists. 
	 */
	if (Tcl_GetCommandInfo(interp, (char *)instName, &cmdInfo)) {
	    continue;
	}
	if (!Blt_TupleTableExists(interp, instName)) {
	    /* 
	     * We want the name of the tuple command and the underlying
	     * tuple object to be the same. Check that the free command
	     * name isn't an already a tuple object name.  
	     */
	    break;
	}
    }
    return instName;
}


/*
 *----------------------------------------------------------------------
 *
 * GetTupleCmd --
 *
 *	Find the tuple command associated with the Tcl command "string".
 *	
 *	We have to perform multiple lookups to get this right.  
 *
 *	The first step is to generate a canonical command name.  If an
 *	unqualified command name (i.e. no namespace qualifier) is
 *	given, we should search first the current namespace and then
 *	the global one.  Most Tcl commands (like Tcl_GetCmdInfo) look
 *	only at the global namespace.
 *
 *	Next check if the string is 
 *		a) a Tcl command and 
 *		b) really is a command for a tuple object.  
 *	Tcl_GetCommandInfo will get us the objClientData field that 
 *	should be a cmdPtr.  We verify that by searching our hashtable 
 *	of cmdPtr addresses.
 *
 * Results:
 *	A pointer to the tuple command.  If no associated tuple command
 *	can be found, NULL is returned.  It's up to the calling routines
 *	to generate an error message.
 *
 *---------------------------------------------------------------------- 
 */
static TupleCmd *
GetTupleCmd(
    TupleCmdInterpData *interpDataPtr, 
    Tcl_Interp *interp, 
    CONST char *string)
{
    CONST char *name;
    Tcl_Namespace *nsPtr;
    Tcl_CmdInfo cmdInfo;
    Blt_HashEntry *hPtr;
    Tcl_DString dString;
    char *qualName;
    int result;

    /* Put apart the tuple name and put is back together in a standard
     * format. */
    if (Blt_ParseQualifiedName(interp, string, &nsPtr, &name) != TCL_OK) {
	return NULL;		/* No such parent namespace. */
    }
    if (nsPtr == NULL) {
	nsPtr = Tcl_GetCurrentNamespace(interp);
    }
    /* Rebuild the fully qualified name. */
    qualName = Blt_GetQualifiedName(nsPtr, name, &dString);
    result = Tcl_GetCommandInfo(interp, qualName, &cmdInfo);
    Tcl_DStringFree(&dString);

    if (!result) {
	return NULL;
    }
    hPtr = Blt_FindHashEntry(&interpDataPtr->instTable, 
		(char *)(cmdInfo.objClientData));
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * GetTraceFlags --
 *
 *	Parses a string representation of the trace bit flags and
 *	returns the mask.
 *
 * Results:
 *	The trace mask is returned.
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
	    flags |= TUPLE_TRACE_READ;
	    break;
	case 'W':
	    flags |= TUPLE_TRACE_WRITE;
	    break;
	case 'U':
	    flags |= TUPLE_TRACE_UNSET;
	    break;
	case 'C':
	    flags |= TUPLE_TRACE_CREATE;
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
 * PrintTraceFlags --
 *
 *	Generates a string representation of the trace bit flags.
 *	It's assumed that the provided string is at least 5 bytes.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The bitflag information is written to the provided string.
 *
 *---------------------------------------------------------------------- 
 */
static void
PrintTraceFlags(unsigned int flags, char *string)
{
    register char *p;

    p = string;
    if (flags & TUPLE_TRACE_READ) {
	*p++ = 'r';
    } 
    if (flags & TUPLE_TRACE_WRITE) {
	*p++ = 'w';
    } 
    if (flags & TUPLE_TRACE_UNSET) {
	*p++ = 'u';
    } 
    if (flags & TUPLE_TRACE_CREATE) {
	*p++ = 'c';
    } 
    *p = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * PrintTuple --
 *
 *	Generates a string representation of a tuple in the provided
 *	dynamic string (the dstring is assumed to be previously
 *	initialized).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The tuple information is appended to the dynamic string.
 *
 *---------------------------------------------------------------------- 
 */
static void
PrintTuple(
    TupleCmd *cmdPtr, 
    Blt_Tuple tuple, 
    Tcl_DString *resultPtr)
{
    /* Tuple index */
    Tcl_DStringAppendElement(resultPtr, Blt_Itoa(Blt_TupleRowIndex(tuple)));

    /* Key-value pairs */
    {
	int column, nColumns;
	CONST char *key;
	Tcl_Obj *valueObjPtr;
	
	Tcl_DStringStartSublist(resultPtr);
	nColumns = Blt_TupleTableWidth(cmdPtr->table);
	for (column = 0; column < nColumns; column++) {
	    key = Blt_TupleGetColumnKey(cmdPtr->table, column);
	    if (Blt_TupleGetValue((Tcl_Interp *)NULL, cmdPtr->table, tuple, 
			key, &valueObjPtr) == TCL_OK) {
		Tcl_DStringAppendElement(resultPtr, key);
		if (valueObjPtr != NULL) {
		    Tcl_DStringAppendElement(resultPtr, 
			Tcl_GetString(valueObjPtr));
		} else {
		    Tcl_DStringAppendElement(resultPtr, "NA");
		}
	    }
	}	    
	Tcl_DStringEndSublist(resultPtr);
    }

    
    /* Tags */
    {				
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	Blt_TupleTagEntry *tPtr;

	Tcl_DStringStartSublist(resultPtr);
	for (hPtr = Blt_TupleFirstTag(cmdPtr->table, &cursor); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    Tcl_DStringAppendElement(resultPtr, tPtr->tagName);
	}
	Tcl_DStringEndSublist(resultPtr);
    }
    Tcl_DStringAppend(resultPtr, "\n", -1);
}

/*
 *----------------------------------------------------------------------
 *
 * UnsetValues --
 *
 *	This is a helper routine used by UnsetOp.  It unsets one or
 *	more key value pairs for a given tuple.
 *
 * Results:
 *	A standard Tcl result.  If an error occurred when unsetting
 *	the new value TCL_ERROR is returned and an error message is
 *	left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
static int
UnsetValues(
    TupleCmd *cmdPtr, 
    Blt_Tuple tuple, 
    int objc, 
    Tcl_Obj *CONST *objv)
{
    register int i;

    for (i = 0; i < objc; i ++) {
	if (Blt_TupleUnsetValue(cmdPtr->interp, cmdPtr->table, tuple, 
		Tcl_GetString(objv[i])) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteNotifier --
 *
 *	This is a helper routine used to delete notifiers.  It
 *	releases the Tcl_Objs used in the notification callback
 *	command and the actual tuple notifier.  Memory for the
 *	notifier is also freed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory is deallocated and the notitifer is no longer 
 *	active.
 *
 *---------------------------------------------------------------------- 
 */
static void
DeleteNotifier(NotifyInfo *notifyPtr)
{
    int i;

    for (i = 0; i < notifyPtr->objc; i++) {
	Tcl_DecrRefCount(notifyPtr->objv[i]);
    }
    Blt_TupleDeleteNotifier(notifyPtr->notifier);
    Blt_Free(notifyPtr->objv);
    Blt_Free(notifyPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteTrace --
 *
 *	This is a helper routine used to delete traces.  It releases
 *	the Tcl_Objs used in the trace callback command and the actual
 *	tuple trace.  Memory for the trace is also freed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory is deallocated and the trace is no longer active.
 *
 *---------------------------------------------------------------------- 
 */
static void
DeleteTrace(TraceInfo *tracePtr)
{
    int i;

    for(i = 0; i < tracePtr->objc; i++) {
	Tcl_DecrRefCount(tracePtr->objv[i]);
    }
    Blt_TupleDeleteTrace(tracePtr->trace);
    Blt_Free(tracePtr->objv);
    Blt_Free(tracePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TupleTraceProc --
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TupleTraceProc(
    ClientData clientData,
    Tcl_Interp *interp,
    unsigned int row,		/* Tuple that has just been updated. */
    unsigned int column,	/* Field that's updated. */
    unsigned int flags)
{
    TraceInfo *tracePtr = clientData; 
    char string[5];
    CONST char *key;
    int result;
    int i;

    i = tracePtr->objc;		/* Add extra command arguments
				 * starting at this index. */
    tracePtr->objv[i] = Tcl_NewIntObj(row);
    key = Blt_TupleGetColumnKey(tracePtr->cmdPtr->table, column);
    tracePtr->objv[i+1] = Tcl_NewStringObj(key, -1);
    PrintTraceFlags(flags, string);
    tracePtr->objv[i+2] = Tcl_NewStringObj(string, -1);
    Tcl_IncrRefCount(tracePtr->objv[i]);
    Tcl_IncrRefCount(tracePtr->objv[i+1]);
    Tcl_IncrRefCount(tracePtr->objv[i+2]);
    result = Tcl_EvalObjv(interp, tracePtr->objc + 3, tracePtr->objv, 0);
    Tcl_DecrRefCount(tracePtr->objv[i+2]);
    Tcl_DecrRefCount(tracePtr->objv[i+1]);
    Tcl_DecrRefCount(tracePtr->objv[i]);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TupleEventProc --
 *
 *---------------------------------------------------------------------- 
 */
static int
TupleEventProc(ClientData clientData, Blt_TupleNotifyEvent *eventPtr)
{
    NotifyInfo *notifyPtr = clientData; 
    int result;
    int i;
    char *string;
    
    switch (eventPtr->type) {
    case TUPLE_NOTIFY_CREATE_ROW:
	string = "-createrow";
	break;
	
    case TUPLE_NOTIFY_CREATE_COLUMN:
	string = "-createcolumn";
	break;

    case TUPLE_NOTIFY_DELETE_ROW:
	string = "-deleterow";
	break;

    case TUPLE_NOTIFY_DELETE_COLUMN:
	string = "-deletecolumn";
	break;
	
    case TUPLE_NOTIFY_MOVE:
	string = "-move";
	break;
	
    case TUPLE_NOTIFY_SORT:
	string = "-sort";
	break;
	
    case TUPLE_NOTIFY_RELABEL:
	string = "-relabel";
	break;
	
    default:
	/* empty */
	string = "???";
	break;
    }	
    
    i = notifyPtr->objc;
    notifyPtr->objv[i] = Tcl_NewStringObj(string, -1);
    notifyPtr->objv[i+1] = Tcl_NewIntObj(eventPtr->row);
    Tcl_IncrRefCount(notifyPtr->objv[i]);
    Tcl_IncrRefCount(notifyPtr->objv[i+1]);
    result = Tcl_EvalObjv(notifyPtr->cmdPtr->interp, notifyPtr->objc + 3, 
			  notifyPtr->objv, 0);
    Tcl_DecrRefCount(notifyPtr->objv[i+1]);
    Tcl_DecrRefCount(notifyPtr->objv[i]);
    if (result != TCL_OK) {
	Tcl_BackgroundError(notifyPtr->cmdPtr->interp);
	return TCL_ERROR;
    }
    Tcl_ResetResult(notifyPtr->cmdPtr->interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTuple --
 *
 *---------------------------------------------------------------------- 
 */
static int 
GetTuple(TupleCmd *cmdPtr, Tcl_Obj *objPtr, Blt_Tuple *tuplePtr)
{
    char c;
    Blt_Tuple tuple;
    char *string;

    string = Tcl_GetString(objPtr);
    c = string[0];

    if (isdigit(UCHAR(c))) {
	int row;

	if (Tcl_GetIntFromObj(cmdPtr->interp, objPtr, &row) != TCL_OK) {
	    return TCL_ERROR;
	}
	tuple = Blt_TupleGetTupleByIndex(cmdPtr->table, row);
	if (tuple != NULL) {
	    *tuplePtr = tuple;
	    return TCL_OK;
	}
    }  else if (cmdPtr != NULL) {
	if (strcmp(string, "all") == 0) {
	    if (Blt_TupleTableLength(cmdPtr->table) > 1) {
		Tcl_AppendResult(cmdPtr->interp, 
			"more than one node tagged as \"", string, "\"", 
			(char *)NULL);
		return TCL_ERROR;
	    }
	    tuple = Blt_TupleGetTupleByIndex(cmdPtr->table, 0);
	} else {
	    Blt_HashTable *tablePtr;
	    Blt_HashSearch cursor;
	    Blt_HashEntry *hPtr;
	    int result;

	    tuple = NULL;
	    result = TCL_ERROR;
	    tablePtr = Blt_TupleTagHashTable(cmdPtr->table, string);
	    if (tablePtr == NULL) {
		Tcl_AppendResult(cmdPtr->interp, "can't find tag or index \"", 
			string, "\" in ", Blt_TupleTableName(cmdPtr->table), 
			(char *)NULL);
	    } else if (tablePtr->numEntries > 1) {
		Tcl_AppendResult(cmdPtr->interp, 
			"more than one node tagged as \"", string, "\"", 
			(char *)NULL);
	    } else if (tablePtr->numEntries > 0) {
		hPtr = Blt_FirstHashEntry(tablePtr, &cursor);
		tuple = Blt_GetHashValue(hPtr);
		result = TCL_OK;
	    }
	    if (result == TCL_ERROR) {
		return TCL_ERROR;
	    }
	}
	if (tuple != NULL) {
	    *tuplePtr = tuple;
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(cmdPtr->interp, "can't find tag or index \"", string, 
		"\" in ", Blt_TupleTableName(cmdPtr->table), (char *)NULL);
    return TCL_ERROR;
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
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    if (objc == 3) {
	CONST char *objName;
	CONST char *name;
	Blt_TupleTable table;
	Tcl_Namespace *nsPtr;
	Tcl_DString dString;
	int result;

	objName = Tcl_GetString(objv[2]);
	if (Blt_ParseQualifiedName(interp, objName, &nsPtr, &name) 
	    != TCL_OK) {
	    Tcl_AppendResult(interp, "can't find namespace in \"", objName, 
			     "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (nsPtr == NULL) {
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	}
	objName = Blt_GetQualifiedName(nsPtr, name, &dString);
	result = Blt_TupleGetTable(interp, objName, &table);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if (cmdPtr->table != NULL) {
	    Blt_HashEntry *hPtr;
	    Blt_HashSearch cursor;
	    TraceInfo *tracePtr;
	    NotifyInfo *notifyPtr;
	    
	    Blt_TupleReleaseTable(cmdPtr->table);
	    /* Dump the current set of tags, traces, and notifiers. */
	    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &cursor); 
		 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		tracePtr = Blt_GetHashValue(hPtr);
		DeleteTrace(tracePtr);
	    }
	    Blt_DeleteHashTable(&cmdPtr->traceTable);
	    Blt_InitHashTable(&cmdPtr->traceTable, TCL_STRING_KEYS);
	    for (hPtr = Blt_FirstHashEntry(&cmdPtr->notifyTable, &cursor); 
		hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		notifyPtr = Blt_GetHashValue(hPtr);
		DeleteNotifier(notifyPtr);
	    }
	    Blt_DeleteHashTable(&cmdPtr->notifyTable);
	    Blt_InitHashTable(&cmdPtr->notifyTable, TCL_STRING_KEYS);
	}
	cmdPtr->table = table;
    }
    Tcl_SetResult(interp, (char *)Blt_TupleTableName(cmdPtr->table), 
	TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnOp --
 *
 *	Parses the given command line and calls one of several
 *	column-specific operations.
 *	
 * Results:
 *	Returns a standard Tcl result.  It is the result of 
 *	operation called.
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec columnOps[] =
{
#ifdef notdef
    {"create", 1, (Blt_Op)ColumnCreateOp, 4, 0, "name ?switches?",},
    {"delete", 1, (Blt_Op)ColumnDeleteOp, 3, 0, "name...",},
    {"get", 1, (Blt_Op)ColumnGetOp, 4, 0, "name...",},
    {"info", 1, (Blt_Op)ColumnInfoOp, 4, 4, "name",},
    {"names", 1, (Blt_Op)ColumnNamesOp, 3, 3, "",},
    {"set", 1, (Blt_Op)ColumnSetOp, 4, 0, "name values ?name values?...",},
    {"type", 1, (Blt_Op)ColumnTypeOp, 4, 0, "?newType?",},
    {"unset", 1, (Blt_Op)ColumnUnsetOp, 4, 0, "name...",},
    {"type", 1, (Blt_Op)ColumnTypeOp, 4, 0, "?newType?",},
#endif
};

static int nColumnOps = sizeof(columnOps) / sizeof(Blt_OpSpec);

static int
ColumnOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nColumnOps, columnOps, BLT_OP_ARG2, objc, 
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
 * DeleteOp --
 *
 *		d0 delete tagOrIdx ?tagOrIdx...?
 *
 *	Deletes one or more tuples from the table.  Tuples may be
 *	specified by their index or a tag.
 *	
 *---------------------------------------------------------------------- 
 */
static int
DeleteOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    Blt_TupleTagSearch cursor;
    int i;

    Blt_TupleStartDeleteRows(cmdPtr->table);
    for (i = 2; i < objc; i++) {
	/* A tag may designate more than one row. */
	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[i], &cursor);
	if (tuple == NULL) {
	    Blt_TupleEndDeleteRows(cmdPtr->table);
	    return TCL_ERROR;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    Blt_TupleDeleteTuple(cmdPtr->table, tuple);
	}
    }
    Blt_TupleEndDeleteRows(cmdPtr->table);
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
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    int bool;
    
    bool = TRUE;
    if (GetTuple(cmdPtr, objv[2], &tuple) != TCL_OK) {
	bool = FALSE;
    } else if (objc == 4) { 
	Tcl_Obj *valueObjPtr;
	char *key;
	
	key = Tcl_GetString(objv[3]);
	if (Blt_TupleGetValue((Tcl_Interp *)NULL, cmdPtr->table, tuple, 
			     key, &valueObjPtr) != TCL_OK) {
	    bool = FALSE;
	}
	bool = (valueObjPtr != NULL);
    } 
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 *	Retrieves the value from a given tuple for a designated
 *	column.  It's an error if the column key or row tuple is
 *	invalid.  If no key argument is provided then, all key-value
 *	pairs in the tuple are returned.  If a value is empty (the
 *	Tcl_Obj value is NULL), then the tuple command's "empty"
 *	string representation will be used.
 *	
 *	A third argument may be provided as the default return value.
 *	If no value exists for the given key, then this value is
 *	returned.
 * 
 * Results:
 *	A standard Tcl result. If the tag or index is invalid, 
 *	TCL_ERROR is returned and an error message is left in the 
 *	interpreter result.
 *	
 *
 *---------------------------------------------------------------------- 
 */
static int
GetOp(
    TupleCmd *cmdPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;

    if (GetTuple(cmdPtr, objv[2], &tuple) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	int row, column;
	Tcl_Obj *valueObjPtr, *listObjPtr;

	/* Return all the key-value pairs in the tuple. Ignore columns
	 * with empty values. */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	row = Blt_TupleRowIndex(tuple);
	for (column = 0; column < Blt_TupleTableWidth(cmdPtr->table); column++){
	    if (Blt_TupleGetValueByIndex((Tcl_Interp *)NULL, cmdPtr->table, 
			 row, column, &valueObjPtr) == TCL_OK) {
		if (valueObjPtr != NULL) {
		    Tcl_Obj *objPtr;
		    CONST char *key;

		    key = Blt_TupleGetColumnKey(cmdPtr->table, column);
		    objPtr = Tcl_NewStringObj(key, -1);
		    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		    Tcl_ListObjAppendElement(interp, listObjPtr, valueObjPtr);
		}
	    }
	}	    
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *valueObjPtr;
	char *key;
	int row, column;

	row = Blt_TupleRowIndex(tuple);
	
	key = Tcl_GetString(objv[3]); 
	if (Blt_TupleGetColumnIndex(interp, cmdPtr->table, key, &column) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_TupleGetValueByIndex((Tcl_Interp *)NULL, cmdPtr->table, row, 
		column, &valueObjPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (valueObjPtr == NULL) {
	    if (objc == 4) {
		Tcl_AppendResult(interp, "tuple "\", Tcl_GetString(objv[2]), 
			 "\" in table \"", Tcl_GetString(objv[0]), 
			 "\" is empty.", (char *)NULL);
		return TCL_ERROR;
	    }
	    valueObjPtr = objv[4];
	} 
	Tcl_SetObjResult(interp, valueObjPtr);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NotifyCreateOp --
 *
 *	Creates a notifier for this instance.  Notifiers represent
 *	a bitmask of events and a command prefix to be invoked 
 *	when a matching event occurs.  
 *
 *	The command prefix is parsed and saved in an array of
 *	Tcl_Objs. Extra slots are allocated for the 
 *
 * Results:
 *	A standard Tcl result.  The name of the new notifier is
 *	returned in the interpreter result.  Otherwise, if it failed
 *	to parse a switch, then TCL_ERROR is returned and an error
 *	message is left in the interpreter result.
 *
 * Example:
 *	tuple0 notify create ?flags? command arg
 *
 *---------------------------------------------------------------------- 
 */
static int
NotifyCreateOp(
    TupleCmd *cmdPtr,
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
	data.mask = TUPLE_NOTIFY_ALL;
    }
    sprintf(idString, "notify%d", cmdPtr->notifyCounter++);
    hPtr = Blt_CreateHashEntry(&cmdPtr->notifyTable, idString, &isNew);
    Blt_SetHashValue(hPtr, notifyPtr);
    notifyPtr->notifier = Blt_TupleCreateNotifier(cmdPtr->table, 
	 data.mask, TupleEventProc, notifyPtr);
    Tcl_SetStringObj(Tcl_GetObjResult(interp), idString, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyDeleteOp --
 *
 *	Deletes one or more notifiers.  
 *
 * Results:
 *	A standard Tcl result.  If a name given doesn't represent
 *	a notifier, then TCL_ERROR is returned and an error message
 *	is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
static int
NotifyDeleteOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    NotifyInfo *notifyPtr;
    Blt_HashEntry *hPtr;

    register int i;
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
	Blt_DeleteHashEntry(&cmdPtr->notifyTable, hPtr);
	DeleteNotifier(notifyPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyInfoOp --
 *
 *	Returns the details for a given notifier.  The name
 *	of the notifier is passed as an argument.  The details
 *	are both the event mask and command prefix.
 *
 * Results:
 *	A standard Tcl result.  If the name given doesn't represent
 *	a notifier, then TCL_ERROR is returned and an error message
 *	is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NotifyInfoOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    NotifyInfo *notifyPtr;
    struct Blt_TupleNotifierStruct *nPtr;
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
    nPtr = notifyPtr->notifier;
    if (nPtr->mask & TUPLE_NOTIFY_CREATE) {
	Tcl_DStringAppendElement(&dString, "-create");
    }
    if (nPtr->mask & TUPLE_NOTIFY_DELETE) {
	Tcl_DStringAppendElement(&dString, "-delete");
    }
    if (nPtr->mask & TUPLE_NOTIFY_MOVE) {
	Tcl_DStringAppendElement(&dString, "-move");
    }
    if (nPtr->mask & TUPLE_NOTIFY_SORT) {
	Tcl_DStringAppendElement(&dString, "-sort");
    }
    if (nPtr->mask & TUPLE_NOTIFY_RELABEL) {
	Tcl_DStringAppendElement(&dString, "-relabel");
    }
    if (nPtr->mask & TUPLE_NOTIFY_WHENIDLE) {
	Tcl_DStringAppendElement(&dString, "-whenidle");
    }
    Tcl_DStringEndSublist(&dString);
    Tcl_DStringStartSublist(&dString);
    for (i = 0; i < notifyPtr->objc; i++) {
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
 *	Returns the names of all the notifiers in use by this
 *	instance.  Notifiers issues by other instances or object
 *	clients are not reported.  
 *
 * Results:
 *	Always TCL_OK.  A list of notifier names is left in the
 *	interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
NotifyNamesOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *objPtr, *listObjPtr;
    char *idString;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&(cmdPtr->notifyTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	idString = Blt_GetHashKey(&(cmdPtr->notifyTable), hPtr);
	objPtr = Tcl_NewStringObj(idString, -1);
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
 *	Parses the given command line and calls one of several
 *	notifier-specific operations.
 *	
 * Results:
 *	Returns a standard Tcl result.  It is the result of 
 *	operation called.
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
    TupleCmd *cmdPtr,
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

/*
 *----------------------------------------------------------------------
 *
 * ColumnOp --
 *
 *	Parses the given command line and calls one of several
 *	column-specific operations.
 *	
 * Results:
 *	Returns a standard Tcl result.  It is the result of 
 *	operation called.
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec columnOps[] =
{
#ifdef notdef
    {"create", 1, (Blt_Op)ColumnCreateOp, 4, 0, "name ?switches?",},
    {"cut",    1, (Blt_Op)ColumnDeleteOp, 3, 0, "name...",},
    {"dup",    1, (Blt_Op)ColumnGetOp,    4, 0, "name...",},
    {"pack",   1, (Blt_Op)ColumnNamesOp,  3, 3, "",},
    {"paste",  1, (Blt_Op)ColumnInfoOp,   4, 4, "name",},
    {"range",  1, (Blt_Op)ColumnSetOp,    4, 0, "name values ?name values?..",},
    {"type",   1, (Blt_Op)ColumnTypeOp,   4, 0, "?newType?",},
    {"unset",  1, (Blt_Op)ColumnUnsetOp,  4, 0, "name...",},
    {"type",   1, (Blt_Op)ColumnTypeOp,   4, 0, "?newType?",},
#endif
};

static int nColumnOps = sizeof(columnOps) / sizeof(Blt_OpSpec);

static int
SelectOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nColumnOps, columnOps, BLT_OP_ARG2, objc, 
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
 * SetOp --
 *
 *	Sets one or more key-value pairs for tuples.  One or more
 *	tuples may be set.  If any of the columns (keys) given don't
 *	already exist, the columns will be automatically created.  The
 *	same holds true for rows.  If a row index is beyond the end of
 *	the table (tags are always in range), new rows are allocated.
 * 
 * Results:
 *	A standard Tcl result. If the tag or index is invalid, 
 *	TCL_ERROR is returned and an error message is left in the 
 *	interpreter result.
 *	
 *---------------------------------------------------------------------- 
 */
static int
SetOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    register int i;
    int isNew;
    char *string;

    string = Tcl_GetString(objv[2]);
    if (isdigit(UCHAR(*string))) {
	int row, extra;
	CONST char *key;
	int column;

	if (Tcl_GetIntFromObj(interp, objv[2], &row) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* Extend the array of tuples if necessary */
	extra = row - Blt_TupleTableLength(cmdPtr->table);
	if ((extra > 0) && 
	    (Blt_TupleExtendRows(cmdPtr->table, extra) != TCL_OK)) {
	    return TCL_ERROR;
	}
	/* Add the values, creating new keys if necessary. */
	for (i = 3; i < objc; i++) {
	    key = Tcl_GetString(objv[i]);
	    column = Blt_TupleAddColumn(cmdPtr->table, key, &isNew);
	    i++;
	    if (i == objc) {
		Tcl_AppendResult(cmdPtr->interp, "missing value for field \"", 
			key, "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    if (Blt_TupleSetValue(interp, cmdPtr->table, tuple, key, objv[i]) 
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    } else {
	Blt_TupleTagSearch cursor;
	KeyValue *values;
	CONST char *key;
	int n;

	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[2], &cursor);
	if (tuple == NULL) {
	    return TCL_ERROR;
	}
	values = Blt_Malloc(sizeof(KeyValue) * objc - 3);
	/* Create new keys if necessary. */
	n = 0;
	for (i = 3; i < objc; i++) {
	    key = Tcl_GetString(objv[i]);
	    values[n].column = Blt_TupleAddColumn(cmdPtr->table, key, &isNew);
	    i++;
	    if (i == objc) {
		Tcl_AppendResult(cmdPtr->interp, "missing value for field \"", 
			key, "\"", (char *)NULL);
		goto error;
	    }
	    values[n].objPtr = objv[i];
	    n++;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    for (i = 0; i < n; i++) {
		if (Blt_TupleSetValueByIndex(cmdPtr->interp, cmdPtr->table, 
			Blt_TupleRowIndex(tuple), values[i].column, 
			values[i].objPtr) != TCL_OK) {
		    goto error;
		}
	    }
	}
	Blt_Free(values);
	return TCL_OK;
    error:
	Blt_Free(values);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TagAddOp --
 *
 *	Adds a tag to one or more tuples. A tuple may already have the
 *	tag. Tag names can not start with a digit.  It's also an error
 *	to try to add the reserved tag "all".
 *  
 * Results:
 *	A standard Tcl result.  If a tag isn't valid then TCL_ERROR is
 *	returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagAddOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    register int i;
    char *tagName;
    Blt_TupleTagSearch cursor;

    tagName = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(tagName[0]))) {
	Tcl_AppendResult(interp, "bad tag \"", tagName, 
		 "\": can't start with a digit", (char *)NULL);
	return TCL_ERROR;
    }
    if (strcmp(tagName, "all") == 0) {
	Tcl_AppendResult(cmdPtr->interp, "can't add reserved tag \"", tagName, 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[i], &cursor);
	if (tuple == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    Blt_TupleAddTag(cmdPtr->table, tuple, tagName);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagDeleteOp --
 *
 *	Deletes a tag from one or more tuples. It's an error to try to
 *	remove the tag "all".  It's not an error is the tuple doesn't
 *	currently have the tag.
 *  
 * Results:
 *	A standard Tcl result.  If a tag isn't valid then TCL_ERROR is
 *	returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagDeleteOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    char *tagName;
    Blt_Tuple tuple;
    int i;
    Blt_TupleTagSearch cursor;

    tagName = Tcl_GetString(objv[3]);
    if (strcmp(tagName, "all") == 0) {
	Tcl_AppendResult(interp, "can't delete reserved tag \"", tagName, "\"", 
			 (char *)NULL);
        return TCL_ERROR;
    }
      
    for (i = 4; i < objc; i++) {
	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[i], &cursor);
	if (tuple == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    Blt_TupleRemoveTag(cmdPtr->table, tuple, tagName);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagDumpOp --
 *
 *	Reports information about the tuples tagged with the given
 *	tags.  
 *  
 * Results:
 *	A standard Tcl result.  If a tag isn't valid then TCL_ERROR is
 *	returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagDumpOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Tcl_DString dString;
    Blt_TupleTagSearch cursor;
    Blt_Tuple tuple;
    register int i;

    Tcl_DStringInit(&dString);
    for (i = 3; i < objc; i++) {
	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[i], &cursor);
	if (tuple == NULL) {
	    Tcl_DStringFree(&dString);
	    return TCL_ERROR;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    PrintTuple(cmdPtr, tuple, &dString);
	}
    }
    Tcl_DStringResult(interp, &dString);
    Tcl_DStringFree(&dString);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * TagForgetOp --
 *
 *	Deletes one or more tags.  The tuples currently associated
 *	with the tag remain otherwise intact.  It's not an error if
 *	the tag doesn't currently exist.
 *  
 * Results:
 *	Always TCL_OK.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TagForgetOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    register int i;

    for (i = 3; i < objc; i++) {
	Blt_TupleForgetTag(cmdPtr->table, Tcl_GetString(objv[i]));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagNamesOp --
 *
 *	Returns a list of the unique tags for the given tuples.  If no
 *	tuples are given, then all tags are reported.
 *  
 * Results:
 *	A standard Tcl result.  If a tag or index of a tuple isn't
 *	valid then TCL_ERROR is returned and an error message is left
 *	in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagNamesOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_HashSearch cursor;
    Blt_TupleTagEntry *tPtr;
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    objPtr = Tcl_NewStringObj("all", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (objc == 3) {
	Blt_HashEntry *hPtr;

	for (hPtr = Blt_TupleFirstTag(cmdPtr->table, &cursor); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    objPtr = Tcl_NewStringObj(tPtr->tagName, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	register int i;
	Blt_HashEntry *hPtr;
	Blt_HashTable uniqTable;
	int isNew;
	Blt_Tuple tuple;

	Blt_InitHashTable(&uniqTable, BLT_STRING_KEYS);
	for (i = 3; i < objc; i++) {
	    if (GetTuple(cmdPtr, objv[i], &tuple) != TCL_OK) {
		goto error;
	    }
	    for (hPtr = Blt_TupleFirstTag(cmdPtr->table, &cursor); hPtr != NULL;
		 hPtr = Blt_NextHashEntry(&cursor)) {
		tPtr = Blt_GetHashValue(hPtr);
		if (Blt_TupleHasTag(cmdPtr->table, tuple, tPtr->tagName)) {
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
 * TagRangeOp --
 *
 *	Adds a tag to a range of tuples. A tuple may already have the
 *	tag. Tag names can not start with a digit.  It's also an error
 *	to try to add the reserved tag "all".
 *  
 * Results:
 *	A standard Tcl result.  If a tag isn't valid then TCL_ERROR is
 *	returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagRangeOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    char *tagName;
    Blt_Tuple first, last;

    tagName = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(tagName[0]))) {
	Tcl_AppendResult(interp, "bad tag \"", tagName, 
		 "\": can't start with a digit", (char *)NULL);
	return TCL_ERROR;
    }
    if (strcmp(tagName, "all") == 0) {
	Tcl_AppendResult(cmdPtr->interp, "can't add reserved tag \"", tagName, 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((GetTuple(cmdPtr, objv[4], &first) != TCL_OK) ||
	(GetTuple(cmdPtr, objv[5], &last) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (Blt_TupleRowIndex(first) <= Blt_TupleRowIndex(last)) {
	Blt_Tuple tuple;

	do {
	    Blt_TupleAddTag(cmdPtr->table, tuple, tagName);
	    tuple = Blt_TupleNextTuple(cmdPtr->table, tuple);
	} while(tuple != last);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TagTuplesOp --
 *
 *	Returns a list of unique tuple indices associated with the
 *	tags given.
 *  
 * Results:
 *	A standard Tcl result.  If a tag isn't valid then TCL_ERROR is
 *	returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TagTuplesOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable uniqTable;
    Blt_Tuple tuple;
    Tcl_Obj *listObjPtr;
    Tcl_Obj *objPtr;
    char *tagName;
    int isNew;
    register int i;
	
    Blt_InitHashTable(&uniqTable, BLT_ONE_WORD_KEYS);
    for (i = 3; i < objc; i++) {
	tagName = Tcl_GetString(objv[i]);
	if (strcmp(tagName, "all") == 0) {
	    break;
	} else {
	    Blt_HashTable *tablePtr;
	    
	    tablePtr = Blt_TupleTagHashTable(cmdPtr->table, tagName);
	    if (tablePtr != NULL) {
		for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); 
		     hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		    tuple = Blt_GetHashValue(hPtr);
		    Blt_CreateHashEntry(&uniqTable, (char *)tuple, &isNew);
		}
		continue;
	    }
	}
	Tcl_AppendResult(interp, "can't find a tag \"", tagName, "\"",
			 (char *)NULL);
	goto error;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&uniqTable, &cursor); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&cursor)) {
	tuple = (Blt_Tuple)Blt_GetHashKey(&uniqTable, hPtr);
	objPtr = Tcl_NewIntObj(Blt_TupleRowIndex(tuple));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_DeleteHashTable(&uniqTable);
    return TCL_OK;

 error:
    Blt_DeleteHashTable(&uniqTable);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TagOp --
 *
 * 	This procedure is invoked to process tag operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec tagOps[] = {
    {"add", 1, (Blt_Op)TagAddOp, 5, 0, "tag tagOrIdx...",},
    {"delete", 2, (Blt_Op)TagDeleteOp, 5, 0, "tag tagOrIdx...",},
    {"dump", 2, (Blt_Op)TagDumpOp, 4, 0, "tag...",},
    {"forget", 1, (Blt_Op)TagForgetOp, 4, 0, "tag...",},
    {"names", 1, (Blt_Op)TagNamesOp, 3, 0, "?tagOrIdx...?",},
    {"range", 1, (Blt_Op)TagRangeOp, 6, 6, "tag first last",},
    {"tuples", 1, (Blt_Op)TagTuplesOp, 4, 0, "tag ?tag...?",},
};

static int nTagOps = sizeof(tagOps) / sizeof(Blt_OpSpec);

static int
TagOp(
    TupleCmd *cmdPtr,
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
 *	Creates a trace for this instance.  Traces represent
 *	list of keys, a bitmask of trace flags, and a command prefix 
 *	to be invoked when a matching trace event occurs.  
 *
 *	The command prefix is parsed and saved in an array of
 *	Tcl_Objs. The qualified name of the instance is saved
 *	also.
 *
 * Results:
 *	A standard Tcl result.  The name of the new trace is
 *	returned in the interpreter result.  Otherwise, if it failed
 *	to parse a switch, then TCL_ERROR is returned and an error
 *	message is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceCreateOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    TraceInfo *tracePtr;
    char *keyList, *tagName, *string;
    char idString[200];
    int flags, isNew;
    struct Blt_TupleTraceStruct *tPtr;
    Tcl_Obj **args;
    int nArgs;
    int i;

    tagName = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(*tagName))) {
	if (GetTuple(cmdPtr, objv[3], &tuple) != TCL_OK) {
	    return TCL_ERROR;
	}
	tagName = NULL;
    } else {
	tagName = Blt_Strdup(tagName);
	tuple = NULL;
    }
    keyList = Tcl_GetString(objv[4]);
    string = Tcl_GetString(objv[5]);
    flags = GetTraceFlags(string);
    if (flags < 0) {
	Tcl_AppendResult(interp, "unknown flag in \"", string, "\"", 
		     (char *)NULL);
	return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, objv[6], &nArgs, &args) != TCL_OK) {
	return TCL_ERROR;
    }
    tPtr = Blt_TupleCreateTrace(cmdPtr->table, tuple, keyList, tagName, 
	      flags, TupleTraceProc, tracePtr);
    if (tPtr == NULL) {
	return TCL_ERROR;
    }
    /* Stash away the command in structure and pass that to the trace. */
    tracePtr = Blt_Malloc(sizeof(TraceInfo));
    tracePtr->cmdPtr = cmdPtr;
    tracePtr->trace = tPtr;
    sprintf(idString, "trace%d", cmdPtr->traceCounter++);
    tracePtr->hashPtr = Blt_CreateHashEntry(&cmdPtr->traceTable, idString, 
	&isNew);
    Blt_SetHashValue(tracePtr->hashPtr, tracePtr);
    Tcl_SetStringObj(Tcl_GetObjResult(interp), idString, -1);

    tracePtr->objv = Blt_Calloc(nArgs + 1 + 3 + 1, sizeof(Tcl_Obj *));
    for(i = 0; i < nArgs; i++) {
	tracePtr->objv[i] = args[i];
	Tcl_IncrRefCount(tracePtr->objv[i]);
    }
    {
	Tcl_DString dString;
	char *qualName;

	Tcl_DStringInit(&dString);
	qualName = Blt_GetQualifiedName(
		Blt_GetCommandNamespace(interp, cmdPtr->cmdToken), 
		Tcl_GetCommandName(interp, cmdPtr->cmdToken), &dString);
	tracePtr->objv[i] = Tcl_NewStringObj(qualName, -1);
	Tcl_IncrRefCount(tracePtr->objv[i]);
	Tcl_DStringFree(&dString);
	tracePtr->objc = nArgs + 1;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceDeleteOp --
 *
 *	Deletes one or more traces.  
 *
 * Results:
 *	A standard Tcl result.  If a name given doesn't represent
 *	a trace, then TCL_ERROR is returned and an error message
 *	is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TraceDeleteOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TraceInfo *tracePtr;
    Blt_HashEntry *hPtr;
    register int i;
    char *name;

    for (i = 3; i < objc; i++) {
	name = Tcl_GetString(objv[i]);
	hPtr = Blt_FindHashEntry(&cmdPtr->traceTable, name);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "unknown trace \"", name, "\"", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	tracePtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashEntry(&cmdPtr->traceTable, hPtr); 
	DeleteTrace(tracePtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceInfoOp --
 *
 *	Returns the details for a given trace.  The name of the trace
 *	is passed as an argument.  The details are returned as a list
 *	of key-value pairs: trace name, tag, row index, keys, flags,
 *	and the command prefix.
 *
 * Results:
 *	A standard Tcl result.  If the name given doesn't represent
 *	a trace, then TCL_ERROR is returned and an error message
 *	is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceInfoOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    TraceInfo *tracePtr;
    struct Blt_TupleTraceStruct *tPtr;
    Blt_HashEntry *hPtr;
    Tcl_DString dString;
    char string[5];
    char *withTag;
    char *idString;
    int i;
    CONST char **p;

    idString = Tcl_GetString(objv[3]);
    hPtr = Blt_FindHashEntry(&cmdPtr->traceTable, idString);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown trace \"", idString, "\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    tPtr = tracePtr->trace;
    tracePtr = Blt_GetHashValue(hPtr);
    Tcl_DStringAppendElement(&dString, "id");
    Tcl_DStringAppendElement(&dString, idString);

    Tcl_DStringAppendElement(&dString, "tag");
    withTag = tPtr->withTag;
    if (withTag == NULL) {
	withTag = "";
    }
    Tcl_DStringAppendElement(&dString, withTag);
    Tcl_DStringAppendElement(&dString, "row");
    Tcl_DStringAppendElement(&dString, 
		Blt_Itoa(Blt_TupleRowIndex(tPtr->tuple)));

    Tcl_DStringAppendElement(&dString, "keys");
    Tcl_DStringStartSublist(&dString);
    for (p = tPtr->keys; *p != NULL; p++) {
	Tcl_DStringAppendElement(&dString, *p);
    }
    Tcl_DStringEndSublist(&dString);
    Tcl_DStringAppendElement(&dString, "flags");
    PrintTraceFlags(tPtr->mask, string);
    Tcl_DStringAppendElement(&dString, string);
    Tcl_DStringAppendElement(&dString, "command");
    Tcl_DStringStartSublist(&dString);
    for (i = 0; i < tracePtr->objc; i++) {
	Tcl_DStringAppendElement(&dString, Tcl_GetString(tracePtr->objv[i]));
    }
    Tcl_DStringEndSublist(&dString);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceNamesOp --
 *
 *	Returns the names of all the traces in use by this instance.
 *	Traces created by other instances or object clients are not
 *	reported.
 *
 * Results:
 *	Always TCL_OK.  A list of trace names is left in the
 *	interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TraceNamesOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Tcl_AppendElement(interp, Blt_GetHashKey(&cmdPtr->traceTable, hPtr));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceOp --
 *
 * 	This procedure is invoked to process trace operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec traceOps[] =
{
    {"create", 1, (Blt_Op)TraceCreateOp, 7, 7, "tagOrIdx keyList how command",},
    {"delete", 1, (Blt_Op)TraceDeleteOp, 3, 0, "traceId...",},
    {"info", 1, (Blt_Op)TraceInfoOp, 4, 4, "traceId",},
    {"names", 1, (Blt_Op)TraceNamesOp, 3, 3, "",},
};

static int nTraceOps = sizeof(traceOps) / sizeof(Blt_OpSpec);

static int
TraceOp(
    TupleCmd *cmdPtr,
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
 * UnsetOp --
 *
 *	Unsets one or more values.  One or more tuples may be unset
 *	(using tags).  It's not an error if one of the key names
 *	(columns) doesn't exist.  The same holds true for rows.  If a
 *	row index is beyond the end of the table (tags are always in
 *	range), it is also ignored.
 * 
 * Results:
 *	A standard Tcl result. If the tag or index is invalid, 
 *	TCL_ERROR is returned and an error message is left in the 
 *	interpreter result.
 *	
 *---------------------------------------------------------------------- 
 */
static int
UnsetOp(
    TupleCmd *cmdPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Tuple tuple;
    char *string;
	
    string = Tcl_GetString(objv[2]);
    if (isdigit(UCHAR(*string))) {
	if (GetTuple(cmdPtr, objv[2], &tuple) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (UnsetValues(cmdPtr, tuple, objc - 3, objv + 3) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Blt_TupleTagSearch cursor;

	tuple = Blt_TupleFirstTagged(interp, cmdPtr->table, objv[2], &cursor);
	if (tuple == NULL) {
	    return TCL_ERROR;
	}
	for (/* empty */; tuple != NULL; tuple = Blt_TupleNextTagged(&cursor)) {
	    if (UnsetValues(cmdPtr, tuple, objc - 3, objv + 3) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * TupleInstObjCmd --
 *
 * 	This procedure is invoked to process commands on behalf of
 *	the instance of the tuple-object.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */
static Blt_OpSpec tupleOps[] =
{
    {"attach", 2, (Blt_Op)AttachOp, 2, 3, "?tupleCmd?",},
    {"delete", 2, (Blt_Op)DeleteOp, 3, 0, "tagOrIdx ?tagOrIdx...?",},
    {"exists", 1, (Blt_Op)ExistsOp, 3, 4, "tagOrIdx ?key?",},
    {"get", 1, (Blt_Op)GetOp, 3, 5, "tagOrIdx ?key? ?defaultValue?",},
    {"notify", 2, (Blt_Op)NotifyOp, 2, 0, "args...",},
    {"set", 3, (Blt_Op)SetOp, 3, 0, "tagOrIdx ?key value...?",},
    {"tag", 2, (Blt_Op)TagOp, 3, 0, "args...",},
    {"trace", 2, (Blt_Op)TraceOp, 2, 0, "args...",},
    {"unset", 3, (Blt_Op)UnsetOp, 3, 0, "tagOrIdx ?key...?",},
#ifdef notdef
    {"-ancestor", 2, (Blt_Op)AncestorOp, 4, 4, "node1 node2",},
    {"-apply", 1, (Blt_Op)ApplyOp, 3, 0, "first last ?switches?",},
    {"column", 2, (Blt_Op)ColumnOp, 2, 0, "args...", },
    {"copy", 2, (Blt_Op)CopyOp, 4, 0, 
	"srcNode ?destTuple? destNode ?switches?",},
    {"dump", 4, (Blt_Op)DumpOp, 3, 3, "first ?last?",},
    {"dumpfile", 5, (Blt_Op)DumpfileOp, 4, 4, "first ?last? fileName",},
    {"find", 4, (Blt_Op)FindOp, 3, 0, "node ?switches?",},
    {"index", 3, (Blt_Op)IndexOp, 3, 3, "name",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0, "parent ?switches?",},
    {"-is", 2, (Blt_Op)IsOp, 2, 0, "oper args...",},
    {"keys", 1, (Blt_Op)KeysOp, 3, 0, "node ?node...?",},
    {"-label", 3, (Blt_Op)LabelOp, 3, 4, "node ?newLabel?",},
    {"move", 1, (Blt_Op)MoveOp, 4, 0, "node newParent ?switches?",},
    {"-next", 4, (Blt_Op)NextOp, 3, 3, "node",},
    {"-position", 2, (Blt_Op)PositionOp, 3, 0, "?switches? node...",},
    {"-previous", 5, (Blt_Op)PreviousOp, 3, 3, "node",},
    {"range", 2, (Blt_Op)ChildrenOp, 4, 4, "first last",},
    {"restore", 7, (Blt_Op)RestoreOp, 4, 4, "node dataString",},
    {"restorefile", 8, (Blt_Op)RestorefileOp, 4, 4, "node fileName",},
    {"width", 2, (Blt_Op)SizeOp, 3, 3, "?newWidth?",},
    {"length", 2, (Blt_Op)SizeOp, 3, 3, "?newLength?",},
    {"sort", 2, (Blt_Op)SortOp, 3, 0, "?flags...?",},
    {"type", 2, (Blt_Op)TypeOp, 4, 4, "key",},
#endif
};

static int nTupleOps = sizeof(tupleOps) / sizeof(Blt_OpSpec);

static int
TupleInstObjCmd(
    ClientData clientData,	/* Pointer to the command structure. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST *objv)	/* Vector of argument strings. */
{
    Blt_Op proc;
    TupleCmd *cmdPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nTupleOps, tupleOps, BLT_OP_ARG1, objc, 
	objv, BLT_OP_LINEAR_SEARCH);
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
 * TupleInstDeleteProc --
 *
 *	Deletes the command associated with the tuple.  This is
 *	called only when the command associated with the tuple is
 *	destroyed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The tuple object is released and bookkeeping data for traces
 *	and notifiers are freed.
 *
 * ----------------------------------------------------------------------
 */
static void
TupleInstDeleteProc(ClientData clientData)
{
    TupleCmd *cmdPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    TraceInfo *tracePtr;
    NotifyInfo *notifyPtr;

    if (cmdPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(cmdPtr->instTablePtr, cmdPtr->hashPtr);
    }
    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	tracePtr = Blt_GetHashValue(hPtr);
	DeleteTrace(tracePtr);
    }
    Blt_DeleteHashTable(&cmdPtr->traceTable);
    for (hPtr = Blt_FirstHashEntry(&cmdPtr->notifyTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	notifyPtr = Blt_GetHashValue(hPtr);
	DeleteNotifier(notifyPtr);
    }
    Tcl_DecrRefCount(cmdPtr->emptyObjPtr);
    Blt_DeleteHashTable(&cmdPtr->notifyTable);
    Blt_TupleReleaseTable(cmdPtr->table);
    Blt_Free(cmdPtr);
}
/*
 *----------------------------------------------------------------------
 *
 * TupleCreateOp --
 *
 *	Creates a new instance of a tuple object.  This routine
 *	ensures that instance and object names are the same.  For
 *	example, you can't create an instance with the name of an
 *	object that already exists.  And because each instance has a
 *	Tcl command associated with it that is used to access the
 *	object, we also check more generally that is it also not the
 *	name of an existing Tcl command.  
 *
 *	Instance names as namespace-qualified.  If no namespace is
 *	designated, it is assumed that instance is to be created in
 *	the current namespace (not the global namespace).
 *	
 * Results:
 *	A standard Tcl result.  If the instance is successfully created,
 *	the namespace-qualified name of the instance is returned. If not,
 *	then TCL_ERROR is returned and an error message is left in the 
 *	interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TupleCreateOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TupleCmdInterpData *interpDataPtr = clientData;
    CONST char *instName;
    Tcl_DString dString;
    Blt_TupleTable table;

    instName = NULL;
    if (objc == 3) {
	instName = Tcl_GetString(objv[2]);
    }
    Tcl_DStringInit(&dString);
    if (instName == NULL) {
	instName = GenerateName(interp, "", "", &dString);
    } else {
	char *p;

	p = strstr(instName, "#auto");
	if (p != NULL) {
	    *p = '\0';
	    instName = GenerateName(interp, instName, p + 5, &dString);
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
	    if (Blt_ParseQualifiedName(interp, instName, &nsPtr, &name) 
		!= TCL_OK) {
		Tcl_AppendResult(interp, "can't find namespace in \"", instName,
			 "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    if (nsPtr == NULL) {
		nsPtr = Tcl_GetCurrentNamespace(interp);
	    }
	    instName = Blt_GetQualifiedName(nsPtr, name, &dString);
	    /* 
	     * Check if the command already exists. 
	     */
	    if (Tcl_GetCommandInfo(interp, (char *)instName, &cmdInfo)) {
		Tcl_AppendResult(interp, "a command \"", instName,
				 "\" already exists", (char *)NULL);
		goto error;
	    }
	    if (Blt_TupleTableExists(interp, instName)) {
		Tcl_AppendResult(interp, "a tuple \"", instName, 
			"\" already exists", (char *)NULL);
		goto error;
	    }
	} 
    } 
    if (instName == NULL) {
	goto error;
    }
    if (Blt_TupleCreateTable(interp, instName, &table) == TCL_OK) {
	TupleCmd *cmdPtr;

	cmdPtr = NewTupleCmd(interp, interpDataPtr, table, instName);
	Tcl_SetResult(interp, (char *)instName, TCL_VOLATILE);
	Tcl_DStringFree(&dString);
	return TCL_OK;
    }
 error:
    Tcl_DStringFree(&dString);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TupleDestroyOp --
 *
 *	Deletes one or more instances of tuple objects.  The deletion
 *	process is done by deleting the Tcl command associated with
 *	the object.  
 *	
 * Results:
 *	A standard Tcl result.  If one of the names given doesn't
 *	represent an instance, TCL_ERROR is returned and an error
 *	message is left in the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
static int
TupleDestroyOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TupleCmdInterpData *interpDataPtr = clientData;
    TupleCmd *cmdPtr;
    char *string;
    register int i;

    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	cmdPtr = GetTupleCmd(interpDataPtr, interp, string);
	if (cmdPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a tuple named \"", string,
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
 * TupleNamesOp --
 *
 *	Returns the names of all the tuple-object instances matching a
 *	given pattern.  If no pattern argument is provided, then all
 *	object names are returned.  The names returned are namespace
 *	qualified.
 *	
 * Results:
 *	Always returns TCL_OK.  The names of the matching objects is
 *	returned via the interpreter result.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
TupleNamesOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TupleCmdInterpData *interpDataPtr = clientData;
    TupleCmd *cmdPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *objPtr, *listObjPtr;
    Tcl_DString dString;
    char *qualName;

    Tcl_DStringInit(&dString);
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&interpDataPtr->instTable, &cursor);
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
 * TupleObjCmd --
 *
 *---------------------------------------------------------------------- 
 */
static Blt_OpSpec tupleCmdOps[] =
{
    {"create", 1, (Blt_Op)TupleCreateOp, 2, 3, "?name?",},
    {"destroy", 1, (Blt_Op)TupleDestroyOp, 3, 0, "name...",},
    {"names", 1, (Blt_Op)TupleNamesOp, 2, 3, "?pattern?...",},
};

static int nCmdOps = sizeof(tupleCmdOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
TupleObjCmd(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;

    proc = Blt_GetOpFromObj(interp, nCmdOps, tupleCmdOps, BLT_OP_ARG1, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, objc, objv);
}

/*
 * -----------------------------------------------------------------------
 *
 * TupleInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the "tuple" command
 *	is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the hash table managing all tuple names.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TupleInterpDeleteProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp)
{
    TupleCmdInterpData *interpDataPtr = clientData;

    /* All tuple instances should already have been destroyed when
     * their respective Tcl commands were deleted. */
    Blt_DeleteHashTable(&interpDataPtr->instTable);
    Tcl_DeleteAssocData(interp, TUPLE_THREAD_KEY);
    Blt_Free(interpDataPtr);
}


/*
 * -----------------------------------------------------------------------
 *
 * Blt_TupleInit --
 *
 *	This procedure is invoked to initialize the "tuple" command.
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
Blt_TupleInit(Tcl_Interp *interp)
{
    TupleCmdInterpData *interpDataPtr;	/* Interpreter-specific data. */
    static Blt_ObjCmdSpec cmdSpec = { 
	"tuple", TupleObjCmd, 
    };
    interpDataPtr = GetTupleCmdInterpData(interp);
    cmdSpec.clientData = interpDataPtr;
    if (Blt_InitObjCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_TupleCmdGetToken(
    Tcl_Interp *interp,
    CONST char *string,
    Blt_TupleTable  *tablePtr)
{
    TupleCmdInterpData *interpDataPtr;
    TupleCmd *cmdPtr;

    interpDataPtr = GetTupleCmdInterpData(interp);
    cmdPtr = GetTupleCmd(interpDataPtr, interp, string);
    if (cmdPtr == NULL) {
	Tcl_AppendResult(interp, "can't find a tuple object associated with \"",
		 string, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *tablePtr = cmdPtr->table;
    return TCL_OK;
}

/* Dump tuple to dbm */
/* Convert node data to datablock */

#endif /* NO_TUPLE */
