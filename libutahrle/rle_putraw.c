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
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/* 
 * rle_putraw.c - Generate RLE from "raw" form.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Jul  8 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdio.h>
#include "rle_put.h"
#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( rle_putraw )
 * 
 * Put "raw" RLE data to an output file.
 * Inputs:
 *	nraw:		Array of lengths of the rows.  One per color channel.
 * 	rows:		array of pointers to individual channels of rle data.
 *			Rows is assumed to have have the_hdr->ncolors (plus
 *			a [-1] element if alpha is being saved) pointers to
 *			arrays of rle_op.  The length of each array is given
 *			by the corresponding nraw element.
 *	the_hdr:	Header struct describing this image.
 *
 * Outputs:
 * 	Writes RLE data to output file.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
rle_putraw( scanraw, nraw, the_hdr )
rle_op **scanraw;
int *nraw;
rle_hdr *the_hdr;
{
    register int channel;
    int scan_x,
	i,
	n_op;
    register rle_op * scan_r;

    for ( channel = (the_hdr->alpha ? -1 : 0);
	  channel < the_hdr->ncolors;
	  channel++ )
    {
	if ( ! RLE_BIT( *the_hdr, channel ) || nraw[channel] == 0 )
	{
	    continue;
	}

	/* If really data on this scanline, skip to here */
	if ( the_hdr->priv.put.nblank > 0 )
	{
	    SkipBlankLines( the_hdr->priv.put.nblank );
	    the_hdr->priv.put.nblank = 0;
	}

	SetColor( channel );
	n_op = nraw[channel] - 1;
	scan_x = the_hdr->xmin;
	for ( i = 0, scan_r = scanraw[channel]; i <= n_op; i++, scan_r++ )
	{
	    if ( scan_r->xloc > scan_x )
		SkipPixels( scan_r->xloc - scan_x, 0,
			    i > 0 && (scan_r - 1)->opcode == RRunDataOp );
	    scan_x = scan_r->xloc + scan_r->length;
	    switch( scan_r->opcode )
	    {
	    case RRunDataOp:
		putrun( scan_r->u.run_val, scan_r->length,
			i < n_op && scan_x == (scan_r + 1)->xloc );
		break;

	    case RByteDataOp:
		putdata( scan_r->u.pixels, scan_r->length );
		break;
	    }
	}
	if ( scan_x <= the_hdr->xmax )
	    SkipPixels( the_hdr->xmax - scan_x,
			1,
			i > 0 && (scan_r - 1)->opcode == RRunDataOp );
	if ( channel != the_hdr->ncolors - 1 )
	    NewScanLine( 0 );
    }

    the_hdr->priv.put.nblank++;	/* increment to next scanline */
    /* Flush each scanline */
/*    fflush( the_hdr->rle_file );*/
}
