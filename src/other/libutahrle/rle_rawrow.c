/* 
 * rle_rawrow.c - Convert RLE "raw" input to "row" input.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Mon Jun 18 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( rle_rawtorow )
 * 
 * Convert a "raw" scanline to a row format.
 * Inputs:
 * 	the_hdr:	RLE header describing the image.
 * 	raw:		Pointer to pointers to vectors of "raw" data.
 * 	nraw:		Pointer to vector of lengths of raw data vectors.
 * Outputs:
 * 	outrows:	Pointer to pointers to scanline data for
 * 			this scanline.
 * Algorithm:
 * 	Interpret successive "op codes", filling in scanline array.
 * 	Scanline between xmin and xmax is prefilled with background
 * 	color, if one is given.
 */
void    
rle_rawtorow(the_hdr, raw, nraw, outrows)
rle_hdr * the_hdr;
rle_op ** raw;
int * nraw;
rle_pixel ** outrows;
{
    register int i, j;
    register rle_pixel * outptr;
    int chan;
    
    for (chan = -the_hdr->alpha; chan < the_hdr->ncolors; chan++)
	if ( RLE_BIT( *the_hdr, chan ) )
	{
	    if ( chan >= 0 && the_hdr->background == 2 && the_hdr->bg_color &&
		 the_hdr->bg_color[chan] != 0 )
	    {
		j = the_hdr->bg_color[chan];
		for ( i = the_hdr->xmin,
		      outptr = &outrows[chan][the_hdr->xmin];
		      i <= the_hdr->xmax;
		      i++, outptr++ )
		    *outptr = j;
	    }
	    else
		bzero( (char *)&outrows[chan][the_hdr->xmin],
		       the_hdr->xmax - the_hdr->xmin + 1 );

	    for( i = 0; i < nraw[chan]; i++ )
	    {
		outptr = &(outrows[chan][raw[chan][i].xloc]);
		switch (raw[chan][i].opcode)
		{
		case RByteDataOp:
		    bcopy( (char *)raw[chan][i].u.pixels, (char *)outptr,
			   raw[chan][i].length );
		    break;

		case RRunDataOp:
		    for ( j = raw[chan][i].length; j > 0; j--)
			*(outptr++) = (rle_pixel) raw[chan][i].u.run_val;
		    break;
		}
	    }
	}
}
