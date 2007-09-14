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
 * rle_raw_alc.c - Allocate buffers for rle_getraw/rle_putraw.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Nov 14 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( rle_raw_alloc )
 *
 * Allocate buffer space for use by rle_getraw and rle_putraw.
 * Inputs:
 * 	the_hdr:	Header structure for RLE file to be read or
 *			written.
 * Outputs:
 *	scanp:		Pointer to pointer to created opcode buffer.
 * 	nrawp:		Pointer to pointer to created length buffer.
 *			These pointers are adjusted for the alpha channel,
 *			if present.
 *	Returns 0 for success, -1 if malloc failed.
 * Assumptions:
 * 	Since buffers are built to have as many slots as there are pixels
 *	in the input scanline, it is assumed that no input scanline will
 *	have more data elements than this.
 * Algorithm:
 *	Count number of channels actually used (check bitmap).
 * 	Allocate nchan*rowlength elements, allocate a buffer
 *	to hold (ncolors+alpha) pointers.
 *	Also allocate a buffer of ncolors+alpha
 *	integers for the length buffer.
 */
int
rle_raw_alloc( the_hdr, scanp, nrawp )
rle_hdr *the_hdr;
rle_op ***scanp;
int **nrawp;
{
    rle_op ** scanbuf, * opbuf;
    int rowlen, nchan = 0, i, ncol;

    rowlen = the_hdr->xmax - the_hdr->xmin + 1;
    if ( the_hdr->alpha && RLE_BIT( *the_hdr, RLE_ALPHA ) )
	nchan++;
    for ( i = 0; i < the_hdr->ncolors; i++ )
	if ( RLE_BIT( *the_hdr, i ) )
	     nchan++;

    ncol = the_hdr->ncolors + the_hdr->alpha;

    if ( (scanbuf = (rle_op **) malloc( ncol * sizeof(rle_op *) )) == 0 )
	return -1;

    if ( (opbuf = (rle_op *)malloc( nchan * rowlen * sizeof(rle_op) )) == 0 )
    {
	free( scanbuf );
	return -1;
    }

    if ( nrawp && (*nrawp = (int *)malloc( ncol * sizeof(int) )) == 0 )
    {
	free( scanbuf );
	free( opbuf );
	return -1;
    }

    if ( the_hdr->alpha )
    {
	scanbuf++;
	if ( nrawp )
	    (*nrawp)++;
    }

    for ( i = -the_hdr->alpha; i < the_hdr->ncolors; i++ )
	if ( RLE_BIT( *the_hdr, i ) )
	{
	    scanbuf[i] = opbuf;
	    opbuf += rowlen;
	}
	else
	    scanbuf[i] = 0;
    *scanp = scanbuf;

    return 0;
}


/*****************************************************************
 * TAG( rle_raw_free )
 *
 * Free storage allocated by rle_raw_alloc().
 * Inputs:
 * 	the_hdr:	Header structure as above.
 *	scanp:		Pointer to scanbuf above.
 *	nrawp:		Pointer to length buffer.
 * Outputs:
 * 	Frees storage referenced by scanp and nrawp.
 * Assumptions:
 * 	Storage was allocated by rle_raw_alloc, or by use of same
 *	algorithm, at least.
 * Algorithm:
 * 	free scanp[0], scanp, and nrawp.
 */
void
rle_raw_free( the_hdr, scanp, nrawp )
rle_hdr *the_hdr;
rle_op **scanp;
int *nrawp ;
{
    int i;

    if ( the_hdr->alpha )
    {
	scanp--;
	if ( nrawp )
	    nrawp--;
    }
    for ( i = 0; i < the_hdr->ncolors + the_hdr->alpha; i++ )
	if ( scanp[i] != 0 )
	{
	    free( (char *)scanp[i] );
	    break;
	}
    free( (char *)scanp );
    if ( nrawp )
	free( (char *)nrawp );
}
