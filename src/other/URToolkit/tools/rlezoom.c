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
 * rlezoom.c - Zoom an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Feb 26 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

#define ROUND(x) ((int)((x) + 0.5))

static void integer_zoom(), expand_raw(), float_zoom(), build_row();
static int advance_fp();

/*****************************************************************
 * TAG( main )
 * 
 * Zoom an RLE file by an integral factor.
 *
 * Usage:
 * 	rlezoom [-f] factor [y-factor] [-o outfile] [rlefile]
 * Inputs:
 * 	-f:		If specified, output image will have same
 * 			dimensions as
 * 				fant -p 0 0 -s factor y-factor
 * 			would produce.
 * 	factor:		Factor to zoom by.  Must be positive.
 * 	y-factor:	If provided, Y axis will be zoomed by this
 * 			factor, X axis by 'factor'.
 * 	rlefile:	Input RLE file.  Default is stdin.
 * Outputs:
 * 	Writes magnified RLE file to standard output or file specified by
 *      the -o flag.
 * Assumptions:
 * 	...
 * Algorithm:
 *      Read input file in raw mode.  For each line, expand it by the
 *      X expansion factor.  If the factor is > 3, convert pixel data
 *      into runs.  Write each line a number of times equal to the Y
 *      factor. 
 */
void
main( argc, argv )
int argc;
char **argv;
{
    float xfact, yfact = 0;
    rle_hdr 	in_hdr, out_hdr;
    char * rlename = NULL, * out_fname = NULL;
    FILE *outfile = stdout;
    int oflag = 0, fflag = 0;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
		   "% f%- o%-outfile!s factor!f y-factor%f rlefile%s",
		   &fflag, &oflag, &out_fname, &xfact, &yfact, &rlename )
	 == 0 )
	exit( 1 );

    /* Error check */
    if ( xfact <= 0 )
    {
	fprintf( stderr, "%s: Zoom factor (%g) must be >= 0\n",
		 cmd_name( argv ), xfact );
	exit( 1 );
    }
    if ( yfact < 0 )
    {
	fprintf( stderr, "%s: Y zoom factor (%g) must be >= 0\n",
		 cmd_name( argv ), yfact );
	exit( 1 );
    }

    /* If yfact is 0, it wasn't specified, set it to xfact */
    if ( yfact == 0 )
	yfact = xfact;

    /* Open input rle file and read header */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), rlename, "r");
    rle_names( &in_hdr, cmd_name( argv ), rlename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
    
	/* Figure out output file size and parameters */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/* more error checks */
	if ( (float)(out_hdr.xmax + 1) * xfact > 32767 )
	{
	    fprintf( stderr,
		     "%s: X zoom factor (%g) makes image too large (%g)\n",
		     cmd_name( argv ), xfact,
		     (float)(out_hdr.xmax + 1) * xfact );
	    exit( 1 );
	}
	if ( (float)(out_hdr.ymax + 1) * yfact > 32767 )
	{
	    fprintf( stderr,
		     "%s: Y zoom factor (%g) makes image too large (%g)\n",
		     cmd_name( argv ), yfact,
		     (float)(out_hdr.ymax + 1) * (float)yfact );
	    exit( 1 );
	}
	if ( fflag )
	{
	    /* Use 'fant' arithmetic. */
	    out_hdr.xmin = ROUND( in_hdr.xmin * xfact );
	    out_hdr.ymin = ROUND( in_hdr.ymin * yfact );
	    out_hdr.xmax = ROUND( in_hdr.xmax * xfact );
	    out_hdr.ymax = ROUND( in_hdr.ymax * xfact );
	}
	else
	{
	    out_hdr.xmin = ROUND( xfact * in_hdr.xmin );
	    out_hdr.xmax = ROUND( xfact * (in_hdr.xmax - in_hdr.xmin + 1) ) +
		out_hdr.xmin - 1;
	    out_hdr.ymin = ROUND( yfact * in_hdr.ymin );
	    out_hdr.ymax = ROUND( yfact * (in_hdr.ymax - in_hdr.ymin + 1) ) +
		out_hdr.ymin - 1;
	}
	rle_put_setup( &out_hdr );

	if ( xfact == (float)(int)xfact && yfact == (float)(int)yfact )
	    integer_zoom( &in_hdr, (int)xfact, (int)yfact, &out_hdr );
	else
	    float_zoom( &in_hdr, xfact, yfact, &out_hdr );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), rlename );

    exit( 0 );
}

/*****************************************************************
 * TAG( integer_zoom )
 * 
 * Zoom the input image by an integer factor.
 * Inputs:
 * 	in_hdr:		Input image header.
 * 	xfact, yfact:	X and Y expansion factors.
 * Outputs:
 * 	out_hdr:	Output image header.
 * Assumptions:
 * 	xfact, yfact > 0.
 * Algorithm:
 * 	Read input file in "raw" form, expand pixels and runs, write
 * 	output file in "raw" form.  If xfact >= 4, each input pixel
 * 	expands to an output run.
 */
static void
integer_zoom( in_hdr, xfact, yfact, out_hdr )
rle_hdr *in_hdr;
int xfact;
int yfact;
rle_hdr *out_hdr;
{
    int y, ynext, i;
    rle_op ** in_raw, ** out_raw;
    int * in_nraw, * out_nraw;
    
    /* Create raw arrays for input and output files */
    if ( rle_raw_alloc( in_hdr, &in_raw, &in_nraw ) < 0 ||
	 rle_raw_alloc( out_hdr, &out_raw, &out_nraw ) < 0 )
	RLE_CHECK_ALLOC( in_hdr->cmd, 0, 0 );
    
    y = in_hdr->ymin;
    while ( (ynext = rle_getraw( in_hdr, in_raw, in_nraw )) != 32768 )
    {
	if ( ynext - y > 1 )
	    rle_skiprow( out_hdr, yfact * (ynext - y) );
	expand_raw( in_hdr, in_raw, in_nraw, xfact, out_raw, out_nraw );
	for ( i = 0; i < yfact; i++ )
	    rle_putraw( out_raw, out_nraw, out_hdr );
	rle_freeraw( in_hdr, in_raw, in_nraw );
	rle_freeraw( out_hdr, out_raw, out_nraw );
	y = ynext;
    }
    rle_puteof( out_hdr );
    
    rle_raw_free( in_hdr, in_raw, in_nraw );
    rle_raw_free( out_hdr, out_raw, out_nraw );
}


/*****************************************************************
 * TAG( expand_raw )
 * 
 * Zoom the input scanline by an integer factor.
 * Inputs:
 * 	the_hdr:		Used for ncolors and alpha channel indication.
 *	in_raw:		Input raw opcodes.
 * 	in_nraw:	Input raw opcode counts.
 *	xfact:		Magnification factor.
 * Outputs:
 * 	out_raw:	Output raw opcodes.
 *	out_nraw:	Output counts.
 * Assumptions:
 * 	
 * Algorithm:
 * 	Replicate pixels by xfact.  If xfact > 3, turn pixel data into
 * 	runs.
 */
static void
expand_raw( the_hdr, in_raw, in_nraw, xfact, out_raw, out_nraw )
rle_hdr * the_hdr;
rle_op **in_raw;
int xfact;
int *in_nraw;
rle_op **out_raw;
int *out_nraw;
{
    register rle_op * inp, * outp;
    register rle_pixel * inc, * outc;
    int chan, i, j, k;

    for ( chan = -the_hdr->alpha; chan < the_hdr->ncolors; chan++ )
    {
	for ( inp = in_raw[chan], outp = out_raw[chan], i = in_nraw[chan];
	      i > 0; i--, inp++ )
	{
	    *outp = *inp;		/* copy, then modify */
	    outp->xloc *= xfact;
	    if ( inp->opcode == RRunDataOp )
	    {
		/* Just change length */
		outp->length *= xfact;
		outp++;
	    }
	    else		/* must be byte data */
		if ( xfact < 4 )
		{
		    /* Be cheap, replicate pixel data for small zooms */
		    outp->length *= xfact;
		    outp->u.pixels =
			(rle_pixel *)malloc( outp->length * sizeof(rle_pixel));
		    RLE_CHECK_ALLOC( the_hdr->cmd, outp->u.pixels, 0 );
		    for ( inc = inp->u.pixels, outc = outp->u.pixels,
			  j = inp->length;
			  j > 0; j--, inc++ )
			for ( k = 0; k < xfact; k++ )
			    *outc++ = *inc;
		    outp++;
		}
		else
		{
		    /*
		     * Change pixels to runs, coalesce adjacent
		     * identical pixels
		     */
		    for ( inc = inp->u.pixels, j = 0;
			  j < inp->length; j++, inc++ )
			if ( j > 0 && outp[-1].u.run_val == *inc )
			    outp[-1].length += xfact;
			else
			{
			    outp->opcode = RRunDataOp;
			    outp->xloc = (inp->xloc + j) * xfact;
			    outp->length = xfact;
			    outp->u.run_val = *inc;
			    outp++;
			}
		}
	}
	/* Remember how many in this row */
	out_nraw[chan] = outp - out_raw[chan];
    }
}


/*****************************************************************
 * TAG( float_zoom )
 * 
 * Expand (or contract) input image by floating point amount.
 * Inputs:
 * 	in_hdr:		Header for input image.
 * 	xfact:		X scaling factor.
 * 	yfact:		Y scaling factor.
 * Outputs:
 * 	out_hdr:	Header for output image.
 * Assumptions:
 * 	xfact, yfact > 0.
 * Algorithm:
 * 	Simple sub/super-sampling based on DDA line drawing algorithm.
 */
static void
float_zoom( in_hdr, xfact, yfact, out_hdr )
rle_hdr *in_hdr;
float xfact;
float yfact;
rle_hdr *out_hdr;
{
    rle_pixel **in_scan, **out_scan;
    rle_pixel **out_rows;
    int curr_row, in_row;
    int wid, i;

    if (rle_row_alloc( in_hdr, &in_scan ) < 0 || 
	rle_row_alloc( out_hdr, &out_scan ) < 0 )
	RLE_CHECK_ALLOC( in_hdr->cmd, 0, 0 );

    /* Set up pointers for rle_putrow to use.  The problem is that
     * rle_putrow expects pointers to the [xmin] element of each
     * scanline, while rle_getrow expects pointers to the [0] element
     * of each scanline.
     */
    out_rows = (rle_pixel **)calloc( (out_hdr->ncolors + out_hdr->alpha),
				     sizeof(rle_pixel *) );
    RLE_CHECK_ALLOC( in_hdr->cmd, out_rows, 0 );
    out_rows += out_hdr->alpha;
    for ( i = -out_hdr->alpha; i < out_hdr->ncolors; i++ )
	out_rows[i] = out_hdr->xmin + out_scan[i];

    wid = out_hdr->xmax - out_hdr->xmin + 1;

    curr_row = -1;		/* Current input row (in in_scan). */
    for ( i = out_hdr->ymin; i <= out_hdr->ymax; i++ )
    {
	in_row = (int)(i / yfact);
	if ( curr_row < in_row )
	{
	    curr_row = advance_fp( in_hdr, in_row, in_scan );
	    build_row( out_hdr, xfact, in_scan, out_scan );
	}
	rle_putrow( out_rows, wid, out_hdr );
    }
    rle_puteof( out_hdr );

    /* If any input left, skip over it. */
    while (rle_getskip( in_hdr ) < 32768 )
	;

    rle_row_free( in_hdr, in_scan );
    rle_row_free( out_hdr, out_scan );
    out_rows -= out_hdr->alpha;
    free( out_rows );
}

/*****************************************************************
 * TAG( advance_fp )
 * 
 * Advance the input to the specified row.
 * Inputs:
 * 	hdr:		Input image header.
 * 	des_row:	The desired row.
 * Outputs:
 * 	in_scan:	Input scanline data array.
 * 	Returns scanline number of last row read.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Read scanlines until des_row is reached; return scanline
 * 	number of row last read (should = des_row).
 */
static int
advance_fp( hdr, des_row, in_scan )
rle_hdr *hdr;
int des_row;
rle_pixel **in_scan;
{
    int row;

    while ( (row = rle_getrow( hdr, in_scan )) < des_row )
	;
    return row;
}

/*****************************************************************
 * TAG( build_row )
 * 
 * Build zoomed output scanline from an input scanline.
 * Inputs:
 * 	hdr:		Output image header.
 * 	fact:		Scaling factor.
 * 	in_scan:	Input scanline data.
 * Outputs:
 * 	out_scan:	Output scanline data.
 * Assumptions:
 *	Input scanline data runs from hdr->xmin/fact to hdr->xmax/fact.
 * Algorithm:
 *	For each output pixel, determine the input pixel it comes from
 *	by a trivial mapping.
 */
static void
build_row( hdr, fact, in_scan, out_scan )
rle_hdr *hdr;
float fact;
rle_pixel **in_scan;
rle_pixel **out_scan;
{
    register rle_pixel *is, *os;
    register int i;
    int c;
    int m = hdr->xmax;

    for ( c = -hdr->alpha; c < hdr->ncolors; c++ )
    {
	is = in_scan[c];
	os = out_scan[c];
	for ( i = hdr->xmin; i <= m; i++ )
	    os[i] = is[(int)(i / fact)];
    }
}
