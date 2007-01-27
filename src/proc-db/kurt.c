/*                          K U R T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file kurt.c
 *
 *  Program to generate polygons from a multi-valued function.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


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

struct val {
	double	v_z[3];
	double	v_x;
	double	v_y;
	int	v_n;
} val[20][20];

void	do_cell(struct val *vp, double xc, double yc);
void	draw_rect(struct val *a, struct val *b, struct val *c, struct val *d);
void	pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *centroid, int npts);
void	do_light(char *name, fastf_t *pos, fastf_t *dir_at, int da_flag, double r, unsigned char *rgb);

struct rt_wdb	*outfp;

int
main(int argc, char **argv)
{
	int	ix, iy;
	double	x, y;
	double	size;
	double	base;
	int	quant;

	outfp = wdb_fopen("kurt.g");
	mk_id( outfp, "Kurt's multi-valued function");

	/* Create the detail cells */
	size = 10;	/* mm */
	quant = 18;
	base = -size*(quant/2);
	for( ix=quant-1; ix>=0; ix-- )  {
		x = base + ix*size;
		for( iy=quant-1; iy>=0; iy-- )  {
			y = base + iy*size;
			do_cell( &val[ix][iy], x, y );
		}
	}
	/* Draw cells */
/* XXX This should be a height field / DSP solid! */
#if 0
	mk_polysolid( outfp, "kurt" );
	for( ix=quant-2; ix>=0; ix-- )  {
		for( iy=quant-2; iy>=0; iy-- )  {
			draw_rect( &val[ix][iy],
				   &val[ix+1][iy],
				   &val[ix][iy+1],
				   &val[ix+1][iy+1] );
		}
	}
#endif

#ifdef never
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
	mk_fcomb( outfp, "clut", quant*quant+1+4, 0 );
	mk_memb( outfp, "plane.r", identity, UNION );
	for( ix=quant-1; ix>=0; ix-- )  {
		for( iy=quant-1; iy>=0; iy-- )  {
			sprintf( name, "x%dy%d", ix, iy );
			mk_memb( outfp, name, identity, UNION );
		}
	}
	mk_memb( outfp, "l1", identity, UNION );
	mk_memb( outfp, "l2", identity, UNION );
	mk_memb( outfp, "l3", identity, UNION );
	mk_memb( outfp, "l4", identity, UNION );
#endif
	return 0;
}

void
do_cell(struct val *vp, double xc, double yc)

			/* center coordinates, z=0+ */
{
	LOCAL poly	polynom;
	LOCAL bn_complex_t	roots[4];	/* roots of final equation */
	int		l;
	int		nroots;
	int		lim;

	polynom.dgr = 3;
	polynom.cf[0] = 1;
	polynom.cf[1] = 0;
	polynom.cf[2] = yc;
	polynom.cf[3] = xc;
	vp->v_n = 0;
	vp->v_x = xc;
	vp->v_y = yc;
	nroots = rt_poly_roots( &polynom, roots, "" );
	if( nroots < 0 || (nroots & 1) == 0 )  {
		fprintf(stderr,"%d roots?\n", nroots);
		return;
	}
	for ( l=0; l < nroots; l++ ){
		if ( NEAR_ZERO( roots[l].im, 0.0001 ) )
			vp->v_z[vp->v_n++] = roots[l].re;
	}
	/* Sort in increasing Z */
	for( lim = nroots-1; lim > 0; lim-- )  {
		for( l=0; l < lim; l++ )  {
			register double t;
			if( (t=vp->v_z[l]) > vp->v_z[l+1] )  {
				vp->v_z[l] = vp->v_z[l+1];
				vp->v_z[l+1] = t;
			}
		}
	}
}

void
draw_rect(struct val *a, struct val *b, struct val *c, struct val *d)
{
	int min, max;
	register int i;
	point_t	centroid, work;
	int	ndiff;
	int	lvl;
	int	j;
	struct val	*vp[4];

	/* Find min and max # of points at the 4 vertices */
	max = a->v_n;
	if( b->v_n > max )  max = b->v_n;
	if( c->v_n > max )  max = c->v_n;
	if( d->v_n > max )  max = d->v_n;
	min = a->v_n;
	if( b->v_n < min )  min = b->v_n;
	if( c->v_n < min )  min = c->v_n;
	if( d->v_n < min )  min = d->v_n;

	ndiff = 0;
	if( a->v_n > min )  vp[ndiff++] = a;
	if( b->v_n > min )  vp[ndiff++] = b;
	if( c->v_n > min )  vp[ndiff++] = c;
	if( d->v_n > min )  vp[ndiff++] = d;


	VSET( work, a->v_x, a->v_y, a->v_z[0] );
	VMOVE( centroid, work );
	VSET( work, b->v_x, b->v_y, b->v_z[0] );
	VADD2( centroid, centroid, work );
	VSET( work, c->v_x, c->v_y, c->v_z[0] );
	VADD2( centroid, centroid, work );
	VSET( work, d->v_x, d->v_y, d->v_z[0] );
	VADD2( centroid, centroid, work );
	VSCALE( centroid, centroid, 0.25 );

	/* First, the simple part.  Assume that all 4-way shared levels
	 * are stacked plates.  Do them now, then worry about oddities.
	 * For general functions, this may be dangerous, but works fine here.
	 */
	for( i=0; i<min; i++ )  {
#if 0
		VSET( verts[0], a->v_x, a->v_y, a->v_z[i] );
		VSET( verts[1], b->v_x, b->v_y, b->v_z[i] );
		VSET( verts[2], c->v_x, c->v_y, c->v_z[i] );
		/* even # faces point up, odd#s point down */
		pnorms( norms, verts, (i&1)?down:up, 3 );
		mk_poly( outfp, 3, verts, norms );

		VSET( verts[0], d->v_x, d->v_y, d->v_z[i] );
		VSET( verts[1], b->v_x, b->v_y, b->v_z[i] );
		VSET( verts[2], c->v_x, c->v_y, c->v_z[i] );
		/* even # faces point up, odd#s point down */
		pnorms( norms, verts, (i&1)?down:up, 3 );
		mk_poly( outfp, 3, verts, norms );
#endif
	}
	/* If 0 or 1 corners have something above them, nothing needs drawn */
	if( ndiff == 0 || ndiff == 1 )  return;
	/* Harder case:  handle different depths on corners */
	if( ndiff == 2 &&
	    vp[0]->v_x != vp[1]->v_x &&
	    vp[0]->v_y != vp[1]->v_y )  {
		fprintf(stderr, "2 corners on diagonal differ?\n");
		return;
	}

	/* Draw 1 or 2 extra faces to close off each new upper zone */
	for( lvl = min; lvl < max; lvl += 2 )  {
		for( i=0; i<ndiff-1; i++ )  {
			for( j=i; j<ndiff; j++ )  {
				/* Reject diagonals */
				if( vp[i]->v_x != vp[j]->v_x &&
				    vp[i]->v_y != vp[j]->v_y )
					continue;

#if 0
				VSET( verts[0],
					vp[i]->v_x, vp[i]->v_y,
					vp[i]->v_z[lvl] );
				VSET( verts[1],
					vp[j]->v_x, vp[j]->v_y,
					vp[j]->v_z[lvl] );
				VSET( verts[2],
					vp[i]->v_x, vp[i]->v_y,
					vp[i]->v_z[lvl+1] );

				VSUB2( work, centroid, verts[0] );
				VUNITIZE( work );
				pnorms( norms, verts, work, 3 );
				mk_poly( outfp, 3, verts, norms );

				VSET( verts[0],
					vp[i]->v_x, vp[i]->v_y,
					vp[i]->v_z[lvl+1] );
				VSET( verts[1],
					vp[j]->v_x, vp[j]->v_y,
					vp[j]->v_z[lvl] );
				VSET( verts[2],
					vp[j]->v_x, vp[j]->v_y,
					vp[j]->v_z[lvl+1] );

				VSUB2( work, centroid, verts[0] );
				VUNITIZE( work );
				pnorms( norms, verts, work, 3 );
				mk_poly( outfp, 3, verts, norms );
#endif
			}
		}
	}
}

/*
 *  Find the single outward pointing normal for a facet.
 *  Assumes all points are coplanar (they better be!).
 */
void
pnorms(fastf_t (*norms)[3], fastf_t (*verts)[3], fastf_t *out, int npts)


			/* hopefully points outwards */

{
	register int i;
	vect_t	ab, ac;
	vect_t	n;

	VSUB2( ab, verts[1], verts[0] );
	VSUB2( ac, verts[2], verts[0] );
	VCROSS( n, ab, ac );
	VUNITIZE( n );

	/* If normal points inwards, flip it */
	if( VDOT( n, out ) < 0 )  {
		VREVERSE( n, n );
	}

	/* Use same normal for all vertices (flat shading) */
	for( i=0; i<npts; i++ )  {
		VMOVE( norms[i], n );
	}
}

void
do_light(char *name, fastf_t *pos, fastf_t *dir_at, int da_flag, double r, unsigned char *rgb)


			/* direction or aim point */
			/* 0 = direction, !0 = aim point */
			/* radius of light */

{
	char	nbuf[64];
	vect_t	center;
	mat_t	rot;
	mat_t	xlate;
	mat_t	both;
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
	mk_sph( outfp, nbuf, center, r );

	/*
	 * Need to rotate from 0,0,-1 to vect "dir",
	 * then xlate to final position.
	 */
	VSET( from, 0, 0, -1 );
	bn_mat_fromto( rot, from, dir );
	MAT_IDN( xlate );
	MAT_DELTAS_VEC( xlate, pos);
	bn_mat_mul( both, xlate, rot );

	mk_region1( outfp, name, nbuf, "light", "shadows=1", rgb );
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
