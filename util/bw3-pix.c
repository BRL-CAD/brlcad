/*
 *		B W 3 - P I X . C
 *
 * Merge three BW files into one RGB pix file.
 *  (i.e. combine the colors)
 *
 *  Author -
 *	Phillip Dykstra
 *	13 June 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"

unsigned char	obuf[3*1024];
unsigned char	red[1024], green[1024], blue[1024];

void	open_file(FILE **fp, char *name);

char *Usage = "usage: bw3-pix redin greenin bluein > file.pix (- stdin, . skip)\n";

int
main(int argc, char **argv)
{
	int	i;
	int	nr, ng, nb, num;
	register unsigned char *obufp;
	FILE	*rfp, *bfp, *gfp;

	if( argc != 4 || isatty(fileno(stdout)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	open_file( &rfp, argv[1] );
	open_file( &gfp, argv[2] );
	open_file( &bfp, argv[3] );

	while( 1 ) {
		nr = fread( red, sizeof( char ), 1024, rfp );
		ng = fread( green, sizeof( char ), 1024, gfp );
		nb = fread( blue, sizeof( char ), 1024, bfp );
		if( nr <= 0 && ng <= 0 && nb <= 0 )
			break;

		/* find max */
		num = (nr > ng) ? nr : ng;
		if( nb > num ) num = nb;
		if( nr < num )
			bzero( (char *)&red[nr], num-nr );
		if( ng < num )
			bzero( (char *)&green[ng], num-ng );
		if( nb < num )
			bzero( (char *)&blue[nb], num-nb );

		obufp = &obuf[0];
		for( i = 0; i < num; i++ ) {
			*obufp++ = red[i];
			*obufp++ = green[i];
			*obufp++ = blue[i];
		}
		fwrite( obuf, sizeof( char ), num*3, stdout );
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
		fprintf( stderr, "bw3-pix: Can't open \"%s\"\n", name );
		exit( 2 );
	}
}
