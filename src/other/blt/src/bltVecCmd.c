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

#if (TCL_MAJOR_VERSION == 7)

static void
GetValues(vPtr, first, last, resultPtr) 
    VectorObject *vPtr;
    int first, last;
    Tcl_DString *resultPtr;
{ 
    register int i; 
    char valueString[TCL_DOUBLE_SPACE + 1]; 

    for (i = first; i <= last; i++) { 
	Tcl_PrintDouble(vPtr->interp, vPtr->valueArr[i], valueString); 
	Tcl_DStringAppendElement(resultPtr, valueString); 
    } 
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
CopyList(vPtr, nElem, elemArr)
    VectorObject *vPtr;
    int nElem;
    char **elemArr;
{
    register int i;
    double value;

    if (Blt_VectorChangeLength(vPtr, nElem) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < nElem; i++) {
	if (Tcl_GetDouble(vPtr->interp, elemArr[i], &value)!= TCL_OK) {
	    vPtr->length = i;
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
AppendList(vPtr, nElem, elemArr)
    VectorObject *vPtr;
    int nElem;
    char **elemArr;
{
    int count;
    register int i;
    double value;
    int oldSize;

    oldSize = vPtr->length;
    if (Blt_VectorChangeLength(vPtr, vPtr->length + nElem) != TCL_OK) {
	return TCL_ERROR;
    }
    count = oldSize;
    for (i = 0; i < nElem; i++) {
	if (Tcl_ExprDouble(vPtr->interp, elemArr[i], &value) 
	    != TCL_OK) {
	    vPtr->length = count;
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
AppendOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    int result;
    VectorObject *v2Ptr;

    for (i = 2; i < argc; i++) {
	v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL, vPtr->dataPtr, 
		argv[i], (char **)NULL, NS_SEARCH_BOTH);
	if (v2Ptr != NULL) {
	    result = AppendVector(vPtr, v2Ptr);
	} else {
	    int nElem;
	    char **elemArr;

	    if (Tcl_SplitList(interp, argv[i], &nElem, &elemArr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    result = AppendList(vPtr, nElem, elemArr);
	    Blt_Free(elemArr);
	}
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc > 2) {
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
ClearOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
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
DeleteOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    unsigned char *unsetArr;
    register int i, j;
    register int count;

    if (argc == 2) {
	Blt_VectorFree(vPtr);
	return TCL_OK;
    }
    /*
     * Allocate an "unset" bitmap the size of the vector.  We should
     * try to use bit fields instead of a character array, since
     * memory may be an issue if the vector is large.
     */
    unsetArr = Blt_Calloc(sizeof(unsigned char), vPtr->length);
    assert(unsetArr);
    for (i = 2; i < argc; i++) {
	if (Blt_VectorGetIndexRange(interp, vPtr, argv[i], 
		(INDEX_COLON | INDEX_CHECK), (Blt_VectorIndexProc **) NULL) 
		!= TCL_OK) {
	    Blt_Free(unsetArr);
	    return TCL_ERROR;
	}
	for (j = vPtr->first; j <= vPtr->last; j++) {
	    unsetArr[j] = TRUE;
	}
    }
    count = 0;
    for (i = 0; i < vPtr->length; i++) {
	if (unsetArr[i]) {
	    continue;
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
DupOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    VectorObject *v2Ptr;
    int isNew;
    register int i;

    for (i = 2; i < argc; i++) {
	v2Ptr = Blt_VectorCreate(vPtr->dataPtr, argv[i], argv[i], argv[i], 
		&isNew);
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
IndexOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int first, last;

    if (Blt_VectorGetIndexRange(interp, vPtr, argv[2], INDEX_ALL_FLAGS, 
		(Blt_VectorIndexProc **) NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    first = vPtr->first, last = vPtr->last;
    if (argc == 3) {
	Tcl_DString dString;

	if (first == vPtr->length) {
	    Tcl_AppendResult(interp, "can't get index \"", argv[2], "\"",
		(char *)NULL);
	    return TCL_ERROR;	/* Can't read from index "++end" */
	}
	Tcl_DStringInit(&dString);
	GetValues(vPtr, first, last, &dString);
	Tcl_DStringResult(interp, &dString);
	Tcl_DStringFree(&dString);
    } else {
	char string[TCL_DOUBLE_SPACE + 1];
	double value;

	if (first == SPECIAL_INDEX) {
	    Tcl_AppendResult(interp, "can't set index \"", argv[2], "\"",
		(char *)NULL);
	    return TCL_ERROR;	/* Tried to set "min" or "max" */
	}
	if (Tcl_ExprDouble(interp, argv[3], &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (first == vPtr->length) {
	    if (Blt_VectorChangeLength(vPtr, vPtr->length + 1) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	ReplicateValue(vPtr, first, last, value);

	Tcl_PrintDouble(interp, value, string);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
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
LengthOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 3) {
	int size;

	if (Tcl_GetInt(interp, argv[2], &size) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (size < 0) {
	    Tcl_AppendResult(interp, "bad vector size \"", argv[3], "\"",
		(char *)NULL);
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
    Tcl_SetResult(interp, Blt_Itoa(vPtr->length), TCL_VOLATILE);
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
MapOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    if (argc > 2) {
	if (Blt_VectorMapVariable(interp, vPtr, argv[2]) != TCL_OK) {
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
MergeOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    VectorObject *v2Ptr;
    VectorObject **vecArr;
    register VectorObject **vPtrPtr;
    int refSize, length, nElem;
    register int i;
    double *valuePtr, *valueArr;

    /* Allocate an array of vector pointers of each vector to be
     * merged in the current vector.  */
    vecArr = Blt_Malloc(sizeof(VectorObject *) * argc);
    assert(vecArr);
    vPtrPtr = vecArr;

    refSize = -1;
    nElem = 0;
    for (i = 2; i < argc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, argv[i], &v2Ptr) != TCL_OK) {
	    Blt_Free(vecArr);
	    return TCL_ERROR;
	}
	/* Check that all the vectors are the same length */
	length = v2Ptr->last - v2Ptr->first + 1;
	if (refSize < 0) {
	    refSize = length;
	} else if (length != refSize) {
	    Tcl_AppendResult(vPtr->interp, "vector \"", v2Ptr->name,
		"\" has inconsistent length", (char *)NULL);
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
	Blt_Free(vecArr);
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
NormalizeOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    double range;

    Blt_VectorUpdateRange(vPtr);
    range = vPtr->max - vPtr->min;
    if (argc > 2) {
	VectorObject *v2Ptr;
	int isNew;

	v2Ptr = Blt_VectorCreate(vPtr->dataPtr, argv[2], argv[2], argv[2], 
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

	for (i = 0; i < vPtr->length; i++) {
	    norm = (vPtr->valueArr[i] - vPtr->min) / range;
	    Tcl_AppendElement(interp, Blt_Dtoa(interp, norm));
	}
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
NotifyOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char c;
    int length;

    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'a') && (length > 1)
	&& (strncmp(argv[2], "always", length) == 0)) {
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_ALWAYS;
    } else if ((c == 'n') && (length > 2)
	&& (strncmp(argv[2], "never", length) == 0)) {
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_NEVER;
    } else if ((c == 'w') && (length > 1)
	&& (strncmp(argv[2], "whenidle", length) == 0)) {
	vPtr->notifyFlags &= ~NOTIFY_WHEN_MASK;
	vPtr->notifyFlags |= NOTIFY_WHENIDLE;
    } else if ((c == 'n') && (length > 2)
	&& (strncmp(argv[2], "now", length) == 0)) {
	/* How does this play when an update is pending? */
	Blt_VectorNotifyClients(vPtr);
    } else if ((c == 'c') && (length > 1)
	&& (strncmp(argv[2], "cancel", length) == 0)) {
	if (vPtr->notifyFlags & NOTIFY_PENDING) {
	    vPtr->notifyFlags &= ~NOTIFY_PENDING;
	    Tcl_CancelIdleCall(Blt_VectorNotifyClients, vPtr);
	}
    } else if ((c == 'p') && (length > 1)
	&& (strncmp(argv[2], "pending", length) == 0)) {
	Blt_SetBooleanResult(interp, (vPtr->notifyFlags & NOTIFY_PENDING));
    } else {
	Tcl_AppendResult(interp, "bad qualifier \"", argv[2], "\": should be \
\"always\", \"never\", \"whenidle\", \"now\", \"cancel\", or \"pending\"",
	    (char *)NULL);
	return TCL_ERROR;
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
PopulateOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    VectorObject *v2Ptr;
    int size, density;
    int isNew;
    register int i, j;
    double slice, range;
    register double *valuePtr;
    int count;

    v2Ptr = Blt_VectorCreate(vPtr->dataPtr, argv[2], argv[2], argv[2], 
		     &isNew);
    if (v2Ptr == NULL) {
	return TCL_ERROR;
    }
    if (vPtr->length == 0) {
	return TCL_OK;		/* Source vector is empty. */
    }
    if (Tcl_GetInt(interp, argv[3], &density) != TCL_OK) {
	return TCL_ERROR;
    }
    if (density < 1) {
	Tcl_AppendResult(interp, "bad density \"", argv[3], "\"", (char *)NULL);
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
RangeOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int first, last;
    register int i;

    if ((Blt_VectorGetIndex(interp, vPtr, argv[2], &first, INDEX_CHECK,
		(Blt_VectorIndexProc **) NULL) != TCL_OK) ||
	(Blt_VectorGetIndex(interp, vPtr, argv[3], &last, INDEX_CHECK,
		(Blt_VectorIndexProc **) NULL) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (first > last) {
	/* Return the list reversed */
	for (i = last; i <= first; i++) {
	    Tcl_AppendElement(interp, Blt_Dtoa(interp, vPtr->valueArr[i]));
	}
    } else {
	for (i = first; i <= last; i++) {
	    Tcl_AppendElement(interp, Blt_Dtoa(interp, vPtr->valueArr[i]));
	}
    }
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
 *	Please note, min can't be greater than max.
 *
 * Results:
 *	If the value is within of the interval [min..max], 1 is 
 *	returned; 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
INLINE static int
InRange(value, min, max)
    register double value, min, max;
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
	return TCL_ERROR;
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
BinreadOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *byteArr;
    enum NativeFormats fmt;
    int size, length, mode;
    Tcl_Channel channel;
    int arraySize, bytesRead;
    int count, total;
    int first;
    int swap;
    register int i;

    channel = Tcl_GetChannel(interp, argv[2], &mode);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
	Tcl_AppendResult(interp, "channel \"", argv[2],
	    "\" wasn't opened for reading", (char *)NULL);
	return TCL_ERROR;
    }
    first = vPtr->length;
    fmt = FMT_DOUBLE;
    size = sizeof(double);
    swap = FALSE;
    count = 0;

    if ((argc > 3) && (argv[3][0] != '-')) {
	long int value;
	/* Get the number of values to read.  */
	if (Tcl_ExprLong(interp, argv[3], &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (value < 0) {
	    Tcl_AppendResult(interp, "count can't be negative", (char *)NULL);
	    return TCL_ERROR;
	}
	count = (int)value;
	argc--, argv++;
    }
    /* Process any option-value pairs that remain.  */
    for (i = 3; i < argc; i++) {
	if (strcmp(argv[i], "-swap") == 0) {
	    swap = TRUE;
	} else if (strcmp(argv[i], "-format") == 0) {
	    i += 1;
	    if (i >= argc) {
		Tcl_AppendResult(interp, "missing arg after \"", argv[i - 1],
		    "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    fmt = GetBinaryFormat(interp, argv[i], &size);
	    if (fmt == FMT_UNKNOWN) {
		return TCL_ERROR;
	    }
	} else if (strcmp(argv[i], "-at") == 0) {
	    i += 1;
	    if (i >= argc) {
		Tcl_AppendResult(interp, "missing arg after \"", argv[i - 1],
		    "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    if (Blt_VectorGetIndex(interp, vPtr, argv[i], &first, 0, 
			 (Blt_VectorIndexProc **)NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (first > vPtr->length) {
		Tcl_AppendResult(interp, "index \"", argv[i],
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
    Tcl_SetResult(interp, Blt_Itoa(total), TCL_VOLATILE);
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
SearchOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    double min, max;
    register int i;
    int wantValue;

    wantValue = FALSE;
    if ((argv[2][0] == '-') && (strcmp(argv[2], "-value") == 0)) {
	wantValue = TRUE;
	argv++, argc--;
    }
    if (Tcl_ExprDouble(interp, argv[2], &min) != TCL_OK) {
	return TCL_ERROR;
    }
    max = min;
    if ((argc > 3) && (Tcl_ExprDouble(interp, argv[3], &max) != TCL_OK)) {
	return TCL_ERROR;
    }
    if ((min - max) >= DBL_EPSILON) {
	return TCL_OK;		/* Bogus range. Don't bother looking. */
    }
    if (wantValue) {
	for (i = 0; i < vPtr->length; i++) {
	    if (InRange(vPtr->valueArr[i], min, max)) {
		Tcl_AppendElement(interp, Blt_Dtoa(interp, vPtr->valueArr[i]));
	    }
	}
    } else {
	for (i = 0; i < vPtr->length; i++) {
	    if (InRange(vPtr->valueArr[i], min, max)) {
		Tcl_AppendElement(interp, Blt_Itoa(i + vPtr->offset));
	    }
	}
    }
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
OffsetOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    if (argc == 3) {
	int newOffset;

	if (Tcl_GetInt(interp, argv[2], &newOffset) != TCL_OK) {
	    return TCL_ERROR;
	}
	vPtr->offset = newOffset;
    }
    Tcl_SetResult(interp, Blt_Itoa(vPtr->offset), TCL_VOLATILE);
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
RandomOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
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
SeqOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    double start, finish, step;
    int fillVector;
    int nSteps;

    if (Tcl_ExprDouble(interp, argv[2], &start) != TCL_OK) {
	return TCL_ERROR;
    }
    fillVector = FALSE;
    if ((argv[3][0] == 'e') && (strcmp(argv[3], "end") == 0)) {
	fillVector = TRUE;
    } else if (Tcl_ExprDouble(interp, argv[3], &finish) != TCL_OK) {
	return TCL_ERROR;
    }
    step = 1.0;
    if ((argc > 4) && (Tcl_ExprDouble(interp, argv[4], &step) != TCL_OK)) {
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
SetOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int result;
    VectorObject *v2Ptr;
    int nElem;
    char **elemArr;

    /* The source can be either a list of expressions of another
     * vector.  */
    if (Tcl_SplitList(interp, argv[2], &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* If there's only one element, see whether it's the name of a
     * vector.  Otherwise, treat it as a single numeric expression. */

    if ((nElem == 1) && ((v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL,
	vPtr->dataPtr, argv[2], (char **)NULL, NS_SEARCH_BOTH)) != NULL)) {
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
    } else {
	result = CopyList(vPtr, nElem, elemArr);
    }
    Blt_Free(elemArr);

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

static VectorObject **sortVectorArr;	/* Pointer to the array of values currently
				 * being sorted. */
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

    indexArr = Blt_Malloc(sizeof(int) * vPtr->length);
    assert(indexArr);
    for (i = 0; i < vPtr->length; i++) {
	indexArr[i] = i;
    }
    sortVectorArr = vPtrPtr;
    nSortVectors = nVectors;
    qsort((char *)indexArr, vPtr->length, sizeof(int),
	    (QSortCompareProc *)CompareVectors);
    return indexArr;
}

static int *
SortVectors(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    VectorObject **vPtrArray, *v2Ptr;
    int *iArr;
    register int i;

    vPtrArray = Blt_Malloc(sizeof(VectorObject *) * (argc + 1));
    assert(vPtrArray);
    vPtrArray[0] = vPtr;
    iArr = NULL;
    for (i = 0; i < argc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, argv[i], &v2Ptr) != TCL_OK) {
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
    iArr = Blt_VectorSortIndex(vPtrArray, argc + 1);
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
SortOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int *iArr;
    double *mergeArr;
    VectorObject *v2Ptr;
    int refSize, nBytes;
    int result;
    register int i, n;

    reverse = FALSE;
    if ((argc > 2) && (argv[2][0] == '-')) {
	int length;

	length = strlen(argv[2]);
	if ((length > 1) && (strncmp(argv[2], "-reverse", length) == 0)) {
	    reverse = TRUE;
	} else {
	    Tcl_AppendResult(interp, "unknown flag \"", argv[2],
		"\": should be \"-reverse\"", (char *)NULL);
	    return TCL_ERROR;
	}
	argc--, argv++;
    }
    if (argc > 2) {
	iArr = SortVectors(vPtr, interp, argc - 2, argv + 2);
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
    for (i = 2; i < argc; i++) {
	if (Blt_VectorLookupName(vPtr->dataPtr, argv[i], &v2Ptr) != TCL_OK) {
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
InstExprOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (Blt_ExprVector(interp, argv[2], (Blt_Vector *) vPtr) != TCL_OK) {
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
ArithOp(vPtr, interp, argc, argv)
    VectorObject *vPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register double value;
    register int i;
    VectorObject *v2Ptr;

    v2Ptr = Blt_VectorParseElement((Tcl_Interp *)NULL, vPtr->dataPtr, argv[2], 
		(char **)NULL, NS_SEARCH_BOTH);
    if (v2Ptr != NULL) {
	register int j;
	int length;

	length = v2Ptr->last - v2Ptr->first + 1;
	if (length != vPtr->length) {
	    Tcl_AppendResult(interp, "vectors \"", argv[0], "\" and \"",
		argv[2], "\" are not the same length", (char *)NULL);
	    return TCL_ERROR;
	}
	switch (argv[1][0]) {
	case '*':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] * v2Ptr->valueArr[j];
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '/':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] / v2Ptr->valueArr[j];
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '-':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] - v2Ptr->valueArr[j];
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '+':
	    for (i = 0, j = v2Ptr->first; i < vPtr->length; i++, j++) {
		value = vPtr->valueArr[i] + v2Ptr->valueArr[j];
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;
	}
    } else {
	double scalar;

	if (Tcl_ExprDouble(interp, argv[2], &scalar) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (argv[1][0]) {
	case '*':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] * scalar;
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '/':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] / scalar;
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '-':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] - scalar;
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;

	case '+':
	    for (i = 0; i < vPtr->length; i++) {
		value = vPtr->valueArr[i] + scalar;
		Tcl_AppendElement(interp, Blt_Dtoa(interp, value));
	    }
	    break;
	}
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
    {"variable", 1, (Blt_Op)MapOp, 2, 3, "?varName?",},
};

static int nInstOps = sizeof(vectorInstOps) / sizeof(Blt_OpSpec);

int
Blt_VectorInstCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    VectorObject *vPtr = clientData;

    vPtr->first = 0;
    vPtr->last = vPtr->length - 1;
    proc = Blt_GetOp(interp, nInstOps, vectorInstOps, BLT_OP_ARG1, argc, argv,
	0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (vPtr, interp, argc, argv);
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
    ClientData clientData;	/* File output information. */
    Tcl_Interp *interp;
    char *part1, *part2;
    int flags;
{
    VectorObject *vPtr = clientData;
    char string[TCL_DOUBLE_SPACE + 1];
#define MAX_ERR_MSG	1023
    static char message[MAX_ERR_MSG + 1];
    Blt_VectorIndexProc *indexProc;
    int varFlags;
    int first, last;

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
	char *newValue;

	if (first == SPECIAL_INDEX) { /* Tried to set "min" or "max" */
	    return "read-only index";
	}
	newValue = Tcl_GetVar2(interp, part1, part2, varFlags);
	if (newValue == NULL) {
	    goto error;
	}
	if (Tcl_ExprDouble(interp, newValue, &value) != TCL_OK) {
	    if ((last == first) && (first >= 0)) {
		/* Single numeric index. Reset the array element to
		 * its old value on errors */
		Tcl_PrintDouble(interp, vPtr->valueArr[first], string);
		Tcl_SetVar2(interp, part1, part2, string, varFlags);
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
	    Tcl_PrintDouble(interp, value, string);
	    if (Tcl_SetVar2(interp, part1, part2, string, varFlags) 
		== NULL) {
		goto error;
	    }
	} else {
	    Tcl_DString dString;
	    char *result;

	    Tcl_DStringInit(&dString);
	    GetValues(vPtr, first, last, &dString);
	    result = Tcl_SetVar2(interp, part1, part2, 
		Tcl_DStringValue(&dString), varFlags);
	    Tcl_DStringFree(&dString);
	    if (result == NULL) {
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

#endif /* TCL_MAJOR_VERSION == 7 */
