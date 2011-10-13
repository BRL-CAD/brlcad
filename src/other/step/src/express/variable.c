static char rcsid[] = "$Id: variable.c,v 1.7 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Variable
** Description:	This module implements the Variable abstraction.  A
**	Variable consists of a name, a type, a reference class, and
**	some flags, e.g. 'optional', 'variable'.  It is used to represent
**	variable attributes, variables, and formal parameters.
** Constants:
**	VARIABLE_NULL	- the null variable
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: variable.c,v $
 * Revision 1.7  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.6  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.5  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 * Revision 1.5  1992/05/10  06:03:51  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.4  1992/02/17  14:34:01  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:06:10  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:50:45  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.6  1991/09/16  23:13:12  libes
 * added print functionsalgorithm.c
 *
 * Revision 4.5  1991/07/21  05:22:17  libes
 * added VARget and put_inverse, initialized private ->inverse
 *
 * Revision 4.4  1991/06/14  20:42:02  libes
 * Remove reference_class from variable, and added support for true name
 * of variable in another schema to which variable corresponds.
 *
 * Revision 4.3  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.2  91/01/08  18:56:07  pdesadmn
 * Initial - Beta checkin at SCRA
 * 
 * Revision 4.1  90/09/06  11:40:50  clark
 * Initial checkin at SCRA
 * 
 * Revision 4.1  90/09/06  11:40:50  clark
 * initial checkin at SCRA
 * 
 * Revision 4.1  90/09/06  11:40:50  clark
 * BPR 2.1 alpha
 * 
 */

#include <stdlib.h>
#include "express/variable.h"
#include "express/object.h"

struct freelist_head VAR_fl;

/*
** Procedure:	VAR_create/free/copy/equal
** Description:	These are the low-level defining functions for Class_Variable
*/

Symbol *
VAR_get_symbol(Generic v)
{
	return(&((Variable )v)->name->symbol);
}

/*
** Procedure:	VARinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Variable module.
*/

void
VARinitialize()
{
	MEMinitialize(&VAR_fl,sizeof(struct Variable_),100,50);
/*	OBJcreate(OBJ_VARIABLE,VAR_get_symbol,"variable",OBJ_UNUSED_BITS);*/
	OBJcreate(OBJ_VARIABLE,VAR_get_symbol,"variable",OBJ_VARIABLE_BITS);
}

/* returns simple name of variable */
/* for example, if var is named SELF\xxx.yyy, return yyy */
extern char *
VARget_simple_name(Variable v)
{
	char tmp;

	Expression e = VARget_name(v);

	while (TYPEis_expression(EXPget_type(e))) {
		switch (e->e.op_code) {
		case OP_DOT:
		case OP_GROUP:
			e = e->e.op2;
			break;
		default:
			fprintf(stderr,"unexpected op_code (%s) encountered in variable name expression\n",opcode_print(e->e.op_code));
			abort();
		}
	}
	return EXPget_name(e);
}
 
/*
** Procedure:	VARcreate
** Parameters:	String name	- name of variable to create
**		Type   type	- type of new variable
**		Error* experrc	- buffer for error code
** Returns:	Variable	- the Variable created
** Description:	Create and return a new variable.
**
** Notes:	The reference class of the variable is, by default,
**		dynamic.  Special flags associated with the variable
**		(e.g., optional) are initially false.
*/

Variable 
VARcreate(Expression name, Type type)
{
	Variable v = VAR_new();
	v->name = name;
	v->type = type;
	return v;
}

#if 0

/*
** Procedure:	VARput_type
** Parameters:	Variable var	- variable to examine
**		Type     type	- type for variable
** Returns:	void
** Description:	Set the type of a variable.
*/

void
VARput_type(Variable var, Type type)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    OBJfree(data->type, &experrc);
    data->type = OBJreference(type);
}

/*
** Procedure:	VARput_initializer
** Parameters:	Variable   var	- variable to modify
**		Expression init	- initializer
** Returns:	void
** Description:	Set the initializing expression for a variable.
**
** Notes:	When a variable used to represent an attribute has an
**	initializer, the represented attribute is a derived attribute.
*/

void
VARput_initializer(Variable var, Expression init)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    OBJfree(data->initializer, &experrc);
    data->initializer = OBJreference(init);
}

/*
** Procedure:	VARput_derived
** Parameters:	Variable var	- variable to modify
**		Boolean  val	- new value for derived flag
** Returns:	void
** Description:	Set the value of the 'derived' flag for a variable.
**
** Notes:	This is currently redundant information, as a derived
**	attribute can be identified by the fact that it has an
**	initializing expression.  This may not always be true, however.
*/

void
VARput_derived(Variable var, Boolean val)
{
    return;
}

/*
** Procedure:	VARput_optional
** Parameters:	Variable var	- variable to modify
**		Boolean  val	- new value for optional flag
** Returns:	void
** Description:	Set the value of the 'optional' flag for a variable.
*/

void
VARput_optional(Variable var, Boolean val)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    if (val)
	data->flags |= VAR_OPT_MASK;
    else
	data->flags &= ~VAR_OPT_MASK;
}

/*
** Procedure:	VARput_variable
** Parameters:	Variable var	- variable to modify
**		Boolean  val	- new value for variable flag
** Returns:	void
** Description:	Set the value of the 'variable' flag for a variable.
**
** Notes:	This flag is intended for use in the cases when a variable
**	is used to represent a formal parameter, which may be passed by
**	reference (a variable parameter, in Pascal terminology).
*/

void
VARput_variable(Variable var, Boolean val)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    if (val)
	data->flags |= VAR_VAR_MASK;
    else
	data->flags &= ~VAR_VAR_MASK;
}

/*
** Procedure:	VARput_reference
** Parameters:	Variable        var	- variable to modify
**		Expression ref	- the variable's reference
** Returns:	void
** Description:	Set the reference class of a variable.
*/

void
VARput_reference(Variable var, Expression ref)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    data->reference = ref;
}

/*
** Procedure:	VARput_inverse
** Parameters:	Variable        var	- variable to modify
**		Expression ref	- attr of entity related to be inverse attr
** Returns:	void
** Description:	Marks this as an inverse attribute and also says to which
		attribute in the remove entity this relates to.
*/

void
VARput_inverse(Variable var, Symbol attr)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    data->inverse = attr;
}

/*
** Procedure:	VARget_inverse
** Parameters:	Variable        var	- variable to modify
** Returns:	Symbol		attr	- returns what was put
** Description:	See VARput_inverse
*/

Symbol
VARget_inverse(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return data->inverse;
}

/*
** Procedure:	VARput_reference_class
** Parameters:	Variable        var	- variable to modify
**		Reference_Class ref	- the variable's reference class
** Returns:	void
** Description:	Set the reference class of a variable.
*/

void
VARput_reference_class(Variable var, Reference_Class ref)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    data->ref_class = ref;
}

/*
** Procedure:	VARput_offset
** Parameters:	Variable var	- variable to modify
**		int      offset	- offset to variable in local frame
** Returns:	void
** Description:	Set a variable's offset in its local frame.
*/

void
VARput_offset(Variable var, int offset)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    data->offset = offset;
}

/*
** Procedure:	VARget_type
** Parameters:	Variable var	- variable to examine
** Returns:	Type		- the type of the variable
** Description:	Retrieve the type of a variable.
*/

Type
VARget_type(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return OBJreference(data->type);
}

/*
** Procedure:	VARget_derived
** Parameters:	Variable var	- variable to examine
** Returns:	Boolean		- value of variable's derived flag
** Description:	Retrieve the value of a variable's 'derived' flag.
*/

/* this function is inlined in variable.h */

/*
** Procedure:	VARget_optional
** Parameters:	Variable var	- variable to examine
** Returns:	Boolean		- value of variable's optional flag
** Description:	Retrieve the value of a variable's 'optional' flag.
*/

Boolean
VARget_optional(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return (data->flags & VAR_OPT_MASK) != 0;
}

/*
** Procedure:	VARget_variable
** Parameters:	Variable var	- variable to examine
** Returns:	Boolean		- value of variable's variable flag
** Description:	Retrieve the value of a variable's 'variable' flag.
*/

Boolean
VARget_variable(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return (data->flags & VAR_VAR_MASK) != 0;
}

/*
** Procedure:	VARget_reference
** Parameters:	Variable var	- variable to examine
** Returns:	Expression	- the variable's reference
** Description:	Retrieve a variable's reference
*/

Expression
VARget_reference_class(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return data->reference;
}

/*
** Procedure:	VARget_reference_class
** Parameters:	Variable var	- variable to examine
** Returns:	Reference_Class	- the variable's reference class
** Description:	Retrieve a variable's reference class.
*/

Reference_Class
VARget_reference_class(Variable var)
{
    struct Variable*	data;
    Error		experrc;

    data = (struct Variable*)OBJget_data(var, Class_Variable, &experrc);
    return data->ref_class;
}

/*
** Procedure:	VARget_offset
** Parameters:	Variable var	- variable to examine
** Returns:	int		- offset to variable in local frame
** Description:	Retrieve the offset to a variable in it's local frame.
*/

/* now a macro in variable.h */

void
VARprint(Variable var)
{
	Error experrc;
	struct Var *data;

	print_force(var_print);

	data = OBJget_data(var,Class_Variable,&experrc);
	VAR_print(data);

	print_unforce(var_print);
}
#endif
