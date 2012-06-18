#ifndef EXPRESS_H
#define EXPRESS_H

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

#define EXPRESS_NULL    (struct Scope_ *)0

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"   /* get basic definitions */
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

typedef struct Scope_ * Express;

/****************/
/* modules used */
/****************/

/***************************/
/* hidden type definitions */
/***************************/

struct Express_ {
    FILE * file;
    char * filename;
    char * basename; /* name of file but without directory or .exp suffix */
};

/********************/
/* global variables */
/********************/

extern SCL_EXPRESS_EXPORT Linked_List EXPRESS_path;
extern SCL_EXPRESS_EXPORT int EXPRESSpass;

extern SCL_EXPRESS_EXPORT void ( *EXPRESSinit_args ) PROTO( ( int, char ** ) );
extern SCL_EXPRESS_EXPORT void ( *EXPRESSinit_parse ) PROTO( ( void ) );
extern SCL_EXPRESS_EXPORT int ( *EXPRESSfail ) PROTO( ( Express ) );
extern SCL_EXPRESS_EXPORT int ( *EXPRESSsucceed ) PROTO( ( Express ) );
extern SCL_EXPRESS_EXPORT void ( *EXPRESSbackend ) PROTO( ( Express ) );
extern SCL_EXPRESS_EXPORT char * EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];  /* initialized elsewhere */
extern SCL_EXPRESS_EXPORT int ( *EXPRESSgetopt ) PROTO( ( int, char * ) );
extern SCL_EXPRESS_EXPORT bool    EXPRESSignore_duplicate_schemas;

extern SCL_EXPRESS_EXPORT Dictionary EXPRESSbuiltins;  /* procedures/functions */

extern SCL_EXPRESS_EXPORT Error ERROR_bail_out;
extern SCL_EXPRESS_EXPORT Error ERROR_syntax;
extern SCL_EXPRESS_EXPORT Error ERROR_unlabelled_param_type;
extern SCL_EXPRESS_EXPORT Error ERROR_file_unreadable;
extern SCL_EXPRESS_EXPORT Error ERROR_file_unwriteable;
extern SCL_EXPRESS_EXPORT Error ERROR_warn_unsupported_lang_feat;

extern SCL_EXPRESS_EXPORT struct Scope_ * FUNC_NVL;
extern SCL_EXPRESS_EXPORT struct Scope_ * FUNC_USEDIN;

extern SCL_EXPRESS_EXPORT char * KW_ABS;
extern SCL_EXPRESS_EXPORT char * KW_ABSTRACT;
extern SCL_EXPRESS_EXPORT char * KW_ACOS;
extern SCL_EXPRESS_EXPORT char * KW_AGGREGATE;
extern SCL_EXPRESS_EXPORT char * KW_ALIAS;
extern SCL_EXPRESS_EXPORT char * KW_AND;
extern SCL_EXPRESS_EXPORT char * KW_ANDOR;
extern SCL_EXPRESS_EXPORT char * KW_ARRAY;
extern SCL_EXPRESS_EXPORT char * KW_AS;
extern SCL_EXPRESS_EXPORT char * KW_ASIN;
extern SCL_EXPRESS_EXPORT char * KW_ATAN;
extern SCL_EXPRESS_EXPORT char * KW_BAG;
extern SCL_EXPRESS_EXPORT char * KW_BEGIN;
extern SCL_EXPRESS_EXPORT char * KW_BINARY;
extern SCL_EXPRESS_EXPORT char * KW_BLENGTH;
extern SCL_EXPRESS_EXPORT char * KW_BOOLEAN;
extern SCL_EXPRESS_EXPORT char * KW_BY;
extern SCL_EXPRESS_EXPORT char * KW_CASE;
extern SCL_EXPRESS_EXPORT char * KW_CONST_E;
extern SCL_EXPRESS_EXPORT char * KW_CONSTANT;
extern SCL_EXPRESS_EXPORT char * KW_CONTEXT;
extern SCL_EXPRESS_EXPORT char * KW_COS;
extern SCL_EXPRESS_EXPORT char * KW_DERIVE;
extern SCL_EXPRESS_EXPORT char * KW_DIV;
extern SCL_EXPRESS_EXPORT char * KW_ELSE;
extern SCL_EXPRESS_EXPORT char * KW_END;
extern SCL_EXPRESS_EXPORT char * KW_END_ALIAS;
extern SCL_EXPRESS_EXPORT char * KW_END_CASE;
extern SCL_EXPRESS_EXPORT char * KW_END_CONSTANT;
extern SCL_EXPRESS_EXPORT char * KW_END_CONTEXT;
extern SCL_EXPRESS_EXPORT char * KW_END_ENTITY;
extern SCL_EXPRESS_EXPORT char * KW_END_FUNCTION;
extern SCL_EXPRESS_EXPORT char * KW_END_IF;
extern SCL_EXPRESS_EXPORT char * KW_END_LOCAL;
extern SCL_EXPRESS_EXPORT char * KW_END_MODEL;
extern SCL_EXPRESS_EXPORT char * KW_END_PROCEDURE;
extern SCL_EXPRESS_EXPORT char * KW_END_REPEAT;
extern SCL_EXPRESS_EXPORT char * KW_END_RULE;
extern SCL_EXPRESS_EXPORT char * KW_END_SCHEMA;
extern SCL_EXPRESS_EXPORT char * KW_END_TYPE;
extern SCL_EXPRESS_EXPORT char * KW_ENTITY;
extern SCL_EXPRESS_EXPORT char * KW_ENUMERATION;
extern SCL_EXPRESS_EXPORT char * KW_ESCAPE;
extern SCL_EXPRESS_EXPORT char * KW_EXISTS;
extern SCL_EXPRESS_EXPORT char * KW_EXP;
extern SCL_EXPRESS_EXPORT char * KW_FALSE;
extern SCL_EXPRESS_EXPORT char * KW_FIXED;
extern SCL_EXPRESS_EXPORT char * KW_FOR;
extern SCL_EXPRESS_EXPORT char * KW_FORMAT;
extern SCL_EXPRESS_EXPORT char * KW_FROM;
extern SCL_EXPRESS_EXPORT char * KW_FUNCTION;
extern SCL_EXPRESS_EXPORT char * KW_GENERIC;
extern SCL_EXPRESS_EXPORT char * KW_HIBOUND;
extern SCL_EXPRESS_EXPORT char * KW_HIINDEX;
extern SCL_EXPRESS_EXPORT char * KW_IF;
extern SCL_EXPRESS_EXPORT char * KW_IN;
extern SCL_EXPRESS_EXPORT char * KW_INCLUDE;
extern SCL_EXPRESS_EXPORT char * KW_INSERT;
extern SCL_EXPRESS_EXPORT char * KW_INTEGER;
extern SCL_EXPRESS_EXPORT char * KW_INVERSE;
extern SCL_EXPRESS_EXPORT char * KW_LENGTH;
extern SCL_EXPRESS_EXPORT char * KW_LIKE;
extern SCL_EXPRESS_EXPORT char * KW_LIST;
extern SCL_EXPRESS_EXPORT char * KW_LOBOUND;
extern SCL_EXPRESS_EXPORT char * KW_LOCAL;
extern SCL_EXPRESS_EXPORT char * KW_LOG;
extern SCL_EXPRESS_EXPORT char * KW_LOG10;
extern SCL_EXPRESS_EXPORT char * KW_LOG2;
extern SCL_EXPRESS_EXPORT char * KW_LOGICAL;
extern SCL_EXPRESS_EXPORT char * KW_LOINDEX;
extern SCL_EXPRESS_EXPORT char * KW_MOD;
extern SCL_EXPRESS_EXPORT char * KW_MODEL;
extern SCL_EXPRESS_EXPORT char * KW_NOT;
extern SCL_EXPRESS_EXPORT char * KW_NUMBER;
extern SCL_EXPRESS_EXPORT char * KW_NVL;
extern SCL_EXPRESS_EXPORT char * KW_ODD;
extern SCL_EXPRESS_EXPORT char * KW_OF;
extern SCL_EXPRESS_EXPORT char * KW_ONEOF;
extern SCL_EXPRESS_EXPORT char * KW_OPTIONAL;
extern SCL_EXPRESS_EXPORT char * KW_OR;
extern SCL_EXPRESS_EXPORT char * KW_OTHERWISE;
extern SCL_EXPRESS_EXPORT char * KW_PI;
extern SCL_EXPRESS_EXPORT char * KW_PROCEDURE;
extern SCL_EXPRESS_EXPORT char * KW_QUERY;
extern SCL_EXPRESS_EXPORT char * KW_REAL;
extern SCL_EXPRESS_EXPORT char * KW_REFERENCE;
extern SCL_EXPRESS_EXPORT char * KW_REMOVE;
extern SCL_EXPRESS_EXPORT char * KW_REPEAT;
extern SCL_EXPRESS_EXPORT char * KW_RETURN;
extern SCL_EXPRESS_EXPORT char * KW_ROLESOF;
extern SCL_EXPRESS_EXPORT char * KW_RULE;
extern SCL_EXPRESS_EXPORT char * KW_SCHEMA;
extern SCL_EXPRESS_EXPORT char * KW_SELECT;
extern SCL_EXPRESS_EXPORT char * KW_SELF;
extern SCL_EXPRESS_EXPORT char * KW_SET;
extern SCL_EXPRESS_EXPORT char * KW_SIN;
extern SCL_EXPRESS_EXPORT char * KW_SIZEOF;
extern SCL_EXPRESS_EXPORT char * KW_SKIP;
extern SCL_EXPRESS_EXPORT char * KW_SQRT;
extern SCL_EXPRESS_EXPORT char * KW_STRING;
extern SCL_EXPRESS_EXPORT char * KW_SUBTYPE;
extern SCL_EXPRESS_EXPORT char * KW_SUPERTYPE;
extern SCL_EXPRESS_EXPORT char * KW_TAN;
extern SCL_EXPRESS_EXPORT char * KW_THEN;
extern SCL_EXPRESS_EXPORT char * KW_TO;
extern SCL_EXPRESS_EXPORT char * KW_TRUE;
extern SCL_EXPRESS_EXPORT char * KW_TYPE;
extern SCL_EXPRESS_EXPORT char * KW_TYPEOF;
extern SCL_EXPRESS_EXPORT char * KW_UNIQUE;
extern SCL_EXPRESS_EXPORT char * KW_UNKNOWN;
extern SCL_EXPRESS_EXPORT char * KW_UNTIL;
extern SCL_EXPRESS_EXPORT char * KW_USE;
extern SCL_EXPRESS_EXPORT char * KW_USEDIN;
extern SCL_EXPRESS_EXPORT char * KW_VALUE;
extern SCL_EXPRESS_EXPORT char * KW_VALUE_IN;
extern SCL_EXPRESS_EXPORT char * KW_VALUE_UNIQUE;
extern SCL_EXPRESS_EXPORT char * KW_VAR;
extern SCL_EXPRESS_EXPORT char * KW_WHERE;
extern SCL_EXPRESS_EXPORT char * KW_WHILE;
extern SCL_EXPRESS_EXPORT char * KW_XOR;

/******************************/
/* macro function definitions */
/******************************/

#define EXPRESSget_basename(e)      ((e)->u.express->basename)
#define EXPRESSget_filename(e)      ((e)->u.express->filename)
#define EXPRESSput_basename(e,n)    ((e)->u.express->basename = (n))
#define EXPRESSput_filename(e,n)    ((e)->u.express->filename = (n))

/***********************/
/* function prototypes */
/***********************/

extern SCL_EXPRESS_EXPORT Express  EXPRESScreate PROTO( ( void ) );
extern SCL_EXPRESS_EXPORT void     EXPRESSparse PROTO( ( Express, FILE *, char * ) );
extern SCL_EXPRESS_EXPORT void     EXPRESSinitialize PROTO( ( void ) );
extern SCL_EXPRESS_EXPORT void     EXPRESSresolve PROTO( ( Express ) );
extern SCL_EXPRESS_EXPORT char  *  EXPRESSversion PROTO( ( void ) );
extern SCL_EXPRESS_EXPORT int      EXPRESS_fail PROTO( ( Express ) );
extern SCL_EXPRESS_EXPORT int      EXPRESS_succeed PROTO( ( Express ) );
extern void     EXPRESSinit_init PROTO( ( void ) );
extern SCL_EXPRESS_EXPORT void     build_complex( Express );

#endif /*EXPRESS_H*/
