/*
 * convert BRL .pix files to ppm
 * sahayman 1991 01 18
 *
 * Pixels run left-to-right, from the bottom of the screen to the top.
 *
 *  Authors -
 *	Steve Hayman <sahayman@iuvax.cs.indiana.edu>
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
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"

static int	file_width = 512;	/* default input width */
static int	file_height = 512;	/* default input height */

static int	autosize = 0;		/* !0 to autosize input */

static int	fileinput = 0;		/* file of pipe on input? */
static char	*file_name;
static FILE	*infp;

#define BYTESPERPIXEL 3
#define ROWSIZE (file_width * BYTESPERPIXEL)

#define SIZE (file_width * file_height * BYTESPERPIXEL)

char *scanbuf;

static char usage[] = "\
Usage: pix-ppm [-a] [-w file_width] [-n file_height]\n\
	[-s square_file_size] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "as:w:n:" )) != EOF )  {
		switch( c )  {
		case 'a':
			autosize = 1;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(optarg);
			autosize = 0;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			perror(file_name);
			(void)fprintf( stderr,
				"pix-ppm: cannot open \"%s\" for reading\n",
				file_name );
			exit(1);
		}
		fileinput++;
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-ppm: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int	argc;
char	**argv;
{
	int i;
	char *row;


	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		int	w, h;
		if( bn_common_file_size(&w, &h, file_name, 3) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr,"pix-ppm: unable to autosize\n");
		}
	}


	/*
	 * gobble up the bytes
	 */
	scanbuf = malloc( SIZE );
	if ( scanbuf == NULL ) {
		perror("pix-ppm: malloc");
		exit(1);
	}
	if ( fread(scanbuf, 1, SIZE, infp) == 0 ) {
		fprintf(stderr, "pix-ppm: Short read\n");
		exit(1);
	}

	/* PPM magic number */
	printf("P6\n");

	/* width height */
	printf("%d %d\n", file_width, file_height);

	/* maximum color component value */
	printf("255\n");
	fflush(stdout);

	/*
	 * now write them out in the right order, 'cause the
	 * input is upside down.
	 */
	
	for ( i = 0; i < file_height; i++ ) {
		row = scanbuf + (file_height - i) * ROWSIZE;
		fwrite(row, 1, ROWSIZE, stdout);
	}

	exit(0);
}
