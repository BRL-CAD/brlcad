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
 *	Jeff Hanes
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
/* complex.c */
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
/* poly.c */
/*
 *  Polynomial data type
 */

			/* This could be larger, or even dynamic... */
#define BN_MAX_POLY_DEGREE	4	/* Maximum Poly Order */
typedef  struct bn_poly {
	long		magic;
	int		dgr;
	double		cf[BN_MAX_POLY_DEGREE+1];
}  bn_poly_t;
#define BN_POLY_MAGIC	0x506f4c79	/* 'PoLy' */
#define BN_CK_POLY(_p)	BU_CKMAG(_p, BN_POLY_MAGIC, "struct bn_poly")
#define BN_POLY_NULL	((struct bn_poly *)NULL)

/*----------------------------------------------------------------------*/
/*
 *  Declarations of external functions in LIBBN.
 *  Source file names listed alphabetically.
 */

/* complex.c */
BU_EXTERN(void			bn_cx_div, (bn_complex_t *ap, CONST bn_complex_t *bp) );
BU_EXTERN(void			bn_cx_sqrt, (bn_complex_t *op, CONST bn_complex_t *ip) );

/* poly.c */
BU_EXTERN(struct bn_poly *	bn_poly_mul, (struct bn_poly *product,
				CONST struct bn_poly *m1, CONST struct bn_poly *m2));
BU_EXTERN(struct bn_poly *	bn_poly_scale, (struct bn_poly *eqn,
				double factor));
BU_EXTERN(struct bn_poly *	bn_poly_add, (struct bn_poly *sum,
				CONST struct bn_poly *poly1, CONST struct bn_poly *poly2));
BU_EXTERN(struct bn_poly *	bn_poly_sub, (struct bn_poly *diff,
				CONST struct bn_poly	*poly1,
				CONST struct bn_poly	*poly2));
BU_EXTERN(void			bn_poly_synthetic_division, (
				struct bn_poly *quo, struct bn_poly *rem,
				CONST struct bn_poly	*dvdend,
				CONST struct bn_poly	*dvsor));
BU_EXTERN(int			bn_poly_quadratic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*quadrat));
BU_EXTERN(int			bn_poly_cubic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*eqn));
BU_EXTERN(int			bn_poly_quartic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*eqn));
BU_EXTERN(void			bn_pr_poly, (CONST char *title,
				CONST struct bn_poly	*eqn));
BU_EXTERN(void			bn_pr_roots, (CONST char *title,
				CONST struct bn_complex	roots[], int n));

/* vers.c (created by the Cakefile) */
extern CONST char		bn_version[];

#ifdef __cplusplus
}
#endif
#endif /* SEEN_BN_H */
