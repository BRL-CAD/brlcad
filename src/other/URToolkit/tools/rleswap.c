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
#ifndef lint
char rcsid[] = "$Header$";
#endif
/*
rleswap()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

extern void scan_usage();
void setup_map(), print_map();

/*****************************************************************
 * TAG( main )
 *
 * Usage:
 *	rleswap [-mM] [-p channel-pairs,...] [-f from-channels,...]
 *		[-t to-channels,...] [-d delete-channels,...] [infile]
 * Inputs:
 * 	-m:		Modify only the colormap.  Image data is not
 * 			affected.
 * 	-M:		Don't modify the colormap.  Only the image
 * 			data is affected.
 *
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
main( argc, argv )
int argc;
char **argv;
{
    int nspec, * specs;
    int tflag = 0, fflag = 0, oflag = 0, dflag = 0, pflag = 0, verbose = 0;
    int mflag = 0;
    char * fname = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    static CONST_DECL char * argfmt =
	"% v%- Mm%- f%-from-channels!,d t%-to-channels!,d \n\
\td%-delete-channels!,d p%-channel-pairs!,d o%-outfile!s infile%s\n(\
\tRearrange or delete channels in an image.\n\
\t-v\tVerbose: explain (sort of) what's happening.\n\
\t-m\tModify only the colormap, leave the image alone.\n\
\t-M\tDon't modify the colormap.\n\
\t-f\tComma separated list of channels to copy from.  First goes to\n\
\t\toutput channel 0, second to output channel 1, etc.  An input\n\
\t\tchannel can be copied to several output channels this way.\n\
\t\tThe alpha channel is always copied \"straight across\".\n\
\t-t\tList of channels to copy to.  First specifies where input\n\
\t\tchannel 0 goes, second where input channel 1 goes, etc.  It is\n\
\t\tan error to try to copy more than one input channel to the same\n\
\t\toutput.  The alpha channel is always copied \"straight across\".\n\
\t-p\tSpecify input and output channels in pairs.  The first number in\n\
\t\ta pair is the input channel, the second is the output channel it\n\
\t\twill be copied to.  A given output channel should appear at most\n\
\t\tonce in the list.  The alpha channel is copied only if it is\n\
\t\texplicitly specified.\n\
\t-d\tA list of channels to delete from the input.)";
    register int i;
    int j;
    int * outchan = 0, noutput = 0, outalpha = -2;
    int *mapoutchan = 0, mapnoutput = 0, mapoutalpha = -2;
    rle_hdr 	in_hdr, out_hdr;
    rle_op ** scanraw, ** outraw;
    int * nraw, * outnraw, y, nskip;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, argfmt, &verbose, &mflag,
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
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), fname, "r");
    rle_names( &in_hdr, cmd_name( argv ), fname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    /* Read in header */
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Mflag == 1 means do color map only. */
	if ( mflag != 1 )
	    setup_map( fflag, tflag, dflag, pflag, specs, nspec,
		       &in_hdr, in_hdr.ncolors, in_hdr.alpha, "image data",
		       &outchan, &noutput, &outalpha );
	else
	    /* Make identity mapping. */
	    setup_map( 0, 0, 1, 0, (int *)0, 0,
		       &in_hdr, in_hdr.ncolors, in_hdr.alpha, "image data",
		       &outchan, &noutput, &outalpha );

	/* Mflag == 2 means don't touch color map. */
	if ( mflag != 2 && in_hdr.ncmap > 0 )
	    setup_map( fflag, tflag, dflag, pflag, specs, nspec,
		       &in_hdr, in_hdr.ncmap, 1, "color map",
		       &mapoutchan, &mapnoutput, &mapoutalpha );

	/* Be verbose if requested */
	if ( verbose )
	{
	    int cmap_different = mflag != 2 &&
		(mflag == 1 || mapnoutput != noutput);
	    if ( mflag != 1 )
		print_map( outalpha, noutput, outchan,
			   cmap_different ? "image data " : "" );
	    if ( cmap_different )
		print_map( -3, mapnoutput, mapoutchan, "color map " );
	}

	/* Set up output the_hdr now */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	/* Same as input, except for a few changes. */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(out_hdr.cmd, out_fname, "w");
	out_hdr.rle_file = outfile;
	out_hdr.ncolors = noutput;

	rle_addhist( argv, &in_hdr, &out_hdr );

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
	if ( out_hdr.background && out_hdr.ncolors > 0 )
	{
	    out_hdr.bg_color =
		(int *)malloc( out_hdr.ncolors * sizeof(int) );
	    RLE_CHECK_ALLOC( out_hdr.cmd, out_hdr.bg_color,
			     "output background color" );

	    for ( i = 0; i < noutput; i++ )
		if ( outchan[i] < 0 )
		    out_hdr.bg_color[i] = 0;	/* got to be something */
		else
		    out_hdr.bg_color[i] = in_hdr.bg_color[outchan[i]];
	}

	/* And color map!? */
	if ( mflag != 2 && in_hdr.ncmap > 0 )
	{
	    int cmaplen = 1 << out_hdr.cmaplen;
	    int cmapshift = 16 - out_hdr.cmaplen;

	    if ( mapnoutput > 0 )
	    {
		out_hdr.cmap = (rle_map *)malloc( mapnoutput * cmaplen *
						  sizeof(rle_map) );
		RLE_CHECK_ALLOC( out_hdr.cmd, out_hdr.cmap,
				 "output color map" );
	    }
	    else
		out_hdr.cmap = 0;
	    out_hdr.ncmap = mapnoutput;

	    /* If input channel is in color map, copy it, else use identity? */
	    for ( i = 0; i < mapnoutput; i++ )
		if ( mapoutchan[i] >= 0 && mapoutchan[i] < in_hdr.ncmap )
		{
		    register rle_map * imap, * omap;

		    imap = &in_hdr.cmap[mapoutchan[i] * cmaplen];
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
	if ( rle_raw_alloc( &in_hdr, &scanraw, &nraw ) < 0 )
	    RLE_CHECK_ALLOC( in_hdr.cmd, 0, "scanline memory" );


	/* Allocate buffer pointers for output */
	if ( rle_raw_alloc( &out_hdr, &outraw, &outnraw ) < 0 )
	    RLE_CHECK_ALLOC( out_hdr.cmd, 0, "scanline pointers" );
	for ( i = -out_hdr.alpha; i < out_hdr.ncolors; i++ )
	    if ( outraw[i] )
	    {
		free( (char *)outraw[i] );	/* don't need these */
		break;
	    };

	/* Finally, do the work */
	y = in_hdr.ymin - 1;
	while ( (nskip = rle_getraw( &in_hdr, scanraw, nraw )) != 32768 )
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

	    rle_freeraw( &in_hdr, scanraw, nraw );
	}

	rle_puteof( &out_hdr );	/* all done! */

	/* Release memory. */
	if ( outchan != specs )
	    free( outchan );
	rle_raw_free( &in_hdr, scanraw, nraw );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, in_hdr.cmd, fname );

    exit( 0 );
}

void
setup_map( fflag, tflag, dflag, pflag, specs, nspec,
		   in_hdr, ncolors, alpha, where,
		   outchan, noutput, outalpha )
int fflag, tflag, dflag, pflag;
int *specs;
int nspec;
rle_hdr *in_hdr;
int ncolors, alpha;
char *where;
register int **outchan, *noutput, *outalpha;
{
    int errs = 0;
    register int i;

    /* Initialize output variables. */
    *outchan = 0;
    *noutput = -1;
    *outalpha = -2;

    /* Set up mapping and sanity check simultaneously */
    if ( fflag )		/* where does output come from? */
    {
	*outchan = (int *)malloc( nspec * sizeof(int) );
	bcopy( (char *)specs, (char *)*outchan, nspec * sizeof(int) );
	*noutput = nspec;
	/* Sanity check */
	for ( i = 0; i < *noutput; i++ )
	    if ( (*outchan)[i] >= ncolors ||
		 (*outchan)[i] < -1 ||
		 (*outchan)[i] < 0 && !alpha )
	    {
		fprintf( stderr, "%s: No input channel %d in %s\n",
			 in_hdr->cmd, (*outchan)[i], where );
		errs++;
	    }

	if ( alpha )
	    *outalpha = -1;	/* pass alpha channel through */
    }

    if ( tflag )		/* where does input go */
    {
	if ( nspec > ncolors )
	{
	    fprintf( stderr,
	     "%s: Input %s has %d channels, can't swap %d channels\n",
		     where, in_hdr->cmd, ncolors, nspec );
	    errs++;
	}
	else
	{
	    /* Find highest output channel */
	    *noutput = -1;	/* assume none */
	    for ( i = 0; i < nspec; i++ )
		if ( specs[i] > *noutput )
		    *noutput = specs[i];
	    (*noutput)++;

	    /* Allocate space for output pointers */
	    if ( *noutput > 0 )
	    {
		*outchan = (int *)malloc( *noutput * sizeof( int ) );
		RLE_CHECK_ALLOC( in_hdr->cmd, *outchan,
				 "output scanline pointers" );
	    }

	    /* Initialize to empty state */
	    for ( i = 0; i < *noutput; i++ )
		(*outchan)[i] = -2;

	    /* Fill it in */
	    for ( i = 0; i < nspec; i++ )
	    {
		if ( specs[i] < -1 )
		{
		    fprintf( stderr, "%s: No channel %d in output %s\n",
			     in_hdr->cmd, specs[i], where );
		    errs++;
		}
		else if ( specs[i] < 0 )
		{
		    if ( *outalpha > -2 )
		    {
			fprintf( stderr,
	 "%s: Output alpha channel can't come from both inputs %d and %d.\n",
				 in_hdr->cmd, *outalpha, i );
			errs++;
		    }
		    else
			*outalpha = i;
		}
		else
		{
		    if ( (*outchan)[specs[i]] > -2 )
		    {
			fprintf( stderr,
	 "%s: Output channel %d can't come from both inputs %d and %d.\n",
				 in_hdr->cmd, specs[i],
				 (*outchan)[specs[i]], i );
			errs++;
		    }
		    else
			(*outchan)[specs[i]] = i;
		}
	    }
	    if ( *outalpha < -1 && alpha )
		*outalpha = -1;	/* map alpha channel through */
	}
    }

    if ( dflag )		/* Delete some input channels */
    {
	/* First, set up identity mapping from input to output */
	*noutput = ncolors;

	/* Allocate space for output pointers */
	*outchan = (int *)malloc( *noutput * sizeof( int ) );
	RLE_CHECK_ALLOC( in_hdr->cmd, *outchan,
			 "output scanline pointers" );

	for ( i = 0; i < *noutput; i++ )
	    (*outchan)[i] = i;
	if ( alpha )
	    *outalpha = -1;

	/* Now, delete indicated channels from mapping */
	for ( i = 0; i < nspec; i++ )
	    if ( specs[i] < -alpha ||
		 specs[i] >= ncolors )
		fprintf( stderr, "%s: Warning: No channel %d in input %s\n",
			 in_hdr->cmd, specs[i], where );
	    else if ( specs[i] == -1 && *outalpha == -2 ||
		      (*outchan)[specs[i]] == -2 )
		fprintf( stderr, "%s: Warning: Deleted channel %d twice\n",
			 in_hdr->cmd, specs[i] );
	    else if ( specs[i] >= 0 )
		(*outchan)[specs[i]] = -2;	/* null it out */
	    else
		*outalpha = -2;

	/* Figure out how many output channels we really have */
	while ( (*outchan)[*noutput - 1] == -2 )
	    (*noutput)--;
    }

    if ( pflag )		/* Do pairs of mappings */
    {
	/* Find highest output channel */
	*noutput = -1;		/* assume none */
	for ( i = 1; i < nspec; i += 2 )
	    if ( specs[i] > *noutput )
		*noutput = specs[i];
	(*noutput)++;

	/* Allocate space for output pointers */
	if ( *noutput > 0 )
	{
	    *outchan = (int *)malloc( *noutput * sizeof( int ) );
	    RLE_CHECK_ALLOC( in_hdr->cmd, *outchan,
			     "output scanline pointers" );
	}

	/* Initialize to empty state */
	for ( i = 0; i < *noutput; i++ )
	    (*outchan)[i] = -2;

	/* Fill it in */
	for ( i = 0; i < nspec; i += 2 )
	{
	    if ( specs[i] < -alpha ||
		 specs[i] >= ncolors )
	    {
		fprintf( stderr, "%s: No channel %d in input %s\n",
			 in_hdr->cmd, specs[i], where );
		errs++;
	    }
	    if ( specs[i+1] < -1 )
	    {
		fprintf( stderr, "%s: No channel %d in output %s\n",
			 in_hdr->cmd, specs[i+1], where );
		errs++;
	    }
	    else if ( specs[i+1] < 0 )
	    {
		if ( *outalpha > -2 )
		{
		    fprintf( stderr,
     "%s: Output alpha channel can't come from both inputs %d and %d.\n",
			     in_hdr->cmd, *outalpha, specs[i] );
		    errs++;
		}
		else
		    *outalpha = specs[i];
	    }
	    else
	    {
		if ( (*outchan)[specs[i+1]] > -2 )
		{
		    fprintf( stderr,
     "%s: Output channel %d can't come from both inputs %d and %d.\n",
			     in_hdr->cmd, specs[i+1],
			     (*outchan)[specs[i+1]], specs[i] );
		    errs++;
		}
		else
		    (*outchan)[specs[i+1]] = specs[i];
	    }
	}
    }

    /* If errors, go away */
    if ( errs )
	exit( 1 );
}

void
print_map( outalpha, noutput, outchan, where )
int outalpha, noutput;
int *outchan;
char *where;
{
    int i;

    fprintf( stderr, "Channel mapping %sfrom input to output:\n", where );
    if ( outalpha > -2 )
	if ( outalpha < 0 )
	    fprintf( stderr, "alpha\t-> alpha\n" );
	else
	    fprintf( stderr, "%d\t-> alpha\n", outalpha );
    else if ( outalpha == -2 )
	fprintf( stderr, "No output alpha channel\n" );
    for ( i = 0; i < noutput; i++ )
	if ( outchan[i] == -2 )
	    fprintf( stderr, "nothing\t-> %d\n", i );
	else if ( outchan[i] == -1 )
	    fprintf( stderr, "alpha\t-> %d\n", i );
	else
	    fprintf( stderr, "%d\t-> %d\n", outchan[i], i );
    if ( noutput == 0 && outalpha < -1 )
	fprintf( stderr, "Deleting %s\n", where );
}
