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
#include "db.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 * Input Vector Fields
 */
#define F(i)	(rec.s.s_values+(i-1)*3)
#define F1	(rec.s.s_values+(1-1)*3)
#define F2	(rec.s.s_values+(2-1)*3)
#define F3	(rec.s.s_values+(3-1)*3)
#define F4	(rec.s.s_values+(4-1)*3)
#define F5	(rec.s.s_values+(5-1)*3)
#define F6	(rec.s.s_values+(6-1)*3)
#define F7	(rec.s.s_values+(7-1)*3)
#define F8	(rec.s.s_values+(8-1)*3)

/*
 *			M K _ I D
 *
 *  Make a database header (ID) record.
 */
int
mk_id( fp, title )
FILE	*fp;
char	*title;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.i.i_id = ID_IDENT;
	rec.i.i_units = ID_MM_UNIT;
	strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title) );
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ H A L F
 *
 *  Make a halfspace.  Specified by distance from origin, and
 *  outward pointing normal vector.
 */
int
mk_half( fp, name, norm, d )
FILE	*fp;
char	*name;
vect_t	norm;
double	d;
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
FILE	*fp;
char	*name;
point_t	min, max;
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
point_t		vert;
vect_t		xdirv;
vect_t		zdirv;
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
FILE	*fp;
char	*name;
point_t	pts[];
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
FILE	*fp;
char	*name;
point_t	pts[];
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
FILE	*fp;
char	*name;
point_t	center;
fastf_t	radius;
{
	union record rec;
	fastf_t		nrad = radius * mk_conv2mm;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	NAMEMOVE(name,rec.s.s_name);

	VSCALE( &rec.s.s_values[0], center, mk_conv2mm );
	VSET( &rec.s.s_values[3], nrad, 0, 0 );
	VSET( &rec.s.s_values[6], 0, nrad, 0 );
	VSET( &rec.s.s_values[9], 0, 0, nrad );

	if( fwrite( (char *) &rec, sizeof(rec), 1, fp) != 1 )
		return(-1);
	return(0);
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
FILE	*fp;
char	*name;
point_t	center;
vect_t	a, b, c;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	NAMEMOVE(name,rec.s.s_name);

	VSCALE( &rec.s.s_values[0], center, mk_conv2mm );
	VSCALE( &rec.s.s_values[3], a, mk_conv2mm );
	VSCALE( &rec.s.s_values[6], b, mk_conv2mm );
	VSCALE( &rec.s.s_values[9], c, mk_conv2mm );

	if( fwrite( (char *) &rec, sizeof(rec), 1, fp) != 1 )
		return(-1);
	return(0);
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
FILE	*fp;
char	*name;
point_t	center;
vect_t	inorm;
double	r1, r2;
{
	union record rec;
	vect_t	norm;
	vect_t	cross1, cross2;
	double	r3, r4;
	double	m2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = TOR;
	NAMEMOVE(name,rec.s.s_name);

	/* Validate that 0 < r2 <= r1 */
	if( r2 <= 0.0 )  {
		fprintf(stderr, "mk_tor(%s):  illegal r2=%.12e <= 0\n",
			name, r2);
		return(-1);
	}
	if( r2 > r1 )  {
		fprintf(stderr, "mk_tor(%s):  illegal r2=%.12e > r1=%.12e\n",
			name, r2, r1);
		return(-1);
	}

	r1 *= mk_conv2mm;
	r2 *= mk_conv2mm;
	r3=r1-r2;	/* Radius to inner circular edge */
	r4=r1+r2;	/* Radius to outer circular edge */

	VSCALE( F1, center, mk_conv2mm );

	VMOVE( norm, inorm );
	m2 = MAGNITUDE( norm );		/* F2 is NORMAL to torus */
	if( m2 <= SQRT_SMALL_FASTF )  {
		(void)fprintf(stderr, "mk_tor(%s): normal magnitude is zero!\n", name);
		return(-1);		/* failure */
	}
	m2 = 1.0/m2;
	VSCALE( norm, norm, m2 );	/* Give normal unit length */
	VSCALE( F2, norm, r2 );		/* Give F2 normal radius length */

	/* Create two mutually perpendicular vectors, perpendicular to Norm */
	vec_ortho( cross1, norm );
	VCROSS( cross2, cross1, norm );
	VUNITIZE( cross2 );

	/* F3, F4 are perpendicular, goto center of solid part */
	VSCALE( F3, cross1, r1 );
	VSCALE( F4, cross2, r1 );

	/* The rest of these provide no real extra information */
	/* F5, F6 are perpendicular, goto inner edge of ellipse */
	VSCALE( F5, cross1, r3 );
	VSCALE( F6, cross2, r3 );

	/* F7, F8 are perpendicular, goto outer edge of ellipse */
	VSCALE( F7, cross1, r4 );
	VSCALE( F8, cross2, r4 );
	
	if( fwrite( (char *) &rec, sizeof(rec), 1, fp) != 1 )
		return(-1);	/* failure */
	return(0);		/* OK */
}

/*
 *			M K _ R C C
 *
 *  Make a Right Circular Cylinder (special case of the TGC).
 */
int
mk_rcc( fp, name, base, height, radius )
FILE	*fp;
char	*name;
point_t	base;
vect_t	height;
fastf_t	radius;
{
	union record rec;
	vect_t	cross1, cross2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	NAMEMOVE(name, rec.s.s_name);

	/* Units conversion */
	radius *= mk_conv2mm;
	VSCALE( F1, base, mk_conv2mm );
	VSCALE( F2, height, mk_conv2mm  );

	/* Create two mutually perpendicular vectors, perpendicular to H */
	vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( F3, cross1, radius );
	VSCALE( F4, cross2, radius );
	VMOVE( F5, F3);
	VMOVE( F6, F4);

	if( fwrite( (char *)&rec, sizeof( rec), 1, fp) != 1 )
		return(-1);
	return(0);		/* OK */
}

/*
 *			M K _ T G C
 *
 *  Make a Truncated General Cylinder.
 */
int
mk_tgc( fp, name, base, height, a, b, c, d )
FILE	*fp;
char	*name;
point_t	base;
vect_t	height;
vect_t	a, b;
vect_t	c, d;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	NAMEMOVE(name, rec.s.s_name);

	/* Really, converting from fastf_t to dbfloat_t here */
	VSCALE( F1, base, mk_conv2mm );
	VSCALE( F2, height, mk_conv2mm );
	VSCALE( F3, a, mk_conv2mm );
	VSCALE( F4, b, mk_conv2mm );
	VSCALE( F5, c, mk_conv2mm );
	VSCALE( F6, d, mk_conv2mm );

	if( fwrite( (char *)&rec, sizeof( rec), 1, fp) != 1 )
		return(-1);
	return(0);		/* OK */
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
point_t		base;
vect_t		dirv;
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

	VMOVE(h_unitv, dirv);
	VUNITIZE(h_unitv);
	VSCALE(hgtv, h_unitv, height);

	/* Now make a, b, c, and d vectors. */

	vec_ortho(a, h_unitv);
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
mk_trc( fp, name, ibase, iheight, radbase, radtop )
FILE	*fp;
char	*name;
point_t	ibase;
vect_t	iheight;
fastf_t	radbase;
fastf_t	radtop;
{
	point_t	base;
	vect_t	height;
	union record rec;
	vect_t	cross1, cross2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	NAMEMOVE(name, rec.s.s_name);

	/* Units conversion */
	radbase *= mk_conv2mm;
	radtop *= mk_conv2mm;
	VSCALE( base, ibase, mk_conv2mm );
	VSCALE( height, iheight, mk_conv2mm );

	VMOVE( F1, base );
	VMOVE( F2, height );

	/* Create two mutually perpendicular vectors, perpendicular to H */
	vec_ortho( cross1, height );
	VCROSS( cross2, cross1, height );
	VUNITIZE( cross2 );

	VSCALE( F3, cross1, radbase );
	VSCALE( F4, cross2, radbase );

	VSCALE( F5, cross1, radtop );
	VSCALE( F6, cross2, radtop );

	if( fwrite( (char *)&rec, sizeof( rec), 1, fp) != 1 )
		return(-1);
	return(0);	/* OK */
}

/*
 *			M K _ T R C _ T O P
 *
 *  Convenience wrapper for mk_trc().
 */
int
mk_trc_top( fp, name, ibase, itop, radbase, radtop )
FILE	*fp;
char	*name;
point_t	ibase;
point_t	itop;
fastf_t	radbase;
fastf_t	radtop;
{
	vect_t	height;

	VSUB2( height, itop, ibase );
	return( mk_trc( fp, name, ibase, height, radbase, radtop ) );
}

/*
 *			M K _ P O L Y S O L I D
 *
 *  Make the header record for a polygon solid.
 *  Must be followed by 1 or more mk_poly() or mk_fpoly() calls
 *  before any other mk_* routines.
 */
int
mk_polysolid( fp, name )
FILE	*fp;
char	*name;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.p.p_id = ID_P_HEAD;
	NAMEMOVE( name, rec.p.p_name );
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ P O L Y
 *
 *  Must follow a call to mk_polysolid(), mk_poly(), or mk_fpoly().
 */
int
mk_poly( fp, npts, verts, norms )
FILE	*fp;
int	npts;
fastf_t	verts[][3];
fastf_t	norms[][3];
{
	union record rec;
	register int i,j;

	if( npts < 3 || npts > 5 )  {
		fprintf(stderr,"mk_poly:  npts=%d is bad\n", npts);
		return(-1);
	}

	bzero( (char *)&rec, sizeof(rec) );
	rec.q.q_id = ID_P_DATA;
	rec.q.q_count = npts;
	for( i=0; i<npts; i++ )  {
		for( j=0; j<3; j++ )  {
			rec.q.q_verts[i][j] = verts[i][j] * mk_conv2mm;
			rec.q.q_norms[i][j] = norms[i][j] * mk_conv2mm;
		}
	}
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1)
		return(-1);
	return(0);
}

/*
 *			M K _ F P O L Y
 *
 *  Must follow a call to mk_polysolid(), mk_poly(), or mk_fpoly().
 */
int
mk_fpoly( fp, npts, verts )
FILE	*fp;
int	npts;
fastf_t	verts[][3];
{
	int	i;
	vect_t	v1, v2, norms[5];

	if( npts < 3 || npts > 5 )  {
		fprintf(stderr,"mk_poly:  npts=%d is bad\n", npts);
		return(-1);
	}

	VSUB2( v1, verts[1], verts[0] );
	VSUB2( v2, verts[npts-1], verts[0] );
	VCROSS( norms[0], v1, v2 );
	VUNITIZE( norms[0] );
	for( i = 1; i < npts; i++ ) {
		VMOVE( norms[i], norms[0] );
	}
	return( mk_poly(fp, npts, verts, norms) );
}
