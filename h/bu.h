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

#ifndef SEEN_COMPAT4_H
# include "compat4.h"
#endif

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
 * Handy memory allocator macro
 */

/* Acquire storage for a given struct, eg, BU_GETSTRUCT(ptr,structname); */
#if __STDC__
# define BU_GETSTRUCT(_p,_str) \
	_p = (struct _str *)bu_calloc(1,sizeof(struct _str), #_str " (getstruct)" )
# define BU_GETUNION(_p,_unn) \
	_p = (union _unn *)bu_calloc(1,sizeof(union _unn), #_unn " (getstruct)")
#else
# define BU_GETSTRUCT(_p,_str) \
	_p = (struct _str *)bu_calloc(1,sizeof(struct _str), "_str (getstruct)")
# define BU_GETUNION(_p,_unn) \
	_p = (union _unn *)bu_calloc(1,sizeof(union _unn), "_unn (getstruct)")
#endif


/*
 *  Macros to check and validate a structure pointer, given that
 *  the first entry in the structure is a magic number.
 */
#define BU_CKMAG(_ptr, _magic, _str)	\
	if( !(_ptr) || *((long *)(_ptr)) != (_magic) )  { \
		bu_badmagic( (long *)(_ptr), _magic, _str, __FILE__, __LINE__ ); \
	}

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
 *	RT_LIST_INSERT( &(head.l), &((p)->l) );				*
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
	if (BU_LIST_NON_EMPTY(hp))				\
	{							\
	    (p) = ((struct structure *)((hp)->forw));		\
	    BU_LIST_DEQUEUE((struct bu_list *)(p));		\
	}							\
	else							\
	     (p) = (struct structure *) 0
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
#define BU_LIST_PREV(structure,hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_FIRST(structure,hp)	\
	((struct structure *)((hp)->forw))
#define BU_LIST_NEXT(structure,hp)	\
	((struct structure *)((hp)->forw))

/* Boolean test to see if current list element is the head */
#define BU_LIST_IS_HEAD(p,hp)	\
	(((struct bu_list *)(p)) == (hp))
#define BU_LIST_NOT_HEAD(p,hp)	\
	(((struct bu_list *)(p)) != (hp))
#define BU_CK_LIST_HEAD( _p )	BU_CKMAG( (_p), BU_LIST_HEAD_MAGIC, "bu_list")

/* Boolean test to see if the next list element is the head */
#define BU_LIST_NEXT_IS_HEAD(p,hp)	\
	(((struct bu_list *)(p))->forw == (hp))
#define BU_LIST_NEXT_NOT_HEAD(p,hp)	\
	(((struct bu_list *)(p))->forw != (hp))

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
 *  Bit vector data structure.
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

#define BU_BITTEST(_bv,bit)	\
	(((_bv)->bits[(bit)>>BITV_SHIFT] & (((bitv_t)1)<<((bit)&BITV_MASK)))?1:0)
#define BU_BITSET(_bv,bit)	\
	((_bv)->bits[(bit)>>BITV_SHIFT] |= (((bitv_t)1)<<((bit)&BITV_MASK)))
#define BU_BITCLR(_bv,bit)	\
	((_bv)->bits[(bit)>>BITV_SHIFT] &= ~(((bitv_t)1)<<((bit)&BITV_MASK)))

/* This is not done by default for performance reasons */
#define BU_BITV_BITNUM_CHECK(_bv,_bit)	/* Validate bit number */ \
	if( ((unsigned)(_bit)) >= (_bv)->nbits )  \
		rt_bomb("BU_BITV_BITNUM_CHECK bit number out of range");
#define BU_BITV_NBITS_CHECK(_bv,_nbits)	/* Validate number of bits */ \
	if( ((unsigned)(_nbits)) > (_bv)->nbits )  \
		rt_bomb("BU_BITV_NBITS_CHECK number of bits out of range");

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
#define BU_PTBL_GET(ptbl,i)	((ptbl)->buffer[(i)])
#define BU_PTBL_TEST(ptbl)	((ptbl)->l.magic == BU_PTBL_MAGIC)

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
	/* XXX Needs date stamp, in case file is modified */
	int		uses;		/* # ptrs to this struct handed out */
};
#define BU_MAPPED_FILE_MAGIC	0x4d617066	/* Mapf */
#define BU_CK_MAPPED_FILE(_p)	BU_CKMAG(_p, BU_MAPPED_FILE_MAGIC, "bu_mapped_file")

/*----------------------------------------------------------------------*/

/* formerly rt_g.rtg_logindent, now use bu_log_indent_delta() */
typedef int (*bu_hook_t)BU_ARGS((genptr_t, genptr_t));
#define BUHOOK_NULL 0

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

/*
 *  Section for manifest constants for bu_semaphore_acquire()
 */
#define BU_SEM_SYSCALL	0
#define BU_SEM_BN_NOISE	4	/* XXX really old res_model. should get own */
#define BU_SEM_LAST	5	/* XXX allocate this many (want 6 really) */
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

/*----------------------------------------------------------------------*/
/*
 * Section for BU_DEBUG values
 */
extern int	bu_debug;
/* These definitions are each for one bit */
#define BU_DEBUG_OFF		0	/* No debugging */
#define BU_DEBUG_MEM_CHECK	0x00000001	/* 001 Mem barrier & leak checking */
#define BU_DEBUG_MEM_LOG	0x00000002	/* 002 Print all dynamic memory operations */

#define BU_DEBUG_PARALLEL	0x00000010	/* 005 parallel support */

#define BU_DEBUG_MATH		0x00000100	/* 011 Fundamental math routines (plane.c, mat.c) */
#define BU_DEBUG_PTBL		0x00000200	/* 012 bu_ptbl_*() logging */

#define BU_DEBUG_COREDUMP	0x00001000	/* 015 If set, bu_bomb() will dump core */

/* Format string for bu_printb() */
#define BU_DEBUG_FORMAT	\
"\020\
\015COREDUMP\
\012PTBL\011MATH\010?\7?\6?\5PARALLEL\
\4?\3?\2MEM_LOG\1MEM_CHECK"

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
 * };
 *
 * struct bu_structparse data_sp[] ={
 * {"%c", 1,  "char_value", bu_offsetof(data_structure, a_char),   FUNC_NULL}, 
 * {"%s", 32, "char_value", bu_offsetofarray(data_structure, str), FUNC_NULL}, 
 * {"%i", 1,  "char_value", bu_offsetof(data_structure, a_short),  FUNC_NULL}, 
 * {"%d", 1,  "char_value", bu_offsetof(data_structure, a_int),    FUNC_NULL}, 
 * {"%f", 1,  "char_value", bu_offsetof(data_structure, a_double), FUNC_NULL}, 
 * };
 */
struct bu_structparse {
	char		sp_fmt[4];		/* "i" or "%f", etc */
	long		sp_count;		/* number of elements */
	char		*sp_name;		/* Element's symbolic name */
	long		sp_offset;		/* Byte offset in struct */
	void		(*sp_hook)();		/* Optional hooked function, or indir ptr */
};
#define BU_STRUCTPARSE_FUNC_NULL	((void (*)())0)

/*----------------------------------------------------------------------*/
/*
 *  An "opaque" handle for holding onto objects,
 *  typically in some kind of external form that is not directly
 *  usable without passing through an "importation" function.
 * A "bu_external" struct holds the "external binary" representation of a
 * structure or other block of arbitrary data.
 */
struct bu_external  {
	long	ext_magic;
	int	ext_nbytes;
	genptr_t ext_buf;
};
#define BU_EXTERNAL_MAGIC	0x768dbbd0
#define BU_INIT_EXTERNAL(_p)	{(_p)->ext_magic = BU_EXTERNAL_MAGIC; \
	(_p)->ext_buf = (genptr_t)NULL; (_p)->ext_nbytes = 0;}
#define BU_CK_EXTERNAL(_p)	RT_CKMAG(_p, BU_EXTERNAL_MAGIC, "bu_external")

/*----------------------------------------------------------------------*/
/*
 *  Declarations of external functions in LIBBU.
 *  Source file names listed alphabetically.
 */

/* badmagic.c */
BU_EXTERN(void			bu_badmagic, (CONST long *ptr, long magic,
				CONST char *str, CONST char *file, int line));

/* bitv.c */
BU_EXTERN(struct bu_bitv *	bu_bitv_new, (int nbits));
BU_EXTERN(void			bu_bitv_clear, (struct bu_bitv *bv));
BU_EXTERN(void			bu_bitv_or, (struct bu_bitv *ov,
				CONST struct bu_bitv *iv));
BU_EXTERN(void			bu_bitv_and, (struct bu_bitv *ov,
				CONST struct bu_bitv *iv));
BU_EXTERN(void			bu_bitv_vls, (struct bu_vls *v,
				CONST struct bu_bitv *bv));
BU_EXTERN(void			bu_pr_bitv, (CONST char *str,
				CONST struct bu_bitv *bv));

/* bomb.c */
BU_EXTERN(void			bu_bomb, (CONST char *str) );

/* getopt.c */
#define opterr			bu_opterr	/* libsysv compat */
#define optind			bu_optind
#define optopt			bu_optopt
#define optarg			bu_optarg
extern int			bu_opterr;
extern int			bu_optind;
extern int			bu_optopt;
extern char *			bu_optarg;
#define getopt			bu_getopt
BU_EXTERN(int			bu_getopt, (int nargc, char * CONST nargv[],
				CONST char *ostr) );

/* hist.c */
BU_EXTERN(void			bu_hist_free, (struct bu_hist *histp));
BU_EXTERN(void			bu_hist_init, (struct bu_hist *histp,
				fastf_t min, fastf_t max, int nbins));
BU_EXTERN(void			bu_hist_range, (struct bu_hist *hp,
				fastf_t low, fastf_t high));
BU_EXTERN(void			bu_hist_pr, (struct bu_hist *histp,
				CONST char *title));

/* ispar.c */
BU_EXTERN(int			bu_is_parallel, () );
BU_EXTERN(void			bu_kill_parallel, () );

/* linebuf.c */
#define port_setlinebuf		bu_setlinebuf	/* libsysv compat */
BU_EXTERN(void			bu_setlinebuf, (FILE *fp) );

/* list.c */
BU_EXTERN(int			bu_list_len, (CONST struct bu_list *hd));
BU_EXTERN(void			bu_list_reverse, (struct bu_list *hd));

/* log.c */
BU_EXTERN(void			bu_log_indent_delta, (int delta) );
BU_EXTERN(void			bu_log_indent_vls, (struct bu_vls *v) );
BU_EXTERN(void			bu_add_hook, (bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_delete_hook, (bu_hook_t func, genptr_t clientdata));
BU_EXTERN(void			bu_putchar, (int c) );
#if __STDC__
 BU_EXTERN(void			bu_log, (char *, ... ) );
 BU_EXTERN(void			bu_flog, (FILE *, char *, ... ) );
#else
 BU_EXTERN(void			bu_log, () );
 BU_EXTERN(void			bu_flog, () );
#endif

/* magic.c */
BU_EXTERN(CONST char *		bu_identify_magic, (long magic) );

/* malloc.c */
BU_EXTERN(genptr_t		bu_malloc, (unsigned int cnt, CONST char *str));
BU_EXTERN(void			bu_free, (genptr_t ptr, CONST char *str));
BU_EXTERN(genptr_t		bu_realloc, (genptr_t ptr, unsigned int cnt,
				CONST char *str));
BU_EXTERN(genptr_t		bu_calloc, (unsigned int nelem,
				unsigned int elsize, CONST char *str));
BU_EXTERN(void			bu_prmem, (CONST char *str));
BU_EXTERN(char *		bu_strdup, (CONST char *cp));
BU_EXTERN(int			bu_malloc_len_roundup, (int nbytes));
BU_EXTERN(void			bu_ck_malloc_ptr, (genptr_t ptr, CONST char *str));
BU_EXTERN(int			bu_mem_barriercheck, ());

/* mappedfile.c */
BU_EXTERN(struct bu_mapped_file *bu_open_mapped_file, (CONST char *name,
					CONST char *appl));
BU_EXTERN(void			bu_close_mapped_file, (struct bu_mapped_file *mp));


/* parallel.c */
BU_EXTERN(void			bu_nice_set, (int newnice));
BU_EXTERN(int			bu_cpulimit_get, ());
BU_EXTERN(void			bu_cpulimit_set, (int sec));
BU_EXTERN(int			bu_avail_cpus, ());
BU_EXTERN(void			bu_parallel, (void (*func)(), int ncpu));

/* parse.c */
BU_EXTERN(int			bu_struct_export, (struct bu_external *ext,
				CONST genptr_t base,
				CONST struct bu_structparse *imp));
BU_EXTERN(int			bu_struct_import, (genptr_t base,
				CONST struct bu_structparse *imp,
				CONST struct bu_external *ext));
BU_EXTERN(int			bu_struct_put, (FILE *fp,
				CONST struct bu_external *ext));
BU_EXTERN(int			bu_struct_get, (struct bu_external *ext,
				FILE *fp));
BU_EXTERN(unsigned short	bu_gshort, (CONST unsigned char *msgp));
BU_EXTERN(unsigned long		bu_glong, (CONST unsigned char *msgp));
BU_EXTERN(unsigned char *	bu_pshort, (register unsigned char *msgp,
				register int s));
BU_EXTERN(unsigned char *	bu_plong, (register unsigned char *msgp,
				register unsigned long l));
BU_EXTERN(void			bu_struct_wrap_buf,
				(struct bu_external *ext, genptr_t buf));
BU_EXTERN(int			bu_struct_parse, (CONST struct bu_vls *in_vls,
				CONST struct bu_structparse *desc, 
				char *base));
BU_EXTERN(void			bu_struct_print, ( CONST char *title,
				CONST struct bu_structparse	*parsetab,
				CONST char			*base));
BU_EXTERN(void			bu_vls_struct_print, (struct bu_vls *vls,
				CONST struct bu_structparse *sdp,
				CONST char *base));
BU_EXTERN(void			bu_vls_struct_item, (struct bu_vls *vp,
				CONST struct bu_structparse *sdp,
				CONST char *base,
				int sep_char));
BU_EXTERN(int			bu_vls_struct_item_named, (struct bu_vls *vp,
				CONST struct bu_structparse *sdp,
				CONST char *name,
				CONST char *base,
				int sep_char));






/* printb.c */
BU_EXTERN(void			bu_vls_printb, (struct bu_vls *vls,
				CONST char *s, unsigned long v,
				CONST char *bits));
BU_EXTERN(void			bu_printb, (CONST char *s, unsigned long v,
				CONST char *bits));

/* ptbl.c */
BU_EXTERN(void			bu_ptbl_init, (struct bu_ptbl *b, int len, CONST char *str));
BU_EXTERN(void			bu_ptbl_reset, (struct bu_ptbl	*b));
BU_EXTERN(int			bu_ptbl_ins, (struct bu_ptbl *b, long *p));
BU_EXTERN(int			bu_ptbl_locate, (CONST struct bu_ptbl *b, CONST long *p));
BU_EXTERN(void			bu_ptbl_zero, (struct bu_ptbl *b, CONST long *p));
BU_EXTERN(int			bu_ptbl_ins_unique, (struct bu_ptbl *b, long *p));
BU_EXTERN(int			bu_ptbl_rm, (struct bu_ptbl *b, CONST long *p));
BU_EXTERN(void			bu_ptbl_cat, (struct bu_ptbl *dest,
				CONST struct bu_ptbl *src));
BU_EXTERN(void			bu_ptbl_cat_uniq, (struct bu_ptbl *dest,
				CONST struct bu_ptbl *src));
BU_EXTERN(void			bu_ptbl_free, (struct bu_ptbl	*b));
BU_EXTERN(int			bu_ptbl, (struct bu_ptbl *b, int func, long *p));
BU_EXTERN(void			bu_pr_ptbl, (CONST char *title,
				CONST struct bu_ptbl *tbl, int verbose));

/* semaphore.c */
BU_EXTERN(void			bu_semaphore_init, (unsigned int nsemaphores));
BU_EXTERN(void			bu_semaphore_acquire, (unsigned int i));
BU_EXTERN(void			bu_semaphore_release, (unsigned int i));

/* vls.c */
BU_EXTERN(void			bu_vls_init, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_init_if_uninit, (struct bu_vls *vp) );
BU_EXTERN(struct bu_vls *	bu_vls_vlsinit, () );
BU_EXTERN(char *		bu_vls_addr, (CONST struct bu_vls *vp) );
BU_EXTERN(char *		bu_vls_strdup, (CONST struct bu_vls *vp) );
BU_EXTERN(char *		bu_vls_strgrab, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_extend, (struct bu_vls *vp, int extra) );
BU_EXTERN(void			bu_vls_setlen, (struct bu_vls *vp, int newlen));
BU_EXTERN(int			bu_vls_strlen, (CONST struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_trunc, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_trunc2, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_nibble, (struct bu_vls *vp, int len) );
BU_EXTERN(void			bu_vls_free, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_vlsfree, (struct bu_vls *vp) );
BU_EXTERN(void			bu_vls_strcpy, (struct bu_vls *vp, CONST char *s) );
BU_EXTERN(void			bu_vls_strncpy, (struct bu_vls *vp, CONST char *s, int n) );
BU_EXTERN(void			bu_vls_strcat, (struct bu_vls *vp, CONST char *s) );
BU_EXTERN(void			bu_vls_strncat, (struct bu_vls *vp, CONST char *s, int n) );
BU_EXTERN(void			bu_vls_vlscat, (struct bu_vls *dest, CONST struct bu_vls *src) );
BU_EXTERN(void			bu_vls_vlscatzap, (struct bu_vls *dest, struct bu_vls *src) );
BU_EXTERN(void			bu_vls_from_argv, (struct bu_vls *vp, int argc, char **argv) );
BU_EXTERN(void			bu_vls_fwrite, (FILE *fp, CONST struct bu_vls *vp) );
BU_EXTERN(int			bu_vls_gets, (struct bu_vls *vp, FILE *fp) );
BU_EXTERN(void			bu_vls_putc, (struct bu_vls *vp, int c) );
#if 0
BU_EXTERN(void			bu_vls_vprintf, (struct bu_vls *vls,
				CONST char *fmt, va_list ap));
#endif
BU_EXTERN(void			bu_vls_printf, (struct bu_vls *vls, char *fmt, ... ) );
#if 0
BU_EXTERN(void			bu_vls_blkset, (struct bu_vls *vp, int len, int ch) );
#endif

/* vers.c (created by the Cakefile) */
extern CONST char		bu_version[];

#ifdef __cplusplus
}
#endif
#endif /* SEEN_BU_H */
