/* 
 * pyrhalf.c - Create bandpass pyramid for pyramid operations.
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

#include <stdio.h>
#include "rle.h"
#include "pyramid.h"

int rle_to_pyramids();
void alloc_pyramid(), copy_mask_bands(), rebuild_image(), dump_pyramid();
void copy_half_bands();

void
main(argc, argv)
int  argc;
char *argv[];
{
    char * leftname = NULL, * rightname = NULL, * outfname = NULL;
    char * maskname = NULL, * errname;
    FILE * leftfile, * rightfile, *maskfile, * outfile;
    int oflag = 0, lflag = 0;
    int i, j, k;
    int rle_cnt, rle_err;

    float *maskmult;
    float *mask_mult_table;
    float *mask;
    int level, levels = 0;
    rle_pixel *imgcorner;
    pyramid leftgausspyr, leftbandpyr;
    pyramid rightgausspyr, rightbandpyr;
    pyramid maskgausspyr;
    pyramid splitpyr;
    /*
     * "Left" and "right" are historical relics from a program that
     * joined two images at the middle.  Left == inside mask; right == outside.
     */
    rle_hdr left_hdr;
    rle_hdr right_hdr;
    rle_hdr mask_hdr;
    rle_hdr out_hdr;

    left_hdr = *rle_hdr_init( NULL );
    right_hdr = *rle_hdr_init( NULL );
    mask_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
	    "% l%-levels!d o%-outfile!s inmask!s outmask!s maskfile!s",
		&lflag, &levels,
		&oflag, &outfname, &leftname, &rightname, &maskname ) == 0 )
	exit( 1 );

    leftfile = rle_open_f( cmd_name( argv ), leftname, "r" );
    rightfile = rle_open_f( cmd_name( argv ), rightname, "r" );
    maskfile = rle_open_f( cmd_name( argv ), maskname, "r" );

    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );

    /* actual mask is always 5x5 */
    mask = gauss_mask(5);

    /* initialize mask_mult_table */
    mask_mult_table = (float *) malloc(sizeof(float) * 5 * 5 * 256);
    RLE_CHECK_ALLOC( cmd_name( argv ), mask_mult_table, "mask" );
    for (i=0; i < 5; i++)
    {
	maskmult = &(mask_mult_table[(i*5) << 8]);
	for (j=0; j < 5; j++)
	{
	    for (k=0;k < 256; k++)
		maskmult[ k ] = (float) k * mask[i*5+j];
	    maskmult += 256;
	}
    }

    rle_names( &left_hdr, cmd_name( argv ), leftname, 0 );
    rle_names( &right_hdr, cmd_name( argv ), rightname, 0 );
    rle_names( &mask_hdr, cmd_name( argv ), maskname, 0 );
    rle_names( &out_hdr, left_hdr.cmd, outfname, 0 );

    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	if ( (rle_err = rle_to_pyramids( leftfile, &leftgausspyr, &leftbandpyr,
					 &left_hdr, levels, mask_mult_table ))
	     != RLE_SUCCESS )
	{
	    errname = leftname;
	    break;
	}

	if ( (rle_err = rle_to_pyramids( rightfile, &rightgausspyr,
					 &rightbandpyr, &right_hdr,
					 levels, mask_mult_table ))
	     != RLE_SUCCESS )
	{
	    errname = rightname;
	    break;
	}

	if ( (rle_err = rle_to_pyramids( maskfile, &maskgausspyr, 0,
					 &mask_hdr, levels, mask_mult_table ))
	     != RLE_SUCCESS )
	{
	    errname = maskname;
	    break;
	}

	if (leftgausspyr.nchan != rightgausspyr.nchan)
	{
	    fprintf(stderr,
	    "%s: Left and right images must have same number of channels\n",
		    cmd_name( argv ));
	    exit(-2);
	}
	if ((leftgausspyr.xlen[0] != rightgausspyr.xlen[0]) ||
	    (leftgausspyr.ylen[0] != rightgausspyr.ylen[0]))
	{
	    fprintf(stderr,
		    "%s: Left and right images must have same dimensions\n",
		    cmd_name( argv ));
	    exit(-2);
	}
	if (leftgausspyr.nchan != maskgausspyr.nchan)
	{
	    fprintf(stderr,
	    "%s: Currently, mask image must have a mask per input channel\n",
		    cmd_name( argv ));
	    exit(-2);
	}
	if ((leftgausspyr.xlen[0] != maskgausspyr.xlen[0]) ||
	    (leftgausspyr.ylen[0] != maskgausspyr.ylen[0]))
	{
	    fprintf(stderr,
	    "%s: Mask image must have same dimensions as input images\n",
		    cmd_name( argv ));
	    exit(-2);
	}

	splitpyr.nchan = leftbandpyr.nchan;
	splitpyr.levels = leftbandpyr.levels;
	splitpyr.corners = (rle_pixel **) malloc( splitpyr.levels
						  * sizeof( rle_pixel *) );
	splitpyr.xlen = (int *) malloc( splitpyr.levels * sizeof( int ) );
	splitpyr.ylen = (int *) malloc( splitpyr.levels * sizeof( int ) );
	RLE_CHECK_ALLOC( cmd_name( argv ),
			 splitpyr.corners && splitpyr.xlen && splitpyr.ylen,
			 "split pyramid" );

	splitpyr.xlen[0] = leftbandpyr.xlen[0];
	splitpyr.ylen[0] = leftbandpyr.ylen[0];
	alloc_pyramid(&splitpyr);

	for(level = splitpyr.levels - 1; level >= 0; level--)
	{
	    copy_mask_bands(level,&leftbandpyr,&rightbandpyr,
			    &splitpyr,&maskgausspyr);
	}
	/****************************************************
	 * Reconstruct the image from the band pyramid
	 ****************************************************/
	rebuild_image(&imgcorner,&splitpyr,mask_mult_table);

	rle_hdr_cp( &left_hdr, &out_hdr );
	rle_addhist( argv, &left_hdr, &out_hdr );
	dump_pyramid(outfile,1,&imgcorner,splitpyr.xlen[0],splitpyr.ylen[0],
		     splitpyr.nchan - 1,left_hdr);
    }
    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), errname );


    exit(0);
}

void
copy_half_bands(level,leftbandpyr, rightbandpyr, splitbandpyr)
int level;
pyramid * leftbandpyr, * rightbandpyr, *splitbandpyr;
{
    int xsize, ysize, x, y, chan, xlinewidth, nchan;
    rle_pixel *leftbase, *leftrastptr, *leftpxl, *leftsign;
    rle_pixel *rightbase, *rightrastptr, *rightpxl, *rightsign;
    rle_pixel *outbase, *outrastptr, *outpxl, *outsign;
    float tval;
    int rightval, leftval, outval;

    leftbase = leftbandpyr->corners[level];
    rightbase = rightbandpyr->corners[level];
    outbase = splitbandpyr->corners[level];
    xsize = leftbandpyr->xlen[level];
    ysize = leftbandpyr->ylen[level];
    nchan = leftbandpyr->nchan - 1;
    splitbandpyr->xlen[level] = xsize;
    splitbandpyr->ylen[level] = ysize;
    xlinewidth = xsize + MASKSIZE - 1;

    fprintf(stderr,"Copying half of level %d, size %d\n",level,xsize);
    for (y = 0; y < ysize; y++)
    {
	leftrastptr = &(leftbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);
	rightrastptr = &(rightbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);

	outsign = &(outrastptr[nchan * xlinewidth]);
	for(x=0; x < xsize; x++)
	{
	    *outsign = 0;
	}

	for (chan = 0; chan < nchan; chan++)
	{
	    leftpxl = &(leftrastptr[chan * xlinewidth]);
	    leftsign = &(leftrastptr[nchan * xlinewidth]);
	    rightpxl = &(rightrastptr[chan * xlinewidth]);
	    rightsign = &(rightrastptr[nchan * xlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    outsign = &(outrastptr[nchan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		/* should do some mask magic here */
		if (x <= (15 * xsize / 32))
		{
		    *outpxl = (*leftpxl);
		    if ((1 << chan) & (*leftsign))
			*outsign |= (1 << chan);
		}
		else if (x >= (17 * xsize / 32))
		{
		    *outpxl = (*rightpxl);
		    if ((1 << chan) & (*rightsign))
			*outsign |= (1 << chan);
		}
		else
		{
		    tval = ((float) x - (15.0 * (float) xsize / 32.0)) /
			   ((float) xsize / 16.0);
		    leftval = (int) (*leftpxl) *
			(((1 << chan) & (*leftsign)) ? -1 : 1);
		    rightval = (int) (*rightpxl) *
			(((1 << chan) & (*rightsign)) ? -1 : 1);
		    outval = (int) (tval * (float) rightval +
			      (1.0 - tval) * (float) leftval);
		    if (outval < 0)
		    {
			*outpxl = (rle_pixel) (-outval);
			*outsign |= (1 << chan);
		    }
		    else
			*outpxl = (rle_pixel) (outval);
		}
		leftpxl++;
		rightpxl++;
		outpxl++;
		leftsign++;
		rightsign++;
		outsign++;
	    }
	}
    }
}

void
copy_mask_bands(level,leftbandpyr, rightbandpyr, splitbandpyr, maskgausspyr)
int level;
pyramid * leftbandpyr, * rightbandpyr, *splitbandpyr, *maskgausspyr;
{
    int xsize, ysize, x, y, chan, xlinewidth, nchan;
    rle_pixel *leftbase, *leftrastptr, *leftpxl, *leftsign;
    rle_pixel *rightbase, *rightrastptr, *rightpxl, *rightsign;
    rle_pixel *outbase, *outrastptr, *outpxl, *outsign;
    rle_pixel *maskbase, *maskrastptr, *maskpxl;
    float tval;
    int rightval, leftval, outval;

    leftbase = leftbandpyr->corners[level];
    rightbase = rightbandpyr->corners[level];
    outbase = splitbandpyr->corners[level];
    maskbase = maskgausspyr->corners[level];
    xsize = leftbandpyr->xlen[level];
    ysize = leftbandpyr->ylen[level];
    nchan = leftbandpyr->nchan - 1;
    splitbandpyr->xlen[level] = xsize;
    splitbandpyr->ylen[level] = ysize;
    xlinewidth = xsize + MASKSIZE - 1;

/*    fprintf(stderr,"Masking level %d, size %d\n",level,xsize); */
    for (y = 0; y < ysize; y++)
    {
	leftrastptr = &(leftbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);
	rightrastptr = &(rightbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan+1)]);
	maskrastptr = &(maskbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);

	outsign = &(outrastptr[nchan * xlinewidth]);
	for(x=0; x < xsize; x++)
	{
	    *outsign = 0;
	}

	for (chan = 0; chan < nchan; chan++)
	{
	    leftpxl = &(leftrastptr[chan * xlinewidth]);
	    leftsign = &(leftrastptr[nchan * xlinewidth]);
	    rightpxl = &(rightrastptr[chan * xlinewidth]);
	    rightsign = &(rightrastptr[nchan * xlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    outsign = &(outrastptr[nchan * xlinewidth]);
	    maskpxl = &(maskrastptr[chan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		/* should do some mask magic here */
		if (*maskpxl == 255)	/* full coverage means left image */
		{
		    *outpxl = (*leftpxl);
		    if ((1 << chan) & (*leftsign))
			*outsign |= (1 << chan);
		}
		else if (*maskpxl == 0)	/* no coverage means right image */
		{
		    *outpxl = (*rightpxl);
		    if ((1 << chan) & (*rightsign))
			*outsign |= (1 << chan);
		}
		else
		{
		    tval = ((float) (*maskpxl)) / 255.0;
		    leftval = (int) (*leftpxl) *
			(((1 << chan) & (*leftsign)) ? -1 : 1);
		    rightval = (int) (*rightpxl) *
			(((1 << chan) & (*rightsign)) ? -1 : 1);
		    outval = (int) (tval * (float) leftval +
			      (1.0 - tval) * (float) rightval);
		    if (outval < 0)
		    {
			*outpxl = (rle_pixel) (-outval);
			*outsign |= (1 << chan);
		    }
		    else
			*outpxl = (rle_pixel) (outval);
		}
		leftpxl++;
		rightpxl++;
		outpxl++;
		maskpxl++;
		leftsign++;
		rightsign++;
		outsign++;
	    }
	}
    }
}
