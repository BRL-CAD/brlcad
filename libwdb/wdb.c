/*
 *			L I B W D B . C
 *
 * Library for writing databases.
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

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/*
 *			M K _ I D
 */
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
}

/*
 *			M K _ H A L F
 *
 *  Make a halfspace.  Specified by distance from origin, and
 *  outward pointing normal vector.
 */
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
}

/*
 *			M K _ R P P
 *
 *  Make a right parallelpiped.  Specified by minXYZ, maxXYZ.
 */
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
}

/*
 *			M K _ A R B 4
 */
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

	mk_arb8( fp, name, pt8 );
}

/*
 *			M K _ A R B 8
 *
 *  If there are degeneracies (ie, all 8 vertices are not distinct),
 *  then certain requirements must be met.
 *  If we think of the ARB8 as having a top and a bottom plate,
 *  the first four points listed must lie on one plate, and
 *  the second four points listed must lie on the other plate.
 */
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
}

/*
 *			M K _ S P H
 *
 * Make a sphere centered at point with radius r.
 */

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
}


/*
 * Input Vector Fields
 */
#define Fi	rec.s.s_values+(i-1)*3
#define F1	rec.s.s_values+(1-1)*3
#define F2	rec.s.s_values+(2-1)*3
#define F3	rec.s.s_values+(3-1)*3
#define F4	rec.s.s_values+(4-1)*3
#define F5	rec.s.s_values+(5-1)*3
#define F6	rec.s.s_values+(6-1)*3
#define F7	rec.s.s_values+(7-1)*3
#define F8	rec.s.s_values+(8-1)*3

/*
 * mk_rcc( name, base, height, radius)
 * Make a Right Circular Cylinder into a generalized truncated cyinder
 */
mk_rcc( fp, name, base, height, radius )
FILE	*fp;
char	*name;
point_t	base;
vect_t	height;
fastf_t	radius;
{
	static union record rec;
	static float pi = 3.14159265358979323264;
	fastf_t m1, m2;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENTGC;
	rec.s.s_cgtype = RCC;
	NAMEMOVE(name, rec.s.s_name);

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
}

/*
 *			M K _ M C O M B
 *
 *  Make a combination with material properties info
 */
mk_mcomb( fp, name, len, region, matname, matparm, override, rgb )
FILE	*fp;
char	*name;
char	*matname;
char	*matparm;
char	*rgb;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = 'R';
	else
		rec.c.c_flags = ' ';
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_length = len;
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( override )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
}


/*
 *			M K _ C O M B
 *
 *  Make a simple combination header.
 * Must be followed by 'len' mk_memb() calls before any other mk_* routines
 */
mk_comb( fp, name, len, region )
FILE	*fp;
char	*name;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = 'R';
	else
		rec.c.c_flags = ' ';
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_length = len;
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
}

/*
 *			M K _ M E M B
 *
 *  Must be part of combination/member clump of records.
 */
mk_memb( fp, name, mat, op )
FILE	*fp;
char	*name;
mat_t	mat;
int	op;
{
	static union record rec;
	register int i;

	bzero( (char *)&rec, sizeof(rec) );
	rec.M.m_id = ID_MEMB;
	rec.M.m_relation = op;
	NAMEMOVE( name, rec.M.m_instname );
	for( i=0; i<16; i++ )
		rec.M.m_mat[i] = mat[i];
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
}

/*
 *			M K _ P O L Y S O L I D
 *
 *  Make the header record for a polygon solid.
 *  Must be followed by 1 or more mk_facet() calls before
 *  any other mk_* routines
 */
mk_polysolid( fp, name )
FILE	*fp;
char	*name;
{
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.p.p_id = ID_P_HEAD;
	NAMEMOVE( name, rec.p.p_name );
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
}

/*
 *			M K _ F A C E T
 *
 *  Must follow a call to mk_polysolid() or mk_facet()
 */
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
