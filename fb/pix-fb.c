/*
 *  			P I X - F B . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to a framebuffer.
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
#include "fb.h"

#define MAX_LINE	2048		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

Pixel outline[MAX_LINE];

int inverse = 0;			/* Draw upside-down */
int clear = 0;

FBIO *fbp;

char usage[] = "Usage: pix-fb [-h] [-i] [-c] [width] <file.pix\n";

main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int fbsize;

	if( argc < 1 || isatty(fileno(stdin)) )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	fbsize = 512;
	nlines = 512;
	while( argv[1] != NULL && argv[1][0] == '-' )  {
		if( strcmp( argv[1], "-h" ) == 0 )  {
			fbsize = 1024;
			nlines = 1024;
			argc--; argv++;
			continue;
		}
		if( strcmp( argv[1], "-i" ) == 0 )  {
			inverse++;
			argc--; argv++;
			continue;
		}
		if( strcmp( argv[1], "-c" ) == 0 )  {
			clear++;
			argc--; argv++;
			continue;
		}
	}
	infd = 0;	/* stdin */

	if( argc == 2 )
		nlines = atoi(argv[1] );
	if( argc > 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	scanbytes = nlines * 3;

	if( (fbp = fb_open( NULL, fbsize, fbsize )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}
	if( clear )
		fb_clear( fbp, (Pixel *) 0 );
	fb_zoom( fbp, fbsize==nlines? 0 : fbsize/nlines,
		fbsize==nlines? 0 : fbsize/nlines );
	fb_window( fbp, nlines/2, nlines/2 );		/* center of view */

	if( !inverse )  {
		/* Normal way -- bottom to top */
		for( y = nlines-1; y >= 0; y-- )  {
			register char *in;
			register Pixel *out;
			register int i;

			if( fread( (char *)scanline, scanbytes, 1, stdin ) != 1 )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				out++;
			}
			fb_write( fbp, 0, y, outline, nlines );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y=0; y < nlines; y++ )  {
			register char *in;
			register Pixel *out;
			register int i;

			if( fread( (char *)scanline, scanbytes, 1, stdin ) != 1 )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
			fb_write( fbp, 0, y, outline, nlines );
		}
	}
	fb_close( fbp );
	exit(0);
}
