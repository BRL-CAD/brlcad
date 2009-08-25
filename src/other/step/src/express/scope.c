static char rcsid[] = "$Id: scope.c,v 1.9 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Scope
** Description:	This module implements a hierarchical (i.e., scoped)
**	symbol table.  The symbol table can store definitions of entities,
**	types, algorithms, and variables, as well as containing a list
**	of subscopes.
** Constants:
**	SCOPE_NULL	- the null scope
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: scope.c,v $
 * Revision 1.9  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.8  1995/06/08  22:59:59  clark
 * bug fixes
 *
 * Revision 1.7  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.6  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/01/19  22:16:43  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 */

#define SCOPE_C
#include "express/scope.h"
#include "express/resolve.h"

Symbol *
SCOPE_get_symbol(Generic s)
{
	return(&((Scope)s)->symbol);
}

void
SCOPEinitialize(void)
{
	OBJcreate(OBJ_SCHEMA,SCOPE_get_symbol,"schema",OBJ_SCHEMA_BITS);
	MEMinitialize(&SCOPE_fl,sizeof(struct Scope_),100,50);
}

/*
** Procedure:	SCOPEget_entities
** Parameters:	Scope       scope	- scope to examine
** Returns:	Linked_List of Entity	- entities defined locally
** Description:	Retrieve a list of the entities defined locally in a scope.
**
** Notes:	This function is considerably faster than
**	SCOPEget_entities_superclass_order(), and should be used whenever
**	the order of the entities on the list is not important.
*/

void
SCOPE_get_entities(Scope scope,Linked_List result)
{
	DictionaryEntry de;
	Generic x;

	DICTdo_type_init(scope->symbol_table,&de,OBJ_ENTITY);
	while (0 != (x = DICTdo(&de))) {
		LISTadd_last(result,x);
	}
}

Linked_List
SCOPEget_entities(Scope scope)
{
	Linked_List result = LISTcreate();
	SCOPE_get_entities(scope,result);
	return(result);
}

/*
** Procedure:	SCOPEget_entities_superclass_order
** Parameters:	Scope       scope	- scope to examine
** Returns:	Linked_List of Entity	- entities defined locally
** Description:	Retrieve a list of the entities defined locally in a scope.
**
** Notes:	The list returned is ordered such that an entity appears
**	before all of its subtypes.
*/

void
SCOPE_dfs(Dictionary symbols, Entity root, Linked_List result)
{
    Entity ent;

    if ((ENTITYget_mark(root) != ENTITY_MARK)) {
	ENTITYput_mark(root, ENTITY_MARK);
	LISTdo(ENTITYget_supertypes(root), super, Entity)
		/* if super explicitly defined in scope, recurse. */
		/* this chops out USEd and REFd entities */
		if ((ent = (Entity)DICTlookup(symbols, ENTITYget_name(super))) != ENTITY_NULL)
			SCOPE_dfs(symbols, ent, result);
	LISTod
	LISTadd_last(result, (Generic)root);
    }
}

Linked_List
SCOPEget_entities_superclass_order(Scope scope)
{
	Linked_List result;
	DictionaryEntry	de;

	result = LISTcreate();
	++ENTITY_MARK;
	SCOPEdo_entities(scope,e,de)
		SCOPE_dfs(scope->symbol_table, e, result);
	SCOPEod;
	return result;
}

#if 0

/*
** Procedure:	SCOPEget_types
** Parameters:	Scope       scope	- scope to examine
** Returns:	Linked_List of Typedef	- local type definitions
** Description:	Retrieve a list of the types defined locally in a scope.
*/

Linked_List
SCOPEget_types(Scope scope)
{
    struct Scope*	data;
    Linked_List		list;
    Error		experrc;
    Symbol s;
    DictionaryEntry	de;

    list = OBJcreate(Class_Linked_List, &experrc);
    DICTdo_init(scope->symbol_table,&de);
    while (s = DICTdo(&de)) {
	if (OBJis_kind_of(s, Class_Type))
	    LISTadd_last(list, (Generic)s);
    }
    return list;
}

/*
** Procedure:	SCOPEget_variables
** Parameters:	Scope       scope	- scope to examine
** Returns:	Linked_List of Variable	- variables defined locally
** Description:	Retrieve a list of the variables defined locally in a scope.
*/

Linked_List
SCOPEget_variables(Scope scope)
{
    struct Scope*	data;
    Linked_List		list;
    Error		experrc;
    Symbol s;
    DictionaryEntry	de;

    list = OBJcreate(Class_Linked_List, &experrc);
    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    DICTdo_init(data->symbol_table,&de);
    while (s = DICTdo(&de)) {
	if (OBJis_kind_of(s, Class_Variable))
	    LISTadd_last(list, (Generic)s);
    }
    return list;
}

/*
** Procedure:	SCOPEget_algorithms
** Parameters:	Scope       scope		- scope to examine
** Returns:	Linked_List of Algorithm	- algorithms defined locally
** Description:	Retrieve a list of the algorithms defined locally in a scope.
*/

Linked_List
SCOPEget_algorithms(Scope scope)
{
    struct Scope*	data;
    Linked_List		list;
    Error		experrc;
    Symbol s;
    DictionaryEntry	de;

    list = OBJcreate(Class_Linked_List, &experrc);
    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    DICTdo_init(data->symbol_table,&de);
    while (s = DICTdo(&de)) {
	if (OBJis_kind_of(s, Class_Algorithm))
	    LISTadd_last(list, (Generic)s);
    }
    return list;
}

/*
** Procedure:	SCOPEget_constants
** Parameters:	Scope       scope	- scope to examine
** Returns:	Linked_List of Constant	- constants defined locally
** Description:	Retrieve a list of the constants defined locally in a scope.
*/

Linked_List
SCOPEget_constants(Scope scope)
{
    struct Scope*	data;
    Linked_List		list;
    Error		experrc;
    Symbol s;
    DictionaryEntry	de;

    list = OBJcreate(Class_Linked_List, &experrc);
    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    DICTdo_init(data->symbol_table,&de);
    while (s = DICTdo(&de)) {
	if (OBJis_kind_of(s, Class_Constant))
	    LISTadd_last(list, (Generic)s);
    }
    return list;
}

/*
** Procedure:   SCOPEget_references
** Parameters:  Scope       scope                       - scope to examine
** Returns:     Dictionary of Symbols REFERENCE'd by a schema
** Description: Retrieve a Dictionary of Symbols REFERENCE'd by a schema
*/

Dictionary
SCOPEget_references(Scope scope)
{
    struct Scope*       data;
    Error               experrc;

    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    return OBJcopy(data->references, &experrc);
}

/*
** Procedure:	SCOPEput_resolved
** Parameters:	Scope scope	- scope to modify
** Returns:	void
** Description:	Set the 'resolved' flag for a scope.
**
** Notes:	This should be called only when the scope has indeed
**		been resolved.
*/

void
SCOPEput_resolved(Scope scope)
{
    struct Scope*	data;
    Error		experrc;

    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    data->resolved = true;
}

/*
** Procedure:	SCOPEget_resolved
** Parameters:	Scope scope	- scope to examine
** Returns:	Boolean		- has scope been resolved?
** Description:	Checks whether references within a scope have been resolved.
*/

Boolean
SCOPEget_resolved(Scope scope)
{
    struct Scope*	data;
    Error		experrc;

    data = (struct Scope*)OBJget_data(scope, Class_Scope, &experrc);
    return data->resolved;
}

/*
** Procedure:	SCOPEdump
** Parameters:	Scope scope	- scope to dump
**		FILE* file	- file stream to dump to
** Returns:	void
** Description:	Dump a schema to a file.
**
** Notes:	This function is provided for debugging purposes.
*/

void
SCOPEdump(Scope scope, FILE* file)
{
    Dictionary	dict;
    Linked_List	list, exp_list,ref;
    Error	experrc;
    Symbol s;
    DictionaryEntry	de;

    fprintf(file, "definitions:\n");
    dict = ((struct Scope*)OBJget_data(scope, Class_Scope, &experrc))->symbol_table;
    DICTdo_init(dict,&de);
    while (s = DICTdo(&de)) {
	fprintf(file, "%s %s\n", SYMBOLget_name(s), CLASSget_name(OBJget_class(s)));
    }

    fprintf(file, "Used symbols:\n");
    list = ((struct Scope*)OBJget_data(scope, Class_Scope, &experrc))->use;
    LISTdo(list, use, Linked_List)
	fprintf(file, "From schema %s\n", SYMBOLget_name(LISTget_first(use)));
        exp_list = LISTget_second(use);
        LISTdo(exp_list,exp, Expression);
            fprintf(file, "   %s AS %s\n",
                    SYMBOLget_name((Symbol)BIN_EXPget_first_operand(exp)),
                    SYMBOLget_name(BIN_EXPget_second_operand(exp)));
        LISTod; /* exp */
    LISTod; 

    fprintf(file, "References:\n");
    dict = ((struct Scope*)OBJget_data(scope, Class_Scope, &experrc))->references;
    DICTdo_init(dict,&de);
    while (ref = DICTdo(&de)) {
	fprintf(file, "From schema %s\n", SYMBOLget_name(LISTget_first(ref)));
        exp_list = LISTget_second(ref);
        LISTdo(exp_list,exp, Expression);
            fprintf(file, "   %s AS %s\n",
                    SYMBOLget_name((Symbol)BIN_EXPget_first_operand(exp)),
                    SYMBOLget_name(BIN_EXPget_second_operand(exp)));
        LISTod; /* exp */ 
    }; /* while */

/* N14 Nested schemas are obsolete 
    list = SCOPEget_schemata(scope);
    LISTdo(list, s, Schema)
	SCHEMAdump(s, file);
    LISTod;
    OBJfree(list, &experrc); */

}
/*
** Procedure:	SCOPE_get_ref_name
** Parameters:	Linked_List ref_entry	- Reference dictionary entry
** Returns:	char *			- Local name or schema name
** Description:	Naming function for Reference dictionary
**
*/

char * SCOPE_get_ref_name(Linked_List ref_entry) 
{
    if (LISTget_second(ref_entry) == LIST_NULL) 
      {  return SYMBOLget_name(LISTget_first(ref_entry));
	 
	 }
    else return SYMBOLget_name(LISTget_second(ref_entry));
}

void
SCOPEprint(Scope scope)
{
	Error experrc;
	struct Scope *data;

	print_force(scope_print);

	data = OBJget_data(scope,Class_Scope,&experrc);
	SCOPE_print(data);

	print_unforce(scope_print);
}
#endif

/* find an entity type, return without resolving it */
/* note that object found is not actually checked, only because */
/* caller is in a better position to describe the error with context */
Generic
SCOPEfind(Scope scope, char *name,int type)
{
	extern Generic SCOPE_find(Scope , char *,int);
	extern Dictionary EXPRESSbuiltins;	/* procedures/functions */
	Generic x;

	__SCOPE_search_id++;

	x = SCOPE_find(scope,name,type);
	if (x) return x;

	if (type & (SCOPE_FIND_FUNCTION | SCOPE_FIND_PROCEDURE)) {
		x = DICTlookup(EXPRESSbuiltins,name);
	}
	return x;
}


/* look up types, functions, etc.  anything not inherited through */
/* the supertype/subtype hierarchy */
/* EH???  -> lookup an object when the current scope is not a schema */
Generic
SCOPE_find(Scope scope,char *name,int type)
{
	Generic result;
	Rename *rename;

	if (scope->search_id == __SCOPE_search_id) return 0;
	scope->search_id = __SCOPE_search_id;

	/* go up the superscopes, looking for object */
	while (1) {
		/* first look up locally */
		result = DICTlookup(scope->symbol_table,name);
		if (result && OBJtype_is_oneof(DICT_type,type)) {
			return result;
		}
		if (scope->type == OBJ_SCHEMA) break;

		scope = scope->superscope;
	}

	if (type & (SCOPE_FIND_ENTITY | SCOPE_FIND_TYPE)) {
		/* Occurs in a fully USE'd schema? */
		LISTdo(scope->u.schema->use_schemas,schema,Schema)
			/* follow chain'd USEs */
			if (schema == 0) continue;
			result = SCOPE_find(schema,name,type);
			if (result) return(result);
		LISTod;

		/* Occurs in a partially USE'd schema? */
		rename = (Rename *)DICTlookup(scope->u.schema->usedict,name);
		if (rename) {
			DICT_type = rename->type;
			return(rename->object);
		}
	}

	/* Occurs in a fully REF'd schema? */
	LISTdo(scope->u.schema->ref_schemas,schema,Schema)
		if (schema == 0) continue;
		result = DICTlookup(schema->symbol_table,name);
		if (result) return result;
		else continue;	/* try another schema */
	LISTod;

	/* Occurs in a partially REF'd schema? */
	rename = (Rename *)DICTlookup(scope->u.schema->refdict,name);
	if (rename) {
		DICT_type = rename->type;
		return(rename->object);
	}

	return 0;
}

#if 0
/* useful for extracting true ref of SELF when you're inside of a tiny scope */
Entity
SCOPEget_nearest_enclosing_entity(Scope s)
{
	for (;s;s = s->superscope) {
		if (s->type == OBJ_ENTITY) break;
	}
	return (s);
}
#endif
