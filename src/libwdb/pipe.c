/*                          P I P E . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file pipe.c
 *
 *  Support for particles and pipes.
 *  Library for writing geometry databases from arbitrary procedures.
 *
 *  Note that routines which are passed point_t or vect_t or mat_t
 *  parameters (which are call-by-address) must be VERY careful to
 *  leave those parameters unmodified (eg, by scaling), so that the
 *  calling routine is not surprised.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	Michael John Muuss
 *	Susanne L. Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
#ifndef lint
static const char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


/*
 *			M K _ P A R T I C L E
 *
 *
 *  Returns -
 *	0	OK
 *	<0	failure
 */
int
mk_particle(struct rt_wdb *fp, const char *name, fastf_t *vertex, fastf_t *height, double vradius, double hradius)
{
	struct rt_part_internal	*part;

	BU_GETSTRUCT( part, rt_part_internal );
	part->part_magic = RT_PART_INTERNAL_MAGIC;
	VMOVE( part->part_V, vertex );
	VMOVE( part->part_H, height );
	part->part_vrad = vradius;
	part->part_hrad = hradius;
	part->part_type = 0;		/* sanity, unused */

	return wdb_export( fp, name, (genptr_t)part, ID_PARTICLE, mk_conv2mm );
}


/*
 *			M K _ P I P E
 *
 *  Note that the linked list of pipe segments headed by 'headp'
 *  must be freed by the caller.  mk_pipe_free() can be used.
 *
 *  Returns -
 *	0	OK
 *	<0	failure
 */
int
mk_pipe(struct rt_wdb *fp, const char *name, struct bu_list *headp)
{
	struct rt_pipe_internal		*pipe;
	int				ret;

	if( rt_pipe_ck( headp ) )
	{
		bu_log( "mk_pipe: BAD PIPE SOLID (%s)\n" , name );
		return( 1 );
	}

	BU_GETSTRUCT( pipe, rt_pipe_internal );
	pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	BU_LIST_INIT( &pipe->pipe_segs_head );
	/* "borrow" linked list from caller */
	BU_LIST_APPEND_LIST( &pipe->pipe_segs_head, headp );

	ret = wdb_export( fp, name, (genptr_t)pipe, ID_PIPE, mk_conv2mm );

	/* "return" linked list to caller */
	BU_LIST_APPEND_LIST( headp, &pipe->pipe_segs_head );
	return ret;
}

/*
 *			M K _ P I P E _ F R E E
 *
 *  Release the storage from a list of pipe segments.
 *  The head is left in initialized state (ie, forward & back point to head).
 */
void
mk_pipe_free( struct bu_list *headp )
{
	register struct wdb_pipept	*wp;

	while( BU_LIST_WHILE( wp, wdb_pipept, headp ) )  {
		BU_LIST_DEQUEUE( &wp->l );
		bu_free( (char *)wp, "mk_pipe_free" );
	}
}


/*
 *		M K _ A D D _ P I P E _ P T
 *
 *	Add another pipe segment to the linked list of pipe segents
 *
 */
void
mk_add_pipe_pt(
	struct bu_list *headp,
	const point_t coord,
	double od,
	double id,
	double bendradius )
{
	struct wdb_pipept *new;

	BU_CKMAG( headp, WDB_PIPESEG_MAGIC, "pipe point" )

	BU_GETSTRUCT( new, wdb_pipept );
	new->l.magic = WDB_PIPESEG_MAGIC;
	new->pp_od = od;
	new->pp_id = id;
	new->pp_bendradius = bendradius;
	VMOVE( new->pp_coord, coord );
	BU_LIST_INSERT( headp, &new->l );
}

/*
 *	M K _ P I P E _ I N I T
 *
 *	initialize a linked list of pipe segments with the first segment
 */
void
mk_pipe_init( struct bu_list *headp )
{
	BU_LIST_INIT( headp );
	headp->magic = WDB_PIPESEG_MAGIC;
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
