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
 * rleswap.c - Swap the channels in an RLE file around
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Jan 22 1987
 * Copyright (c) 1987, University of Utah
 */

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For malloc and free */
#include "rle.h"
#include "rle_raw.h"

extern void scan_usage();

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	rleswap [-p channel-pairs,...] [-f from-channels,...]
 *		[-t to-channels,...] [-d delete-channels,...] [infile]
 * Inputs:
 *	-p, -f, -t, -d:	Only one of these may be specified, they indicate
 *			different ways of indicating the channel swapping
 *			from input to output.
 *
 * 	channel-pairs:	A comma separated list of pairs of channel numbers
 *			The first channel of each pair indicates a channel
 *			in the input file that will be mapped to the
 *			the channel in the output file indicated by the
 *			second number in the pair.  No output channel
 *			number may appear more than once.  Any input channel
 *			not mentioned will not appear in the output file.
 *			Any output channel not mentioned will not receive
 *			image data.
 *
 *	from-channels:	A comma separated list of numbers indicating the
 *			input channel that maps to each output channel
 *			in sequence.  I.e., the first number indicates
 *			the input channel mapping to output channel 0.
 *			The alpha channel will be passed through unchanged
 *			if present.  Any input channels not mentioned
 *			in the list will not appear in the output.
 *
 *	to-channels:	A comma separated list of numbers indicating the
 *			output channel to which each input channel, in
 *			sequence, will map.  I.e., the first number gives
 *			the output channel to which the first input channel
 *			will map.  No number may be repeated in this list.
 *			The alpha channel will be passed through unchanged
 *			if present.  Any output channel not mentioned in
 *			the list will not receive image data.  If there
 *			are fewer numbers in the list than there are input
 *			channels, the excess input channels will be ignored.
 *			If there are more numbers than input channels, it
 *			is an error.
 *
 *	delete-channels:A comma separated list of input channels that should
 *			not appear in the output.  All other channels will
 *			be passed through unchanged.
 *
 *	infile:		Optional input file name specification.
 *      outfile:        Optional output file name specification.
 * Outputs:
 * 	An RLE file similar to the input file, but with the color channels
 *	remapped as indicated, will be written to the output.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
int
main(int argc, char **argv)
{
    int nspec, * specs;
    int tflag = 0, fflag = 0, oflag = 0, dflag = 0, pflag = 0, verbose = 0;
    char * fname = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    static const char * argfmt =
	"% v%- f%-from-channels!,d t%-to-channels!,d \n\
\td%-delete-channels!,d p%-channel-pairs!,d o%-outfile!s infile%s";
    register int i;
    int j, errs = 0;
    int * outchan = 0, noutput = 0, outalpha = -2;
    rle_hdr out_hdr;
    rle_op ** scanraw, ** outraw;
    int * nraw, * outnraw, y, nskip;
    int rle_cnt, rle_err;

    if ( scanargs( argc, argv, argfmt, &verbose,
		   &fflag, &nspec, &specs,
		   &tflag, &nspec, &specs,
		   &dflag, &nspec, &specs,
		   &pflag, &nspec, &specs,
		   &oflag, &out_fname, &fname ) == 0 )
	exit( 1 );

    /* Do some sanity checks */
    if ( fflag + tflag + dflag + pflag != 1 )
    {
	fprintf( stderr,
		 "%s: You must specify exactly one of -d, -f, -t, or -p\n",
		 argv[0] );
	/*
	 * Generate usage message.
	 */
	scan_usage( argv, argfmt );
	exit( 1 );
    }

    if ( pflag && (nspec % 2) != 0 )
    {
	fprintf( stderr, "%s: You must specify pairs of channels with -p\n",
		 cmd_name( argv ) );
	exit( 1 );
    }
    /* More later, after we have the RLE header in hand */

    /* Open input */
    rle_dflt_hdr.rle_file = rle_open_f(cmd_name( argv ), fname, "r");
   
    /* Read in header */
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &rle_dflt_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Set up mapping and sanity check simultaneously */
	if ( fflag )		/* where does output come from? */
	{
	    outchan = specs;
	    noutput = nspec;
	    /* Sanity check */
	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] >= rle_dflt_hdr.ncolors ||
		     outchan[i] < -1 ||
		     (outchan[i] < 0 && !rle_dflt_hdr.alpha) )
		{
		    fprintf( stderr, "%s: No input channel %d\n",
			     cmd_name( argv ), outchan[i] );
		    errs++;
		}

	    if ( rle_dflt_hdr.alpha )
		outalpha = -1;	/* pass alpha channel through */
	}

	if ( tflag )		/* where does input go */
	{
	    if ( nspec > rle_dflt_hdr.ncolors )
	    {
		fprintf( stderr,
		 "%s: Input file has %d channels, can't swap %d channels\n",
			 cmd_name( argv ), rle_dflt_hdr.ncolors, nspec );
		errs++;
	    }
	    else
	    {
		/* Find highest output channel */
		noutput = -1;	/* assume none */
		for ( i = 0; i < nspec; i++ )
		    if ( specs[i] > noutput )
			noutput = specs[i];
		noutput++;

		/* Allocate space for output pointers */
		outchan = (int *)malloc( noutput * sizeof( int ) );
		if ( outchan == NULL )
		{
		    fprintf( stderr, "%s: Malloc failed\n", cmd_name( argv ) );
		    exit( 1 );
		}

		/* Initialize to empty state */
		for ( i = 0; i < noutput; i++ )
		    outchan[i] = -2;

		/* Fill it in */
		for ( i = 0; i < nspec; i++ )
		{
		    if ( specs[i] < -1 )
		    {
			fprintf( stderr, "%s: No channel %d in output\n",
				 cmd_name( argv ), specs[i] );
			errs++;
		    }
		    else if ( specs[i] < 0 )
		    {
			if ( outalpha > -2 )
			{
			    fprintf( stderr,
     "%s: Output alpha channel can't come from both inputs %d and %d.\n",
				     cmd_name( argv ), outalpha, i );
			    errs++;
			}
			else
			    outalpha = i;
		    }
		    else
		    {
			if ( outchan[specs[i]] > -2 )
			{
			    fprintf( stderr,
	     "%s: Output channel %d can't come from both inputs %d and %d.\n",
				     cmd_name( argv ), specs[i],
				     outchan[specs[i]], i );
			    errs++;
			}
			else
			    outchan[specs[i]] = i;
		    }
		}
		if ( outalpha < -1 && rle_dflt_hdr.alpha )
		    outalpha = -1;	/* map alpha channel through */
	    }
	}

	if ( dflag )		/* Delete some input channels */
	{
	    /* First, set up identity mapping from input to output */
	    noutput = rle_dflt_hdr.ncolors;

	    /* Allocate space for output pointers */
	    outchan = (int *)malloc( noutput * sizeof( int ) );
	    if ( outchan == NULL )
	    {
		fprintf( stderr, "%s: Malloc failed\n", cmd_name( argv ) );
		exit( 1 );
	    }

	    for ( i = 0; i < noutput; i++ )
		outchan[i] = i;
	    if ( rle_dflt_hdr.alpha )
		outalpha = -1;

	    /* Now, delete indicated channels from mapping */
	    for ( i = 0; i < nspec; i++ )
		if ( specs[i] < -rle_dflt_hdr.alpha ||
		     specs[i] >= rle_dflt_hdr.ncolors )
		    fprintf( stderr, "%s: Warning: No channel %d in input\n",
			     cmd_name( argv ), specs[i] );
		else if ( (specs[i] == -1 && outalpha == -2) ||
			  outchan[specs[i]] == -2 )
		    fprintf( stderr, "%s: Warning: Deleted channel %d twice\n",
			     cmd_name( argv ), specs[i] );
		else if ( specs[i] >= 0 )
		    outchan[specs[i]] = -2;	/* null it out */
		else
		    outalpha = -2;

	    /* Figure out how many output channels we really have */
	    while ( outchan[noutput - 1] == -2 )
		noutput--;
	}

	if ( pflag )		/* Do pairs of mappings */
	{
	    /* Find highest output channel */
	    noutput = -1;	/* assume none */
	    for ( i = 1; i < nspec; i += 2 )
		if ( specs[i] > noutput )
		    noutput = specs[i];
	    noutput++;

	    /* Allocate space for output pointers */
	    outchan = (int *)malloc( noutput * sizeof( int ) );
	    if ( outchan == NULL )
	    {
		fprintf( stderr, "%s: Malloc failed\n", cmd_name( argv ) );
		exit( 1 );
	    }

	    /* Initialize to empty state */
	    for ( i = 0; i < noutput; i++ )
		outchan[i] = -2;

	    /* Fill it in */
	    for ( i = 0; i < nspec; i += 2 )
	    {
		if ( specs[i] < -rle_dflt_hdr.alpha ||
		     specs[i] >= rle_dflt_hdr.ncolors )
		{
		    fprintf( stderr, "%s: No channel %d in input\n",
			     cmd_name( argv ), specs[i] );
		    errs++;
		}
		if ( specs[i+1] < -1 )
		{
		    fprintf( stderr, "%s: No channel %d in output\n",
			     cmd_name( argv ), specs[i+1] );
		    errs++;
		}
		else if ( specs[i+1] < 0 )
		{
		    if ( outalpha > -2 )
		    {
			fprintf( stderr,
	 "%s: Output alpha channel can't come from both inputs %d and %d.\n",
				 cmd_name( argv ), outalpha, specs[i] );
			errs++;
		    }
		    else
			outalpha = specs[i];
		}
		else
		{
		    if ( outchan[specs[i+1]] > -2 )
		    {
			fprintf( stderr,
	 "%s: Output channel %d can't come from both inputs %d and %d.\n",
				 cmd_name( argv ), specs[i+1],
				 outchan[specs[i+1]], specs[i] );
			errs++;
		    }
		    else
			outchan[specs[i+1]] = specs[i];
		}
	    }
	}

	/* If errors, go away */
	if ( errs )
	    exit( 1 );

	/* Be verbose if requested */
	if ( verbose )
	{
	    fprintf( stderr, "Channel mapping from input to output:\n" );
	    if ( outalpha != -2 )
	    {
		if ( outalpha < 0 )
		    fprintf( stderr, "alpha\t-> alpha\n" );
		else
		    fprintf( stderr, "%d\t-> alpha\n", outalpha );
	    }
	    else
		fprintf( stderr, "No output alpha channel\n" );
	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] == -2 )
		    fprintf( stderr, "nothing\t-> %d\n", i );
		else if ( outchan[i] == -1 )
		    fprintf( stderr, "alpha\t-> %d\n", i );
		else
		    fprintf( stderr, "%d\t-> %d\n", outchan[i], i );
	}

	/* Set up output the_hdr now */
	out_hdr = rle_dflt_hdr;	/* same as input, basically */
	/* Except for a few changes */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(cmd_name( argv ), out_fname, "w");
	out_hdr.rle_file = outfile;
	out_hdr.ncolors = noutput;

	rle_addhist( argv, &rle_dflt_hdr, &out_hdr );

	if ( outalpha != -2 )
	{
	    out_hdr.alpha = 1;
	    RLE_SET_BIT( out_hdr, RLE_ALPHA );
	}
	else
	{
	    out_hdr.alpha = 0;
	    RLE_CLR_BIT( out_hdr, RLE_ALPHA );
	}
	for ( i = 0; i < noutput; i++ )
	    if ( outchan[i] != -2 )
		RLE_SET_BIT( out_hdr, i );
	    else
		RLE_CLR_BIT( out_hdr, i );

	/* Do background color */
	if ( out_hdr.background )
	{
	    out_hdr.bg_color =
		(int *)malloc( out_hdr.ncolors * sizeof(int) );

	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] < 0 )
		    out_hdr.bg_color[i] = 0;	/* got to be something */
		else
		    out_hdr.bg_color[i] = rle_dflt_hdr.bg_color[outchan[i]];
	}

	/* And color map!? */
	if ( rle_dflt_hdr.ncmap > 1 )	/* only one channel color map stays */
	{
	    int cmaplen = 1 << out_hdr.cmaplen;
	    int cmapshift = 16 - out_hdr.cmaplen;

	    out_hdr.cmap = (rle_map *)malloc( noutput * cmaplen *
					      sizeof(rle_map) );
	    out_hdr.ncmap = noutput;

	    /* If input channel is in color map, copy it, else use identity? */
	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] >= 0 && outchan[i] < rle_dflt_hdr.ncmap )
		{
		    register rle_map * imap, * omap;

		    imap = &rle_dflt_hdr.cmap[outchan[i] * cmaplen];
		    omap = &out_hdr.cmap[i * cmaplen];

		    for ( j = 0; j < cmaplen; j++ )
			*omap++ = *imap++;
		}
		else
		{
		    register rle_map * omap;

		    omap = &out_hdr.cmap[i * cmaplen];
		    for ( j = 0; j < cmaplen; j++ )
			*omap++ = j << cmapshift;
		}
	}

	/* Write output header */
	rle_put_setup( &out_hdr );

	/* Allocate raw buffers for input */
	rle_raw_alloc( &rle_dflt_hdr, &scanraw, &nraw );

	/* Allocate buffer pointers for output */
	rle_raw_alloc( &out_hdr, &outraw, &outnraw );
	for ( i = -out_hdr.alpha; i < out_hdr.ncolors; i++ )
	    if ( outraw[i] )
	    {
		free( (char *)outraw[i] );	/* don't need these */
		break;
	    };

	/* Finally, do the work */
	y = rle_dflt_hdr.ymin - 1;
	while ( (nskip = rle_getraw( &rle_dflt_hdr, scanraw, nraw )) != 32768 )
	{
	    nskip -= y;		/* difference from previous line */
	    y += nskip;
	    if ( nskip > 1 )
		rle_skiprow( &out_hdr, nskip - 1 );

	    if ( outalpha != -2 )
	    {
		outraw[-1] = scanraw[outalpha];
		outnraw[-1] = nraw[outalpha];
	    }

	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] != -2 )
		{
		    outraw[i] = scanraw[outchan[i]];
		    outnraw[i] = nraw[outchan[i]];
		}
		else
		    outnraw[i] = 0;

	    rle_putraw( outraw, outnraw, &out_hdr );

	    rle_freeraw( &rle_dflt_hdr, scanraw, nraw );
	}

	rle_puteof( &out_hdr );	/* all done! */

	/* Release memory. */
	if ( outchan != specs )
	    free( outchan );
	rle_raw_free( &rle_dflt_hdr, scanraw, nraw );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), fname );

    exit( 0 );
}
