#ifndef HASH_H
#define HASH_H

/* $Id: hash.h,v 1.6 1997/10/22 16:36:49 sauderd Exp $ */

/************************************************************************
** Hash_Table:	Hash_Table
** Description:	
**	
**	
** Largely based on code written by ejp@ausmelb.oz
**
** Constants:
**	HASH_TABLE_NULL	- the null Hash_Table
**
************************************************************************/

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: hash.h,v $
 * Revision 1.6  1997/10/22 16:36:49  sauderd
 * Changed the use and definitions of the compiler macros MUL and DIV. They
 * seem to be defined and used only within hash.h and hash.c
 *
 * Revision 1.5  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.4  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.3  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/05/31  08:36:48  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:56:02  libes
 * Initial revision
 *
 * Revision 1.4  1992/05/14  10:14:33  libes
 * don't remember
 *
 * Revision 1.3  1992/02/12  07:06:15  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:47:45  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:40:30  libes
 * Initial revision
 *
 * Revision 1.1  1992/01/22  02:17:49  libes
 * Initial revision
 *
 * Revision 1.7  1992/01/15  19:56:15  shepherd
 * Commented out text after #else and #endif.
 *
 * Revision 1.5  91/10/30  08:36:54  silver
 * Express N14 Changes
 * 
 * Revision 1.6  1991/08/30  16:36:07  libes
 * fixed HASHlist to use state from parameter instead of static
 *
 * Revision 1.5  1991/08/06  18:55:21  libes
 * Declared HASHcopy for DICT_copy support
 *
 * Revision 1.4  1991/07/19  03:53:36  libes
 * added action HASH_DELETE
 * made action HASH_INSERT return failure upon duplicate entry
 *
 * Revision 1.3  1991/02/26  21:05:59  libes
 * Defined routines to visit every element in hash table.
 *
 * Revision 1.2  1990/09/06  10:51:11  clark
 * BPR 2.1 alpha
 *
 * Revision 1.1  90/06/11  17:05:54  clark
 * Initial revision
 * 
 */

#include "basic.h"	/* get basic definitions */

/*************/
/* constants */
/*************/

#define HASH_NULL	(Hash_Table)NULL

#define SEGMENT_SIZE		256
#define SEGMENT_SIZE_SHIFT	8	/* log2(SEGMENT_SIZE)	*/
#define DIRECTORY_SIZE		256
#define DIRECTORY_SIZE_SHIFT	8	/* log2(DIRECTORY_SIZE)	*/
#define PRIME1			37
#define PRIME2			1048583
#define MAX_LOAD_FACTOR	5

/*****************/
/* packages used */
/*****************/

#ifdef HASH_C
#include <assert.h>
#endif /*HASH_C*/

#include "memory.h"

/************/
/* typedefs */
/************/

typedef enum { HASH_FIND, HASH_INSERT, HASH_DELETE } Action;

typedef unsigned long Address;

/****************/
/* abstractions */
/****************/

typedef struct Element_ {
	char		*key;
	char		*data;
	struct Element_	*next;
	Symbol		*symbol;/* for debugging hash conflicts */
	char		type;	/* user-supplied type */
} *Element;

typedef Element *Segment;

typedef struct Hash_Table_ {
#if 0
	int 	in_use;		/* If someone is traversing the hash table */
#endif
	short	p;		/* Next bucket to be split	*/
	short	maxp;		/* upper bound on p during expansion	*/
	long	KeyCount;	/* current # keys	*/
	short	SegmentCount;	/* current # segments	*/
	short	MinLoadFactor;
	short	MaxLoadFactor;
	Segment	Directory[DIRECTORY_SIZE];
} *Hash_Table;

typedef struct {
	int i;	/* segment index (i think) */
	int j;	/* key index in segment (ditto) */
	Element p;	/* usually the next element to be returned */
	Hash_Table table;
	char type;
	Element e;	/* originally thought of as a place for */
/* the caller of HASHlist to temporarily stash the return value */
/* to allow the caller (i.e., DICTdo) to be macroized, but now */
/* conveniently used by HASHlist, which both stores the ultimate */
/* value here as well as returns it via the return value of HASHlist */
} HashEntry;

/****************/
/* modules used */
/****************/

/********************/
/* global variables */
/********************/

#ifdef HASH_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif /*HASH_C*/

GLOBAL struct freelist_head HASH_Table_fl;
GLOBAL struct freelist_head HASH_Element_fl;

#undef GLOBAL
#undef INITIALLY

/******************************/
/* macro function definitions */
/******************************/

/*
** Fast arithmetic, relying on powers of 2
*/

/* 
The centerline compiler was having problems with MUL and DIV. Where DIV
was called like this DIV(NewAddress, SEGMENT_SIZE) it is now being called like
DIV(NewAddress, SEGMENT_SIZE_SHIFT). The compiler was mentioning some kind of 
problem with ##. I wonder if maybe the precompiler was expanding SEGMENT_SIZE 
(which is also defined as a macro) before turning it into SEGMENT_SIZE_SHIFT.
SEGMENT_SIZE and DIRECTORY_SIZE were used to call these macros. They are both
defined to be 8 though so it seems these are always being used the same way.
This change only seems to have affected hash.h and hash.c
*/
/* #define MUL(x,y)		((x) << (y##_SHIFT)) */
#define MUL(x,y)		((x) << (y))
/* #define DIV(x,y)		((x) >> (y##_SHIFT)) */
#define DIV(x,y)		((x) >> (y))
#define MOD(x,y)		((x) & ((y)-1))

#define HASH_Table_new()	(struct Hash_Table_ *)MEM_new(&HASH_Table_fl)
#define HASH_Table_destroy(x)	MEM_destroy(&HASH_Table_fl,(Freelist *)(Generic)x)
#define HASH_Element_new()	(struct Element_ *)MEM_new(&HASH_Element_fl)
#define HASH_Element_destroy(x)	MEM_destroy(&HASH_Element_fl,(Freelist *)(char *)x)


/***********************/
/* function prototypes */
/***********************/

extern void	HASHinitialize PROTO((void));
extern Hash_Table	HASHcreate PROTO((unsigned));
extern Hash_Table	HASHcopy PROTO((Hash_Table));
extern void	HASHdestroy PROTO((Hash_Table));
extern Element	HASHsearch PROTO((Hash_Table, Element, Action));
extern void	HASHlistinit PROTO((Hash_Table,HashEntry *));
extern void	HASHlistinit_by_type PROTO((Hash_Table,HashEntry *,char));
extern Element	HASHlist PROTO((HashEntry *));
#if 0
extern void	HASHlistend PROTO((HashEntry *));
#endif

#endif /*HASH_H*/
