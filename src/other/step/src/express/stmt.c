static char rcsid[] = "$Id: stmt.c,v 1.3 1997/01/21 19:19:51 dar Exp $";

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
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: stmt.c,v $
 * Revision 1.3  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/16  03:27:02  libes
 * removed artifical begin/end nesting for improved reconstruction (printing)
 * of original Express file
 *
 * Revision 1.5  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 * 
 * Revision 4.1  90/09/13  15:13:01  clark
 * BPR 2.1 alpha
 * 
 */

#define STATEMENT_C
#include "express/stmt.h"

Statement 
STMTcreate(int type)
{
	Statement s;
	s = STMT_new();
	SYMBOLset(s);
	s->type = type;
	return s;
}

/*
** Procedure:	STMTinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Statement module.
*/

void
STMTinitialize(void)
{
	MEMinitialize(&STMT_fl,sizeof(struct Statement_),500,100);

	MEMinitialize(&ALIAS_fl,sizeof(struct Alias_),10,10);
	MEMinitialize(&ASSIGN_fl,sizeof(struct Assignment_),100,30);
	MEMinitialize(&CASE_fl,sizeof(struct Case_Statement_),100,30);
	MEMinitialize(&COMP_STMT_fl,sizeof(struct Compound_Statement_),100,30);
	MEMinitialize(&COND_fl,sizeof(struct Conditional_),100,30);
	MEMinitialize(&LOOP_fl,sizeof(struct Loop_),100,30);
	MEMinitialize(&PCALL_fl,sizeof(struct Procedure_Call_),100,30);
	MEMinitialize(&RET_fl,sizeof(struct Return_Statement_),100,30);
	MEMinitialize(&INCR_fl,sizeof(struct Increment_),100,30);

	OBJcreate(OBJ_ALIAS,SCOPE_get_symbol,"alias scope",OBJ_UNUSED_BITS);
	OBJcreate(OBJ_INCREMENT,SCOPE_get_symbol,"increment scope",OBJ_UNUSED_BITS);

	STATEMENT_SKIP = STMTcreate(STMT_SKIP);
	STATEMENT_ESCAPE = STMTcreate(STMT_ESCAPE);
}

/*
** Procedure:	ASSIGNcreate
** Parameters:	Expression lhs	- the left-hand-side of the assignment
**		Expression rhs	- the right-hand-side of the assignment
**		Error*     experrc	- buffer for error code
** Returns:	Assignment	- the assignment statement created
** Description:	Create and return an assignment statement.
*/

Statement 
ASSIGNcreate(Expression lhs, Expression rhs)
{
	Statement s;
	s = STMTcreate(STMT_ASSIGN);
	s->u.assign = ASSIGN_new();
	s->u.assign->lhs = lhs;
	s->u.assign->rhs = rhs;
	return s;
}

/*
** Procedure:	CASEcreate
** Parameters:	Expression  selector	- expression to case on
**		Linked_List cases	- list of Case_Items
**		Error*      experrc	- buffer for error code
** Returns:	Case_Statement		- the case statement created
** Description:	Create and return a case statement.
*/

Statement 
CASEcreate(Expression selector, Linked_List cases)
{
	Statement s;

	s = STMTcreate(STMT_CASE);
	s->u.Case = CASE_new();
	s->u.Case->selector = selector;
	s->u.Case->cases = cases;
	return(s);
}

/*
** Procedure:	COMP_STMTcreate
** Parameters:	Linked_List statements	- list of Statements making up
**					  the compound statement
**		Error*      experrc	- buffer for error code
** Returns:	Compound_Statement	- the compound statement created
** Description:	Create and return a compound statement.
*/

Statement 
COMP_STMTcreate(Linked_List statements)
{
	Statement s;
	s = STMTcreate(STMT_COMPOUND);
	s->u.compound = COMP_STMT_new();
	s->u.compound->statements = statements;
	return s;
}

/*
** Procedure:	CONDcreate
** Parameters:	Expression test		- the condition for the if
**		Statement  then		- code executed for test == true
**		Statement  otherwise	- code executed for test == false
**		Error*      experrc	- buffer for error code
** Returns:	Conditional		- the if statement created
** Description:	Create and return an if statement.
*/

Statement 
CONDcreate(Expression test, Linked_List then, Linked_List otherwise)
{
	Statement s;
	s = STMTcreate(STMT_COND);
	s->u.cond = COND_new();
	s->u.cond->test = test;
	s->u.cond->code = then;
	s->u.cond->otherwise = otherwise;
	return(s);
}

/*
** Procedure:	PCALLcreate
** Parameters:	Procedure   procedure	- procedure called by statement
**		Linked_List parameters	- list of actual parameter Expressions
**		Error*      experrc	- buffer for error code
** Returns:	Procedure_Call		- the procedure call generated
** Description:	Create and return a procedure call statement.
*/

Statement 
PCALLcreate(Linked_List parameters)
{
	Statement s;
	s = STMTcreate(STMT_PCALL);
	s->u.proc = PCALL_new();
/*	s->u.proc->procedure = 0;  fill in later during resolution  */
	s->u.proc->parameters = parameters;
	return s;
}

/*
** Procedure:	LOOPcreate
** Parameters:	Linked_List controls	- list of Loop_Controls for the loop
**		Statement   body	- statement to be repeated
**		Error*      experrc	- buffer for error code
** Returns:	Loop			- the loop generated
** Description:	Create and return a loop statement.
*/

Statement 
LOOPcreate(Scope scope,Expression while_expr,Expression until_expr,Linked_List statements)
{
	Statement s = STMTcreate(STMT_LOOP);
	s->u.loop = LOOP_new();
	s->u.loop->scope = scope;
	s->u.loop->while_expr = while_expr;
	s->u.loop->until_expr = until_expr;
	s->u.loop->statements = statements;
	return(s);
}

Statement 
ALIAScreate(Scope scope,Variable variable,Linked_List statements)
{
	Statement s = STMTcreate(STMT_ALIAS);
	s->u.alias = ALIAS_new();
	s->u.alias->scope = scope;
	s->u.alias->variable = variable;
	s->u.alias->statements = statements;
	return(s);
}

/*
** Procedure:	INCR_CTLcreate
** Parameters:	Expression control	- controlling expression
**		Expression start	- initial value
**		Expression end		- terminal value
**		Expression increment	- value by which to increment
**		Error*     experrc		- buffer for error code
** Returns:	Increment_Control	- increment control created
** Description:	Create and return an increment control as specified.
*/

Scope 
INCR_CTLcreate(Symbol *control, Expression start,
	       Expression end, Expression increment)
{
	Scope s = SCOPEcreate_tiny(OBJ_INCREMENT);
	Expression e = EXPcreate_from_symbol(Type_Attribute,control);
	Variable v = VARcreate(e,Type_Number);
	DICTdefine(s->symbol_table,control->name,
		(Generic)v,control,OBJ_VARIABLE);
	s->u.incr = INCR_new();
	s->u.incr->init = start;
	s->u.incr->end = end;
	s->u.incr->increment = increment;
	return s;
}

/*
** Procedure:	RETcreate
** Parameters:	Expression expression	- value to return
**		Error*     experrc		- buffer for error code
** Returns:	Return_Statement	- the return statement created
** Description:	Create and return a return statement.
*/

Statement 
RETcreate(Expression expression)
{
	Statement s = STMTcreate(STMT_RETURN);
	s->u.ret = RET_new();
	s->u.ret->value = expression;
	return s;
}
