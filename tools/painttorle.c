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
 * paint2rle.c - Convert MacPaint images to RLE.
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Oct 15 1986
 * Copyright (c) 1986, John W. Peterson
 *
 * Usage is:
 *   paint2rle [-c [r] [g] [b] [alpha]] [-r] [-o outfile.rle] [infile.paint] 
 *
 * -r 		Inverts the pixels in the macpaint file
 *  r,g,b,a	Allows the value of the resulting RLE file to be specified.
 *
 * The rleflip is needed because MacPaint images have their origin at the
 * top left instead of the bottom.
 */

#include "conf.h"

#include <stdio.h>

#include <stdlib.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "rle.h"

/* Default color values */
int redval = 255, grnval = 255, bluval = 255, alfval = 255;
unsigned char in_line[72];
rle_pixel **outrows;
int  xlat[256];
int invert_flag = 0, oflag = 0;

void init(), read_scan(), write_scan();

int
main(argc,argv) 
int argc;
char *argv[];
{ 
    char 	       *in_fname = NULL,
    		       *out_fname = NULL;
    int			cflag = 0;
    FILE	       *infile;
    int i;

    if ( scanargs( argc, argv,
		   "% c%-red%dgreen%dblue%dalpha%d r%- o%-outfile!s infile.paint%s",
		   &cflag, &redval, &grnval, &bluval, &alfval, &invert_flag,
		   &oflag, &out_fname, &in_fname ) == 0)
	exit(-1);
  
    infile = rle_open_f( "painttorle", in_fname, "r" );
    rle_dflt_hdr.rle_file = rle_open_f("painttorle", out_fname, "w");

    rle_dflt_hdr.xmax = 575;
    rle_dflt_hdr.ymax = 719;
    rle_dflt_hdr.alpha = 1;
    rle_dflt_hdr.ncolors = 3;
    RLE_SET_BIT( rle_dflt_hdr, RLE_ALPHA );	/* So it gets generated */

    if (rle_row_alloc( &rle_dflt_hdr, &outrows ))
    {
	fprintf(stderr, "painttorle: No heap space\n");
	exit(-2);
    }

    rle_addhist( argv, (rle_hdr *)NULL, &rle_dflt_hdr );
    rle_put_setup( &rle_dflt_hdr );
    /* initialize bit-twiddling tables */
    init();

    /* read and discard 512 byte MacPaint header */
    for (i=0; i<512; i++)
	getc(infile);

    /* Read and process each of the 720 MacPaint scan lines */
    for (i=0; i < 720; i++)
    {
	read_scan( infile );
	write_scan();
    }

    rle_puteof( &rle_dflt_hdr );
    exit(0);
}

/* 
 * Read a line from the MacPaint file, uncompressing the data into in_line 
 */
void
read_scan( infile )
FILE *infile;
{
    int in_pos, count, data_byte;

    in_pos = 0;
    while (in_pos < 72)
    {
	count = getc(infile);
	if (count > 127) count -= 256;

	if (count >= 0) {	/* run of raw bytes */
	    count++;		/* # of bytes to read */
	    while (count--)
		in_line[in_pos++] = getc(infile);
	}
	else {			/* run of repeated byte */
	    count = -count+1;	/* repetition factor */
	    data_byte = getc(infile); /* byte to repeat */

	    while (count--)
		in_line[in_pos++] = data_byte;
	}
    }
}

/*
 * Write out a scanline
 */
void
write_scan()
{
    register int i, j;
    register int bit;
    int outval, outpos;

    /* Convert the array of bits to an array of pixels */

    for (i = 0; i < 72; i++ )
    {
	for (bit = 7; bit >= 0; bit--)
	{
	    outval = xlat[in_line[i] & 0xff] & 0xff;
	    outval = (outval >> bit) & 1; /* Convert to boolean */
	    if (invert_flag) outval = 1-outval;
	    outpos = i*8 + (7-bit);
	    if (outval)
	    {
		outrows[RLE_ALPHA][outpos] = alfval;
		outrows[RLE_RED][outpos]   = redval;
		outrows[RLE_GREEN][outpos] = grnval;
		outrows[RLE_BLUE][outpos]  = bluval;
	    }
	    else
	    {
		for( j = RLE_ALPHA; j <= RLE_BLUE; j++ )
		    outrows[j][outpos] = 0;
	    }
 	}
    }
    rle_putrow( outrows, 576, &rle_dflt_hdr );
}

/* Set up some tables for converting bits to bytes */
void
init() 
{
    int bits[8], i, j;

    /* initialize translation table */
    j = 1;
    for( i=0; i<8; i++ )
    {
	bits[i] = j;
	j *= (1 << 1);
    }

    for( i=0; i<256; i++ )
    {
	if( i &	  1 )	xlat[i] = bits[0];
	else		xlat[i] = 0;

	if( i &   2 )	xlat[i] += bits[1];
	if( i &   4 )	xlat[i] += bits[2];
	if( i &   8 )   xlat[i] += bits[3];
	if( i &  16 )	xlat[i] += bits[4];
	if( i &  32 )	xlat[i] += bits[5];
	if( i &  64 )	xlat[i] += bits[6];
	if( i & 128 )	xlat[i] += bits[7];
    }
}
