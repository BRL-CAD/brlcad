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
 * avg4.c - Reduce image by half in X and Y, producing alphas even
 *          if they weren't there originally.
 * 
 * Author:	Rod Bogart & John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Jun 20 1986
 * Copyright (c) 1986, University of Utah
 * 
 */
#ifndef lint
static char rcs_ident[] = "$Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

static int bit_count[16] = {0, 63, 63, 127, 63, 127, 127,
    192, 63, 127, 127, 192, 127, 192, 192, 255};

int
main(argc, argv)
int	argc;
char	*argv[];
{
    char	*infname = NULL, *outfname = NULL;
    char	*cmd = cmd_name( argv );
    int		oflag = 0;
    int		rle_cnt;
    FILE	*outfile = stdout;
    int         i, j;
    int		new_xlen,
		new_ylen;
    int		rle_err;
    rle_hdr 	in_hdr, out_hdr;
    rle_pixel **rows0, **rows1, **rowsout;
    rle_pixel *ptr0, *ptr1, *ptrout, *alphptr;
    int		A, chan;
    
    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% o%-outfile!s infile%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    in_hdr.rle_file = rle_open_f( cmd, infname, "r" );
    rle_names( &in_hdr, cmd, infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd, outfname, "w" );

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	rle_addhist( argv, &in_hdr, &out_hdr );

	/* Force input to an even length line. */
	if ( (in_hdr.xmax - in_hdr.xmin) % 2 == 0 )
	    in_hdr.xmax++;

	new_xlen = (in_hdr.xmax - in_hdr.xmin + 1 ) / 2;
	new_ylen = (in_hdr.ymax - in_hdr.ymin + 2 ) / 2;
	out_hdr.xmin = in_hdr.xmin / 2;
	out_hdr.ymin = in_hdr.ymin / 2;
	out_hdr.xmax = out_hdr.xmin + new_xlen - 1;
	out_hdr.ymax = out_hdr.ymin + new_ylen - 1;

	out_hdr.alpha = 1;	/* Force alpha in output. */
	RLE_SET_BIT( out_hdr, RLE_ALPHA );

	out_hdr.rle_file = outfile;
	rle_put_setup( &out_hdr );

	/* Oink. */
	if ( rle_row_alloc( &in_hdr, &rows0 ) < 0 ||
	     rle_row_alloc( &in_hdr, &rows1 ) < 0 ||
	     rle_row_alloc( &out_hdr, &rowsout ) < 0 )
	    RLE_CHECK_ALLOC( cmd, 0, "image" );

	for ( j = 0; j < new_ylen*2; j+=2 )
	{
	    rle_getrow(&in_hdr, rows0 );
	    rle_getrow(&in_hdr, rows1 );

	    for (chan = RLE_ALPHA; chan < in_hdr.ncolors; chan++)
	    {
		ptr0 = &(rows0[chan][in_hdr.xmin]);
		ptr1 = &(rows1[chan][in_hdr.xmin]);
		ptrout = rowsout[chan];
		alphptr = rowsout[RLE_ALPHA];
		/*
		 * If we don't start out with an alpha channel in the
		 * original image, then we want to fake one up.  This
		 * works by counting the number of non-zero pixels in the
		 * R, G and B channels.  We set bits in the alpha channel
		 * for the non-zero pixels found, then use bit_count to
		 * convert this to reasonable coverage values.
		 */
		if ((chan == RLE_ALPHA) && (!in_hdr.alpha))
		{
		    bzero(alphptr, new_xlen);
		}
		else for( i = 0; i < new_xlen; i++)
		{
		    if (!in_hdr.alpha)
		    {
			*alphptr |= (*ptr0 ? 1 : 0) | (ptr0[1] ? 2 : 0) |
			    (*ptr1 ? 4 : 0) | (ptr1[1] ? 8 : 0);

			/* calc fake alpha from bit count */
			if (chan == (in_hdr.ncolors - 1))
			    *alphptr = bit_count[*alphptr];

			alphptr++;
		    }
		    A  = (int) *ptr0++ + (int) *ptr1++;
		    A += (int) *ptr0++ + (int) *ptr1++;
		    *ptrout++ = (rle_pixel) (A / 4);
		}
	    }
	    rle_putrow( rowsout, new_xlen, &out_hdr );
	}
	rle_puteof( &out_hdr );

	/* Skip last row if odd number of rows. */
	while ( rle_getskip( &in_hdr ) != 32768 )
	    ;

	rle_row_free( &in_hdr, rows0 );
	rle_row_free( &in_hdr, rows1 );
	rle_row_free( &out_hdr, rowsout );
    }

    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd, infname );
    
    exit( 0 );
}
