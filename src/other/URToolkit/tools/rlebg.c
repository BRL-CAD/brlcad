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
 * background.c - Create a background frame of any color/alpha
 *
 * Author:	Rod Bogart & John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Jun 24 1986
 * Copyright (c) 1986, University of Utah
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

int
main(argc, argv)
int	argc;
char	*argv[];
{
    rle_hdr	the_hdr;
    rle_op backrundata[4], *rows[4];
    char * outfilename = NULL;
    static int numrundata[4] = { 1, 1, 1, 1};
    float top_inten = 1.0 , bot_inten = 0.1, delta = 0;
    unsigned char a, r, g, b;
    int no_alpha_given;
    float scale, inten;
    int linear_ramp = 0;
    int i, j, alpha = -1, red, green, blue;
    int oflag = 0, sizeflag = 0, xsize = 512, ysize = 480, vbackflag = 0;

    if ( scanargs( argc, argv,
		   "% s%-xsize!dysize!d l%- v%-top%fbottom%f o%-outfile!s \n\
		red!d green!d blue!d alpha%d",
		   &sizeflag, &xsize, &ysize, &linear_ramp, &vbackflag,
		   &top_inten, &bot_inten, &oflag, &outfilename,
		   &red, &green, &blue, &alpha ) == 0 )
    {
	exit(-1);
    }

    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &the_hdr, cmd_name( argv ), outfilename, 0 );

    the_hdr.rle_file = rle_open_f(the_hdr.cmd, outfilename, "w");
    rle_addhist( argv, (rle_hdr *)0, &the_hdr );

    if ((top_inten < 0.0) || (top_inten > 1.0) || (bot_inten < 0.0) ||
	(bot_inten > 1.0))
    {
	fprintf(stderr,"background: -v bounds must be 0.0 to 1.0");
	exit(-1);
    }

    if (no_alpha_given = (alpha == -1))
	alpha = 255;

    for (i=0; i < 4; i++)
    {
	backrundata[i].opcode = RRunDataOp;
	backrundata[i].xloc = 0;
	backrundata[i].length = xsize;
    }
    /* Initialize scanline */
    if (!vbackflag)
    {
	backrundata[0].u.run_val = alpha;
	backrundata[1].u.run_val = red;
	backrundata[2].u.run_val = green;
	backrundata[3].u.run_val = blue;
    }
    else
	delta = (top_inten - bot_inten) / ysize;

    for(i=0; i < 4; i++)
	rows[i] = &(backrundata[i]);

    RLE_SET_BIT(the_hdr, RLE_ALPHA);

    the_hdr.xmax = xsize - 1;
    the_hdr.ymax = ysize - 1;
    the_hdr.alpha = 1;
    rle_put_setup( &the_hdr );

    a = alpha;
    scale = (top_inten - bot_inten) / (ysize * ysize);
    inten = bot_inten;
    for (j = 0; j < ysize; j++)
    {
	if (vbackflag)
	{
	    if (linear_ramp)
		inten += delta;
	    else
	    {
		if (top_inten > bot_inten)
		    inten = scale * (float) (j * j) + bot_inten;
		else
		    inten = top_inten - (scale * (float)
			    ((ysize - j) * (ysize - j)));
	    }
	    if (!no_alpha_given)
		/* Cast to int to get around HP compiler bug. */
		a = (int) ((float)alpha * inten);
	    /* Cast to int to get around HP compiler bug. */
	    r = (int) ((float)red * inten);
	    g = (int) ((float)green * inten);
	    b = (int) ((float)blue * inten);
	    backrundata[0].u.run_val = a;
	    backrundata[1].u.run_val = r;
	    backrundata[2].u.run_val = g;
	    backrundata[3].u.run_val = b;
	}
	rle_putraw( &rows[1], &numrundata[1], &the_hdr );
    }
    rle_puteof( &the_hdr );
    exit( 0 );
}
