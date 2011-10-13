static char rcsid[] = "$Id: alg.c,v 1.5 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Algorithm
** Description:	This module implements the Algorithm abstraction.  An
**	algorithm can be a procedure, a function, or a rule.  It consists
**	of a name, a return type (for functions and rules), a list of
**	formal parameters, and a list of statements (body).
** Constants:
**	ALGORITHM_NULL	- the null algorithm
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: alg.c,v $
 * Revision 1.5  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.4  1995/04/05  15:11:02  clark
 * CADDETC preval
 *
 * Revision 1.3  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/16  03:13:56  libes
 * add Where type
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 */

#include "express/alg.h"
#include "express/object.h"
#include "express/schema.h"

struct freelist_head ALG_fl;
struct freelist_head FUNC_fl;
struct freelist_head RULE_fl;
struct freelist_head PROC_fl;
struct freelist_head WHERE_fl;

Scope
ALGcreate(char type)
{
	Scope s = SCOPEcreate(type);

	switch(type) {
	case OBJ_PROCEDURE:
		s->u.proc = PROC_new();
		break;
	case OBJ_FUNCTION:
		s->u.func = FUNC_new();
		break;
	case OBJ_RULE:
		s->u.rule = RULE_new();
		break;
	}
	return s;
}

/*
** Procedure:	ALGinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Algorithm module.
*/

Symbol *
WHERE_get_symbol(Generic w)
{
	return(((Where)w)->label);
}

void
ALGinitialize(void)
{
	MEMinitialize(&FUNC_fl,sizeof(struct Function_),  100,50);
	MEMinitialize(&RULE_fl,sizeof(struct Rule_),	  100,50);
	MEMinitialize(&PROC_fl,sizeof(struct Procedure_), 100,50);
	MEMinitialize(&WHERE_fl,sizeof(struct Where_),    100,50);
	OBJcreate(OBJ_RULE,SCOPE_get_symbol,"rule",OBJ_UNUSED_BITS);
	OBJcreate(OBJ_PROCEDURE,SCOPE_get_symbol,"procedure",OBJ_PROCEDURE_BITS);
	OBJcreate(OBJ_FUNCTION,SCOPE_get_symbol,"function",OBJ_FUNCTION_BITS);
	OBJcreate(OBJ_WHERE,WHERE_get_symbol,"where",OBJ_WHERE_BITS);
}

void
ALGput_full_text(Scope s, int start, int end)
{
    switch (s->type) {
    case OBJ_FUNCTION:
	s->u.func->text.filename = s->symbol.filename;
	s->u.func->text.start = start;
	s->u.func->text.end = end;
	break;
    case OBJ_PROCEDURE:
	s->u.proc->text.filename = s->symbol.filename;
	s->u.proc->text.start = start;
	s->u.proc->text.end = end;
	break;
    case OBJ_RULE:
	s->u.rule->text.filename = s->symbol.filename;
	s->u.rule->text.start = start;
	s->u.rule->text.end = end;
	break;
    }
}
