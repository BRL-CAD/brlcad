/*
 *			P I X D I F F . C
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
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

char usage[] = "Usage: pixdiff f1.pix f2.pix >file.pix\n";

main(argc, argv)
char **argv;
{
	register FILE *f1, *f2;

	if( argc != 3 || isatty(fileno(stdout)) )  {
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
		register unsigned char r1, g1, b1;
		static unsigned char r2, g2, b2;

		r1 = fgetc( f1 );
		g1 = fgetc( f1 );
		b1 = fgetc( f1 );
		r2 = fgetc( f2 );
		g2 = fgetc( f2 );
		b2 = fgetc( f2 );
		if( feof(f1) || feof(f2) )  exit(0);

		if( r1 != r2 || g1 != g2 || b1 != b2 )  {
			/* Highlight differing channels */
			if( r1 != r2 )
				putc( 0xFF, stdout);
			else
				putc( 0, stdout);
			if( g1 != g2 )
				putc( 0xFF, stdout);
			else
				putc( 0, stdout);
			if( b1 != b2 )
				putc( 0xFF, stdout);
			else
				putc( 0, stdout);
		}  else  {
			/* Common case:  equal.  Give B&W NTSC average */
			/* .35 R +  .55 G + .10 B, done in fixed-point */
			register long i;
			i = ((22937 * r1 + 36044 * g1 + 6553 * b1)>>17);
			if( i < 0 )  i = 0;
			putc( i, stdout);
			putc( i, stdout);
			putc( i, stdout);
		}
	}
}
