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

#ifndef SEEN_RTLIST_H
# include "rtlist.h"
#endif

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
/*
 *  Bit vector data structure.
 */
struct bu_bitv {
	struct rt_list	l;		/* linked list for caller's use */
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
	struct rt_list	l;	/* linked list for caller's use */
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


/* histogram */

/* vlist, vlblock?  But they use vmath.h... */

/* rt_mapped_file */

/* formerly rt_g.rtg_logindent, now use bu_log_indent_delta() */
typedef int (*bu_hook_t)BU_ARGS((genptr_t, genptr_t));
#define BUHOOK_NULL 0

/*----------------------------------------------------------------------*/
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
#	define offsetofarray(_t, _m)	offsetof(_t, _m[0])
#else
#	if !defined(offsetof)
#		define offsetof(_t, _m)		(int)(&(((_t *)0)->_m))
#	endif
#	define offsetofarray(_t, _m)	(int)( (((_t *)0)->_m))
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
 * {"%c", 1,  "char_value", offsetof(data_structure, a_char),	FUNC_NULL}, 
 * {"%s", 32, "char_value", offsetofarray(data_structure, str), FUNC_NULL}, 
 * {"%i", 1,  "char_value", offsetof(data_structure, a_short),	FUNC_NULL}, 
 * {"%d", 1,  "char_value", offsetof(data_structure, a_int), 	FUNC_NULL}, 
 * {"%f", 1,  "char_value", offsetof(data_structure, a_double), FUNC_NULL}, 
 * };
 */
struct bu_structparse {
	char		sp_fmt[4];		/* "i" or "%f", etc */
	long		sp_count;		/* number of elements */
	char		*sp_name;		/* Element's symbolic name */
	long		sp_offset;		/* Byte offset in struct */
	void		(*sp_hook)();		/* Optional hooked function, or indir ptr */
};
#define FUNC_NULL	((void (*)())0)

/* A "bu_external" struct holds the "external binary" representation of a
 * structure.
 */
struct bu_external  {
	long	ext_magic;
	int	ext_nbytes;
	genptr_t ext_buf;
};
#define BU_EXTERNAL_MAGIC	0x768dbbd0
#define BU_INIT_EXTERNAL(_p)	{(_p)->ext_magic = BU_EXTERNAL_MAGIC; \
	(_p)->ext_buf = GENPTR_NULL; (_p)->ext_nbytes = 0;}
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

/* ispar.c */
BU_EXTERN(int			bu_is_parallel, () );
BU_EXTERN(void			bu_kill_parallel, () );

/* linebuf.c */
#define port_setlinebuf		bu_setlinebuf	/* libsysv compat */
BU_EXTERN(void			bu_setlinebuf, (FILE *fp) );

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

/* parallel.c */
BU_EXTERN(void			bu_nice_set, (int newnice));
BU_EXTERN(int			bu_cpulimit_get, ());
BU_EXTERN(void			bu_cpulimit_set, (int sec));
BU_EXTERN(int			bu_avail_cpus, ());
BU_EXTERN(void			bu_parallel, (void (*func)(), int ncpu));

/* parse.c */
BU_EXTERN(int			bu_structparse, (CONST struct bu_vls *in_vls,
				CONST struct bu_structparse *desc, 
				char *base));
BU_EXTERN(void			bu_vls_item_print, (struct bu_vls *vp,
				CONST struct bu_structparse *sdp,
				CONST char *base ));
BU_EXTERN(void			bu_vls_item_print_nc, (struct bu_vls *vp,
				CONST struct bu_structparse *sdp,
				CONST char *base ));
BU_EXTERN(int			bu_vls_name_print, (struct bu_vls *vp,
				CONST struct bu_structparse *parsetab,
				CONST char *name, CONST char *base ));
BU_EXTERN(int			bu_vls_name_print_nc, (struct bu_vls *vp,
				CONST struct bu_structparse *parsetab,
				CONST char *name, CONST char *base ));
BU_EXTERN(void			bu_structprint, (CONST char *title,
				CONST struct bu_structparse *parsetab,
				CONST char *base));
BU_EXTERN(void			bu_vls_structprint, (struct  bu_vls *vls,
				register CONST struct bu_structparse *sdp,
				CONST char *base));
				
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
