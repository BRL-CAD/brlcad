/*
 *  			R O O T S . C
 *
 *  Functions -
 *	polyRoots		Find the roots of a polynomial
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

#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./polyno.h"
#include "./complex.h"

int		polyRoots();
HIDDEN void	synthetic(), deflate();
HIDDEN int	findRoot(), evalpoly();

/*	>>>  p o l y R o o t s ( )  <<<
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
polyRoots( eqn, roots )
register poly		*eqn;		/* equation to be solved	*/
register complex	roots[];	/* space to put roots found	*/
{
	register int	n;		/* number of roots found	*/
	LOCAL fastf_t	factor;		/* scaling factor for copy	*/

	/* Allocate space for the roots and set the first guess to
	 * (almost) zero.
	 *
	 * This method seems to require a small nudge off the real axis
	 * despite documentation to the contrary.
	 */
	for ( n=0; n < MAXP; n++ ){
		CxCons( &roots[n], 0.0, SMALL );
	}

	/* Remove leading coefficients which are too close to zero,
	 * to prevent the polynomial factoring from blowing up, below.
	 */
	while( eqn->cf[0] > -SMALL && eqn->cf[0] < SMALL )  {
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
	(void) polyScal( eqn, factor );
	n = 0;		/* Number of roots found */

	/* A trailing coefficient of zero indicates that zero
	 * is a root of the equation.
	 */
	while ( eqn->cf[eqn->dgr] == 0.0 ){
		roots[n].re = roots[n].im = 0.0;
		--eqn->dgr;
		++n;
	}

	while ( eqn->dgr > 2 ){

		if ( eqn->dgr == 4 )  {
			if( quartic( eqn, &roots[n] ) )
				return( n+4 );
			if( evalpoly( eqn, &roots[n], 4 ) == 0 )
				return ( n+4 );
		}

		if ( eqn->dgr == 3 )  {
			if( cubic( eqn, &roots[n] ) )
				return( n+3 );
			if ( evalpoly( eqn, &roots[n], 3 ) == 0 )
				return ( n+3 );
		}

		if ( (findRoot( eqn, &roots[n] )) < 0 )
			return(n);	/* return those we found, anyways */

		if ( Abs(roots[n].im) > 1.0e-5* Abs(roots[n].re) ){
			/* If root is complex, its complex conjugate is
			 * also a root since complex roots come in con-
			 * jugate pairs when all coefficients are real.
			 */
			++n;
			roots[n] = roots[n-1];
			CxConj(&roots[n]);
		} else {
			/* Change 'practically real' to real		*/
			roots[n].im = 0.0;
		}

		deflate( eqn, &roots[n] );
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
		quadratic( eqn, &roots[n] );
		n += 2;
	}
	return n;
}

/*	>>>  f i n d R o o t ( )  <<<
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
HIDDEN int
findRoot( eqn, nxZ )
register poly		*eqn;	/* polynomial			*/
register complex	*nxZ;	/* initial guess for root	*/
{
	LOCAL complex  p0, p1, p2;	/* evaluated polynomial+derivatives */
	LOCAL complex	p1_H;		/* p1 - H, temporary */
	LOCAL complex  cZ, cH;		/* 'Z' and H(Z) in comment	*/
	LOCAL complex  T;		/* temporary for making H */
	FAST fastf_t	diff;		/* test values for convergence	*/
	FAST fastf_t	b;		/* floating temps */
	LOCAL int	n;
	register int	i;		/* iteration counter		*/

	for( i=0; i < 20; i++ ) {
		cZ = *nxZ;
		synthetic( &cZ, eqn, &p0, &p1, &p2 );

		/* Compute H for Laguerre's method. */
		n = eqn->dgr-1;
		cH = p1;
		CxMul( &cH, &p1 );
		CxScal( &cH, (double)(n*n) );
		T = p0;
		CxMul( &T, &p2 );
		CxScal( &T, (double)(eqn->dgr*n) );
		CxSub( &cH, &T );

		/* Calculate the next iteration for Laguerre's method.
		 * Test to see whether addition or subtraction gives the
		 * larger denominator for the next 'Z' , and use the
		 * appropriate value in the formula.
		 */
		CxSqrt( &cH );
		p1_H = p1;
		CxSub( &p1_H, &cH );
		CxAdd( &p1, &cH );		/* p1 <== p1+H */
		CxScal( &p0, (double)(eqn->dgr) );
		if ( CxAmplSq( &p1_H ) > CxAmplSq( &p1 ) ){
			CxDiv( &p0, &p1_H);
			CxSub( nxZ, &p0 );
		} else {
			CxDiv( &p0, &p1 );
			CxSub( nxZ, &p0 );
		}

		/* Use proportional convergence test to allow very small
		 * roots and avoid wasting time on large roots.
		 * The original version used CxAmpl(), which requires
		 * a square root.  Using CxAmplSq() saves lots of cycles,
		 * but changes loop termination conditions somewhat.
		 *
		 * diff is |p0|**2.  nxZ = Z - p0.
		 *
		 * SGI XNS IRIS 3.5 compiler fails if following 2 assignments
		 * are imbedded in the IF statement, as before.
		 */
		b = CxAmplSq( nxZ );
		diff = CxAmplSq( &p0 );
		if( b < diff )
			continue;
		if( (b-diff) == b )
			return(i);		/* OK -- can't do better */
		if( diff > (b - diff)*1.0e-5 ) 
			continue;
		return(i);			/* OK */
	}

	/* If the thing hasn't converged yet, it probably won't. */
	rt_log("findRoot:  didn't converge in %d iterations, b=%g, diff=%g\n",
		i, b, diff);
	rt_log("nxZ=%gR+%gI, p0=%gR+%gI\n", nxZ->re, nxZ->im, p0.re, p0.im);
	return(-1);		/* ERROR */
}


/*	>>>  s y n t h e t i c ( )  <<<
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
HIDDEN void
synthetic( cZ, eqn, b, c, d )
register complex	*cZ;		/* input */
register poly		*eqn;		/* input */
register complex	*b, *c, *d;	/* outputs */
{
	register int	n;
	register int	m;

	CxCons(b,eqn->cf[0],0.0);
	*c = *b;
	*d = *c;

	for ( n=1; ( m = eqn->dgr - n ) >= 0; ++n){
		CxMul( b, cZ );
		b->re += eqn->cf[n];
		if ( m > 0 ){
			CxMul( c, cZ );
			CxAdd( c, b );
		}
		if ( m > 1 ){
			CxMul( d, cZ );
			CxAdd( d, c );
		}
	}
}


/*	>>>  e v a l p o l y ( )  <<<
 *
 *	Evaluates p(Z) for any Z (real or complex).
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
HIDDEN int
evalpoly( eqn, roots, nroots )
register poly		*eqn;
register complex	roots[];
register int		nroots;
{
	LOCAL complex	epoly;
	register int	n, m;

	for ( m=0; m < nroots; ++m ){
		CxCons( &epoly, eqn->cf[0], 0.0 );

		for ( n=1; n <= eqn->dgr; ++n){
			CxMul( &epoly, &roots[m] );
			epoly.re += eqn->cf[n];
			}
		if ( Abs( epoly.re ) > 1.0e-5 || Abs( epoly.im ) > 1.0e-5 )
			return 1;
	}
	return 0;
}


/*	>>>  d e f l a t e ( )  <<<
 *
 *	Deflates a polynomial by a given root.
 */
HIDDEN void
deflate( oldP, root )
register poly		*oldP;
register complex		*root;
{
	LOCAL poly	div, rem;

	/* Make a polynomial out of the given root:  Linear for a real
	 * root, Quadratic for a complex root (since they come in con-
	 * jugate pairs).
	 */
	if ( root->im == 0 ) {
		/*  root is real		*/
		div.dgr = 1;
		div.cf[0] = 1;
		div.cf[1] = - root->re;
	} else {
		/*  root is complex		*/
		div.dgr = 2;
		div.cf[0] = 1;
		div.cf[1] = -2 * root->re;
		div.cf[2] = CxAmplSq( root );
	}

	/* Use synthetic division to find the quotient (new polynomial)
	 * and the remainder (should be zero if the root was really a
	 * root).
	 */
	synDiv( oldP, &div, oldP, &rem );
}
