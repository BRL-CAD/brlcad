/*                          B I T V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/

/** @file bitv.c
 *  Routines for managing bit vectors of arbitrary length.
 *
 *  The basic type "bitv_t" is defined in h/machine.h; it is the
 *  widest integer datatype for which efficient hardware support exists.
 *  BITV_SHIFT and BITV_MASK are also defined in machine.h
 *
 *  These bit vectors are "little endian", bit 0 is in the right hand
 *  side of the [0] word.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static const char libbu_bitv_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#  include <string.h>		/* for bzero() */
#else
#  include <strings.h>
#endif
#include <ctype.h>
#include "machine.h"
#include "bu.h"

/*
 *			B U _ B I T V _ N E W
 *
 *  Allocate storage for a new bit vector of at least 'nbits' in length.
 *  For efficiency, the bit vector itself is not initialized.
 */
struct bu_bitv *
bu_bitv_new(unsigned int nbits)
{
	struct bu_bitv	*bv;
	int		bv_bytes;
	int		total_bytes;

	bv_bytes = BU_BITS2BYTES(nbits);
	total_bytes = sizeof(struct bu_bitv) - 2*sizeof(bitv_t) + bv_bytes;

	bv = (struct bu_bitv *)bu_malloc( (size_t)total_bytes, "struct bu_bitv" );
	BU_LIST_INIT( &bv->l );
	bv->l.magic = BU_BITV_MAGIC;
	bv->nbits = bv_bytes * 8;
	return bv;
}

/*
 *			B U _ B I T V _ F R E E
 *
 *  Release all internal storage for this bit vector.
 *  It is the caller's responsibility to not use the pointer 'bv' any longer.
 *  It is the caller's responsibility to dequeue from any linked list first.
 */
void
bu_bitv_free(struct bu_bitv *bv)
{
	BU_CK_BITV(bv);

	bv->l.forw = bv->l.back = BU_LIST_NULL;	/* sanity */
	bu_free( (char *)bv, "struct bu_bitv" );
}

/*
 *			B U _ B I T V _ C L E A R
 *
 *  Set all the bits in the bit vector to zero.
 *
 *  Also available as a macro if you don't desire the pointer checking.
 */
void
bu_bitv_clear(struct bu_bitv *bv)
{
	BU_CK_BITV(bv);

	BU_BITV_ZEROALL(bv);
}

/*
 *			B U _ B I T V _ O R
 */
void
bu_bitv_or(struct bu_bitv *ov, const struct bu_bitv *iv)
{
	register bitv_t		*out;
	register const bitv_t	*in;
	register int		words;

	if( ov->nbits != iv->nbits )  bu_bomb("bu_bitv_or: length mis-match");
	out = ov->bits;
	in = iv->bits;
	words = BU_BITS2WORDS(iv->nbits);
#ifdef VECTORIZE
#	include "noalias.h"
	for( --words; words >= 0; words-- )
		out[words] |= in[words];
#else
	while( words-- > 0 )
		*out++ |= *in++;
#endif
}

/*
 *			B U _ B I T V _ A N D
 */
void
bu_bitv_and(struct bu_bitv *ov, const struct bu_bitv *iv)
{
	register bitv_t		*out;
	register const bitv_t	*in;
	register int		words;

	if( ov->nbits != iv->nbits )  bu_bomb("bu_bitv_and: length mis-match");
	out = ov->bits;
	in = iv->bits;
	words = BU_BITS2WORDS(iv->nbits);
#ifdef VECTORIZE
#	include "noalias.h"
	for( --words; words >= 0; words-- )
		out[words] &= in[words];
#else
	while( words-- > 0 )
		*out++ &= *in++;
#endif
}

/*
 *			B U _ B I T V _ V L S
 *
 *  Print the bits set in a bit vector.
 */
void
bu_bitv_vls(struct bu_vls *v, register const struct bu_bitv *bv)
{
	int		seen = 0;
	register int	i;
	int		len;

	BU_CK_VLS( v );
	BU_CK_BITV( bv );

	len = bv->nbits;

	bu_vls_strcat( v, "(" );

	/* Visit all the bits in ascending order */
	for( i=0; i<len; i++ )  {
		if( BU_BITTEST(bv, i) == 0 )  continue;
		if( seen )  bu_vls_strcat( v, ", " );
		bu_vls_printf( v, "%d", i );
		seen = 1;
	}
	bu_vls_strcat( v, ") " );
}

/*
 *			B U _ P R _ B I T V
 *
 *  Print the bits set in a bit vector.
 *  Use bu_vls stuff, to make only a single call to bu_log().
 */
void
bu_pr_bitv(const char *str, register const struct bu_bitv *bv)
{
	struct bu_vls	v;

	BU_CK_BITV(bv)
	bu_vls_init( &v );
	bu_vls_strcat( &v, str );
	bu_vls_strcat( &v, ": " );
	bu_bitv_vls( &v, bv );
	bu_log("%s", bu_vls_addr( &v ) );
	bu_vls_free( &v );
}

/*
 *			B U _ B I T V _ T O _ H E X
 *
 *	Convert a bit vector to an ascii string of hex digits.
 *	The string is from MSB to LSB (bytes and bits).
 */
void
bu_bitv_to_hex(struct bu_vls *v, register const struct bu_bitv *bv)
{
	unsigned int word_count, byte_no;

	BU_CK_VLS( v );
	BU_CK_BITV( bv );

	word_count = bv->nbits/8/sizeof( bitv_t );
	byte_no = sizeof( bitv_t );

	bu_vls_extend( v, word_count * sizeof( bitv_t ) * 2 + 1 );
	while( word_count-- )
	{
		while( byte_no-- )
		{
			bu_vls_printf( v, "%02lx",
				       ((bv->bits[word_count] & (((bitv_t)0xff)<<(byte_no*8))) >> (byte_no*8)) & (bitv_t)0xff );
		}
		byte_no = sizeof( bitv_t );
	}
}

/*
 *			B U _ H E X _ T O _ B I T V
 *
 *	Convert a string of HEX digits (as produces by bu_bitv_to_hex) into a bit vector.
 */
struct bu_bitv *
bu_hex_to_bitv(const char *str)
{
	char abyte[3];
	const char *str_start;
	unsigned int len=0;
	int bytes;
	struct bu_bitv *bv;
	unsigned long c;
	int word_count, byte_no;

	abyte[2] = '\0';

	/* skip over any initial white space */
	while( isspace( *str ) )
		str++;

	str_start = str;
	/* count hex digits */
	while( isxdigit( *str ) )
		str++;

	len = str - str_start;

	if( len < 2 || len%2 )
	{
		/* Must be two digits per byte */
		bu_log( "bu_hex_to_bitv: illegal hex bitv (%s)\n", str_start );
		return( (struct bu_bitv *)NULL );
	}

	bytes = len / 2; /* two hex digits per byte */
	bv = bu_bitv_new( len * 4 ); /* 4 bits per hex digit */
	bu_bitv_clear( bv );
	word_count = bytes/sizeof( bitv_t );
	byte_no = bytes % sizeof( bitv_t );
	if( !byte_no )
		byte_no = sizeof( bitv_t );
	else
		word_count++;

	str = str_start;
	while( word_count-- )
	{
		while( byte_no-- )
		{
			/* get next two hex digits from string */
			abyte[0] = *str++;
			abyte[1] = *str++;

			/* convert into an unsigned long */
			c = strtoul( abyte, (char **)NULL, 16 );

			/* set the appropriate bits in the bit vector */
			bv->bits[word_count] |= (bitv_t)c<<(byte_no*8);
		}
		byte_no = sizeof( bitv_t );
	}

	return( bv );
}

/*
 *			B U _ B I T V _ D U P
 *
 *	Make a copy of a bit vector
 */
struct bu_bitv *
bu_bitv_dup(register const struct bu_bitv *bv)
{
	struct bu_bitv *bv2;

	bv2 = bu_bitv_new( bv->nbits );
	bu_bitv_clear( bv2 );
	bu_bitv_or( bv2, bv );

	return( bv2 );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
