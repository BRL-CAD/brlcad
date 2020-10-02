#ifndef ALGORITHM_H
#define ALGORITHM_H

/** **********************************************************************
** Module:  Algorithm \file alg.h
** Description: This module implements the Algorithm abstraction.  An
**  algorithm can be a procedure, a function, or a rule.  It consists
**  of a name, a return type (for functions and rules), a list of
**  formal parameters, and a list of statements (body).
** Constants:
**  ALGORITHM_NULL  - the null algorithm
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: alg.h,v $
 * Revision 1.4  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.3  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.4  1993/02/16  03:13:56  libes
 * add Where type
 *
 * Revision 1.3  1992/08/18  17:12:41  libes
 * *** empty log message ***
 *
 * Revision 1.2  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 *
 */

/*****************/
/* packages used */
/*****************/

#include "expbasic.h"   /* get basic definitions */
#include "symbol.h"
#include "scope.h"
#include "type.h"

/************/
/* typedefs */
/************/

typedef struct Scope_ * Procedure;
typedef struct Scope_ * Function;
typedef struct Scope_ * Rule;
typedef struct Where_ * Where;

/***************************/
/* hidden type definitions */
/***************************/

/** each formal tag has one of these structs allocated to it
 * As each (real) call is resolved, the tag->type is temporarily borrowed
 */
struct tag {
    char * name;
    Type type;
};

/** location of fulltext of algorithm in source file */
struct FullText {
    char * filename;
    unsigned int start, end;
};

/** 'parameters' are lists of lists of (type) expressions */
struct Procedure_ {
    int pcount; /**< # of parameters */
    int tag_count;  /**< # of different parameter tags */
    Linked_List parameters;
    Linked_List body;
    struct FullText text;
    int builtin;    /**< builtin if true */
};

struct Function_ {
    int pcount; /**< # of parameters */
    int tag_count;  /**< # of different parameter/return value tags */
    Linked_List parameters;
    Linked_List body;
    Type return_type;
    struct FullText text;
    int builtin;    /**< builtin if true */
};

struct Rule_ {
    Linked_List parameters;
    Linked_List body;
    struct FullText text;
};

/** define a where clause */
struct Where_ {
    Symbol   *   label;
    Expression  expr;
};

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT struct freelist_head ALG_fl;
extern SC_EXPRESS_EXPORT struct freelist_head FUNC_fl;
extern SC_EXPRESS_EXPORT struct freelist_head RULE_fl;
extern SC_EXPRESS_EXPORT struct freelist_head PROC_fl;
extern SC_EXPRESS_EXPORT struct freelist_head WHERE_fl;

/******************************/
/* macro function definitions */
/******************************/

#define ALG_new()   (struct Algorithm *)MEM_new(&ALG_fl);
#define ALG_destroy(x)  MEM_destroy(&ALG_fl,(Freelist *)(Generic)x)
#define FUNC_new()  (struct Function_ *)MEM_new(&FUNC_fl)
#define FUNC_destroy(x) MEM_destroy(&FUNC_fl,(Freelist *)(Generic)x)
#define RULE_new()  (struct Rule_ *)MEM_new(&RULE_fl)
#define RULE_destroy(x) MEM_destroy(&RULE_fl,(Freelist *)(Generic)x)
#define PROC_new()  (struct Procedure_ *)MEM_new(&PROC_fl)
#define PROC_destroy(x) MEM_destroy(&PROC_fl,(Freelist *)(Generic)x)
#define WHERE_new() (struct Where_ *)MEM_new(&WHERE_fl)
#define WHERE_destroy(x) MEM_destroy(&WHERE_fl,(Freelist *)(Generic)x)

#define ALGput_name(algorithm, name)    SCOPEput_name(algorithm,name)
#define ALGget_name(algorithm)      SCOPEget_name(algorithm)
#define ALGget_symbol(a)        SCOPEget_symbol(a)
#define FUNCget_name(f)         SCOPEget_name(f)
#define PROCget_name(p)         SCOPEget_name(p)
#define RULEget_name(r)         SCOPEget_name(r)
#define FUNCput_name(f)         SCOPEput_name(f,n)
#define PROCput_name(p)         SCOPEput_name(p,n)
#define RULEput_name(r)         SCOPEput_name(r,n)
#define FUNCget_symbol(f)       SCOPEget_symbol(f)
#define PROCget_symbol(r)       SCOPEget_symbol(p)
#define RULEget_symbol(r)       SCOPEget_symbol(r)
#define FUNCget_parameters(f)   ((f)->u.func->parameters)
#define PROCget_parameters(p)   ((p)->u.proc->parameters)
#define RULEget_parameters(r)   ((r)->u.rule->parameters)
#define FUNCget_body(f)     ((f)->u.func->body)
#define PROCget_body(p)     ((p)->u.proc->body)
#define RULEget_body(r)     ((r)->u.rule->body)
#define FUNCput_body(f,b)   ((f)->u.func->body = b)
#define PROCput_body(p,b)   ((p)->u.proc->body = b)
#define RULEput_body(r,b)   ((r)->u.rule->body = b)
#define FUNCget_return_type(f)  ((f)->u.func->return_type)
#define RULEget_where(r)    ((r)->where)
#define RULEput_where(r,w)  ((r)->where = (w))
#define WHEREget_label(w)   ((w)->label)
#define WHEREget_expression(w)  ((w)->expr)

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT Scope    ALGcreate PROTO( ( char ) );
extern SC_EXPRESS_EXPORT void     ALGinitialize PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     ALGput_full_text PROTO( ( Scope, int, int ) );

#endif /* ALGORITHM_H */
