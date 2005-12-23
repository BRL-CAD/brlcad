/*
 * bltVecInt.h --
 *
 *	This module implements vector data objects.
 *
 * Copyright 1995-1998 Lucent Technologies, Inc.
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
#include <bltHash.h>
#include <bltChain.h>

#define VECTOR_THREAD_KEY	"BLT Vector Data"
#define VECTOR_MAGIC		((unsigned int) 0x46170277)

/* These defines allow parsing of different types of indices */

#define INDEX_SPECIAL	(1<<0)	/* Recognize "min", "max", and "++end" as
				 * valid indices */
#define INDEX_COLON	(1<<1)	/* Also recognize a range of indices 
				 * separated by a colon */
#define INDEX_CHECK	(1<<2)	/* Verify that the specified index or 
				 * range of indices are within limits */
#define INDEX_ALL_FLAGS    (INDEX_SPECIAL | INDEX_COLON | INDEX_CHECK)

#define SPECIAL_INDEX		-2


typedef struct {
    Blt_HashTable vectorTable;	/* Table of vectors */
    Blt_HashTable mathProcTable; /* Table of vector math functions */
    Blt_HashTable indexProcTable;
    Tcl_Interp *interp;
    unsigned int nextId;
} VectorInterpData;

/*
 * VectorObject --
 *
 *	A vector is an array of double precision values.  It can be
 *	accessed through a Tcl command, a Tcl array variable, or C
 *	API. The storage for the array points initially to a
 *	statically allocated buffer, but to malloc-ed memory if more
 *	is necessary.
 *
 *	Vectors can be shared by several clients (for example, two
 *	different graph widgets).  The data is shared. When a client
 *	wants to use a vector, it allocates a vector identifier, which
 *	identifies the client.  Clients use this ID to specify a
 *	callback routine to be invoked whenever the vector is modified
 *	or destroyed.  Whenever the vector is updated or destroyed,
 *	each client is notified of the change by their callback
 *	routine.
 */

typedef struct {

    /*
     * If you change these fields, make sure you change the definition
     * of Blt_Vector in bltInt.h and blt.h too.
     */

    double *valueArr;		/* Array of values (malloc-ed) */

    int length;			/* Current number of values in the array. */

    int size;			/* Maximum number of values that can be stored
				 * in the value array. */

    double min, max;		/* Minimum and maximum values in the vector */

    int dirty;			/* Indicates if the vector has been updated */

    int reserved;

    /* The following fields are local to this module  */

    char *name;			/* The namespace-qualified name of the vector.
				 * It points to the hash key allocated for the
				 * entry in the vector hash table. */

    VectorInterpData *dataPtr;
    Tcl_Interp *interp;		/* Interpreter associated with the
				 * vector */

    Blt_HashEntry *hashPtr;	/* If non-NULL, pointer in a hash table to
				 * track the vectors in use. */

    Tcl_FreeProc *freeProc;	/* Address of procedure to call to
				 * release storage for the value
				 * array, Optionally can be one of the
				 * following: TCL_STATIC, TCL_DYNAMIC,
				 * or TCL_VOLATILE. */

    char *arrayName;		/* The name of the Tcl array variable
				 * mapped to the vector
				 * (malloc'ed). If NULL, indicates
				 * that the vector isn't mapped to any
				 * variable */

    Tcl_Namespace *varNsPtr;	/* Namespace context of the Tcl variable
				 * associated with the vector. This is
				 * needed to reset the indices of the array
				 * variable. */

    Tcl_Namespace *nsPtr;	/* Namespace context of the vector itself. */

    int offset;			/* Offset from zero of the vector's
				 * starting index */

    Tcl_Command cmdToken;	/* Token for vector's Tcl command. */

    Blt_Chain *chainPtr;	/* List of clients using this vector */

    int notifyFlags;		/* Notification flags. See definitions
				 * below */

    int varFlags;		/* Indicate if the variable is global,
				 * namespace, or local */

    int freeOnUnset;		/* For backward compatibility only: If
				 * non-zero, free the vector when its
				 * variable is unset. */
    int flush;

    int first, last;		/* Selected region of vector. This is used
				 * mostly for the math routines */
} VectorObject;

#define NOTIFY_UPDATED		((int)BLT_VECTOR_NOTIFY_UPDATE)
#define NOTIFY_DESTROYED	((int)BLT_VECTOR_NOTIFY_DESTROY)

#define NOTIFY_NEVER		(1<<3)	/* Never notify clients of updates to
					 * the vector */
#define NOTIFY_ALWAYS		(1<<4)	/* Notify clients after each update
					 * of the vector is made */
#define NOTIFY_WHENIDLE		(1<<5)	/* Notify clients at the next idle point
					 * that the vector has been updated. */

#define NOTIFY_PENDING		(1<<6)	/* A do-when-idle notification of the
					 * vector's clients is pending. */
#define NOTIFY_NOW		(1<<7)	/* Notify clients of changes once
					 * immediately */

#define NOTIFY_WHEN_MASK	(NOTIFY_NEVER|NOTIFY_ALWAYS|NOTIFY_WHENIDLE)

#define UPDATE_RANGE		(1<<9)	/* The data of the vector has changed.
					 * Update the min and max limits when
					 * they are needed */

#define FindRange(array, first, last, min, max) \
{ \
    min = max = 0.0; \
    if (first <= last) { \
	register int i; \
	min = max = array[first]; \
	for (i = first + 1; i <= last; i++) { \
	    if (min > array[i]) { \
		min = array[i]; \
	    } else if (max < array[i]) { \
		max = array[i]; \
	    } \
	} \
    } \
}

extern void Blt_VectorInstallSpecialIndices 
	_ANSI_ARGS_((Blt_HashTable *tablePtr));

extern void Blt_VectorInstallMathFunctions 
	_ANSI_ARGS_((Blt_HashTable *tablePtr));

extern void Blt_VectorUninstallMathFunctions 
	_ANSI_ARGS_((Blt_HashTable *tablePtr));

extern VectorInterpData *Blt_VectorGetInterpData 
	_ANSI_ARGS_((Tcl_Interp *interp));

extern VectorObject *Blt_VectorNew _ANSI_ARGS_((VectorInterpData *dataPtr));

extern int Blt_VectorDuplicate _ANSI_ARGS_((VectorObject *destPtr, 
	VectorObject *srcPtr));

extern int Blt_VectorChangeLength _ANSI_ARGS_((VectorObject *vPtr, 
	int length));

extern VectorObject *Blt_VectorParseElement _ANSI_ARGS_((Tcl_Interp *interp, 
	VectorInterpData *dataPtr, CONST char *start, char **endPtr, 
	int flags));

extern void Blt_VectorFree _ANSI_ARGS_((VectorObject *vPtr));

extern int *Blt_VectorSortIndex _ANSI_ARGS_((VectorObject **vPtrPtr, 
	int nVectors));

extern int Blt_VectorLookupName _ANSI_ARGS_((VectorInterpData *dataPtr, 
	char *vecName, VectorObject **vPtrPtr));

extern VectorObject *Blt_VectorCreate _ANSI_ARGS_((VectorInterpData *dataPtr, 
	CONST char *name, CONST char *cmdName, CONST char *varName, 
	int *newPtr));

extern void Blt_VectorUpdateRange _ANSI_ARGS_((VectorObject *vPtr));

extern void Blt_VectorUpdateClients _ANSI_ARGS_((VectorObject *vPtr));

extern void Blt_VectorFlushCache _ANSI_ARGS_((VectorObject *vPtr));

extern int Blt_VectorReset _ANSI_ARGS_((VectorObject *vPtr, double *dataArr,
	int nValues, int arraySize, Tcl_FreeProc *freeProc));

extern int  Blt_VectorGetIndex _ANSI_ARGS_((Tcl_Interp *interp, 
	VectorObject *vPtr, CONST char *string, int *indexPtr, int flags,
	Blt_VectorIndexProc **procPtrPtr));

extern int  Blt_VectorGetIndexRange _ANSI_ARGS_((Tcl_Interp *interp, 
	VectorObject *vPtr, CONST char *string, int flags, 
	Blt_VectorIndexProc **procPtrPtr));

extern int Blt_VectorMapVariable _ANSI_ARGS_((Tcl_Interp *interp, 
	VectorObject *vPtr, CONST char *name));

#if (TCL_MAJOR_VERSION == 7) 
extern Tcl_CmdProc Blt_VectorInstCmd;
#else 
extern Tcl_ObjCmdProc Blt_VectorInstCmd;
#endif

extern Tcl_VarTraceProc Blt_VectorVarTrace;

extern Tcl_IdleProc Blt_VectorNotifyClients;
