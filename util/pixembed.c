/*
 *			P I X E M B E D . C
 *
 *  Embed a smaller pix file in a larger space, replicating the boundary
 *  pixels to fill out the borders, and output as a pix file.
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
 *	This software is Copyright (C) 1992-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"

unsigned char	*obuf;

int	scanlen;			/* length of infile (and buffer) scanlines */

FILE	*buffp;
static char	*file_name;

int	xin = 512;
int	yin = 512;
int	xout = 512;
int	yout = 512;

int	border_inset = 0;		/* Sometimes border pixels are bad */

void	load_buffer(), write_buffer();

static	char usage[] = "\
Usage: pixembed [-h] [-b border_inset] \n\
	[-s squareinsize] [-w inwidth] [-n inheight]\n\
	[-S squareoutsize] [-W outwidth] [-N outheight] [in.pix] > out.pix\n";

/*
 *			G E T _ A R G S
 */
int
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "b:hs:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'b':
			border_inset = atoi(optarg);
			break;
		case 'h':
			/* high-res */
			xin = yin = 1024;
			break;
		case 'S':
			/* square size */
			xout = yout = atoi(optarg);
			break;
		case 's':
			/* square size */
			xin = yin = atoi(optarg);
			break;
		case 'W':
			xout = atoi(optarg);
			break;
		case 'w':
			xin = atoi(optarg);
			break;
		case 'N':
			yout = atoi(optarg);
			break;
		case 'n':
			yin = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		buffp = stdin;
	} else {
		file_name = argv[optind];
		if( (buffp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixembed: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pixembed: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main( argc, argv )
int	argc;
char	**argv;
{
	int	ydup;
	int	i;
	int	y;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( xin <= 0 || yin <= 0 || xout <= 0 || yout <= 0 ) {
		fprintf( stderr, "pixembed: sizes must be positive\n" );
		exit( 2 );
	}
	if( xout < xin || yout < yin )  {
		fprintf( stderr, "pixembed: output size must exceed input size\n");
		exit(3);
	}

	if( border_inset < 0 || border_inset >= xin )  {
		fprintf(stderr, "pixembed: border inset out of range\n");
		exit(4);
	}

	/* Allocate storage for one output line */
	scanlen = 3*xout;
	if( (obuf = (unsigned char *)malloc( scanlen )) == (unsigned char *)0 )  {
		fprintf( stderr, "pixembed: malloc failure\n");
		exit(4);
	}

	/* Pre-fetch the first line (after skipping) */
	for( i= -1; i<border_inset; i++ )  load_buffer();

	/* Write out duplicates at bottom, including real copy of 1st line */
	ydup = (yout - yin) / 2 + border_inset + 1;
	for( y = 0; y < ydup; y++ )  write_buffer();

	/* Read and write the remaining lines */
	for( ; i < yin-border_inset; i++, y++ )  {
		load_buffer();
		write_buffer();
	}

	/* Write out duplicates at the top, until all done */
	for( ; y < yout; y++ )  write_buffer();

	return 0;
}

/*
 *			L O A D _ B U F F E R
 *
 *  Read one input scanline into the middle of the output scanline,
 *  and duplicate the border pixels.
 */
void
load_buffer()
{
	register unsigned char	r,g,b;
	register unsigned char	*cp;
	register int		i;
	register int		inbase;

	inbase = (xout - xin) / 2;

	if( fread( obuf + inbase*3, 3, xin, buffp ) != xin )  {
		perror("pixembed fread");
		exit(7);
	}
	r = obuf[(inbase+border_inset)*3+0];
	g = obuf[(inbase+border_inset)*3+1];
	b = obuf[(inbase+border_inset)*3+2];

	cp = obuf;
	for( i=0; i<inbase+border_inset; i++ )  {
		*cp++ = r;
		*cp++ = g;
		*cp++ = b;
	}

	r = obuf[(xin+inbase-1-border_inset)*3+0];
	g = obuf[(xin+inbase-1-border_inset)*3+1];
	b = obuf[(xin+inbase-1-border_inset)*3+2];
	cp = &obuf[(xin+inbase+0-border_inset)*3+0];
	for( i=xin+inbase-border_inset; i<xout; i++ )  {
		*cp++ = r;
		*cp++ = g;
		*cp++ = b;
	}

}

/*
 *			W R I T E _ B U F F E R
 *
 *  Write the buffer to stdout, with error checking.
 */
void
write_buffer()
{
	if( fwrite( obuf, 3, xout, stdout ) != xout )  {
		perror("pixembed stdout fwrite");
		exit(8);
	}
}
