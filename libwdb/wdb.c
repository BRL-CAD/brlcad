/*
 *			W D B . C
 *
 *  Library for writing MGED databases from arbitrary procedures.
 *  Assumes that some of the structure of such databases are known
 *  by the calling routines.
 *
 *  It is expected that this library will grow as experience is gained.
 *  Routines for writing every permissible solid do not yet exist.
 *
 *  Note that routines which are passed point_t or vect_t or mat_t
 *  parameters (which are call-by-address) must be VERY careful to
 *  leave those parameters unmodified (eg, by scaling), so that the
 *  calling routine is not surprised.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *	Susanne Muuss, J.D.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ H A L F
 *
 *  Make a halfspace.  Specified by distance from origin, and
 *  outward pointing normal vector.
 */
int
mk_half( fp, name, norm, d )
FILE		*fp;
char		*name;
CONST vect_t	norm;
double		d;
{
	struct rt_half_internal		half;

	half.magic = RT_HALF_INTERNAL_MAGIC;
	VMOVE( half.eqn, norm );
	half.eqn[3] = d;

	return mk_export_fwrite( fp, name, (genptr_t)&half, ID_HALF );
}

/*
 *			M K _ R P P
 *
 *  Make a right parallelpiped.  Specified by minXYZ, maxXYZ.
 */
int
mk_rpp( fp, name, min, max )
FILE		*fp;
char		*name;
CONST point_t	min;
CONST point_t	max;
{
	point_t	pt8[8];

	VSET( pt8[0], min[X], min[Y], min[Z] );
	VSET( pt8[1], max[X], min[Y], min[Z] );
	VSET( pt8[2], max[X], max[Y], min[Z] );
	VSET( pt8[3], min[X], max[Y], min[Z] );

	VSET( pt8[4], min[X], min[Y], max[Z] );
	VSET( pt8[5], max[X], min[Y], max[Z] );
	VSET( pt8[6], max[X], max[Y], max[Z] );
	VSET( pt8[7], min[X], max[Y], max[Z] );

	return( mk_arb8( fp, name, pt8 ) );
}



/*			M K _ W E D G E
 *
 *  Makes a right angular wedge given a starting vertex located in the, lower
 *  left corner, an x and a z direction vector, x, y, and z lengths, and an
 *  x length for the top.  The y direcion vector is x cross z.
 */
int
mk_wedge(fp, name, vert, xdirv, zdirv, xlen, ylen, zlen, x_top_len)
FILE		*fp;
char		*name;
CONST point_t	vert;
CONST vect_t	xdirv;
CONST vect_t	zdirv;
fastf_t		xlen;
fastf_t		ylen;
fastf_t		zlen;
fastf_t		x_top_len;
{
	point_t		pts[8];		/* vertices for the wedge */
	vect_t		xvec;		/* x_axis vector */
	vect_t		txvec;		/* top x_axis vector */
	vect_t		yvec;		/* y-axis vector */
	vect_t		zvec;		/* z-axix vector */
	vect_t		x_unitv;	/* x-axis unit vector*/
	vect_t		z_unitv;	/* z-axis unit vector */
	vect_t		y_unitv;

	VMOVE( x_unitv, xdirv);
	VUNITIZE(x_unitv);
	VMOVE( z_unitv, zdirv );
	VUNITIZE(z_unitv);
	
	/* Make y_unitv */
	VCROSS(y_unitv, x_unitv, z_unitv);

	/* Scale all vectors. */
	VSCALE(xvec, x_unitv, xlen);
	VSCALE(txvec, x_unitv, x_top_len);
	VSCALE(zvec, z_unitv, zlen);
	VSCALE(yvec, y_unitv, ylen);

	/* Make bottom face */

	VMOVE(pts[0], vert);		/* Move given vertex into pts[0] */
	VADD2(pts[1], pts[0], xvec);	/* second vertex. */
	VADD2(pts[2], pts[1], yvec);	/* third vertex */
	VADD2(pts[3], pts[0], yvec);	/* foruth vertex */

	/* Make top face by extruding bottom face vertices */

	VADD2(pts[4], pts[0], zvec);	/* fifth vertex */
	VADD2(pts[5], pts[4], txvec);	/* sixth vertex */
	VADD2(pts[6], pts[5], yvec);	/* seventh vertex */
	VADD2(pts[7], pts[4], yvec);	/* eighth vertex */

	return( mk_arb8(fp, name, pts) );
}


/*
 *			M K _ A R B 4
 */
int
mk_arb4( fp, name, pts )
FILE		*fp;
char		*name;
CONST point_t	pts[4];
{
	point_t	pt8[8];

	VMOVE( pt8[0], pts[0] );
	VMOVE( pt8[1], pts[1] );
	VMOVE( pt8[2], pts[2] );
	VMOVE( pt8[3], pts[2] );	/* shared point for base */

	VMOVE( pt8[4], pts[3] );	/* top point */
	VMOVE( pt8[5], pts[3] );
	VMOVE( pt8[6], pts[3] );
	VMOVE( pt8[7], pts[3] );

	return( mk_arb8( fp, name, pt8 ) );
}

/*
 *			M K _ A R B 8
 *
 *  All plates with 4 points must be co-planar.
 *  If there are degeneracies (i.e., all 8 vertices are not distinct),
 *  then certain requirements must be met.
 *  If we think of the ARB8 as having a top and a bottom plate,
 *  the first four points listed must lie on one plate, and
 *  the second four points listed must lie on the other plate.
 */
int
mk_arb8( fp, name, pts )
FILE		*fp;
char		*name;
CONST point_t	pts[8];
{
	register int i;
	struct rt_arb_internal	arb;

	arb.magic = RT_ARB_INTERNAL_MAGIC;
#	include "noalias.h"
	for( i=0; i < 8; i++ )  {
		VMOVE( arb.pt[i], pts[i] );
	}

	return mk_export_fwrite( fp, name, (genptr_t)&arb, ID_ARB8 );
}

/*
 *			M K _ S P H
 *
 *  Make a sphere with the given center point and radius.
 */
int
mk_sph( fp, name, center, radius )
FILE		*fp;
char		*name;
CONST point_t	center;
fastf_t		radius;
{
	struct rt_ell_internal	ell;

	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE( ell.v, center );
	VSET( ell.a, radius, 0, 0 );
	VSET( ell.b, 0, radius, 0 );
	VSET( ell.c, 0, 0, radius );

	return mk_export_fwrite( fp, name, (genptr_t)&ell, ID_ELL );
}

/*
 *			M K _ E L L
 *
 *  Make an ellipsoid at the given center point with 3 perp. radius vectors.
 *  The eccentricity of the ellipsoid is controlled by the relative
 *  lengths of the three radius vectors.
 */
int
mk_ell( fp, name, center, a, b, c )
FILE		*fp;
char		*name;
CONST point_t	center;
CONST vect_t	a, b, c;
{
	struct rt_ell_internal	ell;

	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE( ell.v, center );
	VMOVE( ell.a, a );
	VMOVE( ell.b, b );
	VMOVE( ell.c, c );

	return mk_export_fwrite( fp, name, (genptr_t)&ell, ID_ELL );
}

/*
 *			M K _ T O R
 *
 *  Make a torus.  Specify center, normal,
 *  r1: distance from center point to center of solid part,
 *  r2: radius of solid part.
 */
int
mk_tor( fp, name, center, inorm, r1, r2 )
FILE		*fp;
char		*name;
CONST point_t	center;
CONST vect_t	inorm;
double		r1, r2;
{
	struct rt_tor_internal	tor;

	tor.magic = RT_TOR_INTERNAL_MAGIC;
	VMOVE( tor.v, center );
	VMOVE( tor.h, inorm );
	tor.r_a = r1;
	tor.r_h = r2;

	return mk_export_fwrite( fp, name, (genptr_t)&tor, ID_TOR );
}

/*
 *			M K _ R C C
 *
 *  Make a Right Circular Cylinder (special case of the TGC).
 */
int
mk_rcc( fp, name, base, height, radius )
FILE		*fp;
char		*name;
CONST point_t	base;
CONST vect_t	height;
fastf_t		radius;
{
	vect_t	cross1, cross2;
	vect_t	a, b;

	/* Create two mutually perpendicular vectors, perpendicular to H */
	mat_vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( a, cross1, radius );
	VSCALE( b, cross2, radius );

	return mk_tgc( fp, name, base, height, a, b, a, b );
}

/*
 *			M K _ T G C
 *
 *  Make a Truncated General Cylinder.
 */
int
mk_tgc( fp, name, base, height, a, b, c, d )
FILE		*fp;
char		*name;
CONST point_t	base;
CONST vect_t	height;
CONST vect_t	a;
CONST vect_t	b;
CONST vect_t	c;
CONST vect_t	d;
{
	struct rt_tgc_internal	tgc;

	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VMOVE( tgc.v, base );
	VMOVE( tgc.h, height );
	VMOVE( tgc.a, a );
	VMOVE( tgc.b, b );
	VMOVE( tgc.c, c );
	VMOVE( tgc.d, d );

	return mk_export_fwrite( fp, name, (genptr_t)&tgc, ID_TGC );
}


/*			M K _ C O N E
 *
 *  Makes a right circular cone given the center point of the base circle,
 *  a direction vector, a scalar height, and the radii at each end of the
 *  cone.
 */
int
mk_cone( fp, name, base, dirv, height, rad1, rad2)
FILE		*fp;
char		*name;
CONST point_t	base;
CONST vect_t	dirv;
fastf_t		height;
fastf_t		rad1;
fastf_t		rad2;
{
	vect_t		a, avec;	/* one base radius vector */
	vect_t		b, bvec;	/* another base radius vector */
	vect_t		cvec;		/* nose radius vector */
	vect_t		dvec;		/* another nose radius vector */
	vect_t		h_unitv;	/* local copy of dirv */
	vect_t		hgtv;		/* height vector */
	fastf_t		f;

	if( (f = MAGNITUDE(dirv)) <= SQRT_SMALL_FASTF )
		return -2;
	f = 1/f;
	VSCALE( h_unitv, dirv, f );
	VSCALE(hgtv, h_unitv, height);

	/* Now make a, b, c, and d vectors. */

	mat_vec_ortho(a, h_unitv);
	VUNITIZE(a);
	VCROSS(b, h_unitv, a);
	VSCALE(avec, a, rad1);
	VSCALE(bvec, b, rad1);
	VSCALE(cvec, a, rad2);
	VSCALE(dvec, b, rad2);

	return( mk_tgc(fp, name, base, hgtv, avec, bvec, cvec, dvec) );
}


/*		M K _ T R C
 *
 *  mk_trc( name, base, height, radius1, radius2 )
 *  Make a truncated right cylinder. 
 */
int
mk_trc( fp, name, base, height, radbase, radtop )
FILE		*fp;
char		*name;
CONST point_t	base;
CONST vect_t	height;
fastf_t		radbase;
fastf_t		radtop;
{
	vect_t	cross1, cross2;
	vect_t	a, b, c, d;

	/* Create two mutually perpendicular vectors, perpendicular to H */
	vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( a, cross1, radbase );
	VSCALE( b, cross2, radbase );

	VSCALE( c, cross1, radtop );
	VSCALE( d, cross2, radtop );

	return mk_tgc( fp, name, base, height, a, b, c, d );
}

/*
 *			M K _ T R C _ T O P
 *
 *  Convenience wrapper for mk_trc().
 */
int
mk_trc_top( fp, name, ibase, itop, radbase, radtop )
FILE		*fp;
char		*name;
CONST point_t	ibase;
CONST point_t	itop;
fastf_t		radbase;
fastf_t		radtop;
{
	vect_t	height;

	VSUB2( height, itop, ibase );
	return( mk_trc( fp, name, ibase, height, radbase, radtop ) );
}
