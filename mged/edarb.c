/*
 *			E D A R B . C
 *
 * Functions -
 *	editarb		edit ARB edge (and move points)
 *	planeqn		finds plane equation given 3 points
 *	intersect	finds intersection point of three planes
 *	mv_edge		moves an arb edge
 *
 *  Author -
 *	Keith A. Applin
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

#include <stdio.h>
#include <math.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

/* face definitions for each arb type */
int arb_faces[5][24] = {
	{0,1,2,3, 0,1,4,5, 1,2,4,5, 0,2,4,5, -1,-1,-1,-1, -1,-1,-1,-1},	/* ARB4 */
	{0,1,2,3, 4,0,1,5, 4,1,2,5, 4,2,3,5, 4,3,0,5, -1,-1,-1,-1},	/* ARB5 */
	{0,1,2,3, 1,2,4,6, 0,4,6,3, 4,1,0,5, 6,2,3,7, -1,-1,-1,-1},	/* ARB6 */
	{0,1,2,3, 4,5,6,7, 0,3,4,7, 1,2,6,5, 0,1,5,4, 3,2,6,4},		/* ARB7 */
	{0,1,2,3, 4,5,6,7, 0,4,7,3, 1,2,6,5, 0,1,5,4, 3,2,6,7},		/* ARB8 */
};

/*
 *  			E D I T A R B
 *  
 *  An ARB edge is moved by finding the direction of
 *  the line containing the edge and the 2 "bounding"
 *  planes.  The new edge is found by intersecting the
 *  new line location with the bounding planes.  The
 *  two "new" planes thus defined are calculated and the
 *  affected points are calculated by intersecting planes.
 *  This keeps ALL faces planar.
 *
 */

/*  The storage for the "specific" ARB types is :
 *
 *	ARB4	0 1 2 0 3 3 3 3
 *	ARB5	0 1 2 3 4 4 4 4
 *	ARB6	0 1 2 3 4 4 5 5
 *	ARB7	0 1 2 3 4 5 6 4
 *	ARB8	0 1 2 3 4 5 6 7
 */

/* Another summary of how the vertices of ARBs are stored:
 *
 * Vertices:	1	2	3	4	5	6	7	8
 * Location----------------------------------------------------------------
 *	ARB8	0	1	2	3	4	5	6	7
 *	ARB7	0	1	2	3	4,7	5	6
 *	ARB6	0	1	2	3	4,5	6,7
 * 	ARB5	0	1	2	3	4,5,6,7
 *	ARB4	0,3	1	2	4,5,6,7
 */

/* The following arb editing arrays generally contain the following:
 *
 *	location 	comments
 *------------------------------------------------------------------------
 *	0,1		edge end points
 * 	2,3		bounding planes 1 and 2
 *	4, 5,6,7	plane 1 to recalculate, using next 3 points
 *	8, 9,10,11	plane 2 to recalculate, using next 3 points
 *	12, 13,14,15	plane 3 to recalculate, using next 3 points
 *	16,17		points (vertices) to recalculate
 *
 *
 * Each line is repeated for each edge (or point) to move
*/

/* edit array for arb8's */
static short earb8[12][18] = {
	{0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
	{1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
	{2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,7},	/* edge 34 */
	{0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,7},	/* edge 41 */
	{0,4, 0,1, 2,0,4,3, 4,0,1,4, -1,0,0,0, 7,5},	/* edge 15 */
	{1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
	{4,5, 2,3, 4,0,5,4, 1,4,5,6, -1,0,0,0, 1,7},	/* edge 56 */
	{5,6, 4,5, 3,1,5,6, 1,4,5,6, -1,0,0,0, 2,7},	/* edge 67 */
	{6,7, 3,2, 5,2,7,6, 1,4,6,7, -1,0,0,0, 3,4},	/* edge 78 */
	{4,7, 4,5, 2,0,7,4, 1,4,5,7, -1,0,0,0, 3,6},	/* edge 58 */
	{2,6, 0,1, 3,1,2,6, 5,2,3,6, -1,0,0,0, 5,7},	/* edge 37 */
	{3,7, 0,1, 2,0,3,7, 5,2,3,7, -1,0,0,0, 4,6},	/* edge 48 */
};

/* edit array for arb7's */
static short earb7[12][18] = {
	{0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
	{1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
	{2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,4},	/* edge 34 */
	{0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,-1},	/* edge 41 */
	{0,4, 0,5, 4,0,5,4, 2,0,3,4, 1,4,5,6, 1,-1},	/* edge 15 */
	{1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
	{4,5, 5,3, 2,0,3,4, 4,0,5,4, 1,4,5,6, 1,-1},	/* edge 56 */
	{5,6, 4,5, 3,1,6,5, 1,4,5,6, -1,0,0,0, 2, -1},	/* edge 67 */
	{2,6, 0,1, 5,2,3,6, 3,1,2,6, -1,0,0,0, 4,5},	/* edge 37 */
	{4,6, 4,3, 2,0,3,4, 5,3,4,6, 1,4,5,6, 2,-1},	/* edge 57 */
	{3,4, 0,1, 4,0,1,4, 2,0,3,4, 5,2,3,4, 5,6},	/* edge 45 */
	{-1,-1, -1,-1, 5,2,3,4, 4,0,1,4, 8,2,1,-1, 6,5},	/* point 5 */
};

/* edit array for arb6's */
static short earb6[10][18] = {
	{0,1, 2,1, 3,0,1,4, 0,0,1,2, -1,0,0,0, 3,-1},	/* edge 12 */
	{1,2, 3,4, 1,1,2,5, 0,0,1,2, -1,0,0,0, 3,4},	/* edge 23 */
	{2,3, 1,2, 4,2,3,5, 0,0,2,3, -1,0,0,0, 1,-1},	/* edge 34 */
	{0,3, 3,4, 2,0,3,5, 0,0,1,3, -1,0,0,0, 4,2},	/* edge 14 */
	{0,4, 0,1, 3,0,1,4, 2,0,3,4, -1,0,0,0, 6,-1},	/* edge 15 */
	{1,4, 0,2, 3,0,1,4, 1,1,2,4, -1,0,0,0, 6,-1},	/* edge 25 */
	{2,6, 0,2, 4,6,2,3, 1,1,2,6, -1,0,0,0, 4,-1},	/* edge 36 */
	{3,6, 0,1, 4,6,2,3, 2,0,3,6, -1,0,0,0, 4,-1},	/* edge 46 */
	{-1,-1, -1,-1, 2,0,3,4, 1,1,2,4, 3,0,1,4, 6,-1},/* point 5 */
	{-1,-1, -1,-1, 2,0,3,6, 1,1,2,6, 4,2,3,6, 4,-1},/* point 6 */
};

/* edit array for arb5's */
static short earb5[9][18] = {
	{0,1, 4,2, 0,0,1,2, 1,0,1,4, -1,0,0,0, 3,-1},	/* edge 12 */
	{1,2, 1,3, 0,0,1,2, 2,1,2,4, -1,0,0,0, 3,-1},	/* edge 23 */
	{2,3, 2,4, 0,0,2,3, 3,2,3,4, -1,0,0,0, 1,-1},	/* edge 34 */
	{0,3, 1,3, 0,0,1,3, 4,0,3,4, -1,0,0,0, 2,-1},	/* edge 14 */
	{0,4, 0,2, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 15 */
	{1,4, 0,3, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 25 */
	{2,4, 0,4, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, 	/* edge 35 */
	{3,4, 0,1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 45 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 5 */
};

/* edit array for arb4's */
static short earb4[5][18] = {
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 1 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 2 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 3 */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* dummy */
	{-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 4 */
};


int
editarb( pos_model )
vect_t pos_model;
{
	static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
	short *edptr;		/* pointer to arb edit array */
	short *final;		/* location of points to redo */
	register dbfloat_t *op;
	static int i, *iptr;

	/* set the pointer */
	switch( es_type ) {

		case ARB4:
			edptr = &earb4[es_menu][0];
			final = &earb4[es_menu][16];
		break;

		case ARB5:
			edptr = &earb5[es_menu][0];
			final = &earb5[es_menu][16];
			if(es_edflag == PTARB) {
				edptr = &earb5[8][0];
				final = &earb5[8][16];
			}
		break;

		case ARB6:
			edptr = &earb6[es_menu][0];
			final = &earb6[es_menu][16];
			if(es_edflag == PTARB) {
				i = 9;
				if(es_menu == 4)
					i = 8;
				edptr = &earb6[i][0];
				final = &earb6[i][16];
			}
		break;

		case ARB7:
			edptr = &earb7[es_menu][0];
			final = &earb7[es_menu][16];
			if(es_edflag == PTARB) {
				edptr = &earb7[11][0];
				final = &earb7[11][16];
			}
		break;

		case ARB8:
			edptr = &earb8[es_menu][0];
			final = &earb8[es_menu][16];
		break;

		default:
			(void)printf("edarb: unknown ARB type\n");
		return(1);
	}

	/* convert to point notation (in place ----- DANGEROUS) */
	for(i=3; i<=21; i+=3) {
		op = &es_rec.s.s_values[i];
		VADD2( op, op, &es_rec.s.s_values[0] );
	}

	/* do the arb editing */

	if( es_edflag == PTARB ) {
		/* moving a point - not an edge */
		VMOVE(&es_rec.s.s_values[es_menu*3], &pos_model[0]);
		edptr += 4;
	} else if( es_edflag == EARB ) {
		vect_t	edge_dir;

		/* moving an edge */
		pt1 = *edptr++;
		pt2 = *edptr++;
		/* direction of this edge */
		if( newedge ) {
			/* edge direction comes from edgedir() in pos_model */
			VMOVE( edge_dir, pos_model );
			VMOVE(pos_model, &es_rec.s.s_values[pt1*3]);
			newedge = 0;
		} else {
			/* must calculate edge direction */
			VSUB2(edge_dir, &es_rec.s.s_values[3*pt2], &es_rec.s.s_values[3*pt1]);
		}
		if(MAGNITUDE(edge_dir) == 0.0) 
			goto err;
		/* bounding planes bp1,bp2 */
		bp1 = *edptr++;
		bp2 = *edptr++;

		/* move the edge */
/*
printf("moving edge: %d%d  bound planes: %d %d\n",pt1+1,pt2+1,bp1+1,bp2+1);
*/
		if( mv_edge(pos_model, bp1, bp2, pt1, pt2, edge_dir) )
			goto err;
	}

	/* editing is done - insure planar faces */
	/* redo plane eqns that changed */
	newp = *edptr++; 	/* plane to redo */
	if( newp == 9 ) {
		/* special flag --> redo all the planes */
		iptr = &arb_faces[es_type-4][0];
		for(i=0; i<6; i++) {
			p1 = *iptr++;
			p2 = *iptr++;
			p3 = *iptr++;
			iptr++;
/*
printf("REDO plane %d with points %d %d %d\n",i+1,p1+1,p2+1,p3+1);
*/
			if( planeqn(i, p1, p2, p3, &es_rec.s) )
				goto err;
			if( *iptr == -1 )
				break;		/* finished */
		}
	}
	if(newp >= 0 && newp < 6) {
		for(i=0; i<3; i++) {
			/* redo this plane (newp), use points p1,p2,p3 */
			p1 = *edptr++;
			p2 = *edptr++;
			p3 = *edptr++;
/*
printf("redo plane %d with points %d %d %d\n",newp+1,p1+1,p2+1,p3+1);
*/
			if( planeqn(newp, p1, p2, p3, &es_rec.s) )
				goto err;
			/* next plane */
			if( (newp = *edptr++) == -1 || newp == 8 )
				break;
		}
	}
	if(newp == 8) {
		/* special...redo next planes using pts defined in faces */
		for(i=0; i<3; i++) {
			if( (newp = *edptr++) == -1 )
				break;
			iptr = &arb_faces[es_type-4][4*newp];
			p1 = *iptr++;
			p2 = *iptr++;
			p3 = *iptr++;
/*
printf("REdo plane %d with points %d %d %d\n",newp+1,p1+1,p2+1,p3+1);
*/
			if( planeqn(newp, p1, p2, p3, &es_rec.s) )
				goto err;
		}
	}

	/* the changed planes are all redone
	 *	push necessary points back into the planes
	 */
	edptr = final;	/* point to the correct location */
	for(i=0; i<2; i++) {
		if( (p1 = *edptr++) == -1 )
			break;
		/* intersect proper planes to define vertex p1 */
/*
printf("intersect: type=%d   point = %d\n",es_type,p1+1);
*/
		if( intersect( es_type, p1*3, p1, &es_rec.s ) )
			goto err;
	}

	/* Special case for ARB7: move point 5 .... must
	 *	recalculate plane 2 = 456
	 */
	if(es_type == ARB7 && es_edflag == PTARB) {
/*
printf("redo plane 2 == 5,6,7 for ARB7\n");
*/
		if( planeqn(2, 4, 5, 6, &es_rec.s) )
			goto err;
	}

	/* carry along any like points */
	switch( es_type ) {
		case ARB8:
		break;

		case ARB7:
			VMOVE(&es_rec.s.s_values[21], &es_rec.s.s_values[12]);
		break;

		case ARB6:
			VMOVE(&es_rec.s.s_values[15], &es_rec.s.s_values[12]);
			VMOVE(&es_rec.s.s_values[21], &es_rec.s.s_values[18]);
		break;

		case ARB5:
			for(i=15; i<=21; i+=3) {
				VMOVE(&es_rec.s.s_values[i], &es_rec.s.s_values[12]);
			}
		break;

		case ARB4:
			VMOVE(&es_rec.s.s_values[9], &es_rec.s.s_values[0]);
			for(i=15; i<=21; i+=3) {
				VMOVE(&es_rec.s.s_values[i], &es_rec.s.s_values[12]);
			}
		break;
	}

	/* back to vector notation */
	for(i=3; i<=21; i+=3) {
		op = &es_rec.s.s_values[i];
		VSUB2( op, op, &es_rec.s.s_values[0] );
	}
	return(0);		/* OK */

err:
	/* Error handling */
	(void)printf("cannot move edge: %d%d\n", pt1+1,pt2+1);
	es_edflag = IDLE;

	/* back to vector notation */
	for(i=3; i<=21; i+=3) {
		op = &es_rec.s.s_values[i];
		VSUB2(op, op, &es_rec.s.s_values[0]);
	}
	return(1);		/* BAD */
}

/*   PLANEQN:
 *	finds equation of a plane defined by 3 points use1,use2,use3
 *	of solid record sp.  Equation is stored at "loc" of es_peqn
 *	array.
 *
 *  Returns -
 *	 0	success
 *	-1	failure
 */
int
planeqn(loc, use1, use2, use3, sp)
int loc, use1, use2, use3;
struct solidrec *sp;
{
	vect_t	a,b,c;
	struct rt_tol	tol;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	/* XXX This converts data types as well! */
	VMOVE( a, &sp->s_values[use1*3] );
	VMOVE( b, &sp->s_values[use2*3] );
	VMOVE( c, &sp->s_values[use3*3] );

	return( rt_mk_plane_3pts( es_peqn[loc], a, b, c, &tol ) );
}

/* planes to define ARB vertices */
CONST int rt_arb_planes[5][24] = {
	{0,1,3, 0,1,2, 0,2,3, 0,1,3, 1,2,3, 1,2,3, 1,2,3, 1,2,3},	/* ARB4 */
	{0,1,4, 0,1,2, 0,2,3, 0,3,4, 1,2,4, 1,2,4, 1,2,4, 1,2,4},	/* ARB5 */
	{0,2,3, 0,1,3, 0,1,4, 0,2,4, 1,2,3, 1,2,3, 1,2,4, 1,2,4},	/* ARB6 */
	{0,2,4, 0,3,4, 0,3,5, 0,2,5, 1,4,5, 1,3,4, 1,3,5, 1,2,4},	/* ARB7 */
	{0,2,4, 0,3,4, 0,3,5, 0,2,5, 1,2,4, 1,3,4, 1,3,5, 1,2,5},	/* ARB8 */
};

/*	INTERSECT:
 *	Finds intersection point of three planes.
 *		The planes are at es_planes[type][loc] and
 *		the result is stored at "pos" in solid struct sp.
 *
 *  XXX replaced by rt_arb_3face_intersect().
 *
 *  Returns -
 *	 0	success
 *	 1	failure
 */
int
intersect( type, loc, pos, sp )
int type, loc, pos;
struct solidrec *sp;
{
	vect_t	vec1;
	int	j;
	int	i1, i2, i3;

	j = type - 4;

	i1 = rt_arb_planes[j][loc];
	i2 = rt_arb_planes[j][loc+1];
	i3 = rt_arb_planes[j][loc+2];

	if( rt_mkpoint_3planes( vec1, es_peqn[i1], es_peqn[i2],
	    es_peqn[i3] ) < 0 )
		return(1);
	VMOVE( &sp->s_values[pos*3], vec1 );	/* XXX type conversion too */
	return( 0 );
}

/*
 *			R T _ A R B _ 3 F A C E _ I N T E R S E C T
 *
 *	Finds the intersection point of three faces of an ARB.
 *
 *  Returns -
 *	  0	success
 *	 -1	failure
 */
int
rt_arb_3face_intersect( point, planes, type, loc )
point_t			point;
plane_t			planes[6];
int			type;		/* 4..8 */
int			loc;
{
	int	j;
	int	i1, i2, i3;

	j = type - 4;

	i1 = rt_arb_planes[j][loc];
	i2 = rt_arb_planes[j][loc+1];
	i3 = rt_arb_planes[j][loc+2];

	return rt_mkpoint_3planes( point, planes[i1], planes[i2], planes[i3] );
}

/*  MV_EDGE:
 *	Moves an arb edge (end1,end2) with bounding
 *	planes bp1 and bp2 through point "thru".
 *	The edge has (non-unit) slope "dir".
 *	Note that the fact that the normals here point in rather than
 *	out makes no difference for computing the correct intercepts.
 *	After the intercepts are found, they should be checked against
 *	the other faces to make sure that they are always "inside".
 */
int
mv_edge(thru, bp1, bp2, end1, end2, dir)
vect_t thru;
int bp1, bp2, end1, end2;
vect_t	dir;
{
	dbfloat_t *op;
	fastf_t	t1, t2;

	if( rt_isect_ray_plane( &t1, thru, dir, es_peqn[bp1] ) < 0 ||
	    rt_isect_ray_plane( &t2, thru, dir, es_peqn[bp2] ) < 0 )  {
		(void)printf("edge (direction) parallel to face normal\n");
		return( 1 );
	}

	op = &es_rec.s.s_values[end1*3];
	VJOIN1( op, thru, t1, dir );

	op = &es_rec.s.s_values[end2*3];
	VJOIN1( op, thru, t2, dir );

	return( 0 );
}
