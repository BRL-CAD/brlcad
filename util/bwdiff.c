/*
 *		B W D I F F . C
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
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

FILE *fp1, *fp2;

int	mflag = 0;
unsigned char ibuf1[512], ibuf2[512], obuf[512];

static	char *Usage = "usage: bwdiff [-m] file1.bw file2.bw (- for stdin)\n";

main( argc, argv )
int argc; char **argv;
{
	register unsigned char *p1, *p2, *op;
	int	i, n, m;
	
	if( argc > 1 && strcmp(argv[1],"-m") == 0 ) {
		mflag++;
		argv++;
		argc--;
	}

	if( argc != 3 ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	if( strcmp(argv[1], "-") == 0 )
		fp1 = stdin;
	else
		fp1 = fopen( argv[1], "r" );
	if( strcmp(argv[2], "-") == 0 && fp1 != stdin )
		fp2 = stdin;
	else
		fp2 = fopen( argv[2], "r" );

	if( fp1 == NULL || fp2 == NULL ) {
		fprintf( stderr, "bwdiff: can't open files\n" );
		exit( 42 );
	}

	while( (n = fread(ibuf1,1,512,fp1)) > 0 && (m = fread(ibuf2,1,512,fp2)) > 0 ) {
		p1 = &ibuf1[0];
		p2 = &ibuf2[0];
		op = &obuf[0];
		if( m < n ) n = m;
		for( i = 0; i < n; i++ ) {
			if( mflag ) {
				*op++ = abs( *p1 - *p2 );
			} else {
				*op++ = (*p1 - *p2)/2 + 127;
			}
			p1++;
			p2++;
		}
		fwrite( &obuf[0], 1, n, stdout );
	}
}
