/*                           M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file mat.c
 *
 * 4 x 4 Matrix manipulation functions..............
 *
 *	mat_zero( &m )			Fill matrix m with zeros
 *	mat_idn( &m )			Fill matrix m with identity matrix
 *	mat_copy( &o, &i )		Copy matrix i to matrix o
 *	mat_mul( &o, &i1, &i2 )		Multiply i1 by i2 and store in o
 *	vecXmat( &ov, &iv, &m )		Multiply vector iv by m and store in ov
 *	matXvec( &ov, &m, &iv )		Multiply m by vector iv and store in ov
 *	mat_print( &m )			Print out the 4x4 matrix - calls printf
 *	mat_hscale( &m, hscale )	Homogenious scale of input matrix
 *	mat_inv( &om, &im )		Invert matrix im and store result in om
 *
 *
 * Matrix array elements have the following positions in the matrix:
 *
 *				|  0  1  2  3 |		| 0 |
 *	  [ 0 1 2 3 ]		|  4  5  6  7 |		| 1 |
 *				|  8  9 10 11 |		| 2 |
 *				| 12 13 14 15 |		| 3 |
 *
 *     preVector (vect_t)	 Matrix (mat_t)    postVector (vect_t)
 *
 */

#include "common.h"

#include <stdio.h>
#ifdef HAVE_FABS
# ifdef HAVE_MATH_H
#  include <math.h>
# endif
#else
# define fabs(x) ((x)>0?(x):-(x))
#endif

#include "./ged_types.h"


/*
 *			M A T _ Z E R O
 *
 * Fill in the matrix "m" with zeros.
 */
mat_zero(register matp_t m)
{
	register int i = 0;

	/* Clear everything */
	for(; i<16; i++)
		*m++ = 0;
}


/*
 *			M A T _ I D N
 *
 * Fill in the matrix "m" with an identity matrix.
 */
mat_idn(register matp_t m)
{
	/* Clear everything first */
	mat_zero( m );

	/* Set ones in the diagonal */
	m[0] = m[5] = m[10] = m[15] = 1;
}


/*
 *			M A T _ C O P Y
 *
 * Copy the matrix "im" into the matrix "om".
 */
mat_copy(register matp_t om, register matp_t im)
{
	register int i = 0;

	/* Copy all elements */
	for(; i<16; i++)
		*om++ = *im++;
}


/*
 *			M A T _ M U L
 *
 * Multiply matrix "im1" by "im2" and store the result in "om".
 * NOTE:  This is different from multiplying "im2" by "im1" (most
 * of the time!)
 */
mat_mul(register matp_t om, register matp_t im1, register matp_t im2)
{
	register int em1;		/* Element subscript for im1 */
	register int em2;		/* Element subscript for im2 */
	register int el = 0;		/* Element subscript for om */
	register int i;			/* For counting */

	/* For each element in the output matrix... */
	for(; el<16; el++) {

		om[el] = 0;		/* Start with zero in output */
		em1 = (el/4)*4;		/* Element at right of row in im1 */
		em2 = el%4;		/* Element at top of column in im2 */

		for(i=0; i<4; i++) {
			om[el] += im1[em1] * im2[em2];

			em1++;		/* Next row element in m1 */
			em2 += 4;	/* Next column element in m2 */
		}
	}
}


/*
 *			V E C X M A T
 *
 * Multiply the vector "iv" by the matrix "im" and store the result
 * in the vector "ov".  Note this is pre-multiply.
 */
vecXmat(register vectp_t ov, register vectp_t iv, register matp_t im)
{
	register int el = 0;		/* Position in output vector */
	register int ev;		/* Position in input vector */
	register int em;		/* Position in input matrix */

	/* For each element in the output array... */
	for(; el<4; el++) {

		ov[el] = 0;		/* Start with zero in output */
		em = el;		/* Top of column in input matrix */

		for(ev=0; ev<4; ev++) {
			ov[el] += iv[ev] * im[em];
			em += 4;	/* Next element in column from im */
		}
	}
}


/*
 *			M A T X V E C
 *
 * Multiply the matrix "im" by the vector "iv" and store the result
 * in the vector "ov".  Note this is post-multiply.
 */
matXvec(register vectp_t ov, register matp_t im, register vectp_t iv)
{
	register int eo = 0;		/* Position in output vector */
	register int em = 0;		/* Position in input matrix */
	register int ei;		/* Position in input vector */

	/* For each element in the output array... */
	for(; eo<4; eo++) {

		ov[eo] = 0;		/* Start with zero in output */

		for(ei=0; ei<4; ei++)
			ov[eo] += im[em++] * iv[ei];
	}
}


/*
 *			M A T _ P R I N T
 *
 * Print out the 4x4 matrix addressed by "m".
 */
mat_print(register matp_t m)
{
	register int i;

	for(i=0; i<16; i++) {
		printf("%f%c", m[i], ((i+1)%4) ? '\t' : '\n');
	}
}


/*
 *			M A T _ H S C A L E
 *
 * The matrix pointed at by "m" is homogeniously scaled by the
 * variable "hscale".  NOTE that the input matrix is ALSO the output
 * matrix.
 */
mat_hscale(register matp_t m, float hscale)
{
	m[0] *= hscale;
	m[5] *= hscale;
	m[10] *= hscale;
}


/*
 *			M A T _ I N V
 *
 * The matrix pointed at by "im" is inverted and stored in the area
 * pointed at by "om".
 */
#define EPSILON	0.000001

/*
 * Invert a 4-by-4 matrix using Algorithm 120 from ACM.
 * This is a modified Gauss-Jordan alogorithm
 * Note:  Inversion is done in place, with 3 work vectors
 */
void
mat_inv(register matp_t output, matp_t input)
{
	register int i, j;			/* Indices */
	static int k;				/* Indices */
	static int	z[4];			/* Temporary */
	static float	b[4];			/* Temporary */
	static float	c[4];			/* Temporary */

	mat_copy( output, input );	/* Duplicate */

	/* Initialization */
	for( j = 0; j < 4; j++ )
		z[j] = j;

	/* Main Loop */
	for( i = 0; i < 4; i++ )  {
		static float y;				/* local temporary */

		k = i;
		y = output[i*4+i];
		for( j = i+1; j < 4; j++ )  {
			static float w;			/* local temporary */

			w = output[i*4+j];
			if( fabs(w) > fabs(y) )  {
				k = j;
				y = w;
			}
		}

		if( fabs(y) < EPSILON )  {
			printf("mat_inv:  error!\n");
			return;
		}
		y = 1.0 / y;

		for( j = 0; j < 4; j++ )  {
			static float temp;		/* Local */

			c[j] = output[j*4+k];
			output[j*4+k] = output[j*4+i];
			output[j*4+i] = - c[j] * y;
			temp = output[i*4+j] * y;
			b[j] = temp;
			output[i*4+j] = temp;
		}

		output[i*4+i] = y;
		j = z[i];
		z[i] = z[k];
		z[k] = j;
		for( k = 0; k < 4; k++ )  {
			if( k == i )  continue;
			for( j = 0; j < 4; j++ )  {
				if( j == i )  continue;
				output[k*4+j] = output[k*4+j] - b[j] * c[k];
			}
		}
	}

	/*  Second Loop */
	for( i = 0; i < 4; i++ )  {
		while( (k = z[i]) != i )  {
			static int p;			/* Local temp */

			for( j = 0; j < 4; j++ )  {
				static float w;		/* Local temp */

				w = output[i*4+j];
				output[i*4+j] = output[k*4+j];
				output[k*4+j] = w;
			}
			p = z[i];
			z[i] = z[k];
			z[k] = p;
		}
	}
	return;
}


/*
 *			V T O H _ M O V E
 *
 * Takes a pointer to a [x,y,z] vector, and a pointer
 * to space for a homogeneous vector [x,y,z,w],
 * and builds [x,y,z,1].
 */
vtoh_move(register float *h, register float *v)
{
	*h++ = *v++;
	*h++ = *v++;
	*h++ = *v;
	*h++ = 1;
}

/*
 *			H T O V _ M O V E
 *
 * Takes a pointer to [x,y,z,w], and converts it to
 * an ordinary vector [x/w, y/w, z/w].
 * Optimization for the case of w==1 is performed.
 */
htov_move(register float *v, register float *h)
{
	static float inv;

	if( h[3] == 1 )  {
		*v++ = *h++;
		*v++ = *h++;
		*v   = *h;
	}  else  {
		inv = 1 / h[3];

		*v++ = *h++ * inv;
		*v++ = *h++ * inv;
		*v   = *h   * inv;
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
