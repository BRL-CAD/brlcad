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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char part_RCSid[] = "@(#)$Header$ (BRL)";
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
FILE	*fp;
char	*name;
point_t	vertex;
vect_t	height;
double	vradius;
double	hradius;
{
	struct rt_part_internal	part;

	part.part_magic = RT_PART_INTERNAL_MAGIC;
	VMOVE( part.part_V, vertex );
	VMOVE( part.part_H, height );
	part.part_vrad = vradius;
	part.part_hrad = hradius;
	part.part_type = 0;		/* sanity, unused */

	return mk_export_fwrite( fp, name, (genptr_t)&part, ID_PARTICLE );
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
FILE			*fp;
char			*name;
struct wdb_pipept	*headp;
{
	struct rt_pipe_internal		pipe;
	int				ret;

	if( rt_pipe_ck( headp ) )
	{
		rt_log( "mk_pipe: BAD PIPE SOLID (%s)\n" , name );
		return( 1 );
	}

	pipe.pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	RT_LIST_INIT( &pipe.pipe_segs_head );
	/* "borrow" linked list from caller */
	RT_LIST_APPEND_LIST( &pipe.pipe_segs_head, &headp->l );

	ret = mk_export_fwrite( fp, name, (genptr_t)&pipe, ID_PIPE );

	/* "return" linked list to caller */
	RT_LIST_APPEND_LIST( &headp->l, &pipe.pipe_segs_head );
	return ret;
}

/*
 *			M K _ P I P E _ F R E E
 *
 *  Release the storage from a list of pipe segments.
 *  The head is left in initialized state (ie, forward & back point to head).
 */
void
mk_pipe_free( headp )
register struct wdb_pipept	*headp;
{
	register struct wdb_pipept	*wp;

	while( RT_LIST_WHILE( wp, wdb_pipept, &headp->l ) )  {
		RT_LIST_DEQUEUE( &wp->l );
		rt_free( (char *)wp, "mk_pipe_free" );
	}
}
