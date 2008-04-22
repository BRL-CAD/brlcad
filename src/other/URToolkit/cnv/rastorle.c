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
 * rastorle  --
 *   convert Sun Rasterfile to Utah RLE format
 *   reads Sun rasterfile or stdin and writes stdout.
 *
 * Author:         Berry Kercheval (berry@mordor.s1.gov)
 *                 Lawrence Livermore National Laboratory
 *                 Livermore, CA.
 * Date:           9 March 1987
 * History:
 *   27 March 1987:  bbk: make it understand depth one (B&W) rasterfiles.
 *   6 September 1990: Clark: make it understand RT_BYTE_ENCODED rasterfiles,
 *			      add flag so alpha channel is done only if desired,
 *                            and output a colormap for images of depth 8.
 *
 * Usage is:
 *   rastorle [-a] [-o outfile.rle] [ infile.ras ] >out.rle
 *
 * -a		Fake an alpha channel.
 *
 * This will be difficult to compile on a non-sun, as it uses the pixrect
 * library include files.
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif
#if 0
rastorle()			/* Tag. */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <rasterfile.h>
#include <sysexits.h>
#include "rle.h"

#define MAXLINE        1280	/* max width of a line    */
#define MAXVAL          255	/* max value for pixel */
#define CMAPSIZE	256	/* Max size of color map  */
#define ESCAPE		128	/* Escape value for RT_BYTE_ENCODED rasterfiles */
/* #define DEBUG                /* debug output turned on */

int getbit();

static rle_map out_map[3*(1<<8)];

static unsigned char   *outrows[4];	/* array of rows for RLE output */
static unsigned char   *redline,	/* rle red row values */
	               *grnline,
	      	       *bluline,
	      	       *alfline;	/* alpha channel values */
static int		depth, npix;	/* Image size factors. */
static unsigned char   *image;

static unsigned char 	red[CMAPSIZE],	/* red colormap entries */
    		        green[CMAPSIZE],	/* Green ditto */
	    		blue[CMAPSIZE];	/* see a pattern? */

void
main(argc, argv)
int argc;
char *argv[];
{
    char	     *outfname = NULL;
    char	     *infname = NULL;
    int		      aflag = 0, oflag = 0;
    FILE             *rasfile;	/* where input comes from */
    FILE             *outfile;	/* where output goes to */
    int               i;	/* useful index */
    int               p = 0;	/* pixel value read from a file --
				 * used to index rasterfile color map. */
    int               count = 9999;	/* Holds current byte count for
				 *  rasterfiles of type RT_BYTE_ENCODED */
    int               h;	/* index for looping along a row */
    struct rasterfile rashdr;	/* standard header of a Sun rasterfile*/
    rle_hdr	      rlehdr;	/* Header of RLE file. */

    if ( scanargs( argc, argv, "% a%- o%-outfile!s infile%s\n(\
\tConvert a Sun Raster file to URT RLE format.\n\
\t-a\tFake an alpha channel: opaque when pixel non-zero.)",
		   &aflag, &oflag, &outfname, &infname ) == 0 )
	exit( EX_USAGE );

    rlehdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &rlehdr, cmd_name( argv ), outfname, 0 );

    rasfile = rle_open_f( rlehdr.cmd, infname, "r" );
    outfile = rle_open_f( rlehdr.cmd, outfname, "w" );

    rle_addhist( argv, (rle_hdr *)NULL, &rlehdr );

    /* first read the rasterfile header */
    if(fread(&rashdr, sizeof(rashdr), 1, rasfile) != 1)
    {
	fprintf(stderr, "Can't read rasterfile header.\n");
	exit(EX_DATAERR);
    }
    /* it has to start with the magic number... */
    if (rashdr.ras_magic != RAS_MAGIC)
    {
	fprintf(stderr, "Error: \"%s\" is not a rasterfile.\n",
		rasfile==stdin?"stdin":argv[1]);
	exit(EX_DATAERR);
    }
#ifdef DEBUG
    fprintf (stderr, "rasterfile width     =  %d\n", rashdr.ras_width);
    fprintf (stderr, "rasterfile height    =  %d\n", rashdr.ras_height);
    fprintf (stderr, "rasterfile depth     =  %d\n", rashdr.ras_depth);
    fprintf (stderr, "rasterfile length    =  %d\n", rashdr.ras_length);
    fprintf (stderr, "rasterfile type      =  %d\n", rashdr.ras_type);
    fprintf (stderr, "rasterfile maplength =  %d\n", rashdr.ras_maplength);
#endif

    /* Allocate image memory. */
    if ( rashdr.ras_depth == 32 )
	aflag = 0;		/* Alpha counted in depth for 32-bit image. */
    depth = (rashdr.ras_depth + 7) / 8 + aflag;
    npix = rashdr.ras_width * rashdr.ras_height;
    image = (unsigned char *) malloc( depth * npix );
    RLE_CHECK_ALLOC( rlehdr.cmd, image, "pixel" );
    if ( rashdr.ras_depth == 32 )
	aflag = 1;		/* Always alpha channel from 32-bit image. */
    if ( aflag )
	alfline = image + depth * npix - rashdr.ras_width;
    redline = image + npix - rashdr.ras_width;
    /* Depth > 2 means RGB or RGBA, <= 2 means 8-bit w/ or w/o alpha. */
    if ( depth > 2 )
    {
	grnline = image + 2 * npix - rashdr.ras_width;
	bluline = image + 3 * npix - rashdr.ras_width;
    }

    /* read in color map */
    switch(rashdr.ras_maptype)
    {
      case RMT_NONE:
#ifdef DEBUG
	fprintf (stderr, "No color map\n");
#endif
	for (i = 0; i < 256; i++)
	  red[i] = green[i] = blue[i] = i ;
	break;
      case RMT_RAW:
#ifdef DEBUG
	fprintf (stderr, "Raw color map\n");
#endif
	for (i = 0; i < 256; i++)
	  red[i] = green[i] = blue[i] = i ;
	for (i = 0; i < rashdr.ras_maplength; i++)
	  getc(rasfile);
	break;
      case RMT_EQUAL_RGB:
#ifdef DEBUG
	fprintf (stderr, "RGB color map\n");
#endif
	/* read red */
	for (i = 0; i < rashdr.ras_maplength/3; i++)
	  red[i] = getc(rasfile);
	/* read green */
	for (i = 0; i < rashdr.ras_maplength/3; i++)
	  green[i] = getc(rasfile);
	/* read blue */
	for (i = 0; i < rashdr.ras_maplength/3; i++)
	  blue[i] = getc(rasfile);
	break;
      default:
	fprintf (stderr, "Unknown color map type (%d)\n", rashdr.ras_maptype);
	exit (EX_DATAERR);
    }

    /* Read in the image. */
    switch(rashdr.ras_depth)
    {
      default:
	fprintf(stderr,
	  "Sorry, I can't deal with a rasterfile depth %d\n",rashdr.ras_depth);
	break;

      case 1:			/* black & white */
	/* A one channel output seems most reasonable. */
	RLE_SET_BIT(rlehdr, RLE_RED);
	RLE_CLR_BIT(rlehdr, RLE_GREEN);
	RLE_CLR_BIT(rlehdr, RLE_BLUE);
	if ( aflag )
            RLE_SET_BIT(rlehdr, RLE_ALPHA );
	else
            RLE_CLR_BIT(rlehdr, RLE_ALPHA );
	rlehdr.ncolors = 1;
	rlehdr.ncmap = 0 ;
	for (i=0; i<rashdr.ras_height; i++)
	{
	    for(h = 0; h < rashdr.ras_width; h++)
	    {
		p = getbit(rasfile, 0);
		redline[h] = p?0:MAXVAL;
		if ( aflag )
		    alfline[h] = p?0:MAXVAL;
	    }
	    redline -= rashdr.ras_width;
	    if ( aflag )
		alfline -= rashdr.ras_width;
	    getbit(NULL, 1);
	}
	break; /* end case 1: */

      case 8:
      case 24:
      case 32:
	/* next few lines stolen from painttorle.c */
	RLE_SET_BIT(rlehdr, RLE_RED);
	RLE_SET_BIT(rlehdr, RLE_GREEN);
	RLE_SET_BIT(rlehdr, RLE_BLUE);
	if ( aflag )
            RLE_SET_BIT(rlehdr, RLE_ALPHA );
        else
            RLE_CLR_BIT(rlehdr, RLE_ALPHA );

	if (rashdr.ras_depth == 8)
	{
	    rlehdr.ncolors =  1;
	    rlehdr.ncmap = 3;
	    rlehdr.cmaplen = 8;
	    rlehdr.cmap = out_map;
	    for (i=0;i<(1<<8);i++)
	    {
        	out_map[i+(0<<8)] = red[i] << 8;
        	out_map[i+(1<<8)] = green[i] << 8;
        	out_map[i+(2<<8)] = blue[i] << 8;
	    }
        }

	if (rashdr.ras_type == RT_BYTE_ENCODED)
	    count = 9999;
	for (i=0; i<rashdr.ras_height; i++)
	{
	    /* read a line from the rasterfile */
	    switch(rashdr.ras_depth)
	    {
	      case 8:
		for (h=0; h < rashdr.ras_width; h++)
		{
		    if (rashdr.ras_type != RT_BYTE_ENCODED)
			p = getc(rasfile);
		    else
		    {
			if (count == 9999)
			{
			    p = getc(rasfile);
			    if (p == ESCAPE)
			    {
				count = getc(rasfile);
				if (count == 0)
				    count = 9999;
				else
				    p = getc(rasfile);
			    }
			}
			else
			{
			    if (--count == 0)
				count = 9999;
			}
		    }
		    redline[h] = p;
		    /* fake up an alpha channel */
		    if ( aflag )
		    {
            		if (redline[h])
		  	    alfline[h] = 255;
			else
			    alfline[h] = 0;
		    }
		}
		/* Since records are an even number of bytes */
		if (rashdr.ras_width & 1) p = getc(rasfile);
		break ;

	      case 24:
		for (h=0; h < rashdr.ras_width; h++)
		{
		    register int r,g,b ;
		    r = getc(rasfile);
		    g = getc(rasfile);
		    b = getc(rasfile);
		    redline[h] = red[r];
		    grnline[h] = green[g];
		    bluline[h] = blue[b];
		    /* fake up an alpha channel */
		    if ( aflag )
		    {
            		if (redline[h] || grnline[h] || bluline[h])
		  	    alfline[h] = 255;
			else
			    alfline[h] = 0;
		    }
		}
		/* Since records are an even number of bytes */
		if (rashdr.ras_width & 1) p = getc(rasfile);
		break ;

	      case 32:
		for (h=0; h < rashdr.ras_width; h++)
		{
		    register int r,g,b,a ;
		    a = getc(rasfile);
		    r = getc(rasfile);
		    g = getc(rasfile);
		    b = getc(rasfile);
		    redline[h] = red[r];
		    grnline[h] = green[g];
		    bluline[h] = blue[b];
		    alfline[h] = a;
		}
		break ;
	    }
	    if ( aflag )
		alfline -= rashdr.ras_width;
	    redline -= rashdr.ras_width;
	    if ( depth > 2 )
	    {
		grnline -= rashdr.ras_width;
		bluline -= rashdr.ras_width;
	    }
	}
	break; /* end case 8: */
    } /* end switch */

    /* Set up common output parameters. */
    rlehdr.rle_file = outfile;
    rlehdr.xmax = rashdr.ras_width-1;
    rlehdr.ymax = rashdr.ras_height-1;
    if ( aflag )
	rlehdr.alpha = 1;
    else
	rlehdr.alpha = 0;

    /* Write the header. */
    rle_put_setup( &rlehdr );

    /* Write the image. */
    outrows[0] = alfline;
    outrows[1] = redline;
    outrows[2] = grnline;
    outrows[3] = bluline;
    for ( i = 0; i <= rlehdr.xmax; i++ )
    {
	for ( h = 1 - aflag; h < depth + 1 - aflag; h++ )
	    outrows[h] += rashdr.ras_width;
	rle_putrow( &outrows[1], rashdr.ras_width, &rlehdr );
    }

    rle_puteof( &rlehdr );

    /* Free memory. */
    free( image );
}

/*
 * get a bit from the file fp.  Read a byte and return its bits
 * one at a time.  (actually returns zero or non-zero).
 */
int
getbit(fp, force)
FILE *fp;
int force;			/* true if we should read a byte anyway */
{
    static int c;		/* the char we are picking apart */
    static unsigned int mask
      = 0x0;			/* mask to get the next bit -- init to huge
				 * so that we get a byte the first time */
    int val;			/* value to be returned */
    if (force)			/* read a new byte next time */
    {
	mask = 0x0;
	return 0;
    }
    if(mask == 0)		/* time to get the next byte */
    {
	c = getc(fp);
	mask = 0x80;		/* reset the mask */
    }
    val = c & mask;		/* true if this bit on */
    mask >>= 1;			/* shift mask over one bit */
    return val;			/* the bit we saved goes back to caller */
}
