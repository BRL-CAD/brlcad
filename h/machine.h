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
#define PARALLEL	1

#endif HEP


#ifdef alliant
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
/* RES_ACQUIRE is a function in rt.c, using tas instruction */
#define RES_RELEASE(ptr)	*(ptr)=0;

#define MAX_PSW		8	/* Max number of processors */
#define PARALLEL	1

#endif alliant


#ifdef cray
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

/**#define CRAY_COS	1	/* Running on Cray under COS w/bugs */

#endif cray


#ifdef convex
/********************************
 *				*
 *  Convex C1			*
 *				*
 ********************************/
typedef double		fastf_t;/* double|float, "Fastest" float type */
#define LOCAL		auto	/* static|auto, for serial|parallel cpu */
#define FAST		register /* LOCAL|register, for fastest floats */
typedef long long	bitv_t;	/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define RES_INIT(ptr)		;
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#define MAX_PSW	1		/* only one processor, max */

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
#define MAX_PSW	4		/* # processors, max */

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
#define MAX_PSW	1		/* only one processor, max */

#endif

#define BITV_MASK	((1<<BITV_SHIFT)-1)

/*
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 */
#define MAX_FASTF		1.0e37	/* Very close to the largest number */
#define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#define SMALL			SQRT_SMALL_FASTF

/*
 *  Some very common BSD --> SYSV conversion aids
 */
#if defined(SYSV) && !defined(bzero)
#	define bzero(str,n)		memset( str, '\0', n )
#	define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/* To aid in using ADB, for now */
#ifdef lint
#define HIDDEN	static		/* (nil)|static, for func's local to 1 file */
#else
#define HIDDEN	/***/		/* (nil)|static, for func's local to 1 file */
#endif

#endif MACHINE_H
