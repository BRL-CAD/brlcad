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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
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
mk_half(struct rt_wdb *wdbp, const char *name, const fastf_t *norm, double d)
{
	struct rt_half_internal		*half;

	BU_GETSTRUCT( half, rt_half_internal );
	half->magic = RT_HALF_INTERNAL_MAGIC;
	VMOVE( half->eqn, norm );
	half->eqn[3] = d;

	return wdb_export( wdbp, name, (genptr_t)half, ID_HALF, mk_conv2mm );
}

/*
 *			M K _ G R I P
 *
 *  Make a grip psuedo solid.  Specified by a center, normal vector, and
 *  magnitude.
 */
int
mk_grip( 
	struct rt_wdb *wdbp,
	const char *name,
	const point_t center,
	const vect_t normal,
	const fastf_t magnitude )
{
	struct rt_grip_internal		*grip;

	BU_GETSTRUCT( grip, rt_grip_internal );
	grip->magic = RT_GRIP_INTERNAL_MAGIC;
	VMOVE( grip->center, center );
	VMOVE( grip->normal, normal );
	grip->mag = magnitude;

	return wdb_export( wdbp, name, (genptr_t)grip, ID_GRIP, mk_conv2mm );
}

/*
 *			M K _ R P P
 *
 *  Make a right parallelpiped.  Specified by minXYZ, maxXYZ.
 */
int
mk_rpp(struct rt_wdb *wdbp, const char *name, const fastf_t *min, const fastf_t *max)
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

	return( mk_arb8( wdbp, name, &pt8[0][X] ) );
}



/*			M K _ W E D G E
 *
 *  Makes a right angular wedge given a starting vertex located in the, lower
 *  left corner, an x and a z direction vector, x, y, and z lengths, and an
 *  x length for the top.  The y direcion vector is x cross z.
 */
int
mk_wedge(struct rt_wdb *wdbp, const char *name, const fastf_t *vert, const fastf_t *xdirv, const fastf_t *zdirv, fastf_t xlen, fastf_t ylen, fastf_t zlen, fastf_t x_top_len)
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

	return( mk_arb8(wdbp, name, &pts[0][X]) );
}


/*
 *			M K _ A R B 4
 */
int
mk_arb4(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)
             		      
          		      
             	     	/* [4*3] */
{
	point_t	pt8[8];

	VMOVE( pt8[0], &pts[0*3] );
	VMOVE( pt8[1], &pts[1*3] );
	VMOVE( pt8[2], &pts[2*3] );
	VMOVE( pt8[3], &pts[2*3] );	/* shared point for base */

	VMOVE( pt8[4], &pts[3*3] );	/* top point */
	VMOVE( pt8[5], &pts[3*3] );
	VMOVE( pt8[6], &pts[3*3] );
	VMOVE( pt8[7], &pts[3*3] );

	return( mk_arb8( wdbp, name, &pt8[0][X] ) );
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
mk_arb8(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)
             		      
          		      
             	     		/* [24] */
{
	register int i;
	struct rt_arb_internal	*arb;

	BU_GETSTRUCT( arb, rt_arb_internal );
	arb->magic = RT_ARB_INTERNAL_MAGIC;
#	include "noalias.h"
	for( i=0; i < 8; i++ )  {
		VMOVE( arb->pt[i], &pts[i*3] );
	}

	return wdb_export( wdbp, name, (genptr_t)arb, ID_ARB8, mk_conv2mm );
}

/*
 *			M K _ S P H
 *
 *  Make a sphere with the given center point and radius.
 */
int
mk_sph(struct rt_wdb *wdbp, const char *name, const fastf_t *center, fastf_t radius)
{
	struct rt_ell_internal	*ell;

	BU_GETSTRUCT( ell, rt_ell_internal );
	ell->magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE( ell->v, center );
	VSET( ell->a, radius, 0, 0 );
	VSET( ell->b, 0, radius, 0 );
	VSET( ell->c, 0, 0, radius );

	return wdb_export( wdbp, name, (genptr_t)ell, ID_ELL, mk_conv2mm );
}

/*
 *			M K _ E L L
 *
 *  Make an ellipsoid at the given center point with 3 perp. radius vectors.
 *  The eccentricity of the ellipsoid is controlled by the relative
 *  lengths of the three radius vectors.
 */
int
mk_ell(struct rt_wdb *wdbp, const char *name, const fastf_t *center, const fastf_t *a, const fastf_t *b, const fastf_t *c)
{
	struct rt_ell_internal	*ell;

	BU_GETSTRUCT( ell, rt_ell_internal );
	ell->magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE( ell->v, center );
	VMOVE( ell->a, a );
	VMOVE( ell->b, b );
	VMOVE( ell->c, c );

	return wdb_export( wdbp, name, (genptr_t)ell, ID_ELL, mk_conv2mm );
}

/*
 *			M K _ T O R
 *
 *  Make a torus.  Specify center, normal,
 *  r1: distance from center point to center of solid part,
 *  r2: radius of solid part.
 */
int
mk_tor(struct rt_wdb *wdbp, const char *name, const fastf_t *center, const fastf_t *inorm, double r1, double r2)
{
	struct rt_tor_internal	*tor;

	BU_GETSTRUCT( tor, rt_tor_internal );
	tor->magic = RT_TOR_INTERNAL_MAGIC;
	VMOVE( tor->v, center );
	VMOVE( tor->h, inorm );
	tor->r_a = r1;
	tor->r_h = r2;

	return wdb_export( wdbp, name, (genptr_t)tor, ID_TOR, mk_conv2mm );
}

/*
 *			M K _ R C C
 *
 *  Make a Right Circular Cylinder (special case of the TGC).
 */
int
mk_rcc(struct rt_wdb *wdbp, const char *name, const fastf_t *base, const fastf_t *height, fastf_t radius)
{
	vect_t	cross1, cross2;
	vect_t	a, b;

	if( MAGSQ(height) <= SQRT_SMALL_FASTF )
		return -2;

	/* Create two mutually perpendicular vectors, perpendicular to H */
	mat_vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( a, cross1, radius );
	VSCALE( b, cross2, radius );

	return mk_tgc( wdbp, name, base, height, a, b, a, b );
}

/*
 *			M K _ T G C
 *
 *  Make a Truncated General Cylinder.
 */
int
mk_tgc(struct rt_wdb *wdbp, const char *name, const fastf_t *base, const fastf_t *height, const fastf_t *a, const fastf_t *b, const fastf_t *c, const fastf_t *d)
{
	struct rt_tgc_internal	*tgc;

	BU_GETSTRUCT( tgc, rt_tgc_internal );
	tgc->magic = RT_TGC_INTERNAL_MAGIC;
	VMOVE( tgc->v, base );
	VMOVE( tgc->h, height );
	VMOVE( tgc->a, a );
	VMOVE( tgc->b, b );
	VMOVE( tgc->c, c );
	VMOVE( tgc->d, d );

	return wdb_export( wdbp, name, (genptr_t)tgc, ID_TGC, mk_conv2mm );
}


/*			M K _ C O N E
 *
 *  Makes a right circular cone given the center point of the base circle,
 *  a direction vector, a scalar height, and the radii at each end of the
 *  cone.
 */
int
mk_cone(struct rt_wdb *wdbp, const char *name, const fastf_t *base, const fastf_t *dirv, fastf_t height, fastf_t rad1, fastf_t rad2)
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

	return( mk_tgc(wdbp, name, base, hgtv, avec, bvec, cvec, dvec) );
}


/*
 *		M K _ T R C _ H
 *
 *  mk_trc( name, base, height, radius1, radius2 )
 *  Make a truncated right cylinder, with base and height.
 *  Not just called mk_trc() to avoid conflict with a previous routine
 *  of that name with different calling sequence.
 */
int
mk_trc_h(struct rt_wdb *wdbp, const char *name, const fastf_t *base, const fastf_t *height, fastf_t radbase, fastf_t radtop)
{
	vect_t	cross1, cross2;
	vect_t	a, b, c, d;

	if( MAGSQ(height) <= SQRT_SMALL_FASTF )
		return -2;

	/* Create two mutually perpendicular vectors, perpendicular to H */
	vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( a, cross1, radbase );
	VSCALE( b, cross2, radbase );

	VSCALE( c, cross1, radtop );
	VSCALE( d, cross2, radtop );

	return mk_tgc( wdbp, name, base, height, a, b, c, d );
}

/*
 *			M K _ T R C _ T O P
 *
 *  Convenience wrapper for mk_trc_h().
 */
int
mk_trc_top(struct rt_wdb *wdbp, const char *name, const fastf_t *ibase, const fastf_t *itop, fastf_t radbase, fastf_t radtop)
{
	vect_t	height;

	VSUB2( height, itop, ibase );
	return( mk_trc_h( wdbp, name, ibase, height, radbase, radtop ) );
}

/*
 *			M K _ R P C
 *
 *  Makes a right parabolic cylinder given the origin, or main vertex,
 *  a height vector, a breadth vector (B . H must be 0), and a scalar
 *  rectangular half-width (for the top of the rpc).
 */
int
mk_rpc(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	double half_w )
{
	struct rt_rpc_internal	*rpc;

	BU_GETSTRUCT( rpc, rt_rpc_internal );
	rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;

	VMOVE( rpc->rpc_V, vert );
	VMOVE( rpc->rpc_H, height );
	VMOVE( rpc->rpc_B, breadth );
	rpc->rpc_r = half_w;

	return wdb_export( wdbp, name, (genptr_t)rpc, ID_RPC, mk_conv2mm );
}

/*
 *			M K _ R H C
 *
 *  Makes a right hyperbolic cylinder given the origin, or main vertex,
 *  a height vector, a breadth vector (B . H must be 0), a scalar
 *  rectangular half-width (for the top of the rpc), and the scalar
 *  distance from the tip of the hyperbola to the intersection of the
 *  asymptotes.
 */
int
mk_rhc(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t	half_w,
	fastf_t asymp )
{
	struct rt_rhc_internal	*rhc;

	BU_GETSTRUCT( rhc, rt_rhc_internal );
	rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;

	VMOVE( rhc->rhc_V, vert );
	VMOVE( rhc->rhc_H, height );
	VMOVE( rhc->rhc_B, breadth );
	rhc->rhc_r = half_w;
	rhc->rhc_c = asymp;

	return wdb_export( wdbp, name, (genptr_t)rhc, ID_RHC, mk_conv2mm );
}

/*
 *			M K _ E P A
 *
 *  Makes an elliptical paraboloid given the origin, a height vector H,
 *  a unit vector A along the semi-major axis (A . H must equal 0), and
 *  the scalar lengths, r1 and r2, of the semi-major and -minor axes.
 */
int
mk_epa(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t r1,
	fastf_t r2 )
{
	struct rt_epa_internal	*epa;

	BU_GETSTRUCT( epa, rt_epa_internal );
	epa->epa_magic = RT_EPA_INTERNAL_MAGIC;

	VMOVE( epa->epa_V, vert );
	VMOVE( epa->epa_H, height );
	VMOVE( epa->epa_Au, breadth );
	epa->epa_r1 = r1;
	epa->epa_r2 = r2;

	return wdb_export( wdbp, name, (genptr_t)epa, ID_EPA, mk_conv2mm );
}

/*
 *			M K _ E H Y
 *
 *  Makes an elliptical hyperboloid given the origin, a height vector H,
 *  a unit vector A along the semi-major axis (A . H must equal 0),
 *  the scalar lengths, r1 and r2, of the semi-major and -minor axes,
 *  and the distance c between the tip of the hyperboloid and the vertex
 *  of the asymptotic cone.
 */
int
mk_ehy(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t r1,
	fastf_t r2,
	fastf_t c )
{
	struct rt_ehy_internal	*ehy;

	BU_GETSTRUCT( ehy, rt_ehy_internal );
	ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;

	VMOVE( ehy->ehy_V, vert );
	VMOVE( ehy->ehy_H, height );
	VMOVE( ehy->ehy_Au, breadth );
	ehy->ehy_r1 = r1;
	ehy->ehy_r2 = r2;
	ehy->ehy_c = c;

	return wdb_export( wdbp, name, (genptr_t)ehy, ID_EHY, mk_conv2mm );
}

/*
 *			M K _ E T O
 *
 *  Makes an elliptical torus given the origin, a plane normal vector N,
 *  a vector C along the semi-major axis of the elliptical cross-section,
 *  the scalar lengths r and rd, of the radius of revolution and length
 *  of semi-minor axis of the elliptical cross section.
 */
int
mk_eto(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t norm,
	const vect_t smajor,
	fastf_t rrot,
	fastf_t sminor )
{
	struct rt_eto_internal	*eto;

	BU_GETSTRUCT( eto, rt_eto_internal );
	eto->eto_magic = RT_ETO_INTERNAL_MAGIC;

	VMOVE( eto->eto_V, vert );
	VMOVE( eto->eto_N, norm );
	VMOVE( eto->eto_C, smajor );
	eto->eto_r = rrot;
	eto->eto_rd = sminor;

	return wdb_export( wdbp, name, (genptr_t)eto, ID_ETO, mk_conv2mm );
}
