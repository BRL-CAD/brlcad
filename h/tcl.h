/*
 * tcl.h --
 *
 *	This header file describes the externally-visible facilities
 *	of the Tcl interpreter.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) tcl.h 1.153 95/06/27 15:42:31
 */

#ifndef _TCL
#define _TCL

#ifndef BUFSIZ
#include <stdio.h>
#endif

#define TCL_VERSION "7.4"
#define TCL_MAJOR_VERSION 7
#define TCL_MINOR_VERSION 4

/*
 * Definitions that allow this header file to be used either with or
 * without ANSI C features like function prototypes.
 */

#undef _ANSI_ARGS_
#undef CONST
#if ((defined(__STDC__) || defined(SABER)) && !defined(NO_PROTOTYPE)) || defined(__cplusplus)
#   define _USING_PROTOTYPES_ 1
#   define _ANSI_ARGS_(x)	x
#   define CONST const
#   ifdef __cplusplus
#       define VARARGS(first) (first, ...)
#   else
#       define VARARGS(first) ()
#   endif
#else
#   define _ANSI_ARGS_(x)	()
#   define CONST
#endif

#ifdef __cplusplus
#   define EXTERN extern "C"
#else
#   define EXTERN extern
#endif

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.
 */

#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif

/*
 * Miscellaneous declarations (to allow Tcl to be used stand-alone,
 * without the rest of Sprite).
 */

#ifndef NULL
#define NULL 0
#endif

#ifndef _CLIENTDATA
#   if defined(__STDC__) || defined(__cplusplus)
    typedef void *ClientData;
#   else
    typedef int *ClientData;
#   endif /* __STDC__ */
#define _CLIENTDATA
#endif

/*
 * Data structures defined opaquely in this module.  The definitions
 * below just provide dummy types.  A few fields are made visible in
 * Tcl_Interp structures, namely those for returning string values.
 * Note:  any change to the Tcl_Interp definition below must be mirrored
 * in the "real" definition in tclInt.h.
 */

typedef struct Tcl_Interp{
    char *result;		/* Points to result string returned by last
				 * command. */
    void (*freeProc) _ANSI_ARGS_((char *blockPtr));
				/* Zero means result is statically allocated.
				 * If non-zero, gives address of procedure
				 * to invoke to free the result.  Must be
				 * freed by Tcl_Eval before executing next
				 * command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number within the command where
				 * the error occurred (1 means first line). */
} Tcl_Interp;

typedef int *Tcl_Trace;
typedef int *Tcl_Command;
typedef struct Tcl_AsyncHandler_ *Tcl_AsyncHandler;
typedef struct Tcl_RegExp_ *Tcl_RegExp;

/*
 * When a TCL command returns, the string pointer interp->result points to
 * a string containing return information from the command.  In addition,
 * the command procedure returns an integer value, which is one of the
 * following:
 *
 * TCL_OK		Command completed normally;  interp->result contains
 *			the command's result.
 * TCL_ERROR		The command couldn't be completed successfully;
 *			interp->result describes what went wrong.
 * TCL_RETURN		The command requests that the current procedure
 *			return;  interp->result contains the procedure's
 *			return value.
 * TCL_BREAK		The command requests that the innermost loop
 *			be exited;  interp->result is meaningless.
 * TCL_CONTINUE		Go on to the next iteration of the current loop;
 *			interp->result is meaningless.
 */

#define TCL_OK		0
#define TCL_ERROR	1
#define TCL_RETURN	2
#define TCL_BREAK	3
#define TCL_CONTINUE	4

#define TCL_RESULT_SIZE 200

/*
 * Argument descriptors for math function callbacks in expressions:
 */

typedef enum {TCL_INT, TCL_DOUBLE, TCL_EITHER} Tcl_ValueType;
typedef struct Tcl_Value {
    Tcl_ValueType type;		/* Indicates intValue or doubleValue is
				 * valid, or both. */
    long intValue;		/* Integer value. */
    double doubleValue;		/* Double-precision floating value. */
} Tcl_Value;

/*
 * Procedure types defined by Tcl:
 */

typedef int (Tcl_AppInitProc) _ANSI_ARGS_((Tcl_Interp *interp));
typedef int (Tcl_AsyncProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int code));
typedef void (Tcl_CmdDeleteProc) _ANSI_ARGS_((ClientData clientData));
typedef int (Tcl_CmdProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char *argv[]));
typedef void (Tcl_CmdTraceProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int level, char *command, Tcl_CmdProc *proc,
	ClientData cmdClientData, int argc, char *argv[]));
typedef void (Tcl_FreeProc) _ANSI_ARGS_((char *blockPtr));
typedef void (Tcl_InterpDeleteProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp));
typedef int (Tcl_MathProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tcl_Value *args, Tcl_Value *resultPtr));
typedef char *(Tcl_VarTraceProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *part1, char *part2, int flags));

/*
 * The structure returned by Tcl_GetCmdInfo and passed into
 * Tcl_SetCmdInfo:
 */

typedef struct Tcl_CmdInfo {
    Tcl_CmdProc *proc;			/* Procedure that implements command. */
    ClientData clientData;		/* ClientData passed to proc. */
    Tcl_CmdDeleteProc *deleteProc;	/* Procedure to call when command
					 * is deleted. */
    ClientData deleteData;		/* Value to pass to deleteProc (usually
					 * the same as clientData). */
} Tcl_CmdInfo;

/*
 * The structure defined below is used to hold dynamic strings.  The only
 * field that clients should use is the string field, and they should
 * never modify it.
 */

#define TCL_DSTRING_STATIC_SIZE 200
typedef struct Tcl_DString {
    char *string;		/* Points to beginning of string:  either
				 * staticSpace below or a malloc'ed array. */
    int length;			/* Number of non-NULL characters in the
				 * string. */
    int spaceAvl;		/* Total number of bytes available for the
				 * string and its terminating NULL char. */
    char staticSpace[TCL_DSTRING_STATIC_SIZE];
				/* Space to use in common case where string
				 * is small. */
} Tcl_DString;

#define Tcl_DStringLength(dsPtr) ((dsPtr)->length)
#define Tcl_DStringValue(dsPtr) ((dsPtr)->string)
#define Tcl_DStringTrunc Tcl_DStringSetLength

/*
 * Definitions for the maximum number of digits of precision that may
 * be specified in the "tcl_precision" variable, and the number of
 * characters of buffer space required by Tcl_PrintDouble.
 */

#define TCL_MAX_PREC 17
#define TCL_DOUBLE_SPACE (TCL_MAX_PREC+10)

/*
 * Flag that may be passed to Tcl_ConvertElement to force it not to
 * output braces (careful!  if you change this flag be sure to change
 * the definitions at the front of tclUtil.c).
 */

#define TCL_DONT_USE_BRACES	1

/*
 * Flag values passed to Tcl_RecordAndEval.
 * WARNING: these bit choices must not conflict with the bit choices
 * for evalFlag bits in tclInt.h!!
 */

#define TCL_NO_EVAL		0x10000
#define TCL_EVAL_GLOBAL		0x20000

/*
 * Special freeProc values that may be passed to Tcl_SetResult (see
 * the man page for details):
 */

#define TCL_VOLATILE	((Tcl_FreeProc *) 1)
#define TCL_STATIC	((Tcl_FreeProc *) 0)
#define TCL_DYNAMIC	((Tcl_FreeProc *) 3)

/*
 * Flag values passed to variable-related procedures.
 */

#define TCL_GLOBAL_ONLY		1
#define TCL_APPEND_VALUE	2
#define TCL_LIST_ELEMENT	4
#define TCL_TRACE_READS		0x10
#define TCL_TRACE_WRITES	0x20
#define TCL_TRACE_UNSETS	0x40
#define TCL_TRACE_DESTROYED	0x80
#define TCL_INTERP_DESTROYED	0x100
#define TCL_LEAVE_ERR_MSG	0x200

/*
 * Types for linked variables:
 */

#define TCL_LINK_INT		1
#define TCL_LINK_DOUBLE		2
#define TCL_LINK_BOOLEAN	3
#define TCL_LINK_STRING		4
#define TCL_LINK_READ_ONLY	0x80

/*
 * Permission flags for files:
 */

#define TCL_FILE_READABLE	1
#define TCL_FILE_WRITABLE	2

/*
 * The following declarations either map ckalloc and ckfree to
 * malloc and free, or they map them to procedures with all sorts
 * of debugging hooks defined in tclCkalloc.c.
 */

#ifdef TCL_MEM_DEBUG

EXTERN char *		Tcl_DbCkalloc _ANSI_ARGS_((unsigned int size,
			    char *file, int line));
EXTERN int		Tcl_DbCkfree _ANSI_ARGS_((char *ptr,
			    char *file, int line));
EXTERN char *		Tcl_DbCkrealloc _ANSI_ARGS_((char *ptr,
			    unsigned int size, char *file, int line));
EXTERN int		Tcl_DumpActiveMemory _ANSI_ARGS_((char *fileName));
EXTERN void		Tcl_ValidateAllMemory _ANSI_ARGS_((char *file,
			    int line));
#  define ckalloc(x) Tcl_DbCkalloc(x, __FILE__, __LINE__)
#  define ckfree(x)  Tcl_DbCkfree(x, __FILE__, __LINE__)
#  define ckrealloc(x,y) Tcl_DbCkrealloc((x), (y),__FILE__, __LINE__)

#else

#  define ckalloc(x) malloc(x)
#  define ckfree(x)  free(x)
#  define ckrealloc(x,y) realloc(x,y)
#  define Tcl_DumpActiveMemory(x)
#  define Tcl_ValidateAllMemory(x,y)

#endif /* TCL_MEM_DEBUG */

/*
 * Macro to free up result of interpreter.
 */

#define Tcl_FreeResult(interp)					\
    if ((interp)->freeProc != 0) {				\
	if ((interp)->freeProc == (Tcl_FreeProc *) free) {	\
	    ckfree((interp)->result);				\
	} else {						\
	    (*(interp)->freeProc)((interp)->result);		\
	}							\
	(interp)->freeProc = 0;					\
    }

/*
 * Forward declaration of Tcl_HashTable.  Needed by some C++ compilers
 * to prevent errors when the forward reference to Tcl_HashTable is
 * encountered in the Tcl_HashEntry structure.
 */

#ifdef __cplusplus
struct Tcl_HashTable;
#endif

/*
 * Structure definition for an entry in a hash table.  No-one outside
 * Tcl should access any of these fields directly;  use the macros
 * defined below.
 */

typedef struct Tcl_HashEntry {
    struct Tcl_HashEntry *nextPtr;	/* Pointer to next entry in this
					 * hash bucket, or NULL for end of
					 * chain. */
    struct Tcl_HashTable *tablePtr;	/* Pointer to table containing entry. */
    struct Tcl_HashEntry **bucketPtr;	/* Pointer to bucket that points to
					 * first entry in this entry's chain:
					 * used for deleting the entry. */
    ClientData clientData;		/* Application stores something here
					 * with Tcl_SetHashValue. */
    union {				/* Key has one of these forms: */
	char *oneWordValue;		/* One-word value for key. */
	int words[1];			/* Multiple integer words for key.
					 * The actual size will be as large
					 * as necessary for this table's
					 * keys. */
	char string[4];			/* String for key.  The actual size
					 * will be as large as needed to hold
					 * the key. */
    } key;				/* MUST BE LAST FIELD IN RECORD!! */
} Tcl_HashEntry;

/*
 * Structure definition for a hash table.  Must be in tcl.h so clients
 * can allocate space for these structures, but clients should never
 * access any fields in this structure.
 */

#define TCL_SMALL_HASH_TABLE 4
typedef struct Tcl_HashTable {
    Tcl_HashEntry **buckets;		/* Pointer to bucket array.  Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    Tcl_HashEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
					/* Bucket array used for small tables
					 * (to avoid mallocs and frees). */
    int numBuckets;			/* Total number of buckets allocated
					 * at **bucketPtr. */
    int numEntries;			/* Total number of entries present
					 * in table. */
    int rebuildSize;			/* Enlarge table when numEntries gets
					 * to be this large. */
    int downShift;			/* Shift count used in hashing
					 * function.  Designed to use high-
					 * order bits of randomized keys. */
    int mask;				/* Mask value used in hashing
					 * function. */
    int keyType;			/* Type of keys used in this table. 
					 * It's either TCL_STRING_KEYS,
					 * TCL_ONE_WORD_KEYS, or an integer
					 * giving the number of ints in a
					 */
    Tcl_HashEntry *(*findProc) _ANSI_ARGS_((struct Tcl_HashTable *tablePtr,
	    char *key));
    Tcl_HashEntry *(*createProc) _ANSI_ARGS_((struct Tcl_HashTable *tablePtr,
	    char *key, int *newPtr));
} Tcl_HashTable;

/*
 * Structure definition for information used to keep track of searches
 * through hash tables:
 */

typedef struct Tcl_HashSearch {
    Tcl_HashTable *tablePtr;		/* Table being searched. */
    int nextIndex;			/* Index of next bucket to be
					 * enumerated after present one. */
    Tcl_HashEntry *nextEntryPtr;	/* Next entry to be enumerated in the
					 * the current bucket. */
} Tcl_HashSearch;

/*
 * Acceptable key types for hash tables:
 */

#define TCL_STRING_KEYS		0
#define TCL_ONE_WORD_KEYS	1

/*
 * Macros for clients to use to access fields of hash entries:
 */

#define Tcl_GetHashValue(h) ((h)->clientData)
#define Tcl_SetHashValue(h, value) ((h)->clientData = (ClientData) (value))
#define Tcl_GetHashKey(tablePtr, h) \
    ((char *) (((tablePtr)->keyType == TCL_ONE_WORD_KEYS) ? (h)->key.oneWordValue \
						: (h)->key.string))

/*
 * Macros to use for clients to use to invoke find and create procedures
 * for hash tables:
 */

#define Tcl_FindHashEntry(tablePtr, key) \
	(*((tablePtr)->findProc))(tablePtr, key)
#define Tcl_CreateHashEntry(tablePtr, key, newPtr) \
	(*((tablePtr)->createProc))(tablePtr, key, newPtr)

/*
 * Exported Tcl variables:
 */

EXTERN int		tcl_AsyncReady;
EXTERN void		(*tcl_FileCloseProc) _ANSI_ARGS_((FILE *f));
EXTERN char *		tcl_RcFileName;

/*
 * Exported Tcl procedures:
 */

EXTERN void		Tcl_AddErrorInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *message));
EXTERN void		Tcl_AllowExceptions _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_AppendElement _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN void		Tcl_AppendResult _ANSI_ARGS_(
			    VARARGS(Tcl_Interp *interp));
EXTERN int		Tcl_AppInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_AsyncMark _ANSI_ARGS_((Tcl_AsyncHandler async));
EXTERN Tcl_AsyncHandler	Tcl_AsyncCreate _ANSI_ARGS_((Tcl_AsyncProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_AsyncDelete _ANSI_ARGS_((Tcl_AsyncHandler async));
EXTERN int		Tcl_AsyncInvoke _ANSI_ARGS_((Tcl_Interp *interp,
			    int code));
EXTERN char		Tcl_Backslash _ANSI_ARGS_((char *src,
			    int *readPtr));
EXTERN void		Tcl_CallWhenDeleted _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_InterpDeleteProc *proc,
			    ClientData clientData));
EXTERN int		Tcl_CommandComplete _ANSI_ARGS_((char *cmd));
EXTERN char *		Tcl_Concat _ANSI_ARGS_((int argc, char **argv));
EXTERN int		Tcl_ConvertElement _ANSI_ARGS_((char *src,
			    char *dst, int flags));
EXTERN Tcl_Command	Tcl_CreateCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdProc *proc,
			    ClientData clientData,
			    Tcl_CmdDeleteProc *deleteProc));
EXTERN Tcl_Interp *	Tcl_CreateInterp _ANSI_ARGS_((void));
EXTERN void		Tcl_CreateMathFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int numArgs, Tcl_ValueType *argTypes,
			    Tcl_MathProc *proc, ClientData clientData));
EXTERN int		Tcl_CreatePipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, int **pidArrayPtr,
			    int *inPipePtr, int *outPipePtr,
			    int *errFilePtr));
EXTERN Tcl_Trace	Tcl_CreateTrace _ANSI_ARGS_((Tcl_Interp *interp,
			    int level, Tcl_CmdTraceProc *proc,
			    ClientData clientData));
EXTERN int		Tcl_DeleteCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName));
EXTERN void		Tcl_DeleteHashEntry _ANSI_ARGS_((
			    Tcl_HashEntry *entryPtr));
EXTERN void		Tcl_DeleteHashTable _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr));
EXTERN void		Tcl_DeleteInterp _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_DeleteTrace _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Trace trace));
EXTERN void		Tcl_DetachPids _ANSI_ARGS_((int numPids, int *pidPtr));
EXTERN void		Tcl_DontCallWhenDeleted _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_InterpDeleteProc *proc,
			    ClientData clientData));
EXTERN char *		Tcl_DStringAppend _ANSI_ARGS_((Tcl_DString *dsPtr,
			    char *string, int length));
EXTERN char *		Tcl_DStringAppendElement _ANSI_ARGS_((
			    Tcl_DString *dsPtr, char *string));
EXTERN void		Tcl_DStringEndSublist _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringFree _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringGetResult _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringInit _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringResult _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringSetLength _ANSI_ARGS_((Tcl_DString *dsPtr,
			    int length));
EXTERN void		Tcl_DStringStartSublist _ANSI_ARGS_((
			    Tcl_DString *dsPtr));
EXTERN void		Tcl_EnterFile _ANSI_ARGS_((Tcl_Interp *interp,
			    FILE *file, int permissions));
EXTERN char *		Tcl_ErrnoId _ANSI_ARGS_((void));
EXTERN int		Tcl_Eval _ANSI_ARGS_((Tcl_Interp *interp, char *cmd));
EXTERN int		Tcl_EvalFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName));
EXTERN int		Tcl_ExprBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *ptr));
EXTERN int		Tcl_ExprDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *ptr));
EXTERN int		Tcl_ExprLong _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, long *ptr));
EXTERN int		Tcl_ExprString _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN int		Tcl_FilePermissions _ANSI_ARGS_((FILE *file));
EXTERN Tcl_HashEntry *	Tcl_FirstHashEntry _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr,
			    Tcl_HashSearch *searchPtr));
EXTERN int		Tcl_GetBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *boolPtr));
EXTERN int		Tcl_GetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN char *		Tcl_GetCommandName _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Command command));
EXTERN int		Tcl_GetDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *doublePtr));
EXTERN int		Tcl_GetInt _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *intPtr));
EXTERN int		Tcl_GetOpenFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int write, int checkUsage,
			    FILE **filePtr));
EXTERN char *		Tcl_GetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags));
EXTERN char *		Tcl_GetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags));
EXTERN int		Tcl_GlobalEval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *command));
EXTERN char *		Tcl_HashStats _ANSI_ARGS_((Tcl_HashTable *tablePtr));
EXTERN int		Tcl_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_InitHashTable _ANSI_ARGS_((Tcl_HashTable *tablePtr,
			    int keyType));
EXTERN void		Tcl_InitMemory _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_LinkVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *addr, int type));
EXTERN void		Tcl_Main _ANSI_ARGS_((int argc, char **argv,
			    Tcl_AppInitProc *appInitProc));
EXTERN char *		Tcl_Merge _ANSI_ARGS_((int argc, char **argv));
EXTERN Tcl_HashEntry *	Tcl_NextHashEntry _ANSI_ARGS_((
			    Tcl_HashSearch *searchPtr));
EXTERN char *		Tcl_ParseVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char **termPtr));
EXTERN char *		Tcl_PosixError _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_PrintDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    double value, char *dst));
EXTERN void		Tcl_ReapDetachedProcs _ANSI_ARGS_((void));
EXTERN int		Tcl_RecordAndEval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmd, int flags));
EXTERN Tcl_RegExp	Tcl_RegExpCompile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN int		Tcl_RegExpExec _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_RegExp regexp, char *string, char *start));
EXTERN int		Tcl_RegExpMatch _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *pattern));
EXTERN void		Tcl_RegExpRange _ANSI_ARGS_((Tcl_RegExp regexp,
			    int index, char **startPtr, char **endPtr));
EXTERN void		Tcl_ResetResult _ANSI_ARGS_((Tcl_Interp *interp));
#define Tcl_Return Tcl_SetResult
EXTERN int		Tcl_ScanElement _ANSI_ARGS_((char *string,
			    int *flagPtr));
EXTERN int		Tcl_SetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN void		Tcl_SetErrorCode _ANSI_ARGS_(
			    VARARGS(Tcl_Interp *interp));
EXTERN int		Tcl_SetRecursionLimit _ANSI_ARGS_((Tcl_Interp *interp,
			    int depth));
EXTERN void		Tcl_SetResult _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tcl_FreeProc *freeProc));
EXTERN char *		Tcl_SetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *newValue, int flags));
EXTERN char *		Tcl_SetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, char *newValue,
			    int flags));
EXTERN char *		Tcl_SignalId _ANSI_ARGS_((int sig));
EXTERN char *		Tcl_SignalMsg _ANSI_ARGS_((int sig));
EXTERN int		Tcl_SplitList _ANSI_ARGS_((Tcl_Interp *interp,
			    char *list, int *argcPtr, char ***argvPtr));
EXTERN int		Tcl_StringMatch _ANSI_ARGS_((char *string,
			    char *pattern));
EXTERN char *		Tcl_TildeSubst _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN int		Tcl_TraceVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags, Tcl_VarTraceProc *proc,
			    ClientData clientData));
EXTERN int		Tcl_TraceVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *proc, ClientData clientData));
EXTERN void		Tcl_UnlinkVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName));
EXTERN int		Tcl_UnsetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags));
EXTERN int		Tcl_UnsetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags));
EXTERN void		Tcl_UntraceVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags, Tcl_VarTraceProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_UntraceVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *proc, ClientData clientData));
EXTERN int		Tcl_UpVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *frameName, char *varName,
			    char *localName, int flags));
EXTERN int		Tcl_UpVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *frameName, char *part1, char *part2,
			    char *localName, int flags));
EXTERN int		Tcl_VarEval _ANSI_ARGS_(VARARGS(Tcl_Interp *interp));
EXTERN ClientData	Tcl_VarTraceInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags,
			    Tcl_VarTraceProc *procPtr,
			    ClientData prevClientData));
EXTERN ClientData	Tcl_VarTraceInfo2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *procPtr,
			    ClientData prevClientData));

#endif /* _TCL */
