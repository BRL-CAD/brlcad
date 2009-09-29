static char rcsid[] = "$Id: type.c,v 1.12 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Type
** Description:	This module implements the type abstraction.  It is
**	rather unpleasant, since this abstraction is quite well suited
**	to an object-oriented environment with inheritance.
** Constants:
**	TYPE_AGGREGATE		- generic general aggregate
**	TYPE_BINARY		- binary type
**	TYPE_BOOLEAN		- boolean type
**	TYPE_GENERIC		- generic type
**	TYPE_INTEGER		- integer type with default precision
**	TYPE_LOGICAL		- logical type
**	TYPE_NULL		- the null type
**	TYPE_NUMBER		- number type
**	TYPE_REAL		- real type with default precision
**	TYPE_SET_OF_GENERIC	- type for unconstrained set of generic items
**	TYPE_STRING		- string type with default precision
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: type.c,v $
 * Revision 1.12  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.11  1995/03/09  18:44:45  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.10  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.9  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.8  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/01/19  22:16:09  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/09/16  18:23:45  libes
 * added some functions for easier access to base types
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
 * Revision 1.9  1992/05/14  10:14:06  libes
 * don't remember
 *
 * Revision 1.8  1992/05/10  06:03:18  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.7  1992/05/10  01:42:27  libes
 * does enums and repeat properly
 *
 * Revision 1.6  1992/05/05  19:52:11  libes
 * final alpha
 *
 * Revision 1.5  1992/02/19  15:48:46  libes
 * changed types to enums & flags
 *
 * Revision 1.4  1992/02/17  14:33:41  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:05:45  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:50:20  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.8  1991/11/14  07:37:50  libes
 * added TYPEget/put_original_type
 *
 * Revision 4.7  1991/09/16  23:13:12  libes
 * added print functions
 *
 * Revision 4.6  1991/06/14  20:48:16  libes
 * added binary type
 *
 * Revision 4.5  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.4  91/01/08  18:56:05  pdesadmn
 * Initial - Beta checkin at SCRA
 * 
 * Revision 4.3  90/09/14  16:02:28  clark
 * Initial checkin at SCRA
 * 
 * Revision 4.3  90/09/14  16:02:28  clark
 * initial checkin at SCRA
 * 
 * Revision 4.3  90/09/14  16:02:28  clark
 * Reintroduce ENT_TYPEget_entity
 * 
 * Revision 4.2  90/09/14  09:33:20  clark
 * Add Class_{Boolean,Generic,Logical,Number}_Type
 * Fix TYPE_equal
 * 
 * Revision 4.1  90/09/13  15:13:21  clark
 * BPR 2.1 alpha
 * 
 */

#define TYPE_C
#include "express/type.h"

static Error ERROR_undefined_tag;

Type
TYPEcreate_nostab(struct Symbol_ *symbol,Scope scope,char objtype)
{
	Type t = SCOPEcreate_nostab(OBJ_TYPE);
	TypeHead th = TYPEHEAD_new();

	t->u.type = th;
	t->symbol = *symbol;
	DICTdefine(scope->symbol_table,symbol->name,(Generic)t,&t->symbol,objtype);

	return t;
}

/* create a type but this is just a shell, either to be completed later */
/* such as enumerations (which have a symbol table added later) */
/* or to be used as a type reference */
Type
TYPEcreate_name(Symbol *symbol)
{
	Scope s = SCOPEcreate_nostab(OBJ_TYPE);
	TypeHead t = TYPEHEAD_new();

	s->u.type = t;
	s->symbol = *symbol;
	return s;
}

Type
TYPEcreate_user_defined_tag(Type base,Scope scope,struct Symbol_ *symbol)
{
	Type t;
	extern int tag_count;

	t = (Type)DICTlookup(scope->symbol_table,symbol->name);
	if (t) {
		if (DICT_type == OBJ_TAG) return(t);
		else {
			/* easiest to just generate the error this way! */
			/* following call WILL fail intentionally */
			DICTdefine(scope->symbol_table,symbol->name,0,symbol,OBJ_TAG);
			return(0);
		}
	}

	/* tag is undefined */
	/* if we are outside a formal parameter list (hack, hack) */
	/* then we can only refer to existing tags, so produce an error */
	if (tag_count < 0) {
	    ERRORreport_with_symbol(ERROR_undefined_tag, symbol,
				    symbol->name);
	    return(0);
	}

	/* otherwise, we're in a formal parameter list, */
	/* so it's ok to define it */
	t = TYPEcreate_nostab(symbol,scope,OBJ_TAG);
	t->u.type->head = base;

	/* count unique type tags inside PROC and FUNC headers */
	tag_count++;

	return(t);
}

Type
TYPEcreate(enum type_enum type)
{
	TypeBody tb = TYPEBODYcreate(type);
	Type t = TYPEcreate_from_body_anonymously(tb);
	return(t);
}	

Type
TYPEcreate_from_body_anonymously(TypeBody tb)
{
	Type t = SCOPEcreate_nostab(OBJ_TYPE);
	TypeHead th = TYPEHEAD_new();

	t->u.type = th;
	t->u.type->body = tb;
	t->symbol.name = 0;
	SYMBOLset(t);
	return t;
}

TypeBody
TYPEBODYcreate(enum type_enum type)
{
	TypeBody tb = TYPEBODY_new();
	tb->type = type;
	return tb;
}

/* return true if "type t" inherits from "enum type_enum" */
/* may need to be implemented for more types */
#define TYPE_inherits_from(t,e)	((t) && TYPEinherits_from((t),(e)))

Boolean
TYPEinherits_from(Type t, enum type_enum e)
{
	TypeBody tb = t->u.type->body;

	switch (e) {
	case aggregate_:
		if (tb->type == aggregate_ ||
		    tb->type == array_ ||
		    tb->type == bag_ ||
		    tb->type == set_ ||
		    tb->type == list_) return True;
		else return(TYPE_inherits_from(tb->base,e));
	case array_:
		return((tb->type == array_)?True:TYPE_inherits_from(tb->base,e));
	case bag_:
		return((tb->type == bag_ ||
			tb->type == set_)?True:TYPE_inherits_from(tb->base,e));
	case set_:
		return((tb->type == set_)?True:TYPE_inherits_from(tb->base,e));
	case list_:
		return((tb->type == list_)?True:TYPE_inherits_from(tb->base,e));
	}
	return (tb->type == e);
}

#if 0
	case binary_:
		return((t->type == binary_)?True:TYPEinherits_from(t->base,e));
	case integer_:
		return((t->type == integer_)?True:TYPEinherits_from(t->base,e));
	case real_:
		return((t->type == real_)?True:TYPEinherits_from(t->base,e));
	case string_:
		return((t->type == string_)?True:TYPEinherits_from(t->base,e));
	case logical_:
		return((t->type == logical_)?True:TYPEinherits_from(t->base,e));
	case boolean_:
		return((t->type == boolean_)?True:TYPEinherits_from(t->base,e));
	default: return(false);
	}
}
#endif

Symbol *
TYPE_get_symbol(Generic t)
{
	return(&((Type)t)->symbol);
}


/*
** Procedure:	TYPEinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Type module.
*/

void
TYPEinitialize()
{
	MEMinitialize(&TYPEHEAD_fl,sizeof(struct TypeHead_),500,100);
	MEMinitialize(&TYPEBODY_fl,sizeof(struct TypeBody_),200,100);
	OBJcreate(OBJ_TYPE,TYPE_get_symbol,"type", OBJ_TYPE_BITS);
/*	OBJcreate(OBJ_TYPE,TYPE_get_symbol,"(headless) type", OBJ_UNFINDABLE_BITS);*/
	OBJcreate(OBJ_TAG,TYPE_get_symbol,"tag", OBJ_TYPE_BITS);

	/* Very commonly-used read-only types */
	Type_Unknown = TYPEcreate(unknown_);
	Type_Dont_Care = TYPEcreate(special_);
	Type_Bad = TYPEcreate(special_);
	Type_Runtime = TYPEcreate(runtime_);

	Type_Enumeration = TYPEcreate(enumeration_);
	Type_Enumeration->u.type->body->flags.shared = 1;
	resolved_all(Type_Enumeration);

	Type_Expression = TYPEcreate(op_);
	Type_Expression->u.type->body->flags.shared = 1;

	Type_Aggregate = TYPEcreate(aggregate_);
	Type_Aggregate->u.type->body->flags.shared = 1;
	Type_Aggregate->u.type->body->base = Type_Runtime;

	Type_Integer = TYPEcreate(integer_);
	Type_Integer->u.type->body->flags.shared = 1;
	resolved_all(Type_Integer);

	Type_Real = TYPEcreate(real_);
	Type_Real->u.type->body->flags.shared = 1;
	resolved_all(Type_Real);

	Type_Number = TYPEcreate(number_);
	Type_Number->u.type->body->flags.shared = 1;
	resolved_all(Type_Number);

	Type_String = TYPEcreate(string_);
	Type_String->u.type->body->flags.shared = 1;
	resolved_all(Type_String);

	Type_String_Encoded = TYPEcreate(string_);
	Type_String_Encoded->u.type->body->flags.shared = 1;
	Type_String_Encoded->u.type->body->flags.encoded = 1;
	resolved_all(Type_String);

	Type_Logical = TYPEcreate(logical_);
	Type_Logical->u.type->body->flags.shared = 1;
	resolved_all(Type_Logical);

	Type_Binary = TYPEcreate(binary_);
	Type_Binary->u.type->body->flags.shared = 1;
	resolved_all(Type_Binary);

	Type_Number = TYPEcreate(number_);
	Type_Number->u.type->body->flags.shared = 1;
	resolved_all(Type_Number);

	Type_Boolean = TYPEcreate(boolean_);
	Type_Boolean->u.type->body->flags.shared = 1;
	resolved_all(Type_Boolean);

	Type_Generic = TYPEcreate(generic_);
	Type_Generic->u.type->body->flags.shared = 1;
	resolved_all(Type_Generic);

	Type_Set_Of_String = TYPEcreate(set_);
	Type_Set_Of_String->u.type->body->flags.shared = 1;
	Type_Set_Of_String->u.type->body->base = Type_String;

	Type_Set_Of_Generic = TYPEcreate(set_);
	Type_Set_Of_Generic->u.type->body->flags.shared = 1;
	Type_Set_Of_Generic->u.type->body->base = Type_Generic;

	Type_Bag_Of_Generic = TYPEcreate(bag_);
	Type_Bag_Of_Generic->u.type->body->flags.shared = 1;
	Type_Bag_Of_Generic->u.type->body->base = Type_Generic;

	Type_Attribute = TYPEcreate(attribute_);
	Type_Attribute->u.type->body->flags.shared = 1;

	Type_Entity = TYPEcreate(entity_);
	Type_Entity->u.type->body->flags.shared = 1;

	Type_Funcall = TYPEcreate(funcall_);
	Type_Funcall->u.type->body->flags.shared = 1;

	Type_Generic = TYPEcreate(generic_);
	Type_Generic->u.type->body->flags.shared = 1;

	Type_Identifier = TYPEcreate(identifier_);
	Type_Identifier->u.type->body->flags.shared = 1;

	Type_Oneof = TYPEcreate(oneof_);
	Type_Oneof->u.type->body->flags.shared = 1;

	Type_Query = TYPEcreate(query_);
	Type_Query->u.type->body->flags.shared = 1;

	Type_Self = TYPEcreate(self_);
	Type_Self->u.type->body->flags.shared = 1;

    ERROR_corrupted_type =
	ERRORcreate("Corrupted type in %s", SEVERITY_DUMP);

    ERROR_undefined_tag =
	ERRORcreate("Undefined type tag %s", SEVERITY_ERROR);
}

/*
** Procedure:	TYPEget_size
** Parameters:	Type type	- type to examine
** Returns:	Boolean		- logical size of a type instance
** Description:	Compute the size of an instance of some type. 
**
** Notes:	Simple types all have size 1, as does a select type. 
**		The size of an aggregate type is the maximum number
**		of elements an instance can contain; and the size of
**		an entity type is its total attribute count.
*/

int
TYPEget_size(Type type)
{
    Class	class;
    int		size;
    Expression	upper;
    Linked_List	list;
    Error	experrc;

    class = OBJget_class(type);
    if (class == Class_Select_Type) {
	fprintf(stderr, "NOTICE: TYPEget_size(Select_Type) called\n");
	return 1;
    } else if (class == Class_String_Type) {
 	return 1;
    } else if ((class == Class_Enumeration_Type) ||
	       (class == Class_Generic_Type) ||
	       (class == Class_Number_Type) ||
	       TYPEinherits_from(type, Class_Logical_Type) ||
/*	       TYPEinherits_from(type, Class_Sized_Type)) {*/
		TYPEinherits_from(type, Class_Binary_Type) ||
		TYPEinherits_from(type, Class_Integer_Type) ||
		TYPEinherits_from(type, Class_Real_Type) ||
		TYPEinherits_from(type, Class_String_Type)) {
 	return 1;
    } else if (TYPEinherits_from(type, Class_Bag_Type)) {
 	upper = AGGR_TYPEget_upper_limit(type);
	if ((upper == LITERAL_INFINITY) ||
	    (upper == EXPRESSION_NULL))
	    return AGGR_CHUNK_SIZE;
	experrc = 0;
	size = EXPget_integer_value(upper);
	if (experrc) ERRORreport_with_symbol(experrc, &type->symbol);
	return size;
    } else if (TYPEinherits_from(type, Class_Aggregate_Type)) {
	upper = AGGR_TYPEget_upper_limit(type);
	if ((upper == LITERAL_INFINITY) ||
	    (upper == EXPRESSION_NULL) ||
	    TYPEis_identifier(upper->type))
	    return AGGR_CHUNK_SIZE;
	experrc = 0;
	size = EXPget_integer_value(upper);
	if (experrc) ERRORreport_with_symbol(experrc, &type->symbol);
	experrc = 0;
	size = size - EXPget_integer_value(AGGR_TYPEget_lower_limit(type)) + 1;
	if (experrc) ERRORreport_with_symbol(experrc, &type->symbol);
	return size;
    } else if (class == Class_Entity_Type) {
#if 0
	list = COMP_TYPEget_items(type);
	list = ENTITYget_attributes(ENT_TYPEget_entity(type));
	size = 0;
	LISTdo(list, entity, Entity)
	    size += ENTITYget_size(entity);
	LISTod;
	return size;
#endif
	return(ENTITYget_size(ENT_TYPEget_entity(type)));
    } else {
	ERRORreport(ERROR_corrupted_type, "TYPEget_size");
	return 0;
    }
}

#if 0

/*
** Procedure:	TYPEcompatible
** Parameters:	Type lhs_type	- type for left-hand-side of assignment
**		Type rhs_type	- type for right-hand-side of assignment
** Returns:	Boolean		- are the types assignment compatible? 
** Description:	Determine whether two types are assignment-compatible.
*/

Boolean
TYPEcompatible(Type lhs_type, Type rhs_type)
{
    Class	lhs_class;
    Class	rhs_class;
    Entity	entity;
    Linked_List	list;
    Error	experrc;

    lhs_class = OBJget_class(lhs_type);
    rhs_class = OBJget_class(rhs_type);
    if (lhs_class == Class_Aggregate_Type) {
 	return (CLASSinherits_from(rhs_class, Class_Aggregate_Type) &&
		TYPEcompatible(AGGR_TYPEget_base_type(lhs_type),
			       AGGR_TYPEget_base_type(rhs_type)));
    } else if (CLASSinherits_from(lhs_class, Class_Aggregate_Type)) {
 	return ((rhs_class == lhs_class) &&
		TYPEcompatible(AGGR_TYPEget_base_type(lhs_type),
			       AGGR_TYPEget_base_type(rhs_type)));
    } else if (lhs_class == Class_Entity_Type) {
	if (rhs_class == Class_Entity_Type) {
	    entity = ENT_TYPEget_entity(lhs_type);
	    list = COMP_TYPEget_items(rhs_type);
	    LISTdo(list, e, Entity)
		if (!ENTITYhas_supertype(e, entity))
		    return False;
	    LISTod;
	    return True;
	} else {
	    return False;
	}
    } else if (CLASSinherits_from(lhs_class, Class_Composed_Type)) {
	return OBJequal(rhs_type, lhs_type, &experrc);
    } else if (lhs_class == Class_Number_Type) {
	return ((rhs_class == Class_Number_Type) ||
		(rhs_class == Class_Real_Type) ||
		(rhs_class == Class_Integer_Type));
    } else if (lhs_class == Class_Real_Type) {
	return ((rhs_class == Class_Real_Type) ||
		(rhs_class == Class_Integer_Type));
    } else {
	return rhs_class == lhs_class;
    }
}
#endif

/*
** Procedure:	AGGR_TYPEget_base_type
** Parameters:	Aggregate_Type type	- type to examine 
** Returns:	Type			- the base type of the aggregate type
** Description:	Retrieve the base type of an aggregate.
*/

Type
TYPEget_nonaggregate_base_type(Type t)
{
	while (TYPEis_aggregate(t)) t = t->u.type->body->base;
	return t;
}

