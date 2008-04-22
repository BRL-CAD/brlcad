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
 * rle_row_alc.c - Allocate buffers for rle_getrow/rle_putrow.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Nov 14 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */

#include <stdio.h>
#include "rle.h"

/*****************************************************************
 * TAG( rle_row_alloc )
 *
 * Allocate buffer space for use by rle_getrow and rle_putrow.
 * Inputs:
 * 	the_hdr:	Header structure for RLE file to be read or
 *			written.
 * Outputs:
 *	scanp:		Pointer to pointer to created scanline buffer.
 *			This pointer is adjusted for the alpha channel,
 *			if present.
 *	Returns 0 for success, -1 if malloc failed.
 * Assumptions:
 * 	No input scanline will extend beyond the declared xmax endpoint.
 * Algorithm:
 *	Count number of channels actually used (check bitmap).
 * 	Allocate nchan*rowlength pixels, allocate a buffer
 *	to hold ncolors+alpha pointers, and give each channel
 *	rowlength pixels.  Rowlength is xmax + 1, to allow for rle_getrow
 *	usage.
 */
int
rle_row_alloc( the_hdr, scanp )
rle_hdr *the_hdr;
rle_pixel ***scanp;
{
    rle_pixel ** scanbuf, * pixbuf;
    int rowlen, nchan = 0, i, ncol;

    rowlen = the_hdr->xmax + 1;
    if ( the_hdr->alpha && RLE_BIT( *the_hdr, RLE_ALPHA ) )
	nchan++;
    for ( i = 0; i < the_hdr->ncolors; i++ )
	if ( RLE_BIT( *the_hdr, i ) )
	     nchan++;

    ncol = the_hdr->ncolors + the_hdr->alpha;

    if ( (scanbuf = (rle_pixel **)malloc( ncol * sizeof(rle_pixel *) )) == 0 )
	return -1;
    if ( (pixbuf = (rle_pixel *)malloc( nchan * rowlen *
				       sizeof(rle_pixel) )) == 0 )
    {
	free( scanbuf );
	return -1;
    }

    if ( the_hdr->alpha )
	scanbuf++;

    for ( i = -the_hdr->alpha; i < the_hdr->ncolors; i++ )
	if ( RLE_BIT( *the_hdr, i ) )
	{
	    scanbuf[i] = pixbuf;
	    pixbuf += rowlen;
	}
	else
	    scanbuf[i] = 0;
    *scanp = scanbuf;

    return 0;
}


/*****************************************************************
 * TAG( rle_row_free )
 *
 * Free storage allocated by rle_row_alloc().
 * Inputs:
 * 	the_hdr:	Header structure as above.
 *	scanp:		Pointer to scanbuf above.
 * Outputs:
 * 	Frees storage referenced by scanp and nrawp.
 * Assumptions:
 * 	Storage was allocated by rle_row_alloc, or by use of same
 *	algorithm, at least.
 * Algorithm:
 * 	free scanp[0] and scanp.
 */
void
rle_row_free( the_hdr, scanp )
rle_hdr *the_hdr;
rle_pixel **scanp;
{
    int i;

    if ( the_hdr->alpha )
	scanp--;
    for ( i = 0; i < the_hdr->ncolors + the_hdr->alpha; i++ )
	if ( scanp[i] != 0 )
	{
	    free( (char *)scanp[i] );
	    break;
	}
    free( (char *)scanp );
}
