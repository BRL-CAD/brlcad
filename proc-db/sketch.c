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
static char RCSid[] = "@(#)$Header$ (BRL)";
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
#include "rtlist.h"
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
	struct curve crv;
	struct carc_seg csg;
	struct line_seg lsg[3];
	int vert_count=5;
	point2d_t verts[] = {
		{ 20, 10 },
		{ 10, 10 },
		{ 0, 0 },
		{ 50, 0 },
		{ 0, 50 }
	};

	VSET( V, 10, 20, 30 );
	VSET( u_vec, 1, 0, 0 );
	VSET( v_vec, 0, 1, 0 );

	skt = sketch_start( V, u_vec, v_vec );

	sketch_add_verts( skt, vert_count, verts );

	/* first curve is a full cirle */
	strcpy( crv.crv_name, "first_curve" );
	crv.seg_count = 1;
	crv.segments = (genptr_t *)bu_calloc( 3, sizeof( genptr_t ), "segments" );
	crv.reverse = (int *)bu_calloc( 3, sizeof( int ), "reverse" );
	crv.segments[0] = (genptr_t)&csg;
	csg.magic = CURVE_CARC_MAGIC;
	csg.start = 0;
	csg.end = 1;
	csg.radius = -1.0;
	csg.orientation = 0;
	csg.curve_count = 1;
	csg.curves = (genptr_t)&crv;

	sketch_add_curve( skt, &crv );

	/* second curve is a triangle */
	strcpy( crv.crv_name, "second_curve" );
	crv.seg_count = 3;
	crv.segments[0] = (genptr_t)&lsg[0];
	crv.segments[1] = (genptr_t)&lsg[1];
	crv.segments[2] = (genptr_t)&lsg[2];
	lsg[0].magic = CURVE_LSEG_MAGIC;
	lsg[0].start = 2;
	lsg[0].end = 3;
	lsg[0].curve_count = 1;
	lsg[0].curves = (genptr_t)&crv;
	lsg[1].magic = CURVE_LSEG_MAGIC;
	lsg[1].start = 3;
	lsg[1].end = 4;
	lsg[1].curve_count = 1;
	lsg[1].curves = (genptr_t)&crv;
	lsg[2].magic = CURVE_LSEG_MAGIC;
	lsg[2].start = 4;
	lsg[2].end = 2;
	lsg[2].curve_count = 1;
	lsg[2].curves = (genptr_t)&crv;

	sketch_add_curve( skt, &crv );

	mk_id( stdout, "sketch test" );
	mk_sketch( stdout, "test_sketch", skt );
}
