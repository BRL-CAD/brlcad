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
 **********************************/
#ifdef HEP
/*
 *  Denelcor HEP H-1000
 */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

/* full means resource free, empty means resource busy */
#define RES_INIT(ptr)		RES_RELEASE(ptr)
#define	RES_ACQUIRE(ptr)	(void)Daread(ptr)	/* wait full set empty */
#define RES_RELEASE(ptr)	(void)Daset(ptr,3)	/* set full */

#endif HEP


#ifdef alliant
/*
 *  Alliant FX/8
 */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

/* All FX/8's have parallel capability -- compile the locking in always */
#define RES_INIT(ptr)		RES_RELEASE(ptr)
/* RES_ACQUIRE is a function in rt.c, using tas instruction */
#define RES_RELEASE(ptr)	*(ptr)=0;

#endif alliant


#ifdef cray
/*
 *  Cray-X/MP under COS, on Cray-2 under "UNICOS"
 *  To date, only on 1 processor.
 */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#ifndef PARALLEL
#define RES_INIT(ptr)		;
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#else
/* Cray multi-tasking routines */
#define RES_INIT(ptr)		LOCKASGN(ptr);
#define RES_ACQUIRE(ptr)	LOCKON(ptr);
#define RES_RELEASE(ptr)	LOCKOFF(ptr);
#endif

/**buggy #define bzero(str,n)		memset( str, '\0', n ) ***/
#define bcopy(from,to,count)	memcpy( to, from, count )

/**#define CRAY_COS	1	/* Running on Cray under COS w/bugs */

#endif cray


#ifndef LOCAL
/*
 * Default 32-bit uniprocessor configuration:  VAX, Gould, SUN
 */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define RES_INIT(ptr)		;
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#endif


#define BITV_MASK	((1<<BITV_SHIFT)-1)

/* To aid in using ADB, for now */
#ifdef lint
#define HIDDEN	static		/* (nil)|static, for func's local to 1 file */
#else
#define HIDDEN	/***/		/* (nil)|static, for func's local to 1 file */
#endif

#endif MACHINE_H
