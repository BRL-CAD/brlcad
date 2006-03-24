
/*
 * bltVecCmd.c --
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
#include <ctype.h>

#if (TCL_MAJOR_VERSION > 7) 

static 
int GetDouble(interp, objPtr, valuePtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    double *valuePtr;
{
    /* First try to extract the value as a double precision number. */
    if (Tcl_GetDoubleFromObj(interp, objPtr, valuePtr) == TCL_OK) {
	return TCL_OK;
    }
    Tcl_ResetResult(interp);

    /* Then try to parse it as an expression. */
    if (Tcl_ExprDouble(interp, Tcl_GetString(objPtr), valuePtr) == TCL_OK) {
	return TCL_OK;
    }
    return TCL_ERROR;
}

static Tcl_Obj *
GetValues(VectorObject *vPtr, int first, int last)
{ 
    register int i; 
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (i = first; i <= last; i++) { 
	Tcl_ListObjAppendElement(vPtr->interp, listObjPtr, 
				 Tcl_NewDoubleObj(vPtr->valueArr[i]));
    } 
    return listObjPtr;
}

static void
ReplicateValue(vPtr, first, last, value) 
    VectorObject *vPtr;
    int first, last;
    double value;
{ 
    register int i;
 
    for (i = first; i <= last; i++) { 
	vPtr->valueArr[i] = value; 
    } 
    vPtr->notifyFlags |= UPDATE_RANGE; 
}

static int
CopyList(vPtr, objc, objv)
    VectorObject *vPtr;
    int objc;
    Tcl_Obj *CONST *objv;
{
    register int i;
    double value;

    if (Blt_VectorChangeLength(vPtr, objc) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
	if (GetDouble(vPtr->interp, objv[i], &value) != TCL_OK) {
	    Blt_VectorChangeLength(vPtr, i);
	    return TCL_ERROR;
	}
	vPtr->valueArr[i] = value;
    }
    return TCL_OK;
}

static int
AppendVector(destPtr, srcPtr)
    VectorObject *destPtr, *srcPtr;
{
    int nBytes;
    int oldSize, newSize;

    oldSize = destPtr->length;
    newSize = oldSize + srcPtr->last - srcPtr->first + 1;
    if (Blt_VectorChangeLength(destPtr, newSize) != TCL_OK) {
	return TCL_ERROR;
    }
    nBytes = (newSize - oldSize) * sizeof(double);
    memcpy((char *)(destPtr->valueArr + oldSize),
	(srcPtr->valueArr + srcPtr->first), nBytes);
    destPtr->notifyFlags |= UPDATE_RANGE;
    return TCL_OK;
}

static int
AppendList(vPtr, objc, objv)
    VectorObject *vPtr;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int count;
    register int i;
    double value;
    int oldSize;

    oldSize = vPtr->length;
    if (Blt_VectorChangeLength(vPtr, vPtr->length + objc) != TCL_OK) {
	return TCL_ERROR;
    }
    count = oldSize;
    for (i = 0; i < objc; i++) {
	if (GetDouble(vPtr->interp, objv[i], &value) != TCL_OK) {
	    Blt_VectorChangeLength(vPtr, count);
	    return TCL_ERROR;
	}
	vPtr->valueArr[count++] = value;
    }
    vPtr->notifyFlags |= UPDATE_RANGE;
    return TCL_OK;
}

/* Vector instance option commands */

/*
 * -----------------------------------------------------------------------
 *
 * AppendOp --
 *
 *	Appends one of more Tcl lists of values, or vector objects
 *	onto the end of the current vector object.
 *
 * Results:
 *	A standard Tcl result.  If a current vector can't be created,
 *      resized, any of the named vectors can't be found, or one of
 *	lists of values is invalid, TCL_ERROR is returned.
 *
 * Side Effects:
 *	Clients of current vector will be notified of the change.
 *
 * -----------------------------------------------------------------------
 */
static int
AppendOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    register int i;
    int result;
    VectorObject *v2Ptr;

    for (i = 2; i < objc; i++) {
	v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL, vPtr->dataPtr, 
	       Tcl_GetString(objv[i]), (char **)NULL, NS_SEARCH_BOTH);
	if (v2Ptr != NULL) {
	    result = AppendVector(vPtr, v2Ptr);
	} else {
	    int nElem;
	    Tcl_Obj **elemObjArr;

	    if (Tcl_ListObjGetElements(interp, objv[i], &nElem, &elemObjArr) 
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    result = AppendList(vPtr, nElem, elemObjArr);
	}
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (objc > 2) {
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
	Blt_VectorUpdateClients(vPtr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * ClearOp --
 *
 *	Deletes all the accumulated array indices for the Tcl array
 *	associated will the vector.  This routine can be used to
 *	free excess memory from a large vector.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Memory used for the entries of the Tcl array variable is freed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ClearOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;	/* Not used. */
{
    Blt_VectorFlushCache(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes the given indices from the vector.  If no indices are
 *	provided the entire vector is deleted.
 *
 * Results:
 *	A standard Tcl result.  If any of the given indices is invalid,
 *	interp->result will an error message and TCL_ERROR is returned.
 *
 * Side Effects:
 *	The clients of the vector will be notified of the vector
 *	deletions.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    unsigned char *unsetArr;
    register int i, j;
    register int count;
    char *string;

    /* FIXME: Don't delete vector with no indices.  */
    if (objc == 2) {
	Blt_VectorFree(vPtr);
	return TCL_OK;
    }
    /*
     * Allocate an "unset" bitmap the size of the vector.  
     */
    unsetArr = Blt_Calloc(sizeof(unsigned char), (vPtr->length + 7) / 8);
    assert(unsetArr);

#define SetBit(i) \
    unsetArr[(i) >> 3] |= (1 << ((i) & 0x07))
#define GetBit(i) \
    (unsetArr[(i) >> 3] & (1 << ((i) & 0x07)))

    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (Blt_VectorGetIndexRange(interp, vPtr, string, 
		(INDEX_COLON | INDEX_CHECK), (Blt_VectorIndexProc **) NULL) 
		!= TCL_OK) {
	    Blt_Free(unsetArr);
	    return TCL_ERROR;
	}
	for (j = vPtr->first; j <= vPtr->last; j++) {
	    SetBit(j);		/* Mark the range of elements for deletion. */
	}
    }
    count = 0;
    for (i = 0; i < vPtr->length; i++) {
	if (GetBit(i)) {
	    continue;		/* Skip elements marked for deletion. */
	}
	if (count < i) {
	    vPtr->valueArr[count] = vPtr->valueArr[i];
	}
	count++;
    }
    Blt_Free(unsetArr);
    vPtr->length = count;
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * DupOp --
 *
 *	Creates one or more duplicates of the vector object.
 *
 * Results:
 *	A standard Tcl result.  If a new vector can't be created,
 *      or and existing vector resized, TCL_ERROR is returned.
 *
 * Side Effects:
 *	Clients of existing vectors will be notified of the change.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DupOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;		/* Not used. */
    int objc;
    Tcl_Obj *CONST *objv;
{
    VectorObject *v2Ptr;
    int isNew;
    register int i;
    char *string;

    for (i = 2; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	v2Ptr = Blt_VectorCreate(vPtr->dataPtr, string, string, string,&isNew);
	if (v2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (v2Ptr == vPtr) {
	    continue;
	}
	if (Blt_VectorDuplicate(v2Ptr, vPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!isNew) {
	    if (v2Ptr->flush) {
		Blt_VectorFlushCache(v2Ptr);
	    }
	    Blt_VectorUpdateClients(v2Ptr);
	}
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *	Sets or reads the value of the index.  This simulates what the
 *	vector's variable does.
 *
 * Results:
 *	A standard Tcl result.  If the index is invalid,
 *	interp->result will an error message and TCL_ERROR is returned.
 *	Otherwise interp->result will contain the values.
 *
 * -----------------------------------------------------------------------
 */
static int
IndexOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int first, last;
    char *string;

    string = Tcl_GetString(objv[2]);
    if (Blt_VectorGetIndexRange(interp, vPtr, string, INDEX_ALL_FLAGS, 
		(Blt_VectorIndexProc **) NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    first = vPtr->first, last = vPtr->last;
    if (objc == 3) {
	Tcl_Obj *listObjPtr;

	if (first == vPtr->length) {
	    Tcl_AppendResult(interp, "can't get index \"", string, "\"",
		(char *)NULL);
	    return TCL_ERROR;	/* Can't read from index "++end" */
	}
	listObjPtr = GetValues(vPtr, first, last);
	Tcl_SetObjResult(interp, listObjPtr);
    } else {
	double value;

	/* FIXME: huh? Why set values here?.  */
	if (first == SPECIAL_INDEX) {
	    Tcl_AppendResult(interp, "can't set index \"", string, "\"",
		(char *)NULL);
	    return TCL_ERROR;	/* Tried to set "min" or "max" */
	}
	if (GetDouble(vPtr->interp, objv[3], &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (first == vPtr->length) {
	    if (Blt_VectorChangeLength(vPtr, vPtr->length + 1) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	ReplicateValue(vPtr, first, last, value);
	Tcl_SetObjResult(interp, objv[3]);
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
	Blt_VectorUpdateClients(vPtr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * LengthOp --
 *
 *	Returns the length of the vector.  If a new size is given, the
 *	vector is resized to the new vector.
 *
 * Results:
 *	A standard Tcl result.  If the new length is invalid,
 *	interp->result will an error message and TCL_ERROR is returned.
 *	Otherwise interp->result will contain the length of the vector.
 *
 * -----------------------------------------------------------------------
 */
static int
LengthOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (objc == 3) {
	int size;

	if (Tcl_GetIntFromObj(interp, objv[2], &size) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (size < 0) {
	    Tcl_AppendResult(interp, "bad vector size \"", 
		Tcl_GetString(objv[2]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (Blt_VectorChangeLength(vPtr, size) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
	Blt_VectorUpdateClients(vPtr);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(vPtr->length));
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * MapOp --
 *
 *	Queries or sets the offset of the array index from the base
 *	address of the data array of values.
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MapOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    if (objc > 2) {
	if (Blt_VectorMapVariable(interp, vPtr, Tcl_GetString(objv[2])) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (vPtr->arrayName != NULL) {
	Tcl_SetResult(interp, vPtr->arrayName, TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * MergeOp --
 *
 *	Merges the values from the given vectors to the current vector.
 *
 * Results:
 *	A standard Tcl result.  If any of the given vectors differ in size,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	vector data will contain merged values of the given vectors.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MergeOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    VectorObject *v2Ptr;
    VectorObject **vecArr;
    register VectorObject **vPtrPtr;
    int refSize, length, nElem;
    register int i;
    double *valuePtr, *valueArr;
    
    /* Allocate an array of vector pointers of each vector to be
     * merged in the current vector.  */
    vecArr = Blt_Malloc(sizeof(VectorObject *) * objc);
    assert(vecArr);
    vPtrPtr = vecArr;

    refSize = -1;
    nElem = 0;
    for (i = 2; i < objc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, Tcl_GetString(objv[i]), &v2Ptr)
		!= TCL_OK) {
	    Blt_Free(vecArr);
	    return TCL_ERROR;
	}
	/* Check that all the vectors are the same length */
	length = v2Ptr->last - v2Ptr->first + 1;
	if (refSize < 0) {
	    refSize = length;
	} else if (length != refSize) {
	    Tcl_AppendResult(vPtr->interp, "vectors \"", vPtr->name,
		"\" and \"", v2Ptr->name, "\" differ in length",
		(char *)NULL);
	    Blt_Free(vecArr);
	    return TCL_ERROR;
	}
	*vPtrPtr++ = v2Ptr;
	nElem += refSize;
    }
    *vPtrPtr = NULL;

    valueArr = Blt_Malloc(sizeof(double) * nElem);
    if (valueArr == NULL) {
	Tcl_AppendResult(vPtr->interp, "not enough memory to allocate ", 
		 Blt_Itoa(nElem), " vector elements", (char *)NULL);
	return TCL_ERROR;
    }
    /* Merge the values from each of the vectors into the current vector */
    valuePtr = valueArr;
    for (i = 0; i < refSize; i++) {
	for (vPtrPtr = vecArr; *vPtrPtr != NULL; vPtrPtr++) {
	    *valuePtr++ = (*vPtrPtr)->valueArr[i + (*vPtrPtr)->first];
	}
    }
    Blt_Free(vecArr);
    Blt_VectorReset(vPtr, valueArr, nElem, nElem, TCL_DYNAMIC);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * NormalizeOp --
 *
 *	Normalizes the vector.
 *
 * Results:
 *	A standard Tcl result.  If the density is invalid, TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NormalizeOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    register int i;
    double range;

    Blt_VectorUpdateRange(vPtr);
    range = vPtr->max - vPtr->min;
    if (objc > 2) {
	VectorObject *v2Ptr;
	int isNew;
	char *string;

	string = Tcl_GetString(objv[2]);
	v2Ptr = Blt_VectorCreate(vPtr->dataPtr, string, string, string, 
				 &isNew);
	if (v2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (Blt_VectorChangeLength(v2Ptr, vPtr->length) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (i = 0; i < vPtr->length; i++) {
	    v2Ptr->valueArr[i] = (vPtr->valueArr[i] - vPtr->min) / range;
	}
	Blt_VectorUpdateRange(v2Ptr);
	if (!isNew) {
	    if (v2Ptr->flush) {
		Blt_VectorFlushCache(v2Ptr);
	    }
	    Blt_VectorUpdateClients(v2Ptr);
	}
    } else {
	double norm;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	for (i = 0; i < vPtr->length; i++) {
	    norm = (vPtr->valueArr[i] - vPtr->min) / range;
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewDoubleObj(norm));
	}
	Tcl_SetObjResult(interp, listObjPtr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * NotifyOp --
 *
 *	Notify clients of vector.
 *
 * Results:
 *	A standard Tcl result.  If any of the given vectors differ in size,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	vector data will contain merged values of the given vectors.
 *
 *  x vector notify now
 *  x vector notify always
 *  x vector notify whenidle
 *  x vector notify update {}
 *  x vector notify delete {}
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NotifyOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int option;
    int bool;
    enum optionIndices {
	OPTION_ALWAYS, OPTION_NEVER, OPTION_WHENIDLE, 
	OPTION_NOW, OPTION_CANCEL, OPTION_PENDING
    };
    static char *optionArr[] = {
	"always", "never", "whenidle", "now", "cancel", "pending", NULL
    };

    if (Tcl_GetIndexFromObj(interp, objv[2], optionArr, "qualifier", TCL_EXACT,
	    &option) != TCL_OK) {
	return TCL_OK;
    }
    switch (option) {
    case OPTION_ALWAYS:
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_ALWAYS;
	break;
    case OPTION_NEVER:
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_NEVER;
	break;
    case OPTION_WHENIDLE:
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_WHENIDLE;
	break;
    case OPTION_NOW:
	/* FIXME: How does this play when an update is pending? */
	Blt_VectorNotifyClients(vPtr);
	break;
    case OPTION_CANCEL:
	if (vPtr->notifyFlags & NOTIFY_PENDING) {
	    vPtr->notifyFlags &= ~NOTIFY_PENDING;
	    Tcl_CancelIdleCall(Blt_VectorNotifyClients, (ClientData)vPtr);
	}
	break;
    case OPTION_PENDING:
	bool = (vPtr->notifyFlags & NOTIFY_PENDING);
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool));
	break;
    }	
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * PopulateOp --
 *
 *	Creates or resizes a new vector based upon the density specified.
 *
 * Results:
 *	A standard Tcl result.  If the density is invalid, TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PopulateOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    VectorObject *v2Ptr;
    int size, density;
    int isNew;
    register int i, j;
    double slice, range;
    register double *valuePtr;
    int count;
    char *string;

    string = Tcl_GetString(objv[2]);
    v2Ptr = Blt_VectorCreate(vPtr->dataPtr, string, string, string, &isNew);
    if (v2Ptr == NULL) {
	return TCL_ERROR;
    }
    if (vPtr->length == 0) {
	return TCL_OK;		/* Source vector is empty. */
    }
    if (Tcl_GetIntFromObj(interp, objv[3], &density) != TCL_OK) {
	return TCL_ERROR;
    }
    if (density < 1) {
	Tcl_AppendResult(interp, "bad density \"", Tcl_GetString(objv[3]), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    size = (vPtr->length - 1) * (density + 1) + 1;
    if (Blt_VectorChangeLength(v2Ptr, size) != TCL_OK) {
	return TCL_ERROR;
    }
    count = 0;
    valuePtr = v2Ptr->valueArr;
    for (i = 0; i < (vPtr->length - 1); i++) {
	range = vPtr->valueArr[i + 1] - vPtr->valueArr[i];
	slice = range / (double)(density + 1);
	for (j = 0; j <= density; j++) {
	    *valuePtr = vPtr->valueArr[i] + (slice * (double)j);
	    valuePtr++;
	    count++;
	}
    }
    count++;
    *valuePtr = vPtr->valueArr[i];
    assert(count == v2Ptr->length);
    if (!isNew) {
	if (v2Ptr->flush) {
	    Blt_VectorFlushCache(v2Ptr);
	}
	Blt_VectorUpdateClients(v2Ptr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * RangeOp --
 *
 *	Returns a Tcl list of the range of vector values specified.
 *
 * Results:
 *	A standard Tcl result.  If the given range is invalid, TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned and interp->result
 *	will contain the list of values.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RangeOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    Tcl_Obj *listObjPtr;
    int first, last;
    register int i;

    if ((Blt_VectorGetIndex(interp, vPtr, Tcl_GetString(objv[2]), &first, 
		INDEX_CHECK, (Blt_VectorIndexProc **) NULL) != TCL_OK) ||
	(Blt_VectorGetIndex(interp, vPtr, Tcl_GetString(objv[3]), &last, 
		INDEX_CHECK, (Blt_VectorIndexProc **) NULL) != TCL_OK)) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (first > last) {
	/* Return the list reversed */
	for (i = last; i <= first; i++) {
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewDoubleObj(vPtr->valueArr[i]));
	}
    } else {
	for (i = first; i <= last; i++) {
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewDoubleObj(vPtr->valueArr[i]));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InRange --
 *
 *	Determines if a value lies within a given range.
 *
 *	The value is normalized and compared against the interval
 *	[0..1], where 0.0 is the minimum and 1.0 is the maximum.
 *	DBL_EPSILON is the smallest number that can be represented
 *	on the host machine, such that (1.0 + epsilon) != 1.0.
 *
 *	Please note, min cannot be greater than max.
 *
 * Results:
 *	If the value is within of the interval [min..max], 1 is 
 *	returned; 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
INLINE static int
InRange(value, min, max)
    double value, min, max;
{
    double range;

    range = max - min;
    if (range < DBL_EPSILON) {
	return (FABS(max - value) < DBL_EPSILON);
    } else {
	double norm;

	norm = (value - min) / range;
	return ((norm >= -DBL_EPSILON) && ((norm - 1.0) < DBL_EPSILON));
    }
}

enum NativeFormats {
    FMT_UNKNOWN = -1,
    FMT_UCHAR, FMT_CHAR,
    FMT_USHORT, FMT_SHORT,
    FMT_UINT, FMT_INT,
    FMT_ULONG, FMT_LONG,
    FMT_FLOAT, FMT_DOUBLE
};

/*
 * -----------------------------------------------------------------------
 *
 * GetBinaryFormat
 *
 *      Translates a format string into a native type.  Formats may be
 *	as follows.
 *
 *		signed		i1, i2, i4, i8
 *		unsigned 	u1, u2, u4, u8
 *		real		r4, r8, r16
 *
 *	But there must be a corresponding native type.  For example,
 *	this for reading 2-byte binary integers from an instrument and
 *	converting them to unsigned shorts or ints.
 *
 * -----------------------------------------------------------------------
 */
static enum NativeFormats
GetBinaryFormat(interp, string, sizePtr)
    Tcl_Interp *interp;
    char *string;
    int *sizePtr;
{
    char c;

    c = tolower(string[0]);
    if (Tcl_GetInt(interp, string + 1, sizePtr) != TCL_OK) {
	Tcl_AppendResult(interp, "unknown binary format \"", string,
	    "\": incorrect byte size", (char *)NULL);
	return FMT_UNKNOWN;
    }
    switch (c) {
    case 'r':
	if (*sizePtr == sizeof(double)) {
	    return FMT_DOUBLE;
	} else if (*sizePtr == sizeof(float)) {
	    return FMT_FLOAT;
	}
	break;

    case 'i':
	if (*sizePtr == sizeof(char)) {
	    return FMT_CHAR;
	} else if (*sizePtr == sizeof(int)) {
	    return FMT_INT;
	} else if (*sizePtr == sizeof(long)) {
	    return FMT_LONG;
	} else if (*sizePtr == sizeof(short)) {
	    return FMT_SHORT;
	}
	break;

    case 'u':
	if (*sizePtr == sizeof(unsigned char)) {
	    return FMT_UCHAR;
	} else if (*sizePtr == sizeof(unsigned int)) {
	    return FMT_UINT;
	} else if (*sizePtr == sizeof(unsigned long)) {
	    return FMT_ULONG;
	} else if (*sizePtr == sizeof(unsigned short)) {
	    return FMT_USHORT;
	}
	break;

    default:
	Tcl_AppendResult(interp, "unknown binary format \"", string,
	    "\": should be either i#, r#, u# (where # is size in bytes)",
	    (char *)NULL);
	return FMT_UNKNOWN;
    }
    Tcl_AppendResult(interp, "can't handle format \"", string, "\"", 
		     (char *)NULL);
    return FMT_UNKNOWN;
}

static int
CopyValues(vPtr, byteArr, fmt, size, length, swap, indexPtr)
    VectorObject *vPtr;
    char *byteArr;
    enum NativeFormats fmt;
    int size;
    int length;
    int swap;
    int *indexPtr;
{
    register int i, n;
    int newSize;

    if ((swap) && (size > 1)) {
	int nBytes = size * length;
	register unsigned char *p;
	register int left, right;

	for (i = 0; i < nBytes; i += size) {
	    p = (unsigned char *)(byteArr + i);
	    for (left = 0, right = size - 1; left < right; left++, right--) {
		p[left] ^= p[right];
		p[right] ^= p[left];
		p[left] ^= p[right];
	    }

	}
    }
    newSize = *indexPtr + length;
    if (newSize > vPtr->length) {
	if (Blt_VectorChangeLength(vPtr, newSize) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
#define CopyArrayToVector(vPtr, arr) \
    for (i = 0, n = *indexPtr; i < length; i++, n++) { \
	(vPtr)->valueArr[n] = (double)(arr)[i]; \
    }

    switch (fmt) {
    case FMT_CHAR:
	CopyArrayToVector(vPtr, (char *)byteArr);
	break;

    case FMT_UCHAR:
	CopyArrayToVector(vPtr, (unsigned char *)byteArr);
	break;

    case FMT_INT:
	CopyArrayToVector(vPtr, (int *)byteArr);
	break;

    case FMT_UINT:
	CopyArrayToVector(vPtr, (unsigned int *)byteArr);
	break;

    case FMT_LONG:
	CopyArrayToVector(vPtr, (long *)byteArr);
	break;

    case FMT_ULONG:
	CopyArrayToVector(vPtr, (unsigned long *)byteArr);
	break;

    case FMT_SHORT:
	CopyArrayToVector(vPtr, (short int *)byteArr);
	break;

    case FMT_USHORT:
	CopyArrayToVector(vPtr, (unsigned short int *)byteArr);
	break;

    case FMT_FLOAT:
	CopyArrayToVector(vPtr, (float *)byteArr);
	break;

    case FMT_DOUBLE:
	CopyArrayToVector(vPtr, (double *)byteArr);
	break;

    case FMT_UNKNOWN:
	break;
    }
    *indexPtr += length;
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * BinreadOp --
 *
 *	Reads binary values from a Tcl channel. Values are either appended
 *	to the end of the vector or placed at a given index (using the
 *	"-at" option), overwriting existing values.  Data is read until EOF
 *	is found on the channel or a specified number of values are read.
 *	(note that this is not necessarily the same as the number of bytes).
 *
 *	The following flags are supported:
 *		-swap		Swap bytes
 *		-at index	Start writing data at the index.
 *		-format fmt	Specifies the format of the data.
 *
 *	This binary reader was created by Harald Kirsch (kir@iitb.fhg.de).
 *	Anything that's wrong is due to my munging of his code.
 *
 * Results:
 *	Returns a standard Tcl result. The interpreter result will contain
 *	the number of values (not the number of bytes) read.
 *
 * Caveats:
 *	Channel reads must end on an element boundary.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BinreadOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Tcl_Channel channel;
    char *byteArr;
    char *string;
    enum NativeFormats fmt;
    int arraySize, bytesRead;
    int count, total;
    int first;
    int size, length, mode;
    int swap;
    register int i;

    string = Tcl_GetString(objv[2]);
    channel = Tcl_GetChannel(interp, string, &mode);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
	Tcl_AppendResult(interp, "channel \"", string,
	    "\" wasn't opened for reading", (char *)NULL);
	return TCL_ERROR;
    }
    first = vPtr->length;
    fmt = FMT_DOUBLE;
    size = sizeof(double);
    swap = FALSE;
    count = 0;

    if (objc > 3) {
	string = Tcl_GetString(objv[3]);
	if (string[0] != '-') {
	    long int value;
	    /* Get the number of values to read.  */
	    if (Tcl_GetLongFromObj(interp, objv[3], &value) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (value < 0) {
		Tcl_AppendResult(interp, "count can't be negative", 
				 (char *)NULL);
		return TCL_ERROR;
	    }
	    count = (int)value;
	    objc--, objv++;
	}
    }
    /* Process any option-value pairs that remain.  */
    for (i = 3; i < objc; i++) {
	string = Tcl_GetString(objv[i]);
	if (strcmp(string, "-swap") == 0) {
	    swap = TRUE;
	} else if (strcmp(string, "-format") == 0) {
	    i++;
	    if (i >= objc) {
		Tcl_AppendResult(interp, "missing arg after \"", string,
		    "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    string = Tcl_GetString(objv[i]);
	    fmt = GetBinaryFormat(interp, string, &size);
	    if (fmt == FMT_UNKNOWN) {
		return TCL_ERROR;
	    }
	} else if (strcmp(string, "-at") == 0) {
	    i++;
	    if (i >= objc) {
		Tcl_AppendResult(interp, "missing arg after \"", string,
		    "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    string = Tcl_GetString(objv[i]);
	    if (Blt_VectorGetIndex(interp, vPtr, string, &first, 0, 
			 (Blt_VectorIndexProc **)NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (first > vPtr->length) {
		Tcl_AppendResult(interp, "index \"", string,
		    "\" is out of range", (char *)NULL);
		return TCL_ERROR;
	    }
	}
    }

#define BUFFER_SIZE 1024
    if (count == 0) {
	arraySize = BUFFER_SIZE * size;
    } else {
	arraySize = count * size;
    }

    byteArr = Blt_Malloc(arraySize);
    assert(byteArr);

    /* FIXME: restore old channel translation later? */
    if (Tcl_SetChannelOption(interp, channel, "-translation",
	    "binary") != TCL_OK) {
	return TCL_ERROR;
    }
    total = 0;
    while (!Tcl_Eof(channel)) {
	bytesRead = Tcl_Read(channel, byteArr, arraySize);
	if (bytesRead < 0) {
	    Tcl_AppendResult(interp, "error reading channel: ",
		Tcl_PosixError(interp), (char *)NULL);
	    return TCL_ERROR;
	}
	if ((bytesRead % size) != 0) {
	    Tcl_AppendResult(interp, "error reading channel: short read",
		(char *)NULL);
	    return TCL_ERROR;
	}
	length = bytesRead / size;
	if (CopyValues(vPtr, byteArr, fmt, size, length, swap, &first)
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	total += length;
	if (count > 0) {
	    break;
	}
    }
    Blt_Free(byteArr);

    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);

    /* Set the result as the number of values read.  */
    Tcl_SetObjResult(interp, Tcl_NewIntObj(total));
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * SearchOp --
 *
 *	Searchs for a value in the vector. Returns the indices of all
 *	vector elements matching a particular value.
 *
 * Results:
 *	Always returns TCL_OK.  interp->result will contain a list of
 *	the indices of array elements matching value. If no elements
 *	match, interp->result will contain the empty string.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SearchOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    double min, max;
    register int i;
    int wantValue;
    char *string;
    Tcl_Obj *listObjPtr;

    wantValue = FALSE;
    string = Tcl_GetString(objv[2]);
    if ((string[0] == '-') && (strcmp(string, "-value") == 0)) {
	wantValue = TRUE;
	objv++, objc--;
    }
    if (GetDouble(interp, objv[2], &min) != TCL_OK) {
	return TCL_ERROR;
    }
    max = min;
    if ((objc > 3) && (GetDouble(interp, objv[3], &max) != TCL_OK)) {
	return TCL_ERROR;
    }
    if ((min - max) >= DBL_EPSILON) {
	return TCL_OK;		/* Bogus range. Don't bother looking. */
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (wantValue) {
	for (i = 0; i < vPtr->length; i++) {
	    if (InRange(vPtr->valueArr[i], min, max)) {
		Tcl_ListObjAppendElement(interp, listObjPtr, 
			Tcl_NewDoubleObj(vPtr->valueArr[i]));
	    }
	}
    } else {
	for (i = 0; i < vPtr->length; i++) {
	    if (InRange(vPtr->valueArr[i], min, max)) {
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewIntObj(i + vPtr->offset));
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * OffsetOp --
 *
 *	Queries or sets the offset of the array index from the base
 *	address of the data array of values.
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
OffsetOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    if (objc == 3) {
	int newOffset;

	if (Tcl_GetIntFromObj(interp, objv[2], &newOffset) != TCL_OK) {
	    return TCL_ERROR;
	}
	vPtr->offset = newOffset;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(vPtr->offset));
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * RandomOp --
 *
 *	Generates random values for the length of the vector.
 *
 * Results:
 *	A standard Tcl result.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RandomOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;		/* Not used. */
{
#ifdef HAVE_DRAND48
    register int i;

    for (i = 0; i < vPtr->length; i++) {
	vPtr->valueArr[i] = drand48();
    }
#endif /* HAVE_DRAND48 */
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * SeqOp --
 *
 *	Generates a sequence of values in the vector.
 *
 * Results:
 *	A standard Tcl result.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SeqOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    register int i;
    double start, finish, step;
    int fillVector;
    int nSteps;
    char *string;

    if (GetDouble(interp, objv[2], &start) != TCL_OK) {
	return TCL_ERROR;
    }
    fillVector = FALSE;
    string = Tcl_GetString(objv[3]);
    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
	fillVector = TRUE;
    } else if (GetDouble(interp, objv[3], &finish) != TCL_OK) {
	return TCL_ERROR;
    }
    step = 1.0;
    if ((objc > 4) && (GetDouble(interp, objv[4], &step) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (fillVector) {
	nSteps = vPtr->length;
    } else {
	nSteps = (int)((finish - start) / step) + 1;
    }
    if (nSteps > 0) {
	if (Blt_VectorChangeLength(vPtr, nSteps) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (i = 0; i < nSteps; i++) {
	    vPtr->valueArr[i] = start + (step * (double)i);
	}
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
	Blt_VectorUpdateClients(vPtr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * SetOp --
 *
 *	Sets the data of the vector object from a list of values.
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vector data is reset.  Clients of the vector are notified.
 *	Any cached array indices are flushed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SetOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    int result;
    VectorObject *v2Ptr;
    int nElem;
    Tcl_Obj **elemObjArr;

    /* The source can be either a list of numbers or another vector.  */

    v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL, vPtr->dataPtr, 
	   Tcl_GetString(objv[2]), (char **)NULL, NS_SEARCH_BOTH);
    if (v2Ptr != NULL) {
	if (vPtr == v2Ptr) {
	    VectorObject *tmpPtr;
	    /* 
	     * Source and destination vectors are the same.  Copy the
	     * source first into a temporary vector to avoid memory
	     * overlaps. 
	     */
	    tmpPtr = Blt_VectorNew(vPtr->dataPtr);
	    result = Blt_VectorDuplicate(tmpPtr, v2Ptr);
	    if (result == TCL_OK) {
		result = Blt_VectorDuplicate(vPtr, tmpPtr);
	    }
	    Blt_VectorFree(tmpPtr);
	} else {
	    result = Blt_VectorDuplicate(vPtr, v2Ptr);
	}
    } else if (Tcl_ListObjGetElements(interp, objv[2], &nElem, &elemObjArr) 
	       == TCL_OK) {
	result = CopyList(vPtr, nElem, elemObjArr);
    } else {
	return TCL_ERROR;
    }

    if (result == TCL_OK) {
	/*
	 * The vector has changed; so flush the array indices (they're
	 * wrong now), find the new range of the data, and notify
	 * the vector's clients that it's been modified.
	 */
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
	Blt_VectorUpdateClients(vPtr);
    }
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * SplitOp --
 *
 *	Copies the values from the vector evens into one of more
 *	vectors.
 *
 * Results:
 *	A standard Tcl result.  
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SplitOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    int nVectors;

    nVectors = objc - 2;
    if ((vPtr->length % nVectors) != 0) {
	Tcl_AppendResult(interp, "can't split vector \"", vPtr->name, 
	   "\" into ", Blt_Itoa(nVectors), " even parts.", (char *)NULL);
	return TCL_ERROR;
    }
    if (nVectors > 0) {
	VectorObject *v2Ptr;
	char *string;		/* Name of vector. */
	int i, j, k;
	int oldSize, newSize, extra, isNew;

	extra = vPtr->length / nVectors;
	for (i = 0; i < nVectors; i++) {
	    string = Tcl_GetString(objv[i+2]);
	    v2Ptr = Blt_VectorCreate(vPtr->dataPtr, string, string, string,
		&isNew);
	    oldSize = v2Ptr->length;
	    newSize = oldSize + extra;
	    if (Blt_VectorChangeLength(v2Ptr, newSize) != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (j = i, k = oldSize; j < vPtr->length; j += nVectors, k++) {
		v2Ptr->valueArr[k] = vPtr->valueArr[j];
	    }
	    Blt_VectorUpdateClients(v2Ptr);
	    if (v2Ptr->flush) {
		Blt_VectorFlushCache(v2Ptr);
	    }
	}
    }
    return TCL_OK;
}


static VectorObject **sortVectorArr; /* Pointer to the array of values
				      * currently being sorted. */
static int nSortVectors;
static int reverse;		/* Indicates the ordering of the sort. If
				 * non-zero, the vectors are sorted in
				 * decreasing order */

static int
CompareVectors(a, b)
    void *a;
    void *b;
{
    double delta;
    int i;
    int sign;
    register VectorObject *vPtr;

    sign = (reverse) ? -1 : 1;
    for (i = 0; i < nSortVectors; i++) {
	vPtr = sortVectorArr[i];
	delta = vPtr->valueArr[*(int *)a] - vPtr->valueArr[*(int *)b];
	if (delta < 0.0) {
	    return (-1 * sign);
	} else if (delta > 0.0) {
	    return (1 * sign);
	}
    }
    return 0;
}

int *
Blt_VectorSortIndex(vPtrPtr, nVectors)
    VectorObject **vPtrPtr;
    int nVectors;
{
    int *indexArr;
    register int i;
    VectorObject *vPtr = *vPtrPtr;
    int length;

    length = vPtr->last - vPtr->first + 1;
    indexArr = Blt_Malloc(sizeof(int) * length);
    assert(indexArr);
    for (i = vPtr->first; i <= vPtr->last; i++) {
	indexArr[i] = i;
    }
    sortVectorArr = vPtrPtr;
    nSortVectors = nVectors;
    qsort((char *)indexArr, length, sizeof(int), 
	  (QSortCompareProc *)CompareVectors);
    return indexArr;
}

static int *
SortVectors(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    VectorObject **vPtrArray, *v2Ptr;
    int *iArr;
    register int i;

    vPtrArray = Blt_Malloc(sizeof(VectorObject *) * (objc + 1));
    assert(vPtrArray);
    vPtrArray[0] = vPtr;
    iArr = NULL;
    for (i = 0; i < objc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, Tcl_GetString(objv[i]), &v2Ptr)
	    != TCL_OK) {
	    goto error;
	}
	if (v2Ptr->length != vPtr->length) {
	    Tcl_AppendResult(interp, "vector \"", v2Ptr->name,
		"\" is not the same size as \"", vPtr->name, "\"",
		(char *)NULL);
	    goto error;
	}
	vPtrArray[i + 1] = v2Ptr;
    }
    iArr = Blt_VectorSortIndex(vPtrArray, objc + 1);
  error:
    Blt_Free(vPtrArray);
    return iArr;
}


/*
 * -----------------------------------------------------------------------
 *
 * SortOp --
 *
 *	Sorts the vector object and any other vectors according to
 *	sorting order of the vector object.
 *
 * Results:
 *	A standard Tcl result.  If any of the auxiliary vectors are
 *	a different size than the sorted vector object, TCL_ERROR is
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vectors are sorted.
 *
 * -----------------------------------------------------------------------
 */

static int
SortOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    VectorObject *v2Ptr;
    char *string;
    double *mergeArr;
    int *iArr;
    int refSize, nBytes;
    int result;
    register int i, n;

    reverse = FALSE;
    if (objc > 2) {
	int length;

	string = Tcl_GetStringFromObj(objv[2], &length);
	if (string[0] == '-') {
	    if ((length > 1) && (strncmp(string, "-reverse", length) == 0)) {
		reverse = TRUE;
	    } else {
		Tcl_AppendResult(interp, "unknown flag \"", string,
			 "\": should be \"-reverse\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    objc--, objv++;
	}
    }
    if (objc > 2) {
	iArr = SortVectors(vPtr, interp, objc - 2, objv + 2);
    } else {
	iArr = Blt_VectorSortIndex(&vPtr, 1);
    }
    if (iArr == NULL) {
	return TCL_ERROR;
    }
    refSize = vPtr->length;

    /*
     * Create an array to store a copy of the current values of the
     * vector. We'll merge the values back into the vector based upon
     * the indices found in the index array.
     */
    nBytes = sizeof(double) * refSize;
    mergeArr = Blt_Malloc(nBytes);
    assert(mergeArr);
    memcpy((char *)mergeArr, (char *)vPtr->valueArr, nBytes);
    for (n = 0; n < refSize; n++) {
	vPtr->valueArr[n] = mergeArr[iArr[n]];
    }
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);

    /* Now sort any other vectors in the same fashion.  The vectors
     * must be the same size as the iArr though.  */
    result = TCL_ERROR;
    for (i = 2; i < objc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, Tcl_GetString(objv[i]), &v2Ptr)
	    != TCL_OK) {
	    goto error;
	}
	if (v2Ptr->length != refSize) {
	    Tcl_AppendResult(interp, "vector \"", v2Ptr->name,
		"\" is not the same size as \"", vPtr->name, "\"",
		(char *)NULL);
	    goto error;
	}
	memcpy((char *)mergeArr, (char *)v2Ptr->valueArr, nBytes);
	for (n = 0; n < refSize; n++) {
	    v2Ptr->valueArr[n] = mergeArr[iArr[n]];
	}
	Blt_VectorUpdateClients(v2Ptr);
	if (v2Ptr->flush) {
	    Blt_VectorFlushCache(v2Ptr);
	}
    }
    result = TCL_OK;
  error:
    Blt_Free(mergeArr);
    Blt_Free(iArr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InstExprOp --
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
InstExprOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{

    if (Blt_ExprVector(interp, Tcl_GetString(objv[2]), (Blt_Vector *) vPtr) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (vPtr->flush) {
	Blt_VectorFlushCache(vPtr);
    }
    Blt_VectorUpdateClients(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * ArithOp --
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vector data is reset.  Clients of the vector are notified.
 *	Any cached array indices are flushed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ArithOp(vPtr, interp, objc, objv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int objc;			/* Not used. */
    Tcl_Obj *CONST *objv;
{
    register double value;
    register int i;
    VectorObject *v2Ptr;
    double scalar;
    Tcl_Obj *listObjPtr;
    char *string;

    v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL, vPtr->dataPtr, 
		Tcl_GetString(objv[2]), (char **)NULL, NS_SEARCH_BOTH);
    if (v2Ptr != NULL) {
	register int j;
	int length;

	length = v2Ptr->last - v2Ptr->first + 1;
	if (length != vPtr->length) {
	    Tcl_AppendResult(interp, "vectors \"", Tcl_GetString(objv[0]), 
		"\" and \"", Tcl_GetString(objv[2]), 
		"\" are not the same length", (char *)NULL);
	    return TCL_ERROR;
	}
	string = Tcl_GetString(objv[1]);
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	switch (string[0]) {
	case '*':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] * v2Ptr->valueArr[j];
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '/':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] / v2Ptr->valueArr[j];
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '-':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] - v2Ptr->valueArr[j];
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '+':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] + v2Ptr->valueArr[j];
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;
	}
	Tcl_SetObjResult(interp, listObjPtr);

    } else if (GetDouble(interp, objv[2], &scalar) == TCL_OK) {
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	string = Tcl_GetString(objv[1]);
	switch (string[0]) {
	case '*':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] * scalar;
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '/':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] / scalar;
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '-':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] - scalar;
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;

	case '+':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] + scalar;
		Tcl_ListObjAppendElement(interp, listObjPtr,
			 Tcl_NewDoubleObj(value));
	    }
	    break;
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorInstCmd --
 *
 *	Parses and invokes the appropriate vector instance command
 *	option.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec vectorInstOps[] =
{
    {"*", 1, (Blt_Op)ArithOp, 3, 3, "item",},	/*Deprecated*/
    {"+", 1, (Blt_Op)ArithOp, 3, 3, "item",},	/*Deprecated*/
    {"-", 1, (Blt_Op)ArithOp, 3, 3, "item",},	/*Deprecated*/
    {"/", 1, (Blt_Op)ArithOp, 3, 3, "item",},	/*Deprecated*/
    {"append", 1, (Blt_Op)AppendOp, 3, 0, "item ?item...?",},
    {"binread", 1, (Blt_Op)BinreadOp, 3, 0, "channel ?numValues? ?flags?",},
    {"clear", 1, (Blt_Op)ClearOp, 2, 2, "",},
    {"delete", 2, (Blt_Op)DeleteOp, 2, 0, "index ?index...?",},
    {"dup", 2, (Blt_Op)DupOp, 3, 0, "vecName",},
    {"expr", 1, (Blt_Op)InstExprOp, 3, 3, "expression",},
    {"index", 1, (Blt_Op)IndexOp, 3, 4, "index ?value?",},
    {"length", 1, (Blt_Op)LengthOp, 2, 3, "?newSize?",},
    {"merge", 1, (Blt_Op)MergeOp, 3, 0, "vecName ?vecName...?",},
    {"normalize", 3, (Blt_Op)NormalizeOp, 2, 3, "?vecName?",},	/*Deprecated*/
    {"notify", 3, (Blt_Op)NotifyOp, 3, 3, "keyword",},
    {"offset", 2, (Blt_Op)OffsetOp, 2, 3, "?offset?",},
    {"populate", 1, (Blt_Op)PopulateOp, 4, 4, "vecName density",},
    {"random", 4, (Blt_Op)RandomOp, 2, 2, "",},	/*Deprecated*/
    {"range", 4, (Blt_Op)RangeOp, 4, 4, "first last",},
    {"search", 3, (Blt_Op)SearchOp, 3, 4, "?-value? value ?value?",},
    {"seq", 3, (Blt_Op)SeqOp, 4, 5, "start end ?step?",},
    {"set", 3, (Blt_Op)SetOp, 3, 3, "list",},
    {"sort", 2, (Blt_Op)SortOp, 2, 0, "?-reverse? ?vecName...?",},
    {"split", 2, (Blt_Op)SplitOp, 2, 0, "?vecName...?",},
    {"variable", 1, (Blt_Op)MapOp, 2, 3, "?varName?",},
};

static int nInstOps = sizeof(vectorInstOps) / sizeof(Blt_OpSpec);

int
Blt_VectorInstCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST *objv;
{
    Blt_Op proc;
    VectorObject *vPtr = clientData;

    vPtr->first = 0;
    vPtr->last = vPtr->length - 1;
    proc = Blt_GetOpFromObj(interp, nInstOps, vectorInstOps, BLT_OP_ARG1, objc,
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (vPtr, interp, objc, objv);
}


/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorVarTrace --
 *
 * Results:
 *	Returns NULL on success.  Only called from a variable trace.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------
 */
char *
Blt_VectorVarTrace(clientData, interp, part1, part2, flags)
    ClientData clientData;	/* Vector object. */
    Tcl_Interp *interp;
    char *part1, *part2;
    int flags;
{
    Blt_VectorIndexProc *indexProc;
    VectorObject *vPtr = clientData;
    int first, last;
    int varFlags;
#define MAX_ERR_MSG	1023
    static char message[MAX_ERR_MSG + 1];

    if (part2 == NULL) {
	if (flags & TCL_TRACE_UNSETS) {
	    Blt_Free(vPtr->arrayName);
	    vPtr->arrayName = NULL;
	    vPtr->varNsPtr = NULL;
	    if (vPtr->freeOnUnset) {
		Blt_VectorFree(vPtr);
	    }
	}
	return NULL;
    }
    if (Blt_VectorGetIndexRange(interp, vPtr, part2, INDEX_ALL_FLAGS, 
		&indexProc) != TCL_OK) {
	goto error;
    }
    first = vPtr->first, last = vPtr->last;
    varFlags = TCL_LEAVE_ERR_MSG | (TCL_GLOBAL_ONLY & flags);
    if (flags & TCL_TRACE_WRITES) {
	double value;
	Tcl_Obj *objPtr;

	if (first == SPECIAL_INDEX) { /* Tried to set "min" or "max" */
	    return "read-only index";
	}
	objPtr = Tcl_GetVar2Ex(interp, part1, part2, varFlags);
	if (objPtr == NULL) {
	    goto error;
	}
	if (GetDouble(interp, objPtr, &value) != TCL_OK) {
	    if ((last == first) && (first >= 0)) {
		/* Single numeric index. Reset the array element to
		 * its old value on errors */
		Tcl_SetVar2Ex(interp, part1, part2, objPtr, varFlags);
	    }
	    goto error;
	}
	if (first == vPtr->length) {
	    if (Blt_VectorChangeLength(vPtr, vPtr->length + 1) != TCL_OK) {
		return "error resizing vector";
	    }
	}
	/* Set possibly an entire range of values */
	ReplicateValue(vPtr, first, last, value);
    } else if (flags & TCL_TRACE_READS) {
	double value;
	Tcl_Obj *objPtr;

	if (vPtr->length == 0) {
	    if (Tcl_SetVar2(interp, part1, part2, "", varFlags) == NULL) {
		goto error;
	    }
	    return NULL;
	}
	if  (first == vPtr->length) {
	    return "write-only index";
	}
	if (first == last) {
	    if (first >= 0) {
		value = vPtr->valueArr[first];
	    } else {
		vPtr->first = 0, vPtr->last = vPtr->length - 1;
		value = (*indexProc) ((Blt_Vector *) vPtr);
	    }
	    objPtr = Tcl_NewDoubleObj(value);
	    if (Tcl_SetVar2Ex(interp, part1, part2, objPtr, varFlags) == NULL) {
		Tcl_DecrRefCount(objPtr);
		goto error;
	    }
	} else {
	    objPtr = GetValues(vPtr, first, last);
	    if (Tcl_SetVar2Ex(interp, part1, part2, objPtr, varFlags) == NULL) {
		Tcl_DecrRefCount(objPtr);
		goto error;
	    }
	}
    } else if (flags & TCL_TRACE_UNSETS) {
	register int i, j;

	if ((first == vPtr->length) || (first == SPECIAL_INDEX)) {
	    return "special vector index";
	}
	/*
	 * Collapse the vector from the point of the first unset element.
	 * Also flush any array variable entries so that the shift is
	 * reflected when the array variable is read.
	 */
	for (i = first, j = last + 1; j < vPtr->length; i++, j++) {
	    vPtr->valueArr[i] = vPtr->valueArr[j];
	}
	vPtr->length -= ((last - first) + 1);
	if (vPtr->flush) {
	    Blt_VectorFlushCache(vPtr);
	}
    } else {
	return "unknown variable trace flag";
    }
    if (flags & (TCL_TRACE_UNSETS | TCL_TRACE_WRITES)) {
	Blt_VectorUpdateClients(vPtr);
    }
    Tcl_ResetResult(interp);
    return NULL;

 error: 
    strncpy(message, Tcl_GetStringResult(interp), MAX_ERR_MSG);
    message[MAX_ERR_MSG] = '\0';
    return message;
}

#endif /* TCL_MAJOR_VERSION > 7 */
