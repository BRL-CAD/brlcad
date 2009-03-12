#ifndef EXPRESSION_H
#define EXPRESSION_H

/* $Id: expr.h,v 1.4 1997/01/21 19:17:11 dar Exp $ */

/************************************************************************
** Module:	Expression
** Description:	This module implements the Expression abstraction.
**	Several types of expressions are supported: identifiers,
**	literals, operations (arithmetic, logical, array indexing,
**	etc.), function calls, and query expressions.  Every expression
**	is marked with a type.
** Constants:
**	EXPRESSION_NULL		- the null expression
**	LITERAL_E		- a real literal with the value 2.7182...
**	LITERAL_EMPTY_SET	- a set literal representing the empty set
**	LITERAL_INFINITY	- a numeric literal representing infinity
**	LITERAL_PI		- a real literal with the value 3.1415...
**	LITERAL_ZERO		- an integer literal representing 0
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: expr.h,v $
 * Revision 1.4  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.3  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/16  03:21:31  libes
 * fixed numerous confusions of type with return type
 * fixed implicit loop variable type declarations
 * improved errors
 *
 * Revision 1.5  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/27  23:39:59  libes
 * *** empty log message ***
 *
 * Revision 1.3  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define EXPRESSION_NULL	(Expression)0

/*****************/
/* packages used */
/*****************/

#include <math.h>
#include "expbasic.h"	/* get basic definitions */

#ifndef MAXINT
#define MAXINT (~(1 << 31))
#endif

/************/
/* typedefs */
/************/

typedef enum {
	OP_AND,			OP_ANDOR,
	OP_ARRAY_ELEMENT,	OP_CONCAT,
	OP_DIV,			OP_DOT,			OP_EQUAL,
	OP_EXP,			OP_GREATER_EQUAL,	OP_GREATER_THAN,
	OP_GROUP,
	OP_IN,			OP_INST_EQUAL,		OP_INST_NOT_EQUAL,
	OP_LESS_EQUAL,		OP_LESS_THAN,		OP_LIKE,
	OP_MINUS,		OP_MOD,			OP_NEGATE,
	OP_NOT,			OP_NOT_EQUAL,		OP_OR,
	OP_PLUS,		OP_REAL_DIV,		OP_SUBCOMPONENT,
	OP_TIMES,		OP_XOR,			OP_UNKNOWN,
	OP_LAST	/* must be last - used only to size tables */
} Op_Code;

typedef struct Qualified_Attr	Qualified_Attr;
typedef struct Expression_	*Expression;
typedef Expression		Ary_Expression, One_Of_Expression, Identifier,
				Literal;
typedef struct Query_		*Query;
typedef One_Of_Expression	Function_Call;
typedef Ary_Expression		Ternary_Expression, Binary_Expression,
				Unary_Expression;
typedef Literal			Aggregate_Literal, Integer_Literal,
				Logical_Literal, Real_Literal, String_Literal,
				Binary_Literal;

/****************/
/* modules used */
/****************/

#include "entity.h"
#include "type.h"
#include "variable.h"
#include "alg.h"
#include "scope.h"

/***************************/
/* hidden type definitions */
/***************************/

/* expression types */

struct Qualified_Attr {
	struct Expression_ *complex;	/* complex entity instance */
	Symbol *entity;
	Symbol *attribute;
};

struct Op_Subexpression {
	Op_Code op_code;
	Expression op1;
	Expression op2;
	Expression op3;
};

struct Query_ {
	Variable local;
	Expression aggregate;	/* set from which to test */
	Expression expression;	/* logical expression */
	struct Scope_ *scope;
};

struct Funcall {
	struct Scope_ *function;/* can also be an entity because entities */
				/* can be called as functions */
	Linked_List list;
};

union expr_union {
	int integer;
	float real;
/*	char *string;		find string name in symbol in Expression */
	char *attribute;	/* inverse .... for 'attr' */
	char *binary;
	int logical;
	Boolean boolean;
	struct Query_ *query;
	struct Funcall funcall;

	/* if etype == aggregate, list of expressions */
	/* if etype == funcall, 1st element of list is funcall or entity */
	/*	remaining elements are parameters */
	Linked_List list;	/* aggregates (bags, lists, sets, arrays) */
				/* or lists for oneof expressions */
	Expression expression;	/* derived value in derive attrs, or*/
				/* initializer in local vars, or */
				/* enumeration tags */
				/* or oneof value */
	struct Scope_ *entity;	/* used by subtype exp, group expr */
				/* and self expr, some funcall's and any */
				/* expr that results in an entity */
	Variable variable;	/* attribute reference */
};

struct Expression_ {
	Symbol symbol;		/* contains things like funcall names */
				/* string names, binary values, */
				/* enumeration names */
	Type type;
	Type return_type;	/* type of value returned by expression */
		/* The difference between 'type' and 'return_type' is */
		/* illustrated by "func(a)".  Here, 'type' is Type_Function */
		/* while 'return_type'  might be Type_Integer (assuming func */
		/* returns an integer). */
	struct Op_Subexpression e;
	union expr_union u;
};

/* indexed by the op enumeration values */
struct EXPop_entry {
	char *token;		/* literal token, e.g., "<>" */
	Type (*resolve) PROTO((Expression,struct Scope_ *));
};

/********************/
/* global variables */
/********************/

#ifdef EXPRESSION_C
#include "defstart.h"
#else
#include "decstart.h"
#endif /*EXPRESSION_C*/

GLOBAL struct EXPop_entry EXPop_table[OP_LAST];

GLOBAL Expression 	LITERAL_E		INITIALLY(EXPRESSION_NULL);
GLOBAL Expression 	LITERAL_INFINITY	INITIALLY(EXPRESSION_NULL);
GLOBAL Expression 	LITERAL_PI		INITIALLY(EXPRESSION_NULL);
GLOBAL Expression 	LITERAL_ZERO		INITIALLY(EXPRESSION_NULL);
GLOBAL Expression 	LITERAL_ONE;

GLOBAL Error	ERROR_bad_qualification			INITIALLY(ERROR_none);
GLOBAL Error	ERROR_integer_expression_expected	INITIALLY(ERROR_none);
GLOBAL Error	ERROR_implicit_downcast			INITIALLY(ERROR_none);
GLOBAL Error	ERROR_ambig_implicit_downcast		INITIALLY(ERROR_none);

GLOBAL struct freelist_head EXP_fl;
GLOBAL struct freelist_head OP_fl;
GLOBAL struct freelist_head QUERY_fl;
GLOBAL struct freelist_head QUAL_ATTR_fl;

#include "de_end.h"

/******************************/
/* macro function definitions */
/******************************/

#define EXP_new()	(struct Expression_ *)MEM_new(&EXP_fl)
#define EXP_destroy(x)	MEM_destroy(&EXP_fl,(Freelist *)(Generic)x)
#define OP_new()	(struct Op_Subexpression *)MEM_new(&OP_fl)
#define OP_destroy(x)	MEM_destroy(&OP_fl,(Freelist *)(Generic)x)
#define QUERY_new()	(struct Query_ *)MEM_new(&QUERY_fl)
#define QUERY_destroy(x) MEM_destroy(&QUERY_fl,(Freelist *)(Generic)x)
#define QUAL_ATTR_new()	(struct Qualified_Attr *)MEM_new(&QUAL_ATTR_fl)
#define QUAL_ATTR_destroy(x) MEM_destroy(&QUAL_ATTR_fl,(Freelist *)(Generic)x)

#define EXPget_name(e)			((e)->symbol.name)
#define ENUMget_name(e)			((e)->symbol.name)

#define BIN_EXPget_operator(e)		ARY_EXPget_operator(e)
#define BIN_EXPget_first_operand(e)	ARY_EXPget_operand(e)

#define UN_EXPget_operator(e)		ARY_EXPget_operator(e)
#define UN_EXPget_operand(e)		ARY_EXPget_operand(e)

#define FCALLput_parameters(expr,parms)	((e)->u.funcall.list = (parms))
#define FCALLget_parameters(e)		((e)->u.funcall.list)
#define FCALLput_function(expr,func)	((e)->u.funcall.function = (func))
#define FCALLget_function(e)		((e)->u.funcall.function)
/* assumes the function is not an entity-function! */
#define FCALLget_algorithm(e)		((e)->u.funcall.function->u.function->body)

#define INT_LITget_value(e)		((e)->u.integer)
#define LOG_LITget_value(e)		((e)->u.logical)
#define REAL_LITget_value(e)		((e)->u.real)
#define STR_LITget_value(e)		((e)->symbol.name)
#define STR_LITput_value(e,s)		((e)->symbol.name = (s))
#define BIN_LITget_value(e)		((e)->u.binary)
#define AGGR_LITget_value(e)		((e)->u.list)
#define EXPget_type(e)			((e)->type)
#define ARY_EXPget_operand(e)		((e)->e.op1)
#define ARY_EXPget_operator(e)		((e)->e.op_code)
#define BIN_EXPget_operand(e)		((e)->e.op2)
#define TERN_EXPget_second_operand(e)	((e)->e.op2)
#define TERN_EXPget_third_operand(e)	((e)->e.op3)
#define	QUERYget_variable(e)		((e)->u.query->local)
#define QUERYget_source(e)		((e)->u.query->aggregate)
#define QUERYget_discriminant(e)	((e)->u.query->expression)
#define ONEOFget_list(e)		((e)->u.list)

/***********************/
/* function prototypes */
/***********************/

extern Expression 	EXPcreate PROTO((Type));
extern Expression 	EXPcreate_simple PROTO((Type));
extern Expression 	EXPcreate_from_symbol PROTO((Type,Symbol *));
extern Expression 	UN_EXPcreate PROTO((Op_Code, Expression));
extern Expression 	BIN_EXPcreate PROTO((Op_Code, Expression, Expression));
extern Expression 	TERN_EXPcreate PROTO((Op_Code, Expression, Expression, Expression));
extern Expression 	QUERYcreate PROTO((Symbol *, Expression));
extern void		EXPinitialize PROTO((void));
extern Type		EXPtype PROTO((Expression,struct Scope_ *));
extern int		EXPget_integer_value PROTO((Expression));

/********************/
/* inline functions */
/********************/

#if supports_inline_functions || defined(EXPRESSION_C)

static_inline
int
OPget_number_of_operands(Op_Code op)
{
	if ((op == OP_NEGATE) || (op == OP_NOT)) return 1;
	else if (op == OP_SUBCOMPONENT) return 3;
	else return 2;
}

#endif /* supports_inline_functions || defined(EXPRESSION_C) */

#endif /*EXPRESSION_H*/
