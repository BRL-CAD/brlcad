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
 * rlegrid.c - Generate grids and checkerboards for test images
 * 
 * Author:	James Painter
 * 		Computer Science
 * 		University of Utah
 * Date:	Tue November 20, 1990 
 * Copyright (c) 1990, University of Utah
 */
#ifndef lint
char rcsid[] = "$Header$";
#endif
/*
rlegrid()			Tag the file.
*/

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( main )
 * 
 * Generate simple grids
 *
 * Usage:
 * 	rlegrid [-o outfile] [-s xsize ysize] [-w width] [-f fb_color] 
 *              [-b bg_color][-c]
 *
 * Outputs:
 * 	-o outfile:	The output RLE file.  Default stdout.
 * 			"-" means stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 */
void
main( argc, argv )
int argc;
char **argv;
{
    char       *outfname = NULL;
    int 	oflag = 0, sflag=0, wflag=0, fflag=0, bflag=0, cflag=0;
    int         xsize=512, ysize=512, width=16, fg_color=255, bg_color=0;
    int		x,y,i;
    FILE       *outfile;
    rle_hdr out_hdr;
    rle_op **scanraw[2];	/* space for two raw scanline buffers */
    rle_op *p, *q;
    int    *nrawp[2];
    unsigned char fg;

    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, 
 "% o%-outfile!s s%-xsize!dysize!d w%-width!d f%-fg_color!d b%-bg_color!d c%-",
		   &oflag, &outfname, 
		   &sflag, &xsize, &ysize,
		   &wflag, &width,
		   &fflag, &fg_color,
		   &bflag, &bg_color,
		  &cflag )
	     == 0 )
       exit( 1 );

    fg = fg_color;

    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );
    
    /* Set up the output header.
     */
    (void)rle_hdr_init( &out_hdr );
    rle_names( &out_hdr, cmd_name( argv ), outfname, 0 );
    out_hdr.rle_file = outfile;
    out_hdr.xmax = xsize -1;
    out_hdr.ymax = ysize -1;
    out_hdr.ncolors = 1;
    out_hdr.alpha = 0;


    /* Add to the history comment. */
    rle_addhist( argv, (rle_hdr *)0, &out_hdr );

    /* Write the output image header. */
    rle_put_setup( &out_hdr );


    /* Allocate memory into which the image scanlines can be read.
     */
    if ( rle_raw_alloc( &out_hdr, scanraw+0, nrawp+0 ) < 0  ||
	 rle_raw_alloc( &out_hdr, scanraw+1, nrawp+1 ) < 0 )
	RLE_CHECK_ALLOC( cmd_name( argv ), 0, "image memory" );

    /* Fill the scan lines */
    if (cflag)
      {
	/* checkerboard option */
	p = scanraw[0][0];
	q = scanraw[1][0];
	*nrawp[0] = *nrawp[1] = 0;
	for(x=0; x<xsize;x+=2*width)
	  {
	    p->opcode = RRunDataOp;
	    p->xloc = x;
	    p->length = width;
	    *q = *p;	
	    p->u.run_val = fg_color;
	    q->u.run_val = bg_color;
	    (*nrawp[0])++; (*nrawp[1])++;
	    
	    p++; q++;
	    p->opcode = RRunDataOp;
	    p->xloc = x+width;
	    p->length = (x+width < xsize) ? width : (xsize-x);
	    *q = *p;
	    p->u.run_val = bg_color;
	    q->u.run_val = fg_color;
	    (*nrawp[0])++; (*nrawp[1])++;
	    p++; q++;
	  }
      }
    else
      {  /* default: grid */

	p = scanraw[0][0];
	p->opcode = RRunDataOp;
	p->xloc = 0;
	p->length = xsize;
	p->u.run_val = fg_color;
	*nrawp[0] = 1;

	/* grided scanline */
	p = scanraw[1][0];
	*nrawp[1] = 0;
	for(x=0; x<xsize;x+=width)
	  {
	    p->opcode = RByteDataOp;
	    p->xloc = x;
	    p->length = 1;
	    p->u.pixels = (rle_pixel *) &fg;
	    (*nrawp[1])++;
	    
	    p++;
	    p->opcode = RRunDataOp;
	    p->xloc = x+1;
	    p->length = (x+width < xsize) ? width-1 : (xsize-x);
	    p->u.run_val = bg_color;
	    (*nrawp[1])++;
	    p++;
	  }
      }

    /* write the output file */
    for ( y = 0; y < ysize; y++ )
      {
	i = (cflag) ? ((y/width) %2) : ((y %width) != 0);
	rle_putraw( scanraw[i], nrawp[i], &out_hdr );
      }

    /* Free memory. */
    rle_raw_free( &out_hdr, scanraw[0], nrawp[0] );
    rle_raw_free( &out_hdr, scanraw[1], nrawp[1] );

    /* Write an end-of-image code. */
    rle_puteof( &out_hdr );

    exit( 0 );
}
