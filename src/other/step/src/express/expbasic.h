/* $Id: expbasic.h,v 1.7 1997/01/21 19:17:11 dar Exp $ */

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: expbasic.h,v $
 * Revision 1.7  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.6  1995/05/17  14:25:12  libes
 * Added type char for handling pass diagnostics.
 *
 * Revision 1.5  1995/03/09  18:43:28  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.4  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.3  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

#ifndef EXPRESS_BASIC_H
#define EXPRESS_BASIC_H

#include "basic.h"	/* get basic definitions */

/************/
/* typedefs */
/************/

typedef int Integer;
typedef double Real;
typedef enum { Lfalse, Lunknown, Ltrue } Logical;
/* typedef ... Binary; done below because String not defined yet */

#ifndef _CLIENTDATA
#   ifdef __STDC__
    typedef void *ClientData;
#   else
    typedef int *ClientData;
#   endif /* __STDC__ */
#define _CLIENTDATA
#endif

/****************************/
/* packages used throughout */
/****************************/

#include "memory.h"

typedef struct Scope_ *Type;
typedef struct Scope_ *Scope;
typedef struct Scope_ *Schema;

typedef char * Binary;
#include "linklist.h"

#define UNRESOLVED		0x0
#define	RESOLVED		0x1
#define RESOLVE_FAILED		0x2
#define RESOLVE_IN_PROGRESS	0x4	/* only if actively being resolved */
					/* not if it's just between passes */

/* used during rename resolution to catch recursive USE/REFs */
#define is_resolve_in_progress_raw(x)	((x)->resolved & RESOLVE_IN_PROGRESS)
#define resolve_in_progress_raw(x)	(x)->resolved |= RESOLVE_IN_PROGRESS
#define resolve_not_in_progress_raw(x)	(x)->resolved &= ~RESOLVE_IN_PROGRESS

/* and type resolution to catch recursive types (A = B and B = A) */
#define resolve_in_progress(x)		(x)->symbol.resolved |= RESOLVE_IN_PROGRESS

/* test/set whether anything has failed resolution */
#define is_resolve_failed(x)		((x)->symbol.resolved & RESOLVE_FAILED)
#define is_resolve_failed_raw(x)	((x)->resolved & RESOLVE_FAILED)
#define resolve_failed(x)		(x)->symbol.resolved |= RESOLVE_FAILED
#define resolve_failed_raw(x)		(x)->resolved |= RESOLVE_FAILED

/* test/set whether anything has succeeded resolution */
#define is_resolved(x)			((x)->symbol.resolved & RESOLVED)

/* test possibility of future resolution */
#define is_resolvable(x)		(!is_not_resolvable(x))
#define is_not_resolvable(x)		((x)->symbol.resolved & (RESOLVE_FAILED|RESOLVED|RESOLVE_IN_PROGRESS))

/* mark something resolved */
#define resolved_all(x)			x->symbol.resolved = RESOLVED


/* define the object types that can appear in a symbol table */
/* basically anything that is usefully named, but that cannot be */
/* differentiated from another type when handed a generic pointer */
/* Entity, schema and alg can be differentiated but it is just very */
/* useful to be able to differentiate this way. */
#define OBJ_ANY		'*'	/* Matches anything - special to DICT funcs */
#define OBJ_EXPRESS	'!'	/* I can't figure out the mnemonic either! */
#define OBJ_PASS	'#'

/* The following are all odd-ball scopes, that aren't ever looked up by */
/* type - but the data structures might as well be descriptive */
#define OBJ_INCREMENT	'+'
#define OBJ_ALIAS	'a'
#define OBJ_QUERY	'q'	/* isn't even stored anywhere! */

#define OBJ_PROCEDURE	'p'
#define OBJ_RENAME	'n'
#define OBJ_RULE	'r'
#define OBJ_FUNCTION	'f'
#define OBJ_TAG		'g'	/* a type, but only when used as a type tag */
#define OBJ_ENTITY	'e'
#define OBJ_SCHEMA	's'
#define OBJ_TYPE	't'
#define OBJ_UNKNOWN	'u'	/*things that won't ever be looked up by type*/
#define OBJ_VARIABLE	'v'
#define OBJ_WHERE	'w'
#define OBJ_EXPRESSION	'x'	/* so far only enums get looked up by name & */
#define OBJ_ENUM	'x'	/* appear this way, perhaps they should be */
				/* variables? */
#define OBJ_AMBIG_ENUM	'z'	/* enumerations of the same name which are */
				/* visible in the same scope when qualified */
				/* z is of no mnemonic significance (so far) */
#define IS_ENUM(x)	(((x) == OBJ_ENUM) || ((x) == OBJ_AMBIG_ENUM))

/* these represent a different way of selecting classes of objects */
/* these are particularly useful when looking for multiple types concurrently */
#define OBJ_TYPE_BITS		0x1
#define OBJ_ENTITY_BITS		0x2
#define OBJ_FUNCTION_BITS	0x4
#define OBJ_PROCEDURE_BITS	0x8
#define OBJ_PASS_BITS		0x10		/* i.e., EXPRESSpass */
#define OBJ_RULE_BITS		0x20
#define OBJ_EXPRESSION_BITS	0x40		/* as above, only used for */
						/* finding enums */
#define OBJ_SCHEMA_BITS		0x80
#define OBJ_VARIABLE_BITS	0x100
#define OBJ_WHERE_BITS		0x200

#define OBJ_ANYTHING_BITS	0x0fffffff
#define OBJ_UNFINDABLE_BITS	0x10000000	/* should never be found */
#define OBJ_ALGORITHM_BITS	(OBJ_FUNCTION_BITS | OBJ_PROCEDURE_BITS | \
				 OBJ_RULE_BITS)
#define OBJ_SCOPE_BITS		(OBJ_ALGORITHM_BITS | OBJ_ENTITY_BITS | \
				 OBJ_SCHEMA_BITS)

/* OBJ_UNUSED should die, but until it does, allow it to work */
#define OBJ_UNUSED_BITS		OBJ_UNFINDABLE_BITS

#endif /*EXPRESS_BASIC_H*/

