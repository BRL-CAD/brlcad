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

#ifdef SYSV
#define bzero(p,cnt)	memset(p,'\0',cnt);
#endif

#include <stdio.h>

FILE *fp1, *fp2;

int	mflag = 0;
unsigned char ibuf1[512], ibuf2[512], obuf[512];

static	char *Usage = "usage: bwdiff [-m] file1.bw file2.bw (- stdin, . skip)\n";

main( argc, argv )
int argc; char **argv;
{
	register unsigned char *p1, *p2, *op;
	int	i, n, m, dot;
	
	if( argc > 1 && strcmp(argv[1],"-m") == 0 ) {
		mflag++;
		argv++;
		argc--;
	}

	if( argc != 3 || isatty(fileno(stdout)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	if( strcmp(argv[1], argv[2]) == 0) {
		fprintf( stderr,
		  "bwdiff: comparing a file to itself is a nop\n");
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
			 bzero( (&ibuf2[m]), (n - m));
		}
		if( m > n ) {
			 bzero( (&ibuf1[n]), (m - n));
			 n = m;
		}
		for( i = 0; i < n; i++ ) {
			if( mflag ) {
				dot = abs( (int)*p1 - (int)*p2 );
			} else {
				dot = ((int)*p1 - (int)*p2)/2 + 127;
			}
			if(dot < 0) dot = 0;
			if(dot > 255) dot = 255;
			*op++ = (unsigned char) dot;
			p1++;
			p2++;
		}
		fwrite( &obuf[0], 1, n, stdout );
	}
}

open_file( fp, name )
FILE **fp;
char *name;
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
