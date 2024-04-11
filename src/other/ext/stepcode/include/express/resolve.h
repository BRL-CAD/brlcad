#ifndef RESOLVE_H
#define RESOLVE_H

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: resolve.h,v $
 * Revision 1.6  1994/11/10 19:20:03  clark
 * Update to IS
 *
 * Revision 1.5  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.3  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.2  1992/09/16  18:23:08  libes
 * fixed bug in TYPEresolve connecting reference types back to using types
 *
 * Revision 1.1  1992/08/18  17:12:41  libes
 * Initial revision
 *
 * Revision 1.2  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.1  1992/05/28  03:54:00  libes
 * Initial revision
 */

#include <sc_export.h>
#include "expbasic.h"   /* get basic definitions */
#include "type.h"
#include "variable.h"
#include "expr.h"
#include "entity.h"
#include "caseitem.h"
#include "stmt.h"
#include "alg.h"
#include "scope.h"
#include "schema.h"
#include "object.h"

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT int print_objects_while_running;

/* macros */

/* cheaper doing the check here, then inside the function call.  Return */
/* immediately for RESOLVED, FAILED_TO_RESOLVE, and RESOLVE_IN_PROGRESS */
#define TYPEresolve(t)        if (is_resolvable((*(t)))) TYPE_resolve((t))
#define VARresolve_types(v)       if (is_resolvable((v)->name)) VAR_resolve_types((v))
#define VARresolve_expressions(v,s) if (is_resolvable((v)->name)) VAR_resolve_expressions((v),(s))
#define EXPresolve(expr,scope,type) if (!is_resolved(expr)) EXP_resolve(expr,scope,type)

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void RESOLVEinitialize( void );
extern SC_EXPRESS_EXPORT void RESOLVEcleanup( void );
extern SC_EXPRESS_EXPORT void SCOPEresolve_expressions_statements( Scope );
extern SC_EXPRESS_EXPORT void SCOPEresolve_subsupers( Scope );
extern SC_EXPRESS_EXPORT void SCOPEresolve_types( Scope );
extern SC_EXPRESS_EXPORT void TYPE_resolve( Type * );
extern SC_EXPRESS_EXPORT void EXP_resolve( Expression, Scope, Type );
extern SC_EXPRESS_EXPORT void ALGresolve( Scope );
extern SC_EXPRESS_EXPORT void SCHEMAresolve( Scope );
extern SC_EXPRESS_EXPORT void RENAMEresolve( Rename *, Schema );

/*
 * for unit tests, no extern / export
 */
void VAR_resolve_expressions( Variable, Entity );
void ENTITYresolve_subtypes( Schema );
void ENTITYresolve_supertypes( Entity );
void ENTITYresolve_expressions( Entity e );
void ALGresolve_expressions_statements( Scope, Linked_List );
int WHEREresolve( Linked_List, Scope, int );
void TYPEresolve_expressions( Type, Scope );
void STMTresolve( Statement, Scope );
void STMTlist_resolve( Linked_List, Scope );
int  ENTITYresolve_subtype_expression( Expression, Entity, Linked_List * );

#endif /*RESOLVE_H*/
