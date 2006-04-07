/* 
 * unslice.c - Build a finished frame from a series of slices
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue May 19 1987
 * Copyright (c) 1987, University of Utah
 *
 * Lots of this code is swiped from comp.c
 */
static char rcsid[] = "$Header$";
/*
unslice()			Tag the file.
*/

/*
 * The "control file" is a text file with two numbers per line, each
 * line giving the starting and ending lines (inclusive) that are to
 * be taken from each input file to the output file.  This allows
 * potentially ragged portions of slices to be cropped away.
 *
 * If no control file is given, then the extents given in the slices' headers
 * are used.  If two files overlap, the first lines from the second file
 * are thrown away.  The maximum y of the image must be specified this way.
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

void do_slice(), copy_scanline();

rle_hdr in_hdr, out_hdr;

/* 
 * Global raw data structures for copy_scanline.
 */
rle_op ** out_raw;
int * out_nraw;

int max_y = 0;			/* Maximum Y value */
int num_skip = 0;		/* Counter for keeping track of raw lines */

int ctlflag = 0;		/* If true then we are using a ctl file */
int ctl_lines;			/* Number of lines in control file */
int *start_line,
    *stop_line;			/* Start and ending lines for the slices */

char default_ctl_file[] = "unslice.ctl";
char *progname;
char **gargv;

int
main (argc, argv)
int argc;
char **argv;
{
    int nfiles, i;
    char **slicefiles;
    FILE *ctlfile;
    int y_flag, oflag = 0;

    char * ctlfilename = NULL, * out_fname = NULL;

    progname = cmd_name( argv );
    gargv = argv;

    if (! scanargs(argc, argv,
		   "% y%-ymax!d f%-ctlfile!s files!*s o%-outfile!s",
		   &y_flag, &max_y,
		   &ctlflag, &ctlfilename, &nfiles, &slicefiles,
                   &oflag, &out_fname ))
	exit(1);

    /* Open the output file */
    (void)rle_hdr_init( &in_hdr );
    (void)rle_hdr_init( &out_hdr );
    out_hdr.rle_file = rle_open_f(progname, out_fname, "w");
    rle_names( &out_hdr, cmd_name( argv ), out_fname, 0 );

    start_line = (int *) malloc( nfiles * sizeof(int) );
    RLE_CHECK_ALLOC( progname, start_line, 0 );
    stop_line = (int *) malloc( nfiles * sizeof(int) );
    RLE_CHECK_ALLOC( progname, stop_line, 0 );

    if (ctlflag)		/* Read in and verify the control file */
    {
	if (! ctlfilename)
	    ctlfilename = default_ctl_file;

	/* Read in the control file */

	ctlfile = rle_open_f( progname, ctlfilename, "r" );

	ctl_lines = 0;
	while (! feof( ctlfile ))
	{
	    fscanf( ctlfile, "%d %d",
		   &(start_line[ctl_lines]), &(stop_line[ctl_lines]) );

	    if (start_line[ctl_lines] >= stop_line[ctl_lines])
		fprintf( stderr, "%s: ctl file garbled? (start %d, stop %d)\n",
			 progname,
			 start_line[ctl_lines], stop_line[ctl_lines] );
	    ctl_lines++;
	}
	ctl_lines--;

	if (ctl_lines > nfiles)
	    fprintf(stderr,
		    "%s: Warning: not enough slices for ctl file\n", progname);
	if (ctl_lines < nfiles)
	    fprintf(stderr,
		    "%s: Warning: too many slices for ctl file\n", progname);

    }
    else
	if (! max_y)
	{
	    fprintf(stderr,
		    "%s: max y (-y ymax) must be given if no ctl file used\n",
		    progname);
	    exit(-1);
	}

    /* Process the slices */

    for (i = 0; i < nfiles; i++)
	do_slice( i, slicefiles[i] );

    rle_puteof( &out_hdr );

    exit( 0 );
}

/*****************************************************************
 * TAG( do_slice )
 * 
 * Read one slice from the given file and write it to the output.
 * Also generate the output header if it's the first file.
 */
void
do_slice( num, filename )
int num;
char *filename;
{
    register int y;
    static int current_y = 0;

    in_hdr.rle_file = rle_open_f(progname, filename, "r");
    rle_names( &in_hdr, progname, filename, 0 );
    
    /*
     * Many sanity checks.  Code must be 3am-proof!
     */

    rle_get_setup_ok( &in_hdr, progname, filename );
    
    if (ctlflag &&
       ((start_line[num] > in_hdr.ymax) ||
	(stop_line[num] < in_hdr.ymin)))
    {
	fprintf(stderr, "%s: %s is out of slice range (%d %d)\n",
		progname, filename, start_line[num], stop_line[num]);
	exit(-1);
    }

    /* 
     * If this is the first slice, generate the output header.
     */
    
    if (num == 0)
    {
	FILE *f = out_hdr.rle_file;
	CONST_DECL char *name = out_hdr.file_name;

	out_hdr.file_name = NULL;
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.file_name = name;
	out_hdr.rle_file = f;
	rle_addhist( gargv, (rle_hdr *)0, &out_hdr );

	if (ctlflag)
	{
	    out_hdr.ymin = start_line[0];
	    out_hdr.ymax = stop_line[ctl_lines-1];
	}
	else
	{
	    out_hdr.ymin = in_hdr.ymin;
	    out_hdr.ymax = max_y;
	}
	current_y = out_hdr.ymin;
	rle_put_setup( &out_hdr );

	if (rle_raw_alloc( &out_hdr, &out_raw, &out_nraw ) < 0)
	    RLE_CHECK_ALLOC( progname, 0, 0 );
    }

    if ((! ctlflag) && (in_hdr.ymax < current_y))
    {
	fprintf(stderr,
	       "%s: warning: slice %s completely ignored (wrong order?)\n",
		progname, filename);
 	fclose( in_hdr.rle_file );
	return;
    }

    /*
     * Copy the file to the output.
     */
    num_skip = 0;

    if (ctlflag)
    {
	for (y = in_hdr.ymin; y <= in_hdr.ymax; y++ )
	    if ((y >= start_line[num]) && (y <= stop_line[num]))
		copy_scanline( y, 1 );
	    else
		copy_scanline( y, 0 ); /* Data out of range, just toss it. */
    }
    else
    {
	for (y = in_hdr.ymin; y <= in_hdr.ymax; y++ )
	    if (y >= current_y)
		copy_scanline( y, 1 );
	    else
		copy_scanline( y, 0 );

	current_y = in_hdr.ymax + 1;
    }

    fclose( in_hdr.rle_file );
}

/*
 * The "skip counter" is stolen from comp.  It works like this:
 * if num_skip == 0, then we read the next line normally.  If it's
 * positive, then it tells us how many blank lines before the
 * next available real data.  If it's -1, then it means that the
 * output raw data should be used before calling rle_getraw again.
 */

/*****************************************************************
 * TAG( copy_scanline )
 * 
 * Copy the scanlines using the raw format, if the copy_flag is on.  If
 * copy_flag is false, scanlines are just eaten away from the input file.
 */
void
copy_scanline( ypos, copy_flag )
int ypos;
int copy_flag;			/* If true, write the output */
{

 SKIP_ROW:

    if (num_skip > 0)		/* Must skip blank rows */
    {
	if (copy_flag)
	    rle_skiprow( &out_hdr, 1 );
	num_skip--;
	if (num_skip == 0)
	    num_skip = -1;	/* Flag raw data available */
	return;
    }

    if (num_skip == 0)		/* ...Must read new data */
	num_skip = rle_getraw( &in_hdr, out_raw, out_nraw );
    else
	num_skip = ypos;	/* num_skip < 0, data was already there */

    if (num_skip == 32768)	/* Hit the EOF */
    {
	if (copy_flag)
	    rle_skiprow( &out_hdr, 1 );
	return;
    }

    num_skip -= ypos;		/* Find how many blank lines left */

    if (num_skip > 0)
	goto SKIP_ROW;

    if (copy_flag)
	rle_putraw( out_raw, out_nraw, &out_hdr );
    rle_freeraw( &out_hdr, out_raw, out_nraw );
}
