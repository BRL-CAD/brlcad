/*
 *			S K T . C
 *
 * Support for sketches
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char skt_RCSid[] = "@(#)$Header$ (BRL)";
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


/* create a sketch object with no curves or vertices */
struct rt_sketch_internal *
sketch_start( V, u_vec, v_vec )
point_t V;
vect_t u_vec, v_vec;
{
	struct rt_sketch_internal *skt;

	skt = bu_malloc( sizeof( struct rt_sketch_internal ), "sketch" );

	skt->magic = RT_SKETCH_INTERNAL_MAGIC;
	VMOVE( skt->V, V );
	VMOVE( skt->u_vec, u_vec );
	VMOVE( skt->v_vec, v_vec );
	skt->vert_count = 0;
	skt->verts = (point2d_t *)NULL;
	skt->curve_count = 0;
	skt->curves = (struct curve *)NULL;

	return( skt );
}

/* add some vertices to the sketch object
 * note that the array of vertices is copied (caller is responsible for freeing it)
 */
void
sketch_add_verts( skt, vert_count, verts )
struct rt_sketch_internal *skt;
int vert_count;
point2d_t *verts;
{
	RT_SKETCH_CK_MAGIC( skt );

	if( skt->vert_count )
	{
		skt->verts = (point2d_t *)bu_realloc( skt->verts, (skt->vert_count + vert_count)*sizeof( point2d_t), "sketch verts" );
		bcopy( verts, &skt->verts[skt->vert_count],
			vert_count*sizeof( point2d_t) );
		skt->vert_count += vert_count;
	}
	else
	{
		skt->vert_count = vert_count;
		skt->verts = (point2d_t *)bu_calloc( vert_count, sizeof( point2d_t ), "sketch verts" );
		bcopy( verts, skt->verts, vert_count*sizeof( point2d_t ) );
	}
}

/* add a curve to a sketch
 * note that the curve is copied (caller is responsible for freeing it)
 */
void
sketch_add_curve( skt, crv )
struct rt_sketch_internal *skt;
struct curve *crv;
{
	RT_SKETCH_CK_MAGIC( skt );

	if( skt->curve_count )
	{
		skt->curves = (struct curve *)bu_realloc( skt->curves, (++skt->curve_count)*sizeof( struct curve ), "sketch curve " );
		rt_copy_curve( &skt->curves[skt->curve_count-1], crv );
	}
	else
	{
		skt->curves = (struct curve *)bu_malloc( sizeof( struct curve ), "sketch curve" );
		skt->curve_count = 1;
		rt_copy_curve( skt->curves, crv );
	}
}

int
mk_sketch( fp, name, skt )
FILE *fp;
char *name;
struct rt_sketch_internal *skt;
{
	struct rt_db_internal intern;
	int ret;

	RT_SKETCH_CK_MAGIC( skt );

	ret = mk_export_fwrite( fp, name, (genptr_t)skt, ID_SKETCH );

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_ptr = (genptr_t)skt;
	intern.idb_type = ID_SKETCH;

	rt_sketch_ifree( &intern );

	return( ret );
}
