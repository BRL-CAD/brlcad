/*
 *			A R B . C
 *
 * Functions -
 *	draw_arb	draw an ARB8
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include	<stdio.h>
#include	<math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"

#include "./dm.h"
#include "./objdir.h"
#include "./ged.h"

#define FACE( valp, a, b, c, d ) \
	ADD_VL( vhead, &valp[a*3], 0 ); \
	ADD_VL( vhead, &valp[b*3], 1 ); \
	ADD_VL( vhead, &valp[c*3], 1 ); \
	ADD_VL( vhead, &valp[d*3], 1 );

/*
 *  			D R A W _ A R B 8
 *
 * Draw an ARB8, which is represented as a vector
 * from the origin to the first point, and 7 vectors
 * from the first point to the remaining points.
 */
void
draw_arb8( origp, matp, vhead )
struct solidrec *origp;
register matp_t matp;
struct vlhead	*vhead;
{
	register int i;
	register dbfloat_t *ip;
	register fastf_t *op;
	static vect_t work;
	static fastf_t	points[3*8];
	
	/*
	 * Convert from vector to point notation for drawing.
	 */
	MAT4X3PNT( &points[0], matp, &origp->s_values[0] );

	ip = &origp->s_values[1*3];
	op = &points[1*3];
	for( i=1; i<8; i++ )  {
		VADD2( work, &origp->s_values[0], ip );
		MAT4X3PNT( op, matp, work );
		ip += 3;
		op += ELEMENTS_PER_VECT;
	}

	FACE( points, 0, 1, 2, 3 );
	FACE( points, 4, 0, 3, 7 );
	FACE( points, 5, 4, 7, 6 );
	FACE( points, 1, 5, 6, 2 );
}

void
move_arb( sp, dp, mat )
struct solidrec *sp;
struct directory *dp;
mat_t mat;
{
	;
}

void
dbpr_arb( sp, dp )
struct solidrec *sp;
register struct directory *dp;
{
	int i;
	char *s;

	if( (i=sp->s_cgtype) < 0 )
		i = -i;
	switch( i )  {
	case ARB4:
		s="ARB4";
		break;
	case ARB5:
		s="ARB5";
		break;
	case RAW:
	case ARB6:
		s="ARB6";
		break;
	case ARB7:
		s="ARB7";
		break;
	case ARB8:
		s="ARB8";
		break;
	default:
		s="??";
		break;
	}

	(void)printf("%s:  ARB8 (%s)\n", dp->d_namep, s );

	/* more in edsol.c/pr_solid, called from do_list */
}
