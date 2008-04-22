/*
 * rlebox.c - Find bounding box for an RLE image
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Feb 11 1987
 * Copyright (c) 1987, University of Utah
 */
static char rcsid[] = "$Header$";
/*
rlebox()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( main )
 *
 * Find a bounding box for an RLE image.
 *
 * Usage:
 *	rlebox [-v] [-c] [infile]
 * Inputs:
 *	-v:		Verbose mode - identifies numbers with text.
 *			Otherwise, just prints numbers.
 * 	-c:		Outputs corners of box in an order useful for use
 *			as arguments to the crop program:
 *			crop `rlebox -c foo.rle` foo.rle
 *	infile:		The input file.
 * Outputs:
 * 	Prints the bounding box for the rle file.  That is, it finds the
 *	minimum and maximum values of x and y for which there is some
 *	non-background data.
 * Assumptions:
 *
 * Algorithm:
 * 	Read the image file and find the smallest and largest X and Y
 *	coordinates of real image data.  Use raw interface for speed.
 */
int
main( argc, argv )
int argc;
char **argv;
{
    extern void rle_box();
    rle_hdr the_hdr;
    char * rlefname = NULL;
    int vflag = 0, cflag = 0, margin = 0;
    int xmin, xmax, ymin, ymax;
    int rle_cnt, rle_err;

    if ( scanargs( argc, argv, "% v%- c%- m%-margin!d infile%s",
		   &vflag, &cflag, &margin, &margin,
		   &rlefname ) == 0 )
	exit( 1 );

    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &the_hdr, cmd_name( argv ), rlefname, 0 );
    the_hdr.rle_file = rle_open_f(the_hdr.cmd, rlefname, "r");

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &the_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {

	rle_box( &the_hdr, &xmin, &xmax, &ymin, &ymax );

	/* If margin, enlarge bounds.  Don't let them go negative */
	if ( margin )
	{
	    xmax += margin;
	    ymax += margin;
	    xmin -= margin;
	    if ( xmin < 0 )
		xmin = 0;
	    ymin -= margin;
	    if ( ymin < 0 )
		ymin = 0;
	}

	if ( cflag )
	    printf( vflag ? "xmin=%d ymin=%d xmax=%d ymax=%d\n" :
		    "%d %d %d %d\n",
		    xmin, ymin, xmax, ymax );
	else
	    printf( vflag ? "xmin=%d xmax=%d ymin=%d ymax=%d\n" :
		    "%d %d %d %d\n",
		    xmin, xmax, ymin, ymax );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, the_hdr.cmd, rlefname );

    exit( 0 );
}
