/*                      B W H I S T E Q . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file bwhisteq.c
 *
 *  Build up the histgram of a picture and output the "equalized"
 *  version of it on stdout.
 *
 *  Author -
 *	Phillip Dykstra
 *	2 Sept 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"

long bin[256];
unsigned char new[256];

#define	rand01()	((random()&0xffff)/65535.0)

FILE *fp;

char usage[] = "Usage: bwhisteq [-v] file.bw > file.equalized\n";

int	verbose = 0;

int
main(int argc, char **argv)
{
	int	n, i;
	unsigned char buf[512];
	unsigned char obuf[512];
	register unsigned char *bp;
	int	left[256], right[256];
	double	hint, havg;
	long	r;

	if( argc > 1 && strcmp( argv[1], "-v" ) == 0 ) {
		verbose++;
		argc--;
		argv++;
	}

	if( argc != 2 || isatty(fileno(stdout)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	if( (fp = fopen( argv[1], "r" )) == NULL ) {
		fprintf( stderr, "bwhisteq: Can't open \"%s\"\n", argv[1] );
		exit( 2 );
	}

	/* Tally up the intensities */
	while( (n = fread(buf, sizeof(*buf), 512, fp)) > 0 ) {
		bp = &buf[0];
		for( i = 0; i < n; i++ )
			bin[ *bp++ ]++;
	}

	havg = 0.0;
	for( i = 0; i < 256; i++ )
		havg += bin[ i ];
	havg /= 256.0;

	r = 0;
	hint = 0;
	for( i = 0; i < 256; i++ ) {
		left[i] = r;
		hint += bin[i];
		while( hint > havg ) {
			hint -= havg;
			r++;
		}
		right[i] = r;
#ifdef METHOD2
		new[i] = right[i] - left[i];
#else
		new[i] = (left[i] + right[i]) / 2;
#endif /* Not METHOD2 */
	}

	if( verbose ) {
		for( i = 0; i < 256; i++ )
			fprintf( stderr, "new[%d] = %d\n", i, new[i] );
	}

	fseek( fp, 0, 0 );
	while( (n = fread( buf, 1, 512, fp )) > 0 ) {
		for( i = 0; i < n; i++ ) {
			if( left[buf[i]] == right[buf[i]] )
				obuf[i] = left[buf[i]];
			else {
#ifdef METHOD2
				obuf[i] = left[buf[i]] + new[buf[i]] * rand01();
#else
				obuf[i] = new[buf[i]];
#endif /* Not METHOD2 */
			}
		}
		fwrite( obuf, 1, n, stdout );
	}
	return 0;
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
