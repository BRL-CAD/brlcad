/*                       S T O R A G E . C
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

/** \addtogroup librt */
/*@{*/
/** @file storage.c
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *	rt_get_seg	Invoked by GET_SEG() macro
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
static const char RCSstorage[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "bu.h"
#include "./debug.h"

/*
 *  			R T _ G E T _ S E G
 *
 *  This routine is called by the GET_SEG macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the seg resource must already be locked.
 *  malloc() locking is done in bu_malloc.
 */
void
rt_get_seg(register struct resource *res)
{
	register struct seg	*sp;
	register int		bytes;

	RT_RESOURCE_CHECK(res);

	if( BU_LIST_UNINITIALIZED( &res->re_seg ) )  {
		BU_LIST_INIT( &(res->re_seg) );
		bu_ptbl_init( &res->re_seg_blocks, 64, "re_seg_blocks ptbl" );
	}
	bytes = bu_malloc_len_roundup(64*sizeof(struct seg));
	sp = (struct seg *)bu_malloc(bytes, "rt_get_seg()");
	bu_ptbl_ins( &res->re_seg_blocks, (long *)sp );
	while( bytes >= sizeof(struct seg) )  {
		sp->l.magic = RT_SEG_MAGIC;
		BU_LIST_INSERT(&(res->re_seg), &(sp->l));
		res->re_seglen++;
		sp++;
		bytes -= sizeof(struct seg);
	}
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
