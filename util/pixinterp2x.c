/*
 *  			P I X I N T E R P 2 X . C
 *  
 *  Read a .pix file of a given resolution, and produce one with
 *  twice as many pixels by interpolating between the pixels.
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

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"		/* For malloc and getopt */

FILE	*infp;

int	file_width = 512;
int	file_height = 512;

int	inbytes;			/* # bytes of one input scanline */
int	outbytes;			/* # bytes of one output scanline */
int	outsize;			/* size of output buffer */
unsigned char	*outbuf;		/* ptr to output image buffer */
void	widen_line();

void	interp_lines();

char usage[] = "\
Usage: pixinterp2x [-h] [-s squarefilesize]\n\
	[-w file_width] [-n file_height] [file.pix] > outfile.pix\n";

/*
 *			G E T _ A R G S
 */
static int
get_args( argc, argv )
register char	**argv;
{
	register int	c;

	while( (c = getopt( argc, argv, "hs:w:n:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;
		case 'n':
			file_height = atoi(optarg);
			break;
		case '?':
			return	0;
		}
	}
	if( argv[optind] != NULL )  {
		if( (infp = fopen( argv[optind], "r" )) == NULL )  {
			perror(argv[optind]);
			return	0;
		}
		optind++;
	}
	if( argc > ++optind )
		(void) fprintf( stderr, "Excess arguments ignored\n" );

	if( isatty(fileno(infp)) || isatty(fileno(stdout)) )
		return 0;
	return	1;
}

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	register int iny, outy;
	unsigned char *inbuf;

	infp = stdin;
	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	inbytes = file_width * 3;	/* bytes/ input line */
	inbuf = (unsigned char *)malloc( inbytes );

	outbytes = inbytes * 2;		/* bytes/ output line */
	outsize = file_width * file_height * 4 * 3;
	if( (outbuf = (unsigned char *)malloc( outsize )) == (unsigned char *)0 )  {
		fprintf(stderr,"pixinterp2x:  unable to malloc buffer\n");
		exit( 1 );
	}

	outy = -2;
	for( iny = 0; iny < file_height; iny++ )  {
		if( fread( (char *)inbuf, 1, inbytes, infp ) != inbytes )  {
			fprintf(stderr,"pixinterp2x fread error\n");
			break;
		}

		outy += 2;
		/* outy is line we will write on */
		widen_line( inbuf, outy );
		if( outy == 0 )
			widen_line( inbuf, ++outy );
		else
			interp_lines( outy-1, outy, outy-2 );
	}
	if( write( 1, (char *)outbuf, outsize ) != outsize )  {
		perror("pixinterp2x write");
		exit(1);
	}
	exit(0);
}

void
widen_line( cp, y )
register unsigned char *cp;
int y;
{
	register unsigned char *op;
	register int i;

	op = (unsigned char *)outbuf + (y * outbytes);
	/* Replicate first pixel */
	*op++ = cp[0];
	*op++ = cp[1];
	*op++ = cp[2];
	/* Prep process by copying first pixel */
	*op++ = *cp++;
	*op++ = *cp++;
	*op++ = *cp++;
	for( i=0; i<inbytes; i+=3)  {
		/* Average previous pixel with current pixel */
		*op++ = ((int)cp[-3+0] + (int)cp[0])>>1;
		*op++ = ((int)cp[-3+1] + (int)cp[1])>>1;
		*op++ = ((int)cp[-3+2] + (int)cp[2])>>1;
		/* Copy pixel */
		*op++ = *cp++;
		*op++ = *cp++;
		*op++ = *cp++;
	}
}

void
interp_lines( out, i1, i2 )
int out, i1, i2;
{
	register unsigned char *a, *b;	/* inputs */
	register unsigned char *op;
	register int i;

	a = (unsigned char *)outbuf + (i1 * outbytes);
	b = (unsigned char *)outbuf + (i2 * outbytes);
	op = (unsigned char *)outbuf + (out * outbytes);

	for( i=0; i<outbytes; i+=3 )  {
		*op++ = ((int)*a++ + (int)*b++)>>1;
		*op++ = ((int)*a++ + (int)*b++)>>1;
		*op++ = ((int)*a++ + (int)*b++)>>1;
	}
}
