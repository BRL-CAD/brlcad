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

#ifdef EXPRESS_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif /*EXPRESS_C*/

GLOBAL Linked_List EXPRESS_path;
GLOBAL int EXPRESSpass;

GLOBAL void (*EXPRESSinit_args) PROTO((int, char**))	INITIALLY(0);
GLOBAL void (*EXPRESSinit_parse) PROTO((void))		INITIALLY(0);
GLOBAL int  (*EXPRESSfail) PROTO((Express))		INITIALLY(0);
GLOBAL int  (*EXPRESSsucceed) PROTO((Express))		INITIALLY(0);
GLOBAL void (*EXPRESSbackend) PROTO((Express))		INITIALLY(0);
GLOBAL char  *EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];	/* initialized elsewhere */
GLOBAL int  (*EXPRESSgetopt) PROTO((int,char *))	INITIALLY(0);
GLOBAL int    EXPRESSignore_duplicate_schemas		INITIALLY(False);

GLOBAL Dictionary EXPRESSbuiltins;	/* procedures/functions */

GLOBAL Error ERROR_bail_out		INITIALLY(ERROR_none);
GLOBAL Error ERROR_syntax		INITIALLY(ERROR_none);
GLOBAL Error ERROR_unlabelled_param_type INITIALLY(ERROR_none);
GLOBAL Error ERROR_file_unreadable;
GLOBAL Error ERROR_file_unwriteable;

GLOBAL struct Scope_ *FUNC_NVL;
GLOBAL struct Scope_ *FUNC_USEDIN;

GLOBAL char *KW_ABS		INITIALLY("ABS");
GLOBAL char *KW_ABSTRACT	INITIALLY("ABSTRACT");
GLOBAL char *KW_ACOS		INITIALLY("ACOS");
GLOBAL char *KW_AGGREGATE	INITIALLY("AGGREGATE");
GLOBAL char *KW_ALIAS		INITIALLY("ALIAS");
GLOBAL char *KW_AND		INITIALLY("AND");
GLOBAL char *KW_ANDOR		INITIALLY("ANDOR");
GLOBAL char *KW_ARRAY		INITIALLY("ARRAY");
GLOBAL char *KW_AS		INITIALLY("AS");
GLOBAL char *KW_ASIN		INITIALLY("ASIN");
GLOBAL char *KW_ATAN		INITIALLY("ATAN");
GLOBAL char *KW_BAG		INITIALLY("BAG");
GLOBAL char *KW_BEGIN		INITIALLY("BEGIN");
GLOBAL char *KW_BINARY		INITIALLY("BINARY");
GLOBAL char *KW_BLENGTH		INITIALLY("BLENGTH");
GLOBAL char *KW_BOOLEAN		INITIALLY("BOOLEAN");
GLOBAL char *KW_BY		INITIALLY("BY");
GLOBAL char *KW_CASE		INITIALLY("CASE");
GLOBAL char *KW_CONST_E		INITIALLY("CONST_E");
GLOBAL char *KW_CONSTANT	INITIALLY("CONSTANT");
GLOBAL char *KW_CONTEXT		INITIALLY("CONTEXT");
GLOBAL char *KW_COS		INITIALLY("COS");
GLOBAL char *KW_DERIVE		INITIALLY("DERIVE");
GLOBAL char *KW_DIV		INITIALLY("DIV");
GLOBAL char *KW_ELSE		INITIALLY("ELSE");
GLOBAL char *KW_END		INITIALLY("END");
GLOBAL char *KW_END_ALIAS	INITIALLY("END_ALIAS");
GLOBAL char *KW_END_CASE	INITIALLY("END_CASE");
GLOBAL char *KW_END_CONSTANT	INITIALLY("END_CONSTANT");
GLOBAL char *KW_END_CONTEXT	INITIALLY("END_CONTEXT");
GLOBAL char *KW_END_ENTITY	INITIALLY("END_ENTITY");
GLOBAL char *KW_END_FUNCTION	INITIALLY("END_FUNCTION");
GLOBAL char *KW_END_IF		INITIALLY("END_IF");
GLOBAL char *KW_END_LOCAL	INITIALLY("END_LOCAL");
GLOBAL char *KW_END_MODEL	INITIALLY("END_MODEL");
GLOBAL char *KW_END_PROCEDURE	INITIALLY("END_PROCEDURE");
GLOBAL char *KW_END_REPEAT	INITIALLY("END_REPEAT");
GLOBAL char *KW_END_RULE	INITIALLY("END_RULE");
GLOBAL char *KW_END_SCHEMA	INITIALLY("END_SCHEMA");
GLOBAL char *KW_END_TYPE	INITIALLY("END_TYPE");
GLOBAL char *KW_ENTITY		INITIALLY("ENTITY");
GLOBAL char *KW_ENUMERATION	INITIALLY("ENUMERATION");
GLOBAL char *KW_ESCAPE		INITIALLY("ESCAPE");
GLOBAL char *KW_EXISTS		INITIALLY("EXISTS");
GLOBAL char *KW_EXP		INITIALLY("EXP");
GLOBAL char *KW_FALSE		INITIALLY("FALSE");
GLOBAL char *KW_FIXED		INITIALLY("FIXED");
GLOBAL char *KW_FOR		INITIALLY("FOR");
GLOBAL char *KW_FORMAT		INITIALLY("FORMAT");
GLOBAL char *KW_FROM		INITIALLY("FROM");
GLOBAL char *KW_FUNCTION	INITIALLY("FUNCTION");
GLOBAL char *KW_GENERIC		INITIALLY("GENERIC");
GLOBAL char *KW_HIBOUND		INITIALLY("HIBOUND");
GLOBAL char *KW_HIINDEX		INITIALLY("HIINDEX");
GLOBAL char *KW_IF		INITIALLY("IF");
GLOBAL char *KW_IN		INITIALLY("IN");
GLOBAL char *KW_INCLUDE		INITIALLY("INCLUDE");
GLOBAL char *KW_INSERT		INITIALLY("INSERT");
GLOBAL char *KW_INTEGER		INITIALLY("INTEGER");
GLOBAL char *KW_INVERSE		INITIALLY("INVERSE");
GLOBAL char *KW_LENGTH		INITIALLY("LENGTH");
GLOBAL char *KW_LIKE		INITIALLY("LIKE");
GLOBAL char *KW_LIST		INITIALLY("LIST");
GLOBAL char *KW_LOBOUND		INITIALLY("LOBOUND");
GLOBAL char *KW_LOCAL		INITIALLY("LOCAL");
GLOBAL char *KW_LOG		INITIALLY("LOG");
GLOBAL char *KW_LOG10		INITIALLY("LOG10");
GLOBAL char *KW_LOG2		INITIALLY("LOG2");
GLOBAL char *KW_LOGICAL		INITIALLY("LOGICAL");
GLOBAL char *KW_LOINDEX		INITIALLY("LOINDEX");
GLOBAL char *KW_MOD		INITIALLY("MOD");
GLOBAL char *KW_MODEL		INITIALLY("MODEL");
GLOBAL char *KW_NOT		INITIALLY("NOT");
GLOBAL char *KW_NUMBER		INITIALLY("NUMBER");
GLOBAL char *KW_NVL		INITIALLY("NVL");
GLOBAL char *KW_ODD		INITIALLY("ODD");
GLOBAL char *KW_OF		INITIALLY("OF");
GLOBAL char *KW_ONEOF		INITIALLY("ONEOF");
GLOBAL char *KW_OPTIONAL	INITIALLY("OPTIONAL");
GLOBAL char *KW_OR		INITIALLY("OR");
GLOBAL char *KW_OTHERWISE	INITIALLY("OTHERWISE");
GLOBAL char *KW_PI		INITIALLY("PI");
GLOBAL char *KW_PROCEDURE	INITIALLY("PROCEDURE");
GLOBAL char *KW_QUERY		INITIALLY("QUERY");
GLOBAL char *KW_REAL		INITIALLY("REAL");
GLOBAL char *KW_REFERENCE	INITIALLY("REFERENCE");
GLOBAL char *KW_REMOVE		INITIALLY("REMOVE");
GLOBAL char *KW_REPEAT		INITIALLY("REPEAT");
GLOBAL char *KW_RETURN		INITIALLY("RETURN");
GLOBAL char *KW_ROLESOF		INITIALLY("ROLESOF");
GLOBAL char *KW_RULE		INITIALLY("RULE");
GLOBAL char *KW_SCHEMA		INITIALLY("SCHEMA");
GLOBAL char *KW_SELECT		INITIALLY("SELECT");
GLOBAL char *KW_SELF		INITIALLY("SELF");
GLOBAL char *KW_SET		INITIALLY("SET");
GLOBAL char *KW_SIN		INITIALLY("SIN");
GLOBAL char *KW_SIZEOF		INITIALLY("SIZEOF");
GLOBAL char *KW_SKIP		INITIALLY("SKIP");
GLOBAL char *KW_SQRT		INITIALLY("SQRT");
GLOBAL char *KW_STRING		INITIALLY("STRING");
GLOBAL char *KW_SUBTYPE		INITIALLY("SUBTYPE");
GLOBAL char *KW_SUPERTYPE	INITIALLY("SUPERTYPE");
GLOBAL char *KW_TAN		INITIALLY("TAN");
GLOBAL char *KW_THEN		INITIALLY("THEN");
GLOBAL char *KW_TO		INITIALLY("TO");
GLOBAL char *KW_TRUE		INITIALLY("TRUE");
GLOBAL char *KW_TYPE		INITIALLY("TYPE");
GLOBAL char *KW_TYPEOF		INITIALLY("TYPEOF");
GLOBAL char *KW_UNIQUE		INITIALLY("UNIQUE");
GLOBAL char *KW_UNKNOWN		INITIALLY("UNKNOWN");
GLOBAL char *KW_UNTIL		INITIALLY("UNTIL");
GLOBAL char *KW_USE		INITIALLY("USE");
GLOBAL char *KW_USEDIN		INITIALLY("USEDIN");
GLOBAL char *KW_VALUE		INITIALLY("VALUE");
GLOBAL char *KW_VALUE_IN	INITIALLY("VALUE_IN");
GLOBAL char *KW_VALUE_UNIQUE	INITIALLY("VALUE_UNIQUE");
GLOBAL char *KW_VAR		INITIALLY("VAR");
GLOBAL char *KW_WHERE		INITIALLY("WHERE");
GLOBAL char *KW_WHILE		INITIALLY("WHILE");
GLOBAL char *KW_XOR		INITIALLY("XOR");

#undef GLOBAL
#undef INITIALLY

/******************************/
/* macro function definitions */
/******************************/

#define EXPRESSget_basename(e)		((e)->u.express->basename)
#define EXPRESSget_filename(e)		((e)->u.express->filename)
#define EXPRESSput_basename(e,n)	((e)->u.express->basename = (n))
#define EXPRESSput_filename(e,n)	((e)->u.express->filename = (n))

/********************/
/* Inline functions */
/********************/

#if supports_inline_functions || defined(EXPRESS_C)

#endif /*supports_inline_functions || defined(EXPRESS_C)*/

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
#if 0
extern void		EXPRESSdump_model PROTO((Express));
#endif /*0*/
extern void             build_complex(Express);

#endif /*EXPRESS_H*/
