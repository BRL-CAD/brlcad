/*
 *  		P O L Y L I B . C
 */
#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <setjmp.h>
#include "vmath.h"
#include "polyno.h"
#include "complex.h"

static poly	Zpoly = { 0, 0.0 };

/*
 *	polyMul -- multiply two polynomials
 */
poly *
polyMul(m1,m2,product)
register poly	*m1, *m2, *product;
{
	if( m1->dgr == 1 && m2->dgr == 1 )  {
		product->dgr = 2;
		product->cf[0] = m1->cf[0] * m2->cf[0];
		product->cf[1] = m1->cf[0] * m2->cf[1] +
				 m1->cf[1] * m2->cf[0];
		product->cf[2] = m1->cf[1] * m2->cf[1];
		return(product);
	}
	if( m1->dgr == 2 && m2->dgr == 2 )  {
		product->dgr = 4;
		product->cf[0] = m1->cf[0] * m2->cf[0];
		product->cf[1] = m1->cf[0] * m2->cf[1] +
				 m1->cf[1] * m2->cf[0];
		product->cf[2] = m1->cf[0] * m2->cf[2] +
				 m1->cf[1] * m2->cf[1] +
				 m1->cf[2] * m2->cf[0];
		product->cf[3] = m1->cf[1] * m2->cf[2] +
				 m1->cf[2] * m2->cf[1];
		product->cf[4] = m1->cf[2] * m2->cf[2];
		return(product);
	}

	/* Not one of the common (or easy) cases. */
	{
		register int		ct1, ct2;

		*product = Zpoly;

		/* If the degree of the product will be larger than the
		 * maximum size allowed in "polyno.h", then return a null
		 * pointer to indicate failure.
		 */
		if ( (product->dgr = m1->dgr + m2->dgr) > MAXP )
			return PM_NULL;

		for ( ct1=0; ct1 <= m1->dgr; ++ct1 ){
			for ( ct2=0; ct2 <= m2->dgr; ++ct2 ){
				product->cf[ct1+ct2] +=
					m1->cf[ct1] * m2->cf[ct2];
			}
		}
	}
	return product;
}


/*
 *	polyScal -- scale a polynomial
 */
poly *
polyScal(eqn,factor)
register poly	*eqn;
double	factor;
{
	register int		cnt;

	for ( cnt=0; cnt <= eqn->dgr; ++cnt ){
		eqn->cf[cnt] *= factor;
	}
	return eqn;
}


/*
 *	polyAdd -- add two polynomials
 */
poly *
polyAdd(poly1,poly2,sum)
register poly	*poly1, *poly2, *sum;
{
	LOCAL poly		tmp;
	register int		i, offset;

	offset = Abs(poly1->dgr - poly2->dgr);

	tmp = Zpoly;

	if ( poly1->dgr >= poly2->dgr ){
		*sum = *poly1;
		for ( i=0; i <= poly2->dgr; ++i ){
			tmp.cf[i+offset] = poly2->cf[i];
		}
	} else {
		*sum = *poly2;
		for ( i=0; i <= poly1->dgr; ++i ){
			tmp.cf[i+offset] = poly1->cf[i];
		}
	}

	for ( i=0; i <= sum->dgr; ++i ){
		sum->cf[i] += tmp.cf[i];
	}
	return sum;
}


/*
 *	polySub -- subtract two polynomials
 */
poly *
polySub(poly1,poly2,diff)
register poly	*poly1, *poly2, *diff;
{
	LOCAL poly		tmp;
	register int		i, offset;

	offset = Abs(poly1->dgr - poly2->dgr);

	*diff = Zpoly;
	tmp = Zpoly;

	if ( poly1->dgr >= poly2->dgr ){
		*diff = *poly1;
		for ( i=0; i <= poly2->dgr; ++i ){
			tmp.cf[i+offset] = poly2->cf[i];
		}
	} else {
		diff->dgr = poly2->dgr;
		for ( i=0; i <= poly1->dgr; ++i ){
			diff->cf[i+offset] = poly1->cf[i];
		}
		tmp = *poly2;
	}

	for ( i=0; i <= diff->dgr; ++i ){
		diff->cf[i] -= tmp.cf[i];
	}
	return diff;
}


/*	>>>  s y n D i v ( )  <<<
 *	Divides any polynomial into any other polynomial using synthetic
 *	division.  Both polynomials must have real coefficients.
 */
void
synDiv(dvdend,dvsor,quo,rem)
register poly	*dvdend, *dvsor, *quo, *rem;
{
	register int	div;
	register int	n;

	*quo = *dvdend;
	*rem = Zpoly;

	if ((quo->dgr = dvdend->dgr - dvsor->dgr) < 0)
		quo->dgr = -1;
	if ((rem->dgr = dvsor->dgr - 1) > dvdend->dgr)
		rem->dgr = dvdend->dgr;

	for ( n=0; n <= quo->dgr; ++n){
		quo->cf[n] /= dvsor->cf[0];
		for ( div=1; div <= dvsor->dgr; ++div){
			quo->cf[n+div] -= quo->cf[n] * dvsor->cf[div];
		}
	}
	for ( n=1; n<=(rem->dgr+1); ++n){
		rem->cf[n-1] = quo->cf[quo->dgr+n];
		quo->cf[quo->dgr+n] = 0;
	}
}


/*	>>>  q u a d r a t i c ( )  <<<
 *
 *	Uses the quadratic formula to find the roots (in `complex' form)
 *	of any quadratic equation with real coefficients.
 */
void
quadratic( quad, root )
register poly		*quad;
register complex	root[];
{
	LOCAL fastf_t	discrim, denom, rad;

	discrim = quad->cf[1]*quad->cf[1] - 4.0* quad->cf[0]*quad->cf[2];
	denom = 2.0* quad->cf[0];
	if ( discrim >= 0.0 ){
		rad = sqrt( discrim );
		root[0].re = ( -quad->cf[1] + rad )/ denom;
		root[0].im = 0.0;
		root[1].re = ( -quad->cf[1] - rad )/ denom;
		root[1].im = 0.0;
	} else {
		rad = sqrt( -discrim );
		root[0].re = -quad->cf[1] / denom;
		root[0].im =  rad / denom;
		root[1].re = -quad->cf[1] / denom;
		root[1].im = -rad / denom;
	}
}


/*	>>>  c u b i c ( )  <<<
 *
 *	Uses the cubic formula to find the roots ( in `complex' form )
 *	of any cubic equation with real coefficients.
 *
 *	to solve a polynomial of the form:
 *
 *		X**3 + c1*X**2 + c2*X + c3 = 0,
 *
 *	first reduce it to the form:
 *
 *		Y**3 + a*Y + b = 0,
 *
 *	where
 *		Y = X + c1/3,
 *	and
 *		a = c2 - c1**2/3,
 *		b = ( 2*c1**3 - 9*c1*c2 + 27*c3 )/27.
 *
 *	Then we define the value delta,   D = b**2/4 + a**3/27.
 *
 *	If D > 0, there will be one real root and two conjugate
 *	complex roots.
 *	If D = 0, there will be three real roots at least two of
 *	which are equal.
 *	If D < 0, there will be three unequal real roots.
 *
 *	Returns 1 for success, 0 for fail.
 */
static jmp_buf abort_buf;
static void cubic_catch()  {
	fprintf(stderr,"cubic FPE\n");
	longjmp(abort_buf, 1);	/* return error code */
}

int
cubic( eqn, root )
register poly		*eqn;
register complex	root[];
{
	LOCAL fastf_t		a, b, c1, delta;
	register int		i;
	void (*sav)();

	sav=signal(SIGFPE, cubic_catch);
	if( setjmp( abort_buf ) )  {
		signal(SIGFPE, sav);
		pr_poly(eqn);
		return(0);	/* FAIL */
	}

	c1 = eqn->cf[1];

	a = eqn->cf[2] - c1*c1/3.0;
	b = ( 2.0*c1*c1*c1 - 9.0*c1*eqn->cf[2] + 27.0*eqn->cf[3] )/27.0;

	delta = b*b/4.0 + a*a*a/27.0;

#define SQRT3			1.732050808

#define third			.3333333333333333333333333
#define	CubeRoot( a )	( a >= 0.0 ) ? pow( a, third ) : -pow( -a, third )

	if ( delta > 0.0 ){
		LOCAL fastf_t		r_delta, A, B;

		r_delta = sqrt( delta );
		A = -b*0.5 + r_delta;
		B = -b*0.5 - r_delta;

		A = CubeRoot( A );
		B = CubeRoot( B );

		root[2].re = root[1].re = -( root[0].re = A + B )*0.5;

		root[0].im = 0.0;
		root[2].im = -( root[1].im = (A - B)*SQRT3*0.5 );
	} else if ( delta == 0.0 ){
		LOCAL fastf_t	b_2;
		b_2 = -b*0.5;

		root[0].re = 2.0* CubeRoot( b_2 );
		root[2].re = root[1].re = -root[0].re*0.5;
		root[2].im = root[1].im = root[0].im = 0.0;
	} else {
		LOCAL fastf_t		cs_3phi, phi, fact;
		LOCAL fastf_t		cs_phi, sn_phi;

		fact = sqrt( -a/3.0 );
		cs_3phi = ( -b*0.5 )/( fact*fact*fact );
		phi = acos( cs_3phi )/3.0;
		cs_phi = cos( phi );
		sn_phi = sin( phi );

		root[0].re = 2.0*fact*cs_phi; 
		root[1].re = fact*( -cs_phi + SQRT3*sn_phi );
		root[2].re = fact*( -cs_phi - SQRT3*sn_phi );

		root[2].im = root[1].im = root[0].im = 0.0;
	}
	for ( i=0; i < 3; ++i ){
		root[i].re -= c1/3.0;
	}
	(void)signal(SIGFPE, sav);
	return(1);		/* OK */
}


/*	>>>  q u a r t i c ( )  <<<
 *
 *	Uses the quartic formula to find the roots ( in `complex' form )
 *	of any quartic equation with real coefficients.
 *
 *	Returns 1 for success, 0 for fail.
 */
int
quartic( eqn, root )
register poly		*eqn;
register complex	root[];
{
	LOCAL poly		cube, quad1, quad2;
	LOCAL complex		u[3];
	LOCAL fastf_t		U, p, q, q1, q2;
#define Max3( a, b, c )		( Max( c, Max( a, b )) )

	cube.dgr = 3;
	cube.cf[0] = 1.0;
	cube.cf[1] = -eqn->cf[2];
	cube.cf[2] = eqn->cf[3]*eqn->cf[1]
			- 4*eqn->cf[4];
	cube.cf[3] = -eqn->cf[3]*eqn->cf[3]
			- eqn->cf[4]*eqn->cf[1]*eqn->cf[1]
			+ 4*eqn->cf[4]*eqn->cf[2];

	if( !cubic( &cube, u ) )
		return( 0 );		/* FAIL */
	if ( u[1].im != 0.0 ){
		U = u[0].re;
	} else {
		U = Max3( u[0].re, u[1].re, u[2].re );
	}

#define NearZero( a )		{ if ( a < 0.0 && a > -SMALL ) a = 0.0; }
	p = eqn->cf[1]*eqn->cf[1]/4.0 + U - eqn->cf[2];
	q = U*U/4.0 - eqn->cf[4];
	NearZero( p );
	NearZero( q );
	if ( p < 0 || q < 0 ){
		return(0);	/* FAIL */
	} else {
		p = sqrt( p );
		q = sqrt( q );
	}

	quad1.dgr = quad2.dgr = 2;
	quad1.cf[0] = quad2.cf[0] = 1.0;
	quad1.cf[1] = eqn->cf[1]*0.5 - p;
	quad2.cf[1] = eqn->cf[1]*0.5 + p;
	q1 = U*0.5 - q;
	q2 = U*0.5 + q;
	if ( Abs( quad1.cf[1]*q2 + quad2.cf[1]*q1 - eqn->cf[3] ) < SMALL){
		quad1.cf[2] = q1;
		quad2.cf[2] = q2;
	} else if ( Abs( quad1.cf[1]*q1 + quad2.cf[1]*q2 - eqn->cf[3]) < SMALL ){
		quad1.cf[2] = q2;
		quad2.cf[2] = q1;
	} else {
		return(0);	/* FAIL */
	}

	quadratic( &quad1, root );
	quadratic( &quad2, &root[2] );
	return(1);		/* SUCCESS */
}


/*
 *			P R _ P O L Y
 */
void
pr_poly(eqn)
register poly	*eqn;
{
	register int	n;

	fprintf(stderr,"\nDegree of polynomial = %d\n",eqn->dgr);
	for ( n=0; n<=eqn->dgr; ++n){
		fprintf(stderr," %g ",eqn->cf[n]);
	}
	fprintf(stderr,"\n");
}
