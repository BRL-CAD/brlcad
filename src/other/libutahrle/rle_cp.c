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
 * rle_cp.c - Copy the contents of one RLE image file to another.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Wed Jun 27 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "rle.h"
#include "rle_code.h"
#include "rle_put.h"

/* Read a two-byte "short" that started in VAX (LITTLE_ENDIAN) order */
#define VAXSHORT( var, fp )\
	{ var = fgetc(fp)&0xFF; var |= (fgetc(fp)) << 8; }
  
/* Instruction format -- first byte is opcode, second is datum. */

#define OPCODE(inst) (inst[0] & ~LONG)
#define LONGP(inst) (inst[0] & LONG)
#define DATUM(inst) (inst[1] & 0xff)	/* Make sure it's unsigned. */

/* Write a two-byte value in little_endian order. */
#define	put16(a)    (putc((a)&0xff,outfile),putc(((a)>>8)&0xff,outfile))

/*****************************************************************
 * TAG( rle_cp )
 * 
 * Copy the image described by in_hdr to that described by out_hdr
 * until an end-of-image is encountered.
 *
 * Replaces the fread/fwrite loop used that was before we were
 * concerned with concatenated images.
 * 
 * Inputs:
 * 	in_hdr:		Describes input image.
 * Outputs:
 * 	out_hdr:	Describes output image.
 * Assumptions:
 * 	rle_get_setup/rle_put_setup have been called.
 * 	in_hdr and out_hdr are compatible -- same number of channels,
 * 	same size, all relevant channel bits set.
 * 	The scanline most recently read from the input has been
 * 	written to the output.
 * Algorithm:
 * 	Minimal processing is done.  Each opcode is recognized to the
 * 	extent necessary to copy it and its data to the output.
 */
void
rle_cp( in_hdr, the_hdr )
rle_hdr *in_hdr;
rle_hdr *the_hdr;
{
    register FILE *infile = in_hdr->rle_file;
    register FILE *outfile = the_hdr->rle_file;
    char inst[2];
    short nc, buflen;
    char *buffer;

    /* Add in vertical skip from last scanline */
    if ( in_hdr->priv.get.vert_skip > 0 )
    {
	in_hdr->priv.get.scan_y += in_hdr->priv.get.vert_skip;
	if ( in_hdr->priv.get.vert_skip > 1 )
	    rle_skiprow( the_hdr, in_hdr->priv.get.vert_skip - 1 );
    }

    if ( in_hdr->priv.get.is_eof )
    {
	rle_puteof( the_hdr );
	return;
    }

    if ( the_hdr->priv.put.nblank > 0 )
    {
	SkipBlankLines( the_hdr->priv.put.nblank );
	the_hdr->priv.put.nblank = 0;
    }

    /* Allocate memory for reading byte data. */
    buflen = in_hdr->xmax - in_hdr->xmin + 2;
    buffer = (char *)malloc( buflen );

    /* Otherwise, read and write instructions until an EOF
     * instruction is encountered.
     */
    for (;;)
    {
        inst[0] = getc( infile );
	inst[1] = getc( infile );

	/* Don't 'put' the instruction until we know what it is. */
	if ( feof(infile) )
	{
	    in_hdr->priv.get.is_eof = 1;
	    rle_puteof( the_hdr );
	    break;		/* <--- one of the exits */
	}

	switch( OPCODE(inst) )
	{
	case RSkipLinesOp:
	    putc( inst[0], outfile );
	    putc( inst[1], outfile );
	    if ( LONGP(inst) )
	    {
		putc( getc( infile ), outfile );
		putc( getc( infile ), outfile );
	    }
	    break;			/* need to break for() here, too */

	case RSetColorOp:
	    putc( inst[0], outfile );
	    putc( inst[1], outfile );
	    break;

	case RSkipPixelsOp:
	    putc( inst[0], outfile );
	    putc( inst[1], outfile );
	    if ( LONGP(inst) )
	    {
		putc( getc( infile ), outfile );
		putc( getc( infile ), outfile );
	    }
	    break;

	case RByteDataOp:
	    putc( inst[0], outfile );
	    putc( inst[1], outfile );
	    if ( LONGP(inst) )
	    {
	        VAXSHORT( nc, infile );
		put16( nc );
	    }
	    else
		nc = DATUM(inst);
	    nc++;
	    nc = 2 * ((nc + 1) / 2);
	    /* Total paranoia.  nc should never be > buflen. */
	    while ( nc > buflen )
	    {
		fread( buffer, nc, 1, infile );
		fwrite( buffer, nc, 1, outfile );
		nc -= buflen;
	    }

	    fread( buffer, nc, 1, infile );
	    fwrite( buffer, nc, 1, outfile );
	    break;

	case RRunDataOp:
	    putc( inst[0], outfile );
	    putc( inst[1], outfile );
	    if ( LONGP(inst) )
	    {
		putc( getc( infile ), outfile );
		putc( getc( infile ), outfile );
	    }

	    putc( getc( infile ), outfile );
	    putc( getc( infile ), outfile );
	    break;

	case REOFOp:
	    in_hdr->priv.get.is_eof = 1;
	    rle_puteof( the_hdr );
	    break;

	default:
	    fprintf( stderr,
		     "%s: rle_cp: Unrecognized opcode: %d, reading %s\n",
		     the_hdr->cmd, OPCODE(inst), the_hdr->file_name );
	    fflush( the_hdr->rle_file );
	    exit(1);
	}
	if ( OPCODE(inst) == REOFOp )
	    break;			/* <--- the other loop exit */
    }

    /* Just in case the caller does something silly like calling rle_getrow. */
    in_hdr->priv.get.scan_y = in_hdr->ymax;
    in_hdr->priv.get.vert_skip = 0;

    return;
}
