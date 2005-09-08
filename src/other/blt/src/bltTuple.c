#include <bltInt.h>
#include <bltTuple.h>

/*
 *  array or row pointers
 *   _
 *  |_---> [row index
 *  |_     [#columns             array of Tcl_Objs
 *  |_     [tuple pointer ---> [ . . . . . ]
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *  |_
 *
 */

enum TagTypes { TAG_TYPE_NONE, TAG_TYPE_ALL, TAG_TYPE_TAG };

#define TUPLE_THREAD_KEY		"BLT Tuple Data"
#define TUPLE_MAGIC		((unsigned int) 0x46170277)
#define TUPLE_DESTROYED		(1<<0)

#define TRACE_IGNORE		(0)
#define TRACE_OK		(1)

#define VALUE(r,c)	\
	((((c)->index < (r)->nColumns) && ((r)->data != NULL)) \
		? (r)->data[(c)->index] : NULL)

typedef struct Blt_TupleColumnStruct Column;
typedef struct Blt_TupleRowStruct Row;
typedef struct Blt_TupleTagTableStruct TagTable;
typedef struct Blt_TupleTraceStruct Trace;
typedef struct Blt_TupleClientStruct TupleClient;
typedef struct Blt_TupleNotifierStruct Notifier;

struct Blt_TupleRowStruct {
    Tcl_Obj **data;		/* Array of Tcl_Objs represents the tuple's
				 * data. */
    unsigned short nColumns;	/* # columns allocated for this tuple. */
    unsigned short flags;	/* Special flags for this tuple. */
    unsigned int index;		/* Index of the tuple in the array. */
};

struct Blt_TupleColumnStruct {
    char *key;			/* Name of column. */
    int type;			/* Type of column:
				 * TYPE_INTEGER
				 * TYPE_NUMBER
				 * TYPE_FLOAT
				 * TYPE_STRING
				 * TYPE_BOOLEAN
				 * TYPE_LIST
				 * TYPE_ARRAY
				 */
    int nz;			/* Number of (non-NULL) defined values
				 * in the column. */
    unsigned int index;		/* Location of column in tuple. */
    unsigned int flags;		/* Special flags for this column. */
    Blt_HashEntry *hashPtr;

    /* Vector linkage */
};

struct Blt_TupleTagTableStruct {
    Blt_HashTable tagTable;
    int refCount;
};

typedef struct {
    Blt_HashTable instTable;	/* Table of tuple objects. */
    unsigned int nextId;
    Tcl_Interp *interp;
} TupleInterpData;

typedef struct {
    Row *rowPtr;
    Column *columnPtr;
} BusyKey;

/*
Blt_TupleRemoveColumn(table, key);
Blt_TupleAddColumn(table, key);
Blt_TupleRenameColumn(table, key, newKey);
Blt_TupleSetColumnType(table, key, type);
Blt_TupleGetColumnType(table, key, typePtr);
unsigned int Blt_TupleColumnLength(table, key, flags);
Tcl_Obj **Blt_TupleGetColumnVector(tuple, key);
unsigned int Blt_TupleLength(table);
unsigned int Blt_TupleSize(table);

Blt_TupleTuple Blt_TableGetTuple(table, index);
Blt_TupleSetTuple(table, index, tuple);
Blt_TupleTuple Blt_TableAppendTuple(tuple);
Blt_TableTuple Blt_TupleInsertTuple(table, newPosition);
Blt_TupleDeleteTuple(table, tuple);
Blt_TupleMoveTuple(table, tuple, newPosition);
*/

/*
 * Blt_TupleObject --
 *
 *	Structure representing a tuple object. 
 *
 *	Tuple objects are uniquely identified by a combination of
 *	their name and the originating namespace.  Two objects in the
 *	same interpreter can have similar names but must reside in
 *	different namespaces.
 *
 *	The tuple object is an array of tuples. A tuple itself is an
 *	array of Tcl_Objs representing data for each column (or key).
 *	Value are identified by their column name.  A tuple does not
 *	need to contain values for all columns.  Undefined values
 *	(Tcl_Objs) are NULL.
 *
 *	A tuple object can be shared by several clients.  When a
 *	client wants to use a tuple object, it is given a token that
 *	represents the table.  The object tracks its clients by its
 *	token. When all clients have released their tokens, the tuple
 *	object is automatically destroyed.
 * */

typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with this
				 * object. */ 
     
    char *name;			/* Name of tuple object. */
    
    Tcl_Namespace *nsPtr;
    
    Blt_HashEntry *hashPtr;	/* Pointer to this object in object
				 * hash table. */
    Blt_HashTable *tablePtr;
    
    unsigned int columnsAllocated;
    unsigned int rowsAllocated;
    unsigned int nColumns;	/* Number columns in each tuple. */
    unsigned int nRows;		/* Number of rows (tuples). */
    Row **rows;
    Column **columns;		/* Array of column pointers. Supports
				 * mappings of numeric indices back to
				 * columns. */
    Blt_HashTable columnTable;	/* Hash table of columns. Supports
				 * mappings of string names back to
				 * columns. */
    
    Blt_Pool rowPool;		/* Pool that allocates row containers. */

    char *sortCmd;		/* Tcl command to invoke to sort
				 * entries. */
    
    Blt_Chain *clients;		/* List of clients using this table */
    
    unsigned int flags;		/* Internal flags. See definitions
				 * below. */
    unsigned int notifyFlags;	/* Notification flags. See definitions
				 * below. */
    Blt_Chain *notifiers;	/* Chain of node event handlers. */
    Blt_Chain *traces;		/* Chain of callbacks for value changes. */
    Blt_HashTable busyTable;	/* Hash table tracking the cells that
				 * have traces currently active. */
    int notifyHold;
} TupleObject;

/*
 * Blt_TupleClientStruct --
 *
 *	A tuple object can be shared by several clients.  Each client
 *	allocates this structure which acts as a ticket for using the
 *	table.  Clients can designate notifier routines that are
 *	automatically invoked by the tuple object whenever the object
 *	is changed is specific ways by other clients.  
 */
struct Blt_TupleClientStruct {
    unsigned int magic;		/* Magic value indicating whether a
				 * generic pointer is really a tuple
				 * token or not. */
    Blt_ChainLink *linkPtr;	/* Pointer into the server's chain of
				 * clients. */
    Tcl_Interp *interp;
    TupleObject *tupleObjPtr;	/* Pointer to the structure containing
				 * the master information about the
				 * table used by the client.  If NULL,
				 * this indicates that the table has
				 * been destroyed (but as of yet, this
				 * client hasn't recognized it). */

    TagTable *tagTablePtr;
};

#define Blt_TupleTableLength(table)	((table)->nRows)
#define Blt_TupleTableWidth(table)	((table)->nColumns)

static Tcl_InterpDeleteProc TupleInterpDeleteProc;

static Row *
NewRow(TupleObject *tupleObjPtr, unsigned int rowIndex)
{
    Row *rowPtr;

    rowPtr = Blt_PoolAllocItem(tupleObjPtr->rowPool, sizeof(Row));
    rowPtr->data = NULL;
    rowPtr->nColumns = 0;
    rowPtr->index = rowIndex;
    return rowPtr;
}

static void
DeleteRow(TupleObject *tupleObjPtr, Row *rowPtr)
{
    if (rowPtr->data != NULL) {
	int i;
	Tcl_Obj *objPtr;

	for(i = 0; i < rowPtr->nColumns; i++) {
	    objPtr = rowPtr->data[i];
	    if (objPtr != NULL) {
		Tcl_DecrRefCount(objPtr);
	    }
	}
	Blt_Free(rowPtr->data);
    }
    Blt_PoolFreeItem(tupleObjPtr->rowPool, (char *)rowPtr);
}

/*
 * --------------------------------------------------------------
 *
 * NewTupleObject --
 *
 *	Creates and initializes a new tuple object. 
 *
 * Results:
 *	Returns a pointer to the new object is successful, NULL
 *	otherwise.  If a tuple object can't be generated,
 *	interp->result will contain an error message.
 *
 * -------------------------------------------------------------- 
 */
static TupleObject *
NewTupleObject(
    TupleInterpData *dataPtr, 
    Tcl_Interp *interp, 
    CONST char *name)
{
    TupleObject *tupleObjPtr;
    int isNew;
    Blt_HashEntry *hPtr;

    tupleObjPtr = Blt_Calloc(1, sizeof(TupleObject));
    if (tupleObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate tuple object", (char *)NULL);
	return NULL;
    }
    tupleObjPtr->interp = interp;
    tupleObjPtr->clients = Blt_ChainCreate();
    tupleObjPtr->traces = Blt_ChainCreate();
    tupleObjPtr->notifiers = Blt_ChainCreate();
    tupleObjPtr->notifyFlags = 0;
    Blt_InitHashTableWithPool(&tupleObjPtr->busyTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tupleObjPtr->columnTable, BLT_STRING_KEYS);

    tupleObjPtr->tablePtr = &dataPtr->instTable;
    hPtr = Blt_CreateHashEntry(tupleObjPtr->tablePtr, name, &isNew);
    tupleObjPtr->hashPtr = hPtr;
    tupleObjPtr->name = Blt_GetHashKey(tupleObjPtr->tablePtr, hPtr);
    Blt_SetHashValue(hPtr, tupleObjPtr);

    return tupleObjPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyTupleObject --
 *

 *	Destroys the tuple object.  This is the final clean up of the
 *	object.  The object's entry is removed from the hash table of
 *	tuples.
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
DestroyTupleObject(TupleObject *tupleObjPtr)
{
    Blt_ChainLink *linkPtr;
    Trace *tracePtr;
    struct Blt_TupleNotifierStruct *notifyPtr;
	
    tupleObjPtr->flags |= TUPLE_DESTROYED;

    for (linkPtr = Blt_ChainFirstLink(tupleObjPtr->traces); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tracePtr = Blt_ChainGetValue(linkPtr);
	tracePtr->linkPtr = NULL;
	Blt_TupleDeleteTrace(tracePtr);
    }
    for (linkPtr = Blt_ChainFirstLink(tupleObjPtr->notifiers); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	notifyPtr->linkPtr = NULL;
	Blt_TupleDeleteNotifier(notifyPtr);
    }
    Blt_ChainDestroy(tupleObjPtr->traces);
    Blt_ChainDestroy(tupleObjPtr->notifiers);
    Blt_ChainDestroy(tupleObjPtr->clients);

    if (tupleObjPtr->rows != NULL) {
	int i;

	for (i = 0; i < tupleObjPtr->nRows; i++) {
	    DeleteRow(tupleObjPtr, tupleObjPtr->rows[i]);
	}
	Blt_Free(tupleObjPtr->rows);
    }
    tupleObjPtr->nRows = 0;

    Blt_PoolDestroy(tupleObjPtr->rowPool);
    Blt_DeleteHashTable(&tupleObjPtr->columnTable);
    Blt_DeleteHashTable(&tupleObjPtr->busyTable);

    if (tupleObjPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(tupleObjPtr->tablePtr, tupleObjPtr->hashPtr); 
    }
    Blt_Free(tupleObjPtr);
}

/*
 * -----------------------------------------------------------------------
 *
 * TupleInterpDeleteProc --
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
TupleInterpDeleteProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp)
{
    TupleInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    TupleObject *tupleObjPtr;
    
    for (hPtr = Blt_FirstHashEntry(&dataPtr->instTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tupleObjPtr = (TupleObject *)Blt_GetHashValue(hPtr);
	tupleObjPtr->hashPtr = NULL;
	DestroyTupleObject(tupleObjPtr);
    }
    Blt_DeleteHashTable(&dataPtr->instTable);
    Tcl_DeleteAssocData(interp, TUPLE_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*
 * --------------------------------------------------------------
 *
 * GetTupleInterpData --
 *
 *	Creates or retrieves data associated with tuple data objects
 *	for a particular thread.  We're using Tcl_GetAssocData rather
 *	than the Tcl thread routines so BLT can work with pre-8.0 
 *	Tcl versions that don't have thread support.
 *
 * Results:
 *	Returns a pointer to the tuple interpreter data.
 *
 * -------------------------------------------------------------- 
 */
static TupleInterpData *
GetTupleInterpData(Tcl_Interp *interp)
{
    Tcl_InterpDeleteProc *proc;
    TupleInterpData *dataPtr;

    dataPtr = (TupleInterpData *)
	Tcl_GetAssocData(interp, TUPLE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TupleInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TUPLE_THREAD_KEY, TupleInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&dataPtr->instTable, BLT_STRING_KEYS);
    }
    return dataPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * NewTagTable --
 *
 *---------------------------------------------------------------------- 
 */
static TagTable *
NewTagTable(void)
{
    TagTable *tablePtr;

    tablePtr = Blt_Malloc(sizeof(TagTable));
    Blt_InitHashTable(&tablePtr->tagTable, BLT_STRING_KEYS);
    tablePtr->refCount = 1;
    return tablePtr;
}

static void
ReleaseTagTable(TagTable *tablePtr)
{
    tablePtr->refCount--;
    if (tablePtr->refCount <= 0) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	Blt_TupleTagEntry *tPtr;

	for(hPtr = Blt_FirstHashEntry(&tablePtr->tagTable, &cursor); 
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tPtr = Blt_GetHashValue(hPtr);
	    Blt_DeleteHashTable(&tPtr->rowTable);
	    Blt_Free(tPtr);
	}
	Blt_DeleteHashTable(&tablePtr->tagTable);
	Blt_Free(tablePtr);
    }
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

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetColumnIndex--
 *
 *	Returns the index of the column by the name given.  Translates
 *	between column names and indices.  Column indices are a direct
 *	lookup while names must be hashed.
 *
 * Results:
 *	A standard Tcl result. Returns the index of via *indexPtr.  
 *	If no column can be found, then TCL_ERROR is returned and
 *	an error message is left in the interpreter result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleGetColumnIndex(
    Tcl_Interp *interp, 
    TupleObject *tupleObjPtr, 
    CONST char *key, 
    unsigned int *indexPtr)
{
    char *left, *right;
    Column *columnPtr;
    Blt_HashEntry *hPtr;
    
    if (ParseParentheses(interp, key, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = '\0';
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	*left = '(';
    } else {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
    }
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find column \"", key, "\" in \"",
			     tupleObjPtr->name, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    columnPtr = Blt_GetHashValue(hPtr);
    *indexPtr = columnPtr->index;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CallTraces --
 *
 *	Examines the traces set for each client of the tuple object 
 *	and fires any matching trace.  
 *
 *	Traces can match on
 *	     row		if the trace was specified by index.
 *	     tag		if the trace was specified by tag.
 *	     key		if the trace was specified with a list of
 *				one or more keys.
 *	     flag		type of trace (read, write, unset, create)
 *
 *	If no matching criteria is specified (no tag, key, or tuple
 *	address) then only the bit flag has to match.
 *
 *	If the TUPLE_TRACE_FOREIGN_ONLY is set in the handler, it
 *	means to ignore actions that are initiated by that client of
 *	the object.  Only actions by other clients are handled.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the tuple table location may be fired.
 *
 *----------------------------------------------------------------------
 */
static void
CallTraces(
    Tcl_Interp *interp,
    TupleClient *clientPtr,	/* Client holding a reference to the
				 * tuple object. */
    Row *rowPtr,		/* Tuple being modified. */
    Column *columnPtr,
    unsigned int flags)
{
    Blt_ChainLink *linkPtr;
    Trace *tracePtr;	
    TupleObject *tupleObjPtr;	/* Tuple that was changed. */

    tupleObjPtr = clientPtr->tupleObjPtr;
    /* For each client, examine its trace handlers. */
    for(linkPtr = Blt_ChainFirstLink(tupleObjPtr->traces); 
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	tracePtr = Blt_ChainGetValue(linkPtr);
	if ((tracePtr->tuple != NULL) && (tracePtr->tuple != rowPtr)) {
	    continue;	/* Row doesn't match. */
	}
	if ((tracePtr->mask & flags) == 0) {
	    continue;	/* Missing the required trace flags. */
	}
	if ((tracePtr->table == clientPtr) && 
	    (tracePtr->mask & TUPLE_TRACE_FOREIGN_ONLY)) {
	    continue;	/* This client initiated the action. */
	}
	if ((tracePtr->withTag != NULL) && 
	    (!Blt_TupleHasTag(tracePtr->table, rowPtr, tracePtr->withTag))){
	    continue;	/* Tag doesn't match. */
	}
	if (tracePtr->keys != NULL) {
	    register CONST char **p;
	    
	    for (p = tracePtr->keys; *p != NULL; p++) {
		if (strcmp(*p, columnPtr->key) == 0) {
		    break;
		}
	    }
	    if (*p == NULL) {
		continue;	/* No keys match. */
	    }
	}
	if ((*tracePtr->proc) (tracePtr->clientData, tupleObjPtr->interp, 
		rowPtr->index, columnPtr->index, flags) != TCL_OK) {
	    if (interp != NULL) {
		Tcl_BackgroundError(interp);
	    }
	}
    }
}

/*
 * --------------------------------------------------------------
 *
 * GetValue --
 *
 *	Gets a scalar Tcl_Obj value from the table at the designated
 *	row and column.  Create and read traces may be fired.
 *
 * Results:
 *	Always returns TCL_OK.  
 *
 * -------------------------------------------------------------- 
 */
static int
GetValue(
    Tcl_Interp *interp,		/* Interpreter to reports result to. */
    TupleClient *clientPtr,	/* Table. */
    Row *rowPtr,		/* Row. */
    Column *columnPtr,		/* Column.  */
    Tcl_Obj **objPtrPtr)	/* (out) Value from table.  */
{
    Blt_HashEntry *hPtr;
    BusyKey busy;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int isNew;
    
    busy.rowPtr = rowPtr;
    busy.columnPtr = columnPtr;
    hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
    if (isNew) {
	CallTraces(interp, clientPtr, rowPtr, columnPtr, TUPLE_TRACE_READ);
    }
    Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);

    /* Access the data value after traces have been called. */
    *objPtrPtr = VALUE(rowPtr, columnPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * SetValue --
 *
 *	Sets a scalar Tcl_Obj value in the table at the designated row
 *	and column.  Write traces may be fired.
 *
 * Results:
 *	Always returns TCL_OK.  
 *
 * -------------------------------------------------------------- 
 */
static int
SetValue(
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Row *rowPtr,
    Column *columnPtr,
    Tcl_Obj *objPtr)
{
    Tcl_Obj *oldObjPtr;
    unsigned int flags;
    Blt_HashEntry *hPtr;
    int isNew;
    BusyKey busy;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;

    if ((rowPtr->nColumns < columnPtr->index) && (rowPtr->data != NULL)) {
	oldObjPtr = rowPtr->data[columnPtr->index];
    } else {
	AllocateRows(tupleObjPtr, rowPtr);
	oldObjPtr = NULL;
    }
    if (objPtr != NULL) {
	Tcl_IncrRefCount(objPtr);
    }
    if (oldObjPtr != NULL) {
	Tcl_DecrRefCount(oldObjPtr);
    }
    rowPtr->data[columnPtr->index] = objPtr;
    flags = TUPLE_TRACE_WRITE;
    if (oldObjPtr == NULL) {
	flags |= TUPLE_TRACE_CREATE;
    }
    busy.rowPtr = rowPtr;
    busy.columnPtr = columnPtr;
    hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
    if (isNew) {
	CallTraces(interp, clientPtr, rowPtr, columnPtr, flags);
    }
    Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * UnsetValue --
 *
 *	Unsets a scalar Tcl_Obj value in the table at the designated
 *	row and column.  Unset traces may be fired.
 *
 * Results:
 *	Always returns TCL_OK.  
 *
 * -------------------------------------------------------------- 
 */
static int
UnsetValue(
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Row *rowPtr,
    Column *columnPtr)
{
    Tcl_Obj *objPtr;

    objPtr = VALUE(rowPtr, columnPtr);
    if (objPtr != NULL) {
	TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
	Blt_HashEntry *hPtr;
	BusyKey busy;
	int isNew;

        Tcl_DecrRefCount(objPtr);
        rowPtr->data[columnPtr->index] = NULL;
	busy.rowPtr = rowPtr;
	busy.columnPtr = columnPtr;
	hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
	if (isNew) {
	    CallTraces(interp, clientPtr, rowPtr, columnPtr, TUPLE_TRACE_UNSET);
	}
	Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);
    }
    return TCL_OK;
}

static int
GetArrayValue(
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Row *rowPtr,
    Column *columnPtr,
    CONST char *elemName,
    Tcl_Obj **objPtrPtr,
    int doTrace)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Tcl_Obj *objPtr;
    Blt_HashTable *tablePtr;
    Blt_HashEntry *hPtr;

    if (doTrace) {
	BusyKey busy;
	int isNew;

	busy.rowPtr = rowPtr;
	busy.columnPtr = columnPtr;
	hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
	if (isNew) {
	    CallTraces(interp, clientPtr, rowPtr, columnPtr, TUPLE_TRACE_READ);
	}
	Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);
    }

    /* Access the data value after traces have been called. */
    objPtr = VALUE(rowPtr, columnPtr);
    if (objPtr != NULL) {
	if (Tcl_IsShared(objPtr)) {
	    Tcl_DecrRefCount(objPtr);
	    objPtr = Tcl_DuplicateObj(objPtr);
	    Tcl_IncrRefCount(objPtr);
	    rowPtr->data[columnPtr->index] = objPtr;
	}
	if (Blt_GetArrayFromObj(interp, objPtr, &tablePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	hPtr = Blt_FindHashEntry(tablePtr, elemName);
	if (hPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find \"", columnPtr->key, "(",
				 elemName, ")\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	objPtr = Blt_GetHashValue(hPtr);
    } 
    *objPtrPtr = objPtr;
    return TCL_OK;
}

static int
SetArrayValue(
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Row *rowPtr,
    Column *columnPtr,
    CONST char *elemName,
    Tcl_Obj *valueObjPtr)
{
    Blt_HashTable *tablePtr;
    Tcl_Obj *arrayObjPtr;
    unsigned int flags;
    Blt_HashEntry *hPtr;
    int isNew;
    BusyKey busy;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;

    flags = TUPLE_TRACE_WRITE;
    arrayObjPtr = VALUE(rowPtr, columnPtr);
    if (arrayObjPtr == NULL) {
	arrayObjPtr = Blt_NewArrayObj(0, (Tcl_Obj **)NULL);
	Tcl_IncrRefCount(arrayObjPtr);
	flags |= TUPLE_TRACE_CREATE;
	AllocateRows(tupleObjPtr, rowPtr);
    } else if (Tcl_IsShared(arrayObjPtr)) {
	Tcl_DecrRefCount(arrayObjPtr);
	arrayObjPtr = Tcl_DuplicateObj(arrayObjPtr);
	Tcl_IncrRefCount(arrayObjPtr);
    }
    rowPtr->data[columnPtr->index] = arrayObjPtr;
    if (Blt_GetArrayFromObj(interp, arrayObjPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_InvalidateStringRep(arrayObjPtr);
    hPtr = Blt_CreateHashEntry(tablePtr, elemName, &isNew);
    assert(hPtr);

    Tcl_IncrRefCount(valueObjPtr);
    if (!isNew) {
	Tcl_Obj *oldValueObjPtr;

	/* An element by the same name already exists. Decrement the
	 * reference count of the old value. */

	oldValueObjPtr = Blt_GetHashValue(hPtr);
	if (oldValueObjPtr != NULL) {
	    Tcl_DecrRefCount(oldValueObjPtr);
	}
    }
    Blt_SetHashValue(hPtr, valueObjPtr);

    /*
     * We don't handle traces on a per array element basis.  Setting
     * any element can fire traces for the value.
     */
    busy.rowPtr = rowPtr;
    busy.columnPtr = columnPtr;
    hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
    if (isNew) {
	CallTraces(interp, clientPtr, rowPtr, columnPtr, flags);
    }
    Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);
    return TCL_OK;
}

static int
UnsetArrayValue( 
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Row *rowPtr,
    Column *columnPtr,
    CONST char *elemName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    BusyKey busy;
    Tcl_Obj *arrayObjPtr, *valueObjPtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int isNew;

    arrayObjPtr = VALUE(rowPtr, columnPtr);
    if (arrayObjPtr == NULL) {
	return TCL_OK;
    }
    if (Tcl_IsShared(arrayObjPtr)) {
	Tcl_DecrRefCount(arrayObjPtr);
	arrayObjPtr = Tcl_DuplicateObj(arrayObjPtr);
	Tcl_IncrRefCount(arrayObjPtr);
    }
    if (Blt_GetArrayFromObj(interp, arrayObjPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    if (hPtr == NULL) {
	return TCL_OK;		/* Element doesn't exist, Ok. */
    }
    valueObjPtr = Blt_GetHashValue(hPtr);
    Tcl_DecrRefCount(valueObjPtr);
    Blt_DeleteHashEntry(tablePtr, hPtr);
    rowPtr->data[columnPtr->index] = NULL;

    /*
     * Un-setting any element in the array can cause the trace on the value
     * to fire.
     */
    busy.rowPtr = rowPtr;
    busy.columnPtr = columnPtr;
    hPtr = Blt_CreateHashEntry(&tupleObjPtr->busyTable, &busy, &isNew);
    if (isNew) {
	CallTraces(interp, clientPtr, rowPtr, columnPtr, TUPLE_TRACE_UNSET);
    }
    Blt_DeleteHashEntry(&tupleObjPtr->busyTable, hPtr);
    return TCL_OK;
}


static int
ExtendRows(TupleObject *tupleObjPtr, int extra)
{
    int nRows;
    int i;

    nRows = tupleObjPtr->nRows + extra;
    if (tupleObjPtr->rowsAllocated < nRows) {
	Row **rows;
	    
	if (tupleObjPtr->rowsAllocated == 0) {
	    tupleObjPtr->rowsAllocated = 32;
	}
	while (tupleObjPtr->rowsAllocated < nRows) {
	    tupleObjPtr->rowsAllocated += tupleObjPtr->rowsAllocated;
	}
	rows = Blt_Realloc(tupleObjPtr->rows, 
			   tupleObjPtr->rowsAllocated * sizeof(Row *));
	if (rows == NULL) {
	    return -1;
	}
	tupleObjPtr->rows = rows;
    }
    for (i = tupleObjPtr->nRows; i < nRows; i++) {
	tupleObjPtr->rows[i] = NewRow(tupleObjPtr, i);
    }
    tupleObjPtr->nRows = nRows;
    return 0;
}

static int
ExtendColumns(TupleObject *tupleObjPtr, Row *rowPtr)
{
    if (tupleObjPtr->columnsAllocated < rowPtr->nColumns) {
	Tcl_Obj **data;
	int i;

	data = Blt_Realloc(rowPtr->data, 
		      tupleObjPtr->columnsAllocated * sizeof(Tcl_Obj *));
	if (data == NULL) {
	    return TCL_ERROR;
	}
	for(i = rowPtr->nColumns; i < tupleObjPtr->columnsAllocated; i++) {
	    data[i] = NULL;
	}
	rowPtr->data = data;
	rowPtr->nColumns = tupleObjPtr->columnsAllocated;
    }
    return TCL_OK;
}


/*
 * ----------------------------------------------------------------------
 *
 * FindTupleObjectInNamespace --
 *
 *	Searches for the tuple object in a given namespace.
 *
 * Results:
 *	Returns a pointer to the tuple object if found, otherwise NULL.
 *
 * ----------------------------------------------------------------------
 */
static TupleObject *
FindTupleObjectInNamespace(
    TupleInterpData *dataPtr,	/* Interpreter-specific data. */
    Tcl_Namespace *nsPtr,
    CONST char *objName)
{
    Tcl_DString dString;
    char *name;
    Blt_HashEntry *hPtr;

    name = Blt_GetQualifiedName(nsPtr, objName, &dString);
    hPtr = Blt_FindHashEntry(&dataPtr->instTable, name);
    Tcl_DStringFree(&dString);
    if (hPtr != NULL) {
	return Blt_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetTupleObject --
 *
 *	Searches for the tuple object associated by the name given.
 *
 * Results:
 *	Returns a pointer to the tuple object if found, otherwise NULL.
 *
 * ----------------------------------------------------------------------
 */
static TupleObject *
GetTupleObject(Tcl_Interp *interp, CONST char *name, unsigned int flags)
{
    CONST char *tupleName;
    Tcl_Namespace *nsPtr;	/* Namespace associated with the tuple object.
				 * If NULL, indicates to look in first the
				 * current namespace and then the global
				 * for the object. */
    TupleInterpData *dataPtr;	/* Interpreter-specific data. */
    TupleObject *tupleObjPtr;

    tupleObjPtr = NULL;
    if (Blt_ParseQualifiedName(interp, name, &nsPtr, &tupleName) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", name, "\"", 
		(char *)NULL);
	return NULL;
    }
    dataPtr = GetTupleInterpData(interp);
    if (nsPtr != NULL) { 
	tupleObjPtr = FindTupleObjectInNamespace(dataPtr, nsPtr, tupleName);
    } else { 
	if (flags & NS_SEARCH_CURRENT) {
	    /* Look first in the current namespace. */
	    nsPtr = Tcl_GetCurrentNamespace(interp);
	    tupleObjPtr = FindTupleObjectInNamespace(dataPtr, nsPtr, tupleName);
	}
	if ((tupleObjPtr == NULL) && (flags & NS_SEARCH_GLOBAL)) {
	    nsPtr = Tcl_GetGlobalNamespace(interp);
	    tupleObjPtr = FindTupleObjectInNamespace(dataPtr, nsPtr, tupleName);
	}
    }
    return tupleObjPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * NewTupleClient --
 *
 *	Creates a new tuple client.  Clients shared a tuple data
 *	object.  They individually manage traces and events on tuple
 *	objects.  Returns a pointer to the malloc'ed structure.  This
 *	is passed to the client as a tuple token.
 *	
 * Results:
 *	A pointer to a TupleClient is returned.  If one can't
 *	be allocated, NULL is returned.  
 *
 *---------------------------------------------------------------------- 
 */
static TupleClient *
NewTupleClient(TupleObject *tupleObjPtr)
{
    TupleClient *clientPtr;

    clientPtr = Blt_Calloc(1, sizeof(TupleClient));
    if (clientPtr != NULL) {
	clientPtr->magic = TUPLE_MAGIC;
	clientPtr->linkPtr = Blt_ChainAppend(tupleObjPtr->clients, clientPtr);
	clientPtr->tupleObjPtr = tupleObjPtr;
	clientPtr->tagTablePtr = NewTagTable();
    }
    return clientPtr;
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
    Notifier *notifyPtr = clientData;
    int result;

    notifyPtr->notifyPending = FALSE;
    notifyPtr->mask |= TUPLE_NOTIFY_ACTIVE;
    result = (*notifyPtr->proc)(notifyPtr->clientData, &notifyPtr->event);
    notifyPtr->mask &= ~TUPLE_NOTIFY_ACTIVE;
    if (result != TCL_OK) {
	Tcl_BackgroundError(notifyPtr->interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyClients --
 *
 *	Traverses the list of event callbacks and checks if one
 *	matches the given event.  A client may trigger an action that
 *	causes the tuple object to notify it.  This can be prevented
 *	by setting the TUPLE_NOTIFY_FOREIGN_ONLY bit in the event
 *	handler.
 *
 *	If a matching handler is found, a callback may be called either
 *	immediately or at the next idle time depending upon the
 *	TUPLE_NOTIFY_WHENIDLE bit.  
 *
 *	Since a handler routine may trigger yet another call to
 *	itself, callbacks are ignored while the event handler is
 *	executing.
 *	
 *---------------------------------------------------------------------- 
 */
static void
NotifyClients(
    TupleClient *sourcePtr,
    TupleObject *tupleObjPtr,
    unsigned int eventFlag)
{
    Blt_ChainLink *linkPtr;
    Blt_TupleNotifyEvent event;
    Notifier *notifyPtr;

    event.type = eventFlag;

    /* Issue callbacks indicating that the structure of the tuple
     * table has changed.  */
    for (linkPtr = Blt_ChainFirstLink(tupleObjPtr->notifiers);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if ((notifyPtr->mask & TUPLE_NOTIFY_ACTIVE) ||
	    (notifyPtr->mask & eventFlag) == 0) {
	    continue;		/* Ignore callbacks that are generated
				 * inside of a notify handler routine. */
	}
	if ((notifyPtr->table == sourcePtr) && 
	    (notifyPtr->mask & TUPLE_NOTIFY_FOREIGN_ONLY)) {
	    continue;		/* Don't notify yourself. */
	}
	event.table = notifyPtr->table;
	if (notifyPtr->mask & TUPLE_NOTIFY_WHENIDLE) {
	    if (!notifyPtr->notifyPending) {
		notifyPtr->notifyPending = TRUE;
		notifyPtr->event = event;
		Tcl_DoWhenIdle(NotifyIdleProc, notifyPtr);
	    }
	} else {
	    int result;

	    notifyPtr->mask |= TUPLE_NOTIFY_ACTIVE;
	    result = (*notifyPtr->proc) (notifyPtr->clientData, &event);
	    notifyPtr->mask &= ~TUPLE_NOTIFY_ACTIVE;
	    if (result != TCL_OK) {
		Tcl_BackgroundError(notifyPtr->interp);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleCreateObject --
 *
 *	Creates a tuple object by the designated name.  It's an error
 *	if a tuple object already exists by that name.  After
 *	successfully creating the object, the caller must then call
 *	Blt_TupleGetToken to allocate a token to share and manipulate
 *	the object.
 *
 * Results:
 *	A standard Tcl result.  If successful, a new tuple object is
 *	created and TCL_OK is returned.  If an object already exists
 *	or the tuple object can't be allocated, then TCL_ERROR is
 *	returned and an error message is left in the interpreter.
 *
 * Side Effects:
 *	A new tuple object is created.
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TupleCreateTable(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    CONST char *name,		/* Name of tuple in namespace.  Object
				 * must not already exist. */
    TupleClient **clientPtrPtr)	/* (out) Client token of newly created
				 * tuple object.  Releasing the token
				 * will free the tuple.  If NULL, no
				 * token is generated. */
{
    Tcl_DString dString;
    Tcl_Namespace *nsPtr;
    TupleInterpData *dataPtr;
    TupleObject *tupleObjPtr;
    CONST char *tupleName;
    char string[200];

    dataPtr = GetTupleInterpData(interp);
    if (name != NULL) {
	/* Does an object by this name already exist in the current
	 * namespace? */
	tupleObjPtr = GetTupleObject(interp, name, NS_SEARCH_CURRENT);
	if (tupleObjPtr != NULL) {
	    Tcl_AppendResult(interp, "a tuple object \"", name,
		     "\" already exists", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	/* Generate a unique name in the current namespace. */
	do  {
	    sprintf(string, "tuple%d", dataPtr->nextId++);
	} while (GetTupleObject(interp, name, NS_SEARCH_CURRENT) != NULL);
	name = string;
    } 
    /* 
     * Tear apart and put back together the namespace-qualified name 
     * of the object.  This is to ensure that naming is consistent.
     */ 
    tupleName = name;
    if (Blt_ParseQualifiedName(interp, name, &nsPtr, &tupleName) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", name, "\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (nsPtr == NULL) {
	/* 
	 * Note that unlike Tcl_CreateCommand, an unqualified name
	 * doesn't imply the global namespace, but the current one.  
	 */
	nsPtr = Tcl_GetCurrentNamespace(interp);
    }
    name = Blt_GetQualifiedName(nsPtr, tupleName, &dString);
    tupleObjPtr = NewTupleObject(dataPtr, interp, name);
    if (tupleObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate tuple object \"", name, "\"", 
		(char *)NULL);
	Tcl_DStringFree(&dString);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&dString);
    if (clientPtrPtr != NULL) {
	TupleClient *clientPtr;
	
	clientPtr = NewTupleClient(tupleObjPtr);
	if (clientPtr == NULL) {
	    Tcl_AppendResult(interp, "can't allocate tuple token", 
		(char *)NULL);
	    return TCL_ERROR;
	}
	*clientPtrPtr = clientPtr;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetToken --
 *
 *	Allocates a token for the tuple object designated by name.
 *	It's an error if no tuple object exists by that name.  The
 *	token returned is passed to various routines to manipulate the
 *	object.  Traces and event notifications are also made through
 *	the token.
 *
 * Results:
 *	A new token is returned representing the tuple object.  
 *
 * Side Effects:
 *	If this is the remaining client, then the tuple object itself
 *	is freed.
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TupleGetTable(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    CONST char *name,		/* Name of tuple object in namespace. */
    TupleClient **clientPtrPtr)
{
    TupleClient *clientPtr;
    TupleObject *tupleObjPtr;

    tupleObjPtr = GetTupleObject(interp, name, NS_SEARCH_BOTH);
    if (tupleObjPtr == NULL) {
	Tcl_AppendResult(interp, "can't find a tuple object \"", name, "\"", 
		 (char *)NULL);
	return TCL_ERROR;
    }
    clientPtr = NewTupleClient(tupleObjPtr);
    if (clientPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate token for tuple \"", name, 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    *clientPtrPtr = clientPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleReleaseToken --
 *
 *	Releases the tuple token, indicating this the client is no
 *	longer using the object. The client is removed from the tuple
 *	object's client list.  If this is the last client, then the
 *	object itself is destroyed and memory is freed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If this is the remaining client, then the tuple object itself
 *	is freed.
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TupleReleaseTable(TupleClient *clientPtr)
{
    TupleObject *tupleObjPtr;
    Blt_ChainLink *linkPtr;
    Notifier *notifyPtr;
    Trace *tracePtr;

    if (clientPtr->magic != TUPLE_MAGIC) {
	fprintf(stderr, "invalid tuple object token 0x%lx\n", 
		(unsigned long)clientPtr);
	return;
    }
    /* Remove any traces that were set by this client. */
    for (linkPtr = Blt_ChainFirstLink(tupleObjPtr->traces); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	tracePtr = Blt_ChainGetValue(linkPtr);
	if (tracePtr->table == clientPtr) {
	    Blt_TupleDeleteTrace(tracePtr);
	}
    }
    /* Also remove all event handlers created by this client. */
    for(linkPtr = Blt_ChainFirstLink(tupleObjPtr->notifiers); 
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	notifyPtr = Blt_ChainGetValue(linkPtr);
	if (notifyPtr->table == clientPtr) {
	    Blt_TupleDeleteNotifier(notifyPtr);
	}
    }

    if (clientPtr->tagTablePtr != NULL) {
	ReleaseTagTable(clientPtr->tagTablePtr);
    }
    tupleObjPtr = clientPtr->tupleObjPtr;
    if (tupleObjPtr != NULL) {
	/* Remove the client from the server's list */
	Blt_ChainDeleteLink(tupleObjPtr->clients, clientPtr->linkPtr);
	if (Blt_ChainGetLength(tupleObjPtr->clients) == 0) {
	    DestroyTupleObject(tupleObjPtr);
	}
    }
    clientPtr->magic = 0;
    Blt_Free(clientPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleTableExists --
 *
 *	Indicates if a tuple object by the given name exists in either
 *	the current or global namespace.
 *
 * Results:
 *	Returns 1 if a tuple object exists and 0 otherwise.
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TupleTableExists(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    CONST char *name)		/* Name of tuple object in the
				 * designated namespace. */
{
    TupleObject *tupleObjPtr;

    tupleObjPtr = GetTupleObject(interp, name, NS_SEARCH_BOTH);
    if (tupleObjPtr == NULL) {
	Tcl_ResetResult(interp);
	return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleCreateNotifier --
 *
 *	Creates an notifier using the following three pieces of
 *	information: 
 *		1. C function pointer, 
 *		2. one-word of data passed on each call, and 
 *		3. event mask indicating which events are of interest.  
 *	If an event already exists matching all of the above criteria,
 *	it is repositioned on the end of the event handler list.  This
 *	means that it will be the last to fire.
 *
 * Results:
 *      Returns a pointer to the event handler.
 *
 * Side Effects:
 *	Memory for the event handler is possibly allocated.
 *
 *---------------------------------------------------------------------- 
 */
Blt_TupleNotifier
Blt_TupleCreateNotifier(
    TupleClient *clientPtr,
    unsigned int mask,
    Blt_TupleNotifyEventProc *proc,
    ClientData clientData)
{
    Notifier *notifyPtr;

    notifyPtr = Blt_Malloc(sizeof (Notifier));
    assert(notifyPtr);
    notifyPtr->proc = proc;
    notifyPtr->clientData = clientData;
    notifyPtr->mask = mask;
    notifyPtr->notifyPending = FALSE;
    notifyPtr->interp = clientPtr->tupleObjPtr->interp;
    notifyPtr->linkPtr = Blt_ChainAppend(clientPtr->tupleObjPtr->notifiers, 
	notifyPtr);
    return notifyPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleDeleteNotifier --
 *
 *	Removes the event handler designated by following three pieces
 *	of information: 1. C function pointer, 2. one-word of data
 *	passed on each call, and 3. event mask indicating which events
 *	are of interest.
 *
 * Results:
 *      Nothing.
 *
 * Side Effects:
 *	Memory for the event handler is freed.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TupleDeleteNotifier(Notifier *notifyPtr)
{
    if (notifyPtr->notifyPending) {
	Tcl_CancelIdleCall(NotifyIdleProc, notifyPtr);
    }
    if (notifyPtr->linkPtr != NULL){
	Blt_ChainDeleteLink(notifyPtr->table->tupleObjPtr->notifiers, 
		    notifyPtr->linkPtr);
	Blt_Free(notifyPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleRowIndex --
 *
 *	Returns the index of the tuple.
 *
 * Results:
 *	Returns the row index of the tuple.
 *
 *----------------------------------------------------------------------
 */
unsigned int
Blt_TupleRowIndex(Row *rowPtr)
{
    return rowPtr->index;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetTupleByIndex --
 *
 *	Returns the tuple at the row designated int the tuple table.
 *
 * Results:
 *	Returns the tuple at the row.  If the index is out of range
 *	then NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_Tuple
Blt_TupleGetTupleByIndex(TupleClient *clientPtr, unsigned int row)
{
    if (row >= clientPtr->tupleObjPtr->nRows) {
	return NULL;
    }
    return clientPtr->tupleObjPtr->rows[row];
}

Blt_Tuple
Blt_TupleNextTuple(TupleClient *clientPtr, Row *rowPtr)
{
    return Blt_TupleGetTupleByIndex(clientPtr, rowPtr->index + 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetColumnKey --
 *
 *	Returns the key of the column with the given index.  This is a
 *	helper routine to translate between column indices and keys.
 *
 * Results:
 *	Returns the key of the column with the given index.  If the
 *	index is invalid, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
CONST char *
Blt_TupleGetColumnKey(TupleClient *clientPtr, int columnIndex)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Column *columnPtr;

    if ((columnIndex < 0) || (columnIndex >= tupleObjPtr->nColumns)) {
	return NULL;
    }
    columnPtr = tupleObjPtr->columns[columnIndex];
    return columnPtr->key;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleAddColumn --
 *
 *	Add a new column to the tuple object.  Rows are slots in an
 *	array of tuples.  The array grows by doubling its size, so
 *	there may be more slots than needed (# rows).
 *
 * Results:
 *	Returns TCL_OK is the table is resized and TCL_ERROR if an
 *	not enough memory was available.
 *
 * Side Effects:
 *	If more rows are needed, the array which holds the tuples is
 *	reallocated by doubling its size.  Storage for the tuples
 *	isn't allocated until the tuple is needed.
 *
 *---------------------------------------------------------------------- 
 */
unsigned int
Blt_TupleAddColumn(
    TupleClient *clientPtr,
    CONST char *key,
    int *isNewPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Blt_HashEntry *hPtr;
    Column *columnPtr;

    hPtr = Blt_CreateHashEntry(&tupleObjPtr->columnTable, key, isNewPtr);
    if (!*isNewPtr) {
	columnPtr = Blt_GetHashValue(hPtr);
    } else {
	columnPtr = Blt_Calloc(1, sizeof(Column));
	columnPtr->key = Blt_GetHashKey(&tupleObjPtr->columnTable, hPtr);
	columnPtr->type = 0;
	columnPtr->hashPtr = hPtr;
	columnPtr->index = tupleObjPtr->nColumns;
	tupleObjPtr->columns[columnPtr->index] = columnPtr;
	tupleObjPtr->nColumns++;
	NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_CREATE_COLUMN);
    }
    return columnPtr->index;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleAddColumns --
 *
 *	Adds one or more columns to the tuple table.  This creates a
 *	new column.  The tuples are individually resized when new data
 *	is added to them.
 *
 * Results:
 *	Returns TCL_OK is the tuple is resized and TCL_ERROR if an
 *	not enough memory was available.
 *
 * Side Effects:
 *	If more rows are needed, the array which holds the tuples is
 *	reallocated by doubling its size.  Storage for the tuples
 *	isn't allocated until the tuple is needed.
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TupleAddColumns(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    char **keys)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int i;
    int nColumns, extra;
    char **p;
    Blt_HashEntry *hPtr;
    int isNew;

    extra = 0;
    for (p = keys; *p != NULL; p++) {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, *p);
	if (hPtr != NULL) {
	    Tcl_AppendResult(interp, "a column \"", *p, 
			"\" already exists in \"", tupleObjPtr->name, "\"", 
			(char *)NULL);
	    return TCL_ERROR;
	}
	extra++;		/* Count the number of new columns. */
    }
    nColumns = tupleObjPtr->nColumns + extra;
    if (nColumns >= tupleObjPtr->columnsAllocated) {
	Column **columns;
	int columnsAllocated;

	columnsAllocated = tupleObjPtr->columnsAllocated;
	while (nColumns >= columnsAllocated) {
	    columnsAllocated += columnsAllocated;
	}
	columns = Blt_Realloc(tupleObjPtr->columns, 
		sizeof(Column *) * columnsAllocated);
	if (columns == NULL) {
	    return TCL_ERROR;
	}
	tupleObjPtr->columns = columns;
	tupleObjPtr->columnsAllocated = columnsAllocated;
    }
    for (p = keys; *p != NULL; p++) {
	Column *columnPtr;

	hPtr = Blt_CreateHashEntry(&tupleObjPtr->columnTable, *p, &isNew);
	columnPtr = Blt_Calloc(1, sizeof(Column));
	columnPtr->key = Blt_GetHashKey(&tupleObjPtr->columnTable, hPtr);
	columnPtr->index = 0;
	columnPtr->type = 0;
	columnPtr->hashPtr = hPtr;
	columnPtr->index = tupleObjPtr->nColumns;
	tupleObjPtr->columns[columnPtr->index] = columnPtr;
	tupleObjPtr->nColumns++;
    }	
    for(i = tupleObjPtr->nColumns; i < tupleObjPtr->columnsAllocated; i++) {
	tupleObjPtr->columns[i] = NULL;
    }
    NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_CREATE_COLUMN);
    return TCL_OK;
}


int
Blt_TupleDeleteColumnByIndex(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    unsigned int column)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Column *columnPtr;

    columnPtr = tupleObjPtr->columns[column];
    if (columnPtr != NULL) {
	Row *rowPtr;
	int i;
	int j, k;

	if (columnPtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(&tupleObjPtr->columnTable, columnPtr->hashPtr);
	}
	for (i = 0; i < tupleObjPtr->nRows; i++) {
	    rowPtr = tupleObjPtr->rows[i];
	    if ((column < rowPtr->nColumns) && (rowPtr->data != NULL)) {
		Tcl_Obj *objPtr;

		/* Release the Tcl_Obj associated with this column and
		 * compress the tuple. */
		objPtr = rowPtr->data[column];
		if (objPtr != NULL) {
		    Tcl_DecrRefCount(objPtr);
		    rowPtr->data[column] = NULL;
		}
		for (j = column, k = column + 1; j < rowPtr->nColumns; 
		     j++, k++) {
		    rowPtr->data[k] = rowPtr->data[j];
		}
		rowPtr->nColumns--;
	    }
	}
	for (j = column, k = column + 1; j < tupleObjPtr->nColumns; j++, k++) {
	    tupleObjPtr->columns[k] = tupleObjPtr->columns[j];
	}
	tupleObjPtr->columns[k] = NULL;
	tupleObjPtr->nColumns--;
	Blt_Free(columnPtr);
	NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_DELETE_COLUMN);
    }
    return TCL_OK;
}

int
Blt_TupleDeleteColumn(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    CONST char *key)
{
    unsigned int column;

    if (Blt_TupleGetColumnIndex(interp, clientPtr->tupleObjPtr, key, &column) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_TupleDeleteColumnByIndex(interp, clientPtr, column);
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleValueExists --
 *
 *	Indicates if a value exists for a given row,column index in
 *	the tuple.  Note that this routine does not fire read traces.
 *
 * Results:
 *	Returns 1 is a value exists, 0 otherwise.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleValueExists(
    TupleClient *clientPtr, 
    Row *rowPtr, 
    CONST char *key)
{
    Blt_HashEntry *hPtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    char *left, *right;
    int result;

    if (rowPtr->index < tupleObjPtr->nRows) {
	return FALSE;
    }
    if (ParseParentheses((Tcl_Interp *)NULL, key, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    result = FALSE;
    if (left != NULL) {
	*left = *right = '\0';
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr != NULL) {
	    Tcl_Obj *objPtr;
	    Column *columnPtr;

	    columnPtr = Blt_GetHashValue(hPtr);
	    if (GetArrayValue((Tcl_Interp *)NULL, clientPtr, rowPtr, columnPtr,
			 left + 1, &objPtr, TRACE_IGNORE) == TCL_OK) {
		result = (objPtr != NULL);
	    }
	}
	*left = '(', *right = ')';
    } else {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr != NULL) {
	    Tcl_Obj *objPtr;
	    Column *columnPtr;

	    columnPtr = Blt_GetHashValue(hPtr);
	    objPtr = VALUE(rowPtr, columnPtr);
	    result = (objPtr != NULL);
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetValue --
 *
 *	Returns the value at the given row, column index, if one
 *	exists. If no traces are currently active on the the slot,
 *	then a call to check if any trace should fire is called.
 *
 * Results:
 *	A standard Tcl result. If the column key isn't valid TCL_ERROR
 *	is returned and an error message is left as the interpreter
 *	result.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleGetValue(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    Row *rowPtr, 
    CONST char *key,
    Tcl_Obj **objPtrPtr)
{
    Blt_HashEntry *hPtr;
    Column *columnPtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    char *left, *right;
    int result;

    if (ParseParentheses(interp, key, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr != NULL) {
	    columnPtr = Blt_GetHashValue(hPtr);
	    result = GetArrayValue(interp, clientPtr, rowPtr, columnPtr, 
		left + 1, objPtrPtr, TRACE_OK);
	} else if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find column \"", key, 
		"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	}
	*left = '(', *right = ')';
	if (hPtr == NULL) {
	    return TCL_ERROR;
	}
    } else {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find column \"", key, 
			"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	columnPtr = Blt_GetHashValue(hPtr);
	result = GetValue(interp, clientPtr, rowPtr, columnPtr, objPtrPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleSetValue --
 *
 *	Sets the value at the given row, column index.  If the slot
 *	is currently filled, the old Tcl_Obj is decremented.  The
 *	new Tcl_Obj is incremented.  If no traces are currently active 
 *	on the the slot, then a call to check if any trace should 
 *	fire is called.  There are two type of traces that may fire:
 *
 *	    TUPLE_TRACE_WRITE		indicates that the slot was
 *					written to.
 *	    TUPLE_TRACE_CREATE		indicates there was no former
 *					value set.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleSetValue(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    Row *rowPtr, 
    CONST char *key,
    Tcl_Obj *objPtr)
{
    Blt_HashEntry *hPtr;
    Column *columnPtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    char *left, *right;
    int result;

    if (ParseParentheses(interp, key, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr != NULL) {
	    columnPtr = Blt_GetHashValue(hPtr);
	    result = SetArrayValue(interp, clientPtr, rowPtr, columnPtr, 
		left + 1, objPtr);
	} else if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find column \"", key, 
		"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	}
	*left = '(', *right = ')';
	if (hPtr == NULL) {
	    return TCL_ERROR;
	}
    } else {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find column \"", key, 
			"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	columnPtr = Blt_GetHashValue(hPtr);
	result = SetValue(interp, clientPtr, rowPtr, columnPtr, objPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleUnsetValue --
 *
 *	Removes the value at the given tuple and column key, if one
 *	exists.  The Tcl_Obj is decremented and the slot is set to
 *	NULL.  If no traces are currently active on the the slot, then
 *	a call to check if any trace should fire is called.
 *
 *	The row, column indices are assumed to be valid.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleUnsetValue(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    Row *rowPtr, 
    CONST char *key)
{
    Blt_HashEntry *hPtr;
    Column *columnPtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    char *left, *right;
    int result;

    if (ParseParentheses(interp, key, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr != NULL) {
	    columnPtr = Blt_GetHashValue(hPtr);
	    result = UnsetArrayValue(interp, clientPtr, rowPtr, columnPtr, 
		left + 1);
	} else if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find column \"", key, 
		"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	}
	*left = '(', *right = ')';
	if (hPtr == NULL) {
	    return TCL_ERROR;
	}
    } else {
	hPtr = Blt_FindHashEntry(&tupleObjPtr->columnTable, key);
	if (hPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find column \"", key, 
			"\" in \"", tupleObjPtr->name, "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	columnPtr = Blt_GetHashValue(hPtr);
	result = UnsetValue(interp, clientPtr, rowPtr, columnPtr);
    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleGetValueByIndex --
 *
 *	Returns the value at the given row, column index, if one
 *	exists. If no traces are currently active on the the slot,
 *	then a call to check if any trace should fire is called.
 *
 *	The row, column indices are assumed to be valid.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleGetValueByIndex(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    unsigned int row, 
    unsigned int column,
    Tcl_Obj **objPtrPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Row *rowPtr;
    Column *columnPtr;

    assert((row >= 0) && (row < tupleObjPtr->nRows));
    assert((column >= 0) && (column < tupleObjPtr->nColumns));

    rowPtr = tupleObjPtr->rows[row];
    columnPtr = tupleObjPtr->columns[column];
    return GetValue(interp, clientPtr, rowPtr, columnPtr, objPtrPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleSetValueByIndex --
 *
 *	Sets the value at the given row, column index.  If the slot
 *	is currently filled, the old Tcl_Obj is decremented.  The
 *	new Tcl_Obj is incremented.  If no traces are currently active 
 *	on the the slot, then a call to check if any trace should 
 *	fire is called.  There are two type of traces that may fire:
 *
 *	    TUPLE_TRACE_WRITE		indicates that the slot was
 *					written to.
 *	    TUPLE_TRACE_CREATE		indicates there was no former
 *					value set.
 *
 *	The row, column indices are assumed to be valid.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleSetValueByIndex(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    unsigned int row, 
    unsigned int column,
    Tcl_Obj *objPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Row *rowPtr;
    Column *columnPtr;

    assert(row < tupleObjPtr->nRows);
    assert(column < tupleObjPtr->nColumns);

    rowPtr = tupleObjPtr->rows[row];
    columnPtr = tupleObjPtr->columns[column];
    return SetValue(interp, clientPtr, rowPtr, columnPtr, objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleUnsetValueByIndex --
 *
 *	Removes the value at the given row, column index, if one
 *	exists.  The Tcl_Obj is decremented and the slot is set
 *	to NULL.  If no traces are currently active on the the slot,
 *	then a call to check if any trace should fire is called.
 *
 *	It's not an error to "unset" nonexistent table locations.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Traces on the table location may be fired.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleUnsetValueByIndex(
    Tcl_Interp *interp, 
    TupleClient *clientPtr, 
    unsigned int row, 
    unsigned int column)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Row *rowPtr;
    Column *columnPtr;

    assert(row < tupleObjPtr->nRows);
    assert(column < tupleObjPtr->nColumns);

    rowPtr = tupleObjPtr->rows[row];
    columnPtr = tupleObjPtr->columns[column];
    return UnsetValue(interp, clientPtr, rowPtr, columnPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleInsertRows --
 *
 *	Creates a new rows in the table.  The name and position in 
 *	the parent are also provided.
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleInsertRows(
    TupleClient *clientPtr,	/* The tuple client that is creating
				 * this node.  If NULL, indicates to
				 * trigger notify events on behalf of
				 * the initiating client also. */
    unsigned int insertRow,	/* Position in the parent's list of children
				 * where to insert the new node. */
    unsigned int nRows)		/* Number of rows tuples to insert. */
{
    TupleObject *tupleObjPtr;
    int oldRows;

    tupleObjPtr = clientPtr->tupleObjPtr;

    if (insertRow >= tupleObjPtr->nRows) {
	insertRow = tupleObjPtr->nRows;
    }
    oldRows = tupleObjPtr->nRows;
    if (!ExtendRows(tupleObjPtr, nRows)) {
	return TCL_ERROR;
    }
    if (insertRow < oldRows) {
	int i, j;

	/* Slide the rows down, creating new tuples in its place. */
	for (i = insertRow, j = insertRow + nRows; i < oldRows; i++, j++) {
	    tupleObjPtr->rows[j] = tupleObjPtr->rows[i];
	    tupleObjPtr->rows[j]->index = j;
	    tupleObjPtr->rows[i] = NewRow(tupleObjPtr, i);
	}
    } else {
	int i;

	for (i = oldRows; i < tupleObjPtr->nRows; i++) {
	    tupleObjPtr->rows[i] = NewRow(tupleObjPtr, i);
	}
    }	
    /* 
     * Issue callbacks to each client indicating that a new node has
     * been created.
     */
    NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_CREATE);
    return insertRow;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleMoveRows --
 *
 *	Move one of more rows to a new location in the tuple.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_TupleMoveRows(
    TupleClient *clientPtr,
    int fromRow, 
    int toRow, 
    int nRows)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    Row *fromPtr;
    int i;

    /* [ | |a| | | | | | | | |b| | | ] */
        
    fromPtr = tupleObjPtr->rows[fromRow];
    for (i = fromRow; i < tupleObjPtr->nRows; i++) {
	/* Compress the array. */
    }
    /* 
     * Issue callbacks to each client indicating that a node has
     * been moved.
     */
    NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_MOVE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleDeleteTuple --
 *
 *	Deletes one or more rows starting at the given index.  
 *	This will cause a single delete event.
 *
 * Results:
 *	Returns the row at the given index.  If the index is out
 *	of range, then NULL is returned.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TupleDeleteTuple(
    TupleClient *clientPtr, 
    Row *rowPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int i, j;

    if (!tupleObjPtr->notifyHold) {
	/* 
	 * Issue callbacks to each client indicating that a tuple will be
	 * removed.
	 */
	NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_DELETE_ROW);
	for (j = tupleObjPtr->nRows - 1, i = j - 1; i >= rowPtr->index; 
	     i--, j--) {
	    tupleObjPtr->rows[i] = tupleObjPtr->rows[j];
	    tupleObjPtr->rows[i]->index = i;
	}
	tupleObjPtr->nRows--;
    }
    if (Blt_ChainGetLength(tupleObjPtr->clients) < 2) {
	Blt_TupleClearTags(clientPtr, rowPtr);
    }
    DeleteRow(tupleObjPtr, rowPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleStartDeleteRows --
 *
 *	Deletes one or more rows starting at the given index.  
 *	This will cause a single delete event.
 *
 * Results:
 *	Returns the row at the given index.  If the index is out
 *	of range, then NULL is returned.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TupleStartDeleteRows(TupleClient *clientPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;

    tupleObjPtr->notifyHold = TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleEndDeleteRows --
 *
 *	Deletes one or more rows starting at the given index.  
 *	This will cause a single delete event.
 *
 * Results:
 *	Returns the row at the given index.  If the index is out
 *	of range, then NULL is returned.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TupleEndDeleteRows(TupleClient *clientPtr)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int first;
    int count;
    int i;

    /* 
     * Issue callbacks to each client indicating that a tuple will be
     * removed.
     */
    tupleObjPtr->notifyHold = FALSE;
    count = 0;
    for (i = 0; i < tupleObjPtr->nRows; i++) {
	if (tupleObjPtr->rows[i] == NULL) {
	    if (first == -1) {
		first = i;
	    }
	    continue;
	}
	if (count < i) {
	    tupleObjPtr->rows[count] = tupleObjPtr->rows[i];
	    tupleObjPtr->rows[count]->index = count;
	}
	count++;
    }
    tupleObjPtr->nRows = count++;
    NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_DELETE_ROW);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleCreateTrace --
 *
 *	Creates a trace for one or more tuples with one or more column
 *	keys.  Whenever a matching action occurs in the tuple object,
 *	the specified procedure is executed.
 *
 * Results:
 *	Returns a token for the trace.
 *
 * Side Effects:
 *	Memory is allocated for the trace.
 *
 *---------------------------------------------------------------------- 
 */
Blt_TupleTrace
Blt_TupleCreateTrace(
    TupleClient *clientPtr,
    Row *rowPtr,		/* Tuple to be traced. May be NULL,
				 * indicating no row. */
    CONST char *tagName,	/* Tag of tuple(s) to be traced. May
				 * be NULL. */
    CONST char *keyList,	/* Tcl list of keys.  Split and freed
				 * by Blt_TupleDeleteTrace. May be
				 * NULL indicating all keys match. */
    unsigned int mask,		/* Bit mask indicating what actions to
				   trace. */
    Blt_TupleTraceProc *proc,   /* Callback procedure for the trace. */
    ClientData clientData)	/* One-word of data passed along when
				 * the callback is executed. */
{
    Trace *tracePtr;
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;

    tracePtr = Blt_Calloc(1, sizeof (Trace));
    assert(tracePtr);
    if (keyList != NULL) {
	CONST char **keys;
	int nKeys;

	if (Tcl_SplitList(clientPtr->interp, keyList, &nKeys, (char ***)&keys) 
	    != TCL_OK) {
	    Blt_Free(tracePtr);
	    return NULL;
	}
	if (nKeys == 0) {
	    Blt_Free(keys);
	} else {
	    tracePtr->keys = keys;
	}
    }
    if (tagName != NULL) {
	tracePtr->withTag = Blt_Strdup(tagName);
    }
    tracePtr->table = clientPtr;
    tracePtr->proc = proc;
    tracePtr->clientData = clientData;
    tracePtr->mask = mask;
    tracePtr->tuple = rowPtr;
    tracePtr->withTag = NULL;
    tracePtr->linkPtr = Blt_ChainAppend(tupleObjPtr->traces, tracePtr);
    return tracePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleDeleteTrace --
 *
 *	Deletes a trace.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory is de-allocated for the trace.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TupleDeleteTrace(Trace *tracePtr)
{
    Blt_ChainDeleteLink(tracePtr->table->tupleObjPtr->traces, 
		tracePtr->linkPtr);
    if (tracePtr->keys != NULL) {
	Blt_Free(tracePtr->keys);
    }
    if (tracePtr->withTag != NULL) {
	Blt_Free(tracePtr->withTag);
    }
    Blt_Free(tracePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleExtendRows --
 *
 *	Adds new rows to the table.  Rows are slots in an array
 *	of Rows.  The array grows by doubling its size, so there
 *	may be more slots than needed (# rows).  
 *
 * Results:
 *	Returns TCL_OK is the tuple is resized and TCL_ERROR if an
 *	not enough memory was available.
 *
 * Side Effects:
 *	If more rows are needed, the array which holds the tuples is
 *	reallocated by doubling its size.  
 *
 *----------------------------------------------------------------------
 */
int
Blt_TupleExtendRows(TupleClient *clientPtr, unsigned int nRows)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    int oldSize;
    int i;
    
    oldSize = tupleObjPtr->nRows;
    if (!ExtendRows(tupleObjPtr, nRows)) {
	return TCL_ERROR;
    }
    for (i = oldSize; i < tupleObjPtr->nRows; i++) {
	tupleObjPtr->rows[i] = NewRow(tupleObjPtr, i);
    }
    tupleObjPtr->nRows = nRows;
    NotifyClients(clientPtr, tupleObjPtr, TUPLE_NOTIFY_CREATE);
    return TCL_OK;
}

int
Blt_TupleReduceRows(TupleClient *clientPtr, int nRows)
{
    TupleObject *tupleObjPtr = clientPtr->tupleObjPtr;
    
    if (nRows < tupleObjPtr->nRows) {
	if (tupleObjPtr->rowsAllocated < nRows) {
	    Row **rows;

	    while (tupleObjPtr->rowsAllocated < nRows) {
		tupleObjPtr->rowsAllocated += tupleObjPtr->rowsAllocated;
	    }
	    rows = Blt_Realloc(tupleObjPtr->rows, 
	       tupleObjPtr->rowsAllocated * sizeof(Row *));
	    if (rows == NULL) {
		return TCL_ERROR;
	    }
	    tupleObjPtr->rows = rows;
	}
	tupleObjPtr->nRows = nRows;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleAddTag --
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TupleAddTag(
    TupleClient *clientPtr,
    Row *rowPtr,
    CONST char *tagName)
{
    if (strcmp(tagName, "all") != 0) {
	Blt_HashEntry *hPtr;
	Blt_HashTable *tablePtr;
	Blt_TupleTagEntry *tPtr;
	int isNew;

	tablePtr = &clientPtr->tagTablePtr->tagTable;
	hPtr = Blt_CreateHashEntry(tablePtr, tagName, &isNew);
	assert(hPtr);
	if (isNew) {
	    tPtr = Blt_Malloc(sizeof(Blt_TupleTagEntry));
	    Blt_InitHashTable(&tPtr->rowTable, BLT_ONE_WORD_KEYS);
	    Blt_SetHashValue(hPtr, tPtr);
	    tPtr->hashPtr = hPtr;
	    tPtr->tagName = Blt_GetHashKey(tablePtr, hPtr);
	} else {
	    tPtr = Blt_GetHashValue(hPtr);
	}
	hPtr = Blt_CreateHashEntry(&tPtr->rowTable, (char *)rowPtr, &isNew);
	assert(hPtr);
	if (isNew) {
	    Blt_SetHashValue(hPtr, rowPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleClearTags --
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TupleClearTags(TupleClient *clientPtr, Row *rowPtr)
{
    Blt_HashEntry *hPtr, *h2Ptr;
    Blt_HashSearch cursor;
    Blt_TupleTagEntry *tPtr;

    for (hPtr = Blt_FirstHashEntry(&clientPtr->tagTablePtr->tagTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tPtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(&tPtr->rowTable, (char *)rowPtr);
	if (h2Ptr != NULL) {
	    Blt_DeleteHashEntry(&tPtr->rowTable, h2Ptr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleFirstTag --
 *
 *---------------------------------------------------------------------- 
 */
Blt_HashEntry *
Blt_TupleFirstTag(
    TupleClient *clientPtr, 
    Blt_HashSearch *cursorPtr)
{
    return Blt_FirstHashEntry(&clientPtr->tagTablePtr->tagTable, cursorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleForgetTag --
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TupleForgetTag(
    TupleClient *clientPtr,
    CONST char *tagName)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
    if (hPtr != NULL) {
	Blt_TupleTagEntry *tPtr;

	tPtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashEntry(&clientPtr->tagTablePtr->tagTable, hPtr);
	Blt_DeleteHashTable(&tPtr->rowTable);
	Blt_Free(tPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleHasTag --
 *
 *---------------------------------------------------------------------- 
 */
int
Blt_TupleHasTag(
    TupleClient *clientPtr,
    Row *rowPtr,
    CONST char *tagName)
{
    Blt_HashEntry *hPtr;

    if (strcmp(tagName, "all") == 0) {
	return TRUE;
    }
    hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
    if (hPtr != NULL) {
	Blt_TupleTagEntry *tPtr;

	tPtr = Blt_GetHashValue(hPtr);
	hPtr = Blt_FindHashEntry(&tPtr->rowTable, (char *)rowPtr);
	if (hPtr != NULL) {
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleRemoveTag --
 *
 *---------------------------------------------------------------------- 
 */
void
Blt_TupleRemoveTag(
    TupleClient *clientPtr,
    Row *rowPtr,
    CONST char *tagName)
{
    if (strcmp(tagName, "all") != 0) {
	Blt_HashEntry *hPtr;
	Blt_HashTable *tablePtr;

	tablePtr = &clientPtr->tagTablePtr->tagTable;
	hPtr = Blt_FindHashEntry(tablePtr, tagName);
	if (hPtr != NULL) {
	    Blt_TupleTagEntry *tPtr;

	    tPtr = Blt_GetHashValue(hPtr);

	    hPtr = Blt_FindHashEntry(&tPtr->rowTable, (char *)rowPtr);
	    if (hPtr != NULL) {
		Blt_DeleteHashEntry(&tPtr->rowTable, hPtr);
	    }
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleTagSetHashTable --
 *
 *---------------------------------------------------------------------- 
 */
Blt_HashTable *
Blt_TupleTagTableHashTable(
    TupleClient *clientPtr,
    CONST char *tagName)
{
    Blt_HashEntry *hPtr;
   
    hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
    if (hPtr != NULL) {
	Blt_TupleTagEntry *tPtr;
	
	tPtr = Blt_GetHashValue(hPtr);
	return &tPtr->rowTable;
    }
    return NULL;
}

int
Blt_TupleTagTableIsShared(TupleClient *clientPtr)
{
    return (clientPtr->tagTablePtr->refCount > 1);
}   

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleFirstTagged --
 *
 *	Returns the id of the first tuple tagged by the given tag in
 *	objPtr.  
 *
 *---------------------------------------------------------------------- 
 */
Blt_Tuple
Blt_TupleFirstTagged(
    Tcl_Interp *interp,
    TupleClient *clientPtr,
    Tcl_Obj *objPtr,
    Blt_TupleTagSearch *cursorPtr)
{
    char *tagName;
    cursorPtr->tagType = TAG_TYPE_NONE;

    tagName = Tcl_GetString(objPtr);
    /* Process strings that start with digits as indices, not tags. */
    if (isdigit(UCHAR(*tagName))) {
	int row;

	if (Tcl_GetIntFromObj(interp, objPtr, &row) != TCL_OK) {
	    return NULL;
	}
	if ((row < 0) || (row >= clientPtr->tupleObjPtr->nRows)) {
	    Tcl_AppendResult(interp, "row index \"", tagName, 
			     "\" is out of range", (char *)NULL);
	    return NULL;
	}
	return clientPtr->tupleObjPtr->rows[row];
    }
    cursorPtr->nRows = clientPtr->tupleObjPtr->nRows;
    if (strcmp(tagName, "all") == 0) {
	cursorPtr->tagType = TAG_TYPE_ALL;
	cursorPtr->row = 0;
	return clientPtr->tupleObjPtr->rows[0];
    } else {
	Blt_HashEntry *hPtr;
	
	hPtr = Blt_FindHashEntry(&clientPtr->tagTablePtr->tagTable, tagName);
	if (hPtr != NULL) {
	    Blt_TupleTagEntry *tPtr;
	    Row *rowPtr;
	    
	    tPtr = Blt_GetHashValue(hPtr);
	    cursorPtr->rowTablePtr = &tPtr->rowTable;
	    cursorPtr->tagType = TAG_TYPE_TAG;
	    hPtr = Blt_FirstHashEntry(cursorPtr->rowTablePtr, 
		&cursorPtr->cursor); 
	    if (hPtr == NULL) {
		return NULL;
	    }
	    rowPtr = Blt_GetHashValue(hPtr);
	    return rowPtr;
	}
    }
    Tcl_AppendResult(interp, "can't find tag or id \"", tagName, "\" in ", 
	clientPtr->tupleObjPtr->name, (char *)NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TupleNextTagged --
 *
 *---------------------------------------------------------------------- 
 */
Blt_Tuple
Blt_TupleNextTagged(Blt_TupleTagSearch *cursorPtr)
{
    if (cursorPtr->tagType == TAG_TYPE_ALL) {
	TupleClient *clientPtr = cursorPtr->table;

	cursorPtr->row++;
	if (cursorPtr->row >= clientPtr->tupleObjPtr->nRows) {
	    return NULL;
	}
	return clientPtr->tupleObjPtr->rows[cursorPtr->row];
    }
    if (cursorPtr->tagType == TAG_TYPE_TAG) {
	Blt_HashEntry *hPtr;
	Row *rowPtr;

	hPtr = Blt_NextHashEntry(&cursorPtr->cursor);
	if (hPtr == NULL) {
	    return NULL;
	}
	rowPtr = Blt_GetHashValue(hPtr);
	return rowPtr;
    }
    return NULL;
}
