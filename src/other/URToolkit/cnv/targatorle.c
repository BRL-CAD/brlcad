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
 * targatorle.c - Convert some Truevision TARGA images to RLE.  Not all
 *   variations of the TARGA format are supported (yet).
 * 
 * Author:	Hann-Bin Chuang
 * 		Department of Chemistry
 *		Boston University
 * Date:	Tue. Aug 25 1987
 *
 * Usage is:
 *   targatorle  [-h header.lis] [-n nchannels] [-o outfile.rle] [infile.tga] 
 *
 * -h header.lis	write the targa header information to file "header.lis"
 * -n nchannels		Number of channels to write to output RLE file for 
 *			color images (3 or 4), default is same as source image.
 * -o outfile.rle	instead of stdout, use outfile.rle as output.
 *
 * rleflip is not necessary because Targa images and RLE both use
 * the last line (in the file) as the upper left of the image.
 */

/*
  Modifications:

  Made By:	Andrew Hadenfeldt
		Department of Electrical Engineering
		University of Nebraska-Lincoln

  Date:		16-Mar-1991

  What:		Rewrote file I/O code to try to be machine and/or byte-order independent.
		Changed pixel line arrays to use malloc to allow images wider than
		512 pixels to be processed.  Also added code to convert some 8- and 16-bit
		TARGA image formats.  For 8-bit images, a single channel is written.  For
		16-bit files, the color map is still ignored, and the 16-bit pixels
		are converted to 24 bits.  Added a swtich (-n) to specify the number of
		channels to write to the RLE file (3 or 4) for color images, allowing the
		alpha channel to be omitted if desired.  By default, the alpha channel will
		be copied to the RLE file.

		Also allow processing of "color" images with 8-bit pixels, but only if they
		have no colormap data.  This is sort of a hack, since our Truevision ATVista
		software can generate this format (they don't seem to use the B/W formats).

  Date:		12-Jul-1991

  What:		Added run-length compression decoding, tested with known type 10 images.
		Type 11 *should* also work, but who knows?  No support for any of the
		colormapped formats yet.  Also changed colormap code to properly read 
		and discard the colormap section if the image is in a true-color format.
*/

#include <stdio.h>
#include <sys/types.h>
#include "rle.h"

#define HEADSIZE 18
#define GETSHORT(N) (((unsigned short)(*(N+1))<<8) + (unsigned short)(*(N)))

/*
 * Description of header for files containing TARGA (Truevision, Inc.) images
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
      unsigned char   *image_id;      /* Image ID Field */
      unsigned char   *cmap_data;     /* color Map data */
      };

unsigned char	*outrows[4];
unsigned char *redline, *grnline, *bluline, *alfline;
FILE *infile;

int bwimage=0;
int truecolor=0;
int cmapped=0;
int rlcomp=0;
int nchannels;
int cmapbytes;
int pixwidth;
struct targafile tga_head;

int havedata=0;
int rlcount;
int bufinx;
unsigned char rlbuf[4*128];

void targa_getrow();
void init_comp();
unsigned char getbyte();

void
main(argc,argv) 
int argc;
char *argv[];
{ 
    FILE       *outfile;
    FILE       *hdrfile=NULL;
    char       *infname=NULL, 
    	       *outfname=NULL, 
    	       *hdrfname=NULL;
    int		oflag=0,
		nflag=0,
    		hflag=0;
    int		i;
    unsigned char headbuf[HEADSIZE];
    rle_hdr	out_hdr;

    if ( scanargs( argc,argv,
		   "% h%-hdrfile!s n%-nchannels!d o%-outfile!s infile%s\n(\
\tConvert TARGA file to URT format.\n\
\t-h\tSpecify separate header file.\n\
\t-n\tNumber of channels to write, defaults to same as input.)",
		   &hflag, &hdrfname, &nflag, &nchannels,
		   &oflag, &outfname, &infname ) == 0 )
	exit(1);

    out_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &out_hdr, cmd_name( argv ), outfname, 0 );

    infile  = rle_open_f(out_hdr.cmd, infname, "r");
    if ( hflag )
	hdrfile = rle_open_f(out_hdr.cmd, hdrfname, "w");
    outfile = rle_open_f(out_hdr.cmd, outfname, "w");

    /* Check that hdrfile and outfile aren't the same. */

    if ( hdrfile == outfile )
    {
	fprintf( stderr,
		 "%s: Can't write header and RLE data to same file.\n",
		 out_hdr.cmd );
	exit( 1 );
    }

    /* Read the image header */

    if (fread (headbuf, 1, HEADSIZE, infile)!=HEADSIZE)
    {
      fprintf(stderr,"targatorle: Read Error.\n");
      exit(1);
    }

    tga_head.num_char_id = headbuf[0];
    tga_head.cmap_type   = headbuf[1];
    tga_head.image_type  = headbuf[2];

    tga_head.cmap_origin = GETSHORT(headbuf+3);
    tga_head.cmap_length = GETSHORT(headbuf+5);
    tga_head.cmap_size   = headbuf[7];

    tga_head.image_x_origin = GETSHORT(headbuf+8);
    tga_head.image_y_origin = GETSHORT(headbuf+10);
    tga_head.image_width    = GETSHORT(headbuf+12);
    tga_head.image_height   = GETSHORT(headbuf+14);
    tga_head.image_pix_size = headbuf[16];

    tga_head.image_descriptor = headbuf[17];

    if (hflag) {
	fprintf (hdrfile, "num_char_id = %u \n", (unsigned)tga_head.num_char_id);
	fprintf (hdrfile, "cmap_type   = %u \n", (unsigned)tga_head.cmap_type);
	fprintf (hdrfile, "image_type  = %u \n", (unsigned)tga_head.image_type);
    }

    /* Check Color Map Type.  Currently, Truevision defines only 0 and 1 */

    if (tga_head.cmap_type==0) { 
	if (hflag) {
	    fprintf (hdrfile, "Color Map Type = 0 \n");
	}
    }
    else if (tga_head.cmap_type==1) {
	cmapped = 1;
	if (hflag) {
	    fprintf (hdrfile, "cmap_origin = %u \n",
		     (unsigned)tga_head.cmap_origin);
	    fprintf (hdrfile, "cmap_length = %u \n",
		     (unsigned)tga_head.cmap_length);
	    fprintf (hdrfile, "cmap_size   = %u \n",
		     (unsigned)tga_head.cmap_size);
	}
    }
    else {
	fprintf (stderr,
          "%s: Invalid Color Map Type Code (%d)\n", out_hdr.cmd,
		 tga_head.cmap_type);
	exit (1);
    }

    if (hflag) {
	fprintf (hdrfile, "image_x_origin = %u \n",
		 (unsigned)tga_head.image_x_origin);
	fprintf (hdrfile, "image_y_origin = %u \n",
		 (unsigned)tga_head.image_y_origin);
	fprintf (hdrfile, "image_width    = %u \n",
		 (unsigned)tga_head.image_width);
	fprintf (hdrfile, "image_height   = %u \n",
		 (unsigned)tga_head.image_height);
	fprintf (hdrfile, "image_pix_size = %u \n",
		 (unsigned)tga_head.image_pix_size);
    }

    /* Is this a supported image type? */

    switch (tga_head.image_type)
    {
				/* Image type codes from imagefmt.h, */
				/*   by Truevision, Inc.             */

      case 0:			/* No Image Data */
        fprintf(stderr,"%s: Image file is header only, no data.\n",
		out_hdr.cmd);
        exit(1);
        break;

      case 2:			/* Uncompressed Real-Color Image */

				/* Check for special case of a B/W */
				/* pretending to be a color image  */
        if ((tga_head.image_pix_size==8) &&
            (tga_head.cmap_length==0))
        {
          bwimage = 1;
          nchannels = 1;
	}
        else
        {
          truecolor = 1;
	}
        break;

      case 3:			/* Uncompressed B/W Image */
        bwimage = 1;
        nchannels = 1;
        break;

      case 10:			/* Run-length Encoded, Real-Color Image */
        truecolor = 1;
        rlcomp = 1;
        break;

      case 11:			/* Run-length Encoded, B/W Image */
        bwimage = 1;
        nchannels = 1;
        rlcomp = 1;
        break;

      case 1:			/* Uncompressed, Color-Mapped Image */
      case 9:			/* Run-length Encoded, Color-Mapped Image */
      default:
        fprintf(stderr,"%s: Image type (%d) not supported by this program.\n",
                out_hdr.cmd, tga_head.image_type);
        exit(1);
    }

    /* Check limits on what this program can handle */

    if (bwimage)
    {
      if (tga_head.image_pix_size!=8)
      {
         fprintf(stderr,"%s: B/W image pixel size must be 8.\n", out_hdr.cmd);
         exit(1);
      }
    }
    else if (truecolor)
    {
      if ( !((tga_head.image_pix_size==16) ||
             (tga_head.image_pix_size==24) ||
             (tga_head.image_pix_size==32) ))
      {
         fprintf(stderr,
		 "%s: Real-color image pixel size must be 16, 24 or 32.\n",
		 out_hdr.cmd);
         exit(1);
      }
      if (tga_head.image_pix_size==24) nchannels = 3;
    }

    /* Read image ID field */

    if ((i=(unsigned)tga_head.num_char_id)>=1) {
	tga_head.image_id = (unsigned char *)malloc(i);
	fread (tga_head.image_id, i, 1, infile);
    }

   /* Read Color Map, if any.  Do this even if the image is "true-color", since  */
   /* Truevision and other developers use the colormap area for their own needs. */

    if ((unsigned)tga_head.cmap_type==1) {
	switch((unsigned)tga_head.cmap_size) {

	  case 15:	/* Typical values for colormap width */
	  case 16:
	    cmapbytes = 2;
	    break;
	  case 24:
	    cmapbytes = 3;
	    break;
	  case 32:
	    cmapbytes = 4;
	    break;

	  default:
	    fprintf (stderr,"%s: Invalid color map width (%u bits)\n",
		     out_hdr.cmd, (unsigned)tga_head.cmap_size);
	    exit(1);
	}
	tga_head.cmap_data = 
		(unsigned char *)calloc((unsigned)tga_head.cmap_length,cmapbytes);
	fread (tga_head.cmap_data, cmapbytes, (unsigned)tga_head.cmap_length, infile);

    }

    /* Build RLE image header */

    RLE_SET_BIT(out_hdr, RLE_RED);

    if (nchannels==1)
    {
      out_hdr.ncolors = 1;
    }
    else
    {
      RLE_SET_BIT(out_hdr, RLE_GREEN);
      RLE_SET_BIT(out_hdr, RLE_BLUE);
      out_hdr.ncolors = 3;

      if (nchannels==4)		/* Save the alpha channel? */
      {
        RLE_SET_BIT(out_hdr, RLE_ALPHA);
        out_hdr.alpha = 1;
      }
    }

    out_hdr.rle_file = outfile;
    out_hdr.xmax = (unsigned)tga_head.image_width-1;
    out_hdr.ymax = (unsigned)tga_head.image_height-1;

    /* Allocate pixel buffers */

    alfline = (unsigned char *)malloc(tga_head.image_width);
    redline = (unsigned char *)malloc(tga_head.image_width);
    grnline = (unsigned char *)malloc(tga_head.image_width);
    bluline = (unsigned char *)malloc(tga_head.image_width);

    if ((!alfline) || (!redline) || (!grnline) || (!bluline))
    {
      fprintf(stderr,"%s: Image is too large for this program.\n",
	      out_hdr.cmd);
      exit(1);
    }

    outrows[0] = alfline;
    outrows[1] = redline;
    outrows[2] = grnline;
    outrows[3] = bluline;

    rle_addhist( argv, (rle_hdr *)NULL, &out_hdr );
    rle_put_setup( &out_hdr );

    /* Read and process each of the TARGA scan lines */

    pixwidth = ((unsigned)tga_head.image_pix_size+7)/8;	/* Pixel size in bytes */
    init_comp();
    for (i=0;i<(unsigned)tga_head.image_height;i++)
    {
      targa_getrow();
      rle_putrow (&outrows[1], (unsigned)tga_head.image_width, &out_hdr);
    }
    rle_puteof( &out_hdr );

    free(alfline);
    free(redline);
    free(grnline);
    free(bluline);
}

void targa_getrow()
{
  int j;
  int numpix;
  unsigned pxltmp;

  numpix = (unsigned)tga_head.image_width;

  switch (pixwidth) {

    case 1:					/* 8-bit pixels */
      for (j=0;j<numpix;j++) redline[j] = getbyte();
      return;
      break;

    case 2:					/* 16-bit pixels */
      for (j=0;j<numpix;j++)
      {
        pxltmp = (unsigned)getbyte();
        pxltmp |= ((unsigned)getbyte())<<8;

        bluline[j] =  (pxltmp        & 0x1f) << 3;
        grnline[j] = ((pxltmp >>  5) & 0x1f) << 3;
        redline[j] = ((pxltmp >> 10) & 0x1f) << 3;
        alfline[j] = ((pxltmp & 0x8000) ? 0xff : 0);
      }
      break;

    case 3:					/* 24/32 bit pixels */
    case 4:
      for (j=0;j<numpix;j++)
      {
        bluline[j]=getbyte();
        grnline[j]=getbyte();
        redline[j]=getbyte();
        if (pixwidth==4)
          alfline[j]=getbyte();
      }
      break;

    default:
      fprintf(stderr,"targatorle: Invalid pixel size (%d bytes) for this program.\n",
	      pixwidth);
  }
}

void init_comp()
{
  havedata = 0;
}

unsigned char getbyte()
{
  int j,k;
  int pkhdr;
  unsigned char pixdata[4];
  unsigned char *pp;
  unsigned char next;

  if (!rlcomp)
  {
     next = (unsigned)getc(infile)&0xff;
     return next;
  }

  if (!havedata)
  {
    pkhdr = getc(infile);
    rlcount = (pkhdr & 0x7f) + 1;

    if (pkhdr & 0x80)	/* Run-length packet */
    {
      fread(pixdata,pixwidth,1,infile);
      pp = rlbuf;
      for(k=0; k<rlcount; k++)
      for(j=0; j<pixwidth; j++)
        *(pp++) = pixdata[j];
    }
    else		/* Raw packet */
    {
      fread(rlbuf,pixwidth,rlcount,infile);
    }
    rlcount *= pixwidth;
    bufinx = 0;
    havedata = 1;
  }

  next = rlbuf[bufinx];
  if (++bufinx == rlcount) havedata = 0;

  return next;
}
