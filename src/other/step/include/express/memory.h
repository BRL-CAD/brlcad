#ifndef MEMORY_H
#define MEMORY_H

/* $Id: memory.h,v 1.5 1997/01/21 19:17:11 dar Exp $ */

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: memory.h,v $
 * Revision 1.5  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.4  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.3  1993/02/22  21:41:13  libes
 * ANSI compat
 *
 * Revision 1.2  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.1  1992/05/28  03:56:02  libes
 * Initial revision
 */

#include "basic.h"

/*****************/
/* packages used */
/*****************/

/* mem.h - defs for fixed size block memory allocator */

typedef long Align;

union freelist {
	union freelist *next;	/* next block on freelist */
	char memory;		/* user data */
	Align aligner;		/* force alignment of blocks */
};

typedef union freelist Freelist;

struct freelist_head {
	int size_elt;		/* size of a single elt */
#ifndef NOSTAT
	int alloc;		/* # of allocations */
	int dealloc;
	int create;		/* number of calls to create a new freelist */
	Generic max;		/* end of freelist */
#endif
	int size;		/* size of a single elt incl. next ptr */
	int bytes;		/* if we run out, allocate memory by this
				   many bytes */
	Freelist *freelist;
#if SPACE_PROFILE
	int count;
#endif
};

char *nnew();

#include "error.h"

/* should be MEMORY_C but there is none, so we'll tie it to express.c */
#ifdef EXPRESS_C
#include "defstart.h"
#else
#include "decstart.h"
#endif /* EXPRESS_C */

/***********************************************/
/* space allocation macros with error package: */
/***********************************************/

extern int yylineno;

/* CALLOC grabs and initializes to all 0s space for the indicated */
/* number of instances of the indicated type */
#define	CALLOC(ptr, num, type)					\
    if (((ptr) = (type*)calloc((num), (unsigned)sizeof(type)))==NULL) {	\
        fprintf(stderr,"fedex: out of space");\
    } else {}

void	_MEMinitialize PROTO((void));
void	MEMinitialize PROTO((struct freelist_head *,int,int,int));
void	MEM_destroy PROTO((  struct freelist_head *,Freelist *));
Generic	MEM_new PROTO((      struct freelist_head *));

#include "de_end.h"

#endif /* MEMORY_H */


