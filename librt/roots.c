#include  "./polyno.h"
#include  "./complex.h"
#include  <math.h>
#include  <stdio.h>

#define   SMALL                 .000001
#define	Abs( a )	((a) >= 0 ? (a) : -(a))

int		polyRoots();
static void	findRoot(), synthetic(), compH(), nextZ(), deflate();


/*	>>>  p o l y R o o t s ( )  <<<
 *	
 *	NOTE :  This routine is written for polynomials with real coef-
 *		ficients ONLY.  To use with complex coefficients, the
 *		Complex Math library should be used throughout.
 *
 *			e.g.	typedef  struct
 *					{
 *					int		dgr;
 *					complex		cf[MAXP];
 *					}  cx_poly;
 *
 *		Some changes in the algorithm will also be required.
 */
int
polyRoots(orig,roots)
poly		*orig;		/* equation to be solved	*/
complex		*roots[];	/* space to put roots found	*/

{
  	int	n=0;		/* number of roots found	*/
	int	init, slp;	/* loop counters		*/
	poly	*copy;		/* abusable copy of equation	*/
	double	factor;		/* scaling factor for copy	*/

	copy = polyAllo();
	*copy = *orig;

	/* Allocate space for the roots and set the first guess to
	 * (almost) zero.
	 *
	 * This method seems to require a small nudge off the real axis
	 * despite documentation to the contrary.
	 */
	for ( init=0; init < MAXP; ++init ){
		roots[init] = CxCons( CxAllo(),0.0,SMALL);
	}

	/* Remove leading coefficients which are too close to zero.
	 */
	while ( Abs(copy->cf[0]) <= SMALL ){
		for ( slp=0; slp <= copy->dgr; ++slp){
			copy->cf[slp] = copy->cf[slp+1];
		}
		--copy->dgr;
	}

	/* Factor the polynomial so the first coefficient is one
	 * for ease of handling.
	 */
	factor = 1.0 / copy->cf[0];
	(void) polyScal(copy,factor);

	while ( copy->dgr > 2 ){
		/* A trailing coefficient of zero indicates that zero
		 * is a root of the equation.
		 */
		if ( copy->cf[copy->dgr] == 0.0 ){
			roots[n]->im = 0.0;
			--copy->dgr;
			++n;
			continue;	/* go to next loop		*/
		}

		findRoot(copy,roots[n]);

		if ( Abs(roots[n]->im) > SMALL* Abs(roots[n]->re) ){
			/* If root is complex, its complex conjugate is
			 * also a root since complex roots come in con-
			 * jugate pairs when all coefficients are real.
			 */
			++n;
			*roots[n] = *roots[n-1];
			(void) CxConj(roots[n]);
		} else {
			/* Change 'practically real' to real		*/
			roots[n]->im = 0.0;
		}

		deflate(copy,roots[n]);
		++n;
	}

	/* For polynomials of lower degree, iterative techniques
	 * are an inefficient way to find the roots.
	 */
	if ( copy->dgr == 1 ){
		roots[n]->re = -copy->cf[1];
		roots[n]->im = 0.0;
		++n;
	} else {
		quadratic(copy,roots[n],roots[n+1]);
		n += 2;
	}
	return n;
}


/*	>>>  f i n d R o o t ( )  <<<
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
findRoot(eqn,nxZ)
poly		*eqn;	/* polynomial			*/
complex		*nxZ;	/* initial guess for root	*/

{
	complex		p0, p1, p2;	/* evaluated polynomial
						and derivatives		*/
	complex		*Z, *H;		/* 'Z' and H(Z) in comment	*/
	int		i=0;		/* iteration counter		*/
	double		diff, dist;	/* test values for convergence	*/

	H = CxAllo();
	Z = CxAllo();

	do {
		*Z = *nxZ;
		synthetic(Z,eqn,&p0,&p1,&p2);
		compH(H,&p0,&p1,&p2,eqn->dgr);
		nextZ(nxZ,&p0,&p1,H,eqn->dgr);

		/* If the thing hasn't converged after 20 iterations,
		 * it probably won't.
		 */
		if (++i > 20)
			break;

		/* Use proportional convergence test to allow very small
		 * roots and avoid wasting time on large roots.
		 */
		diff = Abs(CxAmpl(Z) - CxAmpl(nxZ));
		if ( ( dist = CxAmpl(nxZ) - diff ) < 0 ){
			dist = 0;
		} else {
			dist = SMALL* Abs(dist);
		}
	} while ( diff > dist );

	return;
}


/*	>>>  s y n t h e t i c ( )  <<<
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
synthetic(Z,eqn,b,c,d)
poly		*eqn;
complex		*Z, *b, *c, *d;

{
	int	n, m;

	(void) CxCons(b,eqn->cf[0],0.0);
	*c = *b;
	*d = *c;


	for ( n=1; ( m = eqn->dgr - n ) >= 0; ++n){
		(void) CxMul(b,Z);
		b->re += eqn->cf[n];
		if ( m > 0 ){
			(void) CxAdd( CxMul(c,Z), b);
		}
		if ( m > 1 ){
			(void) CxAdd( CxMul(d,Z), c);
		}
	}
	return;
}


/*	>>>  c o m p H ( )  <<<
 *	Computes H for Laguerre's method.
 *	See note under 'findRoot' for explicit formula.
 */
static void
compH(H,p0,p1,p2,degr)
complex   *H, *p0, *p1, *p2;
int       degr;

{
	complex   *T;
	int       n=degr-1;

	T = CxAllo();
	*T = *p0;
	*H = *p1;

	(void) CxScal( CxMul(H,p1), (double)(n*n) );
	(void) CxScal( CxMul(T,p2), (double)(degr*n) );

	(void) CxSub(H,T);
	return;
}


/*	>>>  n e x t Z ( )  <<<
 *	Calculates the next iteration for Laguerre's method.
 *	See note under 'findRoot' for explicit formula.
 */
static void
nextZ(nxZ,p0,p1,H,degr)
complex   *nxZ, *p0, *p1, *H;
int       degr;

{
	complex   *p1A, *p1S;

	p1A = CxAllo();
	p1S = CxAllo();
	*p1A = *p1;
	*p1S = *p1;
	(void) CxSqrt(H);

	/* Test to see whether addition or subtraction gives the larger
	 * denominator for the next 'Z' , and use the appropriate value
	 * in the formula.
	 */
	if ( CxAmpl( CxSub(p1S,H) ) > CxAmpl( CxAdd(p1A,H) ) ){
		(void) CxSub(nxZ, CxDiv( CxScal(p0,(double)(degr)), p1S) );
	} else {
		(void) CxSub(nxZ, CxDiv( CxScal(p0,(double)(degr)), p1A) );
	}
	return;
}


/*	>>>  d e f l a t e ( )  <<<
 *	Deflates a polynomial by a given root.
 */
static void
deflate(oldP,root)
poly		*oldP;
complex		*root;

{
	poly		*div, *rem;

	div = polyAllo();
	rem = polyAllo();

	/* Make a polynomial out of the given root:  Linear for a real
	 * root, Quadratic for a complex root (since they come in con-
	 * jugate pairs).
	 */
	if ( root->im == 0 ) {		/*  root is real		*/
		div->dgr = 1;
		div->cf[0] = 1;
		div->cf[1] = - root->re;
	} else {			/*  root is complex		*/
		div->dgr = 2;
		div->cf[0] = 1;
		div->cf[1] = -2 * root->re;
		div->cf[2] = CxAmpl(root) * CxAmpl(root);
	}

	/* Use synthetic division to find the quotient (new polynomial)
	 * and the remainder (should be zero if the root was really a
	 * root).
	 */
	synDiv(oldP,div,oldP,rem);
	return;
}
