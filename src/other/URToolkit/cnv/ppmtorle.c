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
 * ppmtorle - A program which will convert pbmplus/ppm images
 *            to Utah's "rle" image format.
 *
 * Author:      Wesley C. Barris
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        Fri July 20 1990
 * Copyright (c) Wesley C. Barris
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif
#if 0
ppmtorle()			/* Tag. */
#endif
/*-----------------------------------------------------
 * System includes.
 */
#define NO_DECLARE_MALLOC	/* ppm.h does it */
#include "rle.h"
#include <stdio.h>
#include <ppm.h>

#define VPRINTF if (verbose || header) fprintf

typedef unsigned char U_CHAR;
/*
 * Global variables.
 */
FILE	*fp;
rle_hdr hdr;
int	format;
int	width, height;
int	verbose = 0, header = 0, do_alpha = 0;
gray	maxval;
/*-----------------------------------------------------------------------------
 *                                        Read the ppm image file header.
 */
void read_ppm_header()
{
   ppm_readppminit(fp, &width, &height, &maxval, &format);
   VPRINTF(stderr, "Image type: 24 bit true color\n");
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
   hdr.ncolors = 3;
   hdr.background = 0;
   if (do_alpha) {
      hdr.alpha = 1;
      RLE_SET_BIT(hdr, RLE_ALPHA);
      }
   RLE_SET_BIT(hdr, RLE_RED);
   RLE_SET_BIT(hdr, RLE_GREEN);
   RLE_SET_BIT(hdr, RLE_BLUE);
   rle_put_setup(&hdr);
}
/*-----------------------------------------------------------------------------
 *                                      Write the rle data portion of the file.
 */
void write_rle_data()
{
   register int		x;
   register int		scan;
   register pixel	*pixelrow, *pP;
   rle_pixel		***scanlines, **scanline;
   /*
    * Allocate some memory.
    */
   pixelrow = ppm_allocrow(width);
   scanlines = (rle_pixel ***)malloc( height * sizeof(rle_pixel **) );
   RLE_CHECK_ALLOC( hdr.cmd, scanlines, "scanline pointers" );

   for ( scan = 0; scan < height; scan++ )
      RLE_CHECK_ALLOC( hdr.cmd, (rle_row_alloc(&hdr, &scanlines[scan]) >= 0),
		       "pixel memory" );

   /*
    * Loop through the ppm files image window, read data and flip vertically.
    */
   for (scan = 0; scan < height; scan++) {
      scanline = scanlines[height - scan - 1];
      ppm_readppmrow(fp, pixelrow, width, maxval, format);
      for (x = 0, pP = pixelrow; x < width; x++, pP++) {
	 scanline[RLE_RED][x]   = PPM_GETR(*pP);
	 scanline[RLE_GREEN][x] = PPM_GETG(*pP);
	 scanline[RLE_BLUE][x]  = PPM_GETB(*pP);
	 if (do_alpha) {
	    scanline[RLE_ALPHA][x] = (scanline[RLE_RED][x] ||
				      scanline[RLE_GREEN][x] ||
				      scanline[RLE_BLUE][x] ? 255 : 0);
	 }
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
}
/*-----------------------------------------------------------------------------
 *                      Convert an PPM image file into an rle image file.
 */
int
main(argc, argv)
int argc;
char **argv;
{
   char		*periodP, *ppmname = NULL, *outname = NULL;
   static char	filename[BUFSIZ];
   int		oflag, c;
/*
 * Get those options.
 */
   if (!scanargs(argc,argv,
       "% v%- h%- a%- o%-outfile!s infile.ppm%s\n(\
\tConvert PPM file to URT RLE format.\n\
\t-a\tFake an alpha channel.  Alpha=0 when input=0, 255 otherwise.\n\
\t-h\tPrint header of PGM file.\n\
\t-v\tVerbose mode.\n\
\tInput file name (if given) is forced to end in .ppm.)",
       &verbose,
       &header,
       &do_alpha,
       &oflag, &outname,
       &ppmname))
      exit(-1);

   hdr = *rle_hdr_init( (rle_hdr *)NULL );
   rle_names( &hdr, cmd_name( argv ), outname, 0 );
/*
 * Open the file.
 */
   if (ppmname == NULL) {
      strcpy(filename, "stdin");
      fp = stdin;
      }
   else {
      periodP = strrchr(ppmname, '.');
      strcpy(filename, ppmname);
      if (periodP) {
         if (strcmp(periodP, ".ppm")) /* does not end in ppm */
            strcat(filename, ".ppm");
         }
      else				/* no ext -- add one */
         strcat(filename, ".ppm");
      if (!(fp = fopen(filename, "r"))) {
         fprintf(stderr, "%s: Cannot open %s for reading.\n",
		 hdr.cmd, filename);
         exit(-1);
         }
      }

   hdr.rle_file = rle_open_f( hdr.cmd, outname, "w" );
   while ( (c = getc( fp )) != EOF )
   {
      ungetc( c, fp );
      /*
       * Read the PPM file header.
       */
      read_ppm_header();
      if (header)
	 break;

      /*
       * Write the rle file header.
       */
      rle_addhist(argv, (rle_hdr *)NULL, &hdr);
      write_rle_header();
      /*
       * Write the rle file data.
       */
      write_rle_data();
   }

   fclose(fp);
   return 0;
}
