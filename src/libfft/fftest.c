/*                        F F T E S T . C
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
/** @file fftest.c
 * Complex Number and FFT Library
 *
 *  12 Oct 84
 */
#include "common.h"

#include <stdio.h>
#include <math.h>

#include "fft.h"


typedef struct {
	double	re;	/* Real Part */
	double	im;	/* Imaginary Part */
} COMPLEX;

/***** TEST ROUTINES *****/
COMPLEX data[64];

main(void)
{
	int	i;

	for (i = 0; i < 64; i++) {
		data[i].re = sin((double)2.0*3.1415926535*i/64.0);
		data[i].re += 3*cos((double)2.0*3.1415926535*i/32.0);
		data[i].im = (double)0.0;
	}

	printf("Original Data:\n\n");
	display(data, 64);

	cfft(data, 64);
	printf("\n\nTransformed Data:\n\n");
	display(data, 64);

	icfft(data, 64);
	printf("\n\nInversed Data:\n\n");
	display(data, 64);
}

display(COMPLEX *dat, int num)
{
	int	i;

	for (i = 0; i < num; i++) {
		printf("%3d : ", i);
		printf("%f, %f\n", dat[i].re, dat[i].im);
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
