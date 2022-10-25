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

extern SC_EXPRESS_EXPORT char * input_filename;
extern SC_EXPRESS_EXPORT Linked_List EXPRESS_path;
extern SC_EXPRESS_EXPORT int EXPRESSpass;

extern SC_EXPRESS_EXPORT void ( *EXPRESSinit_args )( int, char ** );
extern SC_EXPRESS_EXPORT void ( *EXPRESSinit_parse )( void );
extern SC_EXPRESS_EXPORT int ( *EXPRESSfail )( Express );
extern SC_EXPRESS_EXPORT int ( *EXPRESSsucceed )( Express );
extern SC_EXPRESS_EXPORT void ( *EXPRESSbackend )( Express );
extern SC_EXPRESS_EXPORT char * EXPRESSprogram_name;
extern char   EXPRESSgetopt_options[];  /* initialized elsewhere */
extern SC_EXPRESS_EXPORT int ( *EXPRESSgetopt )( int, char * );
extern SC_EXPRESS_EXPORT bool    EXPRESSignore_duplicate_schemas;

extern SC_EXPRESS_EXPORT Dictionary EXPRESSbuiltins;  /* procedures/functions */

extern SC_EXPRESS_EXPORT struct Scope_ * FUNC_NVL;
extern SC_EXPRESS_EXPORT struct Scope_ * FUNC_USEDIN;

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

extern SC_EXPRESS_EXPORT Express  EXPRESScreate( void );
extern SC_EXPRESS_EXPORT void     EXPRESSdestroy( Express );
extern SC_EXPRESS_EXPORT void     EXPRESSparse( Express, FILE *, char * );
extern SC_EXPRESS_EXPORT void     EXPRESSinitialize( void );
extern SC_EXPRESS_EXPORT void     EXPRESScleanup( void );
extern SC_EXPRESS_EXPORT void     EXPRESSresolve( Express );
extern SC_EXPRESS_EXPORT int      EXPRESS_fail( Express model );
extern SC_EXPRESS_EXPORT int      EXPRESS_succeed( Express model );
extern                   void     EXPRESSinit_init( void );
extern SC_EXPRESS_EXPORT void     build_complex( Express );

#endif /*EXPRESS_H*/
