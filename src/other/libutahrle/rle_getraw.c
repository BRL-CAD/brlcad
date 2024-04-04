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
 * rle_getraw.c -
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Nov 10 1986
 * Copyright (c) 1986, University of Utah
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

/* Read a two-byte "short" that started in VAX (LITTLE_ENDIAN) order */
#define VAXSHORT( var, fp )\
	{ var = fgetc(fp)&0xFF; var |= (fgetc(fp)) << 8; }

/* Instruction format -- first byte is opcode, second is datum. */

#define OPCODE(inst) (inst[0] & ~LONG)
#define LONGP(inst) (inst[0] & LONG)
#define DATUM(inst) (inst[1] & 0xff)	/* Make sure it's unsigned. */

/*****************************************************************
 * TAG( rle_getraw )
 *
 * Get a raw scanline from the input file.
 * Inputs:
 *	the_hdr:    rle_hdr structure containing information about
 *		    the input file.
 * Outputs:
 * 	scanraw:    an array of pointers to the individual color
 *		    scanlines.  Scanraw is assumed to have
 *		    the_hdr->ncolors pointers to arrays of rle_op,
 *		    each of which with enough elements, at least
 *		    1 + (the_hdr->xmax - the_hdr->xmin) / 3.
 *	nraw:	    an array of integers giving the number of rle_ops for
 *		    each color channel.
 *	Returns the current scanline number.  Returns 32768 at EOF.
 * Assumptions:
 * 	rle_get_setup has already been called.
 * Algorithm:
 *	Read input until a vertical skip is encountered,
 *	decoding the instructions into the scanraw array.
 *	Vertical skips that separate scanlines with no data do not
 *	cause a return.  In other words, the only reason for returning
 *	with an empty scanline is end of file.
 *
 *	When the scan_y reaches or exceeds the ymax, the rest of the
 *	input image is skipped.  This avoids problems with malformed
 *	input files.
 */
unsigned int
rle_getraw( the_hdr, scanraw, nraw )
rle_hdr *the_hdr;
rle_op *scanraw[];
int nraw[];
{
    register int channel;
    register rle_op * rawp = NULL;
    FILE *infile = the_hdr->rle_file;
    char inst[2];
    int scan_x = the_hdr->xmin;
    register int was_data;
    short word, long_data, nc, been_some = 0;

    /* Add in vertical skip from last scanline */
    if ( the_hdr->priv.get.vert_skip > 0 )
	the_hdr->priv.get.scan_y += the_hdr->priv.get.vert_skip;

    /* Set run lengths to 0 */
    for ( channel = (the_hdr->alpha ? -1 : 0);
	  channel < the_hdr->ncolors;
	  channel++ )
	if ( RLE_BIT( *the_hdr, channel ) )
	     nraw[channel] = 0;
    channel = 0;

    if ( the_hdr->priv.get.is_eof )
	return 32768;		/* too big for 16 bits, signal EOF */

    /* Otherwise, read and interpret instructions until a skipLines
     * instruction is encountered.
     */
    for (was_data = 0;;)
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
	    was_data = 1;
	    if ( LONGP(inst) )
	    {
		VAXSHORT( the_hdr->priv.get.vert_skip, infile );
	    }
	    else
		the_hdr->priv.get.vert_skip = DATUM(inst);
	    break;			/* need to break for() here, too */

	case RSetColorOp:
	    was_data = 1;
	    channel = DATUM(inst);	/* select color channel */
	    if ( channel == 255 )
		channel = -1;
	    scan_x = the_hdr->xmin;
	    if ( RLE_BIT( *the_hdr, channel ) )
		rawp = scanraw[channel];
	    else
		rawp = NULL;
	    break;

	case RSkipPixelsOp:
	    was_data = 1;
	    if ( LONGP(inst) )
	    {
	        VAXSHORT( long_data, infile );
		scan_x += long_data;
	    }
	    else
	    {
		scan_x += DATUM(inst);
	    }
	    break;

	case RByteDataOp:
	    was_data = 1;
	    if ( LONGP(inst) )
	    {
	        VAXSHORT( nc, infile );
	    }
	    else
		nc = DATUM(inst);
	    nc++;
	    if ( rawp != NULL )
	    {
		rawp->opcode = RByteDataOp;
		rawp->xloc = scan_x;
		rawp->length = nc;
		rawp->u.pixels = (rle_pixel *)malloc( (unsigned)nc );
		fread( (char *)rawp->u.pixels, 1, nc, infile );
		if ( nc & 1 )
		    (void)getc( infile );	/* throw away odd byte */
		rawp++;
		nraw[channel]++;
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

	    scan_x += nc;
	    been_some = 1;
	    break;

	case RRunDataOp:
	    was_data = 1;
	    if ( LONGP(inst) )
	    {
	        VAXSHORT( nc, infile );
	    }
	    else
		nc = DATUM(inst);

	    nc++;
	    VAXSHORT( word, infile );
	    if ( rawp != NULL )
	    {
		rawp->opcode = RRunDataOp;
		rawp->xloc = scan_x;
		rawp->length = nc;
		rawp->u.run_val = word;
		rawp++;
		nraw[channel]++;
	    }
	    scan_x += nc;
	    been_some = 1;
	    break;

	case REOFOp:
	    the_hdr->priv.get.is_eof = 1;
	    break;

	default:
	    fprintf( stderr,
		     "%s: rle_getraw: Unrecognized opcode: %d, reading %s\n",
		     the_hdr->cmd, OPCODE(inst), the_hdr->file_name );
	    exit(1);
	}
	if ( OPCODE(inst) == REOFOp )
	    break;			/* <--- the other loop exit */
	if ( OPCODE(inst) == RSkipLinesOp )
	{
	    if ( been_some )
		break;			/* <--- the other loop exit */
	    else
		/* No data on that scanline, so move up to this scanline */
		the_hdr->priv.get.scan_y +=
		    the_hdr->priv.get.vert_skip;
	}
    }

    /* If at top of image, skip any remaining. */
    if ( the_hdr->priv.get.scan_y >= the_hdr->ymax )
    {
	int y = the_hdr->priv.get.scan_y;
	while ( rle_getskip( the_hdr ) != 32768 )
	    ;
	return y;
    }

    /* Return current Y value */
    return (was_data == 0) ? 32768 : the_hdr->priv.get.scan_y;
}

/*****************************************************************
 * TAG( rle_freeraw )
 *
 * Free all the pixel arrays in the raw scan struct.
 * Inputs:
 *  	the_hdr:    Header struct corresponding to this RLE data.
 *  	scanraw:    Array of pointers to array of rle_op, as above.
 * 	nraw:	    Array of lengths (as above)
 * Outputs:
 * 	Frees the areas pointed to by the pixels elements of any
 *  	RByteDataOp type rle_op structs.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
rle_freeraw( the_hdr, scanraw, nraw )
rle_hdr * the_hdr;
int nraw[];
rle_op *scanraw[] ;
{
    int c, i;
    register rle_op * raw_p;

    for ( c = -the_hdr->alpha; c < the_hdr->ncolors; c++ )
	if ( RLE_BIT( *the_hdr, c ) )
	    for ( i = nraw[c], raw_p = scanraw[c]; i > 0; i--, raw_p++ )
		if ( raw_p->opcode == RByteDataOp )
		{
		    if ( raw_p->u.pixels )
			free( raw_p->u.pixels );
		    else
			fprintf( stderr,
	 "%s(%s): rle_freeraw given NULL pixel pointer, %d[%d].\n",
				 the_hdr->cmd, the_hdr->file_name,
				 c, nraw[c] - i );
		    raw_p->u.pixels = NULL;
		}
}
