#ifndef SYMBOL_H
#define SYMBOL_H

/* $Id: symbol.h,v 1.8 1997/01/21 19:17:11 dar Exp $ */

/************************************************************************
** Module:	Symbol
** Description:	This module implements the Symbol abstraction.  
** Constants:
**	SYMBOL_NULL	- the null Symbol
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: symbol.h,v $
 * Revision 1.8  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.7  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.6  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/01/19  22:16:43  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/05/31  23:31:49  libes
 * implemented ALIAS resolution
 */

/*************/
/* constants */
/*************/

#define SYMBOL_NULL		(Symbol)NULL

/*****************/
/* packages used */
/*****************/

#ifdef SYMBOL_C
#endif    /*  SYMBOL_C  */

#include "basic.h"	/* get basic definitions */
#include "memory.h"

/************/
/* typedefs */
/************/

typedef struct Symbol_ Symbol;

/***************************/
/* hidden type definitions */
/***************************/

struct Symbol_ {
	char *name;
	char *filename;
	short line;
	char resolved;
};

/****************/
/* modules used */
/****************/

/********************/
/* global variables */
/********************/

#ifdef SYMBOL_C
#include "defstart.h"
#else
#include "decstart.h"
#endif    /*  SYMBOL_C  */

GLOBAL struct freelist_head SYMBOL_fl;

#include "de_end.h"

/******************************/
/* macro function definitions */
/******************************/

#define SYMBOL_new()		(struct Symbol_ *)MEM_new(&SYMBOL_fl)
#define SYMBOL_destroy(x)	MEM_destroy(&SYMBOL_fl,(Freelist *)(Generic)x)

#define SYMBOLset(obj)		obj->symbol.line = yylineno; \
				obj->symbol.filename = current_filename
/* for backwards compatibility only, no one should ever need this */
#define SYMBOLget_name(sym)	((sym)->name)

/***********************/
/* function prototypes */
/***********************/

extern void	SYMBOLinitialize PROTO((void));
Symbol *SYMBOLcreate PROTO((char *, int, char *));

/********************/
/* inline functions */
/********************/

#if supports_inline_functions || defined(SYMBOL_C)
#endif /* supports_inline_functions || defined(SYMBOL_C) */

#endif    /*  SYMBOL_H  */
