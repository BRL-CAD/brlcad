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
double sin60;

struct ntab {
	char	nm[64];
} ntab[64];

double	ball_stack(), prim_stack();

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
	int	n;
	double	height, maxheight = 0;

	sin60 = sin(60.0 * 3.14159265358979323846264 / 180.0);

	mk_id( stdout, "Procedural Clutter" );

	/* Create the underpinning */
	VSET( norm, 0, 0, 1 );
	mk_half( stdout, "plane", -10.0, norm );
	rgb[0] = 240;	/* gold/brown */
	rgb[1] = 180;
	rgb[2] = 64;
	mat_idn( identity );
	mk_mcomb( stdout, "plane.r", 1, 1, "", "", 1, rgb );
	mk_memb( stdout, "plane", identity, UNION );

	/* Create the detail cells */
	size = 1000;	/* mm */
	quant = 5;
	base = -size*(quant/2);
	for( ix=quant-1; ix>=0; ix-- )  {
		x = base + ix*size;
		for( iy=quant-1; iy>=0; iy-- )  {
			y = base + iy*size;
			sprintf( name, "x%dy%d", ix, iy );
			n = rand() & 03;
			switch(n)  {
			case 0:
				height = ball_stack( name, x, y, size );
				break;
			default:
				height = prim_stack( name, x, y, size );
				break;
			}
			if( height > maxheight )  maxheight = height;
		}
	}

	/* Create some light */
	white[0] = white[1] = white[2] = 255;
	base = size*(quant/2+1);
	VSET( aim, 0, 0, 0 );
	VSET( pos, base, base, maxheight*((rand()&7)+1)/8 );
	do_light( "l1", pos, aim, 1, 100.0, white );
	VSET( pos, -base, base, maxheight*((rand()&7)+1)/8 );
	do_light( "l2", pos, aim, 1, 100.0, white );
	VSET( pos, -base, -base, maxheight*((rand()&7)+1)/8 );
	do_light( "l3", pos, aim, 1, 100.0, white );
	VSET( pos, base, -base, maxheight*((rand()&7)+1)/8 );
	do_light( "l4", pos, aim, 1, 100.0, white );

	VSET( pos, 0, 0, size/2 );
	do_rings( "rings", pos, 1.414*base, size/2, 4 );

	/* Build the overall combination */
	mk_comb( stdout, "clut", quant*quant+1+4+1, 0 );
	mk_memb( stdout, "plane.r", identity, UNION );
	for( ix=quant-1; ix>=0; ix-- )  {
		for( iy=quant-1; iy>=0; iy-- )  {
			sprintf( name, "x%dy%d", ix, iy );
			mk_memb( stdout, name, identity, UNION );
		}
	}
	mk_memb( stdout, "l1", identity, UNION );
	mk_memb( stdout, "l2", identity, UNION );
	mk_memb( stdout, "l3", identity, UNION );
	mk_memb( stdout, "l4", identity, UNION );
	mk_memb( stdout, "rings", identity, UNION );
}

double
ball_stack( name, xc, yc, size )
char	*name;
double	xc, yc;		/* center coordinates, z=0+ */
double	size;
{
	vect_t	min, max;
	point_t	center;
	double	esz;
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
	mk_rpp( stdout, ntab[len++].nm, min, max );

	/* Make some objects */
	n = rand()&7;
	for( i=0; i<n; i++ )  {
		sprintf( ntab[len].nm, "%s%c", name, 'A'+len );
		VSET( center, xc, yc, size/2+i*size );
		mk_sph( stdout, ntab[len].nm, center, esz/2 );
		len++;
	}

	/* Build the combination */
	get_rgb(rgb);
	mk_mcomb( stdout, name, len, 1, "", "", 1, rgb );
	for( i=0; i<len; i++ )  {
		mk_memb( stdout, ntab[i].nm, identity, UNION );
	}
	return( i*size );
}

double
prim_stack( name, xc, yc, size )
char	*name;
double	xc, yc;		/* center coordinates, z=0+ */
double	size;
{
	point_t	pt[8];
	int	len;
	vect_t	min, max;
	vect_t	base, hvec;
	point_t	center;
	char	rgb[4];		/* needs all 4 */
	int	nobj;
	int	i;
	int	n;
	double	vpos = 0.0;
	double	height;
	double	xbase, ybase;
	double	esz;

	len = 0;

	/* Make the base */
	esz = size*0.5*0.9;	/* dist from ctr to edge of base */
	height = 10;
	VSET( min, xc-esz, yc-esz, 0 );
	VSET( max, xc+esz, yc+esz, height );
	sprintf( ntab[len].nm, "%s%c", name, 'A'+len );
	mk_rpp( stdout, ntab[len++].nm, min, max );

	size *= 0.3;		/* Don't occupy full cell */
	xbase = xc - size/2;
	ybase = yc - size/2;

	/* Make some objects */
	n = (rand()&7)+1;
	for( nobj=0; nobj<n; nobj++ )  {
		sprintf( ntab[len].nm, "%s%c", name, 'A'+len );
		height = ((rand()&7)+1)*size/3;
		i = rand()%5;
		switch(i)  {
		default:
			VSET( center, xc, yc, vpos+size/2 );
			mk_sph( stdout, ntab[len++].nm, center, size/2 );
			vpos += size;
			break;
		case 0:
			VSET( min, xc-size/2, yc-size/2, vpos );
			VSET( max, xc+size/2, yc+size/2, vpos+height );
			mk_rpp( stdout, ntab[len++].nm, min, max );
			vpos += height;
			break;
		case 1:
			VSET( base, xc, yc, vpos );
			VSET( hvec, 0, 0, height );
			mk_rcc( stdout, ntab[len++].nm, base, hvec, size/2 );
			vpos += height;
			break;
		case 2:
			VSET( pt[0], xbase, ybase, vpos);
			VSET( pt[1], xbase+size, ybase, vpos);
			VSET( pt[2], xbase+size/2, ybase+size, vpos);
			VSET( pt[3], xbase+size/2, ybase+size*sin60/3, vpos+height );
			mk_arb4( stdout, ntab[len++].nm, pt );
			vpos += height;
			break;
		case 3:
			VSET( center, xc, yc, vpos+height/2 );
			VSET( pt[0], size/2, 0, 0 );
			VSET( pt[1], 0, size/2, 0 );
			VSET( pt[2], 0, 0, height/2 );
			mk_ell( stdout, ntab[len++].nm, center,
				pt[0], pt[1], pt[2] );
			vpos += height;
			break;
		}
	}

	/* Build the combination */
	get_rgb( rgb );
	mk_mcomb( stdout, name, len, 1, "", "", 1, rgb );
	for( i=0; i<len; i++ )  {
		mk_memb( stdout, ntab[i].nm, identity, UNION );
	}
	return(vpos);
}

struct colors {
	char		*name;
	unsigned char	c_pixel[3];
}colortab[] = {
	"black",	20,20,20,
	"blue",		0,0,255,
	"brown",	200,130,0,
	"cyan",		0,255,200,
	"flesh",	255,200,160,
	"gray",		120,120,120,
	"green",	0,255,0,
	"lime", 	200,255,0,
	"magenta",	255,0,255,
	"olive",	220,190,0,
	"orange",	255,100,0,
	"pink",		255,200,200,
	"red",		255,0,0,
	"rose",		255,0,175,
	"rust",		200,100,0,
	"silver",	237,237,237,
	"sky",		0,255,255,
	"violet",	200,0,255,
	"white",	255,255,255,
	"yellow",	255,200,0
};
int	ncolors = sizeof(colortab)/sizeof(struct colors);
int	curcolor = 0;

get_rgb( rgb )
register char	*rgb;
{
	register struct colors *cp;
	if( ++curcolor >= ncolors )  curcolor = 0;
	cp = &colortab[curcolor];
	rgb[0] = cp->c_pixel[0];
	rgb[1] = cp->c_pixel[1];
	rgb[2] = cp->c_pixel[2];
}

do_rings( name, center, r1, r2, n )
char	*name;
double	r1;
double	r2;
int	n;
{
	struct ntab snames[32];
	int	i;
	int	len = 0;	/* combination length, in ntab & snames */
	vect_t	normal;
	char	rgb[4];

	VSET( normal, 0, 0, 1 );
	for( i=0; i<n; i++ )  {
		sprintf( snames[len].nm, "%s%ds", name, i );
		sprintf( ntab[len].nm, "%s%dr", name, i );
		mk_tor( stdout, snames[len++].nm, center, normal, r1, r2 );
		r1 += 4*r2;
	}

	/* Build the region that contains each solid */
	for( i=0; i<n; i++ )  {
		get_rgb(rgb);
		mk_mcomb( stdout, ntab[i].nm, 1, 1, "", "", 1, rgb );
		mk_memb( stdout, snames[i].nm, identity, UNION );
	}

	/* Build the group that holds all the regions */
	mk_comb( stdout, name, n, 0 );
	for( i=0; i<n; i++ )
		mk_memb( stdout, ntab[i].nm, identity, UNION );
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
	vect_t	center;
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
	VSETALL( center, 0 );
	mk_sph( stdout, nbuf, center, r );

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
	mk_memb( stdout, nbuf, both, UNION );
}

/* wrapper for atan2.  On SGI (and perhaps others), x==0 returns infinity */
double
xatan2(y,x)
double	y,x;
{
	if( x > -1.0e-20 && x < 1.0e-20 )  {
		/* X is equal to zero, check Y */
		if( y < -1.0e-20 )  return( -3.14159265358979323/2 );
		if( y >  1.0e-20 )  return(  3.14159265358979323/2 );
		return(0.0);
	}
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
