/*
 *			P Y R A M I D . C
 *
 *  Program to generate recursive 3-d pyramids (arb4).
 *  Inspired by the SigGraph paper of Glasser.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

void	do_leaf(), do_pleaf(), pnorms(), do_tree();

double sin60;

struct rt_wdb *outfp;

int
main(argc, argv)
char	**argv;
{
	int depth;

	if( argc != 2 )  {
		fprintf(stderr, "Usage:  pyramid recursion\n");
		exit(1);
	}
	depth = atoi( argv[1] );
	sin60 = sin(60.0 * 3.14159265358979323846264 / 180.0);

	outfp = wdb_fopen( "pyramid.g" );

	mk_id( outfp, "3-D Pyramids" );

	do_leaf("leaf");
#if 0
	do_pleaf("polyleaf");
#endif
	do_tree("tree", "leaf", depth);

	return 0;
}

/* Make a leaf node out of an ARB4 */
void
do_leaf(name)
char	*name;
{
	point_t pt[4];

	VSET( pt[0], 0, 0, 0);
	VSET( pt[1], 100, 0, 0);
	VSET( pt[2], 50, 100*sin60, 0);
	VSET( pt[3], 50, 100*sin60/3, 100*sin60 );

	mk_arb4( outfp, name, &pt[0][X] );
}

#if 0
/* Make a leaf node out of 4 polygons */
void
do_pleaf(name)
char	*name;
{
	point_t pt[4];
	fastf_t	verts[5][3];
	fastf_t	norms[5][3];
	point_t	centroid;
	register int i;

	VSET( pt[0], 0, 0, 0);
	VSET( pt[1], 100, 0, 0);
	VSET( pt[2], 50, 100*sin60, 0);
	VSET( pt[3], 50, 100*sin60/3, 100*sin60 );

	VMOVE( centroid, pt[0] );
	for( i=1; i<4; i++ )  {
		VADD2( centroid, centroid, pt[i] );
	}
	VSCALE( centroid, centroid, 0.25 );

	mk_polysolid( outfp, name );

	VMOVE( verts[0], pt[0] );
	VMOVE( verts[1], pt[1] );
	VMOVE( verts[2], pt[2] );
	pnorms( norms, verts, centroid, 3 );
	mk_poly( outfp, 3, verts, norms );

	VMOVE( verts[0], pt[0] );
	VMOVE( verts[1], pt[1] );
	VMOVE( verts[2], pt[3] );
	pnorms( norms, verts, centroid, 3 );
	mk_poly( outfp, 3, verts, norms );

	VMOVE( verts[0], pt[0] );
	VMOVE( verts[1], pt[2] );
	VMOVE( verts[2], pt[3] );
	pnorms( norms, verts, centroid, 3 );
	mk_poly( outfp, 3, verts, norms );

	VMOVE( verts[0], pt[1] );
	VMOVE( verts[1], pt[2] );
	VMOVE( verts[2], pt[3] );
	pnorms( norms, verts, centroid, 3 );
	mk_poly( outfp, 3, verts, norms );
}
#endif

/*
 *  Find the single outward pointing normal for a facet.
 *  Assumes all points are coplanar (they better be!).
 */
void
pnorms( norms, verts, centroid, npts )
fastf_t	norms[5][3];
fastf_t	verts[5][3];
point_t	centroid;
int	npts;
{
	register int i;
	vect_t	ab, ac;
	vect_t	n;
	vect_t	out;		/* hopefully points outwards */

	VSUB2( ab, verts[1], verts[0] );
	VSUB2( ac, verts[2], verts[0] );
	VCROSS( n, ab, ac );
	VUNITIZE( n );

	/* If normal points inwards (towards centroid), flip it */
	VSUB2( out, verts[0], centroid );
	if( VDOT( n, out ) < 0 )  {
		VREVERSE( n, n );
	}

	/* Use same normal for all vertices (flat shading) */
	for( i=0; i<npts; i++ )  {
		VMOVE( norms[i], n );
	}
}

void
do_tree(name, lname, level)
char	*name;
char	*lname;
int	level;
{
	register int i;
	char nm[64];
	char *leafp;
	int scale;
	struct wmember	head;
	struct wmember	*wp;

	BU_LIST_INIT(&head.l);

	if( level <= 1 )
		leafp = lname;
	else
		leafp = nm;

	scale = 100;
	for( i=1; i<level; i++ )
		scale *= 2;

	sprintf(nm, "%sL", name);
	wp = mk_addmember( leafp, &head.l, NULL, WMOP_UNION );
	MAT_IDN( wp->wm_mat );

	sprintf(nm, "%sR", name);
	wp = mk_addmember( leafp, &head.l, NULL, WMOP_UNION );
	MAT_DELTAS( wp->wm_mat, 1*scale, 0, 0 );

	sprintf(nm, "%sB", name);
	wp = mk_addmember( leafp, &head.l, NULL, WMOP_UNION );
	MAT_DELTAS( wp->wm_mat, 0.5*scale, sin60*scale, 0 );

	sprintf(nm, "%sT", name);
	wp = mk_addmember( leafp, &head.l, NULL, WMOP_UNION );
	MAT_DELTAS( wp->wm_mat, 0.5*scale, sin60/3*scale, sin60*scale );

	/* Set region flag on lowest level */
	mk_lcomb( outfp, name, &head, level<=1, NULL, NULL, NULL, 0 );

	/* Loop for children if level > 1 */
	if( level <= 1 )
		return;
	for( i=0; i<4; i++ )  {
		sprintf(nm, "%s%c", name, "LRBTx"[i] );
		do_tree( nm, lname, level-1 );
	}
}
