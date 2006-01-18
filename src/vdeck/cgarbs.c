/*                        C G A R B S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file cgarbs.c
 *
 *  Routines to take a generalized ARB in 8-point form,
 *  determine which points are equivalent,
 *  decide which GIFT special-case solid fits this case,
 *  and convert the point list into GIFT format.
 *
 *  Author -
 *	Keith Applin
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"

#define YES	1
#define NO	0

/*
 *			C G A R B S
 *
 *  Determines COMGEOM arb type of a general arb.
 *
 *  Returns -
 *	> 0	number of unique points, and updated *cgtype
 *	  0	error
 */
int
cgarbs( cgtype, gp, uniq, svec, dist_tol )
int			*cgtype;
const struct rt_arb_internal	*gp;
int			uniq[8];	/* array of unique pt subscripts */
int			svec[11];	/* array of like pt subscripts */
const double		dist_tol;	/* distance tolerance */
{
	register int	i, j;
	int		numuniq, unique, done;
	int		si;

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO)
			svec[si] = i;
		for(j=i+1; j<8; j++) {
			if( VAPPROXEQUAL( gp->pt[i], gp->pt[j], dist_tol ) )  {
				if( done == NO ) svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}
	if( si > 2 && si < 6 )
		svec[0] = si - 1;
	if( si > 6 )
		svec[1] = si - 5;
	for(i=1; i<=svec[1]; i++)
		svec[svec[0]+1+i] = svec[5+i];
	for(i=svec[0]+svec[1]+2; i<11; i++)
		svec[i] = -1;

	/* find the unique points */
	numuniq = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES )
			uniq[numuniq++] = j;
	}

	/* put comgeom solid typpe into s_cgtype */
	switch( numuniq ) {
	case 8:
		*cgtype = 8;  /* ARB8 */
		break;
	case 6:
		*cgtype = 7;	/* ARB7 */
		break;
	case 4:
		if(svec[0] == 2)
			*cgtype = 6;	/* ARB6 */
		else
			*cgtype = 5;	/* ARB5 */
		break;
	case 2:
		*cgtype = 4;	/* ARB4 */
		break;
	default:
		(void) fprintf( stderr,
		    "cgarbs: bad number of unique vectors (%d)\n",
		    numuniq );
		return(0);
	}
	return( numuniq );
}

/*
 *			A R B _ M V
 *
 *  Permute the points as directed.
 */
static void
arb_mv( pts, gp, p0, p1, p2, p3, p4, p5, p6, p7 )
register point_t	pts[8];
register const struct rt_arb_internal	*gp;
const int		p0, p1, p2, p3, p4, p5, p6, p7;
{
	RT_ARB_CK_MAGIC(gp);
	VMOVE( pts[0], gp->pt[p0] );
	VMOVE( pts[1], gp->pt[p1] );
	VMOVE( pts[2], gp->pt[p2] );
	VMOVE( pts[3], gp->pt[p3] );
	VMOVE( pts[4], gp->pt[p4] );
	VMOVE( pts[5], gp->pt[p5] );
	VMOVE( pts[6], gp->pt[p6] );
	VMOVE( pts[7], gp->pt[p7] );
}

/*
 *			R E D O A R B
 *
 *  Rearranges arbs to be GIFT compatible
 *  The input in "gp" is not modified.
 *  The first "numvec" entires of "pts" are the GIFT format of arb "cgtype".
 *
 *  Returns -
 *	1	OK
 *	0	error
 */
int
redoarb( pts, gp, uniq, svec, numvec, cgtype )
point_t		pts[8];
const struct rt_arb_internal	*gp;
int		uniq[8];
int		svec[11];
const int	numvec;
const int	cgtype;
{
	register int	i, j;
	int		prod;

	/* For all the cases that don't require shuffling, duplicate first */
	for( i=0; i<8; i++ )  {
		VMOVE( pts[i], gp->pt[i] );
	}

	/* cgtype indicates which kind of ARB it is */
	switch( cgtype ) {
	case 8:
		break;

	case 7:
		/* arb7 vectors: 0 1 2 3 4 5 6 4 */
		switch( svec[2] ) {
		case 0:			/* 0 = 1, 3, or 4 */
			if(svec[3] == 1)	arb_mv(pts,gp,4,7,6,5,1,4,3,1);
			if(svec[3] == 3)	arb_mv(pts,gp,4,5,6,7,0,1,2,0);
			if(svec[3] == 4)	arb_mv(pts,gp,1,2,6,5,0,3,7,0);
			break;
		case 1:			/* 1 = 2 or 5 */
			if(svec[3] == 2)	arb_mv(pts,gp,0,4,7,3,1,5,6,1);
			if(svec[3] == 5)	arb_mv(pts,gp,0,3,7,4,1,2,6,1);
			break;
		case 2:			/* 2 = 3 or 6 */
			if(svec[3] == 3)	arb_mv(pts,gp,6,5,4,7,2,1,0,2);
			if(svec[3] == 6)	arb_mv(pts,gp,3,0,4,7,2,1,5,2);
			break;
		case 3:			/* 3 = 7 */
			arb_mv(pts,gp,2,1,5,6,3,0,4,3);
			break;
		case 4:			/* 4 = 5 */
			/* if 4 = 7  do nothing */
			if(svec[3] == 5)	arb_mv(pts,gp,1,2,3,0,5,6,7,5);
			break;
		case 5:			/* 5 = 6 */
			arb_mv(pts,gp,2,3,0,1,6,7,4,6);
			break;
		case 6:			/* 6 = 7 */
			arb_mv(pts,gp,3,0,1,2,7,4,5,7);
			break;
		default:
			(void) fprintf( stderr, "redoarb: bad arb7\n" );
			return( 0 );
		}
		break;
		/* end of ARB7 case */
	case 6:
		/* arb6 vectors:  0 1 2 3 4 4 6 6 */
		prod = 1;
		for(i=0; i<numvec; i++)
			prod = prod * (uniq[i] + 1);
		switch( prod ) {
		case 24:	/* 0123 unique */
			/* 4=7 and 5=6  OR  4=5 and 6=7 */
			if(svec[3] == 7)	arb_mv(pts,gp,3,0,1,2,4,4,5,5);
			else	arb_mv(pts,gp,0,1,2,3,4,4,6,6);
			break;
		case 1680:	/* 4567 unique */
			/* 0=3 and 1=2  OR  0=1 and 2=3 */
			if(svec[3] == 3)	arb_mv(pts,gp,7,4,5,6,0,0,1,1);
			else	arb_mv(pts,gp,4,5,6,7,0,0,2,2);
			break;
		case 160:	/* 0473 unique */
			/* 1=2 and 5=6  OR  1=5 and 2=6 */
			if(svec[3] == 2)	arb_mv(pts,gp,0,3,7,4,1,1,5,5);
			else	arb_mv(pts,gp,4,0,3,7,1,1,2,2);
			break;
		case 672:	/* 3267 unique */
			/* 0=1 and 4=5  OR  0=4 and 1=5 */
			if(svec[3] == 1)	arb_mv(pts,gp,3,2,6,7,0,0,4,4);
			else	arb_mv(pts,gp,7,3,2,6,0,0,1,1);
			break;
		case 252:	/* 1256 unique */
			/* 0=3 and 4=7  OR 0=4 and 3=7 */
			if(svec[3] == 3)	arb_mv(pts,gp,1,2,6,5,0,0,4,4);
			else	arb_mv(pts,gp,5,1,2,6,0,0,3,3);
			break;
		case 60:	/* 0154 unique */
			/* 2=3 and 6=7  OR  2=6 and 3=7 */
			if(svec[3] == 3)	arb_mv(pts,gp,0,1,5,4,2,2,6,6);
			else	arb_mv(pts,gp,5,1,0,4,2,2,3,3);
			break;
		default:
			(void) fprintf( stderr,"redoarb: bad arb6\n");
			return( 0 );
		}
		break;
		/* end of ARB6 case */
	case 5:
		/* arb5 vectors:  0 1 2 3 4 4 4 4 */
		prod = 1;
		for(i=2; i<6; i++)
			prod = prod * (svec[i] + 1);
		switch( prod ) {
		case 24:	/* 0=1=2=3 */
			arb_mv(pts,gp,4,5,6,7,0,0,0,0);
			break;
		case 1680:	/* 4=5=6=7 */
			/* do nothing */
			break;
		case 160:	/* 0=3=4=7 */
			arb_mv(pts,gp,1,2,6,5,0,0,0,0);
			break;
		case 672:	/* 2=3=7=6 */
			arb_mv(pts,gp,0,1,5,4,2,2,2,2);
			break;
		case 252:	/* 1=2=5=6 */
			arb_mv(pts,gp,0,3,7,4,1,1,1,1);
			break;
		case 60:	/* 0=1=5=4 */
			arb_mv(pts,gp,3,2,6,7,0,0,0,0);
			break;
		default:
			(void) fprintf( stderr,"redoarb: bad arb5\n" );
			return( 0 );
		}
		break;
		/* end of ARB5 case */
	case 4:
		/* arb4 vectors:  0 1 2 0 4 4 4 4 */
		j = svec[6];
		if( svec[0] == 2 )	j = svec[4];
		arb_mv(pts,gp,uniq[0],uniq[1],svec[2],uniq[0],j,j,j,j);
		break;
	default:
		(void) fprintf( stderr,
		    "redoarb: unknown arb type (%d)\n", cgtype );
		return( 0 );
	}
	return( 1 );
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
