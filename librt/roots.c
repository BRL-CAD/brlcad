#include  "./polyno.h"
#include  "./complex.h"
#include  <math.h>
#include  <stdio.h>

#define	SMALL		0.000001
#define	Abs( a )	((a) >= 0 ? (a) : -(a))

int		polyRoots();
static void	findRoot(), synthetic(), compH(), nextZ(), deflate();


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
register  poly	*eqn;		/* equation to be solved	*/
register complex roots[];	/* space to put roots found	*/
{
	register int	n;		/* number of roots found	*/
	static double	factor;		/* scaling factor for copy	*/

	/* Allocate space for the roots and set the first guess to
	 * (almost) zero.
	 *
	 * This method seems to require a small nudge off the real axis
	 * despite documentation to the contrary.
	 */
	for ( n=0; n < MAXP; n++ ){
		CxCons( &roots[n], 0.0, SMALL );
	}

	/* Remove leading coefficients which are too close to zero.
	 */
	while ( Abs(eqn->cf[0]) <= SMALL ){
		for ( n=0; n <= eqn->dgr; n++ ){
			eqn->cf[n] = eqn->cf[n+1];
		}
		--eqn->dgr;
	}

	/* Factor the polynomial so the first coefficient is one
	 * for ease of handling.
	 */
	factor = 1.0 / eqn->cf[0];
	(void) polyScal( eqn, factor );

	n = 0;		/* Number of roots found */
	while ( eqn->dgr > 2 ){
		/* A trailing coefficient of zero indicates that zero
		 * is a root of the equation.
		 */
		if ( eqn->cf[eqn->dgr] == 0.0 ){
			roots[n].im = 0.0;
			--eqn->dgr;
			++n;
			continue;	/* go to next loop		*/
		}

		findRoot( eqn, &(roots[n]) );

		if ( Abs(roots[n].im) > SMALL* Abs(roots[n].re) ){
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
	} else {
		quadratic( eqn, &roots[n], &roots[n+1] );
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
static void
findRoot( eqn, nxZ )
register poly		*eqn;	/* polynomial			*/
register complex	*nxZ;	/* initial guess for root	*/
{
	static complex  p0, p1, p2;	/* evaluated polynomial+derivatives */
	static complex  Z, H;		/* 'Z' and H(Z) in comment	*/
	static double	diff, dist;	/* test values for convergence	*/
	static double	f;		/* floating temp */
	register int	i;		/* iteration counter		*/

	i = 0;
	do {
		Z = *nxZ;
		synthetic( &Z, eqn, &p0, &p1, &p2 );
		compH( &H, &p0, &p1, &p2, eqn->dgr );
		nextZ( nxZ, &p0, &p1, &H, eqn->dgr );

		/* If the thing hasn't converged after 20 iterations,
		 * it probably won't.
		 */
		if (++i > 20)
			break;

		/* Use proportional convergence test to allow very small
		 * roots and avoid wasting time on large roots.
		 */
		f = CxAmpl( nxZ );
		diff = CxAmpl( &Z ) - f;
		diff = Abs( diff );
		if ( ( dist = f - diff ) < 0 ){
			dist = 0;
		} else {
			dist = SMALL* Abs(dist);
		}
	} while ( diff > dist );
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
static void
synthetic( Z, eqn, b, c, d )
register poly		*eqn;
register complex	*Z, *b, *c, *d;
{
	register int	n;
	static int	m;

	CxCons(b,eqn->cf[0],0.0);
	*c = *b;
	*d = *c;

	for ( n=1; ( m = eqn->dgr - n ) >= 0; ++n){
		CxMul( b, Z );
		b->re += eqn->cf[n];
		if ( m > 0 ){
			CxMul( c, Z );
			CxAdd( c, b );
		}
		if ( m > 1 ){
			CxMul( d, Z );
			CxAdd( d, c );
		}
	}
}


/*	>>>  c o m p H ( )  <<<
 *
 *	Computes H for Laguerre's method.
 *	See note under 'findRoot' for explicit formula.
 */
static void
compH( H, p0, p1, p2, degr )
register complex   *H, *p0, *p1, *p2;
int       degr;
{
	static complex   T;
	register int       n=degr-1;

	T = *p0;
	*H = *p1;

	CxMul( H, p1 );
	CxScal( H, (double)(n*n) );
	CxMul( &T, p2 );
	CxScal( &T, (double)(degr*n) );

	CxSub( H, &T );
}


/*	>>>  n e x t Z ( )  <<<
 *
 *	Calculates the next iteration for Laguerre's method.
 *	See note under 'findRoot' for explicit formula.
 */
static void
nextZ( nxZ, p0, p1, H, degr )
register complex   *nxZ, *p0, *p1, *H;
int       degr;
{
	static complex   p1A, p1S;

	p1A = *p1;
	p1S = *p1;
	CxSqrt(H);
	/* Test to see whether addition or subtraction gives the larger
	 * denominator for the next 'Z' , and use the appropriate value
	 * in the formula.
	 */
	CxSub( &p1S, H );
	CxAdd( &p1A, H );
	CxScal( p0, (double)(degr) );
	if ( CxAmpl( &p1S ) > CxAmpl( &p1A ) ){
		CxDiv( p0, &p1S);
		CxSub( nxZ, p0 );
	} else {
		CxDiv( p0, &p1A );
		CxSub( nxZ, p0 );
	}
}


/*	>>>  d e f l a t e ( )  <<<
 *
 *	Deflates a polynomial by a given root.
 */
static void
deflate( oldP, root )
register poly		*oldP;
register complex		*root;
{
	static poly	div, rem;

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
		div.cf[2] = root->re * root->re + root->im * root->im;
	}

	/* Use synthetic division to find the quotient (new polynomial)
	 * and the remainder (should be zero if the root was really a
	 * root).
	 */
	synDiv( oldP, &div, oldP, &rem );
}
