/*                        S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file sketch.c
 *
 *  Program to generate test sketches
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"

extern struct rt_sketch_internal *sketch_start();

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{
	struct rt_sketch_internal *skt;
	struct bezier_seg *bsg;
	struct line_seg *lsg;
	struct carc_seg *csg;
	point_t V;
	vect_t u_vec, v_vec;
	point2d_t verts[] = {
		{ 250, 0 },	/* 0 */
		{ 500, 0 },	/* 1 */
		{ 500, 500 },	/* 2 */
		{ 0, 500 },	/* 3 */
		{ 0, 250 },	/* 4 */
		{ 250, 250 },	/* 5 */
		{ 125, 125 },	/* 6 */
		{ 0, 125 },	/* 7 */
		{ 125, 0 },	/* 8 */
		{ 200, 200 }	/* 9 */
	};
	int i;

	VSET( V, 10, 20, 30 );
	VSET( u_vec, 1, 0, 0 );
	VSET( v_vec, 0, 1, 0 );

	skt = (struct rt_sketch_internal *)bu_calloc( 1, sizeof( struct rt_sketch_internal ), "sketch" );
	skt->magic = RT_SKETCH_INTERNAL_MAGIC;
	VMOVE( skt->V, V );
	VMOVE( skt->u_vec, u_vec );
	VMOVE( skt->v_vec, v_vec );
	skt->vert_count = 10;
	skt->verts = (point2d_t *)bu_calloc( skt->vert_count, sizeof( point2d_t ), "verts" );
	for( i=0 ; i<skt->vert_count ; i++ ) {
		V2MOVE( skt->verts[i], verts[i] );
	}

	skt->skt_curve.seg_count = 6;
	skt->skt_curve.reverse = (int *)bu_calloc( skt->skt_curve.seg_count, sizeof( int ), "sketch: reverse" );

	skt->skt_curve.segments = (genptr_t *)bu_calloc( skt->skt_curve.seg_count, sizeof( genptr_t ), "segs" );
	bsg = (struct bezier_seg *)bu_malloc( sizeof( struct bezier_seg ), "sketch: bsg" );
	bsg->magic = CURVE_BEZIER_MAGIC;
	bsg->degree = 4;
	bsg->ctl_points = (int *)bu_calloc( bsg->degree+1, sizeof( int ), "sketch: bsg->ctl_points" );
	bsg->ctl_points[0] = 4;
	bsg->ctl_points[1] = 7;
	bsg->ctl_points[2] = 9;
	bsg->ctl_points[3] = 8;
	bsg->ctl_points[4] = 0;
	skt->skt_curve.segments[0] = (genptr_t)bsg;

	lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 0;
	lsg->end = 1;

	skt->skt_curve.segments[1] = (genptr_t)lsg;

	lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 1;
	lsg->end = 2;

	skt->skt_curve.segments[2] = (genptr_t)lsg;

	lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 2;
	lsg->end = 3;

	skt->skt_curve.segments[3] = (genptr_t)lsg;

	lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "sketch: lsg" );
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 3;
	lsg->end = 4;

	skt->skt_curve.segments[4] = (genptr_t)lsg;

	csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "sketch: csg" );
	csg->magic = CURVE_CARC_MAGIC;
	csg->radius = -1.0;
	csg->start = 6;
	csg->end = 5;

	skt->skt_curve.segments[5] = (genptr_t)csg;

	outfp = wdb_fopen( "sketch.g" );
	mk_id( outfp, "sketch test" );
	mk_sketch( outfp, "test_sketch", skt );
	return 0;
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
