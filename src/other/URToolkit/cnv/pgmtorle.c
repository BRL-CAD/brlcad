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
 * pgmtorle - A program which will convert pbmplus/pgm images
 *            to Utah's "rle" image format.
 *
 * Author:      Wesley C. Barris
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        Fri July 20 1990
 * Copyright (c) 1990 Wesley C. Barris
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif

/*-----------------------------------------------------
 * System includes.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pgm.h>

#undef abs			/* Screws up stdlib.h. */
#define NO_DECLARE_MALLOC
#include "rle.h"

#define VPRINTF if (verbose || header) fprintf

typedef unsigned char U_CHAR;
/*
 * Global variables.
 */
FILE	*fp;
int	format;
int	width, height;
int	verbose = 0, header = 0, do_alpha = 0;
gray	maxval;
rle_hdr hdr;

/*-----------------------------------------------------------------------------
 *                                        Read the Wavefront image file header.
 */
void read_pgm_header()
{
   pgm_readpgminit(fp, &width, &height, &maxval, &format);
   VPRINTF(stderr, "Image type: 8 bit grayscale\n");
   VPRINTF(stderr, "Full image: %dx%d\n", width, height);
   VPRINTF(stderr, "Maxval:     %d\n", maxval);
   if (do_alpha)
      VPRINTF(stderr, "Computing alpha channel...\n");
}
/*-----------------------------------------------------------------------------
 *                                             Write the rle image file header.
 */
void write_rle_header()
{
   hdr.xmin    = 0;
   hdr.xmax    = width-1;
   hdr.ymin    = 0;
   hdr.ymax    = height-1;
   hdr.ncolors = 1;
   hdr.background = 0;
   RLE_SET_BIT(hdr, RLE_RED);
   if (do_alpha) {
      hdr.alpha = 1;
      RLE_SET_BIT(hdr, RLE_ALPHA);
      }
   rle_put_setup(&hdr);
}
/*-----------------------------------------------------------------------------
 *                                      Write the rle data portion of the file.
 */
void write_rle_data()
{
    register int		x;
    register int		scan;
    register gray	*grayrow, *gP;
    rle_pixel	***scanlines, **scanline;
    /*
     * Allocate some memory.
     */
    grayrow = pgm_allocrow(width);
    scanlines = (rle_pixel ***)malloc( height * sizeof(rle_pixel **) );
    RLE_CHECK_ALLOC( hdr.cmd, scanlines, "scanline pointers" );

    for ( scan = 0; scan < height; scan++ )
	RLE_CHECK_ALLOC( hdr.cmd, (rle_row_alloc(&hdr, &scanlines[scan]) >= 0),
			 "pixel memory" );

    /*
     * Loop through the pgm files image window, write blank lines outside
     * active window.
     */
    for (scan = 0; scan < height; scan++) {
	pgm_readpgmrow(fp, grayrow, width, maxval, format);
	scanline = scanlines[height - scan - 1];
        for (x = 0, gP = grayrow; x < width; x++, gP++) {
	    scanline[RLE_RED][x] = *gP;
	    if (do_alpha)
		scanline[RLE_ALPHA][x] = (*gP ? 255 : 0);
	}
    }
    /*
     * Write out data in URT order (bottom to top).
     */
    for ( scan = 0; scan < height; scan++ )
    {
	rle_putrow(scanlines[scan], width, &hdr);
	rle_row_free( &hdr, scanlines[scan] );
    }
    free( scanlines );
    VPRINTF(stderr, "Done -- write eof to RLE data.\n");
    rle_puteof(&hdr);
    /*
     * Free up some stuff.
     */
}
/*-----------------------------------------------------------------------------
 *                      Convert a pgm image file into an rle image file.
 */
int
main(argc, argv)
int argc;
char **argv;
{
   int 		oflag = 0;
   char		*periodP, *pgmname = 0, *outname = 0;
   static char	filename[BUFSIZ];
/*
 * Get those options.
 */
   if (!scanargs(argc,argv,
       "% v%- h%- a%- o%-outfile!s infile.pgm%s\n(\
\tConvert a PGM file to URT RLE format.\n\
\t-a\tFake an alpha channel.  Alpha=0 when input=0, 255 otherwise.\n\
\t-h\tPrint header of PGM file.\n\
\t-v\tVerbose mode.\n\
\tInput file name is forced to end in .pgm.)",
       &verbose,
       &header,
       &do_alpha,
       &oflag, &outname,
       &pgmname))
      exit(-1);
   hdr = *rle_hdr_init( (rle_hdr *)NULL );
   rle_names( &hdr, cmd_name( argv ), outname, 0 );

/*
 * Open the file.
 */
   if (pgmname == NULL) {
      strcpy(filename, "stdin");
      fp = stdin;
      }
   else {
      periodP = strrchr(pgmname, '.');
      strcpy(filename, pgmname);
      if (periodP) {
         if (strcmp(periodP, ".pgm")) /* does not end in pgm */
            strcat(filename, ".pgm");
         }
      else				/* no ext -- add one */
         strcat(filename, ".pgm");
      if (!(fp = fopen(filename, "r"))) {
         fprintf(stderr, "%s: Cannot open %s for reading.\n",
		 hdr.cmd, filename);
         exit(-1);
         }
      }
/*
 * Read the PGM file header.
 */
   read_pgm_header();
   if (header)
      exit(0);
/*
 * Write the rle file header.
 */
   hdr.rle_file = rle_open_f( cmd_name( argv ), outname, "w" );
   rle_addhist(argv, (rle_hdr *)NULL, &hdr);
   write_rle_header();
/*
 * Write the rle file data.
 */
   write_rle_data();
   fclose(fp);

   return 0;
}
