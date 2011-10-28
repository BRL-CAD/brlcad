#ifndef CASE_ITEM_H
#define CASE_ITEM_H

/* $Id: caseitem.h,v 1.3 1997/01/21 19:17:11 dar Exp $ */

/************************************************************************
** Module:	Case_Item
** Description:	This module implements the Case_Item abstraction.  A
**	case item represents a single branch in a case statement; it
**	thus consists of a list of labels and a statement to execute
**	when one of the labels matches the selector.
** Constants:
**	CASE_ITEM_NULL	- the null item
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: caseitem.h,v $
 * Revision 1.3  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
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

#define CASE_ITEM_NULL		(struct Case_Item_ *)0

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"	/* get basic definitions */
#include "scope.h"

/************/
/* typedefs */
/************/

typedef struct Case_Item_ *Case_Item;

/****************/
/* modules used */
/****************/

#include "stmt.h"

/***************************/
/* hidden type definitions */
/***************************/

struct Case_Item_ {
	Symbol symbol;
	Linked_List labels;
	struct Statement_ *action;
};

/********************/
/* global variables */
/********************/

extern struct freelist_head CASE_IT_fl;

/******************************/
/* macro function definitions */
/******************************/

#define CASE_ITget_labels(ci)		((ci)->labels)
#define CASE_ITget_statement(ci)	((ci)->action)

/***********************/
/* function prototypes */
/***********************/

#define CASE_IT_new()		(struct Case_Item_ *)MEM_new(&CASE_IT_fl)
#define CASE_IT_destroy(x)	MEM_destroy(&CASE_IT_fl,(Freelist *)(Generic)x)

extern Case_Item 	CASE_ITcreate PROTO((Linked_List, struct Statement_ *));
extern void		CASE_ITinitialize PROTO((void));

#endif /*CASE_ITEM_H*/
