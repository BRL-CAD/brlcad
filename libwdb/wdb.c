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
mk_id( title )
char *title;
{
	static union record rec;

	rec.i.i_id = ID_IDENT;
	rec.i.i_units = ID_MM_UNIT;
	strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title) );
	fwrite( (char *)&rec, sizeof(rec), 1, stdout );
}

/*
 *			M K _ A R B
 */
mk_arb( name, pts, npts )
char *name;
point_t pts[];
int npts;
{
	register int i;
	static union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENARB8;
	NAMEMOVE( name, rec.s.s_name );
	VMOVE( &rec.s.s_values[3*0], pts[0] );
	for( i=1; i<npts; i++ )  {
		VSUB2( &rec.s.s_values[3*i], pts[i], pts[0] );
	}
	for( ; i<8; i++ )  {
		VSET( &rec.s.s_values[3*i], 0, 0, 0 );
	}
	fwrite( (char *)&rec, sizeof(rec), 1, stdout );
}

/*
 *			M K _ S P H
 *
 * Make a sphere centered at sx, sy, sz with radius r.
 */

mk_sph(name,  sx, sy, sz, r)
char * name;
fastf_t sx, sy, sz, r;
{
	register int i;
	static union record rec;

#ifdef BSD
	bzero( (char *)&rec, sizeof(rec) );
#endif

	rec.s.s_id = ID_SOLID;
	rec.s.s_type = GENELL;
	rec.s.s_cgtype = SPH;
	NAMEMOVE(name,rec.s.s_name);

	VSET( &rec.s.s_values[0], sx, sy, sz);
	VSET( &rec.s.s_values[3], r, 0, 0 );
	VSET( &rec.s.s_values[6], 0, r, 0 );
	VSET( &rec.s.s_values[9], 0, 0, r );
	
	fwrite( (char *) &rec, sizeof(rec), 1, stdout);
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


mk_rcc( name, base, height, radius )
char * name;
point_t base;
vect_t height;
fastf_t radius;
{
	static union record rec;
	static float pi = 3.14159265358979323264;
	fastf_t m1, m2;

#ifdef BSD
	bzero( (char *)&rec, sizeof(rec) );
#endif


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

	fwrite( (char *)&rec, sizeof( rec), 1, stdout);
}

/*
 *			M K _ M C O M B
 *
 *  Make a combination with material properties info
 */
mk_mcomb( name, len, region, matname, matparm, override, rgb )
char *name;
char *matname;
char *matparm;
char *rgb;
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
	fwrite( (char *)&rec, sizeof(rec), 1, stdout );
}


/*
 *			M K _ C O M B
 *
 *  Make a simple combination header.
 * Must be followed by 'len' mk_memb() calls before any other mk_* routines
 */
mk_comb( name, len, region )
char *name;
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
	fwrite( (char *)&rec, sizeof(rec), 1, stdout );
}

/*
 *			M K _ M E M B
 *
 *  Must be part of combination/member clump of records.
 */
mk_memb( op, name, mat )
int op;
char *name;
mat_t mat;
{
	static union record rec;
	register int i;

	rec.M.m_id = ID_MEMB;
	rec.M.m_relation = op;
	NAMEMOVE( name, rec.M.m_instname );
	for( i=0; i<16; i++ )
		rec.M.m_mat[i] = mat[i];
	fwrite( (char *)&rec, sizeof(rec), 1, stdout );
}
