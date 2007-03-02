/*
 * tclCompExpr.c --
 *
 *	This file contains the code to compile Tcl expressions.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Boolean variable that controls whether expression compilation tracing is
 * enabled.
 */

#ifdef TCL_COMPILE_DEBUG
static int traceExprComp = 0;
#endif /* TCL_COMPILE_DEBUG */

/*
 * Definitions of numeric codes representing each expression operator.  The
 * order of these must match the entries in the operatorTable below.  Also the
 * codes for the relational operators (OP_LESS, OP_GREATER, OP_LE, OP_GE,
 * OP_EQ, and OP_NE) must be consecutive and in that order.  Note that OP_PLUS
 * and OP_MINUS represent both unary and binary operators.
 */

#define OP_MULT		0
#define OP_DIVIDE	1
#define OP_MOD		2
#define OP_PLUS		3
#define OP_MINUS	4
#define OP_LSHIFT	5
#define OP_RSHIFT	6
#define OP_LESS		7
#define OP_GREATER	8
#define OP_LE		9
#define OP_GE		10
#define OP_EQ		11
#define OP_NEQ		12
#define OP_BITAND	13
#define OP_BITXOR	14
#define OP_BITOR	15
#define OP_LAND		16
#define OP_LOR		17
#define OP_QUESTY	18
#define OP_LNOT		19
#define OP_BITNOT	20
#define OP_STREQ	21
#define OP_STRNEQ	22
#define OP_EXPON	23
#define OP_IN_LIST	24
#define OP_NOT_IN_LIST	25

/*
 * Table describing the expression operators. Entries in this table must
 * correspond to the definitions of numeric codes for operators just above.
 */

static int opTableInitialized = 0; /* 0 means not yet initialized. */

TCL_DECLARE_MUTEX(opMutex)

typedef struct OperatorDesc {
    char *name;			/* Name of the operator. */
    int numOperands;		/* Number of operands. 0 if the operator
				 * requires special handling. */
    int instruction;		/* Instruction opcode for the operator.
				 * Ignored if numOperands is 0. */
} OperatorDesc;

static OperatorDesc operatorTable[] = {
    {"*",   2,  INST_MULT},
    {"/",   2,  INST_DIV},
    {"%",   2,  INST_MOD},
    {"+",   0},
    {"-",   0},
    {"<<",  2,  INST_LSHIFT},
    {">>",  2,  INST_RSHIFT},
    {"<",   2,  INST_LT},
    {">",   2,  INST_GT},
    {"<=",  2,  INST_LE},
    {">=",  2,  INST_GE},
    {"==",  2,  INST_EQ},
    {"!=",  2,  INST_NEQ},
    {"&",   2,  INST_BITAND},
    {"^",   2,  INST_BITXOR},
    {"|",   2,  INST_BITOR},
    {"&&",  0},
    {"||",  0},
    {"?",   0},
    {"!",   1,  INST_LNOT},
    {"~",   1,  INST_BITNOT},
    {"eq",  2,  INST_STR_EQ},
    {"ne",  2,  INST_STR_NEQ},
    {"**",  2,	INST_EXPON},
    {"in",  2,	INST_LIST_IN},
    {"ni",  2,	INST_LIST_NOT_IN},
    {NULL}
};

/*
 * Hashtable used to map the names of expression operators to the index of
 * their OperatorDesc description.
 */

static Tcl_HashTable opHashTable;

/*
 * Declarations for local procedures to this file:
 */

static void		CompileCondExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int *convertPtr,
			    CompileEnv *envPtr);
static void		CompileLandOrLorExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int opIndex,
			    CompileEnv *envPtr);
static void		CompileMathFuncCall(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, CONST char *funcName,
			    CompileEnv *envPtr);
static void		CompileSubExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int *convertPtr,
			    CompileEnv *envPtr);

/*
 * Macro used to debug the execution of the expression compiler.
 */

#ifdef TCL_COMPILE_DEBUG
#define TRACE(exprBytes, exprLength, tokenBytes, tokenLength) \
    if (traceExprComp) { \
	fprintf(stderr, "CompileSubExpr: \"%.*s\", token \"%.*s\"\n", \
		(exprLength), (exprBytes), (tokenLength), (tokenBytes)); \
    }
#else
#define TRACE(exprBytes, exprLength, tokenBytes, tokenLength)
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExpr --
 *
 *	This procedure compiles a string containing a Tcl expression into Tcl
 *	bytecodes. This procedure is the top-level interface to the the
 *	expression compilation module, and is used by such public procedures
 *	as Tcl_ExprString, Tcl_ExprStringObj, Tcl_ExprLong, Tcl_ExprDouble,
 *	Tcl_ExprBoolean, and Tcl_ExprBooleanObj.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    CONST char *script,		/* The source script to compile. */
    int numBytes,		/* Number of bytes in script. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Parse parse;
    int needsNumConversion = 1;

    /*
     * If this is the first time we've been called, initialize the table of
     * expression operators.
     */

    if (numBytes < 0) {
	numBytes = (script? strlen(script) : 0);
    }
    if (!opTableInitialized) {
	Tcl_MutexLock(&opMutex);
	if (!opTableInitialized) {
	    int i;
	    Tcl_InitHashTable(&opHashTable, TCL_STRING_KEYS);
	    for (i = 0;  operatorTable[i].name != NULL;  i++) {
		int new;
		Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&opHashTable,
			operatorTable[i].name, &new);
		if (new) {
		    Tcl_SetHashValue(hPtr, (ClientData) i);
		}
	    }
	    opTableInitialized = 1;
	}
	Tcl_MutexUnlock(&opMutex);
    }

    /*
     * Parse the expression then compile it.
     */

    if (TCL_OK != Tcl_ParseExpr(interp, script, numBytes, &parse)) {
	return TCL_ERROR;
    }
    CompileSubExpr(interp, parse.tokenPtr, &needsNumConversion, envPtr);

    if (needsNumConversion) {
	/*
	 * Attempt to convert the primary's object to an int or double.  This
	 * is done in order to support Tcl's policy of interpreting operands
	 * if at all possible as first integers, else floating-point numbers.
	 */

	TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
    }
    Tcl_FreeParse(&parse);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeCompilation --
 *
 *	Clean up the compilation environment so it can later be properly
 *	reinitialized. This procedure is called by Tcl_Finalize().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up the compilation environment. At the moment, just the table
 *	of expression operators is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeCompilation(void)
{
    Tcl_MutexLock(&opMutex);
    if (opTableInitialized) {
	Tcl_DeleteHashTable(&opHashTable);
	opTableInitialized = 0;
    }
    Tcl_MutexUnlock(&opMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * CompileSubExpr --
 *
 *	Given a pointer to a TCL_TOKEN_SUB_EXPR token describing a
 *	subexpression, this procedure emits instructions to evaluate the
 *	subexpression at runtime.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the subexpression.
 *
 *----------------------------------------------------------------------
 */

static void
CompileSubExpr(
    Tcl_Interp *interp,		/* Interp in which to compile expression */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token to
				 * compile. */
    int *convertPtr,		/* Writes 0 here if it is determined the
				 * final INST_TRY_CVT_TO_NUMERIC is
				 * not needed */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /* Switch on the type of the first token after the subexpression token. */
    Tcl_Token *tokenPtr = exprTokenPtr+1;
    TRACE(exprTokenPtr->start, exprTokenPtr->size,
	    tokenPtr->start, tokenPtr->size);
    switch (tokenPtr->type) {
    case TCL_TOKEN_WORD:
	TclCompileTokens(interp, tokenPtr+1, tokenPtr->numComponents, envPtr);
	break;

    case TCL_TOKEN_TEXT:
	TclEmitPush(TclRegisterNewLiteral(envPtr,
		tokenPtr->start, tokenPtr->size), envPtr);
	break;

    case TCL_TOKEN_BS: {
	char buffer[TCL_UTF_MAX];
	int length = Tcl_UtfBackslash(tokenPtr->start, NULL, buffer);
	TclEmitPush(TclRegisterNewLiteral(envPtr, buffer, length), envPtr);
	break;
    }

    case TCL_TOKEN_COMMAND:
	TclCompileScript(interp, tokenPtr->start+1, tokenPtr->size-2, envPtr);
	break;

    case TCL_TOKEN_VARIABLE:
	TclCompileTokens(interp, tokenPtr, 1, envPtr);
	break;

    case TCL_TOKEN_SUB_EXPR:
	CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	break;

    case TCL_TOKEN_OPERATOR: {
	/*
	 * Look up the operator.  If the operator isn't found, treat it as a
	 * math function.
	 */
	OperatorDesc *opDescPtr;
	Tcl_HashEntry *hPtr;
	CONST char *operator;
	Tcl_DString opBuf;
	int opIndex;

	Tcl_DStringInit(&opBuf);
	operator = Tcl_DStringAppend(&opBuf, tokenPtr->start, tokenPtr->size);
	hPtr = Tcl_FindHashEntry(&opHashTable, operator);
	if (hPtr == NULL) {
	    CompileMathFuncCall(interp, exprTokenPtr, operator, envPtr);
	    Tcl_DStringFree(&opBuf);
	    break;
	}
	Tcl_DStringFree(&opBuf);
	opIndex = (int) Tcl_GetHashValue(hPtr);
	opDescPtr = &(operatorTable[opIndex]);

	/*
	 * If the operator is "normal", compile it using information from the
	 * operator table.
	 */

	if (opDescPtr->numOperands > 0) {
	    tokenPtr++;
	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    tokenPtr += (tokenPtr->numComponents + 1);

	    if (opDescPtr->numOperands == 2) {
		CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    }
	    TclEmitOpcode(opDescPtr->instruction, envPtr);
	    *convertPtr = 0;
	    break;
	}

	/*
	 * The operator requires special treatment, and is either "+" or "-",
	 * or one of "&&", "||" or "?".
	 */

	switch (opIndex) {
	case OP_PLUS:
	case OP_MINUS: {
	    Tcl_Token *afterSubexprPtr = exprTokenPtr
		    + exprTokenPtr->numComponents+1;
	    tokenPtr++;
	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    tokenPtr += (tokenPtr->numComponents + 1);

	    /*
	     * Check whether the "+" or "-" is unary.
	     */

	    if (tokenPtr == afterSubexprPtr) {
		TclEmitOpcode(((opIndex==OP_PLUS)? INST_UPLUS : INST_UMINUS),
			envPtr);
		break;
	    }

	    /*
	     * The "+" or "-" is binary.
	     */

	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    TclEmitOpcode(((opIndex==OP_PLUS)? INST_ADD : INST_SUB), envPtr);
	    *convertPtr = 0;
	    break;
	}

	case OP_LAND:
	case OP_LOR:
	    CompileLandOrLorExpr(interp, exprTokenPtr, opIndex, envPtr);
	    *convertPtr = 0;
	    break;

	case OP_QUESTY:
	    CompileCondExpr(interp, exprTokenPtr, convertPtr, envPtr);
	    break;

	default:
	    Tcl_Panic("CompileSubExpr: unexpected operator %d "
		    "requiring special treatment", opIndex);
	} /* end switch on operator requiring special treatment */
	break;

    }

    default:
	Tcl_Panic("CompileSubExpr: unexpected token type %d", tokenPtr->type);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CompileLandOrLorExpr --
 *
 *	This procedure compiles a Tcl logical and ("&&") or logical or ("||")
 *	subexpression.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static void
CompileLandOrLorExpr(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the "&&" or "||" operator. */
    int opIndex,		/* A code describing the expression operator:
				 * either OP_LAND or OP_LOR. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixup shortCircuitFixup;/* Used to fix up the short circuit jump after
				 * the first subexpression. */
    JumpFixup shortCircuitFixup2;
				/* Used to fix up the second jump to the
				 * short-circuit target. */
    JumpFixup endFixup;		/* Used to fix up jump to the end. */
    int convert = 0;
    int savedStackDepth = envPtr->currStackDepth;
    Tcl_Token *tokenPtr = exprTokenPtr+2;

    /*
     * Emit code for the first operand.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit the short-circuit jump.
     */

    TclEmitForwardJump(envPtr,
	    ((opIndex==OP_LAND)? TCL_FALSE_JUMP : TCL_TRUE_JUMP),
	    &shortCircuitFixup);

    /*
     * Emit code for the second operand.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);

    /*
     * The result is the boolean value of the second operand. We code this in
     * a somewhat contorted manner to be able to reuse the shortCircuit value
     * and save one INST_JUMP.
     */

    TclEmitForwardJump(envPtr,
	    ((opIndex==OP_LAND)? TCL_FALSE_JUMP : TCL_TRUE_JUMP),
	    &shortCircuitFixup2);

    if (opIndex == OP_LAND) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
    } else {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "0", 1), envPtr);
    }
    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &endFixup);

    /*
     * Fixup the short-circuit jumps and push the shortCircuit value.  Note
     * that shortCircuitFixup2 is always a short jump.
     */

    TclFixupForwardJumpToHere(envPtr, &shortCircuitFixup2, 127);
    if (TclFixupForwardJumpToHere(envPtr, &shortCircuitFixup, 127)) {
	/*
	 * shortCircuit jump grown by 3 bytes: update endFixup.
	 */

	 endFixup.codeOffset += 3;
    }

    if (opIndex == OP_LAND) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "0", 1), envPtr);
    } else {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
    }

    TclFixupForwardJumpToHere(envPtr, &endFixup, 127);
    envPtr->currStackDepth = savedStackDepth + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileCondExpr --
 *
 *	This procedure compiles a Tcl conditional expression:
 *	condExpr ::= lorExpr ['?' condExpr ':' condExpr]
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static void
CompileCondExpr(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the "?" operator. */
    int *convertPtr,		/* Describes the compilation state for the
				 * expression being compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixup jumpAroundThenFixup, jumpAroundElseFixup;
				/* Used to update or replace one-byte jumps
				 * around the then and else expressions when
				 * their target PCs are determined. */
    Tcl_Token *tokenPtr = exprTokenPtr+2;
    int elseCodeOffset, dist, convert = 0;
    int convertThen = 1, convertElse = 1;
    int savedStackDepth = envPtr->currStackDepth;

    /*
     * Emit code for the test.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit the jump to the "else" expression if the test was false.
     */

    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpAroundThenFixup);

    /*
     * Compile the "then" expression. Note that if a subexpression is only a
     * primary, we need to try to convert it to numeric. We do this to support
     * Tcl's policy of interpreting operands if at all possible as first
     * integers, else floating-point numbers.
     */

    CompileSubExpr(interp, tokenPtr, &convertThen, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit an unconditional jump around the "else" condExpr.
     */

    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpAroundElseFixup);

    /*
     * Compile the "else" expression.
     */

    envPtr->currStackDepth = savedStackDepth;
    elseCodeOffset = (envPtr->codeNext - envPtr->codeStart);
    CompileSubExpr(interp, tokenPtr, &convertElse, envPtr);

    /*
     * Fix up the second jump around the "else" expression.
     */

    dist = (envPtr->codeNext - envPtr->codeStart)
	    - jumpAroundElseFixup.codeOffset;
    if (TclFixupForwardJump(envPtr, &jumpAroundElseFixup, dist, 127)) {
	/*
	 * Update the else expression's starting code offset since it moved
	 * down 3 bytes too.
	 */

	elseCodeOffset += 3;
    }

    /*
     * Fix up the first jump to the "else" expression if the test was false.
     */

    dist = (elseCodeOffset - jumpAroundThenFixup.codeOffset);
    TclFixupForwardJump(envPtr, &jumpAroundThenFixup, dist, 127);
    *convertPtr = convertThen || convertElse;

    envPtr->currStackDepth = savedStackDepth + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileMathFuncCall --
 *
 *	This procedure compiles a call on a math function in an expression:
 *	mathFuncCall ::= funcName '(' [condExpr {',' condExpr}] ')'
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the math function at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static void
CompileMathFuncCall(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the math function call. */
    CONST char *funcName,	/* Name of the math function. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_DString cmdName;
    int objIndex;
    Tcl_Token *tokenPtr, *afterSubexprPtr;
    int argCount;

    /*
     * Prepend "tcl::mathfunc::" to the function name, to produce the name of
     * a command that evaluates the function.  Push that command name on the
     * stack, in a literal registered to the namespace so that resolution can
     * be cached.
     */

    Tcl_DStringInit(&cmdName);
    Tcl_DStringAppend(&cmdName, "tcl::mathfunc::", -1);
    Tcl_DStringAppend(&cmdName, funcName, -1);
    objIndex = TclRegisterNewNSLiteral(envPtr, Tcl_DStringValue(&cmdName),
	    Tcl_DStringLength(&cmdName));
    TclEmitPush(objIndex, envPtr);
    Tcl_DStringFree(&cmdName);

    /*
     * Compile any arguments for the function.
     */

    argCount = 1;
    tokenPtr = exprTokenPtr+2;
    afterSubexprPtr = exprTokenPtr + (exprTokenPtr->numComponents + 1);
    while (tokenPtr != afterSubexprPtr) {
	int convert = 0;
	++argCount;
	CompileSubExpr(interp, tokenPtr, &convert, envPtr);
	tokenPtr += (tokenPtr->numComponents + 1);
    }

    /* Invoke the function */

    if (argCount < 255) {
	TclEmitInstInt1(INST_INVOKE_STK1, argCount, envPtr);
    } else {
	TclEmitInstInt4(INST_INVOKE_STK4, argCount, envPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
