/*
 *  			P I X M E R G E . C
 *  
 *  Given two streams of data, typically pix(5) or bw(5) images,
 *  generate an output stream of the same size, where the value of
 *  the output is determined by a formula involving the first
 *  (foreground) stream and a constant, or the value of the second
 *  (background) stream.
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

extern int	getopt();
extern char	*optarg;
extern int	optind;

static char	*f1_name;
static char	*f2_name;
static FILE	*f1;
static FILE	*f2;

#define LT	1
#define EQ	2
#define GT	4
static int	wanted;			/* LT|EQ|GT conditions to pick fb */

static int	seen_const;
static int	seen_formula;

static int	width = 3;
static unsigned char	const[32];
static unsigned char	fg[32];
static unsigned char	bg[32];

static char usage[] = "\
Usage: pixmerge [-g -l -e -n] [-w bytes_wide] [-c r/g/b]\n\
	foreground.pix background.pix > out.pix\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "glenw:c:" )) != EOF )  {
		switch( c )  {
		case 'g':
			wanted |= GT;
			seen_formula = 1;
			break;
		case 'l':
			wanted |= LT;
			seen_formula = 1;
			break;
		case 'e':
			wanted |= EQ;
			seen_formula = 1;
			break;
		case 'n':
			wanted |= GT|LT;
			seen_formula = 1;
			break;
		case 'w':
			c = atoi(optarg);
			if( c > 1 && c < sizeof(const) )
				width = c;
			break;
		case 'c':
			{
				register char *cp = optarg;
				register unsigned char *conp = const;

				/* premature null => atoi gives zeros */
				for( c=0; c < width; c++ )  {
					*conp++ = atoi(cp);
					while( *cp && *cp++ != '/' ) ;
				}
			}
			seen_const = 1;
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
			"pixmerge: cannot open \"%s\" for reading\n",
			f1_name );
		return(0);
	}

	f2_name = argv[optind++];
	if( strcmp( f2_name, "-" ) == 0 )
		f2 = stdin;
	else if( (f2 = fopen(f2_name, "r")) == NULL )  {
		perror(f2_name);
		(void)fprintf( stderr,
			"pixmerge: cannot open \"%s\" for reading\n",
			f2_name );
		return(0);
	}

	if ( argc > optind )
		(void)fprintf( stderr, "pixmerge: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register int i;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( !seen_formula )  {
		wanted = GT;
		seen_const = 1;		/* Default is const of 0/0/0 */
	}
	fprintf(stderr, "Selecting foreground when fg ");
	if( wanted & LT )  putc( '<', stderr );
	if( wanted & EQ )  putc( '=', stderr );
	if( wanted & GT )  putc( '>', stderr );
	if( seen_const )  {
		putc( ' ', stderr );
		for( i = 0; i < width; i++ )
			fprintf( stderr, "%d/", const[i] );
		putc( '\n', stderr );
	} else {
		fprintf( stderr, " bg\n" );
	}

	while(1)  {
		register unsigned char	*ap, *bp;
		register unsigned char	*ep;		/* end ptr */

		if( fread( fg, width, 1, f1 ) != 1 ||
		    fread( bg, width, 1, f2 ) != 1 )
			break;

		/*
		 * Stated condition must hold for all input bytes
		 * to select the foreground for output
		 */
		ap = fg;
		if( seen_const )
			bp = const;
		else
			bp = bg;
		for( ep = fg+width; ap < ep; ap++,bp++ )  {
			if( *ap > *bp )  {
				if( !(GT & wanted) ) goto fail;
			} else if( *ap == *bp )  {
				if( !(EQ & wanted) ) goto fail;
			} else  {
				if( !(LT & wanted) ) goto fail;
			}
		}
		/* success */
		fwrite( fg, width, 1, stdout );
		continue;
fail:
		fwrite( bg, width, 1, stdout );
	}
	exit(0);
}
