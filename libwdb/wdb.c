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
#define F(i)	rec.s.s_values+(i-1)*3
#define F1	rec.s.s_values+(1-1)*3
#define F2	rec.s.s_values+(2-1)*3
#define F3	rec.s.s_values+(3-1)*3
#define F4	rec.s.s_values+(4-1)*3
#define F5	rec.s.s_values+(5-1)*3
#define F6	rec.s.s_values+(6-1)*3
#define F7	rec.s.s_values+(7-1)*3
#define F8	rec.s.s_values+(8-1)*3

/*
 *			M K _ I D
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
mk_half( fp, name, d, norm )
FILE	*fp;
char	*name;
double	d;
point_t	norm;
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
 *  If there are degeneracies (ie, all 8 vertices are not distinct),
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
 * Make a sphere centered at point with radius r.
 */
int
mk_sph( fp, name, point, r)
FILE	*fp;
char	*name;
point_t	point;
fastf_t	r;
{
	register int i;
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	rec.s.s_cgtype = SPH;
	NAMEMOVE(name,rec.s.s_name);

	VMOVE( &rec.s.s_values[0], point );
	VSET( &rec.s.s_values[3], r, 0, 0 );
	VSET( &rec.s.s_values[6], 0, r, 0 );
	VSET( &rec.s.s_values[9], 0, 0, r );
	
	fwrite( (char *) &rec, sizeof(rec), 1, fp);
	return(0);
}

/*
 *			M K _ E L L
 *
 * Make an ellipsoid centered at point with 3 perp. radius vectors.
 * The eccentricity of the ellipsoid is controlled by the relative
 * lengths of the three radius vectors.
 */
int
mk_ell( fp, name, point, a, b, c)
FILE	*fp;
char	*name;
point_t	point;
vect_t	a, b, c;
{
	register int i;
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	NAMEMOVE(name,rec.s.s_name);

	VMOVE( &rec.s.s_values[0], point );
	VMOVE( &rec.s.s_values[3], a );
	VMOVE( &rec.s.s_values[6], b );
	VMOVE( &rec.s.s_values[9], c );
	
	fwrite( (char *) &rec, sizeof(rec), 1, fp);
	return(0);
}

/*
 *			M K _ T O R
 *
 * Make a torus.  Specify center, normal,
 * r1:  distance from center point to center of solid part,
 * and r2: radius of solid part.
 */
int
mk_tor( fp, name, center, n, r1, r2 )
FILE	*fp;
char	*name;
point_t	center;
vect_t	n;
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
	VMOVE( F2, n );

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
 * Make a Right Circular Cylinder into a generalized truncated cyinder
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

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	rec.s.s_cgtype = RCC;
	NAMEMOVE(name, rec.s.s_name);

	VMOVE( F1, base );
	VMOVE( F2, height  );
	F1[0] += PI;
	F1[1] += PI;
	F1[2] += PI;
	VCROSS( F3, F1, F2 );
	m1 = MAGNITUDE( F3 );
	if( m1 == 0.0 )  {
		F1[1] = 0.0;		/* Vector is colinear, so */
		F1[2] = 0.0;		/* make it different */
		VCROSS( F3, F1, F2 );
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
 * Make truncated general cylinder
 */
int
mk_tgc( fp, name, v, h, a, b, c, d )
FILE	*fp;
char	*name;
point_t	v;
vect_t	h;
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
	VMOVE( F1, v );
	VMOVE( F2, h );
	VMOVE( F3, a );
	VMOVE( F4, b );
	VMOVE( F5, c );
	VMOVE( F6, d );

	fwrite( (char *)&rec, sizeof( rec), 1, fp);
	return(0);		/* OK */
}

/*
 *			M K _ P O L Y S O L I D
 *
 *  Make the header record for a polygon solid.
 *  Must be followed by 1 or more mk_facet() calls before
 *  any other mk_* routines
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
 *			M K _ F A C E T
 *
 *  Must follow a call to mk_polysolid() or mk_facet()
 */
int
mk_facet( fp, npts, vert, norm )
FILE	*fp;
int	npts;
fastf_t	vert[][3];
fastf_t	norm[][3];
{
	static union record rec;
	register int i,j;

	if( npts < 3 || npts > 5 )  {
		fprintf(stderr,"mk_facet:  npts=%d is bad\n", npts);
		return(-1);
	}

	bzero( (char *)&rec, sizeof(rec) );
	rec.q.q_id = ID_P_DATA;
	rec.q.q_count = npts;
	for( i=0; i<npts; i++ )  {
		for( j=0; j<3; j++ )  {
			rec.q.q_verts[i][j] = vert[i][j];
			rec.q.q_norms[i][j] = norm[i][j];
		}
	}
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	return(0);
}
