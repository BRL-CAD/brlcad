/*
 *			P I X F I E L D S E P . C
 *
 *  Separate an interlaced video image into two separate .pix files.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include "machine.h"

FILE	*out1;
FILE	*out2;

char	*buf;
int	file_width = 720;
int	bytes_per_sample = 3;
int	doubleit = 0;

char	*even_file = "even.pix";
char	*odd_file = "odd.pix";

static char usage[] = "\
Usage: pixfieldsep [-w file_width] [-s square_size] [-# nbytes/pixel] \n\
	[-d] [even.pix odd.pix]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "ds:w:#:" )) != EOF )  {
		switch( c )  {
		case 'd':
			doubleit = 1;
			break;
		case '#':
			bytes_per_sample = atoi(optarg);
			break;
		case 's':
			/* square file size */
			file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind < argc )  {
		even_file = argv[optind++];
	}
	if( optind < argc )  {
		odd_file = argv[optind++];
	}

	if( ++optind <= argc )
		(void)fprintf( stderr, "pixfieldsep: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int	i;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (out1 = fopen(even_file, "w")) == NULL )  {
		perror(even_file);
		exit(1);
	}
	if( (out2 = fopen(odd_file, "w")) == NULL )  {
		perror(odd_file);
		exit(2);
	}

	buf = (char *)malloc( (file_width+1)*bytes_per_sample );

	for(;;)  {
		/* Even line */
		if( fread( buf, bytes_per_sample, file_width, stdin ) != file_width )
			break;
		for( i=0; i <= doubleit; i++ )  {
			if( fwrite( buf, bytes_per_sample, file_width, out1 ) != file_width )  {
				perror("fwrite even");
				exit(1);
			}
		}
		/* Odd line */
		if( fread( buf, bytes_per_sample, file_width, stdin ) != file_width )
			break;
		for( i=0; i <= doubleit; i++ )  {
			if( fwrite( buf, bytes_per_sample, file_width, out2 ) != file_width )  {
				perror("fwrite odd");
				exit(1);
			}
		}
	}
	exit(0);
}
