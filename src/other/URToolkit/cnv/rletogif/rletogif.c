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
/* rletogif.c */

/*************************************************************************
 * rletogif -   Main program for RLE to GIF graphics format conversion.
 *              Works only with 8-bit RLE files.  Use to8 to convert
 *              from 24-bit files.
 *	Will detectect if mcut has been used to reduce the number of
 *	colors and act accordingly.
 *
 *              Written by Bailey Brown, Jr.  May 24, 1990.
 *************************************************************************
 */

#include <stdio.h>
#include "rle.h"
#include "rletogif.h"

CONST_DECL char *MY_NAME = "rletogif";

static rle_hdr in_hdr;
static rle_pixel **scan;
static gif_pixel **scanbuf;  /* needed to read all scanlines before
				converting to gif (rle is upside down) */
static int pix_mask;		/* To make sure pixels have specified #bits. */

int get_scanlines();
int get_color_bits();
void GIFEncode();

void
main(argc, argv)
int argc;
char *argv[];
{
    char 	*outfname = NULL,
    		*infname = NULL;
    int		oflag = 0;
    int i, color_bits;

    MY_NAME = cmd_name( argv );

    if ( scanargs( argc, argv, "% o%-outfile.gif!s infile.rle%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    in_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &in_hdr, MY_NAME, infname, 0 );
    in_hdr.rle_file = rle_open_f( MY_NAME, infname, "r" );

    rle_get_setup_ok( &in_hdr, NULL, NULL);
    in_hdr.xmax -= in_hdr.xmin;
    in_hdr.xmin = 0;

    if (in_hdr.ncolors > 1) error("Input file should have only 1 channel.");

    get_scanlines();
    color_bits = get_color_bits(in_hdr.comments);
    if ( in_hdr.ncmap && color_bits > in_hdr.cmaplen )
	color_bits = in_hdr.cmaplen;
    else if ( in_hdr.ncmap == 0 )
    {
	in_hdr.cmap = (rle_map *)malloc( sizeof(rle_map) * (1 << color_bits) );
	in_hdr.cmaplen = color_bits;
	for ( i = 0; i < (1 << color_bits); i++ )
	    in_hdr.cmap[i] = i << (16 - color_bits);
    }
    pix_mask = (1 << color_bits) - 1;

    GIFEncode( outfname, in_hdr.xmax - in_hdr.xmin + 1,
		in_hdr.ymax - in_hdr.ymin + 1, 0, 0,
	       color_bits, &in_hdr.cmap[0],
	       &in_hdr.cmap[in_hdr.ncmap > 1 ? 1 << in_hdr.cmaplen : 0],
	       &in_hdr.cmap[in_hdr.ncmap > 2 ? 2 << in_hdr.cmaplen : 0],
	       getpixel );
    rle_row_free(&in_hdr, scan);
    for (i = 0; i <= in_hdr.ymax - in_hdr.ymin; i++ ) free(scanbuf[i]);
    free(scanbuf);
}

/*
 * get_scanlines() reads in all RLE raster data at once.  This is
 * necessary because of RLE stores images bottom to top and GIF stores
 * them top to bottom.
 */
int get_scanlines()
{
    int i;
    rle_pixel *save_scan_0;


    if (sizeof(rle_pixel) != sizeof(gif_pixel))
        error("rle pixel size not 8 bits");
    if (rle_row_alloc(&in_hdr, &scan) < 0)
	error("can't allocate scan");
    save_scan_0 = scan[0];
    scanbuf = (gif_pixel**)
		malloc(sizeof(gif_pixel*)*(in_hdr.ymax-in_hdr.ymin+1));
    if (scanbuf == NULL)
	error("can't allocate scanbuf");
    for (i = 0; i <= in_hdr.ymax - in_hdr.ymin; i++) {
	scan[0] = (rle_pixel*)malloc(sizeof(rle_pixel)*(in_hdr.xmax+1));
        if (scan[0] == NULL)
	    error("can't allocate current scanline");
        rle_getrow(&in_hdr, scan);
        scanbuf[in_hdr.ymax - in_hdr.ymin - i] = (gif_pixel*)scan[0];
    }
    scan[0] = save_scan_0;
    return (1);
}

/********************************************************************
 * get_color_bits() searches the array of comment strings for the substring
 * "color_map_length=" which is added by mcut if it has worked on the 
 * file.  If it is found, it sets color_bits to the ceiling of log 2
 * color_map_length.  Otherwise it returns the default value 8.
 ********************************************************************/
int get_color_bits(comments)
char **comments;
{
    int i, color_bits, num_colors;

    color_bits = 8;
    if (comments == NULL) return (color_bits);
    for (i = 0; comments[i]; i++) {
        if (!strncmp(comments[i],"color_map_length=",17)) {
            num_colors = atoi(&(comments[i][17]));
            for (color_bits = 1; num_colors >> color_bits; color_bits++);
            if (num_colors == (1<<(color_bits-1))) color_bits--;
	 break;
        }
    }
    if ((color_bits < 0) || (color_bits > 8))
        error("invalid number of color bits");
    return (color_bits);
}

int getpixel( x, y )
int x, y;
{
    return pix_mask & (int)scanbuf[y][x];
}

void error(s)
char *s;
{
    fprintf(stderr,"%s\n", s);
    exit(2);
}
