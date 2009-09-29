static char rcsid[] = "$Id: entity.c,v 1.11 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Entity
** Description:	This module is used to represent Express entity definitions.
**	An entity definition consists of a name, a set of attributes, a
**	collection of uniqueness sets, a specification of the entity's
**	position in a class hierarchy (i.e., sub- and supertypes), and
**	a list of constraints which must be satisfied by instances of
**	the entity.  A uniquess set is a set of one or more attributes
**	which, when taken together, uniquely identify a specific instance
**	of the entity.  Thus, the uniqueness set { name, ssn } indicates
**	that no two instances of the entity may share both the same
**	values for both name and ssn.
** Constants:
**	ENTITY_NULL	- the null entity
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: entity.c,v $
 * Revision 1.11  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.10  1995/03/09  18:43:03  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.9  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.8  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/16  03:14:47  libes
 * simplified find calls
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
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
 * Revision 1.7  1992/05/10  06:01:29  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.6  1992/05/05  19:51:47  libes
 * final alpha
 *
 * Revision 1.5  1992/02/19  15:48:18  libes
 * changed types to enums & flags
 *
 * Revision 1.4  1992/02/17  14:32:57  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:05:23  libes
 * y
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:49:53  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.7  1991/09/20  06:20:43  libes
 * fixed bug that printed out entity->attributes incorrectly
 * assumed they were objects rather than strings
 *
 * Revision 4.6  1991/09/16  23:13:12  libes
 * added print functionsalgorithm.c
 *
 * Revision 4.5  1991/07/18  05:07:08  libes
 * added SCOPEget_use
 *
 * Revision 4.4  1991/07/13  05:04:17  libes
 * added ENTITYget_supertype and ..get_subtype
 *
 * Revision 4.3  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.3  91/01/08  18:55:42  pdesadmn
 * Initial - Beta checkin at SCRA
 * 
 * Revision 4.2  90/11/23  16:33:43  clark
 * Initial checkin at SCRA
 * 
 * Revision 4.2  90/11/23  16:33:43  clark
 * initial checkin at SCRA
 * 
 * Revision 4.2  90/11/23  16:33:43  clark
 * Fixes for better error handling on supertype lists
 * 
 * Revision 4.1  90/09/13  15:13:08  clark
 * BPR 2.1 alpha
 * 
 */

#define ENTITY_C
#include "express/entity.h"
#include "express/express.h"
#include "express/object.h"

/* returns true if variable is declared (or redeclared) directly by entity */
int
ENTITYdeclares_variable(Entity e,Variable v)
{
	LISTdo(e->u.entity->attributes, attr, Variable)
		if (attr == v) return True;
	LISTod;

	return False;
}

static
Entity 
ENTITY_find_inherited_entity(Entity entity, char *name, int down)
{
	Entity result;

	/* avoid searching scopes that we've already searched */
	/* this can happen due to several things */
	/* if A ref's B which ref's C, and A ref's C.  Then C */
	/* can be searched twice by A.  Similar problem with */
	/* sub/super inheritance. */
	if (entity->search_id == __SCOPE_search_id) return NULL;
	entity->search_id = __SCOPE_search_id;

	LISTdo(entity->u.entity->supertypes,super,Entity )
		if (streq(super->symbol.name,name)) return super;
	LISTod

	LISTdo(entity->u.entity->supertypes,super,Entity )
		result = ENTITY_find_inherited_entity(super, name, down);
		if (result) {
			return result;
		}
	LISTod;

	if (down) {
	    LISTdo(entity->u.entity->subtypes,sub,Entity )
		if (streq(sub->symbol.name,name)) return sub;
	    LISTod;

	    LISTdo(entity->u.entity->subtypes,sub,Entity )
		result = ENTITY_find_inherited_entity(sub, name, down);
		if (result) {
		    return result;
		}
	    LISTod;
	}

	return 0;
}

struct Scope_ *
ENTITYfind_inherited_entity(struct Scope_ *entity, char *name, int down)
{
	if (streq(name,entity->symbol.name)) return(entity);

	__SCOPE_search_id++;
	return ENTITY_find_inherited_entity(entity, name, down);
}

/* find a (possibly inherited) attribute */
Variable
ENTITY_find_inherited_attribute(Entity entity, char *name, int *down, struct Symbol_ **where)
{
    Variable result;

    /* avoid searching scopes that we've already searched */
    /* this can happen due to several things */
    /* if A ref's B which ref's C, and A ref's C.  Then C */
    /* can be searched twice by A.  Similar problem with */
    /* sub/super inheritance. */
    if (entity->search_id == __SCOPE_search_id) return NULL;
    entity->search_id = __SCOPE_search_id;

    /* first look locally */
    result = (Variable)DICTlookup(entity->symbol_table, name);
    if (result) {
	if (down && *down && where) *where = &entity->symbol;
	return result;
    }

    /* check supertypes */
    LISTdo(entity->u.entity->supertypes,super,Entity )
	result = ENTITY_find_inherited_attribute(super,name,down,where);
	if (result) {
	    return result;
	}
    LISTod;

    /* check subtypes, if requested */
    if (down) {
	++*down;
	LISTdo(entity->u.entity->subtypes,sub,Entity )
	    result = ENTITY_find_inherited_attribute(sub,name,down,where);
	    if (result) {
		return result;
	    }
	LISTod;
	--*down;
    }

    return 0;
}

Variable
ENTITYfind_inherited_attribute(struct Scope_ *entity, char *name,
			       struct Symbol_ **down_sym)
{
    extern int __SCOPE_search_id;
    int down_flag = 0;

    __SCOPE_search_id++;
    if (down_sym) {
	return ENTITY_find_inherited_attribute(entity, name, &down_flag, down_sym);
    } else {
	return ENTITY_find_inherited_attribute(entity, name, 0, 0);
    }
}

/* resolve a (possibly group-qualified) attribute ref. *
/* report errors as appropriate */
Variable
ENTITYresolve_attr_ref(Entity e, Symbol *grp_ref, Symbol *attr_ref)
{
    extern Error ERROR_unknown_supertype;
    extern Error ERROR_unknown_attr_in_entity;
    Entity ref_entity;
    Variable attr;
    struct Symbol_ *where;

    if (grp_ref) {
	/* use entity provided in group reference */
	ref_entity = ENTITYfind_inherited_entity(e, grp_ref->name, 0);
	if (!ref_entity) {
	    ERRORreport_with_symbol(ERROR_unknown_supertype, grp_ref,
				    grp_ref->name, e->symbol.name);
	    return 0;
	}
	attr = (Variable)DICTlookup(ref_entity->symbol_table,
				    attr_ref->name);
	if (!attr) {
	    ERRORreport_with_symbol(ERROR_unknown_attr_in_entity,
				    attr_ref, attr_ref->name,
				    ref_entity->symbol.name);
/*	    resolve_failed(e);*/
	}
    } else {
	/* no entity provided, look through supertype chain */
	where = NULL;
	attr = ENTITYfind_inherited_attribute(e, attr_ref->name, &where);
	if (!attr /* was ref_entity? */) {
	    ERRORreport_with_symbol(ERROR_unknown_attr_in_entity,
				    attr_ref, attr_ref->name,
				    e->symbol.name);
	} else if (where != NULL) {
	    ERRORreport_with_symbol(ERROR_implicit_downcast, attr_ref,
				    where->name);
	}
    }
    return attr;
}

#if 0
/* find a simple object referred to be an entity */
/* such as an entity, proc, etc. */
Generic
ENTITY_lookup(Entity entity, char * name)
{
	Generic	result;

	/* avoid searching scopes that we've already searched */
	/* this can happen due to several things */
	/* if A ref's B which ref's C, and A ref's C.  Then C */
	/* can be searched twice by A.  Similar problem with */
	/* sub/super inheritance. */
#if 0
	if (entity->last_search == __ENTITY_search_count) return NULL;
	entity->last_search = __ENTITY_search_count;
#endif

	/* first look locally */
        result = DICTlookup(entity->symbol_table, name);
        if (result != 0) {
	  ENTITY_type = DICT_type;
          return result;
        }

	/* look in the rest of the schema */
	/* note that this convenient gets things LOCAL to schema, but not */
	/* LOCAL to any declarations inside the schema (like a proc) or */
	/* another entity's attributes */
	result = DICTlookup(entity->schema->symbol_table,name);
	if (result != 0) {
		ENTITY_type = DICT_type;
		return result;
	}

        /* check if it's explicitly REFERENCE'd */
        result = DICTlookup(entity->schema->referenced_objects, name);
        if (result != 0) {
	  ENTITY_type = DICT_type;
          experrc = ERROR_none;
          return result;
        }

	/* check if it is one of the object ref'd implicitly by only a */
	/* schema name */
	LISTdo(entity->schema->referenced_schemas,schema,Schema *)
		/* would be nice if we could prevent these referenced scopes from */
		/* being looked up more than once in a single scopelookup */
		result = DICTlookup(schema->symbol_table,name);
		if (result) return(result);
        LISTod;

	experrc = ERROR_undefined_identifier;
	return 0;
}
#endif

/*
** Procedure:	ENTITY_create/free/copy/equal
** Description:	These are the low-level defining functions for Class_Entity
**
** Notes:	The attribute list of a new entity is defined as an
**	empty list; all other aspects of the entity are initially
**	undefined (i.e., have appropriate NULL values).
*/

Entity 
ENTITYcreate(Symbol *sym)
{
	Scope s = SCOPEcreate(OBJ_ENTITY);

	s->u.entity = ENTITY_new();
	s->u.entity->attributes = LISTcreate();
	s->u.entity->inheritance = ENTITY_INHERITANCE_UNINITIALIZED;

	/* it's so useful to have a type hanging around for each entity */
	s->u.entity->type = TYPEcreate_name(sym);
	s->u.entity->type->u.type->body = TYPEBODYcreate(entity_);
	s->u.entity->type->u.type->body->entity = s;
	return(s);
}

/* currently, this is only used by USEresolve */
Entity 
ENTITYcopy(Entity e)
{
	/* for now, do a totally shallow copy */

	Entity e2 = SCOPE_new();
	*e2 = *e;
	return e2;
}

#if 0
void
ENTITY_free(Generic dummy)
{
    struct Entity*	entity = (struct Entity*)dummy;
    Error		experrc;

    OBJfree(entity->attributes, &experrc);
    OBJfree(entity->supertypes, &experrc);
    OBJfree(entity->subtypes, &experrc);
    OBJfree(entity->subtype_expression, &experrc);
    OBJfree(entity->unique, &experrc);
    OBJfree(entity->constraints, &experrc);
    OBJfree(entity->instances, &experrc);
}

void
ENTITY_copy(Generic dummy1, Generic dummy2)
{
    struct Entity*	dest = (struct Entity*)dummy1;
    struct Entity*	source = (struct Entity*)dummy2;
    Error		experrc;

    dest->supertypes = OBJcopy(source->supertypes, &experrc);
    dest->subtypes = OBJcopy(source->subtypes, &experrc);
    dest->subtype_expression = OBJcopy(source->subtype_expression, &experrc);
    dest->attributes = OBJcopy(source->attributes, &experrc);
    dest->inheritance = source->inheritance;
    dest->attribute_count = source->attribute_count;
    dest->unique = OBJcopy(source->unique, &experrc);
    dest->constraints = OBJcopy(source->constraints, &experrc);
    dest->instances = OBJcopy(source->instances, &experrc);
    dest->mark = source->mark;
}

Boolean
ENTITY_equal(Generic dummy1, Generic dummy2)
{
    struct Entity*	entity1 = (struct Entity*)dummy1;
    struct Entity*	entity2 = (struct Entity*)dummy2;
    Error		experrc;

    return (OBJequal(entity1->attributes, entity2->attributes, &experrc) &&
	    OBJequal(entity1->supertypes, entity2->supertypes, &experrc));
}

void
ENTITY_print(Generic dummy)
{
	struct Entity*	entity = (struct Entity*)dummy;

	if (print_none(entity_print)) return;

	if (print_some(entity_print,supertypes)) {
		iprint("supertypes:\n");
		OBJprint(entity->supertypes);
	}
	if (print_some(entity_print,subtypes)) {
		iprint("subtype list:\n");
		OBJprint(entity->subtypes);
	}
	if (print_some(entity_print,subtype_expression)) {
		iprint("subtype expr:\n");
		OBJprint(entity->subtype_expression);
	}
	if (print_some(entity_print,attributes)) {
		int i;

		LISTdo(entity->attributes, name, String)
			iprint("attribute %d: %s\n",i++,name);
		LISTod;
	}
	if (print_some(entity_print,inheritance)) {
		iprint("inheritance: %d\n",entity->inheritance);
	}
	if (print_some(entity_print,attribute_count)) {
		iprint("attribute_count: %d\n",entity->attribute_count);
	}
	if (print_some(entity_print,unique)) {
		iprint("unique:\n");
		OBJprint(entity->unique);
	}
	if (print_some(entity_print,constraints)) {
		iprint("constraints:\n");
		OBJprint(entity->constraints);
	}
	if (print_some(entity_print,instances)) {
		iprint("instances:\n");
		OBJprint(entity->instances);
	}
	if (print_some(entity_print,mark)) {
		iprint("mark: %d\n",entity->mark);
	}
	if (print_some(entity_print,abstract)) {
		iprint("abstract: %s\n",BOOLprint(entity->abstract));
	}
}

#endif /*0*/

/*
** Procedure:	ENTITYinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Entity module.
*/

void
ENTITYinitialize()
{
	MEMinitialize(&ENTITY_fl,sizeof(struct Entity_),500,100);
	OBJcreate(OBJ_ENTITY,SCOPE_get_symbol,"entity",
		  OBJ_ENTITY_BITS);
}

#if 0

/*
** Procedure:	ENTITYput_name
** Parameters:	Entity entity - entity to modify
**		String name - entity's name
** Returns:	void
** Description:	Set the name of an entity.
*/

/* this function is defined as a macro in entity.h */

/*
** Procedure:	ENTITYput_supertypes
** Parameters:	Entity      entity	- entity to modify
**		Linked_List list	- superclass entity name Symbols
** Returns:	void
** Description:	Set the (immediate) supertype list of an entity.
*/

void
ENTITYput_supertypes(Entity entity, Linked_List list)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    OBJfree(data->supertypes, &experrc);
    data->supertypes = OBJreference(list);
}

/*
** Procedure:	ENTITYput_subtypes
** Parameters:	Entity      entity	- entity to modify
**		Expression  expression	- controlling subtype expression
** Returns:	void
** Description:	Set the (immediate) subtypes of an entity.
*/

void
ENTITYput_subtypes(Entity entity, Expression expression)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    OBJfree(data->subtypes, &experrc);
    OBJfree(data->subtype_expression, &experrc);
    data->subtypes = LIST_NULL;
    data->subtype_expression = OBJreference(expression);
}

#endif /*0*/

/*
** Procedure:	ENTITYadd_attribute
** Parameters:	Entity   entity		- entity to modify
** 		Variable attribute	- attribute to add
** Returns:	void
** Description:	Add an attribute to an entity.
*/

void
ENTITYadd_attribute(Entity entity, Variable attr)
{
	int rc;

	if (attr->name->type->u.type->body->type != op_) {
		/* simple id */
		rc = DICTdefine(entity->symbol_table,attr->name->symbol.name,
			(Generic)attr,&attr->name->symbol,OBJ_VARIABLE);
	} else {
		/* SELF\ENTITY.SIMPLE_ID */
		rc = DICTdefine(entity->symbol_table,attr->name->e.op2->symbol.name,
			(Generic)attr,&attr->name->symbol,OBJ_VARIABLE);
	}
	if (rc == 0) {
		LISTadd_last(entity->u.entity->attributes,(Generic)attr);
		VARput_offset(attr, entity->u.entity->attribute_count);
		entity->u.entity->attribute_count++;
	}
}

#if 0

/*
** Procedure:	ENTITYput_uniqueness_list
** Parameters:	Entity      entity	- entity to modify
**		Linked_List list	- uniqueness list of Linked_Lists
** Returns:	void
** Description:	Set the attribute uniqueness list of an entity.
**
** Notes:	Each element of the uniqueness list should itself be
**	a list.  The elements of these sublists are attribute names.
**	The attributes in a single sublist, when taken together, must
**	uniquely determine an entity instatiation.  Thus, if person_name
**	has a sublist (first, last), then no two person_name entities
**	may share the same first and last names.
*/

void
ENTITYput_uniqueness_list(Entity entity, Linked_List list)
{
    Symbol sym;

    entity->u.entity->unique = list;
    LISTdo(list, c, Linked_List)
	sym = (Symbol)LISTpeek_first(list);
	DICTdefine(entity->symbol_table, sym.name, , &sym, 
		   );
    LISTod;
}

/*
** Procedure:	ENTITYput_constraints
** Parameters:	Entity      entity	- entity to modify
**		Linked_List constraints	- list of constraints (Expressions)
**					  which entity must satisfy
** Returns:	void
** Description:	Set the constraints on an entity.
**
** Notes:	This is a list of constraints which must be satisfied by
**		each instatiation of the entity, or by the set of all
**		instantiations of the entity.
*/

void
ENTITYput_constraints(Entity entity, Linked_List list)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    OBJfree(data->constraints, &experrc);
    data->constraints = OBJreference(list);
}

#endif

/*
** Procedure:	ENTITYadd_instance
** Parameters:	Entity  entity		- entity to modify
**		Generic instance	- new instance
** Returns:	void
** Description:	Add an item to the instance list of an entity.
*/

void
ENTITYadd_instance(Entity entity, Generic instance)
{
    if (entity->u.entity->instances == LIST_NULL)
	entity->u.entity->instances = LISTcreate();
    LISTadd(entity->u.entity->instances, instance);
}

#if 0

/*
** Procedure:	ENTITYdelete_instance
** Parameters:	Entity  entity		- entity to modify
**		Generic instance	- instance to delete
** Returns:	void
** Description:	Remove an item from the instance list of an entity.
*/

void
ENTITYdelete_instance(Entity entity, Generic instance)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    LISTdo_links(data->instances, link)
	if (link->data == instance)
	    LISTremove(data->instances, link);
    LISTod;
}

/*
** Procedure:	ENTITYput_mark
** Parameters:	Entity entity - entity to modify
**		int value - new mark for entity
** Returns:	void
** Description:	Set an entity's mark.
*/

/* this function is inlined in entity.h */

#endif

/*
** Procedure:	ENTITYhas_supertype
** Parameters:	Entity child	- entity to check parentage of
**		Entity parent	- parent to check for
** Returns:	Boolean		- does child's superclass chain include parent?
** Description:	Look for a certain entity in the supertype graph of an entity.
*/

Boolean
ENTITYhas_supertype(Entity child, Entity parent)
{
    LISTdo(child->u.entity->supertypes, entity, Entity)
	if (entity == parent)
	    return True;
#if 0
	if (OBJis_kind_of(entity, Class_Entity) &&
	    ENTITYhas_supertype(entity, parent))
#endif
	if (ENTITYhas_supertype(entity,parent))
	    return True;
    LISTod;
    return False;
}

#if 0

/*
** Procedure:	ENTITYget_supertype (by name)
** Parameters:	Entity child	- entity to check parentage of
**		String supername- entity to check for
** Returns:	Boolean		- does child's superclass chain include named entity?
** Description:	Returns supertype given name and entity (or ENTITY_NULL).
*/

Entity
ENTITYget_supertype(Entity child, String supername)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(child, Class_Entity, &experrc);
    LISTdo(data->supertypes, super, Entity)
	if (STRINGequal(supername,ENTITYget_name(super))) return(super);
	if (OBJis_kind_of(super, Class_Entity))
	    if (ENTITY_NULL != (super = ENTITYget_supertype(super,supername)))
		return(super);
    LISTod;
    return ENTITY_NULL;
}

/*
** Procedure:	ENTITYget_subtype (by name)
** Parameters:	Entity child	- entity to check parentage of
**		String subname	- entity to check for
** Returns:	Boolean		- does child's subclass chain include named entity?
** Description:	Returns subtype given name and entity (or ENTITY_NULL).
*/

Entity
ENTITYget_subtype(Entity child, String subname)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(child, Class_Entity, &experrc);
    LISTdo(data->subtypes, sub, Entity)
	if (STRINGequal(subname,ENTITYget_name(sub))) return(sub);
	if (ENTITY_NULL != (sub = ENTITYget_subtype(sub,subname)))
		return(sub);
    LISTod;
    return ENTITY_NULL;
}

/*
** Procedure:	ENTITYhas_subtype
** Parameters:	Entity parent	- entity to check descendants of
**		Entity child	- child to check for
** Returns:	Boolean		- does parent's subclass tree include child?
** Description:	Look for a certain entity in the subtype graph of an entity.
*/

Boolean
ENTITYhas_subtype(Entity parent, Entity child)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(parent, Class_Entity, &experrc);
    LISTdo(data->subtypes, entity, Entity)
	if (OBJequal(entity, child, &experrc))
	    return True;
	if (ENTITYhas_subtype(entity, child))
	    return True;
    LISTod;
    return False;
}

/*
** Procedure:	ENTITYhas_immediate_supertype
** Parameters:	Entity child	- entity to check parentage of
**		Entity parent	- parent to check for
** Returns:	Boolean		- is parent a direct supertype of child?
** Description:	Check whether an entity has a specific immediate superclass.
*/

#endif

Boolean
ENTITYhas_immediate_supertype(Entity child, Entity parent)
{
    LISTdo(child->u.entity->supertypes, entity,Entity )
	if (entity == parent) return True;
    LISTod;
    return False;
}

#if 0

/*
** Procedure:	ENTITYhas_immediate_subtype
** Parameters:	Entity parent	- entity to check children of
**		Entity child	- child to check for
** Returns:	Boolean		- is child a direct subtype of parent?
** Description:	Check whether an entity has a specific immediate subclass.
*/

Boolean
ENTITYhas_immediate_subtype(Entity parent, Entity child)
{
    LISTdo(parent->u.entity->subtypes, entity, Entity )
	if (entity == child) return True;
    LISTod;
    return False;
}

Boolean
ENTITYhas_immediate_subtype(Entity parent, Entity child)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(parent, Class_Entity, &experrc);
    LISTdo(data->subtypes, entity, Entity)
	if (OBJequal(entity, child, &experrc))
	    return True;
    LISTod;
    return False;
}

/*
** Procedure:	ENTITYget_name
** Parameters:	Entity entity	- entity to examine
** Returns:	String		- entity name
** Description:	Retrieve the name of an entity.
*/

/* this function is defined as a macro in entity.h */

/*
** Procedure:	ENTITYget_supertypes
** Parameters:	Entity      entity	- entity to examine
** Returns:	Linked_List of Entity	- immediate supertypes of this entity
** Description:	Retrieve a list of an entity's immediate supertypes.
*/

/* this function is defined as a macro in entity.h */

/*
** Procedure:	ENTITYget_subtypes
** Parameters:	Entity      entity	- entity to examine
** Returns:	Linked_List of Entity	- immediate subtypes of this entity
** Description:	Retrieve a list of an entity's immediate subtypes.
*/

Linked_List
ENTITYget_subtypes(Entity entity)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    return OBJreference(data->subtypes);
}

/*
** Procedure:	ENTITY{get,put}_abstract
** Parameters:	Entity      entity	- entity to examine
**		Boolean	    abstract	- is entity abstract?
** Returns:	Boolean			- is entity abstract?
** Description:	Retrieve/Set whether an entity is abstract.
*/

Boolean
ENTITYget_abstract(Entity entity)
{
	struct Entity*	data;
	Error		experrc;

	data = (struct Entity*)OBJget_data(entity,Class_Entity,&experrc);
	return data->abstract;
}

void
ENTITYput_abstract(Entity entity, Boolean abstract)
{
	struct Entity*	data;
	Error		experrc;

	data = (struct Entity*)OBJget_data(entity,Class_Entity,&experrc);
	data->abstract = abstract;
}

/*
** Procedure:	ENTITYget_subtype_expression
** Parameters:	Entity     entity	- entity to examine
** Returns:	Expression		- immediate subtype expression
** Description:	Retrieve the controlling expression for an entity's
**	immediate subtype list.
*/

Expression
ENTITYget_subtype_expression(Entity entity)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    return OBJreference(data->subtype_expression);
}

#endif

/*
** Procedure:	ENTITYget_all_attributes
** Parameters:	Entity      entity	- entity to examine
** Returns:	Linked_List of Variable	- all attributes of this entity
** Description:	Retrieve the attribute list of an entity.
**
** Notes:	If an entity has neither defines nor inherits any
**		attributes, this call returns an empty list.  Note
**		that this is distinct from the constant LIST_NULL.
*/

static
void
ENTITY_get_all_attributes(Entity entity, Linked_List result)
{
    LISTdo(entity->u.entity->supertypes, super, Entity )
/*	if (OBJis_kind_of(super, Class_Entity))*/
	    ENTITY_get_all_attributes(super, result);
    LISTod;
/* Gee, aren't they resolved by this time? */
#if 0
    LISTdo(entity->attributes, name, char *)
	LISTadd_last(result,
		     (Generic)SCOPElookup(entity, name, False, &experrc));
#endif
	LISTdo(entity->u.entity->attributes,attr,Generic)
		LISTadd_last(result,attr);
    LISTod;
}

Linked_List
ENTITYget_all_attributes(Entity entity)
{
    Linked_List result = LISTcreate();

    ENTITY_get_all_attributes(entity, result);
    return result;
}

/*
** Procedure:	ENTITYget_named_attribute
** Parameters:	Entity  entity	- entity to examine
**		String	name	- name of attribute to retrieve
** Returns:	Variable	- the named attribute of this entity
** Description:	Retrieve an entity attribute by name.
**
** Notes:	If the entity has no attribute with the given name,
**	VARIABLE_NULL is returned.
*/

Variable
ENTITYget_named_attribute(Entity entity, char *name)
{
    Variable attribute;

    LISTdo(entity->u.entity->attributes, attr, Variable)
	if (streq(VARget_simple_name(attr), name))
		return attr;
    LISTod;

    LISTdo(entity->u.entity->supertypes, super, Entity )
/*	if (OBJis_kind_of(super, Class_Entity) && */
	if (0 != (attribute = ENTITYget_named_attribute(super,name)))
		return attribute;
    LISTod;
    return 0;
}

/*
** Procedure:	ENTITYget_attribute_offset
** Parameters:	Entity   entity		- entity to examine
**		Variable attribute	- attribute to retrieve offset for
** Returns:	int			- offset to given attribute
** Description:	Retrieve offset to an entity attribute.
**
** Notes:	If the entity does not include the attribute, -1
**	is returned.
*/

int
ENTITYget_attribute_offset(Entity entity, Variable attribute)
{
    int			offset, value;

    LISTdo(entity->u.entity->attributes, attr, Variable)
	if (attr == attribute)
	    return entity->u.entity->inheritance + VARget_offset(attribute);
    LISTod;
    offset = 0;
    LISTdo(entity->u.entity->supertypes, super, Entity )
/*	if (OBJis_kind_of(super, Class_Entity)) {*/
	    if ((value = ENTITYget_attribute_offset(super, attribute)) != -1)
		return value + offset;
	    offset += ENTITYget_initial_offset(super);
/*	}*/
    LISTod;
    return -1;
}

/*
** Procedure:	ENTITYget_named_attribute_offset
** Parameters:	Entity  entity	- entity to examine
**		String	name	- name of attribute to retrieve
** Returns:	int		- offset to named attribute of this entity
** Description:	Retrieve offset to an entity attribute by name.
**
** Notes:	If the entity has no attribute with the given name,
**		-1 is returned.
*/

int
ENTITYget_named_attribute_offset(Entity entity, String name)
{
    int			offset, value;

    LISTdo(entity->u.entity->attributes, attr, Variable)
	if (STRINGequal(VARget_simple_name(attr), name))
	    return entity->u.entity->inheritance +
/*		   VARget_offset(SCOPElookup(entity, name, False));*/
		   VARget_offset(ENTITY_find_inherited_attribute(entity,name,0,0));
    LISTod;
    offset = 0;
    LISTdo(entity->u.entity->supertypes, super, Entity )
/*	if (OBJis_kind_of(super, Class_Entity)) {*/
	    if ((value = ENTITYget_named_attribute_offset(super, name)) != -1)
		return value + offset;
	    offset += ENTITYget_initial_offset(super);
/*	}*/
    LISTod;
    return -1;
}

/*
** Procedure:	ENTITYget_initial_offset
** Parameters:	Entity entity	- entity to examine
** Returns:	int		- number of inherited attributes
** Description:	Retrieve the initial offset to an entity's local frame.
*/

int
ENTITYget_initial_offset(Entity entity)
{
    return entity->u.entity->inheritance;
}

/*
** Procedure:	ENTITYget_size
** Parameters:	Entity entity	- entity to examine
** Returns:	int		- storage size of instantiated entity
** Description:	Compute the storage size of an entity instance.
**
** Notes:	The size is computed in units of Object.
*/

/* macroized in entity.h */

#if 0

/*
** Procedure:	ENTITYget_uniqueness_list
** Parameters:	Entity      entity		- entity to examine
** Returns:	Linked_List of Linked_List	- this entity's uniqueness sets
** Description:	Retrieve an entity's uniqueness list.
**
** Notes:	For a description of the uniqueness list, see the notes to
**		ENTITYput_uniqueness_list.
*/

Linked_List
ENTITYget_uniqueness_list(Entity entity)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    return OBJreference(data->unique);
}

/*
** Procedure:	ENTITYget_constraints
** Parameters:	Entity      entity		- entity to examine
** Returns:	Linked_List of Expression	- this entity's constraints
** Description:	Retrieve the list of constraints on an entity.
*/

Linked_List
ENTITYget_constraints(Entity entity)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    return OBJreference(data->constraints);
}

/*
** Procedure:	ENTITYget_instances
** Parameters:	Entity entity	- entity to examine
** Returns:	Linked_List	- list of instances
** Description:	Retrieve a list of instances of an entity.
*/

Linked_List
ENTITYget_instances(Entity entity)
{
    struct Entity*	data;
    Error		experrc;

    data = (struct Entity*)OBJget_data(entity, Class_Entity, &experrc);
    return OBJreference(data->instances);
}

/*
** Procedure:	ENTITYput_resolved
** Parameters:	Entity entity	- entity to modify
** Returns:	void
** Description:	Set the 'resolved' flag for an entity.
**
** Notes:	This should be called only when the entity has indeed
**		been resolved.
*/

/* this function is defined as a macro in entity.h */

/*
** Procedure:	ENTITYget_resolved
** Parameters:	Entity entity	- entity to examine
** Returns:	Boolean		- has entity been resolved?
** Description:	Checks whether references within an entity have been resolved.
*/

/* this function is defined as a macro in entity.h */

/*
** Procedure:	ENTITYput_inheritance_count
** Parameters:	Entity entity	- entity to modify
**		int    count	- number of inherited attributes
** Returns:	void
** Description:	Set the count of attributes inherited by an entity.
**
** Notes:	This should be computed automatically (perhaps only when
**		needed), and this call removed.  The count is currently
**		computed by ENTITYresolve().
*/

void
ENTITYput_inheritance_count(Entity e, int count)
{
	e->u.entity->inheritance = count;
}
#endif
