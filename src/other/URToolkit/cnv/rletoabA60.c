/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/*
 * rletoabA60.c - Convert rle images into 4:2:2 yuv format for Abekas A60
 *
 * Author:	T. Todd Elvins
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri June 3 1988
 * Copyright (c) 1988, University of Utah
 *
 */
#if 0
rletoabA60()			/* For tags. */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

#ifndef ABEKAS_PAL
#define LINE_LENGTH 720
#define FRAME_LENGTH 486
#else
#define LINE_LENGTH 720
#define FRAME_LENGTH 576
#endif

#define shifty(val) y4=y3;y3=y2;y2=y1;y1=y0;y0=(val)
#define shiftu(val) u4=u3;u3=u2;u2=u1;u1=u0;u0=(val)
#define shiftv(val) v4=v3;v3=v2;v2=v1;v1=v0;v0=(val)

void read_image(), send_image();

rle_hdr	hdr;
rle_pixel scanred[FRAME_LENGTH][LINE_LENGTH];
rle_pixel scanblu[FRAME_LENGTH][LINE_LENGTH];
rle_pixel scangrn[FRAME_LENGTH][LINE_LENGTH];

int
main(argc,argv)
int argc;
char **argv;
{
    char       *infname = NULL,
    	       *out_fname = NULL;
    int 	cflag = 0,
    		Pflag = 0,
    		pflag = 0,
    		oflag = 0;
    register int i, xrun, maxy;
    int minx, maxx, miny, yrun, px, py;
    int ix, iy;
    rle_pixel *scanbuf[3];
    FILE *outfile;

    if ( scanargs( argc, argv,
	"% c%- P%-incx!dincy!d p%-posx!dposy!d o%-outfile!s infile%s\n(\
\t-c\tCenter image in Abekas frame.\n\
\t-p\tPosition image at given position.\n\
\t-P\tIncrementally position image by given offset.)",
	      &cflag,
	      &Pflag, &ix, &iy,
	      &pflag, &px, &py, &oflag, &out_fname, &infname ) == 0 )
	exit(3);

    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname, 0 );

    if ( cflag + pflag +  Pflag > 1 )  {
	fprintf( stderr, "%s: specify exactly one of -c -p -P\n", hdr.cmd );
	exit(3);
    }
    hdr.rle_file = rle_open_f(hdr.cmd, infname, "r");
    outfile = rle_open_f(hdr.cmd, out_fname, "w");
    /*
     * Do the rle initialization stuff
     */
    rle_get_setup_ok( &hdr, NULL, NULL );

    if ( hdr.xmax > LINE_LENGTH || hdr.ymax > FRAME_LENGTH )  {
	fprintf( stderr,"%s: rle image too big for Abekas\n", hdr.cmd);
	exit(3);
    }
    RLE_CLR_BIT( hdr, RLE_ALPHA );
    for (i = 3 ; i < hdr.ncolors ; i++)
	RLE_CLR_BIT( hdr, i );

    /*
     * Initialize some variables.
     */
    bzero( scanred, LINE_LENGTH * FRAME_LENGTH );
    bzero( scangrn, LINE_LENGTH * FRAME_LENGTH );
    bzero( scanblu, LINE_LENGTH * FRAME_LENGTH );

    for (i=0; i<3; i++)
	scanbuf[i] = (rle_pixel *) malloc ( LINE_LENGTH * sizeof(rle_pixel));

    /*
     * Put the entire rle image into a buffer upside down and centered.
     */
    xrun = hdr.xmax - hdr.xmin + 1;
    yrun = hdr.ymax - hdr.ymin + 1;
    if ( cflag )  {
	minx = (LINE_LENGTH - xrun) / 2;
	miny = (FRAME_LENGTH - yrun) / 2;
    }
    else if ( pflag )  {
	minx = px;
	miny = FRAME_LENGTH - (yrun + py);
    }
    else if ( Pflag )  {
	minx = hdr.xmin + ix;
	miny = FRAME_LENGTH - (yrun + hdr.ymin + iy);
    }
    else {
	minx = hdr.xmin;
	miny = FRAME_LENGTH - (yrun + hdr.ymin);
    }

    maxx = minx + xrun - 1;
    maxy = miny + yrun - 1;

    if ( maxx > LINE_LENGTH-1 )
	xrun = LINE_LENGTH - minx;

    read_image( yrun, scanbuf, minx, maxy, xrun );
    send_image( outfile );

    exit(0);
}

void
read_image ( yrun, scanbuf, minx, maxy, xrun )
int yrun;
rle_pixel *scanbuf[3];
int minx, maxy, xrun;
{
    register int line, upsidedowny;

    for( line = 0; line < yrun; line++ )  {
	rle_getrow( &hdr, scanbuf );
	upsidedowny = maxy - line;

	if ( ( upsidedowny > FRAME_LENGTH-1 ) || ( upsidedowny < 0 ) )
	    continue;

	bcopy( scanbuf[RLE_RED],   &scanred[upsidedowny][minx], xrun );
	bcopy( scanbuf[RLE_GREEN], &scangrn[upsidedowny][minx], xrun );
	bcopy( scanbuf[RLE_BLUE],  &scanblu[upsidedowny][minx], xrun );
    }

}

void
send_image ( outfile )
FILE *outfile;
{
    unsigned char buf[LINE_LENGTH*5];
    register unsigned char *bp;
    int line, col;
    register rle_pixel *sr, *sg, *sb;
    register int r, g, b;
    int y, u, v;
    int y0=0, y1=0, y2=0, y3=0, y4=0;
    int u0=0, u1=0, u2=0, u3=0, u4=0;
    int v0=0, v1=0, v2=0, v3=0, v4=0;

    /*
     * Convert the 720x486 rle image to yuv one byte at a time.
     *
     * All arithmetic is performed in "fixed point" integer for speed.
     * The original equations are shown in comments.
     */
    for( line=0; line < FRAME_LENGTH; line++ )  {
	sr = scanred[line];
	sg = scangrn[line];
	sb = scanblu[line];
	bp = buf;

	for( col=0; col < LINE_LENGTH; col+=2 )  {
	    if ( *sr || *sg || *sb )
	    {
		/*
		 * r = *sr++ / 255.0;
		 * g = *sg++ / 255.0;
		 * b = *sb++ / 255.0;
		 */
		r = (*sr++ * 8000000) / 255;
		g = (*sg++ * 8000000) / 255;
		b = (*sb++ * 8000000) / 255;

		/*
		 * shifty( ( 0.2990 * r + 0.5870 * g + 0.1140 * b));
		 * shiftu( (-0.1686 * r - 0.3311 * g + 0.4997 * b));
		 * shiftv( ( 0.4998 * r - 0.4185 * g - 0.0813 * b));
		 */
		shifty( ( r / 33445 + g / 17036 + b / 87719) * 1000000 );
		shiftu( (-r / 59312 - g / 30202 + b / 20012) * 1000000 );
		shiftv( ( r / 20012 - g / 23895 - b / 123001) * 1000000 );
	    }
	    else
	    {
		sr++; sg++; sb++;
		shifty( 0 ); shiftu( 0 ); shiftv( 0 );
	    }

	    /*
	     * u = ( (0.14963 * u0) + (0.22010 * u1)
	     *      +(0.26054 * u2) + (0.22010 * u3)
	     *      +(0.14963 * u4)) * 224;
	     */
	    u = ( (u0 / 66832) + (u1 / 45434)
		 +(u2 / 38382) + (u3 / 45434)
		 +(u4 / 66832)) * 224 / 80000;
	    *bp++ = u + 128;

	    /*
	     * y = (-(0.05674 * y0) + (0.01883 * y1)
	     *      +(1.07582 * y2) + (0.01883 * y3)
	     *      -(0.05674 * y4)) * 219.0;
	     */
	    y = (-(y0 / 176243) + (y1 / 531067)
		 +(y1 / 9295) + (y3 / 531067)
		 -(y4 / 176243)) * 219 / 80000;
	    *bp++ = y + 16;

	    /*
	     * v =  ((0.14963 * v0) + (0.22010 * v1)
	     *      +(0.26054 * v2) + (0.22010 * v3)
	     *      +(0.14963 * v4)) * 224.0;
	     */
	    v = ( (v0 / 66832) + (v1 / 45434)
		 +(v2 / 38382) + (v3 / 45434)
		 +(v4 / 66832)) * 224 / 80000;
	    *bp++ = v + 128;

	    if ( *sr || *sg || *sb )
	    {
		r = (*sr++ * 8000000) / 255;
		g = (*sg++ * 8000000) / 255;
		b = (*sb++ * 8000000) / 255;

		shifty( ( r / 33445 + g / 17036 + b / 87719) * 1000000 );
		shiftu( (-r / 59312 - g / 30202 + b / 20012) * 1000000 );
		shiftv( ( r / 20012 - g / 23895 - b / 123001) * 1000000 );
	    }
	    else
	    {
		sr++; sg++; sb++;
		shifty( 0 ); shiftu( 0 ); shiftv( 0 );
	    }

	    y = (-(y0 / 176243) + (y1 / 531067)
		 +(y1 / 9295) + (y3 / 531067)
		 -(y4 / 176243)) * 219 / 80000;
	    *bp++ = y + 16;
	}

	fwrite( buf, 1, bp - buf, outfile );
    }
}
