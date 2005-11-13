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
 *
 * Copyright (c) 1986, John W. Peterson
 */

/*
 * rletotarga.c - Convert RLE files into Truevision TARGA format.
 * Modified from targatorle.c, with help from rletogray.c
 *
 * By:		Andrew Hadenfeldt
 *		Department of Electrical Engineering
 *		University of Nebraska-Lincoln
 *		16-Mar-1991
 *
 * Usage:
 *
 *  rletotarga [infile] outfile
 *
 *  If no input filename is given, input is take from stdin.
 *
 * Since RLE and TARGA formats can agree on which end of the image is up, we
 * do not need to flip the image.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "rle.h"

#define PUTBYTE(N)  putc((N)&0xff,outfile)
#define PUTSHORT(N) (putc((N)&0xff,outfile),putc(((N)>>8)&0xff,outfile))

/*
 * Description of header for files containing TARGA images
 */
struct targafile {
      unsigned char   num_char_id,    /* Number of Characters in ID Field */
                      cmap_type,      /* Color Map Type */
                      image_type;     /* Image Type Code */
      unsigned short  cmap_origin,    /* Color Map Origin */
                      cmap_length;    /* Color Map Length */
      unsigned char   cmap_size;      /* Color Map Entry Size */
      unsigned short  image_x_origin, /* X-Origin of Image */
                      image_y_origin, /* Y-Origin of Image */
                      image_width,    /* Width of Image */
                      image_height;   /* Height of Image */
      unsigned char   image_pix_size, /* Image Pixel Size */
                      image_descriptor;       /* Image Descriptor Byte */
      };

void
main(argc,argv) 
int argc;
char *argv[];
{ 
    FILE       *outfile;
    char       *infname=NULL, 
    	       *outfname=NULL;
    int        aflag=0;
    unsigned   i, j;
    rle_hdr    hdr;
    rle_pixel  **inprows;
    struct targafile tga_head;

    if ( scanargs( argc, argv, "% o!-outfile!s infile%s\n(\
\tConvert RLE file to TARGA format.\n\
\tNote that output file name is required.)", &i, &outfname, &infname ) == 0 )
	exit( 1 );

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname, 0 );

    /* Open specified input file (or stdin if none) */
    hdr.rle_file  = rle_open_f(hdr.cmd, infname, "r");
    outfile = rle_open_f(hdr.cmd, outfname, "w");

    /* Read the image header */

    rle_get_setup_ok(&hdr, NULL, NULL);

    /* Fill in the TARGA image header */

    tga_head.num_char_id = 0;
    tga_head.cmap_type   = 0;

    tga_head.cmap_origin = 0;
    tga_head.cmap_length = 0;
    tga_head.cmap_size   = 0;

    tga_head.image_x_origin = 0;
    tga_head.image_y_origin = 0;
    tga_head.image_width    = hdr.xmax - hdr.xmin + 1;
    tga_head.image_height   = hdr.ymax - hdr.ymin + 1;

    tga_head.image_descriptor = 0;

    switch (hdr.ncolors)
    {
      case 1:				/* Black and White image type */
        tga_head.image_type = 3;
        tga_head.image_pix_size = 8;
        break;

      case 3:				/* Real-Color image */
        tga_head.image_type = 2;
        tga_head.image_pix_size = 24;
        if (hdr.alpha)
        {
          aflag = 1;
          tga_head.image_pix_size = 32;
          tga_head.image_descriptor = 0x08;	/* Indicate 8 alpha bits */
	}
        break;

      default:
        fprintf(stderr,"%s: Invalid number of color channels (%d).\n",
                hdr.cmd, hdr.ncolors);
        exit(1);
    }

    /* Allocate pixel buffer */

    if (rle_row_alloc(&hdr,&inprows))
    {
      fprintf(stderr,"%s: Not enough memory.\n", hdr.cmd);
      exit(1);
    }

    /* Write the TARGA image header */

    PUTBYTE(tga_head.num_char_id);
    PUTBYTE(tga_head.cmap_type);
    PUTBYTE(tga_head.image_type);

    PUTSHORT(tga_head.cmap_origin);
    PUTSHORT(tga_head.cmap_length);
    PUTBYTE(tga_head.cmap_size);

    PUTSHORT(tga_head.image_x_origin);
    PUTSHORT(tga_head.image_y_origin);
    PUTSHORT(tga_head.image_width);
    PUTSHORT(tga_head.image_height);
    PUTBYTE(tga_head.image_pix_size);

    PUTBYTE(tga_head.image_descriptor);

    /* Process the RLE image data */

    for (i=0;i<tga_head.image_height;i++)
    {
      rle_getrow(&hdr,inprows);

      if (tga_head.image_pix_size==8)
        fwrite(inprows[0],1,tga_head.image_width,outfile);
      else
        for (j=0;j<tga_head.image_width;j++)
        {
           PUTBYTE(inprows[2][j]);			/* Blue plane  */
           PUTBYTE(inprows[1][j]);			/* Green plane */
           PUTBYTE(inprows[0][j]);			/* Red plane   */
           if (aflag) PUTBYTE(inprows[-1][j]);		/* Alpha plane */
	}
    }

    fclose(hdr.rle_file);
    fclose(outfile);
}
