/*
 *			S K E T C H . C
 *
 *  Program to generate test sketches
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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"

extern struct rt_sketch_internal *sketch_start();

main( argc, argv )
int argc;
char *argv[];
{
	struct rt_sketch_internal *skt;
	point_t V;
	vect_t u_vec, v_vec;
	struct curve *crv;
	struct carc_seg *csg;
	struct line_seg lsg[3];
	int vert_count=5;
	point2d_t verts[] = {
		{ 20, 10 },
		{ 10, 10 },
		{ 0, 0 },
		{ 50, 0 },
		{ 0, 50 }
	};
	int i;

	VSET( V, 10, 20, 30 );
	VSET( u_vec, 1, 0, 0 );
	VSET( v_vec, 0, 1, 0 );

	skt = (struct rt_sketch_internal *)bu_calloc( 1, sizeof( struct rt_sketch_internal ), "sketch" );
	VMOVE( skt->V, V );
	VMOVE( skt->u_vec, u_vec );
	VMOVE( skt->v_vec, v_vec );
	skt->vert_count = 5;
	skt->verts = (point2d_t *)bu_calloc( skt->vert_count, sizeof( point2d_t ), "verts" );
	for( i=0 ; i<skt->vert_count ; i++ )
		V2MOVE( skt->verts[i], verts[i] )

	/* curve is a full cirle */
	crv = &skt->skt_curve;
	crv->seg_count = 1;
	crv->segments = (genptr_t *)bu_calloc( 3, sizeof( genptr_t ), "segments" );
	crv->reverse = (int *)bu_calloc( 3, sizeof( int ), "reverse" );
	csg = (struct carc_seg *)bu_calloc( 1, sizeof( struct carc_seg ), "carc" );
	crv->segments[0] = (genptr_t)csg;
	csg->magic = CURVE_CARC_MAGIC;
	csg->start = 0;
	csg->end = 1;
	csg->radius = -1.0;
	csg->orientation = 0;

	mk_id( stdout, "sketch test" );
	mk_sketch( stdout, "test_sketch", skt );
}
