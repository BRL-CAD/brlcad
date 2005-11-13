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
#ifndef lint
char rcsid[] = "$Header$";
#endif
/*
rlehdr()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "../patchlevel.h"

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

void
main( argc, argv )
int argc;
char **argv;
{
    CONST_DECL char ** fname = NULL;
    CONST_DECL char *stdname = "-";
    char **comment_names = NULL;
    int     	brief = 0, 
		cflag = 0,
    		hflag = 0,
    		num_hdr = 1,
		mflag = 0,
    		suppress = 0,
		dbg_flag = 0,
		nfname = 0,
		version = 0;
    int rle_err = 0, rle_cnt;
    rle_hdr in_hdr;

    in_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
	   "% b%- c%-comment-name!,s h%-n%d Mm%- s%- d%- v%- infile%*s (\n\
\t-b\tBrief mode, one line per image.\n\
\t-c\tWith -b, print 1st line of 1st comment matching a name.\n\
\t-h [n]\tPrint only first (n) header(s) in each file.\n\
\t-m\tPrint contents of colormap.\n\
\t-M\tPrint colormap suitable for rleldmap -t.\n\
\t-s\tSuppress printing header at all (useful with -M).\n\
\t-d\tPrint image data (LOTS of output)\n\
\t-v\tPrint URT version number.  Ignores other arguments.)",
		   &brief, &cflag, &cflag, &comment_names,
		   &hflag, &num_hdr, &mflag, &suppress, &dbg_flag, &version,
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
	in_hdr.rle_file = rle_open_f(cmd_name( argv ), *fname, "r");
	rle_names( &in_hdr, cmd_name( argv ), *fname, 0 );

	for ( rle_cnt = 0;
	      !(hflag && rle_cnt >= num_hdr) &&
	      (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	      rle_cnt++ )
	{
	    /* Separate headers with a newline. */
	    if ( !brief && rle_cnt > 0 )
		putchar( '\n' );

	    if ( !suppress )
		if ( brief )
		    print_brief_hdr( &in_hdr, cflag, comment_names );
		else
		    print_hdr( &in_hdr );
	    if ( mflag )
		print_map( &in_hdr, mflag );
	    if ( dbg_flag )
		print_codes( &in_hdr );
	    else if ( !hflag || rle_cnt + 1 < num_hdr )
		while ( rle_getskip( &in_hdr ) != 32768 )
		    ;
	}

	/* Separate headers with a newline. */
	if ( !brief && nfname > 1 )
	    putchar( '\n' );

	fclose( in_hdr.rle_file );

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
 *	the_hdr:	Header information.
 * Outputs:
 * 	Prints info on stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
print_hdr( the_hdr )
rle_hdr *the_hdr;
{
    register int i;

    if ( the_hdr->img_num > 1 )
	printf( "RLE header information for %s image %d:\n",
		the_hdr->file_name, the_hdr->img_num );
    else
	printf( "RLE header information for %s:\n", the_hdr->file_name );
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
 *	the_hdr:	Header information.
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
print_brief_hdr( the_hdr, ncomment, comment_names )
rle_hdr *the_hdr;
int ncomment;
char **comment_names;
{
    register int i;

    if ( the_hdr->img_num > 1 )
	printf( "%s(%d):",
		the_hdr->file_name, the_hdr->img_num );
    else
	printf( "%s:", the_hdr->file_name );
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
		if (the_comment = rle_getcom( *comment_names, the_hdr ))
		{
		    if ( (cp = index( the_comment, '\n' )) )
			printf( ", %s=%.*s", *comment_names,
				*comment_names, cp - the_comment - 1, the_comment );
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
 * 	mflag:		If 1, print "human readable".  If 2, print in
 * 			form suitable for rleldmap -t.
 * Outputs:
 * 	Prints color map (if present) on stdout.
 * Assumptions:
 *	color_map_length comment is relevant.
 * Algorithm:
 *	[None]
 */
void
print_map( the_hdr, mflag )
rle_hdr *the_hdr;
int mflag;
{
    register int i, j;
    int c, maplen, ncmap, nmap;
    rle_map * cmap;
    char *len_com;

    if ( the_hdr->ncmap == 0 )
	return;

    maplen = (1 << the_hdr->cmaplen);
    ncmap = the_hdr->ncmap;
    cmap = the_hdr->cmap;

    if ( (len_com = rle_getcom( "color_map_length", the_hdr )) != NULL &&
	 atoi( len_com ) > 0 )
	nmap = atoi( len_com );
    else
	nmap = maplen;

    if ( mflag == 1 )
    {
	printf( "Color map contents, values are 16-bit(8-bit):\n" );
	for ( i = 0; i < nmap; i++ )
	{
	    printf( "%3d:\t", i );
	    for ( j = 0, c = 0; j < ncmap; j++, c += maplen )
		printf( "%5d(%3d)%c", cmap[i+c], cmap[i+c] >> 8,
			j == ncmap - 1 ? '\n' : '\t' );

	}
    }
    else
	for ( i = 0; i < nmap; i++ )
	{
	    for ( j = 0, c = 0; j < ncmap; j++, c += maplen )
		printf( "%3d%c", cmap[i+c] >> 8,
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

    if ( rle_row_alloc( the_hdr, &scans ) < 0 )
	RLE_CHECK_ALLOC( "rlehdr", 0, 0 );

    rle_debug(1);
    while ( !feof( the_hdr->rle_file ) && 
	    !the_hdr->priv.get.is_eof )
	rle_getrow( the_hdr, scans );

    rle_row_free( the_hdr, scans );
}
