/*                          P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup poly */
/*@{*/
/** @file poly.c
 *	Library for dealing with polynomials.
 *
 *  Author -
 *	Jeff Hanes
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */


#ifndef lint
static const char RCSpoly[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

#define Abs( a )		((a) >= 0 ? (a) : -(a))
#define Max( a, b )		((a) > (b) ? (a) : (b))

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif
#define PI_DIV_3	(M_PI/3.0)

static const struct bn_poly	bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };

/**
 *	bn_poly_mul 
 *
 * @brief multiply two polynomials
 */
struct bn_poly *
bn_poly_mul(register struct bn_poly *product, register const struct bn_poly *m1, register const struct bn_poly *m2)
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

		*product = bn_Zero_poly;

		/* If the degree of the product will be larger than the
		 * maximum size allowed in "polyno.h", then return a null
		 * pointer to indicate failure.
		 */
		if ( (product->dgr = m1->dgr + m2->dgr) > BN_MAX_POLY_DEGREE )
			return BN_POLY_NULL;

		for ( ct1=0; ct1 <= m1->dgr; ++ct1 ){
			for ( ct2=0; ct2 <= m2->dgr; ++ct2 ){
				product->cf[ct1+ct2] +=
					m1->cf[ct1] * m2->cf[ct2];
			}
		}
	}
	return product;
}


/**
 *	bn_poly_scale 
 * @brief
 * scale a polynomial
 */
struct bn_poly *
bn_poly_scale(register struct bn_poly *eqn, double factor)
{
	register int		cnt;

	for ( cnt=0; cnt <= eqn->dgr; ++cnt ){
		eqn->cf[cnt] *= factor;
	}
	return eqn;
}


/**
 *	bn_poly_add 
 * @brief
 * add two polynomials
 */
struct bn_poly *
bn_poly_add(register struct bn_poly *sum, register const struct bn_poly *poly1, register const struct bn_poly *poly2)
{
	LOCAL struct bn_poly	tmp;
	register int		i, offset;

	offset = Abs(poly1->dgr - poly2->dgr);

	tmp = bn_Zero_poly;

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


/**
 *	bn_poly_sub 
 * @brief
 * subtract two polynomials
 */
struct bn_poly *
bn_poly_sub(register struct bn_poly *diff, register const struct bn_poly *poly1, register const struct bn_poly *poly2)
{
	LOCAL struct bn_poly	tmp;
	register int		i, offset;

	offset = Abs(poly1->dgr - poly2->dgr);

	*diff = bn_Zero_poly;
	tmp = bn_Zero_poly;

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


/**	s y n D i v ( )
 * @brief
 *	Divides any polynomial into any other polynomial using synthetic
 *	division.  Both polynomials must have real coefficients.
 */
void
bn_poly_synthetic_division(register struct bn_poly *quo, register struct bn_poly *rem, register const struct bn_poly *dvdend, register const struct bn_poly *dvsor)
{
	register int	div;
	register int	n;

	*quo = *dvdend;
	*rem = bn_Zero_poly;

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


/**	q u a d r a t i c ( )
 *@brief
 *	Uses the quadratic formula to find the roots (in `complex' form)
 *	of any quadratic equation with real coefficients.
 */
int
bn_poly_quadratic_roots(register struct bn_complex *roots, register const struct bn_poly *quadrat)
{
	LOCAL fastf_t	discrim, denom, rad;

	if( NEAR_ZERO( quadrat->cf[0], SQRT_SMALL_FASTF ) )  {
		/* root = -cf[2] / cf[1] */
		if( NEAR_ZERO( quadrat->cf[1], SQRT_SMALL_FASTF ) )  {
			/* No solution.  Now what? */
			bu_log("bn_poly_quadratic_roots(): ERROR, no solution\n");
			return -1;
		}
		/* Fake it as a repeated root. */
		roots[0].re = roots[1].re = -quadrat->cf[2]/quadrat->cf[1];
		roots[0].im = roots[1].im = 0.0;
		return 1;	/* OK - repeated root */
	}
	/* What to do if cf[1] > SQRT_MAX_FASTF ? */

	discrim = quadrat->cf[1]*quadrat->cf[1] - 4.0* quadrat->cf[0]*quadrat->cf[2];
	denom = 0.5 / quadrat->cf[0];
	if ( discrim >= 0.0 ){
		rad = sqrt( discrim );
		roots[0].re = ( -quadrat->cf[1] + rad ) * denom;
		roots[1].re = ( -quadrat->cf[1] - rad ) * denom;
		roots[1].im = roots[0].im = 0.0;
	} else {
		roots[1].re = roots[0].re = -quadrat->cf[1] * denom;
		roots[1].im = -(roots[0].im = sqrt( -discrim ) * denom);
	}
	return 0;		/* OK */
}


#define SQRT3			1.732050808
#define THIRD			0.333333333333333333333333333
#define INV_TWENTYSEVEN		0.037037037037037037037037037
#define	CUBEROOT( a )	(( (a) >= 0.0 ) ? pow( a, THIRD ) : -pow( -(a), THIRD ))

/**	c u b i c ( )
 *@brief
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
static int	bn_expecting_fpe = 0;
static jmp_buf	bn_abort_buf;
HIDDEN void bn_catch_FPE(int sig)
{
	if( !bn_expecting_fpe )
		bu_bomb("bn_catch_FPE() unexpected SIGFPE!");
	if( !bu_is_parallel() )
		(void)signal(SIGFPE, bn_catch_FPE);	/* Renew handler */
	longjmp(bn_abort_buf, 1);	/* return error code */
}

/*
 *			B N _ P O L Y _ C U B I C _ R O O T S
 */
int
bn_poly_cubic_roots(register struct bn_complex *roots, register const struct bn_poly *eqn)
{
	LOCAL fastf_t	a, b, c1, c1_3rd, delta;
	register int	i;
	static int	first_time = 1;

	if( !bu_is_parallel() ) {
		/* bn_abort_buf is NOT parallel! */
		if( first_time )  {
			first_time = 0;
			(void)signal(SIGFPE, bn_catch_FPE);
		}
		bn_expecting_fpe = 1;
		if( setjmp( bn_abort_buf ) )  {
			(void)signal(SIGFPE, bn_catch_FPE);
			bu_log("bn_poly_cubic_roots() Floating Point Error\n");
			return(0);	/* FAIL */
		}
	}

	c1 = eqn->cf[1];
	if( Abs(c1) > SQRT_MAX_FASTF )  return(0);	/* FAIL */

	c1_3rd = c1 * THIRD;
	a = eqn->cf[2] - c1*c1_3rd;
	if( Abs(a) > SQRT_MAX_FASTF )  return(0);	/* FAIL */
	b = (2.0*c1*c1*c1 - 9.0*c1*eqn->cf[2] + 27.0*eqn->cf[3])*INV_TWENTYSEVEN;
	if( Abs(b) > SQRT_MAX_FASTF )  return(0);	/* FAIL */

	if( (delta = a*a) > SQRT_MAX_FASTF ) return(0);	/* FAIL */
	delta = b*b*0.25 + delta*a*INV_TWENTYSEVEN;

	if ( delta > 0.0 ){
		LOCAL fastf_t		r_delta, A, B;

		r_delta = sqrt( delta );
		A = B = -0.5 * b;
		A += r_delta;
		B -= r_delta;

		A = CUBEROOT( A );
		B = CUBEROOT( B );

		roots[2].re = roots[1].re = -0.5 * ( roots[0].re = A + B );

		roots[0].im = 0.0;
		roots[2].im = -( roots[1].im = (A - B)*SQRT3*0.5 );
	} else if ( delta == 0.0 ){
		LOCAL fastf_t	b_2;
		b_2 = -0.5 * b;

		roots[0].re = 2.0* CUBEROOT( b_2 );
		roots[2].re = roots[1].re = -0.5 * roots[0].re;
		roots[2].im = roots[1].im = roots[0].im = 0.0;
	} else {
		LOCAL fastf_t		phi, fact;
		LOCAL fastf_t		cs_phi, sn_phi_s3;

		if( a >= 0.0 )  {
			fact = 0.0;
			phi = 0.0;
			cs_phi = 1.0;		/* cos( phi ); */
			sn_phi_s3 = 0.0;	/* sin( phi ) * SQRT3; */
		} else {
			FAST fastf_t	f;
			a *= -THIRD;
			fact = sqrt( a );
			if( (f = b * (-0.5) / (a*fact)) >= 1.0 )  {
				phi = 0.0;
				cs_phi = 1.0;		/* cos( phi ); */
				sn_phi_s3 = 0.0;	/* sin( phi ) * SQRT3; */
			}  else if( f <= -1.0 )  {
				phi = PI_DIV_3;
				cs_phi = cos( phi );
				sn_phi_s3 = sin( phi ) * SQRT3;
			}  else  {
				phi = acos( f ) * THIRD;
				cs_phi = cos( phi );
				sn_phi_s3 = sin( phi ) * SQRT3;
			}
		}

		roots[0].re = 2.0*fact*cs_phi;
		roots[1].re = fact*(  sn_phi_s3 - cs_phi);
		roots[2].re = fact*( -sn_phi_s3 - cs_phi);
		roots[2].im = roots[1].im = roots[0].im = 0.0;
	}
	for ( i=0; i < 3; ++i )
		roots[i].re -= c1_3rd;

	if( !bu_is_parallel() )
		bn_expecting_fpe = 0;

	return(1);		/* OK */
}


/**
 *			B N _ P O L Y _ Q U A R T I C _ R O O T S
 *@brief
 *	Uses the quartic formula to find the roots ( in `complex' form )
 *	of any quartic equation with real coefficients.
 *
 *	@return 1 for success
 *	@return 0 for fail.
 */
int
bn_poly_quartic_roots(register struct bn_complex *roots, register const struct bn_poly *eqn)
{
	LOCAL struct bn_poly	cube, quad1, quad2;
	LOCAL bn_complex_t	u[3];
	LOCAL fastf_t		U, p, q, q1, q2;
#define Max3(a,b,c) ((c)>((a)>(b)?(a):(b)) ? (c) : ((a)>(b)?(a):(b)))

	cube.dgr = 3;
	cube.cf[0] = 1.0;
	cube.cf[1] = -eqn->cf[2];
	cube.cf[2] = eqn->cf[3]*eqn->cf[1]
			- 4*eqn->cf[4];
	cube.cf[3] = -eqn->cf[3]*eqn->cf[3]
			- eqn->cf[4]*eqn->cf[1]*eqn->cf[1]
			+ 4*eqn->cf[4]*eqn->cf[2];

	if( !bn_poly_cubic_roots( u, &cube ) )  {
		return( 0 );		/* FAIL */
	}
	if ( u[1].im != 0.0 ){
		U = u[0].re;
	} else {
		U = Max3( u[0].re, u[1].re, u[2].re );
	}

	p = eqn->cf[1]*eqn->cf[1]*0.25 + U - eqn->cf[2];
	U *= 0.5;
	q = U*U - eqn->cf[4];
	if( p < 0 )  {
		if( p < -1.0e-8 )  {
			return(0);	/* FAIL */
		}
		p = 0;
	} else {
		p = sqrt( p );
	}
	if( q < 0 )  {
		if( q < -1.0e-8 )  {
			return(0);	/* FAIL */
		}
		q = 0;
	} else {
		q = sqrt( q );
	}

	quad1.dgr = quad2.dgr = 2;
	quad1.cf[0] = quad2.cf[0] = 1.0;
	quad1.cf[1] = eqn->cf[1]*0.5;
	quad2.cf[1] = quad1.cf[1] + p;
	quad1.cf[1] -= p;

	q1 = U - q;
	q2 = U + q;

	p = quad1.cf[1]*q2 + quad2.cf[1]*q1 - eqn->cf[3];
	if( Abs( p ) < 1.0e-8){
		quad1.cf[2] = q1;
		quad2.cf[2] = q2;
	} else {
		q = quad1.cf[1]*q1 + quad2.cf[1]*q2 - eqn->cf[3];
		if( Abs( q ) < 1.0e-8 ){
			quad1.cf[2] = q2;
			quad2.cf[2] = q1;
		} else {
			return(0);	/* FAIL */
		}
	}

	bn_poly_quadratic_roots( &roots[0], &quad1 );
	bn_poly_quadratic_roots( &roots[2], &quad2 );
	return(1);		/* SUCCESS */
}


/**
 *			B N _ P R _ P O L Y
 */
void
bn_pr_poly(const char *title, register const struct bn_poly *eqn)
{
	register int	n;
	register int	exp;
	struct bu_vls	str;
	char		buf[48];

	bu_vls_init( &str );
	bu_vls_extend( &str, 196 );
	bu_vls_strcat( &str, title );
	sprintf(buf, " polynomial, degree = %d\n", eqn->dgr);
	bu_vls_strcat( &str, buf );

	exp = eqn->dgr;
	for ( n=0; n<=eqn->dgr; n++,exp-- )  {
		register double coeff = eqn->cf[n];
		if( n > 0 )  {
			if( coeff < 0 )  {
				bu_vls_strcat( &str, " - " );
				coeff = -coeff;
			}  else  {
				bu_vls_strcat( &str, " + " );
			}
		}
		bu_vls_printf( &str, "%g", coeff );
		if( exp > 1 )  {
			bu_vls_printf( &str, " *X^%d", exp );
		} else if( exp == 1 )  {

			bu_vls_strcat( &str, " *X" );
		} else {
			/* For constant term, add nothing */
		}
	}
	bu_vls_strcat( &str, "\n" );
	bu_log( "%s", bu_vls_addr(&str) );
	bu_vls_free( &str );
}

/*
 *			B N _ P R _ R O O T S
 */
void
bn_pr_roots(const char *title, const struct bn_complex *roots, int n)
{
	register int	i;

	bu_log("%s: %d roots:\n", title, n );
	for( i=0; i<n; i++ )  {
		bu_log("%4d %e + i * %e\n", i, roots[i].re, roots[i].im );
	}
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
