/*                       C L U T T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file clutter.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


mat_t	identity;
double degtorad = 0.0174532925199433;
double sin60;

struct mtab {
	char	mt_name[64];
	char	mt_param[96];
} mtab[] = {
	{"plastic",	""},
	{"glass",	""},
	{"plastic",	""},
	{"mirror",	""},
	{"plastic",	""},
	{"testmap",	""},
	{"plastic",	""},
	{"",		""}
};
int	nmtab = sizeof(mtab)/sizeof(struct mtab);

#define PICK_MAT	((rand() % nmtab) )

double ball_stack(char *bname, double xc, double yc, double size);
double prim_stack(char *pname, double xc, double yc, double size);
double crystal_stack(char *cname, double xc, double yc, double size);
double crystal_layer(char *crname, fastf_t *center, double radius, fastf_t *maj, fastf_t *min, double var, double ratio, int nsolids);

void	do_plate(char *name, double xc, double yc, double size);
void	do_rings(char *ringname, fastf_t *center, double r1, double r2, double incr, int n);

void	get_rgb(unsigned char *rgb);
void	do_light(char *name, point_t pos, vect_t dir_at, int da_flag, double r, unsigned char *rgb, struct wmember *headp);

struct bn_unif	*rbuf;

struct rt_wdb	*outfp;

int
main(int argc, char **argv)
{
	vect_t	norm;
	unsigned char	rgb[3];
	int	ix, iy;
	double	x, y;
	double	size;
	double	base;
	int	quant;
	char	name[64];
	vect_t	pos, aim;
	unsigned char	white[3];
	int	n;
	double	height, maxheight, minheight;
	struct wmember head;

	bu_debug = BU_DEBUG_COREDUMP;
	rbuf = bn_unif_init( 0, 0 );

#define rand_num(p)	(BN_UNIF_DOUBLE(p)+0.5)



	BU_LIST_INIT( &head.l );

	sin60 = sin(60.0 * 3.14159265358979323846264 / 180.0);

	outfp = wdb_fopen( "clutter.g" );
	mk_id( outfp, "Procedural Clutter" );

	/* Create the underpinning */
	VSET( norm, 0, 0, 1 );
	mk_half( outfp, "plane", norm, -10.0 );
	rgb[0] = 240;	/* gold/brown */
	rgb[1] = 180;
	rgb[2] = 64;
	MAT_IDN( identity );

	mk_region1( outfp, "plane.r", "plane", NULL, NULL, rgb );

	(void)mk_addmember( "plane.r", &head.l, NULL, WMOP_UNION );

	/* Create the detail cells */
	size = 1000;	/* mm */
	quant = 7;	/* XXXX 5 */
	base = -size*(quant/2);
	maxheight = size/2;		/* keep lights off the floor */
	for( ix=quant-1; ix>=0; ix-- )  {
		x = base + ix*size;
		for( iy=quant-1; iy>=0; iy-- )  {
			y = base + iy*size;
			sprintf( name, "Bx%dy%d", ix, iy );
			do_plate( name, x, y, size );
			(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );

			sprintf( name, "x%dy%d", ix, iy );
			(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );
			n = rand() & 03;
			switch(n)  {
			case 0:
				height = ball_stack( name, x, y, size );
				break;
			case 1:
				height = crystal_stack( name, x, y, size );
				break;
			default:
				height = prim_stack( name, x, y, size );
				break;
			}
			if( height > maxheight )  maxheight = height;
		}
	}

	/* Enclose in some rings */
	minheight = size/2;
	VSET( pos, 0, 0, size/4 );
	do_rings( "rings", pos, 2*size*quant/2, size/4, size, 4 );
	(void)mk_addmember( "rings", &head.l, NULL, WMOP_UNION );

	if( maxheight < minheight ) maxheight = minheight;

	/* Create some light */
	white[0] = white[1] = white[2] = 255;
	base = size*(quant/2+1);
	VSET( aim, 0, 0, 0 );
	VSET( pos, base, base, minheight+maxheight*rand_num(rbuf) );
	do_light( "l1", pos, aim, 1, 100.0, white, &head );
	VSET( pos, -base, base, minheight+maxheight*rand_num(rbuf) );
	do_light( "l2", pos, aim, 1, 100.0, white, &head );
	VSET( pos, -base, -base, minheight+maxheight*rand_num(rbuf) );
	do_light( "l3", pos, aim, 1, 100.0, white, &head );
	VSET( pos, base, -base, minheight+maxheight*rand_num(rbuf) );
	do_light( "l4", pos, aim, 1, 100.0, white, &head );

	/* Build the overall combination */
	mk_lfcomb( outfp, "clut", &head, 0 );

	return 0;
}

double
crystal_stack(char *cname, double xc, double yc, double size)

      	       		/* center coordinates, z=0+ */

{
	int	i;
	point_t	center;
	vect_t	maj, min;
	int	nsolids;
	unsigned char	rgb[4];		/* needs all 4 */
	double	high;
	double	height = 0;
	double	esz;
	char	name[64];
	char	rppname[64];
	char	crystalname[64];
	vect_t	minpt, maxpt;
	struct wmember head;
	struct wmember reg_head;

	BU_LIST_INIT( &head.l );

	/* These should change somewhat for each layer, and be done by rots */
	VSET( maj, 1, 0, .2 );
	VSET( min, 0, 1, .2 );
	VUNITIZE( maj );
	VUNITIZE( min );

	for( i=0; i<3; i++ )  {
		sprintf( name, "%sL%c", cname, 'a'+i);
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );
		VSET( center, xc, yc, size/2*i );
		nsolids = 3 + (rand() & 7);

		high = crystal_layer( name, center, size/2,
			maj, min,
			rand_num(rbuf) * 90.0,
			rand_num(rbuf) * 8.0 + 2.0,
			nsolids );
		if( high > height )  height = high;
	}

	/* Build the crystal union */
	sprintf( crystalname, "%scrystal", cname );
	mk_lfcomb( outfp, crystalname, &head, 0 );

	/* Make the trimming RPP */
	esz = size*0.5;	/* dist from ctr to edge of base */
	VSET( minpt, xc-esz, yc-esz, 10 );
	VSET( maxpt, xc+esz, yc+esz, height );
	sprintf( rppname, "%srpp", cname );
	mk_rpp( outfp, rppname, minpt, maxpt );

	/* Build the final combination */
	BU_LIST_INIT( &reg_head.l );
	get_rgb(rgb);
	i = PICK_MAT;
	(void)mk_addmember( crystalname, &reg_head.l, NULL, WMOP_UNION );
	(void)mk_addmember( rppname, &reg_head.l, NULL, WMOP_INTERSECT );
	mk_lcomb( outfp, cname, &reg_head, 1,
		mtab[i].mt_name, mtab[i].mt_param, rgb, 0 );
	return(height);
}

double
crystal_layer(char *crname, fastf_t *center, double radius, fastf_t *maj, fastf_t *min, double var, double ratio, int nsolids)

       	       		/* center coordinates, (min Z) */
      	       		/* cell radius */
      	    		/* main axis of growth */
      	    		/* minor axis of growth */
      	    		/* max degrees of variation off axis (0..90) */
      	      		/* len/width ratio */
   	        	/* number of solids for this layer */
{
	int	todo;
	double	height = center[Z];
	int	index = 0;
	point_t	loc_cent;
	point_t	a,b;
	fastf_t	*maj_axis, *min_axis;
	vect_t	long_axis, short_axis;
	vect_t	other_axis;
	double	cos_var;
	double	m_cos_var;
	double	length, width;
	point_t	pt[8];
	char	name[32];
	int	i;
	struct wmember head;

	BU_LIST_INIT( &head.l );

	for( todo = nsolids-1; todo >= 0; todo-- )  {
		cos_var = cos( var*rand_num(rbuf) );
		m_cos_var = 1 - cos_var;
		/* Blend together two original axes for new orthog. set */
		if( rand() & 1 )  {
			maj_axis = maj;
			min_axis = min;
		}  else  {
			maj_axis = min;
			min_axis = maj;
		}
		VSETALL( long_axis, 0 );
		VSETALL( short_axis, 0 );
		VJOIN2( long_axis, long_axis,
			cos_var, maj_axis,
			m_cos_var, min_axis );
		VJOIN2( short_axis, short_axis,
			m_cos_var, maj_axis,
			cos_var, min_axis );
		VCROSS( other_axis, long_axis, short_axis );

		/* dither center position */
		VMOVE(loc_cent, center );
		loc_cent[X] += rand_num(rbuf) * radius;
		loc_cent[Y] += rand_num(rbuf) * radius;

		length = radius * rand_num(rbuf);
		width = length / ratio;

		VJOIN1( a, loc_cent, length, long_axis );
		VJOIN2( pt[0], a, -width, short_axis, -width, other_axis );
		VJOIN2( pt[1], a,  width, short_axis, -width, other_axis );
		VJOIN2( pt[2], a,  width, short_axis,  width, other_axis );
		VJOIN2( pt[3], a, -width, short_axis,  width, other_axis );

		VJOIN1( b, loc_cent, -length, long_axis );
		VJOIN2( pt[4], b, -width, short_axis, -width, other_axis );
		VJOIN2( pt[5], b,  width, short_axis, -width, other_axis );
		VJOIN2( pt[6], b,  width, short_axis,  width, other_axis );
		VJOIN2( pt[7], b, -width, short_axis,  width, other_axis );

		/* Consider fusing points here, for visual complexity */

		sprintf( name, "%s%d", crname, index++ );
		mk_arb8( outfp, name, &pt[0][X] );
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );

		for( i=0; i<8; i++ )  {
			if( pt[i][Z] > height )
				height = pt[i][Z];
		}
	}

	mk_lfcomb( outfp, crname, &head, 0 );
	return(height);
}

void
do_plate(char *name, double xc, double yc, double size)

      	       		/* center coordinates, z=0+ */

{
	double	esz;
	vect_t	minpt, maxpt;
	char	sname[64];
	unsigned char	rgb[4];		/* needs all 4 */
	int	i;

	sprintf(sname, "%s.s", name);
	/* Make the base */
	esz = size*0.5*0.9;	/* dist from ctr to edge of base */
	VSET( minpt, xc-esz, yc-esz, -9 );
	VSET( maxpt, xc+esz, yc+esz, -1 );
	mk_rpp( outfp, sname, minpt, maxpt );

	/* Needs to be in a region, with color!  */
	get_rgb(rgb);
	i = PICK_MAT;
	mk_region1( outfp, name, sname,
		mtab[i].mt_name, mtab[i].mt_param, rgb );
}

double
ball_stack(char *bname, double xc, double yc, double size)

      	       		/* center coordinates, z=0+ */

{
	point_t	center;
	double	esz;
	unsigned char	rgb[4];		/* needs all 4 */
	int	i;
	int	n;
	char	name[32];
	struct wmember head;

	BU_LIST_INIT( &head.l );

	/* Make some objects */
	esz = size*0.5*0.9;	/* dist from ctr to edge of base */
	n = rand()&7;
	for( i=0; i<n; i++ )  {
		sprintf( name, "%s%c", bname, 'A'+i );
		VSET( center, xc, yc, size/2+i*size );
		mk_sph( outfp, name, center, esz/2 );
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );
	}

	/* Build the combination */
	get_rgb(rgb);
	mk_lcomb( outfp, bname, &head, 0, (char *)0, "", rgb, 0 );

	return( n*size );
}

double
prim_stack(char *pname, double xc, double yc, double size)

      	       		/* center coordinates, z=0+ */

{
	point_t	pt[8];
	vect_t	min, max;
	vect_t	base, hvec;
	point_t	center;
	unsigned char	rgb[4];		/* needs all 4 */
	int	nobj;
	int	i;
	int	n;
	double	vpos = 0.0;
	double	height;
	double	xbase, ybase;
	char	name[32];
	struct wmember head;

	BU_LIST_INIT( &head.l );

	size *= 0.3;		/* Don't occupy full cell */
	xbase = xc - size/2;
	ybase = yc - size/2;

	/* Make some objects */
	n = (rand()&7)+1;
	for( nobj=0; nobj<n; nobj++ )  {
		sprintf( name, "%s%c", pname, 'A'+nobj );
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );
		height = ((rand()&7)+1)*size/3;
		i = rand()%5;
		switch(i)  {
		default:
			VSET( center, xc, yc, vpos+size/2 );
			mk_sph( outfp, name, center, size/2 );
			vpos += size;
			break;
		case 0:
			VSET( min, xc-size/2, yc-size/2, vpos );
			VSET( max, xc+size/2, yc+size/2, vpos+height );
			mk_rpp( outfp, name, min, max );
			vpos += height;
			break;
		case 1:
			VSET( base, xc, yc, vpos );
			VSET( hvec, 0, 0, height );
			mk_rcc( outfp, name, base, hvec, size/2 );
			vpos += height;
			break;
		case 2:
			VSET( pt[0], xbase, ybase, vpos);
			VSET( pt[1], xbase+size, ybase, vpos);
			VSET( pt[2], xbase+size/2, ybase+size, vpos);
			VSET( pt[3], xbase+size/2, ybase+size*sin60/3, vpos+height );
			mk_arb4( outfp, name, &pt[0][X] );
			vpos += height;
			break;
		case 3:
			VSET( center, xc, yc, vpos+height/2 );
			VSET( pt[0], size/2, 0, 0 );
			VSET( pt[1], 0, size/2, 0 );
			VSET( pt[2], 0, 0, height/2 );
			mk_ell( outfp, name, center,
				pt[0], pt[1], pt[2] );
			vpos += height;
			break;
		}
	}

	/* Build the combination */
	get_rgb( rgb );
	i = PICK_MAT;
	mk_lcomb( outfp, pname, &head, 0,
		mtab[i].mt_name, mtab[i].mt_param,
		rgb, 0 );
	return(vpos);
}

void
do_rings(char *ringname, fastf_t *center, double r1, double r2, double incr, int n)
{
	int	i;
	vect_t	normal;
	unsigned char	rgb[4];
	char	rname[32];
	char	sname[32];
	struct wmember head;

	BU_LIST_INIT( &head.l );

	VSET( normal, 0, 0, 1 );
	for( i=0; i<n; i++ )  {
		sprintf( sname, "%s%ds", ringname, i );
		sprintf( rname, "%s%dr", ringname, i );

		mk_tor( outfp, sname, center, normal, r1, r2 );
		r1 += incr;

		/* Build the region that contains each solid */
		get_rgb(rgb);
		mk_region1( outfp, rname, sname, NULL, NULL, rgb );
		(void)mk_addmember( rname, &head.l, NULL, WMOP_UNION );
	}

	/* Build the group that holds all the regions */
	mk_lfcomb( outfp, ringname, &head, 0 );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
