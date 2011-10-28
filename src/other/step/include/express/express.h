#ifndef EXPRESS_H
#define EXPRESS_H

/* $Id: express.h,v 1.13 1997/01/21 19:16:42 dar Exp $ */

/*
 * Express package manager.
 *
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: express.h,v $
 * Revision 1.13  1997/01/21 19:16:42  dar
 * made C++ compatible
 *
 * Revision 1.12  1995/04/05  15:09:49  clark
 * CADDETC preval
 *
 * Revision 1.11  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.10  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.9  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.7  1993/02/16  03:19:56  libes
 * added unwriteable error
 *
 * Revision 1.6  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/09/16  18:20:10  libes
 * changed user-visible names
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 * 
 */

/*************/
/* constants */
/*************/

#define EXPRESS_NULL	(struct Scope_ *)0

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include <string.h>
#include "scope.h"
#include "type.h"
#include "variable.h"
#include "expr.h"
#include "entity.h"
#include "caseitem.h"
#include "stmt.h"
#include "alg.h"
#include "schema.h"
#include "lexact.h"
#include "dict.h"

/************/
/* typedefs */
/************/

typedef struct Scope_ *Express;

/****************/
/* modules used */
/****************/

/***************************/
/* hidden type definitions */
/***************************/

struct Express_ {
	FILE *file;
	char *filename;
	char *basename;	/* name of file but without directory or .exp suffix */
};

/********************/
/* global variables */
/********************/

extern Linked_List EXPRESS_path;
extern int EXPRESSpass;

extern void (*EXPRESSinit_args) PROTO((int, char**));
extern void (*EXPRESSinit_parse) PROTO((void));
extern int  (*EXPRESSfail) PROTO((Express));
extern int  (*EXPRESSsucceed) PROTO((Express));
extern void (*EXPRESSbackend) PROTO((Express));
extern char  *EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];	/* initialized elsewhere */
extern int  (*EXPRESSgetopt) PROTO((int,char *));
extern int    EXPRESSignore_duplicate_schemas;

extern Dictionary EXPRESSbuiltins;	/* procedures/functions */

extern Error ERROR_bail_out;
extern Error ERROR_syntax;
extern Error ERROR_unlabelled_param_type;
extern Error ERROR_file_unreadable;
extern Error ERROR_file_unwriteable;

extern struct Scope_ *FUNC_NVL;
extern struct Scope_ *FUNC_USEDIN;

extern char *KW_ABS;
extern char *KW_ABSTRACT;
extern char *KW_ACOS;
extern char *KW_AGGREGATE;
extern char *KW_ALIAS;
extern char *KW_AND;
extern char *KW_ANDOR;
extern char *KW_ARRAY;
extern char *KW_AS;
extern char *KW_ASIN;
extern char *KW_ATAN;
extern char *KW_BAG;
extern char *KW_BEGIN;
extern char *KW_BINARY;
extern char *KW_BLENGTH;
extern char *KW_BOOLEAN;
extern char *KW_BY;
extern char *KW_CASE;
extern char *KW_CONST_E;
extern char *KW_CONSTANT;
extern char *KW_CONTEXT;
extern char *KW_COS;
extern char *KW_DERIVE;
extern char *KW_DIV;
extern char *KW_ELSE;
extern char *KW_END;
extern char *KW_END_ALIAS;
extern char *KW_END_CASE;
extern char *KW_END_CONSTANT;
extern char *KW_END_CONTEXT;
extern char *KW_END_ENTITY;
extern char *KW_END_FUNCTION;
extern char *KW_END_IF;
extern char *KW_END_LOCAL;
extern char *KW_END_MODEL;
extern char *KW_END_PROCEDURE;
extern char *KW_END_REPEAT;
extern char *KW_END_RULE;
extern char *KW_END_SCHEMA;
extern char *KW_END_TYPE;
extern char *KW_ENTITY;
extern char *KW_ENUMERATION;
extern char *KW_ESCAPE;
extern char *KW_EXISTS;
extern char *KW_EXP;
extern char *KW_FALSE;
extern char *KW_FIXED;
extern char *KW_FOR;
extern char *KW_FORMAT;
extern char *KW_FROM;
extern char *KW_FUNCTION;
extern char *KW_GENERIC;
extern char *KW_HIBOUND;
extern char *KW_HIINDEX;
extern char *KW_IF;
extern char *KW_IN;
extern char *KW_INCLUDE;
extern char *KW_INSERT;
extern char *KW_INTEGER;
extern char *KW_INVERSE;
extern char *KW_LENGTH;
extern char *KW_LIKE;
extern char *KW_LIST;
extern char *KW_LOBOUND;
extern char *KW_LOCAL;
extern char *KW_LOG;
extern char *KW_LOG10;
extern char *KW_LOG2;
extern char *KW_LOGICAL;
extern char *KW_LOINDEX;
extern char *KW_MOD;
extern char *KW_MODEL;
extern char *KW_NOT;
extern char *KW_NUMBER;
extern char *KW_NVL;
extern char *KW_ODD;
extern char *KW_OF;
extern char *KW_ONEOF;
extern char *KW_OPTIONAL;
extern char *KW_OR;
extern char *KW_OTHERWISE;
extern char *KW_PI;
extern char *KW_PROCEDURE;
extern char *KW_QUERY;
extern char *KW_REAL;
extern char *KW_REFERENCE;
extern char *KW_REMOVE;
extern char *KW_REPEAT;
extern char *KW_RETURN;
extern char *KW_ROLESOF;
extern char *KW_RULE;
extern char *KW_SCHEMA;
extern char *KW_SELECT;
extern char *KW_SELF;
extern char *KW_SET;
extern char *KW_SIN;
extern char *KW_SIZEOF;
extern char *KW_SKIP;
extern char *KW_SQRT;
extern char *KW_STRING;
extern char *KW_SUBTYPE;
extern char *KW_SUPERTYPE;
extern char *KW_TAN;
extern char *KW_THEN;
extern char *KW_TO;
extern char *KW_TRUE;
extern char *KW_TYPE;
extern char *KW_TYPEOF;
extern char *KW_UNIQUE;
extern char *KW_UNKNOWN;
extern char *KW_UNTIL;
extern char *KW_USE;
extern char *KW_USEDIN;
extern char *KW_VALUE;
extern char *KW_VALUE_IN;
extern char *KW_VALUE_UNIQUE;
extern char *KW_VAR;
extern char *KW_WHERE;
extern char *KW_WHILE;
extern char *KW_XOR;

/******************************/
/* macro function definitions */
/******************************/

#define EXPRESSget_basename(e)		((e)->u.express->basename)
#define EXPRESSget_filename(e)		((e)->u.express->filename)
#define EXPRESSput_basename(e,n)	((e)->u.express->basename = (n))
#define EXPRESSput_filename(e,n)	((e)->u.express->filename = (n))

/***********************/
/* function prototypes */
/***********************/

extern Express 		EXPRESScreate PROTO((void));
extern void 		EXPRESSparse PROTO((Express,FILE *,char *));
extern void		EXPRESSinitialize PROTO((void));
extern void	 	EXPRESSresolve PROTO((Express));
extern char *		EXPRESSversion PROTO((void));
extern int		EXPRESS_fail PROTO((Express));
extern int		EXPRESS_succeed PROTO((Express));
extern void		EXPRESSinit_init PROTO((void));
extern void             build_complex(Express);

#endif /*EXPRESS_H*/
