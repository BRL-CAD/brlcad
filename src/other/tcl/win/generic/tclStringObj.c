/*
 * tclStringObj.c --
 *
 *	This file contains functions that implement string operations on Tcl
 *	objects. Some string operations work with UTF strings and others
 *	require Unicode format. Functions that require knowledge of the width
 *	of each character, such as indexing, operate on Unicode data.
 *
 *	A Unicode string is an internationalized string. Conceptually, a
 *	Unicode string is an array of 16-bit quantities organized as a
 *	sequence of properly formed UTF-8 characters. There is a one-to-one
 *	map between Unicode and UTF characters. Because Unicode characters
 *	have a fixed width, operations such as indexing operate on Unicode
 *	data. The String object is optimized for the case where each UTF char
 *	in a string is only one byte. In this case, we store the value of
 *	numChars, but we don't store the Unicode data (unless Tcl_GetUnicode
 *	is explicitly called).
 *
 *	The String object type stores one or both formats. The default
 *	behavior is to store UTF. Once Unicode is calculated by a function, it
 *	is stored in the internal rep for future access (without an additional
 *	O(n) cost).
 *
 *	To allow many appends to be done to an object without constantly
 *	reallocating the space for the string or Unicode representation, we
 *	allocate double the space for the string or Unicode and use the
 *	internal representation to keep track of how much space is used vs.
 *	allocated.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$ */

#include "tclInt.h"
#include "tommath.h"

/*
 * Set COMPAT to 1 to restore the shimmering patterns to those of Tcl 8.5.
 * This is an escape hatch in case the changes have some unexpected unwelcome
 * impact on performance.  If things go well, this mechanism can go away when
 * post-8.6 development begins.
 */
#define COMPAT 0

/*
 * Prototypes for functions defined later in this file:
 */

static void		AppendPrintfToObjVA(Tcl_Obj *objPtr,
			    const char *format, va_list argList);
static void		AppendUnicodeToUnicodeRep(Tcl_Obj *objPtr,
			    const Tcl_UniChar *unicode, int appendNumChars);
static void		AppendUnicodeToUtfRep(Tcl_Obj *objPtr,
			    const Tcl_UniChar *unicode, int numChars);
static void		AppendUtfToUnicodeRep(Tcl_Obj *objPtr,
			    const char *bytes, int numBytes);
static void		AppendUtfToUtfRep(Tcl_Obj *objPtr,
			    const char *bytes, int numBytes);
static void		DupStringInternalRep(Tcl_Obj *objPtr,
			    Tcl_Obj *copyPtr);
static int		ExtendStringRepWithUnicode(Tcl_Obj *objPtr,
			    const Tcl_UniChar *unicode, int numChars);
static void		ExtendUnicodeRepWithString(Tcl_Obj *objPtr,
			    const char *bytes, int numBytes,
			    int numAppendChars);
static void		FillUnicodeRep(Tcl_Obj *objPtr);
static void		FreeStringInternalRep(Tcl_Obj *objPtr);
static void		GrowStringBuffer(Tcl_Obj *objPtr, int needed, int flag);
static void		GrowUnicodeBuffer(Tcl_Obj *objPtr, int needed);
static int		SetStringFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		SetUnicodeObj(Tcl_Obj *objPtr,
			    const Tcl_UniChar *unicode, int numChars);
static int		UnicodeLength(const Tcl_UniChar *unicode);
static void		UpdateStringOfString(Tcl_Obj *objPtr);

/*
 * The structure below defines the string Tcl object type by means of
 * functions that can be invoked by generic object code.
 */

const Tcl_ObjType tclStringType = {
    "string",			/* name */
    FreeStringInternalRep,	/* freeIntRepPro */
    DupStringInternalRep,	/* dupIntRepProc */
    UpdateStringOfString,	/* updateStringProc */
    SetStringFromAny		/* setFromAnyProc */
};

/*
 * The following structure is the internal rep for a String object. It keeps
 * track of how much memory has been used and how much has been allocated for
 * the Unicode and UTF string to enable growing and shrinking of the UTF and
 * Unicode reps of the String object with fewer mallocs. To optimize string
 * length and indexing operations, this structure also stores the number of
 * characters (same of UTF and Unicode!) once that value has been computed.
 *
 * Under normal configurations, what Tcl calls "Unicode" is actually UTF-16
 * restricted to the Basic Multilingual Plane (i.e. U+00000 to U+0FFFF). This
 * can be officially modified by altering the definition of Tcl_UniChar in
 * tcl.h, but do not do that unless you are sure what you're doing!
 */

typedef struct String {
    int numChars;		/* The number of chars in the string. -1 means
				 * this value has not been calculated. >= 0
				 * means that there is a valid Unicode rep, or
				 * that the number of UTF bytes == the number
				 * of chars. */
    int allocated;		/* The amount of space actually allocated for
				 * the UTF string (minus 1 byte for the
				 * termination char). */
    int maxChars;		/* Max number of chars that can fit in the
				 * space allocated for the unicode array. */
    int hasUnicode;		/* Boolean determining whether the string has
				 * a Unicode representation. */
    Tcl_UniChar unicode[1];	/* The array of Unicode chars. The actual size
				 * of this field depends on the 'maxChars'
				 * field above. */
} String;

#define STRING_MAXCHARS \
	(int)(((size_t)UINT_MAX - sizeof(String))/sizeof(Tcl_UniChar))
#define STRING_SIZE(numChars) \
	(sizeof(String) + ((numChars) * sizeof(Tcl_UniChar)))
#define stringCheckLimits(numChars) \
    if ((numChars) < 0 || (numChars) > STRING_MAXCHARS) { \
	Tcl_Panic("max length for a Tcl unicode value (%d chars) exceeded", \
		STRING_MAXCHARS); \
    }
#define stringAlloc(numChars) \
	(String *) ckalloc((unsigned) STRING_SIZE(numChars) )
#define stringRealloc(ptr, numChars) \
	(String *) ckrealloc((char *) ptr, (unsigned) STRING_SIZE(numChars) )
#define stringAttemptRealloc(ptr, numChars) \
	(String *) attemptckrealloc((char *) ptr, \
		(unsigned) STRING_SIZE(numChars) )
#define GET_STRING(objPtr) \
	((String *) (objPtr)->internalRep.otherValuePtr)
#define SET_STRING(objPtr, stringPtr) \
	((objPtr)->internalRep.otherValuePtr = (void *) (stringPtr))

/*
 * TCL STRING GROWTH ALGORITHM
 *
 * When growing strings (during an append, for example), the following growth
 * algorithm is used:
 *
 *   Attempt to allocate 2 * (originalLength + appendLength)
 *   On failure:
 *	attempt to allocate originalLength + 2*appendLength +
 *			TCL_GROWTH_MIN_ALLOC
 *
 * This algorithm allows very good performance, as it rapidly increases the
 * memory allocated for a given string, which minimizes the number of
 * reallocations that must be performed. However, using only the doubling
 * algorithm can lead to a significant waste of memory. In particular, it may
 * fail even when there is sufficient memory available to complete the append
 * request (but there is not 2*totalLength memory available). So when the
 * doubling fails (because there is not enough memory available), the
 * algorithm requests a smaller amount of memory, which is still enough to
 * cover the request, but which hopefully will be less than the total
 * available memory.
 *
 * The addition of TCL_GROWTH_MIN_ALLOC allows for efficient handling of very
 * small appends. Without this extra slush factor, a sequence of several small
 * appends would cause several memory allocations. As long as
 * TCL_GROWTH_MIN_ALLOC is a reasonable size, we can avoid that behavior.
 *
 * The growth algorithm can be tuned by adjusting the following parameters:
 *
 * TCL_GROWTH_MIN_ALLOC		Additional space, in bytes, to allocate when
 *				the double allocation has failed. Default is
 *				1024 (1 kilobyte).
 */

#ifndef TCL_GROWTH_MIN_ALLOC
#define TCL_GROWTH_MIN_ALLOC	1024
#endif

static void
GrowStringBuffer(
    Tcl_Obj *objPtr,
    int needed,
    int flag)
{
    /* Pre-conditions: 
     *	objPtr->typePtr == &tclStringType
     *	needed > stringPtr->allocated
     *	flag || objPtr->bytes != NULL
     */
    String *stringPtr = GET_STRING(objPtr);
    char *ptr = NULL;
    int attempt;

    if (objPtr->bytes == tclEmptyStringRep) {
	objPtr->bytes = NULL;
    }
    if (flag == 0 || stringPtr->allocated > 0) {
	attempt = 2 * needed;
	if (attempt >= 0) {
	    ptr = attemptckrealloc(objPtr->bytes, (unsigned) attempt + 1);
	}
	if (ptr == NULL) {
	    /*
	     * Take care computing the amount of modest growth to avoid
	     * overflow into invalid argument values for attempt.
	     */
	    unsigned int limit = INT_MAX - needed;
	    unsigned int extra = needed - objPtr->length + TCL_GROWTH_MIN_ALLOC;
	    int growth = (int) ((extra > limit) ? limit : extra);
	    attempt = needed + growth;
	    ptr = attemptckrealloc(objPtr->bytes, (unsigned) attempt + 1);
	}
    }
    if (ptr == NULL) {
	/* First allocation - just big enough; or last chance fallback. */
	attempt = needed;
	ptr = ckrealloc(objPtr->bytes, (unsigned) attempt + 1);
    }
    objPtr->bytes = ptr;
    stringPtr->allocated = attempt;
}

static void
GrowUnicodeBuffer(
    Tcl_Obj *objPtr,
    int needed)
{
    /* Pre-conditions: 
     *	objPtr->typePtr == &tclStringType
     *	needed > stringPtr->maxChars
     *	needed < STRING_MAXCHARS
     */
    String *ptr = NULL, *stringPtr = GET_STRING(objPtr);
    int attempt;

    if (stringPtr->maxChars > 0) {
	/* Subsequent appends - apply the growth algorithm. */
	attempt = 2 * needed;
	if (attempt >= 0 && attempt <= STRING_MAXCHARS) {
	    ptr = stringAttemptRealloc(stringPtr, attempt);
	}
	if (ptr == NULL) {
	    /*
	     * Take care computing the amount of modest growth to avoid
	     * overflow into invalid argument values for attempt.
	     */
	    unsigned int limit = STRING_MAXCHARS - needed;
	    unsigned int extra = needed - stringPtr->numChars
		    + TCL_GROWTH_MIN_ALLOC/sizeof(Tcl_UniChar);
	    int growth = (int) ((extra > limit) ? limit : extra);
	    attempt = needed + growth;
	    ptr = stringAttemptRealloc(stringPtr, attempt);
	}
    }
    if (ptr == NULL) {
	/* First allocation - just big enough; or last chance fallback. */
	attempt = needed;
	ptr = stringRealloc(stringPtr, attempt);
    }
    stringPtr = ptr;
    stringPtr->maxChars = attempt;
    SET_STRING(objPtr, stringPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewStringObj --
 *
 *	This function is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new string object and
 *	initializes it from the byte pointer and length arguments.
 *
 *	When TCL_MEM_DEBUG is defined, this function just returns the result
 *	of calling the debugging version Tcl_DbNewStringObj.
 *
 * Results:
 *	A newly created string object is returned that has ref count zero.
 *
 * Side effects:
 *	The new object's internal string representation will be set to a copy
 *	of the length bytes starting at "bytes". If "length" is negative, use
 *	bytes up to the first NUL byte; i.e., assume "bytes" points to a
 *	C-style NUL-terminated string. The object's type is set to NULL. An
 *	extra NUL is added to the end of the new object's byte array.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewStringObj
Tcl_Obj *
Tcl_NewStringObj(
    const char *bytes,		/* Points to the first of the length bytes
				 * used to initialize the new object. */
    int length)			/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If
				 * negative, use bytes up to the first NUL
				 * byte. */
{
    return Tcl_DbNewStringObj(bytes, length, "unknown", 0);
}
#else /* if not TCL_MEM_DEBUG */
Tcl_Obj *
Tcl_NewStringObj(
    const char *bytes,		/* Points to the first of the length bytes
				 * used to initialize the new object. */
    int length)			/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If
				 * negative, use bytes up to the first NUL
				 * byte. */
{
    Tcl_Obj *objPtr;

    if (length < 0) {
	length = (bytes? strlen(bytes) : 0);
    }
    TclNewStringObj(objPtr, bytes, length);
    return objPtr;
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewStringObj --
 *
 *	This function is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new string objects. It is the
 *	same as the Tcl_NewStringObj function above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the [memory active]
 *	command will report the correct file name and line number when
 *	reporting objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this function just returns the
 *	result of calling Tcl_NewStringObj.
 *
 * Results:
 *	A newly created string object is returned that has ref count zero.
 *
 * Side effects:
 *	The new object's internal string representation will be set to a copy
 *	of the length bytes starting at "bytes". If "length" is negative, use
 *	bytes up to the first NUL byte; i.e., assume "bytes" points to a
 *	C-style NUL-terminated string. The object's type is set to NULL. An
 *	extra NUL is added to the end of the new object's byte array.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
Tcl_Obj *
Tcl_DbNewStringObj(
    const char *bytes,		/* Points to the first of the length bytes
				 * used to initialize the new object. */
    int length,			/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If
				 * negative, use bytes up to the first NUL
				 * byte. */
    const char *file,		/* The name of the source file calling this
				 * function; used for debugging. */
    int line)			/* Line number in the source file; used for
				 * debugging. */
{
    Tcl_Obj *objPtr;

    if (length < 0) {
	length = (bytes? strlen(bytes) : 0);
    }
    TclDbNewObj(objPtr, file, line);
    TclInitStringRep(objPtr, bytes, length);
    return objPtr;
}
#else /* if not TCL_MEM_DEBUG */
Tcl_Obj *
Tcl_DbNewStringObj(
    const char *bytes,		/* Points to the first of the length bytes
				 * used to initialize the new object. */
    int length,			/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If
				 * negative, use bytes up to the first NUL
				 * byte. */
    const char *file,		/* The name of the source file calling this
				 * function; used for debugging. */
    int line)			/* Line number in the source file; used for
				 * debugging. */
{
    return Tcl_NewStringObj(bytes, length);
}
#endif /* TCL_MEM_DEBUG */

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_NewUnicodeObj --
 *
 *	This function is creates a new String object and initializes it from
 *	the given Unicode String. If the Utf String is the same size as the
 *	Unicode string, don't duplicate the data.
 *
 * Results:
 *	The newly created object is returned. This object will have no initial
 *	string representation. The returned object has a ref count of 0.
 *
 * Side effects:
 *	Memory allocated for new object and copy of Unicode argument.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_NewUnicodeObj(
    const Tcl_UniChar *unicode,	/* The unicode string used to initialize the
				 * new object. */
    int numChars)		/* Number of characters in the unicode
				 * string. */
{
    Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    SetUnicodeObj(objPtr, unicode, numChars);
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCharLength --
 *
 *	Get the length of the Unicode string from the Tcl object.
 *
 * Results:
 *	Pointer to unicode string representing the unicode object.
 *
 * Side effects:
 *	Frees old internal rep. Allocates memory for new "String" internal
 *	rep.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetCharLength(
    Tcl_Obj *objPtr)		/* The String object to get the num chars
				 * of. */
{
    String *stringPtr;
    int numChars;

    /*
     * Optimize the case where we're really dealing with a bytearray object
     * without string representation; we don't need to convert to a string to
     * perform the get-length operation.
     */

    if (TclIsPureByteArray(objPtr)) {
	int length;

	(void) Tcl_GetByteArrayFromObj(objPtr, &length);
	return length;
    }

    /*
     * OK, need to work with the object as a string.
     */

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);
    numChars = stringPtr->numChars;

    /* If numChars is unknown, compute it. */
    if (numChars == -1) {
	TclNumUtfChars(numChars, objPtr->bytes, objPtr->length);
	stringPtr->numChars = numChars;

#if COMPAT
	if (numChars < objPtr->length) {
	    /*
	     * Since we've just computed the number of chars, and not all
	     * UTF chars are 1-byte long, go ahead and populate the unicode
	     * string.
	     */

	    FillUnicodeRep(objPtr);
	}
#endif
    }
    return numChars;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetUniChar --
 *
 *	Get the index'th Unicode character from the String object. The index
 *	is assumed to be in the appropriate range.
 *
 * Results:
 *	Returns the index'th Unicode character in the Object.
 *
 * Side effects:
 *	Fills unichar with the index'th Unicode character.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar
Tcl_GetUniChar(
    Tcl_Obj *objPtr,		/* The object to get the Unicode charater
				 * from. */
    int index)			/* Get the index'th Unicode character. */
{
    String *stringPtr;

    /*
     * Optimize the case where we're really dealing with a bytearray object
     * without string representation; we don't need to convert to a string to
     * perform the indexing operation.
     */

    if (TclIsPureByteArray(objPtr)) {
	unsigned char *bytes = Tcl_GetByteArrayFromObj(objPtr, NULL);

	return (Tcl_UniChar) bytes[index];
    }

    /*
     * OK, need to work with the object as a string.
     */

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (stringPtr->hasUnicode == 0) {
	/* If numChars is unknown, compute it. */
	if (stringPtr->numChars == -1) {
	    TclNumUtfChars(stringPtr->numChars, objPtr->bytes, objPtr->length);
	}
	if (stringPtr->numChars == objPtr->length) {
	    return (Tcl_UniChar) objPtr->bytes[index];
	}
	FillUnicodeRep(objPtr);
	stringPtr = GET_STRING(objPtr);
    }
    return stringPtr->unicode[index];
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetUnicode --
 *
 *	Get the Unicode form of the String object. If the object is not
 *	already a String object, it will be converted to one. If the String
 *	object does not have a Unicode rep, then one is create from the UTF
 *	string format.
 *
 * Results:
 *	Returns a pointer to the object's internal Unicode string.
 *
 * Side effects:
 *	Converts the object to have the String internal rep.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar *
Tcl_GetUnicode(
    Tcl_Obj *objPtr)		/* The object to find the unicode string
				 * for. */
{
    return Tcl_GetUnicodeFromObj(objPtr, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetUnicodeFromObj --
 *
 *	Get the Unicode form of the String object with length. If the object
 *	is not already a String object, it will be converted to one. If the
 *	String object does not have a Unicode rep, then one is create from the
 *	UTF string format.
 *
 * Results:
 *	Returns a pointer to the object's internal Unicode string.
 *
 * Side effects:
 *	Converts the object to have the String internal rep.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar *
Tcl_GetUnicodeFromObj(
    Tcl_Obj *objPtr,		/* The object to find the unicode string
				 * for. */
    int *lengthPtr)		/* If non-NULL, the location where the string
				 * rep's unichar length should be stored. If
				 * NULL, no length is stored. */
{
    String *stringPtr;

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (stringPtr->hasUnicode == 0) {
	FillUnicodeRep(objPtr);
	stringPtr = GET_STRING(objPtr);
    }

    if (lengthPtr != NULL) {
	*lengthPtr = stringPtr->numChars;
    }
    return stringPtr->unicode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetRange --
 *
 *	Create a Tcl Object that contains the chars between first and last of
 *	the object indicated by "objPtr". If the object is not already a
 *	String object, convert it to one. The first and last indices are
 *	assumed to be in the appropriate range.
 *
 * Results:
 *	Returns a new Tcl Object of the String type.
 *
 * Side effects:
 *	Changes the internal rep of "objPtr" to the String type.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_GetRange(
    Tcl_Obj *objPtr,		/* The Tcl object to find the range of. */
    int first,			/* First index of the range. */
    int last)			/* Last index of the range. */
{
    Tcl_Obj *newObjPtr;		/* The Tcl object to find the range of. */
    String *stringPtr;

    /*
     * Optimize the case where we're really dealing with a bytearray object
     * without string representation; we don't need to convert to a string to
     * perform the substring operation.
     */

    if (TclIsPureByteArray(objPtr)) {
	unsigned char *bytes = Tcl_GetByteArrayFromObj(objPtr, NULL);

	return Tcl_NewByteArrayObj(bytes+first, last-first+1);
    }

    /*
     * OK, need to work with the object as a string.
     */

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (stringPtr->hasUnicode == 0) {
	/* If numChars is unknown, compute it. */
	if (stringPtr->numChars == -1) {
	    TclNumUtfChars(stringPtr->numChars, objPtr->bytes, objPtr->length);
	}
	if (stringPtr->numChars == objPtr->length) {
	    newObjPtr = Tcl_NewStringObj(objPtr->bytes + first, last-first+1);

	    /* Since we know the char length of the result, store it. */
	    SetStringFromAny(NULL, newObjPtr);
	    stringPtr = GET_STRING(newObjPtr);
	    stringPtr->numChars = newObjPtr->length;
	    return newObjPtr;
	}
	FillUnicodeRep(objPtr);
	stringPtr = GET_STRING(objPtr);
    }

    return Tcl_NewUnicodeObj(stringPtr->unicode + first, last-first+1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetStringObj --
 *
 *	Modify an object to hold a string that is a copy of the bytes
 *	indicated by the byte pointer and length arguments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string representation will be set to a copy of the
 *	"length" bytes starting at "bytes". If "length" is negative, use bytes
 *	up to the first NUL byte; i.e., assume "bytes" points to a C-style
 *	NUL-terminated string. The object's old string and internal
 *	representations are freed and the object's type is set NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetStringObj(
    Tcl_Obj *objPtr,		/* Object whose internal rep to init. */
    const char *bytes,		/* Points to the first of the length bytes
				 * used to initialize the object. */
    int length)			/* The number of bytes to copy from "bytes"
				 * when initializing the object. If negative,
				 * use bytes up to the first NUL byte.*/
{
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetStringObj");
    }

    /*
     * Set the type to NULL and free any internal rep for the old type.
     */

    TclFreeIntRep(objPtr);
    objPtr->typePtr = NULL;

    /*
     * Free any old string rep, then set the string rep to a copy of the
     * length bytes starting at "bytes".
     */

    TclInvalidateStringRep(objPtr);
    if (length < 0) {
	length = (bytes? strlen(bytes) : 0);
    }
    TclInitStringRep(objPtr, bytes, length);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetObjLength --
 *
 *	This function changes the length of the string representation of an
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the size of objPtr's string representation is greater than length,
 *	then it is reduced to length and a new terminating null byte is stored
 *	in the strength. If the length of the string representation is greater
 *	than length, the storage space is reallocated to the given length; a
 *	null byte is stored at the end, but other bytes past the end of the
 *	original string representation are undefined. The object's internal
 *	representation is changed to "expendable string".
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetObjLength(
    Tcl_Obj *objPtr,		/* Pointer to object. This object must not
				 * currently be shared. */
    int length)			/* Number of bytes desired for string
				 * representation of object, not including
				 * terminating null byte. */
{
    String *stringPtr;

    if (length < 0) {
	/*
	 * Setting to a negative length is nonsense. This is probably the
	 * result of overflowing the signed integer range.
	 */

	Tcl_Panic("Tcl_SetObjLength: negative length requested: "
		"%d (integer overflow?)", length);
    }
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetObjLength");
    }

    if (objPtr->bytes && objPtr->length == length) {
	return;
    }

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (objPtr->bytes != NULL) {
	/*
	 * Change length of an existing string rep.
	 */
	if (length > stringPtr->allocated) {
	    /*
	     * Need to enlarge the buffer.
	     */
	    if (objPtr->bytes == tclEmptyStringRep) {
		objPtr->bytes = ckalloc((unsigned) length+1);
	    } else {
		objPtr->bytes = ckrealloc(objPtr->bytes, (unsigned) length+1);
	    }
	    stringPtr->allocated = length;
	}

	objPtr->length = length;
	objPtr->bytes[length] = 0;

	/*
	 * Invalidate the unicode data.
	 */

	stringPtr->numChars = -1;
	stringPtr->hasUnicode = 0;
    } else {
	/*
	 * Changing length of pure unicode string.
	 */

	stringCheckLimits(length);
	if (length > stringPtr->maxChars) {
	    stringPtr = stringRealloc(stringPtr, length);
	    SET_STRING(objPtr, stringPtr);
	    stringPtr->maxChars = length;
	}

	/* Mark the new end of the unicode string */
	stringPtr->numChars = length;
	stringPtr->unicode[length] = 0;
	stringPtr->hasUnicode = 1;

	/*
	 * Can only get here when objPtr->bytes == NULL.
	 * No need to invalidate the string rep.
	 */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AttemptSetObjLength --
 *
 *	This function changes the length of the string representation of an
 *	object. It uses the attempt* (non-panic'ing) memory allocators.
 *
 * Results:
 *	1 if the requested memory was allocated, 0 otherwise.
 *
 * Side effects:
 *	If the size of objPtr's string representation is greater than length,
 *	then it is reduced to length and a new terminating null byte is stored
 *	in the strength. If the length of the string representation is greater
 *	than length, the storage space is reallocated to the given length; a
 *	null byte is stored at the end, but other bytes past the end of the
 *	original string representation are undefined. The object's internal
 *	representation is changed to "expendable string".
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AttemptSetObjLength(
    Tcl_Obj *objPtr,		/* Pointer to object. This object must not
				 * currently be shared. */
    int length)			/* Number of bytes desired for string
				 * representation of object, not including
				 * terminating null byte. */
{
    String *stringPtr;

    if (length < 0) {
	/*
	 * Setting to a negative length is nonsense.  This is probably the
	 * result of overflowing the signed integer range.
	 */
	return 0;
    }
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_AttemptSetObjLength");
    }
    if (objPtr->bytes && objPtr->length == length) {
	return 1;
    }

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (objPtr->bytes != NULL) {
	/*
	 * Change length of an existing string rep.
	 */
	if (length > stringPtr->allocated) {
	    /*
	     * Need to enlarge the buffer.
	     */
	    char *newBytes;

	    if (objPtr->bytes == tclEmptyStringRep) {
		newBytes = attemptckalloc((unsigned) length+1);
	    } else {
		newBytes = attemptckrealloc(objPtr->bytes, (unsigned) length+1);
	    }
	    if (newBytes == NULL) {
		return 0;
	    }
	    objPtr->bytes = newBytes;
	    stringPtr->allocated = length;
	}

	objPtr->length = length;
	objPtr->bytes[length] = 0;

	/*
	 * Invalidate the unicode data.
	 */

	stringPtr->numChars = -1;
	stringPtr->hasUnicode = 0;
    } else {
	/*
	 * Changing length of pure unicode string.
	 */

	if (length > STRING_MAXCHARS) {
	    return 0;
	}
	if (length > stringPtr->maxChars) {
	    stringPtr = stringAttemptRealloc(stringPtr, length);
	    if (stringPtr == NULL) {
		return 0;
	    }
	    SET_STRING(objPtr, stringPtr);
	    stringPtr->maxChars = length;
	}

	/* Mark the new end of the unicode string */
	stringPtr->unicode[length] = 0;
	stringPtr->numChars = length;
	stringPtr->hasUnicode = 1;

	/*
	 * Can only get here when objPtr->bytes == NULL.
	 * No need to invalidate the string rep.
	 */
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_SetUnicodeObj --
 *
 *	Modify an object to hold the Unicode string indicated by "unicode".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory allocated for new "String" internal rep.
 *
 *---------------------------------------------------------------------------
 */

void
Tcl_SetUnicodeObj(
    Tcl_Obj *objPtr,		/* The object to set the string of. */
    const Tcl_UniChar *unicode,	/* The unicode string used to initialize the
				 * object. */
    int numChars)		/* Number of characters in the unicode
				 * string. */
{
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetUnicodeObj");
    }
    TclFreeIntRep(objPtr);
    SetUnicodeObj(objPtr, unicode, numChars);
}

static int
UnicodeLength(
    const Tcl_UniChar *unicode)
{
    int numChars = 0;

    if (unicode) {
	while (numChars >= 0 && unicode[numChars] != 0) {
	    numChars++;
	}
    }
    stringCheckLimits(numChars);
    return numChars;
}

static void
SetUnicodeObj(
    Tcl_Obj *objPtr,		/* The object to set the string of. */
    const Tcl_UniChar *unicode,	/* The unicode string used to initialize the
				 * object. */
    int numChars)		/* Number of characters in the unicode
				 * string. */
{
    String *stringPtr;

    if (numChars < 0) {
	numChars = UnicodeLength(unicode);
    }

    /*
     * Allocate enough space for the String structure + Unicode string.
     */

    stringCheckLimits(numChars);
    stringPtr = stringAlloc(numChars);
    SET_STRING(objPtr, stringPtr);
    objPtr->typePtr = &tclStringType;

    stringPtr->maxChars = numChars;
    memcpy(stringPtr->unicode, unicode, numChars * sizeof(Tcl_UniChar));
    stringPtr->unicode[numChars] = 0;
    stringPtr->numChars = numChars;
    stringPtr->hasUnicode = 1;

    TclInvalidateStringRep(objPtr);
    stringPtr->allocated = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendLimitedToObj --
 *
 *	This function appends a limited number of bytes from a sequence of
 *	bytes to an object, marking any limitation with an ellipsis.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bytes at *bytes are appended to the string representation of
 *	objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendLimitedToObj(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const char *bytes,		/* Points to the bytes to append to the
				 * object. */
    int length,			/* The number of bytes available to be
				 * appended from "bytes". If < 0, then all
				 * bytes up to a NUL byte are available. */
    int limit,			/* The maximum number of bytes to append to
				 * the object. */
    const char *ellipsis)	/* Ellipsis marker string, appended to the
				 * object to indicate not all available bytes
				 * at "bytes" were appended. */
{
    String *stringPtr;
    int toCopy = 0;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_AppendLimitedToObj");
    }

    if (length < 0) {
	length = (bytes ? strlen(bytes) : 0);
    }
    if (length == 0) {
	return;
    }

    if (length <= limit) {
	toCopy = length;
    } else {
	if (ellipsis == NULL) {
	    ellipsis = "...";
	}
	toCopy = Tcl_UtfPrev(bytes+limit+1-strlen(ellipsis), bytes) - bytes;
    }

    /*
     * If objPtr has a valid Unicode rep, then append the Unicode conversion
     * of "bytes" to the objPtr's Unicode rep, otherwise append "bytes" to
     * objPtr's string rep.
     */

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (stringPtr->hasUnicode && stringPtr->numChars > 0) {
	AppendUtfToUnicodeRep(objPtr, bytes, toCopy);
    } else {
	AppendUtfToUtfRep(objPtr, bytes, toCopy);
    }

    if (length <= limit) {
	return;
    }

    stringPtr = GET_STRING(objPtr);
    if (stringPtr->hasUnicode && stringPtr->numChars > 0) {
	AppendUtfToUnicodeRep(objPtr, ellipsis, strlen(ellipsis));
    } else {
	AppendUtfToUtfRep(objPtr, ellipsis, strlen(ellipsis));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendToObj --
 *
 *	This function appends a sequence of bytes to an object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bytes at *bytes are appended to the string representation of
 *	objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendToObj(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const char *bytes,		/* Points to the bytes to append to the
				 * object. */
    int length)			/* The number of bytes to append from "bytes".
				 * If < 0, then append all bytes up to NUL
				 * byte. */
{
    Tcl_AppendLimitedToObj(objPtr, bytes, length, INT_MAX, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendUnicodeToObj --
 *
 *	This function appends a Unicode string to an object in the most
 *	efficient manner possible. Length must be >= 0.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invalidates the string rep and creates a new Unicode string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendUnicodeToObj(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const Tcl_UniChar *unicode,	/* The unicode string to append to the
				 * object. */
    int length)			/* Number of chars in "unicode". */
{
    String *stringPtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_AppendUnicodeToObj");
    }

    if (length == 0) {
	return;
    }

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    /*
     * If objPtr has a valid Unicode rep, then append the "unicode" to the
     * objPtr's Unicode rep, otherwise the UTF conversion of "unicode" to
     * objPtr's string rep.
     */

    if (stringPtr->hasUnicode
#if COMPAT
		&& stringPtr->numChars > 0
#endif
	    ) {
	AppendUnicodeToUnicodeRep(objPtr, unicode, length);
    } else {
	AppendUnicodeToUtfRep(objPtr, unicode, length);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendObjToObj --
 *
 *	This function appends the string rep of one object to another.
 *	"objPtr" cannot be a shared object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The string rep of appendObjPtr is appended to the string
 *	representation of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendObjToObj(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    Tcl_Obj *appendObjPtr)	/* Object to append. */
{
    String *stringPtr;
    int length, numChars, appendNumChars = -1;
    const char *bytes;

    /*
     * Handle append of one bytearray object to another as a special case.
     * Note that we only do this when the objects don't have string reps; if
     * it did, then appending the byte arrays together could well lose
     * information; this is a special-case optimization only.
     */

    if (TclIsPureByteArray(objPtr) && TclIsPureByteArray(appendObjPtr)) {
	unsigned char *bytesDst, *bytesSrc;
	int lengthSrc, lengthTotal;

	/*
	 * We do not assume that objPtr and appendObjPtr must be distinct!
	 * This makes this code a bit more complex than it otherwise would be,
	 * but in turn makes it much safer.
	 */

	(void) Tcl_GetByteArrayFromObj(objPtr, &length);
	(void) Tcl_GetByteArrayFromObj(appendObjPtr, &lengthSrc);
	lengthTotal = length + lengthSrc;
	if (((length > lengthSrc) ? length : lengthSrc) > lengthTotal) {
	    Tcl_Panic("overflow when calculating byte array size");
	}
	bytesDst = Tcl_SetByteArrayLength(objPtr, lengthTotal);
	bytesSrc = Tcl_GetByteArrayFromObj(appendObjPtr, NULL);
	memcpy(bytesDst + length, bytesSrc, lengthSrc);
	return;
    }

    /*
     * Must append as strings.
     */

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    /*
     * If objPtr has a valid Unicode rep, then get a Unicode string from
     * appendObjPtr and append it.
     */

    if (stringPtr->hasUnicode 
#if COMPAT
		&& stringPtr->numChars > 0
#endif
	    ) {
	/*
	 * If appendObjPtr is not of the "String" type, don't convert it.
	 */

	if (appendObjPtr->typePtr == &tclStringType) {
	    Tcl_UniChar *unicode =
		    Tcl_GetUnicodeFromObj(appendObjPtr, &numChars);

	    AppendUnicodeToUnicodeRep(objPtr, unicode, numChars);
	} else {
	    bytes = TclGetStringFromObj(appendObjPtr, &length);
	    AppendUtfToUnicodeRep(objPtr, bytes, length);
	}
	return;
    }

    /*
     * Append to objPtr's UTF string rep. If we know the number of characters
     * in both objects before appending, then set the combined number of
     * characters in the final (appended-to) object.
     */

    bytes = TclGetStringFromObj(appendObjPtr, &length);

    numChars = stringPtr->numChars;
    if ((numChars >= 0) && (appendObjPtr->typePtr == &tclStringType)) {
	String *appendStringPtr = GET_STRING(appendObjPtr);
	appendNumChars = appendStringPtr->numChars;
    }

    AppendUtfToUtfRep(objPtr, bytes, length);

    if (numChars >= 0 && appendNumChars >= 0
#if COMPAT
		&& appendNumChars == length
#endif
	    ) {
	stringPtr->numChars = numChars + appendNumChars;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AppendUnicodeToUnicodeRep --
 *
 *	This function appends the contents of "unicode" to the Unicode rep of
 *	"objPtr". objPtr must already have a valid Unicode rep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	objPtr's internal rep is reallocated.
 *
 *----------------------------------------------------------------------
 */

static void
AppendUnicodeToUnicodeRep(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const Tcl_UniChar *unicode,	/* String to append. */
    int appendNumChars)		/* Number of chars of "unicode" to append. */
{
    String *stringPtr;
    int numChars;

    if (appendNumChars < 0) {
	appendNumChars = UnicodeLength(unicode);
    }
    if (appendNumChars == 0) {
	return;
    }

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    /*
     * If not enough space has been allocated for the unicode rep, reallocate
     * the internal rep object with additional space. First try to double the
     * required allocation; if that fails, try a more modest increase. See the
     * "TCL STRING GROWTH ALGORITHM" comment at the top of this file for an
     * explanation of this growth algorithm.
     */

    numChars = stringPtr->numChars + appendNumChars;
    stringCheckLimits(numChars);

    if (numChars > stringPtr->maxChars) {
	/*
	 * Protect against case where unicode points into the existing
	 * stringPtr->unicode array.  Force it to follow any relocations
	 * due to the reallocs below.
	 */
	int offset = -1;
	if (unicode >= stringPtr->unicode
		&& unicode <= stringPtr->unicode + stringPtr->maxChars) {
	    offset = unicode - stringPtr->unicode;
	}

	GrowUnicodeBuffer(objPtr, numChars);
	stringPtr = GET_STRING(objPtr);

	/* Relocate unicode if needed; see above. */
	if (offset >= 0) {
	    unicode = stringPtr->unicode + offset;
	}
    }

    /*
     * Copy the new string onto the end of the old string, then add the
     * trailing null.
     */

    memcpy(stringPtr->unicode + stringPtr->numChars, unicode,
	    appendNumChars * sizeof(Tcl_UniChar));
    stringPtr->unicode[numChars] = 0;
    stringPtr->numChars = numChars;
    stringPtr->allocated = 0;

    TclInvalidateStringRep(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AppendUnicodeToUtfRep --
 *
 *	This function converts the contents of "unicode" to UTF and appends
 *	the UTF to the string rep of "objPtr".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	objPtr's internal rep is reallocated.
 *
 *----------------------------------------------------------------------
 */

static void
AppendUnicodeToUtfRep(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const Tcl_UniChar *unicode,	/* String to convert to UTF. */
    int numChars)		/* Number of chars of "unicode" to convert. */
{
    String *stringPtr = GET_STRING(objPtr);

    numChars = ExtendStringRepWithUnicode(objPtr, unicode, numChars);

    if (stringPtr->numChars != -1) {
	stringPtr->numChars += numChars;
    }

#if COMPAT
    /* Invalidate the unicode rep */
    stringPtr->hasUnicode = 0;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * AppendUtfToUnicodeRep --
 *
 *	This function converts the contents of "bytes" to Unicode and appends
 *	the Unicode to the Unicode rep of "objPtr". objPtr must already have a
 *	valid Unicode rep.  numBytes must be non-negative.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	objPtr's internal rep is reallocated.
 *
 *----------------------------------------------------------------------
 */

static void
AppendUtfToUnicodeRep(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const char *bytes,		/* String to convert to Unicode. */
    int numBytes)		/* Number of bytes of "bytes" to convert. */
{
    String *stringPtr;

    if (numBytes == 0) {
	return;
    }

    ExtendUnicodeRepWithString(objPtr, bytes, numBytes, -1);
    TclInvalidateStringRep(objPtr);
    stringPtr = GET_STRING(objPtr);
    stringPtr->allocated = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendUtfToUtfRep --
 *
 *	This function appends "numBytes" bytes of "bytes" to the UTF string
 *	rep of "objPtr". objPtr must already have a valid String rep.
 *	numBytes must be non-negative.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	objPtr's internal rep is reallocated.
 *
 *----------------------------------------------------------------------
 */

static void
AppendUtfToUtfRep(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    const char *bytes,		/* String to append. */
    int numBytes)		/* Number of bytes of "bytes" to append. */
{
    String *stringPtr;
    int newLength, oldLength;

    if (numBytes == 0) {
	return;
    }

    /*
     * Copy the new string onto the end of the old string, then add the
     * trailing null.
     */

    if (objPtr->bytes == NULL) {
	objPtr->length = 0;
    }
    oldLength = objPtr->length;
    newLength = numBytes + oldLength;
    if (newLength < 0) {
	Tcl_Panic("max size for a Tcl value (%d bytes) exceeded", INT_MAX);
    }

    stringPtr = GET_STRING(objPtr);
    if (newLength > stringPtr->allocated) {
	/*
	 * Protect against case where unicode points into the existing
	 * stringPtr->unicode array.  Force it to follow any relocations
	 * due to the reallocs below.
	 */
	int offset = -1;
	if (bytes >= objPtr->bytes
		&& bytes <= objPtr->bytes + objPtr->length) {
	    offset = bytes - objPtr->bytes;
	}

	/* TODO: consider passing flag=1: no overalloc on first append.
	 * This would make test stringObj-8.1 fail.*/
	GrowStringBuffer(objPtr, newLength, 0);

	/* Relocate bytes if needed; see above. */
	if (offset >= 0) {
	    bytes = objPtr->bytes + offset;
	}
    }

    /*
     * Invalidate the unicode data.
     */

    stringPtr->numChars = -1;
    stringPtr->hasUnicode = 0;

    memcpy(objPtr->bytes + oldLength, bytes, numBytes);
    objPtr->bytes[newLength] = 0;
    objPtr->length = newLength;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendStringsToObjVA --
 *
 *	This function appends one or more null-terminated strings to an
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of all the string arguments are appended to the string
 *	representation of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendStringsToObjVA(
    Tcl_Obj *objPtr,		/* Points to the object to append to. */
    va_list argList)		/* Variable argument list. */
{
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_AppendStringsToObj");
    }

    while (1) {
	const char *bytes = va_arg(argList, char *);
	if (bytes == NULL) {
	    break;
	}
	Tcl_AppendToObj(objPtr, bytes, -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendStringsToObj --
 *
 *	This function appends one or more null-terminated strings to an
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of all the string arguments are appended to the string
 *	representation of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendStringsToObj(
    Tcl_Obj *objPtr,
    ...)
{
    va_list argList;

    va_start(argList, objPtr);
    Tcl_AppendStringsToObjVA(objPtr, argList);
    va_end(argList);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendFormatToObj --
 *
 *	This function appends a list of Tcl_Obj's to a Tcl_Obj according to
 *	the formatting instructions embedded in the format string. The
 *	formatting instructions are inspired by sprintf(). Returns TCL_OK when
 *	successful. If there's an error in the arguments, TCL_ERROR is
 *	returned, and an error message is written to the interp, if non-NULL.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppendFormatToObj(
    Tcl_Interp *interp,
    Tcl_Obj *appendObj,
    const char *format,
    int objc,
    Tcl_Obj *const objv[])
{
    const char *span = format, *msg;
    int numBytes = 0, objIndex = 0, gotXpg = 0, gotSequential = 0;
    int originalLength, limit;
    static const char *mixedXPG =
	    "cannot mix \"%\" and \"%n$\" conversion specifiers";
    static const char *const badIndex[2] = {
	"not enough arguments for all format specifiers",
	"\"%n$\" argument index out of range"
    };
    static const char *overflow = "max size for a Tcl value exceeded";

    if (Tcl_IsShared(appendObj)) {
	Tcl_Panic("%s called with shared object", "Tcl_AppendFormatToObj");
    }
    TclGetStringFromObj(appendObj, &originalLength);
    limit = INT_MAX - originalLength;

    /*
     * Format string is NUL-terminated.
     */

    while (*format != '\0') {
	char *end;
	int gotMinus, gotHash, gotZero, gotSpace, gotPlus, sawFlag;
	int width, gotPrecision, precision, useShort, useWide, useBig;
	int newXpg, numChars, allocSegment = 0, segmentLimit, segmentNumBytes;
	Tcl_Obj *segment;
	Tcl_UniChar ch;
	int step = Tcl_UtfToUniChar(format, &ch);

	format += step;
	if (ch != '%') {
	    numBytes += step;
	    continue;
	}
	if (numBytes) {
	    if (numBytes > limit) {
		msg = overflow;
		goto errorMsg;
	    }
	    Tcl_AppendToObj(appendObj, span, numBytes);
	    limit -= numBytes;
	    numBytes = 0;
	}

	/*
	 * Saw a % : process the format specifier.
	 *
	 * Step 0. Handle special case of escaped format marker (i.e., %%).
	 */

	step = Tcl_UtfToUniChar(format, &ch);
	if (ch == '%') {
	    span = format;
	    numBytes = step;
	    format += step;
	    continue;
	}

	/*
	 * Step 1. XPG3 position specifier
	 */

	newXpg = 0;
	if (isdigit(UCHAR(ch))) {
	    int position = strtoul(format, &end, 10);
	    if (*end == '$') {
		newXpg = 1;
		objIndex = position - 1;
		format = end + 1;
		step = Tcl_UtfToUniChar(format, &ch);
	    }
	}
	if (newXpg) {
	    if (gotSequential) {
		msg = mixedXPG;
		goto errorMsg;
	    }
	    gotXpg = 1;
	} else {
	    if (gotXpg) {
		msg = mixedXPG;
		goto errorMsg;
	    }
	    gotSequential = 1;
	}
	if ((objIndex < 0) || (objIndex >= objc)) {
	    msg = badIndex[gotXpg];
	    goto errorMsg;
	}

	/*
	 * Step 2. Set of flags.
	 */

	gotMinus = gotHash = gotZero = gotSpace = gotPlus = 0;
	sawFlag = 1;
	do {
	    switch (ch) {
	    case '-':
		gotMinus = 1;
		break;
	    case '#':
		gotHash = 1;
		break;
	    case '0':
		gotZero = 1;
		break;
	    case ' ':
		gotSpace = 1;
		break;
	    case '+':
		gotPlus = 1;
		break;
	    default:
		sawFlag = 0;
	    }
	    if (sawFlag) {
		format += step;
		step = Tcl_UtfToUniChar(format, &ch);
	    }
	} while (sawFlag);

	/*
	 * Step 3. Minimum field width.
	 */

	width = 0;
	if (isdigit(UCHAR(ch))) {
	    width = strtoul(format, &end, 10);
	    format = end;
	    step = Tcl_UtfToUniChar(format, &ch);
	} else if (ch == '*') {
	    if (objIndex >= objc - 1) {
		msg = badIndex[gotXpg];
		goto errorMsg;
	    }
	    if (TclGetIntFromObj(interp, objv[objIndex], &width) != TCL_OK) {
		goto error;
	    }
	    if (width < 0) {
		width = -width;
		gotMinus = 1;
	    }
	    objIndex++;
	    format += step;
	    step = Tcl_UtfToUniChar(format, &ch);
	}
	if (width > limit) {
	    msg = overflow;
	    goto errorMsg;
	}

	/*
	 * Step 4. Precision.
	 */

	gotPrecision = precision = 0;
	if (ch == '.') {
	    gotPrecision = 1;
	    format += step;
	    step = Tcl_UtfToUniChar(format, &ch);
	}
	if (isdigit(UCHAR(ch))) {
	    precision = strtoul(format, &end, 10);
	    format = end;
	    step = Tcl_UtfToUniChar(format, &ch);
	} else if (ch == '*') {
	    if (objIndex >= objc - 1) {
		msg = badIndex[gotXpg];
		goto errorMsg;
	    }
	    if (TclGetIntFromObj(interp, objv[objIndex], &precision)
		    != TCL_OK) {
		goto error;
	    }

	    /*
	     * TODO: Check this truncation logic.
	     */

	    if (precision < 0) {
		precision = 0;
	    }
	    objIndex++;
	    format += step;
	    step = Tcl_UtfToUniChar(format, &ch);
	}

	/*
	 * Step 5. Length modifier.
	 */

	useShort = useWide = useBig = 0;
	if (ch == 'h') {
	    useShort = 1;
	    format += step;
	    step = Tcl_UtfToUniChar(format, &ch);
	} else if (ch == 'l') {
	    format += step;
	    step = Tcl_UtfToUniChar(format, &ch);
	    if (ch == 'l') {
		useBig = 1;
		format += step;
		step = Tcl_UtfToUniChar(format, &ch);
	    } else {
#ifndef TCL_WIDE_INT_IS_LONG
		useWide = 1;
#endif
	    }
	}

	format += step;
	span = format;

	/*
	 * Step 6. The actual conversion character.
	 */

	segment = objv[objIndex];
	if (ch == 'i') {
	    ch = 'd';
	}
	switch (ch) {
	case '\0':
	    msg = "format string ended in middle of field specifier";
	    goto errorMsg;
	case 's': {
	    numChars = Tcl_GetCharLength(segment);
	    if (gotPrecision && (precision < numChars)) {
		segment = Tcl_GetRange(segment, 0, precision - 1);
		Tcl_IncrRefCount(segment);
		allocSegment = 1;
	    }
	    break;
	}
	case 'c': {
	    char buf[TCL_UTF_MAX];
	    int code, length;

	    if (TclGetIntFromObj(interp, segment, &code) != TCL_OK) {
		goto error;
	    }
	    length = Tcl_UniCharToUtf(code, buf);
	    segment = Tcl_NewStringObj(buf, length);
	    Tcl_IncrRefCount(segment);
	    allocSegment = 1;
	    break;
	}

	case 'u':
	    if (useBig) {
		msg = "unsigned bignum format is invalid";
		goto errorMsg;
	    }
	case 'd':
	case 'o':
	case 'x':
	case 'X':
	case 'b': {
	    short int s = 0;	/* Silence compiler warning; only defined and
				 * used when useShort is true. */
	    long l;
	    Tcl_WideInt w;
	    mp_int big;
	    int toAppend, isNegative = 0;

	    if (useBig) {
		if (Tcl_GetBignumFromObj(interp, segment, &big) != TCL_OK) {
		    goto error;
		}
		isNegative = (mp_cmp_d(&big, 0) == MP_LT);
	    } else if (useWide) {
		if (Tcl_GetWideIntFromObj(NULL, segment, &w) != TCL_OK) {
		    Tcl_Obj *objPtr;

		    if (Tcl_GetBignumFromObj(interp,segment,&big) != TCL_OK) {
			goto error;
		    }
		    mp_mod_2d(&big, (int) CHAR_BIT*sizeof(Tcl_WideInt), &big);
		    objPtr = Tcl_NewBignumObj(&big);
		    Tcl_IncrRefCount(objPtr);
		    Tcl_GetWideIntFromObj(NULL, objPtr, &w);
		    Tcl_DecrRefCount(objPtr);
		}
		isNegative = (w < (Tcl_WideInt)0);
	    } else if (TclGetLongFromObj(NULL, segment, &l) != TCL_OK) {
		if (Tcl_GetWideIntFromObj(NULL, segment, &w) != TCL_OK) {
		    Tcl_Obj *objPtr;

		    if (Tcl_GetBignumFromObj(interp,segment,&big) != TCL_OK) {
			goto error;
		    }
		    mp_mod_2d(&big, (int) CHAR_BIT * sizeof(long), &big);
		    objPtr = Tcl_NewBignumObj(&big);
		    Tcl_IncrRefCount(objPtr);
		    TclGetLongFromObj(NULL, objPtr, &l);
		    Tcl_DecrRefCount(objPtr);
		} else {
		    l = Tcl_WideAsLong(w);
		}
		if (useShort) {
		    s = (short int) l;
		    isNegative = (s < (short int)0);
		} else {
		    isNegative = (l < (long)0);
		}
	    } else if (useShort) {
		s = (short int) l;
		isNegative = (s < (short int)0);
	    } else {
		isNegative = (l < (long)0);
	    }

	    segment = Tcl_NewObj();
	    allocSegment = 1;
	    segmentLimit = INT_MAX;
	    Tcl_IncrRefCount(segment);

	    if ((isNegative || gotPlus || gotSpace) && (useBig || ch=='d')) {
		Tcl_AppendToObj(segment,
			(isNegative ? "-" : gotPlus ? "+" : " "), 1);
		segmentLimit -= 1;
	    }

	    if (gotHash) {
		switch (ch) {
		case 'o':
		    Tcl_AppendToObj(segment, "0", 1);
		    segmentLimit -= 1;
		    precision--;
		    break;
		case 'x':
		case 'X':
		    Tcl_AppendToObj(segment, "0x", 2);
		    segmentLimit -= 2;
		    break;
		case 'b':
		    Tcl_AppendToObj(segment, "0b", 2);
		    segmentLimit -= 2;
		    break;
		}
	    }

	    switch (ch) {
	    case 'd': {
		int length;
		Tcl_Obj *pure;
		const char *bytes;

		if (useShort) {
		    pure = Tcl_NewIntObj((int) s);
		} else if (useWide) {
		    pure = Tcl_NewWideIntObj(w);
		} else if (useBig) {
		    pure = Tcl_NewBignumObj(&big);
		} else {
		    pure = Tcl_NewLongObj(l);
		}
		Tcl_IncrRefCount(pure);
		bytes = TclGetStringFromObj(pure, &length);

		/*
		 * Already did the sign above.
		 */

		if (*bytes == '-') {
		    length--;
		    bytes++;
		}
		toAppend = length;

		/*
		 * Canonical decimal string reps for integers are composed
		 * entirely of one-byte encoded characters, so "length" is the
		 * number of chars.
		 */

		if (gotPrecision) {
		    if (length < precision) {
			segmentLimit -= (precision - length);
		    }
		    while (length < precision) {
			Tcl_AppendToObj(segment, "0", 1);
			length++;
		    }
		    gotZero = 0;
		}
		if (gotZero) {
		    length += Tcl_GetCharLength(segment);
		    if (length < width) {
			segmentLimit -= (width - length);
		    }
		    while (length < width) {
			Tcl_AppendToObj(segment, "0", 1);
			length++;
		    }
		}
		if (toAppend > segmentLimit) {
		    msg = overflow;
		    goto errorMsg;
		}
		Tcl_AppendToObj(segment, bytes, toAppend);
		Tcl_DecrRefCount(pure);
		break;
	    }

	    case 'u':
	    case 'o':
	    case 'x':
	    case 'X':
	    case 'b': {
		Tcl_WideUInt bits = (Tcl_WideUInt)0;
		Tcl_WideInt numDigits = (Tcl_WideInt)0;
		int length, numBits = 4, base = 16;
		int index = 0, shift = 0;
		Tcl_Obj *pure;
		char *bytes;

		if (ch == 'u') {
		    base = 10;
		} else if (ch == 'o') {
		    base = 8;
		    numBits = 3;
		} else if (ch=='b') {
		    base = 2;
		    numBits = 1;
		}
		if (useShort) {
		    unsigned short int us = (unsigned short int) s;

		    bits = (Tcl_WideUInt) us;
		    while (us) {
			numDigits++;
			us /= base;
		    }
		} else if (useWide) {
		    Tcl_WideUInt uw = (Tcl_WideUInt) w;

		    bits = uw;
		    while (uw) {
			numDigits++;
			uw /= base;
		    }
		} else if (useBig && big.used) {
		    int leftover = (big.used * DIGIT_BIT) % numBits;
		    mp_digit mask = (~(mp_digit)0) << (DIGIT_BIT-leftover);

		    numDigits = 1 +
			    (((Tcl_WideInt)big.used * DIGIT_BIT) / numBits);
		    while ((mask & big.dp[big.used-1]) == 0) {
			numDigits--;
			mask >>= numBits;
		    }
		    if (numDigits > INT_MAX) {
			msg = overflow;
			goto errorMsg;
		    }
		} else if (!useBig) {
		    unsigned long int ul = (unsigned long int) l;

		    bits = (Tcl_WideUInt) ul;
		    while (ul) {
			numDigits++;
			ul /= base;
		    }
		}

		/*
		 * Need to be sure zero becomes "0", not "".
		 */

		if ((numDigits == 0) && !((ch == 'o') && gotHash)) {
		    numDigits = 1;
		}
		pure = Tcl_NewObj();
		Tcl_SetObjLength(pure, (int)numDigits);
		bytes = TclGetString(pure);
		toAppend = length = (int)numDigits;
		while (numDigits--) {
		    int digitOffset;

		    if (useBig && big.used) {
			if ((size_t) shift <
				CHAR_BIT*sizeof(Tcl_WideUInt) - DIGIT_BIT) {
			    bits |= (((Tcl_WideUInt)big.dp[index++]) <<shift);
			    shift += DIGIT_BIT;
			}
			shift -= numBits;
		    }
		    digitOffset = (int) (bits % base);
		    if (digitOffset > 9) {
			bytes[numDigits] = 'a' + digitOffset - 10;
		    } else {
			bytes[numDigits] = '0' + digitOffset;
		    }
		    bits /= base;
		}
		if (useBig) {
		    mp_clear(&big);
		}
		if (gotPrecision) {
		    if (length < precision) {
			segmentLimit -= (precision - length);
		    }
		    while (length < precision) {
			Tcl_AppendToObj(segment, "0", 1);
			length++;
		    }
		    gotZero = 0;
		}
		if (gotZero) {
		    length += Tcl_GetCharLength(segment);
		    if (length < width) {
			segmentLimit -= (width - length);
		    }
		    while (length < width) {
			Tcl_AppendToObj(segment, "0", 1);
			length++;
		    }
		}
		if (toAppend > segmentLimit) {
		    msg = overflow;
		    goto errorMsg;
		}
		Tcl_AppendObjToObj(segment, pure);
		Tcl_DecrRefCount(pure);
		break;
	    }

	    }
	    break;
	}

	case 'e':
	case 'E':
	case 'f':
	case 'g':
	case 'G': {
#define MAX_FLOAT_SIZE 320
	    char spec[2*TCL_INTEGER_SPACE + 9], *p = spec;
	    double d;
	    int length = MAX_FLOAT_SIZE;
	    char *bytes;

	    if (Tcl_GetDoubleFromObj(interp, segment, &d) != TCL_OK) {
		/* TODO: Figure out ACCEPT_NAN here */
		goto error;
	    }
	    *p++ = '%';
	    if (gotMinus) {
		*p++ = '-';
	    }
	    if (gotHash) {
		*p++ = '#';
	    }
	    if (gotZero) {
		*p++ = '0';
	    }
	    if (gotSpace) {
		*p++ = ' ';
	    }
	    if (gotPlus) {
		*p++ = '+';
	    }
	    if (width) {
		p += sprintf(p, "%d", width);
		if (width > length) {
		    length = width;
		} 
	    }
	    if (gotPrecision) {
		*p++ = '.';
		p += sprintf(p, "%d", precision);
		if (precision > INT_MAX - length) {
		    msg=overflow;
		    goto errorMsg;
		}
		length += precision;
	    }

	    /*
	     * Don't pass length modifiers!
	     */

	    *p++ = (char) ch;
	    *p = '\0';

	    segment = Tcl_NewObj();
	    allocSegment = 1;
	    if (!Tcl_AttemptSetObjLength(segment, length)) {
		msg = overflow;
		goto errorMsg;
	    }
	    bytes = TclGetString(segment);
	    if (!Tcl_AttemptSetObjLength(segment, sprintf(bytes, spec, d))) {
		msg = overflow;
		goto errorMsg;
	    }
	    break;
	}
	default:
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
			Tcl_ObjPrintf("bad field specifier \"%c\"", ch));
	    }
	    goto error;
	}

	switch (ch) {
	case 'E':
	case 'G':
	case 'X': {
	    Tcl_SetObjLength(segment, Tcl_UtfToUpper(TclGetString(segment)));
	}
	}

	numChars = Tcl_GetCharLength(segment);
	if (!gotMinus) {
	    if (numChars < width) {
		limit -= (width - numChars);
	    }
	    while (numChars < width) {
		Tcl_AppendToObj(appendObj, (gotZero ? "0" : " "), 1);
		numChars++;
	    }
	}

	Tcl_GetStringFromObj(segment, &segmentNumBytes);
	if (segmentNumBytes > limit) {
	    msg = overflow;
	    goto errorMsg;
	}
	Tcl_AppendObjToObj(appendObj, segment);
	limit -= segmentNumBytes;
	if (allocSegment) {
	    Tcl_DecrRefCount(segment);
	}
	if (numChars < width) {
	    limit -= (width - numChars);
	}
	while (numChars < width) {
	    Tcl_AppendToObj(appendObj, (gotZero ? "0" : " "), 1);
	    numChars++;
	}

	objIndex += gotSequential;
    }
    if (numBytes) {
	if (numBytes > limit) {
	    msg = overflow;
	    goto errorMsg;
	}
	Tcl_AppendToObj(appendObj, span, numBytes);
	limit -= numBytes;
	numBytes = 0;
    }

    return TCL_OK;

  errorMsg:
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(msg, -1));
    }
  error:
    Tcl_SetObjLength(appendObj, originalLength);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_Format--
 *
 * Results:
 *	A refcount zero Tcl_Obj.
 *
 * Side effects:
 * 	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_Format(
    Tcl_Interp *interp,
    const char *format,
    int objc,
    Tcl_Obj *const objv[])
{
    int result;
    Tcl_Obj *objPtr = Tcl_NewObj();
    result = Tcl_AppendFormatToObj(interp, objPtr, format, objc, objv);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(objPtr);
	return NULL;
    }
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * AppendPrintfToObjVA --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */

static void
AppendPrintfToObjVA(
    Tcl_Obj *objPtr,
    const char *format,
    va_list argList)
{
    int code, objc;
    Tcl_Obj **objv, *list = Tcl_NewObj();
    const char *p;
    char *end;

    p = format;
    Tcl_IncrRefCount(list);
    while (*p != '\0') {
	int size = 0, seekingConversion = 1, gotPrecision = 0;
	int lastNum = -1;

	if (*p++ != '%') {
	    continue;
	}
	if (*p == '%') {
	    p++;
	    continue;
	}
	do {
	    switch (*p) {

	    case '\0':
		seekingConversion = 0;
		break;
	    case 's': {
		const char *q, *end, *bytes = va_arg(argList, char *);
		seekingConversion = 0;

		/*
		 * The buffer to copy characters from starts at bytes and ends
		 * at either the first NUL byte, or after lastNum bytes, when
		 * caller has indicated a limit.
		 */

		end = bytes;
		while ((!gotPrecision || lastNum--) && (*end != '\0')) {
		    end++;
		}

		/*
		 * Within that buffer, we trim both ends if needed so that we
		 * copy only whole characters, and avoid copying any partial
		 * multi-byte characters.
		 */

		q = Tcl_UtfPrev(end, bytes);
		if (!Tcl_UtfCharComplete(q, (int)(end - q))) {
		    end = q;
		}

		q = bytes + TCL_UTF_MAX;
		while ((bytes < end) && (bytes < q)
			&& ((*bytes & 0xC0) == 0x80)) {
		    bytes++;
		}

		Tcl_ListObjAppendElement(NULL, list,
			Tcl_NewStringObj(bytes , (int)(end - bytes)));

		break;
	    }
	    case 'c':
	    case 'i':
	    case 'u':
	    case 'd':
	    case 'o':
	    case 'x':
	    case 'X':
		seekingConversion = 0;
		switch (size) {
		case -1:
		case 0:
		    Tcl_ListObjAppendElement(NULL, list, Tcl_NewLongObj(
			    (long int)va_arg(argList, int)));
		    break;
		case 1:
		    Tcl_ListObjAppendElement(NULL, list, Tcl_NewLongObj(
			    va_arg(argList, long int)));
		    break;
		}
		break;
	    case 'e':
	    case 'E':
	    case 'f':
	    case 'g':
	    case 'G':
		Tcl_ListObjAppendElement(NULL, list, Tcl_NewDoubleObj(
			va_arg(argList, double)));
		seekingConversion = 0;
		break;
	    case '*':
		lastNum = (int)va_arg(argList, int);
		Tcl_ListObjAppendElement(NULL, list, Tcl_NewIntObj(lastNum));
		p++;
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		lastNum = (int) strtoul(p, &end, 10);
		p = end;
		break;
	    case '.':
		gotPrecision = 1;
		p++;
		break;
	    /* TODO: support for wide (and bignum?) arguments */
	    case 'l':
		size = 1;
		p++;
		break;
	    case 'h':
		size = -1;
	    default:
		p++;
	    }
	} while (seekingConversion);
    }
    TclListObjGetElements(NULL, list, &objc, &objv);
    code = Tcl_AppendFormatToObj(NULL, objPtr, format, objc, objv);
    if (code != TCL_OK) {
	Tcl_AppendPrintfToObj(objPtr,
		"Unable to format \"%s\" with supplied arguments: %s",
		format, Tcl_GetString(list));
    }
    Tcl_DecrRefCount(list);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_AppendPrintfToObj --
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 * 	None.
 *
 *---------------------------------------------------------------------------
 */

void
Tcl_AppendPrintfToObj(
    Tcl_Obj *objPtr,
    const char *format,
    ...)
{
    va_list argList;

    va_start(argList, format);
    AppendPrintfToObjVA(objPtr, format, argList);
    va_end(argList);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_ObjPrintf --
 *
 * Results:
 *	A refcount zero Tcl_Obj.
 *
 * Side effects:
 * 	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_ObjPrintf(
    const char *format,
    ...)
{
    va_list argList;
    Tcl_Obj *objPtr = Tcl_NewObj();

    va_start(argList, format);
    AppendPrintfToObjVA(objPtr, format, argList);
    va_end(argList);
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclStringObjReverse --
 *
 *	Implements the [string reverse] operation.
 *
 * Results:
 *	An unshared Tcl value which is the [string reverse] of the argument
 *	supplied.  When sharing rules permit, the returned value might be
 *	the argument with modifications done in place.
 *
 * Side effects:
 *	May allocate a new Tcl_Obj.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclStringObjReverse(
    Tcl_Obj *objPtr)
{
    String *stringPtr;
    char *src = NULL, *dest = NULL;
    Tcl_UniChar *usrc = NULL, *udest = NULL;
    Tcl_Obj *resultPtr = NULL;

    SetStringFromAny(NULL, objPtr);
    stringPtr = GET_STRING(objPtr);

    if (stringPtr->hasUnicode == 0) {
	if (stringPtr->numChars == -1) {
	    TclNumUtfChars(stringPtr->numChars, objPtr->bytes, objPtr->length);
	}
	if (stringPtr->numChars <= 1) {
	    return objPtr;
	}
	if (stringPtr->numChars == objPtr->length) {
	    /* All one-byte chars.  Reverse in objPtr->bytes. */
	    if (Tcl_IsShared(objPtr)) {
		resultPtr = Tcl_NewObj();
		Tcl_SetObjLength(resultPtr, objPtr->length);
		dest = TclGetString(resultPtr);
		src = objPtr->bytes + objPtr->length - 1;
		while (src >= objPtr->bytes) {
		    *dest++ = *src--;
		}
		return resultPtr;
	    }
	    /* Unshared.  Reverse objPtr->bytes in place. */
	    dest = objPtr->bytes;
	    src = dest + objPtr->length - 1;
	    while (dest < src) {
		char tmp = *src;
		*src-- = *dest;
		*dest++ = tmp;
	    }
	    return objPtr;
	}
	FillUnicodeRep(objPtr);
	stringPtr = GET_STRING(objPtr);
    }
    if (stringPtr->numChars <= 1) {
	return objPtr;
    }

    /* Reverse the Unicode rep. */
    if (Tcl_IsShared(objPtr)) {
	Tcl_UniChar ch = 0;

	/*
	 * Create a non-empty, pure unicode value, so we can coax
	 * Tcl_SetObjLength into growing the unicode rep buffer.
	 */

	resultPtr = Tcl_NewUnicodeObj(&ch, 1);
	Tcl_SetObjLength(resultPtr, stringPtr->numChars);
	udest = Tcl_GetUnicode(resultPtr);
	usrc = stringPtr->unicode + stringPtr->numChars - 1;
	while (usrc >= stringPtr->unicode) {
	    *udest++ = *usrc--;
	}
	return resultPtr;
    }

    /* Unshared.  Reverse objPtr->bytes in place. */
    udest = stringPtr->unicode;
    usrc = udest + stringPtr->numChars - 1;
    while (udest < usrc) {
	Tcl_UniChar tmp = *usrc;
	*usrc-- = *udest;
	*udest++ = tmp;
    }

    TclInvalidateStringRep(objPtr);
    stringPtr->allocated = 0;
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * FillUnicodeRep --
 *
 *	Populate the Unicode internal rep with the Unicode form of its string
 *	rep. The object must alread have a "String" internal rep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Reallocates the String internal rep.
 *
 *---------------------------------------------------------------------------
 */

static void
FillUnicodeRep(
    Tcl_Obj *objPtr)		/* The object in which to fill the unicode
				 * rep. */
{
    String *stringPtr = GET_STRING(objPtr);
    ExtendUnicodeRepWithString(objPtr, objPtr->bytes, objPtr->length,
	    stringPtr->numChars);
}

static void
ExtendUnicodeRepWithString(
    Tcl_Obj *objPtr,
    const char *bytes,
    int numBytes,
    int numAppendChars)
{
    String *stringPtr = GET_STRING(objPtr);
    int needed, numOrigChars = 0;
    Tcl_UniChar *dst;

    if (stringPtr->hasUnicode) {
	numOrigChars = stringPtr->numChars;
    }
    if (numAppendChars == -1) {
	TclNumUtfChars(numAppendChars, bytes, numBytes);
    }
    needed = numOrigChars + numAppendChars;
    stringCheckLimits(needed);
	
    if (needed > stringPtr->maxChars) {
	GrowUnicodeBuffer(objPtr, needed);
	stringPtr = GET_STRING(objPtr);
    }

    stringPtr->hasUnicode = 1;
    stringPtr->numChars = needed;
    for (dst=stringPtr->unicode + numOrigChars; numAppendChars-- > 0; dst++) {
	bytes += TclUtfToUniChar(bytes, dst);
    }
    *dst = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * DupStringInternalRep --
 *
 *	Initialize the internal representation of a new Tcl_Obj to a copy of
 *	the internal representation of an existing string object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	copyPtr's internal rep is set to a copy of srcPtr's internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static void
DupStringInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. Must have
				 * an internal rep of type "String". */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. Must not
				 * currently have an internal rep.*/
{
    String *srcStringPtr = GET_STRING(srcPtr);
    String *copyStringPtr = NULL;

#if COMPAT==0
    if (srcStringPtr->numChars == -1) {
	/*
	 * The String struct in the source value holds zero useful data.
	 * Don't bother copying it.  Don't even bother allocating space in
	 * which to copy it.  Just let the copy be untyped.
	 */
	return;
    }

    if (srcStringPtr->hasUnicode) {
	int copyMaxChars;
	if (srcStringPtr->maxChars / 2 >= srcStringPtr->numChars) {
	    copyMaxChars = 2 * srcStringPtr->numChars;
	} else {
	    copyMaxChars = srcStringPtr->maxChars;
	}
	copyStringPtr = stringAlloc(copyMaxChars);
	copyStringPtr->maxChars = copyMaxChars;
	memcpy(copyStringPtr->unicode, srcStringPtr->unicode,
		srcStringPtr->numChars * sizeof(Tcl_UniChar));
	copyStringPtr->unicode[srcStringPtr->numChars] = 0;
    } else {
	copyStringPtr = stringAlloc(0);
	copyStringPtr->maxChars = 0;
	copyStringPtr->unicode[0] = 0;
    }
    copyStringPtr->hasUnicode = srcStringPtr->hasUnicode;
    copyStringPtr->numChars = srcStringPtr->numChars;

    /*
     * Tricky point: the string value was copied by generic object
     * management code, so it doesn't contain any extra bytes that
     * might exist in the source object.
     */
    copyStringPtr->allocated = copyPtr->bytes ? copyPtr->length : 0;
#else
    /*
     * If the src obj is a string of 1-byte Utf chars, then copy the string
     * rep of the source object and create an "empty" Unicode internal rep for
     * the new object. Otherwise, copy Unicode internal rep, and invalidate
     * the string rep of the new object.
     */

    if (srcStringPtr->hasUnicode && srcStringPtr->numChars > 0) {
	/* Copy the full allocation for the Unicode buffer. */
	copyStringPtr = stringAlloc(srcStringPtr->maxChars);
	copyStringPtr->maxChars = srcStringPtr->maxChars;
	memcpy(copyStringPtr->unicode, srcStringPtr->unicode,
		srcStringPtr->numChars * sizeof(Tcl_UniChar));
	copyStringPtr->unicode[srcStringPtr->numChars] = 0;
	copyStringPtr->allocated = 0;
    } else {
	copyStringPtr = stringAlloc(0);
	copyStringPtr->unicode[0] = 0;
	copyStringPtr->maxChars = 0;
	/*
	 * Tricky point: the string value was copied by generic object
	 * management code, so it doesn't contain any extra bytes that
	 * might exist in the source object.
	 */
	copyStringPtr->allocated = copyPtr->length;
    }
    copyStringPtr->numChars = srcStringPtr->numChars;
    copyStringPtr->hasUnicode = srcStringPtr->hasUnicode;
#endif

    SET_STRING(copyPtr, copyStringPtr);
    copyPtr->typePtr = &tclStringType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetStringFromAny --
 *
 *	Create an internal representation of type "String" for an object.
 *
 * Results:
 *	This operation always succeeds and returns TCL_OK.
 *
 * Side effects:
 *	Any old internal reputation for objPtr is freed and the internal
 *	representation is set to "String".
 *
 *----------------------------------------------------------------------
 */

static int
SetStringFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr)		/* The object to convert. */
{
    if (objPtr->typePtr != &tclStringType) {
	String *stringPtr = stringAlloc(0);

	/*
	 * Convert whatever we have into an untyped value.  Just A String.
	 */

	(void) TclGetString(objPtr);
	TclFreeIntRep(objPtr);

	/*
	 * Create a basic String intrep that just points to the UTF-8 string
	 * already in place at objPtr->bytes.
	 */

	stringPtr->numChars = -1;
	stringPtr->allocated = objPtr->length;
	stringPtr->maxChars = 0;
	stringPtr->hasUnicode = 0;
	SET_STRING(objPtr, stringPtr);
	objPtr->typePtr = &tclStringType;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfString --
 *
 *	Update the string representation for an object whose internal
 *	representation is "String".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string may be set by converting its Unicode represention
 *	to UTF format.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfString(
    Tcl_Obj *objPtr)		/* Object with string rep to update. */
{
    String *stringPtr = GET_STRING(objPtr);
    if (stringPtr->numChars == 0) {
	TclInitStringRep(objPtr, tclEmptyStringRep, 0);
    } else {
	(void) ExtendStringRepWithUnicode(objPtr, stringPtr->unicode,
		stringPtr->numChars);
    }
}

static int
ExtendStringRepWithUnicode(
    Tcl_Obj *objPtr,
    const Tcl_UniChar *unicode,
    int numChars)
{
    int i, origLength, size = 0;	
    char *dst, buf[TCL_UTF_MAX];

    /* Pre-condition: this is the "string" Tcl_ObjType */
    String *stringPtr = GET_STRING(objPtr);

    if (numChars < 0) {
	numChars = UnicodeLength(unicode);
    }

    if (numChars == 0) {
	return 0;
    }

    if (objPtr->bytes == NULL) {
	objPtr->length = 0;
    }
    size = origLength = objPtr->length;
    
    /* Quick cheap check in case we have more than enough room. */
    if (numChars <= (INT_MAX - size)/TCL_UTF_MAX 
	    && stringPtr->allocated >= size + numChars * TCL_UTF_MAX) {
	goto copyBytes;
    }

    for (i = 0; i < numChars && size >= 0; i++) {
	size += Tcl_UniCharToUtf((int) unicode[i], buf);
    }
    if (size < 0) {
	Tcl_Panic("max size for a Tcl value (%d bytes) exceeded", INT_MAX);
    }

    /* Grow space if needed */
    if (size > stringPtr->allocated) {
	GrowStringBuffer(objPtr, size, 1);
    }

    copyBytes:
    dst = objPtr->bytes + origLength;
    for (i = 0; i < numChars; i++) {
	dst += Tcl_UniCharToUtf((int) unicode[i], dst);
    }
    *dst = '\0';
    objPtr->length = dst - objPtr->bytes;
    return numChars;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeStringInternalRep --
 *
 *	Deallocate the storage associated with a String data object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *----------------------------------------------------------------------
 */

static void
FreeStringInternalRep(
    Tcl_Obj *objPtr)		/* Object with internal rep to free. */
{
    ckfree((char *) GET_STRING(objPtr));
    objPtr->typePtr = NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
