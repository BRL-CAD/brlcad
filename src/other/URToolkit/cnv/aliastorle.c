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
 * aliastorle.c - Convert Alias "pix" format to Utah's rle images
 *
 * Author:      Raul Rivero
 *              Mathematics Dept.
 *              University of Oviedo
 * Date:        Fri Dec 27 1991
 * Copyright (c) 1991, Raul Rivero
 *
 */
#ifndef lint
static char rcs_id[] = "$Header$";
#endif
/*
  aliastorle()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

#define byte                    unsigned char
#define Fread(p, s, n, f)       if ( fread(p, s, n, f) != n ) error(3)
#define Fwrite(p, s, n, f)      if ( fwrite(p, s, n, f) != n ) error(4)
#define VPRINTF                 if (verbose) fprintf

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

/*
 * Prototypes ( only what is necesary ).
 */
#ifdef USE_PROTOTYPES
static char *Malloc(long int);
static char *read_file(FILE *, int *);
static long filelen(FILE *);
static int read_alias(FILE *, bitmap_hdr *);
static void write_rle(FILE *, bitmap_hdr *);
static void read_alias_header(FILE *, alias_hdr *);
static void create_alias_cmap(bitmap_hdr *);
static void uncode_alias24(byte *, byte *, byte *, byte *, byte *);
static void uncode_alias(byte *, byte *, byte *);
static int read_line_alias24(FILE *, byte *, byte *, byte *, int);
static int read_line_alias(FILE *, byte *, int);
static void error( int );
#else
static char *Malloc();
static char *read_file();
static long filelen();
static int read_alias();
static void write_rle(), read_alias_header();
static void create_alias_cmap(), uncode_alias24(), uncode_alias();
static int read_line_alias24(), read_line_alias();
static void error();
#endif


/*
 * The flag for verbose.
 */
int verbose= 0;
/*
 * RLE file header (put here so can be initialized with cmd and file names).
 */
rle_hdr the_hdr;

/*****************************************************************
 * TAG( main )
 *
 * Usage: aliastorle [-v] [-o outfile] [infile]
 *
 * Inputs:
 *	-v:		Verbose switch.
 *      infile:         Input Alias pix file. Stdin used if not
 *                      specified.
 * Outputs:
 *      outfile:        Output a RLE file.  Stdout used if not
 *                      specified.
 * Assumptions:
 *      [None]
 * Algorithm:
 *      [None]
 */

int
main( argc, argv )
int argc;
char **argv;
{
  char *inname  = NULL;
  char *outname = NULL;
  FILE *infile;
  FILE *outfile;
  int oflag = 0;
  bitmap_hdr bitmap;

  the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
  /*
   * Get options
   */
  if ( !scanargs( argc, argv, "% v%- o%-outfile!s infile%s",
                  &verbose, &oflag, &outname, &inname ))
    exit( 1 );

  /* An input file name ? */
  rle_names( &the_hdr, cmd_name( argv ), inname, 0 );
  infile= rle_open_f(the_hdr.cmd, inname, "r");


  /*
   * Translate Alias "pix" format into my bitmap.
   */
  read_alias(infile, &bitmap);
  rle_close_f(infile);    /* We finish with this file */

  /*
   * We have a bitmap with the image, so we open the
   * new rle file and write it.
   */
  outfile = rle_open_f(cmd_name(argv), outname, "w");
  write_rle(outfile, &bitmap);
  fclose(outfile);            /* it is not necesary, but ... */

  exit(0);
}

static int
read_alias(handle, image)
FILE *handle;
bitmap_hdr *image;
{
  register int i;
  alias_hdr header;
  int total_size;
  register int xsize;
  byte *end;
  byte *r, *g = 0, *b = 0;
  int can_read_all;
  byte *buffer = 0, *ptr = 0;
  int filesize;
  int allplanes = 0;
  int error;
  int cmapsize;

  /*
   * Alias "pix" is a bad format to read line per line. If every
   * triplet is different to its next, then we read 2 bytes, 2 bytes ...
   * ... and read a file of 1 Mb. with this form ... hmmmmmm !!!
   * This is necesary if we are reading from stdin, but if we have
   * a file, we'll read the file into memory ( one access to disk ).
   */
  can_read_all = (handle != stdin);     /* a regular file ? */
  VPRINTF(stderr, "Reading Alias from %s\n", 
                  (can_read_all ? "file" : "stdin"));

  if (can_read_all) {
    /*
     * We are using a regular file ( not a pipe ), so we can
     * read it into memory.
     */
    ptr= buffer= (byte *) read_file(handle, &filesize);
    VPRINTF(stderr, "File size: %d\n", filesize);
    /* Copy the header */
    bcopy(ptr, &header, sizeof(alias_hdr));
    ptr += sizeof(alias_hdr);           /* skip the header */
  }else {
    /*
     * We are using stdin so ...
     */
    /* We need the header of Alias pix */
    VPRINTF(stderr, "Reading Alias header\n");
    read_alias_header(handle, &header);
  }

  /* Size in pixels */
  total_size = header.xsize * header.ysize;

  /* Fill our header */
  image->xsize  = header.xsize;
  image->ysize  = header.ysize;
  image->depth  = header.depth;
  image->colors = 1 << image->depth;
  allplanes = ( image->depth > 8 );     /* an image with 24 planes ? */
  VPRINTF(stderr, "Image size: %dx%d\n", image->xsize, image->ysize);
  VPRINTF(stderr, "Depth: %d\n", image->depth);

  /* Get some memory */
  if ( allplanes ) {
    /*
     * We have a image with 24 planes, so we need three buffers to
     * store it.
     */
    r= image->r = (byte *) Malloc(total_size);
    g= image->g = (byte *) Malloc(total_size);
    b= image->b = (byte *) Malloc(total_size);
  }else {
    /*
     * The image has less than 256 colors, so we use one plane,
     * for the bitmap, and the cmap.
     */
    r= image->r = (byte *) Malloc(total_size);
    cmapsize= image->colors * 3;        /* size in bytes */
    image->cmap = (byte *) Malloc(cmapsize);
    /* Creating the cmap from file */
    VPRINTF(stderr, "Creating cmap\n");
    create_alias_cmap(image);
  }

  /*
   * We are ready to uncompress Alias file.
   */
  VPRINTF(stderr, "Uncompressing Alias file\n");
  if (can_read_all) {
    /*
     * We have the file into memory so we uncode it directly.
     */
    end = r + total_size;       /* the end of main buffer */
    if ( allplanes )
      uncode_alias24(ptr, r, g, b, end);
    else uncode_alias(ptr, r, end);
    /*
     * We have read the file and uncompressed, so we can
     * free the memory ( and exit ).
     */
    free(buffer);
  }else {
    /*
     * We'll read each line from Alias file ( stdin ) until we
     * fill our buffers or we get an error.
     */
    xsize = image->xsize;
    for (i= 0; i< image->ysize; i++) {
      /*
       * Read and decode the Alias raster information, two cases:
       * with planes > 8 => a triplet (RGB), else a index.
       */
      if ( allplanes ) {
        /* An image with 24 planes */
        error= read_line_alias24(handle, r, b, g, xsize);
      }else {
        /* An image with indexes to cmap */
        error= read_line_alias(handle, r, xsize);
      }
      if (error) {
        /* An error, so we exit */
        fprintf(stderr, "Error while reading line %d\n", i);
        return 1;
      }else {
        /* A line has been read, so we increment theirs pointers */
        r += xsize;
        if ( allplanes ) {
          g += xsize;
          b += xsize;
        }
      }
    }
  } /* end of reading from stdin */

  /* Finish with no problems */
  return 0;
}

static void
read_alias_header(handle, header)
FILE *handle;
alias_hdr *header;
{
  /* Read the header */
  Fread(header, sizeof(alias_hdr), 1, handle);

  /* Check some things */
  if ( header->xsize < 1 || header -> xsize > 2560 ||
       header->ysize < 1 || header -> ysize > 2560 )
    error(5);
  if ( header->depth > 24 || header->depth < 1 )
    error(5);
}

static int
read_line_alias24(handle, r, g, b, size)
FILE *handle;
byte *r, *g, *b;
register int size;
{
  register int i;
  register int count = 0;
  byte *end;
  byte buffer[4];

  end = r + size;
  while (count < size) {
    /*
     * Alias code format:  <repeat>BGR => 4 bytes.
     */
    if ( !fread(buffer, 4, 1, handle) )         /* read next buffer */
      return 1;   /* hey !, an EOF here ?. Hmmmm, this is an error ! */

    count += buffer[0];                 /* # of repetitions */
    /* repeat 'buffer[0]' times these triplet */
    for (i= 0; i< buffer[0]; i++) {
      *r++ = buffer[3];
      *g++ = buffer[1];
      *b++ = buffer[2];
    }
    if ( r > end )
      error(6);         /* uncompress more bytes than size */
  }

  /* No problems */
  return 0;
}

static int
read_line_alias(handle, r, size)
FILE *handle;
byte *r;
register int size;
{
  register int i;
  register int count = 0;
  byte *end;
  byte buffer[2];

  end = r + size;
  while (count < size) {
    /*
     * Alias code format:  <repeat><index> => 2 bytes.
     */
    if ( !fread(buffer, 2, 1, handle) )         /* read next buffer */
      return 1;   /* hey !, an EOF here ?. Hmmmm, this is an error ! */

    count += buffer[0];                 /* # of repetitions */
    /* repeat 'buffer[0]' times these index */
    for (i= 0; i< buffer[0]; i++) {
      *r++ = buffer[1];
    }
    if ( r > end )
      error(6);         /* uncompress more bytes than size */
  }

  /* No problems */
  return 0;
}

static void
create_alias_cmap(image)
bitmap_hdr *image;
{
  register int i;
  byte *ptr;

  /*
   * Alias 8 bits files are b&w, so ...
   */
  ptr = (byte *) image->cmap;
  for (i= 0; i< image->colors; i++) {
    *ptr++ = i;
    *ptr++ = i;
    *ptr++ = i;
  }
}

static void
bitmapcmap_to_rlecmap(bitmap, rle)
bitmap_hdr *bitmap;
rle_hdr *rle;
{
  register int i;
  rle_map *rch, *gch, *bch;
  byte *ptr;

  /* Allocate memory */
  rle->cmap= (rle_map *) Malloc(bitmap->colors * 3 * sizeof(rle_map));

  /*
   * We'll use 3 ptrs, first to R channel, second to G channel, ...
   */
  ptr = bitmap->cmap;
  rch = rle->cmap;
  gch = &(rle->cmap[bitmap->colors]);
  bch = &(rle->cmap[2 * bitmap->colors]);
  for (i= 0; i< bitmap->colors; i++) {
    *rch++ = (*ptr++ << 8);
    *gch++ = (*ptr++ << 8);
    *bch++ = (*ptr++ << 8);
  }
}

static void
uncode_alias24(ptr, rbuf, gbuf, bbuf, end)
byte *ptr;
byte *rbuf, *gbuf, *bbuf;
byte *end;
{
  register int i;
  byte r, g, b;

  while ( rbuf < end ) {        /* while not end of buffer */
    if ( (i= *ptr++) > 1 ) {            /* how mary repetitions ? */
      b= *ptr++;
      g= *ptr++;
      r= *ptr++;
      while( i-- ) {    /* copy these triplet 'i' times */
        *rbuf++ = r;
        *gbuf++ = g;
        *bbuf++ = b;
      }
    }else {                     /* else, copy these triplet */
      *bbuf++ = *ptr++;
      *gbuf++ = *ptr++;
      *rbuf++ = *ptr++;
    }
  }
}

static void
uncode_alias(ptr, rbuf, end)
byte *ptr;
byte *rbuf;
byte *end;
{
  register int i;
  byte r;

  while ( rbuf < end ) {        /* while not end of buffer */
    if ( (i= *ptr++) > 1 ) {            /* how mary repetitions ? */
      r= *ptr++;
      while( i-- ) {    /* copy these index 'i' times */
        *rbuf++ = r;
      }
    }else {                     /* else, copy these index */
      *rbuf++ = *ptr++;
    }
  }
}

static void
write_rle(handle, image)
FILE *handle;
bitmap_hdr *image;
{
  register int i, j;
  rle_pixel **row;
  byte *r;
  int offset_last;

  VPRINTF(stderr, "Writing RLE file\n");
  /*
   * Fill the rle header with our ( little ) information.
   */
  the_hdr.rle_file = handle;
  the_hdr.xmin = 0;
  the_hdr.ymin = 0;
  the_hdr.xmax = image->xsize - 1;
  the_hdr.ymax = image->ysize - 1;
  if (image->depth > 8) {
    the_hdr.ncolors = 3;            /* 24 planes */
    the_hdr.ncmap = 0;
  }else {
    the_hdr.ncolors = 1;
    the_hdr.ncmap = 3;
    the_hdr.cmaplen = image->depth;
    /* Convert our cmap to rle cmap */
    bitmapcmap_to_rlecmap(image, &the_hdr);
  }
  the_hdr.alpha = 0;           /* we don't use alpha channels */

  /* Write the header */
  rle_put_setup(&the_hdr);

  /*
   * RLE write raster lines in reverse order, so we'll put
   * pointers to the last line.
   */
  offset_last = image->xsize * image->ysize - image->xsize;

  VPRINTF(stderr, "Compressing RLE lines\n");
  if (image->depth > 8) {
    /*
     * 24 planes, rle_putrow functions use a buffer which contains
     * a line, so we need do it.
     */
    /* Allocate some memory */
    row = (rle_pixel **)Malloc( 3 * sizeof(rle_pixel *) );
    if ( row == 0 )
      error(2);

    /* Pointers to last line */
    row[0] = image->r + offset_last;
    row[1] = image->g + offset_last;
    row[2] = image->b + offset_last;

    /* Write each line */
    for (i= 0; i< image->ysize; i++) {
      rle_putrow(row, image->xsize, &the_hdr);
      /*
       * Back up to the previous line.
       */
      for ( j = 0; j < 3; j++ )
	row[0] -= image->xsize;
    }
    /* free rle row buffer */
    free(row);

  }else {
    /*
     * A image with cmap.
     */
    /* if a simple plane => stored on R component */
    r = image->r + offset_last;      
    for (i= 0; i< image->ysize; i++) {
      rle_putrow(&r, image->xsize, &the_hdr);
      r -= image->xsize;
    }
  }

  /*
   * Put an EOF into the RLE file ( and THE END ! )
   */
  rle_puteof(&the_hdr);
}

static void
error(code)
int code;
{
  fprintf(stderr, "%s: ", the_hdr.cmd);
  switch (code) {
    case  0:
             break;
    case  1: fprintf(stderr, "Cannot open file\n");
             break;
    case  2: fprintf(stderr, "Out of memory\n");
             break;
    case  3: fprintf(stderr, "Error while reading input file\n");
             break;
    case  4: fprintf(stderr, "Error while writing output file\n");
             break;
    case  5: fprintf(stderr, "Input file is not an Alias pix\n");
             break;
    case  6: fprintf(stderr, "File corrupt ( uncompress too bytes )\n");
             break;
    case 99: fprintf(stderr, "Not ready\n");
             break;
    default: fprintf(stderr, "Unknow error code (%d)\n", code);
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


static char *read_file(handle, bytes)
FILE *handle;
int *bytes;
{
  char *buffer;

  /* Get size of file and allocate memory */
  *bytes= (int) filelen(handle);
  if ( *bytes > 0 )		/* Ok, it's a regular file. */
  {
    buffer= (char *) Malloc(*bytes);

    /* Read it */
    Fread(buffer, (int) (*bytes), 1, handle);
  }
  else				/* Oops!  It's a pipe. */
  {
    int n = 0, bufsize = 0;
    /* Read in chunks of BUFSIZ. */
    buffer = Malloc( BUFSIZ );
    while ( (n = fread( buffer + bufsize, 1, BUFSIZ, handle )) == BUFSIZ )
    {
      bufsize += BUFSIZ;
      buffer = realloc( buffer, bufsize + BUFSIZ );
      RLE_CHECK_ALLOC( the_hdr.cmd, buffer, "input image" );
    }
    if ( n >= 0 )
      n += bufsize;
    else
      n = bufsize;
  }

  /* Return the buffer */
  return buffer;
}

static long filelen(handle)
FILE *handle;
{
  long current_pos;
  long len;

  /* Save current position */
  current_pos= ftell(handle);

  /* Get len of file */
  fseek(handle, 0, 2);
  len= ftell(handle);

  /* Restore position */
  fseek(handle, current_pos, 0);

  return len;
}


