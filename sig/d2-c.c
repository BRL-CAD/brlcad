/*
 *		D 2 - C . C
 *
 * Merge two double files into one complex file.
 *
 *  Author -
 *	Phillip Dykstra
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include "machine.h"

double	obuf[2*1024];
double	real[1024], imag[1024];

void	open_file();

static char usage[] = "\
Usage: d2-c real_file imag_file > complex (- stdin, . skip)\n";

int main( argc, argv )
int argc;
char **argv;
{
	int	i;
	int	nr, ni, num;
	register double *obufp;
	FILE	*rfp, *ifp;

	if( argc != 3 || isatty(fileno(stdout)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	open_file( &rfp, argv[1] );
	open_file( &ifp, argv[2] );

	while( 1 ) {
		nr = fread( real, sizeof( double ), 1024, rfp );
		ni = fread( real, sizeof( double ), 1024, ifp );
		if( nr <= 0 && ni <= 0 )
			break;

		/* find max */
		num = (nr > ni) ? nr : ni;
		if( nr < num )
			bzero( &real[nr], (num-nr)*sizeof(double) );
		if( ni < num )
			bzero( &imag[ni], (num-ni)*sizeof(double) );

		obufp = &obuf[0];
		for( i = 0; i < num; i++ ) {
			*obufp++ = real[i];
			*obufp++ = imag[i];
		}
		fwrite( obuf, sizeof( double ), num*2, stdout );
	}
	return 0;
}

void
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
		fprintf( stderr, "d2-c: Can't open \"%s\"\n", name );
		exit( 2 );
	}
}
