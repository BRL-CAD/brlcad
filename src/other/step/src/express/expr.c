static char rcsid[] = "$Id: expr.c,v 1.6 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Expression
** Description:	This module implements the Expression abstraction.  Several
**	types of expressions are supported: identifiers, literals,
**	operations (arithmetic, logical, array indexing, etc.), and
**	function calls.  Every expression is marked with a type.
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
 * $Log: expr.c,v $
 * Revision 1.6  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.5  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.4  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.3  1994/06/02  14:56:06  libes
 * made plus-like ops check both args
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.9  1993/02/22  21:46:00  libes
 * ANSI compat fixes
 *
 * Revision 1.8  1993/02/16  03:21:31  libes
 * fixed numerous confusions of type with return type
 * fixed implicit loop variable type declarations
 * improved errors
 *
 * Revision 1.7  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.6  1992/09/16  18:20:40  libes
 * made expression resolution routines search through references
 *
 * Revision 1.5  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.3  1992/05/31  23:32:26  libes
 * implemented ALIAS resolution
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 * Revision 4.1  90/09/13  15:12:48  clark
 * BPR 2.1 alpha
 * 
 */

#define EXPRESSION_C
#include "express/expr.h"
#include "express/resolve.h"

void EXPop_init();
static Error ERROR_internal_unrecognized_op_in_EXPresolve;
		/* following two could probably be combined */
static Error ERROR_attribute_reference_on_aggregate;
static Error ERROR_attribute_ref_from_nonentity;
static Error ERROR_indexing_illegal;
static Error ERROR_enum_no_such_item;
static Error ERROR_group_ref_no_such_entity;
static Error ERROR_group_ref_unexpected_type;

Expression 
EXPcreate(Type type)
{
	Expression e;
	e = EXP_new();
	SYMBOLset(e);
	e->type = type;
	e->return_type = Type_Unknown;
	return(e);
}

/* use this when the return_type is the same as the type */
/* For example, for constant integers */
Expression 
EXPcreate_simple(Type type)
{
	Expression e;
	e = EXP_new();
	SYMBOLset(e);
	e->type = e->return_type = type;
	return(e);
}

Expression 
EXPcreate_from_symbol(Type type, Symbol *symbol)
{
	Expression e;
	e = EXP_new();
	e->type = type;
	e->return_type = Type_Unknown;
	e->symbol = *symbol;
	return e;
}

Symbol *
EXP_get_symbol(Generic e)
{
	return(&((Expression )e)->symbol);
}

/*
** Procedure:	EXPinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Expression module.
*/

void
EXPinitialize(void)
{
	MEMinitialize(&EXP_fl,sizeof(struct Expression_),500,200);
	MEMinitialize(&OP_fl,sizeof(struct Op_Subexpression),500,100);
	MEMinitialize(&QUERY_fl,sizeof(struct Query_),50,10);
	MEMinitialize(&QUAL_ATTR_fl,sizeof(struct Query_),20,10);
	OBJcreate(OBJ_EXPRESSION,EXP_get_symbol,"expression",OBJ_EXPRESSION_BITS);
	OBJcreate(OBJ_AMBIG_ENUM,EXP_get_symbol,"ambiguous enumeration",OBJ_UNUSED_BITS);

#ifdef does_not_appear_to_be_necessary_or_even_make_sense
	LITERAL_EMPTY_SET = EXPcreate_simple(Type_Set);
	LITERAL_EMPTY_SET->u.list = LISTcreate();
	resolved_all(LITERAL_EMPTY_SET);
#endif

/* E and PI might come out of math.h */

	LITERAL_E = EXPcreate_simple(Type_Real);
#ifndef M_E
#define	M_E		2.7182818284590452354
#endif
	LITERAL_E->u.real = M_E;
	resolved_all(LITERAL_E);

	LITERAL_PI = EXPcreate_simple(Type_Real);
#ifndef M_PI
#define	M_PI	3.14159265358979323846
#endif
	LITERAL_PI->u.real = M_PI;
	resolved_all(LITERAL_PI);

	LITERAL_INFINITY = EXPcreate_simple(Type_Integer);
	LITERAL_INFINITY->u.integer = MAXINT;
	resolved_all(LITERAL_INFINITY);

	LITERAL_ZERO = EXPcreate_simple(Type_Integer);
	LITERAL_ZERO->u.integer = 0;
	resolved_all(LITERAL_ZERO);

	LITERAL_ONE = EXPcreate_simple(Type_Integer);
	LITERAL_ONE->u.integer = 1;
	resolved_all(LITERAL_ONE);

	ERROR_integer_expression_expected =	ERRORcreate(
"Integer expression expected", SEVERITY_WARNING);

	ERROR_internal_unrecognized_op_in_EXPresolve = ERRORcreate(
"Opcode unrecognized while trying to resolve expression", SEVERITY_ERROR);

	ERROR_attribute_reference_on_aggregate = ERRORcreate(
"Attribute %s cannot be referenced from an aggregate",SEVERITY_ERROR);

	ERROR_attribute_ref_from_nonentity = ERRORcreate(
"Attribute %s cannot be referenced from a non-entity",SEVERITY_ERROR);

	ERROR_indexing_illegal = ERRORcreate(
"Indexing is only permitted on aggregates",SEVERITY_ERROR);

	ERROR_enum_no_such_item = ERRORcreate(
"Enumeration type %s does not contain item %s",SEVERITY_ERROR);

	ERROR_group_ref_no_such_entity = ERRORcreate(
"Group reference failed to find entity %s",SEVERITY_ERROR);

	ERROR_group_ref_unexpected_type = ERRORcreate(
"Group reference of unusual expression %s",SEVERITY_ERROR);

	ERROR_implicit_downcast = ERRORcreate(
"Implicit downcast to %s.", SEVERITY_WARNING);

	ERROR_ambig_implicit_downcast = ERRORcreate(
"Possibly ambiguous implicit downcast (%s?).", SEVERITY_WARNING);

	ERRORcreate_warning("downcast", ERROR_implicit_downcast);
	ERRORcreate_warning("downcast", ERROR_ambig_implicit_downcast);

	EXPop_init();
}

/* search id is a parameter to avoid colliding with ENTITYfind... */
/* there will be no ambiguities, since we're looking at (and marking) */
/* only types, and it's marking only entities */
static int
EXP_resolve_op_dot_fuzzy(Type selection, Symbol ref, Variable* v, char* dt,
			 struct Symbol_ **where, int s_id)
{
    Variable tmp;
    int options = 0;
    struct Symbol_ *w = NULL;

    if (selection->search_id == s_id) return 0;

    switch (selection->u.type->body->type) {
    case entity_:
	tmp = ENTITYfind_inherited_attribute(selection->u.type->body->entity,
					     ref.name, &w);
	if (tmp) {
	    if (w != NULL) *where = w;
	    *v = tmp;
	    *dt = DICT_type;
	    return 1;
	} else
	    return 0;
    case select_:
	selection->search_id = s_id;
	LISTdo(selection->u.type->body->list, t, Type)
	    if (EXP_resolve_op_dot_fuzzy(t, ref, v, dt, &w, s_id)) {
		if (w != NULL) *where = w;
		++options;
	    }
	LISTod;
	switch (options) {
	case 0:
	    return 0;
	case 1:
	    return 1;
	default:
	    /* found more than one, so ambiguous */
	    *v = VARIABLE_NULL;
	    return 1;
	}
    default:
	return 0;
    }
}

Type
EXPresolve_op_dot(Expression expr,Scope scope)
{
    Expression op1 = expr->e.op1;
    Expression op2 = expr->e.op2;
    Variable v;
    Expression item;
    Type op1type;

    /* stuff for dealing with select_ */
    int options = 0;
    Variable tmp;
    char dt;
    struct Symbol_ *where = NULL;

    /* op1 is entity expression, op2 is attribute */
    /* could be very impossible to determine except */
    /* at run-time, .... */
    EXPresolve(op1,scope,Type_Dont_Care);
    if (is_resolve_failed(op1)) {
	resolve_failed(expr);
	return(Type_Bad);
    }
    op1type = op1->return_type;

    switch (op1type->u.type->body->type) {
    case generic_:
    case runtime_:
	/* defer */
	return(Type_Runtime);
    case select_:
	__SCOPE_search_id++;
	/* don't think this actually actually catches anything on the */
	/* first go-round, but let's be consistent */
	op1type->search_id = __SCOPE_search_id;
	LISTdo(op1type->u.type->body->list, t, Type)
	    if (EXP_resolve_op_dot_fuzzy(t, op2->symbol, &v, &dt, &where,
					 __SCOPE_search_id)) {
		++options;
	    }
	LISTod;

	switch (options) {
	case 0:
	    /* no possible resolutions */
	    ERRORreport_with_symbol(ERROR_undefined_attribute,
				    &op2->symbol, op2->symbol.name);
	    resolve_failed(expr);
	    return(Type_Bad);
	case 1:
	    /* only one possible resolution */
	    if (dt != OBJ_VARIABLE) {
		printf("EXPresolved_op_dot: attribute not an attribute?\n");
		ERRORabort(0);
	    } else if (where) {
		ERRORreport_with_symbol(ERROR_implicit_downcast, &op2->symbol,
					where->name);
	    }

	    op2->u.variable = v;
	    op2->return_type = v->type;
	    resolved_all(expr);
	    return(v->type);
	default:
	    /* compile-time ambiguous */
	    if (where) {
		ERRORreport_with_symbol(ERROR_ambig_implicit_downcast,
					&op2->symbol, where->name);
	    }
	    return(Type_Runtime);
	}
    case attribute_:
	v = ENTITYresolve_attr_ref(op1->u.variable->type->u.type->body->entity, (struct Symbol_*)0, &op2->symbol);

	if (!v) {
/*	    reported by ENTITYresolve_attr_ref */
/*	    ERRORreport_with_symbol(ERROR_undefined_attribute,*/
/*				&expr->symbol,op2->symbol.name);*/
	    resolve_failed(expr);
	    return(Type_Bad);
	}
	if (DICT_type != OBJ_VARIABLE) {
	    printf("EXPresolved_op_dot: attribute not an attribute?\n");
	    ERRORabort(0);
	}

	op2->u.variable = v;
	op2->return_type = v->type;
	resolved_all(expr);
	return(v->type);
    case entity_:
    case op_:	/* (op1).op2 */
	v = ENTITYresolve_attr_ref(op1type->u.type->body->entity,
				   (struct Symbol_*)0, &op2->symbol);
	if (!v) {
/*	    reported by ENTITYresolve_attr_ref */
/*	    ERRORreport_with_symbol(ERROR_undefined_attribute,*/
/*				&expr->symbol,op2->symbol.name);*/
	    resolve_failed(expr);
	    return(Type_Bad);
	}
	if (DICT_type != OBJ_VARIABLE) {
	    printf("EXPresolved_op_dot: attribute not an attribute? - press ^C now to trap to debugger\n");
	    pause();
	}

	op2->u.variable = v;
	/* changed to set return_type */
	op2->return_type = op2->u.variable->type;
	resolved_all(expr);
	return(op2->return_type);
    case enumeration_:
	item = (Expression )DICTlookup(TYPEget_enum_tags(op1type),op2->symbol.name);
/*	item = (Expression )DICTlookup(TYPEget_enum_tags(op1->return_type),op2->symbol.name);*/
	if (!item) {
	    ERRORreport_with_symbol(ERROR_enum_no_such_item, &op2->symbol,
				    op1type->symbol.name, op2->symbol.name);
/*	    ERRORreport_with_symbol(ERROR_enum_no_such_item,&op2->symbol,op1->return_type->symbol.name,op2->symbol.name);*/
	    resolve_failed(expr);
	    return(Type_Bad);
	}

	op2->u.expression = item;
	op2->return_type = item->type;
	resolved_all(expr);
	return(item->type);
    case aggregate_:
    case array_:
    case bag_:
    case list_:
    case set_:
	ERRORreport_with_symbol(ERROR_attribute_reference_on_aggregate,
				&op2->symbol,op2->symbol.name);
	/*FALLTHRU*/
    case unknown_:	/* unable to resolved operand */
	/* presumably error has already been reported */
	resolve_failed(expr);
	return(Type_Bad);
    default:
	ERRORreport_with_symbol(ERROR_attribute_ref_from_nonentity,
				&op2->symbol,op2->symbol.name);
	resolve_failed(expr);
	return(Type_Bad);
    }
}

/* search id is a parameter to avoid colliding with ENTITYfind... */
/* there will be no ambiguities, since we're looking at (and marking) */
/* only types, and it's marking only entities */
static int
EXP_resolve_op_group_fuzzy(Type selection, Symbol ref, Entity* e,
			   int s_id)
{
    Entity tmp;
    int options = 0;

    if (selection->search_id == s_id) return 0;

    switch (selection->u.type->body->type) {
    case entity_:
	tmp = (Entity)ENTITYfind_inherited_entity(
	    selection->u.type->body->entity, ref.name, 1);
	if (tmp) {
	    *e = tmp;
	    return 1;
	}

	return 0;
    case select_:
	tmp = *e;
	selection->search_id = s_id;
	LISTdo(selection->u.type->body->list, t, Type)
	    if (EXP_resolve_op_group_fuzzy(t, ref, e, s_id)) {
		if (*e != tmp) {
		    tmp = *e;
		    ++options;
		}
	    }
	LISTod;

	switch (options) {
	case 0:
	    return 0;
	case 1:
	    return 1;
	default:
	    /* found more than one, so ambiguous */
	    *e = ENTITY_NULL;
	    return 1;
	}
    default:
	return 0;
    }
}

Type
EXPresolve_op_group(Expression expr,Scope scope)
{
	Expression op1 = expr->e.op1;
	Expression op2 = expr->e.op2;
	Entity ent_ref = ENTITY_NULL;
	Entity tmp = ENTITY_NULL;
	Type op1type;

	/* stuff for dealing with select_ */
	int options = 0;
	char dt;

	/* op1 is entity expression, op2 is entity */
	/* could be very impossible to determine except */
	/* at run-time, .... */
	EXPresolve(op1,scope,Type_Dont_Care);
	if (is_resolve_failed(op1)) {
	    resolve_failed(expr);
	    return(Type_Bad);
	}
	op1type = op1->return_type;

	switch (op1type->u.type->body->type) {
	case generic_:
	case runtime_:
	case op_:
	    /* All these cases are very painful to do right */
	    /* "Generic" and sometimes others require runtime evaluation */
	    op2->return_type = Type_Runtime;
	    return(Type_Runtime);
	case self_:
	case entity_:
	    /* Get entity denoted by "X\" */
	    tmp = ((op1type->u.type->body->type == self_)
		   ? scope
		   : op1type->u.type->body->entity);

	    /* Now get entity denoted by "X\Y" */
	    ent_ref =
		(Entity)ENTITYfind_inherited_entity(tmp, op2->symbol.name, 1);
	    if (!ent_ref) {
		ERRORreport_with_symbol(ERROR_group_ref_no_such_entity,
					&op2->symbol,op2->symbol.name);
		resolve_failed(expr);
		return(Type_Bad);
	    }

	    op2->u.entity = ent_ref;
	    op2->return_type = ent_ref->u.entity->type;
	    resolved_all(expr);
	    return(op2->return_type);
	case select_:
	    __SCOPE_search_id++;
	    /* don't think this actually actually catches anything on the */
	    /* first go-round, but let's be consistent */
	    op1type->search_id = __SCOPE_search_id;
	    LISTdo(op1type->u.type->body->list, t, Type)
		if (EXP_resolve_op_group_fuzzy(t, op2->symbol, &ent_ref,
					       __SCOPE_search_id)) {
		    if (ent_ref != tmp) {
			tmp = ent_ref;
			++options;
		    }
		}
	    LISTod;

	    switch (options) {
	    case 0:
		/* no possible resolutions */
		ERRORreport_with_symbol(ERROR_group_ref_no_such_entity,
					&op2->symbol, op2->symbol.name);
		resolve_failed(expr);
		return(Type_Bad);
	    case 1:
		/* only one possible resolution */
		op2->u.entity = ent_ref;
		op2->return_type = ent_ref->u.entity->type;
		resolved_all(expr);
		return(op2->return_type);
	    default:
		/* compile-time ambiguous */
/*		ERRORreport_with_symbol(ERROR_ambiguous_group,*/
/*					&op2->symbol, op2->symbol.name);*/
		return(Type_Runtime);
	    }
	case unknown_:	/* unable to resolve operand */
	    /* presumably error has already been reported */
	    resolve_failed(expr);
	    return(Type_Bad);
	case aggregate_:
	case array_:
	case bag_:
	case list_:
	case set_:
	default:
	    ERRORreport_with_symbol(ERROR_group_ref_unexpected_type,
				    &op1->symbol);
	    return(Type_Bad);
	}
}

Type
EXPresolve_op_relational(Expression e, Scope s)
{
	Type t = 0;
	int failed = 0;
	Type op1type;

	/* Prevent op1 from complaining if it fails */

	EXPresolve(e->e.op1,s,Type_Unknown);
	failed = is_resolve_failed(e->e.op1);
	op1type = e->e.op1->return_type;

	/* now, either op1 was resolved in which case, we use its return type */
	/* for typechecking, OR, it wasn't resolved in which case we resolve */
	/* op2 in such a way that it complains if it fails to resolved */

	if (op1type == Type_Unknown) t = Type_Dont_Care;
	else t = op1type;

	EXPresolve(e->e.op2,s,t);
	if (is_resolve_failed(e->e.op2)) failed = 1;

	/* If op1 wasn't successfully resolved, retry it now with new information */

	if ((failed == 0) && !is_resolved(e->e.op1)) {
		EXPresolve(e->e.op1,s,e->e.op2->return_type);
		if (is_resolve_failed(e->e.op1)) failed = 1;
	}

	if (failed) resolve_failed(e);
	else resolved_all(e);
	return(Type_Logical);
}	

void
EXPresolve_op_default(Expression e, Scope s)
{
	int failed = 0;

	switch (OPget_number_of_operands(e->e.op_code)) {
	case 3: EXPresolve(e->e.op3,s,Type_Dont_Care);
		failed = is_resolve_failed(e->e.op3);
	case 2: EXPresolve(e->e.op2,s,Type_Dont_Care);
		failed |= is_resolve_failed(e->e.op2);
	}
	EXPresolve(e->e.op1, s,Type_Dont_Care);
	if (failed || is_resolve_failed(e->e.op1)) resolve_failed(e);
	else resolved_all(e);
}

/*ARGSUSED*/
Type
EXPresolve_op_unknown(Expression e, Scope s)
{
	ERRORreport(ERROR_internal_unrecognized_op_in_EXPresolve);
	return Type_Bad;
}

typedef Type Resolve_expr_func PROTO((Expression ,Scope));

Type
EXPresolve_op_logical(Expression e,Scope s)
{
	EXPresolve_op_default(e,s);
	return(Type_Logical);
}

Type
EXPresolve_op_array_like(Expression e, Scope s)
{
	Type op1type;

	EXPresolve_op_default(e,s);
	op1type = e->e.op1->return_type;

	if (TYPEis_aggregate(op1type)) {
		return(op1type->u.type->body->base);
	} else if (TYPEis_string(op1type)) {
		return(op1type);
	} else if (op1type == Type_Runtime) {
		return(Type_Runtime);
	} else {
		ERRORreport_with_symbol(ERROR_indexing_illegal,&e->symbol);
		return(Type_Unknown);
	}
}

Type
EXPresolve_op_entity_constructor(Expression e, Scope s)
{
	EXPresolve_op_default(e,s);
	/* perhaps should return Type_Runtime? */
	return Type_Entity;
}

Type
EXPresolve_op_int_div_like(Expression e, Scope s)
{
	EXPresolve_op_default(e,s);
	return Type_Integer;
}

Type
EXPresolve_op_plus_like(Expression e, Scope s)
{
	/* i.e., Integer or Real */
	EXPresolve_op_default(e,s);
	if (is_resolve_failed(e)) {
		resolve_failed(e);
		return(Type_Unknown);
	}

	/* could produce better results with a lot of pain but the EXPRESS */
	/* spec is a little confused so what's the point.  For example */
	/* it says bag+set=bag */
	/*     and set+bag=set */
	/*     and set+list=set */
	/*     and list+set=? */

	/* crude but sufficient */
	if ((TYPEis_aggregate(e->e.op1->return_type)) ||
	    (TYPEis_aggregate(e->e.op2->return_type))) {
		return Type_Aggregate;
	}

	/* crude but sufficient */
	if ((e->e.op1->return_type->u.type->body->type == real_) ||
	    (e->e.op2->return_type->u.type->body->type == real_))
	    	return(Type_Real);
	return Type_Integer;
}

Type
EXPresolve_op_unary_minus(Expression e, Scope s)
{
	EXPresolve_op_default(e,s);
	return e->e.op1->return_type;
}

/*
resolve_func:	resolves an expression of this type
type_func:	returns final type of expression of this type
		avoids resolution if possible
*/
void
EXPop_create(int token_number,char *string,Resolve_expr_func *resolve_func) {
	EXPop_table[token_number].token = string;
	EXPop_table[token_number].resolve = resolve_func;
}

void EXPop_init() {
	EXPop_create(OP_AND,"AND",		EXPresolve_op_logical);
	EXPop_create(OP_ANDOR,"ANDOR",		EXPresolve_op_logical);
	EXPop_create(OP_ARRAY_ELEMENT,"[array element]",EXPresolve_op_array_like);
	EXPop_create(OP_CONCAT,"||",		EXPresolve_op_entity_constructor);
	EXPop_create(OP_DIV,"/ (INTEGER)",	EXPresolve_op_int_div_like);
	EXPop_create(OP_DOT,".",		EXPresolve_op_dot);
	EXPop_create(OP_EQUAL,"=",		EXPresolve_op_relational);
	EXPop_create(OP_EXP,"**",		EXPresolve_op_plus_like);
	EXPop_create(OP_GREATER_EQUAL,">=",	EXPresolve_op_relational);
	EXPop_create(OP_GREATER_THAN,">",	EXPresolve_op_relational);
	EXPop_create(OP_GROUP,"\\",		EXPresolve_op_group);
	EXPop_create(OP_IN,"IN",		EXPresolve_op_relational);
	EXPop_create(OP_INST_EQUAL,":=:",	EXPresolve_op_relational);
	EXPop_create(OP_INST_NOT_EQUAL,":<>:",	EXPresolve_op_relational);
	EXPop_create(OP_LESS_EQUAL,"<=",	EXPresolve_op_relational);
	EXPop_create(OP_LESS_THAN,"<",		EXPresolve_op_relational);
	EXPop_create(OP_LIKE,"LIKE",		EXPresolve_op_relational);
	EXPop_create(OP_MINUS,"- (MINUS)",	EXPresolve_op_plus_like);
	EXPop_create(OP_MOD,"MOD",		EXPresolve_op_int_div_like);
	EXPop_create(OP_NEGATE,"- (NEGATE)",	EXPresolve_op_unary_minus);
	EXPop_create(OP_NOT,"NOT",		EXPresolve_op_logical);
	EXPop_create(OP_NOT_EQUAL,"<>",		EXPresolve_op_relational);
	EXPop_create(OP_OR,"OR",		EXPresolve_op_logical);
	EXPop_create(OP_PLUS,"+",		EXPresolve_op_plus_like);
	EXPop_create(OP_REAL_DIV,"/ (REAL)",	EXPresolve_op_plus_like);
	EXPop_create(OP_SUBCOMPONENT,"[:]",	EXPresolve_op_array_like);
	EXPop_create(OP_TIMES,"*",		EXPresolve_op_plus_like);
	EXPop_create(OP_XOR,"XOR",		EXPresolve_op_logical);
	EXPop_create(OP_UNKNOWN,"UNKNOWN OP",	EXPresolve_op_unknown);
}

#if 0

/*
** Procedure:	EXPput_type
** Parameters:	Expression expression	- expression to modify
**		Type       type		- the new type for the expression
** Returns:	void
** Description:	Set the type of an expression.
**
** Notes:	This call should actually be unnecessary: the type of
**	an expression should be uniquely determined by its definition.
**	While this is currently true in the case of literals, there are
**	no rules in place for deriving the type from, for example, the
**	return type of a function or an operator together with its
**	operands.
*/

void
EXPput_type(Expression expression, Type type)
{
    Type	data;
    Error	experrc;

    data = (Type)OBJget_data(expression, Class_Expression, &experrc);
    OBJfree(*data, &experrc);
    *data = OBJreference(type);
}

/*
** Procedure:	EXPget_type
** Parameters:	Expression expression	- expression to examine
** Returns:	Type			- the type of the expression
** Description:	Retrieve the type of an expression.
*/

Type
EXPget_type(Expression expression)
{
    Type	data;
    Error	experrc;

    data = (Type)OBJget_data(expression, Class_Expression, &experrc);
    return OBJreference(*data);
}


/*
** Procedure:	EXPresolve_qualification
** Parameters:	Expression expression	- qualified identifier to resolve
**		Scope      scope	- scope in which to resolve
**		Error*     experrc		- buffer for error code
** Returns:	Symbol			- the symbol referenced by the expression
** Description:	Retrieves the symbol definition referenced by a (possibly
**	qualified) identifier.
*/

Symbol
EXPresolve_qualification(Expression expression, Scope scope, Error* experrc)
{
    String	name;

    if (expression == EXPRESSION_NULL)
	return SYMBOL_NULL;
    if (OBJis_kind_of(expression, Class_Identifier)) {
	name = SYMBOLget_name(IDENTget_identifier(expression));
	return SCOPElookup(scope, name, true, experrc);
    } else if (OBJis_kind_of(expression, Class_Binary_Expression) &&
	       (BIN_EXPget_operator(expression) == OP_DOT)) {
	scope =
	    (Scope)EXPresolve_qualification(BIN_EXPget_first_operand(expression),
					    scope, experrc);
	if (*experrc != ERROR_none)
	    return SYMBOL_NULL;
	return EXPresolve_qualification(BIN_EXPget_second_operand(expression),
					scope, experrc);
    } else {
	*experrc = ERROR_bad_qualification;
	return SYMBOL_NULL;
    }
}

#endif

/*
** Procedure:	TERN_EXPcreate
** Parameters:	Op_Code	   op		- operation
**		Expression operand1	- first operand
**		Expression operand2	- second operand
**		Expression operand3	- third operand
**		Error*     experrc		- buffer for error code
** Returns:	Ternary_Expression	- the expression created
** Description:	Create a ternary operation Expression.
*/

Expression 
TERN_EXPcreate(Op_Code op, Expression operand1, Expression operand2, Expression operand3)
{
	Expression e = EXPcreate(Type_Expression);

	e->e.op_code = op;
	e->e.op1 = operand1;
	e->e.op2 = operand2;
	e->e.op3 = operand3;
	return e;
}
#if 0

/*
** Procedure:	TERN_EXPget_second/third_operand
** Parameters:	Ternary_Expression expression	- expression to examine
** Returns:	Expression			- the second/third operand
** Description:	Retrieve the second/third operand from a binary expression.
*/

Expression
TERN_EXPget_second_operand(Ternary_Expression expression)
{
    struct Ternary_Expression*	data;
    Error	experrc;

    data = (struct Ternary_Expression )OBJget_data(expression, Class_Binary_Expression, &experrc);
    return OBJreference(data->op2);
}

Expression
TERN_EXPget_third_operand(Ternary_Expression expression)
{
    struct Ternary_Expression*	data;
    Error	experrc;

    data = (struct Ternary_Expression )OBJget_data(expression, Class_Binary_Expression, &experrc);
    return OBJreference(data->op3);
}

#endif /*0*/

/*
** Procedure:	BIN_EXPcreate
** Parameters:	Op_Code	   op		- operation
**		Expression operand1	- first operand
**		Expression operand2	- second operand
**		Error*     experrc		- buffer for error code
** Returns:	Binary_Expression	- the expression created
** Description:	Create a binary operation Expression.
*/

Expression 
BIN_EXPcreate(Op_Code op, Expression operand1, Expression operand2)
{
	Expression e = EXPcreate(Type_Expression);

	e->e.op_code = op;
	e->e.op1 = operand1;
	e->e.op2 = operand2;
	return e;
}

#if 0

/*
** Procedure:	BIN_EXPget_second_operand
** Parameters:	Binary_Expression expression	- expression to examine
** Returns:	Expression			- the second operand
** Description:	Retrieve the second operand from a binary expression.
*/

Expression
BIN_EXPget_second_operand(Binary_Expression expression)
{
    Expression*	data;
    Error	experrc;

    data = (Expression*)OBJget_data(expression, Class_Binary_Expression, &experrc);
    return OBJreference(*data);
}

#endif /*0*/
/*
** Procedure:	UN_EXPcreate
** Parameters:	Op_Code	   op		- operation
**		Expression operand	- operand
**		Error*     experrc		- buffer for error code
** Returns:	Unary_Expression	- the expression created
** Description:	Create a unary operation Expression.
*/

Expression 
UN_EXPcreate(Op_Code op, Expression operand)
{
	Expression e = EXPcreate(Type_Expression);

	e->e.op_code = op;
	e->e.op1 = operand;
	return e;
}

#if 0

/*
** Procedure:	ONEOFcreate
** Parameters:	Linked_List selections	- list of selections for oneof()
**		Error*      experrc	- buffer for error code
** Returns:	One_Of_Expression	- the oneof expression created
** Description:	Create a oneof() Expression.
*/

One_Of_Expression
ONEOFcreate(Linked_List selections, Error* experrc)
{
    One_Of_Expression	result;
    Linked_List	data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_One_Of_Expression, experrc);
    data = (Linked_List)OBJget_data(result, Class_One_Of_Expression, experrc);
    *data = OBJreference(selections);
    return result;
}

/*
** Procedure:	ONEOFput_selections
** Parameters:	One_Of_Expression expression	- expression to modify
**		Linked_List       selections	- list of selection Expressions
** Returns:	void
** Description:	Set the selections for a oneof() expression.
*/

void
ONEOFput_selections(One_Of_Expression expression, Linked_List selections)
{
    Linked_List	data;
    Error		experrc;

    data = (Linked_List)OBJget_data(expression, Class_One_Of_Expression, &experrc);
    OBJfree(*data, &experrc);
    *data = OBJreference(selections);
}

/*
** Procedure:	ONEOFget_selections
** Parameters:	One_Of_Expression expression	- expression to modify
** Returns:	Linked_List			- list of selection Expressions
** Description:	Retrieve the selections from a oneof() expression.
*/

Linked_List
ONEOFget_selections(One_Of_Expression expression)
{
    Linked_List	data;
    Error		experrc;

    data = (Linked_List)OBJget_data(expression, Class_One_Of_Expression, &experrc);
    return *data;
}

/*
** Procedure:	FCALLcreate
** Parameters:	Function    function	- function called by expression
**		Linked_List parameters	- parameters to function call
**		Error*      experrc	- buffer for error code
** Returns:	Function_Call		- the function call created
** Description:	Create a function call Expression.
*/

Function_Call
FCALLcreate(Function function, Linked_List parameters, Error* experrc)
{
    Function_Call	result;
    Algorithm*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Function_Call, experrc);
    data = (Algorithm*)OBJget_data(result, Class_Function_Call, experrc);
    *data = OBJreference(function);
    ONEOFput_selections(result, parameters);
    return result;
}

/*
** Procedure:	FCALLput_algorithm
** Parameters:	Function_Call expression - expression to modify
**		Function      function	 - function called by expression
** Returns:	void
** Description:	Set the algorithm for a function call expression.
*/

void
FCALLput_algorithm(Function_Call expression, Function function)
{
    Algorithm*	data;
    Error	experrc;

    data = (Algorithm*)OBJget_data(expression, Class_Function_Call, &experrc);
    if (*data == ALGORITHM_NULL)
	*data = OBJreference(function);
    else
	OBJbecome(*data, function, &experrc);
}

/*
** Procedure:	FCALLput_parameters
** Parameters:	Function_Call expression - expression to modify
**		Linked_List   parameters - list of actual parameter Expressions.
** Returns:	void
** Description:	Set the actual parameters to a function call expression.
**
** Notes:	The actual parameter list is not verified against the
**		formal parameters list of the called algorithm.
*/

/* this function is implemented as a macro in expression.h */

/*
** Procedure:	FCALLget_algorithm
** Parameters:	Function_Call expression	- function call to examine
** Returns:	Function			- the algorithm called in the
**						  expression
** Description:	Retrieve the algorithm called by a function call expression.
*/

Function
FCALLget_algorithm(Function_Call expression)
{
    Algorithm*	data;
    Error	experrc;

    data = (Algorithm*)OBJget_data(expression, Class_Function_Call, &experrc);
    return OBJreference(*data);
}

/*
** Procedure:	FCALLget_parameters
** Parameters:	Function_Call  expression	- expression to examine
** Returns:	Linked_List of Expression	- list of actual parameters
** Description:	Retrieve the actual parameters from a function call expression.
*/

/* this function is defined as a macro in expression.h */

/*
** Procedure:	IDENTcreate
** Parameters:	Symbol ident	- identifier referenced by expression
**		Error* experrc	- buffer for error code
** Returns:	Identifier	- the identifier expression created
** Description:	Create a simple identifier Expression.
*/

Identifier
IDENTcreate(Symbol ident, Error* experrc)
{
    Identifier	result;
    Variable	data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Identifier, experrc);
    data = (Variable)OBJget_data(result, Class_Identifier, experrc);
    *data = OBJreference(ident);
    return result;
}

/*
** Procedure:	IDENTput_identifier
** Parameters:	Identifier expression	- expression to modify
**		Symbol     identifier	- the name of the identifier
** Returns:	void
** Description:	Set the name of an identifier expression.
*/

void
IDENTput_identifier(Identifier expression, Symbol identifier)
{
    Variable	data;
    Error	experrc;

    data = (Variable)OBJget_data(expression, Class_Identifier, &experrc);
    OBJfree(*data, &experrc);
    *data = OBJreference(identifier);
}

/*
** Procedure:	IDENTget_identifier
** Parameters:	Identifier expression	- expression to examine
** Returns:	Symbol			- the identifier represented by
**					  the expression
** Description:	Retrieve the identifier of an identifier expression.
*/

Symbol
IDENTget_identifier(Identifier expression)
{
    Variable	data;
    Error	experrc;

    data = (Variable)OBJget_data(expression, Class_Identifier, &experrc);
    return OBJreference(*data);
}

/*
** Procedure:	AGGR_LITcreate
** Parameters:	Type        type	- type of aggregate literal
**		Linked_List value	- value of aggregate literal
**		Error*      experrc	- buffer for error code
** Returns:	Aggregate_Literal	- the literal created
** Description:	Create an aggregate literal Expression.
*/

Aggregate_Literal
AGGR_LITcreate(Type type, Linked_List value, Error* experrc)
{
    Aggregate_Literal	result;
    Linked_List	data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Aggregate_Literal, experrc);
    EXPput_type(result, OBJreference(type));
    data = (Linked_List)OBJget_data(result, Class_Aggregate_Literal, experrc);
    *data = OBJreference(value);
    return result;
}

/*
** Procedure:	INT_LITcreate
** Parameters:	Integer value	- value of integer literal
**		Error*  experrc	- buffer for error code
** Returns:	Integer_Literal	- the literal created
** Description:	Create an integer literal Expression.
*/

Integer_Literal
INT_LITcreate(Integer value, Error* experrc)
{
    Integer_Literal	result;
    Integer*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Integer_Literal, experrc);
    EXPput_type(result, OBJreference(TYPE_INTEGER));
    data = (Integer*)OBJget_data(result, Class_Integer_Literal, experrc);
    *data = value;
    return result;
}

/*
** Procedure:	LOG_LITcreate
** Parameters:	Logical value	- value of logical literal
**		Error*  experrc	- buffer for error code
** Returns:	Logical_Literal	- the literal created
** Description:	Create a logical literal Expression.
*/

Logical_Literal
LOG_LITcreate(Logical value, Error* experrc)
{
    Logical_Literal	result;
    Logical*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Logical_Literal, experrc);
    EXPput_type(result, OBJreference(TYPE_LOGICAL));
    data = (Logical*)OBJget_data(result, Class_Logical_Literal, experrc);
    *data = value;
    return result;
}

/*
** Procedure:	REAL_LITcreate
** Parameters:	Real    value	- value of real literal
**		Error*  experrc	- buffer for error code
** Returns:	Real_Literal	- the literal created
** Description:	Create a real literal Expression.
*/

Real_Literal
REAL_LITcreate(Real value, Error* experrc)
{
    Real_Literal	result;
    Real*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Real_Literal, experrc);
    EXPput_type(result, OBJreference(TYPE_REAL));
    data = (Real*)OBJget_data(result, Class_Real_Literal, experrc);
    *data = value;
    return result;
}

/*
** Procedure:	STR_LITcreate
** Parameters:	String value	- value of string literal
**		Error* experrc	- buffer for error code
** Returns:	String_Literal	- the literal created
** Description:	Create a string literal Expression.
*/

String_Literal
STR_LITcreate(String value, Error* experrc)
{
    String_Literal	result;
    String*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_String_Literal, experrc);
    EXPput_type(result, OBJreference(TYPE_STRING));
    data = (String*)OBJget_data(result, Class_String_Literal, experrc);
    *data = STRINGcopy(value);
    return result;
}
/*
** Procedure:	BIN_LITcreate
** Parameters:	Binary value	- value of binary literal
**		Error* experrc	- buffer for error code
** Returns:	Binary_Literal	- the literal created
** Description:	Create a string literal Expression.
*/

Binary_Literal
BIN_LITcreate(Binary value, Error* experrc)
{
    Binary_Literal	result;
    Binary*		data;

    *experrc = ERROR_none;
    result = OBJcreate(Class_Binary_Literal, experrc);
    EXPput_type(result, OBJreference(TYPE_BINARY));
    data = (Binary*)OBJget_data(result, Class_Binary_Literal, experrc);
    *data = STRINGcopy(value);
    return result;
}

/*
** Procedure:	AGGR_LITget_value
** Parameters:	Aggregate_Literal literal	- literal to examine
**		Error*            experrc		- buffer for error code
** Returns:	Linked_List			- the literal's value
** Description:	Retrieve the value of an aggregate literal.
*/

Linked_List
AGGR_LITget_value(Aggregate_Literal literal, Error* experrc)
{
    Linked_List	data;

    data = (Linked_List)OBJget_data(literal, Class_Aggregate_Literal, experrc);
    return OBJcopy(*data, experrc);
}

/*
** Procedure:	INT_LITget_value
** Parameters:	Integer_Literal literal	- literal to examine
**		Error*          experrc	- buffer for error code
** Returns:	Integer			- the literal's value
** Description:	Retrieve the value of an integer literal.
*/

Integer
INT_LITget_value(Integer_Literal literal, Error* experrc)
{
    Integer*	data;

    data = (Integer*)OBJget_data(literal, Class_Integer_Literal, experrc);
    return *data;
}

/*
** Procedure:	LOG_LITget_value
** Parameters:	Logical_Literal literal	- literal to examine
**		Error*          experrc	- buffer for error code
** Returns:	Logical			- the literal's value
** Description:	Retrieve the value of a logical literal.
*/

Logical
LOG_LITget_value(Logical_Literal literal, Error* experrc)
{
    Logical*	data;

    data = (Logical*)OBJget_data(literal, Class_Logical_Literal, experrc);
    return *data;
}

/*
** Procedure:	REAL_LITget_value
** Parameters:	Real_Literal literal	- literal to examine
**		Error*       experrc	- buffer for error code
** Returns:	Real			- the literal's value
** Description:	Retrieve the value of a real literal.
*/

Real
REAL_LITget_value(Real_Literal literal, Error* experrc)
{
    Real*	data;

    data = (Real*)OBJget_data(literal, Class_Real_Literal, experrc);
    return *data;
}

/*
** Procedure:	STR_LITget_value
** Parameters:	String_Literal literal	- literal to examine
**		Error*       experrc	- buffer for error code
** Returns:	String			- the literal's value
** Description:	Retrieve the value of a string literal.
*/

String
STR_LITget_value(String_Literal literal, Error* experrc)
{
    String*	data;

    data = (String*)OBJget_data(literal, Class_String_Literal, experrc);
    return STRINGcopy(*data);
}

/*
** Procedure:	BIN_LITget_value
** Parameters:	Binary_Literal literal	- literal to examine
**		Error*       experrc	- buffer for error code
** Returns:	Binary			- the literal's value
** Description:	Retrieve the value of a binary literal.
*/

Binary
BIN_LITget_value(Binary_Literal literal, Error* experrc)
{
    String*	data;

    data = (String*)OBJget_data(literal, Class_Binary_Literal, experrc);
    return STRINGcopy(*data);
}

#endif

/*
** Procedure:	QUERYcreate
** Parameters:	String     ident	- local identifier for source elements
**		Expression source	- source aggregate to query
**		Expression discriminant	- discriminating expression for query
**		Error*     experrc		- buffer for error code
** Returns:	Query			- the query expression created
** Description:	Create a query Expression.
*/

Expression 
QUERYcreate(Symbol *local, Expression aggregate)
{
	Expression e = EXPcreate_from_symbol(Type_Query,local);
	Scope s = SCOPEcreate_tiny(OBJ_QUERY);
	Expression e2 = EXPcreate_from_symbol(Type_Attribute,local);

	Variable v = VARcreate(e2,Type_Attribute);

	DICTdefine(s->symbol_table,local->name,(Generic)v,&e2->symbol,OBJ_VARIABLE);
	e->u.query = QUERY_new();
	e->u.query->scope = s;
	e->u.query->local = v;
	e->u.query->aggregate = aggregate;
	return e;
}

#if 0

/*
** Procedure:	QUERYget_variable
** Parameters:	Query expression	- query expression to examine
** Returns:	Variable		- the local variable for the query
** Description:	Retrieve the variable used locally within the query to
**	iterate over the contents of the source aggregate.
*/

Variable
QUERYget_variable(Query expression)
{
    struct Query*	data;
    Error		experrc;

    data = (struct Query*)OBJget_data(expression, Class_Query, &experrc);
    return OBJreference(data->identifier);
}

/*
** Procedure:	QUERYget_source
** Parameters:	Query expression	- query expression to examine
** Returns:	Expression		- the source set for the query
** Description:	Retrieve the aggregate examined by a query expression.
*/

Expression
QUERYget_source(Query expression)
{
    struct Query*	data;
    Error		experrc;

    data = (struct Query*)OBJget_data(expression, Class_Query, &experrc);
    return OBJreference(data->fromSet);
}

/*
** Procedure:	QUERYget_discriminant
** Parameters:	Query expression	- query expression to examine
** Returns:	Expression		- the discriminant for the query
** Description:	Retrieve a query's discriminant expression.
*/

Expression
QUERYget_discriminant(Query expression)
{
    struct Query*	data;
    Error		experrc;

    data = (struct Query*)OBJget_data(expression, Class_Query, &experrc);
    return OBJreference(data->discriminant);
}

/*
** Procedure:	OPget_number_of_operands
** Parameters:	Op_Code	operation	- the opcode to query
** Returns:	int			- number of operands required
** Description:	Determine the number of operands required by an operator.
*/

/* this function is inlined in expression.h */

#endif

/*
** Procedure:	EXPget_integer_value
** Parameters:	Expression  expression	- expression to evaluate
**		Error*      experrc	- buffer for error code
** Returns:	int			- value of expression
** Description:	Compute the value of an integer expression.
*/

int
EXPget_integer_value(Expression expression)
{
    experrc = ERROR_none;
    if (expression == EXPRESSION_NULL)
	return 0;
    if (expression->return_type->u.type->body->type == integer_) {
	return INT_LITget_value(expression);
    } else {
	experrc = ERROR_integer_expression_expected;
	return 0;
    }
}

char *
opcode_print(Op_Code o)
{
	switch (o) {
	case OP_AND: return("OP_AND");
	case OP_ANDOR: return("OP_ANDOR");
	case OP_ARRAY_ELEMENT: return("OP_ARRAY_ELEMENT");
	case OP_CONCAT: return("OP_CONCAT");
	case OP_DIV: return("OP_DIV");
	case OP_DOT: return("OP_DOT");
	case OP_EQUAL: return("OP_EQUAL");
	case OP_EXP: return("OP_EXP");
	case OP_GREATER_EQUAL: return("OP_GREATER_EQUAL");
	case OP_GREATER_THAN: return("OP_GREATER_THAN");
	case OP_GROUP: return("OP_GROUP");
	case OP_IN: return("OP_IN");
	case OP_INST_EQUAL: return("OP_INST_EQUAL");
	case OP_INST_NOT_EQUAL: return("OP_INST_NOT_EQUAL");
	case OP_LESS_EQUAL: return("OP_LESS_EQUAL");
	case OP_LESS_THAN: return("OP_LESS_THAN");
	case OP_LIKE: return("OP_LIKE");
	case OP_MINUS: return("OP_MINUS");
	case OP_MOD: return("OP_MOD");
	case OP_NEGATE: return("OP_NEGATE");
	case OP_NOT: return("OP_NOT");
	case OP_NOT_EQUAL: return("OP_NOT_EQUAL");
	case OP_OR: return("OP_OR");
	case OP_PLUS: return("OP_PLUS");
	case OP_REAL_DIV: return("OP_REAL_DIV");
	case OP_SUBCOMPONENT: return("OP_SUBCOMPONENT");
	case OP_TIMES: return("OP_TIMES");
	case OP_XOR: return("OP_XOR");
	case OP_UNKNOWN: return("OP_UNKNOWN");
	default: return("no such op");
	}
}
