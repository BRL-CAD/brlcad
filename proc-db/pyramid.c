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

main()
{
	sin60 = sin(3.14159265358979323846264/3.0);

	mk_id( "Triangles" );

	do_leaf();
	do_tree("tree",4);
}

do_leaf()
{
	point_t pt[8];

	VSET( pt[0], 0, 0, 0);
	VSET( pt[1], 1, 0, 0);
	VSET( pt[2], 0.5, sin60, 0);
	VMOVE( pt[3], pt[2] );

	VSET( pt[4], 0.5, sin60/3, sin60 );

	VMOVE( pt[5], pt[1] );
	VMOVE( pt[6], pt[2] );
	VMOVE( pt[7], pt[4] );

	mk_arb( "leaf", pt, 8 );
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
	mk_comb( name, 4, level<=1 );

	mat_idn( m );
	sprintf(nm, "%sL", name);
	mk_memb( UNION, leafp, m );

	MAT_DELTAS( m, 1*scale, 0, 0 );
	sprintf(nm, "%sR", name);
	mk_memb( UNION, leafp, m );

	MAT_DELTAS( m, 0.5*scale, sin60*scale, 0 );
	sprintf(nm, "%sB", name);
	mk_memb( UNION, leafp, m );

	MAT_DELTAS( m, 0.5*scale, sin60/3*scale, sin60*scale );
	sprintf(nm, "%sT", name);
	mk_memb( UNION, leafp, m );

	/* Loop for children if level > 1 */
	if( level <= 1 )
		return;
	for( i=0; i<4; i++ )  {
		sprintf(nm, "%s%c", name, "LRBTx"[i] );
		do_tree( nm, level-1 );
	}
}
