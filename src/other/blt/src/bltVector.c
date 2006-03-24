/*
 * bltVector.c --
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

/*
 * TODO:
 *	o Add H. Kirsch's vector binary read operation
 *		x binread file0
 *		x binread -file file0
 *
 *	o Add ASCII/binary file reader
 *		x read fileName
 *
 *	o Allow Tcl-based client notifications.
 *		vector x
 *		x notify call Display
 *		x notify delete Display
 *		x notify reorder #1 #2
 */

#include "bltVecInt.h"
#include <bltMath.h>
#include <ctype.h>

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

#ifndef TCL_NAMESPACE_ONLY
#define TCL_NAMESPACE_ONLY TCL_GLOBAL_ONLY
#endif

#define DEF_ARRAY_SIZE		64
#define VECFLAGS(v)	\
	(((v)->varNsPtr != NULL) ? (TCL_NAMESPACE_ONLY | TCL_GLOBAL_ONLY) : 0;
#define TRACE_ALL  (TCL_TRACE_WRITES | TCL_TRACE_READS | TCL_TRACE_UNSETS)


#define VECTOR_CHAR(c)	((isalnum(UCHAR(c))) || \
	(c == '_') || (c == ':') || (c == '@') || (c == '.'))


/*
 * VectorClient --
 *
 *	A vector can be shared by several clients.  Each client
 *	allocates this structure that acts as its key for using the
 *	vector.  Clients can also designate a callback routine that is
 *	executed whenever the vector is updated or destroyed.
 *
 */
typedef struct {
    unsigned int magic;		/* Magic value designating whether this
				 * really is a vector token or not */

    VectorObject *serverPtr;	/* Pointer to the master record of the
				 * vector.  If NULL, indicates that the
				 * vector has been destroyed but as of
				 * yet, this client hasn't recognized
				 * it. */

    Blt_VectorChangedProc *proc;/* Routine to call when the contents
				 * of the vector change or the vector
				 * is deleted. */

    ClientData clientData;	/* Data passed whenever the vector
				 * change procedure is called. */

    Blt_ChainLink *linkPtr;	/* Used to quickly remove this entry from
				 * its server's client chain. */
} VectorClient;

static Tcl_CmdDeleteProc VectorInstDeleteProc;
static Tcl_CmdProc VectorCmd;
static Tcl_InterpDeleteProc VectorInterpDeleteProc;

#if defined(HAVE_SRAND48) && defined(NO_DECL_SRAND48)
extern void srand48 _ANSI_ARGS_((long int seed));
#endif

static VectorObject *
FindVectorInNamespace(dataPtr, nsPtr, vecName)
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Namespace *nsPtr;
    CONST char *vecName;
{
    Tcl_DString dString;
    CONST char *name;
    Blt_HashEntry *hPtr;

    name = Blt_GetQualifiedName(nsPtr, vecName, &dString);
    hPtr = Blt_FindHashEntry(&(dataPtr->vectorTable), name);
    Tcl_DStringFree(&dString);
    if (hPtr != NULL) {
	return (VectorObject *)Blt_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetVectorObject --
 *
 *	Searches for the vector associated with the name given.
 *	Allow for a range specification.
 *
 * Results:
 *	Returns a pointer to the vector if found, otherwise NULL.
 *
 * ----------------------------------------------------------------------
 */
static VectorObject *
GetVectorObject(dataPtr, name, flags)
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    CONST char *name;
    int flags;
{
    CONST char *vecName;
    Tcl_Namespace *nsPtr;
    VectorObject *vPtr;

    nsPtr = NULL;
    vecName = name;
    if (Blt_ParseQualifiedName(dataPtr->interp, name, &nsPtr, &vecName) 
	!= TCL_OK) {
	return NULL;		/* Can't find namespace. */
    } 
    vPtr = NULL;
    if (nsPtr != NULL) {
	vPtr = FindVectorInNamespace(dataPtr, nsPtr, vecName);
    } else {
	if (flags & NS_SEARCH_CURRENT) {
	    nsPtr = Tcl_GetCurrentNamespace(dataPtr->interp);
	    vPtr = FindVectorInNamespace(dataPtr, nsPtr, vecName);
	}
	if ((vPtr == NULL) && (flags & NS_SEARCH_GLOBAL)) {
	    nsPtr = Tcl_GetGlobalNamespace(dataPtr->interp);
	    vPtr = FindVectorInNamespace(dataPtr, nsPtr, vecName);
	}
    }
    return vPtr;
}

void
Blt_VectorUpdateRange(vPtr)
    VectorObject *vPtr;
{
    double min, max;
    register int i;

    min = DBL_MAX, max = -DBL_MAX;
    for (i = 0; i < vPtr->length; i++) {
	if (FINITE(vPtr->valueArr[i])) {
	    min = max = vPtr->valueArr[i];
	    break;
	}
    }
    for (/* empty */; i < vPtr->length; i++) {
	if (FINITE(vPtr->valueArr[i])) {
	    if (min > vPtr->valueArr[i]) {
		min = vPtr->valueArr[i]; 
	    } else if (max < vPtr->valueArr[i]) { 
		max = vPtr->valueArr[i]; 
	    } 
	} 
    } 
    vPtr->min = min;
    vPtr->max = max;
    vPtr->notifyFlags &= ~UPDATE_RANGE;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorGetIndex --
 *
 *	Converts the string representing an index in the vector, to
 *	its numeric value.  A valid index may be an numeric string of
 *	the string "end" (indicating the last element in the string).
 *
 * Results:
 *	A standard Tcl result.  If the string is a valid index, TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and interp->result
 *	will contain an error message.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorGetIndex(interp, vPtr, string, indexPtr, flags, procPtrPtr)
    Tcl_Interp *interp;
    VectorObject *vPtr;
    CONST char *string;
    int *indexPtr;
    int flags;
    Blt_VectorIndexProc **procPtrPtr;
{
    char c;
    int value;

    c = string[0];

    /* Treat the index "end" like a numeric index.  */

    if ((c == 'e') && (strcmp(string, "end") == 0)) {
	if (vPtr->length < 1) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "bad index \"end\": vector is empty", 
				 (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	*indexPtr = vPtr->length - 1;
	return TCL_OK;
    } else if ((c == '+') && (strcmp(string, "++end") == 0)) {
	*indexPtr = vPtr->length;
	return TCL_OK;
    }
    if (procPtrPtr != NULL) {
	Blt_HashEntry *hPtr;

	hPtr = Blt_FindHashEntry(&(vPtr->dataPtr->indexProcTable), string);
	if (hPtr != NULL) {
	    *indexPtr = SPECIAL_INDEX;
	    *procPtrPtr = (Blt_VectorIndexProc *) Blt_GetHashValue(hPtr);
	    return TCL_OK;
	}
    }
    if (Tcl_GetInt(interp, (char *)string, &value) != TCL_OK) {
	long int lvalue;
	/*   
	 * Unlike Tcl_GetInt, Tcl_ExprLong needs a valid interpreter,
	 * but the interp passed in may be NULL.  So we have to use
	 * vPtr->interp and then reset the result.  
	 */
	if (Tcl_ExprLong(vPtr->interp, (char *)string, &lvalue) != TCL_OK) {
	    Tcl_ResetResult(vPtr->interp);
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "bad index \"", string, "\"", 
				 (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	value = lvalue;
    }
    /*
     * Correct the index by the current value of the offset. This makes
     * all the numeric indices non-negative, which is how we distinguish
     * the special non-numeric indices.
     */
    value -= vPtr->offset;

    if ((value < 0) || ((flags & INDEX_CHECK) && (value >= vPtr->length))) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "index \"", string, "\" is out of range", 
			 (char *)NULL);
	}
	return TCL_ERROR;
    }
    *indexPtr = (int)value;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorGetIndexRange --
 *
 *	Converts the string representing an index in the vector, to
 *	its numeric value.  A valid index may be an numeric string of
 *	the string "end" (indicating the last element in the string).
 *
 * Results:
 *	A standard Tcl result.  If the string is a valid index, TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and interp->result
 *	will contain an error message.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorGetIndexRange(interp, vPtr, string, flags, procPtrPtr)
    Tcl_Interp *interp;
    VectorObject *vPtr;
    CONST char *string;
    int flags;
    Blt_VectorIndexProc **procPtrPtr;
{
    int ielem;
    char *colon;

    colon = NULL;
    if (flags & INDEX_COLON) {
	colon = strchr(string, ':');
    }
    if (colon != NULL) {
	if (string == colon) {
	    vPtr->first = 0;	/* Default to the first index */
	} else {
	    int result;

	    *colon = '\0';
	    result = Blt_VectorGetIndex(interp, vPtr, string, &ielem, flags,
		(Blt_VectorIndexProc **) NULL);
	    *colon = ':';
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	    vPtr->first = ielem;
	}
	if (*(colon + 1) == '\0') {
	    /* Default to the last index */
	    vPtr->last = (vPtr->length > 0) ? vPtr->length - 1 : 0;
	} else {
	    if (Blt_VectorGetIndex(interp, vPtr, colon + 1, &ielem, flags,
		    (Blt_VectorIndexProc **) NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    vPtr->last = ielem;
	}
	if (vPtr->first > vPtr->last) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "bad range \"", string,
			 "\" (first > last)", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    } else {
	if (Blt_VectorGetIndex(interp, vPtr, string, &ielem, flags, 
		       procPtrPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	vPtr->last = vPtr->first = ielem;
    }
    return TCL_OK;
}

VectorObject *
Blt_VectorParseElement(interp, dataPtr, start, endPtr, flags)
    Tcl_Interp *interp;
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    CONST char *start;
    char **endPtr;
    int flags;
{
    register char *p;
    char saved;
    VectorObject *vPtr;

    p = (char *)start;
    /* Find the end of the vector name */
    while (VECTOR_CHAR(*p)) {
	p++;
    }
    saved = *p;
    *p = '\0';

    vPtr = GetVectorObject(dataPtr, start, flags);
    if (vPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find vector \"", start, "\"", 
			     (char *)NULL);
	}
	*p = saved;
	return NULL;
    }
    *p = saved;
    vPtr->first = 0;
    vPtr->last = vPtr->length - 1;
    if (*p == '(') {
	int count, result;

	start = p + 1;
	p++;

	/* Find the matching right parenthesis */
	count = 1;
	while (*p != '\0') {
	    if (*p == ')') {
		count--;
		if (count == 0) {
		    break;
		}
	    } else if (*p == '(') {
		count++;
	    }
	    p++;
	}
	if (count > 0) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "unbalanced parentheses \"", start, 
			"\"", (char *)NULL);
	    }
	    return NULL;
	}
	*p = '\0';
	result = Blt_VectorGetIndexRange(interp, vPtr, start, 
		(INDEX_COLON | INDEX_CHECK), (Blt_VectorIndexProc **) NULL);
	*p = ')';
	if (result != TCL_OK) {
	    return NULL;
	}
	p++;
    }
    if (endPtr != NULL) {
      *endPtr = p;
    }
    return vPtr;
}


/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorNotifyClients --
 *
 *	Notifies each client of the vector that the vector has changed
 *	(updated or destroyed) by calling the provided function back.
 *	The function pointer may be NULL, in that case the client is
 *	not notified.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The results depend upon what actions the client callbacks
 *	take.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_VectorNotifyClients(clientData)
    ClientData clientData;
{
    VectorObject *vPtr = clientData;
    Blt_ChainLink *linkPtr;
    VectorClient *clientPtr;
    Blt_VectorNotify notify;

    notify = (vPtr->notifyFlags & NOTIFY_DESTROYED)
	? BLT_VECTOR_NOTIFY_DESTROY : BLT_VECTOR_NOTIFY_UPDATE;
    vPtr->notifyFlags &= ~(NOTIFY_UPDATED | NOTIFY_DESTROYED | NOTIFY_PENDING);

    for (linkPtr = Blt_ChainFirstLink(vPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	if (clientPtr->proc != NULL) {
	    (*clientPtr->proc) (vPtr->interp, clientPtr->clientData, notify);
	}
    }
    /*
     * Some clients may not handle the "destroy" callback properly
     * (they should call Blt_FreeVectorId to release the client
     * identifier), so mark any remaining clients to indicate that
     * vector's server has gone away.
     */
    if (notify == BLT_VECTOR_NOTIFY_DESTROY) {
	for (linkPtr = Blt_ChainFirstLink(vPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    clientPtr = Blt_ChainGetValue(linkPtr);
	    clientPtr->serverPtr = NULL;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorUpdateClients --
 *
 *	Notifies each client of the vector that the vector has changed
 *	(updated or destroyed) by calling the provided function back.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The individual client callbacks are eventually invoked.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_VectorUpdateClients(vPtr)
    VectorObject *vPtr;
{
    vPtr->dirty++;
    vPtr->max = vPtr->min = bltNaN;
    if (vPtr->notifyFlags & NOTIFY_NEVER) {
	return;
    }
    vPtr->notifyFlags |= NOTIFY_UPDATED;
    if (vPtr->notifyFlags & NOTIFY_ALWAYS) {
	Blt_VectorNotifyClients(vPtr);
	return;
    }
    if (!(vPtr->notifyFlags & NOTIFY_PENDING)) {
	vPtr->notifyFlags |= NOTIFY_PENDING;
	Tcl_DoWhenIdle(Blt_VectorNotifyClients, vPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorFlushCache --
 *
 *	Unsets all the elements of the Tcl array variable associated
 *	with the vector, freeing memory associated with the variable.
 *	This includes both the hash table and the hash keys.  The down
 *	side is that this effectively flushes the caching of vector
 *	elements in the array.  This means that the subsequent reads
 *	of the array will require a decimal to string conversion.
 *
 *	This is needed when the vector changes its values, making
 *	the array variable out-of-sync.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All elements of array variable (except one) are unset, freeing
 *	the memory associated with the variable.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_VectorFlushCache(vPtr)
    VectorObject *vPtr;
{
    Tcl_CallFrame *framePtr;
    Tcl_Interp *interp = vPtr->interp;

    if (vPtr->arrayName == NULL) {
	return;			/* Doesn't use the variable API */
    }
    framePtr = NULL;
    if (vPtr->varNsPtr != NULL) {
	framePtr = Blt_EnterNamespace(interp, vPtr->varNsPtr);
    }
    /* Turn off the trace temporarily so that we can unset all the
     * elements in the array.  */

    Tcl_UntraceVar2(interp, vPtr->arrayName, (char *)NULL,
	TRACE_ALL | vPtr->varFlags, Blt_VectorVarTrace, vPtr);

    /* Clear all the element entries from the entire array */
    Tcl_UnsetVar2(interp, vPtr->arrayName, (char *)NULL, vPtr->varFlags);

    /* Restore the "end" index by default and the trace on the entire array */
    Tcl_SetVar2(interp, vPtr->arrayName, "end", "", vPtr->varFlags);
    Tcl_TraceVar2(interp, vPtr->arrayName, (char *)NULL,
	TRACE_ALL | vPtr->varFlags, Blt_VectorVarTrace, vPtr);

    if ((vPtr->varNsPtr != NULL) && (framePtr != NULL)) {
	Blt_LeaveNamespace(interp, framePtr);	/* Go back to current */
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorLookupName --
 *
 *	Searches for the vector associated with the name given.  Allow
 *	for a range specification.
 *
 * Results:
 *	Returns a pointer to the vector if found, otherwise NULL.
 *	If the name is not associated with a vector and the
 *	TCL_LEAVE_ERR_MSG flag is set, and interp->result will contain
 *	an error message.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorLookupName(dataPtr, vecName, vPtrPtr)
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    char *vecName;
    VectorObject **vPtrPtr;
{
    VectorObject *vPtr;
    char *endPtr;

    vPtr = Blt_VectorParseElement(dataPtr->interp, dataPtr, vecName, &endPtr, 
	NS_SEARCH_BOTH);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    if (*endPtr != '\0') {
	Tcl_AppendResult(dataPtr->interp, 
			 "extra characters after vector name", (char *)NULL);
	return TCL_ERROR;
    }
    *vPtrPtr = vPtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteCommand --
 *
 *	Deletes the Tcl command associated with the vector, without
 *	triggering a callback to "VectorInstDeleteProc".
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
DeleteCommand(vPtr)
    VectorObject *vPtr;		/* Vector associated with the Tcl command. */
{
    Tcl_Interp *interp = vPtr->interp;
    char *qualName;		/* Name of Tcl command. */
    Tcl_CmdInfo cmdInfo;
    Tcl_DString dString;

    Tcl_DStringInit(&dString);
    qualName = Blt_GetQualifiedName(
	Blt_GetCommandNamespace(interp, vPtr->cmdToken), 
	Tcl_GetCommandName(interp, vPtr->cmdToken), &dString);
    if (Tcl_GetCommandInfo(interp, qualName, &cmdInfo)) {
	cmdInfo.deleteProc = NULL;	/* Disable the callback before
					 * deleting the Tcl command.*/
	Tcl_SetCommandInfo(interp, qualName, &cmdInfo);
	Tcl_DeleteCommandFromToken(interp, vPtr->cmdToken);
    }
    Tcl_DStringFree(&dString);
    vPtr->cmdToken = 0;
}

/*
 * ----------------------------------------------------------------------
 *
 * UnmapVariable --
 *
 *	Destroys the trace on the current Tcl variable designated
 *	to access the vector.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
UnmapVariable(vPtr)
    VectorObject *vPtr;
{
    Tcl_Interp *interp = vPtr->interp;
    Tcl_CallFrame *framePtr;

    framePtr = NULL;
    if (vPtr->varNsPtr != NULL) {	/* Activate namespace */
	framePtr = Blt_EnterNamespace(interp, vPtr->varNsPtr);
    }
    /* Unset the entire array */
    Tcl_UntraceVar2(interp, vPtr->arrayName, (char *)NULL,
	(TRACE_ALL | vPtr->varFlags), Blt_VectorVarTrace, vPtr);
    Tcl_UnsetVar2(interp, vPtr->arrayName, (char *)NULL, vPtr->varFlags);

    if ((vPtr->varNsPtr != NULL) && (framePtr != NULL)) {
	/* Go back to current namespace */
	Blt_LeaveNamespace(interp, framePtr);
    }
    if (vPtr->arrayName != NULL) {
	Blt_Free(vPtr->arrayName);
	vPtr->arrayName = NULL;
    }
    vPtr->varNsPtr = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorMapVariable --
 *
 *	Sets up traces on a Tcl variable to access the vector.
 *
 *	If another variable is already mapped, it's first untraced and
 *	removed.  Don't do anything else for variables named "" (even
 *	though Tcl allows this pathology). Saves the name of the new
 *	array variable.
 *
 * Results:
 *	A standard Tcl result. If an error occurs setting the variable
 *	TCL_ERROR is returned and an error message is left in the
 *	interpreter.
 *
 * Side effects:
 *	Traces are set for the new variable. The new variable name is
 *	saved in a malloc'ed string in vPtr->arrayName.  If this
 *	variable is non-NULL, it indicates that a Tcl variable has
 *	been mapped to this vector.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorMapVariable(interp, vPtr, name)
    Tcl_Interp *interp;
    VectorObject *vPtr;
    CONST char *name;
{
    Tcl_Namespace *nsPtr;
    Tcl_CallFrame *framePtr;
    CONST char *varName;
    CONST char *result;

    if (vPtr->arrayName != NULL) {
	UnmapVariable(vPtr);
    }
    if ((name == NULL) || (name[0] == '\0')) {
	return TCL_OK;		/* If the variable name is the empty
				 * string, simply return after
				 * removing any existing variable. */
    }
    framePtr = NULL;

    /* Get the variable name (without the namespace qualifier). */
    if (Blt_ParseQualifiedName(interp, name, &nsPtr, &varName) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", name, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (nsPtr != NULL) {
	/* [incr Tcl] 2.x doesn't like qualifiers with variable names,
	 * so we need to enter the namespace if one was designated. */
	framePtr = Blt_EnterNamespace(interp, nsPtr);
    }
    /*
     * To play it safe, delete the variable first.  This has
     * side-effect of unmapping the variable from any vector that may
     * be currently using it.
     */
    Tcl_UnsetVar2(interp, (char *)varName, (char *)NULL, 0);

    /* Set the index "end" in the array.  This will create the
     * variable immediately so that we can check its namespace
     * context.  */
    result = Tcl_SetVar2(interp, (char *)varName, "end", "", TCL_LEAVE_ERR_MSG);

    /* Determine if the variable is global or not.  If there wasn't a
     * namespace qualifier, it still may be global.  We need to look
     * inside the Var structure to see what it's namespace field says.
     * NULL indicates that it's local.  */

    vPtr->varNsPtr = Blt_GetVariableNamespace(interp, varName);
    vPtr->varFlags = (vPtr->varNsPtr != NULL) ?
	(TCL_NAMESPACE_ONLY | TCL_GLOBAL_ONLY) : 0;

    if (result != NULL) {
	/* Trace the array on reads, writes, and unsets */
	Tcl_TraceVar2(interp, (char *)varName, (char *)NULL, 
		(TRACE_ALL | vPtr->varFlags), Blt_VectorVarTrace, vPtr);
    }
    if ((nsPtr != NULL) && (framePtr != NULL)) {
	Blt_LeaveNamespace(interp, framePtr);	/* Go back to current */
    }
    vPtr->arrayName = Blt_Strdup(varName);
    return (result == NULL) ? TCL_ERROR : TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorChangeLength --
 *
 *	Resizes the vector to the new size.
 *
 *	The new size of the vector is computed by doubling the
 *	size of the vector until it fits the number of slots needed
 *	(designated by *length*).
 *
 *	If the new size is the same as the old, simply adjust the
 *	length of the vector.  Otherwise we're copying the data from
 *	one memory location to another. The trailing elements of the 
 *	vector need to be reset to zero.
 *
 *	If the storage changed memory locations, free up the old
 *	location if it was dynamically allocated.
 *
 * Results:
 *	A standard Tcl result.  If the reallocation is successful,
 *	TCL_OK is returned, otherwise TCL_ERROR.
 *
 * Side effects:
 *	Memory for the array is reallocated.
 *
 * ----------------------------------------------------------------------
 */

int
Blt_VectorChangeLength(vPtr, length)
    VectorObject *vPtr;
    int length;
{
    int newSize;		/* Size of array in elements */
    double *newArr;
    Tcl_FreeProc *freeProc;

    newArr = NULL;
    newSize = 0;
    freeProc = TCL_STATIC;

    if (length > 0) {
	int wanted, used;

	wanted = length;
	used = vPtr->length;

	/* Compute the new size by doubling old size until it's big enough */
	newSize = DEF_ARRAY_SIZE;
	if (wanted > DEF_ARRAY_SIZE) {
	    while (newSize < wanted) {
		newSize += newSize;
	    }
	}
	freeProc = vPtr->freeProc;
	if (newSize == vPtr->size) {
	    newArr = vPtr->valueArr; /* Same size, use current array. */
	} else {
	    /* Dynamically allocate memory for the new array. */
	    newArr = Blt_Malloc(newSize * sizeof(double));
	    if (newArr == NULL) {
		Tcl_AppendResult(vPtr->interp, "can't allocate ", 
		Blt_Itoa(newSize), " elements for vector \"", vPtr->name, 
				 "\"", (char *)NULL); return TCL_ERROR;
	    }
	    if (used > wanted) {
		used = wanted;
	    }
	    /* Copy any previous data */
	    if (used > 0) {
		memcpy(newArr, vPtr->valueArr, used * sizeof(double));
	    }
	    freeProc = TCL_DYNAMIC;
	}
	/* Clear any new slots that we're now using in the array */
	if (wanted > used) {
	    memset(newArr + used, 0, (wanted - used) * sizeof(double));
	}
    }
    if ((newArr != vPtr->valueArr) && (vPtr->valueArr != NULL)) {
	/* 
	 * We're not using the old storage anymore, so free it if it's
	 * not static.  It's static because the user previously reset
	 * the vector with a statically allocated array (setting freeProc 
	 * to TCL_STATIC).  
	 */
	if (vPtr->freeProc != TCL_STATIC) {
	    if (vPtr->freeProc == TCL_DYNAMIC) {
		Blt_Free(vPtr->valueArr);
	    } else {
		(*vPtr->freeProc) ((char *)vPtr->valueArr);
	    }
	}
    }
    vPtr->valueArr = newArr;
    vPtr->size = newSize;
    vPtr->length = length;
    vPtr->first = 0;
    vPtr->last = length - 1;
    vPtr->freeProc = freeProc;	/* Set the type of the new storage */
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ResetVector --
 *
 *	Resets the vector data.  This is called by a client to
 *	indicate that the vector data has changed.  The vector does
 *	not need to point to different memory.  Any clients of the
 *	vector will be notified of the change.
 *
 * Results:
 *	A standard Tcl result.  If the new array size is invalid,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	new vector data is recorded.
 *
 * Side Effects:
 *	Any client designated callbacks will be posted.  Memory may
 *	be changed for the vector array.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_VectorReset(vPtr, valueArr, length, size, freeProc)
    VectorObject *vPtr;
    double *valueArr;		/* Array containing the elements of the
				 * vector. If NULL, indicates to reset the 
				 * vector.*/
    int length;			/* The number of elements that the vector 
				 * currently holds. */
    int size;			/* The maximum number of elements that the
				 * array can hold. */
    Tcl_FreeProc *freeProc;	/* Address of memory deallocation routine
				 * for the array of values.  Can also be
				 * TCL_STATIC, TCL_DYNAMIC, or TCL_VOLATILE. */
{
    if (vPtr->valueArr != valueArr) {	/* New array of values resides
					 * in different memory than
					 * the current vector.  */
	if ((valueArr == NULL) || (size == 0)) {
	    /* Empty array. Set up default values */
	    freeProc = TCL_STATIC;
	    valueArr = NULL;
	    size = length = 0;
	} else if (freeProc == TCL_VOLATILE) {
	    double *newArr;
	    /* Data is volatile. Make a copy of the value array.  */
	    newArr = Blt_Malloc(size * sizeof(double));
	    if (newArr == NULL) {
		Tcl_AppendResult(vPtr->interp, "can't allocate ", 
			Blt_Itoa(size), " elements for vector \"", 
			vPtr->name, "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    memcpy((char *)newArr, (char *)valueArr, 
		   sizeof(double) * length);
	    valueArr = newArr;
	    freeProc = TCL_DYNAMIC;
	} 

	if (vPtr->freeProc != TCL_STATIC) {
	    /* Old data was dynamically allocated. Free it before
	     * attaching new data.  */
	    if (vPtr->freeProc == TCL_DYNAMIC) {
		Blt_Free(vPtr->valueArr);
	    } else {
		(*freeProc) ((char *)vPtr->valueArr);
	    }
	}
	vPtr->freeProc = freeProc;
	vPtr->valueArr = valueArr;
	vPtr->size = size;
    }

    vPtr->length = length;
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);
    return TCL_OK;
}

VectorObject *
Blt_VectorNew(dataPtr)
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
{
    VectorObject *vPtr;

    vPtr = Blt_Calloc(1, sizeof(VectorObject));
    assert(vPtr);
    vPtr->notifyFlags = NOTIFY_WHENIDLE;
    vPtr->freeProc = TCL_STATIC;
    vPtr->dataPtr = dataPtr;
    vPtr->valueArr = NULL;
    vPtr->length = vPtr->size = 0;
    vPtr->interp = dataPtr->interp;
    vPtr->hashPtr = NULL;
    vPtr->chainPtr = Blt_ChainCreate();
    vPtr->flush = FALSE;
    vPtr->min = vPtr->max = bltNaN;
    return vPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorFree --
 *
 *	Removes the memory and frees resources associated with the
 *	vector.
 *
 *	o Removes the trace and the Tcl array variable and unsets
 *	  the variable.
 *	o Notifies clients of the vector that the vector is being
 *	  destroyed.
 *	o Removes any clients that are left after notification.
 *	o Frees the memory (if necessary) allocated for the array.
 *	o Removes the entry from the hash table of vectors.
 *	o Frees the memory allocated for the name.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------
 */
void
Blt_VectorFree(vPtr)
    VectorObject *vPtr;
{
    Blt_ChainLink *linkPtr;
    VectorClient *clientPtr;

    if (vPtr->cmdToken != 0) {
	DeleteCommand(vPtr);
    }
    if (vPtr->arrayName != NULL) {
	UnmapVariable(vPtr);
    }
    vPtr->length = 0;

    /* Immediately notify clients that vector is going away */
    if (vPtr->notifyFlags & NOTIFY_PENDING) {
	vPtr->notifyFlags &= ~NOTIFY_PENDING;
	Tcl_CancelIdleCall(Blt_VectorNotifyClients, vPtr);
    }
    vPtr->notifyFlags |= NOTIFY_DESTROYED;
    Blt_VectorNotifyClients(vPtr);

    for (linkPtr = Blt_ChainFirstLink(vPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	Blt_Free(clientPtr);
    }
    Blt_ChainDestroy(vPtr->chainPtr);
    if ((vPtr->valueArr != NULL) && (vPtr->freeProc != TCL_STATIC)) {
	if (vPtr->freeProc == TCL_DYNAMIC) {
	    Blt_Free(vPtr->valueArr);
	} else {
	    (*vPtr->freeProc) ((char *)vPtr->valueArr);
	}
    }
    if (vPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&(vPtr->dataPtr->vectorTable), vPtr->hashPtr);
    }
#ifdef NAMESPACE_DELETE_NOTIFY
    if (vPtr->nsPtr != NULL) {
	Blt_DestroyNsDeleteNotify(vPtr->interp, vPtr->nsPtr, vPtr);
    }
#endif /* NAMESPACE_DELETE_NOTIFY */
    Blt_Free(vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * VectorInstDeleteProc --
 *
 *	Deletes the command associated with the vector.  This is
 *	called only when the command associated with the vector is
 *	destroyed.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
VectorInstDeleteProc(clientData)
    ClientData clientData;
{
    VectorObject *vPtr = clientData;

    vPtr->cmdToken = 0;
    Blt_VectorFree(vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorCreate --
 *
 *	Creates a vector structure and the following items:
 *
 *	o Tcl command
 *	o Tcl array variable and establishes traces on the variable
 *	o Adds a  new entry in the vector hash table
 *
 * Results:
 *	A pointer to the new vector structure.  If an error occurred
 *	NULL is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	A new Tcl command and array variable is added to the
 *	interpreter.
 *
 * ---------------------------------------------------------------------- */
VectorObject *
Blt_VectorCreate(dataPtr, vecName, cmdName, varName, newPtr)
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    CONST char *vecName;	/* Namespace-qualified name of the vector */
    CONST char *cmdName;	/* Name of the Tcl command mapped to
				 * the vector */
    CONST char *varName;	/* Name of the Tcl array mapped to the
				 * vector */
    int *newPtr;
{
    Tcl_DString dString;
    VectorObject *vPtr;
    int isNew;
    CONST char *name;
    char *qualName;
    Tcl_Namespace *nsPtr;
    Blt_HashEntry *hPtr;
    Tcl_Interp *interp = dataPtr->interp;

    isNew = 0;
    nsPtr = NULL;
    vPtr = NULL;

    if (Blt_ParseQualifiedName(interp, vecName, &nsPtr, &name) != TCL_OK) {
	Tcl_AppendResult(interp, "can't find namespace in \"", vecName, "\"",
	    (char *)NULL);
	return NULL;
    }
    if (nsPtr == NULL) {
	nsPtr = Tcl_GetCurrentNamespace(interp);
    }
    Tcl_DStringInit(&dString);
    if ((name[0] == '#') && (strcmp(name, "#auto") == 0)) {
	char string[200];

	do {	/* Generate a unique vector name. */
	    sprintf(string, "vector%d", dataPtr->nextId++);
	    qualName = Blt_GetQualifiedName(nsPtr, string, &dString);
	    hPtr = Blt_FindHashEntry(&(dataPtr->vectorTable), qualName);
	} while (hPtr != NULL);
    } else {
	register CONST char *p;

	for (p = name; *p != '\0'; p++) {
	    if (!VECTOR_CHAR(*p)) {
		Tcl_AppendResult(interp, "bad vector name \"", name,
		    "\": must contain digits, letters, underscore, or period",
		    (char *)NULL);
		goto error;
	    }
	}
	qualName = Blt_GetQualifiedName(nsPtr, name, &dString);
	vPtr = Blt_VectorParseElement((Tcl_Interp *)NULL, dataPtr, qualName, 
		(char **)NULL, NS_SEARCH_CURRENT);
    }
    if (vPtr == NULL) {
	hPtr = Blt_CreateHashEntry(&(dataPtr->vectorTable), qualName, &isNew);
	vPtr = Blt_VectorNew(dataPtr);
	vPtr->hashPtr = hPtr;
	vPtr->nsPtr = nsPtr;

	vPtr->name = Blt_GetHashKey(&(dataPtr->vectorTable), hPtr);
#ifdef NAMESPACE_DELETE_NOTIFY
	Blt_CreateNsDeleteNotify(interp, nsPtr, vPtr, VectorInstDeleteProc);
#endif /* NAMESPACE_DELETE_NOTIFY */
	Blt_SetHashValue(hPtr, vPtr);
    }
    if (cmdName != NULL) {
	Tcl_CmdInfo cmdInfo;

	if ((cmdName == vecName) ||
	    ((name[0] == '#') && (strcmp(name, "#auto") == 0))) {
	    cmdName = qualName;
	} 
	if (Tcl_GetCommandInfo(interp, (char *)cmdName, &cmdInfo)) {
#if TCL_MAJOR_VERSION > 7
	    if (vPtr != cmdInfo.objClientData) {
#else
	    if (vPtr != cmdInfo.clientData) {
#endif
		Tcl_AppendResult(interp, "command \"", cmdName,
			 "\" already exists", (char *)NULL);
		goto error;
	    }
	    /* We get here only if the old name is the same as the new. */
	    goto checkVariable;
	}
    }
    if (vPtr->cmdToken != 0) {
	DeleteCommand(vPtr);	/* Command already exists, delete old first */
    }
    if (cmdName != NULL) {
#if (TCL_MAJOR_VERSION == 7)
	vPtr->cmdToken = Blt_CreateCommand(interp, cmdName, Blt_VectorInstCmd, 
		vPtr, VectorInstDeleteProc);
#else
	Tcl_DString dString2;
	
	Tcl_DStringInit(&dString2);
	if (cmdName != qualName) {
	    if (Blt_ParseQualifiedName(interp, cmdName, &nsPtr, &name) 
		!= TCL_OK) {
		Tcl_AppendResult(interp, "can't find namespace in \"", cmdName,
				 "\"", (char *)NULL);
		goto error;
	    }
	    if (nsPtr == NULL) {
		nsPtr = Tcl_GetCurrentNamespace(interp);
	    }
	    cmdName = Blt_GetQualifiedName(nsPtr, name, &dString2);
	}
	vPtr->cmdToken = Tcl_CreateObjCommand(interp, (char *)cmdName, 
		Blt_VectorInstCmd, vPtr, VectorInstDeleteProc);
	Tcl_DStringFree(&dString2);
#endif
    }
  checkVariable:
    if (varName != NULL) {
	if ((varName[0] == '#') && (strcmp(varName, "#auto") == 0)) {
	    varName = qualName;
	}
	if (Blt_VectorMapVariable(interp, vPtr, varName) != TCL_OK) {
	    goto error;
	}
    }

    Tcl_DStringFree(&dString);
    *newPtr = isNew;
    return vPtr;

  error:
    Tcl_DStringFree(&dString);
    if (vPtr != NULL) {
	Blt_VectorFree(vPtr);
    }
    return NULL;
}


int
Blt_VectorDuplicate(destPtr, srcPtr)
    VectorObject *destPtr, *srcPtr;
{
    int nBytes;
    int length;

    if (destPtr == srcPtr) {
	/* Copying the same vector. */
    }
    length = srcPtr->last - srcPtr->first + 1;
    if (Blt_VectorChangeLength(destPtr, length) != TCL_OK) {
	return TCL_ERROR;
    }
    nBytes = length * sizeof(double);
    memcpy(destPtr->valueArr, srcPtr->valueArr + srcPtr->first, nBytes);
    destPtr->offset = srcPtr->offset;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * VectorNamesOp --
 *
 *	Reports the names of all the current vectors in the interpreter.
 *
 * Results:
 *	A standard Tcl result.  interp->result will contain a list of
 *	all the names of the vector instances.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
VectorNamesOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    VectorInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    char *name;
    Blt_HashSearch cursor;

    for (hPtr = Blt_FirstHashEntry(&(dataPtr->vectorTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	name = Blt_GetHashKey(&(dataPtr->vectorTable), hPtr);
	if ((argc == 2) || (Tcl_StringMatch(name, argv[2]))) {
	    Tcl_AppendElement(interp, name);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorCreateOp --
 *
 *	Creates a Tcl command, and array variable representing an
 *	instance of a vector.
 *
 *	vector a
 *	vector b(20)
 *	vector c(-5:14)
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
VectorCreate2(clientData, interp, argStart, argc, argv)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argStart;
    int argc;
    char **argv;
{
    VectorInterpData *dataPtr = clientData;
    VectorObject *vPtr;
    char *leftParen, *rightParen;
    int isNew, size, first, last;
    char *cmdName, *varName;
    int length;
    int inspectFlags, freeOnUnset, flush;
    char **nameArr;
    int count;
    register int i;

    /*
     * Handle switches to the vector command and collect the vector
     * name arguments into an array.
     */
    varName = cmdName = NULL;
    freeOnUnset = 0;
    nameArr = Blt_Malloc(sizeof(char *) * argc);
    assert(nameArr);

    inspectFlags = TRUE;
    flush = FALSE;
    count = 0;
    vPtr = NULL;
    for (i = argStart; i < argc; i++) {
	if ((inspectFlags) && (argv[i][0] == '-')) {
	    length = strlen(argv[i]);
	    if ((length > 1) &&
		(strncmp(argv[i], "-variable", length) == 0)) {
		if ((i + 1) == argc) {
		    Tcl_AppendResult(interp,
			"no variable name supplied with \"",
			argv[i], "\" switch", (char *)NULL);
		    goto error;
		}
		i++;
		varName = argv[i];
	    } else if ((length > 1) &&
		(strncmp(argv[i], "-command", length) == 0)) {
		if ((i + 1) == argc) {
		    Tcl_AppendResult(interp,
			"no command name supplied with \"",
			argv[i], "\" switch", (char *)NULL);
		    goto error;
		}
		i++;
		cmdName = argv[i];
	    } else if ((length > 1) &&
		(strncmp(argv[i], "-watchunset", length) == 0)) {
		int bool;

		if ((i + 1) == argc) {
		    Tcl_AppendResult(interp, "no value name supplied with \"",
			argv[i], "\" switch", (char *)NULL);
		    goto error;
		}
		i++;
		if (Tcl_GetBoolean(interp, argv[i], &bool) != TCL_OK) {
		    goto error;
		}
		freeOnUnset = bool;
	    } else if ((length > 1) && (strncmp(argv[i], "-flush", length) == 0)) {
		int bool;

		if ((i + 1) == argc) {
		    Tcl_AppendResult(interp, "no value name supplied with \"",
			argv[i], "\" switch", (char *)NULL);
		    goto error;
		}
		i++;
		if (Tcl_GetBoolean(interp, argv[i], &bool) != TCL_OK) {
		    goto error;
		}
		flush = bool;
	    } else if ((length > 1) && (argv[i][1] == '-') &&
		(argv[i][2] == '\0')) {
		inspectFlags = FALSE;	/* Allow vector names to start with - */
	    } else {
		Tcl_AppendResult(interp, "bad vector switch \"", argv[i], "\"",
		    (char *)NULL);
		goto error;
	    }
	} else {
	    nameArr[count++] = argv[i];
	}
    }
    if (count == 0) {
	Tcl_AppendResult(interp, "no vector names supplied", (char *)NULL);
	goto error;
    }
    if (count > 1) {
	if ((cmdName != NULL) && (cmdName[0] != '\0')) {
	    Tcl_AppendResult(interp,
		"can't specify more than one vector with \"-command\" switch",
		(char *)NULL);
	    goto error;
	}
	if ((varName != NULL) && (varName[0] != '\0')) {
	    Tcl_AppendResult(interp,
		"can't specify more than one vector with \"-variable\" switch",
		(char *)NULL);
	    goto error;
	}
    }
    for (i = 0; i < count; i++) {
	size = first = last = 0;
	leftParen = strchr(nameArr[i], '(');
	rightParen = strchr(nameArr[i], ')');
	if (((leftParen != NULL) && (rightParen == NULL)) ||
	    ((leftParen == NULL) && (rightParen != NULL)) ||
	    (leftParen > rightParen)) {
	    Tcl_AppendResult(interp, "bad vector specification \"", nameArr[i],
		"\"", (char *)NULL);
	    goto error;
	}
	if (leftParen != NULL) {
	    int result;
	    char *colon;

	    *rightParen = '\0';
	    colon = strchr(leftParen + 1, ':');
	    if (colon != NULL) {

		/* Specification is in the form vecName(first:last) */
		*colon = '\0';
		result = Tcl_GetInt(interp, leftParen + 1, &first);
		if ((*(colon + 1) != '\0') && (result == TCL_OK)) {
		    result = Tcl_GetInt(interp, colon + 1, &last);
		    if (first > last) {
			Tcl_AppendResult(interp, "bad vector range \"",
			    nameArr[i], "\"", (char *)NULL);
			result = TCL_ERROR;
		    }
		    size = (last - first) + 1;
		}
		*colon = ':';
	    } else {
		/* Specification is in the form vecName(size) */
		result = Tcl_GetInt(interp, leftParen + 1, &size);
	    }
	    *rightParen = ')';
	    if (result != TCL_OK) {
		goto error;
	    }
	    if (size < 0) {
		Tcl_AppendResult(interp, "bad vector size \"", nameArr[i], "\"",
		    (char *)NULL);
		goto error;
	    }
	}
	if (leftParen != NULL) {
	    *leftParen = '\0';
	}
	/*
	 * By default, we create a Tcl command by the name of the vector.
	 */
	vPtr = Blt_VectorCreate(dataPtr, nameArr[i],
	    (cmdName == NULL) ? nameArr[i] : cmdName,
	    (varName == NULL) ? nameArr[i] : varName,
	    &isNew);
	if (leftParen != NULL) {
	    *leftParen = '(';
	}
	if (vPtr == NULL) {
	    goto error;
	}
	vPtr->freeOnUnset = freeOnUnset;
	vPtr->flush = flush;
	vPtr->offset = first;
	if (size > 0) {
	    if (Blt_VectorChangeLength(vPtr, size) != TCL_OK) {
		goto error;
	    }
	}
	if (!isNew) {
	    if (vPtr->flush) {
		Blt_VectorFlushCache(vPtr);
	    }
	    Blt_VectorUpdateClients(vPtr);
	}
    }
    Blt_Free(nameArr);
    if (vPtr != NULL) {
	/* Return the name of the last vector created  */
	Tcl_SetResult(interp, vPtr->name, TCL_VOLATILE);
    }
    return TCL_OK;
  error:
    Blt_Free(nameArr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorCreateOp --
 *
 *	Creates a Tcl command, and array variable representing an
 *	instance of a vector.
 *
 *	vector a
 *	vector b(20)
 *	vector c(-5:14)
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
VectorCreateOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    return VectorCreate2(clientData, interp, 2, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * VectorDestroyOp --
 *
 *	Destroys the vector and its related Tcl command and array
 *	variable (if they exist).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes the vector.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
VectorDestroyOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    VectorInterpData *dataPtr = clientData;
    VectorObject *vPtr;
    register int i;

    for (i = 2; i < argc; i++) {
	if (Blt_VectorLookupName(dataPtr, argv[i], &vPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_VectorFree(vPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorExprOp --
 *
 *	Computes the result of the expression which may be
 *	either a scalar (single value) or vector (list of values).
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
VectorExprOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not Used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    return Blt_ExprVector(interp, argv[2], (Blt_Vector *) NULL);
}

static Blt_OpSpec vectorCmdOps[] =
{
    {"create", 1, (Blt_Op)VectorCreateOp, 3, 0,
	"vecName ?vecName...? ?switches...?",},
    {"destroy", 1, (Blt_Op)VectorDestroyOp, 3, 0,
	"vecName ?vecName...?",},
    {"expr", 1, (Blt_Op)VectorExprOp, 3, 3, "expression",},
    {"names", 1, (Blt_Op)VectorNamesOp, 2, 3, "?pattern?...",},
};

static int nCmdOps = sizeof(vectorCmdOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
VectorCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;

    /*
     * Try to replicate the old vector command's behavior:
     */
    if (argc > 1) {
	char c;
	register int i;
	register Blt_OpSpec *specPtr;

	c = argv[1][0];
	for (specPtr = vectorCmdOps, i = 0; i < nCmdOps; i++, specPtr++) {
	    if ((c == specPtr->name[0]) &&
		(strcmp(argv[1], specPtr->name) == 0)) {
		goto doOp;
	    }
	}
	/*
	 * The first argument is not an operation, so assume that its
	 * actually the name of a vector to be created
	 */
	return VectorCreate2(clientData, interp, 1, argc, argv);
    }
  doOp:
    /* Do the usual vector operation lookup now. */
    proc = Blt_GetOp(interp, nCmdOps, vectorCmdOps, BLT_OP_ARG1, argc, argv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, argc, argv);
}

/*
 * -----------------------------------------------------------------------
 *
 * VectorInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the "vector" command
 *	is deleted.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the math and index hash tables.  In addition removes
 *	the hash table managing all vector names.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
VectorInterpDeleteProc(clientData, interp)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
{
    VectorInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    VectorObject *vPtr;
    
    for (hPtr = Blt_FirstHashEntry(&(dataPtr->vectorTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	vPtr = (VectorObject *)Blt_GetHashValue(hPtr);
	vPtr->hashPtr = NULL;
	Blt_VectorFree(vPtr);
    }
    Blt_DeleteHashTable(&(dataPtr->vectorTable));

    /* If any user-defined math functions were installed, remove them.  */
    Blt_VectorUninstallMathFunctions(&(dataPtr->mathProcTable));
    Blt_DeleteHashTable(&(dataPtr->mathProcTable));

    Blt_DeleteHashTable(&(dataPtr->indexProcTable));
    Tcl_DeleteAssocData(interp, VECTOR_THREAD_KEY);
    Blt_Free(dataPtr);
}

VectorInterpData *
Blt_VectorGetInterpData(interp)
    Tcl_Interp *interp;
{
    VectorInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (VectorInterpData *)
	Tcl_GetAssocData(interp, VECTOR_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(VectorInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	dataPtr->nextId = 0;
	Tcl_SetAssocData(interp, VECTOR_THREAD_KEY, VectorInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&(dataPtr->vectorTable), BLT_STRING_KEYS);
	Blt_InitHashTable(&(dataPtr->mathProcTable), BLT_STRING_KEYS);
	Blt_InitHashTable(&(dataPtr->indexProcTable), BLT_STRING_KEYS);
	Blt_VectorInstallMathFunctions(&(dataPtr->mathProcTable));
	Blt_VectorInstallSpecialIndices(&(dataPtr->indexProcTable));
#ifdef HAVE_SRAND48
	srand48(time((time_t *) NULL));
#endif
    }
    return dataPtr;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_VectorInit --
 *
 *	This procedure is invoked to initialize the "vector" command.
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
Blt_VectorInit(interp)
    Tcl_Interp *interp;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    static Blt_CmdSpec cmdSpec = {"vector", VectorCmd, };
    
    dataPtr = Blt_VectorGetInterpData(interp);
    /* 
     * This routine may be run several times in the same interpreter. 
     * For example, if someone tries to initial the BLT commands from 
     * another namespace. Keep a reference count, so we know when it's 
     * safe to clean up.
     */
    cmdSpec.clientData = dataPtr;
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}



/* C Application interface to vectors */

/*
 * -----------------------------------------------------------------------
 *
 * Blt_CreateVector --
 *
 *	Creates a new vector by the name and size.
 *
 * Results:
 *	A standard Tcl result.  If the new array size is invalid or a
 *	vector already exists by that name, TCL_ERROR is returned.
 *	Otherwise TCL_OK is returned and the new vector is created.
 *
 * Side Effects:
 *	Memory will be allocated for the new vector.  A new Tcl command
 *	and Tcl array variable will be created.
 *
 * -----------------------------------------------------------------------
 */

/*LINTLIBRARY*/
int
Blt_CreateVector2(interp, vecName, cmdName, varName, initialSize, vecPtrPtr)
    Tcl_Interp *interp;
    char *vecName;
    char *cmdName, *varName;
    int initialSize;
    Blt_Vector **vecPtrPtr;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    VectorObject *vPtr;
    int isNew;
    char *nameCopy;

    if (initialSize < 0) {
	Tcl_AppendResult(interp, "bad vector size \"", Blt_Itoa(initialSize),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    dataPtr = Blt_VectorGetInterpData(interp);

    nameCopy = Blt_Strdup(vecName);
    vPtr = Blt_VectorCreate(dataPtr, nameCopy, cmdName, varName, &isNew);
    Blt_Free(nameCopy);

    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    if (initialSize > 0) {
	if (Blt_VectorChangeLength(vPtr, initialSize) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (vecPtrPtr != NULL) {
	*vecPtrPtr = (Blt_Vector *) vPtr;
    }
    return TCL_OK;
}

int
Blt_CreateVector(interp, name, size, vecPtrPtr)
    Tcl_Interp *interp;
    char *name;
    int size;
    Blt_Vector **vecPtrPtr;
{
    return Blt_CreateVector2(interp, name, name, name, size, vecPtrPtr);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_DeleteVector --
 *
 *	Deletes the vector of the given name.  All clients with
 *	designated callback routines will be notified.
 *
 * Results:
 *	A standard Tcl result.  If no vector exists by that name,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and
 *	vector is deleted.
 *
 * Side Effects:
 *	Memory will be released for the new vector.  Both the Tcl
 *	command and array variable will be deleted.  All clients which
 *	set call back procedures will be notified.
 *
 * -----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
int
Blt_DeleteVector(vecPtr)
    Blt_Vector *vecPtr;
{
    VectorObject *vPtr = (VectorObject *)vecPtr;

    Blt_VectorFree(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_DeleteVectorByName --
 *
 *	Deletes the vector of the given name.  All clients with
 *	designated callback routines will be notified.
 *
 * Results:
 *	A standard Tcl result.  If no vector exists by that name,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and
 *	vector is deleted.
 *
 * Side Effects:
 *	Memory will be released for the new vector.  Both the Tcl
 *	command and array variable will be deleted.  All clients which
 *	set call back procedures will be notified.
 *
 * -----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
int
Blt_DeleteVectorByName(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    VectorObject *vPtr;
    char *nameCopy;
    int result;

    /*
     * If the vector name was passed via a read-only string (e.g. "x"),
     * the Blt_VectorParseElement routine will segfault when it tries to write
     * into the string.  Therefore make a writable copy and free it
     * when we're done.
     */
    nameCopy = Blt_Strdup(name);
    dataPtr = Blt_VectorGetInterpData(interp);
    result = Blt_VectorLookupName(dataPtr, nameCopy, &vPtr);
    Blt_Free(nameCopy);

    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_VectorFree(vPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorExists2 --
 *
 *	Returns whether the vector associated with the client token
 *	still exists.
 *
 * Results:
 *	Returns 1 is the vector still exists, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorExists2(interp, vecName)
    Tcl_Interp *interp;
    char *vecName;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */

    dataPtr = Blt_VectorGetInterpData(interp);
    if (GetVectorObject(dataPtr, vecName, NS_SEARCH_BOTH) != NULL) {
	return TRUE;
    }
    return FALSE;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorExists --
 *
 *	Returns whether the vector associated with the client token
 *	still exists.
 *
 * Results:
 *	Returns 1 is the vector still exists, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorExists(interp, vecName)
    Tcl_Interp *interp;
    char *vecName;
{
    char *nameCopy;
    int result;

    /*
     * If the vector name was passed via a read-only string (e.g. "x"),
     * the Blt_VectorParseName routine will segfault when it tries to write
     * into the string.  Therefore make a writable copy and free it
     * when we're done.
     */
    nameCopy = Blt_Strdup(vecName);
    result = Blt_VectorExists2(interp, nameCopy);
    Blt_Free(nameCopy);
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_GetVector --
 *
 *	Returns a pointer to the vector associated with the given name.
 *
 * Results:
 *	A standard Tcl result.  If there is no vector "name", TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned and vecPtrPtr will
 *	point to the vector.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_GetVector(interp, name, vecPtrPtr)
    Tcl_Interp *interp;
    char *name;
    Blt_Vector **vecPtrPtr;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    VectorObject *vPtr;
    char *nameCopy;
    int result;

    dataPtr = Blt_VectorGetInterpData(interp);
    /*
     * If the vector name was passed via a read-only string (e.g. "x"),
     * the Blt_VectorParseName routine will segfault when it tries to write
     * into the string.  Therefore make a writable copy and free it
     * when we're done.
     */
    nameCopy = Blt_Strdup(name);
    result = Blt_VectorLookupName(dataPtr, nameCopy, &vPtr);
    Blt_Free(nameCopy);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_VectorUpdateRange(vPtr);
    *vecPtrPtr = (Blt_Vector *) vPtr;
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ResetVector --
 *
 *	Resets the vector data.  This is called by a client to
 *	indicate that the vector data has changed.  The vector does
 *	not need to point to different memory.  Any clients of the
 *	vector will be notified of the change.
 *
 * Results:
 *	A standard Tcl result.  If the new array size is invalid,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	new vector data is recorded.
 *
 * Side Effects:
 *	Any client designated callbacks will be posted.  Memory may
 *	be changed for the vector array.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_ResetVector(vecPtr, valueArr, length, size, freeProc)
    Blt_Vector *vecPtr;
    double *valueArr;		/* Array containing the elements of the
				 * vector. If NULL, indicates to reset the 
				 * vector.*/
    int length;			/* The number of elements that the vector 
				 * currently holds. */
    int size;			/* The maximum number of elements that the
				 * array can hold. */
    Tcl_FreeProc *freeProc;	/* Address of memory deallocation routine
				 * for the array of values.  Can also be
				 * TCL_STATIC, TCL_DYNAMIC, or TCL_VOLATILE. */
{
    VectorObject *vPtr = (VectorObject *)vecPtr;

    if (size < 0) {
	Tcl_AppendResult(vPtr->interp, "bad array size", (char *)NULL);
	return TCL_ERROR;
    }
    return Blt_VectorReset(vPtr, valueArr, length, size, freeProc);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ResizeVector --
 *
 *	Changes the size of the vector.  All clients with designated
 *	callback routines will be notified of the size change.
 *
 * Results:
 *	A standard Tcl result.  If no vector exists by that name,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and
 *	vector is resized.
 *
 * Side Effects:
 *	Memory may be reallocated for the new vector size.  All clients
 *	which set call back procedures will be notified.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_ResizeVector(vecPtr, length)
    Blt_Vector *vecPtr;
    int length;
{
    VectorObject *vPtr = (VectorObject *)vecPtr;

    if (Blt_VectorChangeLength(vPtr, length) != TCL_OK) {
	Tcl_AppendResult(vPtr->interp, "can't resize vector \"", vPtr->name,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_AllocVectorId --
 *
 *	Creates an identifier token for an existing vector.  The
 *	identifier is used by the client routines to get call backs
 *	when (and if) the vector changes.
 *
 * Results:
 *	A standard Tcl result.  If "vecName" is not associated with
 *	a vector, TCL_ERROR is returned and interp->result is filled
 *	with an error message.
 *
 *--------------------------------------------------------------
 */
Blt_VectorId
Blt_AllocVectorId(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    VectorObject *vPtr;
    VectorClient *clientPtr;
    Blt_VectorId clientId;
    int result;
    char *nameCopy;

    dataPtr = Blt_VectorGetInterpData(interp);
    /*
     * If the vector name was passed via a read-only string (e.g. "x"),
     * the Blt_VectorParseName routine will segfault when it tries to write
     * into the string.  Therefore make a writable copy and free it
     * when we're done.
     */
    nameCopy = Blt_Strdup(name);
    result = Blt_VectorLookupName(dataPtr, nameCopy, &vPtr);
    Blt_Free(nameCopy);

    if (result != TCL_OK) {
	return (Blt_VectorId) 0;
    }
    /* Allocate a new client structure */
    clientPtr = Blt_Calloc(1, sizeof(VectorClient));
    assert(clientPtr);
    clientPtr->magic = VECTOR_MAGIC;

    /* Add the new client to the server's list of clients */
    clientPtr->linkPtr = Blt_ChainAppend(vPtr->chainPtr, clientPtr);
    clientPtr->serverPtr = vPtr;
    clientId = (Blt_VectorId) clientPtr;
    return clientId;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_SetVectorChangedProc --
 *
 *	Sets the routine to be called back when the vector is changed
 *	or deleted.  *clientData* will be provided as an argument. If
 *	*proc* is NULL, no callback will be made.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The designated routine will be called when the vector is changed
 *	or deleted.
 *
 * -----------------------------------------------------------------------
 */
void
Blt_SetVectorChangedProc(clientId, proc, clientData)
    Blt_VectorId clientId;	/* Client token identifying the vector */
    Blt_VectorChangedProc *proc;/* Address of routine to call when the contents
				 * of the vector change. If NULL, no routine
				 * will be called */
    ClientData clientData;	/* One word of information to pass along when
				 * the above routine is called */
{
    VectorClient *clientPtr = (VectorClient *)clientId;

    if (clientPtr->magic != VECTOR_MAGIC) {
	return;			/* Not a valid token */
    }
    clientPtr->clientData = clientData;
    clientPtr->proc = proc;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_FreeVectorId --
 *
 *	Releases the token for an existing vector.  This indicates
 *	that the client is no longer interested the vector.  Any
 *	previously specified callback routine will no longer be
 *	invoked when (and if) the vector changes.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Any previously specified callback routine will no longer be
 *	invoked when (and if) the vector changes.
 *
 *--------------------------------------------------------------
 */
void
Blt_FreeVectorId(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    VectorClient *clientPtr = (VectorClient *)clientId;

    if (clientPtr->magic != VECTOR_MAGIC) {
	return;			/* Not a valid token */
    }
    if (clientPtr->serverPtr != NULL) {
	/* Remove the client from the server's list */
	Blt_ChainDeleteLink(clientPtr->serverPtr->chainPtr, clientPtr->linkPtr);
    }
    Blt_Free(clientPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Blt_NameOfVectorId --
 *
 *	Returns the name of the vector (and array variable).
 *
 * Results:
 *	The name of the array variable is returned.
 *
 *--------------------------------------------------------------
 */
char *
Blt_NameOfVectorId(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    VectorClient *clientPtr = (VectorClient *)clientId;

    if ((clientPtr->magic != VECTOR_MAGIC) || (clientPtr->serverPtr == NULL)) {
	return NULL;
    }
    return clientPtr->serverPtr->name;
}

char *
Blt_NameOfVector(vecPtr)
    Blt_Vector *vecPtr;		/* Vector to query. */
{
    VectorObject *vPtr = (VectorObject *)vecPtr;
    return vPtr->name;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_VectorNotifyPending --
 *
 *	Returns the name of the vector (and array variable).
 *
 * Results:
 *	The name of the array variable is returned.
 *
 *--------------------------------------------------------------
 */
int
Blt_VectorNotifyPending(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    VectorClient *clientPtr = (VectorClient *)clientId;

    if ((clientPtr == NULL) || (clientPtr->magic != VECTOR_MAGIC) || 
	(clientPtr->serverPtr == NULL)) {
	return 0;
    }
    return (clientPtr->serverPtr->notifyFlags & NOTIFY_PENDING);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_GetVectorById --
 *
 *	Returns a pointer to the vector associated with the client
 *	token.
 *
 * Results:
 *	A standard Tcl result.  If the client token is not associated
 *	with a vector any longer, TCL_ERROR is returned. Otherwise,
 *	TCL_OK is returned and vecPtrPtr will point to vector.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_GetVectorById(interp, clientId, vecPtrPtr)
    Tcl_Interp *interp;
    Blt_VectorId clientId;	/* Client token identifying the vector */
    Blt_Vector **vecPtrPtr;
{
    VectorClient *clientPtr = (VectorClient *)clientId;

    if (clientPtr->magic != VECTOR_MAGIC) {
	Tcl_AppendResult(interp, "bad vector token", (char *)NULL);
	return TCL_ERROR;
    }
    if (clientPtr->serverPtr == NULL) {
	Tcl_AppendResult(interp, "vector no longer exists", (char *)NULL);
	return TCL_ERROR;
    }
    Blt_VectorUpdateRange(clientPtr->serverPtr);
    *vecPtrPtr = (Blt_Vector *) clientPtr->serverPtr;
    return TCL_OK;
}

/*LINTLIBRARY*/
void
Blt_InstallIndexProc(interp, string, procPtr)
    Tcl_Interp *interp;
    char *string;
    Blt_VectorIndexProc *procPtr; /* Pointer to function to be called
				   * when the vector finds the named index.
				   * If NULL, this indicates to remove
				   * the index from the table.
				   */
{
    VectorInterpData *dataPtr;	/* Interpreter-specific data. */
    Blt_HashEntry *hPtr;
    int isNew;

    dataPtr = Blt_VectorGetInterpData(interp);
    hPtr = Blt_CreateHashEntry(&(dataPtr->indexProcTable), string, &isNew);
    if (procPtr == NULL) {
	Blt_DeleteHashEntry(&(dataPtr->indexProcTable), hPtr);
    } else {
	Blt_SetHashValue(hPtr, procPtr);
    }
}
