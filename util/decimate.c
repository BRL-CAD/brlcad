/*
 *			D E C I M A T E . C
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
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"

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

	iline = (unsigned char *)malloc( (iwidth+1) * nbytes );
	oline = (unsigned char *)malloc( (owidth+1) * nbytes );
	if( !iline || !oline )  {
		fprintf(stderr,"malloc failure\n");
		exit(2);

	}
	/* Zero output buf, for when last elements are unused (wpad>0) */
	for( i = 0; i < owidth*nbytes; i++ )  {
		oline[i] = '\0';
	}

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
			exit(1);
		}

		/* Discard extra scanlines of input data */
		for( i=0; i < discard; i++ )  {
			if( fread( iline, nbytes, iwidth, stdin ) != iwidth )  {
				exit(0);
			}
		}
	}
	exit(0);
}
