/*                   P I X I N T E R P 2 X . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pixinterp2x.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"


FILE	*infp;

int	file_width = 512;
int	file_height = 512;

int	inbytes;			/* # bytes of one input scanline */
int	outbytes;			/* # bytes of one output scanline */
int	outsize;			/* size of output buffer */
unsigned char	*outbuf;		/* ptr to output image buffer */
void	widen_line(register unsigned char *cp, int y);

void	interp_lines(int out, int i1, int i2);

char usage[] = "\
Usage: pixinterp2x [-h] [-s squarefilesize]\n\
	[-w file_width] [-n file_height] [file.pix] > outfile.pix\n";

/*
 *			G E T _ A R G S
 */
static int
get_args(int argc, register char **argv)
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
int
main(int argc, char **argv)
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
widen_line(register unsigned char *cp, int y)
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
interp_lines(int out, int i1, int i2)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
