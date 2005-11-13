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
 * cubitorle.c - Convert cubicomp image to an RLE file.
 * 
 * Author:	Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Nov  6 1986
 * Copyright (c) 1986 Rod Bogart
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

void read_cubi_hdr(), read_cubi_row(), read_cubi_chan(), bit_read();

void
main(argc, argv)
int	argc;
char	*argv[];
{
    FILE *cubifiles[4];
    int i, j, oflag=0;
    rle_pixel ** rows;
    int xlen;
    int cubi_xlen, cubi_ylen;
    char *infname = NULL, *outfname = NULL;
    char filename[256];
    rle_hdr	hdr;

    if ( scanargs( argc, argv,
		  "% o%-outfile!s inprefix!s\n(\
\tConvert Cubicomp image to URT image.  Input image is in 3 files,\n\
\tinprefix.r, inprefix.g, inprefix.b.)",
		  &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), outfname, 0 );
    hdr.rle_file = rle_open_f(hdr.cmd, outfname, "w");

    for ( i = 0; i < 3; i++ )
    {
	sprintf( filename, "%s.%c8", infname, "rgb"[i] );
	cubifiles[i] = rle_open_f( hdr.cmd, filename, "r" );
    }

    read_cubi_hdr(cubifiles, &cubi_xlen, &cubi_ylen);

    rle_dflt_hdr.alpha = 0;
    rle_dflt_hdr.ncolors = 3;
    rle_dflt_hdr.xmin = 0;
    rle_dflt_hdr.xmax = cubi_xlen - 1;
    rle_dflt_hdr.ymin = 0;
    rle_dflt_hdr.ymax = cubi_ylen - 1;
    xlen = rle_dflt_hdr.xmax - rle_dflt_hdr.xmin + 1;

    rle_addhist( argv, (rle_hdr *)NULL, &rle_dflt_hdr );
    rle_put_setup( &rle_dflt_hdr );

    if (rle_row_alloc( &rle_dflt_hdr, &rows ))
    {
	fprintf( stderr, "%s: malloc failed\n", hdr.cmd );
	exit( -2 );
    }

    for (j=rle_dflt_hdr.ymin; j <= rle_dflt_hdr.ymax; j++)
    {
	read_cubi_row( cubifiles, rows );
	rle_putrow( rows, xlen, &rle_dflt_hdr);
    }

}

void
read_cubi_hdr(cubifiles, xlen, ylen)
FILE *cubifiles[];
short *xlen, *ylen;
{
    char junk[128];
    short xmin, ymin, xmax, ymax;

    fread(junk, sizeof(char), 12, cubifiles[0]);
    fread(xlen, sizeof(short), 1, cubifiles[0]);
    fread(ylen, sizeof(short), 1, cubifiles[0]);
    fread(&xmin, sizeof(short), 1, cubifiles[0]);
    fread(&ymin, sizeof(short), 1, cubifiles[0]);
    fread(&xmax, sizeof(short), 1, cubifiles[0]);
    fread(&ymax, sizeof(short), 1, cubifiles[0]);
    fread(junk, sizeof(char), 104, cubifiles[0]);

    fread(junk, sizeof(char), 128, cubifiles[1]);
    fread(junk, sizeof(char), 128, cubifiles[2]);
}

void
read_cubi_row(cubifiles, rows)
FILE *cubifiles[];
rle_pixel ** rows;
{
    read_cubi_chan(cubifiles[0],rows,0);
    read_cubi_chan(cubifiles[1],rows,1);
    read_cubi_chan(cubifiles[2],rows,2);
}

void
read_cubi_chan(infile, rows, chan)
FILE * infile;
rle_pixel **rows;
int chan;
{
    static char headchar[3];
    static int scanfull[3] = {-1, -1, -1};
    int xpos = 0, bit;

    while (xpos < 512)
    {
	if (scanfull[chan] == -1)
	    headchar[chan] = fgetc(infile);

	for (bit = 0; bit < 8; bit++)
	    if (scanfull[chan] <= bit)
	    {
		bit_read(infile, headchar[chan], bit, rows, chan, &xpos);
		if (xpos >= 512) 
		{
		    scanfull[chan] = bit + 1;
		    break;
		}
	    }
	if (bit >= 7) scanfull[chan] = -1;
    }
}

void
bit_read(infile, headchar, bit, rows, chan, xpos)
FILE * infile;
char headchar;
int bit, chan, *xpos;
rle_pixel **rows;
{
    unsigned char runlength, rundata, bytedata;
    int i;

    if (headchar & (1 << bit))
    {
	/* bit set, run data */
	rundata = fgetc(infile);
	runlength = fgetc(infile);
	for (i=(*xpos); i < runlength+(*xpos); i++)
	    rows[chan][i] = rundata;
	*xpos += (int) runlength;
    }
    else
    {
	/* bit not set, byte data */
	bytedata = fgetc(infile);
	rows[chan][*xpos] = bytedata;
	(*xpos)++;
    }
}
