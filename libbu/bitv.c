/*
 *			B I T V . C
 *
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"

/*
 *			B U _ B I T V _ N E W
 *
 *  Allocate storage for a new bit vector of at least 'nbits' in length.
 *  For efficiency, the bit vector itself is not initialized.
 */
struct bu_bitv *
bu_bitv_new(nbits)
int	nbits;
{
	struct bu_bitv	*bv;
	int		bv_bytes;
	int		total_bytes;

	bv_bytes = BU_BITS2BYTES(nbits);
	total_bytes = sizeof(struct bu_bitv) - 2*sizeof(bitv_t) + bv_bytes;

	bv = (struct bu_bitv *)bu_malloc( total_bytes, "struct bu_bitv" );
	RT_LIST_INIT( &bv->l );
	bv->l.magic = BU_BITV_MAGIC;
	bv->nbits = bv_bytes * 8;
	return bv;
}

/*
 *			B U _ B I T V _ C L E A R
 *
 *  Set all the bits in the bit vector to zero.
 */
void
bu_bitv_clear(bv)
struct bu_bitv	*bv;
{
	BU_CK_BITV(bv);

	bzero( (char *)bv->bits, bv->nbits / 8 );	/* 8 bits/byte */
}

/*
 *			B U _ B I T V _ O R
 */
void
bu_bitv_or( ov, iv )
struct bu_bitv		*ov;
CONST struct bu_bitv	*iv;
{
	register bitv_t		*out;
	register CONST bitv_t	*in;
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
bu_bitv_and( ov, iv )
struct bu_bitv		*ov;
CONST struct bu_bitv	*iv;
{
	register bitv_t		*out;
	register CONST bitv_t	*in;
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
bu_bitv_vls( v, bv )
struct bu_vls			*v;
register CONST struct bu_bitv	*bv;
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
bu_pr_bitv( str, bv )
CONST char			*str;
register CONST struct bu_bitv	*bv;
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
