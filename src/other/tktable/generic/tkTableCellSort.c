/* 
 * tkTableCell.c --
 *
 *	This module implements cell sort functions for table
 *	widgets.  The MergeSort algorithm and other aux sorting
 *	functions were taken from tclCmdIL.c lsort command:

 * tclCmdIL.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	I through L.  It contains only commands in the generic core
 *	(i.e. those that don't depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.

 *
 * Copyright (c) 1998-2002 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tkTable.h"

#ifndef UCHAR
#define UCHAR(c) ((unsigned char) (c))
#endif

/*
 * During execution of the "lsort" command, structures of the following
 * type are used to arrange the objects being sorted into a collection
 * of linked lists.
 */

typedef struct SortElement {
    Tcl_Obj *objPtr;			/* Object being sorted. */
    struct SortElement *nextPtr;        /* Next element in the list, or
					 * NULL for end of list. */
} SortElement;

static int		TableSortCompareProc _ANSI_ARGS_((CONST VOID *first,
							  CONST VOID *second));
static SortElement *    MergeSort _ANSI_ARGS_((SortElement *headPt));
static SortElement *    MergeLists _ANSI_ARGS_((SortElement *leftPtr,
						SortElement *rightPtr));
static int		DictionaryCompare _ANSI_ARGS_((char *left,
						       char *right));

/*
 *----------------------------------------------------------------------
 *
 * TableSortCompareProc --
 *	This procedure is invoked by qsort to determine the proper
 *	ordering between two elements.
 *
 * Results:
 *	< 0 means first is "smaller" than "second", > 0 means "first"
 *	is larger than "second", and 0 means they should be treated
 *	as equal.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */
static int
TableSortCompareProc(first, second)
    CONST VOID *first, *second;		/* Elements to be compared. */
{
    char *str1 = *((char **) first);
    char *str2 = *((char **) second);

    return DictionaryCompare(str1, str2);
}

/*
 *----------------------------------------------------------------------
 *
 * TableCellSort --
 *	Sort a list of table cell elements (of form row,col)
 *
 * Results:
 *	Returns the sorted list of elements.  Because Tcl_Merge allocs
 *	the space for result, it must later be Tcl_Free'd by caller.
 *
 * Side effects:
 *	Behaviour undefined for ill-formed input list of elements.
 *
 *----------------------------------------------------------------------
 */
char *
TableCellSort(Table *tablePtr, char *str)
{
    int listArgc;
    CONST84 char **listArgv;
    char *result;

    if (Tcl_SplitList(tablePtr->interp, str, &listArgc, &listArgv) != TCL_OK) {
	return str;
    }
    /* Thread safety: qsort is reportedly not thread-safe... */
    qsort((VOID *) listArgv, (size_t) listArgc, sizeof (char *),
	  TableSortCompareProc);
    result = Tcl_Merge(listArgc, listArgv);
    ckfree((char *) listArgv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DictionaryCompare - Not the Unicode version
 *
 *	This function compares two strings as if they were being used in
 *	an index or card catalog.  The case of alphabetic characters is
 *	ignored, except to break ties.  Thus "B" comes before "b" but
 *	after "a".  Also, integers embedded in the strings compare in
 *	numerical order.  In other words, "x10y" comes after "x9y", not
 *      before it as it would when using strcmp().
 *
 * Results:
 *      A negative result means that the first element comes before the
 *      second, and a positive result means that the second element
 *      should come first.  A result of zero means the two elements
 *      are equal and it doesn't matter which comes first.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DictionaryCompare(left, right)
    char *left, *right;          /* The strings to compare */
{
    int diff, zeros;
    int secondaryDiff = 0;

    while (1) {
	if (isdigit(UCHAR(*right)) && isdigit(UCHAR(*left))) {
	    /*
	     * There are decimal numbers embedded in the two
	     * strings.  Compare them as numbers, rather than
	     * strings.  If one number has more leading zeros than
	     * the other, the number with more leading zeros sorts
	     * later, but only as a secondary choice.
	     */

	    zeros = 0;
	    while ((*right == '0') && (isdigit(UCHAR(right[1])))) {
		right++;
		zeros--;
	    }
	    while ((*left == '0') && (isdigit(UCHAR(left[1])))) {
		left++;
		zeros++;
	    }
	    if (secondaryDiff == 0) {
		secondaryDiff = zeros;
	    }

	    /*
	     * The code below compares the numbers in the two
	     * strings without ever converting them to integers.  It
	     * does this by first comparing the lengths of the
	     * numbers and then comparing the digit values.
	     */

	    diff = 0;
	    while (1) {
		if (diff == 0) {
		    diff = UCHAR(*left) - UCHAR(*right);
		}
		right++;
		left++;
		if (!isdigit(UCHAR(*right))) {
		    if (isdigit(UCHAR(*left))) {
			return 1;
		    } else {
			/*
			 * The two numbers have the same length. See
			 * if their values are different.
			 */

			if (diff != 0) {
			    return diff;
			}
			break;
		    }
		} else if (!isdigit(UCHAR(*left))) {
		    return -1;
		}
	    }
	    continue;
	}
        diff = UCHAR(*left) - UCHAR(*right);
        if (diff) {
            if (isupper(UCHAR(*left)) && islower(UCHAR(*right))) {
                diff = UCHAR(tolower(*left)) - UCHAR(*right);
                if (diff) {
		    return diff;
                } else if (secondaryDiff == 0) {
		    secondaryDiff = -1;
                }
            } else if (isupper(UCHAR(*right)) && islower(UCHAR(*left))) {
                diff = UCHAR(*left) - UCHAR(tolower(UCHAR(*right)));
                if (diff) {
		    return diff;
                } else if (secondaryDiff == 0) {
		    secondaryDiff = 1;
                }
            } else {
                return diff;
            }
        }
        if (*left == 0) {
	    break;
	}
        left++;
        right++;
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeLists -
 *
 *	This procedure combines two sorted lists of SortElement structures
 *	into a single sorted list.
 *
 * Results:
 *      The unified list of SortElement structures.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeLists(leftPtr, rightPtr)
    SortElement *leftPtr;               /* First list to be merged; may be
					 * NULL. */
    SortElement *rightPtr;              /* Second list to be merged; may be
					 * NULL. */
{
    SortElement *headPtr;
    SortElement *tailPtr;

    if (leftPtr == NULL) {
        return rightPtr;
    }
    if (rightPtr == NULL) {
        return leftPtr;
    }
    if (DictionaryCompare(Tcl_GetString(leftPtr->objPtr),
			  Tcl_GetString(rightPtr->objPtr)) > 0) {
	tailPtr = rightPtr;
	rightPtr = rightPtr->nextPtr;
    } else {
	tailPtr = leftPtr;
	leftPtr = leftPtr->nextPtr;
    }
    headPtr = tailPtr;
    while ((leftPtr != NULL) && (rightPtr != NULL)) {
	if (DictionaryCompare(Tcl_GetString(leftPtr->objPtr),
			      Tcl_GetString(rightPtr->objPtr)) > 0) {
	    tailPtr->nextPtr = rightPtr;
	    tailPtr = rightPtr;
	    rightPtr = rightPtr->nextPtr;
	} else {
	    tailPtr->nextPtr = leftPtr;
	    tailPtr = leftPtr;
	    leftPtr = leftPtr->nextPtr;
	}
    }
    if (leftPtr != NULL) {
       tailPtr->nextPtr = leftPtr;
    } else {
       tailPtr->nextPtr = rightPtr;
    }
    return headPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeSort -
 *
 *	This procedure sorts a linked list of SortElement structures
 *	use the merge-sort algorithm.
 *
 * Results:
 *      A pointer to the head of the list after sorting is returned.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeSort(headPtr)
    SortElement *headPtr;               /* First element on the list */
{
    /*
     * The subList array below holds pointers to temporary lists built
     * during the merge sort.  Element i of the array holds a list of
     * length 2**i.
     */

#   define NUM_LISTS 30
    SortElement *subList[NUM_LISTS];
    SortElement *elementPtr;
    int i;

    for(i = 0; i < NUM_LISTS; i++){
        subList[i] = NULL;
    }
    while (headPtr != NULL) {
	elementPtr = headPtr;
	headPtr = headPtr->nextPtr;
	elementPtr->nextPtr = 0;
	for (i = 0; (i < NUM_LISTS) && (subList[i] != NULL); i++){
	    elementPtr = MergeLists(subList[i], elementPtr);
	    subList[i] = NULL;
	}
	if (i >= NUM_LISTS) {
	    i = NUM_LISTS-1;
	}
	subList[i] = elementPtr;
    }
    elementPtr = NULL;
    for (i = 0; i < NUM_LISTS; i++){
        elementPtr = MergeLists(subList[i], elementPtr);
    }
    return elementPtr;
}

#ifndef NO_SORT_CELLS
/*
 *----------------------------------------------------------------------
 *
 * TableCellSortObj --
 *	Sorts a list of table cell elements (of form row,col) in place
 *
 * Results:
 *	Sorts list of elements in place.
 *
 * Side effects:
 *	Behaviour undefined for ill-formed input list of elements.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TableCellSortObj(Tcl_Interp *interp, Tcl_Obj *listObjPtr)
{
    int length, i;
    Tcl_Obj *sortedObjPtr, **listObjPtrs;
    SortElement *elementArray;
    SortElement *elementPtr;        

    if (Tcl_ListObjGetElements(interp, listObjPtr,
			       &length, &listObjPtrs) != TCL_OK) {
	return NULL;
    }
    if (length <= 0) {
	return listObjPtr;
    }

    elementArray = (SortElement *) ckalloc(length * sizeof(SortElement));
    for (i=0; i < length; i++){
	elementArray[i].objPtr = listObjPtrs[i];
	elementArray[i].nextPtr = &elementArray[i+1];
    }
    elementArray[length-1].nextPtr = NULL;
    elementPtr = MergeSort(elementArray);
    sortedObjPtr = Tcl_NewObj();
    for (; elementPtr != NULL; elementPtr = elementPtr->nextPtr){
	Tcl_ListObjAppendElement(NULL, sortedObjPtr, elementPtr->objPtr);
    }
    ckfree((char*) elementArray);

    return sortedObjPtr;
}
#endif
