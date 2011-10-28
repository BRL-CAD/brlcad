#ifndef STATEMENT_H
#define STATEMENT_H

/* $Id: stmt.h,v 1.3 1997/01/21 19:17:11 dar Exp $ */

/************************************************************************
** Module:	Statement
** Description:	This module implements the Statement abstraction.  A
**	statement is, in effect, a typeless Expression.  Due to the
**	existence of complex language constructs, however, it often is
**	not practical to implement the abstraction thus.  For this reason,
**	there is a separate module for statements.  This abstraction
**	supports various looping constructs, if/then/else, switch
**	statements, and procedure calls.
** Constants:
**	STATEMENT_NULL	- the null statement
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: stmt.h,v $
 * Revision 1.3  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/16  03:27:02  libes
 * removed artifical begin/end nesting for improved reconstruction (printing)
 * of original Express file
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define STATEMENT_NULL		(Statement)0
#define STATEMENT_LIST_NULL	(Linked_List)0

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include "scope.h"

/************/
/* typedefs */
/************/

typedef struct Statement_	*Statement,
				*Alias,
				*Assignment,
				*Case_Statement,
				*Compound_Statement,
				*Conditional,
				*Loop,
				*Procedure_Call,
				*Return_Statement;

typedef struct Scope_ *Increment;

/****************/
/* modules used */
/****************/

#include "expr.h"
#include "alg.h"

/***************************/
/* hidden type definitions */
/***************************/

#define STMT_ASSIGN	0x1
#define STMT_CASE	0x2
#define STMT_COMPOUND	0x4
#define STMT_COND	0x8
#define STMT_LOOP	0x10
#define STMT_PCALL	0x20
#define STMT_RETURN	0x40
#define STMT_ALIAS	0x80
#define STMT_SKIP	0x100
#define STMT_ESCAPE	0x200

/* these should probably all be expression types */

struct Statement_ {
	Symbol symbol;	/* can hold pcall or alias name */
			/* but otherwise is not used for anything */
	int type;	/* one of STMT_XXX above */
	/* hey, is there nothing in common beside symbol and private data?? */
	union u_statement {
		struct Alias_		   *alias;
		struct Assignment_	   *assign;
		struct Case_Statement_	   *Case;
		struct Compound_Statement_ *compound;
		struct Conditional_	   *cond;
		struct Loop_		   *loop;
		struct Procedure_Call_	   *proc;
		struct Return_Statement_   *ret;
		/* skip & escape have no data */
	} u;
};

struct Alias_ {
	struct Scope_ *scope;
	struct Variable_ *variable;
	Linked_List statements;		/* list of statements */
};

struct Assignment_ {
	Expression lhs;
	Expression rhs;
};

struct Case_Statement_ {
	Expression selector;
	Linked_List cases;
};

struct Compound_Statement_ {
	Linked_List	statements;
};

struct Conditional_ {
	Expression test;
	Linked_List code;		/* list of statements */
	Linked_List otherwise;		/* list of statements */
};

struct Loop_ {
	struct Scope_ *scope;		/* scope for increment control */
	Expression while_expr;
	Expression until_expr;
	Linked_List statements;		/* list of statements */
};

/* this is an element in the optional Loop scope */
struct Increment_ {
    Expression init;
    Expression end;
    Expression increment;
};

struct Procedure_Call_ {
    struct Scope_ *procedure;
    Linked_List	parameters;	/* list of expressions */
};

struct Return_Statement_ {
    Expression value;
};



/********************/
/* global variables */
/********************/

extern struct freelist_head STMT_fl;

extern struct freelist_head ALIAS_fl;
extern struct freelist_head ASSIGN_fl;
extern struct freelist_head CASE_fl;
extern struct freelist_head COMP_STMT_fl;
extern struct freelist_head COND_fl;
extern struct freelist_head LOOP_fl;
extern struct freelist_head PCALL_fl;
extern struct freelist_head RET_fl;
extern struct freelist_head INCR_fl;

extern Statement STATEMENT_ESCAPE;
extern Statement STATEMENT_SKIP;

/******************************/
/* macro function definitions */
/******************************/

#define STMT_new()	(struct Statement_ *)MEM_new(&STMT_fl)
#define STMT_destroy(x)	MEM_destroy(&STMT_fl,(Freelist *)(Generic)x)

#define ALIAS_new()		(struct Alias_ *)MEM_new(&ALIAS_fl)
#define ALIAS_destroy(x)		MEM_destroy(&ALIAS_fl,(Freelist *)(Generic)x)
#define ASSIGN_new()		(struct Assignment_ *)MEM_new(&ASSIGN_fl)
#define ASSIGN_destroy(x)		MEM_destroy(&ASSIGN_fl,(Freelist *)(Generic)x)
#define CASE_new()		(struct Case_Statement_ *)MEM_new(&CASE_fl)
#define CASE_destroy(x)		MEM_destroy(&CASE_fl,(Freelist *)(Generic)x)
#define COMP_STMT_new()		(struct Compound_Statement_ *)MEM_new(&COMP_STMT_fl)
#define COMP_STMT_destroy(x)		MEM_destroy(&COMP_STMT_fl,(Freelist *)(Generic)x)
#define COND_new()		(struct Conditional_ *)MEM_new(&COND_fl)
#define COND_destroy(x)		MEM_destroy(&COND_fl,(Freelist *)(Generic)x)
#define LOOP_new()		(struct Loop_ *)MEM_new(&LOOP_fl)
#define LOOP_destroy(x)		MEM_destroy(&LOOP_fl,(Freelist *)(Generic)x)
#define PCALL_new()		(struct Procedure_Call_ *)MEM_new(&PCALL_fl)
#define PCALL_destroy(x)	MEM_destroy(&PCALL_fl,(Freelist *)(Generic)x)
#define RET_new()		(struct Return_Statement_ *)MEM_new(&RET_fl)
#define RET_destroy(x)		MEM_destroy(&RET_fl,(Freelist *)(Generic)x)

#define INCR_new()		(struct Increment_ *)MEM_new(&INCR_fl)
#define INCR_destroy(x)		MEM_destroy(&INCR_fl,(Freelist *)(char *)x)

#define	ASSIGNget_lhs(s)	((s)->u.assign->lhs)
#define	ASSIGNget_rhs(s)	((s)->u.assign->rhs)
#define	CASEget_selector(s)	((s)->u.Case->selector)
#define	CASEget_items(s)	((s)->u.Case->cases)
#define	COMP_STMTget_items(s)	((s)->u.compound->statements)
#define	CONDget_condition(s)	((s)->u.cond->test)
#define	CONDget_then_clause(s)	((s)->u.cond->code)
#define	CONDget_else_clause(s)	((s)->u.cond->otherwise)
#define	LOOPget_while(s)	((s)->u.loop->while)
#define	LOOPget_until(s)	((s)->u.loop->until)
#define	LOOPget_body(s)		((s)->u.loop->statement)
#define	PCALLget_procedure(s)	((s)->u.proc->procedure)
#define	PCALLget_parameters(s)	((s)->u.proc->parameters)
#define	RETget_expression(s)	((s)->u.ret->value)

/***********************/
/* function prototypes */
/***********************/

extern Statement	STMTcreate PROTO((int));
extern Statement	ALIAScreate PROTO((struct Scope_ *,Variable,Linked_List));
extern Statement	CASEcreate PROTO((Expression , Linked_List));
extern Statement	ASSIGNcreate PROTO((Expression , Expression ));
extern Statement	COMP_STMTcreate PROTO((Linked_List));
extern Statement	CONDcreate PROTO((Expression,Linked_List,Linked_List));
extern Statement	LOOPcreate PROTO((struct Scope_ *,Expression,Expression,Linked_List));
extern Statement	PCALLcreate PROTO((Linked_List));
extern Statement	RETcreate PROTO((Expression ));
extern void		STMTinitialize PROTO((void));
extern struct Scope_ *INCR_CTLcreate PROTO((Symbol *, Expression start,
	       Expression end, Expression increment));

#endif /*STATEMENT_H*/
