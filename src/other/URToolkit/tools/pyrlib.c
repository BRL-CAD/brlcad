/*
 * pyrlib.c - Library routines for pyramids
 *
 * Author:	Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Mar 12 1987
 * Copyright (c) 1987 Rod Bogart
 *
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "pyramid.h"

void extrap_level(), extrap_int_level();
void alloc_pyramid(), gauss_level(), band_level(), expand_level();
void dump_level_line(), band_to_int_level(), add_int_level();

int
rle_to_pyramids(infile, gausspyr, bandpyr,
		in_hdr, levellimit, mask_mult_table)
FILE * infile;
pyramid * gausspyr;
pyramid * bandpyr;
rle_hdr * in_hdr;
int levellimit;
float * mask_mult_table;
{
    rle_pixel *rastptr, *rasterbase;
    rle_pixel *firstbase, *outbase;
    int xlen, ylen, nchan;
    int xsize, ysize, levels, xlinewidth;
    int i, chan, x, y, minlen;
    rle_pixel *firstrastptr, *outrastptr;
    rle_pixel *firstpxl, *outpxl, *signpxl;
    rle_pixel **rows;
    int rle_err;

    in_hdr->rle_file = infile;

    if ( (rle_err = rle_get_setup( in_hdr )) < 0 )
    {
	return rle_err;
    }
    if ( in_hdr->alpha )
	RLE_SET_BIT( *in_hdr, RLE_ALPHA );

    in_hdr->xmax -= in_hdr->xmin;
    in_hdr->xmin = 0;
    xlen = in_hdr->xmax + 1;
    ylen = in_hdr->ymax - in_hdr->ymin + 1;
    xlinewidth = xlen + MASKSIZE - 1;

    /* assumes that alpha is 1 or 0 */
    nchan = in_hdr->ncolors + in_hdr->alpha;

    if (nchan > 8)
    {
	fprintf(stderr,"pyrlib: %d is too many input channels, 8 max\n",nchan);
	exit(-2);
    }
    /*************************************************************
     * Figure out how many levels
     * Note that the smallest level will be between 8 and 4 pixels
     * in its minimum direction.
     *************************************************************/

    minlen = (xlen < ylen) ? xlen : ylen;
    levels = levellimit;
    if (!levellimit)
	for(levels = 1; minlen > 8; minlen = minlen >> 1, levels++);
    else if ((minlen >> (levels - 1)) <= 4)
    {
	fprintf(stderr,
		"pyrlib: %d is too many levels to make a pyramid\n",levels);
	exit(-2);
    }

    if (levels < 2)
    {
	fprintf(stderr,
		"pyrlib: %d is too few levels to make a pyramid\n",levels);
	exit(-2);
    }

    gausspyr->levels = levels;
    gausspyr->nchan = nchan;
    gausspyr->xlen = (int *) malloc( levels * sizeof( int ) );
    gausspyr->ylen = (int *) malloc( levels * sizeof( int ) );
    gausspyr->corners = (rle_pixel **) malloc( levels * sizeof( rle_pixel *) );

    RLE_CHECK_ALLOC( in_hdr->cmd,
		     gausspyr->corners && gausspyr->xlen && gausspyr->ylen,
		     "gaussian pyramid" );

    gausspyr->xlen[0] = xlen;
    gausspyr->ylen[0] = ylen;

/*    fprintf(stderr,"Allocating gaussian pyramid structure\n"); */
    alloc_pyramid(gausspyr);

    if ( bandpyr )			/* zero means do gauss only */
    {
	bandpyr->levels = levels;
	bandpyr->nchan = nchan + 1;	/* plus for sign chan */
	bandpyr->xlen = (int *) malloc( levels * sizeof( int ) );
	bandpyr->ylen = (int *) malloc( levels * sizeof( int ) );
	bandpyr->corners = (rle_pixel **) malloc(levels * sizeof(rle_pixel *));

	RLE_CHECK_ALLOC( in_hdr->cmd,
			 bandpyr->corners && bandpyr->xlen && bandpyr->ylen,
			 "band pyramid" );

	bandpyr->xlen[0] = xlen;
	bandpyr->ylen[0] = ylen;

/*	fprintf(stderr,"Allocating band pyramid structure\n"); */
	alloc_pyramid(bandpyr);
    }

    rows = (rle_pixel **) malloc( nchan * sizeof( rle_pixel * ) );
    RLE_CHECK_ALLOC( in_hdr->cmd, rows, 0 );

    rasterbase = gausspyr->corners[0];
    rastptr = &(rasterbase[MASKBELOW * xlinewidth * nchan + MASKBELOW]);

    /****************************************************
     * Read in all of the pixels
     ****************************************************/

/*    fprintf(stderr,"Reading input image\n"); */
    for (i = in_hdr->ymin; i <= in_hdr->ymax; i++)
    {
        for (chan=0; chan < nchan; chan++)
        {
            rows[chan] = rastptr;
	    /* Increment pointer by xlinewidth */
	    rastptr = &(rastptr[xlinewidth]);
	}
	/* assumes 1 or 0 for alpha */
	rle_getrow( in_hdr, &rows[in_hdr->alpha] );
    }

    /****************************************************
     * Build each gauss level
     ****************************************************/
    for(i=1; i < levels; i++)
    {
/*	fprintf(stderr,"Building gauss level %d\n", i); */
	/* extrapolate the level we have */
	extrap_level(i-1, gausspyr);

	/* make gauss image in this level */
	gauss_level(i, gausspyr, mask_mult_table);
    }

    /* extrapolate the highest level */
    extrap_level(levels-1, gausspyr);

    /****************************************************
     * Build each band level from the gauss levels
     ****************************************************/
    if (bandpyr)
    {
	for(i=levels - 2; i >= 0; i--)
	{
/*	    fprintf(stderr,"Building band level %d\n", i); */

	    /* make band image from gauss images */
	    band_level(i, bandpyr, gausspyr, mask_mult_table);
	}

	/****************************************************
	* The highest band is just the highest gauss (remnant)
	****************************************************/
	firstbase = gausspyr->corners[levels - 1];
	outbase = bandpyr->corners[levels - 1];
	xsize = xlen >> (levels - 1);
	ysize = ylen >> (levels - 1);
	xlinewidth = xsize + MASKSIZE - 1;

	for (y = 0; y < ysize; y++)
	{
	    firstrastptr = &(firstbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);
	    outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan + 1)]);

	    signpxl = &(outrastptr[nchan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		/* zero out sign bits for each scanline */
		*signpxl = 0;
		signpxl++;
	    }
	    for (chan = 0; chan < nchan; chan++)
	    {
		firstpxl = &(firstrastptr[chan * xlinewidth]);
		outpxl = &(outrastptr[chan * xlinewidth]);
		for(x=0; x < xsize; x++)
		{
		    *outpxl = *firstpxl;
		    firstpxl++;
		    outpxl++;
		}
	    }
	}
    }
    return RLE_SUCCESS;
}

/* alloc_pyramid -
 *   Allocates a pyramid structure with "channels" channels per level
 * and level zero of size "xlen" by "ylen".  The pointers to each
 * corner are returned in the array "corners", so it is assumed that
 * space has been set aside for the corners to be returned to.
 */
void
alloc_pyramid(pyr)
pyramid * pyr;
{
    int i, xsize = pyr->xlen[0], ysize = pyr->ylen[0];

    for(i=0; i < pyr->levels; i++)
    {
	pyr->corners[i] = (rle_pixel *) malloc( (xsize + MASKSIZE - 1) *
		(ysize + MASKSIZE - 1) * pyr->nchan );
	RLE_CHECK_ALLOC( "pyrlib", pyr->corners[i], "pyramid" );

	pyr->xlen[i] = xsize;
	pyr->ylen[i] = ysize;
	xsize = xsize >> 1;
	ysize = ysize >> 1;
    }
}

/* extrap_level -
 *    "corners" is assumed to be an array of pointers to allocated
 * levels of a gaussian pyramid (band pyramids have sign bits).  The
 * level indicated by "level" (0 to levels - 1) is extrapolated.  This
 * currently means that boundary pixels are reflected thru the edge
 * pixel to find the outer boundary pixels.  In actuality, there
 * should be some nifty mask thingy that flags known and unknown
 * pixels.  Then one could extrapolate any unknown pixel that has
 * known neighbors.
 */
void
extrap_level(level,pyr)
int level;
pyramid * pyr;
{
    int chan, x, y, i, newvalue, xsize, ysize;
    int xlinewidth, nchan;
    rle_pixel *rastptr, *rasterbase;
    rle_pixel *bot_edge_pxl, *bot_data_pxl, *bot_extrap_pxl;
    rle_pixel *top_edge_pxl, *top_data_pxl, *top_extrap_pxl;
    rle_pixel *left_edge_pxl, *left_data_pxl, *left_extrap_pxl;
    rle_pixel *right_edge_pxl, *right_data_pxl, *right_extrap_pxl;

    rasterbase = pyr->corners[level];
    xsize = pyr->xlen[level];
    ysize = pyr->ylen[level];
    nchan = pyr->nchan;
    xlinewidth = xsize + MASKSIZE - 1;
    rastptr = &(rasterbase[MASKBELOW + MASKBELOW * xlinewidth * nchan]);

    for (chan = 0; chan < nchan; chan++)
    {
	bot_edge_pxl = &(rastptr[chan * xlinewidth]);
	top_edge_pxl = &(rastptr[(ysize-1) * nchan * xlinewidth
				    + chan * xlinewidth]);
	for(x=0; x < xsize; x++)
	{
	    for (i=1; i <= MASKBELOW; i++)
	    {
		bot_data_pxl = bot_edge_pxl + i * xlinewidth * nchan;
		bot_extrap_pxl = bot_edge_pxl - i * xlinewidth * nchan;
		newvalue = 2 * (*bot_edge_pxl) - (*bot_data_pxl);
		*bot_extrap_pxl = (newvalue < 0) ? 0 :
			((newvalue > 255) ? 255 : newvalue);
	    }
	    for (i=1; i <= MASKABOVE; i++)
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

    left_edge_pxl = &(rastptr[(-MASKBELOW) * nchan * xlinewidth]);
    right_edge_pxl = &(rastptr[(-MASKBELOW) * nchan * xlinewidth
				    + xlinewidth - MASKSIZE]);
    for (chan = 0; chan < nchan; chan++)
    {
	for(y=0; y < ysize + MASKSIZE - 1; y++)
	{
	    for (i=1; i <= MASKBELOW; i++)
	    {
		left_data_pxl = left_edge_pxl + i;
		left_extrap_pxl = left_edge_pxl - i;
		newvalue = 2 * (*left_edge_pxl) - (*left_data_pxl);
		*left_extrap_pxl = (newvalue < 0) ? 0 :
			((newvalue > 255) ? 255 : newvalue);
	    }
	    for (i=1; i <= MASKABOVE; i++)
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

/* gauss_mask -
 *    Create the gaussian 5x5 mask.  It gets allocated here, a pointer
 * is returned.
 */
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
    RLE_CHECK_ALLOC( "pyrlib", mptr, "mask" );

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

/* gauss_level -
 *    Build gaussian level from level - 1.  It is assumed that the
 * previous level is full of data, and that it has been extrapolated
 * at its boundaries.  Then every other pixel is used for the center
 * of the gaussian mask convolution.  The result is half the size of
 * level - 1.
 */
void
gauss_level(level,pyr,mask_mult_table)
int level;
pyramid * pyr;
float *mask_mult_table;
{
    int xsize, ysize, x, y, xlinewidth, lilxlinewidth;
    int chan,i,j,nchan;
    rle_pixel *inbase, *outbase, *inrastptr, *outrastptr;
    rle_pixel *centerpxl, *outpxl, *calcpxl;
    float total;
    float *maskmult;

    /* this assumes that the level-1 image is extrapolated */

    inbase = pyr->corners[level-1];
    outbase = pyr->corners[level];
    xsize = pyr->xlen[level - 1];
    ysize = pyr->ylen[level - 1];
    nchan = pyr->nchan;
    xlinewidth = xsize + MASKSIZE - 1;
    lilxlinewidth = (xsize >> 1) + MASKSIZE - 1;

    for (y = 0; y < ysize; y+=2)
    {
	inrastptr = &(inbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);
	outrastptr = &(outbase[MASKBELOW + ((y/2)+MASKBELOW) *
				lilxlinewidth * nchan]);

	for (chan = 0; chan < nchan; chan++)
	{
	    centerpxl = &(inrastptr[chan * xlinewidth]);
	    outpxl = &(outrastptr[chan * lilxlinewidth]);
	    for(x=0; x < xsize; x+=2)
	    {
		total = 0.0;
		for (i=0; i < 5; i++)
		{
		    calcpxl = centerpxl + (i - MASKBELOW)
				* xlinewidth * nchan - MASKBELOW;
		    maskmult = &(mask_mult_table[(i*5) << 8]);

		    for (j=0; j < 5; j++)
		    {
			/* look up multiply in maskmult table */
			total += maskmult[ calcpxl[j] ];
			maskmult += 256;
		    }
		}

		/* bound result to 0 to 255 */
		*outpxl = (total < 0.0) ? 0 :
			    ((total > 255.0) ? 255 : (rle_pixel) total );
		centerpxl++; centerpxl++;
		outpxl++;
	    }
	}
    }
}

/* band_level -
 *    Given a complete gaussian pyramid, this creates the band pass
 * levels, which are ((Gn) - (Gn+1)).  The "level + 1" gaussian level is
 * expanded, put in temp location, and then is subtracted pixel by
 * pixel from the "level" gaussian level.  Since this can cause
 * negative values, the band level has an additional channel which is
 * the sign bits (1 = negative, 0 = positive) for up to eight
 * channels.  There is a limit of eight channels for an input image.
 * Hence, a band pyramid has nchan + 1 channels in it.
 */
void
band_level(level, bandpyr, gausspyr, mask_mult_table)
int level;
pyramid * bandpyr;
pyramid * gausspyr;
float *mask_mult_table;
{
    int xsize, ysize, x, y, xlinewidth;
    int chan,subtractval,nchan;
    rle_pixel *firstbase, *secondbase, *outbase;
    rle_pixel *firstrastptr, *secondrastptr, *outrastptr;
    rle_pixel *firstpxl, *secondpxl, *outpxl, *signpxl;

    /* this assumes that the gauss images are extrapolated */
    /* the result pyramid will NOT be extrapolated */

    /* second level will be expanded and subtracted from first
     * to produce out */
    firstbase = gausspyr->corners[level];
    expand_level(level+1, gausspyr, &secondbase, mask_mult_table);
    outbase = bandpyr->corners[level];
    xsize = gausspyr->xlen[level];
    ysize = gausspyr->ylen[level];
    nchan = gausspyr->nchan;
    xlinewidth = xsize + MASKSIZE - 1;

    for (y = 0; y < ysize; y++)
    {
	firstrastptr = &(firstbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);
	secondrastptr = &(secondbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan + 1)]);

	signpxl = &(outrastptr[nchan * xlinewidth]);
	for(x=0; x < xsize; x++)
	{
	    /* zero out sign bits for this scanline */
	    *signpxl = 0;
	    signpxl++;
	}
	for (chan = 0; chan < nchan; chan++)
	{
	    firstpxl = &(firstrastptr[chan * xlinewidth]);
	    secondpxl = &(secondrastptr[chan * xlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    signpxl = &(outrastptr[nchan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		subtractval = (int) (*firstpxl) - (int) (*secondpxl);
		if (subtractval >= 0)
		    *outpxl = (rle_pixel) subtractval;
		else
		{
		    *outpxl = (rle_pixel) -subtractval;
		    *signpxl = (1 << chan) | (*signpxl);
		}
		firstpxl++;
		secondpxl++;
		outpxl++;
		signpxl++;
	    }
	}
    }
    free( secondbase );
}

/* expand_level -
 *    This is used for expanding a level of a gaussian pyramid.  It
 * allocates the storage for the result, and the pointer is returned
 * in corner.  The mask centering process is way weird.  Actually, it
 * might even, be slightly wrong, but don't tell anyone, K?
 */
void
expand_level(level, gausspyr, corner, mask_mult_table)
int level;
pyramid * gausspyr;
rle_pixel ** corner;
float *mask_mult_table;
{
    int xsize, ysize, x, y, xlinewidth, lilxlinewidth;
    int chan,i,j,nchan;
    rle_pixel *inbase, *outbase;
    rle_pixel *inrastptr, *outrastptr;
    rle_pixel *centerpxl, *outpxl, *calcpxl, expandedval;
    float total;
    float *maskmult;

    /* this assumes that the gauss images are extrapolated */

    xsize = gausspyr->xlen[level - 1];
    ysize = gausspyr->ylen[level - 1];
    nchan = gausspyr->nchan;
    /* level is the gauss level to be expanded */
    outbase = (rle_pixel *) malloc( (xsize + MASKSIZE - 1) *
	    (ysize + MASKSIZE - 1) * nchan );
    RLE_CHECK_ALLOC( "pyrlib", outbase, "expand level" );

    inbase = gausspyr->corners[level];
    *corner = outbase;
    xlinewidth = xsize + MASKSIZE - 1;
    lilxlinewidth = (xsize >> 1) + MASKSIZE - 1;

    for (y = 0; y < ysize; y++)
    {
	inrastptr = &(inbase[MASKBELOW + ((y/2)+MASKBELOW) *
				lilxlinewidth * nchan]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);

	for (chan = 0; chan < nchan; chan++)
	{
	    centerpxl = &(inrastptr[chan * lilxlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		total = 0.0;
		for (i=0; i < 5; i++)
		{
		    if ((y + i - MASKBELOW) % 2 == 0)
		    {
			if ((y % 2) == 0)
			    calcpxl = centerpxl + ((i - MASKBELOW) / 2)
				* lilxlinewidth * nchan - ( MASKBELOW / 2);
			else
			    calcpxl = centerpxl + ((i + 1 - MASKBELOW) / 2)
				* lilxlinewidth * nchan - ( MASKBELOW / 2);
			maskmult = &(mask_mult_table[(i*5) << 8]);

			for (j=0; j < 5; j++)
			{
			    if ((x + j - MASKBELOW) % 2 == 0)
			    {
				/* look up multiply in maskmult table */
				if ((x % 2) == 0)
				    total += maskmult[ calcpxl[j / 2] ];
				else
				    total += maskmult[ calcpxl[(j + 1) / 2] ];
			    }
			    maskmult += 256;
			}
		    }
		}

		/* bound result to 0 to 255 */
		total *= 4.0;
		expandedval = (total < 0.0) ? 0 :
			    ((total > 255.0) ? 255 : (rle_pixel) total );
		*outpxl = (rle_pixel) expandedval;
		if ((x % 2) == 1) centerpxl++;
		outpxl++;
	    }
	}
    }
}

/* dump_pyramid -
 *    Given a file pointer (an open file, hmmm...), this dumps a
 * pyramid out on multiple channels.  If the input pyramid is only one
 * level, an attempt is made to do the right thing with alpha, and
 * just dump a 'normal' RLE file.  If it has more channels ( > 1 ),
 * then the levels of the pyramid end up in higher channels, in groups
 * of "channels" channels.
 */
void
dump_pyramid(outfile,levels,corners,xlen,ylen,channels,in_hdr)
FILE * outfile;
int levels;
rle_pixel **corners;
int xlen,ylen,channels;
rle_hdr in_hdr;
{
    rle_hdr out_hdr;
    int chan, level, x, y, activelevels, adjust_alpha = 0;
    rle_pixel **scanout;
    rle_pixel **outrows;

/*    fprintf(stderr,"Dumping %d level pyramid with %d channels per level\n",
		levels, channels);
*/
    if ((levels == 1) && (in_hdr.alpha))
	adjust_alpha = 1;	/* for dumping a normal image */
    (void)rle_hdr_cp( &in_hdr, &out_hdr );
    out_hdr.alpha = adjust_alpha;
    out_hdr.ncolors = channels * levels - adjust_alpha;
    out_hdr.rle_file = outfile;

    for ( chan=0; chan < channels*levels - adjust_alpha; chan++)
    {
	RLE_SET_BIT( out_hdr, chan );
    }
    if ( rle_row_alloc( &out_hdr, &scanout ) < 0 )
	RLE_CHECK_ALLOC( in_hdr.cmd, 0, 0 );
    outrows = (rle_pixel **) malloc( channels * levels * sizeof(rle_pixel *) );
    RLE_CHECK_ALLOC( in_hdr.cmd, outrows, 0 );

    if (adjust_alpha) scanout--;
    for ( chan=0; chan < channels*levels; chan++)
    {
	outrows[chan] = scanout[chan];
	/* fill the scanline with black */
	for (x = 0; x < xlen; x++)
	    scanout[chan][x] = 0;
    }

    rle_put_setup( &out_hdr );

    activelevels = levels;
    for (y = 0; y < ylen; y++)
    {
	/* for each active level, dump the data into the right part
	 * of the scanline.
	 */
	for (level = 0; level < activelevels; level++)
	{
	    dump_level_line(scanout,y,level,corners,xlen,channels);
	}
	rle_putrow( &(outrows[adjust_alpha]), xlen, &out_hdr );

	/* check if top level becomes inactive, dump black
	 * to the scanline for next time.
	 */
	if (y == (ylen >> (activelevels -1)))
	{
	    for (chan = 0; chan < channels; chan++)
		for (x = 0; x < (xlen >> (activelevels -1)); x++)
		    scanout[chan + channels * (activelevels-1)][x] = 0;
	    activelevels--;
	}
    }
    rle_puteof( &out_hdr );
}

void
dump_level_line(scanout,y,level,corners,xlen,nchan)
rle_pixel ** scanout;
int y, level;
rle_pixel ** corners;
int xlen,nchan;
{
    int xsize, x, chan, xlinewidth;
    rle_pixel * outpxl, * outrastptr, *outbase, *scanoutptr;

    xsize = xlen >> level;
    xlinewidth = xsize + MASKSIZE - 1;
    outbase = corners[level];
    outrastptr = &(outbase[MASKBELOW + (y + MASKBELOW) * xlinewidth * nchan]);

    /* for each channel in the level
     * copy the pixel into scanout
     */
    for ( chan=0; chan < nchan; chan++)
    {
	outpxl = &(outrastptr[chan * xlinewidth]);
	scanoutptr = scanout[chan + nchan * level];
	for (x=0; x < xsize; x++)
	{
	    *scanoutptr = *outpxl;
	    scanoutptr++;
	    outpxl++;
	}
    }
}

void
rebuild_image(imgcorner, bandpyr, mask_mult_table)
rle_pixel ** imgcorner;
pyramid * bandpyr;
float *mask_mult_table;
{
    int i, xsize, ysize, level, nchan, xlen, ylen;
    int x, y, chan, xlinewidth,levels;
    int ** intcorners;
    int *intbase, *intrastptr, *intpxl;
    rle_pixel *outbase, *outrastptr, *outpxl;

    /* band pyramid is probably not extrapolated
     * allocate int_pyramid without sign channel
     * copy band pyramid to int pyramid (do correct thing with sign)
     * for each level (highest to lowest+1)
     *   extrapolate level
     *   expand level and add result directly to next level
     *   free temp expanded level
     * allocate rle_pixel image of level 1 size
     * copy from int level 1 to image
     * free int pyramid
     */

    /*************************************************************
    * Allocate int pyramid
    *************************************************************/
    levels = bandpyr->levels;
    nchan = bandpyr->nchan - 1;
    xlen = bandpyr->xlen[0];  ylen = bandpyr->ylen[0];
    intcorners = (int **) malloc( levels * sizeof( int *) );
    RLE_CHECK_ALLOC( "pyrhalf", intcorners, "pyramid" );

    xsize = xlen;    ysize = ylen;
    for(i=0; i < levels; i++)
    {
	intcorners[i] = (int *) malloc( (xsize + MASKSIZE - 1)
		* (ysize + MASKSIZE - 1) * nchan * sizeof(int) );
	RLE_CHECK_ALLOC( "pyrhalf", intcorners[i], "pyramid" );
	xsize = xsize >> 1;
	ysize = ysize >> 1;
    }
    /*************************************************************
    * Copy band pyramid to int (with sign correct)
    *************************************************************/
    for(level = levels - 1; level >= 0; level--)
    {
	band_to_int_level(level,bandpyr->corners,intcorners,
		xlen,ylen,nchan);
    }
    /*************************************************************
    * For each level in the int pyramid (highest to lowest+1)
    *************************************************************/
    for(level = levels - 1; level >= 1; level--)
    {
	/* extrapolate level */
	extrap_int_level(level,intcorners,xlen,ylen,nchan);
	/* expand level and add result directly to next level */
	add_int_level(level,intcorners,xlen,ylen,
		nchan,mask_mult_table);
    }
    /*************************************************************
    * Allocate single image buffer
    *************************************************************/
    *imgcorner = (rle_pixel *) malloc( (xlen + MASKSIZE - 1) *
	    (ylen + MASKSIZE - 1) * nchan );
    RLE_CHECK_ALLOC( "pyrhalf", imgcorner, "pyramid" );

    /*************************************************************
    * Copy lowest level of int pyramid to image buffer (clamp 0 - 255)
    *************************************************************/
    intbase = intcorners[0];
    outbase = *imgcorner;
    xsize = xlen;
    ysize = ylen;
    xlinewidth = xsize + MASKSIZE - 1;

    for (y = 0; y < ysize; y++)
    {
	intrastptr = &(intbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);

	for (chan = 0; chan < nchan; chan++)
	{
	    intpxl = &(intrastptr[chan * xlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		*outpxl = (rle_pixel) ((*intpxl) > 255) ? 255 :
		    (((*intpxl) < 0) ? 0 : (*intpxl));
		intpxl++;
		outpxl++;
	    }
	}
    }
    /*************************************************************
     * Free levels of int pyramid
     *************************************************************/
    for(i=0; i < levels; i++)
	free(intcorners[i]);
}

void
band_to_int_level(level,bandcorners,intcorners,	xlen,ylen,nchan)
int level;
rle_pixel ** bandcorners;
int ** intcorners;
int xlen,ylen,nchan;
{
    int xsize, ysize, x, y, chan, xlinewidth, sign;
    rle_pixel *firstbase, *firstrastptr, *firstpxl, *signpxl;
    int *outbase, *outrastptr, *outpxl;

    /* copy from band level to int level keeping track of the sign */
    firstbase = bandcorners[level];
    outbase = intcorners[level];
    xsize = xlen >> level;
    ysize = ylen >> level;
    xlinewidth = xsize + MASKSIZE - 1;

    for (y = 0; y < ysize; y++)
    {
	firstrastptr = &(firstbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * (nchan + 1)]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);

	for (chan = 0; chan < nchan; chan++)
	{
	    firstpxl = &(firstrastptr[chan * xlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    signpxl = &(firstrastptr[nchan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		sign = ((1 << chan) & (*signpxl)) ? -1 : 1;
		*outpxl = (int) (*firstpxl) * sign;
		firstpxl++;
		outpxl++;
		signpxl++;
	    }
	}
    }
}

void
extrap_int_level(level,corners,xlen,ylen,nchan)
int level;
int **corners;
int xlen,ylen,nchan;
{
    int chan, x, y, i, newvalue, xsize, ysize;
    int xlinewidth;
    int *rastptr, *rasterbase;
    int *bot_edge_pxl, *bot_data_pxl, *bot_extrap_pxl;
    int *top_edge_pxl, *top_data_pxl, *top_extrap_pxl;
    int *left_edge_pxl, *left_data_pxl, *left_extrap_pxl;
    int *right_edge_pxl, *right_data_pxl, *right_extrap_pxl;

    rasterbase = corners[level];
    xsize = xlen >> level;
    ysize = ylen >> level;
    xlinewidth = xsize + MASKSIZE - 1;
    rastptr = &(rasterbase[MASKBELOW + MASKBELOW * xlinewidth * nchan]);

    for (chan = 0; chan < nchan; chan++)
    {
	bot_edge_pxl = &(rastptr[chan * xlinewidth]);
	top_edge_pxl = &(rastptr[(ysize-1) * nchan * xlinewidth
				    + chan * xlinewidth]);
	for(x=0; x < xsize; x++)
	{
	    for (i=1; i <= MASKBELOW; i++)
	    {
		bot_data_pxl = bot_edge_pxl + i * xlinewidth * nchan;
		bot_extrap_pxl = bot_edge_pxl - i * xlinewidth * nchan;
		newvalue = 2 * (*bot_edge_pxl) - (*bot_data_pxl);
		*bot_extrap_pxl = newvalue;
	    }
	    for (i=1; i <= MASKABOVE; i++)
	    {
		top_data_pxl = top_edge_pxl - i * xlinewidth * nchan;
		top_extrap_pxl = top_edge_pxl + i * xlinewidth * nchan;
		newvalue = 2 * (*top_edge_pxl) - (*top_data_pxl);
		*top_extrap_pxl = newvalue;
	    }
	    bot_edge_pxl++;
	    top_edge_pxl++;
	}
    }

    left_edge_pxl = &(rastptr[(-MASKBELOW) * nchan * xlinewidth]);
    right_edge_pxl = &(rastptr[(-MASKBELOW) * nchan * xlinewidth
				    + xlinewidth - MASKSIZE]);
    for (chan = 0; chan < nchan; chan++)
    {
	for(y=0; y < ysize + MASKSIZE - 1; y++)
	{
	    for (i=1; i <= MASKBELOW; i++)
	    {
		left_data_pxl = left_edge_pxl + i;
		left_extrap_pxl = left_edge_pxl - i;
		newvalue = 2 * (*left_edge_pxl) - (*left_data_pxl);
		*left_extrap_pxl = newvalue;
	    }
	    for (i=1; i <= MASKABOVE; i++)
	    {
		right_data_pxl = right_edge_pxl - i;
		right_extrap_pxl = right_edge_pxl + i;
		newvalue = 2 * (*right_edge_pxl) - (*right_data_pxl);
		*right_extrap_pxl = newvalue;
	    }
	    left_edge_pxl += xlinewidth;
	    right_edge_pxl += xlinewidth;
	}
    }
}

void
add_int_level(level,intcorners,xlen,ylen,nchan,mask_mult_table)
int level;
int **intcorners;
int xlen,ylen,nchan;
float *mask_mult_table;
{
    int xsize, ysize, x, y, xlinewidth, lilxlinewidth;
    int chan,i,j;
    int *inbase, *outbase;
    int *inrastptr, *outrastptr;
    int *centerpxl, *outpxl, *calcpxl, expandedval;
    float total;
    float *maskmult;

    /* this assumes that the gauss images are extrapolated */

    xsize = xlen >> (level - 1);
    ysize = ylen >> (level - 1);
    /* level is the int level to be expanded */
    inbase = intcorners[level];
    outbase = intcorners[level-1];
    xlinewidth = xsize + MASKSIZE - 1;
    lilxlinewidth = (xsize >> 1) + MASKSIZE - 1;

    for (y = 0; y < ysize; y++)
    {
	inrastptr = &(inbase[MASKBELOW + ((y/2)+MASKBELOW) *
				lilxlinewidth * nchan]);
	outrastptr = &(outbase[MASKBELOW + (y+MASKBELOW) *
				xlinewidth * nchan]);

	for (chan = 0; chan < nchan; chan++)
	{
	    centerpxl = &(inrastptr[chan * lilxlinewidth]);
	    outpxl = &(outrastptr[chan * xlinewidth]);
	    for(x=0; x < xsize; x++)
	    {
		total = 0.0;
		for (i=0; i < 5; i++)
		{
		    if ((y + i - MASKBELOW) % 2 == 0)
		    {
			if ((y % 2) == 0)
			    calcpxl = centerpxl + ((i - MASKBELOW) / 2)
				* lilxlinewidth * nchan - ( MASKBELOW / 2);
			else
			    calcpxl = centerpxl + ((i + 1 - MASKBELOW) / 2)
				* lilxlinewidth * nchan - ( MASKBELOW / 2);
			maskmult = &(mask_mult_table[(i*5) << 8]);

			for (j=0; j < 5; j++)
			{
			    if ((x + j - MASKBELOW) % 2 == 0)
			    {
				/* since we gots ints, we can't use lookup */
				/* so just multiply by float value (yucko) */
				if ((x % 2) == 0)
				{
				    total += maskmult[ 1 ] * calcpxl[j / 2];
				}
				else
				{
				    total += maskmult[ 1 ] *
						calcpxl[(j + 1) / 2];
				}
			    }
			    maskmult += 256;
			}
		    }
		}

		total *= 4.0;
		expandedval = (int) total;
		*outpxl += expandedval;
		if ((x % 2) == 1) centerpxl++;
		outpxl++;
	    }
	}
    }
}

