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
 * rletopaint.c - Convert an RLE file to macpaint format, using dithering.
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Oct 16 1986
 * Copyright (c) 1986, John W. Peterson
 *
 * Byte compression code (compress_line) by Jim Schimpf.
 *
 * Usage is:
 *   rle2paint [-l] [-i] [-g [gamma]] [-o outfile.paint] [infile.rle] 
 *
 * -l		Uses a linear color map in converting the RLE image
 * -r		Invert the bits in the MacPaint output.
 * -g [gamma]	Use a gamma map of [gamma] in converting the output
 *		(default is 2.0)
 * infile.rle	If specified, the input RLE file.  Default stdin.
 * -o outfile	If specified, the MacPaint file will be written here.
 *		Default stdout.
 *
 * Note that the image should be flipped vertically (by rleflip, e.g.),
 * as the two formats number their scanlines oppositely.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "rle.h"

#ifndef MIN
#define MIN(x,y) ( (x) < (y) ? (x) : (y) )
#endif

/* Scanline storage & RLE row pointers */
rle_pixel ** rows;

int gammamap[256];
unsigned char rawbits[73];	/* Uncompressed MacPaint pixels */
unsigned char squishedbits[74]; /* Compressed data */
int inverse_flag = 0;		/* If true then invert image */

/* Gray scale dither table */
int dmgray[8][8] = {   0 ,128 , 32 ,160 ,  8 ,136 , 40 ,168 ,
                 192 , 64 ,224 , 96 ,200 , 72 ,232 ,104 ,
                  48 ,176 , 16 ,144 , 56 ,184 , 24 ,152 ,
                 240 ,112 ,208 , 80 ,248 ,120 ,216 , 88 ,
                  12 ,140 , 44 ,172 ,  4 ,132 , 36 ,164 ,
                 204 , 76 ,236 ,108 ,196 , 68 ,228 ,100 ,
                  60 ,188 , 28 ,156 , 52 ,220 , 20 ,148 ,
                 252 ,124 ,220 , 92 ,244 ,116 ,212 ,84   } ;

int compress_line();
void write_paint_line(), bytes_to_bits();

void
main(argc, argv)
int argc;
char *argv[];
{
    int i;
    int databytes;
    float gam = 2.0;		/* Default gamma */
    int gamma_flag = 0, oflag = 0;
    int linear_flag = 1;
    char * fname = NULL, * out_fname = NULL;
    FILE * out_fp;

    if ( scanargs( argc, argv, "% r%- l%- g%-gamma%f o%-outfile!s infile%s",
		  &inverse_flag, &linear_flag,
		  &gamma_flag, &gam, &oflag, &out_fname, &fname ) == 0)
	exit(-1);

    if (gamma_flag) linear_flag = 0;
    rle_dflt_hdr.rle_file = rle_open_f("rletopaint", fname, "r");
    rle_get_setup( &rle_dflt_hdr );

    out_fp = rle_open_f("rletopaint", out_fname, "w");
    
    /* set up rows to point to our copy of the scanline */
    if (rle_row_alloc( &rle_dflt_hdr, &rows ))
    {
	fprintf(stderr,"rletopaint: malloc failed\n");
	exit(-2);
    }

    /* Compute a gamma correction map and tables */
    for ( i = 0; i < 256; i++ )
    {
	if (linear_flag)
	    gammamap[i] = i;
	else
	    gammamap[i] = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gam ));
    }

    /* Output MacPaint header */
    for (i = 0; i < 512; i++)
	putc('\0', out_fp);	 /* Null tells MacPaint to ignore header */

    for (i = 0; i < 72; i++)	/* Kludge - pad bottom with blank lines */
	rawbits[i] = 0;

    /* If the picture doesn't start at the bottom, output blank lines
     * to fill the space */

    databytes = compress_line(); 	/* Blank compressed data */
    for (i = 0; i < rle_dflt_hdr.ymin; i++)
	write_paint_line(databytes, out_fp);

    for (i = rle_dflt_hdr.ymin; i <= MIN( rle_dflt_hdr.ymax, 719 ); i++)
    {
	rle_getrow( &rle_dflt_hdr, rows );
        bytes_to_bits( i );
	write_paint_line(compress_line(), out_fp);
    }

    /* Finish up unused picture space */
    if (rle_dflt_hdr.ymax < 719)
    {
	for (i = 0; i < 72; i++) /* Kludge - pad bottom with blank lines */
	rawbits[i] = 0;
	
	databytes = compress_line(); 	/* Blank compressed data */
	for (i = rle_dflt_hdr.ymax+1; i < 720; i++)
	    write_paint_line(databytes, out_fp);
    }
}
	
    
/* Write a scanline to the macpaint file. */
void
write_paint_line(num, fp)
int num;
FILE *fp;
{
    int j;
    for (j = 0; j < num; j++)
	putc(squishedbits[j], fp);
}

/*
 * Map a 24 bit scanline through to get dithered black and white, generating
 * a row of bits in rawbits.
 */
void
bytes_to_bits( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, col, row;
    int pixel;
    int n = MIN(rle_dflt_hdr.xmax, 576);
    int B = 0, W = 1;

    if (inverse_flag)
    {
	B = 1; W = 0;		/* Swap meaning of Black and White */
    }
    for (i = 0; i < 72; i++)
	rawbits[i] = 0;

    for (i = 2; i >= rle_dflt_hdr.ncolors; i--)
	bcopy(rows[0], rows[i], rle_dflt_hdr.xmax - rle_dflt_hdr.xmin);

    for ( row = y % 8, col = 0, i = 0, r = rows[0], g = rows[1], b = rows[2];
	 i < n; i++, r++, g++, b++, col = ((col + 1) & 7) )
    {
	/* Conver to BW (uses YIQ/percentage xformation) */
	pixel = (35*gammamap[*r] + 55*gammamap[*g] + 10*gammamap[*b]) / 100;
	if (pixel < 0) pixel += 256;

	/* Use dither to convert to zero/one */
	pixel = ((pixel > dmgray[col][row]) ? W : B);

	/* Compress scanline bytes to unencoded macpaint bits */
	rawbits[(i / 8)] |= pixel << (7-(i % 8));
    }
}
    
/*
 * Compress the bits in rawbits to run-length encoded bytes in squishedbits.
 *
 * Compression is as follows:
 *
 * <CNT><Byte><Byte>...    If <CNT>  < 0x80  i.e. <CNT> different bytes
 * <CNT><Byte>             If <CNT>  > 0x80  i.e. <CNT> repeated bytes
 *
 */

int
compress_line()
{
    int i,j,k,cntpsn,count;
    int flag;
    unsigned char pixel;
#define SAME 1
#define NOTSAME 0
                      
    i = 0;
    j = 2;
    if( rawbits[0] == rawbits[1] )
    {
	flag = SAME;
	cntpsn = 0;
	squishedbits[1] = rawbits[0];
    }
    else
    {
	flag = NOTSAME;
	cntpsn = 0;
	squishedbits[1] = rawbits[0];
    }

    while( i < 72 )
    {
	switch( flag )
	{
                                
	    /*  Same case see how far the run goes then update stuff */

	case SAME:
	    count = 0;
	    for( k=i; k<72; k++ )
	    {
		if( squishedbits[j-1] != rawbits[k] )
		    break;
		else
		    count++;
	    }

	    /* If count is zero then just skip all this stuff and try again
	     * with the flag the other way
	     */
	    if( count != 0 )
	    {

		/* Ok update the count and save the byte in the output line
		 * NOTE: Count is just 2's compliment of the value
		 */
		pixel = -1 * (count-1);
		squishedbits[cntpsn] = 0xff & pixel;

		/*  Set the flag for the other & advance j to next frame */
                       
		flag = NOTSAME;
	    }
	    else
		flag = NOTSAME;
	    break;
                                
	    /*  Not the same, look for a run of something if found quit */

	case NOTSAME:
	    count = 0;
	    for( k=i+1; k<72; k++ )
	    {
		if( squishedbits[j-1] == rawbits[k] )
		    break;
		else
		{
		    count++;
		    squishedbits[j++] = rawbits[k];
		}
	    }
	    /* If count is zero then skip all the updating stuff and just try
	     * the other method
	     */
	    if( count != 0 )
	    {
		if ( k == 72 )
		    squishedbits[cntpsn] = count;
		else
		{
		    squishedbits[cntpsn] = count - 1;

		    /* Set the flag for the other and back up the psn
		     * to get the start of the run.
		     */
		    
		    k = k - 1;
		    j--;
		    flag = SAME;
		}
	    }
	    else
		flag = SAME;
	    break;
	}


	/* End of loop update the positions of the count save lcn and
	 * next character to look at 
	 *   
	 * Only do update on non zero counts 
	 */
                                             
	if( count != 0 )
	{
	    /* Sometimes 'compression' doesn't.  Check for this. */
	    if ( j > k + 1 )
	    {
		squishedbits[0] = k - 1;
		for ( j = 0; j < k; j++ )
		    squishedbits[j + 1] = rawbits[j];
		j++;
	    }
	    cntpsn = j;
	    squishedbits[++j] = rawbits[k];
	    j++;
	    i = k;
	}
    }

    return( j-2 );
}
