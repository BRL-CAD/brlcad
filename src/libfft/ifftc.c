/*                         I F F T C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
/** @file ifftc.c
 *
 * Split Radix Decimation in Freq Inverse FFT C code generator.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "./fft.h"

extern int irfft_adds, irfft_mults;

int
main(int argc, char **argv)
{
    int n, m;

    if ( argc != 2 ) {
	fprintf( stderr, "Usage: ifftc length > fftlength.c\n" );
	return 1;
    }

    n = atoi(argv[1]);
    m = log((double)n)/log(2.0) + 0.5;	/* careful truncation */

    ditsplit( n, m );
    fprintf( stderr, "adds = %d, mults = %d\n", irfft_adds, irfft_mults );
    return 0;
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
