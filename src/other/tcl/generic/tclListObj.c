/*
 * tclListObj.c --
 *
 *	This file contains functions that implement the Tcl list object type.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"

/*
 * Prototypes for functions defined later in this file:
 */

static List *		NewListIntRep(int objc, Tcl_Obj *CONST objv[]);
static void		DupListInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		FreeListInternalRep(Tcl_Obj *listPtr);
static int		SetListFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		UpdateStringOfList(Tcl_Obj *listPtr);

/*
 * The structure below defines the list Tcl object type by means of functions
 * that can be invoked by generic object code.
 *
 * The internal representation of a list object is a two-pointer
 * representation. The first pointer designates a List structure that contains
 * an array of pointers to the element objects, together with integers that
 * represent the current element count and the allocated size of the array.
 * The second pointer is normally NULL; during execution of functions in this
 * file that operate on nested sublists, it is occasionally used as working
 * storage to avoid an auxiliary stack.
 */

Tcl_ObjType tclListType = {
    "list",			/* name */
    FreeListInternalRep,	/* freeIntRepProc */
    DupListInternalRep,		/* dupIntRepProc */
    UpdateStringOfList,		/* updateStringProc */
    SetListFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * NewListIntRep --
 *
 *	If objc>0 and objv!=NULL, this function creates a list internal rep
 *	with objc elements given in the array objv.
 *	If objc>0 and objv==NULL it creates the list internal rep of a list
 *	with 0 elements, where enough space has been preallocated to store
 *	objc elements.
 *	If objc<=0, it returns NULL.
 *
 * Results:
 *	A new List struct is returned. If objc<=0 or if the allocation fails
 *	for lack of memory, NULL is returned. The list returned has refCount
 *	0.
 *
 * Side effects:
 *	The ref counts of the elements in objv are incremented since the
 *	resulting list now refers to them.
 *
 *----------------------------------------------------------------------
 */

static List *
NewListIntRep(
    int objc,
    Tcl_Obj *CONST objv[])
{
    Tcl_Obj **elemPtrs;
    List *listRepPtr;
    int i;

    if (objc <= 0) {
	return NULL;
    }

    /*
     * First check to see if we'd overflow and try to allocate an object
     * larger than our memory allocator allows. Note that this is actually a
     * fairly small value when you're on a serious 64-bit machine, but that
     * requires API changes to fix.
     */

    if ((size_t)objc > INT_MAX/sizeof(Tcl_Obj *)) {
	return NULL;
    }

    listRepPtr = (List *) attemptckalloc(sizeof(List) +
	    ((objc-1) * sizeof(Tcl_Obj *)));
    if (listRepPtr == NULL) {
	return NULL;
    }

    listRepPtr->canonicalFlag = 0;
    listRepPtr->refCount = 0;
    listRepPtr->maxElemCount = objc;

    if (objv) {
	listRepPtr->elemCount = objc;
	elemPtrs = &listRepPtr->elements;
	for (i = 0;  i < objc;  i++) {
	    elemPtrs[i] = objv[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
    } else {
	listRepPtr->elemCount = 0;
    }
    return listRepPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewListObj --
 *
 *	This function is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new list object from an
 *	(objc,objv) array: that is, each of the objc elements of the array
 *	referenced by objv is inserted as an element into a new Tcl object.
 *
 *	When TCL_MEM_DEBUG is defined, this function just returns the result
 *	of calling the debugging version Tcl_DbNewListObj.
 *
 * Results:
 *	A new list object is returned that is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation is left
 *	NULL. The resulting new list object has ref count 0.
 *
 * Side effects:
 *	The ref counts of the elements in objv are incremented since the
 *	resulting list now refers to them.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewListObj

Tcl_Obj *
Tcl_NewListObj(
    int objc,			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[])	/* An array of pointers to Tcl objects. */
{
    return Tcl_DbNewListObj(objc, objv, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewListObj(
    int objc,			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[])	/* An array of pointers to Tcl objects. */
{
    List *listRepPtr;
    Tcl_Obj *listPtr;

    TclNewObj(listPtr);

    if (objc <= 0) {
	return listPtr;
    }

    /*
     * Create the internal rep.
     */

    listRepPtr = NewListIntRep(objc, objv);
    if (!listRepPtr) {
	Tcl_Panic("Not enough memory to allocate list");
    }

    /*
     * Now create the object.
     */

    Tcl_InvalidateStringRep(listPtr);
    listPtr->internalRep.twoPtrValue.ptr1 = (void *) listRepPtr;
    listPtr->internalRep.twoPtrValue.ptr2 = NULL;
    listPtr->typePtr = &tclListType;
    listRepPtr->refCount++;

    return listPtr;
}
#endif /* if TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewListObj --
 *
 *	This function is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new list objects. It is the same
 *	as the Tcl_NewListObj function above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the [memory active]
 *	command will report the correct file name and line number when
 *	reporting objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this function just returns the
 *	result of calling Tcl_NewListObj.
 *
 * Results:
 *	A new list object is returned that is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation is left
 *	NULL. The new list object has ref count 0.
 *
 * Side effects:
 *	The ref counts of the elements in objv are incremented since the
 *	resulting list now refers to them.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewListObj(
    int objc,			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[],	/* An array of pointers to Tcl objects. */
    CONST char *file,		/* The name of the source file calling this
				 * function; used for debugging. */
    int line)			/* Line number in the source file; used for
				 * debugging. */
{
    Tcl_Obj *listPtr;
    List *listRepPtr;

    TclDbNewObj(listPtr, file, line);

    if (objc <= 0) {
	return listPtr;
    }

    /*
     * Create the internal rep.
     */

    listRepPtr = NewListIntRep(objc, objv);
    if (!listRepPtr) {
	Tcl_Panic("Not enough memory to allocate list");
    }

    /*
     * Now create the object.
     */

    Tcl_InvalidateStringRep(listPtr);
    listPtr->internalRep.twoPtrValue.ptr1 = (VOID *) listRepPtr;
    listPtr->internalRep.twoPtrValue.ptr2 = NULL;
    listPtr->typePtr = &tclListType;
    listRepPtr->refCount++;

    return listPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewListObj(
    int objc,			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[],	/* An array of pointers to Tcl objects. */
    CONST char *file,		/* The name of the source file calling this
				 * function; used for debugging. */
    int line)			/* Line number in the source file; used for
				 * debugging. */
{
    return Tcl_NewListObj(objc, objv);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetListObj --
 *
 *	Modify an object to be a list containing each of the objc elements of
 *	the object array referenced by objv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object is made a list object and is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation is left
 *	NULL. The ref counts of the elements in objv are incremented since the
 *	list now refers to them. The object's old string and internal
 *	representations are freed and its type is set NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetListObj(
    Tcl_Obj *objPtr,		/* Object whose internal rep to init. */
    int objc,			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[])	/* An array of pointers to Tcl objects. */
{
    List *listRepPtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetListObj");
    }

    /*
     * Free any old string rep and any internal rep for the old type.
     */

    TclFreeIntRep(objPtr);
    objPtr->typePtr = NULL;
    Tcl_InvalidateStringRep(objPtr);

    /*
     * Set the object's type to "list" and initialize the internal rep.
     * However, if there are no elements to put in the list, just give the
     * object an empty string rep and a NULL type.
     */

    if (objc > 0) {
	listRepPtr = NewListIntRep(objc, objv);
	if (!listRepPtr) {
	    Tcl_Panic("Cannot allocate enough memory for Tcl_SetListObj");
	}
	objPtr->internalRep.twoPtrValue.ptr1 = (void *) listRepPtr;
	objPtr->internalRep.twoPtrValue.ptr2 = NULL;
	objPtr->typePtr = &tclListType;
	listRepPtr->refCount++;
    } else {
	objPtr->bytes = tclEmptyStringRep;
	objPtr->length = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjGetElements --
 *
 *	This function returns an (objc,objv) array of the elements in a list
 *	object.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *objcPtr is set to
 *	the count of list elements and *objvPtr is set to a pointer to an
 *	array of (*objcPtr) pointers to each list element. If listPtr does not
 *	refer to a list object and the object can not be converted to one,
 *	TCL_ERROR is returned and an error message will be left in the
 *	interpreter's result if interp is not NULL.
 *
 *	The objects referenced by the returned array should be treated as
 *	readonly and their ref counts are _not_ incremented; the caller must
 *	do that if it holds on to a reference. Furthermore, the pointer and
 *	length returned by this function may change as soon as any function is
 *	called on the list object; be careful about retaining the pointer in a
 *	local data structure.
 *
 * Side effects:
 *	The possible conversion of the object referenced by listPtr
 *	to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjGetElements(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr,	/* List object for which an element array is
				 * to be returned. */
    int *objcPtr,		/* Where to store the count of objects
				 * referenced by objv. */
    Tcl_Obj ***objvPtr)		/* Where to store the pointer to an array of
				 * pointers to the list's objects. */
{
    register List *listRepPtr;

    if (listPtr->typePtr != &tclListType) {
	int result, length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    *objcPtr = 0;
	    *objvPtr = NULL;
	    return TCL_OK;
	}

	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    *objcPtr = listRepPtr->elemCount;
    *objvPtr = &listRepPtr->elements;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjAppendList --
 *
 *	This function appends the objects in the list referenced by
 *	elemListPtr to the list object referenced by listPtr. If listPtr is
 *	not already a list object, an attempt will be made to convert it to
 *	one.
 *
 * Results:
 *	The return value is normally TCL_OK. If listPtr or elemListPtr do not
 *	refer to list objects and they can not be converted to one, TCL_ERROR
 *	is returned and an error message is left in the interpreter's result
 *	if interp is not NULL.
 *
 * Side effects:
 *	The reference counts of the elements in elemListPtr are incremented
 *	since the list now refers to them. listPtr and elemListPtr are
 *	converted, if necessary, to list objects. Also, appending the new
 *	elements may cause listObj's array of element pointers to grow.
 *	listPtr's old string representation, if any, is invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjAppendList(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr,	/* List object to append elements to. */
    Tcl_Obj *elemListPtr)	/* List obj with elements to append. */
{
    int listLen, objc, result;
    Tcl_Obj **objv;

    if (Tcl_IsShared(listPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_ListObjAppendList");
    }

    result = Tcl_ListObjLength(interp, listPtr, &listLen);
    if (result != TCL_OK) {
	return result;
    }

    result = Tcl_ListObjGetElements(interp, elemListPtr, &objc, &objv);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Insert objc new elements starting after the lists's last element.
     * Delete zero existing elements.
     */

    return Tcl_ListObjReplace(interp, listPtr, listLen, 0, objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjAppendElement --
 *
 *	This function is a special purpose version of Tcl_ListObjAppendList:
 *	it appends a single object referenced by objPtr to the list object
 *	referenced by listPtr. If listPtr is not already a list object, an
 *	attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case objPtr is added to
 *	the end of listPtr's list. If listPtr does not refer to a list object
 *	and the object can not be converted to one, TCL_ERROR is returned and
 *	an error message will be left in the interpreter's result if interp is
 *	not NULL.
 *
 * Side effects:
 *	The ref count of objPtr is incremented since the list now refers to
 *	it. listPtr will be converted, if necessary, to a list object. Also,
 *	appending the new element may cause listObj's array of element
 *	pointers to grow. listPtr's old string representation, if any, is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjAppendElement(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    Tcl_Obj *listPtr,		/* List object to append objPtr to. */
    Tcl_Obj *objPtr)		/* Object to append to listPtr's list. */
{
    register List *listRepPtr;
    register Tcl_Obj **elemPtrs;
    int numElems, numRequired, newMax, newSize, i;

    if (Tcl_IsShared(listPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_ListObjAppendElement");
    }
    if (listPtr->typePtr != &tclListType) {
	int result, length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    Tcl_SetListObj(listPtr, 1, &objPtr);
	    return TCL_OK;
	}

	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    numElems = listRepPtr->elemCount;
    numRequired = numElems + 1 ;

    /*
     * If there is no room in the current array of element pointers, allocate
     * a new, larger array and copy the pointers to it. If the List struct is
     * shared, allocate a new one.
     */

    if (numRequired > listRepPtr->maxElemCount){
	newMax = (2 * numRequired);
	newSize = sizeof(List)+((newMax-1)*sizeof(Tcl_Obj*));
    } else {
	newMax = listRepPtr->maxElemCount;
	newSize = 0;
    }

    if (listRepPtr->refCount > 1) {
	List *oldListRepPtr = listRepPtr;
	Tcl_Obj **oldElems;

	listRepPtr = NewListIntRep(newMax, NULL);
	if (!listRepPtr) {
	    Tcl_Panic("Not enough memory to allocate list");
	}
	oldElems = &oldListRepPtr->elements;
	elemPtrs = &listRepPtr->elements;
	for (i=0; i<numElems; i++) {
	    elemPtrs[i] = oldElems[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
	listRepPtr->elemCount = numElems;
	listRepPtr->refCount++;
	oldListRepPtr->refCount--;
	listPtr->internalRep.twoPtrValue.ptr1 = (VOID *) listRepPtr;
    } else if (newSize) {
	listRepPtr = (List *) ckrealloc((char *)listRepPtr, (size_t)newSize);
	listRepPtr->maxElemCount = newMax;
	listPtr->internalRep.twoPtrValue.ptr1 = (VOID *) listRepPtr;
    }

    /*
     * Add objPtr to the end of listPtr's array of element pointers. Increment
     * the ref count for the (now shared) objPtr.
     */

    elemPtrs = &listRepPtr->elements;
    elemPtrs[numElems] = objPtr;
    Tcl_IncrRefCount(objPtr);
    listRepPtr->elemCount++;

    /*
     * Invalidate any old string representation since the list's internal
     * representation has changed.
     */

    Tcl_InvalidateStringRep(listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjIndex --
 *
 *	This function returns a pointer to the index'th object from the list
 *	referenced by listPtr. The first element has index 0. If index is
 *	negative or greater than or equal to the number of elements in the
 *	list, a NULL is returned. If listPtr is not a list object, an attempt
 *	will be made to convert it to a list.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case objPtrPtr is set to
 *	the Tcl_Obj pointer for the index'th list element or NULL if index is
 *	out of range. This object should be treated as readonly and its ref
 *	count is _not_ incremented; the caller must do that if it holds on to
 *	the reference. If listPtr does not refer to a list and can't be
 *	converted to one, TCL_ERROR is returned and an error message is left
 *	in the interpreter's result if interp is not NULL.
 *
 * Side effects:
 *	listPtr will be converted, if necessary, to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjIndex(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr,	/* List object to index into. */
    register int index,		/* Index of element to return. */
    Tcl_Obj **objPtrPtr)	/* The resulting Tcl_Obj* is stored here. */
{
    register List *listRepPtr;

    if (listPtr->typePtr != &tclListType) {
	int result, length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    *objPtrPtr = NULL;
	    return TCL_OK;
	}

	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    if ((index < 0) || (index >= listRepPtr->elemCount)) {
	*objPtrPtr = NULL;
    } else {
	*objPtrPtr = (&listRepPtr->elements)[index];
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjLength --
 *
 *	This function returns the number of elements in a list object. If the
 *	object is not already a list object, an attempt will be made to
 *	convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *intPtr will be set
 *	to the integer count of list elements. If listPtr does not refer to a
 *	list object and the object can not be converted to one, TCL_ERROR is
 *	returned and an error message will be left in the interpreter's result
 *	if interp is not NULL.
 *
 * Side effects:
 *	The possible conversion of the argument object to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjLength(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr,	/* List object whose #elements to return. */
    register int *intPtr)	/* The resulting int is stored here. */
{
    register List *listRepPtr;

    if (listPtr->typePtr != &tclListType) {
	int result, length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    *intPtr = 0;
	    return TCL_OK;
	}

	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    *intPtr = listRepPtr->elemCount;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjReplace --
 *
 *	This function replaces zero or more elements of the list referenced by
 *	listPtr with the objects from an (objc,objv) array. The objc elements
 *	of the array referenced by objv replace the count elements in listPtr
 *	starting at first.
 *
 *	If the argument first is zero or negative, it refers to the first
 *	element. If first is greater than or equal to the number of elements
 *	in the list, then no elements are deleted; the new elements are
 *	appended to the list. Count gives the number of elements to replace.
 *	If count is zero or negative then no elements are deleted; the new
 *	elements are simply inserted before first.
 *
 *	The argument objv refers to an array of objc pointers to the new
 *	elements to be added to listPtr in place of those that were deleted.
 *	If objv is NULL, no new elements are added. If listPtr is not a list
 *	object, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK. If listPtr does not refer to a
 *	list object and can not be converted to one, TCL_ERROR is returned and
 *	an error message will be left in the interpreter's result if interp is
 *	not NULL.
 *
 * Side effects:
 *	The ref counts of the objc elements in objv are incremented since the
 *	resulting list now refers to them. Similarly, the ref counts for
 *	replaced objects are decremented. listPtr is converted, if necessary,
 *	to a list object. listPtr's old string representation, if any, is
 *	freed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjReplace(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *listPtr,		/* List object whose elements to replace. */
    int first,			/* Index of first element to replace. */
    int count,			/* Number of elements to replace. */
    int objc,			/* Number of objects to insert. */
    Tcl_Obj *CONST objv[])	/* An array of objc pointers to Tcl objects to
				 * insert. */
{
    List *listRepPtr;
    register Tcl_Obj **elemPtrs;
    Tcl_Obj *victimPtr;
    int numElems, numRequired, numAfterLast;
    int start, shift, newMax, i, j, result;
    int isShared;

    if (Tcl_IsShared(listPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_ListObjReplace");
    }
    if (listPtr->typePtr != &tclListType) {
	int length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    if (objc) {
		Tcl_SetListObj(listPtr, objc, NULL);
	    } else {
		return TCL_OK;
	    }
	} else {
	    result = SetListFromAny(interp, listPtr);
	    if (result != TCL_OK) {
		return result;
	    }
	}
    }

    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    elemPtrs = &listRepPtr->elements;
    numElems = listRepPtr->elemCount;

    if (first < 0) {
    	first = 0;
    }
    if (first >= numElems) {
	first = numElems;	/* So we'll insert after last element. */
    }
    if (count < 0) {
	count = 0;
    } else if (numElems < first+count) {
	count = numElems - first;
    }

    isShared = (listRepPtr->refCount > 1);
    numRequired = (numElems - count + objc);

    if ((numRequired <= listRepPtr->maxElemCount)
	    && !isShared) {
	/*
	 * Can use the current List struct. First "delete" count elements
	 * starting at first.
	 */

	for (j = first;  j < first + count;  j++) {
	    victimPtr = elemPtrs[j];
	    TclDecrRefCount(victimPtr);
	}

	/*
	 * Shift the elements after the last one removed to their new
	 * locations.
	 */

	start = (first + count);
	numAfterLast = (numElems - start);
	shift = (objc - count);	/* numNewElems - numDeleted */
	if ((numAfterLast > 0) && (shift != 0)) {
	    Tcl_Obj **src, **dst;

	    src = elemPtrs + start; dst = src + shift;
	    memmove((VOID*) dst, (VOID*) src,
		    (size_t) (numAfterLast * sizeof(Tcl_Obj*)));
	}
    } else {
	/*
	 * Cannot use the current List struct - it is shared, too small, or
	 * both. Allocate a new struct and insert elements into it.
	 */

	List *oldListRepPtr = listRepPtr;
	Tcl_Obj **oldPtrs = elemPtrs;

	if (numRequired > listRepPtr->maxElemCount){
	    newMax = (2 * numRequired);
	} else {
	    newMax = listRepPtr->maxElemCount;
	}

	listRepPtr = NewListIntRep(newMax, NULL);
	if (!listRepPtr) {
	    Tcl_Panic("Not enough memory to allocate list");
	}

	listPtr->internalRep.twoPtrValue.ptr1 = (VOID *) listRepPtr;
	listRepPtr->refCount++;

	elemPtrs = &listRepPtr->elements;

	if (isShared) {
	    /*
	     * The old struct will remain in place; need new refCounts for the
	     * new List struct references. Copy over only the surviving
	     * elements.
	     */

	    for (i=0; i < first; i++) {
		elemPtrs[i] = oldPtrs[i];
		Tcl_IncrRefCount(elemPtrs[i]);
	    }
	    for (i= first + count, j = first + objc;
		    j < numRequired; i++, j++) {
		elemPtrs[j] = oldPtrs[i];
		Tcl_IncrRefCount(elemPtrs[j]);
	    }

	    oldListRepPtr->refCount--;
	} else {
	    /*
	     * The old struct will be removed; use its inherited refCounts.
	     */

	    if (first > 0) {
		memcpy((VOID *) elemPtrs, (VOID *) oldPtrs,
			(size_t) (first * sizeof(Tcl_Obj *)));
	    }

	    /*
	     * "Delete" count elements starting at first.
	     */

	    for (j = first;  j < first + count;  j++) {
		victimPtr = oldPtrs[j];
		TclDecrRefCount(victimPtr);
	    }

	    /*
	     * Copy the elements after the last one removed, shifted to their
	     * new locations.
	     */

	    start = (first + count);
	    numAfterLast = (numElems - start);
	    if (numAfterLast > 0) {
		memcpy((VOID *) &(elemPtrs[first + objc]),
			(VOID *) &(oldPtrs[start]),
			(size_t) (numAfterLast * sizeof(Tcl_Obj *)));
	    }

	    ckfree((char *) oldListRepPtr);
	}
    }

    /*
     * Insert the new elements into elemPtrs before "first".
     */

    for (i=0,j=first ; i<objc ; i++,j++) {
	elemPtrs[j] = objv[i];
	Tcl_IncrRefCount(objv[i]);
    }

    /*
     * Update the count of elements.
     */

    listRepPtr->elemCount = numRequired;

    /*
     * Invalidate and free any old string representation since it no longer
     * reflects the list's internal representation.
     */

    Tcl_InvalidateStringRep(listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLsetList --
 *
 *	Core of the 'lset' command when objc == 4. Objv[2] may be either a
 *	scalar index or a list of indices.
 *
 * Results:
 *	Returns the new value of the list variable, or NULL if an error
 *	occurs.
 *
 * Side effects:
 *	Surgery is performed on the list value to produce the result.
 *
 *	On entry, the reference count of the variable value does not reflect
 *	any references held on the stack. The first action of this function is
 *	to determine whether the object is shared, and to duplicate it if it
 *	is. The reference count of the duplicate is incremented. At this
 *	point, the reference count will be 1 for either case, so that the
 *	object will appear to be unshared.
 *
 *	If an error occurs, and the object has been duplicated, the reference
 *	count on the duplicate is decremented so that it is now 0: this
 *	dismisses any memory that was allocated by this function.
 *
 *	If no error occurs, the reference count of the original object is
 *	incremented if the object has not been duplicated, and nothing is done
 *	to a reference count of the duplicate. Now the reference count of an
 *	unduplicated object is 2 (the returned pointer, plus the one stored in
 *	the variable). The reference count of a duplicate object is 1,
 *	reflecting that the returned pointer is the only active reference.
 *	The caller is expected to store the returned value back in the
 *	variable and decrement its reference count. (INST_STORE_* does exactly
 *	this.)
 *
 *	Tcl_LsetFlat and related functions maintain a linked list of Tcl_Obj's
 *	whose string representations must be spoilt by threading via 'ptr2' of
 *	the two-pointer internal representation. On entry to Tcl_LsetList, the
 *	values of 'ptr2' are immaterial; on exit, the 'ptr2' field of any
 *	Tcl_Obj that has been modified is set to NULL.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclLsetList(
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *listPtr,		/* Pointer to the list being modified */
    Tcl_Obj *indexArgPtr,	/* Index or index-list arg to 'lset' */
    Tcl_Obj *valuePtr)		/* Value arg to 'lset' */
{
    int indexCount;		/* Number of indices in the index list */
    Tcl_Obj **indices;		/* Vector of indices in the index list*/
    Tcl_Obj *retValuePtr;	/* Pointer to the list to be returned */
    int index;			/* Current index in the list - discarded */
    int i;
    List *indexListRepPtr;

    /*
     * Determine whether the index arg designates a list or a single index.
     * We have to be careful about the order of the checks to avoid repeated
     * shimmering; see TIP #22 and #23 for details.
     */

    if (indexArgPtr->typePtr != &tclListType
	    && TclGetIntForIndex(NULL, indexArgPtr, 0, &index) == TCL_OK) {
	/*
	 * indexArgPtr designates a single index.
	 */

	return TclLsetFlat(interp, listPtr, 1, &indexArgPtr, valuePtr);

    } else if (Tcl_ListObjGetElements(NULL, indexArgPtr, &indexCount,
	    &indices) != TCL_OK) {
	/*
	 * indexArgPtr designates something that is neither an index nor a
	 * well formed list. Report the error via TclLsetFlat.
	 */

	return TclLsetFlat(interp, listPtr, 1, &indexArgPtr, valuePtr);
    }

    /*
     * At this point, we know that argPtr designates a well formed list, and
     * the 'else if' above has parsed it into indexCount and indices.
     * Increase the reference count of the internal rep of indexArgPtr, in
     * order to insure the validity of pointers even if indexArgPtr shimmers
     * to another type.
     */

    if (indexCount) {
	indexListRepPtr = (List *) indexArgPtr->internalRep.twoPtrValue.ptr1;
	indexListRepPtr->refCount++;
    } else {
	indexListRepPtr = NULL; /* avoid compiler warning*/
    }

    /*
     * Let TclLsetFlat handle the actual lset'ting.
     */

    retValuePtr = TclLsetFlat(interp, listPtr, indexCount, indices, valuePtr);

    /*
     * If we are the only users of indexListRepPtr, we free it before
     * returning.
     */

    if (indexCount) {
	if (--indexListRepPtr->refCount <= 0) {
	    for (i=0; i<indexCount; i++) {
		Tcl_DecrRefCount(indices[i]);
	    }
	    ckfree((char *) indexListRepPtr);
	}
    }
    return retValuePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLsetFlat --
 *
 *	Core of the 'lset' command when objc>=5. Objv[2], ... , objv[objc-2]
 *	contain scalar indices.
 *
 * Results:
 *	Returns the new value of the list variable, or NULL if an error
 *	occurs.
 *
 * Side effects:
 *	Surgery is performed on the list value to produce the result.
 *
 *	On entry, the reference count of the variable value does not reflect
 *	any references held on the stack. The first action of this function is
 *	to determine whether the object is shared, and to duplicate it if it
 *	is. The reference count of the duplicate is incremented. At this
 *	point, the reference count will be 1 for either case, so that the
 *	object will appear to be unshared.
 *
 *	If an error occurs, and the object has been duplicated, the reference
 *	count on the duplicate is decremented so that it is now 0: this
 *	dismisses any memory that was allocated by this function.
 *
 *	If no error occurs, the reference count of the original object is
 *	incremented if the object has not been duplicated, and nothing is done
 *	to a reference count of the duplicate. Now the reference count of an
 *	unduplicated object is 2 (the returned pointer, plus the one stored in
 *	the variable). The reference count of a duplicate object is 1,
 *	reflecting that the returned pointer is the only active reference. The
 *	caller is expected to store the returned value back in the variable
 *	and decrement its reference count. (INST_STORE_* does exactly this.)
 *
 *	Tcl_LsetList and related functions maintain a linked list of Tcl_Obj's
 *	whose string representations must be spoilt by threading via 'ptr2' of
 *	the two-pointer internal representation. On entry to Tcl_LsetList, the
 *	values of 'ptr2' are immaterial; on exit, the 'ptr2' field of any
 *	Tcl_Obj that has been modified is set to NULL.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclLsetFlat(
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *listPtr,		/* Pointer to the list being modified */
    int indexCount,		/* Number of index args */
    Tcl_Obj *CONST indexArray[],
				/* Index args */
    Tcl_Obj *valuePtr)		/* Value arg to 'lset' */
{
    int duplicated;		/* Flag == 1 if the obj has been duplicated, 0
				 * otherwise */
    Tcl_Obj *retValuePtr;	/* Pointer to the list to be returned */
    int elemCount;		/* Length of one sublist being changed */
    Tcl_Obj **elemPtrs;		/* Pointers to the elements of a sublist */
    Tcl_Obj *subListPtr;	/* Pointer to the current sublist */
    int index;			/* Index of the element to replace in the
				 * current sublist */
    Tcl_Obj *chainPtr;		/* Pointer to the enclosing list of the
				 * current sublist. */
    int result;			/* Status return from library calls */
    int i;

    /*
     * If there are no indices, then simply return the new value, counting the
     * returned pointer as a reference.
     */

    if (indexCount == 0) {
	Tcl_IncrRefCount(valuePtr);
	return valuePtr;
    }

    /*
     * If the list is shared, make a private copy. Duplicate the intrep to
     * insure that it is modifyable [Bug 1333036]. A plain Tcl_DuplicateObj
     * will just increase the intrep's refCount without upping the sublists'
     * refCount, so that their true shared status cannot be determined from
     * their refCount.
     */

    if (Tcl_IsShared(listPtr)) {
	duplicated = 1;
	if (listPtr->typePtr == &tclListType) {
	    result = Tcl_ListObjGetElements(interp, listPtr, &elemCount,
		    &elemPtrs);
	    listPtr = Tcl_NewListObj(elemCount, elemPtrs);
	} else {
	    listPtr = Tcl_DuplicateObj(listPtr);
	}
	Tcl_IncrRefCount(listPtr);
    } else {
	duplicated = 0;
    }

    /*
     * Anchor the linked list of Tcl_Obj's whose string reps must be
     * invalidated if the operation succeeds.
     */

    retValuePtr = listPtr;
    chainPtr = NULL;

    /*
     * Handle each index arg by diving into the appropriate sublist.
     */

    for (i=0 ; ; i++) {
	/*
	 * Take the sublist apart.
	 */

	result = Tcl_ListObjGetElements(interp, listPtr, &elemCount,
		&elemPtrs);
	if (result != TCL_OK) {
	    break;
	}
	if (elemCount == 0) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("list index out of range", -1));
	    result = TCL_ERROR;
	    break;
	}
	listPtr->internalRep.twoPtrValue.ptr2 = (VOID *) chainPtr;

	/*
	 * Determine the index of the requested element.
	 */

	result = TclGetIntForIndex(interp, indexArray[i], elemCount-1, &index);
	if (result != TCL_OK) {
	    break;
	}

	/*
	 * Check that the index is in range.
	 */

	if (index<0 || index>=elemCount) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("list index out of range", -1));
	    result = TCL_ERROR;
	    break;
	}

	/*
	 * Break the loop after extracting the innermost sublist
	 */

	if (i >= indexCount-1) {
	    result = TCL_OK;
	    break;
	}

	/*
	 * Extract the appropriate sublist, and make sure that it is unshared.
	 * If it is a list, duplicate the intrep to avoid [Bug 1333036], as
	 * per the previous comment.
	 */

	subListPtr = elemPtrs[index];
	if (Tcl_IsShared(subListPtr)) {
	    if (subListPtr->typePtr == &tclListType) {
		result = Tcl_ListObjGetElements(interp, subListPtr, &elemCount,
			&elemPtrs);
		subListPtr = Tcl_NewListObj(elemCount, elemPtrs);
	    } else {
		subListPtr = Tcl_DuplicateObj(subListPtr);
	    }
	    result = TclListObjSetElement(interp, listPtr, index, subListPtr);
	    if (result != TCL_OK) {
		/*
		 * We actually shouldn't be able to get here. If we do, it
		 * would result in leaking subListPtr, but everything's been
		 * validated already; the error exit from TclListObjSetElement
		 * should never happen.
		 */

		break;
	    }
	}

	/*
	 * Chain the current sublist onto the linked list of Tcl_Obj's whose
	 * string reps must be spoilt.
	 */

	chainPtr = listPtr;
	listPtr = subListPtr;
    }

    /*
     * Store the result in the list element.
     */

    if (result == TCL_OK) {
	result = TclListObjSetElement(interp, listPtr, index, valuePtr);
    }

    if (result == TCL_OK) {
	listPtr->internalRep.twoPtrValue.ptr2 = (void *) chainPtr;

	/*
	 * Spoil all the string reps.
	 */

	while (listPtr != NULL) {
	    subListPtr = (Tcl_Obj *) listPtr->internalRep.twoPtrValue.ptr2;
	    Tcl_InvalidateStringRep(listPtr);
	    listPtr->internalRep.twoPtrValue.ptr2 = NULL;
	    listPtr = subListPtr;
	}

	/*
	 * Return the new list if everything worked.
	 */

	if (!duplicated) {
	    Tcl_IncrRefCount(retValuePtr);
	}
	return retValuePtr;
    }

    /*
     * Clean up the one dangling reference otherwise.
     */

    if (duplicated) {
	Tcl_DecrRefCount(retValuePtr);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclListObjSetElement --
 *
 *	Set a single element of a list to a specified value
 *
 * Results:
 *	The return value is normally TCL_OK. If listPtr does not refer to a
 *	list object and cannot be converted to one, TCL_ERROR is returned and
 *	an error message will be left in the interpreter result if interp is
 *	not NULL. Similarly, if index designates an element outside the range
 *	[0..listLength-1], where listLength is the count of elements in the
 *	list object designated by listPtr, TCL_ERROR is returned and an error
 *	message is left in the interpreter result.
 *
 * Side effects:
 *	Tcl_Panic if listPtr designates a shared object. Otherwise, attempts
 *	to convert it to a list with a non-shared internal rep. Decrements the
 *	ref count of the object at the specified index within the list,
 *	replaces with the object designated by valuePtr, and increments the
 *	ref count of the replacement object.
 *
 *	It is the caller's responsibility to invalidate the string
 *	representation of the object.
 *
 *----------------------------------------------------------------------
 */

int
TclListObjSetElement(
    Tcl_Interp *interp,		/* Tcl interpreter; used for error reporting
				 * if not NULL */
    Tcl_Obj *listPtr,		/* List object in which element should be
				 * stored */
    int index,			/* Index of element to store */
    Tcl_Obj *valuePtr)		/* Tcl object to store in the designated list
				 * element */
{
    int result;			/* Return value from this function. */
    List *listRepPtr;		/* Internal representation of the list being
				 * modified. */
    Tcl_Obj **elemPtrs;		/* Pointers to elements of the list. */
    int elemCount;		/* Number of elements in the list. */
    int i;

    /*
     * Ensure that the listPtr parameter designates an unshared list.
     */

    if (Tcl_IsShared(listPtr)) {
	Tcl_Panic("%s called with shared object", "TclListObjSetElement");
    }
    if (listPtr->typePtr != &tclListType) {
	int length;

	(void) Tcl_GetStringFromObj(listPtr, &length);
	if (!length) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("list index out of range", -1));
	    return TCL_ERROR;
	}
	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List*) listPtr->internalRep.twoPtrValue.ptr1;
    elemCount = listRepPtr->elemCount;
    elemPtrs = &listRepPtr->elements;

    /*
     * Ensure that the index is in bounds.
     */

    if (index<0 || index>=elemCount) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("list index out of range", -1));
	    return TCL_ERROR;
	}
    }

    /*
     * If the internal rep is shared, replace it with an unshared copy.
     */

    if (listRepPtr->refCount > 1) {
	List *oldListRepPtr = listRepPtr;
	Tcl_Obj **oldElemPtrs = elemPtrs;

	listRepPtr = NewListIntRep(listRepPtr->maxElemCount, NULL);
	listRepPtr->canonicalFlag = oldListRepPtr->canonicalFlag;
	elemPtrs = &listRepPtr->elements;
	for (i=0; i < elemCount; i++) {
	    elemPtrs[i] = oldElemPtrs[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
	listRepPtr->refCount++;
	listRepPtr->elemCount = elemCount;
	listPtr->internalRep.twoPtrValue.ptr1 = (VOID *) listRepPtr;
	oldListRepPtr->refCount--;
    }

    /*
     * Add a reference to the new list element.
     */

    Tcl_IncrRefCount(valuePtr);

    /*
     * Remove a reference from the old list element.
     */

    Tcl_DecrRefCount(elemPtrs[index]);

    /*
     * Stash the new object in the list.
     */

    elemPtrs[index] = valuePtr;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeListInternalRep --
 *
 *	Deallocate the storage associated with a list object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees listPtr's List* internal representation and sets listPtr's
 *	internalRep.twoPtrValue.ptr1 to NULL. Decrements the ref counts of all
 *	element objects, which may free them.
 *
 *----------------------------------------------------------------------
 */

static void
FreeListInternalRep(
    Tcl_Obj *listPtr)		/* List object with internal rep to free. */
{
    register List *listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    register Tcl_Obj **elemPtrs = &listRepPtr->elements;
    register Tcl_Obj *objPtr;
    int numElems = listRepPtr->elemCount;
    int i;

    if (--listRepPtr->refCount <= 0) {
	for (i = 0;  i < numElems;  i++) {
	    objPtr = elemPtrs[i];
	    Tcl_DecrRefCount(objPtr);
	}
	ckfree((char *) listRepPtr);
    }

    listPtr->internalRep.twoPtrValue.ptr1 = NULL;
    listPtr->internalRep.twoPtrValue.ptr2 = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DupListInternalRep --
 *
 *	Initialize the internal representation of a list Tcl_Obj to share the
 *	internal representation of an existing list object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference count of the List internal rep is incremented.
 *
 *----------------------------------------------------------------------
 */

static void
DupListInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    List *listRepPtr = (List *) srcPtr->internalRep.twoPtrValue.ptr1;

    listRepPtr->refCount++;
    copyPtr->internalRep.twoPtrValue.ptr1 = (void *) listRepPtr;
    copyPtr->internalRep.twoPtrValue.ptr2 = NULL;
    copyPtr->typePtr = &tclListType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetListFromAny --
 *
 *	Attempt to generate a list internal form for the Tcl object "objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a list is stored as "objPtr"s internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetListFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr)		/* The object to convert. */
{
    char *string, *s;
    CONST char *elemStart, *nextElem;
    int lenRemain, length, estCount, elemSize, hasBrace, i, j, result;
    char *limit;		/* Points just after string's last byte. */
    register CONST char *p;
    register Tcl_Obj **elemPtrs;
    register Tcl_Obj *elemPtr;
    List *listRepPtr;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);

    /*
     * Parse the string into separate string objects, and create a List
     * structure that points to the element string objects. We use a modified
     * version of Tcl_SplitList's implementation to avoid one malloc and a
     * string copy for each list element. First, estimate the number of
     * elements by counting the number of space characters in the list.
     */

    limit = (string + length);
    estCount = 1;
    for (p = string;  p < limit;  p++) {
	if (isspace(UCHAR(*p))) { /* INTL: ISO space. */
	    estCount++;
	}
    }

    /*
     * Allocate a new List structure with enough room for "estCount" elements.
     * Each element is a pointer to a Tcl_Obj with the appropriate string rep.
     * The initial "estCount" elements are set using the corresponding "argv"
     * strings.
     */

    listRepPtr = NewListIntRep(estCount, NULL);
    if (!listRepPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"Not enough memory to allocate the list internal rep", -1));
	return TCL_ERROR;
    }
    elemPtrs = &listRepPtr->elements;

    for (p=string, lenRemain=length, i=0;
	    lenRemain > 0;
	    p = nextElem, lenRemain = (limit - nextElem), i++) {
	result = TclFindElement(interp, p, lenRemain, &elemStart, &nextElem,
		&elemSize, &hasBrace);
	if (result != TCL_OK) {
	    for (j = 0;  j < i;  j++) {
		elemPtr = elemPtrs[j];
		Tcl_DecrRefCount(elemPtr);
	    }
	    ckfree((char *) listRepPtr);
	    return result;
	}
	if (elemStart >= limit) {
	    break;
	}
	if (i > estCount) {
	    Tcl_Panic("SetListFromAny: bad size estimate for list");
	}

	/*
	 * Allocate a Tcl object for the element and initialize it from the
	 * "elemSize" bytes starting at "elemStart".
	 */

	s = ckalloc((unsigned) elemSize + 1);
	if (hasBrace) {
	    memcpy((void *) s, (void *) elemStart, (size_t) elemSize);
	    s[elemSize] = 0;
	} else {
	    elemSize = TclCopyAndCollapse(elemSize, elemStart, s);
	}

	TclNewObj(elemPtr);
	elemPtr->bytes = s;
	elemPtr->length = elemSize;
	elemPtrs[i] = elemPtr;
	Tcl_IncrRefCount(elemPtr);	/* since list now holds ref to it */
    }

    listRepPtr->elemCount = i;

    /*
     * Free the old internalRep before setting the new one. We do this as late
     * as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    listRepPtr->refCount++;
    TclFreeIntRep(objPtr);
    objPtr->internalRep.twoPtrValue.ptr1 = (void *) listRepPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &tclListType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfList --
 *
 *	Update the string representation for a list object. Note: This
 *	function does not invalidate an existing old string rep so storage
 *	will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from the
 *	list-to-string conversion. This string will be empty if the list has
 *	no elements. The list internal representation should not be NULL and
 *	we assume it is not NULL.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfList(
    Tcl_Obj *listPtr)		/* List object with string rep to update. */
{
#   define LOCAL_SIZE 20
    int localFlags[LOCAL_SIZE], *flagPtr;
    List *listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    int numElems = listRepPtr->elemCount;
    register int i;
    char *elem, *dst;
    int length;
    Tcl_Obj **elemPtrs;

    /*
     * Convert each element of the list to string form and then convert it to
     * proper list element form, adding it to the result buffer.
     */

    /*
     * Pass 1: estimate space, gather flags.
     */

    if (numElems <= LOCAL_SIZE) {
	flagPtr = localFlags;
    } else {
	flagPtr = (int *) ckalloc((unsigned) numElems*sizeof(int));
    }
    listPtr->length = 1;
    elemPtrs = &listRepPtr->elements;
    for (i = 0; i < numElems; i++) {
	elem = Tcl_GetStringFromObj(elemPtrs[i], &length);
	listPtr->length += Tcl_ScanCountedElement(elem, length,
		&flagPtr[i]) + 1;

	/*
	 * Check for continued sanity. [Bug 1267380]
	 */

	if (listPtr->length < 1) {
	    Tcl_Panic("string representation size exceeds sane bounds");
	}
    }

    /*
     * Pass 2: copy into string rep buffer.
     */

    listPtr->bytes = ckalloc((unsigned) listPtr->length);
    dst = listPtr->bytes;
    for (i = 0; i < numElems; i++) {
	elem = Tcl_GetStringFromObj(elemPtrs[i], &length);
	dst += Tcl_ConvertCountedElement(elem, length, dst,
		flagPtr[i] | (i==0 ? 0 : TCL_DONT_QUOTE_HASH));
	*dst = ' ';
	dst++;
    }
    if (flagPtr != localFlags) {
	ckfree((char *) flagPtr);
    }
    if (dst == listPtr->bytes) {
	*dst = 0;
    } else {
	dst--;
	*dst = 0;
    }
    listPtr->length = dst - listPtr->bytes;

    /*
     * Mark the list as being canonical; although it has a string rep, it is
     * one we derived through proper "canonical" quoting and so it's known to
     * be free from nasties relating to [concat] and [eval].
     */

    listRepPtr->canonicalFlag = 1;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
