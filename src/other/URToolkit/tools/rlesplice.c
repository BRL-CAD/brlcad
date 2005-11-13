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
/* rlesplice.c - Splice two RLE files together horizontally or vertically.
 * 		 Pad smaller dimensioned image with its background color or
 * 		 black.
 *          
 * Author:	Martin R. Friedmann (martin@media-lab.media.mit.edu)
 * 		Vision and Modeling Group
 * 		Massachusetts Institute of Technology
 * Date:	Mon Apr 23 1990 (The Apocalypse)
 * Copyright (c) 1990, Martin R. Friedmann
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

#define MALLOC_ERR RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 )

#define VERT_FLAG  0x01		/* Command line flags */
#define HORIZ_FLAG 0x02		/* must match appearance in scanargs */
#define FOO_FLAG   0x04

#define Max(x, y) (((x) > (y)) ? (x) : (y) )
#define Min(x, y) (((x) < (y)) ? (x) : (y) )

static void	bfill();
char  *progname = NULL;


int compatible_hdr(hdr_1, hdr_2)
rle_hdr *hdr_1, *hdr_2;
{
#define Check(thing,printthing) \
    if (hdr_1->thing != hdr_2->thing) \
    { \
	fprintf( stderr, "%s: %s does not match\n", progname, printthing); \
	return 0; \
    }
    Check(ncolors, "Number of color channels");
    Check(alpha, "Existance of alpha channel");
    Check(ncmap, "Number of colormap channels");
    Check(cmaplen, "Colormap length");
    return 1;
#undef Check
}

void
rle_row_clear( the_hdr, scanline )
rle_hdr * the_hdr;
rle_pixel *scanline[];
{
    int nc;
    rle_pixel bg_color[255];
    
    if ( the_hdr->alpha && RLE_BIT( *the_hdr, -1 ) )
	bfill( (char *)scanline[-1], the_hdr->xmax + 1, 0 );

    /* Clear to background if specified */
    for ( nc = 0; nc < the_hdr->ncolors; nc++ ) {
	bg_color[nc] = (the_hdr->background == 2 ) ? the_hdr->bg_color[nc] : 0;
	if ( RLE_BIT( *the_hdr, nc ) )
	    bfill( (char *)scanline[nc], the_hdr->xmax+1, bg_color[nc] );
    }
}

void
main(argc, argv)
int	argc;
char	*argv[];
{
    char	*infname1 = NULL, *outfname = NULL;
    char	*infname2 = NULL;
    int		oflag = 0, hvflag = 0, cflag = 0;
    int		rle_cnt = 0;
    FILE	*outfile = stdout;
    int         j;
    int		new_xlen,
		new_ylen;
    int		rle_err1, rle_err2 = 0;
    rle_hdr 	in_hdr1, in_hdr2, out_hdr;
    int		xmin, ymin, width1, width2, height1, height2;

    progname = cmd_name( argv );
    in_hdr1 = *rle_hdr_init( NULL );
    in_hdr2 = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% hv!- c%- o%-outfile!s infile1!s infile2!s",
		  &hvflag, &cflag, &oflag, &outfname,
		  &infname1, &infname2 ) == 0 )
	exit( 1 );
    
    in_hdr1.rle_file = rle_open_f( progname, infname1, "r" );
    in_hdr2.rle_file = rle_open_f( progname, infname2, "r" );
    rle_names( &in_hdr1, progname, infname1, 0 );
    rle_names( &in_hdr2, progname, infname2, 0 );
    rle_names( &out_hdr, progname, outfname, 0 );


    /* Both standard input? */
    if ( in_hdr1.rle_file == in_hdr2.rle_file )
    {
	fprintf( stderr, "Both files can't be on standard input.\n" );
	exit( -1 );
    }

    for ( rle_cnt = 0;
	  (rle_err1 = rle_get_setup( &in_hdr1 )) == RLE_SUCCESS &&
	  (rle_err2 = rle_get_setup( &in_hdr2 )) == RLE_SUCCESS;
	  rle_cnt++ ) {
	rle_pixel     **rows1, **rows2, **rowsout;
	rle_pixel      *ptr1, *ptr2, *ptrout;
	int		start_scan1, start_scan2, pad1, pad2;
	int 		chan;

	if ( rle_cnt == 0 )
	    outfile = rle_open_f( progname, outfname, "w" );

	if (!compatible_hdr(&in_hdr1, &in_hdr2)) {
	    fprintf(stderr, "%s: Non-compatible rle files: %s and %s\n",
		    progname, infname1, infname2);
	    exit(1);
	}

	(void)rle_hdr_cp( &in_hdr1, &out_hdr );
	rle_addhist( argv, &in_hdr1, &out_hdr );

	/* preserve xmin and ymin from in_hdr1 */
	xmin = in_hdr1.xmin;
	ymin = in_hdr1.ymin;
	width1 = in_hdr1.xmax - in_hdr1.xmin + 1;
	width2 = in_hdr2.xmax - in_hdr2.xmin + 1;
	height1 = in_hdr1.ymax - in_hdr1.ymin + 1;
	height2 = in_hdr2.ymax - in_hdr2.ymin + 1;

	if (hvflag == VERT_FLAG) {
	    new_xlen = Max( width1, width2 );
	    new_ylen = height1 + height2;
	} else {
	    new_xlen = width1 + width2;
	    new_ylen = Max( height1, height2 );
	}

	out_hdr.xmin = xmin;
	out_hdr.ymin = ymin;
	out_hdr.xmax = xmin + new_xlen - 1;
	out_hdr.ymax = ymin + new_ylen - 1;

	out_hdr.rle_file = outfile;
	rle_put_setup( &out_hdr );

	/* Oink. */
	if ( rle_row_alloc( &in_hdr1, &rows1 ) < 0 ||
	     rle_row_alloc( &in_hdr2, &rows2 ) < 0 ||
	     rle_row_alloc( &out_hdr, &rowsout ) < 0 )
	    MALLOC_ERR;

	rle_row_clear( &in_hdr1, rows1 );
	rle_row_clear( &in_hdr2, rows2 );
	rle_row_clear( &out_hdr, rowsout );

	if ( hvflag == HORIZ_FLAG ) {
	    int diff = height1 - height2 ;
	    start_scan1 = start_scan2 = pad1 = pad2 = 0;
	    if ( height1 < height2 ) 
		start_scan1 = (cflag) ? -diff/2 : new_ylen - height1;
	    else if ( height2 < height1 ) 
		start_scan2 = (cflag) ? diff/2 : new_ylen - height2;
	} else {
	    /* upside down remember */
	    start_scan2 = pad1 = pad2 = 0;
	    start_scan1 = height2;
	    
	    if ( width1 < width2 ) 
		pad1 = (cflag) ? (width2 - width1)/2 : 0;
	    else if (width2 < width1 ) 
		pad2 = (cflag) ? (width1 - width2)/2 : 0;
	}

	for ( j = 0; j < new_ylen; j++ ) {
	    if ( start_scan1 <= j && hvflag == VERT_FLAG ) {
		start_scan2 = new_ylen + 1; /* never again */
		rle_row_clear( &out_hdr, rowsout );
	    }
	    if ( start_scan1 <= j )
		rle_getrow(&in_hdr1, rows1 );
	    if ( start_scan2 <= j )
		rle_getrow(&in_hdr2, rows2 );
	    
	    for (chan = RLE_ALPHA; chan < in_hdr1.ncolors; chan++)
	    {
		if ((chan == RLE_ALPHA) && (!in_hdr1.alpha))
		    continue;
		ptr1 = &(rows1[chan][in_hdr1.xmin]);
		ptr2 = &(rows2[chan][in_hdr2.xmin]);
		ptrout = rowsout[chan];
		
		if ( start_scan1 <= j )
		    bcopy( ptr1, ptrout + pad1, width1 );
		if ( start_scan2 <= j )
		    bcopy( ptr2,((char *)ptrout) + pad2 +
			  ((hvflag == VERT_FLAG) ? 0 : width1), width2 );
		
	    }
	    rle_putrow( rowsout, new_xlen, &out_hdr );
	}
	rle_puteof( &out_hdr );
	
	rle_row_free( &in_hdr1, rows1 );
	rle_row_free( &in_hdr2, rows2 );
	rle_row_free( &out_hdr, rowsout );
    }

    if ( rle_cnt == 0 || (rle_err1 != RLE_EOF && rle_err1 != RLE_EMPTY &&
			  rle_err2 != RLE_EOF && rle_err2 != RLE_EMPTY ) ) {
	if (rle_err1 != RLE_SUCCESS)
	    rle_get_error( rle_err1, progname, infname1 );
	if (rle_err2 != RLE_SUCCESS)
	    rle_get_error( rle_err2, progname, infname2 );
    }
    exit( 0 );
}

/* Fill buffer at s with n copies of character c.  N must be <= 65535*/
/* ARGSUSED */
static void bfill( s, n, c )
char *s;
int n, c;
{
#ifdef vax
    asm("   movc5   $0,*4(ap),12(ap),8(ap),*4(ap)");
#else
    while ( n-- > 0 )
	*s++ = c;
#endif
}
