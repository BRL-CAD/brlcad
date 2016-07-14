/*                      D I T S P L I T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @file libfft/ditsplit.c
 *
 * Split Radix, Decimation in Frequency, Inverse Real-valued FFT.
 *
 * Input order:
 *	[ Re(0), Re(1), ..., Re(N/2), Im(N/2-1), ..., Im(1) ]
 *
 * Transactions on Acoustics, Speech, and Signal Processing, June 1987.
 *
 */

#include "common.h"

#include <stdio.h>

#include "fft.h"


void
irfft(double *x, int n /* length */)
{
    int i, j, k, n1, n2, n4, n8;
    int i0, i1, i2, i3, i4, i5, i6, i7, i8;
    int is, id;
    double t1, t2, t3, t4, t5;
    double cc1, ss1, cc3, ss3, e, a, a3;
    int irfft_adds, irfft_mults;

    irfft_adds = irfft_mults = 0;

    /* L shaped butterflies */
    n2 = n << 1;
    for (k = 2; k < n; k <<= 1) {
	is = 0;
	id = n2;
	n2 = n2 >> 1;
	n4 = n2 >> 2;
	n8 = n4 >> 1;
	e = 2.0*M_PI / n2;
    l17:
	for (i = is; i < n; i += id) {
	    i1 = i + 1;
	    i2 = i1 + n4;
	    i3 = i2 + n4;
	    i4 = i3 + n4;

	    t1 = x[i1-1] - x[i3-1];
	    x[i1-1] += x[i3-1];
	    x[i2-1] *= 2.0;
	    x[i3-1] = t1 - 2.0 * x[i4-1];
	    x[i4-1] = t1 + 2.0 * x[i4-1];
	    irfft_adds += 4; irfft_mults += 3;

	    if (n4 == 1)
		continue;
	    i1 += n8;
	    i2 += n8;
	    i3 += n8;
	    i4 += n8;

	    t1 = (x[i2-1] - x[i1-1]) * M_SQRT1_2;
	    t2 = (x[i4-1] + x[i3-1]) * M_SQRT1_2;
	    x[i1-1] += x[i2-1];
	    x[i2-1] = x[i4-1] - x[i3-1];
	    x[i3-1] = -2.0 * (t2 + t1);
	    x[i4-1] = 2.0 * (-t2 + t1);
	    irfft_adds += 6; irfft_mults += 4;

	}
	is = 2 * id - n2;
	id = 4 * id;
	if (is < n-1)
	    goto l17;

	a = e;
	for (j = 2; j <= n8; j++) {
	    a3 = 3.0 * a;
	    cc1 = cos(a);
	    ss1 = sin(a);
	    cc3 = cos(a3);
	    ss3 = sin(a3);
	    a = j * e;
	    is = 0;
	    id = 2 * n2;
	l40:
	    for (i = is; i < n; i += id) {
		i1 = i + j;
		i2 = i1 + n4;
		i3 = i2 + n4;
		i4 = i3 + n4;
		i5 = i + n4 - j + 2;
		i6 = i5 + n4;
		i7 = i6 + n4;
		i8 = i7 + n4;

		t1 = x[i1-1] - x[i6-1];
		x[i1-1] += x[i6-1];
		t2 = x[i5-1] - x[i2-1];
		x[i5-1] += x[i2-1];
		t3 = x[i8-1] + x[i3-1];
		x[i6-1] = x[i8-1] - x[i3-1];
		t4 = x[i4-1] + x[i7-1];
		x[i2-1] = x[i4-1] - x[i7-1];
		t5 = t1 - t4;
		t1 += t4;
		t4 = t2 - t3;
		t2 += t3;
		x[i3-1] = t5 * cc1 + t4 * ss1;
		x[i7-1] = - t4 * cc1 + t5 * ss1;
		x[i4-1] = t1 * cc3 - t2 * ss3;
		x[i8-1] = t2 * cc3 + t1 * ss3;
		irfft_adds += 16; irfft_mults += 8;

	    }
	    is = 2 * id - n2;
	    id = 4 * id;
	    if (is < n-1)
		goto l40;
	}
    }

    /* Length two butterflies */
    is = 1;
    id = 4;
l70:
    for (i0 = is; i0 <= n; i0 += id) {
	i1 = i0 + 1;

	t1 = x[i0-1];
	x[i0-1] = t1 + x[i1-1];
	x[i1-1] = t1 - x[i1-1];
	irfft_adds += 2;

    }
    is = 2 * id - 1;
    id = 4 * id;
    if (is < n)
	goto l70;

    /* Digit reverse counter */
    j = 1;
    n1 = n - 1;
    for (i = 1; i <= n1; i++) {
	if (i < j) {
	    t1 = x[j-1];
	    x[j-1] = x[i-1];
	    x[i-1] = t1;
	}
	k = n/2;
	while (k < j) {
	    j -= k;
	    k /= 2;
	}
	j += k;
    }

    /* scale result */
    for (i = 0; i < n; i++)
	x[i] /= (double)n;
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
