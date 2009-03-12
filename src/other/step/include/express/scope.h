#ifndef SCOPE_H
#define SCOPE_H

/* $Id: scope.h,v 1.10 1997/01/21 19:17:11 dar Exp $ */

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
 * $Log: scope.h,v $
 * Revision 1.10  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.9  1995/04/05  14:58:31  clark
 * CADDETC preval
 *
 * Revision 1.8  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.7  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/01/19  22:16:43  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define SCOPE_NULL	(Scope)0

/************/
/* typedefs */
/************/

/*typedef struct Scope *Scope;*/

/****************/
/* modules used */
/****************/

#include "type.h"
#include "variable.h"
#include "entity.h"
#include "express.h"
#include "alg.h"
#include "schema.h"

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include "dict.h"
#include "symbol.h"

/***************************/
/* hidden type definitions */
/***************************/

/* these allow one to give SCOPEfind hints to its search strategy */
#define SCOPE_FIND_TYPE		OBJ_TYPE_BITS
#define SCOPE_FIND_ENTITY	OBJ_ENTITY_BITS
#define SCOPE_FIND_FUNCTION	OBJ_FUNCTION_BITS
#define SCOPE_FIND_PROCEDURE	OBJ_PROCEDURE_BITS
#define SCOPE_FIND_VARIABLE	OBJ_VARIABLE_BITS
#define SCOPE_FIND_ANYTHING	OBJ_ANYTHING_BITS

struct Scope_ {
	Symbol		symbol;
	char		type;		/* see above */
	ClientData	clientData;	/* user may use this for any purpose */
	int		search_id;	/* key to avoid searching this scope twice */
	Dictionary	symbol_table;
	struct Scope_  *superscope;
	union {
/*		struct Constant *constant;*/
		struct Procedure_ *proc;
		struct Function_ *func;
		struct Rule_ *rule;
		struct Entity_ *entity;
		struct Schema_ *schema;
		struct Express_ *express;
		struct Increment_ *incr;
		struct TypeHead_ *type;
/* no, query owns a scope rather than scope owning a query */
/*		struct Query *query;*/
	} u;
	Linked_List where;		/* optional where clause */
};

/********************/
/* global variables */
/********************/

#ifdef SCOPE_C
#include "defstart.h"
#else
#include "decstart.h"
#endif /* SCOPE_C */
#include "de_end.h"

/******************************/
/* macro function definitions */
/******************************/

#define SCOPEis(scope)			((scope)->type)
#define SCOPEis_schema(scope)		((scope)->type == OBJ_SCHEMA)

#define SCOPEget_symbol(scope)		(&(scope)->symbol)
#define SCOPEput_name(scope,name)	((scope)->symbol.name = name)
#define SCOPEget_name(scope)		((scope)->symbol.name)
/* get_superscope's' for backward compat */
#define SCOPEget_superscope(scope)	((scope)->superscope)
#define SCOPEget_superscope(scope)	((scope)->superscope)
#define SCOPEget_resolved(scope)	((scope)->symbol.resolved)

#define SCOPEget_clientData(s)		((s)->clientData)
#define SCOPEput_clientData(s,d)	((s)->clientData = (d))


#define SCOPEdo_types(s,t,de) DICTdo_type_init(s->symbol_table,&de,OBJ_TYPE);{\
				Type t; \
			      while (0 != (t = (Type)DICTdo(&de))) {
#define SCOPEdo_entities(s,t,de) DICTdo_type_init(s->symbol_table,&de,OBJ_ENTITY); { \
				Entity t; \
			      while (0 != (t = (Entity)DICTdo(&de))) {
#define SCOPEod			}}

/***********************/
/* function prototypes */
/***********************/

extern struct Symbol_ *	SCOPE_get_symbol PROTO((Generic));
/*extern struct Scope *	SCOPEget_nearest_enclosing_entity PROTO((Scope));*/
extern void		SCOPE_get_entities PROTO((Scope,Linked_List));
extern Linked_List	SCOPEget_entities PROTO((Scope));
extern Linked_List	SCOPEget_entities_superclass_order PROTO((Scope));
extern Generic		SCOPEfind PROTO((Scope,char *,int));

#if 0
extern Linked_List	SCOPEget_types PROTO((Scope));
extern Linked_List	SCOPEget_variables PROTO((Scope));
extern Linked_List	SCOPEget_algorithms PROTO((Scope));
extern Linked_List	SCOPEget_constants PROTO((Scope));
#endif

/********************/
/* inline functions */
/********************/

#if supports_inline_functions || defined(SCOPE_C)

#endif /* supports_inline_functions || defined(SCOPE_C) */
#endif /* SCOPE_H */
