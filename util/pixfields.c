/*
 *		P I X F I E L D S . C
 *
 *  pixfields takes to input pixures and extracts field 1 from the first
 *  pix file and field 2 comes from the second pix file.
 *
 *  Author -
 *	Christopher T. Johnson - 05 December 1989
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

#ifdef BSD
#include	<strings.h>
#endif
#ifdef SYSV
#include	<string.h>
#endif

extern int	getopt();
extern char	*optarg;
extern int	optind;

#define DEFAULT_WIDTH	512

int	width = DEFAULT_WIDTH;
int	height = DEFAULT_WIDTH;
int	verbose = 0;

char *file_name;
FILE *fldonefp,*fldtwofp;

void	dousage();

char	usage[] = "\
Usage: pixfields [-v]\n\
        [-s squaresize] [-w width] [-n height]\n\
	 field1.pix field2.pix > file.pix\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "vw:n:s:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;
		case 's':
			width = height = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc + 1 )  {
		(void) fprintf( stderr,
		    "pixfields: must supply two file names\n");
		return(0);
	} else {

		if( (fldonefp = fopen(argv[optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixfields: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}

		if( (fldtwofp = fopen(argv[++optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixfields: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}

	}

	if( isatty(fileno(stdout)) )
		return(0);

	if ( argc > ++optind )
		(void)fprintf( stderr, "pixfields: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main( argc, argv )
int argc; char **argv;
{
	char *line;
	int line_number;

	if ( !get_args( argc, argv ) )  {
		dousage();
		exit( 1 );
	}
	
	line = (char *) malloc(width*3+1);

	line_number = height-1;
	while(line_number >= 0) {
		if (line_number & 1) {
			fread(line, sizeof(char), 3*width, fldtwofp);
			fread(line, sizeof(char), 3*width,fldonefp);
		} else {
			fread(line, sizeof(char), 3*width,fldonefp);
			fread(line, sizeof(char), 3*width, fldtwofp);
		}

		fwrite( line, sizeof( char ), 3*width, stdout );
		--line_number;
	}
	exit( 0 );
}

void
dousage()
{
	int	i;

	fputs( usage, stderr );
}
