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
 * rlecomp.c - Digitial image compositor (The Poor Man's Pixar)
 * 
 * Author:	Rod Bogart and John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Feb 25 1986
 * Copyright (c) 1986 Rod Bogart and John W. Peterson
 * 
 *    For an explanation of the compositing algorithms, see 
 *    "Compositing Digital Images", by Porter and Duff,
 *    SIGGRAPH 84, p.255.
 */

#include "common.h"



#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "rle.h"
#include "rle_code.h"
#include "rle_raw.h"

#ifndef MAX
#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )
#endif
#ifndef MIN
#define MIN(i,j)   ( (i) < (j) ? (i) : (j) )
#endif
#define MALLOC_ERR {fprintf(stderr, "%s: ran out of heap space\n", \
			    cmd_name(argv));exit(-2);}

#define NUM_OPS		11

#define CLEAR_OP 	0
#define OVER_OP		1	/* Operations */
#define IN_OP		2	/* (these must match the comp_ops table) */
#define OUT_OP		3
#define ATOP_OP		4
#define XOR_OP		5
#define PLUS_OP		6
#define MINUS_OP	7
#define DIFF_OP		8
#define ADD_OP		9
#define SUBTRACT_OP	10

#define IN_WINDOW(y,wind) ((y >= wind.ymin) && (y <= wind.ymax))

void get_scanline(rle_hdr *the_hdr, rle_pixel **scanline, int *num_skip, rle_op **tmp_raw, int *tmp_nraw);
void copy_scanline(rle_hdr *in_hdr, rle_hdr *out_hdr, int ypos, int *num_skip, rle_op **out_raw, int *out_nraw, int blank_output);

/* 
 * Global raw data structures for copy_scanline.
 */
rle_op ** Araw, **Braw;
int * Anraw, *Bnraw;
rle_pixel * non_zero_pixels;

int
main(int argc, char **argv)
{
    FILE	*outfile = stdout;
    int         i, j, k, temp;
    char	*out_fname = NULL;
    int		xlen;
    rle_hdr A_hdr, B_hdr;
    rle_hdr out_hdr;
    rle_pixel	**Ascanline, **Bscanline;
    rle_pixel	**rows;
    rle_pixel	**scanout;
    register
      rle_pixel	*Ascan, *Bscan, *scan;
    register
      rle_pixel	Aalph, Balph;
    int		int_result = 0;
    int 	oflag = 0, op_code;
    char 	*Afilename = NULL, *Bfilename = NULL, *op_name = NULL;
    int 	Askip, Bskip;	/* Blank space counters for copy_scanline */
    int		rle_cnt, rle_err;
    char	*err_fname;
      
    static const char *comp_ops[NUM_OPS] =
	 { "clear",
	   "over",
	   "in",
	   "out",
	   "atop",
	   "xor",
	   "plus",
	   "minus",
	   "diff",
	   "add",
	   "subtract"};

    /*
     * Parse arguments and set up input and output files.
     *   op : select operator other than OV
     */

    if (scanargs(argc, argv, "% o%-outfile!s Afile!s op!s Bfile!s",
                 &oflag, &out_fname, &Afilename, &op_name, &Bfilename ) == 0)
    {
	exit(-1);
    }

    op_code = -1;
    for (i=0; i < NUM_OPS; i++)
	if (strcmp(op_name, comp_ops[i]) == 0) op_code = i;
    
    if (op_code == -1)
    {
	fprintf(stderr, "%s: Invalid compositor operation\n", op_name);
	exit(-2);
    }
    
    A_hdr = rle_dflt_hdr; B_hdr = rle_dflt_hdr;
    A_hdr.rle_file = rle_open_f(cmd_name( argv ), Afilename, "r");
    B_hdr.rle_file = rle_open_f(cmd_name( argv ), Bfilename, "r");
    if (A_hdr.rle_file == stdin && B_hdr.rle_file == stdin)
       fprintf(stderr, "Can't read both inputs from stdin!\n");

    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	if ( (rle_err = rle_get_setup( &A_hdr )) != RLE_SUCCESS )
	{
	    err_fname = Afilename;
	    break;
	}
	if ( (rle_err = rle_get_setup( &B_hdr )) != RLE_SUCCESS )
	{
	    err_fname = Bfilename;
	    break;
	}

	out_hdr = A_hdr;
	out_hdr.xmin = MIN( A_hdr.xmin, B_hdr.xmin );
	out_hdr.ymin = MIN( A_hdr.ymin, B_hdr.ymin );
	out_hdr.xmax = MAX( A_hdr.xmax, B_hdr.xmax );
	out_hdr.ymax = MAX( A_hdr.ymax, B_hdr.ymax );

	out_hdr.alpha = 1;
	out_hdr.ncolors = MAX(A_hdr.ncolors, B_hdr.ncolors);
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &A_hdr, &out_hdr );

	xlen = out_hdr.xmax - out_hdr.xmin + 1;

	/* Enable all channels in output file */

	for (i = -out_hdr.alpha; i < out_hdr.ncolors; i++)
	    RLE_SET_BIT( out_hdr, i );

	rle_put_setup( &out_hdr );

	/*
	 * Allocate row storage
	 */

	if (rle_row_alloc( &out_hdr, &Ascanline ))
	    MALLOC_ERR;
	bzero( (char *)Ascanline[RLE_ALPHA], out_hdr.xmax + 1 );

	if (rle_row_alloc( &out_hdr, &Bscanline ))
	    MALLOC_ERR;
	bzero( (char *)Bscanline[RLE_ALPHA], out_hdr.xmax + 1 );

	if (rle_row_alloc( &out_hdr, &scanout ))
	    MALLOC_ERR;

	if (!(rows = (rle_pixel **) malloc(sizeof(rle_pixel *)
					   * out_hdr.ncolors+out_hdr.alpha)))
	    MALLOC_ERR;

	/*
	 * Allocate raw storage
	 */
	if (rle_raw_alloc( &out_hdr, &Araw, &Anraw ))
	    MALLOC_ERR;

	if (rle_raw_alloc( &out_hdr, &Braw, &Bnraw ))
	    MALLOC_ERR;

	if (!(non_zero_pixels =
	      (rle_pixel *)malloc( xlen * sizeof( rle_pixel ))))
	    MALLOC_ERR;

	Askip = 0;		/* Initialize counters for copy_scanline */
	Bskip = 0;

	/*
	 * Loop through all (possible) scanlines in the output file,
	 * compositing each one.
	 */

	for ( j = out_hdr.ymin; j <= out_hdr.ymax ; j++)
	{
	    /* 
	     * Special case - if this scanline is in picture A but not
	     * B, don't do the compositing arithmaticly - just copy (or
	     * don't copy) the picture information, depending on the
	     * operation.
	     */

	    if (IN_WINDOW(j, A_hdr) && !IN_WINDOW(j, B_hdr))
	    {
		switch (op_code) {
		case OVER_OP:
		case OUT_OP:
		case XOR_OP:
		case PLUS_OP:
		case MINUS_OP:
		case DIFF_OP:
		case SUBTRACT_OP:
		case ADD_OP:
		    /**** Read the A channel and dump it ****/
		    copy_scanline( &A_hdr, &out_hdr, j,
				   &Askip, Araw, Anraw, 0 );
		    break;

		case ATOP_OP:
		case IN_OP:
		    /* Read the A channel, but dump one blank scanline */
		    copy_scanline( &A_hdr, &out_hdr, j,
				   &Askip, Araw, Anraw, 1 );
		    break;
		}
	    }
	    else
		if ((!IN_WINDOW(j, A_hdr)) && IN_WINDOW(j, B_hdr))

		    /* As above - special case */

		{
		    switch (op_code) {
		    case OVER_OP:
		    case ATOP_OP:
		    case XOR_OP:
		    case PLUS_OP:
		    case MINUS_OP:
		    case DIFF_OP:
		    case SUBTRACT_OP:
		    case ADD_OP:
			/**** Read the B channel and dump it ****/
			copy_scanline( &B_hdr, &out_hdr, j,
				       &Bskip, Braw, Bnraw, 0 );
			break;

		    case OUT_OP:
		    case IN_OP:
			/* Read the B channel, but dump one blank scanline */
			copy_scanline( &B_hdr, &out_hdr, j,
				       &Bskip, Braw, Bnraw, 1 );
			break;
		    }
		}
		else if (!IN_WINDOW(j, A_hdr) && !IN_WINDOW(j, B_hdr))
		{
		    rle_skiprow( &out_hdr, 1 );
		}
		else
		{
		    /**** Read the A channel  ****/
		    get_scanline( &A_hdr, Ascanline, &Askip, Araw, Anraw );

		    /**** Read the B channel  ****/
		    get_scanline( &B_hdr, Bscanline, &Bskip, Braw, Bnraw );

		    /* For each channel... */
		    for( k = RLE_ALPHA; k < out_hdr.ncolors; k++)
		    {
			Ascan = &Ascanline[k][out_hdr.xmin];
			Bscan = &Bscanline[k][out_hdr.xmin];
			scan = &scanout[k][out_hdr.xmin];

			for( i = out_hdr.xmin; i <= out_hdr.xmax;
			     i++, Ascan++, Bscan++, scan++)
			{
			    Aalph = Ascanline[RLE_ALPHA][i];
			    Balph = Bscanline[RLE_ALPHA][i];

			    switch (op_code) {

				/* Note OVER has been optimized for special cases */
			    case OVER_OP:	/* cA * 1.0 + cB * (1-alphaA) */
				if (Aalph == 0)
				    int_result = *Bscan;
				else if (Aalph == 255)
				    int_result = *Ascan;
				else
				    int_result = ( (int)*Ascan * 255 +
						   (int)*Bscan * (255 - (int)Aalph))/255;
				break;

			    case IN_OP:	/* cA * alphaB + cB * 0.0 */
				int_result = ( (int)*Ascan * (int)Balph ) /255;
				break;

			    case OUT_OP:	/* cA * (1-alphaB) + cB * 0.0 */
				int_result = ( (int)*Ascan * (255 - (int)Balph) ) /255;
				break;

			    case ATOP_OP:	/* cA * alphaB + cB * (1-alphaA) */
				int_result = ( (int)*Ascan * (int)Balph +
					       (int)*Bscan * (255 - (int)Aalph) )/255;
				break;

			    case XOR_OP:	/* cA * (1-alphaB) + cB * (1-alphaA) */
				int_result = ((int)*Ascan * (255 - (int)Balph) + (int)*Bscan *
					      (255 - (int)Aalph) )/255;
				break;

			    case PLUS_OP:
				int_result = ((temp = ((int)*Ascan + (int)*Bscan))
					      > 255) ? 255 : temp;
				break;

				/* minus is intended for subtracting images only, so
				 * the alpha channel is explicitly set to 255.
				 */
			    case MINUS_OP:
				if (k == RLE_ALPHA)
				    int_result = 255;
				else
				    int_result = ((temp = ((int)*Ascan - (int)*Bscan))
						  < 0) ? 0 : temp;
				break;

			    case ADD_OP:
				int_result = ((temp = ((int)*Ascan + (int)*Bscan))
					      > 255) ? temp - 256: temp;
				break;
			    case SUBTRACT_OP:
				int_result = ((temp = ((int)*Ascan - (int)*Bscan))
					      < 0) ? 256 + temp : temp;
				break;
			    case DIFF_OP:
				int_result = abs((int)*Ascan - (int)*Bscan);
				break;
			    }

			    *scan = (rle_pixel) ((int_result > 255) ? 255 : 
						 ((int_result < 0) ? 0 : int_result));
			}
		    }

		    /* Write out the composited data */

		    for( i = 0; i < out_hdr.ncolors+out_hdr.alpha; i++ )
			rows[i] = &scanout[i-1][out_hdr.xmin];
		    rle_putrow( &rows[1], xlen, &out_hdr );
		}
	}
	rle_puteof( &out_hdr );

	/* Release storage. */
	rle_row_free( &out_hdr, Ascanline );
	rle_row_free( &out_hdr, Bscanline );
	rle_row_free( &out_hdr, scanout );
	free( rows );
	rle_raw_free( &out_hdr, Araw, Anraw );
	rle_raw_free( &out_hdr, Braw, Bnraw );
	free( non_zero_pixels );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), err_fname );


    exit( 0 );
}

/*
 * Read a scanline from an RLE file.  Fake up an alpha channel (from non-
 * background pixels) if alpha isn't present.
 */
void
get_scanline(rle_hdr *the_hdr, rle_pixel **scanline, int *num_skip, rle_op **tmp_raw, int *tmp_nraw)
                  
                     
               
                  		/* Raw pointers for data left behind by */
               			/* copy_scanline. */
{
    int i,j,no_backgr;

    if (*num_skip > 0)		/* Generate a blank (skipped) scanline */
    {
	for( i = RLE_ALPHA; i < the_hdr->ncolors; i++ )
	    bzero( (char *)scanline[i], the_hdr->xmax );
	(*num_skip)--;
	if (*num_skip == 0)
	    *num_skip = -1;	/* Flag raw data available */
	return;
    }

    if (*num_skip < 0)		/* num_skip < 0, use stashed raw data */
    {
	rle_rawtorow( the_hdr, tmp_raw, tmp_nraw, scanline );
	*num_skip = 0;
    }
    else
	rle_getrow(the_hdr, scanline );

    /* If no alpha channel, then fake one up from non-background pixels */

    if (!the_hdr->alpha)
	for( i = the_hdr->xmin; i <= the_hdr->xmax; i++)
	{
	    no_backgr = 0;
	    for( j = 0; j < the_hdr->ncolors; j++)
	    {
		no_backgr = no_backgr || (scanline[j][i] != 0);
	    }
	    if (no_backgr)
	    {
		scanline[RLE_ALPHA][i] = 255;
	    }
	    else
	    {
		scanline[RLE_ALPHA][i] = 0;
	    }
	}
}

/*
 * Read a scanline from an RLE file in raw mode, because we are about to dump
 * it to the outfile.  Fake up an alpha channel (from non-background pixels)
 * if alpha isn't present.
 */
void
copy_scanline(rle_hdr *in_hdr, rle_hdr *out_hdr, int ypos, int *num_skip, rle_op **out_raw, int *out_nraw, int blank_output)
                            
         
              			/* Number of scanlines to be skipped. */
                  
               
                 		/* if non-zero, just eat input & blank output */
{
    register int i,j;
    register rle_pixel * ptr;
    int chan, fakeruns, xlen;

    xlen = in_hdr->xmax - in_hdr->xmin + 1;

    /*
     * If the skip counter == 0, then we read the next line normally.
     * If it's positive, then it tells us how many blank lines before
     * the next available data.  If it's negative, it flags that
     * out_raw contains data to be used.
     */

 SKIP_ROW:
    if (*num_skip > 0)		/* We're in a blank space, output blanks */
    {
	rle_skiprow( out_hdr, 1 );
	(*num_skip)--;
	if (! *num_skip)
	    *num_skip = -1;	/* Flag data available. */
	return;
    }
    
    if (! *num_skip)		/* num_skip == 0, must read data... */
	*num_skip = rle_getraw( in_hdr, out_raw, out_nraw );
    else
	*num_skip = ypos;	/* num_skip < 0, data was already there */

    if (*num_skip == 32768)	/* EOF, just quit. */
    {
	rle_skiprow( out_hdr, 1 );
	return;
    }

    *num_skip -= ypos;
    if ( *num_skip > 0 )
	goto SKIP_ROW;		/* It happens to the best of us... */

    if (!blank_output)
    {
	/*
	 * If no alpha channel, then fake one up from non-background pixels.
	 *
	 * This is not the most intelligent way to do this.  It's
	 * possible to look at the rle_ops directly, and do a set
	 * union of them to produce the fake alpha channel rle_ops.
	 * The algorithm to do this is not obvious to the casual
	 * observer.  If you want to take a crack at it, look at
	 * lib/rle_putrow.c (the findruns routine) for hints.
	 */

	if (! in_hdr->alpha )
	{
	    /*
	     * Create a "bytemask" of the non-zero pixels.
	     */
	    bzero( (char *)non_zero_pixels, xlen );
	    for (chan = 0; chan < in_hdr->ncolors; chan++ )
		for (i = 0; i < out_nraw[chan]; i++)
		    if (out_raw[chan][i].opcode == RByteDataOp ||
			out_raw[chan][i].u.run_val != 0)
		    {
			for (ptr = &(non_zero_pixels[out_raw[chan][i].xloc]),
			     j = out_raw[chan][i].length;
			     j > 0;
			     j--)
			    *(ptr++) = (rle_pixel) 255;
		    }
	    /*
	     * Collect the bytemask into real opcodes.  Assume that this won't
	     * be fragmented into byte data (it doesn't check).
	     */
	    fakeruns = 0;
	    i = 0;
	
	    while ( i < xlen )
	    {
		j = 0;
		out_raw[RLE_ALPHA][fakeruns].opcode = RRunDataOp;
		out_raw[RLE_ALPHA][fakeruns].u.run_val = (rle_pixel) 255;
	    
		while ( (non_zero_pixels[i] == (rle_pixel) 0) && (i < xlen) )
		    i++;
		out_raw[RLE_ALPHA][fakeruns].xloc = i;

		while( (non_zero_pixels[i] != (rle_pixel) 0) && (i < xlen) )
		    j++, i++;
		out_raw[RLE_ALPHA][fakeruns].length = j;
		if (j) fakeruns++;
	    }
	    out_nraw[RLE_ALPHA] = fakeruns;
	}
		
	/* dump the raw stuff to the output file */
	rle_putraw( out_raw, out_nraw, out_hdr );
    }
    else
	rle_skiprow( out_hdr, 1 );

    rle_freeraw( out_hdr, out_raw, out_nraw );
}

