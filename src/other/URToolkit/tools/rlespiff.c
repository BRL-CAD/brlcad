/* 
 * rlespiff.c - Spiff up an image by stretching the contrast.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Thu Feb 15 1990
 * Copyright (c) 1990, University of Michigan
 *
 * Cast threshold as a double to prevent overflow error in calculating thresh
 *   Cliff Hathaway, University of Arizona Computer Science Dept. 082790
 */

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

void copy_raw();

#define CLAMP(x)	((x)<0 ? 0 : ((x)>255 ? 255 : (x)))

/*****************************************************************
 * TAG( main )
 *
 * Spiffs up an image by stretching the contrast range so that the
 * darkest pixel maps to black and the lightest to white.  For color
 * images, the determination is done on the min and max of the RGB
 * values.
 * 
 * Usage:
 * 	rlespiff [-t threshold] [-b blacklevel] [-w whitelevel] [-s] [-v]
 * 		 [-o outfile] [infile]
 * Inputs:
 * 	threshold:	The number of samples of a pixel value that
 * 			should be considered insignificant.  Specified
 * 			in pixels/million.  A threshold of 4 applied
 * 			to a 512x512 image would mean that any value
 * 			that existed at only one pixel would be
 * 			ignored.  Default is 10.
 * 	blacklevel:	The value of "black".  Default is 0.  The
 * 			minimum value input pixel will map to this
 * 			value on output.
 * 	whitelevel:	The value of "white".  Default is 255.  The
 * 			maximum value input pixel will map to this
 * 			value on output.
 * 	-s:		Treat each color channel separately.
 * 	-v:		Be verbose.
 * 	infile:		The input file.  If not specified, file is
 * 			read from standard input.
 * Outputs:
 * 	outfile:	The spiffed up file is written here.
 * Assumptions:
 * 	The color map is ignored.
 * Algorithm:
 * 	Read the file and find the minimum and maximum pixel values.
 * 	Then "rerun" the input, mapping the pixel values and writing
 * 	them to the output file.
 */
void
main( argc, argv )
int argc;
char **argv;
{
    int		rle_cnt = 0;
    int 	tflag = 0,
    		threshold = 10;
    int		bflag = 0,
    		blacklevel = 0;
    int 	wflag = 0,
    		whitelevel = 255;
    int		sflag = 0;
    int		nhist;
    int		verbose = 0;
    int 	oflag = 0;
    char       *outfname = NULL,
	       *infname = NULL;
    FILE       *outfile = stdout;
    int		rle_err;
    rle_hdr 	in_hdr,
    			out_hdr;
    rle_op    **scan, ***save_scan;
    register rle_op *scanp;
    int	       *nraw, **save_nraw;
    int		y, ynext;
    int		c, hc, i, x;
    register int t;		/* Temp value. */
    long int	(*histo)[256];
    int		thresh;		/* Mapped threshold. */
    int		maxval, minval;
    int		*slide;
    float	*stretch;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
  "% t%-threshold!d b%-blacklevel!d w%-whitelevel!d s%- v%- \
\to%-outfile!s infile%s",
		   &tflag, &threshold, &bflag, &blacklevel,
		   &wflag, &whitelevel,
		   &sflag, &verbose, &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
    
    /* Open the files */
    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infname, "r" );
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfname, 0 );
	
    /* Loop over all images in input file */
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );

	if ( rle_raw_alloc( &in_hdr, &scan, &nraw ) < 0 )
	    RLE_CHECK_ALLOC( in_hdr.cmd, 0, "image data" );

	save_scan = (rle_op ***)calloc( in_hdr.ymax + 1,
					sizeof( rle_op ** ) );
	save_nraw = (int **)calloc( in_hdr.ymax + 1,
				    sizeof( int * )  );
	RLE_CHECK_ALLOC( in_hdr.cmd, save_scan && save_nraw, "image data" );

	nhist = sflag ? in_hdr.ncolors : 1;
	histo = (long int (*)[256]) calloc( 256 * nhist, sizeof(long int) );
	stretch = (float *)calloc( nhist, sizeof(float) );
	slide = (int *)calloc( nhist, sizeof(int) );
	RLE_CHECK_ALLOC( in_hdr.cmd, histo && stretch && slide, 0 );

	/* Read the input and find min & max */
	while ( (y = rle_getraw( &in_hdr, scan, nraw )) != 32768 )
	{
	    /* Histogram values. */
	    for ( c = 0; c < in_hdr.ncolors; c++ )
	    {
		if ( sflag )
		    hc = c;
		else
		    hc = 0;

		for ( i = 0, scanp = scan[c]; i < nraw[c]; i++, scanp++ )
		    switch ( scanp->opcode )
		    {
		    case RByteDataOp:
			for ( x = 0; x < scanp->length; x++ )
			    histo[hc][scanp->u.pixels[x]]++;
			break;
		    case RRunDataOp:
			histo[hc][scanp->u.run_val] += scanp->length;
			break;
		    }
	    }

	    /* Save scanline data for second pass. */
	    copy_raw( &in_hdr, y, scan, nraw, save_scan, save_nraw );
	}

        
	/* Determine min & max */
	thresh = (((double)threshold * (in_hdr.xmax - in_hdr.xmin + 1) *
		  (in_hdr.ymax - in_hdr.ymin + 1) +
		  500000)) / 1000000;

	if ( !sflag )
	    thresh *= in_hdr.ncolors;

	for ( hc = 0; hc < nhist; hc++ )
	{
	    for ( i = 0; i < 256 && histo[hc][i] <= thresh; i++ )
		;
	    minval = i;
	    for ( i = 255; i >= 0 && histo[hc][i] <= thresh; i-- )
		;
	    maxval = i;

	    /* Mapping parameters */
	    if ( maxval > minval )
		stretch[hc] = (float)(whitelevel - blacklevel) /
		    (maxval - minval);
	    else
		stretch[hc] = 1.0;
	    slide[hc] = blacklevel - stretch[hc] * minval;

	    if ( verbose )
	    {
		fprintf( stderr, "%s image %d", 
			 in_hdr.rle_file == stdin ? "Standard input" : infname,
			 rle_cnt );
		if ( sflag )
		    fprintf( stderr, ", channel %d", hc );
		fprintf( stderr, " min = %d, max = %d\n", minval, maxval );
	    }
	}

	/* Pass 2 -- map pixels and write output file */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.rle_file = outfile;
	rle_addhist( argv, &in_hdr, &out_hdr );
	rle_put_setup( &out_hdr );
	for ( y = in_hdr.ymin, ynext = -1;
	      y <= in_hdr.ymax;
	      y++ )
	{
	    if ( save_scan[y] )
	    {
		/* Map pixel values */
		for ( c = 0; c < in_hdr.ncolors; c++ )
		{
		    float h_stretch;
		    int h_slide;

		    if ( sflag )
		    {
			h_stretch = stretch[c];
			h_slide = slide[c];
		    }
		    else
		    {
			h_stretch = stretch[0];
			h_slide = slide[0];
		    }

		    for ( i = 0, scanp = save_scan[y][c];
			  i < save_nraw[y][c];
			  i++, scanp++ )
			switch ( scanp->opcode )
			{
			case RByteDataOp:
			    for ( x = 0; x < scanp->length; x++ )
			    {
				t = scanp->u.pixels[x] * h_stretch + h_slide;
				scanp->u.pixels[x] = CLAMP( t );
			    }
			    break;
			case RRunDataOp:
			    t = scanp->u.run_val * h_stretch + h_slide;
			    scanp->u.run_val = CLAMP( t );
			    break;
			}
		}

		if ( ynext == -1 )
		{
		    ynext = in_hdr.ymin;
		    if ( y - ynext > 0 )
			rle_skiprow( &out_hdr, y - ynext );
		}
		else
		    if ( y - ynext > 1 )
			rle_skiprow( &out_hdr, y - ynext );
		rle_putraw( save_scan[y], save_nraw[y], &out_hdr );
		rle_freeraw( &in_hdr, save_scan[y], save_nraw[y] );
		rle_raw_free( &in_hdr, save_scan[y], save_nraw[y] );
		ynext = y;	/* Most recent "real" scanline. */
	    }
	}
	rle_puteof( &out_hdr );
	rle_raw_free( &in_hdr, scan, nraw );
	free( stretch );
	free( slide );
	free( histo );
    }

    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, argv[0], infname );

    exit( 0 );
}


/*****************************************************************
 * TAG( copy_raw )
 * 
 * Copy the raw scan data into a save area.
 * Inputs:
 * 	the_hdr:	Header describing input file.
 * 	y:		Number of this scanline.
 * 	scan:		Raw data for this scanline.
 * 	nraw:		Lengths of raw data for this scanline.
 * Outputs:
 * 	save_scan:	Array of pointers to saved raw data.
 * 	save_nraw:	Array of pointers to saved counts.
 * Assumptions:
 * 	The arrays save_scan and save_nraw have at least
 * 	y + 1 - the_hdr->ymin elements.
 * 	Copies allocation algorithm from rle_raw_alloc to the extent
 * 	that rle_raw_free can free the saved raw data and counts.
 * Algorithm:
 * 	Allocate memory, copy data.  Copies pointers to saved pixel
 * 	data, so caller need not call rle_freeraw.
 */
void
copy_raw( the_hdr, y, scan, nraw, save_scan, save_nraw )
rle_hdr *the_hdr;
int y;
rle_op **scan;
int *nraw;
rle_op ***save_scan;
int **save_nraw;
{
    int 	totlen = 0;
    register int c;

    /* Allocate space to save counts */
    save_nraw[y] = (int *)malloc( (the_hdr->ncolors + the_hdr->alpha) *
				  sizeof(int) );
    if ( save_nraw[y] == NULL )
	goto malloc_err;
    if ( the_hdr->alpha )
	save_nraw[y]++;
    /* Count total number of raw data to save, and save counts. */
    for ( c = -the_hdr->alpha; c < the_hdr->ncolors; c++ )
	totlen += save_nraw[y][c] = nraw[c];

    /* Allocate space to save raw data */
    save_scan[y] = (rle_op **)malloc( (the_hdr->ncolors + the_hdr->alpha) *
				      sizeof(rle_op *) );
    if ( save_scan[y] == NULL )
	goto malloc_err;
/*   BUG fixed by Michel GAUDET
 *   In  case of totlen = 0 (happens if all runs in a given line)
 *   the return value of malloc is NULL
 *   and it is not an error of malloc
 *   Correction : add one unit to the size
 */
    save_scan[y][0] = (rle_op *)malloc( 1 + totlen * sizeof(rle_op) );
    if ( save_scan[y][0] == NULL )
	goto malloc_err;
    if ( the_hdr->alpha )
	save_scan[y]++;

    /* Save raw data */
    for ( c = -the_hdr->alpha; c < the_hdr->ncolors; c++ )
    {
	if ( c > -the_hdr->alpha )
	    save_scan[y][c] = save_scan[y][c-1] + nraw[c-1];
	bcopy( scan[c], save_scan[y][c], nraw[c] * sizeof(rle_op) );
    }
    return;

malloc_err:
    RLE_CHECK_ALLOC( the_hdr->cmd, 0, 0 );
}
