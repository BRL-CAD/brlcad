#ifndef SCHEMA_H
#define SCHEMA_H

/* $Id: schema.h,v 1.11 1997/02/25 19:49:01 dar Exp $ */

/************************************************************************
** Module:	Schema
** Description:	This module implements the Schema abstraction, which
**	basically amounts to a named symbol table.
** Constants:
**	SCHEMA_NULL	- the null schema
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: schema.h,v $
 * Revision 1.11  1997/02/25 19:49:01  dar
 * added Rename::userdata
 *
 * Revision 1.10  1997/01/21  19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.9  1996/02/14  22:22:24  sauderd
 * Included scope.h more than once so I removed the 2nd one.
 *
 * Revision 1.8  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.7  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.6  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/01/19  22:16:43  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/27  23:42:33  libes
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

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include "symbol.h"
#include "scope.h"

/************/
/* typedefs */
/************/

/*typedef struct Scope *Schema;*/

/****************/
/* modules used */
/****************/

#include "alg.h"
#include "dict.h"
#include "entity.h"
#include "linklist.h"

/***************************/
/* hidden type definitions */
/***************************/

enum rename_type { use, ref };

typedef struct Rename {
	struct Symbol_ *schema_sym;
	Schema schema;
	struct Symbol_ *old;
	struct Symbol_ *nnew;
	Generic object;		/* once object has been looked up */
	char type;		/* drat, need to remember this */
			/* once renames have been resolved to avoid */
			/* looking them up in the dictionary again */
	enum rename_type rename_type;
	int userdata;   /* generic for user */
} Rename;

struct Schema_ {
	Linked_List rules;
#if 0
	struct Express_	*express;
#endif
	Linked_List reflist;
	Linked_List uselist;
	/* dictionarys into which are entered renames for each specific */
	/* object specified in a rename clause (even if it uses the same */
	/* name */
	Dictionary refdict;
	Dictionary usedict;
	/* lists of schemas that are fully ref/use'd */
	/* entries can be 0 if schemas weren't found during RENAMEresolve */
	Linked_List use_schemas;
	Linked_List ref_schemas;
};

/********************/
/* global variables */
/********************/

#ifdef SCHEMA_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif /* SCHEMA_C */

GLOBAL struct freelist_head REN_fl;
GLOBAL struct freelist_head SCOPE_fl;
GLOBAL struct freelist_head SCHEMA_fl;

GLOBAL int	__SCOPE_search_id		INITIALLY(0);

#undef GLOBAL
#undef INITIALLY

/******************************/
/* macro function definitions */
/******************************/

#define SCHEMAget_name(schema)		SCOPEget_name(schema)
#define SCHEMAget_symbol(schema)	SCOPEget_symbol(schema)

#define REN_new()	(struct Rename *)MEM_new(&REN_fl)
#define REN_destroy(x)	MEM_destroy(&REN_fl,(Freelist *)(Generic)x)
#define SCOPE_new()	(struct Scope_ *)MEM_new(&SCOPE_fl)
#define SCOPE_destroy(x)	MEM_destroy(&SCOPE_fl,(Freelist *)(Generic)x)
#define SCHEMA_new()	(struct Schema_ *)MEM_new(&SCHEMA_fl)
#define SCHEMA_destroy(x)	MEM_destroy(&SCHEMA_fl,(Freelist *)(Generic)x)

/* the following is simply to make the resulting code easier to read */
/* otherwise, you'd see "entity->superscope" even when you KNOW */
/* it is a schema */
#define ENTITYget_schema(e)		(e)->superscope

/***********************/
/* function prototypes */
/***********************/

extern Variable VARfind PROTO((Scope,char *,int));
extern Schema 	SCHEMAcreate PROTO((void));
extern void	SCHEMAinitialize PROTO((void));
extern void	SCHEMAadd_use PROTO((Schema, Symbol*, Symbol*, Symbol*));
extern void	SCHEMAadd_reference PROTO((Schema, Symbol*, Symbol*, Symbol*));
extern void	SCHEMAdefine_use PROTO((Schema, Rename*));
extern void	SCHEMAdefine_reference PROTO((Schema, Rename*));
extern Generic	SCHEMAfind PROTO((Schema,char *name,int search_refs));
extern Scope	SCOPEcreate PROTO((char));
extern Scope	SCOPEcreate_tiny PROTO((char));
extern Scope	SCOPEcreate_nostab PROTO((char));
extern Linked_List SCHEMAget_entities_use PROTO((Scope));
extern Linked_List SCHEMAget_entities_ref PROTO((Scope));

/********************/
/* inline functions */
/********************/

#if supports_inline_functions || defined(SCHEMA_C)
#endif /* supports_inline_functions || defined(SCHEMA_C) */

#endif /*  SCHEMA_H */


