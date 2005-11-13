/*                          F F T C . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file fftc.c
 *	Split Radix Decimation in Time
 *	FFT C code generator.
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



extern int rfft_adds, rfft_mults;

#include <stdio.h>
#include <math.h>

#include "./fft.h"

static char usage[] = "\
Usage: fftc length > fftlength.c\n";

int
main(int argc, char **argv)
{
	double	x[4097];
	int	n, m;

	if( argc != 2 ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	n = atoi(argv[1]);
	m = log((double)n)/log(2.0) + 0.5;	/* careful truncation */

	splitdit( x, n, m );
(void)fprintf( stderr, "adds = %d, mults = %d\n", rfft_adds, rfft_mults );
	return(0);
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
