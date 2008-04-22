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
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/*
 * rle_getrow.c - Read an RLE file in.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Apr 10 1985
 * Copyright (c) 1985 Spencer W. Thomas
 *
 * $Id$
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include "stdio.h"
#include "rle.h"
#include "rle_code.h"

/* Read a two-byte "short" that started in VAX (LITTLE_ENDIAN) order */
#define VAXSHORT( var, fp )\
	{ var = fgetc(fp)&0xFF; var |= (fgetc(fp)) << 8; }

/* Instruction format -- first byte is opcode, second is datum. */

#define OPCODE(inst) (inst[0] & ~LONG)
#define LONGP(inst) (inst[0] & LONG)
#define DATUM(inst) (inst[1] & 0xff)	/* Make sure it's unsigned. */

static int	   debug_f;		/* If non-zero, print debug info. */
static void	bfill();
extern int vax_gshort();

/*****************************************************************
 * TAG( rle_get_setup )
 *
 * Read the initialization information from an RLE file.
 * Inputs:
 * 	the_hdr:    Contains pointer to the input file.
 * Outputs:
 * 	the_hdr:    Initialized with information from the
 *		    input file.
 *	Returns 0 on success, -1 if the file is not an RLE file,
 *	-2 if malloc of the color map failed, -3 if an immediate EOF
 *	is hit (empty input file), and -4 if an EOF is encountered reading
 *	the setup information.
 * Assumptions:
 * 	infile points to the "magic" number in an RLE file (usually
 * byte 0 in the file).
 * Algorithm:
 * 	Read in the setup info and fill in the_hdr.
 */
int
rle_get_setup( the_hdr )
rle_hdr * the_hdr;
{
    struct XtndRsetup setup;
    short magic;
    register FILE *infile = the_hdr->rle_file;
    rle_pixel * bg_color;
    register int i;
    char * comment_buf;

    /* Clear old stuff out of the header. */
    rle_hdr_clear( the_hdr );
    if ( the_hdr->is_init != RLE_INIT_MAGIC )
	rle_names( the_hdr, "Urt", "some file", 0 );
    the_hdr->img_num++;		/* Count images. */

    VAXSHORT( magic, infile );
    if ( feof( infile ) )
	return RLE_EMPTY;
    if ( magic != RLE_MAGIC )
	return RLE_NOT_RLE;
    fread( &setup, 1, SETUPSIZE, infile );  /* assume VAX packing */
    if ( feof( infile ) )
	return RLE_EOF;

    /* Extract information from setup */
    the_hdr->ncolors = setup.h_ncolors;
    for ( i = 0; i < the_hdr->ncolors; i++ )
	RLE_SET_BIT( *the_hdr, i );

    if ( !(setup.h_flags & H_NO_BACKGROUND) && setup.h_ncolors > 0 )
    {
	the_hdr->bg_color = (int *)malloc(
	    (unsigned)(sizeof(int) * setup.h_ncolors) );
	bg_color = (rle_pixel *)malloc(
	    (unsigned)(1 + (setup.h_ncolors / 2) * 2) );
	RLE_CHECK_ALLOC( the_hdr->cmd, the_hdr->bg_color && bg_color,
			 "background color" );
	fread( (char *)bg_color, 1, 1 + (setup.h_ncolors / 2) * 2, infile );
	for ( i = 0; i < setup.h_ncolors; i++ )
	    the_hdr->bg_color[i] = bg_color[i];
	free( bg_color );
    }
    else
    {
	(void)getc( infile );	/* skip filler byte */
	the_hdr->bg_color = NULL;
    }

    if ( setup.h_flags & H_NO_BACKGROUND )
	the_hdr->background = 0;
    else if ( setup.h_flags & H_CLEARFIRST )
	the_hdr->background = 2;
    else
	the_hdr->background = 1;
    if ( setup.h_flags & H_ALPHA )
    {
	the_hdr->alpha = 1;
	RLE_SET_BIT( *the_hdr, RLE_ALPHA );
    }
    else
	the_hdr->alpha = 0;

    the_hdr->xmin = vax_gshort( setup.hc_xpos );
    the_hdr->ymin = vax_gshort( setup.hc_ypos );
    the_hdr->xmax = the_hdr->xmin + vax_gshort( setup.hc_xlen ) - 1;
    the_hdr->ymax = the_hdr->ymin + vax_gshort( setup.hc_ylen ) - 1;

    the_hdr->ncmap = setup.h_ncmap;
    the_hdr->cmaplen = setup.h_cmaplen;
    if ( the_hdr->ncmap > 0 )
    {
	register int maplen =
		     the_hdr->ncmap * (1 << the_hdr->cmaplen);
	register int i;
	register char *maptemp;

	the_hdr->cmap = (rle_map *)malloc(
	    (unsigned)(sizeof(rle_map) * maplen) );
	maptemp = (char *)malloc( 2 * maplen );
	if ( the_hdr->cmap == NULL || maptemp == NULL )
	{
	    fprintf( stderr,
"%s: Malloc failed for color map of size %d*%d in rle_get_setup, reading %s\n",
		     the_hdr->cmd,
		     the_hdr->ncmap, (1 << the_hdr->cmaplen),
		     the_hdr->file_name );
	    return RLE_NO_SPACE;
	}
	fread( maptemp, 2, maplen, infile );
	for ( i = 0; i < maplen; i++ )
	    the_hdr->cmap[i] = vax_gshort( &maptemp[i * 2] );
	free( maptemp );
    }

    /* Check for comments */
    if ( setup.h_flags & H_COMMENT )
    {
	short comlen, evenlen;
	register char * cp;

	VAXSHORT( comlen, infile );	/* get comment length */
	evenlen = (comlen + 1) & ~1;	/* make it even */
	if ( evenlen )
	{
	    comment_buf = (char *)malloc( (unsigned) evenlen );

	    if ( comment_buf == NULL )
	    {
		fprintf( stderr,
"%s: Malloc failed for comment buffer of size %d in rle_get_setup, reading %s\n",
			 the_hdr->cmd, comlen, the_hdr->file_name );
		return RLE_NO_SPACE;
	    }
	    fread( comment_buf, 1, evenlen, infile );
	    /* Count the comments */
	    for ( i = 0, cp = comment_buf; cp < comment_buf + comlen; cp++ )
		if ( *cp == 0 )
		    i++;
	    i++;			/* extra for NULL pointer at end */
	    /* Get space to put pointers to comments */
	    the_hdr->comments =
		(CONST_DECL char **)malloc( (unsigned)(i * sizeof(char *)) );
	    if ( the_hdr->comments == NULL )
	    {
		fprintf( stderr,
 "%s: Malloc failed for %d comment pointers in rle_get_setup, reading %s\n",
			 the_hdr->cmd, i, the_hdr->file_name );
		return RLE_NO_SPACE;
	    }
	    /* Get pointers to the comments */
	    *the_hdr->comments = comment_buf;
	    for ( i = 1, cp = comment_buf + 1;
		  cp < comment_buf + comlen;
		  cp++ )
		if ( *(cp - 1) == 0 )
		    the_hdr->comments[i++] = cp;
	    the_hdr->comments[i] = NULL;
	}
	else
	    the_hdr->comments = NULL;
    }
    else
	the_hdr->comments = NULL;

    /* Initialize state for rle_getrow */
    the_hdr->priv.get.scan_y = the_hdr->ymin;
    the_hdr->priv.get.vert_skip = 0;
    the_hdr->priv.get.is_eof = 0;
    the_hdr->priv.get.is_seek = ftell( infile ) > 0;
    debug_f = 0;

    if ( !feof( infile ) )
	return RLE_SUCCESS;	/* success! */
    else
    {
	the_hdr->priv.get.is_eof = 1;
	return RLE_EOF;
    }
}


/*****************************************************************
 * TAG( rle_get_setup_ok )
 *
 * Read the initialization information from an RLE file.
 * Inputs:
 * 	the_hdr:    Contains pointer to the input file.
 *	prog_name:  Program name to be printed in the error message.
 *      file_name:  File name to be printed in the error message.
 *                  If NULL, the string "stdin" is generated.
 * Outputs:
 * 	the_hdr:    Initialized with information from the
 *		    input file.
 *      If reading the header fails, it prints an error message
 *	and exits with the appropriate status code.
 * Algorithm:
 * 	rle_get_setup does all the work.
 */
void
rle_get_setup_ok( the_hdr, prog_name, file_name )
rle_hdr * the_hdr;
CONST_DECL char *prog_name;
CONST_DECL char *file_name;
{
    int code;

    /* Backwards compatibility: if is_init is not properly set,
     * initialize the header.
     */
    if ( the_hdr->is_init != RLE_INIT_MAGIC )
    {
	FILE *f = the_hdr->rle_file;
	rle_hdr_init( the_hdr );
	the_hdr->rle_file = f;
	rle_names( the_hdr, prog_name, file_name, 0 );
    }

    code = rle_get_error( rle_get_setup( the_hdr ),
			  the_hdr->cmd, the_hdr->file_name );
    if (code)
	exit( code );
}


/*****************************************************************
 * TAG( rle_debug )
 *
 * Turn RLE debugging on or off.
 * Inputs:
 * 	on_off:		if 0, stop debugging, else start.
 * Outputs:
 * 	Sets internal debug flag.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
rle_debug( on_off )
int on_off;
{
    debug_f = on_off;

    /* Set line buffering on stderr.  Character buffering is the default, and
     * it is SLOOWWW for large amounts of output.
     */
#if defined(_WIN32) && !defined(__CYGWIN__)
    {
	char buf[1024];

	setvbuf(stderr, buf, _IOLBF, sizeof(buf));
    }
#else
    setlinebuf( stderr );
#endif
}


/*****************************************************************
 * TAG( rle_getrow )
 *
 * Get a scanline from the input file.
 * Inputs:
 *	the_hdr:    Header structure containing information about
 *		    the input file.
 * Outputs:
 * 	scanline:   an array of pointers to the individual color
 *		    scanlines.  Scanline is assumed to have
 *		    the_hdr->ncolors pointers to arrays of rle_pixel,
 *		    each of which is at least the_hdr->xmax+1 long.
 *	Returns the current scanline number.
 * Assumptions:
 * 	rle_get_setup has already been called.
 * Algorithm:
 * 	If a vertical skip is being executed, and clear-to-background is
 *	specified (the_hdr->background is true), just set the
 *	scanlines to the background color.  If clear-to-background is
 *	not set, just increment the scanline number and return.
 *
 *	Otherwise, read input until a vertical skip is encountered,
 *	decoding the instructions into scanline data.
 *
 *	If ymax is reached (or, somehow, passed), continue reading and
 *	discarding input until end of image.
 */
int
rle_getrow( the_hdr, scanline )
rle_hdr * the_hdr;
rle_pixel *scanline[];
{
    register rle_pixel * scanc;
    register int nc;
    register FILE *infile = the_hdr->rle_file;
    int scan_x = the_hdr->xmin,	/* current X position */
    	max_x = the_hdr->xmax,	/* End of the scanline */
	   channel = 0;			/* current color channel */
    int ns;			/* Number to skip */
    short word, long_data;
    char inst[2];

    /* Clear to background if specified */
    if ( the_hdr->background != 1 )
    {
	if ( the_hdr->alpha && RLE_BIT( *the_hdr, -1 ) )
	    bzero( (char *)scanline[-1] + the_hdr->xmin,
		   the_hdr->xmax - the_hdr->xmin + 1 );
	for ( nc = 0; nc < the_hdr->ncolors; nc++ )
	    if ( RLE_BIT( *the_hdr, nc ) )
		/* Unless bg color given explicitly, use 0. */
		if ( the_hdr->background != 2 || the_hdr->bg_color[nc] == 0 )
		    bzero( (char *)scanline[nc] + the_hdr->xmin,
			   the_hdr->xmax - the_hdr->xmin + 1 );
		else
		    bfill( (char *)scanline[nc] + the_hdr->xmin,
			   the_hdr->xmax - the_hdr->xmin + 1,
			   the_hdr->bg_color[nc] );
    }

    /* If skipping, then just return */
    if ( the_hdr->priv.get.vert_skip > 0 )
    {
	the_hdr->priv.get.vert_skip--;
	the_hdr->priv.get.scan_y++;
	if ( the_hdr->priv.get.vert_skip > 0 )
	    if ( the_hdr->priv.get.scan_y >= the_hdr->ymax )
	    {
		int y = the_hdr->priv.get.scan_y;
		while ( rle_getskip( the_hdr ) != 32768 )
		    ;
		return y;
	    }
	    else
		return the_hdr->priv.get.scan_y;
    }

    /* If EOF has been encountered, return also */
    if ( the_hdr->priv.get.is_eof )
	return ++the_hdr->priv.get.scan_y;

    /* Otherwise, read and interpret instructions until a skipLines
     * instruction is encountered.
     */
    if ( RLE_BIT( *the_hdr, channel ) )
	scanc = scanline[channel] + scan_x;
    else
	scanc = NULL;
    for (;;)
    {
	inst[0] = getc( infile );
	inst[1] = getc( infile );
	if ( feof(infile) )
	{
	    the_hdr->priv.get.is_eof = 1;
	    break;		/* <--- one of the exits */
	}

	switch( OPCODE(inst) )
	{
	case RSkipLinesOp:
	    if ( LONGP(inst) )
	    {
		VAXSHORT( the_hdr->priv.get.vert_skip, infile );
	    }
	    else
		the_hdr->priv.get.vert_skip = DATUM(inst);
	    if (debug_f)
		fprintf(stderr, "Skip %d Lines (to %d)\n",
			the_hdr->priv.get.vert_skip,
			the_hdr->priv.get.scan_y +
			    the_hdr->priv.get.vert_skip );

	    break;			/* need to break for() here, too */

	case RSetColorOp:
	    channel = DATUM(inst);	/* select color channel */
	    if ( channel == 255 )
		channel = -1;
	    scan_x = the_hdr->xmin;
	    if ( RLE_BIT( *the_hdr, channel ) )
		scanc = scanline[channel]+scan_x;
	    if ( debug_f )
		fprintf( stderr, "Set color to %d (reset x to %d)\n",
			 channel, scan_x );
	    break;

	case RSkipPixelsOp:
	    if ( LONGP(inst) )
	    {
		VAXSHORT( long_data, infile );
		scan_x += long_data;
		scanc += long_data;
		if ( debug_f )
		    fprintf( stderr, "Skip %d pixels (to %d)\n",
			    long_data, scan_x );
	    }
	    else
	    {
		scan_x += DATUM(inst);
		scanc += DATUM(inst);
		if ( debug_f )
		    fprintf( stderr, "Skip %d pixels (to %d)\n",
			    DATUM(inst), scan_x );
	    }
	    break;

	case RByteDataOp:
	    if ( LONGP(inst) )
	    {
		VAXSHORT( nc, infile );
	    }
	    else
		nc = DATUM(inst);
	    nc++;
	    if ( debug_f )
		if ( RLE_BIT( *the_hdr, channel ) )
		    fprintf( stderr, "Pixel data %d (to %d):", nc, scan_x+nc );
		else
		    fprintf( stderr, "Pixel data %d (to %d)\n", nc, scan_x+nc);
	    if ( RLE_BIT( *the_hdr, channel ) )
	    {
		/* Don't fill past end of scanline! */
		if ( scan_x + nc > max_x )
		{
		    ns = scan_x + nc - max_x - 1;
		    nc -= ns;
		}
		else
		    ns = 0;
		fread( (char *)scanc, 1, nc, infile );
		while ( ns-- > 0 )
		    (void)getc( infile );
		if ( nc & 1 )
		    (void)getc( infile );	/* throw away odd byte */
	    }
	    else
		if ( the_hdr->priv.get.is_seek )
		    fseek( infile, ((nc + 1) / 2) * 2, 1 );
		else
		{
		    register int ii;
		    for ( ii = ((nc + 1) / 2) * 2; ii > 0; ii-- )
			(void) getc( infile );	/* discard it */
		}

	    scanc += nc;
	    scan_x += nc;
	    if ( debug_f && RLE_BIT( *the_hdr, channel ) )
	    {
		rle_pixel * cp = scanc - nc;
		for ( ; nc > 0; nc-- )
		    fprintf( stderr, "%02x", *cp++ );
		putc( '\n', stderr );
	    }
	    break;

	case RRunDataOp:
	    if ( LONGP(inst) )
	    {
		VAXSHORT( nc, infile );
	    }
	    else
		nc = DATUM(inst);
	    nc++;
	    scan_x += nc;

	    VAXSHORT( word, infile );
	    if ( debug_f )
		fprintf( stderr, "Run length %d (to %d), data %02x\n",
			    nc, scan_x, word );
	    if ( RLE_BIT( *the_hdr, channel ) )
	    {
		if ( scan_x > max_x )
		{
		    ns = scan_x - max_x - 1;
		    nc -= ns;
		}
		else
		    ns = 0;
		if ( nc >= 10 )		/* break point for 785, anyway */
		{
		    bfill( (char *)scanc, nc, word );
		    scanc += nc;
		}
		else
		    for ( nc--; nc >= 0; nc--, scanc++ )
			*scanc = word;
	    }
	    break;

	case REOFOp:
	    the_hdr->priv.get.is_eof = 1;
	    if ( debug_f )
		fprintf( stderr, "End of Image\n" );
	    break;

	default:
	    fprintf( stderr,
		     "%s: rle_getrow: Unrecognized opcode: %d, reading %s\n",
		     the_hdr->cmd, inst[0], the_hdr->file_name );
	    exit(1);
	}
	if ( OPCODE(inst) == RSkipLinesOp || OPCODE(inst) == REOFOp )
	    break;			/* <--- the other loop exit */
    }

    /* If at end, skip the rest of a malformed image. */
    if ( the_hdr->priv.get.scan_y >= the_hdr->ymax )
    {
	int y = the_hdr->priv.get.scan_y;
	while ( rle_getskip( the_hdr ) != 32768 )
	    ;
	return y;
    }

    return the_hdr->priv.get.scan_y;
}


/* Fill buffer at s with n copies of character c.  N must be <= 65535*/
/* ARGSUSED */
static void bfill( s, n, c )
register char *s;
register int n, c;
{
#ifdef vax
    asm("   movc5   $0,*4(ap),12(ap),8(ap),*4(ap)");
#else
    while ( n-- > 0 )
	*s++ = c;
#endif
}
