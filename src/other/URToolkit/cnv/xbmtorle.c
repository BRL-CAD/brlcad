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
 * xbmtorle.c - Convert X Bitmap files to RLE.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Mon Feb 25 1991
 * Copyright (c) 1991, University of Michigan
 */
static char rcsid[] = "$Header$";
/*
xbmtorle()				Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

#define MAX_LINE 500

static char *MY_NAME;

#ifdef USE_PROTOTYPES
static int ReadBitmapFile( FILE *, int *, int *, int *, char ** );
#else
static int ReadBitmapFile();
#endif

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 * 	xbmtorle [-b bg_color_comps ...] [-f fg_color_comps ...]
 * 			[-o outfile] [infile.xbm]
 * Inputs:
 * 	-b:		Specify background color (default 0).
 * 	-f:		Specify foreground color (default 255).
 * 			By default, the output image has a single
 * 			channel.  If the background color or
 * 			foreground color are specified with more
 * 			components, then that many channels will be
 * 			output.  If both are specified, the number of
 * 			components should match.
 * 	infile.xbm:	The input X bitmap.  Default stdin.
 * Outputs:
 * 	-o outfile:	Specify output RLE file name.  Default stdout.
 * Assumptions:
 * 	Undoubtedly.
 * Algorithm:
 * 	Reads the bitmap, then builds an RLE image with one pixel per
 * 	bit in the input.
 */
void
main( argc, argv )
int argc;
char **argv;
{
    FILE *xbm_file;
    int 	bflag = 0,
    		fflag = 0,
    		oflag = 0;
    int		zero = 0,
    		two_five_five = 255,
    	       *bg = &zero,
	       *fg = &two_five_five,
    		nbg = 1,
		nfg = 1;
    int		width, height, bytes;
    char       *data;
    char       *infname = NULL,
	       *ofname = NULL;
    register    int x, c, y;
    rle_pixel **scans;
    rle_hdr	out_hdr;

    if ( scanargs( argc, argv, 
   "% b%-bg_color_comps%*d f%-fg_color_comps%*d o%-outfile!s infile.xbm%s\n(\
\tConvert an X bitmap file (NOT X window dump) to URT format.\n\
\t-b/-f\tSpecify color to use for background/foreground bits.\n\
\t\tFollowed by one or more color components.  Number of\n\
\t\tcomponents must be the same for both -b and -f.\n\
\t\tDefaults are 0 and 255 respectively.)",
		   &bflag, &nbg, &bg, &fflag, &nfg, &fg,
		   &oflag, &ofname, &infname ) == 0 )
	exit( 1 );

    MY_NAME = cmd_name( argv );

    /* Check fg/bg colors if given. */
    if ( bflag && fflag && nbg != nfg )
    {
	fprintf( stderr,
 "%s: Warning: Number of background color components (%d) not the same as\n\
	the foreground (%d), using %d\n",
		 MY_NAME, nbg, nfg, nbg < nfg ? nbg : nfg );
	if ( nbg > nfg )
	    nbg = nfg;
	else
	    nfg = nbg;
    }

    /* If only specified, fix the other. */
    if ( bflag & !fflag && nbg > 1 )
    {
	fg = (int *)malloc( nbg * sizeof(int) );
	for ( nfg = 0; nfg < nbg; nfg++ )
	    fg[nfg] = 255;
    }
    if ( fflag & !bflag && nfg > 1 )
    {
	bg = (int *)malloc( nfg * sizeof(int) );
	for ( nbg = 0; nbg < nfg; nbg++ )
	    bg[nbg] = 0;
    }

    /* Read the Bitmap now. */
    xbm_file = rle_open_f( MY_NAME, infname, "r" );
    if ( ReadBitmapFile( xbm_file, &width, &height, &bytes, &data ) == 0 )
	exit( 1 );

    /* Create the RLE file. */
    out_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &out_hdr, MY_NAME, ofname, 0 );
    out_hdr.ncolors = nbg;
    out_hdr.bg_color = bg;
    out_hdr.xmin = out_hdr.ymin = 0;
    out_hdr.xmax = width - 1;
    out_hdr.ymax = height - 1;
    out_hdr.ncmap = out_hdr.cmaplen = 0;
    out_hdr.comments = 0;
    out_hdr.rle_file = rle_open_f( MY_NAME, ofname, "w" );
    rle_addhist( argv, NULL, &out_hdr );

    rle_row_alloc( &out_hdr, &scans );

    rle_put_setup( &out_hdr );

    for ( y = 0; y < height; y++ )
    {
	int charcount = 0;
	char mask = 1;
	register char *datap;

	datap = data + (height - y - 1) * bytes;
	for ( x = 0; x < width; x++ )
	{
	    if ( charcount >= 8 )
	    {
		datap++;
		charcount = 0;
		mask = 1;
	    }
	    for ( c = 0; c < nbg; c++ )
		scans[c][x] = ( *datap & mask ) ? fg[c] : bg[c];
	    charcount++;
	    mask = mask << 1;
	}
	rle_putrow( scans, width, &out_hdr );
    }
    rle_puteof( &out_hdr );
}


static int
ReadBitmapFile( stream, widthP, heightP, bytesP, dataP )
FILE *stream;
int *widthP, *heightP, *bytesP;
char **dataP;
{
    char line[MAX_LINE], name_and_type[MAX_LINE];
    char *ptr, *t;
    int version10, raster_length, v;
    register int bytes, bytes_per_line, padding;
    register int c1, c2, value1, value2;
    static int hex_table[256];

    *widthP = *heightP = -1;

    for ( ; ; )
    {
	if ( fgets( line, MAX_LINE, stream ) == NULL )
	{
	    fprintf( stderr, "%s: bitmap premature EOF\n", MY_NAME);
	    return 0;
	}
	if ( strlen( line ) == MAX_LINE - 1 )
	{
	    fprintf( stderr, "%s: bitmap line too long\n", MY_NAME );
	    return 0;
	}

	if ( sscanf( line, "#define %s %d", name_and_type, &v ) == 2 )
	{
	    if ( ! (t = rindex( name_and_type, '_' )) )
		t = name_and_type;
	    else
		t++;
	    if ( ! strcmp( "width", t ) )
		*widthP = v;
	    if ( ! strcmp( "height", t ) )
		*heightP = v;
	    continue;
	}
	
	if ( sscanf( line, "static short %s = {", name_and_type ) == 1 )
	{
	    version10 = 1;
	    break;
	}
	else if ( sscanf( line, "static char %s = {", name_and_type ) == 1 )
	{
	    version10 = 0;
	    break;
	}
	else
	    continue;
    }
 
    if ( *widthP == -1 )
    {
	fprintf( stderr, "%s: bitmap width invalid\n", MY_NAME );
	return 0;
    }
    if ( *heightP == -1 )
    {
	fprintf( stderr, "%s: bitmap height invalid\n", MY_NAME );
	return 0;
    }

    padding = 0;
    if ( ((*widthP % 16) >= 1) && ((*widthP % 16) <= 8) && version10 )
	padding = 1;

    bytes_per_line = (*widthP+7)/8 + padding;
    *bytesP = bytes_per_line;
    
    raster_length =  bytes_per_line * *heightP;
    *dataP = (char *) malloc( raster_length );
    if ( *dataP == (char *) 0 )
    {
	fprintf( stderr, "%s: out of memory\n", MY_NAME );
	return 0;
    }

    /* Initialize hex_table. */
    for ( c1 = 0; c1 < 256; c1++ )
	hex_table[c1] = 256;
    hex_table['0'] = 0;
    hex_table['1'] = 1;
    hex_table['2'] = 2;
    hex_table['3'] = 3;
    hex_table['4'] = 4;
    hex_table['5'] = 5;
    hex_table['6'] = 6;
    hex_table['7'] = 7;
    hex_table['8'] = 8;
    hex_table['9'] = 9;
    hex_table['A'] = 10;
    hex_table['B'] = 11;
    hex_table['C'] = 12;
    hex_table['D'] = 13;
    hex_table['E'] = 14;
    hex_table['F'] = 15;
    hex_table['a'] = 10;
    hex_table['b'] = 11;
    hex_table['c'] = 12;
    hex_table['d'] = 13;
    hex_table['e'] = 14;
    hex_table['f'] = 15;

    if ( version10 )
	for ( bytes = 0, ptr = *dataP; bytes < raster_length; bytes += 2 )
	{
	    while ( ( c1 = getc( stream ) ) != 'x' )
		if ( c1 == EOF )
		    goto premature_eof;

	    c1 = getc( stream );
	    c2 = getc( stream );
	    if ( c1 == EOF || c2 == EOF )
		goto premature_eof;
	    value1 = ( hex_table[c1] << 4 ) + hex_table[c2];
	    if ( value1 >= 256 )
		fprintf( stderr, "%s: bitmap syntax error\n", MY_NAME );
	    c1 = getc( stream );
	    c2 = getc( stream );
	    if ( c1 == EOF || c2 == EOF )
		goto premature_eof;
	    value2 = ( hex_table[c1] << 4 ) + hex_table[c2];
	    if ( value2 >= 256 )
		fprintf( stderr, "%s: bitmap syntax error\n", MY_NAME );
	    *ptr++ = value2;
	    if ( ( ! padding ) || ( ( bytes + 2 ) % bytes_per_line ) )
		*ptr++ = value1;
	}
    else
	for ( bytes = 0, ptr = *dataP; bytes < raster_length; bytes++ )
	{
	    while ( ( c1 = getc( stream ) ) != 'x' )
		if ( c1 == EOF )
		    goto premature_eof;
	    c1 = getc( stream );
	    c2 = getc( stream );
	    if ( c1 == EOF || c2 == EOF )
		goto premature_eof;
	    value1 = ( hex_table[c1] << 4 ) + hex_table[c2];
	    if ( value1 >= 256 )
		fprintf( stderr, "%s: bitmap syntax error\n", MY_NAME );
	    *ptr++ = value1;
	}

    return 1;

 premature_eof:
    fprintf( stderr, "%s: bitmap premature EOF\n", MY_NAME );
    return 1;
}
