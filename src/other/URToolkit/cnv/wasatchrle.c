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
 * wasatchrle.c - Convert Wasatch Paintbox files into RLE
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Dec 15 1987
 * Copyright (c) 1987, University of Utah
 *
 * Thanks to Mike Ware of Wasatch for providing the format information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "rle.h"

extern int errno;

/* "short" in our world is 16 bits.  Beware of swyte-bopping. */

struct was_head {
    short x1, y1;		/* First corner of viewport */
    short x2, y2;		/* Second corner of viewport */
    short top, bottom;		/* "Reserved" */
    short unused[19];
    char  reserved[14];
} was_head;

short	rlemap[3][256];		/* RLE version of the color map */
rle_pixel rgb[3];		/* For reading in the color map */

struct was_op {			/* A wasatch "opcode" */
    rle_pixel count, data;
} was_op;

int
main(argc, argv)
int argc;
char **argv;
{
    FILE       *open_with_ext();
    int 	oflag = 0;
    char       *out_name = NULL;
    rle_hdr	hdr;
    char       *was_basename;
    FILE       *lut_file, *rlc_file;	/* Wasatch input files */
    rle_pixel **row;
    rle_pixel  *pxlptr;
    int 	i, xpos;

    if (scanargs( argc, argv, "% o%-outfile!s basename!s",
		  &oflag, &out_name, &was_basename ) == 0)
	exit(-1);

    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), out_name, 0 );

    lut_file = open_with_ext( was_basename, ".lut" );
    rlc_file = open_with_ext( was_basename, ".rlc" );

    hdr.rle_file = rle_open_f( hdr.cmd, out_name, "w" );

    fread( &was_head, sizeof( was_head ), 1, rlc_file );

    /*
     * The spec provided by Wasatch is not clear on the meaning or any
     * intended order of the "viewport corners".  Also, look out for
     * BYTE SWAPPING.
     */
    hdr.xmin = was_head.x1;
    hdr.ymin = was_head.y1;
    hdr.xmax = was_head.x2;
    hdr.ymax = was_head.y2;

    hdr.ncolors = 1;
    hdr.alpha = 0;	/* What a shame... */
    for (i = 1; i < hdr.ncolors; i++)
	RLE_CLR_BIT( hdr, i );
    hdr.ncmap = 3;
    hdr.cmaplen = 8; /* == 256 entries */
    hdr.cmap = (rle_map *) rlemap;

    /* Grab the map. */

    for (i = 0; i < 256; i++)
    {
	fread( rgb, sizeof( rgb ), 1, lut_file );
	rlemap[RLE_RED][i] = ( (int)rgb[RLE_RED] ) << 8;
	rlemap[RLE_GREEN][i] = ( (int)rgb[RLE_GREEN] ) << 8;
	rlemap[RLE_BLUE][i] = ( (int)rgb[RLE_BLUE] ) << 8;
    }

    rle_addhist( argv, (rle_hdr *)NULL, &hdr );
    rle_put_setup( &hdr );
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;

    if (rle_row_alloc( &hdr, &row ))
    {
	fprintf( stderr, "wasacthrle: malloc failed\n" );
	exit(-2);
    }

    xpos = 0;
    pxlptr = row[0];
    was_op.count = 0;		/* Haven't read anything yet. */

    do {
	while (was_op.count--)
	{
	    if (xpos > hdr.xmax) /* Flush scanline */
	    {
		rle_putrow( row, (hdr.xmax + 1), &hdr );
		xpos = 0;
		pxlptr = row[0];
	    }
	    *pxlptr++ = was_op.data;
	    xpos++;
	}
	fread( &was_op, sizeof( was_op ), 1, rlc_file );
    } while( was_op.count );

    if (xpos)
	rle_putrow( row, (hdr.xmax + 1), &hdr );
    rle_puteof( &hdr );
    exit(0);
}

/*****************************************************************
 * TAG( open_with_ext )
 * 
 * Open a file "basename" with extension (suffix) "ext".  Tries "ext"
 * with both lower and upper case (should be called with "ext" in
 * lower case).  If both opens fail, it exits the program.
 */

FILE *
open_with_ext( basename, ext )
char * basename;
char * ext;
{
    char file_name[255];
    char ext_name[10];
    char * cptr;
    FILE * f;

    strcpy( ext_name, ext );
    strcpy( file_name, basename );
    strcat( file_name, ext_name );

    if (! (f = fopen( file_name, "r" )))
    {
	if (errno == ENOENT)	/* Not found, try upper case */
	{
	    strcpy( file_name, basename );
	    cptr = ext_name;
	    for (; *cptr; cptr++ )
		*cptr = toupper( *cptr );
	    strcat( file_name, ext_name );
	    f = fopen( file_name, "r" );
	}
    }

    if (! f)			/* fopen failed */
    {
	perror( file_name );
	exit( -1 );
    }
    return( f );
}
    
