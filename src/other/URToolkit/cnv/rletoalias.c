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
 * rletoalias.c - Convert Utah's rle images to Alias "pix" format
 *
 * Author:      Raul Rivero
 *              Mathematics Dept.
 *              University of Oviedo
 * Date:        Mon Dec 30 1991
 * Copyright (c) 1991, Raul Rivero
 *
 */
#ifndef lint
static char rcs_id[] = "$Header$";
#endif
/*
aliastorle()		Tag the file.
*/

#include <stdio.h>
#include <math.h>
#include "rle.h"

#define byte                    unsigned char
#define Fread(p, s, n, f)       if ( fread(p, s, n, f) != n ) error(3)
#define Fwrite(p, s, n, f)      if ( fwrite(p, s, n, f) != n ) error(4)
#define VPRINTF                 if (verbose) fprintf
#define PSEUDOCOLOR             1
#define DIRECTCOLOR             2
#define TRUECOLOR               3
#define GRAYSCALE               4

/*
 * Alias header.
 */
typedef struct {
        short xsize, ysize;
        short xinit, yinit;
        short depth;
} alias_hdr;

/*
 * I use a intermediate format ( a simple bitmap ) with
 * this format ...
 */
typedef struct {
        int xsize, ysize;       /* sizes */
        int depth;              /* # of colors */
        int colors;             /* # of colors */
        byte *r, *g, *b;        /* components or bitmap (planes < 8) */
        byte *cmap;             /* cmap if planes < 8 */
} bitmap_hdr;

typedef byte color_map[3];

/*
 * the flag for verbose.
 */
int verbose= 0;
/*
 * RLE file header (here for command name and file name).
 */
rle_hdr the_hdr;

#ifdef USE_PROTOTYPES
static void read_rle( FILE *, bitmap_hdr *);
static void write_alias( FILE *, bitmap_hdr *);
static void error( int );
static void rlecmap_to_bitmapcmap( rle_hdr *, bitmap_hdr * );
static void code_alias24( byte *, byte *, byte *, int, FILE * );
static char *Malloc( long int );
#else
static void read_rle(), write_alias(), error(), rlecmap_to_bitmapcmap();
static void code_alias24();
static char *Malloc();
#endif

/*****************************************************************
 * TAG( main )
 *
 * Usage: rletoalias [-v] [-o outfile] [infile]
 *
 * Inputs:
 *	-v:		Verbose switch.
 *      infile:		Input a color RLE file. Stdin used if
 *                      not specified.
 * Outputs:
 *      outfile:	Output Alias pix file. Stdout used if
 *               	not specified.
 * Assumptions:
 *      [None]
 * Algorithm:
 *      [None]
 */

void
main( argc, argv )
int argc;
char **argv;
{
  char *inname  = NULL;
  char *outname = NULL;
  FILE *infile;
  FILE *outfile = stdout;
  int oflag = 0;
  bitmap_hdr bitmap;

  the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
  /*
   * Get options
   */
  if ( !scanargs( argc, argv, "% v%- o%-outfile!s infile%s",
                  &verbose, &oflag, &outname, &inname ))
    exit( 1 );

  /*
   * Open rle file.
   */
  rle_names( &the_hdr, cmd_name( argv ), inname, 0 );
  infile = rle_open_f(the_hdr.cmd, inname, "r");

  /*
   * Translate Utah's rle format to our bitmap.
   */
  read_rle(infile, &bitmap);
  fclose(infile);    /* we finish with this file */

  outfile = rle_open_f(cmd_name(argv), outname, "w");
  /* An output file name ? */
  rle_names( &the_hdr, cmd_name( argv ), outname, 0 );
  VPRINTF(stderr, "Writing to file %s\n", the_hdr.file_name);
  outfile= rle_open_f(the_hdr.cmd, outname, "w");

  write_alias(outfile, &bitmap);
  fclose(outfile);            /* it is not necesary, but ... */

  exit(0);
}

static void
read_rle(handle, image)
FILE *handle;
bitmap_hdr *image;
{
  register int i, j;
  rle_pixel **row;
  byte *r, *g = 0, *b = 0;
  int totalsize;
  color_map *map = 0;
  int type = 0;
  int two_lines;
  int offset_last;

  /*
   * Read the file's configuration.
   */
  the_hdr.rle_file = handle;
  rle_get_setup_ok( &the_hdr, NULL, NULL );

  /*
   * Fill our bitmap.
   */
  image->xsize = the_hdr.xmax - the_hdr.xmin + 1;
  image->ysize = the_hdr.ymax - the_hdr.ymin + 1;
  image->depth = ( the_hdr.ncolors < 3 ? the_hdr.cmaplen : 24 );
  image->colors = ( 1 << image->depth );
  totalsize= image->xsize * image->ysize;
  offset_last = totalsize - image->xsize;
  two_lines = 2 * image->xsize;
  VPRINTF(stderr, "Image size: %dx%d\n", image->xsize, image->ysize);
  VPRINTF(stderr, "Image depth: %d\n", image->depth);
  VPRINTF(stderr, "%s colormap\n", 
                  (the_hdr.ncmap ? "With" : "Without"));

  /*
   * Check the coherence and image type.
   */
  VPRINTF(stderr, "Image type ");
  switch ( the_hdr.ncolors ) {
    case  1: switch ( the_hdr.ncmap ) {
               case  0: type = GRAYSCALE;       /* 8 planes, no cmap */
                        VPRINTF(stderr, "GRAYSCALE\n");
                        break;
               case  3: type = PSEUDOCOLOR;     /* 8 planes, cmap */
                        VPRINTF(stderr, "PSEUDOCOLOR\n");
                        break;
               default: VPRINTF(stderr, "unkown\n");
                        error(6);
                        break;
             }
             break;
    case  3: switch ( the_hdr.ncmap ) {
               case  0: type = DIRECTCOLOR;     /* 24 planes, no cmap */
                        VPRINTF(stderr, "DIRECTCOLOR\n");
                        break;
               case  3: type = TRUECOLOR;       /* 24 planes, cmap */
                        VPRINTF(stderr, "TRUECOLOR\n");
                        break;
               default: VPRINTF(stderr, "unkown\n");
                        error(6);
                        break;
             }

             break;
    default: error(6);
             VPRINTF(stderr, "unkown\n");
             break;
  }

  /*
   * Allocate some memory.
   */
  if (rle_row_alloc(&the_hdr, &row) < 0)
    error(2);
  if (image->depth > 8 || type == GRAYSCALE) {
    /*
     * 24 planes => we need three components
     * GRAYSCALE use 8 planes but it's defined like 24 planes
     */
    r= image->r = (byte *) Malloc(totalsize);
    g= image->g = (byte *) Malloc(totalsize);
    b= image->b = (byte *) Malloc(totalsize);
    if ( type == TRUECOLOR) {
      image->colors = 256;      /* well, a trap to rlecmap_to_bit... */
      rlecmap_to_bitmapcmap(&the_hdr, image);
      map = (color_map *) image->cmap;	/* will be more easy use it */
    }
  }else {
    /* 8 planes => one component and cmap */
    r= image->r = (byte *) Malloc(totalsize);
    /* Convert rle cmap to bitmap cmap */
    rlecmap_to_bitmapcmap(&the_hdr, image);
    map = (color_map *) image->cmap;	/* will be more easy use it */
  }

  /*
   * Read the input image and convert it to a simple bitmap.
   * RLE writes lines in reverse order, so we point to last
   * line.
   */
  VPRINTF(stderr, "Uncompressing RLE file\n");
  r += offset_last;
  g += offset_last;
  b += offset_last;
  for (j= 0; j< image->ysize; j++) {
    rle_getrow(&the_hdr, row);
    switch ( type ) {
      case GRAYSCALE  :
      case DIRECTCOLOR: for (i= 0; i< image->xsize; i++) {
                          *r++ = row[0][i];
                          *g++ = row[1][i];
                          *b++ = row[2][i];
                        }
                        break;
      case PSEUDOCOLOR: for (i= 0; i<image->xsize; i++) {
                          *r++ = row[0][i];
                        }
                        break;
      case TRUECOLOR  : for (i= 0; i< image->xsize; i++) {
                          *r++ = map[row[0][i]][0];
                          *g++ = map[row[1][i]][1];
                          *b++ = map[row[2][i]][i];
                        }
                        break;
      default         : error(6);
                        break;
    }
    /*
     * Pointers to next byte of current line, so we substract
     * two lines.
     */
    r -= two_lines;
    g -= two_lines;
    b -= two_lines;
  }

  /*
   * TRUECOLOR has map of colors, but we'll not use it.
   */
  if ( type == TRUECOLOR )
    free(image->cmap);
}

static void
rlecmap_to_bitmapcmap(rle, bitmap)
rle_hdr *rle;
bitmap_hdr *bitmap;
{
  register int i;
  rle_map *rch, *gch, *bch;
  byte *ptr;

  /* Allocate memory */
  ptr= bitmap->cmap= (byte *) Malloc(bitmap->colors * 3);

  /*
   * We'll use 3 ptrs, first to R channel, second to G channel, ...
   */
  rch = rle->cmap;
  gch = &(rle->cmap[bitmap->colors]);
  bch = &(rle->cmap[2 * bitmap->colors]);
  for (i= 0; i< bitmap->colors; i++) {
    *ptr++ = (byte) (*rch++ >> 8);
    *ptr++ = (byte) (*gch++ >> 8);
    *ptr++ = (byte) (*bch++ >> 8);
  }
}

static void
write_alias(handle, image)
FILE *handle;
bitmap_hdr *image;
{
  register int i;
  alias_hdr alias;
  byte *rbuf, *gbuf, *bbuf;

  /*
   * Alias 8 bits files are b&w ( no cmap ), so Alias
   * don't support really a 8 bits file.
   */
  if (image->depth <= 8) {
    fprintf(stderr, "Bitmap with 8 planes\n");
    error(99);
  }

  VPRINTF(stderr, "Writing Alias file\n");
  /* Fill the Alias header and write it */
  alias.yinit = 0;
  alias.xinit = 0;
  alias.xsize = image->xsize;
  alias.ysize = image->ysize;
  alias.depth = image->depth;
  Fwrite(&alias, sizeof(alias_hdr), 1, handle);

  /* Set the pointers to buffers */
  rbuf = image->r;
  gbuf = image->g;
  bbuf = image->b;

  /* Compress each line */
  for (i= 0; i< image->ysize; i++) {
    /*
     * Alias "pix" format do a compression method on
     * each raster line.
     */
    code_alias24(rbuf, gbuf, bbuf, image->xsize, handle);
    /* Move pointers to next lines */
    rbuf += image->xsize;
    gbuf += image->xsize;
    bbuf += image->xsize;
  }
}

static void
code_alias24(rbuf, gbuf, bbuf, xmax, handle)
byte *rbuf, *gbuf, *bbuf;
int xmax;
FILE *handle;
{
  byte r, g, b;
  unsigned int number= 0;
  int repeat= 1;
  static byte buf[5120];
  byte *bufptr, *end;

  bufptr= buf;
  end= rbuf + xmax;
  r= *rbuf++; g= *gbuf++; b= *bbuf++;

  while (rbuf < end)
    /*
     * While we have the same pattern ( and < 255 ), we continue
     * skipping bytes ( pixels ).
     */
    if (r== *rbuf && g== *gbuf && b== *bbuf && repeat < 255) {
      repeat++;
      rbuf++; gbuf++; bbuf++;
    }else {
      /*
       * A different triple or repeat == 255 was found, then we add
       * this to the output buffer.
       */
      *bufptr++ = repeat;
      *bufptr++ = b;
      *bufptr++ = g;
      *bufptr++ = r;
      number += 4;
      repeat  = 1;
      r= *rbuf++; g= *gbuf++; b= *bbuf++;
    }

  /*
   * We need add the last triplet.
   */
  *bufptr++ = repeat;
  *bufptr++ = b;
  *bufptr++ = g;
  *bufptr++ = r;
  number += 4;
  /*
   * And ... write it !
   */
  Fwrite(buf, number, 1, handle);
}

static void
error(code)
int code;
{
  fprintf(stderr, "%s: ", the_hdr.cmd);
  switch (code) {
    case  0: fprintf(stderr, "Usage:  %s [-v] [-o outfile] [infile] \n", 
                     the_hdr.cmd);
             break;
    case  1: fprintf(stderr, "Cannot open file.\n");
             break;
    case  2: fprintf(stderr, "Out of memory.\n");
             break;
    case  3: fprintf(stderr, "Error while reading input file\n");
             break;
    case  4: fprintf(stderr, "Error while writing output file\n");
             break;
    case  5: fprintf(stderr, "Input file is not an Alias pix\n");
             break;
    case  6: fprintf(stderr, "Incorrect # of planes or # of colors\n");
             break;
    case 99: fprintf(stderr, "Not ready\n");
             break;
    default: fprintf(stderr, "Unknow erro code (%d)\n", code);
             break;
  }
  exit(1);
}

/*
 * My standard functions.
 */

static char *Malloc(size)
long int size;
{
  char *ptr;

  if ((ptr = (char *) malloc(size)) == NULL)
    error(2);

  /*
   * Usually compilers fill buffers with zeros,
   * but ...
   */
  bzero( ptr, size );
  return ptr;
}

