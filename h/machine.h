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

/*
 *  Machine specific definitions, for maximum speed.
 */
#ifdef HEP
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#else
/* VAX, Gould */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#endif

#define BITV_MASK	((1<<BITV_SHIFT)-1)

/* To aid in using ADB, for now */
#ifdef lint
#define HIDDEN	static		/* (nil)|static, for func's local to 1 file */
#else
#define HIDDEN	/***/		/* (nil)|static, for func's local to 1 file */
#endif
