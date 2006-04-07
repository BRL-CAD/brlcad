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
 * rledither.c - Do Floyd Steinberg dithering on an image.  Edge enhancement
 *    can be tuned with command line argument.
 * 
 * Author:	Rod Bogart 
 * 		University of Michigan
 * Date:	Fri Dec 1 1989
 * Copyright (c) 1989, The Regents of the University of Michigan
 * 
 *   -e edge_factor     Zero means no edge enhancement, 1.0 looks pretty good.
 *   -l nchan length    Number of channels in the colormap, and num entries.
 *			Defaults to 3 channels of 256 entries each.
 *   -{tf} mapfile      -t is a colormap listed as  R G B  R G B  R G B ...
 *                      -f is listed as R R R... G G G... B B B...
 *   -o outfile         Output RLE file (stdout default)
 *   infile             Input RLE file (stdin default)
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

#define MALLOC_ERR RLE_CHECK_ALLOC( progname, 0, 0 )

rle_map **allocmap();
void filemap(), copymap(), shiftmap(), copy_into_shortrow(), edge_enhance();
void fsdither(), find_closest();

char *progname;

int
main(argc, argv)
int	argc;
char	*argv[];
{
    int i, temp, y;
    int oflag = 0;
    int blend, blend_divisor = 256;
    float edge_factor = 0.0;
    int top, middle, bottom, edgetop, edgebottom;
    rle_hdr 	in_hdr, out_hdr;
    rle_pixel **cleanrows[3], **edgerows[2], **outrow;
    short *shortrows[2];
    char *infname = NULL, *outfname = NULL, *mapfname = NULL;
    FILE *outfile = stdout;
    int nchan = 3, length = 256, tflag = 0;
    rle_map **amap, **map;
    int rle_cnt, rle_err;
    char cmap_comment[80];

    progname = cmd_name( argv );
    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );


    if ( scanargs( argc, argv,
		  "% e%-edge_factor!f l%-nchan!dlength!d \n\
\ttf!-mapfile!s o%-outfile!s infile%s",
		  &i, &edge_factor, &i, &nchan, &length,
		  &tflag, &mapfname,
		  &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    in_hdr.rle_file = rle_open_f( progname, infname, "r" );
    rle_names( &in_hdr, progname, infname, 0 );
    rle_names( &out_hdr, progname, outfname, 0 );

    /* Allocate space for the applied map */
    amap = allocmap( nchan, 256, NULL );
    map = allocmap( nchan, 256, NULL );
    filemap( tflag, mapfname, nchan, length, amap );
    copymap( amap, nchan, 256, map );
    shiftmap( amap, nchan, 256, 8 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/* alpha on dithered stuff is too weird... */
	out_hdr.alpha = 0;
	out_hdr.ncolors = 1;	/* Just one chan, the cmap index */
	for (i = 1; i < in_hdr.ncolors; i++)
	    RLE_CLR_BIT( out_hdr, i );	/* Kill extra output channels */
	out_hdr.ncmap = nchan;
	out_hdr.cmaplen = 8;	/* == 256 entries */
	out_hdr.cmap = (rle_map *) amap[0];
	sprintf( cmap_comment, "color_map_length=%d", length );
	rle_putcom( cmap_comment, &out_hdr );
	rle_put_setup( &out_hdr );

	in_hdr.xmax -= in_hdr.xmin;
	in_hdr.xmin = 0;

	/* Oink. */
	for (i = 0; i < 3; i++)
	    if (rle_row_alloc( &in_hdr, &cleanrows[i] ) < 0)
		MALLOC_ERR;

	if (rle_row_alloc( &in_hdr, &edgerows[0] ) < 0)
	    MALLOC_ERR;

	if (rle_row_alloc( &in_hdr, &edgerows[1] ) < 0)
	    MALLOC_ERR;

	if (rle_row_alloc( &out_hdr, &outrow ) < 0)
	    MALLOC_ERR;

	if ( (shortrows[0] = (short *)malloc(in_hdr.ncolors
					     * (in_hdr.xmax+1)
					     * sizeof(short) )) == 0 )
	    MALLOC_ERR;
	if ( (shortrows[1] = (short *)malloc(in_hdr.ncolors
					     * (in_hdr.xmax+1)
					     * sizeof(short) )) == 0 )
	    MALLOC_ERR;

	if (edge_factor < 0.0)
	    blend = 0;
	else
	    blend = (int) (edge_factor * (float) blend_divisor);

	rle_getrow(&in_hdr, cleanrows[2]);
	rle_getrow(&in_hdr, cleanrows[1]);
	bottom = 0;
	middle = 1;
	top = 2;
	edgetop = 1;
	edgebottom = 0;
	/* just copy the top row, it gets no edge enhancement */
	copy_into_shortrow( &in_hdr, 
			    cleanrows[top], shortrows[edgetop] );

	for (y = in_hdr.ymin+2; y <= in_hdr.ymax; y++)
	{
	    rle_getrow(&in_hdr, cleanrows[bottom]);
	    edge_enhance(&in_hdr, 
			 cleanrows[bottom], cleanrows[middle], cleanrows[top],
			 edgerows[edgebottom],
			 blend, blend_divisor);
	    copy_into_shortrow( &in_hdr, 
				edgerows[edgebottom], shortrows[edgebottom] );
	    fsdither(&in_hdr, map, length,
		     shortrows[edgebottom], shortrows[edgetop], outrow);
	    rle_putrow( outrow, in_hdr.xmax+1, &out_hdr);
	    /* swap the row pointers */
	    temp = top;	top = middle;	middle = bottom;	bottom = temp;
	    temp = edgetop;	edgetop = edgebottom;	edgebottom = temp;
	}
	fsdither(&in_hdr, map, length,
		 shortrows[edgebottom], shortrows[edgetop], outrow);
	rle_putrow( outrow, in_hdr.xmax+1, &out_hdr);
	/* just copy the middle row, it gets no edge enhancement */
	copy_into_shortrow( &in_hdr, 
			    cleanrows[middle], shortrows[edgetop] );
	fsdither(&in_hdr, map, length, NULL, shortrows[edgetop], outrow);
	rle_putrow( outrow, in_hdr.xmax+1, &out_hdr);

	rle_puteof( &out_hdr );

	/* Release storage. */
	for (i = 0; i < 3; i++)
	    rle_row_free( &in_hdr, cleanrows[i] );
	rle_row_free( &in_hdr, edgerows[0] );
	rle_row_free( &in_hdr, edgerows[1] );
	rle_row_free( &out_hdr, outrow );
	free( shortrows[0] );
	free( shortrows[1] );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}

void
copy_into_shortrow( in_hdr, rlerow, shortrow )
rle_hdr *in_hdr;
rle_pixel **rlerow;
short *shortrow;
{
    int chan,i;

    for (i=0; i <= in_hdr->xmax; i++)
    {
	for (chan = 0; chan < in_hdr->ncolors; chan++)
	{
	    *shortrow++ = rlerow[chan][i];
	}
    }
}

void
edge_enhance(in_hdr, row_b, row_m, row_t, edge_row, blend, blend_divisor)
rle_hdr *in_hdr;
rle_pixel **row_b, **row_m, **row_t, **edge_row;
int blend_divisor, blend;
{
    int chan, i;
    int total, diff, avg, result;
    for (chan = 0; chan < in_hdr->ncolors; chan++)
    {
	/* i = 0 case, ignore left column of pixels */
	total = row_b[chan][0] + row_b[chan][1] + 
	                         row_m[chan][1] + 
		row_t[chan][0] + row_t[chan][1];
	avg = total / 5;
	diff = avg - row_m[chan][0];
	result = row_m[chan][0] - (diff * blend / blend_divisor);
	if (result < 0)
	    edge_row[chan][0] = 0;
	else if (result > 255)
	    edge_row[chan][0] = 255;
	else
	    edge_row[chan][0] = result;
	
	/* all the nice cases */
	for (i=1; i < in_hdr->xmax; i++)
	{
	    total = row_b[chan][i-1] + row_b[chan][i] + row_b[chan][i+1] + 
		    row_m[chan][i-1]                  + row_m[chan][i+1] + 
		    row_t[chan][i-1] + row_t[chan][i] + row_t[chan][i+1];
	    avg = total >> 3;  /* divide by 8 */
	    diff = avg - row_m[chan][i];
	    result = row_m[chan][i] - (diff * blend / blend_divisor);
	    if (result < 0)
		edge_row[chan][i] = 0;
	    else if (result > 255)
		edge_row[chan][i] = 255;
	    else
		edge_row[chan][i] = result;
	}
	/* i = in_hdr.xmax case, ignore right column of pixels */
	i = in_hdr->xmax;
	total = row_b[chan][i-1] + row_b[chan][i] +  
	        row_m[chan][i-1]                  + 
		row_t[chan][i-1] + row_t[chan][i];
	avg = total / 5;
	diff = avg - row_m[chan][i];
	result = row_m[chan][i] - (diff * blend / blend_divisor);
	if (result < 0)
	    edge_row[chan][i] = 0;
	else if (result > 255)
	    edge_row[chan][i] = 255;
	else
	    edge_row[chan][i] = result;
    }
}

void
fsdither(in_hdr, map, maplen, row_bottom, row_top, outrow)
rle_hdr *in_hdr;
rle_map **map;
int maplen;
short *row_bottom, *row_top;
rle_pixel **outrow;
{
    register rle_pixel *optr;
    register int j;
    register short *thisptr, *nextptr = NULL;
    int chan;
    static int numchan = 0;
    int	lastline = 0, lastpixel ;
    static int *cval=0;
    static rle_pixel *pixel=0;

    if ( numchan != in_hdr->ncolors )
	if ( cval )
	{
	    free( cval );
	    free( pixel );
	}

    numchan = in_hdr->ncolors;
    if (!cval) {
	if ((cval = (int *) malloc( numchan * sizeof(int) )) == 0)
	    MALLOC_ERR;
	if ((pixel = (rle_pixel *) malloc( numchan * sizeof(rle_pixel) )) == 0)
	    MALLOC_ERR;
    }
    optr = outrow[RLE_RED];

    thisptr = row_top;
    if (row_bottom) 
	nextptr = row_bottom;
    else
	lastline = 1;

    for(j=0; j <= in_hdr->xmax ; j++)
    {
	int cmap_index=0;

	lastpixel = (j == in_hdr->xmax) ;

	for (chan = 0; chan < numchan; chan++)
	{
	    cval[chan] = *thisptr++ ;

	    /* Current channel value has been accumulating error, it could be
	     * out of range.
	     */
	    if( cval[chan] < 0 ) cval[chan] = 0 ;
	    else if( cval[chan] > 255 ) cval[chan] = 255 ;

	    pixel[chan] = cval[chan];
	}

	/* find closest color */
	find_closest(map, numchan, maplen, pixel, &cmap_index);
	*optr++ = cmap_index;
	    
	/* thisptr is now looking at pixel to the right of current pixel
	 * nextptr is looking at pixel below current pixel
	 * So, increment thisptr as stuff gets stored.  nextptr gets moved
	 * by one, and indexing is done +/- numchan.
	 */
	for (chan = 0; chan < numchan; chan++)
	{
	    cval[chan] -= map[chan][cmap_index];

	    if( !lastpixel )
	    {
		thisptr[chan] += cval[chan] * 7 / 16 ;
	    }
	    if( !lastline )
	    {
		if( j != 0 )
		{
		    nextptr[-numchan] += cval[chan] * 3 / 16 ;
		}
		nextptr[0] += cval[chan] * 5 / 16 ;
		if( !lastpixel )
		{
		    nextptr[numchan] += cval[chan] / 16 ;
		}
		nextptr ++;
	    }
	}
    }
}

/*****************************************************************
 * TAG( filemap )
 * 
 * NOTE: this code is stolen from rleldmap.
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

    if ( ( mapfile = fopen( mapfname, "r" ) ) == NULL )
    {
	fprintf( stderr, "%s: Couldn't open map file %s: ",
		 progname, mapfname );
	perror("");
	exit(-1);
    }

    if ( tflag == 1 )		/* channel-major order */
	for ( c = 0; c < nchan; c++ )
	    for ( i = 0; i < length; i++ )
		switch ( fscanf( mapfile, "%d", &ent ) )
		{
		case EOF:	/* EOF */
		    fprintf( stderr,
	"%s: Premature end of file reading map %s at channel %d, entry %d\n",
			     progname, mapfname, c, i );
		    exit(-1);
		    /* NOTREACHED */
		case 1:		/* Got it */
		    amap[c][i] = ent;
		    break;
		case 0:		/* no conversion? */
		    fprintf( stderr,
			    "%s: Bad data in map %s at channel %d, entry %d\n",
			     progname, mapfname, c, i );
		    exit(-1);
		    /* NOTREACHED */
		default:	/* error */
		    fprintf( stderr,
		     "%s: Error reading map %s at channel %d, entry %d\n",
			     progname, mapfname, c, i );
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
	"%s: Premature end of file reading map %s at entry %d, channel %d\n",
			     progname, mapfname, i, c );
		    exit(-1);
		    /* NOTREACHED */
		case 1:		/* Got it */
		    amap[c][i] = ent;
		    break;
		case 0:		/* no conversion? */
		    fprintf( stderr,
			    "%s: Bad data in map %s at entry %d, channel %d\n",
			     progname, mapfname, i, c );
		    exit(-1);
		    /* NOTREACHED */
		default:	/* error */
		    fprintf( stderr,
		    "%s: Error reading map %s at entry %d, channel %d: ",
			     progname, mapfname, i, c );
		    perror("");
		    exit(-1);
		    /* NOTREACHED */
		}
    fclose( mapfile );
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
    RLE_CHECK_ALLOC( progname, map, 0 );
    if ( cmap == NULL )
    {
	map[0] = (rle_map *)malloc( nchan * length * sizeof( rle_map ) );
	RLE_CHECK_ALLOC( progname, map[0], 0 );
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
int nchan, length, bits;
rle_map **map;
{
    register rle_map * e;
    register int i;
    
    bits = 16 - bits;
    if ( bits == 0 )
	return;			/* no work! */

    for ( i = nchan * length, e = map[0]; i > 0; i--, e++ )
	*e <<= bits;
}

void
copymap( inmap, nchan, length, outmap )
rle_map **inmap, **outmap;
int nchan, length;
{
    register rle_map *ie, *oe;
    register int i;
    
    for ( i = nchan * length, ie = inmap[0], oe = outmap[0]; 
	  i > 0; i--, ie++, oe++ )
	*oe = *ie;
}

void
find_closest(map, nchan, maplen, pixel, index)
rle_map ** map;
int nchan, maplen;
rle_pixel *pixel;
int *index;
{
    int i, closest, chan;
    long bestdist, dist;

    closest = -1;
    bestdist = 256*256*nchan + 1;
    for (i=0; i < maplen; i++)
    {
	dist = 0L;
	for (chan = 0; chan < nchan; chan++)
	{
	    int tmp = map[chan][i] - pixel[chan];
	    dist += tmp * tmp;
	}
	if (dist < bestdist)
	{
	    closest = i;
	    bestdist = dist;
	}
    }
    *index = closest;
}
