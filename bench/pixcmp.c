/*                        P I X C M P . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file pixcmp.c
 *
 *  Compute the difference between two .pix files.
 *  To establish context, a monochrome image is produced when there
 *  are no differences;  otherwise the channels that differ are
 *  highlighted on differing pixels.
 *
 *  This routine operates on a pixel-by-pixel basis, and thus
 *  is independent of the resolution of the image.
 *
 *  Author -
 *	Charles M. Kennedy
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <stdlib.h>

long	matching;
long	off1;
long	offmany;

char usage[] = "Usage: pixcmp f1.pix f2.pix\n";

int main(int argc, char **argv);


int
main(int argc, char **argv)
{
	register FILE *f1, *f2;

	if( argc != 3 )  {
		fprintf(stderr, "%s", usage);
		exit(0);
	}

	if( strcmp( argv[1], "-" ) == 0 )
		f1 = stdin;
	else if( (f1 = fopen( argv[1], "r" ) ) == NULL )  {
		perror( argv[1] );
		exit(1);
	}
	if( strcmp( argv[2], "-" ) == 0 )
		f2 = stdin;
	else if( (f2 = fopen( argv[2], "r" ) ) == NULL )  {
		perror( argv[2] );
		exit(1);
	}
	while(1)  {
		register int r1, g1, b1;
		int r2, g2, b2;

		r1 = fgetc( f1 );
		g1 = fgetc( f1 );
		b1 = fgetc( f1 );
		r2 = fgetc( f2 );
		g2 = fgetc( f2 );
		b2 = fgetc( f2 );
		if( feof(f1) || feof(f2) )  break;

		if( r1 != r2 || g1 != g2 || b1 != b2 )  {
			register int i;

			/* Highlight differing channels */
			if( r1 != r2 )  {
				if( (i = r1 - r2) < 0 )  i = -i;
				if( i > 1 )  {
					offmany++;
				} else {
					off1++;
				}
			} else {
				matching++;
			}
			if( g1 != g2 )  {
				if( (i = g1 - g2) < 0 )  i = -i;
				if( i > 1 )  {
					offmany++;
				} else {
					off1++;
				}
			} else {
				matching++;
			}
			if( b1 != b2 )  {
				if( (i = b1 - b2) < 0 )  i = -i;
				if( i > 1 )  {
					offmany++;
				} else {
					off1++;
				}
			} else {
				matching++;
			}
		}  else  {
			/* Common case:  equal.  Give B&W NTSC average */
			matching += 3;
		}
	}
	fprintf(stderr,
		"pixcmp bytes: %8ld matching, %8ld off by 1, %8ld off by many\n",
		matching, off1, offmany );

	/* If files were of different lengths, that is an error */
	if( feof(f1) != feof(f2) )
		exit(2);

	/* If any pixels were off by more than +/- 1, that is an error */
	if( offmany)
		exit(1);

	/* Success! */
	exit(0);
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
