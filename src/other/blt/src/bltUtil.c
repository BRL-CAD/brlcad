/*
 * bltUtil.c --
 *
 *	This module implements utility procedures for the BLT
 *	toolkit.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
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
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <bltHash.h>

#ifndef HAVE_STRTOLOWER
void
strtolower(s) 
    register char *s;
{
    while (*s != '\0') {
	*s = tolower(UCHAR(*s));
	s++;
    }
}
#endif /* !HAVE_STRTOLOWER */

#ifndef HAVE_STRCASECMP

static unsigned char caseTable[] =
{
    (unsigned char)'\000', (unsigned char)'\001', 
    (unsigned char)'\002', (unsigned char)'\003', 
    (unsigned char)'\004', (unsigned char)'\005', 
    (unsigned char)'\006', (unsigned char)'\007',
    (unsigned char)'\010', (unsigned char)'\011', 
    (unsigned char)'\012', (unsigned char)'\013', 
    (unsigned char)'\014', (unsigned char)'\015', 
    (unsigned char)'\016', (unsigned char)'\017',
    (unsigned char)'\020', (unsigned char)'\021', 
    (unsigned char)'\022', (unsigned char)'\023', 
    (unsigned char)'\024', (unsigned char)'\025', 
    (unsigned char)'\026', (unsigned char)'\027',
    (unsigned char)'\030', (unsigned char)'\031', 
    (unsigned char)'\032', (unsigned char)'\033', 
    (unsigned char)'\034', (unsigned char)'\035', 
    (unsigned char)'\036', (unsigned char)'\037',
    (unsigned char)'\040', (unsigned char)'\041', 
    (unsigned char)'\042', (unsigned char)'\043', 
    (unsigned char)'\044', (unsigned char)'\045', 
    (unsigned char)'\046', (unsigned char)'\047',
    (unsigned char)'\050', (unsigned char)'\051', 
    (unsigned char)'\052', (unsigned char)'\053', 
    (unsigned char)'\054', (unsigned char)'\055', 
    (unsigned char)'\056', (unsigned char)'\057',
    (unsigned char)'\060', (unsigned char)'\061', 
    (unsigned char)'\062', (unsigned char)'\063', 
    (unsigned char)'\064', (unsigned char)'\065', 
    (unsigned char)'\066', (unsigned char)'\067',
    (unsigned char)'\070', (unsigned char)'\071', 
    (unsigned char)'\072', (unsigned char)'\073', 
    (unsigned char)'\074', (unsigned char)'\075', 
    (unsigned char)'\076', (unsigned char)'\077',
    (unsigned char)'\100', (unsigned char)'\141', 
    (unsigned char)'\142', (unsigned char)'\143', 
    (unsigned char)'\144', (unsigned char)'\145', 
    (unsigned char)'\146', (unsigned char)'\147',
    (unsigned char)'\150', (unsigned char)'\151', 
    (unsigned char)'\152', (unsigned char)'\153', 
    (unsigned char)'\154', (unsigned char)'\155', 
    (unsigned char)'\156', (unsigned char)'\157',
    (unsigned char)'\160', (unsigned char)'\161', 
    (unsigned char)'\162', (unsigned char)'\163', 
    (unsigned char)'\164', (unsigned char)'\165', 
    (unsigned char)'\166', (unsigned char)'\167',
    (unsigned char)'\170', (unsigned char)'\171', 
    (unsigned char)'\172', (unsigned char)'\133', 
    (unsigned char)'\134', (unsigned char)'\135', 
    (unsigned char)'\136', (unsigned char)'\137',
    (unsigned char)'\140', (unsigned char)'\141', 
    (unsigned char)'\142', (unsigned char)'\143', 
    (unsigned char)'\144', (unsigned char)'\145', 
    (unsigned char)'\146', (unsigned char)'\147',
    (unsigned char)'\150', (unsigned char)'\151', 
    (unsigned char)'\152', (unsigned char)'\153', 
    (unsigned char)'\154', (unsigned char)'\155', 
    (unsigned char)'\156', (unsigned char)'\157',
    (unsigned char)'\160', (unsigned char)'\161',
    (unsigned char)'\162', (unsigned char)'\163', 
    (unsigned char)'\164', (unsigned char)'\165', 
    (unsigned char)'\166', (unsigned char)'\167',
    (unsigned char)'\170', (unsigned char)'\171', 
    (unsigned char)'\172', (unsigned char)'\173', 
    (unsigned char)'\174', (unsigned char)'\175', 
    (unsigned char)'\176', (unsigned char)'\177',
    (unsigned char)'\200', (unsigned char)'\201', 
    (unsigned char)'\202', (unsigned char)'\203', 
    (unsigned char)'\204', (unsigned char)'\205', 
    (unsigned char)'\206', (unsigned char)'\207',
    (unsigned char)'\210', (unsigned char)'\211', 
    (unsigned char)'\212', (unsigned char)'\213', 
    (unsigned char)'\214', (unsigned char)'\215', 
    (unsigned char)'\216', (unsigned char)'\217',
    (unsigned char)'\220', (unsigned char)'\221', 
    (unsigned char)'\222', (unsigned char)'\223', 
    (unsigned char)'\224', (unsigned char)'\225', 
    (unsigned char)'\226', (unsigned char)'\227',
    (unsigned char)'\230', (unsigned char)'\231', 
    (unsigned char)'\232', (unsigned char)'\233', 
    (unsigned char)'\234', (unsigned char)'\235', 
    (unsigned char)'\236', (unsigned char)'\237',
    (unsigned char)'\240', (unsigned char)'\241', 
    (unsigned char)'\242', (unsigned char)'\243', 
    (unsigned char)'\244', (unsigned char)'\245', 
    (unsigned char)'\246', (unsigned char)'\247',
    (unsigned char)'\250', (unsigned char)'\251', 
    (unsigned char)'\252', (unsigned char)'\253', 
    (unsigned char)'\254', (unsigned char)'\255', 
    (unsigned char)'\256', (unsigned char)'\257',
    (unsigned char)'\260', (unsigned char)'\261', 
    (unsigned char)'\262', (unsigned char)'\263', 
    (unsigned char)'\264', (unsigned char)'\265', 
    (unsigned char)'\266', (unsigned char)'\267',
    (unsigned char)'\270', (unsigned char)'\271', 
    (unsigned char)'\272', (unsigned char)'\273', 
    (unsigned char)'\274', (unsigned char)'\275', 
    (unsigned char)'\276', (unsigned char)'\277',
    (unsigned char)'\300', (unsigned char)'\341', 
    (unsigned char)'\342', (unsigned char)'\343', 
    (unsigned char)'\344', (unsigned char)'\345', 
    (unsigned char)'\346', (unsigned char)'\347',
    (unsigned char)'\350', (unsigned char)'\351', 
    (unsigned char)'\352', (unsigned char)'\353', 
    (unsigned char)'\354', (unsigned char)'\355', 
    (unsigned char)'\356', (unsigned char)'\357',
    (unsigned char)'\360', (unsigned char)'\361', 
    (unsigned char)'\362', (unsigned char)'\363', 
    (unsigned char)'\364', (unsigned char)'\365', 
    (unsigned char)'\366', (unsigned char)'\367',
    (unsigned char)'\370', (unsigned char)'\371', 
    (unsigned char)'\372', (unsigned char)'\333', 
    (unsigned char)'\334', (unsigned char)'\335', 
    (unsigned char)'\336', (unsigned char)'\337',
    (unsigned char)'\340', (unsigned char)'\341', 
    (unsigned char)'\342', (unsigned char)'\343', 
    (unsigned char)'\344', (unsigned char)'\345', 
    (unsigned char)'\346', (unsigned char)'\347',
    (unsigned char)'\350', (unsigned char)'\351', 
    (unsigned char)'\352', (unsigned char)'\353', 
    (unsigned char)'\354', (unsigned char)'\355', 
    (unsigned char)'\356', (unsigned char)'\357',
    (unsigned char)'\360', (unsigned char)'\361', 
    (unsigned char)'\362', (unsigned char)'\363', 
    (unsigned char)'\364', (unsigned char)'\365', 
    (unsigned char)'\366', (unsigned char)'\367',
    (unsigned char)'\370', (unsigned char)'\371', 
    (unsigned char)'\372', (unsigned char)'\373', 
    (unsigned char)'\374', (unsigned char)'\375', 
    (unsigned char)'\376', (unsigned char)'\377',
};

/*
 *----------------------------------------------------------------------
 *
 * strcasecmp --
 *
 *      Compare two strings, disregarding case.
 *
 * Results:
 *      Returns a signed integer representing the following:
 *
 *	zero      - two strings are equal
 *	negative  - first string is less than second
 *	positive  - first string is greater than second
 *
 *----------------------------------------------------------------------
 */
int
strcasecmp(s1, s2)
    CONST char *s1;
    CONST char *s2;
{
    unsigned char *s = (unsigned char *)s1;
    unsigned char *t = (unsigned char *)s2;

    for ( /* empty */ ; (caseTable[*s] == caseTable[*t]); s++, t++) {
	if (*s == '\0') {
	    return 0;
	}
    }
    return (caseTable[*s] - caseTable[*t]);
}

/*
 *----------------------------------------------------------------------
 *
 * strncasecmp --
 *
 *      Compare two strings, disregarding case, up to a given length.
 *
 * Results:
 *      Returns a signed integer representing the following:
 *
 *	zero      - two strings are equal
 *	negative  - first string is less than second
 *	positive  - first string is greater than second
 *
 *----------------------------------------------------------------------
 */
int
strncasecmp(s1, s2, length)
    CONST char *s1;
    CONST char *s2;
    size_t length;
{
    register unsigned char *s = (unsigned char *)s1;
    register unsigned char *t = (unsigned char *)s2;

    for ( /* empty */ ; (length > 0); s++, t++, length--) {
	if (caseTable[*s] != caseTable[*t]) {
	    return (caseTable[*s] - caseTable[*t]);
	}
	if (*s == '\0') {
	    return 0;
	}
    }
    return 0;
}

#endif /* !HAVE_STRCASECMP */


#if (TCL_VERSION_NUMBER < _VERSION(8,1,0)) && (TCL_MAJOR_VERSION > 7)

char *
Tcl_GetString(Tcl_Obj *objPtr)
{
    unsigned int dummy;

    return Tcl_GetStringFromObj(objPtr, &dummy);
}

int 
Tcl_EvalObjv(Tcl_Interp *interp, int objc, Tcl_Obj **objv, int flags)
{
    Tcl_DString dString;
    register int i;
    int result;

    Tcl_DStringInit(&dString);
    for (i = 0; i < objc; i++) {
	Tcl_DStringAppendElement(&dString, Tcl_GetString(objv[i]));
    }
    result = Tcl_Eval(interp, Tcl_DStringValue(&dString)); 
    Tcl_DStringFree(&dString);
    return result;
}

int 
Tcl_WriteObj(Tcl_Channel channel, Tcl_Obj *objPtr)
{
    char *data;
    int nBytes;

    data = Tcl_GetStringFromObj(objPtr, &nBytes);
    return Tcl_Write(channel, data, nBytes);
}

char *
Tcl_SetVar2Ex(
    Tcl_Interp *interp, 
    char *part1, 
    char *part2, 
    Tcl_Obj *objPtr, 
    int flags)
{
    return Tcl_SetVar2(interp, part1, part2, Tcl_GetString(objPtr), flags);
}

Tcl_Obj *
Tcl_GetVar2Ex(
    Tcl_Interp *interp,
    char *part1, 
    char *part2,
    int flags)
{
    char *result;
    
    result = Tcl_GetVar2(interp, part1, part2, flags);
    if (result == NULL) {
	return NULL;
    }
    return Tcl_NewStringObj(result, -1);
}

#endif

/*
 *----------------------------------------------------------------------
 *
 * CompareByDictionary
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

#if HAVE_UTF
int
Blt_DictionaryCompare(left, right)
    char *left, *right;
{
    Tcl_UniChar uniLeft, uniRight, uniLeftLower, uniRightLower;
    int diff, zeros;
    int secondaryDiff = 0;

    for(;;) {
	if ((isdigit(UCHAR(*right))) && (isdigit(UCHAR(*left)))) { 
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
	    for (;;) {
		if (diff == 0) {
		    diff = UCHAR(*left) - UCHAR(*right);
		}
		right++;
		left++;

		/* Ignore commas in numbers. */
		if (*left == ',') {
		    left++;
		}
		if (*right == ',') {
		    right++;
		}

		if (!isdigit(UCHAR(*right))) { /* INTL: digit */
		    if (isdigit(UCHAR(*left))) { /* INTL: digit */
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
		} else if (!isdigit(UCHAR(*left))) { /* INTL: digit */
		    return -1;
		}
	    }
	    continue;
	}

	/*
	 * Convert character to Unicode for comparison purposes.  If either
	 * string is at the terminating null, do a byte-wise comparison and
	 * bail out immediately.
	 */
	if ((*left != '\0') && (*right != '\0')) {
	    left += Tcl_UtfToUniChar(left, &uniLeft);
	    right += Tcl_UtfToUniChar(right, &uniRight);
	    /*
	     * Convert both chars to lower for the comparison, because
	     * dictionary sorts are case insensitve.  Convert to lower, not
	     * upper, so chars between Z and a will sort before A (where most
	     * other interesting punctuations occur)
	     */
	    uniLeftLower = Tcl_UniCharToLower(uniLeft);
	    uniRightLower = Tcl_UniCharToLower(uniRight);
	} else {
	    diff = UCHAR(*left) - UCHAR(*right);
	    break;
	}

        diff = uniLeftLower - uniRightLower;
        if (diff) {
	    return diff;
	} else if (secondaryDiff == 0) {
	    if (Tcl_UniCharIsUpper(uniLeft) &&
		    Tcl_UniCharIsLower(uniRight)) {
		secondaryDiff = -1;
	    } else if (Tcl_UniCharIsUpper(uniRight)
		    && Tcl_UniCharIsLower(uniLeft)) {
		secondaryDiff = 1;
	    }
        }
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}

#else 

int
Blt_DictionaryCompare(left, right)
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
		/* Ignore commas in numbers. */
		if (*left == ',') {
		    left++;
		}
		if (*right == ',') {
		    right++;
		}
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
#endif

#ifndef NDEBUG
void
Blt_Assert(testExpr, fileName, lineNumber)
    char *testExpr;
    char *fileName;
    int lineNumber;
{
#ifdef WINDEBUG
    PurifyPrintf("line %d of %s: Assert \"%s\" failed\n", lineNumber,
	fileName, testExpr);
#endif
    fprintf(stderr, "line %d of %s: Assert \"%s\" failed\n",
	lineNumber, fileName, testExpr);
    fflush(stderr);
    abort();
}
#endif

/*ARGSUSED*/
void
Blt_Panic TCL_VARARGS_DEF(char *, arg1)
{
    va_list argList;
    char *format;

    format = TCL_VARARGS_START(char *, arg1, argList);
    vfprintf(stderr, format, argList);
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

void 
Blt_DStringAppendElements
TCL_VARARGS_DEF(Tcl_DString *, arg1)
{
    va_list argList;
    Tcl_DString *dsPtr;
    register char *elem;

    dsPtr = TCL_VARARGS_START(Tcl_DString *, arg1, argList);
    while ((elem = va_arg(argList, char *)) != NULL) {
	Tcl_DStringAppendElement(dsPtr, elem);
    }
    va_end(argList);
}

static char stringRep[200];

char *
Blt_Itoa(value)
    int value;
{
    sprintf(stringRep, "%d", value);
    return stringRep;
}

char *
Blt_Utoa(value)
    unsigned int value;
{
    sprintf(stringRep, "%u", value);
    return stringRep;
}

char *
Blt_Dtoa(interp, value)
    Tcl_Interp *interp;
    double value;
{
    Tcl_PrintDouble(interp, value, stringRep);
    return stringRep;
}

#if HAVE_UTF

#undef fopen
FILE *
Blt_OpenUtfFile(fileName, mode)
    char *fileName, *mode;
{
    Tcl_DString dString;
    FILE *f;

    fileName = Tcl_UtfToExternalDString(NULL, fileName, -1, &dString);
    f = fopen(fileName, mode);
    Tcl_DStringFree(&dString);
    return f;
}

#endif /* HAVE_UTF */

/*
 *--------------------------------------------------------------
 *
 * Blt_InitHexTable --
 *
 *	Table index for the hex values. Initialized once, first time.
 *	Used for translation value or delimiter significance lookup.
 *
 *	We build the table at run time for several reasons:
 *
 *     	  1.  portable to non-ASCII machines.
 *	  2.  still reentrant since we set the init flag after setting
 *            table.
 *        3.  easier to extend.
 *        4.  less prone to bugs.
 *
 * Results:
 *	None.
 *
 *--------------------------------------------------------------
 */
void
Blt_InitHexTable(hexTable)
    char hexTable[];
{
    hexTable['0'] = 0;
    hexTable['1'] = 1;
    hexTable['2'] = 2;
    hexTable['3'] = 3;
    hexTable['4'] = 4;
    hexTable['5'] = 5;
    hexTable['6'] = 6;
    hexTable['7'] = 7;
    hexTable['8'] = 8;
    hexTable['9'] = 9;
    hexTable['a'] = hexTable['A'] = 10;
    hexTable['b'] = hexTable['B'] = 11;
    hexTable['c'] = hexTable['C'] = 12;
    hexTable['d'] = hexTable['D'] = 13;
    hexTable['e'] = hexTable['E'] = 14;
    hexTable['f'] = hexTable['F'] = 15;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_GetPosition --
 *
 *	Convert a string representing a numeric position.
 *	A position can be in one of the following forms.
 *
 * 	  number	- number of the item in the hierarchy, indexed
 *			  from zero.
 *	  "end"		- last position in the hierarchy.
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the corresponding numeric index.
 *	If "end" was selected then *indexPtr is set to -1.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
int
Blt_GetPosition(interp, string, indexPtr)
    Tcl_Interp *interp;		/* Interpreter to report results back
				 * to. */
    char *string;		/* String representation of the index.
				 * Can be an integer or "end" to refer
				 * to the last index. */
    int *indexPtr;		/* Holds the converted index. */
{
    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
	*indexPtr = -1;		/* Indicates last position in hierarchy. */
    } else {
	int position;

	if (Tcl_GetInt(interp, string, &position) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (position < 0) {
	    Tcl_AppendResult(interp, "bad position \"", string, "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	*indexPtr = position;
    }
    return TCL_OK;
}

/*
 * The hash table below is used to keep track of all the Blt_Uids created
 * so far.
 */
static Blt_HashTable uidTable;
static int uidInitialized = 0;

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetUid --
 *
 *	Given a string, returns a unique identifier for the string.
 *	A reference count is maintained, so that the identifier
 *	can be freed when it is not needed any more. This can be used
 *	in many places to replace Tcl_GetUid.
 *
 * Results:
 *	This procedure returns a Blt_Uid corresponding to the "string"
 *	argument.  The Blt_Uid has a string value identical to string
 *	(strcmp will return 0), but it's guaranteed that any other
 *	calls to this procedure with a string equal to "string" will
 *	return exactly the same result (i.e. can compare Blt_Uid
 *	*values* directly, without having to call strcmp on what they
 *	point to).
 *
 * Side effects:
 *	New information may be entered into the identifier table.
 *
 *----------------------------------------------------------------------
 */
Blt_Uid
Blt_GetUid(string)
    char *string;		/* String to convert. */
{
    int isNew;
    Blt_HashEntry *hPtr;
    int refCount;
    
    if (!uidInitialized) {
	Blt_InitHashTable(&uidTable, BLT_STRING_KEYS);
	uidInitialized = 1;
    }
    hPtr = Blt_CreateHashEntry(&uidTable, string, &isNew);
    if (isNew) {
	refCount = 0;
    } else {
	refCount = (int)Blt_GetHashValue(hPtr);
    }
    refCount++;
    Blt_SetHashValue(hPtr, (ClientData)refCount);
    return (Blt_Uid)Blt_GetHashKey(&uidTable, hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FreeUid --
 *
 *	Frees the Blt_Uid if there are no more clients using this
 *	identifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The identifier may be deleted from the identifier table.
 *
 *----------------------------------------------------------------------
 */
void
Blt_FreeUid(uid)
    Blt_Uid uid;			/* Identifier to release. */
{
    Blt_HashEntry *hPtr;

    if (!uidInitialized) {
	Blt_InitHashTable(&uidTable, BLT_STRING_KEYS);
	uidInitialized = 1;
    }
    hPtr = Blt_FindHashEntry(&uidTable, uid);
    if (hPtr) {
	int refCount;

	refCount = (int)Blt_GetHashValue(hPtr);
	refCount--;
	if (refCount == 0) {
	    Blt_DeleteHashEntry(&uidTable, hPtr);
	} else {
	    Blt_SetHashValue(hPtr, (ClientData)refCount);
	}
    } else {
	fprintf(stderr, "tried to release unknown identifier \"%s\"\n", uid);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FindUid --
 *
 *	Returns a Blt_Uid associated with a given string, if one exists.
 *
 * Results:
 *	A Blt_Uid for the string if one exists. Otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
Blt_Uid
Blt_FindUid(string)
    char *string;		/* String to find. */
{
    Blt_HashEntry *hPtr;

    if (!uidInitialized) {
	Blt_InitHashTable(&uidTable, BLT_STRING_KEYS);
	uidInitialized = 1;
    }
    hPtr = Blt_FindHashEntry(&uidTable, string);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Blt_Uid) Blt_GetHashKey(&uidTable, hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryOpSearch --
 *
 *      Performs a binary search on the array of command operation
 *      specifications to find a partial, anchored match for the
 *      given operation string.
 *
 * Results:
 *	If the string matches unambiguously the index of the specification
 *	in the array is returned.  If the string does not match, even
 *	as an abbreviation, any operation, -1 is returned.  If the string
 *	matches, but ambiguously -2 is returned.
 *
 *----------------------------------------------------------------------
 */
static int
BinaryOpSearch(specArr, nSpecs, string)
    Blt_OpSpec specArr[];
    int nSpecs;
    char *string;		/* Name of minor operation to search for */
{
    Blt_OpSpec *specPtr;
    char c;
    register int high, low, median;
    register int compare, length;

    low = 0;
    high = nSpecs - 1;
    c = string[0];
    length = strlen(string);
    while (low <= high) {
	median = (low + high) >> 1;
	specPtr = specArr + median;

	/* Test the first character */
	compare = c - specPtr->name[0];
	if (compare == 0) {
	    /* Now test the entire string */
	    compare = strncmp(string, specPtr->name, length);
	    if (compare == 0) {
		if (length < specPtr->minChars) {
		    return -2;	/* Ambiguous operation name */
		}
	    }
	}
	if (compare < 0) {
	    high = median - 1;
	} else if (compare > 0) {
	    low = median + 1;
	} else {
	    return median;	/* Op found. */
	}
    }
    return -1;			/* Can't find operation */
}


/*
 *----------------------------------------------------------------------
 *
 * LinearOpSearch --
 *
 *      Performs a binary search on the array of command operation
 *      specifications to find a partial, anchored match for the
 *      given operation string.
 *
 * Results:
 *	If the string matches unambiguously the index of the specification
 *	in the array is returned.  If the string does not match, even
 *	as an abbreviation, any operation, -1 is returned.  If the string
 *	matches, but ambiguously -2 is returned.
 *
 *----------------------------------------------------------------------
 */
static int
LinearOpSearch(specArr, nSpecs, string)
    Blt_OpSpec specArr[];
    int nSpecs;
    char *string;		/* Name of minor operation to search for */
{
    Blt_OpSpec *specPtr;
    char c;
    int length, nMatches, last;
    register int i;

    c = string[0];
    length = strlen(string);
    nMatches = 0;
    last = -1;
    for (specPtr = specArr, i = 0; i < nSpecs; i++, specPtr++) {
	if ((c == specPtr->name[0]) && 
	    (strncmp(string, specPtr->name, length) == 0)) {
	    last = i;
	    nMatches++;
	    if (length == specPtr->minChars) {
		break;
	    }
	}
    }
    if (nMatches > 1) {
	return -2;		/* Ambiguous operation name */
    } 
    if (nMatches == 0) {
	return -1;		/* Can't find operation */
    } 
    return last;		/* Op found. */
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetOp --
 *
 *      Find the command operation given a string name.  This is useful
 *      where a group of command operations have the same argument
 *      signature.
 *
 * Results:
 *      If found, a pointer to the procedure (function pointer) is
 *      returned.  Otherwise NULL is returned and an error message
 *      containing a list of the possible commands is returned in
 *      interp->result.
 *
 *----------------------------------------------------------------------
 */
Blt_Op
Blt_GetOp(interp, nSpecs, specArr, operPos, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int nSpecs;			/* Number of specifications in array */
    Blt_OpSpec specArr[];	/* Op specification array */
    int operPos;		/* Index of the operation name argument */
    int argc;			/* Number of arguments in the argument vector.
				 * This includes any prefixed arguments */
    char *argv[];		/* Argument vector */
    int flags;			/*  */
{
    Blt_OpSpec *specPtr;
    char *string;
    register int i;
    register int n;

    if (argc <= operPos) {	/* No operation argument */
	Tcl_AppendResult(interp, "wrong # args: ", (char *)NULL);
      usage:
	Tcl_AppendResult(interp, "should be one of...", (char *)NULL);
	for (n = 0; n < nSpecs; n++) {
	    Tcl_AppendResult(interp, "\n  ", (char *)NULL);
	    for (i = 0; i < operPos; i++) {
		Tcl_AppendResult(interp, argv[i], " ", (char *)NULL);
	    }
	    specPtr = specArr + n;
	    Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage,
		(char *)NULL);
	}
	return NULL;
    }
    string = argv[operPos];
    if (flags & BLT_OP_LINEAR_SEARCH) {
	n = LinearOpSearch(specArr, nSpecs, string);
    } else {
	n = BinaryOpSearch(specArr, nSpecs, string);
    }
    if (n == -2) {
	char c;
	int length;

	Tcl_AppendResult(interp, "ambiguous", (char *)NULL);
	if (operPos > 2) {
	    Tcl_AppendResult(interp, " ", argv[operPos - 1], (char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\" matches:",
	    (char *)NULL);

	c = string[0];
	length = strlen(string);
	for (n = 0; n < nSpecs; n++) {
	    specPtr = specArr + n;
	    if ((c == specPtr->name[0]) &&
		(strncmp(string, specPtr->name, length) == 0)) {
		Tcl_AppendResult(interp, " ", specPtr->name, (char *)NULL);
	    }
	}
	return NULL;

    } else if (n == -1) {	/* Can't find operation, display help */
	Tcl_AppendResult(interp, "bad", (char *)NULL);
	if (operPos > 2) {
	    Tcl_AppendResult(interp, " ", argv[operPos - 1], (char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\": ", 
			 (char *)NULL);
	goto usage;
    }
    specPtr = specArr + n;
    if ((argc < specPtr->minArgs) || ((specPtr->maxArgs > 0) &&
	    (argc > specPtr->maxArgs))) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", (char *)NULL);
	for (i = 0; i < operPos; i++) {
	    Tcl_AppendResult(interp, argv[i], " ", (char *)NULL);
	}
	Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage, "\"",
	    (char *)NULL);
	return NULL;
    }
    return specPtr->proc;
}

#if (TCL_VERSION_NUMBER >= _VERSION(8,0,0)) 

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetOpFromObj --
 *
 *      Find the command operation given a string name.  This is useful
 *      where a group of command operations have the same argument
 *      signature.
 *
 * Results:
 *      If found, a pointer to the procedure (function pointer) is
 *      returned.  Otherwise NULL is returned and an error message
 *      containing a list of the possible commands is returned in
 *      interp->result.
 *
 *----------------------------------------------------------------------
 */
Blt_Op
Blt_GetOpFromObj(interp, nSpecs, specArr, operPos, objc, objv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int nSpecs;			/* Number of specifications in array */
    Blt_OpSpec specArr[];	/* Op specification array */
    int operPos;		/* Position of operation in argument list. */
    int objc;			/* Number of arguments in the argument vector.
				 * This includes any prefixed arguments */
    Tcl_Obj *CONST objv[];	/* Argument vector */
    int flags;
{
    Blt_OpSpec *specPtr;
    char *string;
    register int i;
    register int n;

    if (objc <= operPos) {	/* No operation argument */
	Tcl_AppendResult(interp, "wrong # args: ", (char *)NULL);
      usage:
	Tcl_AppendResult(interp, "should be one of...", (char *)NULL);
	for (n = 0; n < nSpecs; n++) {
	    Tcl_AppendResult(interp, "\n  ", (char *)NULL);
	    for (i = 0; i < operPos; i++) {
		Tcl_AppendResult(interp, Tcl_GetString(objv[i]), " ", 
			 (char *)NULL);
	    }
	    specPtr = specArr + n;
	    Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage,
		(char *)NULL);
	}
	return NULL;
    }
    string = Tcl_GetString(objv[operPos]);
    if (flags & BLT_OP_LINEAR_SEARCH) {
	n = LinearOpSearch(specArr, nSpecs, string);
    } else {
	n = BinaryOpSearch(specArr, nSpecs, string);
    }
    if (n == -2) {
	char c;
	int length;

	Tcl_AppendResult(interp, "ambiguous", (char *)NULL);
	if (operPos > 2) {
	    Tcl_AppendResult(interp, " ", Tcl_GetString(objv[operPos - 1]), 
		(char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\" matches:",
	    (char *)NULL);

	c = string[0];
	length = strlen(string);
	for (n = 0; n < nSpecs; n++) {
	    specPtr = specArr + n;
	    if ((c == specPtr->name[0]) &&
		(strncmp(string, specPtr->name, length) == 0)) {
		Tcl_AppendResult(interp, " ", specPtr->name, (char *)NULL);
	    }
	}
	return NULL;

    } else if (n == -1) {	/* Can't find operation, display help */
	Tcl_AppendResult(interp, "bad", (char *)NULL);
	if (operPos > 2) {
	    Tcl_AppendResult(interp, " ", Tcl_GetString(objv[operPos - 1]), 
		(char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\": ", (char *)NULL);
	goto usage;
    }
    specPtr = specArr + n;
    if ((objc < specPtr->minArgs) || 
	((specPtr->maxArgs > 0) && (objc > specPtr->maxArgs))) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", (char *)NULL);
	for (i = 0; i < operPos; i++) {
	    Tcl_AppendResult(interp, Tcl_GetString(objv[i]), " ", 
		(char *)NULL);
	}
	Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage, "\"",
	    (char *)NULL);
	return NULL;
    }
    return specPtr->proc;
}

#endif

#include <stdio.h>

/* open a file
 * calculate the CRC32 of the entire contents
 * return the CRC
 * if there is an error rdet the global variable Crcerror
 */

/* ---------------------------------------------------------------- */

/* this is the CRC32 lookup table
 * thanks Gary S. Brown 
 * 64 lines of 4 values for a 256 dword table (1024 bytes)
 */
static unsigned long crcTab[256] =
{				/* CRC polynomial 0xedb88320 */
    0x00000000UL, 0x77073096UL, 0xee0e612cUL, 0x990951baUL,
    0x076dc419UL, 0x706af48fUL, 0xe963a535UL, 0x9e6495a3UL,
    0x0edb8832UL, 0x79dcb8a4UL, 0xe0d5e91eUL, 0x97d2d988UL,
    0x09b64c2bUL, 0x7eb17cbdUL, 0xe7b82d07UL, 0x90bf1d91UL,
    0x1db71064UL, 0x6ab020f2UL, 0xf3b97148UL, 0x84be41deUL,
    0x1adad47dUL, 0x6ddde4ebUL, 0xf4d4b551UL, 0x83d385c7UL,
    0x136c9856UL, 0x646ba8c0UL, 0xfd62f97aUL, 0x8a65c9ecUL,
    0x14015c4fUL, 0x63066cd9UL, 0xfa0f3d63UL, 0x8d080df5UL,
    0x3b6e20c8UL, 0x4c69105eUL, 0xd56041e4UL, 0xa2677172UL,
    0x3c03e4d1UL, 0x4b04d447UL, 0xd20d85fdUL, 0xa50ab56bUL,
    0x35b5a8faUL, 0x42b2986cUL, 0xdbbbc9d6UL, 0xacbcf940UL,
    0x32d86ce3UL, 0x45df5c75UL, 0xdcd60dcfUL, 0xabd13d59UL,
    0x26d930acUL, 0x51de003aUL, 0xc8d75180UL, 0xbfd06116UL,
    0x21b4f4b5UL, 0x56b3c423UL, 0xcfba9599UL, 0xb8bda50fUL,
    0x2802b89eUL, 0x5f058808UL, 0xc60cd9b2UL, 0xb10be924UL,
    0x2f6f7c87UL, 0x58684c11UL, 0xc1611dabUL, 0xb6662d3dUL,
    0x76dc4190UL, 0x01db7106UL, 0x98d220bcUL, 0xefd5102aUL,
    0x71b18589UL, 0x06b6b51fUL, 0x9fbfe4a5UL, 0xe8b8d433UL,
    0x7807c9a2UL, 0x0f00f934UL, 0x9609a88eUL, 0xe10e9818UL,
    0x7f6a0dbbUL, 0x086d3d2dUL, 0x91646c97UL, 0xe6635c01UL,
    0x6b6b51f4UL, 0x1c6c6162UL, 0x856530d8UL, 0xf262004eUL,
    0x6c0695edUL, 0x1b01a57bUL, 0x8208f4c1UL, 0xf50fc457UL,
    0x65b0d9c6UL, 0x12b7e950UL, 0x8bbeb8eaUL, 0xfcb9887cUL,
    0x62dd1ddfUL, 0x15da2d49UL, 0x8cd37cf3UL, 0xfbd44c65UL,
    0x4db26158UL, 0x3ab551ceUL, 0xa3bc0074UL, 0xd4bb30e2UL,
    0x4adfa541UL, 0x3dd895d7UL, 0xa4d1c46dUL, 0xd3d6f4fbUL,
    0x4369e96aUL, 0x346ed9fcUL, 0xad678846UL, 0xda60b8d0UL,
    0x44042d73UL, 0x33031de5UL, 0xaa0a4c5fUL, 0xdd0d7cc9UL,
    0x5005713cUL, 0x270241aaUL, 0xbe0b1010UL, 0xc90c2086UL,
    0x5768b525UL, 0x206f85b3UL, 0xb966d409UL, 0xce61e49fUL,
    0x5edef90eUL, 0x29d9c998UL, 0xb0d09822UL, 0xc7d7a8b4UL,
    0x59b33d17UL, 0x2eb40d81UL, 0xb7bd5c3bUL, 0xc0ba6cadUL,
    0xedb88320UL, 0x9abfb3b6UL, 0x03b6e20cUL, 0x74b1d29aUL,
    0xead54739UL, 0x9dd277afUL, 0x04db2615UL, 0x73dc1683UL,
    0xe3630b12UL, 0x94643b84UL, 0x0d6d6a3eUL, 0x7a6a5aa8UL,
    0xe40ecf0bUL, 0x9309ff9dUL, 0x0a00ae27UL, 0x7d079eb1UL,
    0xf00f9344UL, 0x8708a3d2UL, 0x1e01f268UL, 0x6906c2feUL,
    0xf762575dUL, 0x806567cbUL, 0x196c3671UL, 0x6e6b06e7UL,
    0xfed41b76UL, 0x89d32be0UL, 0x10da7a5aUL, 0x67dd4accUL,
    0xf9b9df6fUL, 0x8ebeeff9UL, 0x17b7be43UL, 0x60b08ed5UL,
    0xd6d6a3e8UL, 0xa1d1937eUL, 0x38d8c2c4UL, 0x4fdff252UL,
    0xd1bb67f1UL, 0xa6bc5767UL, 0x3fb506ddUL, 0x48b2364bUL,
    0xd80d2bdaUL, 0xaf0a1b4cUL, 0x36034af6UL, 0x41047a60UL,
    0xdf60efc3UL, 0xa867df55UL, 0x316e8eefUL, 0x4669be79UL,
    0xcb61b38cUL, 0xbc66831aUL, 0x256fd2a0UL, 0x5268e236UL,
    0xcc0c7795UL, 0xbb0b4703UL, 0x220216b9UL, 0x5505262fUL,
    0xc5ba3bbeUL, 0xb2bd0b28UL, 0x2bb45a92UL, 0x5cb36a04UL,
    0xc2d7ffa7UL, 0xb5d0cf31UL, 0x2cd99e8bUL, 0x5bdeae1dUL,
    0x9b64c2b0UL, 0xec63f226UL, 0x756aa39cUL, 0x026d930aUL,
    0x9c0906a9UL, 0xeb0e363fUL, 0x72076785UL, 0x05005713UL,
    0x95bf4a82UL, 0xe2b87a14UL, 0x7bb12baeUL, 0x0cb61b38UL,
    0x92d28e9bUL, 0xe5d5be0dUL, 0x7cdcefb7UL, 0x0bdbdf21UL,
    0x86d3d2d4UL, 0xf1d4e242UL, 0x68ddb3f8UL, 0x1fda836eUL,
    0x81be16cdUL, 0xf6b9265bUL, 0x6fb077e1UL, 0x18b74777UL,
    0x88085ae6UL, 0xff0f6a70UL, 0x66063bcaUL, 0x11010b5cUL,
    0x8f659effUL, 0xf862ae69UL, 0x616bffd3UL, 0x166ccf45UL,
    0xa00ae278UL, 0xd70dd2eeUL, 0x4e048354UL, 0x3903b3c2UL,
    0xa7672661UL, 0xd06016f7UL, 0x4969474dUL, 0x3e6e77dbUL,
    0xaed16a4aUL, 0xd9d65adcUL, 0x40df0b66UL, 0x37d83bf0UL,
    0xa9bcae53UL, 0xdebb9ec5UL, 0x47b2cf7fUL, 0x30b5ffe9UL,
    0xbdbdf21cUL, 0xcabac28aUL, 0x53b39330UL, 0x24b4a3a6UL,
    0xbad03605UL, 0xcdd70693UL, 0x54de5729UL, 0x23d967bfUL,
    0xb3667a2eUL, 0xc4614ab8UL, 0x5d681b02UL, 0x2a6f2b94UL,
    0xb40bbe37UL, 0xc30c8ea1UL, 0x5a05df1bUL, 0x2d02ef8dUL
}; 

#define CRC32(c, b) (crcTab[((int)(c) ^ (b)) & 0xff] ^ ((c) >> 8))
#define DO1(buf)  crc = CRC32(crc, *buf++)
#define DO2(buf)  DO1(buf); DO1(buf)
#define DO4(buf)  DO2(buf); DO2(buf)
#define DO8(buf)  DO4(buf); DO4(buf)

static int
Crc32Cmd(
   ClientData clientData,
   Tcl_Interp *interp, 
   int argc, char **argv)
{
    register unsigned int crc;
    char buf[200];
    
    crc = 0L;
    crc = crc ^ 0xffffffffL;
    if (strcmp(argv[1], "-data") == 0) {
	register char *p;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " ?fileName? ?-data dataString?", (char *)NULL);
	    return TCL_ERROR;
	}
	for (p = argv[2]; *p != '\0'; p++) {
	    crc = CRC32(crc, *p);
	}
    } else {
	register int c;
	FILE *f;
	
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " ?fileName? ?-data dataString?", (char *)NULL);
	    return TCL_ERROR;
	}
	f = fopen(argv[1], "rb");
	if (f == NULL) {
	    Tcl_AppendResult(interp, "can't open file \"", argv[1], "\": ",
			     Tcl_PosixError(interp), (char *)NULL);
	    return TCL_ERROR;
	}
	while((c = getc(f)) != EOF) {
	    crc = CRC32(crc, c);
	}
	fclose(f);
    }
    crc = crc ^ 0xffffffffL;
    sprintf(buf, "%x", crc);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}

int
Blt_Crc32Init(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {"crc32", Crc32Cmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

