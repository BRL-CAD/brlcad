/*                        B W D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
 */
/** @file bwdiff.c
 *
 * Take the difference between two BW files.
 * Output is: (file1-file2)/2 + 127
 * or magnitude (-m): abs(file1-file2)
 *
 *  Author -
 *	Phillip Dykstra
 *	26 June 1986
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
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"

FILE *fp1, *fp2;

#define	DIFF	0
#define	MAG	1
#define	GREATER	2
#define	LESS	3
#define	EQUAL	4
#define	NEQ	5

int	mode = DIFF;
int	backgnd = 0;
unsigned char ibuf1[512], ibuf2[512], obuf[512];

void	open_file(FILE **fp, char *name);

char usage[] = "\
Usage: bwdiff [-b -m -g -l -e -n] file1.bw file2.bw (- stdin, . skip)\n";

int
main(int argc, char **argv)
{
	register unsigned char *p1, *p2, *op;
	int	i, n, m;

	while( argc > 3 ) {
		if( strcmp(argv[1],"-m") == 0 ) {
			mode = MAG;
		} else if( strcmp(argv[1],"-g") == 0 ) {
			mode = GREATER;
		} else if( strcmp(argv[1],"-l") == 0 ) {
			mode = LESS;
		} else if( strcmp(argv[1],"-e") == 0 ) {
			mode = EQUAL;
		} else if( strcmp(argv[1],"-n") == 0 ) {
			mode = NEQ;
		} else if( strcmp(argv[1],"-b") == 0 ) {
			backgnd++;
		} else
			break;
		argv++;
		argc--;
	}

	if( argc != 3 || isatty(fileno(stdout)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	open_file(&fp1, argv[1]);
	open_file(&fp2, argv[2]);

	while(1) {
		n = fread(ibuf1,1,512,fp1);
		m = fread(ibuf2,1,512,fp2);
		if( (n == 0) && (m == 0))
			break;
		p1 = &ibuf1[0];
		p2 = &ibuf2[0];
		op = &obuf[0];
		if( m < n ) {
			 bzero( (char *)(&ibuf2[m]), (n - m));
		}
		if( m > n ) {
			 bzero( (char *)(&ibuf1[n]), (m - n));
			 n = m;
		}
		/* unrolled for speed */
		switch( mode ) {
		case DIFF:
			for( i = 0; i < n; i++ ) {
#ifdef vax
				/*
				 * *p's promoted to ints automatically,
				 * VAX then does /2 much faster! (extzv)
				 */
				*op++ = (*p1 - *p2)/2 + 128;
#else
				*op++ = (((int)*p1 - (int)*p2)>>1) + 128;
#endif
				p1++;
				p2++;
			}
			break;
		case MAG:
			for( i = 0; i < n; i++ ) {
				*op++ = abs( (int)*p1++ - (int)*p2++ );
			}
			break;
		case GREATER:
			for( i = 0; i < n; i++ ) {
				if( *p1 > *p2++ )
					*op++ = 255;
				else {
					if( backgnd )
						*op++ = *p1 >> 2;
					else
						*op++ = 0;
				}
				p1++;
			}
			break;
		case LESS:
			for( i = 0; i < n; i++ ) {
				if( *p1 < *p2++ )
					*op++ = 255;
				else {
					if( backgnd )
						*op++ = *p1 >> 2;
					else
						*op++ = 0;
				}
				p1++;
			}
			break;
		case EQUAL:
			for( i = 0; i < n; i++ ) {
				if( *p1 == *p2++ )
					*op++ = 255;
				else {
					if( backgnd )
						*op++ = *p1 >> 2;
					else
						*op++ = 0;
				}
				p1++;
			}
			break;
		case NEQ:
			for( i = 0; i < n; i++ ) {
				if( *p1 != *p2++ )
					*op++ = 255;
				else {
					if( backgnd )
						*op++ = (*p1) >> 1;
					else
						*op++ = 0;
				}
				p1++;
			}
			break;
		}
		fwrite( &obuf[0], 1, n, stdout );
	}
	return 0;
}

void
open_file(FILE **fp, char *name)
{
	/* check for special names */
	if( strcmp( name, "-" ) == 0 ) {
		*fp = stdin;
		return;
	} else if( strcmp( name, "." ) == 0 ) {
		*fp = fopen( "/dev/null", "r" );
		return;
	}

	if( (*fp = fopen( name, "r" )) == NULL ) {
		fprintf( stderr, "bwdiff: Can't open \"%s\"\n", name );
		exit( 2 );
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
