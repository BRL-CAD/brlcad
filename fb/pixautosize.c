/*
 *  			P I X A U T O S I Z E . C
 *  
 *  Program to determine if a given file is one of the "standard"
 *  sizes as known by the framebuffer library.
 *
 *  Used by pixinfo.sh to determine size of .pix and .bw files.
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

static int	bytes_per_sample = 3;
static int	file_length = 0;
static char	*file_name;

static int	width;
static int	height;

static char usage[] = "\
Usage:	pixautosize [-b bytes_per_sample] [-f file_name]\n\
or	pixautosize [-b bytes_per_sample] [-l file_length]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "b:f:l:" )) != EOF )  {
		switch( c )  {
		case 'b':
			bytes_per_sample = atoi(optarg);
			break;
		case 'f':
			file_name = optarg;
			break;
		case 'l':
			file_length = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pixautosize: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	int	ret = 0;
	int	nsamp;

	if ( !get_args( argc, argv ) || bytes_per_sample <= 0 )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if ( !file_name && file_length <= 0 )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( file_name ) {
		if( !bn_common_file_size(&width, &height, file_name, bytes_per_sample) ) {
			fprintf(stderr,"pixautosize: unable to autosize file '%s'\n", file_name);
			ret = 1;		/* ERROR */
		}
	} else {
		nsamp = file_length/bytes_per_sample;
		if( !bn_common_image_size(&width, &height, nsamp) ) {
			fprintf(stderr,"pixautosize: unable to autosize nsamples=%d\n", nsamp);
			ret = 2;		/* ERROR */
		}
	}

	/*
	 *  Whether or not an error message was printed to stderr above,
	 *  print out the width and height on stdout.
	 *  They will be zero on error.
	 */
	printf("WIDTH=%d; HEIGHT=%d\n", width, height);
	return ret;
}
