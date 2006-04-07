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
 * rletops.c - Convert RLE to postscript file.
 * 
 * Author:	Rod Bogart & John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Nov  4 1986
 * Copyright (c) 1986 Rod Bogart
 *
 * Modified by:	Gregg Townsend
 *		Department of Computer Science
 *		University of Arizona
 * Date:	June 22, 1990
 * Changes:	50% speedup using putc(), add -C option, translate to page ctr
 *		Fix comments to conform to EPS Version 2.0  (Al Clark)
 * 
 * Based on "tobw.c" by Spencer Thomas, and 
 * "rps" by Marc Majka (UBC Vision lab)
 *
 * EPSF code from Mike Zyda (Naval Postgraduate School)
 * 
 * Options: 
 * -C   - Generate color PostScript (instead of monochrome)
 * -h X - Image height in inches (default 3", width is found via aspect & size)
 * -s	- Scribe mode: don't generate "showpage", and default center is 3.25
 * -c X - Center image about X inches on the page (or margin if -s is on).
 * -a   - Aspect ratio of image (default 1.0)
 * 
 * If an input file isn't specified, it reads from stdin.  An output file
 * can be specified with -o (otherwise it writes to stdout).
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

void prologue(), puthexpix(), epilogue();

static int gencps = 0;		/* generate color PostScript? */

int
main( argc, argv )
int argc;
char **argv;
{
    char       *infname = NULL, 
    	       *outfname = NULL;
    FILE       *outfile;
    rle_hdr	hdr;
    int 	sflag = 0,
    		oflag = 0, 
    		cflag = 0;
    int		add_extra_white_line = 0;
    float 	heightinch = 3.0, 
    		center = 3.25, 
    		aspect = 1.0;
    int 	nrow, nscan, i, pix, dummy;
    float 	x1, y1, x2, y2, widthinch;
    unsigned char ** scan;
    unsigned char * buffer;

    if ( scanargs( argc, argv,
       "% C%- s%- h%-height!f c%-center!f a%-aspect!f o%-outfile!s infile%s\n(\
\tConvert a URT RLE image to PostScript.\n\
\t-C\tOutput color PostScript.\n\
\t-s\t\"Scribe mode\", no showpage, center about 3.25\" at bottom.\n\
\t-h\tScale output to given height in inches.\n\
\t-c\tCenter at given distance from left (default 3.25\").\n\
\t-a\tPixel aspect ratio (default 1.0).)",
	    &gencps, &sflag, &dummy, &heightinch, &cflag, &center, &dummy,
	    &aspect, &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    /* Open RLE file and get header information */
    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname, 0 );

    hdr.rle_file = rle_open_f(hdr.cmd, infname, "r");
    rle_get_setup_ok( &hdr, hdr.cmd, infname );

    if ( ( hdr.ncolors != 3 ) && ( hdr.ncolors != 1))
    {
	fprintf( stderr, "%s is not RGB or b&w image",
		    infname ? infname : "stdin" );
	exit( 0 );
    }

    if ( hdr.ncolors == 1 )
	gencps = 0;		/* can't have color out if no color in! */

    outfile = rle_open_f(hdr.cmd, outfname, "w");

    /* 
     * Spencer trick: save space by sliding the input image over to the
     * left margin.
     */
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;
    nrow = hdr.xmax + 1;
    nscan = (hdr.ymax - hdr.ymin + 1);

    /* The laserwriters throw out files with an odd number of scanlines! */
    if (nscan % 2)
    {
	nscan++;
	add_extra_white_line = 1;
    }

    /* Allocate scanline memory */

    buffer = (unsigned char *)malloc( nrow );
    rle_row_alloc( &hdr, &scan );

    /* Calculate image size and placement */

    widthinch = (float) nrow * heightinch * aspect / (float) nscan;
    if (sflag)
    {
	x1 = center - widthinch / 2.0;
	y1 = 0.0;
    }
    else
    {
	if (cflag)
	    x1 = center - widthinch / 2.0;
	else
	    /* center on whole page */
	    x1 = 4.25 - widthinch / 2.0;
	/* place top edge one inch from top of page */
	y1 = 11.0 - 1.0 - heightinch;
    }
    x2 = x1 + widthinch;
    y2 = y1 + heightinch;
    prologue(outfile,sflag,nscan,nrow,x1,y1,x2,y2);    

    while ( rle_getrow( &hdr, scan ) <= hdr.ymax )
    {
	if (gencps) {
	    /* generate a color line */
	    for(pix = 0; pix < nrow; pix ++) {
		puthexpix(outfile,scan[0][pix]);
		puthexpix(outfile,scan[1][pix]);
		puthexpix(outfile,scan[2][pix]);
	    }
	} else {
	    /* generate a monochrome line */
	    if (hdr.ncolors == 1)
		buffer = scan[0];
	    else
		rgb_to_bw( scan[0], scan[1], scan[2], buffer, nrow );
	    for(pix = 0; pix < nrow; pix ++)
		puthexpix(outfile,buffer[pix]);
	}
    }
    if (add_extra_white_line)
	for(i = 0; i < (gencps ? 3 : 1); i++)
	    for(pix = 0; pix < nrow; pix ++)
		puthexpix(outfile,255);

    epilogue(outfile, sflag);

    exit( 0 );
}

void
prologue(outfile,scribe_mode,nr,nc,x1,y1,x2,y2)
FILE *outfile;
int scribe_mode;
int nr,nc;
float x1,y1,x2,y2;
{
    fprintf(outfile,"%%!\n");
    fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
	    (int)(x1 * 72), (int)(y1 * 72),
	    (int)(x2 * 72), (int)(y2 * 72));
    fprintf(outfile, "%%%%EndComments\n");
    fprintf(outfile,"gsave\n");
    if (! scribe_mode)
	fprintf(outfile,"initgraphics\n");
    fprintf(outfile,"72 72 scale\n");
    fprintf(outfile,"/imline %d string def\n",nc*2*(gencps?3:1));
    fprintf(outfile,"/drawimage {\n");
    fprintf(outfile,"    %d %d 8\n",nc,nr);
    fprintf(outfile,"    [%d 0 0 %d 0 %d]\n",nc,-1*nr,nr);
    fprintf(outfile,"    { currentfile imline readhexstring pop } ");
    if (gencps)
	fprintf(outfile,"false 3 colorimage\n");
    else
	fprintf(outfile,"image\n");
    fprintf(outfile,"} def\n");
    fprintf(outfile,"%f %f translate\n",x1,y2);
    fprintf(outfile,"%f %f scale\n",x2-x1,y1-y2);
    fprintf(outfile,"drawimage\n");
}

void
epilogue(outfile, scribemode)
FILE *outfile;
int scribemode;
{
    fprintf(outfile,"\n");
    if (!scribemode)
	fprintf(outfile,"showpage\n");
    fprintf(outfile,"grestore\n");
}

void
puthexpix(outfile,p)
FILE *outfile;
unsigned char p;
{
    static int npixo = 0;
    static char tohex[] = "0123456789ABCDEF";

    putc(tohex[(p>>4)&0xF],outfile);
    putc(tohex[p&0xF],outfile);
    npixo += 1;
    if (npixo >= 32) {
	putc('\n',outfile);
        npixo = 0;
    }
}
