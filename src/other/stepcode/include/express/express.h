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
    char * basename; /**< name of file but without directory or .exp suffix */
};

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT Linked_List EXPRESS_path;
extern SC_EXPRESS_EXPORT int EXPRESSpass;

extern SC_EXPRESS_EXPORT void ( *EXPRESSinit_args ) PROTO( ( int, char ** ) );
extern SC_EXPRESS_EXPORT void ( *EXPRESSinit_parse ) PROTO( ( void ) );
extern SC_EXPRESS_EXPORT int ( *EXPRESSfail ) PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT int ( *EXPRESSsucceed ) PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT void ( *EXPRESSbackend ) PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT char * EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];  /* initialized elsewhere */
extern SC_EXPRESS_EXPORT int ( *EXPRESSgetopt ) PROTO( ( int, char * ) );
extern SC_EXPRESS_EXPORT bool    EXPRESSignore_duplicate_schemas;

extern SC_EXPRESS_EXPORT Dictionary EXPRESSbuiltins;  /* procedures/functions */

extern SC_EXPRESS_EXPORT Error ERROR_bail_out;
extern SC_EXPRESS_EXPORT Error ERROR_syntax;
extern SC_EXPRESS_EXPORT Error ERROR_unlabelled_param_type;
extern SC_EXPRESS_EXPORT Error ERROR_file_unreadable;
extern SC_EXPRESS_EXPORT Error ERROR_file_unwriteable;
extern SC_EXPRESS_EXPORT Error ERROR_warn_unsupported_lang_feat;

extern SC_EXPRESS_EXPORT struct Scope_ * FUNC_NVL;
extern SC_EXPRESS_EXPORT struct Scope_ * FUNC_USEDIN;

extern SC_EXPRESS_EXPORT char * KW_ABS;
extern SC_EXPRESS_EXPORT char * KW_ABSTRACT;
extern SC_EXPRESS_EXPORT char * KW_ACOS;
extern SC_EXPRESS_EXPORT char * KW_AGGREGATE;
extern SC_EXPRESS_EXPORT char * KW_ALIAS;
extern SC_EXPRESS_EXPORT char * KW_AND;
extern SC_EXPRESS_EXPORT char * KW_ANDOR;
extern SC_EXPRESS_EXPORT char * KW_ARRAY;
extern SC_EXPRESS_EXPORT char * KW_AS;
extern SC_EXPRESS_EXPORT char * KW_ASIN;
extern SC_EXPRESS_EXPORT char * KW_ATAN;
extern SC_EXPRESS_EXPORT char * KW_BAG;
extern SC_EXPRESS_EXPORT char * KW_BEGIN;
extern SC_EXPRESS_EXPORT char * KW_BINARY;
extern SC_EXPRESS_EXPORT char * KW_BLENGTH;
extern SC_EXPRESS_EXPORT char * KW_BOOLEAN;
extern SC_EXPRESS_EXPORT char * KW_BY;
extern SC_EXPRESS_EXPORT char * KW_CASE;
extern SC_EXPRESS_EXPORT char * KW_CONST_E;
extern SC_EXPRESS_EXPORT char * KW_CONSTANT;
extern SC_EXPRESS_EXPORT char * KW_CONTEXT;
extern SC_EXPRESS_EXPORT char * KW_COS;
extern SC_EXPRESS_EXPORT char * KW_DERIVE;
extern SC_EXPRESS_EXPORT char * KW_DIV;
extern SC_EXPRESS_EXPORT char * KW_ELSE;
extern SC_EXPRESS_EXPORT char * KW_END;
extern SC_EXPRESS_EXPORT char * KW_END_ALIAS;
extern SC_EXPRESS_EXPORT char * KW_END_CASE;
extern SC_EXPRESS_EXPORT char * KW_END_CONSTANT;
extern SC_EXPRESS_EXPORT char * KW_END_CONTEXT;
extern SC_EXPRESS_EXPORT char * KW_END_ENTITY;
extern SC_EXPRESS_EXPORT char * KW_END_FUNCTION;
extern SC_EXPRESS_EXPORT char * KW_END_IF;
extern SC_EXPRESS_EXPORT char * KW_END_LOCAL;
extern SC_EXPRESS_EXPORT char * KW_END_MODEL;
extern SC_EXPRESS_EXPORT char * KW_END_PROCEDURE;
extern SC_EXPRESS_EXPORT char * KW_END_REPEAT;
extern SC_EXPRESS_EXPORT char * KW_END_RULE;
extern SC_EXPRESS_EXPORT char * KW_END_SCHEMA;
extern SC_EXPRESS_EXPORT char * KW_END_TYPE;
extern SC_EXPRESS_EXPORT char * KW_ENTITY;
extern SC_EXPRESS_EXPORT char * KW_ENUMERATION;
extern SC_EXPRESS_EXPORT char * KW_ESCAPE;
extern SC_EXPRESS_EXPORT char * KW_EXISTS;
extern SC_EXPRESS_EXPORT char * KW_EXP;
extern SC_EXPRESS_EXPORT char * KW_FALSE;
extern SC_EXPRESS_EXPORT char * KW_FIXED;
extern SC_EXPRESS_EXPORT char * KW_FOR;
extern SC_EXPRESS_EXPORT char * KW_FORMAT;
extern SC_EXPRESS_EXPORT char * KW_FROM;
extern SC_EXPRESS_EXPORT char * KW_FUNCTION;
extern SC_EXPRESS_EXPORT char * KW_GENERIC;
extern SC_EXPRESS_EXPORT char * KW_HIBOUND;
extern SC_EXPRESS_EXPORT char * KW_HIINDEX;
extern SC_EXPRESS_EXPORT char * KW_IF;
extern SC_EXPRESS_EXPORT char * KW_IN;
extern SC_EXPRESS_EXPORT char * KW_INCLUDE;
extern SC_EXPRESS_EXPORT char * KW_INSERT;
extern SC_EXPRESS_EXPORT char * KW_INTEGER;
extern SC_EXPRESS_EXPORT char * KW_INVERSE;
extern SC_EXPRESS_EXPORT char * KW_LENGTH;
extern SC_EXPRESS_EXPORT char * KW_LIKE;
extern SC_EXPRESS_EXPORT char * KW_LIST;
extern SC_EXPRESS_EXPORT char * KW_LOBOUND;
extern SC_EXPRESS_EXPORT char * KW_LOCAL;
extern SC_EXPRESS_EXPORT char * KW_LOG;
extern SC_EXPRESS_EXPORT char * KW_LOG10;
extern SC_EXPRESS_EXPORT char * KW_LOG2;
extern SC_EXPRESS_EXPORT char * KW_LOGICAL;
extern SC_EXPRESS_EXPORT char * KW_LOINDEX;
extern SC_EXPRESS_EXPORT char * KW_MOD;
extern SC_EXPRESS_EXPORT char * KW_MODEL;
extern SC_EXPRESS_EXPORT char * KW_NOT;
extern SC_EXPRESS_EXPORT char * KW_NUMBER;
extern SC_EXPRESS_EXPORT char * KW_NVL;
extern SC_EXPRESS_EXPORT char * KW_ODD;
extern SC_EXPRESS_EXPORT char * KW_OF;
extern SC_EXPRESS_EXPORT char * KW_ONEOF;
extern SC_EXPRESS_EXPORT char * KW_OPTIONAL;
extern SC_EXPRESS_EXPORT char * KW_OR;
extern SC_EXPRESS_EXPORT char * KW_OTHERWISE;
extern SC_EXPRESS_EXPORT char * KW_PI;
extern SC_EXPRESS_EXPORT char * KW_PROCEDURE;
extern SC_EXPRESS_EXPORT char * KW_QUERY;
extern SC_EXPRESS_EXPORT char * KW_REAL;
extern SC_EXPRESS_EXPORT char * KW_REFERENCE;
extern SC_EXPRESS_EXPORT char * KW_REMOVE;
extern SC_EXPRESS_EXPORT char * KW_REPEAT;
extern SC_EXPRESS_EXPORT char * KW_RETURN;
extern SC_EXPRESS_EXPORT char * KW_ROLESOF;
extern SC_EXPRESS_EXPORT char * KW_RULE;
extern SC_EXPRESS_EXPORT char * KW_SCHEMA;
extern SC_EXPRESS_EXPORT char * KW_SELECT;
extern SC_EXPRESS_EXPORT char * KW_SELF;
extern SC_EXPRESS_EXPORT char * KW_SET;
extern SC_EXPRESS_EXPORT char * KW_SIN;
extern SC_EXPRESS_EXPORT char * KW_SIZEOF;
extern SC_EXPRESS_EXPORT char * KW_SKIP;
extern SC_EXPRESS_EXPORT char * KW_SQRT;
extern SC_EXPRESS_EXPORT char * KW_STRING;
extern SC_EXPRESS_EXPORT char * KW_SUBTYPE;
extern SC_EXPRESS_EXPORT char * KW_SUPERTYPE;
extern SC_EXPRESS_EXPORT char * KW_TAN;
extern SC_EXPRESS_EXPORT char * KW_THEN;
extern SC_EXPRESS_EXPORT char * KW_TO;
extern SC_EXPRESS_EXPORT char * KW_TRUE;
extern SC_EXPRESS_EXPORT char * KW_TYPE;
extern SC_EXPRESS_EXPORT char * KW_TYPEOF;
extern SC_EXPRESS_EXPORT char * KW_UNIQUE;
extern SC_EXPRESS_EXPORT char * KW_UNKNOWN;
extern SC_EXPRESS_EXPORT char * KW_UNTIL;
extern SC_EXPRESS_EXPORT char * KW_USE;
extern SC_EXPRESS_EXPORT char * KW_USEDIN;
extern SC_EXPRESS_EXPORT char * KW_VALUE;
extern SC_EXPRESS_EXPORT char * KW_VALUE_IN;
extern SC_EXPRESS_EXPORT char * KW_VALUE_UNIQUE;
extern SC_EXPRESS_EXPORT char * KW_VAR;
extern SC_EXPRESS_EXPORT char * KW_WHERE;
extern SC_EXPRESS_EXPORT char * KW_WHILE;
extern SC_EXPRESS_EXPORT char * KW_XOR;

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

extern SC_EXPRESS_EXPORT Express  EXPRESScreate     PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     EXPRESSdestroy    PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT void     EXPRESSparse      PROTO( ( Express, FILE *, char * ) );
extern SC_EXPRESS_EXPORT void     EXPRESSinitialize PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     EXPRESScleanup    PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     EXPRESSresolve    PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT char  *  EXPRESSversion    PROTO( ( void ) );
extern SC_EXPRESS_EXPORT int      EXPRESS_fail      PROTO( ( Express ) );
extern SC_EXPRESS_EXPORT int      EXPRESS_succeed   PROTO( ( Express ) );
extern                   void     EXPRESSinit_init  PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     EXPRESSusage( int _exit ); ///< exit unless _exit is non-zero
extern SC_EXPRESS_EXPORT void     build_complex( Express );

#endif /*EXPRESS_H*/
