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
 * rlehdr.c - Print header from an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Jan 22 1987
 * Copyright (c) 1987, University of Utah
 */

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif /* USE_STRING_H */

#include "machine.h"
#include "externs.h"
#include "rle.h"
/* #include "../patchlevel.h" */

/* Utah Raster Toolkit major version number. */
#define URT_VERSION 	3.0

void print_hdr(), print_map(), print_codes(), print_brief_hdr();

/*****************************************************************
 * TAG( main )
 * 
 * Read and print in human readable form the header of an RLE file.
 *
 * Usage:
 *	rlehdr [-b] [-c comment-name] [-d] [-m] [-v] [files ...]
 * Inputs:
 *  	-b: 	    	Brief.  Fit it on one line.
 *      -c comment-names,...:
    	    	    	The first line of the first comment matching
 *  	    	    	one of these names will be printed.  Sets -b.
 *	-m:		Dump color map contents.
 *	-d:		Dump entire file contents.
 *	-v: 	    	Print URT version number (and ignores files.)
 * 	files:		RLE file(s) to interpret.  If none, reads
 * 	    	    	from standard input.
 * Outputs:
 * 	Prints contents of file header.
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
    const char ** fname = NULL;
    const char *stdname = "-";
    char **comment_names = NULL;
    const char *the_file;
    int    	brief = 0, 
		cflag = 0,
		mflag = 0,
		dbg_flag = 0,
		nfname = 0,
		version = 0;
    int rle_err, rle_cnt;

    if ( scanargs( argc, argv, "% b%- c%-comment-name!,s m%- d%- v%- infile%*s",
		   &brief, &cflag, &cflag, &comment_names,
		   &mflag, &dbg_flag, &version,
		   &nfname, &fname ) == 0 )
	exit( 1 );

    /* If -c was specified, set -b. */
    if ( cflag )
	brief = 1;

    if ( nfname == 0 )
    {
	nfname = 1;
	fname = &stdname;
    }

    if ( version )
    {
	printf( "Utah Raster Toolkit version %3.1f", URT_VERSION );
#if PATCHLEVEL > 0
    	printf( "(patch %d)", PATCHLEVEL );
#endif
	putchar( '\n' );
	exit( 0 );
    }

    for ( ; nfname > 0; fname++, nfname-- )
    {
	if ( (rle_dflt_hdr.rle_file =
	      rle_open_f(cmd_name( argv ), *fname, "r")) == stdin)
	    the_file = "Standard Input";
	else
	    the_file = *fname;

	for ( rle_cnt = 0;
	      (rle_err = rle_get_setup( &rle_dflt_hdr )) == RLE_SUCCESS;
	      rle_cnt++ )
	{
	    /* Separate headers with a newline. */
	    if ( !brief && rle_cnt > 0 )
		putchar( '\n' );

	    if ( brief )
		print_brief_hdr( the_file, &rle_dflt_hdr, rle_cnt,
				 cflag, comment_names );
	    else
		print_hdr( the_file, &rle_dflt_hdr, rle_cnt );
	    if ( mflag )
		print_map( &rle_dflt_hdr );
	    if ( dbg_flag )
		print_codes( &rle_dflt_hdr );
	    else
		while ( rle_getskip( &rle_dflt_hdr ) != 32768 )
		    ;
	}

	/* Separate headers with a newline. */
	if ( !brief && nfname > 1 )
	    putchar( '\n' );

	/* Check for an error.  EOF or EMPTY is ok if at least one image
	 * has been read.  Otherwise, print an error message.
	 */
	if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	    rle_get_error( rle_err, cmd_name( argv ), *fname );
    }

    exit( 0 );			/* success */
}


/*****************************************************************
 * TAG( print_hdr )
 * 
 * Print the RLE header information given.
 *
 * Inputs:
 * 	fname:		Name of file header was read from.
 *	the_hdr:	Header information.
 * Outputs:
 * 	Prints info on stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
print_hdr( fname, the_hdr, rle_cnt )
char *fname;
rle_hdr *the_hdr;
int rle_cnt;
{
    register int i;

    if ( rle_cnt > 0 )
	printf( "RLE header information for %s image %d:\n",
		fname, rle_cnt + 1 );
    else
	printf( "RLE header information for %s:\n", fname );
    printf( "Originally positioned at (%d, %d), size (%d, %d)\n",
	    the_hdr->xmin, the_hdr->ymin,
	    the_hdr->xmax - the_hdr->xmin + 1,
	    the_hdr->ymax - the_hdr->ymin + 1 );
    printf( "Saved %d color channels%s\n", the_hdr->ncolors,
	    the_hdr->alpha ? " plus Alpha channel" : "" );
    if ( the_hdr->background == 0 )
	printf( "No background information was saved\n" );
    else
    {
	if ( the_hdr->background == 1 )
	    printf( "Saved in overlay mode with original background color" );
	else
	    printf( "Screen will be cleared to background color" );
	for ( i = 0; i < the_hdr->ncolors; i++ )
	    printf( " %d", the_hdr->bg_color[i] );
	putchar( '\n' );
    }

    if ( the_hdr->ncmap > 0 )
	printf( "%d channels of color map %d entries long were saved.\n",
		the_hdr->ncmap, 1 << the_hdr->cmaplen );

    if ( the_hdr->comments != NULL && *the_hdr->comments != NULL )
    {
	printf( "Comments:\n" );
	for ( i = 0; the_hdr->comments[i] != NULL; i++ )
	    printf( "%s\n", the_hdr->comments[i] );
    }

    fflush( stdout );
}

/*****************************************************************
 * TAG( print_brief_hdr )
 * 
 * Print the RLE header information on one line.
 *
 * Inputs:
 * 	fname:		Name of file header was read from.
 *	the_hdr:	Header information.
 *	rle_cnt:    	Count of this image in the file.
 *	ncomment:   	Number of comment names to test.
 *	comment_names:	Print the first line of the first matching
 *	    	    	comment found. 
 * Outputs:
 * 	Prints info on stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
print_brief_hdr( fname, the_hdr, rle_cnt, ncomment, comment_names )
char *fname;
rle_hdr *the_hdr;
int rle_cnt, ncomment;
char **comment_names;
{
    register int i;

    if ( rle_cnt > 0 )
	printf( "%s(%d):",
		fname, rle_cnt + 1 );
    else
	printf( "%s:", fname );
    printf( "\t[%d,%d]+[%d,%d]",
	    the_hdr->xmin, the_hdr->ymin,
	    the_hdr->xmax - the_hdr->xmin + 1,
	    the_hdr->ymax - the_hdr->ymin + 1 );
    printf( "x%d%s", the_hdr->ncolors,
	    the_hdr->alpha ? "+A" : "" );
    if ( the_hdr->background == 0 )
	;
    else
    {
	if ( the_hdr->background == 1 )
	    printf( ", OV=" );
	else
	    printf( ", BG=" );
	for ( i = 0; i < the_hdr->ncolors; i++ )
	    printf( "%d%s", the_hdr->bg_color[i],
		    i == the_hdr->ncolors - 1 ? " " : "," );
    }

    if ( the_hdr->ncmap > 0 )
	printf( ", map %dx%d",
		the_hdr->ncmap, 1 << the_hdr->cmaplen );

    if ( the_hdr->comments != NULL && *the_hdr->comments != NULL )
    {
	char *the_comment, *cp;

	if ( ncomment > 0 )
	{
	    for ( ; ncomment > 0; ncomment--, comment_names++ )
		if ( (the_comment = rle_getcom( *comment_names, the_hdr )) )
		{
		    if ( (cp = strchr( the_comment, '\n' )) )
			printf( ", %s=%.*s", *comment_names,
				(int)(cp - the_comment - 1), the_comment );
		    else
			printf( ", %s=%s", *comment_names, the_comment );
		    break;
		}
	}
	if ( ncomment == 0 )
	    printf( ", (C)" );
    }

    putchar( '\n' );

    fflush( stdout );
}

/*****************************************************************
 * TAG( print_map )
 * 
 * Print the color map from a the_hdr structure.
 * Inputs:
 * 	the_hdr:	Sv_hdr structure containing color map.
 * Outputs:
 * 	Prints color map (if present) on stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
print_map( the_hdr )
rle_hdr *the_hdr;
{
    register int i, j;
    int c, maplen, ncmap;
    rle_map * cmap;

    if ( the_hdr->ncmap == 0 )
	return;

    maplen = (1 << the_hdr->cmaplen);
    ncmap = the_hdr->ncmap;
    cmap = the_hdr->cmap;

    printf( "Color map contents, values are 16-bit(8-bit):\n" );
    for ( i = 0; i < maplen; i++ )
    {
	printf( "%3d:\t", i );
	for ( j = 0, c = 0; j < ncmap; j++, c += maplen )
	    printf( "%5d(%3d)%c", cmap[i+c], cmap[i+c] >> 8,
		    j == ncmap - 1 ? '\n' : '\t' );

    }
}


/*****************************************************************
 * TAG( print_codes )
 * 
 * Print the RLE opcodes in an RLE file.
 * Inputs:
 * 	the_hdr:		Header for RLE file (already open).
 * Outputs:
 * 	Prints file contents on stderr.
 * Assumptions:
 * 	
 * Algorithm:
 *	[None]
 */
void
print_codes( the_hdr )
rle_hdr *the_hdr;
{
    rle_pixel ** scans;

    rle_row_alloc( the_hdr, &scans );

    rle_debug(1);
    while ( !feof( the_hdr->rle_file ) && 
	    !the_hdr->priv.get.is_eof )
	rle_getrow( the_hdr, scans );

    rle_row_free( the_hdr, scans );
}
