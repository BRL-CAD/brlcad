
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
 * rawtorle.c - Convert the kitchen sink to RLE format.
 *
 * Author:	Martin R. Friedmann while residing@media-lab
 * 		Vision and Modeling Group/Media Lab
 * 		Massachusetts Institute of Technology
 * Date:	Fri Feb 16 1990
 * Copyright (c) 1990, Martin R. Friedmann
 *
 * usage : rawtorle [-{Ns}] [-r] [-w width] [-h height] [-f header-size]
 *                   [-t tailer-size] [-n nchannels] [-a [alpha-value]]
 *                   [-p scanline-pad] [-l left-scanline-pad] [-o outfile]
 *                   [infile]
 *
 * -a [value] add a blank alpha channel to the rle file
 * -s input data is in scanline interleaved order
 * -N input is in non-interleaved order (eg. cat pic.r pic.g pic.b | rawtorle)
 * -r reverse the channel order (e.g. read data as ABGR instead of
 *    the default RGBA order)
 *
 * Examples:
 * 512x512 greyscale image:  rawtorle -w 512 -h 512 -n 1
 * 640x512 raw RGB file   :  rawtorle -w 640 -h 512 -n 3
 * picture.[rgb]          :  cat picture.* | rawtorle -w 640 -h 512 -n 3 -N -r
 * JPLs ODL voyager pics  :  rawtorle -w 800 -h 800 -f 2508 -t 1672 -n 1 -p 36
 * 24bit rasterfile       :  rawtorle -f 32 -w ... -h ... -n 3
 * 8 bit rasterfile w/cmap:  Uh er Uh....   use rastorle anyway huh?
 * pic.[000-100].[rgb]    :  cat pic.* | rawtorle -w ... -h ... -n 3 -s -r
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

int
main(argc,argv)
int argc;
char *argv[];
{
    int i;
    char *header_bytes = NULL, *trailer_bytes = NULL;
    char *infname = NULL, *outfname = NULL;
    FILE *infile, *outfile;
    rle_hdr hdr;
    int oflag = 0, wflag = 0, hflag = 0, nflag = 0;
    int fflag = 0, tflag = 0, Nsflag = 0, rflag = 0;
#define Nflag	(Nsflag & 2)
#define sflag	(Nsflag & 1)
    int header = 0, trailer = 0, alpha_value = 255;
    int pflag= 0, right_pad = 0, lflag = 0, left_pad = 0;
    int aflag = 0, input_alpha = 0;
    int height = 512, width = 512, nochan = 3;
    int red_pos, alpha_pos, green_pos = 0, blue_pos = 0;
    int img_size;

    /* Default color values */
    rle_pixel **outrows;
    unsigned char *inrows;
    int inrows_size;

    if ( scanargs( argc, argv,
		  "% Ns%- r%- w%-width!d h%-height!d f%-header-size!d \n\
\t\tt%-trailer-size!d n%-nchannels!d a%-alpha-value%d  \n\
\t\tp%-scanline-pad!d l%-left-scanline-pad!d o%-outfile!s \n\
\t\tinfile%s\n(\
\t-a\tAdd an alpha channel to the rle file with given value.\n\
\t-f,-t\tEach input image is preceded (followed) by so many bytes.\n\
\t-l,-p\tScanlines are padded on left (right) end with so many bytes.\n\
\t-n\tNumber of channels in the input. If 2 or 4, presence of\n\
\t\talpha channel is assumed.\n\
\t-N\tinput is in non-interleaved order (e.g.\n\
\t\tcat pic.r pic.g pic.b | rawtorle)\n\
\t-r\treverse the channel order (e.g. read data as ABGR instead of\n\
\t\tthe default RGBA order)\n\
\t-s\tInput data is in scanline interleaved order.)",
		  &Nsflag, &rflag, &wflag, &width, &hflag, &height,
		  &fflag, &header, &tflag, &trailer, &nflag, &nochan,
		  &aflag, &alpha_value,
		  &pflag, &right_pad, &lflag, &left_pad,
		  &oflag, &outfname, &infname ) == 0)
	exit( 1 );

    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), outfname, 0 );

    /* Error check arguments. */
    if ( width <= 0 || height <= 0 ) {
	fprintf(stderr, "%s: Invalid width or height arg (%d,%d)\n",
		hdr.cmd, width, height);
	exit(-2);
    }

    if ( 0 > nochan || nochan > 4 ) {
	fprintf(stderr, "%s: Invalid number of channels %d\n",
		hdr.cmd, nochan);
	exit(-2);
    }

    /* For 2 channels we assume one is an alpha channel */
    if ( nochan == 2 || nochan == 4 )
	input_alpha = 1;

    /* either way, who cares */
    if ( Nflag && nochan == 1 )
	Nsflag = 0;

    if ( Nflag && (lflag || pflag))
	fprintf( stderr,
		 "%s: -N with -l or -p is not supported, padding ignored.\n",
		 hdr.cmd );

    /* Open Raw file */
    infile = rle_open_f( hdr.cmd, infname, "r" );
    outfile = rle_open_f( hdr.cmd, outfname, "w" );

    /* for -Non-interleaved case, we need nochan-1 whole channels of buffer */
    inrows_size = Nflag ? width * (height + 1) * (nochan - 1) : width * nochan;

    if ((inrows = (unsigned char *) malloc ( inrows_size )) == NULL) {
	fprintf(stderr, "%s: No memory available for inrows malloc\n",
		hdr.cmd);
	exit(-2);
    }
    /* HACK: we allocate more memory; we jack the size for the first fread */
    if ( Nflag )
	inrows_size = width * height * (nochan - 1) + width;

    img_size = width * height;

    rle_dflt_hdr.rle_file = outfile;
    rle_dflt_hdr.xmin = rle_dflt_hdr.ymin = 0;
    rle_dflt_hdr.xmax = width - 1;
    rle_dflt_hdr.ymax = height - 1;
    rle_dflt_hdr.alpha = aflag || input_alpha;
    switch ( nochan ) {
    case 4: case 3:
	rle_dflt_hdr.ncolors = 3;
	break;
    case 2: case 1:
	rle_dflt_hdr.ncolors = 1;
	break;
    }
    if ( aflag || input_alpha )
	RLE_SET_BIT( rle_dflt_hdr, RLE_ALPHA );
    else
	RLE_CLR_BIT( rle_dflt_hdr, RLE_ALPHA );
    rle_addhist( argv, (rle_hdr *)NULL, &rle_dflt_hdr );

    /* maybe faster to malloc and fread than to do lots of GETCs, Idunno */
    if (fflag)
	header_bytes = (char *) malloc ( header );
    if (tflag)
	trailer_bytes = (char *) malloc ( trailer );

    if (rle_row_alloc( &rle_dflt_hdr, &outrows )) {
	fprintf(stderr, "%s: No memory available for rle_row_alloc\n",
		hdr.cmd);
	exit(-2);
    }

    /* insert alpha channel */
    if ( aflag && !input_alpha )
	for (i = 0; i < width; i++)
	    outrows[RLE_ALPHA][i] = alpha_value;

    /* setup byte positions for reversed colors or otherwise */
    if ( rflag ) {
	alpha_pos = 0;
	/* alpha comes first if it's there */
	if (nochan > 2) {
	    red_pos = 2 + input_alpha;
	    green_pos = 1 + input_alpha;
	    blue_pos = 0 + input_alpha;
	} else
	    red_pos = 0 + input_alpha;
    }
    else {
	alpha_pos = nochan - input_alpha;
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
    }
    if ( sflag ) {
	red_pos *= width;
	green_pos *= width;
	blue_pos *= width;
	alpha_pos *= width;
    }
    while ( !feof( infile ))
    {
	int y = height;
	int ni_y = 0;   /* only used in -N cases */
	int first_line = 1;
	int fread_len = inrows_size;
	unsigned char *fread_pos = inrows;

	/* skip the header */
	if (fflag)
	    fread( header_bytes, 1, header, infile );

	while (--y >= 0) {
	    register rle_pixel *p, *o;
	    register int stride = nochan, count;

	    /* LEFT_PAD */
	    for (count = 0; count < left_pad; count++)
		getc(infile);

	    /* READ_SCANLINE */
	    if (fread( fread_pos, 1, fread_len, infile ) != fread_len) {
		if (!first_line)
		    perror( "read error" );
		exit (!first_line);
	    }

	    /* RIGHT_PAD */
	    for (count = 0; count < right_pad; count++)
		getc(infile);

	    /* non-interleaved data is easier to compute than interleaved */
	    if ( Nflag ) {
		/*
		 * This is a wierd case...  We had to read in all of the
		 * scanlines for all but one of the channels...  Then we can
		 * handle things scanline by scanline...  We have to jack
		 * the fread parameters for all of the remaining scanlines
		 */
		if ( first_line ) {
		    fread_len = width;
		    fread_pos = inrows + (img_size * (nochan - 1));
		}

		if ( input_alpha )
		    outrows[RLE_ALPHA] = (rflag ? inrows + ni_y : fread_pos);

		if ( rflag )
		    outrows[RLE_RED] = fread_pos;
		else
		    outrows[RLE_RED] = inrows + red_pos + ni_y;

		if (nochan > 2) {
		    outrows[RLE_GREEN] = inrows + green_pos + ni_y;
		    if ( rflag || input_alpha )
			outrows[RLE_BLUE] = inrows + blue_pos + ni_y;
		    else
			outrows[RLE_BLUE] = fread_pos;
		}
		ni_y += width;
	    } else if ( sflag ) {
		/* scanline interleaved! Like the rle_putrow format.. cake  */
		/* we only need to jack the pointers on the first scanline */
		if (first_line) {
		    if ( input_alpha )
			outrows[RLE_ALPHA] = inrows + alpha_pos;

		    outrows[RLE_RED] = inrows + red_pos;

		    if (nochan > 2) {
			outrows[RLE_GREEN] = inrows + green_pos;
			outrows[RLE_BLUE] = inrows +  blue_pos;
		    }
		}
	    }
	    else {
		/* ahhh...  the default.  interleaved data */
		if ( input_alpha ) {
		    o = outrows[RLE_ALPHA];
		    p = inrows + alpha_pos;
		    count = width;
		    duff(count, *o++ = *p; p += stride);
		}

		o = outrows[RLE_RED];
		p = inrows + red_pos;
		count = width;
		duff( count, *o++ = *p; p += stride);

		if (nochan > 2) {
		    o = outrows[RLE_GREEN];
		    p = inrows + green_pos;
		    count = width;
		    duff( count, *o++ = *p; p += stride);

		    o = outrows[RLE_BLUE];
		    p = inrows + blue_pos;
		    count = width;
		    duff( count, *o++ = *p; p += stride);
		}
	    }
	    /* start the file */
	    if (first_line) {
		rle_put_setup( &rle_dflt_hdr );
		first_line = 0;
	    }
	    rle_putrow( outrows, width, &rle_dflt_hdr );
	}
	rle_puteof( &rle_dflt_hdr );

 	/* skip the trailer */
	if (tflag)
	    fread( trailer_bytes, 1, trailer, infile );
    }
    exit(0);
}

