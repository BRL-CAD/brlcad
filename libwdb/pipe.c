/*
 *			P I P E . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


extern int rt_pipe_ck( const struct bu_list *headp );


/*
 *			M K _ P A R T I C L E
 *
 *
 *  Returns -
 *	0	OK
 *	<0	failure
 */
int
mk_particle( fp, name, vertex, height, vradius, hradius )
struct rt_wdb	*fp;
const char	*name;
point_t	vertex;
vect_t	height;
double	vradius;
double	hradius;
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
mk_pipe( fp, name, headp )
struct rt_wdb		*fp;
const char			*name;
struct bu_list	*headp;
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
