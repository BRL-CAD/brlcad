#ifndef RESOLVE_H
#define RESOLVE_H

/* $Id: resolve.h,v 1.6 1994/11/10 19:20:03 clark dec96 $ */

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

#include "expbasic.h"	/* get basic definitions */
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

#ifdef RESOLVE_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif /*RESOLVE_C*/

GLOBAL int	print_objects_while_running	INITIALLY(0);

GLOBAL Error	ERROR_undefined_attribute	INITIALLY(ERROR_none);
GLOBAL Error	ERROR_undefined_type		INITIALLY(ERROR_none);
GLOBAL Error	ERROR_undefined_schema		INITIALLY(ERROR_none);
GLOBAL Error	ERROR_unknown_attr_in_entity	INITIALLY(ERROR_none);
GLOBAL Error	ERROR_unknown_subtype		INITIALLY(ERROR_none);
GLOBAL Error	ERROR_unknown_supertype		INITIALLY(ERROR_none);
GLOBAL Error	ERROR_circular_reference	INITIALLY(ERROR_none);
GLOBAL Error	ERROR_ambiguous_attribute	INITIALLY(ERROR_none);
GLOBAL Error	ERROR_ambiguous_group		INITIALLY(ERROR_none);

#undef GLOBAL
#undef INITIALLY

/* macros */

/* cheaper doing the check here, then inside the function call.  Return */
/* immediately for RESOLVED, FAILED_TO_RESOLVE, and RESOLVE_IN_PROGRESS */
/*#define TYPEresolve(t,s)		if (is_resolvable((*(t)))) TYPE_resolve((t),(s))*/
#define TYPEresolve(t,s)		if (is_resolvable((*(t)))) TYPE_resolve((t))
#define VARresolve_types(v,s)		if (is_resolvable((v)->name)) VAR_resolve_types((v),(s))
#define VARresolve_expressions(v,s)	if (is_resolvable((v)->name)) VAR_resolve_expressions((v),(s))
#define EXPresolve(expr,scope,type)	if (!is_resolved(expr)) EXP_resolve(expr,scope,type)

/***********************/
/* function prototypes */
/***********************/

extern void	RESOLVEinitialize PROTO((void));
extern void	SCOPEresolve_expressions_statements PROTO((Scope));
extern void	SCOPEresolve_subsupers PROTO((Scope));
extern void	SCOPEresolve_types PROTO((Scope));
/*extern void	TYPE_resolve PROTO((Type *,Scope));*/
extern void	TYPE_resolve PROTO((Type *));
extern void	EXP_resolve PROTO((Expression,Scope,Type));
extern void	ALGresolve PROTO((Scope));
extern void	SCHEMAresolve PROTO((Scope));

#endif /*RESOLVE_H*/
