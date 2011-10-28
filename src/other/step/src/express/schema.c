static char rcsid[] = "$Id: schema.c,v 1.13 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Schema
** Description:	This module implements the Schema abstraction, which
**	basically amounts to a named symbol table.
** Constants:
**	SCHEMA_NULL	- the null schema
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: schema.c,v $
 * Revision 1.13  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.12  1995/06/08  22:59:59  clark
 * bug fixes
 *
 * Revision 1.11  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.10  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.9  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.8  1993/02/22  21:48:53  libes
 * removed old ifdeffed code
 *
 * Revision 1.7  1993/01/19  22:16:43  libes
 * *** empty log message ***
 *
 * Revision 1.6  1992/08/27  23:42:20  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 */

#include "express/expbasic.h"
#include "express/schema.h"
#include "express/object.h"
#include "express/resolve.h"

struct freelist_head REN_fl;
struct freelist_head SCOPE_fl;
struct freelist_head SCHEMA_fl;

int __SCOPE_search_id = 0;

Symbol *
RENAME_get_symbol(Generic r)
{
	return(((Rename *)r)->old);
}

/*
** Procedure:	SCHEMAinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Schema module.
*/

void
SCHEMAinitialize(void)
{
	OBJcreate(OBJ_RENAME,RENAME_get_symbol,"rename clause",OBJ_UNUSED_BITS);
	MEMinitialize(&REN_fl,sizeof(struct Rename),30,30);
	MEMinitialize(&SCHEMA_fl,sizeof(struct Schema_),40,20);
}

/*
** Procedure:	SCOPEcreate
** Parameters:	Scope  scope	- next higher scope
**		Error* experrc	- buffer for error code
** Returns:	Scope		- the scope created
** Description:	Create and return an empty scope inside a parent scope.
*/

Scope
SCOPEcreate(char type)
{
	Scope d = SCOPE_new();
	d->symbol_table = DICTcreate(50);
	d->type = type;
	return d;
}

Scope
SCOPEcreate_tiny(char type)
{
	Scope d = SCOPE_new();
	d->symbol_table = DICTcreate(1);
	d->type = type;
	return d;
}

/* create a scope without a symbol table */
/* used for simple types */
Scope
SCOPEcreate_nostab(char type)
{
	Scope d = SCOPE_new();
	d->type = type;
	return d;
}

/*
** Procedure:	SCHEMAcreate
** Parameters:	String name	- name of schema to create
**		Scope  scope	- local scope for schema
**		Error* experrc	- buffer for error code
** Returns:	Schema		- the schema created
** Description:	Create and return a schema as specified.
*/

Schema
SCHEMAcreate(void)
{
	Scope s = SCOPEcreate(OBJ_SCHEMA);
	s->u.schema = SCHEMA_new();
	return s;
}

/*
** Procedure:	SCHEMAget_name
** Parameters:	Schema schema	- schema to examine
** Returns:	String		- schema name
** Description:	Retrieve the name of a schema.
*/

/* this function is implemented as a macro in schema.h */

/*
** Procedure:	SCHEMAdump
** Parameters:	Schema schema	- schema to dump
**		FILE*  file	- file to dump to
** Returns:	void
** Description:	Dump a schema to a file.
**
** Notes:	This function is provided for debugging purposes.
*/

#if 0
void
SCHEMAdump(Schema schema, FILE* file)
{
    fprintf(file, "SCHEMA %s:\n", SCHEMAget_name(schema));
    SCOPEdump(schema, file);
    fprintf(file, "END SCHEMA %s\n\n", SCHEMAget_name(schema));
}
#endif

#if 0
SYMBOLprint(Symbol *s)
{
	printf("%s (r:%d #:%d f:%s)\n",s->name,s->resolved,s->line,s->filename);
}
#endif

void
SCHEMAadd_reference(Schema cur_schema, Symbol *ref_schema, Symbol *old, Symbol *nnew)
{
	Rename *r = REN_new();
	r->schema_sym = ref_schema;
	r->old = old;
	r->nnew = nnew;
	r->rename_type = ref;

	if (!cur_schema->u.schema->reflist)
	    cur_schema->u.schema->reflist = LISTcreate();
	LISTadd(cur_schema->u.schema->reflist, (Generic)r);
}

void
SCHEMAadd_use(Schema cur_schema, Symbol *ref_schema, Symbol *old, Symbol *nnew)
{
	Rename *r = REN_new();
	r->schema_sym = ref_schema;
	r->old = old;
	r->nnew = nnew;
	r->rename_type = use;

	if (!cur_schema->u.schema->uselist)
	    cur_schema->u.schema->uselist = LISTcreate();
	LISTadd(cur_schema->u.schema->uselist, (Generic)r);
}

void
SCHEMAdefine_reference(Schema schema, Rename *r)
{
    Rename *old = 0;
    char *name = (r->nnew ? r->nnew : r->old)->name;

    if (!schema->u.schema->refdict) {
	schema->u.schema->refdict = DICTcreate(20);
    } else {
	old = (Rename*)DICTlookup(schema->u.schema->refdict, name);
    }
    if (!old || (DICT_type != OBJ_RENAME) || (old->object != r->object)) {
	DICTdefine(schema->u.schema->refdict, name,
		   (Generic)r, r->old, OBJ_RENAME);
    }
}

void
SCHEMAdefine_use(Schema schema, Rename *r)
{
    Rename *old = 0;
    char *name = (r->nnew ? r->nnew : r->old)->name;

    if (!schema->u.schema->usedict) {
	schema->u.schema->usedict = DICTcreate(20);
    } else {
	old = (Rename*)DICTlookup(schema->u.schema->usedict, name);
    }
    if (!old || (DICT_type != OBJ_RENAME) || (old->object != r->object)) {
	DICTdefine(schema->u.schema->usedict, name,
		   (Generic)r, r->old, OBJ_RENAME);
    }
}

static void
SCHEMA_get_entities_use(Scope scope,Linked_List result)
{
	DictionaryEntry	de;
	Rename *rename;

	if (scope->search_id == __SCOPE_search_id) return;
	scope->search_id = __SCOPE_search_id;

	/* fully USE'd schema */
	LISTdo(scope->u.schema->use_schemas,schema,Schema )
		SCOPE_get_entities(schema,result);
		SCHEMA_get_entities_use(schema,result);
	LISTod

	/* partially USE'd schema */
	if (scope->u.schema->usedict) {
		DICTdo_init(scope->u.schema->usedict,&de);
		while (0 != (rename = (Rename *)DICTdo(&de))) {
			LISTadd(result,rename->object);
		}
	}
}

/* return use'd entities */
Linked_List
SCHEMAget_entities_use(Scope scope)
{
	Linked_List result = LISTcreate();

	__SCOPE_search_id++;
	ENTITY_MARK++;

	SCHEMA_get_entities_use(scope,result);
	return(result);
}

/* return ref'd entities */
static
void
SCHEMA_get_entities_ref(Scope scope,Linked_List result)
{
	Rename *rename;
	DictionaryEntry	de;

	if (scope->search_id == __SCOPE_search_id) return;
	scope->search_id = __SCOPE_search_id;

	ENTITY_MARK++;

	/* fully REF'd schema */
	LISTdo(scope->u.schema->ref_schemas,schema,Schema )
		SCOPE_get_entities(schema,result);
		/* don't go down remote schema's ref_schemas */
	LISTod

	/* partially REF'd schema */
	DICTdo_init(scope->u.schema->refdict,&de);
	while (0 != (rename = (Rename *)DICTdo(&de))) {
		if (DICT_type == OBJ_ENTITY) {
			LISTadd(result,rename->object);
		}
	}
}

/* return ref'd entities */
Linked_List 
SCHEMAget_entities_ref(Scope scope)
{
	Linked_List result = LISTcreate();

	__SCOPE_search_id++;
	ENTITY_MARK++;

	result = LISTcreate();
	SCHEMA_get_entities_ref(scope,result);
	return(result);
}

/* look up an attribute reference */
/* if strict false, anything can be returned, not just attributes */
Variable
VARfind(Scope scope,char *name, int strict)
{
	Variable result;

	/* first look up locally */
	switch (scope->type) {
	case OBJ_ENTITY:
	    result = ENTITYfind_inherited_attribute(scope, name, 0);
	    if (result) {
		if (strict && (DICT_type != OBJ_VARIABLE)) {
		    printf("press ^C now to trap to debugger\n");
		    pause();
		}
		return result;
	    }
	    break;
	case OBJ_INCREMENT:
	case OBJ_QUERY:
	case OBJ_ALIAS:
	    result = (Variable)DICTlookup(scope->symbol_table,name);
	    if (result) {
		if (strict && (DICT_type != OBJ_VARIABLE)) {
		    printf("press ^C now to trap to debugger\n");
		    pause();
		}
		return result;
	    }
	    return(VARfind(scope->superscope,name,strict));
	}
	return 0;
}

#if 0
/* look up an attribute reference */
/* if strict false, anything can be returned, not just attributes */
Variable
VARfind(Scope scope,char *name, int strict)
{
	Variable result;

	/* first look up locally */
	result = (Variable)DICTlookup(scope->symbol_table,name);
	if (result) {
		if (strict && (DICT_type != OBJ_VARIABLE)) {
			printf("press ^C now to trap to debugger\n");
			pause();
		}
		return result;
	}
	switch (scope->type) {
	case OBJ_ENTITY:
		LISTdo(scope->u.entity->supertypes,e,Entity)
			result = VARfind(e,name,strict);
			if (result) return(result);
		LISTod;
		break;
	case OBJ_INCREMENT:
	case OBJ_QUERY:
	case OBJ_ALIAS:
		return(VARfind(scope->superscope,name,strict));
	}
	return 0;
}
#endif
