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
 * rletoraw.c - Convert the RLE format to the kitchen sink.
 * 
 * Author:	Martin R. Friedmann
 * 		Vision and Modeling Group/Media Lab
 * 		Massachusetts Institute of Technology
 * Date:	Thu Sep 13 1990
 * Copyright (c) 1990, Martin R. Friedmann
 *
 * usage : rletoraw [-{Ns}] [-r] [-w width] [-h height] [-f header-size]
 *                   [-t tailer-size] [-n nchannels] [-a]
 *                   [-p scanline-pad] [-l left-scanline-pad] [-o outfile]
 *                   [infile]
 *
 * -a dont strip the alpha channel from the rle file
 * -s output data in scanline interleaved order
 * -N output data in non-interleaved order (eg. | split -Wid*Height -)
 * -r reverse the channel order (e.g. write data as ABGR instead of
 *    the default RGBA order)
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

/* hey.. why spin normally! */
#define duff(counter, block) {\
  while (counter >= 4) {\
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     counter -= 4;\
  } \
  switch (counter & 3) { \
     case 3:    { block; } \
     case 2:    { block; } \
     case 1:    { block; } \
     case 0:    counter = 0;\
     }\
}


#define WRITE_DATA() \
    if (fwrite( outrows, 1, fwrite_len, outfile ) != fwrite_len) {\
	perror( "write error" );\
	exit(-2);\
    }\
    

void
main(argc,argv) 
int argc;
char *argv[];
{ 
    int i, rle_err;
    char *nullbytes = NULL;
    char *infname = NULL, *outfname = NULL, *progname;
    FILE *infile, *outfile;
    rle_hdr hdr;
    int oflag = 0;
    int fflag = 0, tflag = 0, Nflag = 0, rflag = 0;
    int header = 0, trailer = 0;
    int pflag= 0, right_pad = 0, lflag = 0, left_pad = 0;
    int aflag = 0, output_alpha = 0, sflag = 0;

    int height, width, nochan;
    int red_pos, alpha_pos, green_pos = 0, blue_pos = 0;
    int img_size;

    /* Default color values */
    unsigned char *outrows;
    rle_pixel **inrows;
    int outrows_size;
    int y;
    int line_pos;
    int fwrite_len;
    /* unsigned char *fwrite_pos; */

    progname = cmd_name( argv );

    if ( scanargs( argc, argv, 
		  "% Ns%- r%- a%- f%-header-size!d t%-trailer-size!d \n\
\t\tp%-scanline-pad!d l%-left-scanline-pad!d o%-outfile!s \n\
\t\tinfile%s\n(\
\t-a\tDon't strip the alpha channel from the rle file.\n\
\t-f,-t\tEach output image is preceded (followed) by so many bytes.\n\
\t-l,-p\tScanlines are padded on left (right) end with so many bytes.\n\
\t\tAll padding is with null (0) bytes.\n\
\t-s\tOutput data in scanline interleaved order\n\
\t-N\tOutput data in non-interleaved order (eg. | split -Wid*Height -)\n\
\t-r\treverse the channel order (e.g. write data as ABGR instead of\n\
\t\tthe default RGBA order).)",
		  &Nflag, &rflag, &aflag, &fflag, &header,
		  &tflag, &trailer, &pflag, &right_pad, &lflag, &left_pad,
		  &oflag, &outfname, &infname ) == 0)
	exit( 1 );

    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname, 0 );

    if (Nflag) {
	if (Nflag & 1) {
	    Nflag = 0;
	    sflag = 1;
	} else {
	    Nflag = 1;
	    sflag = 0;
	}
    }
	       

    /* Open Raw file */
    infile = rle_open_f( progname, infname, "r" );
    outfile = rle_open_f( progname, outfname, "w" );


    rle_dflt_hdr.rle_file = infile;
    
    if ( (rle_err = rle_get_setup(&rle_dflt_hdr)) < 0) {
	rle_get_error( rle_err, progname, infname );
	exit ( rle_err );
    }

    rle_dflt_hdr.xmax -= rle_dflt_hdr.xmin;
    rle_dflt_hdr.xmin = 0;
    rle_dflt_hdr.ymax -= rle_dflt_hdr.ymin;
    rle_dflt_hdr.ymin = 0;
    
    width = rle_dflt_hdr.xmax + 1;
    height = rle_dflt_hdr.ymax + 1;
    nochan = rle_dflt_hdr.ncolors;
    
    fprintf(stderr, "width %d, height %d, channels %d\n", width, height, nochan);

    if (! aflag)
	RLE_CLR_BIT(rle_dflt_hdr, RLE_ALPHA);
    output_alpha = (aflag && RLE_BIT(rle_dflt_hdr, RLE_ALPHA)) ? 1 : 0;
    
    /* for -Non-interleaved case, we need nochan-1 whole channels of buffer */
    /* HACK: we allocate more memory; we jack the size for the first fread */
    outrows_size = width * (nochan + output_alpha);
    if ( Nflag )
	outrows_size *= height;
    if ((outrows = (unsigned char *) malloc ( outrows_size )) == NULL ) {
	fprintf(stderr, "%s: No memory available for rows malloc\n", progname);
	exit(-2);
    }
    
    img_size = width * height;

    /* Were only interested in R, G, & B */
    for (i = 3; i < rle_dflt_hdr.ncolors; i++)
	RLE_CLR_BIT(rle_dflt_hdr, i);

    if (rle_row_alloc( &rle_dflt_hdr, &inrows )) {
	fprintf(stderr, "%s: No memory available for rle_row_alloc\n", progname);
	exit(-2);
    }

    /* maybe faster to malloc and fwrite than to do lots of GETCs, Idunno */
    if (fflag || tflag) {
	nullbytes = (char *) malloc ( (header > trailer) ? header : trailer );
	if (! nullbytes)
	    fprintf(stderr, "%s: No memory for nullbytes\n", progname);
	else
	    for( i = (header > trailer) ? header : trailer; i > 0; )
		nullbytes[--i] = 0;
    }

    /* setup byte positions for reversed colors or otherwise */
    if ( rflag ) {
	alpha_pos = 0;
	/* alpha comes first if it's there */
	if (nochan > 2) {
	    red_pos = 2 + output_alpha;
	    green_pos = 1 + output_alpha;
	    blue_pos = 0 + output_alpha;
	} else
	    red_pos = 0 + output_alpha;
    }
    else {
	alpha_pos = nochan;
	red_pos = 0;
	if (nochan > 2) {
	    green_pos = 1;
	    blue_pos = 2;
	}
    }
    
    if ( Nflag ) {
	red_pos *= img_size;
	green_pos *= img_size;
	blue_pos *= img_size;
	alpha_pos *= img_size;
    } else if ( sflag ) {
	red_pos *= width;
	green_pos *= width;
	blue_pos *= width;
	alpha_pos *= width;
    }

    y = height;
    line_pos = 0;
    fwrite_len = outrows_size;
    /* fwrite_pos = outrows; */

    /* write the header */
    if (fflag)
	fwrite( nullbytes, 1, header, outfile );
    
    while (--y >= 0) {
	register rle_pixel *p, *o;
	register int stride = nochan + output_alpha, count;
	
	rle_getrow(&rle_dflt_hdr, inrows);
	/* non-interleaved data is easier to compute than interleaved */
	if ( Nflag ) {
	    /*
	     * This is a wierd case...  We had to read in all of the
	     * scanlines for all but one of the channels...  Then we can
	     * handle things scanline by scanline...  We have to jack
	     * the fread parameters for all of the remaining scanlines
	     */
	    
	    if ( output_alpha ) 
		bcopy(inrows[RLE_ALPHA], outrows + alpha_pos + line_pos,width);
	    
	    bcopy(inrows[RLE_RED], outrows + red_pos + line_pos, width);
	    
	    if (nochan > 2) {
		bcopy(inrows[RLE_GREEN], outrows + green_pos + line_pos,width);
		bcopy(inrows[RLE_BLUE], outrows + blue_pos + line_pos,width);
	    }
	    line_pos += width;
	} else if (sflag) {
	    /* scanline interleaved: we only need to copy the data */
	    if ( output_alpha )
		bcopy (inrows[RLE_ALPHA], outrows + alpha_pos, width);
	    
	    bcopy (inrows[RLE_RED], outrows + red_pos, width);
	    
	    if (nochan > 2) {
		bcopy(inrows[RLE_GREEN], outrows + green_pos, width);
		bcopy(inrows[RLE_BLUE], outrows + blue_pos, width);
	    }
	}
	else 
	{
#define COPY_LINE() duff(count, *o = *p++; o += stride);
	    
	    /* ahhh...  the default.  interleaved data */
	    if ( output_alpha ) {
		o = outrows + alpha_pos;
		p = inrows[RLE_ALPHA];
		count = width;
		COPY_LINE();
	    }	    
	    
	    o = outrows + red_pos;
	    p = inrows[RLE_RED];
	    count = width;
	    COPY_LINE();
	    
	    if (nochan > 2) {
		o = outrows + green_pos;
		p = inrows[RLE_GREEN];
		count = width;
		COPY_LINE();
		
		o = outrows + blue_pos;
		p = inrows[RLE_BLUE];
		count = width;
		COPY_LINE();
	    }
	}
	
	/* LEFT_PAD */
	for (count = 0; count < left_pad; count++)
	    putc('\0', outfile);
	
	/* WRITE_SCANLINE */
	if (! Nflag)
	    WRITE_DATA();
	
	/* RIGHT_PAD */
	for (count = 0; count < right_pad; count++)
	    putc('\0', outfile);
	
    }
    if ( Nflag )
	WRITE_DATA();
    
    /* write the trailer */
    if (tflag)
	fwrite( nullbytes, 1, trailer, outfile );
    
    exit(0);
}

