#ifndef EXPRESS_H
#define EXPRESS_H

/*
 * Express package manager.
 *
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log$
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
    char * basename; /**< name of file but without directory or .exp suffix */
};

/********************/
/* global variables */
/********************/

extern STEPCODE_EXPRESS_EXPORT Linked_List EXPRESS_path;
extern STEPCODE_EXPRESS_EXPORT int EXPRESSpass;

extern STEPCODE_EXPRESS_EXPORT void ( *EXPRESSinit_args ) PROTO( ( int, char ** ) );
extern STEPCODE_EXPRESS_EXPORT void ( *EXPRESSinit_parse ) PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT int ( *EXPRESSfail ) PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT int ( *EXPRESSsucceed ) PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT void ( *EXPRESSbackend ) PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT char * EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];  /* initialized elsewhere */
extern STEPCODE_EXPRESS_EXPORT int ( *EXPRESSgetopt ) PROTO( ( int, char * ) );
extern STEPCODE_EXPRESS_EXPORT bool    EXPRESSignore_duplicate_schemas;

extern STEPCODE_EXPRESS_EXPORT Dictionary EXPRESSbuiltins;  /* procedures/functions */

extern STEPCODE_EXPRESS_EXPORT Error ERROR_bail_out;
extern STEPCODE_EXPRESS_EXPORT Error ERROR_syntax;
extern STEPCODE_EXPRESS_EXPORT Error ERROR_unlabelled_param_type;
extern STEPCODE_EXPRESS_EXPORT Error ERROR_file_unreadable;
extern STEPCODE_EXPRESS_EXPORT Error ERROR_file_unwriteable;
extern STEPCODE_EXPRESS_EXPORT Error ERROR_warn_unsupported_lang_feat;

extern STEPCODE_EXPRESS_EXPORT struct Scope_ * FUNC_NVL;
extern STEPCODE_EXPRESS_EXPORT struct Scope_ * FUNC_USEDIN;

extern STEPCODE_EXPRESS_EXPORT char * KW_ABS;
extern STEPCODE_EXPRESS_EXPORT char * KW_ABSTRACT;
extern STEPCODE_EXPRESS_EXPORT char * KW_ACOS;
extern STEPCODE_EXPRESS_EXPORT char * KW_AGGREGATE;
extern STEPCODE_EXPRESS_EXPORT char * KW_ALIAS;
extern STEPCODE_EXPRESS_EXPORT char * KW_AND;
extern STEPCODE_EXPRESS_EXPORT char * KW_ANDOR;
extern STEPCODE_EXPRESS_EXPORT char * KW_ARRAY;
extern STEPCODE_EXPRESS_EXPORT char * KW_AS;
extern STEPCODE_EXPRESS_EXPORT char * KW_ASIN;
extern STEPCODE_EXPRESS_EXPORT char * KW_ATAN;
extern STEPCODE_EXPRESS_EXPORT char * KW_BAG;
extern STEPCODE_EXPRESS_EXPORT char * KW_BEGIN;
extern STEPCODE_EXPRESS_EXPORT char * KW_BINARY;
extern STEPCODE_EXPRESS_EXPORT char * KW_BLENGTH;
extern STEPCODE_EXPRESS_EXPORT char * KW_BOOLEAN;
extern STEPCODE_EXPRESS_EXPORT char * KW_BY;
extern STEPCODE_EXPRESS_EXPORT char * KW_CASE;
extern STEPCODE_EXPRESS_EXPORT char * KW_CONST_E;
extern STEPCODE_EXPRESS_EXPORT char * KW_CONSTANT;
extern STEPCODE_EXPRESS_EXPORT char * KW_CONTEXT;
extern STEPCODE_EXPRESS_EXPORT char * KW_COS;
extern STEPCODE_EXPRESS_EXPORT char * KW_DERIVE;
extern STEPCODE_EXPRESS_EXPORT char * KW_DIV;
extern STEPCODE_EXPRESS_EXPORT char * KW_ELSE;
extern STEPCODE_EXPRESS_EXPORT char * KW_END;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_ALIAS;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_CASE;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_CONSTANT;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_CONTEXT;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_ENTITY;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_FUNCTION;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_IF;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_LOCAL;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_MODEL;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_PROCEDURE;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_REPEAT;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_RULE;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_SCHEMA;
extern STEPCODE_EXPRESS_EXPORT char * KW_END_TYPE;
extern STEPCODE_EXPRESS_EXPORT char * KW_ENTITY;
extern STEPCODE_EXPRESS_EXPORT char * KW_ENUMERATION;
extern STEPCODE_EXPRESS_EXPORT char * KW_ESCAPE;
extern STEPCODE_EXPRESS_EXPORT char * KW_EXISTS;
extern STEPCODE_EXPRESS_EXPORT char * KW_EXP;
extern STEPCODE_EXPRESS_EXPORT char * KW_FALSE;
extern STEPCODE_EXPRESS_EXPORT char * KW_FIXED;
extern STEPCODE_EXPRESS_EXPORT char * KW_FOR;
extern STEPCODE_EXPRESS_EXPORT char * KW_FORMAT;
extern STEPCODE_EXPRESS_EXPORT char * KW_FROM;
extern STEPCODE_EXPRESS_EXPORT char * KW_FUNCTION;
extern STEPCODE_EXPRESS_EXPORT char * KW_GENERIC;
extern STEPCODE_EXPRESS_EXPORT char * KW_HIBOUND;
extern STEPCODE_EXPRESS_EXPORT char * KW_HIINDEX;
extern STEPCODE_EXPRESS_EXPORT char * KW_IF;
extern STEPCODE_EXPRESS_EXPORT char * KW_IN;
extern STEPCODE_EXPRESS_EXPORT char * KW_INCLUDE;
extern STEPCODE_EXPRESS_EXPORT char * KW_INSERT;
extern STEPCODE_EXPRESS_EXPORT char * KW_INTEGER;
extern STEPCODE_EXPRESS_EXPORT char * KW_INVERSE;
extern STEPCODE_EXPRESS_EXPORT char * KW_LENGTH;
extern STEPCODE_EXPRESS_EXPORT char * KW_LIKE;
extern STEPCODE_EXPRESS_EXPORT char * KW_LIST;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOBOUND;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOCAL;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOG;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOG10;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOG2;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOGICAL;
extern STEPCODE_EXPRESS_EXPORT char * KW_LOINDEX;
extern STEPCODE_EXPRESS_EXPORT char * KW_MOD;
extern STEPCODE_EXPRESS_EXPORT char * KW_MODEL;
extern STEPCODE_EXPRESS_EXPORT char * KW_NOT;
extern STEPCODE_EXPRESS_EXPORT char * KW_NUMBER;
extern STEPCODE_EXPRESS_EXPORT char * KW_NVL;
extern STEPCODE_EXPRESS_EXPORT char * KW_ODD;
extern STEPCODE_EXPRESS_EXPORT char * KW_OF;
extern STEPCODE_EXPRESS_EXPORT char * KW_ONEOF;
extern STEPCODE_EXPRESS_EXPORT char * KW_OPTIONAL;
extern STEPCODE_EXPRESS_EXPORT char * KW_OR;
extern STEPCODE_EXPRESS_EXPORT char * KW_OTHERWISE;
extern STEPCODE_EXPRESS_EXPORT char * KW_PI;
extern STEPCODE_EXPRESS_EXPORT char * KW_PROCEDURE;
extern STEPCODE_EXPRESS_EXPORT char * KW_QUERY;
extern STEPCODE_EXPRESS_EXPORT char * KW_REAL;
extern STEPCODE_EXPRESS_EXPORT char * KW_REFERENCE;
extern STEPCODE_EXPRESS_EXPORT char * KW_REMOVE;
extern STEPCODE_EXPRESS_EXPORT char * KW_REPEAT;
extern STEPCODE_EXPRESS_EXPORT char * KW_RETURN;
extern STEPCODE_EXPRESS_EXPORT char * KW_ROLESOF;
extern STEPCODE_EXPRESS_EXPORT char * KW_RULE;
extern STEPCODE_EXPRESS_EXPORT char * KW_SCHEMA;
extern STEPCODE_EXPRESS_EXPORT char * KW_SELECT;
extern STEPCODE_EXPRESS_EXPORT char * KW_SELF;
extern STEPCODE_EXPRESS_EXPORT char * KW_SET;
extern STEPCODE_EXPRESS_EXPORT char * KW_SIN;
extern STEPCODE_EXPRESS_EXPORT char * KW_SIZEOF;
extern STEPCODE_EXPRESS_EXPORT char * KW_SKIP;
extern STEPCODE_EXPRESS_EXPORT char * KW_SQRT;
extern STEPCODE_EXPRESS_EXPORT char * KW_STRING;
extern STEPCODE_EXPRESS_EXPORT char * KW_SUBTYPE;
extern STEPCODE_EXPRESS_EXPORT char * KW_SUPERTYPE;
extern STEPCODE_EXPRESS_EXPORT char * KW_TAN;
extern STEPCODE_EXPRESS_EXPORT char * KW_THEN;
extern STEPCODE_EXPRESS_EXPORT char * KW_TO;
extern STEPCODE_EXPRESS_EXPORT char * KW_TRUE;
extern STEPCODE_EXPRESS_EXPORT char * KW_TYPE;
extern STEPCODE_EXPRESS_EXPORT char * KW_TYPEOF;
extern STEPCODE_EXPRESS_EXPORT char * KW_UNIQUE;
extern STEPCODE_EXPRESS_EXPORT char * KW_UNKNOWN;
extern STEPCODE_EXPRESS_EXPORT char * KW_UNTIL;
extern STEPCODE_EXPRESS_EXPORT char * KW_USE;
extern STEPCODE_EXPRESS_EXPORT char * KW_USEDIN;
extern STEPCODE_EXPRESS_EXPORT char * KW_VALUE;
extern STEPCODE_EXPRESS_EXPORT char * KW_VALUE_IN;
extern STEPCODE_EXPRESS_EXPORT char * KW_VALUE_UNIQUE;
extern STEPCODE_EXPRESS_EXPORT char * KW_VAR;
extern STEPCODE_EXPRESS_EXPORT char * KW_WHERE;
extern STEPCODE_EXPRESS_EXPORT char * KW_WHILE;
extern STEPCODE_EXPRESS_EXPORT char * KW_XOR;

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

extern STEPCODE_EXPRESS_EXPORT Express  EXPRESScreate     PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESSdestroy    PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESSparse      PROTO( ( Express, FILE *, char * ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESSinitialize PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESScleanup    PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESSresolve    PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT char  *  EXPRESSversion    PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT int      EXPRESS_fail      PROTO( ( Express ) );
extern STEPCODE_EXPRESS_EXPORT int      EXPRESS_succeed   PROTO( ( Express ) );
extern                   void     EXPRESSinit_init  PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT void     EXPRESSusage( int _exit ); ///< exit unless _exit is non-zero
extern STEPCODE_EXPRESS_EXPORT void     build_complex( Express );

#endif /*EXPRESS_H*/
