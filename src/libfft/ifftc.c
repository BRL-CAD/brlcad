/*                         I F F T C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file libfft/ifftc.c
 *
 * Split Radix Decimation in Freq Inverse FFT C code generator.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "./fft.h"

static int
parse_fft_length(const char *arg, int *n)
{
    char *end = NULL;
    long parsed = 0;

    errno = 0;
    parsed = strtol(arg, &end, 10);
    if (arg[0] == '\0' || end == arg || *end != '\0' || errno != 0) {
	fprintf(stderr, "ifftc: invalid length '%s' (expected a positive power of two)\n", arg);
	return 0;
    }
    if (parsed <= 0) {
	fprintf(stderr, "ifftc: length must be greater than zero, got '%s'\n", arg);
	return 0;
    }
    if (parsed > INT_MAX) {
	fprintf(stderr, "ifftc: length out of range '%s'\n", arg);
	return 0;
    }
    if ((parsed & (parsed - 1)) != 0) {
	fprintf(stderr, "ifftc: length must be a power of two, got '%s'\n", arg);
	return 0;
    }

    *n = (int)parsed;
    return 1;
}

int
main(int argc, char **argv)
{
    int n, m, t;

    if ( argc != 2 ) {
	fprintf( stderr, "Usage: ifftc length > fftlength.c\n" );
	return 1;
    }

    if (!parse_fft_length(argv[1], &n)) {
	return 1;
    }

    for (m = 0, t = n; t > 1; t >>= 1)
	m++;
    ditsplit( n, m );
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
