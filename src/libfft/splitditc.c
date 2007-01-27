/*                     S P L I T D I T C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file splitditc.c
 *  Real valued, split-radix, decimation in time FFT code generator.
 *
 *  Author -
 *	Phil Dykstra
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>

#include "./complex.h"	/* for TWOPI */
#define	INVSQ2	0.70710678118654752440

int	rfft_adds, rfft_mults;

void
splitdit(double *X, int N, int M)
{
	int	i0, i1, i2, i3;
	int	a0, a1, a2, a3, b0, b1, b2, b3;
	int	s, d;
	double	a, aa3, e;
	double	cc1, ss1, cc3, ss3;
	int	i, j, k, ni;
	int	n2, n4;
rfft_adds = rfft_mults = 0;

printf("/* Machine Generated Real Split Radix Decimation in Time FFT */\n" );
printf("#define	INVSQ2	0.70710678118654752440\n" );
printf("void\n" );
printf("rfft%d(X)\n", N );
printf("register double X[];\n\
{\n\
	register double	t0, t1, c3, d3, c2, d2;\n\
	register int	i;\
\n" );

	/* bit reverse counter */
printf("/* bit reverse */\n");
	j = 1;
	ni = N - 1;
	for( i = 1; i <= ni; i++ ) {
		if( i < j ) {
printf("t0 = X[%d];\n", j-1 );
printf("X[%d] = X[%d];\n", j-1, i-1 );
printf("X[%d] = t0;\n", i-1 );
		}
		k = N/2;
		while( k < j ) {
			j -= k;
			k /= 2;
		}
		j += k;
	}

	/* length two transforms */
printf("/* length two xforms */\n");
	for( s = 1, d = 4; s < N; s = 2*d-1, d *= 4 ) {
		for( i0 = s; i0 <= N; i0 += d ) {
			i1 = i0 + 1;
printf("t0 = X[%d];\n", i0-1 );
printf("X[%d] += X[%d];\n", i0-1, i1-1 );
printf("X[%d] = t0 - X[%d];\n", i1-1, i1-1 );
rfft_adds += 2;
		}
	}

	/* other butterflies */
printf("/* other butterflies */\n");
	n2 = 2;
	for( k = 2; k <= M; k++ ) {
		n2 *= 2;
		n4 = n2/4;

		/* without mult */
		for( s = 1, d = 2*n2; s < N; s = 2*d-n2+1, d *= 4 ) {
			for( i0 = s; i0 < N; i0 += d ) {
				i1 = i0 + n4;
				i2 = i1 + n4;
				i3 = i2 + n4;
printf("t0 = X[%d] + X[%d];\n", i3-1, i2-1 );
printf("X[%d] = X[%d] - X[%d];\n", i3-1, i2-1, i3-1 );
printf("X[%d] = X[%d] - t0;\n", i2-1, i0-1 );
printf("X[%d] += t0;\n", i0-1 );
rfft_adds += 4;
			}
		}
		if( n4 < 2 ) continue;
		/* with 2 real mult */
		for( s = n4/2+1, d = 2*n2; s < N; s = 2*d-n2+n4/2+1, d *= 4 ) {
			for( i0 = s; i0 < N; i0 += d ) {
				i1 = i0 + n4;
				i2 = i1 + n4;
				i3 = i2 + n4;
printf("t0 = (X[%d]-X[%d])*INVSQ2;\n", i2-1, i3-1 );
printf("t1 = (X[%d]+X[%d])*INVSQ2;\n", i2-1, i3-1 );
printf("X[%d] = t1 - X[%d];\n", i2-1, i1-1 );
printf("X[%d] = t1 + X[%d];\n", i3-1, i1-1 );
printf("X[%d] = X[%d] - t0;\n", i1-1, i0-1 );
printf("X[%d] += t0;\n", i0-1 );
rfft_mults += 2; rfft_adds += 6;
			}
		}
		e = TWOPI/n2;
		a = e;
		if( n4 < 4 ) continue;
		for( j = 2; j <= n4/2; j++ ) {
			aa3 = 3*a;
			cc1 = cos(a);
			ss1 = sin(a);
			cc3 = cos(aa3);
			ss3 = sin(aa3);
			a = j * e;
			/* with 6 real mult */
			for( s = j, d = 2*n2; s < N; s = 2*d-n2+j, d *= 4 ) {
				for( a0 = s; a0 < N; a0 += d ) {
					b1 = a0 + n4;
					a1 = b1-j-j+2;
					b0 = a1 + n4;
					a2 = b1 + n4;
					a3 = a2 + n4;
					b2 = b0 + n4;
					b3 = b2 + n4;
printf("c2 = X[%d]*%.24g - X[%d]*%.24g;\n", a2-1, cc1, b2-1, ss1 );
printf("d2 = -(X[%d]*%.24g + X[%d]*%.24g);\n", a2-1, ss1, b2-1, cc1 );
printf("c3 = X[%d]*%.24g - X[%d]*%.24g;\n", a3-1, cc3, b3-1, ss3 );
printf("d3 = -(X[%d]*%.24g + X[%d]*%.24g);\n", a3-1, ss3, b3-1, cc3 );
rfft_mults += 8; rfft_adds += 4;
printf("t0 = c2 + c3;\n" );
printf("c3 = c2 - c3;\n" );
printf("t1 = d2 - d3;\n" );
printf("d3 += d2;\n" );
printf("X[%d] = -X[%d] - d3;\n", a2-1, b0-1 );
printf("X[%d] = -X[%d] + c3;\n", b2-1, b1-1 );
printf("X[%d] = X[%d] + c3;\n", a3-1, b1-1 );
printf("X[%d] = X[%d] - d3;\n", b3-1, b0-1 );
printf("X[%d] = X[%d] + t1;\n", b1-1, a1-1 );
printf("X[%d] = X[%d] - t0;\n", b0-1, a0-1 );
printf("X[%d] += t0;\n", a0-1 );
printf("X[%d] -= t1;\n", a1-1 );
rfft_adds += 12;
				}
			}
		}
	}

/* For some reason the Imag part is comming out with the wrong
 * sign, so we reverse it here!  We need to figure this out!
 */
printf("/* reverse Imag part! */\n");
printf("for( i = %d/2+1; i < %d; i++ )\n", N, N);
printf("	X[i] = -X[i];\n");

printf("}\n");
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
