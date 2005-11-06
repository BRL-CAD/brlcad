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
 * rleldmap.c - Load a color map into an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Jul 10 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif
#if 0
rleldmap()			/* Tag. */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

void get_rle_map(), linmap(), gammap(), filemap(), mfilemap();
void applymap(), shiftmap();

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	rleldmap [-ab] [-n nchan length] [-s bits] [-l [factor]] [-g gamma]
 *		[-{tf} file] [-m files ...] [-r rlefile]
 *		[-o outputfile] [inputfile]
 * Terminology:
 *	input map:	A color map already in the input RLE file.
 *	applied map:	The color map specified by the arguments that will
 *			be applied (loaded) to the input map, producing
 *			the output map.
 *	output map:	Unless -a or -b is specified, this is equal to the
 *			applied map.
 *	map composition:The output map is applied_map[input_map] if -a
 *			(after) is specified, or input_map[applied_map]
 *			if -b (before).  The maps being composed must either
 *			have the same number of channels, or one of them must
 *			have only one channel.  If an entry in the map being
 *			used as a subscript is larger than the length of the
 *			map being subscripted, it remains unchanged.  The
 *			output map will be the same length as the subscript
 *			map and will have the number of channels that is the
 *			larger of the two.  If the input map is used
 *			as a subscript, it will be downshifted the correct
 *			number of bits to serve as a subscript for the applied
 *			map.  This also applies to the applied map if it is
 *			taken from an RLE file (-r option below).
 *	nchan:		Number of separate lookup tables (channels) making up
 *			the color map.  Default 3 (one for red, one for
 *			green, and one for blue).
 *	length:		Number of entries in each channel of the map.  Default
 *			is 256 (8 bits).
 *	bits:		Size of each color map entry in bits.  Default
 *			log2 length.
 *	range:		Maximum value of a color map entry, equal to
 *			2^bits - 1.
 * Inputs:
 * 	-ab:		Compose the applied map with the input map.
 *			If the input file has no map, this flag has no effect.
 *
 *	-n nchan length Gives the size of the applied map  if it is not 3x256.
 *			length should be a power of two and will be rounded
 *			up if necessary.  If applying the map, nchan must be
 *			either 1 or equal to the number of channels in the
 *			input map.
 *
 *	-s bits		Gives the size in bits of each color map entry.
 *
 *	Note: exactly one of -l, -g, -t, -f, -F, or -r must be specified.
 *
 *	-l factor	Generate a linear applied map with entries equal to
 *			range * max(1.0, factor*(n/(length-1))).  Factor
 *			defaults to 1.0.
 *
 *	-g gamma	Generate an applied map with the given gamma.  The
 *			nth entry (out of length) is
 *			range * (n/(length-1))^gamma
 *
 *	-t file		Read color map entries from a file (t for table).
 *			The values for each channel of a particular entry
 *			follow each other in the file.  (Thus, for an RGB
 *			color map, the file would look like:
 *				red0	green0	blue0
 *				red1	green1	blue1
 *				...	...	...
 *			Line breaks in the input file are irrelevant.
 *
 *	-f file		Reads the applied map from a file, with all the
 *			entries for each channel following each other.  Thus,
 *			the input file would appear as
 *				red0 red1 red2 ...
 *				green0 green1 green2 ...
 *				blue0 blue1 blue2 ...
 *			As above, line breaks are irrelevant.
 *
 *	-m files ...	Read the color map for each channel from a separate
 *			file.  The number of files specified must equal the
 *			number of channels in the applied map.  [Note: the
 *			list of files must be followed by another flag or
 *			by the null flag -- to separate it from the inputfile.
 *
 *	-r rlefile	Read the color map from another RLE file.  In this
 *			case, the nchan, length and bits arguments will be
 *			ignored.
 *
 *	inputfile	The input RLE file.  Defaults to stdin.
 * Outputs:
 * 	-o outputfile	The output RLE file.  Defaults to stdout.
 * Assumptions:
 * 	As stated above.
 * Algorithm:
 * 	Compute the desired map, and either replace the input map with it
 *	or compose the maps as specified above.  Copy the image data from
 *	the input file to the output file.  If stdin is empty (no input at
 *	all, an output RLE file with just a color map will be generated).
 */
void
main( argc, argv )
int argc;
char **argv;
{
    int apply = 0, nflag = 0, nchan = 3, length = 256, range, lbits,
	sflag = 0, bits = 8, lflag = 0, gflag = 0,
	tflag = 0, mflag = 0, rflag = 0, oflag = 0, nmfiles = 0;
    int ichan, ilength, ilbits;	/* Input map parameters */
    int ochan, olength, olbits;	/* output map parameters */
    double factor = 1.0, gamma = 1.0;
    char * mapfname = NULL, ** mfnames = NULL, * rlefname = NULL,
	* outputfname = NULL, * inputfname = NULL;
    char map_comment[30];
    FILE *outfile = stdout;
    rle_map ** imap = NULL, ** omap, ** amap, ** allocmap();
    rle_hdr 	in_hdr, out_hdr, rle_f_hdr;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );
    rle_f_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
	"% ab%- n%-nchan!dlength!d s%-bits!d l%-factor%F g%-gamma!F \n\
\ttf%-file!s m%-files!*s r%-rlefile!s o%-outputfile!s inputfile%s",
		   &apply, &nflag, &nchan, &length, &sflag, &bits,
		   &lflag, &factor, &gflag, &gamma,
		   &tflag, &mapfname, &mflag, &nmfiles, &mfnames,
		   &rflag, &rlefname, &oflag, &outputfname,
		   &inputfname ) == 0 )
	exit( 1 );		/* bad arguments */

    /* Check for exclusive flag use */
    if ( (lflag != 0) + (gflag != 0) + (tflag != 0) + (mflag != 0) +
	 (rflag != 0) != 1 )
    {
	fprintf(stderr,
		"%s: Must specify exactly one of -l -g -t -f -m -r\n",
		 cmd_name( argv ) );
	exit(-1);
    }

    /* Compute color map parameters */
    if ( rflag )		/* from RLE file? */
    {
	if ( lflag )
	    fprintf(stderr, 
	    "%s: Nchan, length and bits ignored, values from rle file used\n",
		    cmd_name( argv ));
	rle_names( &rle_f_hdr, cmd_name( argv ), rlefname, 0 );
	get_rle_map( &rle_f_hdr, rlefname );
	lbits = rle_f_hdr.cmaplen;
	length = 1 << lbits;
	nchan = rle_f_hdr.ncmap;
	bits = 16;
	/* Just use the rle map */
	amap = allocmap( length, nchan, rle_f_hdr.cmap );
    }
    else
    {
	for ( lbits = 0; length > 1<<lbits; lbits++ )
	    ;			/* Get log2 of length */
	if ( length != 1 << lbits )
	{
	    fprintf( stderr,
		    "%s: Length (%d) rounded up to power of 2 (%d)\n",
		     cmd_name( argv ), length, 1 << lbits );
	    length = 1 << lbits;	/* round length to power of 2 */
	}
	/* Allocate space for the applied map */
	amap = allocmap( nchan, length, NULL );
    }
    range = (1 << bits) - 1;

    /* Compute the requested map */
    if ( lflag )
	linmap( factor, nchan, length, range, amap );
    if ( gflag )
	gammap( gamma, nchan, length, range, amap );
    if ( tflag )
	filemap( tflag, mapfname, nchan, length, amap );
    if ( mflag )
	mfilemap( mfnames, nchan, length, amap );

    /* Open input file and verify header */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), inputfname, "r");
    rle_names( &in_hdr, cmd_name( argv ), inputfname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outputfname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS ||
	  rle_err == RLE_EMPTY;
	  rle_cnt++ )
    {
	if ( rle_err == RLE_EMPTY )
	{
	    if ( rle_cnt == 0 )
		apply = 0;	/* can't apply to non-existant map */
	    else
		break;
	    /* Build in_hdr for non-existant image (all zeros works fine) */
	    bzero( &in_hdr, sizeof in_hdr );
	}

	/* If apply flag was given, check for compatibility of color maps */
	if ( in_hdr.ncmap == 0 )
	    apply = 0;		/* Can't apply to non-existent map */
	if ( apply )
	{
	    char *c;

	    ilbits = in_hdr.cmaplen;
	    ilength = (1 << ilbits);
	    if ( (c = rle_getcom( "color_map_length", &in_hdr )) &&
		 atoi(c) > 0 && atoi(c) < ilength )
		ilength = atoi(c);
	    ichan = in_hdr.ncmap;
	    if ( ! ( nchan == 1 || ichan == 1 ||
		     nchan == ichan ) )
	    {
		fprintf( stderr,
 "%s: Nchan (%d) and input color map (%d channels) are not compatible\n",
			 cmd_name( argv ), nchan, ichan );
		exit(-1);
	    }
	    ochan = (nchan > ichan) ? nchan : ichan;

	    if ( apply == 1 )	/* "before", omap[i] = imap[amap[i]] */
	    {
		olength = length;
		olbits = lbits;
	    }
	    else		/* "after", omap[i] = amap[imap[i]] */
	    {
		olength = ilength;
		olbits = ilbits;
	    }
	
	    /* Get convenient access to the input map */
	    imap = allocmap( in_hdr.ncmap, 1 << in_hdr.cmaplen,
			     in_hdr.cmap );

	    /* Allocate an output map */
	    omap = allocmap( ochan, olength, NULL );

	    /* And do the application */
	    if ( apply == 1 )
		applymap( imap, in_hdr.ncmap, 1 << in_hdr.cmaplen, 16,
			  amap, nchan, length, bits,
			  omap );
	    else
		applymap( amap, nchan, length, bits,
			  imap, in_hdr.ncmap, ilength, 16,
			  omap );
	}
	else
	{
	    /* "Copy" the applied map, and left justify it */
	    omap = allocmap( nchan, length, NULL );
	    bcopy( amap[0], omap[0], nchan * length * sizeof(rle_map) );
	    olength = length;
	    olbits = lbits;
	    ochan = nchan;
	    shiftmap( omap, ochan, olength, bits );
	}

	/* Now, open the output */
	/* start by copying input parameters */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.ncmap = ochan;
	out_hdr.cmaplen = olbits;
	out_hdr.cmap = omap[0];
	if ( olength != 1 << olbits )
	{
	    sprintf( map_comment, "color_map_length=%d", olength );
	    rle_putcom( map_comment, &out_hdr );
	}
	else
	    rle_delcom( "color_map_length", &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(cmd_name( argv ), outputfname, "w");
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	rle_put_setup( &out_hdr );

	/* Copy the rest of the input to the output */
	if ( rle_err != RLE_EMPTY )
	    rle_cp( &in_hdr, &out_hdr );

	/* Free temp storage. */
	free( omap[0] );
	free( omap );
	out_hdr.cmap = 0;	/* Don't try to free it again? */
	if ( apply )
	{
	    free( imap );
	}
	if ( rle_err == RLE_EMPTY )
	{
	    rle_err = RLE_SUCCESS;
	    break;
	}
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), inputfname );

    exit( 0 );
}


/*****************************************************************
 * TAG( allocmap )
 * 
 * Allocate a color map of a given size.
 * Inputs:
 * 	nchan:		Number of channels in the map.
 *	length:		Length of each channel of the map.
 *	cmap:		If non-null, the storage to be used.
 * Outputs:
 * 	Returns a pointer to an array[nchan] of pointers to
 *	array[length] of rle_map.  The rle_map array can also
 *  	be addressed contiguously as return[0][chan*length+i].
 * Assumptions:
 *	If cmap is supplied, it is an array of nchan*length rle_maps.
 * Algorithm:
 *	[None]
 */
rle_map **
allocmap( nchan, length, cmap )
int nchan;
int length;
rle_map * cmap;
{
    rle_map ** map;
    register int i;

    map = (rle_map **)malloc( nchan * sizeof( rle_map * ) );
    RLE_CHECK_ALLOC( "rleldmap", map, "color map" );
    if ( cmap == NULL )
    {
	map[0] = (rle_map *)malloc( nchan * length * sizeof( rle_map ) );
	RLE_CHECK_ALLOC( "rleldmap", map[0], "color map" );
    }
    else
	map[0] = cmap;
    for ( i = 1; i < nchan; i++ )
	map[i] = &map[i-1][length];
    return map;
}

/*****************************************************************
 * TAG( shiftmap )
 * 
 * Shift the entries in the color map to left justify them.
 * Inputs:
 * 	map:		The color map.
 *	nchan:		Number of color channels in the map.
 *	length:		Number of entries in each channel.
 *	bits:		Number of bits in each entry.
 * Outputs:
 * 	map:		Left justified map (modified in place).
 * Assumptions:
 * 	map[0] points to a contiguous array of nchan*length rle_maps
 *  	(as set up by allocmap).
 * Algorithm:
 *	[None]
 */
void
shiftmap( map, nchan, length, bits )
rle_map **map;
int nchan, length, bits;
{
    register rle_map * e;
    register int i;
    
    bits = 16 - bits;
    if ( bits == 0 )
	return;			/* no work! */

    for ( i = nchan * length, e = map[0]; i > 0; i--, e++ )
	*e <<= bits;
}

/*****************************************************************
 * TAG( applymap )
 * 
 * Compose two maps: map[submap].
 * Inputs:
 *	map:		Map being "applied".
 *	nchan:		Number of channels in map.
 *	length:		Length of each channel in map.
 *	bits:		Number of bits used in each entry of map.
 * 	submap:		Map used as subscript.
 *	subchan:	Number of channels in submap.
 *	sublen:		Length of submap.
 *	subbits:	Number of bits used in each entry of submap.
 * Outputs:
 *	omap:		Result map.
 *			Omap has max(nchan,subchan) channels, and
 *			sublen entries.
 * Assumptions:
 * 	Yeah.
 * Algorithm:
 * 	Basically, assign omap[c][i] = map[c][submap[c][i]].  If map has
 *	only one channel, get omap[c][i] = map[0][submap[c][i]], and if
 *	submap has only one channel, get omap[c][i] = map[c][submap[0][i]].
 *	Extra complications include shifting submap by the right number of
 *	bits so that it will index the full range of map, and left-justifying
 *	the output map.
 */
void
applymap( map, nchan, length, bits, submap, subchan, sublen, subbits, omap )
int nchan, length, bits, subchan, sublen, subbits;
rle_map **submap;
rle_map **map;
rle_map **omap;
{
    register rle_map * s;		/* pointer into submap */
    register rle_map * o;		/* pointer into omap */
    rle_map ** mp, ** sp, ** op;	/* pointer to channel of maps */
    register int subshift;
    int c, i, ochan = ((nchan > subchan) ? nchan : subchan);
    int sub;

    /* Figure out how much to shift subscript */
    for ( i = 1; i < length; i *= 2 )
	;			/* Round up to power of 2. */
    if ( (1 << subbits) > i )	/* Too many bits in subscript? */
	for ( subshift = 0; (1 << (subbits + subshift)) > i; subshift-- )
	    ;
    else			/* not enough bits */
	for ( subshift = 0; (1 << (subbits + subshift)) < i; subshift++ )
	    ;

    if ( subshift >= 0 )
    {
	for ( c = ochan, mp = map, sp = submap, op = omap;
	      c > 0;
	      c--, op++, (nchan > 1 ? mp++ : 0), (subchan > 1 ? sp++ : 0) )
	    for ( i = sublen, s = *sp, o = *op; i > 0; i--, s++, o++ )
		if ( (sub = (*s) << subshift) > length )
		    *o = 0;
		else
		    *o = (*mp)[sub];
    }
    else
    {
	subshift = -subshift;
	for ( c = ochan, mp = map, sp = submap, op = omap;
	      c > 0;
	      c--, op++, (nchan > 1 ? mp++ : 0), (subchan > 1 ? sp++ : 0) )
	    for ( i = sublen, s = *sp, o = *op; i > 0; i--, s++, o++ )
		if ( (sub = (*s) >> subshift) > length )
		    *o = 0;
		else
		    *o = (*mp)[sub];
    }

    shiftmap( omap, ochan, sublen, bits );
}


/*****************************************************************
 * TAG( linmap )
 * 
 * Build a linear map.
 * Inputs:
 * 	factor:		Linear factor to multiply map entries by.
 *	nchan:		Number of color channels.
 *	length:		Length of each channel.
 *	range:		Range of elements of the map.
 * Outputs:
 * 	amap:		Result map.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
linmap( factor, nchan, length, range, amap )
double factor;
int nchan, length, range;
rle_map **amap;
{
    register int i;
    double l = length - 1, m;

    for ( i = 0; i < length; i++ )
    {
	m = range * ((double)i / l) * factor;
	if ( factor < 0 )
	{
	    m = range + m;
	    if ( m < 0 )
		m = 0;
	}
	else
	    if ( m > range )
		m = range;
	amap[0][i] = (rle_map)(0.5 + m);
    }

    for ( i = 1; i < nchan; i++ )
	bcopy( (char *)amap[0], (char *)amap[i],
	       length * sizeof(rle_map) );
}

/*****************************************************************
 * TAG( gammap )
 * 
 * Build a gamma compensation map.
 * Inputs:
 * 	gamma:	    Gamma exponent.
 *	nchan:		Number of color channels.
 *	length:		Length of each channel.
 *	range:		Range of elements of the map.
 * Outputs:
 * 	amap:		Result map.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
gammap( gamma, nchan, length, range, amap )
double gamma;
int nchan, length, range;
rle_map **amap;
{
    register int i;
    double l = length - 1;

    gamma = 1.0 / gamma;
    for ( i = 0; i < length; i++ )
	amap[0][i] = (rle_map)(0.5 + range * pow( (double)i / l,
							 gamma ));

    for ( i = 1; i < nchan; i++ )
	bcopy( (char *)amap[0], (char *)amap[i],
	       length * sizeof(rle_map) );
}

/*****************************************************************
 * TAG( filemap )
 * 
 * Read a color map from a file
 * Inputs:
 * 	tflag:	    	Flag for type of file: 1 means all entries for a
 *  	    	    	channel are together (-f), 2 means all entries for
 *  	    	    	a given index are together (-t).
 *  	mapfname:   	Name of file to read map from.
 *	nchan:		Number of color channels.
 *	length:		Length of each channel.
 * Outputs:
 * 	amap:		Result map.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
filemap( tflag, mapfname, nchan, length, amap )
int tflag, nchan, length;
char *mapfname;
rle_map **amap;
{
    FILE * mapfile;
    register int c, i;
    int ent;

    mapfile = rle_open_f( "rleldmap", mapfname, "r" );

    if ( tflag == 1 )		/* channel-major order */
	for ( c = 0; c < nchan; c++ )
	    for ( i = 0; i < length; i++ )
		switch ( fscanf( mapfile, "%d", &ent ) )
		{
		case EOF:	/* EOF */
		    fprintf( stderr,
		"rleldmap: Premature end of file reading map %s at channel %d, entry %d\n",
			   mapfname, c, i );
		    exit(-1);
		    /* NOTREACHED */
		case 1:		/* Got it */
		    amap[c][i] = ent;
		    break;
		case 0:		/* no conversion? */
		    fprintf( stderr,
			    "rleldmap: Bad data in map %s at channel %d, entry %d\n",
			   mapfname, c, i );
		    exit(-1);
		    /* NOTREACHED */
		default:	/* error */
		    fprintf( stderr,
			     "rleldmap: Error reading map %s at channel %d, entry %d\n",
			     mapfname, c, i );
		    exit(-1);
		    /* NOTREACHED */
		}
    else			/* Entry-major order */
	for ( i = 0; i < length; i++ )
	    for ( c = 0; c < nchan; c++ )
		switch ( fscanf( mapfile, "%d", &ent ) )
		{
		case EOF:	/* EOF */
		    fprintf( stderr, 
		"rleldmap: Premature end of file reading map %s at entry %d, channel %d\n",
			   mapfname, i, c );
		    exit(-1);
		    /* NOTREACHED */
		case 1:		/* Got it */
		    amap[c][i] = ent;
		    break;
		case 0:		/* no conversion? */
		    fprintf( stderr,
			    "rleldmap: Bad data in map %s at entry %d, channel %d\n",
			     mapfname, i, c );
		    exit(-1);
		    /* NOTREACHED */
		default:	/* error */
		    fprintf( stderr,
			    "rleldmap: Error reading map %s at entry %d, channel %d: ",
			  mapfname, i, c );
		    perror("");
		    exit(-1);
		    /* NOTREACHED */
		}
    if ( mapfile != stdin )
	fclose( mapfile );
}

/*****************************************************************
 * TAG( mfilemap )
 * 
 * Read a color map from a multiple files
 * Inputs:
 *  	mfnames:   	Name of files to read map from.  Each file contains
 *  	    	    	the entries for a single channel of the map.
 *	nchan:		Number of color channels (thus number of files).
 *	length:		Length of each channel.
 * Outputs:
 * 	amap:		Result map.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
mfilemap( mfnames, nchan, length, amap )
char **mfnames;
int nchan, length;
rle_map **amap;
{
    FILE * mapfile;
    register int c, i;
    int ent;

    for ( c = 0; c < nchan; c++, mfnames++ )
    {
	mapfile = rle_open_f( "rleldmap", *mfnames, "r" );

	for ( i = 0; i < length; i++ )
	    switch ( fscanf( mapfile, "%d", &ent ) )
	    {
	    case EOF:		/* EOF */
		fprintf( stderr,
		    "rleldmap: Premature end of file reading map %s at channel %d, entry %d\n",
		    *mfnames, c, i );
		exit(-1);
		/* NOTREACHED */
	    case 1:		/* Got it */
		amap[c][i] = ent;
		break;
	    case 0:		/* no conversion? */
		fprintf( stderr,
			"rleldmap: Bad data in map %s at channel %d, entry %d\n",
			 *mfnames, c, i );
		exit(-1);
		/* NOTREACHED */
	    default:		/* error */
		fprintf( stderr,
			 "rleldmap: Error reading map %s at channel %d, entry %d: ",
			 *mfnames, c, i );
		perror("");
		exit(-1);
		/* NOTREACHED */
	    }
	if ( mapfile != stdin )
	    fclose( mapfile );
    }
}

/*****************************************************************
 * TAG( get_rle_map )
 * 
 * Read the map from an RLE file.
 * Inputs:
 *  	fname:	    Name of file to read map from.
 * Outputs:
 * 	the_hdr:    RLE header struct to fill in.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
get_rle_map( the_hdr, fname )
rle_hdr *the_hdr;
char *fname;
{
    FILE * infile;

    infile = rle_open_f( "rleldmap", fname, "r" );

    the_hdr->rle_file = infile;
    if ( rle_get_setup( the_hdr ) < 0 )
    {
	fprintf( stderr,
		 "rleldmap: Can't read setup information from %s\n", fname );
	exit(-1);
    }

    if ( the_hdr->ncmap == 0 )
    {
	fprintf( stderr, "rleldmap: No color map in %s\n", fname );
	exit(-1);
    }

    if ( infile != stdin )
	fclose( infile );
}
