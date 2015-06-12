/*                           M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file comgeom/mat.c
 *
 * 4 x 4 Matrix manipulation functions
 *
 * mat_zero(&m)			Fill matrix m with zeros
 * mat_idn(&m)			Fill matrix m with identity matrix
 * mat_copy(&o, &i)		Copy matrix i to matrix o
 * mat_mul(&o, &i1, &i2)		Multiply i1 by i2 and store in o
 * vecXmat(&ov, &iv, &m)		Multiply vector iv by m and store in ov
 * matXvec(&ov, &m, &iv)		Multiply m by vector iv and store in ov
 * mat_print(&m)			Print out the 4x4 matrix - calls printf
 * mat_hscale(&m, hscale)	        Homogeneous scale of input matrix
 * mat_inv(&om, &im)		Invert matrix im and store result in om
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
#include <math.h>

#include "vmath.h"


/*
 * Fill in the matrix "m" with zeros.
 */
void
mat_zero(matp_t m)
{
    int i = 0;

    /* Clear everything */
    for (; i < 16; i++)
	*m++ = 0;
}


/*
 * Fill in the matrix "m" with an identity matrix.
 */
void
mat_idn(matp_t m)
{
    /* Clear everything first */
    mat_zero(m);

    /* Set ones in the diagonal */
    m[0] = m[5] = m[10] = m[15] = 1;
}


/*
 * Copy the matrix "im" into the matrix "om".
 */
void
mat_copy(matp_t om, matp_t im)
{
    int i = 0;

    /* Copy all elements */
    for (; i < 16; i++)
	*om++ = *im++;
}


/*
 * Multiply matrix "im1" by "im2" and store the result in "om".
 * NOTE:  This is different from multiplying "im2" by "im1" (most
 * of the time!)
 */
void
mat_mul(matp_t om, matp_t im1, matp_t im2)
{
    int em1;		/* Element subscript for im1 */
    int em2;		/* Element subscript for im2 */
    int el = 0;		/* Element subscript for om */
    int i;			/* For counting */

    /* For each element in the output matrix... */
    for (; el < 16; el++) {

	om[el] = 0;		/* Start with zero in output */
	em1 = (el/4)*4;		/* Element at right of row in im1 */
	em2 = el%4;		/* Element at top of column in im2 */

	for (i = 0; i < 4; i++) {
	    om[el] += im1[em1] * im2[em2];

	    em1++;		/* Next row element in m1 */
	    em2 += 4;	/* Next column element in m2 */
	}
    }
}


/*
 * Multiply the vector "iv" by the matrix "im" and store the result
 * in the vector "ov".  Note this is pre-multiply.
 */
void
vecXmat(vectp_t ov, vectp_t iv, matp_t im)
{
    int el = 0;		/* Position in output vector */
    int ev;		/* Position in input vector */
    int em;		/* Position in input matrix */

    /* For each element in the output array... */
    for (; el < 4; el++) {

	ov[el] = 0;		/* Start with zero in output */
	em = el;		/* Top of column in input matrix */

	for (ev = 0; ev < 4; ev++) {
	    ov[el] += iv[ev] * im[em];
	    em += 4;	/* Next element in column from im */
	}
    }
}


/*
 * Multiply the matrix "im" by the vector "iv" and store the result
 * in the vector "ov".  Note this is post-multiply.
 */
void
matXvec(vectp_t ov, matp_t im, vectp_t iv)
{
    int eo = 0;		/* Position in output vector */
    int em = 0;		/* Position in input matrix */
    int ei;		/* Position in input vector */

    /* For each element in the output array... */
    for (; eo < 4; eo++) {

	ov[eo] = 0;		/* Start with zero in output */

	for (ei = 0; ei < 4; ei++)
	    ov[eo] += im[em++] * iv[ei];
    }
}


/*
 * Print out the 4x4 matrix addressed by "m".
 */
void
mat_print(matp_t m)
{
    int i;

    for (i = 0; i < 16; i++) {
	printf("%f%c", m[i], ((i+1)%4) ? '\t' : '\n');
    }
}


/*
 * The matrix pointed at by "m" is homogeneously scaled by the
 * variable "hscale".  NOTE that the input matrix is ALSO the output
 * matrix.
 */
void
mat_hscale(matp_t m, float hscale)
{
    m[0]  *= hscale;
    m[5]  *= hscale;
    m[10] *= hscale;
}


/*
 * The matrix pointed at by "im" is inverted and stored in the area
 * pointed at by "om".
 */
#define EPSILON 0.000001

/*
 * Invert a 4-by-4 matrix using Algorithm 120 from ACM.
 * This is a modified Gauss-Jordan algorithm
 * Note:  Inversion is done in place, with 3 work vectors
 */
void
mat_inv(matp_t output, matp_t input)
{
    int i, j;			/* Indices */
    static int k;				/* Indices */
    static int z[4];			/* Temporary */
    static float b[4];			/* Temporary */
    static float c[4];			/* Temporary */

    mat_copy(output, input);	/* Duplicate */

    /* Initialization */
    for (j = 0; j < 4; j++)
	z[j] = j;

    /* Main Loop */
    for (i = 0; i < 4; i++) {
	static float y;				/* local temporary */

	k = i;
	y = output[i*4+i];
	for (j = i + 1; j < 4; j++) {
	    static float w;			/* local temporary */

	    w = output[i*4+j];
	    if (fabs(w) > fabs(y)) {
		k = j;
		y = w;
	    }
	}

	if (fabs(y) < EPSILON) {
	    printf("mat_inv:  error!\n");
	    return;
	}
	y = 1.0 / y;

	for (j = 0; j < 4; j++) {
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
	for (k = 0; k < 4; k++) {
	    if (k == i)  continue;
	    for (j = 0; j < 4; j++) {
		if (j == i)  continue;
		output[k*4+j] = output[k*4+j] - b[j] * c[k];
	    }
	}
    }

    /* Second Loop */
    for (i = 0; i < 4; i++) {
	while ((k = z[i]) != i) {
	    static int p;			/* Local temp */

	    for (j = 0; j < 4; j++) {
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
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
