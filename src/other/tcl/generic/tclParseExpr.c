/*
 * tclParseExpr.c --
 *
 *	This file contains functions that parse Tcl expressions. They do so in
 *	a general-purpose fashion that can be used for many different
 *	purposes, including compilation, direct execution, code analysis, etc.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Contributions from Don Porter, NIST, 2006.  (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"

/*
 * The ExprNode structure represents one node of the parse tree produced
 * as an interim structure by the expression parser.
 */

typedef struct ExprNode {
    unsigned char lexeme;	/* Code that identifies the type of this node */
    int left;		/* Index of the left operand of this operator node */
    int right;		/* Index of the right operand of this operator node */
    int parent;		/* Index of the operator of this operand node */
    int token;		/* Index of the Tcl_Tokens of this leaf node */
} ExprNode;

/*
 * Set of lexeme codes stored in ExprNode structs to label and categorize
 * the lexemes found.
 */

#define LEAF		(1<<7)
#define UNARY		(1<<6)
#define BINARY		(1<<5)

#define NODE_TYPE	( LEAF | UNARY | BINARY)

#define PLUS		1
#define MINUS		2
#define BAREWORD	3
#define INCOMPLETE	4
#define INVALID		5

#define NUMBER		( LEAF | 1)
#define SCRIPT		( LEAF | 2)
#define BOOLEAN		( LEAF | BAREWORD)
#define BRACED		( LEAF | 4)
#define VARIABLE	( LEAF | 5)
#define QUOTED		( LEAF | 6)
#define EMPTY		( LEAF | 7)

#define UNARY_PLUS	( UNARY | PLUS)
#define UNARY_MINUS	( UNARY | MINUS)
#define FUNCTION	( UNARY | BAREWORD)
#define START		( UNARY | 4)
#define OPEN_PAREN	( UNARY | 5)
#define NOT		( UNARY | 6)
#define BIT_NOT		( UNARY | 7)

#define BINARY_PLUS	( BINARY |  PLUS)
#define BINARY_MINUS	( BINARY |  MINUS)
#define COMMA		( BINARY |  3)
#define MULT		( BINARY |  4)
#define DIVIDE		( BINARY |  5)
#define MOD		( BINARY |  6)
#define LESS		( BINARY |  7)
#define GREATER		( BINARY |  8)
#define BIT_AND		( BINARY |  9)
#define BIT_XOR		( BINARY | 10)
#define BIT_OR		( BINARY | 11)
#define QUESTION	( BINARY | 12)
#define COLON		( BINARY | 13)
#define LEFT_SHIFT	( BINARY | 14)
#define RIGHT_SHIFT	( BINARY | 15)
#define LEQ		( BINARY | 16)
#define GEQ		( BINARY | 17)
#define EQUAL		( BINARY | 18)
#define NEQ		( BINARY | 19)
#define AND		( BINARY | 20)
#define OR		( BINARY | 21)
#define STREQ		( BINARY | 22)
#define STRNEQ		( BINARY | 23)
#define EXPON		( BINARY | 24)
#define IN_LIST		( BINARY | 25)
#define NOT_IN_LIST	( BINARY | 26)
#define CLOSE_PAREN	( BINARY | 27)
#define END		( BINARY | 28)

/*
 * Declarations for local functions to this file:
 */

static void		GenerateTokens(ExprNode *nodes, Tcl_Parse *scratchPtr,
				Tcl_Parse *parsePtr);
static int		ParseLexeme(CONST char *start, int numBytes,
				unsigned char *lexemePtr);


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseExpr --
 *
 *	Given a string, the numBytes bytes starting at start, this function
 *	parses it as a Tcl expression and stores information about the
 *	structure of the expression in the Tcl_Parse struct indicated by the
 *	caller.
 *
 * Results:
 * 	If the string is successfully parsed as a valid Tcl expression,
 *	TCL_OK is returned, and data about the expression structure is
 *	written to *parsePtr.  If the string cannot be parsed as a valid
 *	Tcl expression, TCL_ERROR is returned, and if interp is non-NULL,
 *	an error message is written to interp.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the expression, then additional space is malloc-ed. If the
 *	function returns TCL_OK then the caller must eventually invoke
 *	Tcl_FreeParse to release any additional space that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    CONST char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    Tcl_Parse *parsePtr)	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is ignored. */
{
#define NUM_STATIC_NODES 64
    ExprNode staticNodes[NUM_STATIC_NODES];
    ExprNode *lastOrphanPtr, *nodes = staticNodes;
    int nodesAvailable = NUM_STATIC_NODES;
    int nodesUsed = 0;
    Tcl_Parse scratch;		/* Parsing scratch space */
    Tcl_Obj *msg = NULL, *post = NULL;
    int scanned = 0, code = TCL_OK, insertMark = 0;
    CONST char *mark = "_@_";
    CONST int limit = 25;
    static CONST unsigned char prec[80] = {
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  15,	15, 5,	16, 16,	16, 13,	13, 11,	10, 9,	6,  6,	14, 14,
	13, 13, 12, 12,	8,  7,	12, 12,	17, 12,	12, 3,	1,  0,	0,  0,
	0,  18,	18, 18,	2,  4,	18, 18,	0,  0,	0,  0,	0,  0,	0,  0,
    };

    if (numBytes < 0) {
	numBytes = (start ? strlen(start) : 0);
    }

    TclParseInit(interp, start, numBytes, &scratch);
    TclParseInit(interp, start, numBytes, parsePtr);

    /* Initialize the parse tree with the special "START" node */

    nodes->lexeme = START;
    nodes->left = -1;
    nodes->right = -1;
    nodes->parent = -1;
    nodes->token = -1;
    lastOrphanPtr = nodes;
    nodesUsed++;

    while ((code == TCL_OK) && (lastOrphanPtr->lexeme != END)) {
	ExprNode *nodePtr, *lastNodePtr;
	Tcl_Token *tokenPtr;

	/*
	 * Each pass through this loop adds one more ExprNode.
	 * Allocate space for one if required.
	 */
	if (nodesUsed >= nodesAvailable) {
	    int lastOrphanIdx = lastOrphanPtr - nodes;
	    int size = nodesUsed * 2;
	    ExprNode *newPtr;

	    if (nodes == staticNodes) {
		nodes = NULL;
	    }
	    do {
		newPtr = (ExprNode *) attemptckrealloc( (char *) nodes,
			(unsigned int) (size * sizeof(ExprNode)) );
	    } while ((newPtr == NULL)
		    && ((size -= (size - nodesUsed) / 2) > nodesUsed));
	    if (newPtr == NULL) {
		msg = Tcl_NewStringObj(
			"not enough memory to parse expression", -1);
		code = TCL_ERROR;
		continue;
	    }
	    nodesAvailable = size;
	    if (nodes == NULL) {
		memcpy((VOID *) newPtr, (VOID *) staticNodes,
			(size_t) (nodesUsed * sizeof(ExprNode)));
	    }
	    nodes = newPtr;
	    lastOrphanPtr = nodes + lastOrphanIdx;
	}
	nodePtr = nodes + nodesUsed;
	lastNodePtr = nodePtr - 1;

	/* Skip white space between lexemes */

	scanned = TclParseAllWhiteSpace(start, numBytes);
	start += scanned;
	numBytes -= scanned;

	scanned = ParseLexeme(start, numBytes, &(nodePtr->lexeme));

	/* Use context to categorize the lexemes that are ambiguous */

	if ((NODE_TYPE & nodePtr->lexeme) == 0) {
	    switch (nodePtr->lexeme) {
	    case INVALID:
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg,
			"invalid character \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case INCOMPLETE:
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg,
			"incomplete operator \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case BAREWORD:
		if (start[scanned+TclParseAllWhiteSpace(
			start+scanned, numBytes-scanned)] == '(') {
		    nodePtr->lexeme = FUNCTION;
		} else {
		    Tcl_Obj *objPtr = Tcl_NewStringObj(start, scanned);
		    Tcl_IncrRefCount(objPtr);
		    code = Tcl_ConvertToType(NULL, objPtr, &tclBooleanType);
		    Tcl_DecrRefCount(objPtr);
		    if (code == TCL_OK) {
			nodePtr->lexeme = BOOLEAN;
		    } else {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg, "invalid bareword \"%.*s%s\"",
				(scanned < limit) ? scanned : limit - 3, start,
				(scanned < limit) ? "" : "...");
			post = Tcl_NewObj();
			TclObjPrintf(NULL, post,
				"should be \"$%.*s%s\" or \"{%.*s%s}\"",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			TclObjPrintf(NULL, post, " or \"%.*s%s(...)\" or ...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			continue;
		    }
		}
		break;
	    case PLUS:
	    case MINUS:
		if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		    nodePtr->lexeme |= BINARY;
		} else {
		    nodePtr->lexeme |= UNARY;
		}
	    }
	}

	/* Add node to parse tree based on category */

	switch (NODE_TYPE & nodePtr->lexeme) {
	case LEAF: {
	    CONST char *end;

	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		CONST char *operand =
			scratch.tokenPtr[lastNodePtr->token].start;

		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "missing operator at %s", mark);
		if (operand[0] == '0') {
		    Tcl_Obj *copy = Tcl_NewStringObj(operand,
			    start + scanned - operand);
		    if (TclCheckBadOctal(NULL, Tcl_GetString(copy))) {
			post = Tcl_NewStringObj(
				"looks like invalid octal number", -1);
		    }
		    Tcl_DecrRefCount(copy);
		}
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }

	    if (scratch.numTokens+1 >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_SUB_EXPR;
	    tokenPtr->start = start;
	    scratch.numTokens++;

	    switch (nodePtr->lexeme) {
	    case NUMBER:
	    case BOOLEAN:
		tokenPtr = scratch.tokenPtr + scratch.numTokens;
		tokenPtr->type = TCL_TOKEN_TEXT;
		tokenPtr->start = start;
		tokenPtr->size = scanned;
		tokenPtr->numComponents = 0;
		scratch.numTokens++;

		break;

	    case QUOTED:
		code = Tcl_ParseQuotedString(interp, start, numBytes,
			&scratch, 1, &end);
		if (code != TCL_OK) {
		    scanned = scratch.term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		break;

	    case BRACED:
		code = Tcl_ParseBraces(interp, start, numBytes,
			&scratch, 1, &end);
		if (code != TCL_OK) {
		    continue;
		}
		scanned = end - start;
		break;

	    case VARIABLE:
		code = Tcl_ParseVarName(interp, start, numBytes, &scratch, 1);
		if (code != TCL_OK) {
		    scanned = scratch.term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		tokenPtr = scratch.tokenPtr + nodePtr->token + 1;
		if (tokenPtr->type != TCL_TOKEN_VARIABLE) {
		    msg = Tcl_NewStringObj("invalid character \"$\"", -1);
		    code = TCL_ERROR;
		    continue;
		}
		scanned = tokenPtr->size;
		break;

	    case SCRIPT:
		tokenPtr = scratch.tokenPtr + scratch.numTokens;
		tokenPtr->type = TCL_TOKEN_COMMAND;
		tokenPtr->start = start;
		tokenPtr->numComponents = 0;

		end = start + numBytes;
		start++;
		while (1) {
		    Tcl_Parse nested;
		    code = Tcl_ParseCommand(interp,
			    start, (end - start), 1, &nested);
		    if (code != TCL_OK) {
			parsePtr->term = nested.term;
			parsePtr->errorType = nested.errorType;
			parsePtr->incomplete = nested.incomplete;
			break;
		    }
		    start = (nested.commandStart + nested.commandSize);
		    Tcl_FreeParse(&nested);
		    if ((nested.term < end) && (*nested.term == ']')
			    && !nested.incomplete) {
			break;
		    }

		    if (start == end) {
			msg = Tcl_NewStringObj("missing close-bracket", -1);
			parsePtr->term = tokenPtr->start;
			parsePtr->errorType = TCL_PARSE_MISSING_BRACKET;
			parsePtr->incomplete = 1;
			code = TCL_ERROR;
			break;
		    }
		}
		end = start;
		start = tokenPtr->start;
		if (code != TCL_OK) {
		    scanned = parsePtr->term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		tokenPtr->size = scanned;
		scratch.numTokens++;
		break;
	    }

	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = scratch.numTokens - nodePtr->token - 1;

	    nodePtr->left = -1;
	    nodePtr->right = -1;
	    nodePtr->parent = -1;
	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;
	}

	case UNARY:
	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "missing operator at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }
	    nodePtr->left = -1;
	    nodePtr->right = -1;
	    nodePtr->parent = -1;

	    if (scratch.numTokens >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_OPERATOR;
	    tokenPtr->start = start;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = 0;
	    scratch.numTokens++;

	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;

	case BINARY: {
	    ExprNode *otherPtr = NULL;
	    unsigned char precedence = prec[nodePtr->lexeme];

	    if ((nodePtr->lexeme == CLOSE_PAREN)
		    && (lastNodePtr->lexeme == OPEN_PAREN)) {
		if (lastNodePtr[-1].lexeme == FUNCTION) {
		    /* Normally, "()" is a syntax error, but as a special
		     * case accept it as an argument list for a function */
		    scanned = 0;
		    nodePtr->lexeme = EMPTY;
		    nodePtr->left = -1;
		    nodePtr->right = -1;
		    nodePtr->parent = -1;
		    nodePtr->token = -1;

		    lastOrphanPtr = nodePtr;
		    nodesUsed++;
		    break;

		}
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "empty subexpression at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }


	    if ((NODE_TYPE & lastNodePtr->lexeme) != LEAF) {
		if (prec[lastNodePtr->lexeme] > precedence) {
		    if (lastNodePtr->lexeme == OPEN_PAREN) {
			msg = Tcl_NewStringObj("unbalanced open paren", -1);
		    } else if (lastNodePtr->lexeme == COMMA) {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg,
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    } else if (lastNodePtr->lexeme == START) {
			msg = Tcl_NewStringObj("empty expression", -1);
		    }
		} else {
		    if (nodePtr->lexeme == CLOSE_PAREN) {
			msg = Tcl_NewStringObj("unbalanced close paren", -1);
		    } else if ((nodePtr->lexeme == COMMA)
			    && (lastNodePtr->lexeme == OPEN_PAREN)
			    && (lastNodePtr[-1].lexeme == FUNCTION)) {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg,
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    }
		}
		if (msg == NULL) {
		    msg = Tcl_NewObj();
		    TclObjPrintf(NULL, msg, "missing operand at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		}
		code = TCL_ERROR;
		continue;
	    }

	    while (1) {

		if (lastOrphanPtr->parent >= 0) {
		    otherPtr = nodes + lastOrphanPtr->parent;
		} else if (lastOrphanPtr->left >= 0) {
		    Tcl_Panic("Tcl_ParseExpr: left closure programming error");
		} else {
		    lastOrphanPtr->parent = lastOrphanPtr - nodes;
		    otherPtr = lastOrphanPtr;
		}
		otherPtr--;

		if (prec[otherPtr->lexeme] < precedence) {
		    break;
		}

		/* Special association rules for the ternary operators */
		if (prec[otherPtr->lexeme] == precedence) {
		    if ((otherPtr->lexeme == QUESTION) 
			    && (lastOrphanPtr->lexeme != COLON)) {
			break;
		    }
		    if ((otherPtr->lexeme == COLON)
			    && (nodePtr->lexeme == QUESTION)) {
			break;
		    }
		}

		/* Some checks before linking */
		if ((otherPtr->lexeme == OPEN_PAREN)
			&& (nodePtr->lexeme != CLOSE_PAREN)) {
		    lastOrphanPtr = otherPtr;
		    msg = Tcl_NewStringObj("unbalanced open paren", -1);
		    code = TCL_ERROR;
		    break;
		}
		if ((otherPtr->lexeme == QUESTION)
			&& (lastOrphanPtr->lexeme != COLON)) {
		    msg = Tcl_NewObj();
		    TclObjPrintf(NULL, msg,
			    "missing operator \":\" at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    break;
		}
		if ((lastOrphanPtr->lexeme == COLON)
			&& (otherPtr->lexeme != QUESTION)) {
		    msg = Tcl_NewStringObj(
			    "unexpected operator \":\" without preceding \"?\"",
			    -1);
		    code = TCL_ERROR;
		    break;
		}

		/* Link orphan as right operand of otherPtr */
		otherPtr->right = lastOrphanPtr - nodes;
		lastOrphanPtr->parent = otherPtr - nodes;
		lastOrphanPtr = otherPtr;

		if (otherPtr->lexeme == OPEN_PAREN) {
		    /* CLOSE_PAREN can only close one OPEN_PAREN */
		    tokenPtr = scratch.tokenPtr + otherPtr->token;
		    tokenPtr->size = start + scanned - tokenPtr->start;
		    break;
		}
		if (otherPtr->lexeme == START) {
		    /* Don't backtrack beyond the start */
		    break;
		}
	    }
	    if (code != TCL_OK) {
		continue;
	    }

	    if (nodePtr->lexeme == CLOSE_PAREN) {
		if (otherPtr->lexeme == START) {
		    msg = Tcl_NewStringObj("unbalanced close paren", -1);
		    code = TCL_ERROR;
		    continue;
		}
		/* Create no node for a CLOSE_PAREN lexeme */
		break;
	    }

	    if ((nodePtr->lexeme == COMMA) && ((otherPtr->lexeme != OPEN_PAREN)
		    || (otherPtr[-1].lexeme != FUNCTION))) {
		msg = Tcl_NewStringObj(
			"unexpected \",\" outside function argument list", -1);
		code = TCL_ERROR;
		continue;
	    }

	    if (lastOrphanPtr->lexeme == COLON) {
		msg = Tcl_NewStringObj(
			"unexpected operator \":\" without preceding \"?\"",
			-1);
		code = TCL_ERROR;
		continue;
	    }

	    /* Link orphan as left operand of new node */
	    nodePtr->right = -1;

	    if (scratch.numTokens >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_OPERATOR;
	    tokenPtr->start = start;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = 0;
	    scratch.numTokens++;

	    nodePtr->left = lastOrphanPtr - nodes;
	    nodePtr->parent = lastOrphanPtr->parent;
	    lastOrphanPtr->parent = nodePtr - nodes;
	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;
	}
	}

	start += scanned;
	numBytes -= scanned;
    }

    if (code == TCL_OK) {
	/* Shift tokens from scratch space to caller space */
	GenerateTokens(nodes, &scratch, parsePtr);
    } else {
	if (parsePtr->errorType == TCL_PARSE_SUCCESS) {
	    parsePtr->errorType = TCL_PARSE_SYNTAX;
	    parsePtr->term = start;
	}
	if (interp == NULL) {
	    if (msg) {
		Tcl_DecrRefCount(msg);
	    }
	} else {
	    if (msg == NULL) {
		msg = Tcl_GetObjResult(interp);
	    }
	    TclObjPrintf(NULL, msg, "\nin expression \"%s%.*s%.*s%s%s%.*s%s\"",
		    ((start - limit) < scratch.string) ? "" : "...",
		    ((start - limit) < scratch.string)
		    ? (start - scratch.string) : limit - 3,
		    ((start - limit) < scratch.string) 
		    ? scratch.string : start - limit + 3,
		    (scanned < limit) ? scanned : limit - 3, start,
		    (scanned < limit) ? "" : "...",
		    insertMark ? mark : "",
		    (start + scanned + limit > scratch.end)
		    ? scratch.end - (start + scanned) : limit-3, 
		    start + scanned,
		    (start + scanned + limit > scratch.end) ? "" : "..."
		    );
	    if (post != NULL) {
		Tcl_AppendToObj(msg, ";\n", -1);
		Tcl_AppendObjToObj(msg, post);
		Tcl_DecrRefCount(post);
	    }
	    Tcl_SetObjResult(interp, msg);
	    numBytes = scratch.end - scratch.string;
	    TclFormatToErrorInfo(interp,
		    "\n    (parsing expression \"%.*s%s\")",
		    (numBytes < limit) ? numBytes : limit - 3,
		    scratch.string, (numBytes < limit) ? "" : "...");
	}
    }

    if (nodes != staticNodes) {
	ckfree((char *)nodes);
    }
    Tcl_FreeParse(&scratch);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateTokens --
 *
 * 	Routine that generates Tcl_Tokens that represent a Tcl expression
 * 	and writes them to *parsePtr.  The parse tree of the expression
 * 	is in the array of ExprNodes, nodes.  Some of the Tcl_Tokens are
 *	copied from scratch space at *scratchPtr, where the parsing pass
 *	that constructed the parse tree left them.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateTokens(
    ExprNode *nodes,
    Tcl_Parse *scratchPtr,
    Tcl_Parse *parsePtr)
{
    ExprNode *nodePtr = nodes + nodes->right;
    Tcl_Token *sourcePtr, *destPtr, *tokenPtr = scratchPtr->tokenPtr;
    int toCopy;
    CONST char *end = tokenPtr->start + tokenPtr->size;

    while (nodePtr->lexeme != START) {
	switch (NODE_TYPE & nodePtr->lexeme) {
	case BINARY:
	    if (nodePtr->left >= 0) {
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    nodePtr->token = parsePtr->numTokens;
		    destPtr->type = TCL_TOKEN_SUB_EXPR;
		    destPtr->start = tokenPtr->start;
		    destPtr++;
		    *destPtr = *sourcePtr;
		    parsePtr->numTokens += 2;
		}
		nodePtr = nodes + nodePtr->left;
		nodes[nodePtr->parent].left = -1;
	    } else if (nodePtr->right >= 0) {
		tokenPtr += tokenPtr->numComponents + 1;
		nodePtr = nodes + nodePtr->right;
		nodes[nodePtr->parent].right = -1;
	    } else {
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    destPtr = parsePtr->tokenPtr + nodePtr->token;
		    destPtr->size = end - destPtr->start;
		    destPtr->numComponents =
			    parsePtr->numTokens - nodePtr->token - 1;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;

	case UNARY:
	    if (nodePtr->right >= 0) {
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		if (nodePtr->lexeme != OPEN_PAREN) {
		    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    nodePtr->token = parsePtr->numTokens;
		    destPtr->type = TCL_TOKEN_SUB_EXPR;
		    destPtr->start = tokenPtr->start;
		    destPtr++;
		    *destPtr = *sourcePtr;
		    parsePtr->numTokens += 2;
		}
		if (tokenPtr == sourcePtr) {
		    tokenPtr += tokenPtr->numComponents + 1;
		}
		nodePtr = nodes + nodePtr->right;
		nodes[nodePtr->parent].right = -1;
	    } else {
		if (nodePtr->lexeme != OPEN_PAREN) {
		    destPtr = parsePtr->tokenPtr + nodePtr->token;
		    destPtr->size = end - destPtr->start;
		    destPtr->numComponents =
			    parsePtr->numTokens - nodePtr->token - 1;
		} else {
		    sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		    end = sourcePtr->start + sourcePtr->size;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;

	case LEAF:
	    switch (nodePtr->lexeme) {
	    case EMPTY:
		break;

	    case BRACED:
	    case QUOTED:
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		end = sourcePtr->start + sourcePtr->size;
		if (sourcePtr->numComponents > 1) {
		    toCopy = sourcePtr->numComponents;
		    if (tokenPtr == sourcePtr) {
			tokenPtr += toCopy + 1;
		    }
		    sourcePtr->numComponents++;
		    while (parsePtr->numTokens + toCopy + 1
			    >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    *destPtr++ = *sourcePtr;
		    *destPtr = *sourcePtr++;
		    destPtr->type = TCL_TOKEN_WORD;
		    destPtr->numComponents = toCopy;
		    destPtr++;
		    memcpy((VOID *) destPtr, (VOID *) sourcePtr,
			    (size_t) (toCopy * sizeof(Tcl_Token)));
		    parsePtr->numTokens += toCopy + 2;
		    break;
		}

	    default:
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		end = sourcePtr->start + sourcePtr->size;
		toCopy = sourcePtr->numComponents + 1;
		if (tokenPtr == sourcePtr) {
		    tokenPtr += toCopy;
		}
		while (parsePtr->numTokens + toCopy - 1
			>= parsePtr->tokensAvailable) {
		    TclExpandTokenArray(parsePtr);
		}
		destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		memcpy((VOID *) destPtr, (VOID *) sourcePtr,
			(size_t) (toCopy * sizeof(Tcl_Token)));
		parsePtr->numTokens += toCopy;
		break;

	    }
	    nodePtr = nodes + nodePtr->parent;
	    break;

	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ParseLexeme --
 *
 *	Parse a single lexeme from the start of a string, scanning no
 *	more than numBytes bytes.  
 *
 * Results:
 *	Returns the number of bytes scanned to produce the lexeme.
 *
 * Side effects:
 *	Code identifying lexeme parsed is writen to *lexemePtr.
 *
 *----------------------------------------------------------------------
 */

static int
ParseLexeme(
    CONST char *start,		/* Start of lexeme to parse. */
    int numBytes,		/* Number of bytes in string. */
    unsigned char *lexemePtr)	/* Write code of parsed lexeme to this
				 * storage. */
{
    CONST char *end;
    int scanned;
    Tcl_UniChar ch;

    if (numBytes == 0) {
	*lexemePtr = END;
	return 0;
    }
    switch (*start) {
    case '[':
	*lexemePtr = SCRIPT;
	return 1;

    case '{':
	*lexemePtr = BRACED;
	return 1;

    case '(':
	*lexemePtr = OPEN_PAREN;
	return 1;

    case ')':
	*lexemePtr = CLOSE_PAREN;
	return 1;

    case '$':
	*lexemePtr = VARIABLE;
	return 1;

    case '\"':
	*lexemePtr = QUOTED;
	return 1;

    case ',':
	*lexemePtr = COMMA;
	return 1;

    case '/':
	*lexemePtr = DIVIDE;
	return 1;

    case '%':
	*lexemePtr = MOD;
	return 1;

    case '+':
	*lexemePtr = PLUS;
	return 1;

    case '-':
	*lexemePtr = MINUS;
	return 1;

    case '?':
	*lexemePtr = QUESTION;
	return 1;

    case ':':
	*lexemePtr = COLON;
	return 1;

    case '^':
	*lexemePtr = BIT_XOR;
	return 1;

    case '~':
	*lexemePtr = BIT_NOT;
	return 1;

    case '*':
	if ((numBytes > 1) && (start[1] == '*')) {
	    *lexemePtr = EXPON;
	    return 2;
	}
	*lexemePtr = MULT;
	return 1;

    case '=':
	if ((numBytes > 1) && (start[1] == '=')) {
	    *lexemePtr = EQUAL;
	    return 2;
	}
	*lexemePtr = INCOMPLETE;
	return 1;

    case '!':
	if ((numBytes > 1) && (start[1] == '=')) {
	    *lexemePtr = NEQ;
	    return 2;
	}
	*lexemePtr = NOT;
	return 1;

    case '&':
	if ((numBytes > 1) && (start[1] == '&')) {
	    *lexemePtr = AND;
	    return 2;
	}
	*lexemePtr = BIT_AND;
	return 1;

    case '|':
	if ((numBytes > 1) && (start[1] == '|')) {
	    *lexemePtr = OR;
	    return 2;
	}
	*lexemePtr = BIT_OR;
	return 1;

    case '<':
	if (numBytes > 1) {
	    switch (start[1]) {
	    case '<':
		*lexemePtr = LEFT_SHIFT;
		return 2;
	    case '=':
		*lexemePtr = LEQ;
		return 2;
	    }
	}
	*lexemePtr = LESS;
	return 1;

    case '>':
	if (numBytes > 1) {
	    switch (start[1]) {
	    case '>':
		*lexemePtr = RIGHT_SHIFT;
		return 2;
	    case '=':
		*lexemePtr = GEQ;
		return 2;
	    }
	}
	*lexemePtr = GREATER;
	return 1;

    case 'i':
	if ((numBytes > 1) && (start[1] == 'n')
		&& ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    /*
	     * Must make this check so we can tell the difference between
	     * the "in" operator and the "int" function name and the
	     * "infinity" numeric value.
	     */
	    *lexemePtr = IN_LIST;
	    return 2;
	}
	break;

    case 'e':
	if ((numBytes > 1) && (start[1] == 'q')
		&& ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    *lexemePtr = STREQ;
	    return 2;
	}
	break;

    case 'n':
	if ((numBytes > 1) && ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    switch (start[1]) {
	    case 'e':
		*lexemePtr = STRNEQ;
		return 2;
	    case 'i':
		*lexemePtr = NOT_IN_LIST;
		return 2;
	    }
	}
    }

    if (TclParseNumber(NULL, NULL, NULL, start, numBytes, &end,
	    TCL_PARSE_NO_WHITESPACE) == TCL_OK) {
	*lexemePtr = NUMBER;
	return (end-start);
    }

    if (Tcl_UtfCharComplete(start, numBytes)) {
	scanned = Tcl_UtfToUniChar(start, &ch);
    } else {
	char utfBytes[TCL_UTF_MAX];
	memcpy(utfBytes, start, (size_t) numBytes);
	utfBytes[numBytes] = '\0';
	scanned = Tcl_UtfToUniChar(utfBytes, &ch);
    }
    if (!isalpha(UCHAR(ch))) {
	*lexemePtr = INVALID;
	return scanned;
    }
    end = start;
    while (isalnum(UCHAR(ch)) || (UCHAR(ch) == '_')) {
	end += scanned;
	numBytes -= scanned;
	if (Tcl_UtfCharComplete(end, numBytes)) {
	    scanned = Tcl_UtfToUniChar(end, &ch);
	} else {
	    char utfBytes[TCL_UTF_MAX];
	    memcpy(utfBytes, end, (size_t) numBytes);
	    utfBytes[numBytes] = '\0';
	    scanned = Tcl_UtfToUniChar(utfBytes, &ch);
	}
    }
    *lexemePtr = BAREWORD;
    return (end-start);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
