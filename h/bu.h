/*
 *				B U . H
 *
 *  Header file for the BRL-CAD Utility Library, LIBBU.
 *
 *  This library provides several layers of low-level utility routines,
 *  providing features that make coding much easier.
 *	Parallel processing support:  threads, sempahores, parallel-malloc.
 *	Consolodated logging support:  bu_log(), bu_bomb().
 *
 *  The intention is that these routines are general extensions to
 *  the data types offered by the C language itself, and to the
 *  basic C runtime support provided by the system LIBC.
 *
 *  All of the data types provided by this library are defined in bu.h;
 *  none of the routines in this library will depend on data types defined
 *  in other BRL-CAD header files, such as vmath.h.
 *  Look for those routines in LIBBN.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  Include Sequencing -
 *	#include "conf.h"
 *	#include <stdio.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "rtlist.h"	/_* OPTIONAL, auto-included by bu.h *_/
 *	#include "bu.h"
 *
 *  Libraries Used -
 *	-lm -lc
 *
 *  $Header$
 */

#ifndef SEEN_BU_H
#define SEEN_BU_H seen
#ifdef __cplusplus
extern "C" {
#endif

#include <setjmp.h>

#define BU_H_VERSION	"@(#)$Header$ (BRL)"

/*----------------------------------------------------------------------*/
/*
 *  System library routines used by LIBBS.
 *  If header files are to be included, this should happen first,
 *  to prevent accidentally redefining important stuff.
 */
#if HAVE_STDLIB_H
/*	NOTE:  Nested includes, gets malloc(), offsetof(), etc */
#	include <stdlib.h>
#	include <stddef.h>
#else
extern char	*malloc();
extern char	*calloc();
extern char	*realloc();
/**extern void	free(); **/
#endif

/* 
 * BU_FLSTR   Macro for getting a concatenated string of the current 
 * file and line number.  Produces something of the form:
 *   "filename.c"":""1234"
 */
#define bu_cpp_str(s) # s
#define bu_cpp_xstr(s)  bu_cpp_str(s)
#define bu_cpp_glue(a, b) a ## b
#define bu_cpp_xglue(a, b) bu_cpp_glue(a, b)
#define BU_FLSTR __FILE__ ":" bu_cpp_xstr(__LINE__)
#define BU_QFLSTR bu_cpp_xstr(__FILE__ line __LINE__)

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 *  The setting of USE_PROTOTYPES is done in machine.h
 */
#if USE_PROTOTYPES
#	define	BU_EXTERN(type_and_name,args)	extern type_and_name args
#	define	BU_ARGS(args)			args
#else
#	define	BU_EXTERN(type_and_name,args)	extern type_and_name()
#	define	BU_ARGS(args)			()
#endif

/*
 *			B U _ F O R T R A N
 *
 *  This macro is used to take the 'C' function name,
 *  and convert it at compile time to the
 *  FORTRAN calling convention used for this particular system.
 *
 *  Both lower-case and upper-case alternatives have to be provided
 *  because there is no way to get the C preprocessor to change the
 *  case of a token.
 */
#if CRAY
#	define	BU_FORTRAN(lc,uc)	uc
#endif
#if defined(apollo) || defined(mips) || defined(aux)
	/* Lower case, with a trailing underscore */
#ifdef __STDC__
#	define	BU_FORTRAN(lc,uc)	lc ## _
#else
#	define	BU_FORTRAN(lc,uc)	lc/**/_
#endif
#endif
#if !defined(BU_FORTRAN)
#	define	BU_FORTRAN(lc,uc)	lc
#endif

/*
 * Handy memory allocator macro
 */

/* Acquire storage for a given struct, eg, BU_GETSTRUCT(ptr,structname); */
#if __STDC__
#  define BU_GETSTRUCT(_p,_str) \
	_p = (struct _str *)bu_calloc(1,sizeof(struct _str), #_str " (getstruct)" )
#  define BU_GETUNION(_p,_unn) \
	_p = (union _unn *)bu_calloc(1,sizeof(union _unn), #_unn " (getunion)")
#else
#  define BU_GETSTRUCT(_p,_str) \
	_p = (struct _str *)bu_calloc(1,sizeof(struct _str), "_str (getstruct)")
#  define BU_GETUNION(_p,_unn) \
	_p = (union _unn *)bu_calloc(1,sizeof(union _unn), "_unn (getunion)")
#endif

/*
 *                B U _ G E T T Y P E
 *
 * Acquire storage for a given TYPE, eg, BU_GETTYPE(ptr, typename);
 * Equivalent to BU_GETSTRUCT, except without the 'struct' Useful
 * for typedef'ed objects.
 */
#if __STDC__
#  define BU_GETTYPE(_p,_type) \
	_p = (_type *)bu_calloc(1,sizeof(_type), #_type " (gettype)" )
#else
#  define BU_GETTYPE(_p,_type) \
	_p = (_type *)bu_calloc(1,sizeof(_type), "_type (getstruct)")
#endif



/*
 *			B U _ C K M A G
 *
 *  Macros to check and validate a structure pointer, given that
 *  the first entry in the structure is a magic number.
 */
#ifdef NO_BOMBING_MACROS
#  define BU_CKMAG(_ptr, _magic, _str)
#  define BU_CKMAG_TCL(_interp, _ptr, _magic, _str)
#else
#  define BU_CKMAG(_ptr, _magic, _str)	\
	if( !(_ptr) || ( ((long)(_ptr)) & (sizeof(long)-1) ) || \
	    *((long *)(_ptr)) != (_magic) )  { \
		bu_badmagic( (long *)(_ptr), _magic, _str, __FILE__, __LINE__ ); \
	}
#  define BU_CKMAG_TCL(_interp, _ptr, _magic, _str)	\
	if( !(_ptr) || ( ((long)(_ptr)) & (sizeof(long)-1) ) || \
	     *((long *)(_ptr)) != (_magic) )  { \
		bu_badmagic_tcl( (_interp), (long *)(_ptr), _magic, _str, __FILE__, __LINE__ ); \
		return TCL_ERROR; \
	}
#endif

/*
 *			B U _ A S S E R T
 *
 *  Quick and easy macros to generate an informative error message and
 *  abort execution if the specified condition does not hold true.
 *
 *  Example:		BU_ASSERT_LONG( j+7, <, 42 );
 */
#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT(_equation)
#else
#  ifdef __STDC__
#    define BU_ASSERT(_equation)	\
	if( !(_equation) )  { \
		bu_log("BU_ASSERT( " #_equation " ) failed, file %s, line %d\n", \
			__FILE__, __LINE__ ); \
		bu_bomb("assertion failure\n"); \
	}
#  else
#    define BU_ASSERT(_equation)	\
	if( !(_equation) )  { \
		bu_log("BU_ASSERT( _equation ) failed, file %s, line %d\n", \
			__FILE__, __LINE__ ); \
		bu_bomb("assertion failure\n"); \
	}
#  endif
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_PTR(_lhs,_relation,_rhs)
#else
#  ifdef __STDC__
#    define BU_ASSERT_PTR(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_PTR( " #_lhs #_relation #_rhs " ) failed, lhs=x%lx, rhs=x%lx, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_PTR failure\n"); \
	}
#  else
#    define BU_ASSERT_PTR(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_PTR( _lhs _relation _rhs ) failed, lhs=x%lx, rhs=x%lx, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_PTR failure\n"); \
	}
#  endif
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_LONG(_lhs,_relation,_rhs)
#else
#  ifdef __STDC__
#    define BU_ASSERT_LONG(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_LONG( " #_lhs #_relation #_rhs " ) failed, lhs=%ld, rhs=%ld, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_LONG failure\n"); \
	}
#  else
#    define BU_ASSERT_LONG(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_LONG( _lhs _relation _rhs ) failed, lhs=%ld, rhs=%ld, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_LONG failure\n"); \
	}
#  endif
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_DOUBLE(_lhs,_relation,_rhs)
#else
#  ifdef __STDC__
#    define BU_ASSERT_DOUBLE(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_DOUBLE( " #_lhs #_relation #_rhs " ) failed, lhs=%lf, rhs=%lf, file %s, line %d\n", \
			(double)(_lhs), (double)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_DOUBLE failure\n"); \
	}
#  else
#    define BU_ASSERT_DOUBLE(_lhs,_relation,_rhs)	\
	if( !((_lhs) _relation (_rhs)) )  { \
		bu_log("BU_ASSERT_DOUBLE( _lhs _relation _rhs ) failed, lhs=%lf, rhs=%lf, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs),\
			__FILE__, __LINE__ ); \
		bu_bomb("BU_ASSERT_DOUBLE failure\n"); \
	}
#  endif
#endif

/*----------------------------------------------------------------------*/
/*
 *  Sizes of "network" format data.
 *  We use the same convention as the TCP/IP specification,
 *  namely, big-Endian, IEEE format, twos compliment.
 *  This is the BRL-CAD external data representation (XDR).
 *  See also the support routines in libbu/xdr.c
 */
#define SIZEOF_NETWORK_SHORT	2	/* htons(), bu_gshort(), bu_pshort() */
#define SIZEOF_NETWORK_LONG	4	/* htonl(), bu_glong(), bu_plong() */
#define SIZEOF_NETWORK_FLOAT	4	/* htonf() */
#define SIZEOF_NETWORK_DOUBLE	8	/* htond() */

/*----------------------------------------------------------------------*/
/* convert.c
 *
 *
 */
/*
 * Forward declarations.
 */
extern int bu_cv_itemlen(int cookie);
extern int bu_cv_cookie(char *in);
extern int bu_cv_optimize(int cookie);
extern int bu_cv_w_cookie(genptr_t, int, int, genptr_t, int, int);

extern int bu_cv_ntohss(signed short *, int, genptr_t, int);
extern int bu_cv_ntohus(unsigned short *, int, genptr_t, int);
extern int bu_cv_ntohsl(signed long int *, int, genptr_t, int);
extern int bu_cv_ntohul(unsigned long int *, int, genptr_t, int);
extern int bu_cv_htonss(genptr_t, int, signed short *, int);
extern int bu_cv_htonus(genptr_t, int, unsigned short *, int);
extern int bu_cv_htonsl(genptr_t, int, long *, int);
extern int bu_cv_htonul(genptr_t, int, unsigned long *, int);

/*
 * Theses should be moved to a header file soon.
 */
#define CV_CHANNEL_MASK	0x00ff
#define CV_HOST_MASK	0x0100
#define CV_SIGNED_MASK	0x0200
#define CV_TYPE_MASK	0x1c00  /* 0001 1100 0000 0000 */
#define CV_CONVERT_MASK 0x6000  /* 0110 0000 0000 0000 */

#define CV_TYPE_SHIFT	10
#define CV_CONVERT_SHIFT 13

#define CV_8	0x0400
#define	CV_16	0x0800
#define CV_32	0x0c00
#define CV_64	0x1000
#define CV_D	0x1400

#define CV_CLIP		0x0000
#define CV_NORMAL	0x2000
#define CV_LIT		0x4000

#define	IND_NOTSET	0
#define IND_BIG		1
#define IND_LITTLE	2
#define IND_ILL		3		/* PDP-11 */
#define IND_CRAY	4

/*----------------------------------------------------------------------*/
/* list.c */
/*
 *									*
 *			     B U _ L I S T				*
 *									*
 *			Doubly-linked list support			*
 *									*
 *  These macros assume that all user-provided structures will have	*
 *  a "struct bu_list" as their first element (often named "l" [ell]).	*
 *  Thus, a pointer to the bu_list struct is a "pun" for the		*
 *  user-provided structure as well, and the pointers can be converted	*
 *  back and forth safely with type casts.				*
 *									*
 *  Furthermore, the head of the linked list could be			*
 *  a full instance of the user-provided structure			*
 *  (although the storage-conscious programmer could make the head	*
 *  just an bu_list structure, with careful type casting).		*
 *  This results in a doubly-linked circular list, with the head	*
 *  having the same shape as all the list members.			*
 *  The application is free to make use of this symmetry and store	*
 *  data values in the head, or the extra storage in the head can	*
 *  be ignored.								*
 *									*
 *  Where a macro expects an argument "p", it should be a pointer to	*
 *  a user-provided structure.						*
 *									*
 *  Where a macro expects an argument "hp", it should be a pointer to	*
 *  a "struct bu_list" located in the list head, e.g., &(head.l).	*
 *									*
 *  Where a macro expects an argument "old", "new", or "cur", it should	*
 *  be a pointer to the "struct bu_list" located either			*
 *  in a user-provided structure, e.g. &((p)->l),			*
 *  or for the case of "old" it may also be in the list head, e.g.	*
 *	BU_LIST_INSERT( &(head.l), &((p)->l) );				*
 *									*
 *  Dequeueing the head of a list is a valid and			*
 *  well defined operation which should be performed with caution.	*
 *  Unless a pointer to some other element of the list is retained	*
 *  by the application, the rest of the linked list can no longer be	*
 *  referred to.							*
 *									*
 *  The "magic" field of the list header _must_ be set to the constant	*
 *  BU_LIST_HEAD_MAGIC, but the "magic" field of all list members	*
 *  should be established by user code, to identify the type of		*
 *  structure that the bu_list structure is embedded in.		*
 *  It is permissible for one list to contain an arbitrarily mixed	*
 *  set of user "magic" numbers, as long as the head is properly marked.*
 *									*
 *  There is a dual set of terminology used in some of the macros:	*
 *	FIRST / LAST	from the point of view of the list head		*
 *	NEXT / PREV	from the point of view of a list member		*
 *	forw / back	the actual pointer names			*
 *									*
 ************************************************************************/

struct bu_list {
	long		magic;
	struct bu_list	*forw;		/* "forward", "next" */
	struct bu_list	*back;		/* "back", "last" */
};
#define BU_LIST_HEAD_MAGIC	0x01016580	/* Magic num for list head */
#define BU_LIST_NULL	((struct bu_list *)0)

typedef struct bu_list bu_list_t;

struct bu_list *bu_list_new();
struct bu_list *bu_list_pop( struct bu_list *hp );

#define BU_LIST_CLOSE( hp ) { \
	assert( (hp) != NULL ); \
	if( (hp) == NULL ) \
		return; \
	assert( BU_LIST_IS_EMPTY( (hp) ) ); \
	bu_list_free( (hp) ); \
	bu_free( (char *)(hp), "bu_list head" ); \
}


/*
 *  Insert "new" item in front of "old" item.  Often, "old" is the head.
 *  To put the new item at the tail of the list, insert before the head, e.g.
 *	BU_LIST_INSERT( &(head.l), &((p)->l) );
 */
#define BU_LIST_INSERT(old,new)	{ \
	(new)->back = (old)->back; \
	(old)->back = (new); \
	(new)->forw = (old); \
	(new)->back->forw = (new);  }

/*
 *  Append "new" item after "old" item.  Often, "old" is the head.
 *  To put the new item at the head of the list, append after the head, e.g.
 *	BU_LIST_APPEND( &(head.l), &((p)->l) );
 */
#define BU_LIST_APPEND(old,new)	{ \
	(new)->forw = (old)->forw; \
	(new)->back = (old); \
	(old)->forw = (new); \
	(new)->forw->back = (new);  }

/* Dequeue "cur" item from anywhere in doubly-linked list */
#define BU_LIST_DEQUEUE(cur)	{ \
	(cur)->forw->back = (cur)->back; \
	(cur)->back->forw = (cur)->forw; \
	(cur)->forw = (cur)->back = BU_LIST_NULL;  /* sanity */ }

/* Dequeue "cur" but do not fix its links */
#define BU_LIST_DQ(cur) {\
	(cur)->forw->back = (cur)->back; \
	(cur)->back->forw = (cur)->forw; }

#define BU_LIST_DQ_T(cur, type) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(type *)(cur) )

/* This version of BU_LIST_DEQUEUE uses the comma operator
 * inorder to return a typecast version of the dequeued pointer
 */
#define BU_LIST_DEQUEUE_T( cur, type ) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(cur)->forw = (cur)->back = BU_LIST_NULL, \
	(type *)(cur) )


/*
 *  The Stack Discipline
 *
 *  BU_LIST_PUSH places p at the tail of hp.
 *  BU_LIST_POP  sets p to last element in hp's list (else NULL)
 *		  and, if p is non-null, dequeues it.
 */
#define BU_LIST_PUSH(hp,p)					\
	BU_LIST_APPEND(hp, (struct bu_list *)(p))

#define BU_LIST_POP(structure,hp,p)				\
	{							\
		if (BU_LIST_NON_EMPTY(hp))				\
		{							\
		    (p) = ((struct structure *)((hp)->forw));		\
		    BU_LIST_DEQUEUE((struct bu_list *)(p));		\
		}							\
		else							\
		     (p) = (struct structure *) 0;			\
	}

#define BU_LIST_POP_T(hp, type )				\
	(type *)bu_list_pop( hp )

/*
 *  "Bulk transfer" all elements from the list headed by src_hd
 *  onto the list headed by dest_hd, without examining every element
 *  in the list.  src_hd is left with a valid but empty list.
 *  
 *  BU_LIST_INSERT_LIST places src_hd elements at head of dest_hd list,
 *  BU_LIST_APPEND_LIST places src_hd elements at end of dest_hd list.
 */
#define BU_LIST_INSERT_LIST(dest_hp,src_hp) \
	if( BU_LIST_NON_EMPTY(src_hp) )  { \
		register struct bu_list	*_first = (src_hp)->forw; \
		register struct bu_list	*_last = (src_hp)->back; \
		(dest_hp)->forw->back = _last; \
		_last->forw = (dest_hp)->forw; \
		(dest_hp)->forw = _first; \
		_first->back = (dest_hp); \
		(src_hp)->forw = (src_hp)->back = (src_hp); \
	}

#define BU_LIST_APPEND_LIST(dest_hp,src_hp) \
	if( BU_LIST_NON_EMPTY(src_hp) )  {\
		register struct bu_list	*_first = (src_hp)->forw; \
		register struct bu_list	*_last = (src_hp)->back; \
		_first->back = (dest_hp)->back; \
		(dest_hp)->back->forw = _first; \
		(dest_hp)->back = _last; \
		_last->forw = (dest_hp); \
		(src_hp)->forw = (src_hp)->back = (src_hp); \
	}

/* Test if a doubly linked list is empty, given head pointer */
#define BU_LIST_IS_EMPTY(hp)	((hp)->forw == (hp))
#define BU_LIST_NON_EMPTY(hp)	((hp)->forw != (hp))
#define BU_LIST_NON_EMPTY_P(p,structure,hp)	\
	(((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))
#define BU_LIST_IS_CLEAR(hp)	((hp)->magic == 0 && \
			(hp)->forw == BU_LIST_NULL && \
			(hp)->back == BU_LIST_NULL)

/* Handle list initialization */
#define	BU_LIST_UNINITIALIZED(hp)	((hp)->forw == BU_LIST_NULL)
#define	BU_LIST_IS_INITIALIZED(hp)	((hp)->forw != BU_LIST_NULL)
#define BU_LIST_INIT(hp)	{ \
	(hp)->forw = (hp)->back = (hp); \
	(hp)->magic = BU_LIST_HEAD_MAGIC;	/* used by circ. macros */ }
#define BU_LIST_MAGIC_SET(hp,val)	{(hp)->magic = (val);}
#define BU_LIST_MAGIC_OK(hp,val)	((hp)->magic == (val))
#define BU_LIST_MAGIC_WRONG(hp,val)	((hp)->magic != (val))

/* Return re-cast pointer to first element on list.
 * No checking is performed to see if list is empty.
 */
#define BU_LIST_LAST(structure,hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_BACK(structure,hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_PREV(structure,hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_FIRST(structure,hp)	\
	((struct structure *)((hp)->forw))
#define BU_LIST_FORW(structure,hp)	\
	((struct structure *)((hp)->forw))
#define BU_LIST_NEXT(structure,hp)	\
	((struct structure *)((hp)->forw))

/* Boolean test to see if current list element is the head */
#define BU_LIST_IS_HEAD(p,hp)	\
	(((struct bu_list *)(p)) == (hp))
#define BU_LIST_NOT_HEAD(p,hp)	\
	(((struct bu_list *)(p)) != (hp))
#define BU_CK_LIST_HEAD( _p )	BU_CKMAG( (_p), BU_LIST_HEAD_MAGIC, "bu_list")

/* Boolean test to see if previous list element is the head */
#define BU_LIST_PREV_IS_HEAD(p,hp)\
	(((struct bu_list *)(p))->back == (hp))
#define BU_LIST_PREV_NOT_HEAD(p,hp)\
	(((struct bu_list *)(p))->back != (hp))

/* Boolean test to see if the next list element is the head */
#define BU_LIST_NEXT_IS_HEAD(p,hp)	\
	(((struct bu_list *)(p))->forw == (hp))
#define BU_LIST_NEXT_NOT_HEAD(p,hp)	\
	(((struct bu_list *)(p))->forw != (hp))

#define BU_LIST_EACH( hp, p, type ) \
	 for( (p)=(type *)BU_LIST_FIRST(bu_list,hp); \
	      BU_LIST_NOT_HEAD(p,hp); \
	      (p)=(type *)BU_LIST_PNEXT(bu_list,p) ) \

#define BU_LIST_REVEACH( hp, p, type ) \
	 for( (p)=(type *)BU_LIST_LAST(bu_list,hp); \
	      BU_LIST_NOT_HEAD(p,hp); \
	      (p)=(type *)BU_LIST_PREV(bu_list,((struct bu_list *)(p))) ) \

#define BU_LIST_TAIL( hp, start, p, type ) \
	 for( (p)=(type *)start ; \
	      BU_LIST_NOT_HEAD(p,hp); \
	      (p)=(type *)BU_LIST_PNEXT(bu_list,(p)) )

/*
 *  Intended as innards for a for() loop to visit all nodes on list, e.g.:
 *	for( BU_LIST_FOR( p, structure, hp ) )  {
 *		work_on( p );
 *	}
 */
#define BU_LIST_FOR(p,structure,hp)	\
	(p)=BU_LIST_FIRST(structure,hp); \
	BU_LIST_NOT_HEAD(p,hp); \
	(p)=BU_LIST_PNEXT(structure,p)

#define BU_LIST_FOR_BACKWARDS(p,structure,hp)	\
	(p)=BU_LIST_LAST(structure,hp); \
	BU_LIST_NOT_HEAD(p,hp); \
	(p)=BU_LIST_PLAST(structure,p)

/*
 *  Process all the list members except hp and the actual head.
 *  Useful when starting somewhere besides the head.
 */
#define BU_LIST_FOR_CIRC(p,structure,hp)	\
	(p)=BU_LIST_PNEXT_CIRC(structure,hp); \
	(p) != (hp); \
	(p)=BU_LIST_PNEXT_CIRC(structure,p)

/*
 *  Intended as innards for a for() loop to visit elements of two lists
 *	in tandem, e.g.:
 *	    for (BU_LIST_FOR2(p1, p2, structure, hp1, hp2) ) {
 *		    process( p1, p2 );
 *	    }
 */
#define	BU_LIST_FOR2(p1,p2,structure,hp1,hp2)				\
		(p1)=BU_LIST_FIRST(structure,hp1),			\
		(p2)=BU_LIST_FIRST(structure,hp2);			\
		BU_LIST_NOT_HEAD((struct bu_list *)(p1),(hp1)) &&	\
		BU_LIST_NOT_HEAD((struct bu_list *)(p2),(hp2));		\
		(p1)=BU_LIST_NEXT(structure,(struct bu_list *)(p1)),	\
		(p2)=BU_LIST_NEXT(structure,(struct bu_list *)(p2))

/*
 *  Innards for a while() loop that constantly picks off the first element.
 *  Useful mostly for a loop that will dequeue every list element, e.g.:
 *	while( BU_LIST_WHILE(p, structure, hp) )  {
 *		BU_LIST_DEQUEUE( &(p->l) );
 *		free( (char *)p );
 *	}
 */
#define BU_LIST_WHILE(p,structure,hp)	\
	(((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))

/* Return the magic number of the first (or last) item on a list */
#define BU_LIST_FIRST_MAGIC(hp)		((hp)->forw->magic)
#define BU_LIST_LAST_MAGIC(hp)		((hp)->back->magic)

/* Return pointer to next (or previous) element, which may be the head */
#define BU_LIST_PNEXT(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->forw))
#define BU_LIST_PLAST(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->back))

/* Return pointer two links away, which may include the head */
#define BU_LIST_PNEXT_PNEXT(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->forw->forw))
#define BU_LIST_PNEXT_PLAST(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->forw->back))
#define BU_LIST_PLAST_PNEXT(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->back->forw))
#define BU_LIST_PLAST_PLAST(structure,p)	\
	((struct structure *)(((struct bu_list *)(p))->back->back))

/* Return pointer to circular next element; ie, ignoring the list head */
#define BU_LIST_PNEXT_CIRC(structure,p)	\
	((BU_LIST_FIRST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
		BU_LIST_PNEXT_PNEXT(structure,(struct bu_list *)(p)) : \
		BU_LIST_PNEXT(structure,p) )

/* Return pointer to circular last element; ie, ignoring the list head */
#define BU_LIST_PPREV_CIRC(structure,p)	\
	((BU_LIST_LAST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
		BU_LIST_PLAST_PLAST(structure,(struct bu_list *)(p)) : \
		BU_LIST_PLAST(structure,p) )

/*
 *  Support for membership on multiple linked lists.
 *
 *  When a structure of type '_type' contains more than one bu_list structure
 *  within it (such as the NMG edgeuse), this macro can be used to convert
 *  a pointer '_ptr2' to a "midway" bu_list structure (an element called
 *  '_name2' in structure '_type') back into a pointer to the overall
 *  enclosing structure.  Examples:
 *
 *  eu = BU_LIST_MAIN_PTR( edgeuse, midway, l2 );
 *
 *  eu1 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg1->eu_hd2), l2);
 */  
#define BU_LIST_MAIN_PTR(_type, _ptr2, _name2)	\
	((struct _type *)(((char *)(_ptr2)) - offsetof(struct _type, _name2.magic)))

/*----------------------------------------------------------------------*/
/* bitv.c */
/*
 *			B U _ B I T V
 *
 *  Bit vector data structure.
 *
 *  bu_bitv uses a little-endian encoding, placing bit 0 on the
 *  right side of the 0th word.
 *  This is done only because left-shifting a 1 can be done in an
 *  efficient word-length-independent manner;
 *  going the other way would require a compile-time constant with
 *  only the sign bit set, and an unsigned right shift, which some
 *  machines don't have in hardware, or an extra subtraction.
 *
 *  Application code should *never* peak at the bit-buffer; use the macros.
 *
 *  The external hex form is most signigicant byte first (bit 0 is at the right).
 *  Note that MUVES does it differently
 */
struct bu_bitv {
	struct bu_list	l;		/* linked list for caller's use */
	unsigned int	nbits;		/* actual size of bits[], in bits */
	bitv_t		bits[2];	/* variable size array */
};

#define BU_BITV_MAGIC		0x62697476	/* 'bitv' */
#define BU_CK_BITV(_vp)		BU_CKMAG(_vp, BU_BITV_MAGIC, "bu_bitv")

/*
 *  Bit-string manipulators for arbitrarily long bit strings
 *  stored as an array of bitv_t's.
 */
#define BU_BITS2BYTES(_nb)	(BU_BITS2WORDS(_nb)*sizeof(bitv_t))
#define BU_BITS2WORDS(_nb)	(((_nb)+BITV_MASK)>>BITV_SHIFT)
#define BU_WORDS2BITS(_nw)	((_nw)*sizeof(bitv_t)*8)

#if 1
#define BU_BITTEST(_bv,bit)	\
	(((_bv)->bits[(bit)>>BITV_SHIFT] & (((bitv_t)1)<<((bit)&BITV_MASK)))!=0)
#else
static __inline__ int BU_BITTEST(volatile void * addr, int nr)
{
        int oldbit;

        __asm__ __volatile__(
                "btl %2,%1\n\tsbbl %0,%0"
                :"=r" (oldbit)
                :"m" (addr),"Ir" (nr));
        return oldbit;
}
#endif

#define BU_BITSET(_bv,bit)	\
	((_bv)->bits[(bit)>>BITV_SHIFT] |= (((bitv_t)1)<<((bit)&BITV_MASK)))
#define BU_BITCLR(_bv,bit)	\
	((_bv)->bits[(bit)>>BITV_SHIFT] &= ~(((bitv_t)1)<<((bit)&BITV_MASK)))
#define BU_BITV_ZEROALL(_bv)	\
	{ memset( (char *)((_bv)->bits), 0, BU_BITS2BYTES( (_bv)->nbits ) ); }

/* This is not done by default for performance reasons */
#ifdef NO_BOMBING_MACROS
#  define BU_BITV_BITNUM_CHECK(_bv,_bit)
#else
#  define BU_BITV_BITNUM_CHECK(_bv,_bit)	/* Validate bit number */ \
	if( ((unsigned)(_bit)) >= (_bv)->nbits )  {\
		bu_log("BU_BITV_BITNUM_CHECK bit number (%u) out of range (0..%u)\n", \
			((unsigned)(_bit)), (_bv)->nbits); \
		bu_bomb("process self-terminating\n");\
	}
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_BITV_NBITS_CHECK(_bv,_nbits)
#else
#  define BU_BITV_NBITS_CHECK(_bv,_nbits)	/* Validate number of bits */ \
	if( ((unsigned)(_nbits)) > (_bv)->nbits )  {\
		bu_log("BU_BITV_NBITS_CHECK number of bits (%u) out of range (> %u)", \
			((unsigned)(_nbits)), (_bv)->nbits ); \
		bu_bomb("process self-terminating"); \
		}
#endif


/*
 *  Macros to efficiently find all the ONE bits in a bit vector.
 *  Counts words down, counts bits in words going up, for speed & portability.
 *  It does not matter if the shift causes the sign bit to smear to the right.
 *
 *  Example:
 *	BU_BITV_LOOP_START(bv)  {
 *		fiddle(BU_BITV_LOOP_INDEX);
 *	} BU_BITV_LOOP_END;
 */
#define BU_BITV_LOOP_START(_bv)	\
{ \
	register int		_wd;	/* Current word number */  \
	BU_CK_BITV(_bv); \
	for( _wd=BU_BITS2WORDS((_bv)->nbits)-1; _wd>=0; _wd-- )  {  \
		register int	_b;	/* Current bit-in-word number */  \
		register bitv_t	_val;	/* Current word value */  \
		if((_val = (_bv)->bits[_wd])==0) continue;  \
		for(_b=0; _b < BITV_MASK+1; _b++, _val >>= 1 ) { \
			if( !(_val & 1) )  continue;

/*
 *  This macro is valid only between a BU_BITV_LOOP_START/LOOP_END pair,
 *  and gives the bit number of the current iteration.
 */
#define BU_BITV_LOOP_INDEX	((_wd << BITV_SHIFT) | _b)

#define BU_BITV_LOOP_END	\
		} /* end for(_b) */ \
	} /* end for(_wd) */ \
} /* end block */

/*----------------------------------------------------------------------*/
/* hist.c */

/*
 *			B U _ H I S T
 */
struct bu_hist  {
	long		magic;
	fastf_t		hg_min;		/* minimum value */
	fastf_t		hg_max;		/* maximum value */
	fastf_t		hg_clumpsize;	/* (max-min+1)/nbins+1 */
	long		hg_nsamples;	/* total number of samples spread into histogram */
	long		hg_nbins;	/* # of bins in hg_bins[] */
	long		*hg_bins;	/* array of counters */
};
#define BU_HIST_MAGIC	0x48697374	/* Hist */
#define BU_CK_HIST(_p)	BU_CKMAG(_p, BU_HIST_MAGIC, "struct bu_hist")

#define BU_HIST_TALLY( _hp, _val )	{ \
	if( (_val) <= (_hp)->hg_min )  { \
		(_hp)->hg_bins[0]++; \
	} else if( (_val) >= (_hp)->hg_max )  { \
		(_hp)->hg_bins[(_hp)->hg_nbins]++; \
	} else { \
		(_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)]++; \
	} \
	(_hp)->hg_nsamples++;  }

#define BU_HIST_TALLY_MULTIPLE( _hp, _val, _count )	{ \
	register int	__count = (_count); \
	if( (_val) <= (_hp)->hg_min )  { \
		(_hp)->hg_bins[0] += __count; \
	} else if( (_val) >= (_hp)->hg_max )  { \
		(_hp)->hg_bins[(_hp)->hg_nbins] += __count; \
	} else { \
		(_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)] += __count; \
	} \
	(_hp)->hg_nsamples += __count;  }


/*----------------------------------------------------------------------*/
/* ptbl.c */
/*
 *  Support for generalized "pointer tables".
 */
#define BU_PTBL_INIT	0	/* initialize table pointer struct & get storage */
#define BU_PTBL_INS	1	/* insert an item (long *) into a table */
#define BU_PTBL_LOC 	2	/* locate a (long *) in an existing table */
#define BU_PTBL_FREE	3	/* deallocate buffer associated with a table */
#define BU_PTBL_RST	4	/* empty a table, but keep storage on hand */
#define BU_PTBL_CAT	5	/* catenate one table onto another */
#define BU_PTBL_RM	6	/* remove all occurrences of an item from a table */
#define BU_PTBL_INS_UNIQUE 7	/* insert item into table, if not present */
#define BU_PTBL_ZERO	8	/* replace all occurrences of an item by 0 */

struct bu_ptbl {
	struct bu_list	l;	/* linked list for caller's use */
	int		end;	/* index into buffer of first available location */
	int		blen;	/* # of (long *)'s worth of storage at *buffer */
	long 		**buffer; /* data storage area */
};
#define BU_PTBL_MAGIC		0x7074626c		/* "ptbl" */
#define BU_CK_PTBL(_p)		BU_CKMAG(_p, BU_PTBL_MAGIC, "bu_ptbl")

/*
 *  For those routines that have to "peek" into the ptbl a little bit.
 */
#define BU_PTBL_BASEADDR(ptbl)	((ptbl)->buffer)
#define BU_PTBL_LASTADDR(ptbl)	((ptbl)->buffer + (ptbl)->end - 1)
#define BU_PTBL_END(ptbl)	((ptbl)->end)
#define BU_PTBL_LEN(p)	((p)->end)
#define BU_PTBL_GET(ptbl,i)	((ptbl)->buffer[(i)])
#define BU_PTBL_SET(ptbl,i,val)	((ptbl)->buffer[(i)] = (long*)(val))
#define BU_PTBL_TEST(ptbl)	((ptbl)->l.magic == BU_PTBL_MAGIC)
#define BU_PTBL_CLEAR_I(_ptbl, _i) ((_ptbl)->buffer[(_i)] = (long *)0)

/*
 *  A handy way to visit all the elements of the table is:
 *
 *	struct edgeuse **eup;
 *	for( eup = (struct edgeuse **)BU_PTBL_LASTADDR(&eutab);
 *	     eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&eutab); eup-- )  {
 *		NMG_CK_EDGEUSE(*eup);
 *	}
 *  or
 *	for( BU_PTBL_FOR( eup, (struct edgeuse **), &eutab ) )  {
 *		NMG_CK_EDGEUSE(*eup);
 *	}
 */
#define BU_PTBL_FOR(ip,cast,ptbl)	\
    ip = cast BU_PTBL_LASTADDR(ptbl); ip >= cast BU_PTBL_BASEADDR(ptbl); ip--


/* vlist, vlblock?  But they use vmath.h... */

/*----------------------------------------------------------------------*/
/* mappedfile.c */
/*
 *			B U _ M A P P E D _ F I L E
 *
 *  Structure for opening a mapped file.
 *  Each file is opened and mapped only once (per application,
 *  as tagged by the string in "appl" field).
 *  Subsequent opens require an exact match on both strings.
 *
 *  Before allocating apbuf and performing data conversion into it,
 *  openers should check to see if the file has already been opened and
 *  converted previously.
 *
 *  When used in RT, the mapped files are not closed at the end of a frame,
 *  so that subsequent frames may take advantage of the large data files
 *  having already been read and converted.
 *  Examples include EBMs, texture maps, and height fields.
 *
 *  For appl == "db_i", file is a ".g" database & apbuf is (struct db_i *).
 */
struct bu_mapped_file {
	struct bu_list	l;
	char		*name;		/* bu_strdup() of file name */
	genptr_t	buf;		/* In-memory copy of file (may be mmapped) */
	long		buflen;		/* # bytes in 'buf' */
	int		is_mapped;	/* 1=mmap() used, 0=bu_malloc/fread */
	char		*appl;		/* bu_strdup() of tag for application using 'apbuf' */
	genptr_t	apbuf;		/* opt: application-specific buffer */
	long		apbuflen;	/* opt: application-specific buflen */
	time_t		modtime;	/* date stamp, in case file is modified */
	int		uses;		/* # ptrs to this struct handed out */
	int		dont_restat;	/* 1=on subsequent opens, don't re-stat() */
};
#define BU_MAPPED_FILE_MAGIC	0x4d617066	/* Mapf */
#define BU_CK_MAPPED_FILE(_p)	BU_CKMAG(_p, BU_MAPPED_FILE_MAGIC, "bu_mapped_file")

/*----------------------------------------------------------------------*/

/* formerly rt_g.rtg_logindent, now use bu_log_indent_delta() */
typedef int (*bu_hook_t)BU_ARGS((genptr_t, genptr_t));

struct bu_hook_list {
	struct bu_list	l;
	bu_hook_t	hookfunc;
	genptr_t 	clientdata;
};

#define BUHOOK_NULL 0
#define BUHOOK_LIST_MAGIC	0x90d5dead	/* Nietzsche? */
#define BUHOOK_LIST_NULL	((struct bu_hook_list *) 0)

extern struct bu_hook_list bu_log_hook_list;
extern struct bu_hook_list bu_bomb_hook_list;

/*----------------------------------------------------------------------*/
/* avs.c */
/*
 *  Attribute/value sets
 */

/*
 *			B U _ A T T R I B U T E _ V A L U E _ P A I R
 *
 *  These strings may or may not be individually allocated,
 *  it depends on usage.
 */
struct bu_attribute_value_pair {
	const char	*name;
	const char	*value;
};

/*
 *			B U _ A T T R I B U T E _ V A L U E _ S E T
 *
 *  A variable-sized attribute-value-pair array.
 *
 *  avp points to an array of [max] slots.
 *  The interface routines will realloc to extend as needed.
 *
 *  In general,
 *  each of the names and values is a local copy made with bu_strdup(),
 *  and each string needs to be freed individually.
 *  However, if a name or value pointer is between
 *  readonly_min and readonly_max, then it is part of a big malloc
 *  block that is being freed by the caller, and should not be individually
 *  freed.
 */
struct bu_attribute_value_set {
	long				magic;
	int				count;	/* # valid entries in avp */
	int				max;	/* # allocated slots in avp */
	struct bu_attribute_value_pair	*avp;	/* array[max] */
	char				*readonly_min;
	char				*readonly_max;
};
#define BU_AVS_MAGIC		0x41765321	/* AvS! */
#define BU_CK_AVS(_avp)		BU_CKMAG(_avp, BU_AVS_MAGIC, "bu_attribute_value_set")

#define BU_AVS_FOR(_pp, _avp)	\
	(_pp) = &(_avp)->avp[(_avp)->count-1]; (_pp) >= (_avp)->avp; (_pp)--

/*
 *  Some (but not all) attribute name and value string pointers are
 *  taken from an on-disk format bu_external block,
 *  while others have been bu_strdup()ed and need to be freed.
 *  This macro indicates whether the pointer needs to be freed or not.
 */
#define AVS_IS_FREEABLE(_avsp, _p)	\
	( (_avsp)->readonly_max == NULL || \
	    ((_p) < (_avsp)->readonly_min || (_p) > (_avsp)->readonly_max) )

/*----------------------------------------------------------------------*/
/* vls.c */
/*
 *  Variable Length Strings: bu_vls support (formerly rt_vls in h/rtstring.h)
 */
struct bu_vls  {
	long	vls_magic;
	char	*vls_str;	/* Dynamic memory for buffer */
	int	vls_offset;	/* Offset into vls_str where data is good */
	int	vls_len;	/* Length, not counting the null */
	int	vls_max;
};
#define BU_VLS_MAGIC		0x89333bbb
#define BU_CK_VLS(_vp)		BU_CKMAG(_vp, BU_VLS_MAGIC, "bu_vls")
#define BU_VLS_IS_INITIALIZED(_vp)	\
	((_vp) && ((_vp)->vls_magic == BU_VLS_MAGIC))

/*
 *  Section for manifest constants for bu_semaphore_acquire()
 */
#define BU_SEM_SYSCALL	0
#define BU_SEM_LISTS	1
#define BU_SEM_BN_NOISE	2
#define BU_SEM_MAPPEDFILE 3
#define BU_SEM_LAST	(BU_SEM_MAPPEDFILE+1)	/* allocate this many for LIBBU+LIBBN */
/*
 *  Automatic restart capability in bu_bomb().
 *  The return from BU_SETJUMP is the return from the setjmp().
 *  It is 0 on the first pass through, and non-zero when
 *  re-entered via a longjmp() from bu_bomb().
 *  This is only safe to use in non-parallel applications.
 */
#define BU_SETJUMP	setjmp((bu_setjmp_valid=1,bu_jmpbuf))
#define BU_UNSETJUMP	(bu_setjmp_valid=0)
/* These are global because BU_SETJUMP must be macro.  Please don't touch. */
extern int	bu_setjmp_valid;		/* !0 = bu_jmpbuf is valid */
extern jmp_buf	bu_jmpbuf;			/* for BU_SETJMP() */

/*-------------------------------------------------------------------------*/
/* 			B U _ M R O
 *
 *	Support for Multiply Represented Objects
 */

struct bu_mro {
	long		magic;
	struct bu_vls	string_rep;
	char		long_rep_is_valid;
	long		long_rep;
	char		double_rep_is_valid;
	double		double_rep;
};

#define BU_MRO_MAGIC	0x4D524F4F	/* MROO */
#define BU_CK_MRO(_vp)		BU_CKMAG(_vp, BU_MRO_MAGIC, "bu_mro")

#define BU_MRO_INVALIDATE(_p ) {\
	_p->long_rep_is_valid = '\0';\
	_p->double_rep_is_valid = '\0';\
}

#define BU_MRO_GETDOUBLE( _p ) ( _p->double_rep_is_valid ? _p->double_rep : \
	(_p->double_rep = strtod( bu_vls_addr( &_p->string_rep ), NULL ), \
	( _p->double_rep_is_valid='y', _p->double_rep ) ) )

#define BU_MRO_GETLONG( _p ) ( _p->long_rep_is_valid ? _p->long_rep : \
	(_p->long_rep = strtol( bu_vls_addr( &_p->string_rep ), NULL, 10 ), \
	( _p->long_rep_is_valid='y', _p->long_rep ) ) )

#define BU_MRO_GETSTRING( _p ) bu_vls_addr( &_p->string_rep )

#define BU_MRO_STRLEN( _p ) bu_vls_strlen( &_p->string_rep )

/*----------------------------------------------------------------------*/
/*
 * Section for BU_DEBUG values
 *
 * These can be set from the command-line of RT-compatible programs
 * using the "-! ###" option.
 */
extern int	bu_debug;
/* These definitions are each for one bit */
#define BU_DEBUG_OFF		0	/* No debugging */

#define BU_DEBUG_COREDUMP	0x00000001	/* 001 If set, bu_bomb() will dump core */

#define BU_DEBUG_MEM_CHECK	0x00000002	/* 002 Mem barrier & leak checking */
#define BU_DEBUG_MEM_LOG	0x00000004	/* 003 Print all dynamic memory operations */
#define BU_DEBUG_DB		0x00000008	/* 004 Database debugging */
	
#define BU_DEBUG_PARALLEL	0x00000010	/* 005 parallel support */

#define BU_DEBUG_MATH		0x00000100	/* 011 Fundamental math routines (plane.c, mat.c) */
#define BU_DEBUG_PTBL		0x00000200	/* 012 bu_ptbl_*() logging */
#define BU_DEBUG_AVS		0x00000400	/* 013 bu_avs_*() logging */
#define BU_DEBUG_MAPPED_FILE	0x00000800	/* 014 bu_mapped_file logging */

#define BU_DEBUG_TABDATA	0x00010000	/* 025 LIBBN: tabdata */

/* Format string for bu_printb() */
#define BU_DEBUG_FORMAT	\
"\020\
\025TABDATA\
\015?\
\014MAPPED_FILE\013AVS\012PTBL\011MATH\010?\7?\6?\5PARALLEL\
\4?\3MEM_LOG\2MEM_CHECK\1COREDUMP"

/*----------------------------------------------------------------------*/
/* parse.c */
/*
 *	Structure parse/print
 *
 *  Definitions and data structures needed for routines that assign values
 *  to elements of arbitrary data structures, the layout of which is
 *  described by tables of "bu_structparse" structures.
 *
 *  The general problem of word-addressed hardware
 *  where (int *) and (char *) have different representations
 *  is handled in the parsing routines that use sp_offset,
 *  because of the limitations placed on compile-time initializers.
 */
#if __STDC__ && !defined(ipsc860)
#	define bu_offsetofarray(_t, _m)	offsetof(_t, _m[0])
#else
#	if !defined(offsetof)
#		define bu_offsetof(_t, _m)		(int)(&(((_t *)0)->_m))
#	else
#		define bu_offsetof(_t, _m)	offsetof(_t, _m)
#	endif
#	define bu_offsetofarray(_t, _m)	(int)( (((_t *)0)->_m))
#endif

/*
 *  Convert address of global data object into byte "offset" from address 0.
 */
#if CRAY
#	define bu_byteoffset(_i)	(((int)&(_i)))	/* actually a word offset */
#else
#  if IRIX > 5 && _MIPS_SIM != _MIPS_SIM_ABI32
#	define bu_byteoffset(_i)	((size_t)__INTADDR__(&(_i)))
#  else
#    if sgi || __convexc__ || ultrix || _HPUX_SOURCE
	/* "Lazy" way.  Works on reasonable machines with byte addressing */
#	define bu_byteoffset(_i)	((int)((char *)&(_i)))
#    else
	/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#	define bu_byteoffset(_i)	((int)(((char *)&(_i))-((char *)0)))
#    endif
#  endif
#endif

/* The "bu_structparse" struct describes one element of a structure.
 * Collections of these are combined to describe entire structures (or at
 * least those portions for which parse/print/import/export support is
 * desired.  For example:
 *
 * struct data_structure {
 *	char	a_char;
 *	char	str[32];
 *	short	a_short;
 *	int	a_int;
 *	double	a_double;
 * } 
 *
 * struct data_structure data_default = 
 *	{ 'c', "the default string", 32767, 1, 1.0 };
 *
 * struct data_structure my_values;
 *
 * struct bu_structparse data_sp[] ={
 *
 * {"%c", 1,  "a_char",   bu_offsetof(data_structure, a_char),
 *	BU_STRUCTPARSE_FUNC_NULL,
 *	"a single character",	(void*)&default.a_char }, 
 *
 * {"%s", 32, "str",      bu_offsetofarray(data_structure, str),
 *	BU_STRUCTPARSE_FUNC_NULL,
 *	"This is a full character string", (void*)default.str }, }, 
 *
 * {"%i", 1,  "a_short",  bu_offsetof(data_structure, a_short),
 *	BU_STRUCTPARSE_FUNC_NULL,
 *	"A 16bit integer",	(void*)&default.a_short },
 *
 * {"%d", 1,  "a_int",    bu_offsetof(data_structure, a_int),
 *	BU_STRUCTPARSE_FUNC_NULL,
 *	"A full integer",	(void*)&default.a_int },
 *
 * {"%f", 1,  "a_double", bu_offsetof(data_structure, a_double),
 *	BU_STRUCTPARSE_FUNC_NULL,
 *	"A double-precision floating point value",  (void*)&default.a_double },
 *
 * { "", 0, (char *)NULL, 0,
 *	BU_STRUCTPARSE_FUNC_NULL, 
 *	(char *)NULL, (void *)NULL }
 *
 * };
 *
 *
 * To parse a string, call:
 *
 *	bu_struct_parse( vls_string, data_sp, (char *)my_values)
 *
 * this will parse the vls string and assign values to the members of the
 * structure my_values
 *
 *  A gross hack:  To set global variables (or others for that matter) you
 *	can store the actual address of the variable in the sp_offset field
 *	and pass a null pointer as the last argument to bu_struct_parse().
 *	If you don't understand why this would work, you probably shouldn't
 *	use this technique.
 */
struct bu_structparse {
	char		sp_fmt[4];		/* "i" or "%f", etc */
	long		sp_count;		/* number of elements */
	char		*sp_name;		/* Element's symbolic name */
	long		sp_offset;		/* Byte offset in struct */
	void		(*sp_hook)();		/* Optional hooked function, or indir ptr */
	char		*sp_desc;		/* description of element */
	void		*sp_default;		/* ptr to default value */
};
#define BU_STRUCTPARSE_FUNC_NULL	((void (*)())0)




/*----------------------------------------------------------------------*/
/*
 *			B U _ E X T E R N A L
 *
 *  An "opaque" handle for holding onto objects,
 *  typically in some kind of external form that is not directly
 *  usable without passing through an "importation" function.
 * A "bu_external" struct holds the "external binary" representation of a
 * structure or other block of arbitrary data.
 */
struct bu_external  {
	long	ext_magic;
	long	ext_nbytes;
	genptr_t ext_buf;
};
#define BU_EXTERNAL_MAGIC	0x768dbbd0
#define BU_INIT_EXTERNAL(_p)	{(_p)->ext_magic = BU_EXTERNAL_MAGIC; \
	(_p)->ext_buf = (genptr_t)NULL; (_p)->ext_nbytes = 0;}
#define BU_CK_EXTERNAL(_p)	BU_CKMAG(_p, BU_EXTERNAL_MAGIC, "bu_external")

/*----------------------------------------------------------------------*/
/* color.c */
#define	HUE		0
#define	SAT		1
#define	VAL		2
#define	ACHROMATIC	-1.0

struct bu_color
{
    long	buc_magic;
    fastf_t	buc_rgb[3];
};
#define	BU_COLOR_MAGIC		0x6275636c
#define	BU_COLOR_NULL		((struct bu_color *) 0)
#define BU_CK_COLOR(_bp)	BU_CKMAG(_bp, BU_COLOR_MAGIC, "bu_color")

/*----------------------------------------------------------------------*/
/* red-black tree support */
/*
 *	The data structures and constants for red-black trees.
 *
 *	Many of these routines are based on the algorithms in chapter 13
 *	of T. H. Cormen, C. E. Leiserson, and R. L. Rivest,
 *	_Introduction to algorithms_, MIT Press, Cambridge, MA, 1990.
 *
 *	Author:	Paul Tanenbaum
 *
 */

/*
 *			    B U _ R B _ L I S T
 *
 *		    List of nodes or packages
 *
 *	The red-black tree package uses this structure to maintain lists
 *	of all the nodes and all the packages in the tree.  Applications
 *	should not muck with these things.  They are maintained only to
 *	facilitate freeing bu_rb_trees.
 */
struct bu_rb_list
{
    struct bu_list	l;
    union
    {
	struct bu_rb_node    *rbl_n;
	struct bu_rb_package *rbl_p;
    }			rbl_u;
};
#define	rbl_magic	l.magic
#define	rbl_node	rbl_u.rbl_n
#define	rbl_package	rbl_u.rbl_p
#define	BU_RB_LIST_NULL	((struct bu_rb_list *) 0)


/*
 *			B U _ R B _ T R E E
 *
 *	This is the only data structure used in the red-black tree package
 *	to which application software need make any explicit reference.
 *
 *	The members of this structure are grouped into three
 *	classes:
 *	    Class I:	Reading is appropriate, when necessary,
 *			but applications should not modify.
 *	    Class II:	Reading and modifying are both appropriate,
 *			when necessary.
 *	    Class III:	All access should be through routines
 *			provided in the package.  Touch these
 *			at your own risk!
 */
typedef struct
{
    /* CLASS I - Applications may read directly... */
    long	 	rbt_magic;	  /* Magic no. for integrity check */
    int			rbt_nm_nodes;	  /* Number of nodes */
    /* CLASS II - Applications may read/write directly... */
    void		(*rbt_print)();	  /* Data pretty-print function */
    int			rbt_debug;	  /* Debug bits */
    char		*rbt_description; /* Comment for diagnostics */
    /* CLASS III - Applications should not manipulate directly... */
    int		 	rbt_nm_orders;	  /* Number of simultaneous orders */
    int			(**rbt_order)();  /* Comparison functions */
    struct bu_rb_node	**rbt_root;	  /* The actual trees */
    char		*rbt_unique;	  /* Uniqueness flags */
    struct bu_rb_node	*rbt_current;	  /* Current node */
    struct bu_rb_list	rbt_nodes;	  /* All nodes */
    struct bu_rb_list	rbt_packages;	  /* All packages */
    struct bu_rb_node	*rbt_empty_node;  /* Sentinel representing nil */
}	bu_rb_tree;
#define	BU_RB_TREE_NULL	((bu_rb_tree *) 0)
#define	BU_RB_TREE_MAGIC	0x72627472

/*
 *	Debug bit flags for member rbt_debug
 */
#define BU_RB_DEBUG_INSERT	0x00000001	/* Insertion process */
#define BU_RB_DEBUG_UNIQ	0x00000002	/* Uniqueness of inserts */
#define BU_RB_DEBUG_ROTATE	0x00000004	/* Rotation process */
#define BU_RB_DEBUG_OS	0x00000008	/* Order-statistic operations */
#define BU_RB_DEBUG_DELETE	0x00000010	/* Deletion process */

/*
 *			B U _ R B _ P A C K A G E
 *
 *		    Wrapper for application data
 *
 *	This structure provides a level of indirection between
 *	the application software's data and the red-black nodes
 *	in which the data is stored.  It is necessary because of
 *	the algorithm for deletion, which generally shuffles data
 *	among nodes in the tree.  The package structure allows
 *	the application data to remember which node "contains" it
 *	for each order.
 */
struct bu_rb_package
{
    long		rbp_magic;	/* Magic no. for integrity check */
    struct bu_rb_node	**rbp_node;	/* Containing nodes */
    struct bu_rb_list	*rbp_list_pos;	/* Place in the list of all pkgs. */
    void		*rbp_data;	/* Application data */
};
#define	BU_RB_PKG_NULL	((struct bu_rb_package *) 0)

/*
 *			    B U _ R B _ N O D E
 *
 *	For the most part, there is a one-to-one correspondence
 *	between nodes and chunks of application data.  When a
 *	node is created, all of its package pointers (one per
 *	order of the tree) point to the same chunk of data.
 *	However, subsequent deletions usually muddy this tidy
 *	state of affairs.
 */
struct bu_rb_node
{
    long		rbn_magic;	/* Magic no. for integrity check */
    bu_rb_tree		*rbn_tree;	/* Tree containing this node */
    struct bu_rb_node	**rbn_parent;	/* Parents */
    struct bu_rb_node	**rbn_left;	/* Left subtrees */
    struct bu_rb_node	**rbn_right;	/* Right subtrees */
    char		*rbn_color;	/* Colors of this node */
    int			*rbn_size;	/* Sizes of subtrees rooted here */
    struct bu_rb_package **rbn_package;	/* Contents of this node */
    int			rbn_pkg_refs;	/* How many orders are being used? */
    struct bu_rb_list	*rbn_list_pos;	/* Place in the list of all nodes */
};
#define	BU_RB_NODE_NULL	((struct bu_rb_node *) 0)

/*
 *	Applications interface to bu_rb_extreme()
 */
#define	SENSE_MIN	0
#define	SENSE_MAX	1
#define	bu_rb_min(t,o)	bu_rb_extreme((t), (o), SENSE_MIN)
#define	bu_rb_max(t,o)	bu_rb_extreme((t), (o), SENSE_MAX)
#define bu_rb_pred(t,o)	bu_rb_neighbor((t), (o), SENSE_MIN)
#define bu_rb_succ(t,o)	bu_rb_neighbor((t), (o), SENSE_MAX)

/*
 *	Applications interface to bu_rb_walk()
 */
#define	PREORDER	0
#define	INORDER		1
#define	POSTORDER	2

/*
 *			B U _ O B S E R V E R
 *
 */
struct bu_observer {
  struct bu_list	l;
  struct bu_vls		observer;
  struct bu_vls		cmd;
};
#define BU_OBSERVER_NULL	((struct bu_observer *)0)

/*			B U _ C M D T A B
 *
 *
 */
struct bu_cmdtab {
  char *ct_name;
  int (*ct_func)();
};

/*----------------------------------------------------------------------*/
/* Miscellaneous macros */
#define bu_made_it()		bu_log("Made it to %s:%d\n",	\
					__FILE__, __LINE__)
/*----------------------------------------------------------------------*/
/*
 *  Declarations of external functions in LIBBU.
 *  Source file names listed alphabetically.
 */

/* avs.c */
BU_EXTERN(void			bu_avs_init, (struct bu_attribute_value_set *avp,
				int len, const char *str));
BU_EXTERN(void			bu_avs_init_empty, (struct bu_attribute_value_set *avp ));
BU_EXTERN(struct bu_attribute_value_set	*bu_avs_new, (int len, const char *str));
BU_EXTERN(int			bu_avs_add, (struct bu_attribute_value_set *avp,
				const char *attribute,
				const char *value));
extern int			bu_avs_add_vls(struct bu_attribute_value_set *avp,
				const char *attribute,
				const struct bu_vls *value_vls);
void				bu_avs_merge( struct bu_attribute_value_set *dest,
				const struct bu_attribute_value_set *src );
extern const char *		bu_avs_get( const struct bu_attribute_value_set *avp,
				const char *attribute );
BU_EXTERN(int			bu_avs_remove, (struct bu_attribute_value_set *avp,
				const char *attribute));
BU_EXTERN(void			bu_avs_free, (struct bu_attribute_value_set *avp));
extern void			bu_avs_print( const struct bu_attribute_value_set *avp, const char *title );
extern void bu_avs_add_nonunique( struct bu_attribute_value_set *avsp, char *attribute, char *value );

/* badmagic.c */
BU_EXTERN(void			bu_badmagic, (const long *ptr, long magic,
				const char *str, const char *file, int line));

/* bitv.c */
BU_EXTERN(struct bu_bitv *	bu_bitv_new, (int nbits));
BU_EXTERN(void			bu_bitv_clear, (struct bu_bitv *bv));
BU_EXTERN(void			bu_bitv_or, (struct bu_bitv *ov,
				const struct bu_bitv *iv));
BU_EXTERN(void			bu_bitv_and, (struct bu_bitv *ov,
				const struct bu_bitv *iv));
BU_EXTERN(void			bu_bitv_vls, (struct bu_vls *v,
				const struct bu_bitv *bv));
BU_EXTERN(void			bu_pr_bitv, (const char *str,
				const struct bu_bitv *bv));
BU_EXTERN(void			bu_bitv_to_hex, (struct bu_vls *v,
				const struct bu_bitv *bv));
BU_EXTERN( struct bu_bitv *	bu_hex_to_bitv, (const char *str));
BU_EXTERN( struct bu_bitv *	bu_bitv_dup, (const struct bu_bitv *bv));
BU_EXTERN( void			bu_bitv_free, (struct bu_bitv *bv));

/* bomb.c */
BU_EXTERN(void			bu_bomb, (const char *str) );

/* color.c */
BU_EXTERN(void		bu_rgb_to_hsv,		(unsigned char *rgb,
						    fastf_t *hsv) );
BU_EXTERN(int		bu_hsv_to_rgb,		(fastf_t *hsv,
						    unsigned char *rgb) );
BU_EXTERN(int		bu_str_to_rgb,		(char *str,
						    unsigned char *rgb) );
BU_EXTERN(void		bu_color_of_rgb_chars,	(struct bu_color *cp,
						    unsigned char *rgb) );
BU_EXTERN(int		bu_color_to_rgb_chars,	(struct bu_color *cp,
						    unsigned char *rgb) );
BU_EXTERN(int		bu_color_of_rgb_floats,	(struct bu_color *cp,
						    fastf_t *rgb) );
BU_EXTERN(int		bu_color_to_rgb_floats,	(struct bu_color *cp,
						    fastf_t *rgb) );
BU_EXTERN(int		bu_color_of_hsv_floats,	(struct bu_color *cp,
						    fastf_t *hsv) );
BU_EXTERN(int		bu_color_to_hsv_floats,	(struct bu_color *cp,
						    fastf_t *hsv) );

/* convert.c*/
BU_EXTERN(int bu_cv_cookie, (char *in));
BU_EXTERN(int bu_cv_optimize, (int cookie));
BU_EXTERN(int bu_cv_w_cookie, (genptr_t out, int outcookie, int size,
			     genptr_t in,  int incookie,  int count));

/* file.c */
BU_EXTERN(struct bu_file	*bu_fopen, (char *fname, char *type) );
BU_EXTERN(int			bu_fclose, (struct bu_file *bfp) );
BU_EXTERN(int			bu_fgetc, (struct bu_file *bfp) );
BU_EXTERN(void			bu_printfile, (struct bu_file *bfp) );

/* brlcad_path.c */
BU_EXTERN(int			bu_file_exists, (const char *path) );
BU_EXTERN(char			*bu_brlcad_path, (const char *rhs) );

/* fopen_uniq */
BU_EXTERN(FILE *		bu_fopen_uniq, (const char *outfmt,
						const char *namefmt,
						int n) );
/* getopt.c */
extern int			bu_opterr;
extern int			bu_optind;
extern int			bu_optopt;
extern char *			bu_optarg;
BU_EXTERN(int			bu_getopt, (int nargc, char * const nargv[],
				const char *ostr) );

/* hist.c */
BU_EXTERN(void			bu_hist_free, (struct bu_hist *histp));
BU_EXTERN(void			bu_hist_init, (struct bu_hist *histp,
				fastf_t min, fastf_t max, int nbins));
BU_EXTERN(void			bu_hist_range, (struct bu_hist *hp,
				fastf_t low, fastf_t high));
BU_EXTERN(void			bu_hist_pr, (const struct bu_hist *histp,
				const char *title));

/* htond.c */
BU_EXTERN(void			htond, (unsigned char *out,
				const unsigned char *in, int count));
BU_EXTERN(void			ntohd, (unsigned char *out,
				const unsigned char *in, int count));

/* htonf.c */
BU_EXTERN(void			htonf, (unsigned char *out,
				const unsigned char *in, int count));
BU_EXTERN(void			ntohf, (unsigned char *out,
				const unsigned char *in, int count));

/* ispar.c */
BU_EXTERN(int			bu_is_parallel, () );
BU_EXTERN(void			bu_kill_parallel, () );

/* linebuf.c */
#define port_setlinebuf		bu_setlinebuf	/* libsysv compat */
BU_EXTERN(void			bu_setlinebuf, (FILE *fp) );

/* list.c */
BU_EXTERN(int			bu_list_len, (const struct bu_list *hd));
BU_EXTERN(void			bu_list_reverse, (struct bu_list *hd));
BU_EXTERN(void			bu_list_free, (struct bu_list *hd));
BU_EXTERN(void			bu_list_parallel_append, (struct bu_list *headp,
					struct bu_list *itemp));
BU_EXTERN(struct bu_list *	bu_list_parallel_dequeue, (struct bu_list *headp));
BU_EXTERN(void			bu_ck_list, (const struct bu_list *hd,
				const char *str) );
BU_EXTERN(void			bu_ck_list_magic, (const struct bu_list *hd,
				const char *str, const long magic) );

/* hook.c */
BU_EXTERN(void			bu_hook_list_init, (struct bu_hook_list *hlp));
BU_EXTERN(void			bu_add_hook, (struct bu_hook_list *hlp, bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_delete_hook, (struct bu_hook_list *hlp, bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_call_hook, (struct bu_hook_list *hlp, genptr_t buf));

/* log.c */
BU_EXTERN(void			bu_log_indent_delta, (int delta) );
BU_EXTERN(void			bu_log_indent_vls, (struct bu_vls *v) );
BU_EXTERN(void			bu_log_add_hook, (bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_log_delete_hook, (bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_putchar, (int c) );
#if defined(HAVE_STDARG_H)
 BU_EXTERN(void			bu_log, (char *, ... ) );
 BU_EXTERN(void			bu_flog, (FILE *, char *, ... ) );
#else
 BU_EXTERN(void			bu_log, () );
 BU_EXTERN(void			bu_flog, () );
#endif

/* magic.c */
BU_EXTERN(const char *		bu_identify_magic, (long magic) );

/* malloc.c */
extern long		bu_n_malloc;
extern long		bu_n_free;
extern long		bu_n_realloc;
BU_EXTERN(genptr_t		bu_malloc, (unsigned int cnt, const char *str));
BU_EXTERN(void			bu_free, (genptr_t ptr, const char *str));
BU_EXTERN(genptr_t		bu_realloc, (genptr_t ptr, unsigned int cnt,
				const char *str));
BU_EXTERN(genptr_t		bu_calloc, (unsigned int nelem,
				unsigned int elsize, const char *str));
BU_EXTERN(void			bu_prmem, (const char *str));
BU_EXTERN(char *		bu_strdup, (const char *cp));
BU_EXTERN(char *		bu_dirname, (const char *cp));
BU_EXTERN(int			bu_malloc_len_roundup, (int nbytes));
BU_EXTERN(void			bu_ck_malloc_ptr, (genptr_t ptr, const char *str));
BU_EXTERN(int			bu_mem_barriercheck, ());

/* mappedfile.c */
BU_EXTERN(struct bu_mapped_file *bu_open_mapped_file, (const char *name,
					const char *appl));
BU_EXTERN(void			bu_close_mapped_file, (struct bu_mapped_file *mp));
BU_EXTERN(void			bu_pr_mapped_file, (const char *title,
					const struct bu_mapped_file *mp));
BU_EXTERN(void			bu_free_mapped_files, (int verbose));
BU_EXTERN(struct bu_mapped_file *bu_open_mapped_file_with_path,
					(char * const *path,
					const char *name, const char *appl));

/* parallel.c */
BU_EXTERN(void			bu_nice_set, (int newnice));
BU_EXTERN(int			bu_cpulimit_get, ());
BU_EXTERN(void			bu_cpulimit_set, (int sec));
BU_EXTERN(int			bu_avail_cpus, ());
BU_EXTERN(fastf_t		bu_get_load_average, ());
BU_EXTERN(int			bu_get_public_cpus, ());
BU_EXTERN(int			bu_set_realtime, ());
BU_EXTERN(void			bu_parallel, (void (*func)BU_ARGS((int ncpu, genptr_t arg)),
				int ncpu, genptr_t arg));

/* parse.c */
BU_EXTERN(int			bu_struct_export, (struct bu_external *ext,
				const genptr_t base,
				const struct bu_structparse *imp));
BU_EXTERN(int			bu_struct_import, (genptr_t base,
				const struct bu_structparse *imp,
				const struct bu_external *ext));
BU_EXTERN(int			bu_struct_put, (FILE *fp,
				const struct bu_external *ext));
BU_EXTERN(int			bu_struct_get, (struct bu_external *ext,
				FILE *fp));
BU_EXTERN(void			bu_struct_wrap_buf,
				(struct bu_external *ext, genptr_t buf));
BU_EXTERN(int			bu_struct_parse, (const struct bu_vls *in_vls,
				const struct bu_structparse *desc, 
				char *base));
BU_EXTERN(void			bu_struct_print, ( const char *title,
				const struct bu_structparse	*parsetab,
				const char			*base));
BU_EXTERN(void			bu_vls_struct_print, (struct bu_vls *vls,
				const struct bu_structparse *sdp,
				const char *base));
BU_EXTERN(void			bu_vls_struct_print2, (struct bu_vls *vls,
						       const char *title,
						       const struct bu_structparse *sdp,
						       const char *base));
BU_EXTERN(void			bu_vls_struct_item, (struct bu_vls *vp,
				const struct bu_structparse *sdp,
				const char *base,
				int sep_char));
BU_EXTERN(int			bu_vls_struct_item_named, (struct bu_vls *vp,
				const struct bu_structparse *sdp,
				const char *name,
				const char *base,
				int sep_char));
BU_EXTERN(void			bu_parse_mm, (const struct bu_structparse *sdp,
				const char *name,
				char *base,
				const char *value));
BU_EXTERN( int                  bu_key_eq_to_key_val, (char *in, char **next, struct bu_vls *vls) );
BU_EXTERN( int                  bu_shader_to_tcl_list, (char *in, struct bu_vls *vls) );
BU_EXTERN( int                  bu_key_val_to_key_eq, (char *in) );
BU_EXTERN( int                  bu_shader_to_key_eq, (char *in, struct bu_vls *vls) );
int				bu_fwrite_external( FILE *fp, const struct bu_external *ep );
void				bu_hexdump_external( FILE *fp, const struct bu_external *ep, const char *str);
void				bu_free_external(struct bu_external *ep);
void				bu_copy_external(struct bu_external *op, const struct bu_external *ip);
char				*bu_next_token( char *str );
				
/* printb.c */
BU_EXTERN(void			bu_vls_printb, (struct bu_vls *vls,
				const char *s, unsigned long v,
				const char *bits));
BU_EXTERN(void			bu_printb, (const char *s, unsigned long v,
				const char *bits));

/* ptbl.c */
BU_EXTERN(void			bu_ptbl_init, (struct bu_ptbl *b, int len, const char *str));
BU_EXTERN(void			bu_ptbl_reset, (struct bu_ptbl	*b));
BU_EXTERN(int			bu_ptbl_ins, (struct bu_ptbl *b, long *p));
BU_EXTERN(int			bu_ptbl_locate, (const struct bu_ptbl *b, const long *p));
BU_EXTERN(void			bu_ptbl_zero, (struct bu_ptbl *b, const long *p));
BU_EXTERN(int			bu_ptbl_ins_unique, (struct bu_ptbl *b, long *p));
BU_EXTERN(int			bu_ptbl_rm, (struct bu_ptbl *b, const long *p));
BU_EXTERN(void			bu_ptbl_cat, (struct bu_ptbl *dest,
				const struct bu_ptbl *src));
BU_EXTERN(void			bu_ptbl_cat_uniq, (struct bu_ptbl *dest,
				const struct bu_ptbl *src));
BU_EXTERN(void			bu_ptbl_free, (struct bu_ptbl	*b));
BU_EXTERN(int			bu_ptbl, (struct bu_ptbl *b, int func, long *p));
BU_EXTERN(void			bu_pr_ptbl, (const char *title,
				const struct bu_ptbl *tbl, int verbose));
BU_EXTERN(void			bu_ptbl_trunc, (struct bu_ptbl *tbl, int end));

/* rb_create.c */
BU_EXTERN(bu_rb_tree *bu_rb_create,	(char		*description,
				 int 		nm_orders,
				 int		(**order_funcs)()
				));
BU_EXTERN(bu_rb_tree *bu_rb_create1,	(char		*description,
				 int		(*order_func)()
				));
/* rb_delete.c */
BU_EXTERN(void bu_rb_delete,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_delete1(t)	bu_rb_delete((t), 0)

/* rb_diag.c */
BU_EXTERN(void bu_rb_diagnose_tree,(bu_rb_tree	*tree,
				 int		order,
				 int		trav_type
				));
BU_EXTERN(void bu_rb_summarize_tree,(bu_rb_tree	*tree
				 ));
/* rb_extreme.c */
BU_EXTERN(void *bu_rb_curr,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_curr1(t)	bu_rb_curr((t), 0)
BU_EXTERN(void *bu_rb_extreme,	(bu_rb_tree	*tree,
				 int		order,
				 int		sense
				));
BU_EXTERN(void *bu_rb_neighbor,	(bu_rb_tree	*tree,
				 int		order,
				 int		sense
				));
/* rb_free.c */
BU_EXTERN(void bu_rb_free,		(bu_rb_tree	*tree,
				 void		(*free_data)()
				));
#define	BU_RB_RETAIN_DATA	((void (*)()) 0)
#define		bu_rb_free1(t,f)					\
		{							\
		    BU_CKMAG((t), BU_RB_TREE_MAGIC, "red-black tree");	\
		    bu_free((char *) ((t) -> rbt_order),		\
				"red-black order function");		\
		    bu_rb_free(t,f);					\
		}
/* rb_insert.c */
BU_EXTERN(int bu_rb_insert,	(bu_rb_tree	*tree,
				 void		*data
				));
BU_EXTERN(int bu_rb_is_uniq,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_is_uniq1(t)	bu_rb_is_uniq((t), 0)
BU_EXTERN(void bu_rb_set_uniqv,	(bu_rb_tree	*tree,
				 bitv_t		vec
				));
BU_EXTERN(void bu_rb_uniq_all_off,	(bu_rb_tree	*tree
				));
BU_EXTERN(void bu_rb_uniq_all_on,	(bu_rb_tree	*tree
				));
BU_EXTERN(int bu_rb_uniq_off,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_uniq_off1(t)	bu_rb_uniq_off((t), 0)
BU_EXTERN(int bu_rb_uniq_on,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_uniq_on1(t)	bu_rb_uniq_on((t), 0)

/* rb_order_stats.c */
BU_EXTERN(int bu_rb_rank,	(bu_rb_tree	*tree,
				 int		order
				));
#define		bu_rb_rank1(t)	bu_rb_rank1((t), 0)
BU_EXTERN(void *bu_rb_select,	(bu_rb_tree	*tree,
				 int		order,
				 int		k
				));
#define		bu_rb_select1(t,k)	bu_rb_select((t), 0, (k))

/* rb_search.c */
BU_EXTERN(void *bu_rb_search,	(bu_rb_tree	*tree,
				 int		order,
				 void		*data
				));
#define		bu_rb_search1(t,d)	bu_rb_search((t), 0, (d))

/* rb_walk.c */
BU_EXTERN(void bu_rb_walk,		(bu_rb_tree	*tree,
				 int		order,
				 void		(*visit)(),
				 int		trav_type
				));
#define		bu_rb_walk1(t,v,d)	bu_rb_walk((t), 0, (v), (d))

/* semaphore.c */
BU_EXTERN(void			bu_semaphore_init, (unsigned int nsemaphores));
BU_EXTERN(void			bu_semaphore_acquire, (unsigned int i));
BU_EXTERN(void			bu_semaphore_release, (unsigned int i));

/* vls.c */
BU_EXTERN(void			bu_vls_init, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_init_if_uninit, (struct bu_vls *vp) );
BU_EXTERN(struct bu_vls *	bu_vls_vlsinit, () );
BU_EXTERN(char *		bu_vls_addr, (const struct bu_vls *vp) );
BU_EXTERN(char *		bu_vls_strdup, (const struct bu_vls *vp) );
BU_EXTERN(char *		bu_vls_strgrab, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_extend, (struct bu_vls *vp, int extra) );
BU_EXTERN(void			bu_vls_setlen, (struct bu_vls *vp, int newlen));
BU_EXTERN(int			bu_vls_strlen, (const struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_trunc, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_trunc2, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_nibble, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_free, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_vlsfree, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_strcpy, (struct bu_vls *vp, const char *s) );
BU_EXTERN(void			bu_vls_strncpy, (struct bu_vls *vp, const char *s, long n) );
BU_EXTERN(void			bu_vls_strcat, (struct bu_vls *vp, const char *s) );
BU_EXTERN(void			bu_vls_strncat, (struct bu_vls *vp, const char *s, long n) );
BU_EXTERN(void			bu_vls_vlscat, (struct bu_vls *dest, const struct bu_vls *src) );
BU_EXTERN(void			bu_vls_vlscatzap, (struct bu_vls *dest, struct bu_vls *src) );
BU_EXTERN(void			bu_vls_from_argv, (struct bu_vls *vp, int argc, char **argv) );
BU_EXTERN(int			bu_argv_from_string, (char **argv, int lim, char *lp));
BU_EXTERN(void			bu_vls_fwrite, (FILE *fp, const struct bu_vls *vp) );
void				bu_vls_write( int fd, const struct bu_vls *vp );
int				bu_vls_read( struct bu_vls *vp, int fd );
BU_EXTERN(int			bu_vls_gets, (struct bu_vls *vp, FILE *fp) );
BU_EXTERN(void			bu_vls_putc, (struct bu_vls *vp, int c) );
void				bu_vls_trimspace( struct bu_vls *vp );
#if 0
BU_EXTERN(void			bu_vls_vprintf, (struct bu_vls *vls,
				const char *fmt, va_list ap));
#endif
BU_EXTERN(void			bu_vls_printf, (struct bu_vls *vls, char *fmt, ... ) );
BU_EXTERN(void			bu_vls_spaces, (struct bu_vls *vp, int cnt));
BU_EXTERN(int			bu_vls_print_positions_used, (const struct bu_vls *vp));
BU_EXTERN(void			bu_vls_detab, (struct bu_vls *vp));

#if 0
BU_EXTERN(void			bu_vls_blkset, (struct bu_vls *vp, int len, int ch) );
#endif

BU_EXTERN(void			bu_vls_prepend, (struct bu_vls *vp, char *str) );


/* vers.c (created by the Cakefile) */
extern const char		bu_version[];

/* units.c */
BU_EXTERN(double bu_units_conversion, (const char *str) );
BU_EXTERN(const char *bu_units_string, (const double mm) );
BU_EXTERN(double bu_mm_value, (const char *s) );
BU_EXTERN(void bu_mm_cvt, (register const struct bu_structparse	*sdp,
		register const char *name,  char *base, const char *value) );

/* xdr.c */
/* Macro version of library routine bu_glong() */
/* The argument is expected to be of type "unsigned char" */
#define BU_GLONGLONG(_cp)	\
	    ((((long)((_cp)[0])) << 56) |	\
             (((long)((_cp)[1])) << 48) |	\
             (((long)((_cp)[2])) << 40) |	\
             (((long)((_cp)[3])) << 32) |	\
             (((long)((_cp)[4])) << 24) |	\
             (((long)((_cp)[5])) << 16) |	\
             (((long)((_cp)[6])) <<  8) |	\
              ((long)((_cp)[7])) )
#define BU_GLONG(_cp)	\
	    ((((long)((_cp)[0])) << 24) |	\
             (((long)((_cp)[1])) << 16) |	\
             (((long)((_cp)[2])) <<  8) |	\
              ((long)((_cp)[3])) )
#define BU_GSHORT(_cp)	\
            ((((short)((_cp)[0])) << 8) | \
                       (_cp)[1] )

BU_EXTERN(unsigned short	bu_gshort, (const unsigned char *msgp));
BU_EXTERN(unsigned long		bu_glong, (const unsigned char *msgp));
BU_EXTERN(unsigned char *	bu_pshort, (register unsigned char *msgp,
				register int s));
BU_EXTERN(unsigned char *	bu_plong, (register unsigned char *msgp,
				register unsigned long l));


/* association.c */
BU_EXTERN(struct bu_vls *bu_association, (const char *fname, const char *value, int field_sep));

/* Things that live in libbu/observer.c */
extern void bu_observer_notify();
extern struct bu_cmdtab bu_observer_cmds[];
extern void bu_observer_free(struct bu_observer *);

/* bu_tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
extern void bu_badmagic_tcl();
extern void bu_structparse_get_terse_form();
extern int bu_structparse_argv();
extern int bu_tcl_mem_barriercheck();
extern int bu_tcl_ck_malloc_ptr();
extern int bu_tcl_malloc_len_roundup();
extern int bu_tcl_prmem();
extern int bu_tcl_printb();
extern int bu_get_value_by_keyword();
extern int bu_get_all_keyword_values();
extern int bu_tcl_rgb_to_hsv();
extern int bu_tcl_hsv_to_rgb();
extern int bu_tcl_key_eq_to_key_val();
extern int bu_tcl_shader_to_key_val();
extern int bu_tcl_key_val_to_key_eq();
extern int bu_tcl_shader_to_key_eq();
extern int bu_tcl_brlcad_path();
extern int bu_tcl_units_conversion();
extern void bu_tcl_setup();
extern int Bu_Init();

/* lex.c */
#define BU_LEX_ANY	0	/* pseudo type */
struct bu_lex_t_int {
	int type;
	int value;
};
#define BU_LEX_INT	1
struct bu_lex_t_dbl {
	int	type;
	double	value;
};
#define BU_LEX_DOUBLE	2
struct bu_lex_t_key {
	int	type;
	int	value;
};
#define BU_LEX_SYMBOL	3
#define BU_LEX_KEYWORD	4
struct bu_lex_t_id {
	int	type;
	char 	*value;
};
#define BU_LEX_IDENT	5
#define BU_LEX_NUMBER	6	/* Pseudo type */
union bu_lex_token {
	int			type;
	struct	bu_lex_t_int	t_int;
	struct	bu_lex_t_dbl	t_dbl;
	struct	bu_lex_t_key	t_key;
	struct	bu_lex_t_id	t_id;
};
struct bu_lex_key {
	int	tok_val;
	char	*string;
};
#define BU_LEX_NEED_MORE	0

int bu_lex(
	union bu_lex_token *token,
	struct bu_vls *rtstr,
	struct bu_lex_key *keywords,
	struct bu_lex_key *symbols);


/* mro.c */
void bu_mro_init_with_string( struct bu_mro *mrop, const char *string );
void bu_mro_set( struct bu_mro *mrop, const char *string );
void bu_mro_init( struct bu_mro *mrop );
void bu_mro_free( struct bu_mro *mrop );

/* hash.c */
struct bu_hash_entry {
	long magic;
	unsigned char *key;
	unsigned char *value;
	int key_len;
	struct bu_hash_entry *next;
};

struct bu_hash_tbl {
	long magic;
	unsigned long mask;
	unsigned long num_lists;
	unsigned long num_entries;
	struct bu_hash_entry **lists;
};

struct bu_hash_record {
	long magic;
	struct bu_hash_tbl *tbl;
	unsigned long index;
	struct bu_hash_entry *hsh_entry;
};

#define BU_HASH_TBL_MAGIC	0x48415348	/* "HASH" */
#define BU_HASH_RECORD_MAGIC	0x68617368	/* "hash" */
#define BU_HASH_ENTRY_MAGIC	0x48454E54	/* "HENT" */
#define BU_CK_HASH_TBL(_hp)	BU_CKMAG( _hp, BU_HASH_TBL_MAGIC, "bu_hash_tbl" )
#define BU_CK_HASH_RECORD(_rp)	BU_CKMAG( _rp, BU_HASH_RECORD_MAGIC, "bu_hash_record" )
#define BU_CK_HASH_ENTRY(_ep)	BU_CKMAG( _ep, BU_HASH_ENTRY_MAGIC, "bu_hash_entry" )

unsigned long bu_hash(unsigned char *str, int len);
struct bu_hash_tbl *bu_create_hash_tbl( unsigned long tbl_size );
struct bu_hash_entry *bu_find_hash_entry( struct bu_hash_tbl *hsh_tbl,
					  unsigned char *key,
					  int key_len,
					  struct bu_hash_entry **prev,
					  unsigned long *index );
void bu_set_hash_value( struct bu_hash_entry *hsh_entry, unsigned char *value );
unsigned char *bu_get_hash_value( struct bu_hash_entry *hsh_entry );
struct bu_hash_entry *bu_hash_add_entry( struct bu_hash_tbl *hsh_tbl, unsigned char *key, int key_len, int *new_entry );
void bu_hash_tbl_pr( struct bu_hash_tbl *hsh_tbl, char *str );
void bu_hash_tbl_free( struct bu_hash_tbl *hsh_tbl );
struct bu_hash_entry *bu_hash_tbl_first( struct bu_hash_tbl *hsh_tbl, struct bu_hash_record *rec );
struct bu_hash_entry *bu_hash_tbl_next( struct bu_hash_record *rec );


#ifdef __cplusplus
}
#endif
#endif /* SEEN_BU_H */
