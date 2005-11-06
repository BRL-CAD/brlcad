/* 
 * smush.c - Blur an image.  A level 1 smush is just a convolution with
 * a gaussian mask (or given mask).  Higher levels will be more blurry.
 * 
 * Author:	Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Oct 15 1986
 * Copyright (c) 1986, University of Utah
 * 
 */
#ifndef lint
static char rcs_ident[] = "$Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

char *progname;

void
main(argc, argv)
int  argc;
char *argv[];
{
    char * infname = NULL, * outfname = NULL, * maskfname = NULL;
    FILE * outfile = stdout;
    int oflag = 0, mflag = 0, no_norm = 0;
    int xlinewidth, xlen, ylen, x, y;
    int i, j, k, chan, nchan, level, levels = 1;
    int maskskip, origmasksize;
    int maxmaskskip, maxmasksize, maxmaskbelow;
    int masksize, maskbelow, maskabove; /*number of maskels below and above */
					/* the center maskel. */
    float *maskmult;
    float *mask_mult_table;
    rle_hdr 	in_hdr, out_hdr;
    float total, *mask, *gauss_mask(), *read_mask();
    int newvalue;
    rle_pixel *rastptr, *rasterbase, *centerpxl, *calcpxl;
    rle_pixel *outrastptr, *outrasterbase, *outpxl, *tempbase;
    rle_pixel *bot_edge_pxl, *bot_data_pxl, *bot_extrap_pxl;
    rle_pixel *top_edge_pxl, *top_data_pxl, *top_extrap_pxl;
    rle_pixel *left_edge_pxl, *left_data_pxl, *left_extrap_pxl;
    rle_pixel *right_edge_pxl, *right_data_pxl, *right_extrap_pxl;
    rle_pixel **scanout;
    rle_pixel **rows;
    int rle_cnt, rle_err;

    progname = cmd_name( argv );
    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
		   "% m%-maskfile!s n%- o%-outfile!s level%d infile%s",
		   &mflag, &maskfname, &no_norm,
		   &oflag, &outfname, &levels, &infname ) == 0 )
	exit( 1 );

    if (mflag)
    {
	mask = read_mask(&origmasksize, maskfname, no_norm);
    }
    else
    {
	origmasksize = 5;
	mask = gauss_mask(origmasksize);
    }

    in_hdr.rle_file = rle_open_f(progname, infname, "r");
    rle_names( &in_hdr, progname, infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( in_hdr.alpha )
	    RLE_SET_BIT( in_hdr, RLE_ALPHA );
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( progname, outfname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/* initialize mask_mult_table */
	mask_mult_table = (float *) malloc(sizeof(float) * origmasksize * 
					   origmasksize * 256);
	RLE_CHECK_ALLOC( progname, mask_mult_table, 0 );
	for (i=0; i < origmasksize; i++)
	{
	    maskmult = &(mask_mult_table[(i*origmasksize) << 8]);
	    for (j=0; j < origmasksize; j++)
	    {
		for (k=0;k < 256; k++)
		    maskmult[ k ] = (float) k * mask[i*origmasksize+j];
		maskmult += 256;
	    }
	}

	if (levels < 1)
	{
	    fprintf(stderr, "Level must be greater than 0.\n");
	    exit(-1);
	}

	/* initial size of array has a border large enough for the biggest mask */
	maxmaskskip = 1 << (levels - 1);
	maxmasksize = (origmasksize - 1) * maxmaskskip + 1;
	maxmaskbelow = (maxmasksize-1) / 2;
	in_hdr.xmax -= in_hdr.xmin;
	in_hdr.xmin = 0;
	xlen = in_hdr.xmax - in_hdr.xmin + 1;
	ylen = in_hdr.ymax - in_hdr.ymin + 1;
	xlinewidth = xlen + maxmasksize - 1;

	/* assumes that alpha is 1 or 0 */
	nchan = in_hdr.ncolors + in_hdr.alpha;

	/* Note:
	 * When you read in a row of pixels with rle_getrow, it places blank
	 * pixels between 0 and xmin of your buffer.  However, when you
	 * use rle_putrow to write them out, the buffer must be pointing at
	 * where the data actually starts (i.e., at [xmin] of the getrow
	 * buffer.  */

	/* WARNING: Heavy-duty pointer munging ahead */

	rows = (rle_pixel **) malloc( nchan * sizeof( rle_pixel * ) );
	RLE_CHECK_ALLOC( progname, rows, 0 );

	rasterbase = (unsigned char *)malloc(
	    xlinewidth * (ylen + maxmasksize - 1) * nchan );
	outrasterbase = (unsigned char *)malloc(
	    xlinewidth * (ylen + maxmasksize - 1) * nchan );

	RLE_CHECK_ALLOC( progname, rasterbase && outrasterbase, "raster" );
	rastptr = &(rasterbase[maxmaskbelow * xlinewidth * nchan + maxmaskbelow]);

	/****************************************************
	 * Read in all of the pixels
	 ****************************************************/

	for (i = in_hdr.ymin; i <= in_hdr.ymax; i++)
	{
	    for (chan=0; chan < nchan; chan++)
	    {
		rows[chan] = rastptr;
		/* Increment pointer by xlinewidth */
		rastptr = &(rastptr[xlinewidth]);
	    }
	    /* assumes 1 or 0 for alpha */
	    rle_getrow( &in_hdr, &rows[in_hdr.alpha] );
	}

	for(level = 1; level <= levels; level++)
	{
	    /* actual mask is always size by size of data with extra zeros */
	    maskskip = 1 << (level - 1);
	    masksize = (origmasksize - 1) * maskskip + 1;
	    maskbelow = (masksize-1) / 2;
	    maskabove = masksize / 2;

	    /****************************************************
	     * Extrapolate boundary pixels
	     ****************************************************/
	    if ((maskbelow > 0) && (maskabove > 0))
	    {
		rastptr = &(rasterbase[maxmaskbelow +
				       maxmaskbelow * xlinewidth * nchan]);

		for (chan = 0; chan < nchan; chan++)
		{
		    bot_edge_pxl = &(rastptr[chan * xlinewidth]);
		    top_edge_pxl = &(rastptr[(ylen-1) * nchan * xlinewidth
					     + chan * xlinewidth]);
		    for(x=0; x < xlen; x++)
		    {
			for (i=1; i <= maskbelow; i++)
			{
			    bot_data_pxl = bot_edge_pxl + i * xlinewidth * nchan;
			    bot_extrap_pxl = bot_edge_pxl - i * xlinewidth * nchan;
			    newvalue = 2 * (*bot_edge_pxl) - (*bot_data_pxl);
			    *bot_extrap_pxl = (newvalue < 0) ? 0 :
			    ((newvalue > 255) ? 255 : newvalue);
			}
			for (i=1; i <= maskabove; i++)
			{
			    top_data_pxl = top_edge_pxl - i * xlinewidth * nchan;
			    top_extrap_pxl = top_edge_pxl + i * xlinewidth * nchan;
			    newvalue = 2 * (*top_edge_pxl) - (*top_data_pxl);
			    *top_extrap_pxl = (newvalue < 0) ? 0 :
			    ((newvalue > 255) ? 255 : newvalue);
			}
			bot_edge_pxl++;
			top_edge_pxl++;
		    }
		}

		left_edge_pxl = &(rastptr[(-maskbelow) * nchan * xlinewidth]);
		right_edge_pxl = &(rastptr[(-maskbelow) * nchan * xlinewidth
					   + xlinewidth - masksize]);
		for (chan = 0; chan < nchan; chan++)
		{
		    for(y=0; y < ylen + masksize - 1; y++)
		    {
			for (i=1; i <= maskbelow; i++)
			{
			    left_data_pxl = left_edge_pxl + i;
			    left_extrap_pxl = left_edge_pxl - i;
			    newvalue = 2 * (*left_edge_pxl) - (*left_data_pxl);
			    *left_extrap_pxl = (newvalue < 0) ? 0 :
			    ((newvalue > 255) ? 255 : newvalue);
			}
			for (i=1; i <= maskabove; i++)
			{
			    right_data_pxl = right_edge_pxl - i;
			    right_extrap_pxl = right_edge_pxl + i;
			    newvalue = 2 * (*right_edge_pxl) - (*right_data_pxl);
			    *right_extrap_pxl = (newvalue < 0) ? 0 :
			    ((newvalue > 255) ? 255 : newvalue);
			}
			left_edge_pxl += xlinewidth;
			right_edge_pxl += xlinewidth;
		    }
		}
	    }

	    for (y = 0; y < ylen; y++)
	    {
		rastptr = &(rasterbase[maxmaskbelow + (y+maxmaskbelow) *
				       xlinewidth * nchan]);
		outrastptr = &(outrasterbase[maxmaskbelow + (y+maxmaskbelow) *
					     xlinewidth * nchan]);

		for (chan = 0; chan < nchan; chan++)
		{
		    centerpxl = &(rastptr[chan * xlinewidth]);
		    outpxl = &(outrastptr[chan * xlinewidth]);
		    for(x=0; x < xlen; x++)
		    {
			total = 0.0;
			for (i=0; i < origmasksize; i++)
			{
			    calcpxl = centerpxl + (i * maskskip - maskbelow)
				* xlinewidth * nchan - maskbelow;
			    maskmult = &(mask_mult_table[(i*origmasksize) << 8]);

			    for (j=0; j < origmasksize; j++)
			    {
				/* look up multiply in maskmult table */
				total += maskmult[ calcpxl[j*maskskip] ];
				maskmult += 256;
			    }
			}

			/* bound result to 0 to 255 */
			*outpxl = (total < 0.0) ? 0 :
			((total > 255.0) ? 255 : (unsigned char) total );
			centerpxl++;
			outpxl++;
		    }
		}
	    }
	    /* swap inbase and out base pointers for next pass */
	    tempbase = rasterbase;
	    rasterbase = outrasterbase;
	    outrasterbase = tempbase;
	}

	/****************************************************
	 * Set up output scanline
	 ****************************************************/
	if ( rle_row_alloc( &out_hdr, &scanout ) < 0 )
	    RLE_CHECK_ALLOC( progname, 0, 0 );
	if (out_hdr.alpha)
	    scanout--;		/* Use zero based (vs. -1 based) addressing */
	for ( chan=0; chan < nchan; chan++)
	{
	    rows[chan] = scanout[chan];
	}

	rle_put_setup( &out_hdr );
	for (y = 0; y < ylen; y++)
	{
	    rastptr = &(rasterbase[maxmaskbelow + (y+maxmaskbelow) *
				   xlinewidth * nchan]);

	    for (chan = 0; chan < nchan; chan++)
	    {
		outpxl = &(rastptr[chan * xlinewidth]);
		for(x=0; x < xlen; x++)
		{
		    scanout[chan][x] = *outpxl;
		    outpxl++;
		}
	    }
	    rle_putrow( &rows[out_hdr.alpha], xlen, &out_hdr );
	}

	rle_puteof( &out_hdr );

	free( mask_mult_table );
	free( rows );
	free( rasterbase );
	free( outrasterbase );
	if ( out_hdr.alpha )
	    scanout++;		/* Reverse above action. */
	rle_row_free( &out_hdr, scanout );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}

float *
gauss_mask(siz)
int siz;
{
    int i,j;
    float w[5];
    float *mptr;

    w[0] = 0.05;
    w[1] = 0.25;
    w[2] = 0.4;
    w[3] = 0.25;
    w[4] = 0.05;

    mptr = (float *) malloc( sizeof(float) * siz * siz );
    RLE_CHECK_ALLOC( progname, mptr, 0 );

    if (siz != 5)
    {
	fprintf(stderr,"mask size not 5\n");
	exit(-1);
    }

    for(i=0; i< 5; i++)
    {
	for(j=0;j<5;j++)
	{
	    mptr[j+i*siz] = w[j]*w[i];
	}
    }
    return mptr;
}

float *
read_mask(size, fname, no_norm)
int * size;
char * fname;
int no_norm;
{
    FILE * maskfile;
    int i,j,masksize;
    float *mptr, total;

    maskfile = rle_open_f( progname, fname, "r" );

    fscanf(maskfile, "%d", &masksize);

    mptr = (float *) malloc( sizeof(float) * masksize * masksize);
    RLE_CHECK_ALLOC( progname, mptr, 0 );

    total = 0.0;
    for (i=0; i< masksize; i++)
	for (j=0; j< masksize; j++)
	{
	    fscanf(maskfile, "%f", &(mptr[j+i*masksize]));
	    total += mptr[j+i*masksize];
	}

    if (!no_norm)
    {
	if (total <= 0.0)
	{
	    fprintf(stderr,"%s: Input mask total not valid\n", progname);
	    exit(1);
	}
	/* normalize the mask */
	for (i=0; i< masksize; i++)
	    for (j=0; j< masksize; j++)
	    {
		mptr[j+i*masksize] /= total;
	    }
    }

    *size = masksize;

    if ( maskfile != stdin )
	fclose( maskfile );

    return mptr;
}
