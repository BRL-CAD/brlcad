/*
 *  			P I X B L E N D . C
 *  
 *  Given two streams of data, typically pix(5) or bw(5) images,
 *  generate an output stream of the same size, where the value of
 *  the output is determined by a blending value (0.0 < blend < 1.0)
 *  where a linear blend between pixels is done.
 *  
 *
 *  This routine operates on a pixel-by-pixel basis, and thus
 *  is independent of the resolution of the image.
 *  
 *  Author -
 *	Paul Randal Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For malloc and getopt */

static char	*f1_name;
static char	*f2_name;
static FILE	*f1;
static FILE	*f2;

#define CHUNK	1024
static char	*b1;			/* fg input buffer */
static char	*b2;			/* bg input buffer */
static char	*b3;			/* output buffer */

double		blend;
static unsigned char	pconst[32];

static char usage[] = "\
Usage: pixblend [-b blend] [-w bytes_wide] source.pix destination.pix > out.pix\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "b:" )) != EOF )  {
		switch( c )  {
		case 'b':
			blend = atof( optarg );
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind+2 > argc )
		return(0);

	f1_name = argv[optind++];
	if( strcmp( f1_name, "-" ) == 0 )
		f1 = stdin;
	else if( (f1 = fopen(f1_name, "r")) == NULL )  {
		perror(f1_name);
		(void)fprintf( stderr,
			"pixblend: cannot open \"%s\" for reading\n",
			f1_name );
		return(0);
	}

	f2_name = argv[optind++];
	if( strcmp( f2_name, "-" ) == 0 )
		f2 = stdin;
	else if( (f2 = fopen(f2_name, "r")) == NULL )  {
		perror(f2_name);
		(void)fprintf( stderr,
			"pixblend: cannot open \"%s\" for reading\n",
			f2_name );
		return(0);
	}

	if ( argc > optind )
		(void)fprintf( stderr, "pixblend: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	
	blend = .5;			/* Default Blend */

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( blend < 0.0 || blend > 1.0)
	{
		fprintf(stderr,"pixblend: Blend value must be between 0.0 and 1.0\n");
		exit(0);

	}

	if( (b1 = malloc( CHUNK )) == (char *)0 ||
	    (b2 = malloc( CHUNK )) == (char *)0 ||
	    (b3 = malloc( CHUNK )) == (char *)0 ) {
	    	fprintf(stderr, "pixblend:  malloc failure\n");
	    	exit(3);
	}

	while(1)  {
		unsigned char	*cb1, *cb2;	/* current input buf ptrs */
		register unsigned char	*cb3; 	/* current output buf ptr */
		int r1, r2, len, todo;

		r1 = fread( b1, 1, CHUNK, f1 );
		r2 = fread( b2, 1, CHUNK, f2 );
		len = r1;
		if( r2 < len )
			len = r2;
		if( len <= 0 )
			break;

		cb1 = (unsigned char *)b1;
		cb2 = (unsigned char *)b2;
		cb3 = (unsigned char *)b3;
		todo = len;
		while( todo--){
		  *cb3++ = (char) ((1.0 - blend) * (*cb1++) +
				blend * (*cb2++));
		}
		fwrite( b3, 1, len, stdout );
	}
	exit(0);
}
