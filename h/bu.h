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
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "rtlist.h"	/_* OPTIONAL, auto-included by bu.h *_/
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

/* Bit vector macros would be a good candidate */

/* histogram */

/* vlist, vlblock */

/* rt_mapped_file */

/* formerly rt_g.rtg_logindent, now use bu_log_indent_delta() */
typedef int (*bu_hook_t)BU_ARGS((genptr_t, genptr_t));
#define BUHOOK_NULL 0

/*
 *  bu_vls support (formerly rt_vls in h/rtstring.h)
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

#define BU_DEBUG_COREDUMP	0x00001000	/* 015 If set, bu_bomb() will dump core */

/* Format string for bu_printb() */
#define BU_DEBUG_FORMAT	\
"\020\
\015COREDUMP\
\011MATH\010?\7?\6?\5PARALLEL\
\4?\3?\2MEM_LOG\1MEM_CHECK"

/*
 *  Declarations of external functions in LIBBU.
 *  Source file names listed alphabetically.
 */

/* badmagic.c */
BU_EXTERN(void			bu_badmagic, (CONST long *ptr, long magic,
				CONST char *str, CONST char *file, int line));

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
BU_EXTERN(int			bu_is_parallel, () );

/* printb.c */
BU_EXTERN(void			bu_vls_printb, (struct bu_vls *vls,
				CONST char *s, unsigned long v,
				CONST char *bits));
BU_EXTERN(void			bu_printb, (CONST char *s, unsigned long v,
				CONST char *bits));

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
