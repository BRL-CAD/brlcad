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
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
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

#define PI	3.14159265358979323

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

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
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.i.i_id = ID_IDENT;
	rec.i.i_units = ID_MM_UNIT;
	strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title) );
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
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
point_t	norm;
double	d;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = HALFSPACE;
	NAMEMOVE( name, rec.s.s_name );
	VMOVE( rec.s.s_values, norm );
	rec.s.s_values[3] = d;
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
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
	register int i;
	point_t	pt8[8];

	VSET( pt8[0], min[X], min[Y], min[Z] );
	VSET( pt8[1], max[X], min[Y], min[Z] );
	VSET( pt8[2], max[X], max[Y], min[Z] );
	VSET( pt8[3], min[X], max[Y], min[Z] );

	VSET( pt8[4], min[X], min[Y], max[Z] );
	VSET( pt8[5], max[X], min[Y], max[Z] );
	VSET( pt8[6], max[X], max[Y], max[Z] );
	VSET( pt8[7], min[X], max[Y], max[Z] );

	mk_arb8( fp, name, pt8 );
	return(0);
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
	register int i;
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
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENARB8;
	NAMEMOVE( name, rec.s.s_name );
	VMOVE( &rec.s.s_values[3*0], pts[0] );
	for( i=1; i<8; i++ )  {
		VSUB2( &rec.s.s_values[3*i], pts[i], pts[0] );
	}
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
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
	register int i;
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	rec.s.s_cgtype = SPH;
	NAMEMOVE(name,rec.s.s_name);

	VMOVE( &rec.s.s_values[0], center );
	VSET( &rec.s.s_values[3], radius, 0, 0 );
	VSET( &rec.s.s_values[6], 0, radius, 0 );
	VSET( &rec.s.s_values[9], 0, 0, radius );
	
	fwrite( (char *) &rec, sizeof(rec), 1, fp);
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
	register int i;
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	NAMEMOVE(name,rec.s.s_name);

	VMOVE( &rec.s.s_values[0], center );
	VMOVE( &rec.s.s_values[3], a );
	VMOVE( &rec.s.s_values[6], b );
	VMOVE( &rec.s.s_values[9], c );
	
	fwrite( (char *) &rec, sizeof(rec), 1, fp);
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
mk_tor( fp, name, center, norm, r1, r2 )
FILE	*fp;
char	*name;
point_t	center;
vect_t	norm;
double	r1, r2;
{
	register int i;
	static union record rec;
	vect_t	work;
	double	r3, r4;
	double	m1, m2, m3, m4, m5, m6;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = TOR;
	rec.s.s_cgtype = TOR;
	NAMEMOVE(name,rec.s.s_name);

	if( r1 <= 0 || r2 <= 0 || r1 <= r2 )  {
		fprintf(stderr, "mk_tor(%s):  illegal r1=%g, r2=%g\n",
			name, r1, r2);
		return(-1);
	}
	r3=r1-r2;	/* Radius to inner circular edge */
	r4=r1+r2;	/* Radius to outer circular edge */

	VMOVE( F1, center );
	VMOVE( F2, norm );

	/*
	 * To allow for V being (0,0,0), for VCROSS purposes only,
	 * we add (PI,PI,PI).  THIS DOES NOT GO OUT INTO THE FILE!!
	 * work[] must NOT be colinear with N[].  We check for this
	 * later.
	 */
	VMOVE(work, center);
	work[0] += PI;
	work[1] += PI;
	work[2] += PI;

	m2 = MAGNITUDE( F2 );		/* F2 is NORMAL to torus */
	if( m2 <= 0.0 )  {
		(void)fprintf(stderr, "mk_tor(%s): normal magnitude is zero!\n", name);
		return(-1);		/* failure */
	}
	VSCALE( F2, F2, r2/m2 );	/* Give it radius length */

	/* F3, F4 are perpendicular, goto center of Torus (solid part), for top/bottom */
	VCROSS(F3,work,F2);
	m1=MAGNITUDE(F3);
	if( m1 <= 0.0 )  {
		work[1] = 0.0;		/* Vector is colinear, so */
		work[2] = 0.0;		/* make it different */
		VCROSS(F3,work,F2);
		m1=MAGNITUDE(F3);
		if( m1 <= 0.0 )  {
			(void)fprintf(stderr, "mk_tor(%s): cross product vector is zero!\n", name);
			return(-1);	/* failure */
		}
	}
	VSCALE(F3,F3,r1/m1);

	VCROSS(F4,F3,F2);
	m3=MAGNITUDE(F4);
	if( m3 <= 0.0 )  {
		(void)fprintf(stderr, "mk_tor(%s): magnitude m3 is zero!\n", name);
		return(-1);	 /* failure */
	}

	VSCALE(F4,F4,r1/m3);
	m5 = MAGNITUDE(F3);
	m6 = MAGNITUDE( F4 );
	if( m5 <= 0.0 || m6 <= 0.0 )  {
		(void)fprintf(stderr, "mk_tor(%s): magnitude m5/m6 is zero!\n", name);
		return(-1);	/* failure */
	}

	/* F5, F6 are perpendicular, goto inner edge of ellipse */
	VSCALE( F5, F3, r3/m5 );
	VSCALE( F6, F4, r3/m6 );

	/* F7, F8 are perpendicular, goto outer edge of ellipse */
	VSCALE( F7, F3, r4/m5 );
	VSCALE( F8, F4, r4/m6 );
	
	fwrite( (char *) &rec, sizeof(rec), 1, fp);
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
	static union record rec;
	fastf_t m1, m2;
	vect_t	tvec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	rec.s.s_cgtype = RCC;
	NAMEMOVE(name, rec.s.s_name);

	VMOVE( F1, base );
	VMOVE( F2, height  );
	VMOVE( tvec, base );
	tvec[0] += PI;
	tvec[1] += PI;
	tvec[2] += PI;
	VCROSS( F3, tvec, F2 );
	m1 = MAGNITUDE( F3 );
	if( m1 == 0.0 )  {
		tvec[1] = 0.0;		/* Vector is colinear, so */
		tvec[2] = 0.0;		/* make it different */
		VCROSS( F3, tvec, F2 );
		m1 = MAGNITUDE( F3 );
		if( m1 == 0.0 )  {
			(void)printf("ERROR, magnitude is zero!\n");
			return(-1);	/* failure */
		}
	}
	VSCALE( F3, F3, radius/m1 );
	VCROSS( F4, F2, F3 );
	m2 = MAGNITUDE( F4 );
	if( m2 == 0.0 )  {
		(void)printf("ERROR, magnitude is zero!\n");
		return(-1);	/* failure */
	}

	VSCALE( F4, F4, radius/m2 );

	VMOVE( F5, F3);
	VMOVE( F6, F4);

	fwrite( (char *)&rec, sizeof( rec), 1, fp);
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
	static union record rec;
	fastf_t m1, m2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	rec.s.s_cgtype = TGC;
	NAMEMOVE(name, rec.s.s_name);

	/* Really, converting from fastf_t to dbfloat_t here */
	VMOVE( F1, base );
	VMOVE( F2, height );
	VMOVE( F3, a );
	VMOVE( F4, b );
	VMOVE( F5, c );
	VMOVE( F6, d );

	fwrite( (char *)&rec, sizeof( rec), 1, fp);
	return(0);		/* OK */
}

/*		M K _ T R C
 *
 *  mk_trc( name, base, height, radius)
 *  Make a truncated right cylinder. 
 */
int
mk_trc( fp, name, base, top, radbase,radtop )
FILE	*fp;
char	*name;
point_t	base;
point_t	top;
fastf_t	radbase;
fastf_t	radtop;
{
	vect_t	height;
	static union record rec;
	static float pi = 3.14159265358979323264;
	fastf_t m1, m2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	rec.s.s_cgtype = TRC;
	NAMEMOVE(name, rec.s.s_name);

	VSUB2(height,top,base);

	VMOVE( F1, base );
	VMOVE( F2, height  );
	base[0] += pi;
	base[1] += pi;
	base[2] += pi;
	VCROSS( F3, base, F2 );
	m1 = MAGNITUDE( F3 );
	if( m1 == 0.0 )  {
		base[1] = 0.0;		/* Vector is colinear, so */
		base[2] = 0.0;		/* make it different */
		VCROSS( F3, base, F2 );
		m1 = MAGNITUDE( F3 );
		if( m1 == 0.0 )  {
			(void)printf("ERROR, magnitude is zero!\n");
			return(-1);	/* failure */
		}
	}
	VSCALE( F3, F3, radbase/m1 );
	VCROSS( F4, F2, F3 );
	m2 = MAGNITUDE( F4 );
	if( m2 == 0.0 )  {
		(void)printf("ERROR, magnitude is zero!\n");
		return(-1);	/* failure */
	}

	VSCALE( F4, F4, radbase/m2 );

	VMOVE(F5,F3);
	VMOVE(F6,F4);

	m1 = MAGNITUDE( F5 );
	if( m1 == 0.0 )  {
		base[1] = 0.0;		/* Vector is colinear, so */
		base[2] = 0.0;		/* make it different */
		VCROSS( F5, F2, top );
		m1 = MAGNITUDE( F5 );
		if( m1 == 0.0 )  {
			(void)printf("ERROR, magnitude is zero!\n");
			return(-1);	/* failure */
		}
	}

	VSCALE( F5, F5, radtop/m1 );

	m2 = MAGNITUDE( F6 );
	if( m2 == 0.0 )  {
		(void)printf("ERROR, magnitude is zero!\n");
		return(-1);	/* failure */
	}

	VSCALE( F6, F6, radtop/m2 );

	fwrite( (char *)&rec, sizeof( rec), 1, fp);
	return(0);	/* OK */
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
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.p.p_id = ID_P_HEAD;
	NAMEMOVE( name, rec.p.p_name );
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
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
	static union record rec;
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
			rec.q.q_verts[i][j] = verts[i][j];
			rec.q.q_norms[i][j] = norms[i][j];
		}
	}
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
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
