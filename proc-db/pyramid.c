/*
 *			T R I . C
 *
 *  Program to generate recursive 3-d triangles (arb4).
 *  Inspired by the SigGraph paper of ???
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"

double sin60;

main(argc, argv)
char	**argv;
{
	int depth;

	if( argc != 2 )  {
		fprintf(stderr, "Usage:  tri recursion\n");
		exit(1);
	}
	depth = atoi( argv[1] );
	sin60 = sin(60.0 * 3.14159265358979323846264 / 180.0);

	mk_id( stdout, "Triangles" );

	do_leaf();
	do_tree("tree", depth);
}

do_leaf()
{
	point_t pt[4];

	VSET( pt[0], 0, 0, 0);
	VSET( pt[1], 1, 0, 0);
	VSET( pt[2], 0.5, sin60, 0);
	VSET( pt[3], 0.5, sin60/3, sin60 );

	mk_arb4( stdout, "leaf", pt );
}

do_tree(name, level)
char *name;
int level;
{
	register int i;
	mat_t m;
	char nm[64];
	char *leafp;
	int scale;

	if( level <= 1 )
		leafp = "leaf";		/* XXX */
	else
		leafp = nm;

	scale = 1;
	for( i=1; i<level; i++ )
		scale *= 2;

	/* len = 4, set region on lowest level */
	mk_comb( stdout, name, 4, level<=1 );

	mat_idn( m );
	sprintf(nm, "%sL", name);
	mk_memb( stdout, UNION, leafp, m );

	MAT_DELTAS( m, 1*scale, 0, 0 );
	sprintf(nm, "%sR", name);
	mk_memb( stdout, UNION, leafp, m );

	MAT_DELTAS( m, 0.5*scale, sin60*scale, 0 );
	sprintf(nm, "%sB", name);
	mk_memb( stdout, UNION, leafp, m );

	MAT_DELTAS( m, 0.5*scale, sin60/3*scale, sin60*scale );
	sprintf(nm, "%sT", name);
	mk_memb( stdout, UNION, leafp, m );

	/* Loop for children if level > 1 */
	if( level <= 1 )
		return;
	for( i=0; i<4; i++ )  {
		sprintf(nm, "%s%c", name, "LRBTx"[i] );
		do_tree( nm, level-1 );
	}
}
