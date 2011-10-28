#ifndef STACK_H
#define	STACK_H

/* $Id: stack.h,v 1.3 1993/10/15 18:49:23 libes dec96 $ */

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: stack.h,v $
 * Revision 1.3  1993/10/15 18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.2  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.1  1992/05/28  03:56:02  libes
 * Initial revision
 */

/*************/
/* constants */
/*************/

#define STACK_NULL	(Stack *)NULL

/*****************/
/* packages used */
/*****************/

#include "express/linklist.h"

/************/
/* typedefs */
/************/

typedef Linked_List	Stack;

/****************/
/* modules used */
/****************/

/***************************/
/* hidden type definitions */
/***************************/

/********************/
/* global variables */
/********************/

#ifdef STACK_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif    /*  STACK_C  */

#undef GLOBAL
#undef INITIALLY

/*******************************/
/* macro function definitions  */   
/*******************************/

/* function aliases */
#define STACKinitialize()
#define STACKcreate()		LISTcreate()
#define	STACKpush(stack, item)	LISTadd_first((stack), (item))
#define STACKpop(stack)		LISTremove_first(stack)
#define STACKpeek(stack)	LISTpeek_first(stack)

/***********************/
/* function prototypes */
/***********************/

/*******************************/
/* inline function definitions */
/*******************************/

#if supports_inline_functions || defined(STACK_C)

#endif /* supports_inline_functions || defined(STACK_C) */

#endif    /*  STACK_H  */
