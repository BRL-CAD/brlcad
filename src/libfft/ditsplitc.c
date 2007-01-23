/*                     D I T S P L I T C . C
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
/** @file ditsplitc.c
 *
 *  Split Radix, Decimation in Frequency, Inverse Real-valued FFT.
 *
 *  Input order:
 *	[ Re(0),Re(1),...,Re(N/2),Im(N/2-1),...,Im(1) ]
 *
 *  Transactions on Acoustics, Speech, and Signal Processing, June 1987.
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
#include "./complex.h"
/*#define	TWOPI	6.283185307179586*/
#define	INVSQ2	0.70710678118654752440
#define	SQRT2	1.4142136

int	irfft_adds, irfft_mults;

void
ditsplit(double *x, int n, int m)

   	  	/* length */
   	  	/* n = 2^m */
{
	int	i, j, k, n1, n2, n4, n8;
	int	i0, i1, i2, i3, i4, i5, i6, i7, i8;
	int	is, id;
	double	cc1, ss1, cc3, ss3, e, a, a3;
irfft_adds = irfft_mults = 0;

printf("/* Machine Generated Real Split Radix Decimation in Freq Inverse FFT */\n" );
printf("#define	INVSQ2	0.70710678118654752440\n" );
printf("void\n" );
printf("irfft%d(x)\n", n );
printf("register double x[];\n\
{\n\
	register double	t1, t2, t3, t4, t5;\
	register int	i;\
\n" );

	/* L shaped butterflies */
printf("/* L shaped butterflies */\n");
	n2 = n << 1;
	for( k = 1; k < m; k++ ) {
		is = 0;
		id = n2;
		n2 = n2 >> 1;
		n4 = n2 >> 2;
		n8 = n4 >> 1;
		e = TWOPI / n2;
l17:
		for( i = is; i < n; i += id ) {
			i1 = i + 1;
			i2 = i1 + n4;
			i3 = i2 + n4;
			i4 = i3 + n4;

printf( "t1 = x[%d] - x[%d];\n", i1-1, i3-1 );
printf( "x[%d] += x[%d];\n", i1-1, i3-1 );
printf( "x[%d] *= 2.0;\n", i2-1 );
printf( "x[%d] = t1 - 2.0 * x[%d];\n", i3-1, i4-1 );
printf( "x[%d] = t1 + 2.0 * x[%d];\n", i4-1, i4-1 );
irfft_adds += 4; irfft_mults += 3;

			if( n4 == 1 )
				continue;
			i1 += n8;
			i2 += n8;
			i3 += n8;
			i4 += n8;

printf( "t1 = (x[%d] - x[%d]) * INVSQ2;\n", i2-1, i1-1 );
printf( "t2 = (x[%d] + x[%d]) * INVSQ2;\n", i4-1, i3-1 );
printf( "x[%d] += x[%d];\n", i1-1, i2-1 );
printf( "x[%d] = x[%d] - x[%d];\n", i2-1, i4-1, i3-1 );
printf( "x[%d] = -2.0 * ( t2 + t1 );\n", i3-1 );
printf( "x[%d] = 2.0 * ( -t2 + t1 );\n", i4-1 );
irfft_adds += 6; irfft_mults += 4;

		}
		is = 2 * id - n2;
		id = 4 * id;
		if( is < n-1 )
			goto l17;

		a = e;
		for( j = 2; j <= n8; j++ ) {
			a3 = 3.0 * a;
			cc1 = cos(a);
			ss1 = sin(a);
			cc3 = cos(a3);
			ss3 = sin(a3);
			a = j * e;
			is = 0;
			id = 2 * n2;
l40:
			for( i = is; i < n; i += id ) {
				i1 = i + j;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;
				i5 = i + n4 - j + 2;
				i6 = i5 + n4;
				i7 = i6 + n4;
				i8 = i7 + n4;

printf( "t1 = x[%d] - x[%d];\n", i1-1, i6-1 );
printf( "x[%d] += x[%d];\n", i1-1, i6-1 );
printf( "t2 = x[%d] - x[%d];\n", i5-1, i2-1 );
printf( "x[%d] += x[%d];\n", i5-1, i2-1 );
printf( "t3 = x[%d] + x[%d];\n", i8-1, i3-1 );
printf( "x[%d] = x[%d] - x[%d];\n", i6-1, i8-1, i3-1 );
printf( "t4 = x[%d] + x[%d];\n", i4-1, i7-1 );
printf( "x[%d] = x[%d] - x[%d];\n", i2-1, i4-1, i7-1 );
printf( "t5 = t1 - t4;\n" );
printf( "t1 += t4;\n" );
printf( "t4 = t2 - t3;\n" );
printf( "t2 += t3;\n" );
printf( "x[%d] = t5 * %.24f + t4 * %.24f;\n", i3-1, cc1, ss1 );
printf( "x[%d] = - t4 * %.24f + t5 * %.24f;\n", i7-1, cc1, ss1 );
printf( "x[%d] = t1 * %.24f - t2 * %.24f;\n", i4-1, cc3, ss3 );
printf( "x[%d] = t2 * %.24f + t1 * %.24f;\n", i8-1, cc3, ss3 );
irfft_adds += 16; irfft_mults += 8;

			}
			is = 2 * id - n2;
			id = 4 * id;
			if( is < n-1)
				goto l40;
		}
	}

	/* Length two butterflies */
printf("/* Length two butterflies */\n");
	is = 1;
	id = 4;
l70:
	for( i0 = is; i0 <= n; i0 += id ) {
		i1 = i0 + 1;

printf( "t1 = x[%d];\n", i0-1 );
printf( "x[%d] = t1 + x[%d];\n", i0-1, i1-1 );
printf( "x[%d] = t1 - x[%d];\n", i1-1, i1-1 );
irfft_adds += 2;

	}
	is = 2 * id - 1;
	id = 4 * id;
	if( is < n )
		goto l70;

	/* Digit reverse counter */
printf("/* bit reverse */\n");
	j = 1;
	n1 = n - 1;
	for( i = 1; i <= n1; i++ ) {
		if( i < j ) {
printf( "t1 = x[%d];\n", j-1 );
printf( "x[%d] = x[%d];\n", j-1, i-1 );
printf( "x[%d] = t1;\n", i-1 );
		}
		k = n/2;
		while( k < j ) {
			j -= k;
			k /= 2;
		}
		j += k;
	}

printf("/* scale result */\n");
printf("for( i = 0; i < %d; i++ )\n", n);
printf("	x[i] /= %f;\n", (double)n );

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
