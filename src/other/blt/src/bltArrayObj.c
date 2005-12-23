
/*
 * bltArrayObj.c --
 *
 * Copyright 2000 Silicon Metrics, Inc.
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
 *	The array Tcl object was created by George A. Howlett.
 */

#include "bltInt.h"

#ifndef NO_ARRAY
#include "bltHash.h"

static Tcl_DupInternalRepProc DupArrayInternalRep;
static Tcl_FreeInternalRepProc FreeArrayInternalRep;
static Tcl_UpdateStringProc UpdateStringOfArray;
static Tcl_SetFromAnyProc SetArrayFromAny;

static Tcl_ObjType arrayObjType = {
    "array",
    FreeArrayInternalRep,	/* Called when an object is freed. */
    DupArrayInternalRep,	/* Copies an internal representation 
				 * from one object to another. */
    UpdateStringOfArray,	/* Creates string representation from
				 * an object's internal representation. */
    SetArrayFromAny,		/* Creates valid internal representation 
				 * from an object's string representation. */
};

static int
SetArrayFromAny(interp, objPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Tcl_Obj *elemObjPtr;
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char **elemArr;
    char *string;
    int isNew;
    int nElem;
    register int i;

    if (objPtr->typePtr == &arrayObjType) {
	return TCL_OK;
    }
    /*
     * Get the string representation. Make it up-to-date if necessary.
     */
    string = Tcl_GetString(objPtr);
    if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr = Blt_Malloc(sizeof(Blt_HashTable));
    assert(tablePtr);
    Blt_InitHashTable(tablePtr, BLT_STRING_KEYS);
    for (i = 0; i < nElem; i += 2) {
	hPtr = Blt_CreateHashEntry(tablePtr, elemArr[i], &isNew);
	elemObjPtr = Tcl_NewStringObj(elemArr[i + 1], -1);
	Blt_SetHashValue(hPtr, elemObjPtr);

	/* Make sure we increment the reference count */
	Tcl_IncrRefCount(elemObjPtr);
    }
    
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    objPtr->internalRep.otherValuePtr = (VOID *)tablePtr;
    objPtr->typePtr = &arrayObjType;
    Blt_Free(elemArr);

    return TCL_OK;
}

static void
DupArrayInternalRep(srcPtr, destPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *destPtr;		/* Object with internal rep to set. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable *srcTablePtr, *destTablePtr;
    Tcl_Obj *valueObjPtr;
    char *key;
    int isNew;

    srcTablePtr = (Blt_HashTable *)srcPtr->internalRep.otherValuePtr;
    destTablePtr = Blt_Malloc(sizeof(Blt_HashTable));
    assert(destTablePtr);
    Blt_InitHashTable(destTablePtr, BLT_STRING_KEYS);
    for (hPtr = Blt_FirstHashEntry(srcTablePtr, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	key = Blt_GetHashKey(srcTablePtr, hPtr);
	Blt_CreateHashEntry(destTablePtr, key, &isNew);
	valueObjPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);
	Blt_SetHashValue(hPtr, valueObjPtr);

	/* Make sure we increment the reference count now that both
	 * array objects are using the same elements. */
	Tcl_IncrRefCount(valueObjPtr);
    }
    Tcl_InvalidateStringRep(destPtr);
    destPtr->internalRep.otherValuePtr = (VOID *)destTablePtr;
    destPtr->typePtr = &arrayObjType;
}

static void
UpdateStringOfArray(objPtr)
    Tcl_Obj *objPtr;		/* Array object whose string rep to update. */
{
    Tcl_DString dString;
    Blt_HashTable *tablePtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *elemObjPtr;

    tablePtr = (Blt_HashTable *)objPtr->internalRep.otherValuePtr;
    Tcl_DStringInit(&dString);
    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	elemObjPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);
	Tcl_DStringAppendElement(&dString, Blt_GetHashKey(tablePtr, hPtr));
	Tcl_DStringAppendElement(&dString, Tcl_GetString(elemObjPtr));
    }
    objPtr->bytes = Blt_Strdup(Tcl_DStringValue(&dString));
    objPtr->length = strlen(Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
}

static void
FreeArrayInternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Array object to release. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable *tablePtr;
    Tcl_Obj *elemObjPtr;
    
    Tcl_InvalidateStringRep(objPtr);
    tablePtr = (Blt_HashTable *)objPtr->internalRep.otherValuePtr;
    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	elemObjPtr = (Tcl_Obj *)Blt_GetHashValue(hPtr);
	Tcl_DecrRefCount(elemObjPtr);
    }
    Blt_DeleteHashTable(tablePtr);
    Blt_Free(tablePtr);
}

int
Blt_GetArrayFromObj(interp, objPtr, tablePtrPtr) 
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    Blt_HashTable **tablePtrPtr;
{
    if (objPtr->typePtr == &arrayObjType) {
	*tablePtrPtr = (Blt_HashTable *)objPtr->internalRep.otherValuePtr;
	return TCL_OK;
    }
    if (SetArrayFromAny(interp, objPtr) == TCL_OK) {
	*tablePtrPtr = (Blt_HashTable *)objPtr->internalRep.otherValuePtr;
	return TCL_OK;
    }
    return TCL_ERROR;
}
    
Tcl_Obj *
Blt_NewArrayObj(objc, objv) 
    int objc;
    Tcl_Obj *objv[];
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Tcl_Obj *arrayObjPtr, *objPtr;
    int isNew;
    register int i;

    tablePtr = Blt_Malloc(sizeof(Blt_HashTable));
    assert(tablePtr);
    Blt_InitHashTable(tablePtr, BLT_STRING_KEYS);

    for (i = 0; i < objc; i += 2) {
	hPtr = Blt_CreateHashEntry(tablePtr, Tcl_GetString(objv[i]), &isNew);
	if ((i + 1) == objc) {
	    objPtr = bltEmptyStringObjPtr;
	} else {
	    objPtr = objv[i+1];
	}
	Tcl_IncrRefCount(objPtr);
	if (!isNew) {
	    Tcl_Obj *oldObjPtr;

	    oldObjPtr = Blt_GetHashValue(hPtr);
	    Tcl_DecrRefCount(oldObjPtr);
	}
	Blt_SetHashValue(hPtr, objPtr);
    }
    arrayObjPtr = Tcl_NewObj(); 
    /* 
     * Reference counts for entry objects are initialized to 0. They
     * are incremented as they are inserted into the tree via the 
     * Blt_TreeSetValue call. 
     */
    arrayObjPtr->refCount = 0;	
    arrayObjPtr->internalRep.otherValuePtr = (VOID *)tablePtr;
    arrayObjPtr->bytes = NULL;
    arrayObjPtr->length = 0; 
    arrayObjPtr->typePtr = &arrayObjType;
    return arrayObjPtr;
}

int
Blt_IsArrayObj(objPtr)
    Tcl_Obj *objPtr;
{
    return (objPtr->typePtr == &arrayObjType);
}

/*ARGSUSED*/
void
Blt_RegisterArrayObj(interp)
    Tcl_Interp *interp;		/* Not used. */
{
    Tcl_RegisterObjType(&arrayObjType);
}
#endif /* NO_ARRAY */
