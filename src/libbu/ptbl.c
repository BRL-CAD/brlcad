/*                          P T B L . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

/** @file ptbl.c
 *  Support for generalized "pointer tables",
 *  kept compactly in a dynamic array.
 *
 *  The table is currently un-ordered, and is merely a array of pointers.
 *  The support routine nmg_tbl manipulates the array for you.
 *  Pointers to be operated on (inserted, deleted,
 *  searched for) are passed as a "pointer to long".
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static const char libbu_ptbl_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdio.h>
#include "machine.h"
#include "bu.h"
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

/*
 *			B U _ P T B L _ I N I T
 *
 *  Initialize struct & get storage for table
 */
void
bu_ptbl_init(struct bu_ptbl *b, int len, const char *str)

   		    		/* initial len.  Recommend 8 or 64 */

{
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_init(%8x, len=%d, %s)\n", b, len, str);
	BU_LIST_INIT(&b->l);
	b->l.magic = BU_PTBL_MAGIC;
	if( len <= 0 )  len = 64;
	b->blen = len;
	b->buffer = (long **)bu_calloc(b->blen, sizeof(long *), str);
	b->end = 0;
}

/*
 *			B U _ P T B L _ R E S E T
 *
 *  Reset the table to have no elements, but retain any existing storage.
 */
void
bu_ptbl_reset(struct bu_ptbl *b)
{
	BU_CK_PTBL(b);
	b->end = 0;
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_reset(%8x)\n", b);
	memset( (char *)b->buffer, 0, b->blen*sizeof(long *) );	/* no peeking */
}

/*
 *			B U _ P T B L _ I N S
 *
 *  Append a (long *) item to the table.
 *  Called "insert", for unknown reasons.
 */
int
bu_ptbl_ins(struct bu_ptbl *b, long int *p)
{
	register int i;

	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_ins(%8x, %8x)\n", b, p);

	BU_CK_PTBL(b);

	if (b->blen == 0) bu_ptbl_init(b, 8, "bu_ptbl_ins() buffer");
	if (b->end >= b->blen)  {
		b->buffer = (long **)bu_realloc( (char *)b->buffer,
		    sizeof(p)*(b->blen *= 4),
		    "bu_ptbl.buffer[] (ins)" );
	}

	b->buffer[i=b->end++] = p;
	return(i);
}

/*
 *			B U _ P T B L _ L O C A T E
 *
 *  locate a (long *) in an existing table
 *
 *  Returns -
 *	index of first matching element in array, if found
 *	-1	if not found
 *
 * We do this a great deal, so make it go as fast as possible.
 * this is the biggest argument I can make for changing to an
 * ordered list.  Someday....
 */
int
bu_ptbl_locate(const struct bu_ptbl *b, const long int *p)
{
	register int		k;
	register const long	**pp;

	BU_CK_PTBL(b);
	pp = (const long **)b->buffer;
#	include "noalias.h"
	for( k = b->end-1; k >= 0; k-- )
		if (pp[k] == p) return(k);

	return(-1);
}

/*
 *			B U _ P T B L _ Z E R O
 *
 *  Set all occurrences of "p" in the table to zero.
 *  This is different than deleting them.
 */
void
bu_ptbl_zero(struct bu_ptbl *b, const long int *p)
{
	register int		k;
	register const long	**pp;

	BU_CK_PTBL(b);
	pp = (const long **)b->buffer;
#	include "noalias.h"
	for( k = b->end-1; k >= 0; k-- )
		if (pp[k] == p) pp[k] = (long *)0;
}

/*
 *			B U _ P T B L _ I N S _ U N I Q U E
 *
 *  Append item to table, if not already present.  Unique insert.
 *
 *  Returns -
 *	index of first matchine element in array, if found.  (table unchanged)
 *	-1	if table extended to hold new element
 *
 * We do this a great deal, so make it go as fast as possible.
 * this is the biggest argument I can make for changing to an
 * ordered list.  Someday....
 */
int
bu_ptbl_ins_unique(struct bu_ptbl *b, long int *p)
{
	register int	k;
	register long	**pp = b->buffer;

	BU_CK_PTBL(b);

#	include "noalias.h"

	for( k = b->end-1; k >= 0; k-- )
		if (pp[k] == p) return(k);

	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_ins_unique(%8x, %8x)\n", b, p);

	if (b->blen <= 0 || b->end >= b->blen)  {
		/* Table needs to grow */
		bu_ptbl_ins( b, p );
		return -1;	/* To signal that it was added */
	}

	b->buffer[k=b->end++] = p;
	return(-1);		/* To signal that it was added */
}

/*
 *			B U _ P T B L _ R M
 *
 *  Remove all occurrences of an item from a table
 *
 *  Returns -
 *	Number of copies of 'p' that were removed from the table.
 *	0 if none found.
 *
 * we go backwards down the table looking for occurrences
 * of p to delete.  We do it backwards to reduce the amount
 * of data moved when there is more than one occurrence of p
 * in the table.  A pittance savings, unless you're doing a
 * lot of it.
 */
int
bu_ptbl_rm(struct bu_ptbl *b, const long int *p)
{
	register int end = b->end, j, k, l;
	register long **pp = b->buffer;
	int	ndel = 0;

	BU_CK_PTBL(b);
	for (l = b->end-1 ; l >= 0 ; --l)  {
		if (pp[l] == p){
			/* delete consecutive occurrence(s) of p */
			ndel++;

			j=l+1;
			while (l >= 1 && pp[l-1] == p) --l, ndel++;
			/* pp[l] through pp[j-1] match p */

			end -= j - l;
#			include "noalias.h"
			for(k=l ; j < b->end ;)
				b->buffer[k++] = b->buffer[j++];
			b->end = end;
		}
	}
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_rm(%8x, %8x) ndel=%d\n", b, p, ndel);
	return ndel;
}

/*
 *			B U _ P T B L _ C A T
 *
 *  Catenate one table onto end of another.
 *  There is no checking for duplication.
 */
void
bu_ptbl_cat(struct bu_ptbl *dest, const struct bu_ptbl *src)
{
	BU_CK_PTBL(dest);
	BU_CK_PTBL(src);
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_cat(%8x, %8x)\n", dest, src);

	if ((dest->blen - dest->end) < src->end) {
		dest->blen = (dest->blen + src->end) * 2 + 8;
		dest->buffer = (long **)bu_realloc( (char *)dest->buffer,
			dest->blen * sizeof(long *),
			"bu_ptbl.buffer[] (cat)");
	}
	bcopy( (char *)src->buffer, (char *)&dest->buffer[dest->end],
		src->end*sizeof(long *));
	dest->end += src->end;
}

/*
 *			B U _ P T B L _ C A T _ U N I Q
 *
 *  Catenate one table onto end of another,
 *  ensuring that no entry is duplicated.
 *  Duplications between multiple items in 'src' are not caught.
 *  The search is a nasty n**2 one.  The tables are expected to be short.
 */
void
bu_ptbl_cat_uniq(struct bu_ptbl *dest, const struct bu_ptbl *src)
{
	register long	**p;

	BU_CK_PTBL(dest);
	BU_CK_PTBL(src);
	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_cat_uniq(%8x, %8x)\n", dest, src);

	/* Assume the worst, ensure sufficient space to add all 'src' items */
	if ((dest->blen - dest->end) < src->end) {
		dest->buffer = (long **)bu_realloc( (char *)dest->buffer,
			sizeof(long *)*(dest->blen += src->blen + 8),
			"bu_ptbl.buffer[] (cat_uniq)");
	}
	for( BU_PTBL_FOR( p, (long **), src ) )  {
		bu_ptbl_ins_unique( dest, *p );
	}
}

/*
 *			B U _ P T B L _ F R E E
 *
 *  Deallocate dynamic buffer associated with a table,
 *  and render this table unusable without a subsequent bu_ptbl_init().
 */
void
bu_ptbl_free(struct bu_ptbl *b)
{
	BU_CK_PTBL(b);

	bu_free((genptr_t)b->buffer, "bu_ptbl.buffer[]");
	memset((char *)b, 0, sizeof(struct bu_ptbl));	/* sanity */

	if (bu_debug & BU_DEBUG_PTBL)
		bu_log("bu_ptbl_free(%8x)\n", b);
}



/*
 *			B U _ P T B L
 *
 *  This version maintained for source compatibility with existing NMG code.
 */
int
bu_ptbl(struct bu_ptbl *b, int func, long int *p)
{
	if (func == BU_PTBL_INIT) {
		bu_ptbl_init(b, 64, "bu_ptbl() buffer[]");
		return 0;
	} else if (func == BU_PTBL_RST) {
		bu_ptbl_reset(b);
		return 0;
	} else if (func == BU_PTBL_INS) {
		return bu_ptbl_ins(b, p);
	} else if (func == BU_PTBL_LOC) {
		return bu_ptbl_locate(b, p);
	} else if( func == BU_PTBL_ZERO ) {
		bu_ptbl_zero(b, p);
		return( 0 );
	} else if (func == BU_PTBL_INS_UNIQUE) {
		return bu_ptbl_ins_unique(b, p);
	} else if (func == BU_PTBL_RM) {
		return bu_ptbl_rm(b, p);
	} else if (func == BU_PTBL_CAT) {
		bu_ptbl_cat( b, (const struct bu_ptbl *)p );
		return(0);
	} else if (func == BU_PTBL_FREE) {
		bu_ptbl_free(b);
		return (0);
	} else {
		BU_CK_PTBL(b);
		bu_log("bu_ptbl(%8x) Unknown table function %d\n", b, func);
		bu_bomb("bu_ptbl");
	}
	return(-1);/* this is here to keep lint happy */
}

/*
 *			B U _ P R _ P T B L
 *
 *  Print a bu_ptbl array for inspection.
 */
void
bu_pr_ptbl(const char *title, const struct bu_ptbl *tbl, int verbose)
{
	register long	**lp;

	BU_CK_PTBL(tbl);
	bu_log("%s: bu_ptbl array with %d entries\n",
		title, tbl->end );

	if( !verbose )  return;

	/* Go in ascending order */
	for( lp = (long **)BU_PTBL_BASEADDR(tbl);
	     lp <= (long **)BU_PTBL_LASTADDR(tbl); lp++
	)  {
		if( *lp == 0 )  {
			bu_log("  %.8x NULL entry\n", *lp);
			continue;
		}
		bu_log("  %.8x %s\n", *lp, bu_identify_magic(**lp) );
	}
}

/*			B U _ P T B L _ T R U N C
 *
 *	truncate a bu_ptbl
 */
void
bu_ptbl_trunc(struct bu_ptbl *tbl, int end)
{
	BU_CK_PTBL(tbl);

	if( tbl->end <= end )
		return;

	tbl->end = end;
	return;
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
