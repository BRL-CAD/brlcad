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
 * to8.c - Convert color images to 8 bit dithered.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Feb 28 1986
 * Copyright (c) 1986, University of Utah
 * 
 */
#ifndef lint
static const char rcs_ident[] = "$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "rle.h"


void init_color(void);
void map_scanline(unsigned char **rgb, int n, int y, unsigned char *line);

short map[3][256];			/* Output color map. */
int colmap[216][3];
rle_pixel ** in_cmap;		/* color map in input file */

int dm16[16][16];


int modN[256], divN[256];

double disp_gam = 2.5;
double img_gam = 1.0;

/*****************************************************************
 * TAG( main )
 * 
 * Usage: to8 [-{iI} gamma] [-g gamma] [-o outfile] [infile]
 * 
 * Inputs:
 *	-i gamma:	Specify gamma of image. (default 1.0)
 *	-I gamma:	Specify gamma of display image was computed for.
 * 	to8 will also read picture comments from the input file to determine
 *			the image gamma.  These are
 *	image_gamma=	gamma of image (equivalent to -i)
 *	display_gamma=	gamma of display image was computed for.
 *			Command line arguments override values in the file.
 *		
 *	-g gamma:	Specify gamma of display. (default 2.5)
 * 	infile:		Input (color) RLE file.  Stdin used if not
 *			specified.
 * Outputs:
 * 	outfile:	Output dithered RLE file.  Stdout used
 *			if not specified.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */

int
main(int argc, char **argv)
{
    char * infname = NULL, * outfname = NULL;
    char comment[80];		/* for gamma comment */
    FILE * outfile = stdout;
    int oflag = 0, y, nrow, iflag = 0, gflag = 0;
    rle_hdr out_hdr;
    unsigned char ** scan, *outscan[2];
    unsigned char * buffer;
    int rle_cnt, rle_err;

    if ( scanargs( argc, argv,
		   "% Ii%-gamma!F g%-gamma!F o%-outfile!s infile%s",
		   &iflag, &img_gam, &gflag, &disp_gam,
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
    rle_dflt_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &rle_dflt_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {

	if ( rle_dflt_hdr.ncolors == 1 )
	{
	    fprintf( stderr, "%s is already black & white\n",
		     infname ? infname : "stdin" );
	    exit( 1 );
	}
	if ( rle_dflt_hdr.ncolors < 3 )
	{
	    fprintf( stderr, "%s is not RGB",
		     infname ? infname : "stdin" );
	    exit( 1 );
	}

	/* If no image gamma on command line, check comments in file */
	if ( ! iflag )
	{
	    char * v;
	    if ( (v = rle_getcom( "image_gamma", &rle_dflt_hdr )) != NULL )
	    {
		img_gam = atof( v );
		/* Protect against bogus information */
		if ( img_gam == 0.0 )
		    img_gam = 1.0;
		else
		    img_gam = 1.0 / img_gam;
	    }
	    else if ( (v = rle_getcom( "display_gamma", &rle_dflt_hdr )) != NULL )
	    {
		img_gam = atof( v );
		/* Protect */
		if ( img_gam == 0.0 )
		    img_gam = 1.0;
	    }
	}

	out_hdr = rle_dflt_hdr;
	if ( rle_cnt == 0 )
	    outfile = rle_open_f("to8", outfname, "w");
	rle_dflt_hdr.xmax -= rle_dflt_hdr.xmin;
	rle_dflt_hdr.xmin = 0;
	nrow = rle_dflt_hdr.xmax;
	buffer = (unsigned char *)malloc( nrow );
	rle_row_alloc( &rle_dflt_hdr, &scan );

	if ( rle_dflt_hdr.alpha )
	{
	    outscan[0] = scan[-1];
	}
	outscan[1] = buffer;

	/* Use input color map, too */
	in_cmap = buildmap( &rle_dflt_hdr, 3, img_gam, 1.0 );

	init_color();
	out_hdr.ncolors = 1;
	out_hdr.ncmap = 3;
	out_hdr.cmaplen = 8;	/* 256 entries */
	out_hdr.cmap = (rle_map *)map;

	/* Record gamma color map was computed for */
	sprintf( comment, "display_gamma=%g", disp_gam );
	rle_putcom( comment, &out_hdr );

	/*
	 * Give it a background color of black, since the real background
	 * will be dithered anyway.
	 */
	if ( rle_dflt_hdr.background != 0 )
	{
	    out_hdr.bg_color = (int *)malloc( sizeof( int ) );
	    out_hdr.bg_color[0] = 0;
	}

	out_hdr.rle_file = outfile;

	rle_addhist( argv, &rle_dflt_hdr, &out_hdr );

	rle_put_setup( &out_hdr );

	while ( (y = rle_getrow( &rle_dflt_hdr, scan )) <=
		rle_dflt_hdr.ymax )
	{
	    map_scanline( scan, nrow, y, buffer );
	    rle_putrow( &outscan[1], nrow, &out_hdr );
	}

	rle_puteof( &out_hdr );

	rle_row_free( &rle_dflt_hdr, scan );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}


/*
 * Initialize the 8 bit color map.  Choice of
 * "alpha-1" (perceptual flavor) or linear maps
 */
void
init_color(void)
{
    int     i;

    dithermap( 6, disp_gam, colmap, divN, modN, dm16 );

    for (i = 0; i < 216; i++)
    {
	map[0][i] = colmap[i][0] << 8;
	map[1][i] = colmap[i][1] << 8;
	map[2][i] = colmap[i][2] << 8;
    }

}

/*
 * Map a scanline to 8 bits through the dither matrix.
 */
#define DMAP(v,x,y)	(modN[v]>dm16[x][y] ? divN[v] + 1 : divN[v])

void
map_scanline(unsigned char **rgb, int n, int y, unsigned char *line)
{
	register unsigned char *r, *g, *b;
	register int i, col, row;
	for ( row = y % 16, col = 0, i = 0, r = rgb[0], g = rgb[1], b = rgb[2];
		  i < n; i++, r++, g++, b++, col = ((col + 1) & 15) )
		line[i] = DMAP(in_cmap[0][*r], col, row) +
			  DMAP(in_cmap[1][*g], col, row) * 6 +
			  DMAP(in_cmap[2][*b], col, row) * 36;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
