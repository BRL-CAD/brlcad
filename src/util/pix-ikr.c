/*                       P I X - I K R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file pix-ikr.c
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to the Ikonas, UPSIDE DOWN, for the Dunn camera.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

extern int ikfd;
extern int ikhires;

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

static char outline[MAX_LINE*4+4];	/* Ikonas pixels */

char usage[] = "Usage: pix-ikr [-h] file.pix [width] [fr_offset] [fr_count]\n";

int
main(int argc, char **argv)
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int frame_offset;
	static int frame_count;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		nlines = 1024;
		argc--; argv++;
		ikhires = 1;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	frame_offset = 0;
	if( argc >= 4 )
		frame_offset = atoi(argv[3]);
	frame_count = 1;
	if( argc >= 5 )
		frame_count = atoi(argv[4]);
	if( nlines > 512 )
		ikhires = 1;

	scanbytes = nlines * 3;

	(void)lseek( infd, (long)frame_offset*scanbytes*nlines, 0 );

	ikopen();
	ikclear();

	while(frame_count-- > 0)  {
		/* Go bottom to top of file, (top to bottom of screen)
		 * for the Dunn */
		for( y=0; y < nlines; y++ )  {
			register char *in, *out;

			if( read( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);

			in = scanline;
			out = outline+(4*nlines)-1;
			while( out > outline )  {
				*out-- = 0;
				*out-- = in[2];
				*out-- = in[1];
				*out-- = *in;
				in += 3;
			}
			clustwrite( outline, y, nlines );
		}
	}
	exit(0);
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
