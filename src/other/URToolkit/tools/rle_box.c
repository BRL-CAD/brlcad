/*
 * rle_box.c - Common code between rlebox and crop.
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Tue Jun  5 1990
 * Copyright (c) 1990, University of Michigan
 */

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

void
rle_box( the_hdr, xminp, xmaxp, yminp, ymaxp )
rle_hdr *the_hdr;
int *xminp, *xmaxp, *yminp, *ymaxp;
{
    rle_op ** raw;
    register rle_op ** rawp;
    register int i;
    int y, * nrawp;
    int xmin, xmax, ymin, ymax;

    if ( rle_raw_alloc( the_hdr, &raw, &nrawp ) < 0 )
	RLE_CHECK_ALLOC( the_hdr->cmd, 0, 0 );
    rawp = raw;

    ymax = -1;			/* smaller than it can ever be */
    ymin = 32768;		/* larger than it can ever be */
    xmax = -1;
    xmin = 32768;

    while ( ( y = rle_getraw( the_hdr, rawp, nrawp ) ) != 32768 )
    {
	if ( y < ymin )		/* found ymin yet? */
	{
	    /* Only count it if there is really some data */
	    for ( i = -the_hdr->alpha; i < the_hdr->ncolors; i++ )
		if ( nrawp[i] > 0 )
		    ymin = y;
	}
	if ( y > ymax )		/* update ymax? */
	{
	    /* Only count it if there is really some data */
	    for ( i = -the_hdr->alpha; i < the_hdr->ncolors; i++ )
		if ( nrawp[i] > 0 )
		    ymax = y;
	}

	/* Now do xmin and xmax */
	    for ( i = -the_hdr->alpha; i < the_hdr->ncolors; i++ )
		if ( nrawp[i] > 0 )
		{
		    if ( rawp[i][0].xloc < xmin )
			xmin = rawp[i][0].xloc;
		    if ( xmax < rawp[i][nrawp[i]-1].xloc +
			 rawp[i][nrawp[i]-1].length - 1 )
			xmax =  rawp[i][nrawp[i]-1].xloc +
			    rawp[i][nrawp[i]-1].length - 1;
		}
    }

    /* If no data, arbitrarily use (0,0)x(0,0) */
    if ( xmax < xmin )
    {
	xmax = xmin = ymax = ymin = 0;
    }

    *xminp = xmin;
    *xmaxp = xmax;
    *yminp = ymin;
    *ymaxp = ymax;

    rle_raw_free( the_hdr, raw, nrawp );
}
