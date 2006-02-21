/*                      D E C I M A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file decimate.c
 *
 *  Program to take M by N array of tuples, and reduce it down to
 *  being an I by J array of tuples, by discarding unnecessary stuff.
 *  The borders are padded with zeros.
 *
 *  Especially useful for looking at image files which are larger than
 *  your biggest framebuffer, quickly.
 *
 *  Specify nbytes/pixel as:
 *	1	.bw files
 *	3	.pix files
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

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"


static char usage[] = "\
Usage: decimate nbytes/pixel width height [outwidth outheight]\n\
";

int	nbytes;
int	iwidth;
int	iheight;
int	owidth = 512;
int	oheight = 512;

unsigned char	*iline;
unsigned char	*oline;

int	discard;
int	wpad;

int
main(int argc, char **argv)
{
	register int	i;
	register int	j;
	int	nh, nw;
	int	dh, dw;
	int	todo;

	if( argc < 4 )  {
		fputs( usage, stderr );
		exit(1);
	}

	nbytes = atoi(argv[1]);
	iwidth = atoi(argv[2]);
	iheight = atoi(argv[3]);

	if( argc >= 6 )  {
		owidth = atoi(argv[4]);
		oheight = atoi(argv[5]);
	}

	/* Determine how many samples/lines to discard after each one saved,
	 * and how much padding to add to the end of each line.
	 */
	nh = (iheight + oheight-1) / oheight;
	nw = (iwidth + owidth-1) / owidth;

	dh = nh - 1;
	dw = nw - 1;
	discard = dh;
	if( dw > discard ) discard = dw;


	wpad = owidth - ( iwidth / (discard+1) );

	iline = (unsigned char *)bu_calloc( (iwidth+1), nbytes, "iline" );
	oline = (unsigned char *)bu_calloc( (owidth+1),  nbytes, "oline" );

	todo = iwidth / (discard+1) * (discard+1);
	if( owidth < todo )  todo = owidth;
	if( todo > iwidth/(discard+1) )  todo = iwidth/(discard+1);
#if 0
	fprintf(stderr,"dh=%d dw=%d, discard=%d\n", dh, dw, discard);
	fprintf(stderr,"todo=%d\n", todo);
#endif

	while( !feof(stdin) )  {
		register unsigned char	*ip, *op;

		/* Scrunch down first scanline of input data */
		if( fread( iline, nbytes, iwidth, stdin ) != iwidth ) break;
		ip = iline;
		op = oline;
		for( i=0; i < todo; i++ )  {
			for( j=0; j < nbytes; j++ )  {
				*op++ = *ip++;
			}
			ip += discard * nbytes;
		}
		if( fwrite( oline, nbytes, owidth, stdout ) != owidth )  {
			perror("fwrite");
			goto out;
		}

		/* Discard extra scanlines of input data */
		for( i=0; i < discard; i++ )  {
			if( fread( iline, nbytes, iwidth, stdin ) != iwidth )  {
			    goto out;
			}
		}
	}

 out:
	bu_free(iline, "iline");
	bu_free(oline, "oline");

	return 0;
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
