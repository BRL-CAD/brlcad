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
 * rle_getskip.c - Skip scanlines on input.
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Wed Jun 27 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "rle.h"
#include "rle_code.h"

/* Read a two-byte "short" that started in VAX (LITTLE_ENDIAN) order */
#define VAXSHORT( var, fp )\
	{ var = fgetc(fp)&0xFF; var |= (fgetc(fp)) << 8; }

/* Instruction format -- first byte is opcode, second is datum. */

#define OPCODE(inst) (inst[0] & ~LONG)
#define LONGP(inst) (inst[0] & LONG)
#define DATUM(inst) (inst[1] & 0xff)	/* Make sure it's unsigned. */

/*****************************************************************
 * TAG( rle_getskip )
 *
 * Skip the next scanline with data on it.
 * Most useful for skipping to end-of-image.
 * Inputs:
 * 	the_hdr:	Describes input image.
 * Outputs:
 * 	Returns the number of the next scanline.  At EOF returns 32768.
 * Assumptions:
 * 	rle_get_setup has been called.
 * Algorithm:
 * 	Read input to the beginning of the next scanline, or to EOF or
 * 	end of image.
 */
unsigned int
rle_getskip( the_hdr )
rle_hdr *the_hdr;
{
    unsigned char inst[2];
    register FILE *infile = the_hdr->rle_file;
    int nc;

    /* Add in vertical skip from last scanline */
    if ( the_hdr->priv.get.vert_skip > 0)
	the_hdr->priv.get.scan_y += the_hdr->priv.get.vert_skip;
    the_hdr->priv.get.vert_skip = 0;

    if ( the_hdr->priv.get.is_eof )
	return 32768;		/* too big for 16 bits, signal EOF */

    /* Otherwise, read and interpret instructions until a skipLines
     * instruction is encountered.
     */
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
	    break;			/* need to break for() here, too */

	case RSetColorOp:
	    /* No-op here. */
	    break;

	case RSkipPixelsOp:
	    if ( LONGP(inst) )
	    {
		(void)getc( infile );
		(void)getc( infile );
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
	    if ( the_hdr->priv.get.is_seek )
		fseek( infile, ((nc + 1) / 2) * 2, 1 );
	    else
	    {
		register int ii;
		for ( ii = ((nc + 1) / 2) * 2; ii > 0; ii-- )
		    (void) getc( infile );	/* discard it */
	    }

	    break;

	case RRunDataOp:
	    if ( LONGP(inst) )
	    {
		(void)getc( infile );
		(void)getc( infile );
	    }
	    (void)getc( infile );
	    (void)getc( infile );
	    break;

	case REOFOp:
	    the_hdr->priv.get.is_eof = 1;
	    break;

	default:
	    fprintf( stderr,
		     "%s: rle_getskip: Unrecognized opcode: %d, reading %s\n",
		     the_hdr->cmd, OPCODE(inst), the_hdr->file_name );
	    exit(1);
	}
	if ( OPCODE(inst) == REOFOp )
	    break;			/* <--- the other loop exit */
	if ( OPCODE(inst) == RSkipLinesOp )
	    break;
    }

    /* Return the number of the NEXT scanline. */
    the_hdr->priv.get.scan_y +=
	the_hdr->priv.get.vert_skip;
    the_hdr->priv.get.vert_skip = 0;

    if ( the_hdr->priv.get.is_eof )
	return 32768;		/* too big for 16 bits, signal EOF */
    else
	return the_hdr->priv.get.scan_y;
}
