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

#include	<math.h>
#include	<stdio.h>
#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "../h/db.h"
#include "dm.h"
#include "objdir.h"
#include "ged.h"

static void	face();

/*
 *  			D R A W _ A R B 8
 *
 * Draw an ARB8, which is represented as a vector
 * from the origin to the first point, and 7 vectors
 * from the first point to the remaining points.
 */
void
draw_arb8( origp, matp )
struct solidrec *origp;
register matp_t matp;
{
	register int i;
	register float *ip;
	register float *op;
	static vect_t work;
	static struct solidrec sr;

	/* Because we will modify the values, make a private copy */
	sr = *origp;		/* struct copy */
	
	/*
	 * Convert from vector to point notation for drawing.
	 */
	MAT4X3PNT( &sr.s_values[0], matp, &origp->s_values[0] );

	ip = &origp->s_values[1*3];
	op = &sr.s_values[1*3];
	for( i=1; i<8; i++ )  {
		VADD2( work, &origp->s_values[0], ip );
		MAT4X3PNT( op, matp, work );
		ip += 3;
		op += 3;
	}

	face( sr.s_values, 0, 1, 2, 3 );
	face( sr.s_values, 4, 0, 3, 7 );
	face( sr.s_values, 5, 4, 7, 6 );
	face( sr.s_values, 1, 5, 6, 2 );
}

/*
 *			F A C E
 *
 * This routine traces one face of an ARB8
 */
static void
face( valp, a, b, c, d )
register float *valp;
{
	DM_GOTO( &valp[a*3], PEN_UP );
	DM_GOTO( &valp[b*3], PEN_DOWN );
	DM_GOTO( &valp[c*3], PEN_DOWN );
	DM_GOTO( &valp[d*3], PEN_DOWN );
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
