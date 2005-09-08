
/*
 * bltTuple.h --
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
 *	The tuple data object was created by George A. Howlett.
 */

#ifndef _BLT_TUPLE_H
#define _BLT_TUPLE_H

#include <bltChain.h>
#include <bltHash.h>
#include <bltPool.h>

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
typedef struct Blt_TupleRowStruct *Blt_Tuple;
typedef struct Blt_TupleTraceStruct *Blt_TupleTrace;
typedef struct Blt_TupleNotifierStruct *Blt_TupleNotifier;
typedef struct Blt_TupleClientStruct *Blt_TupleTable;

typedef struct Blt_TupleColumnStruct Blt_TupleColumn;
typedef struct Blt_TupleRowStruct Blt_TupleRow;
typedef struct Blt_TupleTagTableStruct Blt_TupleTagTable;

#define TUPLE_NOTIFY_CREATE_COLUMN	(1<<0)
#define TUPLE_NOTIFY_CREATE_ROW		(1<<1)
#define TUPLE_NOTIFY_CREATE \
	(TUPLE_NOTIFY_CREATE_COLUMN|TUPLE_NOTIFY_CREATE_ROW)
#define TUPLE_NOTIFY_DELETE_ROW		(1<<2)
#define TUPLE_NOTIFY_DELETE_COLUMN	(1<<3)
#define TUPLE_NOTIFY_DELETE \
	(TUPLE_NOTIFY_DELETE_COLUMN|TUPLE_NOTIFY_DELETE_ROW)
#define TUPLE_NOTIFY_MOVE		(1<<4)
#define TUPLE_NOTIFY_SORT		(1<<5)
#define TUPLE_NOTIFY_RELABEL		(1<<6)
#define TUPLE_NOTIFY_ALL	\
	 (TUPLE_NOTIFY_CREATE | TUPLE_NOTIFY_DELETE | TUPLE_NOTIFY_MOVE | \
	  TUPLE_NOTIFY_SORT   | TUPLE_NOTIFY_RELABEL)
#define TUPLE_NOTIFY_MASK		(TUPLE_NOTIFY_ALL)

#define TUPLE_NOTIFY_WHENIDLE	 (1<<8)
#define TUPLE_NOTIFY_FOREIGN_ONLY (1<<9)
#define TUPLE_NOTIFY_ACTIVE	 (1<<10)

typedef struct {
    int type;			/* Indicates type of event received. */
    int row;			/* Index of tuple receiving the event. */
    Tcl_Interp *interp;		/* Interpreter to report to */
    Blt_TupleTable table;	/* Tuple object client that received
				 * the event. */
} Blt_TupleNotifyEvent;

typedef int (Blt_TupleNotifyEventProc)(ClientData clientData, 
	Blt_TupleNotifyEvent *eventPtr);

struct Blt_TupleNotifierStruct {
    Blt_TupleTable table;
    Blt_ChainLink *linkPtr;
    Blt_TupleNotifyEvent event;
    Blt_TupleNotifyEventProc *proc;
    ClientData clientData;
    Tcl_Interp *interp;
    char *key;
    int notifyPending;
    unsigned int mask;
};


EXTERN Blt_TupleNotifier Blt_TupleCreateNotifier(Blt_TupleTable table,
	unsigned int mask, Blt_TupleNotifyEventProc *proc, 
	ClientData clientData);

EXTERN void Blt_TupleDeleteNotifier(Blt_TupleNotifier notifier);

typedef struct {
    char *tagName;
    Blt_HashEntry *hashPtr;
    Blt_HashTable rowTable;	/* Hash table of row pointers.  This
				 * represents all the rows tagged by
				 * this tag. */
} Blt_TupleTagEntry;

typedef struct {
    Blt_TupleTable table;
    int tagType;
    unsigned int row, nRows;
    Blt_HashSearch cursor;
    Blt_HashTable *rowTablePtr;
} Blt_TupleTagSearch;

EXTERN Blt_Tuple Blt_TupleFirstTagged(Tcl_Interp *interp, Blt_TupleTable table,
	Tcl_Obj *objPtr, Blt_TupleTagSearch *cursorPtr);
EXTERN Blt_Tuple Blt_TupleNextTagged(Blt_TupleTagSearch *cursorPtr);

EXTERN void Blt_TupleClearTags(Blt_TupleTable table, Blt_Tuple tuple);
EXTERN void Blt_TupleRemoveTag(Blt_TupleTable table, Blt_Tuple tuple, 
	CONST char *tagName);
EXTERN int Blt_TupleHasTag(Blt_TupleTable table, Blt_Tuple tuple, 
	CONST char *tagName);
EXTERN void Blt_TupleAddTag(Blt_TupleTable table, Blt_Tuple tuple, 
	CONST char *tagName);
EXTERN void Blt_TupleForgetTag(Blt_TupleTable table, CONST char *tagName);
EXTERN Blt_HashTable *Blt_TupleTagHashTable(Blt_TupleTable table, 
	CONST char *tagName);
EXTERN int Blt_TupleTagTableIsShared(Blt_TupleTable table);

#define TUPLE_TRACE_UNSET	(1<<3)
#define TUPLE_TRACE_WRITE	(1<<4)
#define TUPLE_TRACE_READ	(1<<5)
#define TUPLE_TRACE_CREATE	(1<<6)
#define TUPLE_TRACE_ALL		\
	(TUPLE_TRACE_UNSET | TUPLE_TRACE_WRITE | TUPLE_TRACE_READ  | \
	TUPLE_TRACE_CREATE)
#define TUPLE_TRACE_MASK	(TUPLE_TRACE_ALL)

#define TUPLE_TRACE_FOREIGN_ONLY (1<<8)
#define TUPLE_TRACE_ACTIVE	(1<<9)

typedef int (Blt_TupleTraceProc)(ClientData clientData, Tcl_Interp *interp, 
	unsigned int row, unsigned int column, unsigned int flags);

struct Blt_TupleTraceStruct {
    Tcl_Interp *interp;
    Blt_ChainLink *linkPtr;
    Blt_TupleTable table;
    Blt_Tuple tuple;
    CONST char **keys;
    char *withTag;
    unsigned int mask;
    Blt_TupleTraceProc *proc;
    ClientData clientData;
};

EXTERN Blt_TupleTrace Blt_TupleCreateTrace(Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *tagName, CONST char *keyList, 
	unsigned int mask, Blt_TupleTraceProc *proc, ClientData clientData);

EXTERN void Blt_TupleDeleteTrace(Blt_TupleTrace trace);

EXTERN int Blt_TupleGetValueByIndex(Tcl_Interp *interp, Blt_TupleTable table, 
	unsigned int row, unsigned int column, Tcl_Obj **objPtrPtr);
EXTERN int Blt_TupleSetValueByIndex(Tcl_Interp *interp, Blt_TupleTable table, 
	unsigned int row, unsigned int column, Tcl_Obj *objPtr);
EXTERN int Blt_TupleUnsetValueByIndex(Tcl_Interp *interp, Blt_TupleTable table, 
	unsigned int row, unsigned int column);
EXTERN int Blt_TupleGetValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key, Tcl_Obj **objPtrPtr);
EXTERN int Blt_TupleSetValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key, Tcl_Obj *objPtr);
EXTERN int Blt_TupleUnsetValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key);

EXTERN int Blt_TupleGetArrayValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key, CONST char *elemName, 
	Tcl_Obj **objPtrPtr);
EXTERN int Blt_TupleSetArrayValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key, CONST char *elemName,   
	Tcl_Obj *objPtr);
EXTERN int Blt_TupleUnsetArrayValue(Tcl_Interp *interp, Blt_TupleTable table, 
	Blt_Tuple tuple, CONST char *key, CONST char *elemName);

EXTERN unsigned int Blt_TupleAddColumn(Blt_TupleTable table, CONST char *key, 
	int *isNewPtr);
EXTERN unsigned int Blt_TupleRowIndex(Blt_Tuple tuple);
EXTERN CONST char *Blt_TupleGetColumnKey(Blt_TupleTable table, int column);
EXTERN Blt_HashEntry *Blt_TupleFirstTag(Blt_TupleTable table, 
	Blt_HashSearch *cursorPtr);
EXTERN Blt_Tuple Blt_TupleGetTupleByIndex(Blt_TupleTable table, 
	unsigned int row);
EXTERN int Blt_TupleExtendRows(Blt_TupleTable table, unsigned int extra);
EXTERN Blt_Tuple Blt_TupleNextTuple(Blt_TupleTable table, Blt_Tuple tuple);
EXTERN void Blt_TupleStartDeleteRows(Blt_TupleTable table);
EXTERN void Blt_TupleDeleteTuple(Blt_TupleTable table, Blt_Tuple tuple);
EXTERN void Blt_TupleEndDeleteRows(Blt_TupleTable table);

EXTERN int Blt_TupleTableExists(Tcl_Interp *interp, CONST char *objName);
EXTERN int Blt_TupleCreateTable(Tcl_Interp *interp, CONST char *name,
	Blt_TupleTable *tablePtr);
EXTERN int Blt_TupleGetTable(Tcl_Interp *interp, CONST char *name,
	Blt_TupleTable *tablePtr);
EXTERN void Blt_TupleReleaseTable(Blt_TupleTable table);

EXTERN CONST char *Blt_TupleTableName(Blt_TupleTable table);
EXTERN unsigned int Blt_TupleTableLength(Blt_TupleTable table);
EXTERN unsigned int Blt_TupleTableWidth(Blt_TupleTable table);

#endif /* BLT_TUPLE_H */
