/* 
 * rlescale.c - Generate a gray-scale RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		Electrical Engineering & Computer Science Dept.
 * 		University of Michigan
 * Date:	Mon Jun 13 1988
 * Copyright (c) 1988, University of Michigan
 *
 * Usage:
 * 	rlescale [-c] [-l] [-n nsteps] [xsize] [ysize]
 *
 * Generates a xsize x ysize gray-scale RLE file (by default 480x512).
 * Has color squares along the bottom, with nsteps (default 16) gray
 * log stepped intensity gray rectangles above that.  The -l flag
 * makes a linear scale instead of a log scale.  If -c is
 * specified, does separate white, red, green, and blue scales. 
 */

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"
#include <math.h>

void
main( argc, argv )
int argc;
char ** argv;
{
    int xsize = 512, ysize = 480, nsteps = 16, cflag = 0, oflag = 0;
    int lflag = 0;
    int y, i, * nscan;
    rle_hdr hdr;
    rle_op ** scans;
    char buf[80], *out_fname = NULL;

    if ( scanargs(argc, argv,
		  "% c%- l%- n%-nsteps!d o%-outfile!s xsize%d ysize%d\n(\
\tDraw a step scale in gray or color.\n\
\t-c\tDraw color scales: gray, red, green, blue\n\
\t-l\tChange linearly between steps (default is exponential).\n\
\t-n\tNumber of steps in scale.)",
		  &cflag, &lflag, &nsteps, &nsteps,
		  &oflag, &out_fname, &xsize, &ysize ) == 0 )
	exit( 1 );

    /* Sanity check parameters -- Runs must be at least 3 pixels long */
    if ( xsize < 3 * nsteps || xsize < 24 )
    {
	fprintf( stderr,
       "Image isn't wide enough for %d steps, should be at least %d pixels.\n",
		 nsteps, 24 < 3 * nsteps ? 3 * nsteps : 24 );
	exit( 1 );
    }

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), out_fname, 0 );

    hdr.ncolors = 3;
    hdr.alpha = 0;	/* No coverage mask */
    hdr.xmax = xsize - 1;
    hdr.ymax = ysize - 1;
    hdr.rle_file = rle_open_f(hdr.cmd, out_fname, "w");

    rle_addhist( argv, (rle_hdr *)0, &hdr );

    sprintf( buf, "IMAGE_TYPE=%s scale image with %d log steps",
	     cflag ? "Color" : "Gray", nsteps );  
    rle_putcom( buf, &hdr );

    /* Allocate storage for the output rows */
    if ( rle_raw_alloc( &hdr, &scans, &nscan ) < 0 )
	RLE_CHECK_ALLOC( hdr.cmd, 0, "output image data" );

    /* Create the header in the output file */
    rle_put_setup( &hdr );

    /* Create the scanline for the color squares */
    for ( i = 0; i < 8; i++ )
    {
	scans[0][i].u.run_val = (i & 1) ? 255 : 0;
	scans[0][i].opcode = RRunDataOp;
	scans[0][i].xloc = i * xsize / 8;
	scans[0][i].length = (i + 1) * xsize / 8 - scans[0][i].xloc;
	scans[1][i] = scans[0][i];
	scans[1][i].u.run_val = (i & 2) ? 255 : 0;
	scans[2][i] = scans[0][i];
	scans[2][i].u.run_val = (i & 4) ? 255 : 0;
    }
    nscan[0] = 8;
    nscan[1] = 8;
    nscan[2] = 8;

    /* Write the color squares */
    for ( y = 0; y < ysize / 8; y++ )
	rle_putraw( scans, nscan, &hdr );

    /* Create the data for the scale */
    for ( i = 0; i < nsteps; i++ )
    {
	if ( lflag )
	    /* Linear steps. */
	    scans[0][i].u.run_val = (int)(i * 255.0 / (nsteps - 1) + 0.5);
	else
	    /* Exponential steps. */
	    scans[0][i].u.run_val =
		(int)(255.0 / pow(2.0, i*(8.0/nsteps)) + 0.5);
	scans[0][i].opcode = RRunDataOp;
	scans[0][i].xloc = i * xsize / nsteps;
	scans[0][i].length = (i + 1) * xsize / nsteps - scans[0][i].xloc;
	scans[1][i] = scans[0][i];
	scans[2][i] = scans[0][i];
    }
    nscan[0] = nsteps;
    nscan[1] = nsteps;
    nscan[2] = nsteps;

    /* Draw the scale */
    if ( !cflag )
	for ( ; y < ysize; y++ )
	    rle_putraw( scans, nscan, &hdr );
    else
    {
	/* blue scale */
	nscan[0] = nscan[1] = 0;
	for ( ; y < ysize * 11./32. ; y++ )
	    rle_putraw( scans, nscan, &hdr );
	/* green scale */
	nscan[1] = nsteps;
	nscan[2] = 0;
	for ( ; y < ysize * 18./32.; y++ )
	    rle_putraw( scans, nscan, &hdr );
	/* red scale */
	nscan[0] = nsteps;
	nscan[1] = 0;
	for ( ; y < ysize * 25./32.; y++ )
	    rle_putraw( scans, nscan, &hdr );
	/* white scale */
	nscan[1] = nscan[2] = nsteps;
	for ( ; y < ysize; y++ )
	    rle_putraw( scans, nscan, &hdr );
    }

    rle_raw_free( &hdr, scans, nscan );

    rle_puteof( &hdr );

    exit( 0 );
}
