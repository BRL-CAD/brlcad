/*
 *			T U B E . C
 *
 *  Program to generate a gun-tube as a procedural spline.
 *  The tube's core lies on the X axis.
 *
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright 
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
#include "wdb.h"

#include "../libspl/b_spline.h"
#include "../rt/mathtab.h"

extern struct b_spline *spl_new();

mat_t	identity;
double	degtorad = 0.0174532925199433;
double	inches2mm = 25.4;

#define N_CIRCLE_KNOTS	12
fastf_t	circle_knots[N_CIRCLE_KNOTS] = {
	0,	0,	0,
	1,	1,
	2,	2,
	3,	3,
	4,	4,	4
};

#define IRT2	0.70710678	/* 1/sqrt(2) */
#define NCOLS	9
/* When scaling, multiply only XYZ, not W */
fastf_t poly[NCOLS*4] = {
	0,	1,	0,	1,
	0,	IRT2,	IRT2,	IRT2,
	0,	0,	1,	1,
	0,	-IRT2,	IRT2,	IRT2,
	0,	-1,	0,	1,
	0,	-IRT2,	-IRT2,	IRT2,
	0,	0,	-1,	1,
	0,	IRT2,	-IRT2,	IRT2,
	0,	1,	0,	1
};

point_t	sample[1024];

double	radius;
double	length;
double	spacing;

main(argc, argv)
char	**argv;
{
	int	npts;
	register int i;
	int	frame;
	char	name[128];

	mk_id( stdout, "Spline Tube" );

	radius = 5 * inches2mm / 2;	/* 5" diameter */
	length = 187 * inches2mm;
	spacing = 100;			/* mm per sample */
	npts = ceil(length/spacing);
	fprintf(stderr,"radius=%gmm, length=%gmm, spacing=%gmm\n", radius, length, spacing);

	for( frame=0; frame<16; frame++ )  {
		/* Generate some dummy sample data */
		for( i=0; i<npts; i++ )  {
			sample[i][X] = i * spacing;
			sample[i][Y] = 0;
			sample[i][Z] = 4 * radius * sin(
				((double)i*i)/npts * 2 * 3.14159265358979323 +
				frame * 3.141592 * 2 / 8 );
		}

		sprintf( name, "tube%d", frame);
		build_spline( name, npts );
	}
}

build_spline( name, npts )
char	*name;
int	npts;
{
	struct b_spline *bp;
	register int i;
	int	nv;
	int	cur_kv;
	fastf_t	*meshp;
	register int col;
	int	row;
	vect_t	point;

	/*
	 *  This spline will look like a cylinder.
	 *  In the mesh, the circular cross section will be presented
	 *  across the first row by filling in the 9 (NCOLS) columns.
	 *
	 *  The U direction is across the first row,
	 *  and has NCOLS+order[U] positions, 12 in this instance.
	 *  The V direction is down the first column,
	 *  and has NROWS+order[V] positions.
	 */
	bp = spl_new( 3,	4,		/* u,v order */
		N_CIRCLE_KNOTS,	npts+6,		/* u,v knot vector size */
		npts+2,		NCOLS,		/* nrows, ncols */
		P4 );

	/*  Build the U knots */
	for( i=0; i<N_CIRCLE_KNOTS; i++ )
		bp->u_kv->knots[i] = circle_knots[i];

	/* Build the V knots */
	cur_kv = 0;		/* current knot value */
	nv = 0;			/* current knot subscript */
	for( i=0; i<4; i++ )
		bp->v_kv->knots[nv++] = cur_kv;
	cur_kv++;
	for( i=4; i<(npts+4-2); i++ )
		bp->v_kv->knots[nv++] = cur_kv++;
	for( i=0; i<4; i++ )
		bp->v_kv->knots[nv++] = cur_kv;

	/*
	 *  The control mesh is stored in row-major order,
	 *  which works out well for us, as a row is one
	 *  circular slice through the tube.  So we just
	 *  have to write down the slices, one after another.
	 *  The first and last "slice" are the center points that
	 *  create the end caps.
	 */
	meshp = bp->ctl_mesh->mesh;

	/* Row 0 */
	for( col=0; col<9; col++ )  {
		*meshp++ = sample[0][X];
		*meshp++ = sample[0][Y];
		*meshp++ = sample[0][Z];
		*meshp++ = 1;
	}

	/* Rows 1..npts */
	for( i=0; i<npts; i++ )  {
		/* row = i; */
		VMOVE( point, sample[i] );
		for( col=0; col<9; col++ )  {
			register fastf_t h;

			h = poly[col*4+H];
			*meshp++ = poly[col*4+X]*radius + point[X]*h;
			*meshp++ = poly[col*4+Y]*radius + point[Y]*h;
			*meshp++ = poly[col*4+Z]*radius + point[Z]*h;
			*meshp++ = h;
		}
	}

	/* Row npts+1 */
	for( col=0; col<9; col++ )  {
		*meshp++ = sample[npts-1][X];
		*meshp++ = sample[npts-1][Y];
		*meshp++ = sample[npts-1][Z];
		*meshp++ = 1;
	}
		
	mk_bsolid( stdout, name, 1, 0.1 );
	mk_bsurf( stdout, bp );
	spl_sfree( bp );
}
