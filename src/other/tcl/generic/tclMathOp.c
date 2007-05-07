/*
 * tclMathOp.c --
 *
 *	This file contains normal command versions of the contents of the
 *	tcl::mathop namespace.
 *
 * Copyright (c) 2006 by Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

/*
 * NOTE: None of the routines in this file are currently in use.
 * The file itself may be removed, but remains in place for now in
 * case its routine may be useful during performance testing.
 */

#if 0

#include "tclInt.h"
#include "tclCompile.h"
#include "tommath.h"
#include <math.h>
#include <float.h>

/*
 * Hack to determine whether we may expect IEEE floating point. The hack is
 * formally incorrect in that non-IEEE platforms might have the same precision
 * and range, but VAX, IBM, and Cray do not; are there any other floating
 * point units that we might care about?
 */

#if (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MAX_EXP == 1024)
#define IEEE_FLOATING_POINT
#endif

/*
 * The stuff below is a bit of a hack so that this file can be used in
 * environments that include no UNIX.
 * TODO: Does this serve any purpose anymore?
 */

#ifdef TCL_GENERIC_ONLY
#   ifndef NO_FLOAT_H
#	include <float.h>
#   else /* NO_FLOAT_H */
#	ifndef NO_VALUES_H
#	    include <values.h>
#	endif /* !NO_VALUES_H */
#   endif /* !NO_FLOAT_H */
#endif /* !TCL_GENERIC_ONLY */

/*
 * Prototypes for helper functions defined in this file:
 */

static Tcl_Obj *	CombineIntFloat(Tcl_Interp *interp, Tcl_Obj *valuePtr,
			    int opcode, Tcl_Obj *value2Ptr);
static Tcl_Obj *	CombineIntOnly(Tcl_Interp *interp, Tcl_Obj *valuePtr,
			    int opcode, Tcl_Obj *value2Ptr);
static int		CompareNumbers(Tcl_Interp *interp, Tcl_Obj *numObj1,
			    Tcl_Obj *numObj2, int *resultPtr);

/*
 *----------------------------------------------------------------------
 *
 * CombineIntFloat --
 *
 *	Parses and combines two numbers (either entier() or double())
 *	according to the specified operation.
 *
 * Results:
 * 	Returns the resulting number object (or NULL on failure).
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	This code originally extracted from tclExecute.c.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
CombineIntFloat(
    Tcl_Interp *interp,		/* Place to write error messages. */
    Tcl_Obj *valuePtr,		/* First value to combine. */
    int opcode,			/* Operation to use to combine the
				 * values. Must be one of INST_ADD, INST_SUB,
				 * INST_MULT, INST_DIV or INST_EXPON. */
    Tcl_Obj *value2Ptr)		/* Second value to combine. */
{
    ClientData ptr1, ptr2;
    int type1, type2;
    Tcl_Obj *errPtr;

    if ((TclGetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
#ifndef ACCEPT_NAN
	    || (type1 == TCL_NUMBER_NAN)
#endif
	    ) {
	errPtr = valuePtr;
	goto illegalOperand;
    }

#ifdef ACCEPT_NAN
    if (type1 == TCL_NUMBER_NAN) {
	/* NaN first argument -> result is also NaN */
	NEXT_INST_F(1, 1, 0);
    }
#endif

    if ((TclGetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK)
#ifndef ACCEPT_NAN
	    || (type2 == TCL_NUMBER_NAN)
#endif
	    ) {
	errPtr = value2Ptr;
	goto illegalOperand;
    }

#ifdef ACCEPT_NAN
    if (type2 == TCL_NUMBER_NAN) {
	/* NaN second argument -> result is also NaN */
	return value2Ptr;
	NEXT_INST_F(1, 2, 1);
    }
#endif

    if ((type1 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_DOUBLE)) {
	/*
	 * At least one of the values is floating-point, so perform floating
	 * point calculations.
	 */

	double d1, d2, dResult;
	Tcl_GetDoubleFromObj(NULL, valuePtr, &d1);
	Tcl_GetDoubleFromObj(NULL, value2Ptr, &d2);

	switch (opcode) {
	case INST_ADD:
	    dResult = d1 + d2;
	    break;
	case INST_SUB:
	    dResult = d1 - d2;
	    break;
	case INST_MULT:
	    dResult = d1 * d2;
	    break;
	case INST_DIV:
#ifndef IEEE_FLOATING_POINT
	    if (d2 == 0.0) {
		goto divideByZero;
	    }
#endif
	    /*
	     * We presume that we are running with zero-divide unmasked if
	     * we're on an IEEE box. Otherwise, this statement might cause
	     * demons to fly out our noses.
	     */

	    dResult = d1 / d2;
	    break;
	case INST_EXPON:
	    if (d1==0.0 && d2<0.0) {
		goto exponOfZero;
	    }
	    dResult = pow(d1, d2);
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    dResult = 0;
	}

#ifndef ACCEPT_NAN
	/*
	 * Check now for IEEE floating-point error.
	 */

	if (TclIsNaN(dResult)) {
	    TclExprFloatError(interp, dResult);
	    return NULL;
	}
#endif
	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewDoubleObj(dResult);
	}
	Tcl_SetDoubleObj(valuePtr, dResult);
	return valuePtr;
    }

    if ((sizeof(long) >= 2*sizeof(int)) && (opcode == INST_MULT)
	    && (type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	long l1 = *((CONST long *)ptr1);
	long l2 = *((CONST long *)ptr2);
	if ((l1 <= INT_MAX) && (l1 >= INT_MIN)
		&& (l2 <= INT_MAX) && (l2 >= INT_MIN)) {
	    long lResult = l1 * l2;

	    if (Tcl_IsShared(valuePtr)) {
		return Tcl_NewLongObj(lResult);
	    }
	    Tcl_SetLongObj(valuePtr, lResult);
	    return valuePtr;
	}
    }

    if ((sizeof(Tcl_WideInt) >= 2*sizeof(long)) && (opcode == INST_MULT)
	    && (type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	Tcl_WideInt w1, w2, wResult;
	Tcl_GetWideIntFromObj(NULL, valuePtr, &w1);
	Tcl_GetWideIntFromObj(NULL, value2Ptr, &w2);

	wResult = w1 * w2;

	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewWideIntObj(wResult);
	}
	Tcl_SetWideIntObj(valuePtr, wResult);
	return valuePtr;
    }

    /* TODO: Attempts to re-use unshared operands on stack */
    if (opcode == INST_EXPON) {
	long l1, l2 = 0;
	int oddExponent = 0, negativeExponent = 0;
	if (type2 == TCL_NUMBER_LONG) {
	    l2 = *((CONST long *)ptr2);
	    if (l2 == 0) {
		/* Anything to the zero power is 1 */
		return Tcl_NewIntObj(1);
	    }
	}
	switch (type2) {
	case TCL_NUMBER_LONG: {
	    negativeExponent = (l2 < 0);
	    oddExponent = (int) (l2 & 1);
	    break;
	}
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE: {
	    Tcl_WideInt w2 = *((CONST Tcl_WideInt *)ptr2);
	    negativeExponent = (w2 < 0);
	    oddExponent = (int) (w2 & (Tcl_WideInt)1);
	    break;
	}
#endif
	case TCL_NUMBER_BIG: {
	    mp_int big2;
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    negativeExponent = (mp_cmp_d(&big2, 0) == MP_LT);
	    mp_mod_2d(&big2, 1, &big2);
	    oddExponent = !mp_iszero(&big2);
	    mp_clear(&big2);
	    break;
	}
	}

	if (negativeExponent) {
	    if (type1 == TCL_NUMBER_LONG) {
		l1 = *((CONST long *)ptr1);
		switch (l1) {
		case 0:
		    /* zero to a negative power is div by zero error */
		    goto exponOfZero;
		case -1:
		    if (oddExponent) {
			return Tcl_NewIntObj(-1);
		    } else {
			return Tcl_NewIntObj(1);
		    }
		case 1:
		    /* 1 to any power is 1 */
		    return Tcl_NewIntObj(1);
		}
	    }
	    /*
	     * Integers with magnitude greater than 1 raise to a negative
	     * power yield the answer zero (see TIP 123)
	     */
	    return Tcl_NewIntObj(0);
	}

	if (type1 == TCL_NUMBER_LONG) {
	    l1 = *((CONST long *)ptr1);
	    switch (l1) {
	    case 0:
		/* zero to a positive power is zero */
		return Tcl_NewIntObj(0);
	    case 1:
		/* 1 to any power is 1 */
		return Tcl_NewIntObj(1);
	    case -1:
		if (oddExponent) {
		    return Tcl_NewIntObj(-1);
		} else {
		    return Tcl_NewIntObj(1);
		}
	    }
	}
	if (type2 == TCL_NUMBER_BIG) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("exponent too large", -1));
	    return NULL;
	}
	/* TODO: Perform those computations that fit in native types */
	goto overflow;
    }

    if ((opcode != INST_MULT)
	    && (type1 != TCL_NUMBER_BIG) && (type2 != TCL_NUMBER_BIG)) {
	Tcl_WideInt w1, w2, wResult;
	Tcl_GetWideIntFromObj(NULL, valuePtr, &w1);
	Tcl_GetWideIntFromObj(NULL, value2Ptr, &w2);

	switch (opcode) {
	case INST_ADD:
	    wResult = w1 + w2;
#ifndef NO_WIDE_TYPE
	    if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
	    {
		/* Check for overflow */
		if (((w1 < 0) && (w2 < 0) && (wResult >= 0))
			|| ((w1 > 0) && (w2 > 0) && (wResult < 0))) {
		    goto overflow;
		}
	    }
	    break;

	case INST_SUB:
	    wResult = w1 - w2;
#ifndef NO_WIDE_TYPE
	    if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
	    {
		/* Must check for overflow */
		if (((w1 < 0) && (w2 > 0) && (wResult > 0))
			|| ((w1 >= 0) && (w2 < 0) && (wResult < 0))) {
		    goto overflow;
		}
	    }
	    break;

	case INST_DIV:
	    if (w2 == 0) {
		goto divideByZero;
	    }

	    /* Need a bignum to represent (LLONG_MIN / -1) */
	    if ((w1 == LLONG_MIN) && (w2 == -1)) {
		goto overflow;
	    }
	    wResult = w1 / w2;

	    /* Force Tcl's integer division rules */
	    /* TODO: examine for logic simplification */
	    if (((wResult < 0) || ((wResult == 0) &&
		    ((w1 < 0 && w2 > 0) || (w1 > 0 && w2 < 0)))) &&
		    ((wResult * w2) != w1)) {
		wResult -= 1;
	    }
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    wResult = 0;
	}

	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewWideIntObj(wResult);
	}
	Tcl_SetWideIntObj(valuePtr, wResult);
	return valuePtr;
    }

  overflow:
    {
	mp_int big1, big2, bigResult, bigRemainder;
	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	mp_init(&bigResult);
	switch (opcode) {
	case INST_ADD:
	    mp_add(&big1, &big2, &bigResult);
	    break;
	case INST_SUB:
	    mp_sub(&big1, &big2, &bigResult);
	    break;
	case INST_MULT:
	    mp_mul(&big1, &big2, &bigResult);
	    break;
	case INST_DIV:
	    if (mp_iszero(&big2)) {
		mp_clear(&big1);
		mp_clear(&big2);
		goto divideByZero;
	    }
	    mp_init(&bigRemainder);
	    mp_div(&big1, &big2, &bigResult, &bigRemainder);
	    /* TODO: internals intrusion */
	    if (!mp_iszero(&bigRemainder)
		    && (bigRemainder.sign != big2.sign)) {
		/* Convert to Tcl's integer division rules */
		mp_sub_d(&bigResult, 1, &bigResult);
		mp_add(&bigRemainder, &big2, &bigRemainder);
	    }
	    mp_clear(&bigRemainder);
	    break;
	case INST_EXPON:
	    if (big2.used > 1) {
		Tcl_SetObjResult(interp,
			Tcl_NewStringObj("exponent too large", -1));
		mp_clear(&big1);
		mp_clear(&big2);
		return NULL;
	    }
	    mp_expt_d(&big1, big2.dp[0], &bigResult);
	    break;
	}
	mp_clear(&big1);
	mp_clear(&big2);
	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewBignumObj(&bigResult);
	}
	Tcl_SetBignumObj(valuePtr, &bigResult);
	return valuePtr;
    }

    {
	const char *description, *operator;

    illegalOperand:
	switch (opcode) {
	case INST_ADD:	 operator = "+";  break;
	case INST_SUB:	 operator = "-";  break;
	case INST_MULT:	 operator = "*";  break;
	case INST_DIV:	 operator = "/";  break;
	case INST_EXPON: operator = "**"; break;
	default:
	    operator = "???";
	}

	if (TclGetNumberFromObj(NULL, errPtr, &ptr1, &type1) != TCL_OK) {
	    int numBytes;
	    CONST char *bytes = Tcl_GetStringFromObj(errPtr, &numBytes);
	    if (numBytes == 0) {
		description = "empty string";
	    } else if (TclCheckBadOctal(NULL, bytes)) {
		description = "invalid octal number";
	    } else {
		description = "non-numeric string";
	    }
	} else if (type1 == TCL_NUMBER_NAN) {
	    description = "non-numeric floating-point value";
	} else if (type1 == TCL_NUMBER_DOUBLE) {
	    description = "floating-point value";
	} else {
	    /* TODO: No caller needs this.  Eliminate? */
	    description = "(big) integer";
	}

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't use %s as operand of \"%s\"", description, operator));
	return NULL;
    }

  divideByZero:
    Tcl_SetObjResult(interp, Tcl_NewStringObj("divide by zero", -1));
    Tcl_SetErrorCode(interp, "ARITH", "DIVZERO", "divide by zero", NULL);
    return NULL;

  exponOfZero:
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "exponentiation of zero by negative power", -1));
    Tcl_SetErrorCode(interp, "ARITH", "DOMAIN",
	    "exponentiation of zero by negative power", NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CombineIntOnly --
 *
 *	Parses and combines two numbers (must be entier()) according to the
 *	specified operation.
 *
 * Results:
 * 	Returns the resulting number object (or NULL on failure).
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	This code originally extracted from tclExecute.c.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
CombineIntOnly(
    Tcl_Interp *interp,		/* Place to write error messages. */
    Tcl_Obj *valuePtr,		/* First value to combine. */
    int opcode,			/* Operation to use to combine the
				 * values. Must be one of INST_BITAND,
				 * INST_BITOR or INST_BITXOR. */
    Tcl_Obj *value2Ptr)		/* Second value to combine. */
{
    ClientData ptr1, ptr2;
    int type1, type2;
    Tcl_Obj *errPtr;

    if ((TclGetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
	    || (type1 == TCL_NUMBER_NAN) || (type1 == TCL_NUMBER_DOUBLE)) {
	errPtr = valuePtr;
	goto illegalOperand;
    }
    if ((TclGetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK)
	    || (type2 == TCL_NUMBER_NAN) || (type2 == TCL_NUMBER_DOUBLE)) {
	errPtr = value2Ptr;
	goto illegalOperand;
    }

    if ((type1 == TCL_NUMBER_BIG) || (type2 == TCL_NUMBER_BIG)) {
	mp_int big1, big2, bigResult;
	mp_int *First, *Second;
	int numPos;


	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);

	/*
	 * Count how many positive arguments we have. If only one of the
	 * arguments is negative, store it in 'Second'.
	 */

	if (mp_cmp_d(&big1, 0) != MP_LT) {
	    numPos = 1 + (mp_cmp_d(&big2, 0) != MP_LT);
	    First  = &big1;
	    Second = &big2;
	} else {
	    First  = &big2;
	    Second = &big1;
	    numPos = (mp_cmp_d(First, 0) != MP_LT);
	}
	mp_init(&bigResult);

	switch (opcode) {
	case INST_BITAND:
	    switch (numPos) {
	    case 2:
		/* Both arguments positive, base case */
		mp_and(First, Second, &bigResult);
		break;
	    case 1:
		/* First is positive; Second negative
		 * P & N = P & ~~N = P&~(-N-1) = P & (P ^ (-N-1)) */
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_xor(First, Second, &bigResult);
		mp_and(First, &bigResult, &bigResult);
		break;
	    case 0:
		/* Both arguments negative
		 * a & b = ~ (~a | ~b) = -(-a-1|-b-1)-1 */
		mp_neg(First, First);
		mp_sub_d(First, 1, First);
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_or(First, Second, &bigResult);
		mp_neg(&bigResult, &bigResult);
		mp_sub_d(&bigResult, 1, &bigResult);
		break;
	    }
	    break;

	case INST_BITOR:
	    switch (numPos) {
	    case 2:
		/* Both arguments positive, base case */
		mp_or(First, Second, &bigResult);
		break;
	    case 1:
		/* First is positive; Second negative
		 * N|P = ~(~N&~P) = ~((-N-1)&~P) = -((-N-1)&((-N-1)^P))-1 */
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_xor(First, Second, &bigResult);
		mp_and(Second, &bigResult, &bigResult);
		mp_neg(&bigResult, &bigResult);
		mp_sub_d(&bigResult, 1, &bigResult);
		break;
	    case 0:
		/* Both arguments negative
		 * a | b = ~ (~a & ~b) = -(-a-1&-b-1)-1 */
		mp_neg(First, First);
		mp_sub_d(First, 1, First);
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_and(First, Second, &bigResult);
		mp_neg(&bigResult, &bigResult);
		mp_sub_d(&bigResult, 1, &bigResult);
		break;
	    }
	    break;

	case INST_BITXOR:
	    switch (numPos) {
	    case 2:
		/* Both arguments positive, base case */
		mp_xor(First, Second, &bigResult);
		break;
	    case 1:
		/* First is positive; Second negative
		 * P^N = ~(P^~N) = -(P^(-N-1))-1 */
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_xor(First, Second, &bigResult);
		mp_neg(&bigResult, &bigResult);
		mp_sub_d(&bigResult, 1, &bigResult);
		break;
	    case 0:
		/* Both arguments negative
		 * a ^ b = (~a ^ ~b) = (-a-1^-b-1) */
		mp_neg(First, First);
		mp_sub_d(First, 1, First);
		mp_neg(Second, Second);
		mp_sub_d(Second, 1, Second);
		mp_xor(First, Second, &bigResult);
		break;
	    }
	    break;
	}

	mp_clear(&big1);
	mp_clear(&big2);
	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewBignumObj(&bigResult);
	}
	Tcl_SetBignumObj(valuePtr, &bigResult);
	return valuePtr;
    }
#ifndef NO_WIDE_TYPE
    else if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE)) {
	Tcl_WideInt wResult, w1, w2;
	Tcl_GetWideIntFromObj(NULL, valuePtr, &w1);
	Tcl_GetWideIntFromObj(NULL, value2Ptr, &w2);

	switch (opcode) {
	case INST_BITAND:
	    wResult = w1 & w2;
	    break;
	case INST_BITOR:
	    wResult = w1 | w2;
	    break;
	case INST_BITXOR:
	    wResult = w1 ^ w2;
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    wResult = 0;
	}

	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewWideIntObj(wResult);
	}
	Tcl_SetWideIntObj(valuePtr, wResult);
	return valuePtr;
    }
#endif
    else {
	long lResult, l1 = *((const long *)ptr1);
	long l2 = *((const long *)ptr2);

	switch (opcode) {
	case INST_BITAND:
	    lResult = l1 & l2;
	    break;
	case INST_BITOR:
	    lResult = l1 | l2;
	    break;
	case INST_BITXOR:
	    lResult = l1 ^ l2;
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    lResult = 0;
	}

	if (Tcl_IsShared(valuePtr)) {
	    return Tcl_NewLongObj(lResult);
	}
	TclSetLongObj(valuePtr, lResult);
	return valuePtr;
    }

    {
	const char *description, *operator;

    illegalOperand:
	switch (opcode) {
	case INST_BITAND: operator = "&";  break;
	case INST_BITOR:  operator = "|";  break;
	case INST_BITXOR: operator = "^";  break;
	default:
	    operator = "???";
	}

	if (TclGetNumberFromObj(NULL, errPtr, &ptr1, &type1) != TCL_OK) {
	    int numBytes;
	    CONST char *bytes = Tcl_GetStringFromObj(errPtr, &numBytes);
	    if (numBytes == 0) {
		description = "empty string";
	    } else if (TclCheckBadOctal(NULL, bytes)) {
		description = "invalid octal number";
	    } else {
		description = "non-numeric string";
	    }
	} else if (type1 == TCL_NUMBER_NAN) {
	    description = "non-numeric floating-point value";
	} else if (type1 == TCL_NUMBER_DOUBLE) {
	    description = "floating-point value";
	} else {
	    /* TODO: No caller needs this.  Eliminate? */
	    description = "(big) integer";
	}

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't use %s as operand of \"%s\"", description, operator));
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CompareNumbers --
 *
 *	Parses and compares two numbers (may be either entier() or double()).
 *
 * Results:
 * 	TCL_OK if the numbers parse correctly, TCL_ERROR if one is not numeric
 * 	at all, and TCL_BREAK if one or the other is "NaN". The resultPtr
 * 	argument is used to update a variable with how the numbers relate to
 * 	each other in the TCL_OK case.
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	This code originally extracted from tclExecute.c.
 *
 *----------------------------------------------------------------------
 */

static int
CompareNumbers(
    Tcl_Interp *interp,		/* Where to write error messages if any. */
    Tcl_Obj *numObj1,		/* First number to compare. */
    Tcl_Obj *numObj2,		/* Second number to compare. */
    int *resultPtr)		/* Pointer to a variable to write the outcome
				 * of the comparison into. Must not be
				 * NULL. */
{
    ClientData ptr1, ptr2;
    int type1, type2;
    double d1, d2, tmp;
    long l1, l2;
    mp_int big1, big2;
#ifndef NO_WIDE_TYPE
    Tcl_WideInt w1, w2;
#endif

    if (TclGetNumberFromObj(interp, numObj1, &ptr1, &type1) != TCL_OK) {
	return TCL_ERROR;
    }
    if (TclGetNumberFromObj(interp, numObj2, &ptr2, &type2) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Selected special cases. NaNs are not equal to *everything*, otherwise
     * objects are equal to themselves.
     */

    if (type1 == TCL_NUMBER_NAN) {
	/* NaN first arg: NaN != to everything, other compares are false */
	return TCL_BREAK;
    }
    if (numObj1 == numObj2) {
	*resultPtr = MP_EQ;
	return TCL_OK;
    }
    if (type2 == TCL_NUMBER_NAN) {
	/* NaN 2nd arg: NaN != to everything, other compares are false */
	return TCL_BREAK;
    }

    /*
     * Big switch to pick apart the type rules and choose how to compare the
     * two numbers. Also handles a few special cases along the way.
     */

    switch (type1) {
    case TCL_NUMBER_LONG:
	l1 = *((CONST long *)ptr1);
	switch (type2) {
	case TCL_NUMBER_LONG:
	    l2 = *((CONST long *)ptr2);
	    goto longCompare;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w2 = *((CONST Tcl_WideInt *)ptr2);
	    w1 = (Tcl_WideInt)l1;
	    goto wideCompare;
#endif
	case TCL_NUMBER_DOUBLE:
	    d2 = *((CONST double *)ptr2);
	    d1 = (double) l1;

	    /*
	     * If the double has a fractional part, or if the long can be
	     * converted to double without loss of precision, then compare as
	     * doubles.
	     */

	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
		    || (l1 == (long) d1) || (modf(d2, &tmp) != 0.0)) {
		goto doubleCompare;
	    }

	    /*
	     * Otherwise, to make comparision based on full precision, need to
	     * convert the double to a suitably sized integer.
	     *
	     * Need this to get comparsions like
	     *	    expr 20000000000000003 < 20000000000000004.0
	     * right. Converting the first argument to double will yield two
	     * double values that are equivalent within double precision.
	     * Converting the double to an integer gets done exactly, then
	     * integer comparison can tell the difference.
	     */

	    if (d2 < (double)LONG_MIN) {
		*resultPtr = MP_GT;
		return TCL_OK;
	    }
	    if (d2 > (double)LONG_MAX) {
		*resultPtr = MP_LT;
		return TCL_OK;
	    }
	    l2 = (long) d2;
	    goto longCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, numObj2, &big2);
	    if (mp_cmp_d(&big2, 0) == MP_LT) {
		*resultPtr = MP_GT;
	    } else {
		*resultPtr = MP_LT;
	    }
	    mp_clear(&big2);
	}
	return TCL_OK;

#ifndef NO_WIDE_TYPE
    case TCL_NUMBER_WIDE:
	w1 = *((CONST Tcl_WideInt *)ptr1);
	switch (type2) {
	case TCL_NUMBER_WIDE:
	    w2 = *((CONST Tcl_WideInt *)ptr2);
	    goto wideCompare;
	case TCL_NUMBER_LONG:
	    l2 = *((CONST long *)ptr2);
	    w2 = (Tcl_WideInt)l2;
	    goto wideCompare;
	case TCL_NUMBER_DOUBLE:
	    d2 = *((CONST double *)ptr2);
	    d1 = (double) w1;
	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt))
		    || (w1 == (Tcl_WideInt) d1) || (modf(d2, &tmp) != 0.0)) {
		goto doubleCompare;
	    }
	    if (d2 < (double)LLONG_MIN) {
		*resultPtr = MP_GT;
		return TCL_OK;
	    }
	    if (d2 > (double)LLONG_MAX) {
		*resultPtr = MP_LT;
		return TCL_OK;
	    }
	    w2 = (Tcl_WideInt) d2;
	    goto wideCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, numObj2, &big2);
	    if (mp_cmp_d(&big2, 0) == MP_LT) {
		*resultPtr = MP_GT;
	    } else {
		*resultPtr = MP_LT;
	    }
	    mp_clear(&big2);
	}
	return TCL_OK;
#endif

    case TCL_NUMBER_DOUBLE:
	d1 = *((CONST double *)ptr1);
	switch (type2) {
	case TCL_NUMBER_DOUBLE:
	    d2 = *((CONST double *)ptr2);
	    goto doubleCompare;
	case TCL_NUMBER_LONG:
	    l2 = *((CONST long *)ptr2);
	    d2 = (double) l2;

	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
		    || (l2 == (long) d2) || (modf(d1, &tmp) != 0.0)) {
		goto doubleCompare;
	    }
	    if (d1 < (double)LONG_MIN) {
		*resultPtr = MP_LT;
		return TCL_OK;
	    }
	    if (d1 > (double)LONG_MAX) {
		*resultPtr = MP_GT;
		return TCL_OK;
	    }
	    l1 = (long) d1;
	    goto longCompare;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w2 = *((CONST Tcl_WideInt *)ptr2);
	    d2 = (double) w2;
	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt))
		    || (w2 == (Tcl_WideInt) d2) || (modf(d1, &tmp) != 0.0)) {
		goto doubleCompare;
	    }
	    if (d1 < (double)LLONG_MIN) {
		*resultPtr = MP_LT;
		return TCL_OK;
	    }
	    if (d1 > (double)LLONG_MAX) {
		*resultPtr = MP_GT;
		return TCL_OK;
	    }
	    w1 = (Tcl_WideInt) d1;
	    goto wideCompare;
#endif
	case TCL_NUMBER_BIG:
	    if (TclIsInfinite(d1)) {
		*resultPtr = (d1 > 0.0) ? MP_GT : MP_LT;
		return TCL_OK;
	    }
	    Tcl_TakeBignumFromObj(NULL, numObj2, &big2);
	    if ((d1 < (double)LONG_MAX) && (d1 > (double)LONG_MIN)) {
		if (mp_cmp_d(&big2, 0) == MP_LT) {
		    *resultPtr = MP_GT;
		} else {
		    *resultPtr = MP_LT;
		}
		mp_clear(&big2);
		return TCL_OK;
	    }
	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
		    && (modf(d1, &tmp) != 0.0)) {
		d2 = TclBignumToDouble(&big2);
		mp_clear(&big2);
		goto doubleCompare;
	    }
	    Tcl_InitBignumFromDouble(NULL, d1, &big1);
	    goto bigCompare;
	}
	return TCL_OK;

    case TCL_NUMBER_BIG:
	Tcl_TakeBignumFromObj(NULL, numObj1, &big1);
	switch (type2) {
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
#endif
	case TCL_NUMBER_LONG:
	    *resultPtr = mp_cmp_d(&big1, 0);
	    mp_clear(&big1);
	    return TCL_OK;
	case TCL_NUMBER_DOUBLE:
	    d2 = *((CONST double *)ptr2);
	    if (TclIsInfinite(d2)) {
		*resultPtr = (d2 > 0.0) ? MP_LT : MP_GT;
		mp_clear(&big1);
		return TCL_OK;
	    }
	    if ((d2 < (double)LONG_MAX) && (d2 > (double)LONG_MIN)) {
		*resultPtr = mp_cmp_d(&big1, 0);
		mp_clear(&big1);
		return TCL_OK;
	    }
	    if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
		    && (modf(d2, &tmp) != 0.0)) {
		d1 = TclBignumToDouble(&big1);
		mp_clear(&big1);
		goto doubleCompare;
	    }
	    Tcl_InitBignumFromDouble(NULL, d2, &big2);
	    goto bigCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, numObj2, &big2);
	    goto bigCompare;
	}
    }

    /*
     * Should really be impossible to get here
     */

    return TCL_OK;

    /*
     * The real core comparison rules.
     */

  longCompare:
    *resultPtr = (l1 < l2) ? MP_LT : ((l1 > l2) ? MP_GT : MP_EQ);
    return TCL_OK;
#ifndef NO_WIDE_TYPE
  wideCompare:
    *resultPtr = (w1 < w2) ? MP_LT : ((w1 > w2) ? MP_GT : MP_EQ);
    return TCL_OK;
#endif
  doubleCompare:
    *resultPtr = (d1 < d2) ? MP_LT : ((d1 > d2) ? MP_GT : MP_EQ);
    return TCL_OK;
  bigCompare:
    *resultPtr = mp_cmp(&big1, &big2);
    mp_clear(&big1);
    mp_clear(&big2);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInvertOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::~" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclInvertOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    ClientData val;
    int type;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "number");
	return TCL_ERROR;
    }
    if (TclGetNumberFromObj(NULL, objv[1], &val, &type) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"can't use non-numeric string as operand of \"~\"", -1));
	return TCL_ERROR;
    }
    switch (type) {
    case TCL_NUMBER_NAN:
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't use %s as operand of \"~\"",
		"non-numeric floating-point value"));
	return TCL_ERROR;
    case TCL_NUMBER_DOUBLE:
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't use %s as operand of \"~\"", "floating-point value"));
	return TCL_ERROR;
    case TCL_NUMBER_LONG: {
	long l = *((const long *) val);

	Tcl_SetLongObj(Tcl_GetObjResult(interp), ~l);
	return TCL_OK;
    }
#ifndef NO_WIDE_TYPE
    case TCL_NUMBER_WIDE: {
	Tcl_WideInt w = *((const Tcl_WideInt *) val);

	Tcl_SetWideIntObj(Tcl_GetObjResult(interp), ~w);
	return TCL_OK;
    }
#endif
    default: {
	mp_int big;

	Tcl_TakeBignumFromObj(NULL, objv[1], &big);
	/* ~a = - a - 1 */
	mp_neg(&big, &big);
	mp_sub_d(&big, 1, &big);
	if (Tcl_IsShared(objv[1])) {
	    Tcl_SetObjResult(interp, Tcl_NewBignumObj(&big));
	} else {
	    Tcl_SetBignumObj(objv[1], &big);
	    Tcl_SetObjResult(interp, objv[1]);
	}
	return TCL_OK;
    }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclNotOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::!" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclNotOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int b;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "boolean");
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(NULL, objv[1], &b) != TCL_OK) {
	int type;
	ClientData val;
	if (TclGetNumberFromObj(NULL, objv[1], &val, &type) == TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric floating-point value as operand of \"!\"", -1));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"!\"", -1));
	}
	return TCL_ERROR;
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), !b);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclAddOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::+" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclAddOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"+\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntFloat(interp, objv[1], INST_ADD, objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *sumPtr = objv[1];
	int i;

	Tcl_IncrRefCount(sumPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntFloat(interp, sumPtr, INST_ADD,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(sumPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(sumPtr);
	    sumPtr = resPtr;
	}
	Tcl_SetObjResult(interp, sumPtr);
	Tcl_DecrRefCount(sumPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclMulOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::*" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMulOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"*\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntFloat(interp, objv[1],INST_MULT,objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *prodPtr = objv[1];
	int i;

	Tcl_IncrRefCount(prodPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntFloat(interp, prodPtr, INST_MULT,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(prodPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(prodPtr);
	    prodPtr = resPtr;
	}
	Tcl_SetObjResult(interp, prodPtr);
	Tcl_DecrRefCount(prodPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclAndOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::&" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclAndOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), -1);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"&\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntOnly(interp, objv[1],INST_BITAND,objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *accumPtr = objv[1];
	int i;

	Tcl_IncrRefCount(accumPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntOnly(interp, accumPtr, INST_BITAND,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(accumPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(accumPtr);
	    accumPtr = resPtr;
	}
	Tcl_SetObjResult(interp, accumPtr);
	Tcl_DecrRefCount(accumPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclOrOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::|" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclOrOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"|\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntOnly(interp, objv[1],INST_BITOR,objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *accumPtr = objv[1];
	int i;

	Tcl_IncrRefCount(accumPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntOnly(interp, accumPtr, INST_BITOR,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(accumPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(accumPtr);
	    accumPtr = resPtr;
	}
	Tcl_SetObjResult(interp, accumPtr);
	Tcl_DecrRefCount(accumPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclXorOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::^" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclXorOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"^\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntOnly(interp, objv[1],INST_BITXOR,objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *accumPtr = objv[1];
	int i;

	Tcl_IncrRefCount(accumPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntOnly(interp, accumPtr, INST_BITXOR,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(accumPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(accumPtr);
	    accumPtr = resPtr;
	}
	Tcl_SetObjResult(interp, accumPtr);
	Tcl_DecrRefCount(accumPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclPowOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::**" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclPowOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
	return TCL_OK;
    } else if (objc == 2) {
	ClientData ptr1;
	int type1;
	if (TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "can't use non-numeric string as operand of \"**\"",-1));
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntFloat(interp, objv[1],INST_EXPON,objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *powPtr = objv[objc-1];
	int i;

	Tcl_IncrRefCount(powPtr);
	for (i=objc-2 ; i>=1 ; i--) {
	    Tcl_Obj *resPtr = CombineIntFloat(interp, objv[i], INST_EXPON,
		    powPtr);

	    if (resPtr == NULL) {
		TclDecrRefCount(powPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(powPtr);
	    powPtr = resPtr;
	}
	Tcl_SetObjResult(interp, powPtr);
	Tcl_DecrRefCount(powPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclMinusOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::-" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMinusOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value ?value ...?");
	return TCL_ERROR;
    } else if (objc == 2) {
	/*
	 * Only a single argument, so we compute the negation.
	 */

	Tcl_Obj *zeroPtr = Tcl_NewIntObj(0);
	Tcl_Obj *resPtr;

	Tcl_IncrRefCount(zeroPtr);
	resPtr = CombineIntFloat(interp, zeroPtr, INST_SUB, objv[1]);
	if (resPtr == NULL) {
	    TclDecrRefCount(zeroPtr);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	TclDecrRefCount(zeroPtr);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntFloat(interp, objv[1], INST_SUB, objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *diffPtr = objv[1];
	int i;

	Tcl_IncrRefCount(diffPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntFloat(interp, diffPtr, INST_SUB,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(diffPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(diffPtr);
	    diffPtr = resPtr;
	}
	Tcl_SetObjResult(interp, diffPtr);
	Tcl_DecrRefCount(diffPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclDivOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::/" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclDivOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value ?value ...?");
	return TCL_ERROR;
    } else if (objc == 2) {
	/*
	 * Only a single argument, so we compute the reciprocal.
	 */

	Tcl_Obj *onePtr = Tcl_NewDoubleObj(1.0);
	Tcl_Obj *resPtr;

	Tcl_IncrRefCount(onePtr);
	resPtr = CombineIntFloat(interp, onePtr, INST_DIV, objv[1]);
	if (resPtr == NULL) {
	    TclDecrRefCount(onePtr);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	TclDecrRefCount(onePtr);
	return TCL_OK;
    } else if (objc == 3) {
	/*
	 * This is a special case of the version with the loop that allows for
	 * better memory management of objects in some cases.
	 */

	Tcl_Obj *resPtr = CombineIntFloat(interp, objv[1], INST_DIV, objv[2]);
	if (resPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resPtr);
	return TCL_OK;
    } else {
	Tcl_Obj *numeratorPtr = objv[1];
	int i;

	Tcl_IncrRefCount(numeratorPtr);
	for (i=2 ; i<objc ; i++) {
	    Tcl_Obj *resPtr = CombineIntFloat(interp, numeratorPtr, INST_DIV,
		    objv[i]);

	    if (resPtr == NULL) {
		TclDecrRefCount(numeratorPtr);
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(resPtr);
	    TclDecrRefCount(numeratorPtr);
	    numeratorPtr = resPtr;
	}
	Tcl_SetObjResult(interp, numeratorPtr);
	Tcl_DecrRefCount(numeratorPtr);	/* Public form since we know we won't
					 * be freeing this object now. */
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclLshiftOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::<<" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclLshiftOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    ClientData ptr1, ptr2;
    int invalid, shift, type1, type2, idx;
    const char *description;
    long l1;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value value");
	return TCL_ERROR;
    }

    if ((TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK)
	    || (type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	idx = 1;
	goto illegalOperand;
    }
    if ((TclGetNumberFromObj(NULL, objv[2], &ptr2, &type2) != TCL_OK)
	    || (type2 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_NAN)) {
	idx = 2;
	goto illegalOperand;
    }

    /* reject negative shift argument */
    switch (type2) {
    case TCL_NUMBER_LONG:
	invalid = (*((const long *)ptr2) < (long)0);
	break;
#ifndef NO_WIDE_TYPE
    case TCL_NUMBER_WIDE:
	invalid = (*((const Tcl_WideInt *)ptr2) < (Tcl_WideInt)0);
	break;
#endif
    case TCL_NUMBER_BIG:
	/* TODO: const correctness ? */
	invalid = (mp_cmp_d((mp_int *)ptr2, 0) == MP_LT);
	break;
    default:
	/* Unused, here to silence compiler warning */
	invalid = 0;
    }
    if (invalid) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("negative shift argument", -1));
	return TCL_ERROR;
    }

    /* Zero shifted any number of bits is still zero */
    if ((type1 == TCL_NUMBER_LONG) && (*((const long *)ptr1) == (long)0)) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	return TCL_OK;
    }

    /* Large left shifts create integer overflow */
    if ((type2 != TCL_NUMBER_LONG)
	    || (*((const long *)ptr2) > (long) INT_MAX)) {
	/*
	 * Technically, we could hold the value (1 << (INT_MAX+1)) in an
	 * mp_int, but since we're using mp_mul_2d() to do the work, and it
	 * takes only an int argument, that's a good place to draw the line.
	 */

	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"integer value too large to represent", -1));
	return TCL_ERROR;
    }
    shift = (int)(*((const long *)ptr2));

    /* Handle shifts within the native long range */
    if ((type1 == TCL_NUMBER_LONG) && ((size_t)shift < CHAR_BIT*sizeof(long))
	    && (l1 = *((CONST long *)ptr1)) &&
	    !(((l1>0) ? l1 : ~l1) & -(1L<<(CHAR_BIT*sizeof(long)-1-shift)))) {
	Tcl_SetObjResult(interp, Tcl_NewLongObj(l1<<shift));
	return TCL_OK;
    }

    /* Handle shifts within the native wide range */
    if ((type1 != TCL_NUMBER_BIG)
	    && ((size_t)shift < CHAR_BIT*sizeof(Tcl_WideInt))) {
	Tcl_WideInt w;

	Tcl_GetWideIntFromObj(NULL, objv[1], &w);
	if (!(((w>0) ? w : ~w) & -(((Tcl_WideInt)1)
		<< (CHAR_BIT*sizeof(Tcl_WideInt)-1-shift)))) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(w<<shift));
	    return TCL_OK;
	}
    }

    {
	mp_int big, bigResult;

	Tcl_TakeBignumFromObj(NULL, objv[1], &big);

	mp_init(&bigResult);
	mp_mul_2d(&big, shift, &bigResult);
	mp_clear(&big);

	if (!Tcl_IsShared(objv[1])) {
	    Tcl_SetBignumObj(objv[1], &bigResult);
	    Tcl_SetObjResult(interp, objv[1]);
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewBignumObj(&bigResult));
	}
    }
    return TCL_OK;

  illegalOperand:
    if (TclGetNumberFromObj(NULL, objv[idx], &ptr1, &type1) != TCL_OK) {
	int numBytes;
	const char *bytes = Tcl_GetStringFromObj(objv[idx], &numBytes);
	if (numBytes == 0) {
	    description = "empty string";
	} else if (TclCheckBadOctal(NULL, bytes)) {
	    description = "invalid octal number";
	} else {
	    description = "non-numeric string";
	}
    } else if (type1 == TCL_NUMBER_NAN) {
	description = "non-numeric floating-point value";
    } else {
	description = "floating-point value";
    }

    Tcl_SetObjResult(interp,
	    Tcl_ObjPrintf("can't use %s as operand of \"<<\"", description));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRshiftOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::>>" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclRshiftOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    ClientData ptr1, ptr2;
    int invalid, shift, type1, type2, idx;
    const char *description;
    long l1;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value value");
	return TCL_ERROR;
    }

    if ((TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK)
	    || (type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	idx = 1;
	goto illegalOperand;
    }
    if ((TclGetNumberFromObj(NULL, objv[2], &ptr2, &type2) != TCL_OK)
	    || (type2 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_NAN)) {
	idx = 2;
	goto illegalOperand;
    }

    /* reject negative shift argument */
    switch (type2) {
    case TCL_NUMBER_LONG:
	invalid = (*((const long *)ptr2) < (long)0);
	break;
#ifndef NO_WIDE_TYPE
    case TCL_NUMBER_WIDE:
	invalid = (*((const Tcl_WideInt *)ptr2) < (Tcl_WideInt)0);
	break;
#endif
    case TCL_NUMBER_BIG:
	/* TODO: const correctness ? */
	invalid = (mp_cmp_d((mp_int *)ptr2, 0) == MP_LT);
	break;
    default:
	/* Unused, here to silence compiler warning */
	invalid = 0;
    }
    if (invalid) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("negative shift argument", -1));
	return TCL_ERROR;
    }

    /* Zero shifted any number of bits is still zero */
    if ((type1 == TCL_NUMBER_LONG) && (*((const long *)ptr1) == (long)0)) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	return TCL_OK;
    }

    /* Quickly force large right shifts to 0 or -1 */
    if ((type2 != TCL_NUMBER_LONG)
	    || (*((const long *)ptr2) > INT_MAX)) {
	/*
	 * Again, technically, the value to be shifted could be an mp_int so
	 * huge that a right shift by (INT_MAX+1) bits could not take us to
	 * the result of 0 or -1, but since we're using mp_div_2d to do the
	 * work, and it takes only an int argument, we draw the line there.
	 */

	int zero;

	switch (type1) {
	case TCL_NUMBER_LONG:
	    zero = (*((const long *)ptr1) > (long)0);
	    break;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    zero = (*((const Tcl_WideInt *)ptr1) > (Tcl_WideInt)0);
	    break;
#endif
	case TCL_NUMBER_BIG:
	    /* TODO: const correctness ? */
	    zero = (mp_cmp_d((mp_int *)ptr1, 0) == MP_GT);
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    zero = 0;
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(zero ? 0 : -1));
	return TCL_OK;
    }

    shift = (int)(*((const long *)ptr2));
    /* Handle shifts within the native long range */
    if (type1 == TCL_NUMBER_LONG) {
	l1 = *((const long *)ptr1);
	if ((size_t)shift >= CHAR_BIT*sizeof(long)) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(l1 >= (long)0 ? 0 : -1));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(l1 >> shift));
	}
	return TCL_OK;
    }

#ifndef NO_WIDE_TYPE
    /* Handle shifts within the native wide range */
    if (type1 == TCL_NUMBER_WIDE) {
	Tcl_WideInt w = *((const Tcl_WideInt *)ptr1);
	if ((size_t)shift >= CHAR_BIT*sizeof(Tcl_WideInt)) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewIntObj(w >= (Tcl_WideInt)0 ? 0 : -1));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(w >> shift));
	}
	return TCL_OK;
    }
#endif

    {
	mp_int big, bigResult, bigRemainder;

	Tcl_TakeBignumFromObj(NULL, objv[1], &big);

	mp_init(&bigResult);
	mp_init(&bigRemainder);
	mp_div_2d(&big, shift, &bigResult, &bigRemainder);
	if (mp_cmp_d(&bigRemainder, 0) == MP_LT) {
	    /* Convert to Tcl's integer division rules */
	    mp_sub_d(&bigResult, 1, &bigResult);
	}
	mp_clear(&bigRemainder);
	mp_clear(&big);

	if (!Tcl_IsShared(objv[1])) {
	    Tcl_SetBignumObj(objv[1], &bigResult);
	    Tcl_SetObjResult(interp, objv[1]);
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewBignumObj(&bigResult));
	}
    }
    return TCL_OK;

  illegalOperand:
    if (TclGetNumberFromObj(NULL, objv[idx], &ptr1, &type1) != TCL_OK) {
	int numBytes;
	const char *bytes = Tcl_GetStringFromObj(objv[idx], &numBytes);
	if (numBytes == 0) {
	    description = "empty string";
	} else if (TclCheckBadOctal(NULL, bytes)) {
	    description = "invalid octal number";
	} else {
	    description = "non-numeric string";
	}
    } else if (type1 == TCL_NUMBER_NAN) {
	description = "non-numeric floating-point value";
    } else {
	description = "floating-point value";
    }

    Tcl_SetObjResult(interp,
	    Tcl_ObjPrintf("can't use %s as operand of \">>\"", description));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclModOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::%" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclModOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *argObj;
    ClientData ptr1, ptr2;
    int type1, type2;
    long l1, l2 = 0;
    const char *description;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value value");
	return TCL_ERROR;
    }

    if ((TclGetNumberFromObj(NULL, objv[1], &ptr1, &type1) != TCL_OK)
	    || (type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	argObj = objv[1];
	goto badArg;
    }
    if ((TclGetNumberFromObj(NULL, objv[2], &ptr2, &type2) != TCL_OK)
	    || (type2 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_NAN)) {
	argObj = objv[2];
	goto badArg;
    }

    if (type2 == TCL_NUMBER_LONG) {
	l2 = *((CONST long *)ptr2);
	if (l2 == 0) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("divide by zero", -1));
	    Tcl_SetErrorCode(interp, "ARITH", "DIVZERO", "divide by zero",
		    NULL);
	    return TCL_ERROR;
	}
	if ((l2 == 1) || (l2 == -1)) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	    return TCL_OK;
	}
    }
    if (type1 == TCL_NUMBER_LONG) {
	l1 = *((CONST long *)ptr1);
	if (l1 == 0) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	    return TCL_OK;
	}
	if (type2 == TCL_NUMBER_LONG) {
	    /* Both operands are long; do native calculation */
	    long lRemainder, lQuotient = l1 / l2;

	    /* Force Tcl's integer division rules */
	    /* TODO: examine for logic simplification */
	    if (((lQuotient < 0) || ((lQuotient == 0) &&
		    ((l1 < 0 && l2 > 0) || (l1 > 0 && l2 < 0)))) &&
		    ((lQuotient * l2) != l1)) {
		lQuotient -= 1;
	    }
	    lRemainder = l1 - l2*lQuotient;
	    Tcl_SetLongObj(Tcl_GetObjResult(interp), lRemainder);
	    return TCL_OK;
	}
	/*
	 * First operand fits in long; second does not, so the second has
	 * greater magnitude than first. No need to divide to determine the
	 * remainder.
	 */
#ifndef NO_WIDE_TYPE
	if (type2 == TCL_NUMBER_WIDE) {
	    Tcl_WideInt w2 = *((CONST Tcl_WideInt *)ptr2);

	    if ((l1 > 0) ^ (w2 > (Tcl_WideInt)0)) {
		/* Arguments are opposite sign; remainder is sum */
		Tcl_SetObjResult(interp,
			Tcl_NewWideIntObj(w2+(Tcl_WideInt)l1));
		return TCL_OK;
	    }
	    /* Arguments are same sign; remainder is first operand */
	    Tcl_SetObjResult(interp, objv[1]);
	    return TCL_OK;
	}
#endif
	{
	    mp_int big2;
	    Tcl_TakeBignumFromObj(NULL, objv[2], &big2);

	    /* TODO: internals intrusion */
	    if ((l1 > 0) ^ (big2.sign == MP_ZPOS)) {
		/* Arguments are opposite sign; remainder is sum */
		mp_int big1;
		TclBNInitBignumFromLong(&big1, l1);
		mp_add(&big2, &big1, &big2);
		Tcl_SetObjResult(interp, Tcl_NewBignumObj(&big2));
	    } else {
		/* Arguments are same sign; remainder is first operand */
		Tcl_SetObjResult(interp, objv[1]);
		/* TODO: free big2? */
	    }
	}
	return TCL_OK;
    }
#ifndef NO_WIDE_TYPE
    if (type1 == TCL_NUMBER_WIDE) {
	Tcl_WideInt w1 = *((CONST Tcl_WideInt *)ptr1);
	if (type2 != TCL_NUMBER_BIG) {
	    Tcl_WideInt w2, wQuotient, wRemainder;

	    Tcl_GetWideIntFromObj(NULL, objv[2], &w2);
	    wQuotient = w1 / w2;

	    /* Force Tcl's integer division rules */
	    /* TODO: examine for logic simplification */
	    if (((wQuotient < ((Tcl_WideInt) 0))
		    || ((wQuotient == ((Tcl_WideInt) 0)) && (
			(w1 < ((Tcl_WideInt) 0) && w2 > ((Tcl_WideInt) 0))
		    || (w1 > ((Tcl_WideInt) 0) && w2 < ((Tcl_WideInt) 0)))
		    )) && ((wQuotient * w2) != w1)) {
		wQuotient -= (Tcl_WideInt) 1;
	    }
	    wRemainder = w1 - w2*wQuotient;
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(wRemainder));
	} else {
	    mp_int big2;
	    Tcl_TakeBignumFromObj(NULL, objv[2], &big2);

	    /* TODO: internals intrusion */
	    if ((w1 > ((Tcl_WideInt) 0)) ^ (big2.sign == MP_ZPOS)) {
		/* Arguments are opposite sign; remainder is sum */
		mp_int big1;
		TclBNInitBignumFromWideInt(&big1, w1);
		mp_add(&big2, &big1, &big2);
		Tcl_SetObjResult(interp, Tcl_NewBignumObj(&big2));
	    } else {
		/* Arguments are same sign; remainder is first operand */
		Tcl_SetObjResult(interp, objv[1]);
	    }
	}
	return TCL_OK;
    }
#endif
    {
	mp_int big1, big2, bigResult, bigRemainder;

	Tcl_GetBignumFromObj(NULL, objv[1], &big1);
	Tcl_GetBignumFromObj(NULL, objv[2], &big2);
	mp_init(&bigResult);
	mp_init(&bigRemainder);
	mp_div(&big1, &big2, &bigResult, &bigRemainder);
	if (!mp_iszero(&bigRemainder) && (bigRemainder.sign != big2.sign)) {
	    /* Convert to Tcl's integer division rules */
	    mp_sub_d(&bigResult, 1, &bigResult);
	    mp_add(&bigRemainder, &big2, &bigRemainder);
	}
	mp_copy(&bigRemainder, &bigResult);
	mp_clear(&bigRemainder);
	mp_clear(&big1);
	mp_clear(&big2);
	if (Tcl_IsShared(objv[1])) {
	    Tcl_SetObjResult(interp, Tcl_NewBignumObj(&bigResult));
	} else {
	    Tcl_SetBignumObj(objv[1], &bigResult);
	    Tcl_SetObjResult(interp, objv[1]);
	}
	return TCL_OK;
    }

  badArg:
    if (TclGetNumberFromObj(NULL, argObj, &ptr1, &type1) != TCL_OK) {
	int numBytes;
	CONST char *bytes = Tcl_GetStringFromObj(argObj, &numBytes);
	if (numBytes == 0) {
	    description = "empty string";
	} else if (TclCheckBadOctal(NULL, bytes)) {
	    description = "invalid octal number";
	} else {
	    description = "non-numeric string";
	}
    } else if (type1 == TCL_NUMBER_NAN) {
	description = "non-numeric floating-point value";
    } else {
	description = "floating-point value";
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "can't use %s as operand of \"%%\"", description));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNeqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::!=" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclNeqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1, cmp, len1, len2;
    const char *str1, *str2;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value value");
	return TCL_ERROR;
    }

    switch (CompareNumbers(NULL, objv[1], objv[2], &cmp)) {
    case TCL_ERROR:
	/*
	 * Got a string
	 */
	str1 = Tcl_GetStringFromObj(objv[1], &len1);
	str2 = Tcl_GetStringFromObj(objv[2], &len2);
	if (len1 == len2 && !strcmp(str1, str2)) {
	    result = 0;
	}
    case TCL_BREAK:			/* Deliberate fallthrough */
	break;
    case TCL_OK:
	/*
	 * Got proper numbers
	 */
	if (cmp == MP_EQ) {
	    result = 0;
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclStrneqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::ne" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclStrneqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    const char *s1, *s2;
    int s1len, s2len;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value value");
	return TCL_ERROR;
    }

    s1 = Tcl_GetStringFromObj(objv[1], &s1len);
    s2 = Tcl_GetStringFromObj(objv[2], &s2len);
    if (s1len == s2len && !strcmp(s1, s2)) {
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 0);
    } else {
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::in" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclInOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    const char *s1, *s2;
    int s1len, s2len, i, len;
    Tcl_Obj **listObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value list");
	return TCL_ERROR;
    }

    if (Tcl_ListObjGetElements(interp, objv[2], &len, &listObj) != TCL_OK) {
	return TCL_ERROR;
    }
    s1 = Tcl_GetStringFromObj(objv[1], &s1len);
    for (i=0 ; i<len ; i++) {
	s2 = Tcl_GetStringFromObj(listObj[i], &s2len);
	if (s1len == s2len && !strcmp(s1, s2)) {
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 1);
	    return TCL_OK;
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 0);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNiOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::ni" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclNiOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    const char *s1, *s2;
    int s1len, s2len, i, len;
    Tcl_Obj **listObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value list");
	return TCL_ERROR;
    }

    if (Tcl_ListObjGetElements(interp, objv[2], &len, &listObj) != TCL_OK) {
	return TCL_ERROR;
    }
    s1 = Tcl_GetStringFromObj(objv[1], &s1len);
    for (i=0 ; i<len ; i++) {
	s2 = Tcl_GetStringFromObj(listObj[i], &s2len);
	if (s1len == s2len && !strcmp(s1, s2)) {
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 0);
	    return TCL_OK;
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLessOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::<" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclLessOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, cmp, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    switch (CompareNumbers(NULL, objv[i], objv[i+1], &cmp)) {
	    case TCL_ERROR:
		/*
		 * Got a string
		 */
		str1 = Tcl_GetStringFromObj(objv[i], &len1);
		str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
		if (TclpUtfNcmp2(str1, str2,
			(size_t) ((len1 < len2) ? len1 : len2)) >= 0) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_OK:
		/*
		 * Got proper numbers
		 */
		if (cmp != MP_LT) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_BREAK:
		/*
		 * Got a NaN (which is different from everything, including
		 * itself)
		 */
		result = 0;
		i = objc;
		continue;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLeqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::<=" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclLeqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, cmp, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    switch (CompareNumbers(NULL, objv[i], objv[i+1], &cmp)) {
	    case TCL_ERROR:
		/*
		 * Got a string
		 */
		str1 = Tcl_GetStringFromObj(objv[i], &len1);
		str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
		if (TclpUtfNcmp2(str1, str2,
			(size_t) ((len1 < len2) ? len1 : len2)) > 0) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_OK:
		/*
		 * Got proper numbers
		 */
		if (cmp == MP_GT) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_BREAK:
		/*
		 * Got a NaN (which is different from everything, including
		 * itself)
		 */
		result = 0;
		i = objc;
		continue;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGreaterOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::>" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclGreaterOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, cmp, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    switch (CompareNumbers(NULL, objv[i], objv[i+1], &cmp)) {
	    case TCL_ERROR:
		/*
		 * Got a string
		 */
		str1 = Tcl_GetStringFromObj(objv[i], &len1);
		str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
		if (TclpUtfNcmp2(str1, str2,
			(size_t) ((len1 < len2) ? len1 : len2)) <= 0) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_OK:
		/*
		 * Got proper numbers
		 */
		if (cmp != MP_GT) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_BREAK:
		/*
		 * Got a NaN (which is different from everything, including
		 * itself)
		 */
		result = 0;
		i = objc;
		continue;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGeqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::>=" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclGeqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, cmp, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    switch (CompareNumbers(NULL, objv[i], objv[i+1], &cmp)) {
	    case TCL_ERROR:
		/*
		 * Got a string
		 */
		str1 = Tcl_GetStringFromObj(objv[i], &len1);
		str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
		if (TclpUtfNcmp2(str1, str2,
			(size_t) ((len1 < len2) ? len1 : len2)) < 0) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_OK:
		/*
		 * Got proper numbers
		 */
		if (cmp == MP_LT) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_BREAK:
		/*
		 * Got a NaN (which is different from everything, including
		 * itself)
		 */
		result = 0;
		i = objc;
		continue;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclEqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::==" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclEqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, cmp, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    switch (CompareNumbers(NULL, objv[i], objv[i+1], &cmp)) {
	    case TCL_ERROR:
		/*
		 * Got a string
		 */
		str1 = Tcl_GetStringFromObj(objv[i], &len1);
		str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
		if (len1 != len2 || strcmp(str1, str2)) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_OK:
		/*
		 * Got proper numbers
		 */
		if (cmp != MP_EQ) {
		    result = 0;
		    i = objc;
		}
		continue;
	    case TCL_BREAK:
		/*
		 * Got a NaN (which is different from everything, including
		 * itself)
		 */
		result = 0;
		i = objc;
		continue;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclStreqOpCmd --
 *
 *	This procedure is invoked to process the "::tcl::mathop::eq" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclStreqOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result = 1;

    if (objc > 2) {
	int i, len1, len2;
	const char *str1, *str2;

	for (i=1 ; i<objc-1 ; i++) {
	    str1 = Tcl_GetStringFromObj(objv[i], &len1);
	    str2 = Tcl_GetStringFromObj(objv[i+1], &len2);
	    if (len1 != len2 || strcmp(str1, str2)) {
		result = 0;
		break;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
