/*
 *			M A C H I N E . H
 *  
 *  This header file defines the types of various machine-related
 *  objects.  These should be changed for each target machine,
 *  as appropriate.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

#ifndef MACHINE_H
#define MACHINE_H seen

/**********************************
 *                                *
 *  Machine specific definitions  *
 *  Choose for maximum speed      *
 *				  *
 **********************************/

#ifdef HEP
/********************************
 *				*
 *  Denelcor HEP H-1000		*
 *				*
 ********************************/
#define IBM_FLOAT 1		/* Uses IBM style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

/* full means resource free, empty means resource busy */
#define RES_INIT(ptr)		RES_RELEASE(ptr)
#define	RES_ACQUIRE(ptr)	(void)Daread(ptr)	/* wait full set empty */
#define RES_RELEASE(ptr)	(void)Daset(ptr,3)	/* set full */
#define MAX_PSW		128	/* Max number of process streams */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1
#endif


#if defined(alliant) && !defined(i860)
/********************************
 *				*
 *	Alliant FX/8		*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

/* All FX/8's have parallel capability -- compile the locking in always */
#define RES_INIT(ptr)		RES_RELEASE(ptr)
/* RES_ACQUIRE is a function */
#define RES_RELEASE(ptr)	*(ptr)=0;

#define MAX_PSW		8	/* Max number of processors */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1

#endif


#if defined(alliant) && defined(i860)
/********************************
 *				*
 *	Alliant FX/2800		*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
#define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

/* All Alliant machines have parallel capability */
/* RES_INIT, RES_ACQUIRE, and RES_RELASE are all subroutines */

#define MAX_PSW		28	/* Max number of processors */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1

#endif


#ifdef CRAY
/********************************
 *				*
 *  Cray-X/MP, COS or UNICOS	*
 *  Cray-2 under "UNICOS"	*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		4	/* Max number of processors */
#define DEFAULT_PSW	1
#define PARALLEL	1

#define USE_STRING_H
/**#define CRAY_COS	1	/* Running on Cray under COS w/bugs */
#endif

#if defined(convex) || defined(__convex__)
/********************************
 *				*
 *  Convex C1 & C2		*
 *				*
 ********************************/
typedef double		fastf_t;/* double|float, "Fastest" float type */
#define LOCAL		auto	/* static|auto, for serial|parallel cpu */
#define FAST		register /* LOCAL|register, for fastest floats */
typedef long long	bitv_t;	/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define RES_INIT(ptr)		RES_RELEASE(ptr)
/* RES_ACQUIRE is a function in machine.c, using tas instruction */
#define RES_RELEASE(ptr)	*(ptr)=0;

#define MAX_PSW		4	/* Max number of processors */
#define DEFAULT_PSW	1	/* for now */
#define PARALLEL	1
#endif

#ifdef ardent
/********************************
 *				*
 *  Ardent Workstation		*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* for parallel cpus */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

/* RES_INIT, RES_ACQUIRE, RES_RELEASE are subroutines, for now */
#define MAX_PSW		4	/* # processors, max */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1
#endif

#if	(defined(__sgi) && defined(__mips))
/* Strict ANSI C does not define CPP symbols that don't start with __ */
#	define sgi	1
#	define mips	1
#endif
#if	(defined(sgi) && defined(mips))
/********************************
 *				*
 *  SGI 4D, multi-processor	*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define CONST	const

/* RES_INIT, RES_ACQUIRE, RES_RELEASE are subroutines */
#define MAX_PSW		8
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1
#define USE_PROTOTYPES	1	/* not ANSI, but prototypes supported */

#endif

#ifdef apollo
/********************************
 *				*
 *  Apollo			*
 *  with SR 10			*
 *				*
 ********************************/
#if __STDC__
#define const	/**/		/* Does not support const keyword */
#define CONST	/**/		/* Does not support const keyword */
#endif

typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define RES_INIT(ptr)		;
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1

#endif

#ifdef n16
/********************************
 *				*
 *     Encore Multi-Max		*
 *				*
 ********************************/
#define IEEE_FLOAT	1	/* Uses IEEE style floating point */
#define LITTLE_ENDIAN	1	/* Under the influence of National Semiconductor */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		32	/* This number is uncertain */
#define DEFAULT_PSW	1
#define PARALLEL	1
#endif

#ifndef LOCAL
/********************************
 *				*
 * Default 32-bit uniprocessor	*
 *  VAX, Gould, SUN, SGI	*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define RES_INIT(ptr)		;
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1

#endif

/*
 *  Definitions for big-endian -vs- little-endian.
 *	BIG_ENDIAN:	Byte [0] is on left side of word (msb).
 *	LITTLE_ENDIAN:	Byte [0] is on right side of word (lsb).
 */
#ifdef vax
# define LITTLE_ENDIAN	1
#endif

#if !defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
# define BIG_ENDIAN	1	/* The common case */
#endif

/*  Bit vector mask */
#define BITV_MASK	((1<<BITV_SHIFT)-1)

/*
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 */
#if defined(vax) || (defined(sgi) && !defined(mips))
	/* DEC VAX "D" format, the most restrictive */
#define MAX_FASTF		1.0e37	/* Very close to the largest number */
#define SQRT_MAX_FASTF		1.0e18	/* This squared just avoids overflow */
#define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#else
	/* IBM format, being the next most restrictive format */
#define MAX_FASTF		1.0e73	/* Very close to the largest number */
#define SQRT_MAX_FASTF		1.0e36	/* This squared just avoids overflow */
#define SMALL_FASTF		1.0e-77	/* Anything smaller is zero */
#if defined(aux)
#define SQRT_SMALL_FASTF	1.0e-40 /* _doprnt error in libc */
#else
#define SQRT_SMALL_FASTF	1.0e-39	/* This squared gives zero */
#endif
#endif
#define SMALL			SQRT_SMALL_FASTF

/*
 *  Definition of a "generic" pointer that can hold a pointer to anything.
 *  According to tradition, a (char *) was generic, but the ANSI folks
 *  worry about machines where (int *) might be wider than (char *),
 *  so here is the clean way of handling it.
 */
#if !defined(GENPTR_NULL)
#  if __STDC__
	typedef void	*genptr_t;
#  else
	typedef char	*genptr_t;
#  endif
#  define GENPTR_NULL	((genptr_t)0)
#endif

/* A portable way of handling the ANSI C const keyword: use CONST */
#if !defined(CONST)
# if __STDC__
#	define	CONST	const
# else
#	define	CONST	/**/
# endif
#endif

/* A portable way of dealing with pre-ANSI C.  Assume signed variables */
#if !defined(SIGNED)
# if __STDC__
#	define SIGNED	signed
# else
#	define SIGNED	/**/
# endif
#endif

/*
 *  Some very common BSD --> SYSV conversion aids
 */
#if defined(SYSV) && !defined(bzero)
#	define bzero(str,n)		memset( str, '\0', n )
#	define bcopy(from,to,count)	memcpy( to, from, count )
#endif

#if defined(BSD) && !defined(SYSV) && (BSD < 43)
#	define strrchr(sp,c)	rindex(sp,c)
#	define strchr(sp,c)	index(sp,c)
#endif



/* Functions local to one file should be declared HIDDEN:  (nil)|static */
/* To aid in using ADB, generally leave this as nil. */
#if !defined(HIDDEN)
# if defined(lint)
#	define HIDDEN	static
# else
#	define HIDDEN	/***/
# endif
#endif

/* Some non-ANSI C compilers can take advantage of prototypes.  See above */
#if __STDC__ && !defined(USE_PROTOTYPES)
#	define USE_PROTOTYPES 1
#endif

#endif
