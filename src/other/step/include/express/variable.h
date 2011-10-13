#ifndef VARIABLE_H
#define VARIABLE_H

/* $Id: variable.h,v 1.10 1997/01/21 19:17:11 dar Exp $ */

/************************************************************************
** Module:	Variable
** Description:	This module implements the Variable abstraction.  A
**	Variable consists of a name, a type, a reference class, and
**	some flags, e.g. 'optional', 'variable'.  It is used to represent
**	entity attributes, variables, and formal parameters.
** Constants:
**	VARIABLE_NULL	- the null variable
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: variable.h,v $
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
 * Revision 1.6  1993/03/19  20:42:36  libes
 * fixed get_inverse macro
 *
 * Revision 1.5  1993/02/16  03:27:55  libes
 * added flag for simplifying detection of parameters from other vars
 *
 * Revision 1.4  1993/01/19  22:15:13  libes
 * *** empty log message ***
 *
 * Revision 1.3  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define VARIABLE_NULL		(Variable)0

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include "symbol.h"       

/************/
/* typedefs */
/************/

typedef struct Variable_ *Variable;

/****************/
/* modules used */
/****************/
       
#include "type.h"
#include "expr.h"

/***************************/
/* hidden type definitions */
/***************************/

struct Variable_ {
	/* Symbol is inside of 'name' below */
	Expression name;
	Type type;
	Expression initializer;	/* or 'derived' */
	int		offset;

	struct {
		int optional	:1;	/* OPTIONAL keyword */
		int var		:1;	/* VAR keyword */
		int constant	:1;	/* from CONSTANT...END_CONSTANT */
		int unique	:1;	/* appears in UNIQUE list */
		int parameter	:1;	/* is a formal parameter */
		int attribute	:1;	/* is an attribute (rule parameters */
					/* are marked this way, too) */
	} flags;

#define query_symbol inverse_symbol
	Symbol *	inverse_symbol;	/* entity symbol */
	Variable 	inverse_attribute;	/* attribute related by */
				/* inverse relationship */
};

/********************/
/* global variables */
/********************/

extern struct freelist_head VAR_fl;

/******************************/
/* macro function definitions */
/******************************/

#define VAR_new()	(struct Variable_ *)MEM_new(&VAR_fl)
#define VAR_destroy(x)	MEM_destroy(&VAR_fl,(Freelist *)(Generic)x)

#define VARget_name(v)			((v)->name)
#define VARput_name(v,n)		((v)->name = (n))
#define VARput_offset(v,off)		((v)->offset = (off))
#define VARget_offset(v)		((v)->offset)

#define VARget_initializer(v)		((v)->initializer)
#define VARget_type(v)			((v)->type)
#define VARget_optional(v)		((v)->flags.optional)
#define VARget_unique(v)		((v)->flags.unique)

#define VARis_derived(v)		((v)->initializer != 0)
#define VARget_inverse(v)		((v)->inverse_attribute)
/* for backwards compatibility */
#define VARget_derived		VARis_derived

/***********************/
/* function prototypes */
/***********************/

extern Variable VARcreate PROTO((Expression, Type));
extern void VARinitialize PROTO((void));
extern char *VARget_simple_name PROTO((Variable));

#endif    /*  VARIABLE_H  */
