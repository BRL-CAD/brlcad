/*                          P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
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
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

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
    struct rt_pipe_internal *pipep;

    if ( rt_pipe_ck( headp ) )
    {
	bu_log( "mk_pipe: BAD PIPE SOLID (%s)\n", name );
	return 1;
    }

    BU_GETSTRUCT( pipep, rt_pipe_internal );
    pipep->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT( &pipep->pipe_segs_head );
    /* linked list from caller */
    BU_LIST_APPEND_LIST( &pipep->pipe_segs_head, headp );

    return wdb_export( fp, name, (genptr_t)pipep, ID_PIPE, mk_conv2mm );
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
    struct wdb_pipept	*wp;

    while ( BU_LIST_WHILE( wp, wdb_pipept, headp ) )  {
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
