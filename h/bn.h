/*
 *			B N . H
 *
 *  Header file for the BRL-CAD Numerical Computation Library, LIBBN.
 *
 *  The library provides a broad assortment of numerical algorithms
 *  and computational routines, including random number generation,
 *  vector math, matrix math, quaternion math, complex math,
 *  synthetic division, root finding, etc.
 *
 *  This header file depends on vmath.h
 *  This header file depends on bu.h and LIBBU;  it is safe to use
 *  bu.h macros (e.g. BU_EXTERN) here.
 *
 *  ??Should complex.h and plane.h and polyno.h get absorbed in here??
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A Butler
 *	Douglas A Gwyn
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
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "bu.h"
 *	#include "vmath.h"
 *	#include "bn.h"
 *
 *  Libraries Used -
 *	-lm -lc
 *
 *  $Header$
 */

#ifndef SEEN_BN_H
#define SEEN_BN_H seen
#ifdef __cplusplus
extern "C" {
#endif

#define BN_H_VERSION	"@(#)$Header$ (BRL)"

/*----------------------------------------------------------------------*/
/*
 *  Complex numbers
 */

/* "complex number" data type: */
typedef struct bn_complex {
	double		re;		/* real part */
	double		im;		/* imaginary part */
}  bn_complex_t;

/* functions that are efficiently done as macros: */

#define	bn_cx_copy( ap, bp )		{*(ap) = *(bp);}
#define	bn_cx_neg( cp )			{ (cp)->re = -((cp)->re);(cp)->im = -((cp)->im);}
#define	bn_cx_real( cp )		(cp)->re
#define	bn_cx_imag( cp )		(cp)->im

#define bn_cx_add( ap, bp )		{ (ap)->re += (bp)->re; (ap)->im += (bp)->im;}
#define bn_cx_ampl( cp )		hypot( (cp)->re, (cp)->im )
#define bn_cx_amplsq( cp )		( (cp)->re * (cp)->re + (cp)->im * (cp)->im )
#define bn_cx_conj( cp )		{ (cp)->im = -(cp)->im; }
#define bn_cx_cons( cp, r, i )		{ (cp)->re = r; (cp)->im = i; }
#define bn_cx_phas( cp )		atan2( (cp)->im, (cp)->re )
#define bn_cx_scal( cp, s )		{ (cp)->re *= (s); (cp)->im *= (s); }
#define bn_cx_sub( ap, bp )		{ (ap)->re -= (bp)->re; (ap)->im -= (bp)->im;}

#define bn_cx_mul( ap, bp )	 	\
	{ FAST fastf_t a__re, b__re; \
	(ap)->re = ((a__re=(ap)->re)*(b__re=(bp)->re)) - (ap)->im*(bp)->im; \
	(ap)->im = a__re*(bp)->im + (ap)->im*b__re; }

/* Output variable "ap" is different from input variables "bp" or "cp" */
#define bn_cx_mul2( ap, bp, cp )	{ \
	(ap)->re = (cp)->re * (bp)->re - (cp)->im * (bp)->im; \
	(ap)->im = (cp)->re * (bp)->im + (cp)->im * (bp)->re; }

/*----------------------------------------------------------------------*/
/*
 *  Declarations of external functions in LIBBN.
 *  Source file names listed alphabetically.
 */

/* complex.c */
BU_EXTERN(void			bn_cx_div, (bn_complex_t *ap, CONST bn_complex_t *bp) );
BU_EXTERN(void			bn_cx_sqrt, (bn_complex_t *op, CONST bn_complex_t *ip) );

/* vers.c (created by the Cakefile) */
extern CONST char		bn_version[];

#ifdef __cplusplus
}
#endif
#endif /* SEEN_BN_H */
