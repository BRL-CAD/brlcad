/*
 *  			R O O T S . C
 *
 *  Functions -
 *	rt_poly_roots		Find the roots of a polynomial
 *
 *  Author -
 *	Jeff Hanes
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSroots[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

int		rt_poly_roots();
void	rt_poly_eval_w_2derivatives(), rt_poly_deflate();
int	rt_poly_findroot(), rt_poly_checkroots();

/*
 *			R T _ P O L Y _ R O O T S
 *	
 *	WARNING:  The polynomial given as input is destroyed by this
 *		routine.  The caller must save it if it is important!
 *
 *	NOTE :  This routine is written for polynomials with real coef-
 *		ficients ONLY.  To use with complex coefficients, the
 *		Complex Math library should be used throughout.
 *		Some changes in the algorithm will also be required.
 */
int
rt_poly_roots( eqn, roots )
register bn_poly_t	*eqn;		/* equation to be solved	*/
register bn_complex_t	roots[];	/* space to put roots found	*/
{
	register int	n;		/* number of roots found	*/
	LOCAL fastf_t	factor;		/* scaling factor for copy	*/

	/* Remove leading coefficients which are too close to zero,
	 * to prevent the polynomial factoring from blowing up, below.
	 */
	while( NEAR_ZERO( eqn->cf[0], SMALL ) )  {
		for ( n=0; n <= eqn->dgr; n++ ){
			eqn->cf[n] = eqn->cf[n+1];
		}
		if ( --eqn->dgr <= 0 )
			return 0;
	}

	/* Factor the polynomial so the first coefficient is one
	 * for ease of handling.
	 */
	factor = 1.0 / eqn->cf[0];
	(void) bn_poly_scale( eqn, factor );
	n = 0;		/* Number of roots found */

	/* A trailing coefficient of zero indicates that zero
	 * is a root of the equation.
	 */
	while( NEAR_ZERO( eqn->cf[eqn->dgr], SMALL ) )  {
		roots[n].re = roots[n].im = 0.0;
		--eqn->dgr;
		++n;
	}

	while ( eqn->dgr > 2 ){
		if ( eqn->dgr == 4 )  {
			if( rt_poly_quartic_roots( eqn, &roots[n] ) )  {
				if( rt_poly_checkroots( eqn, &roots[n], 4 ) == 0 )  {
					return( n+4 );
				}
			}
		} else if ( eqn->dgr == 3 )  {
			if( rt_poly_cubic_roots( eqn, &roots[n] ) )  {
				if ( rt_poly_checkroots( eqn, &roots[n], 3 ) == 0 )  {
					return ( n+3 );
				}
			}
		}

		/*
		 *  Set initial guess for root to almost zero.
		 *  This method requires a small nudge off the real axis.
		 */
		bn_cx_cons( &roots[n], 0.0, SMALL );
		if ( (rt_poly_findroot( eqn, &roots[n] )) < 0 )
			return(n);	/* return those we found, anyways */

		if ( fabs(roots[n].im) > 1.0e-5* fabs(roots[n].re) ){
			/* If root is complex, its complex conjugate is
			 * also a root since complex roots come in con-
			 * jugate pairs when all coefficients are real.
			 */
			++n;
			roots[n] = roots[n-1];
			bn_cx_conj(&roots[n]);
		} else {
			/* Change 'practically real' to real		*/
			roots[n].im = 0.0;
		}

		rt_poly_deflate( eqn, &roots[n] );
		++n;
	}

	/* For polynomials of lower degree, iterative techniques
	 * are an inefficient way to find the roots.
	 */
	if ( eqn->dgr == 1 ){
		roots[n].re = -(eqn->cf[1]);
		roots[n].im = 0.0;
		++n;
	} else if ( eqn->dgr == 2 ){
		rt_poly_quadratic_roots( eqn, &roots[n] );
		n += 2;
	}
	return n;
}

/*
 *			R T _ P O L Y _ F I N D R O O T
 *
 *	Calculates one root of a polynomial ( p(Z) ) using Laguerre's
 *	method.  This is an iterative technique which has very good
 *	global convergence properties.  The formulas for this method
 *	are
 *
 *					n * p(Z)
 *		newZ  =  Z  -  -----------------------  ,
 *				p'(Z) +- sqrt( H(Z) )
 *
 *	where
 *		H(Z) = (n-1) [ (n-1)(p'(Z))^2 - n p(Z)p''(Z) ],
 *
 *	where n is the degree of the polynomial.  The sign in the 
 *	denominator is chosen so that  |newZ - Z|  is as small as
 *	possible.
 *
 */
int
rt_poly_findroot( eqn, nxZ )
register bn_poly_t	*eqn;	/* polynomial			*/
register bn_complex_t	*nxZ;	/* initial guess for root	*/
{
	LOCAL complex  p0, p1, p2;	/* evaluated polynomial+derivatives */
	LOCAL bn_complex_t	p1_H;		/* p1 - H, temporary */
	LOCAL complex  cZ, cH;		/* 'Z' and H(Z) in comment	*/
	LOCAL complex  T;		/* temporary for making H */
	FAST fastf_t	diff;		/* test values for convergence	*/
	FAST fastf_t	b;		/* floating temps */
	LOCAL int	n;
	register int	i;		/* iteration counter		*/

	for( i=0; i < 20; i++ ) {
		cZ = *nxZ;
		rt_poly_eval_w_2derivatives( &cZ, eqn, &p0, &p1, &p2 );

		/* Compute H for Laguerre's method. */
		n = eqn->dgr-1;
		bn_cx_mul2( &cH, &p1, &p1 );
		bn_cx_scal( &cH, (double)(n*n) );
		bn_cx_mul2( &T, &p2, &p0 );
		bn_cx_scal( &T, (double)(eqn->dgr*n) );
		bn_cx_sub( &cH, &T );

		/* Calculate the next iteration for Laguerre's method.
		 * Test to see whether addition or subtraction gives the
		 * larger denominator for the next 'Z' , and use the
		 * appropriate value in the formula.
		 */
		bn_cx_sqrt( &cH, &cH );
		p1_H = p1;
		bn_cx_sub( &p1_H, &cH );
		bn_cx_add( &p1, &cH );		/* p1 <== p1+H */
		bn_cx_scal( &p0, (double)(eqn->dgr) );
		if ( bn_cx_amplsq( &p1_H ) > bn_cx_amplsq( &p1 ) ){
			bn_cx_div( &p0, &p1_H);
			bn_cx_sub( nxZ, &p0 );
		} else {
			bn_cx_div( &p0, &p1 );
			bn_cx_sub( nxZ, &p0 );
		}

		/* Use proportional convergence test to allow very small
		 * roots and avoid wasting time on large roots.
		 * The original version used bn_cx_ampl(), which requires
		 * a square root.  Using bn_cx_amplsq() saves lots of cycles,
		 * but changes loop termination conditions somewhat.
		 *
		 * diff is |p0|**2.  nxZ = Z - p0.
		 *
		 * SGI XNS IRIS 3.5 compiler fails if following 2 assignments
		 * are imbedded in the IF statement, as before.
		 */
		b = bn_cx_amplsq( nxZ );
		diff = bn_cx_amplsq( &p0 );
		if( b < diff )
			continue;
		if( (b-diff) == b )
			return(i);		/* OK -- can't do better */
		if( diff > (b - diff)*1.0e-5 ) 
			continue;
		return(i);			/* OK */
	}

	/* If the thing hasn't converged yet, it probably won't. */
	bu_log("rt_poly_findroot:  didn't converge in %d iterations, b=%g, diff=%g\n",
		i, b, diff);
	bu_log("nxZ=%gR+%gI, p0=%gR+%gI\n", nxZ->re, nxZ->im, p0.re, p0.im);
	return(-1);		/* ERROR */
}


/*
 *			R T _ P O L Y _ E V A L _ W _ 2 D E R I V A T I V E S
 *
 *	Evaluates p(Z), p'(Z), and p''(Z) for any Z (real or complex).
 *	Given an equation of the form
 *
 *		p(Z) = a0*Z^n + a1*Z^(n-1) +... an != 0,
 *
 *	the function value and derivatives needed for the iterations
 *	can be computed by synthetic division using the formulas
 *
 *		p(Z) = bn,    p'(Z) = c(n-1),    p''(Z) = d(n-2),
 *
 *	where
 *
 *		b0 = a0,	bi = b(i-1)*Z + ai,	i = 1,2,...n
 *		c0 = b0,	ci = c(i-1)*Z + bi,	i = 1,2,...n-1
 *		d0 = c0,	di = d(i-1)*Z + ci,	i = 1,2,...n-2
 *
 */
void
rt_poly_eval_w_2derivatives( cZ, eqn, b, c, d )
register bn_complex_t	*cZ;		/* input */
register bn_poly_t	*eqn;		/* input */
register bn_complex_t	*b, *c, *d;	/* outputs */
{
	register int	n;
	register int	m;

	bn_cx_cons(b,eqn->cf[0],0.0);
	*c = *b;
	*d = *c;

	for ( n=1; ( m = eqn->dgr - n ) >= 0; ++n){
		bn_cx_mul( b, cZ );
		b->re += eqn->cf[n];
		if ( m > 0 ){
			bn_cx_mul( c, cZ );
			bn_cx_add( c, b );
		}
		if ( m > 1 ){
			bn_cx_mul( d, cZ );
			bn_cx_add( d, c );
		}
	}
}


/*
 *			R T _ P O L Y _ C H E C K R O O T S
 *
 *	Evaluates p(Z) for any Z (real or complex).
 *	In this case, test all "nroots" entries of roots[] to ensure
 *	that they are roots (zeros) of this polynomial.
 *
 * Returns -
 *	0	all roots are true zeros
 *	1	at least one "root[]" entry is not a true zero
 *
 *	Given an equation of the form
 *
 *		p(Z) = a0*Z^n + a1*Z^(n-1) +... an != 0,
 *
 *	the function value can be computed using the formula
 *
 *		p(Z) = bn,	where
 *
 *		b0 = a0,	bi = b(i-1)*Z + ai,	i = 1,2,...n
 *
 */
int
rt_poly_checkroots( eqn, roots, nroots )
register bn_poly_t		*eqn;
bn_complex_t			roots[];
register int		nroots;
{
	register fastf_t	er, ei;		/* "epoly" */
	register fastf_t	zr, zi;		/* Z value to evaluate at */
	register int	n;
	int		m;

	for ( m=0; m < nroots; ++m ){
		/* Select value of Z to evaluate at */
		zr = bn_cx_real( &roots[m] );
		zi = bn_cx_imag( &roots[m] );

		/* Initialize */
		er = eqn->cf[0];
		/* ei = 0.0; */

		/* n=1 step.  Broken out because ei = 0.0 */
		ei = er*zi;		/* must come before er= */
		er = er*zr + eqn->cf[1];

		/* Remaining steps */
		for ( n=2; n <= eqn->dgr; ++n)  {
			register fastf_t	tr, ti;	/* temps */
			tr = er*zr - ei*zi + eqn->cf[n];
			ti = er*zi + ei*zr;
			er = tr;
			ei = ti;
		}
		if ( fabs( er ) > 1.0e-5 || fabs( ei ) > 1.0e-5 )
			return 1;	/* FAIL */
	}
	/* Evaluating polynomial for all Z values gives zero */
	return 0;			/* OK */
}


/*
 *			R T _ P O L Y _ D E F L A T E
 *
 *
 *	Deflates a polynomial by a given root.
 */
void
rt_poly_deflate( oldP, root )
register bn_poly_t	*oldP;
register bn_complex_t	*root;
{
	LOCAL bn_poly_t	div, rem;

	/* Make a polynomial out of the given root:  Linear for a real
	 * root, Quadratic for a complex root (since they come in con-
	 * jugate pairs).
	 */
	if ( NEAR_ZERO( root->im, SMALL) ) {
		/*  root is real		*/
		div.dgr = 1;
		div.cf[0] = 1;
		div.cf[1] = - root->re;
	} else {
		/*  root is bn_complex_t		*/
		div.dgr = 2;
		div.cf[0] = 1;
		div.cf[1] = -2 * root->re;
		div.cf[2] = bn_cx_amplsq( root );
	}

	/* Use synthetic division to find the quotient (new polynomial)
	 * and the remainder (should be zero if the root was really a
	 * root).
	 */
	rt_poly_synthetic_division( oldP, &div, oldP, &rem );
}
