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
 * flip.c - Invert, reflect or 90-degree rotate an rle image.
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Jun 23 1986
 * Copyright (c) 1986, University of Utah
 * 
 * Usage is:
 *   flip  [-v | -h | -l | -r] [ infile ] [ -o outfile ]
 *
 * Where the flags mean:
 *	-v : Vertical flip (top to bottom)
 *      -h : Horizontal flip (right to left)
 *      -r : Rotate image 90 degrees right (clockwise)
 *  	-l : Rotate image 90 degrees left (counter-clockwise)
 *      -o : Specify an output file
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

#define VERT_FLAG  0x01		/* Command line flags */
#define HORIZ_FLAG 0x02		/* must match appearance in scanargs */
#define LEFT_FLAG  0x04
#define RIGHT_FLAG 0x08

int
main(argc, argv)
int  argc;
char *argv[];
{
    int rle_cnt;
    int flags = 0, oflag = 0;
    char *infilename = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    int xlen, ylen, i, j, chan;
    int xlinewidth;
    rle_hdr 	in_hdr, out_hdr;
    rle_pixel *rastptr, *rasterbase;
    rle_pixel **temp_line;
    rle_pixel **rows;
    int nchan;			/* Number of channels actually used. */
    int rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if (scanargs(argc, argv, "% rlhv!- o%-outfile!s infile%s", 
        &flags, &oflag, &out_fname, &infilename) == 0)
    {
	exit(-1);
    }

    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infilename, "r");
    rle_names( &in_hdr, cmd_name( argv ), infilename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(cmd_name( argv ), out_fname, "w");
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	nchan = out_hdr.alpha + out_hdr.ncolors;

	/* Make row pointers for all seasons. */
	rows = (rle_pixel **) malloc( nchan * sizeof( rle_pixel * ) );
	RLE_CHECK_ALLOC( cmd_name( argv ), rows, "image data" );

	xlen = in_hdr.xmax - in_hdr.xmin + 1;
	ylen = in_hdr.ymax - in_hdr.ymin + 1;

	/* getrow and putrow assume the scanline starts at pixel 0 */
	xlinewidth = in_hdr.xmax + 1;
    
	/* Note:
	 * When you read in a row of pixels with rle_getrow, it places blank
	 * pixels between 0 and xmin of your buffer.  However, when you
	 * use rle_putrow to write them out, the buffer must be pointing at
	 * where the data actually starts (i.e., at [xmin] of the getrow
	 * buffer.  */

	/* WARNING: Heavy-duty pointer munging ahead */

	rasterbase = (rle_pixel *) malloc( xlinewidth * ylen * nchan );
	RLE_CHECK_ALLOC( cmd_name( argv ), rasterbase, "raster" );
	rastptr = rasterbase;

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
	    rle_getrow( &in_hdr, &rows[out_hdr.alpha] );
	}

	/****************************************************
	 * Invert along vertical axis
	 ****************************************************/

	if (flags == VERT_FLAG)
	{
	    rle_put_setup( &out_hdr );

	    /* Find last row in raster */
	    rastptr = &(rasterbase[xlinewidth * (ylen - 1) * nchan]);

	    for (i = out_hdr.ymin; i <= out_hdr.ymax; i++)
	    {
		for (chan=0; chan < nchan; chan++)
		{
		    rows[chan] = &(rastptr[out_hdr.xmin]);
		    /* Increment pointer by xlinewidth */
		    rastptr = &(rastptr[xlinewidth]);
		}
		rle_putrow( &rows[out_hdr.alpha], xlen, &out_hdr );
		rastptr = &(rastptr[ - 2 * nchan * xlinewidth ]);
	    }
	}
	else

	    /****************************************************
	     * Reflect across horizontal axis
	     ****************************************************/

	    if (flags == HORIZ_FLAG)
	    {
		register rle_pixel *inpxl, *outpxl;

		rle_put_setup( &out_hdr );

		if (rle_row_alloc( &out_hdr, &temp_line ) < 0)
		    RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );
		if (out_hdr.alpha)
		    temp_line--;	/* Use zero based (vs. -1 based) addressing */

		/* Temp row used to swap pixel order */
		for (chan = 0; chan < nchan; chan++)
		    rows[chan] = &(temp_line[chan][out_hdr.xmin]);

		for (i = 0; i < ylen; i++)
		{
		    rastptr = &(rasterbase[i * xlinewidth * nchan]);

		    for (chan = 0; chan < nchan; chan++)
		    {
			inpxl =
			    &(rastptr[chan * xlinewidth + xlinewidth - xlen]);
			outpxl = &(temp_line[chan][xlinewidth-1]);

			for (j = 0; j < xlen; j++)
			    *outpxl-- = *inpxl++;
		    }
		    rle_putrow( &rows[out_hdr.alpha], xlen, &out_hdr );
		}
	    }
	    else

		/****************************************************
		 * Rotation
		 ****************************************************/
		if ((flags == RIGHT_FLAG) || (flags == LEFT_FLAG))
		{
		    int linebytes, chan_offset;
		    int lineoff;
		    int oxlen, oylen;
		    register rle_pixel *outptr;

		    /* Must first adjust size of output image... */
		    out_hdr.xmax = in_hdr.xmin + ylen - 1;
		    out_hdr.ymax = in_hdr.ymin + xlen - 1;

		    oxlen = out_hdr.xmax - out_hdr.xmin + 1;
		    oylen = out_hdr.ymax - out_hdr.ymin + 1;

		    rle_put_setup( &out_hdr );

		    if (rle_row_alloc( &out_hdr, &temp_line ) < 0)
			RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );
		    if (out_hdr.alpha)
			temp_line--;	/* Use zero based (vs. -1 based) */
		    /* addressing */

		    /* Temp row used to swap pixel order */
		    for (chan = 0; chan < nchan; chan++)
			rows[chan] = temp_line[chan];

		    linebytes = nchan * xlinewidth;	/* Bytes in entire */
		    /* input scanline */

		    if (flags == LEFT_FLAG)
		    {
			/****************************************************
			 * Rotate left
			 ****************************************************/
			for (i = 0; i < oylen; i++)
			{
			    lineoff = xlinewidth - xlen + i;
			    for (chan = 0; chan < nchan; chan++)
			    {
				/* Bytes upto input chan */
				chan_offset = lineoff + xlinewidth * chan;  
				outptr = temp_line[chan];
				for (j = oxlen - 1; j >= 0; j--)
				    *outptr++ =
					rasterbase[j * linebytes + chan_offset];
			    }
			    rle_putrow( &rows[out_hdr.alpha], oxlen, &out_hdr );
			}
		    }
		    else
		    {
			/****************************************************
			 * Rotate right
			 ****************************************************/

			for (i = 0; i < oylen; i++)
			{
			    for (chan = 0; chan < nchan; chan++)
			    {
				/* Bytes upto input chan */
				chan_offset = xlinewidth * chan + (xlinewidth - 1 - i);
				outptr = temp_line[chan];
				for (j = 0; j < oxlen; j++)
				{
				    *outptr++ = rasterbase[j * linebytes + chan_offset];
				}
			    }
			    rle_putrow( &rows[out_hdr.alpha], oxlen, &out_hdr );
			}
		    }
		}

	rle_puteof( &out_hdr );
	free( rows );
	free( rasterbase );
    }

    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, argv[0], infilename );

    exit( 0 );
}

