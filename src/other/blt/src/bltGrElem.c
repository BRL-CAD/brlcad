
/*
 * bltGrElem.c --
 *
 *	This module implements generic elements for the BLT graph widget.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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

#include "bltGraph.h"
#include "bltChain.h"
#include <X11/Xutil.h>


static Tk_OptionParseProc StringToData;
static Tk_OptionPrintProc DataToString;
static Tk_OptionParseProc StringToDataPairs;
static Tk_OptionPrintProc DataPairsToString;
static Tk_OptionParseProc StringToAlong;
static Tk_OptionPrintProc AlongToString;
static Tk_CustomOption alongOption =
{
    StringToAlong, AlongToString, (ClientData)0
};
Tk_CustomOption bltDataOption =
{
    StringToData, DataToString, (ClientData)0
};
Tk_CustomOption bltDataPairsOption =
{
    StringToDataPairs, DataPairsToString, (ClientData)0
};
extern Tk_CustomOption bltDistanceOption;


static int counter;

#include "bltGrElem.h"

extern Element *Blt_BarElement();
extern Element *Blt_LineElement();

static Blt_VectorChangedProc VectorChangedProc;

EXTERN int Blt_VectorExists2 _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));

/*
 * ----------------------------------------------------------------------
 * Custom option parse and print procedures
 * ----------------------------------------------------------------------
 */
static int
GetPenStyle(graphPtr, string, type, stylePtr)
    Graph *graphPtr;
    char *string;
    Blt_Uid type;
    PenStyle *stylePtr;
{
    Pen *penPtr;
    Tcl_Interp *interp = graphPtr->interp;
    char **elemArr;
    int nElem;

    elemArr = NULL;
    if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((nElem != 1) && (nElem != 3)) {
	Tcl_AppendResult(interp, "bad style \"", string, "\": should be ", 
		 "\"penName\" or \"penName min max\"", (char *)NULL);
	if (elemArr != NULL) {
	    Blt_Free(elemArr);
	}
	return TCL_ERROR;
    }
    if (Blt_GetPen(graphPtr, elemArr[0], type, &penPtr) != TCL_OK) {
	Blt_Free(elemArr);
	return TCL_ERROR;
    }
    if (nElem == 3) {
	double min, max;

	if ((Tcl_GetDouble(interp, elemArr[1], &min) != TCL_OK) ||
	    (Tcl_GetDouble(interp, elemArr[2], &max) != TCL_OK)) {
	    Blt_Free(elemArr);
	    return TCL_ERROR;
	}
	SetWeight(stylePtr->weight, min, max);
    }
    stylePtr->penPtr = penPtr;
    Blt_Free(elemArr);
    return TCL_OK;
}


static void
SyncElemVector(vPtr)
    ElemVector *vPtr;
{
    vPtr->nValues = Blt_VecLength(vPtr->vecPtr);
    vPtr->valueArr = Blt_VecData(vPtr->vecPtr);
    vPtr->min = Blt_VecMin(vPtr->vecPtr);
    vPtr->max = Blt_VecMax(vPtr->vecPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FindRange --
 *
 *	Find the minimum, positive minimum, and maximum values in a
 *	given vector and store the results in the vector structure.
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Minimum, positive minimum, and maximum values are stored in
 *	the vector.
 *
 *----------------------------------------------------------------------
 */
static void
FindRange(vPtr)
    ElemVector *vPtr;
{
    register int i;
    register double *x;
    register double min, max;

    if ((vPtr->nValues < 1) || (vPtr->valueArr == NULL)) {
	return;			/* This shouldn't ever happen. */
    }
    x = vPtr->valueArr;

    min = DBL_MAX, max = -DBL_MAX;
    for(i = 0; i < vPtr->nValues; i++) {
	if (FINITE(x[i])) {
	    min = max = x[i];
	    break;
	}
    }
    /*  Initialize values to track the vector range */
    for (/* empty */; i < vPtr->nValues; i++) {
	if (FINITE(x[i])) {
	    if (x[i] < min) {
		min = x[i];
	    } else if (x[i] > max) {
		max = x[i];
	    }
	}
    }
    vPtr->min = min, vPtr->max = max;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FindElemVectorMinimum --
 *
 *	Find the minimum, positive minimum, and maximum values in a
 *	given vector and store the results in the vector structure.
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Minimum, positive minimum, and maximum values are stored in
 *	the vector.
 *
 *----------------------------------------------------------------------
 */
double
Blt_FindElemVectorMinimum(vPtr, minLimit)
    ElemVector *vPtr;
    double minLimit;
{
    register int i;
    register double *arr;
    register double min, x;

    min = DBL_MAX;
    arr = vPtr->valueArr;
    for (i = 0; i < vPtr->nValues; i++) {
	x = arr[i];
	if (x < 0.0) {
	    /* What do you do about negative values when using log
	     * scale values seems like a grey area.  Mirror. */
	    x = -x;
	}
	if ((x > minLimit) && (min > x)) {
	    min = x;
	}
    }
    if (min == DBL_MAX) {
	min = minLimit;
    }
    return min;
}

static void
FreeDataVector(vPtr)
    ElemVector *vPtr;
{
    if (vPtr->clientId != NULL) {
	Blt_FreeVectorId(vPtr->clientId);	/* Free the old vector */
	vPtr->clientId = NULL;
    } else if (vPtr->valueArr != NULL) {
	Blt_Free(vPtr->valueArr);
    }
    vPtr->valueArr = NULL;
    vPtr->nValues = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorChangedProc --
 *
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Graph is redrawn.
 *
 *----------------------------------------------------------------------
 */
static void
VectorChangedProc(interp, clientData, notify)
    Tcl_Interp *interp;
    ClientData clientData;
    Blt_VectorNotify notify;
{
    ElemVector *vPtr = clientData;
    Element *elemPtr = vPtr->elemPtr;
    Graph *graphPtr = elemPtr->graphPtr;

    switch (notify) {
    case BLT_VECTOR_NOTIFY_DESTROY:
	vPtr->clientId = NULL;
	vPtr->valueArr = NULL;
	vPtr->nValues = 0;
	break;

    case BLT_VECTOR_NOTIFY_UPDATE:
    default:
	Blt_GetVectorById(interp, vPtr->clientId, &vPtr->vecPtr);
	SyncElemVector(vPtr);
	break;
    }
    graphPtr->flags |= RESET_AXES;
    elemPtr->flags |= MAP_ITEM;
    if (!elemPtr->hidden) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
	Blt_EventuallyRedrawGraph(graphPtr);
    }
}

static int
EvalExprList(interp, list, nElemPtr, arrayPtr)
    Tcl_Interp *interp;
    char *list;
    int *nElemPtr;
    double **arrayPtr;
{
    int nElem;
    char **elemArr;
    double *array;
    int result;

    result = TCL_ERROR;
    elemArr = NULL;
    if (Tcl_SplitList(interp, list, &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    array = NULL;
    if (nElem > 0) {
	register double *valuePtr;
	register int i;

	counter++;
	array = Blt_Malloc(sizeof(double) * nElem);
	if (array == NULL) {
	    Tcl_AppendResult(interp, "can't allocate new vector", (char *)NULL);
	    goto badList;
	}
	valuePtr = array;
	for (i = 0; i < nElem; i++) {
	    if (Tcl_ExprDouble(interp, elemArr[i], valuePtr) != TCL_OK) {
		goto badList;
	    }
	    valuePtr++;
	}
    }
    result = TCL_OK;

  badList:
    Blt_Free(elemArr);
    *arrayPtr = array;
    *nElemPtr = nElem;
    if (result != TCL_OK) {
	Blt_Free(array);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToData --
 *
 *	Given a Tcl list of numeric expression representing the element
 *	values, convert into an array of double precision values. In
 *	addition, the minimum and maximum values are saved.  Since
 *	elastic values are allow (values which translate to the
 *	min/max of the graph), we must try to get the non-elastic
 *	minimum and maximum.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vector is passed
 *	back via the vPtr.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToData(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Type of axis vector to fill */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Tcl list of expressions */
    char *widgRec;		/* Element record */
    int offset;			/* Offset of vector in Element record */
{
    Element *elemPtr = (Element *)(widgRec);
    ElemVector *vPtr = (ElemVector *)(widgRec + offset);

    FreeDataVector(vPtr);
    if (Blt_VectorExists2(interp, string)) {
	Blt_VectorId clientId;

	clientId = Blt_AllocVectorId(interp, string);
	if (Blt_GetVectorById(interp, clientId, &vPtr->vecPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_SetVectorChangedProc(clientId, VectorChangedProc, vPtr);
	vPtr->elemPtr = elemPtr;
	vPtr->clientId = clientId;
	SyncElemVector(vPtr);
	elemPtr->flags |= MAP_ITEM;
    } else {
	double *newArr;
	int nValues;

	if (EvalExprList(interp, string, &nValues, &newArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nValues > 0) {
	    vPtr->valueArr = newArr;
	}
	vPtr->nValues = nValues;
	FindRange(vPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DataToString --
 *
 *	Convert the vector of floating point values into a Tcl list.
 *
 * Results:
 *	The string representation of the vector is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DataToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Type of axis vector to print */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Element record */
    int offset;			/* Offset of vector in Element record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    ElemVector *vPtr = (ElemVector *)(widgRec + offset);
    Element *elemPtr = (Element *)(widgRec);
    Tcl_DString dString;
    char *result;
    char string[TCL_DOUBLE_SPACE + 1];
    double *p, *endPtr; 

    if (vPtr->clientId != NULL) {
	return Blt_NameOfVectorId(vPtr->clientId);
    }
    if (vPtr->nValues == 0) {
	return "";
    }
    Tcl_DStringInit(&dString);
    endPtr = vPtr->valueArr + vPtr->nValues;
    for (p = vPtr->valueArr; p < endPtr; p++) {
	Tcl_PrintDouble(elemPtr->graphPtr->interp, *p, string);
	Tcl_DStringAppendElement(&dString, string);
    }
    result = Tcl_DStringValue(&dString);

    /*
     * If memory wasn't allocated for the dynamic string, do it here (it's
     * currently on the stack), so that Tcl can free it normally.
     */
    if (result == dString.staticSpace) {
	result = Blt_Strdup(result);
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToDataPairs --
 *
 *	This procedure is like StringToData except that it interprets
 *	the list of numeric expressions as X Y coordinate pairs.  The
 *	minimum and maximum for both the X and Y vectors are
 *	determined.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vectors are
 *	passed back via the widget record (elemPtr).
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToDataPairs(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Tcl list of numeric expressions */
    char *widgRec;		/* Element record */
    int offset;			/* Not used. */
{
    Element *elemPtr = (Element *)widgRec;
    int nElem;
    unsigned int newSize;
    double *newArr;

    if (EvalExprList(interp, string, &nElem, &newArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nElem & 1) {
	Tcl_AppendResult(interp, "odd number of data points", (char *)NULL);
	Blt_Free(newArr);
	return TCL_ERROR;
    }
    nElem /= 2;
    newSize = nElem * sizeof(double);

    FreeDataVector(&elemPtr->x);
    FreeDataVector(&elemPtr->y);

    elemPtr->x.valueArr = Blt_Malloc(newSize);
    elemPtr->y.valueArr = Blt_Malloc(newSize);
    assert(elemPtr->x.valueArr && elemPtr->y.valueArr);
    elemPtr->x.nValues = elemPtr->y.nValues = nElem;

    if (newSize > 0) {
	register double *dataPtr;
	register int i;

	for (dataPtr = newArr, i = 0; i < nElem; i++) {
	    elemPtr->x.valueArr[i] = *dataPtr++;
	    elemPtr->y.valueArr[i] = *dataPtr++;
	}
	Blt_Free(newArr);
	FindRange(&elemPtr->x);
	FindRange(&elemPtr->y);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DataPairsToString --
 *
 *	Convert pairs of floating point values in the X and Y arrays
 *	into a Tcl list.
 *
 * Results:
 *	The return value is a string (Tcl list).
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DataPairsToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Element information record */
    int offset;			/* Not used. */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Element *elemPtr = (Element *)widgRec;
    Tcl_Interp *interp = elemPtr->graphPtr->interp;
    int i;
    int length;
    char *result;
    char string[TCL_DOUBLE_SPACE + 1];
    Tcl_DString dString;

    length = NumberOfPoints(elemPtr);
    if (length < 1) {
	return "";
    }
    Tcl_DStringInit(&dString);
    for (i = 0; i < length; i++) {
	Tcl_PrintDouble(interp, elemPtr->x.valueArr[i], string);
	Tcl_DStringAppendElement(&dString, string);
	Tcl_PrintDouble(interp, elemPtr->y.valueArr[i], string);
	Tcl_DStringAppendElement(&dString, string);
    }
    result = Tcl_DStringValue(&dString);

    /*
     * If memory wasn't allocated for the dynamic string, do it here
     * (it's currently on the stack), so that Tcl can free it
     * normally.
     */
    if (result == dString.staticSpace) {
	result = Blt_Strdup(result);
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToAlong --
 *
 *	Given a Tcl list of numeric expression representing the element
 *	values, convert into an array of double precision values. In
 *	addition, the minimum and maximum values are saved.  Since
 *	elastic values are allow (values which translate to the
 *	min/max of the graph), we must try to get the non-elastic
 *	minimum and maximum.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vector is passed
 *	back via the vPtr.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToAlong(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representation of value. */
    char *widgRec;		/* Widget record. */
    int offset;			/* Offset of field in widget record. */
{
    int *intPtr = (int *)(widgRec + offset);

    if ((string[0] == 'x') && (string[1] == '\0')) {
	*intPtr = SEARCH_X;
    } else if ((string[0] == 'y') && (string[1] == '\0')) { 
	*intPtr = SEARCH_Y;
    } else if ((string[0] == 'b') && (strcmp(string, "both") == 0)) {
	*intPtr = SEARCH_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad along value \"", string, "\"",
			 (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AlongToString --
 *
 *	Convert the vector of floating point values into a Tcl list.
 *
 * Results:
 *	The string representation of the vector is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
AlongToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    int along = *(int *)(widgRec + offset);

    switch (along) {
    case SEARCH_X:
	return "x";
    case SEARCH_Y:
	return "y";
    case SEARCH_BOTH:
	return "both";
    default:
	return "unknown along value";
    }
}

void
Blt_FreePalette(graphPtr, palette)
    Graph *graphPtr;
    Blt_Chain *palette;
{
    Blt_ChainLink *linkPtr;

    /* Skip the first slot. It contains the built-in "normal" pen of
     * the element.  */
    linkPtr = Blt_ChainFirstLink(palette);
    if (linkPtr != NULL) {
	register PenStyle *stylePtr;
	Blt_ChainLink *nextPtr;

	for (linkPtr = Blt_ChainNextLink(linkPtr); linkPtr != NULL; 
	     linkPtr = nextPtr) {
	    nextPtr =  Blt_ChainNextLink(linkPtr);
	    stylePtr = Blt_ChainGetValue(linkPtr);
	    Blt_FreePen(graphPtr, stylePtr->penPtr);
	    Blt_ChainDeleteLink(palette, linkPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_StringToStyles --
 *
 *	Parse the list of style names.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_StringToStyles(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representing style list */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
{
    Blt_Chain *palette = *(Blt_Chain **)(widgRec + offset);
    Blt_ChainLink *linkPtr;
    Element *elemPtr = (Element *)(widgRec);
    PenStyle *stylePtr;
    char **elemArr;
    int nStyles;
    register int i;
    size_t size = (size_t)clientData;

    elemArr = NULL;
    Blt_FreePalette(elemPtr->graphPtr, palette);
    if ((string == NULL) || (*string == '\0')) {
	nStyles = 0;
    } else if (Tcl_SplitList(interp, string, &nStyles, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Reserve the first entry for the "normal" pen. We'll set the
     * style later */
    linkPtr = Blt_ChainFirstLink(palette);
    if (linkPtr == NULL) {
	linkPtr = Blt_ChainAllocLink(size);
	Blt_ChainLinkBefore(palette, linkPtr, NULL);
    }
    stylePtr = Blt_ChainGetValue(linkPtr);
    stylePtr->penPtr = elemPtr->normalPenPtr;

    for (i = 0; i < nStyles; i++) {
	linkPtr = Blt_ChainAllocLink(size);
	stylePtr = Blt_ChainGetValue(linkPtr);
	stylePtr->weight.min = (double)i;
	stylePtr->weight.max = (double)i + 1.0;
	stylePtr->weight.range = 1.0;
	if (GetPenStyle(elemPtr->graphPtr, elemArr[i], elemPtr->classUid,
	    (PenStyle *)stylePtr) != TCL_OK) {
	    Blt_Free(elemArr);
	    Blt_FreePalette(elemPtr->graphPtr, palette);
	    return TCL_ERROR;
	}
	Blt_ChainLinkBefore(palette, linkPtr, NULL);
    }
    if (elemArr != NULL) {
	Blt_Free(elemArr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_StylesToString --
 *
 *	Convert the style information into a string.
 *
 * Results:
 *	The string representing the style information is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
char *
Blt_StylesToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Element information record */
    int offset;			/* Not used. */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Blt_Chain *palette = *(Blt_Chain **)(widgRec + offset);
    Tcl_DString dString;
    char *result;
    Blt_ChainLink *linkPtr;

    Tcl_DStringInit(&dString);
    linkPtr = Blt_ChainFirstLink(palette);
    if (linkPtr != NULL) {
	Element *elemPtr = (Element *)(widgRec);
	char string[TCL_DOUBLE_SPACE];
	Tcl_Interp *interp;
	PenStyle *stylePtr;

	interp = elemPtr->graphPtr->interp;
	for (linkPtr = Blt_ChainNextLink(linkPtr); linkPtr != NULL;
	     linkPtr = Blt_ChainNextLink(linkPtr)) {
	    stylePtr = Blt_ChainGetValue(linkPtr);
	    Tcl_DStringStartSublist(&dString);
	    Tcl_DStringAppendElement(&dString, stylePtr->penPtr->name);
	    Tcl_PrintDouble(interp, stylePtr->weight.min, string);
	    Tcl_DStringAppendElement(&dString, string);
	    Tcl_PrintDouble(interp, stylePtr->weight.max, string);
	    Tcl_DStringAppendElement(&dString, string);
	    Tcl_DStringEndSublist(&dString);
	}
    }
    result = Blt_Strdup(Tcl_DStringValue(&dString));
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_StyleMap --
 *
 *	Creates an array of style indices and fills it based on the weight
 *	of each data point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed and allocated for the index array.
 *
 *----------------------------------------------------------------------
 */

PenStyle **
Blt_StyleMap(elemPtr)
    Element *elemPtr;
{
    register int i;
    int nWeights;		/* Number of weights to be examined.
				 * If there are more data points than
				 * weights, they will default to the
				 * normal pen. */

    PenStyle **dataToStyle;	/* Directory of styles.  Each array
				 * element represents the style for
				 * the data point at that index */
    Blt_ChainLink *linkPtr;
    PenStyle *stylePtr;
    double *w;			/* Weight vector */
    int nPoints;

    nPoints = NumberOfPoints(elemPtr);
    nWeights = MIN(elemPtr->w.nValues, nPoints);
    w = elemPtr->w.valueArr;
    linkPtr = Blt_ChainFirstLink(elemPtr->palette);
    stylePtr = Blt_ChainGetValue(linkPtr);

    /* 
     * Create a style mapping array (data point index to style), 
     * initialized to the default style.
     */
    dataToStyle = Blt_Malloc(nPoints * sizeof(PenStyle *));
    assert(dataToStyle);
    for (i = 0; i < nPoints; i++) {
	dataToStyle[i] = stylePtr;
    }

    for (i = 0; i < nWeights; i++) {
	for (linkPtr = Blt_ChainLastLink(elemPtr->palette); linkPtr != NULL;
	     linkPtr = Blt_ChainPrevLink(linkPtr)) {
	    stylePtr = Blt_ChainGetValue(linkPtr);

	    if (stylePtr->weight.range > 0.0) {
		double norm;

		norm = (w[i] - stylePtr->weight.min) / stylePtr->weight.range;
		if (((norm - 1.0) <= DBL_EPSILON) && 
		    (((1.0 - norm) - 1.0) <= DBL_EPSILON)) {
		    dataToStyle[i] = stylePtr;
		    break;		/* Done: found range that matches. */
		}
	    }
	}
    }
    return dataToStyle;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_MapErrorBars --
 *
 *	Creates two arrays of points and pen indices, filled with
 *	the screen coordinates of the visible
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed and allocated for the index array.
 *
 *----------------------------------------------------------------------
 */
void
Blt_MapErrorBars(graphPtr, elemPtr, dataToStyle)
    Graph *graphPtr;
    Element *elemPtr;
    PenStyle **dataToStyle;
{
    int n, nPoints;
    Extents2D exts;
    PenStyle *stylePtr;

    Blt_GraphExtents(graphPtr, &exts);
    nPoints = NumberOfPoints(elemPtr);
    if (elemPtr->xError.nValues > 0) {
	n = MIN(elemPtr->xError.nValues, nPoints);
    } else {
	n = MIN3(elemPtr->xHigh.nValues, elemPtr->xLow.nValues, nPoints);
    }
    if (n > 0) {
	Segment2D *errorBars;
	Segment2D *segPtr;
	double high, low;
	double x, y;
	int *errorToData;
	int *indexPtr;
	register int i;
		
	segPtr = errorBars = Blt_Malloc(n * 3 * sizeof(Segment2D));
	indexPtr = errorToData = Blt_Malloc(n * 3 * sizeof(int));
	for (i = 0; i < n; i++) {
	    x = elemPtr->x.valueArr[i];
	    y = elemPtr->y.valueArr[i];
	    stylePtr = dataToStyle[i];
	    if ((FINITE(x)) && (FINITE(y))) {
		if (elemPtr->xError.nValues > 0) {
		    high = x + elemPtr->xError.valueArr[i];
		    low = x - elemPtr->xError.valueArr[i];
		} else {
		    high = elemPtr->xHigh.valueArr[i];
		    low = elemPtr->xLow.valueArr[i];
		}
		if ((FINITE(high)) && (FINITE(low)))  {
		    Point2D p, q;

		    p = Blt_Map2D(graphPtr, high, y, &elemPtr->axes);
		    q = Blt_Map2D(graphPtr, low, y, &elemPtr->axes);
		    segPtr->p = p;
		    segPtr->q = q;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Left cap */
		    segPtr->p.x = segPtr->q.x = p.x;
		    segPtr->p.y = p.y - stylePtr->errorBarCapWidth;
		    segPtr->q.y = p.y + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Right cap */
		    segPtr->p.x = segPtr->q.x = q.x;
		    segPtr->p.y = q.y - stylePtr->errorBarCapWidth;
		    segPtr->q.y = q.y + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		}
	    }
	}
	elemPtr->xErrorBars = errorBars;
	elemPtr->xErrorBarCnt = segPtr - errorBars;
	elemPtr->xErrorToData = errorToData;
    }
    if (elemPtr->yError.nValues > 0) {
	n = MIN(elemPtr->yError.nValues, nPoints);
    } else {
	n = MIN3(elemPtr->yHigh.nValues, elemPtr->yLow.nValues, nPoints);
    }
    if (n > 0) {
	Segment2D *errorBars;
	Segment2D *segPtr;
	double high, low;
	double x, y;
	int *errorToData;
	int *indexPtr;
	register int i;
		
	segPtr = errorBars = Blt_Malloc(n * 3 * sizeof(Segment2D));
	indexPtr = errorToData = Blt_Malloc(n * 3 * sizeof(int));
	for (i = 0; i < n; i++) {
	    x = elemPtr->x.valueArr[i];
	    y = elemPtr->y.valueArr[i];
	    stylePtr = dataToStyle[i];
	    if ((FINITE(x)) && (FINITE(y))) {
		if (elemPtr->yError.nValues > 0) {
		    high = y + elemPtr->yError.valueArr[i];
		    low = y - elemPtr->yError.valueArr[i];
		} else {
		    high = elemPtr->yHigh.valueArr[i];
		    low = elemPtr->yLow.valueArr[i];
		}
		if ((FINITE(high)) && (FINITE(low)))  {
		    Point2D p, q;
		    
		    p = Blt_Map2D(graphPtr, x, high, &elemPtr->axes);
		    q = Blt_Map2D(graphPtr, x, low, &elemPtr->axes);
		    segPtr->p = p;
		    segPtr->q = q;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Top cap. */
		    segPtr->p.y = segPtr->q.y = p.y;
		    segPtr->p.x = p.x - stylePtr->errorBarCapWidth;
		    segPtr->q.x = p.x + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Bottom cap. */
		    segPtr->p.y = segPtr->q.y = q.y;
		    segPtr->p.x = q.x - stylePtr->errorBarCapWidth;
		    segPtr->q.x = q.x + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		}
	    }
	}
	elemPtr->yErrorBars = errorBars;
	elemPtr->yErrorBarCnt = segPtr - errorBars;
	elemPtr->yErrorToData = errorToData;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * GetIndex --
 *
 *	Given a string representing the index of a pair of x,y
 *	coordinates, return the numeric index.
 *
 * Results:
 *     	A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
static int
GetIndex(interp, elemPtr, string, indexPtr)
    Tcl_Interp *interp;
    Element *elemPtr;
    char *string;
    int *indexPtr;
{
    long ielem;
    int last;

    last = NumberOfPoints(elemPtr) - 1;
    if ((*string == 'e') && (strcmp("end", string) == 0)) {
	ielem = last;
    } else if (Tcl_ExprLong(interp, string, &ielem) != TCL_OK) {
	return TCL_ERROR;
    }
    *indexPtr = (int)ielem;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameToElement --
 *
 *	Find the element represented the given name,  returning
 *	a pointer to its data structure via elemPtrPtr.
 *
 * Results:
 *     	A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
static int
NameToElement(graphPtr, name, elemPtrPtr)
    Graph *graphPtr;
    char *name;
    Element **elemPtrPtr;
{
    Blt_HashEntry *hPtr;

    if (name == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&graphPtr->elements.table, name);
    if (hPtr == NULL) {
	Tcl_AppendResult(graphPtr->interp, "can't find element \"", name,
	    "\" in \"", Tk_PathName(graphPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *elemPtrPtr = (Element *)Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyElement --
 *
 *	Add a new element to the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyElement(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Blt_ChainLink *linkPtr;

    Blt_DeleteBindings(graphPtr->bindTable, elemPtr);
    Blt_LegendRemoveElement(graphPtr->legend, elemPtr);

    Tk_FreeOptions(elemPtr->specsPtr, (char *)elemPtr, graphPtr->display, 0);
    /*
     * Call the element's own destructor to release the memory and
     * resources allocated for it.
     */
    (*elemPtr->procsPtr->destroyProc) (graphPtr, elemPtr);

    /* Remove it also from the element display list */
    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	if (elemPtr == Blt_ChainGetValue(linkPtr)) {
	    Blt_ChainDeleteLink(graphPtr->elements.displayList, linkPtr);
	    if (!elemPtr->hidden) {
		graphPtr->flags |= RESET_WORLD;
		Blt_EventuallyRedrawGraph(graphPtr);
	    }
	    break;
	}
    }
    /* Remove the element for the graph's hash table of elements */
    if (elemPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&graphPtr->elements.table, elemPtr->hashPtr);
    }
    if (elemPtr->name != NULL) {
	Blt_Free(elemPtr->name);
    }
    Blt_Free(elemPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateElement --
 *
 *	Add a new element to the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
CreateElement(graphPtr, interp, argc, argv, classUid)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
    Blt_Uid classUid;
{
    Element *elemPtr;
    Blt_HashEntry *hPtr;
    int isNew;

    if (argv[3][0] == '-') {
	Tcl_AppendResult(graphPtr->interp, "name of element \"", argv[3], 
			 "\" can't start with a '-'", (char *)NULL);
	return TCL_ERROR;
    }
    hPtr = Blt_CreateHashEntry(&graphPtr->elements.table, argv[3], &isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "element \"", argv[3],
	    "\" already exists in \"", argv[0], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (classUid == bltBarElementUid) {
	elemPtr = Blt_BarElement(graphPtr, argv[3], classUid);
    } else { 
	/* Stripcharts are line graphs with some options enabled. */	
	elemPtr = Blt_LineElement(graphPtr, argv[3], classUid);
    }
    elemPtr->hashPtr = hPtr;
    Blt_SetHashValue(hPtr, elemPtr);

    if (Blt_ConfigureWidgetComponent(interp, graphPtr->tkwin, elemPtr->name,
	    "Element", elemPtr->specsPtr, argc - 4, argv + 4, 
		(char *)elemPtr, 0) != TCL_OK) {
	DestroyElement(graphPtr, elemPtr);
	return TCL_ERROR;
    }
    (*elemPtr->procsPtr->configProc) (graphPtr, elemPtr);
    Blt_ChainPrepend(graphPtr->elements.displayList, elemPtr);

    if (!elemPtr->hidden) {
	/* If the new element isn't hidden then redraw the graph.  */
	graphPtr->flags |= REDRAW_BACKING_STORE;
	Blt_EventuallyRedrawGraph(graphPtr);
    }
    elemPtr->flags |= MAP_ITEM;
    graphPtr->flags |= RESET_AXES;
    Tcl_SetResult(interp, elemPtr->name, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RebuildDisplayList --
 *
 *	Given a Tcl list of element names, this procedure rebuilds the
 *	display list, ignoring invalid element names. This list describes
 *	not only only which elements to draw, but in what order.  This is
 *	only important for bar and pie charts.
 *
 * Results:
 *	The return value is a standard Tcl result.  Only if the Tcl list
 *	can not be split, a TCL_ERROR is returned and interp->result contains
 *	an error message.
 *
 * Side effects:
 *	The graph is eventually redrawn using the new display list.
 *
 *----------------------------------------------------------------------
 */
static int
RebuildDisplayList(graphPtr, newList)
    Graph *graphPtr;		/* Graph widget record */
    char *newList;		/* Tcl list of element names */
{
    int nNames;			/* Number of names found in Tcl name list */
    char **nameArr;		/* Broken out array of element names */
    register int i;
    Element *elemPtr;		/* Element information record */

    if (Tcl_SplitList(graphPtr->interp, newList, &nNames, &nameArr) != TCL_OK) {
	Tcl_AppendResult(graphPtr->interp, "can't split name list \"", newList,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    /* Clear the display list and mark all elements as hidden.  */
    Blt_ChainReset(graphPtr->elements.displayList);

    /* Rebuild the display list, checking that each name it exists
     * (currently ignoring invalid element names).  */
    for (i = 0; i < nNames; i++) {
	if (NameToElement(graphPtr, nameArr[i], &elemPtr) == TCL_OK) {
	    Blt_ChainAppend(graphPtr->elements.displayList, elemPtr);
	}
    }
    Blt_Free(nameArr);
    graphPtr->flags |= RESET_WORLD;
    Blt_EventuallyRedrawGraph(graphPtr);
    Tcl_ResetResult(graphPtr->interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DestroyElements --
 *
 *	Removes all the graph's elements. This routine is called when
 *	the graph is destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory allocated for the graph's elements is freed.
 *
 *----------------------------------------------------------------------
 */
void
Blt_DestroyElements(graphPtr)
    Graph *graphPtr;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Element *elemPtr;

    for (hPtr = Blt_FirstHashEntry(&graphPtr->elements.table, &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Blt_GetHashValue(hPtr);
	elemPtr->hashPtr = NULL;
	DestroyElement(graphPtr, elemPtr);
    }
    Blt_DeleteHashTable(&graphPtr->elements.table);
    Blt_DeleteHashTable(&graphPtr->elements.tagTable);
    Blt_ChainDestroy(graphPtr->elements.displayList);
}

void
Blt_MapElements(graphPtr)
    Graph *graphPtr;
{
    Element *elemPtr;
    Blt_ChainLink *linkPtr;

    if (graphPtr->mode != MODE_INFRONT) {
	Blt_ResetStacks(graphPtr);
    }
    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if (elemPtr->hidden) {
	    continue;
	}
	if ((graphPtr->flags & MAP_ALL) || (elemPtr->flags & MAP_ITEM)) {
	    (*elemPtr->procsPtr->mapProc) (graphPtr, elemPtr);
	    elemPtr->flags &= ~MAP_ITEM;
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawElements --
 *
 *	Calls the individual element drawing routines for each
 *	element.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Elements are drawn into the drawable (pixmap) which will
 *	eventually be displayed in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawElements(graphPtr, drawable)
    Graph *graphPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    Blt_ChainLink *linkPtr;
    Element *elemPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if (!elemPtr->hidden) {
	    (*elemPtr->procsPtr->drawNormalProc) (graphPtr, drawable, elemPtr);
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawActiveElements --
 *
 *	Calls the individual element drawing routines to display
 *	the active colors for each element.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Elements are drawn into the drawable (pixmap) which will
 *	eventually be displayed in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawActiveElements(graphPtr, drawable)
    Graph *graphPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    Blt_ChainLink *linkPtr;
    Element *elemPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if ((!elemPtr->hidden) && (elemPtr->flags & ELEM_ACTIVE)) {
	    (*elemPtr->procsPtr->drawActiveProc) (graphPtr, drawable, elemPtr);
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_ElementsToPostScript --
 *
 *	Generates PostScript output for each graph element in the
 *	element display list.
 *
 * -----------------------------------------------------------------
 */
void
Blt_ElementsToPostScript(graphPtr, psToken)
    Graph *graphPtr;
    PsToken psToken;
{
    Blt_ChainLink *linkPtr;
    Element *elemPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if (!elemPtr->hidden) {
	    /* Comment the PostScript to indicate the start of the element */
	    Blt_FormatToPostScript(psToken, "\n%% Element \"%s\"\n\n", 
		elemPtr->name);
	    (*elemPtr->procsPtr->printNormalProc) (graphPtr, psToken, elemPtr);
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_ActiveElementsToPostScript --
 *
 * -----------------------------------------------------------------
 */
void
Blt_ActiveElementsToPostScript(graphPtr, psToken)
    Graph *graphPtr;
    PsToken psToken;
{
    Blt_ChainLink *linkPtr;
    Element *elemPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if ((!elemPtr->hidden) && (elemPtr->flags & ELEM_ACTIVE)) {
	    Blt_FormatToPostScript(psToken, "\n%% Active Element \"%s\"\n\n",
		elemPtr->name);
	    (*elemPtr->procsPtr->printActiveProc) (graphPtr, psToken, elemPtr);
	}
    }
}

int
Blt_GraphUpdateNeeded(graphPtr)
    Graph *graphPtr;
{
    Blt_ChainLink *linkPtr;
    Element *elemPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	if (elemPtr->hidden) {
	    continue;
	}
	/* Check if the x or y vectors have notifications pending */
	if ((Blt_VectorNotifyPending(elemPtr->x.clientId)) ||
	    (Blt_VectorNotifyPending(elemPtr->y.clientId))) {
	    return 1;
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * ActivateOp --
 *
 *	Marks data points of elements (given by their index) as active.
 *
 * Results:
 *	Returns TCL_OK if no errors occurred.
 *
 *----------------------------------------------------------------------
 */
static int
ActivateOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    register int i;
    int *activeArr;
    int nActiveIndices;

    if (argc == 3) {
	register Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;

	/* List all the currently active elements */
	for (hPtr = Blt_FirstHashEntry(&graphPtr->elements.table, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    elemPtr = (Element *)Blt_GetHashValue(hPtr);
	    if (elemPtr->flags & ELEM_ACTIVE) {
		Tcl_AppendElement(graphPtr->interp, elemPtr->name);
	    }
	}
	return TCL_OK;
    }
    if (NameToElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    elemPtr->flags |= ELEM_ACTIVE | ACTIVE_PENDING;

    activeArr = NULL;
    nActiveIndices = -1;
    if (argc > 4) {
	register int *activePtr;

	nActiveIndices = argc - 4;
	activePtr = activeArr = Blt_Malloc(sizeof(int) * nActiveIndices);
	assert(activeArr);
	for (i = 4; i < argc; i++) {
	    if (GetIndex(interp, elemPtr, argv[i], activePtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    activePtr++;
	}
    }
    if (elemPtr->activeIndices != NULL) {
	Blt_Free(elemPtr->activeIndices);
    }
    elemPtr->nActiveIndices = nActiveIndices;
    elemPtr->activeIndices = activeArr;
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

ClientData
Blt_MakeElementTag(graphPtr, tagName)
    Graph *graphPtr;
    char *tagName;
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&graphPtr->elements.tagTable, tagName, &isNew);
    assert(hPtr);
    return Blt_GetHashKey(&graphPtr->elements.tagTable, hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BindOp --
 *
 *	.g element bind elemName sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BindOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	char *tagName;

	for (hPtr = Blt_FirstHashEntry(&graphPtr->elements.tagTable, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tagName = Blt_GetHashKey(&graphPtr->elements.tagTable, hPtr);
	    Tcl_AppendElement(interp, tagName);
	}
	return TCL_OK;
    }
    return Blt_ConfigureBindings(interp, graphPtr->bindTable,
	Blt_MakeElementTag(graphPtr, argv[3]), argc - 4, argv + 4);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateOp --
 *
 *	Add a new element to the graph (using the default type of the
 *	graph).
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
CreateOp(graphPtr, interp, argc, argv, type)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
    Blt_Uid type;
{
    return CreateElement(graphPtr, interp, argc, argv, type);
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    Element *elemPtr;

    if (NameToElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    if (Tk_ConfigureValue(interp, graphPtr->tkwin, elemPtr->specsPtr,
	    (char *)elemPtr, argv[4], 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClosestOp --
 *
 *	Find the element closest to the specified screen coordinates.
 *	Options:
 *	-halo		Consider points only with this maximum distance
 *			from the picked coordinate.
 *	-interpolate	Find closest point along element traces, not just
 *			data points.
 *	-along
 *
 * Results:
 *	A standard Tcl result. If an element could be found within
 *	the halo distance, the interpreter result is "1", otherwise
 *	"0".  If a closest element exists, the designated Tcl array
 *	variable will be set with the following information:
 *
 *	1) the element name,
 *	2) the index of the closest point,
 *	3) the distance (in screen coordinates) from the picked X-Y
 *	   coordinate and the closest point,
 *	4) the X coordinate (graph coordinate) of the closest point,
 *	5) and the Y-coordinate.
 *
 *----------------------------------------------------------------------
 */

static Tk_ConfigSpec closestSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-halo", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(ClosestSearch, halo), 0, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-interpolate", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(ClosestSearch, mode), 0 }, 
    {TK_CONFIG_CUSTOM, "-along", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(ClosestSearch, along), 0, &alongOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static int
ClosestOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    ClosestSearch search;
    int i, x, y;
    int flags = TCL_LEAVE_ERR_MSG;
    int found;

    if (graphPtr->flags & RESET_AXES) {
	Blt_ResetAxes(graphPtr);
    }
    if (Tk_GetPixels(interp, graphPtr->tkwin, argv[3], &x) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window x-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tk_GetPixels(interp, graphPtr->tkwin, argv[4], &y) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window y-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    if (graphPtr->inverted) {
	int temp;

	temp = x, x = y, y = temp;
    }
    for (i = 6; i < argc; i += 2) {	/* Count switches-value pairs */
	if ((argv[i][0] != '-') || 
	    ((argv[i][1] == '-') && (argv[i][2] == '\0'))) {
	    break;
	}
    }
    if (i > argc) {
	i = argc;
    }

    search.mode = SEARCH_POINTS;
    search.halo = graphPtr->halo;
    search.index = -1;
    search.along = SEARCH_BOTH;
    search.x = x;
    search.y = y;

    if (Tk_ConfigureWidget(interp, graphPtr->tkwin, closestSpecs, i - 6,
	    argv + 6, (char *)&search, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;	/* Error occurred processing an option. */
    }
    if ((i < argc) && (argv[i][0] == '-')) {
	i++;			/* Skip "--" */
    }
    search.dist = (double)(search.halo + 1);

    if (i < argc) {
	Blt_ChainLink *linkPtr;

	for ( /* empty */ ; i < argc; i++) {
	    if (NameToElement(graphPtr, argv[i], &elemPtr) != TCL_OK) {
		return TCL_ERROR;	/* Can't find named element */
	    }
	    found = FALSE;
	    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
		 linkPtr == NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
		if (elemPtr == Blt_ChainGetValue(linkPtr)) {
		    found = TRUE;
		    break;
		}
	    }
	    if ((!found) || (elemPtr->hidden)) {
		Tcl_AppendResult(interp, "element \"", argv[i], "\" is hidden",
			(char *)NULL);
		return TCL_ERROR;	/* Element isn't visible */
	    }
	    /* Check if the X or Y vectors have notifications pending */
	    if ((elemPtr->flags & MAP_ITEM) ||
		(Blt_VectorNotifyPending(elemPtr->x.clientId)) ||
		(Blt_VectorNotifyPending(elemPtr->y.clientId))) {
		continue;
	    }
	    (*elemPtr->procsPtr->closestProc) (graphPtr, elemPtr, &search);
	}
    } else {
	Blt_ChainLink *linkPtr;

	/* 
	 * Find the closest point from the set of displayed elements,
	 * searching the display list from back to front.  That way if
	 * the points from two different elements overlay each other
	 * exactly, the last one picked will be the topmost.  
	 */
	for (linkPtr = Blt_ChainLastLink(graphPtr->elements.displayList);
	    linkPtr != NULL; linkPtr = Blt_ChainPrevLink(linkPtr)) {
	    elemPtr = Blt_ChainGetValue(linkPtr);
	    /* Check if the X or Y vectors have notifications pending */
	    if ((elemPtr->hidden) || 
		(elemPtr->flags & MAP_ITEM) ||
		(Blt_VectorNotifyPending(elemPtr->x.clientId)) ||
		(Blt_VectorNotifyPending(elemPtr->y.clientId))) {
		continue;
	    }
	    (*elemPtr->procsPtr->closestProc)(graphPtr, elemPtr, &search);
	}

    }
    if (search.dist < (double)search.halo) {
	char string[200];
	/*
	 *  Return an array of 5 elements
	 */
	if (Tcl_SetVar2(interp, argv[5], "name",
		search.elemPtr->name, flags) == NULL) {
	    return TCL_ERROR;
	}
	sprintf(string, "%d", search.index);
	if (Tcl_SetVar2(interp, argv[5], "index", string, flags) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, search.point.x, string);
	if (Tcl_SetVar2(interp, argv[5], "x", string, flags) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, search.point.y, string);
	if (Tcl_SetVar2(interp, argv[5], "y", string, flags) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, search.dist, string);
	if (Tcl_SetVar2(interp, argv[5], "dist", string, flags) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, "1", TCL_STATIC);
    } else {
	if (Tcl_SetVar2(interp, argv[5], "name", "", flags) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, "0", TCL_STATIC);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	Sets the element specifications by the given the command line
 *	arguments and calls the element specification configuration
 *	routine. If zero or one command line options are given, only
 *	information about the option(s) is returned in interp->result.
 *	If the element configuration has changed and the element is
 *	currently displayed, the axis limits are updated and
 *	recomputed.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 *---------------------------------------------------------------------- 
 */
static int
ConfigureOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    Element *elemPtr;
    int flags;
    int numNames, numOpts;
    char **options;
    register int i;

    /* Figure out where the option value pairs begin */
    argc -= 3;
    argv += 3;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    break;
	}
	if (NameToElement(graphPtr, argv[i], &elemPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find named element */
	}
    }
    numNames = i;		/* Number of element names specified */
    numOpts = argc - i;		/* Number of options specified */
    options = argv + numNames;	/* Start of options in argv  */

    for (i = 0; i < numNames; i++) {
	NameToElement(graphPtr, argv[i], &elemPtr);
	flags = TK_CONFIG_ARGV_ONLY;
	if (numOpts == 0) {
	    return Tk_ConfigureInfo(interp, graphPtr->tkwin, 
		elemPtr->specsPtr, (char *)elemPtr, (char *)NULL, flags);
	} else if (numOpts == 1) {
	    return Tk_ConfigureInfo(interp, graphPtr->tkwin, 
		elemPtr->specsPtr, (char *)elemPtr, options[0], flags);
	}
	if (Tk_ConfigureWidget(interp, graphPtr->tkwin, elemPtr->specsPtr, 
		numOpts, options, (char *)elemPtr, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((*elemPtr->procsPtr->configProc) (graphPtr, elemPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Failed to configure element */
	}
	if (Blt_ConfigModified(elemPtr->specsPtr, "-hide", (char *)NULL)) {
	    graphPtr->flags |= RESET_AXES;
	    elemPtr->flags |= MAP_ITEM;
	}
	/* If data points or axes have changed, reset the axes (may
	 * affect autoscaling) and recalculate the screen points of
	 * the element. */

	if (Blt_ConfigModified(elemPtr->specsPtr, "-*data", "-map*", "-x",
		       "-y", (char *)NULL)) {
	    graphPtr->flags |= RESET_WORLD;
	    elemPtr->flags |= MAP_ITEM;
	}
	/* The new label may change the size of the legend */
	if (Blt_ConfigModified(elemPtr->specsPtr, "-label", (char *)NULL)) {
	    graphPtr->flags |= (MAP_WORLD | REDRAW_WORLD);
	}
    }
    /* Update the pixmap if any configuration option changed */
    graphPtr->flags |= (REDRAW_BACKING_STORE | DRAW_MARGINS);
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeactivateOp --
 *
 *	Clears the active bit for the named elements.
 *
 * Results:
 *	Returns TCL_OK if no errors occurred.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeactivateOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Not used. */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    register int i;

    for (i = 3; i < argc; i++) {
	if (NameToElement(graphPtr, argv[i], &elemPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find named element */
	}
	elemPtr->flags &= ~ELEM_ACTIVE;
	if (elemPtr->activeIndices != NULL) {
	    Blt_Free(elemPtr->activeIndices);
	    elemPtr->activeIndices = NULL;
	}
	elemPtr->nActiveIndices = 0;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Delete the named elements from the graph.
 *
 * Results:
 *	TCL_ERROR is returned if any of the named elements can not be
 *	found.  Otherwise TCL_OK is returned;
 *
 * Side Effects:
 *	If the element is currently displayed, the plotting area of
 *	the graph is redrawn. Memory and resources allocated by the
 *	elements are released.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Not used. */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    register int i;

    for (i = 3; i < argc; i++) {
	if (NameToElement(graphPtr, argv[i], &elemPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find named element */
	}
	DestroyElement(graphPtr, elemPtr);
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExistsOp --
 *
 *	Indicates if the named element exists in the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.  The interpreter
 *	result will contain "1" or "0".
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ExistsOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&graphPtr->elements.table, argv[3]);
    Blt_SetBooleanResult(interp, (hPtr != NULL));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 * 	Returns the name of the picked element (using the element
 *	bind operation).  Right now, the only name accepted is
 *	"current".
 *
 * Results:
 *	A standard Tcl result.  The interpreter result will contain
 *	the name of the element.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char *argv[];
{
    register Element *elemPtr;

    if ((argv[3][0] == 'c') && (strcmp(argv[3], "current") == 0)) {
	elemPtr = (Element *)Blt_GetCurrentItem(graphPtr->bindTable);
	/* Report only on elements. */
	if ((elemPtr != NULL) && 
	    ((elemPtr->classUid == bltBarElementUid) ||
	    (elemPtr->classUid == bltLineElementUid) ||
	    (elemPtr->classUid == bltStripElementUid))) {
	    Tcl_SetResult(interp, elemPtr->name, TCL_VOLATILE);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Returns the names of the elements is the graph matching
 *	one of more patterns provided.  If no pattern arguments
 *	are given, then all element names will be returned.
 *
 * Results:
 *	The return value is a standard Tcl result. The interpreter
 *	result will contain a Tcl list of the element names.
 *
 *----------------------------------------------------------------------
 */
static int
NamesOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Element *elemPtr;
    Blt_HashSearch cursor;
    register Blt_HashEntry *hPtr;
    register int i;

    for (hPtr = Blt_FirstHashEntry(&graphPtr->elements.table, &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Blt_GetHashValue(hPtr);
	if (argc == 3) {
	    Tcl_AppendElement(graphPtr->interp, elemPtr->name);
	    continue;
	}
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(elemPtr->name, argv[i])) {
		Tcl_AppendElement(interp, elemPtr->name);
		break;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ShowOp --
 *
 *	Queries or resets the element display list.
 *
 * Results:
 *	The return value is a standard Tcl result. The interpreter
 *	result will contain the new display list of element names.
 *
 *----------------------------------------------------------------------
 */
static int
ShowOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Element *elemPtr;
    Blt_ChainLink *linkPtr;

    if (argc == 4) {
	if (RebuildDisplayList(graphPtr, argv[3]) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    for (linkPtr = Blt_ChainFirstLink(graphPtr->elements.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	elemPtr = Blt_ChainGetValue(linkPtr);
	Tcl_AppendElement(interp, elemPtr->name);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TypeOp --
 *
 *	Returns the name of the type of the element given by some
 *	element name.
 *
 * Results:
 *	A standard Tcl result. Returns the type of the element in
 *	interp->result. If the identifier given doesn't represent an
 *	element, then an error message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TypeOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Element name */
{
    Element *elemPtr;

    if (NameToElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    Tcl_SetResult(interp, elemPtr->classUid, TCL_STATIC);
    return TCL_OK;
}

/*
 * Global routines:
 */
static Blt_OpSpec elemOps[] =
{
    {"activate", 1, (Blt_Op)ActivateOp, 3, 0, "?elemName? ?index...?",},
    {"bind", 1, (Blt_Op)BindOp, 3, 6, "elemName sequence command",},
    {"cget", 2, (Blt_Op)CgetOp, 5, 5, "elemName option",},
    {"closest", 2, (Blt_Op)ClosestOp, 6, 0,
	"x y varName ?option value?... ?elemName?...",},
    {"configure", 2, (Blt_Op)ConfigureOp, 4, 0,
	"elemName ?elemName?... ?option value?...",},
    {"create", 2, (Blt_Op)CreateOp, 4, 0, "elemName ?option value?...",},
    {"deactivate", 3, (Blt_Op)DeactivateOp, 3, 0, "?elemName?...",},
    {"delete", 3, (Blt_Op)DeleteOp, 3, 0, "?elemName?...",},
    {"exists", 1, (Blt_Op)ExistsOp, 4, 4, "elemName",},
    {"get", 1, (Blt_Op)GetOp, 4, 4, "name",},
    {"names", 1, (Blt_Op)NamesOp, 3, 0, "?pattern?...",},
    {"show", 1, (Blt_Op)ShowOp, 3, 4, "?elemList?",},
    {"type", 1, (Blt_Op)TypeOp, 4, 4, "elemName",},
};
static int numElemOps = sizeof(elemOps) / sizeof(Blt_OpSpec);


/*
 * ----------------------------------------------------------------
 *
 * Blt_ElementOp --
 *
 *	This procedure is invoked to process the Tcl command that
 *	corresponds to a widget managed by this module.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * ----------------------------------------------------------------
 */
int
Blt_ElementOp(graphPtr, interp, argc, argv, type)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* # arguments */
    char **argv;		/* Argument list */
    Blt_Uid type;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, numElemOps, elemOps, BLT_OP_ARG2, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    if (proc == CreateOp) {
	result = CreateOp(graphPtr, interp, argc, argv, type);
    } else {
	result = (*proc) (graphPtr, interp, argc, argv);
    }
    return result;
}
