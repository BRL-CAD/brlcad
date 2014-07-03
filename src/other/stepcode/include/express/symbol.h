#ifndef SYMBOL_H
#define SYMBOL_H

/** **********************************************************************
** Module:  Symbol \file symbol.h
** Description: This module implements the Symbol abstraction.
** Constants:
**  SYMBOL_NULL - the null Symbol
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

#define SYMBOL_NULL     (Symbol)NULL

/*****************/
/* packages used */
/*****************/

#include <sc_export.h>
#include "basic.h"  /* get basic definitions */
#include "memory.h"

/************/
/* typedefs */
/************/

typedef struct Symbol_ Symbol;

/***************************/
/* hidden type definitions */
/***************************/

struct Symbol_ {
    char * name;
    char * filename;
    int line;
    char resolved;
};

/****************/
/* modules used */
/****************/

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT struct freelist_head SYMBOL_fl;

/******************************/
/* macro function definitions */
/******************************/

#define SYMBOL_new()        (struct Symbol_ *)MEM_new(&SYMBOL_fl)
#define SYMBOL_destroy(x)   MEM_destroy(&SYMBOL_fl,(Freelist *)(Generic)x)

#define SYMBOLset(obj)      obj->symbol.line = yylineno; \
                obj->symbol.filename = current_filename

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void SYMBOLinitialize PROTO( ( void ) );
SC_EXPRESS_EXPORT Symbol * SYMBOLcreate( char * name, int line, char * filename );

#endif    /*  SYMBOL_H  */
