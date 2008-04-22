/*
 * unexp.c - take an exponential RLE file and make an integer file.
 *
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Aug 21 17:02:45 1985
 *
 *		Based on code by Spencer Thomas
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )

#define RLE_EXPONENT (in_hdr.ncolors-1) /* Assume last channel is exp */

rle_pixel **in_rows;
rle_pixel **out_rows;

int truncations = 0;
rle_hdr in_hdr, out_hdr;
rle_pixel r_in, g_in, b_in;

int
main(argc,argv)
int argc;
char *argv[];
{
    register int x, chan;
    int y;
    int scan_flag = 0;
    int max_flag = 0;
    int verbose_flag = 0;
    int print_flag = 0;
    int oflag = 0;
    float maxval, tmp, expnt, tmp_max;
    char *infilename = NULL, *outfilename = NULL;
    FILE *outfile = stdout;
    int rle_cnt, rle_err;
    long start;

    if (scanargs(argc, argv, "% v%- p%- s%- m%-maxval!f o%-outfile!s infile!s",
                 &verbose_flag, &print_flag, &scan_flag, &max_flag, &maxval,
		 &oflag, &outfilename, &infilename ) == 0)
    {
	exit(-1);
    }

    (void)rle_hdr_init( &in_hdr );
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infilename, "r");
    rle_names( &in_hdr, cmd_name( argv ), infilename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfilename, 0 );
    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	start = ftell( in_hdr.rle_file );
	if ( start < 0 && !max_flag && !scan_flag )
	{
	    fprintf( stderr,
		     "%s: Can't pipe input unless either -m or -s is given.\n",
		     cmd_name( argv ) );
	    exit( 1 );
	}

	if ( (rle_err = rle_get_setup( &in_hdr )) != RLE_SUCCESS )
	    break;

	if (! rle_getcom( "exponential_data", &in_hdr ))
	    fprintf(stderr, "%s: warning - no exponential_data comment\n",
		    cmd_name( argv ));

	if (in_hdr.ncolors < 2)
	{
	    fprintf(stderr,
		    "%s: File does not contain exponent channel.\n",
		    cmd_name( argv ));
	    exit(-4);
	}

	if ( rle_row_alloc( &in_hdr, &in_rows ) < 0 )
	    RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );

	/* If maximum value isn't given, slosh through file to find it */
	if (! max_flag)
	{
	    maxval = 0.0;
	    for (y = in_hdr.ymin; y <= in_hdr.ymax; y++)
	    {
		if ((verbose_flag) && (y % 100) == 0)
		    fprintf(stderr, "Scanning row %d...\n", y);

		rle_getrow( &in_hdr, in_rows );

		for (x = in_hdr.xmin; x <= in_hdr.xmax; x++)
		{
		    expnt = ldexp( 1/256.0, in_rows[RLE_EXPONENT][x] - 127);
		    tmp = -2000.0;
		    for (chan = 0; chan < RLE_EXPONENT; chan++)
			tmp = MAX( tmp, (float)in_rows[chan][x] );

		    tmp *= expnt;
		    maxval = MAX( maxval, tmp );
		}
	    }
	    if (scan_flag || print_flag || verbose_flag)
		fprintf(stderr, "Maximum value: %1.8g\n", maxval);

	    if (scan_flag) exit(0);

	    fseek( in_hdr.rle_file, start, 0 );

	    rle_get_setup( &in_hdr );
	}

	/* Open output file */

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	rle_delcom( "exponential_data", &out_hdr );
	out_hdr.ncolors = in_hdr.ncolors - 1;
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfilename, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	rle_put_setup( &out_hdr );

	out_hdr.xmin = 0;
	out_hdr.xmax -= in_hdr.xmin;

	if (rle_row_alloc( &out_hdr, &out_rows ) < 0)
	    RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );

	/*
	 * Convert byte/exponent form into straight RLE.
	 * Alpha is passed straight through.
	 */

	if ( maxval == 0.0 )
	    maxval = 1.0;
	else
	    maxval /= 255;	/* Pre-scale to 0-255 */

	for (y = in_hdr.ymin; y <= in_hdr.ymax; y++)
	{
	    if (((y % 100) == 0) && verbose_flag)
		fprintf(stderr, "Processing row %d...\n", y);

	    rle_getrow( &in_hdr, in_rows );

	    for (x = in_hdr.xmin; x <= in_hdr.xmax; x++)
	    {
		/* Only bother with pixels with coverage */
		if (in_rows[RLE_ALPHA][x])
		{
		    expnt = ldexp( 1/256.0, in_rows[RLE_EXPONENT][x] - 127 );
		    tmp_max = -1000;
		    for( chan = 0; chan < RLE_EXPONENT; chan++ )
			tmp_max = MAX( tmp_max, in_rows[chan][x] );

		    tmp = expnt / maxval * tmp_max;
		    if (tmp > 255)
		    {
			tmp = 255 / tmp;
			truncations++;
		    }
		    else
			tmp = 1.0;

		    tmp *= expnt / maxval;
		    for( chan = 0; chan < RLE_EXPONENT; chan++ )
			out_rows[chan][x-in_hdr.xmin] = (rle_pixel) (int)
			    (in_rows[chan][x] * tmp);
		    out_rows[RLE_ALPHA][x-in_hdr.xmin] =
			in_rows[RLE_ALPHA][x];
		}
		else
		    for( chan = RLE_ALPHA; chan < RLE_EXPONENT; chan++ )
			out_rows[chan][x-in_hdr.xmin] = 0;

	    }
	    rle_putrow( out_rows, (in_hdr.xmax - in_hdr.xmin) + 1,
			&out_hdr );
	}
	rle_puteof( &out_hdr );

	if (truncations)
	    fprintf(stderr,"%s: %d bytes truncated (sorry...)\n",
		    cmd_name( argv ), truncations);

	rle_row_free( &in_hdr, in_rows );
	rle_row_free( &out_hdr, out_rows );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infilename );

    exit( 0 );
}
