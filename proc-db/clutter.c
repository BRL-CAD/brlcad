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
fprintf(stderr,"%d %d at %g %g\n", ix, iy, x, y );
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
	vect_t	work;
	vect_t	begin;
	vect_t	trial;

	if( dir_at )  {
		VSUB2( work, dir_at, pos );
		VUNITIZE( work );
		VMOVE( dir_at, work );
	}

	sprintf( nbuf, "%s.s", name );
	mk_sph( stdout, nbuf, pos[X], pos[Y], pos[Z], r );

	mat_idn( rot );
	/* Here, need to rotate from 0,0,-1 to vect "dir_at" */
	mat_angles( rot, acos(dir_at[X]), acos(dir_at[Y]), acos(dir_at[Z]) );
	VSET( begin, 0, 0, -1 );
	MAT4X3VEC( trial, rot, begin );
	VPRINT( "wanted", dir_at );
	VPRINT( "got   ", trial );
	mat_idn( rot );

	mk_mcomb( stdout, name, 1, 1, "light", "shadow=1", 1, rgb );
	mk_memb( stdout, UNION, nbuf, rot );
}

/*
 *			M A T _ A N G L E S
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation, in degrees.
 */
mat_angles( mat, alpha, beta, ggamma )
register matp_t mat;
double alpha, beta, ggamma;
{
	LOCAL double calpha, cbeta, cgamma;
	LOCAL double salpha, sbeta, sgamma;

	if( alpha == 0.0 && beta == 0.0 && ggamma == 0.0 )  {
		mat_idn( mat );
		return;
	}

	alpha *= degtorad;
	beta *= degtorad;
	ggamma *= degtorad;

	calpha = cos( alpha );
	cbeta = cos( beta );
	cgamma = cos( ggamma );

	salpha = sin( alpha );
	sbeta = sin( beta );
	sgamma = sin( ggamma );

	/*
	 * compute the new rotation to apply to the previous
	 * viewing rotation.
	 * Alpha is angle of rotation about the X axis, and is done third.
	 * Beta is angle of rotation about the Y axis, and is done second.
	 * Gamma is angle of rotation about Z axis, and is done first.
	 */
	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = -sbeta;
	mat[3] = 0.0;

	mat[4] = -salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;
	mat[7] = 0.0;

	mat[8] = calpha * sbeta * cgamma + salpha * sgamma;
	mat[9] = -calpha * sbeta * sgamma + salpha * cgamma;
	mat[10] = calpha * cbeta;
	mat[11] = 0.0;
	mat[12] = mat[13] = mat[14] = 0.0;
	mat[15] = 1.0;
}
