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
 * rletocgm.c - Tool to convert RLE files to ANSI/ISO CGM metafiles.
 * 
 * Author:      Joel Welling
 * 		Pittsburgh Supercomputing Center
 * 		Carnegie Mellon University
 *              welling@psc.edu
 *              (derived from rleskel.c)
 * Date:        Thursday, April 11, 1991
 * Copyright (c) 1990, University of Michigan
 * Copyright (c) 1991, Carnegie Mellon University (derived from rleskel.c)
 */
#ifndef lint
static char rcs_id[] = "$Header$";
#endif
#if 0
rletocgm()			/* TAG */
#endif

#include <stdio.h>
#include <rle.h>

/*  This module recognizes what type of machine it's on by the presence
of the symbol VMS, unix, CRAY, or ardent.  The following makes the machine
assignment less ambiguous.
*/
#if ( unix && ( !CRAY && !ardent ) )
#define USE_UNIX
#endif

/* Include defs files that allow linkage to Fortran on various systems */
#ifdef USE_UNIX
#include "unix_defs.h"
#endif
#ifdef CRAY
#include "unicos_defs.h"
#endif
#ifdef ardent
#include "unicos_defs.h"  /* these are also appropriate on Ardent Titan */
#endif
#ifdef _IBMR2
/* include nothing */
#endif

/* Flag to produce verbose output */
static int verboseflag= 0;

static void open_output(outfname)
char *outfname;
/* This routine opens the output file. */
{
  int ierr= 0, i256= 256;
  
  wrcopn(outfname,&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error opening file <%s> for output!\n",outfname);
    exit(2);
  }
  wrmxci(&i256,&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing to file!\n");
    exit(2);
  }
}

static void close_output()
/* This routine closes the output file. */
{
  int ierr= 0;

  wrtend(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error closing output!\n");
    exit(2);
  }  
}

static void begin_direct_page(in_hdr)
rle_hdr *in_hdr;
/* This routine begins a direct color page */
{
  int ierr= 0, one= 1;
  float red, green, blue;

  if (in_hdr->background) {
    red= ((float)in_hdr->bg_color[0])/255.0;
    green= ((float)in_hdr->bg_color[1])/255.0;
    blue= ((float)in_hdr->bg_color[2])/255.0;
  }
  else {
    red= 0.0;
    green= 0.0;
    blue= 0.0;
  }

  wrbegp(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing begin page!\n");
    exit(2);
  }
  wrtcsm(&one,&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing color selection mode!\n");
    exit(2);
  }  
  wrbgdc(&red, &green, &blue, &ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing background color!\n");
    exit(2);
  }  
  wrbgpb(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing begin picture body!\n");
    exit(2);
  }  
}

static void handle_color_table(in_hdr,cmap)
rle_hdr *in_hdr;
rle_pixel **cmap;
/* This routine extracts color table information for the image, or
 * makes it up if need be.
 */
{
  int tablesize, i, zero= 0, ierr= 0;
  float *rarray, *garray, *barray;
  char *comstring;

  /* Calculate table size, then reset it if there is a comment to that
   * effect in the RLE file.
   */
  if (in_hdr->cmaplen) 
    tablesize= 2<<(in_hdr->cmaplen-1);
  else tablesize= 2;
  comstring= rle_getcom( "color_map_length", in_hdr );
  if (!comstring)   comstring= rle_getcom( "colormap_length", in_hdr );
  if (comstring) sscanf( comstring, "%d", &tablesize );

  if ( !(rarray= (float *)malloc( tablesize*sizeof(float) )) ) {
    fprintf(stderr,
	    "rletocgm: unable to allocate %d floats for color table reds!\n",
	    tablesize);
    exit(2);
  }
  if ( !(garray= (float *)malloc( tablesize*sizeof(float) )) ) {
    fprintf(stderr,
	    "rletocgm: unable to allocate %d floats for color table greens!\n",
	    tablesize);
    exit(2);
  }
  if ( !(barray= (float *)malloc( tablesize*sizeof(float) )) ) {
    fprintf(stderr,
	    "rletocgm: unable to allocate %d floats for color table blues!\n",
	    tablesize);
    exit(2);
  }

  /* Transcribe the color map into the float arrays */
  for (i=0; i<tablesize; i++) {
    rarray[i]= (float)cmap[0][i]/255.0;
    garray[i]= (float)cmap[1][i]/255.0;
    barray[i]= (float)cmap[2][i]/255.0;
  }

  /* Actually write the map */
  wrctbl(rarray, garray, barray, &zero, &tablesize, &ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing color map!\n");
    exit(2);
  }      

  /* Clean up */
  free( (char *)rarray );
  free( (char *)garray );
  free( (char *)barray );
}

static void begin_indexed_page(in_hdr)
rle_hdr *in_hdr;
/* This routine begins an indexed color page */
{
  int ierr= 0;
  rle_pixel **cmap;
  float red, green, blue;

  /* Load the color table, and write background color information */
  cmap= buildmap( in_hdr, 3, 0.0, 1.0 );
  if (in_hdr->background) {
    red= (float)cmap[0][in_hdr->bg_color[0]]/255.0;
    green= (float)cmap[1][in_hdr->bg_color[0]]/255.0;
    blue= (float)cmap[2][in_hdr->bg_color[0]]/255.0;
  }
  else {
    red= 0.0;
    green= 0.0;
    blue= 0.0;
  }

  /* Preliminary CGM stuff */
  wrbegp(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing begin page!\n");
    exit(2);
  }  
  wrbgdc(&red, &green, &blue, &ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing background color!\n");
    exit(2);
  }  
  wrbgpb(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing begin picture body!\n");
    exit(2);
  }  

  /* Produce a color table */
  handle_color_table(in_hdr, cmap);

  /* Clean up */
  free( (char *)cmap[0] );
}

static void end_page()
/* This routine ends a page. */
{
  int ierr= 0;

  wrendp(&ierr);
  if (ierr) {
    fprintf(stderr,"rletocgm: Error ending output page!\n");
    exit(2);
  }    
}

static void center_image(in_hdr, px, py, qx, qy, rx, ry)
rle_hdr *in_hdr;
float *px, *py, *qx, *qy, *rx, *ry;
/* This routine centers the image in DrawCGM's unit square coordinates. */
{
  float xrange, yrange;

  xrange= (float)(in_hdr->xmax - in_hdr->xmin + 1);
  yrange= (float)(in_hdr->ymax - in_hdr->ymin + 1);
  if (xrange>=yrange) {
    *px= 0.0;
    *qx= 1.0;
    *rx= *qx;
    *py= 0.5 * (1.0 - yrange/xrange);
    *qy= 1.0 - *py;
    *ry= *py;
  }
  else {
    *py= 0.0;
    *qy= 1.0;
    *ry= *py;
    *px= 0.5 * (1.0 - xrange/yrange);
    *qx= 1.0 - *px;
    *rx= *qy;
  }
}

static void process_indexed_page(in_hdr)
rle_hdr *in_hdr;
/* This routine handles one page of a file, assuming indexed color. */
{
  int x, y;
  rle_pixel **rows;             /* Will be used for scanline storage. */
  static int *iarray= (int *)0; /* Stores the CGM integer image */
  int *iptr;
  static int nx, ny;            /* dimensions of iarray */
  float px, py, qx, qy, rx, ry; /* CGM array boundaries */
  int ierr= 0;

  /* Be verbose if requested */
  if (verboseflag) fprintf(stderr,"Found indexed color (1 channel) page\n");

  /* Begin an output page, including the color table */
  begin_indexed_page(in_hdr);
  
  /* Calculate array boundaries */
  center_image(in_hdr, &px, &py, &qx, &qy, &rx, &ry);

  /* Allocate the CGM image array, if it's not already there */
  if (!iarray || 
      !(nx == in_hdr->xmax - in_hdr->xmin + 1) ||
      !(ny == in_hdr->ymax - in_hdr->ymin + 1)) {
    if (iarray) free( (char *)iarray );
    nx= in_hdr->xmax - in_hdr->xmin + 1;
    ny= in_hdr->ymax - in_hdr->ymin + 1;
    if ( !(iarray= (int *)malloc( nx*ny*sizeof(int) )) ) {
      fprintf(stderr,"rletocgm: cannot allocate %d ints!\n",nx*ny);
      exit(2);
    }
  }

  /* Allocate memory into which the image scanlines can be read. */
  if ( rle_row_alloc( in_hdr, &rows ) < 0 ) {
    fprintf( stderr, "rletocgm: Unable to allocate image memory.\n" );
    exit( RLE_NO_SPACE );
  }
  
  /* Read the input image and copy it to the output file. */
  iptr= iarray;
  for ( y = in_hdr->ymin; y <= in_hdr->ymax; y++ )
    {
      /* Read a scanline. */
      rle_getrow( in_hdr, rows );
      
      for (x=0; x<nx; x++) *iptr++= rows[0][x];
    }
  
  /* Write the CGM cell array */
  wrtcla( iarray, &nx, &ny, &px, &py, &qx, &qy, &rx, &ry, &ierr );
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing indexed cell array!\n");
    exit(2);
  }      

  /* Protect from bad input. */
  while ( rle_getskip( in_hdr ) != 32768 );
  
  /* Free memory. */
  rle_row_free( in_hdr, rows );
  
  /* End the page */
  end_page();
}

static void process_direct_page(in_hdr)
rle_hdr *in_hdr;
/* This routine handles one page of a file, assuming direct color. */
{
  int x, y;
  rle_pixel **rows;             /* Will be used for scanline storage. */
  static float *rarray= (float *)0; /* red CGM image */
  static float *garray= (float *)0; /* green CGM image */
  static float *barray= (float *)0; /* blue CGM image */
  float *rptr, *gptr, *bptr;
  static int nx, ny;            /* dimensions of image */
  float px, py, qx, qy, rx, ry; /* CGM array boundaries */
  int ierr= 0;
  
  /* Be verbose if requested */
  if (verboseflag) fprintf(stderr,"Found direct color (3 channel) page\n");
  
  /* Begin an output page */
  begin_direct_page(in_hdr);
  
  /* Calculate array boundaries */
  center_image(in_hdr, &px, &py, &qx, &qy, &rx, &ry);
  
  /* Allocate the CGM image array, if it's not already there */
  if (!rarray || 
      !(nx == in_hdr->xmax - in_hdr->xmin + 1) ||
      !(ny == in_hdr->ymax - in_hdr->ymin + 1)) {
    if (rarray) free( (char *)rarray );
    if (garray) free( (char *)garray );
    if (barray) free( (char *)barray );
    nx= in_hdr->xmax - in_hdr->xmin + 1;
    ny= in_hdr->ymax - in_hdr->ymin + 1;
    if ( !(rarray= (float *)malloc( nx*ny*sizeof(float) )) ) {
      fprintf(stderr,"rletocgm: cannot allocate %d floats for red!\n",nx*ny);
      exit(2);
    }
    if ( !(garray= (float *)malloc( nx*ny*sizeof(float) )) ) {
      fprintf(stderr,"rletocgm: cannot allocate %d floats for green!\n",nx*ny);
      exit(2);
    }
    if ( !(barray= (float *)malloc( nx*ny*sizeof(float) )) ) {
      fprintf(stderr,"rletocgm: cannot allocate %d floats for blue!\n",nx*ny);
      exit(2);
    }
  }

  /* Allocate memory into which the image scanlines can be read. */
  if ( rle_row_alloc( in_hdr, &rows ) < 0 ) {
    fprintf( stderr, "rletocgm: Unable to allocate image memory.\n" );
    exit( RLE_NO_SPACE );
  }
  
  /* Read the input image and copy it to the output file. */
  rptr= rarray;
  gptr= garray;
  bptr= barray;
  for ( y = in_hdr->ymin; y <= in_hdr->ymax; y++ )
    {
      /* Read a scanline. */
      rle_getrow( in_hdr, rows );
      
      for (x=0; x<nx; x++) {
	*rptr++= (float)rows[0][x]/255.0;
	*gptr++= (float)rows[1][x]/255.0;
	*bptr++= (float)rows[2][x]/255.0;
      }
    }
  
  /* Write the CGM cell array */
  wcladc( rarray, garray, barray, 
	 &nx, &ny, &px, &py, &qx, &qy, &rx, &ry, &ierr );
  if (ierr) {
    fprintf(stderr,"rletocgm: Error writing direct color cell array!\n");
    exit(2);
  }      

  /* Protect from bad input. */
  while ( rle_getskip( in_hdr ) != 32768 );
  
  /* Free memory. */
  rle_row_free( in_hdr, rows );
  
  /* End the page */
  end_page();
}

/*****************************************************************
 * TAG( main )
 * 
 * A skeleton RLE tool.  Demonstrates argument parsing, opening,
 * reading, and writing RLE files.  Includes support for files
 * consisting of concatenated images.
 * Usage:
 * 	rletocgm [-o outfile] [infile]
 * Inputs:
 * 	infile:		The input RLE file.  Default stdin.
 * 			"-" means stdin.
 * Outputs:
 * 	-o outfile:	The output RLE file.  Default stdout.
 * 			"-" means stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Repeatedly read from the input until the file EOF or an
 * 	error is encountered.
 */
main( argc, argv )
int argc;
char **argv;
{
  char       *infname = NULL,
  *outfname = NULL;
  int rle_cnt, rle_err;
  int 	oflag = 0, debugflag= 0;
  FILE       *outfile;
  rle_hdr *in_hdr;	/* Headers for input file. */
  
  in_hdr = rle_init_hdr( (rle_hdr *)NULL );
  if ( scanargs( argc, argv, "% o%-outfile!s d%- v%- infile%s",
		&oflag, &outfname, &debugflag, &verboseflag, &infname ) == 0 )
    exit( 1 );
  
  /* Turn on debugging if requested. */
  if (debugflag) {
    int ierr= 0;
    rle_debug(1);
    tgldbg(&ierr);
    if (ierr) fprintf(stderr,"rletocgm: couldn't set debugging!\n");
  }

  /* Open the input file.
   * The output file won't be opened until the first image header
   * has been read.  This avoids unnecessarily wiping out a
   * pre-existing file if the input is garbage.
   */
  rle_names( in_hdr, cmd_name( argv ), infname, 0 );
  in_hdr->rle_file = rle_open_f( in_hdr->cmd, infname, "r" );
  
  /* Read images from the input file until the end of file is
   * encountered or an error occurs.
   */
  rle_cnt = 0;
  while ( (rle_err = rle_get_setup( in_hdr )) == RLE_SUCCESS )
    {
      
      /* Open the CGM output file */
      if (rle_cnt==0) open_output(oflag ? outfname : "-");
      
      /* Count the input images. */
      rle_cnt++;
      
      /* Be verbose if requested */
      if (verboseflag) fprintf(stderr,"image %d: ",rle_cnt);

      /* Based on the number of colors in the header, select either indexed
       * or direct color processing for the images in the file.
       */
      switch (in_hdr->ncolors) {
      case 1: process_indexed_page(in_hdr); break;
      case 3: process_direct_page(in_hdr); break;
      default: fprintf(stderr,
		       "rletocgm: %d is an invalid number of color planes!\n",
		       in_hdr->ncolors);
      }
    }
  
  /* Close the CGM output file */
  close_output();
  
  /* Check for an error.  EOF or EMPTY is ok if at least one image
   * has been read.  Otherwise, print an error message.
   */
  if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
    rle_get_error( rle_err, in_hdr->cmd, infname );
  
  exit( 0 );
}
