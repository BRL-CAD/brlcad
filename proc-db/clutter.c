/*
 *			C L U T T E R . C
 * 
 *  Program to generate procedural clutter out of primitive
 *  geometric objects.
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

mat_t	identity;
double degtorad = 0.0174532925199433;

main(argc, argv)
char	**argv;
{
	vect_t	norm;
	char	rgb[3];
	int	ix, iy;
	double	x, y;
	double	size;
	double	base;
	int	quant;
	char	name[64];
	vect_t	pos, aim;
	char	white[3];

	mk_id( stdout, "Procedural Clutter" );

	/* Create the underpinning */
	VSET( norm, 0, 0, 1 );
	mk_half( stdout, "plane", -10.0, norm );
	rgb[0] = 240;	/* gold/brown */
	rgb[1] = 180;
	rgb[2] = 64;
	mat_idn( identity );
	mk_mcomb( stdout, "plane.r", 1, 1, "", "", 1, rgb );
	mk_memb( stdout, UNION, "plane", identity );

	/* Create the detail cells */
	size = 1000;	/* mm */
	quant = 3;
	base = -size*(quant/2);
	for( ix=quant-1; ix>=0; ix-- )  {
		x = base + ix*size;
		for( iy=quant-1; iy>=0; iy-- )  {
			y = base + iy*size;
			sprintf( name, "x%dy%d", ix, iy );
			do_cell( name, x, y, size );
		}
	}

	/* Create some light */
	white[0] = white[1] = white[2] = 255;
	base = size*(quant/2+1);
	VSET( aim, 0, 0, 0 );
	VSET( pos, base, base, size );
	do_light( "l1", pos, aim, 1, 100.0, white );
	VSET( pos, -base, base, size );
	do_light( "l2", pos, aim, 1, 100.0, white );
	VSET( pos, -base, -base, size );
	do_light( "l3", pos, aim, 1, 100.0, white );
	VSET( pos, base, -base, size );
	do_light( "l4", pos, aim, 1, 100.0, white );

	/* Build the overall combination */
	mk_comb( stdout, "clut", quant*quant+1+4, 0 );
	mk_memb( stdout, UNION, "plane.r", identity );
	for( ix=quant-1; ix>=0; ix-- )  {
		for( iy=quant-1; iy>=0; iy-- )  {
			sprintf( name, "x%dy%d", ix, iy );
			mk_memb( stdout, UNION, name, identity );
		}
	}
	mk_memb( stdout, UNION, "l1", identity );
	mk_memb( stdout, UNION, "l2", identity );
	mk_memb( stdout, UNION, "l3", identity );
	mk_memb( stdout, UNION, "l4", identity );
}

do_cell( name, xc, yc, size )
char	*name;
double	xc, yc;		/* center coordinates, z=0+ */
double	size;
{
	vect_t	min, max;
	double	esz;
	struct ntab {
		char	nm[64];
	} ntab[9];
	char	rgb[4];		/* needs all 4 */
	int	len;
	int	i;
	int	n;

	len = 0;

	/* Make the base */
	esz = size*0.5*0.9;	/* dist from ctr to edge of base */
	VSET( min, xc-esz, yc-esz, 0 );
	VSET( max, xc+esz, yc+esz, 10 );
	sprintf( ntab[len].nm, "%s%c", name, 'A'+len );
	mk_rpp( stdout, ntab[len].nm, min, max );
	len++;

	/* Make some objects */
	n = rand()&7;
	for( i=0; i<n; i++ )  {
		sprintf( ntab[len].nm, "%s%c", name, 'A'+len );
		mk_sph( stdout, ntab[len].nm, xc, yc, size/2+i*size, esz/2 );
		len++;
	}

	/* Build the combination */
	rgb[0] = rgb[1] = rgb[2] = 32;
	rgb[rand()&3] = 200;
	mk_mcomb( stdout, name, len, 1, "", "", 1, rgb );
	for( i=0; i<len; i++ )  {
		mk_memb( stdout, UNION, ntab[i].nm, identity );
	}
}

do_light( name, pos, dir_at, da_flag, r, rgb )
char	*name;
point_t	pos;
vect_t	dir_at;		/* direction or aim point */
int	da_flag;	/* 0 = direction, !0 = aim point */
double	r;		/* radius of light */
char	*rgb;
{
	char	nbuf[64];
	mat_t	rot;
	mat_t	xlate;
	mat_t	both;
	vect_t	begin;
	vect_t	trial;
	vect_t	from;
	vect_t	dir;

	if( da_flag )  {
		VSUB2( dir, dir_at, pos );
		VUNITIZE( dir );
	} else {
		VMOVE( dir, dir_at );
	}

	sprintf( nbuf, "%s.s", name );
	mk_sph( stdout, nbuf, 0.0, 0.0, 0.0, r );

	/*
	 * Need to rotate from 0,0,-1 to vect "dir",
	 * then xlate to final position.
	 */
	VSET( from, 0, 0, -1 );
	mat_fromto( rot, from, dir );
	mat_idn( xlate );
	MAT_DELTAS( xlate, pos[X], pos[Y], pos[Z] );
	mat_mul( both, xlate, rot );

	mk_mcomb( stdout, name, 1, 1, "light", "shadows=1", 1, rgb );
	mk_memb( stdout, UNION, nbuf, both );
}

/* wrapper for atan2.  On SGI (and perhaps others), x==0 returns infinity */
double
xatan2(y,x)
double	y,x;
{
	if( x > -1.0e-20 && x < 1.0e-20 )  return(0.0);
	return( atan2( y, x ) );
}
/*
 *			M A T _ F R O M T O
 *
 *  Given two vectors, compute a rotation matrix that will transform
 *  space by the angle between the two.  Since there are many
 *  candidate matricies, the method used here is to convert the vectors
 *  to azimuth/elevation form (azimuth is +X, elevation is +Z),
 *  take the difference, and form the rotation matrix.
 *  See mat_ae for that algorithm.
 *
 *  The input 'from' and 'to' vectors must be unit length.
 */
mat_fromto( m, from, to )
mat_t	m;
vect_t	from;
vect_t	to;
{
	double	az, el;
	LOCAL double sin_az, sin_el;
	LOCAL double cos_az, cos_el;

	az = xatan2( to[Y], to[X] ) - xatan2( from[Y], from[X] );
	el = asin( to[Z] ) - asin( from[Z] );

	sin_az = sin(az);
	cos_az = cos(az);
	sin_el = sin(el);
	cos_el = cos(el);

	m[0] = cos_el * cos_az;
	m[1] = -sin_az;
	m[2] = -sin_el * cos_az;
	m[3] = 0;

	m[4] = cos_el * sin_az;
	m[5] = cos_az;
	m[6] = -sin_el * sin_az;
	m[7] = 0;

	m[8] = sin_el;
	m[9] = 0;
	m[10] = cos_el;
	m[11] = 0;

	m[12] = m[13] = m[14] = 0;
	m[15] = 1.0;
}
