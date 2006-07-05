/*                     G _ E X T R U D E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup g */

/*@{*/
/** @file g_extrude.c
 *	Provide support for solids of extrusion.
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_extrude_internal --- parameters for solid
 *	define extrude_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_extrude_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_EXTRUDE, increment ID_MAXIMUM
 *	edit db_scan.c to add support for new solid type
 *	edit Cakefile to add g_extrude.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_extrude() routine
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  	John R. Anderson
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSextrude[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "tcl.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "./debug.h"

extern int seg_to_vlist( struct bu_list *vhead, const struct rt_tess_tol *ttol, point_t	V,
			 vect_t	u_vec, vect_t v_vec, struct rt_sketch_internal *sketch_ip, genptr_t seg);

struct extrude_specific {
	mat_t rot, irot;	/* rotation and translation to get extrsuion vector in +z direction with V at origin */
	vect_t unit_h;		/* unit vector in direction of extrusion vector */
	vect_t u_vec;		/* u vector rotated and projected */
	vect_t v_vec;		/* v vector rotated and projected */
	fastf_t	uv_scale;	/* length of original, untransformed u_vec */
	vect_t rot_axis;	/* axis of rotation for rotation matrix */
	vect_t perp;		/* vector in pl1_rot plane and normal to rot_axis */
	plane_t pl1, pl2;	/* plane equations of the top and bottom planes (not rotated) */
	plane_t pl1_rot;	/* pl1 rotated by rot */
	point_t *verts;		/* sketch vertices projected onto a plane normal to extrusion vector */
	struct curve crv;	/* copy of the referenced curve */
};

static struct bn_tol extr_tol={			/* a fake tolerance structure for the intersection routines */
	BN_TOL_MAGIC,
	RT_LEN_TOL,
	RT_LEN_TOL*RT_LEN_TOL,
	0.0,
	1.0};

#define MAX_HITS 64

/* defines for surf_no in the hit struct (a negative surf_no indicates an exit point) */
#define TOP_FACE	1	/* extruded face */
#define BOTTOM_FACE	2	/* face in uv-plane */
#define LINE_SEG	3
#define CARC_SEG	4
#define NURB_SEG	5
#define BEZIER_SEG	6

/* defines for loop classification */
#define UNKNOWN		0
#define	A_IN_B		1
#define	B_IN_A		2
#define	DISJOINT	3

#define LOOPA		1
#define LOOPB		2

/**
 *  			R T _ E X T R U D E _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid EXTRUDE, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	EXTRUDE is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct extrude_specific is created, and it's address is stored in
 *  	stp->st_specific for use by extrude_shot().
 */
int
rt_extrude_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_extrude_internal *eip;
	register struct extrude_specific *extr;
	struct rt_sketch_internal *skt;
	LOCAL vect_t tmp, xyz[3];
	fastf_t tmp_f, ldir[3];
	int i, j;
	int vert_count;
	int curr_vert;

	eip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC( eip );
	skt = eip->skt;
	RT_SKETCH_CK_MAGIC( skt );

	/* make sure the curve is valid */
	if( rt_check_curve( &skt->skt_curve, skt, 1 ) )
	{
		bu_log( "ERROR: referenced sketch (%s) is bad!!!\n",
			eip->sketch_name );
		return( -1 );
	}

	BU_GETSTRUCT( extr, extrude_specific );
	stp->st_specific = (genptr_t)extr;

	VMOVE( extr->unit_h, eip->h );
	VUNITIZE(extr->unit_h );

	/* the length of the u_vec is used for scaling radii of circular arcs
	 * the u_vec and the v_vec must have the same length
	 */
	extr->uv_scale = MAGNITUDE( eip->u_vec );

	/* build a transformation matrix to rotate extrusion vector to z-axis */
	VSET( tmp, 0, 0, 1 )
	bn_mat_fromto( extr->rot, eip->h, tmp );

	/* and translate to origin */
	extr->rot[MDX] = -VDOT( eip->V, &extr->rot[0] );
	extr->rot[MDY] = -VDOT( eip->V, &extr->rot[4] );
	extr->rot[MDZ] = -VDOT( eip->V, &extr->rot[8] );

	/* and save the inverse */
	bn_mat_inv( extr->irot, extr->rot );

	/* calculate plane equations of top and bottom planes */
	VCROSS( extr->pl1, eip->u_vec, eip->v_vec );
	VUNITIZE( extr->pl1 )
	extr->pl1[3] = VDOT( extr->pl1, eip->V );
	VMOVE( extr->pl2, extr->pl1 );
	VADD2( tmp, eip->V, eip->h );
	extr->pl2[3] = VDOT( extr->pl2, tmp );

	vert_count = skt->vert_count;
	/* count how many additional vertices we will need for arc centers */
	for( i=0 ; i<skt->skt_curve.seg_count ; i++ )
	{
		struct carc_seg *csg=(struct carc_seg *)skt->skt_curve.segments[i];

		if( csg->magic != CURVE_CARC_MAGIC )
			continue;

		if( csg->radius <= 0.0 )
			continue;

		vert_count++;
	}

	/* apply the rotation matrix to all the vertices, and start bounding box calculation */
	if( vert_count )
		extr->verts = (point_t *)bu_calloc( vert_count, sizeof( point_t ), "extr->verts" );
	VSETALL( stp->st_min, MAX_FASTF );
	VSETALL( stp->st_max, -MAX_FASTF );
	for( i=0 ; i<skt->vert_count ; i++ )
	{
		VJOIN2( tmp, eip->V, skt->verts[i][0], eip->u_vec, skt->verts[i][1], eip->v_vec );
		VMINMAX( stp->st_min, stp->st_max, tmp );
		MAT4X3PNT( extr->verts[i], extr->rot, tmp );
		VADD2( tmp, tmp, eip->h );
		VMINMAX( stp->st_min, stp->st_max, tmp );
	}
	curr_vert = skt->vert_count;

	/* and the u,v vectors */
	MAT4X3VEC( extr->u_vec, extr->rot, eip->u_vec );
	MAT4X3VEC( extr->v_vec, extr->rot, eip->v_vec );

	/* calculate the rotated pl1 */
	VCROSS( extr->pl1_rot, extr->u_vec, extr->v_vec );
	VUNITIZE( extr->pl1_rot );
	extr->pl1_rot[3] = VDOT( extr->pl1_rot, extr->verts[0] );

	VSET( tmp, 0, 0, 1 )
	tmp_f = VDOT( tmp, extr->unit_h );
	if( tmp_f < 0.0 )
		tmp_f = -tmp_f;
	tmp_f -= 1.0;
	if( NEAR_ZERO( tmp_f, SQRT_SMALL_FASTF ) )
	{
		VSET( extr->rot_axis, 1.0, 0.0, 0.0 );
		VSET( extr->perp, 0.0, 1.0, 0.0 );
	}
	else
	{
		VCROSS( extr->rot_axis, tmp, extr->unit_h );
		VUNITIZE( extr->rot_axis );
		if( MAGNITUDE( extr->rot_axis ) < SQRT_SMALL_FASTF )
		{
			VSET( extr->rot_axis, 1.0, 0.0, 0.0 );
			VSET( extr->perp, 0.0, 1.0, 0.0 );
		}
		else
		{
			VCROSS( extr->perp, extr->rot_axis, extr->pl1_rot );
			VUNITIZE( extr->perp );
		}
	}

	/* copy the curve */
	rt_copy_curve( &extr->crv, &skt->skt_curve );

	VSET( xyz[X], 1, 0, 0 );
	VSET( xyz[Y], 0, 1, 0 );
	VSET( xyz[Z], 0, 0, 1 );

	for( i=X ; i<=Z ; i++ ) {
		VCROSS( tmp, extr->unit_h, xyz[i] );
		ldir[i] = MAGNITUDE( tmp );
	}

	/* if any part of the curve is a circular arc, the arc may extend beyond the listed vertices */
	for( i=0 ; i<skt->skt_curve.seg_count ; i++ )
	{
		struct carc_seg *csg=(struct carc_seg *)skt->skt_curve.segments[i];
		struct carc_seg *csg_extr=(struct carc_seg *)extr->crv.segments[i];
		point_t center;

		if( csg->magic != CURVE_CARC_MAGIC )
			continue;

		if( csg->radius <= 0.0 )	/* full circle */
		{
			point_t start;
			fastf_t radius;

			csg_extr->center = csg->end;
			VJOIN2( start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec );
			VJOIN2( center, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec );
			VSUB2( tmp, start, center );
			radius = MAGNITUDE( tmp );
			csg_extr->radius = -radius;	/* need the correct magnitude for normal calculation */

			for( j=X ; j<=Z ; j++ ) {
				tmp_f = radius * ldir[j];
				VJOIN1( tmp, center, tmp_f, xyz[j] );
				VMINMAX( stp->st_min, stp->st_max, tmp );
				VADD2( tmp, tmp, eip->h );
				VMINMAX( stp->st_min, stp->st_max, tmp );

				VJOIN1( tmp, center, -tmp_f, xyz[j] );
				VMINMAX( stp->st_min, stp->st_max, tmp );
				VADD2( tmp, tmp, eip->h );
				VMINMAX( stp->st_min, stp->st_max, tmp );
			}
		}
		else	/* circular arc */
		{
			point_t start, end, mid;
			vect_t s_to_m;
			vect_t bisector;
			fastf_t dist;
			fastf_t magsq_s2m;

			VJOIN2( start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec );
			VJOIN2( end, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec );
			VBLEND2( mid, 0.5, start, 0.5, end );
			VSUB2( s_to_m, mid, start );
			VCROSS( bisector, extr->pl1, s_to_m );
			VUNITIZE( bisector );
			magsq_s2m = MAGSQ( s_to_m );
			csg_extr->radius = csg->radius * extr->uv_scale;
			if( magsq_s2m > csg_extr->radius*csg_extr->radius )
			{
				fastf_t max_radius;

				max_radius = sqrt( magsq_s2m );
				if( NEAR_ZERO( max_radius - csg_extr->radius, RT_LEN_TOL ) )
					csg_extr->radius = max_radius;
				else
				{
					bu_log( "Impossible radius for circular arc in extrusion (%s), is %g, cannot be more than %g!!!\n",
							stp->st_dp->d_namep, csg_extr->radius, sqrt(magsq_s2m)  );
					bu_log( "Difference is %g\n", max_radius - csg->radius );
					return( -1 );
				}
			}
			dist = sqrt( csg_extr->radius*csg_extr->radius - magsq_s2m );

			/* save arc center */
			if( csg->center_is_left )
				VJOIN1( center, mid, dist, bisector )
			else
				VJOIN1( center, mid, -dist, bisector )
			MAT4X3PNT( extr->verts[curr_vert], extr->rot, center );
			csg_extr->center = curr_vert;
			curr_vert++;

			for( j=X ; j<=Z ; j++ ) {
				tmp_f = csg_extr->radius * ldir[j];
				VJOIN1( tmp, center, tmp_f, xyz[j] );
				VMINMAX( stp->st_min, stp->st_max, tmp );
				VADD2( tmp, tmp, eip->h );
				VMINMAX( stp->st_min, stp->st_max, tmp );

				VJOIN1( tmp, center, -tmp_f, xyz[j] );
				VMINMAX( stp->st_min, stp->st_max, tmp );
				VADD2( tmp, tmp, eip->h );
				VMINMAX( stp->st_min, stp->st_max, tmp );
			}
		}
	}

	VBLEND2( stp->st_center, 0.5, stp->st_min, 0.5, stp->st_max );
	VSUB2( tmp, stp->st_max, stp->st_min );
	stp->st_aradius = 0.5 * MAGNITUDE( tmp );
	stp->st_bradius = stp->st_aradius;

	return(0);              /* OK */
}

/**
 *			R T _ E X T R U D E _ P R I N T
 */
void
rt_extrude_print(register const struct soltab *stp)
{
}

int
get_quadrant(fastf_t *v, fastf_t *local_x, fastf_t *local_y, fastf_t *vx, fastf_t *vy)
{

	*vx = V2DOT( v, local_x );
	*vy = V2DOT( v, local_y );

	if( *vy >= 0.0 )
	{
		if( *vx >= 0.0 )
			return( 1 );
		else
			return( 2 );
	}
	else
	{
		if( *vx >= 0.0 )
			return( 4 );
		else
			return( 3 );
	}
}

int
isect_line2_ellipse(fastf_t *dist, fastf_t *ray_start, fastf_t *ray_dir, fastf_t *center, fastf_t *ra, fastf_t *rb)
{
	fastf_t a, b, c;
	point2d_t pmc;
	fastf_t pmcda, pmcdb;
	fastf_t ra_sq, rb_sq;
	fastf_t ra_4, rb_4;
	fastf_t dda, ddb;
	fastf_t disc;

	V2SUB2( pmc, ray_start, center );
	pmcda = V2DOT( pmc, ra );
	pmcdb = V2DOT( pmc, rb );
	ra_sq = V2DOT( ra, ra );
	ra_4 = ra_sq * ra_sq;
	rb_sq = V2DOT( rb, rb );
	rb_4 = rb_sq * rb_sq;
	if( ra_4 < SMALL_FASTF || rb_4 < SMALL_FASTF )
	{
		bu_log( "ray (%g %g %g) -> (%g %g %g), semi-axes  = (%g %g %g) and (%g %g %g), center = (%g %g %g)\n",
			V3ARGS( ray_start ), V3ARGS( ray_dir ), V3ARGS( ra ), V3ARGS( rb ), V3ARGS( center ) );
		bu_bomb( "ERROR: isect_line2_ellipse: semi-axis length is too small!!!\n" );
	}

	dda = V2DOT( ray_dir, ra );
	ddb = V2DOT( ray_dir, rb );

	a = dda*dda/ra_4 + ddb*ddb/rb_4;
	b = 2.0 * (pmcda*dda/ra_4 + pmcdb*ddb/rb_4);
	c = pmcda*pmcda/ra_4 + pmcdb*pmcdb/rb_4 - 1.0;

	disc = b*b - 4.0*a*c;
	if( disc < 0.0 )
		return( 0 );

	if( disc <= SMALL_FASTF )
	{
		dist[0] = -b/(2.0*a);
		return( 1 );
	}

	dist[0] = (-b - sqrt( disc )) / (2.0*a);
	dist[1] = (-b + sqrt( disc )) / (2.0*a);
	return( 2 );
}


int
isect_line_earc(fastf_t *dist, fastf_t *ray_start, fastf_t *ray_dir, fastf_t *center, fastf_t *ra, fastf_t *rb, fastf_t *norm, fastf_t *start, fastf_t *end, int orientation)





                	/* 0 -> ccw, !0 -> cw */
{
	int dist_count;
	vect_t local_x, local_y, local_z;
	fastf_t vx, vy;
	fastf_t ex, ey;
	fastf_t sx, sy;
	int quad_start, quad_end, quad_pt;
	point2d_t to_pt, pt;
	int i;

	dist_count = isect_line2_ellipse( dist, ray_start, ray_dir, center, ra, rb);

	if( dist_count == 0 )
		return( 0 );

	if( orientation )
		VREVERSE( local_z, norm )
	else
		VMOVE( local_z, norm )

	VMOVE( local_x, ra );

	VCROSS( local_y, local_z, local_x );

	V2SUB2( to_pt, end, center );
	quad_end = get_quadrant( to_pt, local_x, local_y, &ex, &ey );
	V2SUB2( to_pt, start, center );
	quad_start = get_quadrant( to_pt, local_x, local_y, &sx, &sy );

	i = 0;
	while( i < dist_count )
	{
		int omit;

		omit = 0;
		V2JOIN1( pt, ray_start, dist[i], ray_dir );
		V2SUB2( to_pt, pt, center );
		quad_pt = get_quadrant( to_pt, local_x, local_y, &vx, &vy );

		if( quad_start < quad_end )
		{
			if( quad_pt > quad_end )
				omit = 1;
			else if( quad_pt < quad_start )
				omit = 1;
			else if( quad_pt == quad_end )
			{
				switch( quad_pt )
				{
					case 1:
					case 2:
						if( vx < ex )
							omit = 1;
						break;
					case 3:
					case 4:
						if( vx > ex )
							omit = 1;
						break;
				}
			}
			else if( quad_pt == quad_start )
			{
				switch( quad_pt )
				{
					case 1:
					case 2:
						if( vx > sx )
							omit = 1;
						break;
					case 3:
					case 4:
						if( vx < sx )
							omit = 1;
						break;
				}
			}
		}
		else if( quad_start > quad_end )
		{
			if( quad_pt > quad_end && quad_pt < quad_start )
				omit = 1;
			else if( quad_pt == quad_end )
			{
				switch( quad_pt )
				{
					case 1:
					case 2:
						if( vx < ex )
							omit = 1;
						break;
					case 3:
					case 4:
						if( vx > ex )
							omit = 1;
						break;
				}
			}
			else if( quad_pt == quad_start )
			{
				switch( quad_pt )
				{
					case 1:
					case 2:
						if( vx > sx )
							omit = 1;
						break;
					case 3:
					case 4:
						if( vx < sx )
							omit = 1;
						break;
				}
			}
		}
		else		/* quad_start == quad_end */
		{
			if( quad_pt != quad_start )
				omit = 1;
			else
			{
				switch( quad_pt )
				{
					case 1:
					case 2:
						if( vx < ex || vx > sx )
							omit = 1;
						break;
					case 3:
					case 4:
						if( vx > ex || vx < sx )
							omit = 1;
						break;
				}
			}
		}
		if( omit )
		{
			if( i == 0 )
				dist[0] = dist[1];
			dist_count--;
		}
		else
			i++;
	}

	return( dist_count );
}



/**
 *  			R T _ E X T R U D E _ S H O T
 *
 *  Intersect a ray with a extrude.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_extrude_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
	register int i, j, k;
	fastf_t dist_top, dist_bottom, to_bottom=0;
	fastf_t dist[2];
	fastf_t dot_pl1, dir_dot_z;
	point_t tmp, tmp2;
	point_t ray_start, ray_dir, ray_dir_unit;	/* 2D */
	struct curve *crv;
	struct hit hits[MAX_HITS];
	fastf_t dists_before[MAX_HITS];
	fastf_t dists_after[MAX_HITS];
	fastf_t *dists=NULL;
	int dist_count=0;
	int hit_count=0;
	int hits_before_bottom=0, hits_after_top=0;
	int code;
	int check_inout=0;
	int top_face=TOP_FACE, bot_face=BOTTOM_FACE;
	int surfno= -42;
	int free_dists=0;
	point2d_t *verts;
	point2d_t *intercept;
	point2d_t *normal;
	point2d_t ray_perp;

	crv = &extr->crv;

	/* intersect with top and bottom planes */
	dot_pl1 = VDOT( rp->r_dir, extr->pl1 );
	if( NEAR_ZERO( dot_pl1, SMALL_FASTF ) )
	{
		/* ray is parallel to top and bottom faces */
		dist_bottom = DIST_PT_PLANE( rp->r_pt, extr->pl1 );
		dist_top = DIST_PT_PLANE( rp->r_pt, extr->pl2 );
		if( dist_bottom < 0.0 && dist_top < 0.0 )
			return( 0 );
		if( dist_bottom > 0.0 && dist_top > 0.0 )
			return( 0 );
		dist_bottom = -MAX_FASTF;
		dist_top = MAX_FASTF;
	}
	else
	{
		dist_bottom = -DIST_PT_PLANE( rp->r_pt, extr->pl1 )/dot_pl1;
		to_bottom = dist_bottom;					/* need to remember this */
		dist_top = -DIST_PT_PLANE( rp->r_pt, extr->pl2 )/dot_pl1;	/* pl1 and pl2 are parallel */
		if( dist_bottom > dist_top )
		{
			fastf_t tmp1;

			tmp1 = dist_bottom;
			dist_bottom = dist_top;
			dist_top = tmp1;
			top_face = BOTTOM_FACE;
			bot_face = TOP_FACE;
		}
	}

	/* rotate ray */
	MAT4X3PNT( ray_start, extr->rot, rp->r_pt );
	MAT4X3VEC( ray_dir, extr->rot, rp->r_dir );

	dir_dot_z = ray_dir[Z];
	if( dir_dot_z < 0.0 )
		dir_dot_z = -dir_dot_z;

	if( NEAR_ZERO( dir_dot_z - 1.0, SMALL_FASTF ) )
	{
		/* ray is parallel to extrusion vector
		 * set mode to just count intersections for Jordan Theorem
		 */
		check_inout = 1;

		/* set the ray start to the intersection of the original ray and the base plane */
		VJOIN1( tmp, rp->r_pt, to_bottom, rp->r_dir );
		MAT4X3PNT( ray_start, extr->rot, tmp );

		/* use the u vector as the ray direction */
		VMOVE( ray_dir, extr->u_vec );
	}

	/* intersect with projected curve */
	for( i=0 ; i<crv->seg_count ; i++ )
	{
		long *lng=(long *)crv->segments[i];
		struct line_seg *lsg;
		struct carc_seg *csg=NULL;
		struct bezier_seg *bsg=NULL;
		fastf_t diff;

		free_dists = 0;
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				VSUB2( tmp, extr->verts[lsg->end], extr->verts[lsg->start] );
				VMOVE( tmp2, extr->verts[lsg->start] );
				code = bn_isect_line2_line2( dist, ray_start, ray_dir, tmp2, tmp, &extr_tol );
				if( code < 1 )
					continue;

				if( dist[1] > 1.0 || dist[1] < 0.0 )
					continue;

				dists = dist;
				dist_count = 1;
				surfno = LINE_SEG;
				break;
			case CURVE_CARC_MAGIC:
				/* circular arcs become elliptical arcs when projected in the XY-plane */
				csg = (struct carc_seg *)lng;
				{
					vect_t ra, rb;
					fastf_t radius;

					if( csg->radius <= 0.0 )
					{
						/* full circle */
						radius = -csg->radius;

						/* build the ellipse, this actually builds a circle in 3D,
						 * but the intersection routine only uses the X and Y components
						 */
						VSCALE( ra, extr->rot_axis, radius );
						VSCALE( rb, extr->perp, radius );

						dist_count = isect_line2_ellipse( dist, ray_start, ray_dir, extr->verts[csg->end], ra, rb );
						MAT4X3PNT( tmp, extr->irot, extr->verts[csg->end] ); /* used later in hit->vpriv */
					}
					else
					{
						VSCALE( ra, extr->rot_axis, csg->radius );
						VSCALE( rb, extr->perp, csg->radius );
						dist_count = isect_line_earc( dist, ray_start, ray_dir, extr->verts[csg->center], ra, rb, extr->pl1_rot, extr->verts[csg->start], extr->verts[csg->end], csg->orientation );
						MAT4X3PNT( tmp, extr->irot, extr->verts[csg->center] ); /* used later in hit->vpriv */
					}
				}
				if( dist_count < 1 )
					continue;

				dists = dist;
				surfno = CARC_SEG;
				break;
			case CURVE_BEZIER_MAGIC:
				bsg = (struct bezier_seg *)lng;
				verts = (point2d_t *)bu_calloc( bsg->degree + 1, sizeof( point2d_t ), "Bezier verts" );
				for( j=0 ; j<=bsg->degree ; j++ ) {
					V2MOVE( verts[j], extr->verts[bsg->ctl_points[j]] );
				}
				V2MOVE( ray_dir_unit, ray_dir );
				diff = sqrt( MAG2SQ( ray_dir ) );
				ray_dir_unit[X] /= diff;
				ray_dir_unit[Y] /= diff;
				ray_dir_unit[Z] = 0.0;
				ray_perp[X] = ray_dir[Y];
				ray_perp[Y] = -ray_dir[X];
				dist_count = FindRoots( verts, bsg->degree, &intercept, &normal, ray_start, ray_dir_unit, ray_perp,
							0, extr_tol.dist );
				if( dist_count ) {
					free_dists = 1;
					dists = (fastf_t *)bu_calloc( dist_count, sizeof( fastf_t ), "dists (Bezier)" );
					for( j=0 ; j<dist_count ; j++ ) {
						point2d_t to_pt;
						V2SUB2( to_pt, intercept[j], ray_start );
						dists[j] = V2DOT( to_pt, ray_dir_unit) / diff;
					}
					bu_free( (char *)intercept, "Bezier intercept" );
					surfno = BEZIER_SEG;
				}
				bu_free( (char *)verts, "Bezier verts" );
				break;
			case CURVE_NURB_MAGIC:
				break;
			default:
				bu_log( "Unrecognized segment type in sketch (%s) referenced by extrusion (%s)\n",
					stp->st_dp->d_namep );
				bu_bomb( "Unrecognized segment type in sketch\n" );
				break;
		}

		/* eliminate duplicate hit distances */
		for( j=0 ; j<hit_count ; j++ )
		{
			k = 0;
			while( k < dist_count )
			{
				diff = dists[k] - hits[j].hit_dist;
				if( NEAR_ZERO( diff, extr_tol.dist ) )
				{
					int n;
					for( n=k ; n<dist_count-1 ; n++ ) {
						dists[n] = dists[n+1];
						if( *lng == CURVE_BEZIER_MAGIC ) {
							V2MOVE( normal[n], normal[n+1] );
						}
					}
					dist_count--;
				}
				else
					k++;
			}
		}

		/* eliminate duplicate hits below the bottom plane of the extrusion */
		for( j=0 ; j<hits_before_bottom ; j++ )
		{
			k = 0;
			while( k < dist_count )
			{
				diff = dists[k] - dists_before[j];
				if( NEAR_ZERO( diff, extr_tol.dist ) )
				{
					int n;

					for( n=k ; n<dist_count-1 ; n++ ) {
						dists[n] = dists[n+1];
						if( *lng == CURVE_BEZIER_MAGIC ) {
							V2MOVE( normal[n], normal[n+1] );
						}
					}
					dist_count--;
				}
				else
					k++;
			}
		}

		/* eliminate duplicate hits above the top plane of the extrusion */
		for( j=0 ; j<hits_after_top ; j++ )
		{
			k = 0;
			while( k < dist_count )
			{
				diff = dists[k] - dists_after[j];
				if( NEAR_ZERO( diff, extr_tol.dist ) )
				{
					int n;

					for( n=k ; n<dist_count-1 ; n++ )
						dists[n] = dists[n+1];
					dist_count--;
				}
				else
					k++;
			}
		}

		/* if we are just doing the Jordan curve thereom */
		if( check_inout )
		{
			for( j=0 ; j<dist_count ; j++ )
			{
				if( dists[j] < 0.0 )
					hit_count++;
			}
			continue;
		}

		/* process remaining distances into hits */
		for( j=0 ; j<dist_count ; j++ )
		{
			if( dists[j] < dist_bottom )
			{
				if( hits_before_bottom >= MAX_HITS )
				{
					bu_log( "ERROR: rt_extrude_shot: too many hits before bottom on extrusion (%s), limit is %d\n",
					stp->st_dp->d_namep, MAX_HITS );
					bu_bomb( "ERROR: rt_extrude_shot: too many hits before bottom on extrusion\n" );
				}
				dists_before[hits_before_bottom] = dists[j];
				hits_before_bottom++;
				continue;
			}
			if( dists[j] > dist_top )
			{
				if( hits_after_top >= MAX_HITS )
				{
					bu_log( "ERROR: rt_extrude_shot: too many hits after top on extrusion (%s), limit is %d\n",
					stp->st_dp->d_namep, MAX_HITS );
					bu_bomb( "ERROR: rt_extrude_shot: too many hits after top on extrusion\n" );
				}
				dists_after[hits_after_top] = dists[j];
				hits_after_top++;

				continue;
			}

			/* got a hit at distance dists[j] */
			if( hit_count >= MAX_HITS )
			{
				bu_log( "Too many hits on extrusion (%s), limit is %d\n",
					stp->st_dp->d_namep, MAX_HITS );
				bu_bomb( "Too many hits on extrusion\n" );
			}
			hits[hit_count].hit_magic = RT_HIT_MAGIC;
			hits[hit_count].hit_dist = dists[j];
			hits[hit_count].hit_surfno = surfno;
			switch( *lng )
			{
				case CURVE_CARC_MAGIC:
					hits[hit_count].hit_private = (genptr_t)csg;
					VMOVE( hits[hit_count].hit_vpriv, tmp );
					break;
				case CURVE_LSEG_MAGIC:
					VMOVE( hits[hit_count].hit_vpriv, tmp );
					break;
				case CURVE_BEZIER_MAGIC:
					V2MOVE( hits[hit_count].hit_vpriv, normal[j] );
					hits[hit_count].hit_vpriv[Z] = 0.0;
					break;
				default:
					bu_log( "ERROR: rt_extrude_shot: unrecognized segment type in solid %s\n",
						stp->st_dp->d_namep );
					bu_bomb( "ERROR: rt_extrude_shot: unrecognized segment type in solid\n" );
					break;
			}
			hit_count++;
		}
		if( free_dists )
			bu_free( (char *)dists, "dists" );
	}

	if( check_inout )
	{
		if( hit_count&1 )
		{
			register struct seg *segp;

			hit_count = 2;
			hits[0].hit_magic = RT_HIT_MAGIC;
			hits[0].hit_dist = dist_bottom;
			hits[0].hit_surfno = bot_face;
			VMOVE( hits[0].hit_normal, extr->pl1 );

			hits[1].hit_magic = RT_HIT_MAGIC;
			hits[1].hit_dist = dist_top;
			hits[1].hit_surfno = -top_face;
			VMOVE( hits[1].hit_normal, extr->pl1 );

			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in = hits[0];		/* struct copy */
			segp->seg_out = hits[1];	/* struct copy */
			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
			return( 2 );
		}
		else
		{
			return( 0 );
		}
	}

	if( hit_count )
	{
		/* Sort hits, Near to Far */
		rt_hitsort( hits, hit_count );
	}

	if( hits_before_bottom & 1 )
	{
		if( hit_count >= MAX_HITS )
		{
			bu_log( "Too many hits on extrusion (%s), limit is %d\n",
				stp->st_dp->d_namep, MAX_HITS );
			bu_bomb( "Too many hits on extrusion\n" );
		}
		for( i=hit_count-1 ; i>=0 ; i-- )
			hits[i+1] = hits[i];
		hits[0].hit_magic = RT_HIT_MAGIC;
		hits[0].hit_dist = dist_bottom;
		hits[0].hit_surfno = bot_face;
		VMOVE( hits[0].hit_normal, extr->pl1 );
		hit_count++;
	}

	if( hits_after_top & 1 )
	{
		if( hit_count >= MAX_HITS )
		{
			bu_log( "Too many hits on extrusion (%s), limit is %d\n",
				stp->st_dp->d_namep, MAX_HITS );
			bu_bomb( "Too many hits on extrusion\n" );
		}
		hits[hit_count].hit_magic = RT_HIT_MAGIC;
		hits[hit_count].hit_dist = dist_top;
		hits[hit_count].hit_surfno = top_face;
		VMOVE( hits[hit_count].hit_normal, extr->pl1 );
		hit_count++;
	}

	if( hit_count%2 )
	{
		point_t pt;

		if( hit_count != 1 ) {
			bu_log( "ERROR: rt_extrude_shot(): odd number of hits (%d) (ignoring last hit)\n", hit_count );
			bu_log( "ray start = (%20.10f %20.10f %20.10f)\n", V3ARGS( rp->r_pt ) );
			bu_log( "\tray dir = (%20.10f %20.10f %20.10f)", V3ARGS( rp->r_dir ) );
			VJOIN1( pt, rp->r_pt, hits[hit_count-1].hit_dist, rp->r_dir );
			bu_log( "\tignored hit at (%g %g %g)\n", V3ARGS( pt ) );
		}
		hit_count--;
	}

	/* build segments */
	{
		register struct seg *segp;

		for( i=0; i < hit_count; i += 2 )  {
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in = hits[i];		/* struct copy */
			segp->seg_out = hits[i+1];	/* struct copy */
			segp->seg_out.hit_surfno = -segp->seg_out.hit_surfno;	/* for exit hits */
			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
	}

	return( hit_count );
}

#define RT_EXTRUDE_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ E X T R U D E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_extrude_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */

{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ E X T R U D E _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_extrude_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
        struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
	fastf_t alpha;
	point_t hit_in_plane;
	vect_t tmp, tmp2;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	switch( hitp->hit_surfno )
	{
		case LINE_SEG:
			MAT4X3VEC( tmp, extr->irot, hitp->hit_vpriv );
			VCROSS( hitp->hit_normal, extr->unit_h, tmp );
			VUNITIZE( hitp->hit_normal );
			break;
		case -LINE_SEG:
			MAT4X3VEC( tmp, extr->irot, hitp->hit_vpriv );
			VCROSS( hitp->hit_normal, extr->unit_h, tmp );
			VUNITIZE( hitp->hit_normal );
			break;
		case TOP_FACE:
		case BOTTOM_FACE:
		case -TOP_FACE:
		case -BOTTOM_FACE:
			break;
		case CARC_SEG:
		case -CARC_SEG:
			alpha = DIST_PT_PLANE( hitp->hit_point, extr->pl1 ) / VDOT( extr->unit_h, extr->pl1 );
			VJOIN1( hit_in_plane, hitp->hit_point, -alpha, extr->unit_h );
			VSUB2( tmp, hit_in_plane, hitp->hit_vpriv );
			VCROSS( tmp2, extr->pl1, tmp );
			VCROSS( hitp->hit_normal, tmp2, extr->unit_h );
			VUNITIZE( hitp->hit_normal );
			break;
		case BEZIER_SEG:
		case -BEZIER_SEG:
			MAT4X3VEC( hitp->hit_normal, extr->irot, hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
			break;
		default:
			bu_bomb( "ERROR: rt_extrude_norm(): unrecognized surf_no in hit structure!!!\n" );
			break;
	}
	if( hitp->hit_surfno < 0 )
	{
		if( VDOT( hitp->hit_normal, rp->r_dir ) < 0.0 )
			VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	else
	{
		if( VDOT( hitp->hit_normal, rp->r_dir ) > 0.0 )
			VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}

}

/**
 *			R T _ E X T R U D E _ C U R V E
 *
 *  Return the curvature of the extrude.
 */
void
rt_extrude_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
        struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
	struct carc_seg *csg;
	fastf_t radius, a, b, a_sq, b_sq;
	fastf_t curvature, tmp, dota, dotb;
	fastf_t der;
	vect_t diff;
	vect_t ra, rb;

	switch( hitp->hit_surfno )
	{
		case LINE_SEG:
		case -LINE_SEG:
			VMOVE( cvp->crv_pdir, hitp->hit_vpriv );
			VUNITIZE( cvp->crv_pdir );
			cvp->crv_c1 = cvp->crv_c2 = 0;
			break;
		case CARC_SEG:
		case -CARC_SEG:
			/* curvature for an ellipse (the rotated and projected circular arc) in XY-plane
			 * based on curvature for ellipse = |ra||rb|/(|derivative|**3)
			 */
			csg = (struct carc_seg *)hitp->hit_private;
			VCROSS( cvp->crv_pdir, extr->unit_h, hitp->hit_normal );
			VSUB2( diff, hitp->hit_point, hitp->hit_vpriv );
			if( csg->radius < 0.0 )
				radius = -csg->radius;
			else
				radius = csg->radius;
			VSCALE( ra, extr->rot_axis, radius );
			VSCALE( rb, extr->perp, radius );

			a_sq = MAG2SQ( ra );
			b_sq = MAG2SQ( rb );
			a = sqrt( a_sq );
			b = sqrt( b_sq );
			dota = VDOT( diff, ra );
			dotb = VDOT( diff, rb );
			tmp = (a_sq/(b_sq*b_sq))*dotb*dotb + (b_sq/(a_sq*a_sq))*dota*dota;
			der = sqrt( tmp );
			curvature = a*b/(der*der*der);
			if( VDOT( hitp->hit_normal, diff ) > 0.0 )
				cvp->crv_c1 = curvature;
			else
				cvp->crv_c1 = -curvature;
			cvp->crv_c2 = 0;
			break;
	}
}

/**
 *  			R T _ E X T R U D E _ U V
 *
 *  For a hit on the surface of an extrude, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_extrude_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
}

/**
 *		R T _ E X T R U D E _ F R E E
 */
void
rt_extrude_free(register struct soltab *stp)
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;

	if( extrude->verts )
		bu_free( (char *)extrude->verts, "extrude->verts" );
	rt_curve_free( &(extrude->crv) );
	bu_free( (char *)extrude, "extrude_specific" );
}

/**
 *			R T _ E X T R U D E _ C L A S S
 */
int
rt_extrude_class(void)
{
	return(0);
}

/**
 *			R T _ E X T R U D E _ P L O T
 */
int
rt_extrude_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct curve			*crv=(struct curve *)NULL;
	struct rt_sketch_internal	*sketch_ip;
	point_t				end_of_h;
	int				i1, i2, nused1, nused2;
	struct bn_vlist			*vp1, *vp2, *vp2_start;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	if( !extrude_ip->skt )
	{
		bu_log( "ERROR: no sketch to extrude!\n" );
		RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_DRAW );
		return( 0 );
	}

	sketch_ip = extrude_ip->skt;
	RT_SKETCH_CK_MAGIC( sketch_ip );

	crv = &sketch_ip->skt_curve;

	/* empty sketch, nothing to do */
#if 0
	if (crv->seg_count == 0)
	{
	    if (extrude_ip->sketch_name) {
		bu_log("Sketch [%s] is empty, nothing to draw\n", extrude_ip->sketch_name);
	    } else {
		bu_log("Unnamed sketch is empty, nothing to draw\n");
	    }
	    RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_MOVE );
	    RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_DRAW );
	    return( 0 );
	}
#endif

	/* plot bottom curve */
	vp1 = BU_LIST_LAST( bn_vlist, vhead );
	nused1 = vp1->nused;
	if( curve_to_vlist( vhead, ttol, extrude_ip->V, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, crv ) )
	{
		bu_log( "ERROR: sketch (%s) references non-existent vertices!\n",
			extrude_ip->sketch_name );
		return( -1 );
	}

	/* plot top curve */
	VADD2( end_of_h, extrude_ip->V, extrude_ip->h );
	vp2 = BU_LIST_LAST( bn_vlist, vhead );
	nused2 = vp2->nused;
	curve_to_vlist( vhead, ttol, end_of_h, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, crv );

	/* plot connecting lines */
	vp2_start = vp2;
	i1 = nused1;
	if (i1 >= vp1->nused) {
	    i1 = 0;
	    vp1 = BU_LIST_NEXT( bn_vlist, &vp1->l );
	}
	i2 = nused2;
	if (i2 >= vp2->nused) {
	    i2 = 0;
	    vp2 = BU_LIST_NEXT( bn_vlist, &vp2->l );
	    nused2--;
	}

	while( vp1 != vp2_start || (i1 < BN_VLIST_CHUNK && i2 < BN_VLIST_CHUNK && i1 != nused2) )
	{
		RT_ADD_VLIST( vhead, vp1->pt[i1], BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, vp2->pt[i2], BN_VLIST_LINE_DRAW );
		i1++;
		if( i1 >= vp1->nused )
		{
			i1 = 0;
			vp1 = BU_LIST_NEXT( bn_vlist, &vp1->l );
		}
		i2++;
		if( i2 >= vp2->nused )
		{
			i2 = 0;
			vp2 = BU_LIST_NEXT( bn_vlist, &vp2->l );
		}
	}

	return(0);
}

void
get_indices( genptr_t seg, int *start, int *end )
{
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;
	struct line_seg *lsg=(struct line_seg *)seg;

	switch (lsg->magic) {
		case CURVE_LSEG_MAGIC:
			*start = lsg->start;
			*end = lsg->end;
			break;
		case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)seg;
			if( csg->radius < 0.0 ) {
				*start = csg->start;
				*end = *start;
				break;
			}
			*start = csg->start;
			*end = csg->end;
			break;
		case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)seg;
			*start = nsg->ctl_points[0];
			*end = nsg->ctl_points[nsg->c_size-1];
			break;
		case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)seg;
			*start = bsg->ctl_points[0];
			*end = bsg->ctl_points[bsg->degree];
			break;
	}
}

void
get_seg_midpoint( genptr_t seg, struct rt_sketch_internal *skt, point2d_t pt )
{
	struct edge_g_cnurb eg;
	point_t tmp_pt;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;
	long *lng;
	point2d_t *V;
	point2d_t pta;
	int i;
	int coords;

	lng = (long *)seg;

	switch( *lng )
		{
		case CURVE_LSEG_MAGIC:
			lsg = (struct line_seg *)lng;
			VADD2_2D( pta, skt->verts[lsg->start], skt->verts[lsg->end] );
			VSCALE_2D( pt, pta, 0.5 );
			break;
		case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)lng;
			if( csg->radius < 0.0 ) {
				VMOVE_2D( pt, skt->verts[csg->start] );
			} else {
				point2d_t start2d, end2d, mid_pt, s2m, dir, center2d;
				fastf_t tmp_len, len_sq, mid_ang, s2m_len_sq, cross_z;
				fastf_t start_ang, end_ang;

				/* this is an arc (not a full circle) */
				V2MOVE( start2d, skt->verts[csg->start] );
				V2MOVE( end2d, skt->verts[csg->end] );
				mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
				mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
				V2SUB2( s2m, mid_pt, start2d );
				dir[0] = -s2m[1];
				dir[1] = s2m[0];
				s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
				if( s2m_len_sq < SMALL_FASTF ) {
					bu_log( "start and end points are too close together in circular arc of sketch\n" );
					break;
				}
				len_sq = csg->radius*csg->radius - s2m_len_sq;
				if( len_sq < 0.0 ) {
					bu_log( "Impossible radius for specified start and end points in circular arc\n");
					break;
				}
				tmp_len = sqrt( dir[0]*dir[0] + dir[1]*dir[1] );
				dir[0] = dir[0] / tmp_len;
				dir[1] = dir[1] / tmp_len;
				tmp_len = sqrt( len_sq );
				V2JOIN1( center2d, mid_pt, tmp_len, dir );

				/* check center location */
				cross_z = ( end2d[X] - start2d[X] )*( center2d[Y] - start2d[Y] ) -
					( end2d[Y] - start2d[Y] )*( center2d[X] - start2d[X] );
				if( !(cross_z > 0.0 && csg->center_is_left) ) {
					V2JOIN1( center2d, mid_pt, -tmp_len, dir );
				}
				start_ang = atan2( start2d[Y]-center2d[Y], start2d[X]-center2d[X] );
				end_ang = atan2( end2d[Y]-center2d[Y], end2d[X]-center2d[X] );
				if( csg->orientation ) { /* clock-wise */
					while( end_ang > start_ang )
						end_ang -= 2.0 * M_PI;
				}
				else { /* counter-clock-wise */
					while( end_ang < start_ang )
						end_ang += 2.0 * M_PI;
				}

				/* get mid angle */
				mid_ang = (start_ang + end_ang ) * 0.5;

				/* calculate mid point */
				pt[X] = center2d[X] + csg->radius * cos( mid_ang );
				pt[Y] = center2d[Y] + csg->radius * sin( mid_ang );
					break;
				}
			break;
		case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)lng;

			eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
			eg.order = nsg->order;
			eg.k.k_size = nsg->k.k_size;
			eg.k.knots = nsg->k.knots;
			eg.c_size = nsg->c_size;
			coords = 2 + RT_NURB_IS_PT_RATIONAL( nsg->pt_type );
			eg.pt_type = RT_NURB_MAKE_PT_TYPE( coords, 2, RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) );
			eg.ctl_points = (fastf_t *)bu_malloc( nsg->c_size * coords * sizeof( fastf_t ), "eg.ctl_points" );
			if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) ) {
				for( i=0 ; i<nsg->c_size ; i++ ) {
					VMOVE_2D( &eg.ctl_points[i*coords], skt->verts[nsg->ctl_points[i]] );
					eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
				}
			}
			else {
				for( i=0 ; i<nsg->c_size ; i++ ) {
					VMOVE_2D( &eg.ctl_points[i*coords], skt->verts[nsg->ctl_points[i]] );
				}
			}
			rt_nurb_c_eval( &eg, (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0]) * 0.5, tmp_pt );
			if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) ) {
				int j;

				for( j=0 ; j<coords-1 ; j++ )
					pt[j] = tmp_pt[j] / tmp_pt[coords-1];
			} else {
				V2MOVE( pt, tmp_pt );
			}
			bu_free( (char *)eg.ctl_points, "eg.ctl_points" );
			break;
		case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)lng;
			V = (point2d_t *)bu_calloc( bsg->degree+1, sizeof( point2d_t ), "Bezier control points" );
			for( i=0 ; i<= bsg->degree ; i++ ) {
				VMOVE_2D( V[i], skt->verts[bsg->ctl_points[i]] );
			}
			Bezier( V, bsg->degree, 0.51, NULL, NULL, pt, NULL );
			bu_free( (char *)V, "Bezier control points" );
			break;
		default:
			bu_bomb( "Unrecognized segment type in sketch\n");
			break;
		}
}

struct loop_inter {
	int			which_loop;
	int			vert_index;	/* index of vertex intersected, or -1 if no hit on a vertex */
	fastf_t			dist;		/* hit distance */
	struct loop_inter	*next;
};

void
isect_2D_loop_ray( point2d_t pta, point2d_t dir, struct bu_ptbl *loop, struct loop_inter **root,
		   int which_loop, struct rt_sketch_internal *ip, struct bn_tol *tol )
{
	int i, j;
	int code;
	point2d_t norm;
	fastf_t dist[2];

	norm[0] = -dir[1];
	norm[1] = dir[0];

	for( i=0 ; i<BU_PTBL_END( loop ) ; i++ ) {
		long *lng;
		struct loop_inter *inter;
		struct line_seg *lsg=NULL;
		struct carc_seg *csg=NULL;
		struct bezier_seg *bsg=NULL;
		point2d_t d1;
		point2d_t diff;
		fastf_t radius;
		point2d_t *verts;
		point2d_t *intercept;
		point2d_t *normal;

		lng = BU_PTBL_GET( loop, i );
		switch( *lng ) {
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				V2SUB2( d1, ip->verts[lsg->end], ip->verts[lsg->start] );
				code = bn_isect_line2_lseg2( dist, pta, dir, ip->verts[lsg->start], d1, tol );
				if( code < 0 )
					break;
				if( code == 0 ) {
					/* edge is collinear with ray */
					/* add two intersections, one at each end vertex */
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = lsg->start;
					inter->dist = dist[0];
					inter->next = NULL;
					if( !(*root) ) {
						(*root) = inter;
					} else {
						inter->next = (*root);
						(*root) = inter;
					}
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = lsg->end;
					inter->dist = dist[1];
					inter->next = NULL;
					inter->next = (*root);
					(*root) = inter;
				} else if( code == 1 ) {
					/* hit at start vertex */
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = lsg->start;
					inter->dist = dist[0];
					inter->next = NULL;
					if( !(*root) ) {
						(*root) = inter;
					} else {
						inter->next = (*root);
						(*root) = inter;
					}
				} else if( code == 2 ) {
					/* hit at end vertex */
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = lsg->end;
					inter->dist = dist[0];
					inter->next = NULL;
					if( !(*root) ) {
						(*root) = inter;
					} else {
						inter->next = (*root);
						(*root) = inter;
					}
				} else {
					/* hit on edge, not at a vertex */
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = -1;
					inter->dist = dist[0];
					inter->next = NULL;
					if( !(*root) ) {
						(*root) = inter;
					} else {
						inter->next = (*root);
						(*root) = inter;
					}
				}
				break;
			case CURVE_CARC_MAGIC:
				csg = (struct carc_seg *)lng;
				radius = csg->radius;
				if( csg->radius <= 0.0 ) {
					point2d_t ra, rb;

					V2SUB2( diff, ip->verts[csg->start], ip->verts[csg->end] );
					radius = sqrt( MAG2SQ( diff ) );
					ra[X] = radius;
					ra[Y] = 0.0;
					rb[X] = 0.0;
					rb[Y] = radius;
					code = isect_line2_ellipse( dist, pta, dir, ip->verts[csg->end],
								    ra, rb );

					if( code <= 0 )
						break;
					for( j=0 ; j<code ; j++ ) {
						inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
						inter->which_loop = which_loop;
						inter->vert_index = -1;
						inter->dist = dist[j];
						inter->next = NULL;
						if( !(*root) ) {
							(*root) = inter;
						} else {
							inter->next = (*root);
							(*root) = inter;
						}
					}

				} else {
					point2d_t ra, rb;
					vect_t s2m, tmp_dir;
					point2d_t start2d, end2d, mid_pt, center2d;
					fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;

					V2MOVE( start2d, ip->verts[csg->start] );
					V2MOVE( end2d, ip->verts[csg->end] );
					mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
					mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
					V2SUB2( s2m, mid_pt, start2d )
						tmp_dir[0] = -s2m[1];
					tmp_dir[1] = s2m[0];
					s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
					if( s2m_len_sq < SMALL_FASTF )
						{
							bu_log( "start and end points are too close together in circular arc of sketch\n" );
							break;
						}
					len_sq = radius*radius - s2m_len_sq;
					if( len_sq < 0.0 )
						{
							bu_log( "Impossible radius for specified start and end points in circular arc\n");
							break;
						}
					tmp_len = sqrt( tmp_dir[0]*tmp_dir[0] + tmp_dir[1]*tmp_dir[1] );
					tmp_dir[0] = tmp_dir[0] / tmp_len;
					tmp_dir[1] = tmp_dir[1] / tmp_len;
					tmp_len = sqrt( len_sq );
					V2JOIN1( center2d, mid_pt, tmp_len, tmp_dir )

						/* check center location */
						cross_z = ( end2d[X] - start2d[X] )*( center2d[Y] - start2d[Y] ) -
						( end2d[Y] - start2d[Y] )*( center2d[X] - start2d[X] );
					if( !(cross_z > 0.0 && csg->center_is_left) )
						V2JOIN1( center2d, mid_pt, -tmp_len, tmp_dir );

					ra[X] = radius;
					ra[Y] = 0.0;
					rb[X] = 0.0;
					rb[Y] = radius;
					code = isect_line_earc( dist, pta, dir, center2d, ra, rb,
								norm, ip->verts[csg->start], ip->verts[csg->end],
								csg->orientation );
					if( code <= 0 )
						break;
					for( j=0 ; j<code ; j++ ) {
						inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
						inter->which_loop = which_loop;
						inter->vert_index = -1;
						inter->dist = dist[j];
						inter->next = NULL;
						if( !(*root) ) {
							(*root) = inter;
						} else {
							inter->next = (*root);
							(*root) = inter;
						}
					}
				}
				break;
			case CURVE_BEZIER_MAGIC:
				bsg = (struct bezier_seg *)lng;
				intercept = NULL;
				normal = NULL;
				verts = (point2d_t *)bu_calloc( bsg->degree + 1, sizeof( point2d_t ), "Bezier verts" );
				for( j=0 ; j<=bsg->degree ; j++ ) {
					V2MOVE( verts[j], ip->verts[bsg->ctl_points[j]] );
				}
				code = FindRoots( verts, bsg->degree, &intercept, &normal, pta, dir, norm, 0, tol->dist );
				for( j=0 ; j<code ; j++ ) {
					V2SUB2( diff, intercept[j], pta );
					dist[0] = sqrt( MAG2SQ( diff ) );
					inter = (struct loop_inter *)bu_calloc( sizeof( struct loop_inter ), 1,
										"loop intersection" );
					inter->which_loop = which_loop;
					inter->vert_index = -1;
					inter->dist = dist[0];
					inter->next = NULL;
					if( !(*root) ) {
						(*root) = inter;
					} else {
						inter->next = (*root);
						(*root) = inter;
					}
				}
				if( (*intercept) )
					bu_free( (char *)intercept, "Bezier Intercepts" );
				if( (*normal) )
					bu_free( (char *)normal, "Bezier normals" );
				bu_free( ( char *)verts, "Bezier Ctl points" );
				break;
			default:
				bu_log( "isect_2D_loop_ray: Unrecognized curve segment type x%x\n", *lng );
				bu_bomb( "isect_2D_loop_ray: Unrecognized curve segment type\n" );
				break;
		}
	}
}

static void
sort_intersections( struct loop_inter **root, struct bn_tol *tol )
{
	struct loop_inter *ptr, *prev, *pprev;
	int done=0;
	fastf_t diff;

	/* eliminate any duplicates */
	ptr = (*root);
	while( ptr->next ) {
		prev = ptr;
		ptr = ptr->next;
		if( ptr->vert_index > -1 && ptr->vert_index == prev->vert_index ) {
			prev->next = ptr->next;
			bu_free( (char *)ptr, "struct loop_inter" );
			ptr = prev;
		}
	}

	ptr = (*root);
	while( ptr->next ) {
		prev = ptr;
		ptr = ptr->next;
		diff = fabs( ptr->dist - prev->dist );
		if( diff < tol->dist ) {
			prev->next = ptr->next;
			bu_free( (char *)ptr, "struct loop_inter" );
			ptr = prev;
		}
	}

	while( !done ) {
		done = 1;
		ptr = (*root);
		prev = NULL;
		pprev = NULL;
		while( ptr->next ) {
			pprev = prev;
			prev = ptr;
			ptr = ptr->next;
			if( ptr->dist < prev->dist ) {
				done = 0;
				if( pprev ) {
					prev->next = ptr->next;
					pprev->next = ptr;
					ptr->next = prev;
				} else {
					prev->next = ptr->next;
					ptr->next = prev;
					(*root) = ptr;
				}
			}
		}
	}
}

int
classify_sketch_loops( struct bu_ptbl *loopa, struct bu_ptbl *loopb, struct rt_sketch_internal *ip )
{
	struct loop_inter *inter_root=NULL, *ptr, *tmp;
	struct bn_tol tol;
	point2d_t pta, ptb;
	point2d_t dir;
	genptr_t seg;
	fastf_t inv_len;
	int loopa_count=0, loopb_count=0;
	int ret=UNKNOWN;

	BU_CK_PTBL( loopa );
	BU_CK_PTBL( loopb );
	RT_SKETCH_CK_MAGIC( ip );

	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1.0e-5;;
	tol.para = 1.0 - tol.perp;

	/* find points on a midpoint of a segment for each loop */
	seg = (genptr_t)BU_PTBL_GET( loopa, 0 );
	get_seg_midpoint( seg, ip, pta );
	seg = (genptr_t)BU_PTBL_GET( loopb, 0 );
	get_seg_midpoint( seg, ip, ptb );

	V2SUB2( dir, ptb, pta );
	inv_len = 1.0 / sqrt( MAGSQ_2D( dir ) );
	V2SCALE( dir, dir, inv_len );

	/* intersect pta<->ptb line with both loops */
        isect_2D_loop_ray( pta, dir, loopa, &inter_root, LOOPA, ip, &tol );
	isect_2D_loop_ray( pta, dir, loopb, &inter_root, LOOPB, ip, &tol );

	sort_intersections( &inter_root, &tol );

	/* examine intercepts to determine loop relationship */
	ptr = inter_root;
	while( ptr ) {
		tmp = ptr;
		if( ret == UNKNOWN ) {
			if( ptr->which_loop == LOOPA ) {
				loopa_count++;
				if( loopa_count && loopb_count ) {
					if( loopb_count % 2 ) {
						ret = A_IN_B;
					} else {
						ret = DISJOINT;
					}
				}
			} else {
				loopb_count++;
				if( loopa_count && loopb_count ) {
					if( loopa_count % 2 ) {
						ret = B_IN_A;
					} else {
						ret = DISJOINT;
					}
				}
			}
		}
		ptr = ptr->next;
		bu_free( (char *)tmp, "loop intercept" );
	}

	return( ret );
}

/*
 *			R T _ E X T R U D E _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_extrude_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
#if 0
	return( -1 );
#else
	struct bu_list			vhead;
	struct shell			*s;
	struct faceuse			*fu;
	struct vertex			***verts;
	struct vertex			**vertsa;
	int				vert_count=0;
	struct rt_extrude_internal	*extrude_ip;
	struct rt_sketch_internal	*sketch_ip;
	struct curve			*crv=(struct curve *)NULL;
	struct bu_ptbl			*aloop=NULL, loops, **containing_loops, *outer_loop;
	int				i, j, k;
	int				*used_seg;
	struct bn_vlist			*vlp;
	plane_t				pl;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	if( !extrude_ip->skt )
	{
		bu_log( "rt_extrude_tess: ERROR: no sketch for extrusion!!!!\n" );
		return( -1 );
	}

	sketch_ip = extrude_ip->skt;
	RT_SKETCH_CK_MAGIC( sketch_ip );

	crv = &sketch_ip->skt_curve;

	if( crv->seg_count < 1 )
		return( 0 );

	/* find all the loops */
	used_seg = (int *)bu_calloc( crv->seg_count, sizeof( int ), "used_seg" );
	bu_ptbl_init( &loops, 5, "loops" );
	for( i=0 ; i<crv->seg_count ; i++ ) {
		genptr_t cur_seg;
		int loop_start, loop_end;
		int seg_start, seg_end;

		if( used_seg[i] )
			continue;

		aloop = (struct bu_ptbl *)bu_calloc( 1, sizeof( struct bu_ptbl ), "aloop" );
		bu_ptbl_init( aloop, 5, "aloop" );

		bu_ptbl_ins( aloop, (long *)crv->segments[i] );
		used_seg[i] = 1;
		cur_seg = crv->segments[i];
		get_indices( cur_seg, &loop_start, &loop_end );

		while( loop_end != loop_start ) {
			int j;
			int added_seg;

			added_seg = 0;
			for( j=0 ; j<crv->seg_count ; j++ ) {
				if( used_seg[j] )
					continue;

				get_indices( crv->segments[j], &seg_start, &seg_end );
				if( seg_start != seg_end && seg_start == loop_end ) {
					added_seg++;
					bu_ptbl_ins( aloop, (long *)crv->segments[j] );
					used_seg[j] = 1;
					loop_end = seg_end;
					if( loop_start == loop_end )
						break;
				}
			}
			if( !added_seg ) {
				bu_log( "rt_extrude_tess: A loop is not closed in sketch %s\n",
					extrude_ip->sketch_name );
				bu_log( "\ttessellation failed!!\n" );
				for( j=0 ; j<BU_PTBL_END( &loops ) ; j++ ) {
					aloop = (struct bu_ptbl *)BU_PTBL_GET( &loops, j );
					bu_ptbl_free( aloop );
					bu_free( (char *)aloop, "aloop" );
				}
				bu_ptbl_free( &loops );
				bu_free( ( char *)used_seg, "used_seg" );
				return( -2 );
			}
		}
		bu_ptbl_ins( &loops, (long *)aloop );
	}
	bu_free( ( char *)used_seg, "used_seg" );

	/* sort the loops to find inside/outside relationships */
	containing_loops = (struct bu_ptbl **)bu_calloc( BU_PTBL_END( &loops ),
							 sizeof( struct bu_ptbl *), "containing_loops" );
	for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		containing_loops[i] = (struct bu_ptbl *)bu_calloc( 1, sizeof( struct bu_ptbl ), "containing_loops[i]" );
		bu_ptbl_init( containing_loops[i], BU_PTBL_END( &loops ), "containing_loops[i]" );
	}

	for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		struct bu_ptbl *loopa;
		int j;

		loopa = (struct bu_ptbl *)BU_PTBL_GET( &loops, i );
		for( j=i+1 ; j<BU_PTBL_END( &loops ) ; j++ ) {
			struct bu_ptbl *loopb;

			loopb = (struct bu_ptbl *)BU_PTBL_GET( &loops, j );
			switch( classify_sketch_loops( loopa, loopb, sketch_ip ) ) {
				case A_IN_B:
					bu_ptbl_ins( containing_loops[i], (long *)loopb );
					break;
				case B_IN_A:
					bu_ptbl_ins( containing_loops[j], (long *)loopa );
					break;
				case DISJOINT:
					break;
				default:
					bu_log( "rt_extrude_tess: Failed to classify loops!!\n" );
					goto failed;
			}
		}
	}

	/* make loops */

	/* find an outermost loop */
	outer_loop = (struct bu_ptbl *)NULL;
	for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		if( BU_PTBL_END( containing_loops[i] ) == 0 ) {
			outer_loop = (struct bu_ptbl *)BU_PTBL_GET( &loops, i );
			break;
		}
	}

	if( !outer_loop ) {
		bu_log( "No outer loop in sketch %s\n", extrude_ip->sketch_name );
		bu_log( "\ttessellation failed\n" );
		for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		}
		for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
			aloop = (struct bu_ptbl *)BU_PTBL_GET( &loops, i );
			bu_ptbl_free( aloop );
			bu_free( (char *)aloop, "aloop" );
			bu_ptbl_free( containing_loops[i] );
			bu_free( (char *)containing_loops[i], "aloop" );
		}
		bu_ptbl_free( &loops );
		bu_free( (char *)containing_loops, "containing_loops" );
	}

	BU_LIST_INIT( &vhead );
	if( BU_LIST_UNINITIALIZED( &rt_g.rtg_vlfree ) ) {
		BU_LIST_INIT( &rt_g.rtg_vlfree );
	}
	for( i=0 ; i<BU_PTBL_END( outer_loop ) ; i++ ) {
		genptr_t seg;

		seg = (genptr_t)BU_PTBL_GET( outer_loop, i );
		if( seg_to_vlist( &vhead, ttol, extrude_ip->V, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, seg ) )
			goto failed;
	}

	/* count vertices */
	vert_count = 0;
	for( BU_LIST_FOR( vlp, bn_vlist, &vhead ) ) {
		for( i=0 ; i<vlp->nused ; i++ ) {
			if( vlp->cmd[i] == BN_VLIST_LINE_DRAW )
				vert_count++;
		}
	}

	*r = nmg_mrsv( m );
	s = BU_LIST_FIRST( shell, &((*r)->s_hd) );

	/* make initial face from outer_loop */
	verts = (struct vertex ***)bu_calloc( vert_count, sizeof( struct vertex **), "verts" );
	for( i=0 ; i<vert_count ; i++ ) {
		verts[i] = (struct vertex **)bu_calloc( 1, sizeof( struct vertex *), "verts[i]" );
	}

	fu = nmg_cmface( s, verts, vert_count );
	j = 0;
	for( BU_LIST_FOR( vlp, bn_vlist, &vhead ) ) {
		for( i=0 ; i<vlp->nused ; i++ ) {
			if( vlp->cmd[i] == BN_VLIST_LINE_DRAW ) {
				nmg_vertex_gv( *verts[j], vlp->pt[i] );
				j++;
			}
		}
	}
	BN_FREE_VLIST( &rt_g.rtg_vlfree, &vhead );

	/* make sure face normal is in correct direction */
	bu_free( (char *)verts, "verts" );
	if( nmg_calc_face_plane( fu, pl ) ) {
		bu_log( "Failed to calculate face plane for extrusion\n" );
		return( -1 );
	}
	nmg_face_g( fu, pl );
	if( VDOT( pl, extrude_ip->h ) > 0.0 ) {
		nmg_reverse_face( fu );
		fu = fu->fumate_p;
	}

	/* add the rest of the loops */
	for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		int fdir;
		vect_t cross;
		fastf_t pt_count=0.0;
		fastf_t dot;
		int rev=0;

		aloop = (struct bu_ptbl *)BU_PTBL_GET( &loops, i );
		if( aloop == outer_loop )
			continue;

		if( BU_PTBL_END( containing_loops[i] ) % 2 ) {
			fdir = OT_OPPOSITE;
		} else {
			fdir = OT_SAME;
		}

		for( j=0 ; j<BU_PTBL_END( aloop ) ; j++ ) {
			genptr_t seg;

			seg = (genptr_t)BU_PTBL_GET( aloop, j );
			if( seg_to_vlist( &vhead, ttol, extrude_ip->V,
					  extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, seg ) )
				goto failed;
		}

		/* calculate plane of this loop */
		VSETALLN( pl, 0.0, 4 );
		for( BU_LIST_FOR( vlp, bn_vlist, &vhead ) ) {
			for( j=1 ; j<vlp->nused ; j++ ) {
				if( vlp->cmd[j] == BN_VLIST_LINE_DRAW ) {
					VCROSS( cross, vlp->pt[j-1], vlp->pt[j] );
					VADD2( pl, pl, cross );
				}
			}
		}

		VUNITIZE( pl );

		for( BU_LIST_FOR( vlp, bn_vlist, &vhead ) ) {
			for( j=1 ; j<vlp->nused ; j++ ) {
				if( vlp->cmd[j] == BN_VLIST_LINE_DRAW ) {
					pl[3] += VDOT( pl, vlp->pt[j] );
					pt_count++;
				}
			}
		}
		pl[3] /= pt_count;

		dot = -VDOT( pl, extrude_ip->h );
		rev = 0;
		if( fdir == OT_SAME && dot < 0.0 )
			rev = 1;
		else if( fdir == OT_OPPOSITE && dot > 0.0 )
			rev = 1;

		vertsa = (struct vertex **)bu_calloc((int)pt_count, sizeof( struct vertex *), "verts" );

		fu = nmg_add_loop_to_face( s, fu, vertsa, (int)pt_count, fdir );

		k = 0;
		for( BU_LIST_FOR( vlp, bn_vlist, &vhead ) ) {
			for( j=1 ; j<vlp->nused ; j++ ) {
				if( vlp->cmd[j] == BN_VLIST_LINE_DRAW ) {
					if( rev ) {
						nmg_vertex_gv( vertsa[(int)(pt_count) - k - 1], vlp->pt[j] );
					} else {
						nmg_vertex_gv( vertsa[k], vlp->pt[j] );
					}
					k++;
				}
			}
		}
		RT_FREE_VLIST( &vhead );
	}

	/* extrude this face */
	if( nmg_extrude_face( fu, extrude_ip->h, tol ) ) {
		bu_log( "Failed to extrude face sketch\n" );
		return( -1 );
	}

	nmg_region_a( *r, tol );

	return( 0 );

 failed:
	for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ ) {
		bu_ptbl_free( containing_loops[i] );
		bu_free( (char *)containing_loops[i], "containing_loops[i]" );
	}
	bu_free( (char *)containing_loops, "containing_loops" );
	bu_ptbl_free( aloop );
	bu_free( (char *)aloop, "aloop" );
	return( -1 );
#endif
}

/**
 *			R T _ E X T R U D E _ I M P O R T
 *
 *  Import an EXTRUDE from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_extrude_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip, struct resource *resp)
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct rt_db_internal			tmp_ip;
	struct directory			*dp;
	char					*sketch_name;
	union record				*rp;
	char					*ptr;
	point_t					tmp_vec;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_EXTR )  {
		bu_log("rt_extrude_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EXTRUDE;
	ip->idb_meth = &rt_functab[ID_EXTRUDE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_extrude_internal), "rt_extrude_internal");
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

	sketch_name = (char *)rp + sizeof( struct extr_rec );
	if( !dbip )
		extrude_ip->skt = (struct rt_sketch_internal *)NULL;
	else if( (dp=db_lookup( dbip, sketch_name, LOOKUP_NOISY)) == DIR_NULL )
	{
		bu_log( "rt_extrude_import: ERROR: Cannot find sketch (%.16s) for extrusion (%.16s)\n",
			sketch_name, rp->extr.ex_name );
		extrude_ip->skt = (struct rt_sketch_internal *)NULL;
	}
	else
	{
		if( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity, resp ) != ID_SKETCH )
		{
			bu_log( "rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion (%.16s)\n",
				sketch_name, rp->extr.ex_name );
			bu_free( ip->idb_ptr, "extrusion" );
			return( -1 );
		}
		else
			extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}

	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_V, ELEMENTS_PER_VECT );
	MAT4X3PNT( extrude_ip->V, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_h, ELEMENTS_PER_VECT );
	MAT4X3VEC( extrude_ip->h, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_uvec, ELEMENTS_PER_VECT );
	MAT4X3VEC( extrude_ip->u_vec, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_vvec, ELEMENTS_PER_VECT );
	MAT4X3VEC( extrude_ip->v_vec, mat, tmp_vec );
	extrude_ip->keypoint = bu_glong( rp->extr.ex_key );

	ptr = (char *)rp;
	ptr += sizeof( struct extr_rec );
	extrude_ip->sketch_name = (char *)bu_calloc( 17, sizeof( char ), "Extrude sketch name" );
	strncpy( extrude_ip->sketch_name, ptr, 16 );

	return(0);			/* OK */
}

/**
 *			R T _ E X T R U D E _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_extrude_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_extrude_internal	*extrude_ip;
	vect_t				tmp_vec;
	union record			*rec;
	unsigned char			*ptr;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EXTRUDE )  return(-1);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = 2*sizeof( union record );
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "extrusion external");
	rec = (union record *)ep->ext_buf;

	rec->extr.ex_id = DBID_EXTR;

	VSCALE( tmp_vec, extrude_ip->V, local2mm );
	htond( rec->extr.ex_V, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT );
	VSCALE( tmp_vec, extrude_ip->h, local2mm );
	htond( rec->extr.ex_h, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT );
	VSCALE( tmp_vec, extrude_ip->u_vec, local2mm );
	htond( rec->extr.ex_uvec, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT );
	VSCALE( tmp_vec, extrude_ip->v_vec, local2mm );
	htond( rec->extr.ex_vvec, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT );
	bu_plong( rec->extr.ex_key, extrude_ip->keypoint );
	bu_plong( rec->extr.ex_count, 1 );

	ptr = (unsigned char *)rec;
	ptr += sizeof( struct extr_rec );

	strcpy( (char *)ptr, extrude_ip->sketch_name );

	return(0);
}


/**
 *			R T _ E X T R U D E _ E X P O R T 5
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_extrude_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_extrude_internal	*extrude_ip;
	vect_t				tmp_vec[4];
	unsigned char			*ptr;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EXTRUDE )  return(-1);

	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = 4 * ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG + strlen( extrude_ip->sketch_name ) + 1;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "extrusion external");
	ptr = (unsigned char *)ep->ext_buf;

	VSCALE( tmp_vec[0], extrude_ip->V, local2mm );
	VSCALE( tmp_vec[1], extrude_ip->h, local2mm );
	VSCALE( tmp_vec[2], extrude_ip->u_vec, local2mm );
	VSCALE( tmp_vec[3], extrude_ip->v_vec, local2mm );
	htond( ptr, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT*4 );
	ptr += ELEMENTS_PER_VECT * 4 * SIZEOF_NETWORK_DOUBLE;
	bu_plong( ptr, extrude_ip->keypoint );
	ptr += SIZEOF_NETWORK_LONG;
	strcpy( (char *)ptr, extrude_ip->sketch_name );

	return(0);
}


/**
 *			R T _ E X T R U D E _ I M P O R T 5
 *
 *  Import an EXTRUDE from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_extrude_import5(
	struct rt_db_internal		*ip,
	const struct bu_external	*ep,
	register const mat_t		mat,
	const struct db_i		*dbip,
	struct resource			*resp,
	const int			minor_type )
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct rt_db_internal			tmp_ip;
	struct directory			*dp;
	char					*sketch_name;
	unsigned char				*ptr;
	point_t					tmp_vec[4];

	BU_CK_EXTERNAL( ep );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EXTRUDE;
	ip->idb_meth = &rt_functab[ID_EXTRUDE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_extrude_internal), "rt_extrude_internal");
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

	ptr = (unsigned char *)ep->ext_buf;
	sketch_name = (char *)ptr + ELEMENTS_PER_VECT*4*SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG;
	if( !dbip )
		extrude_ip->skt = (struct rt_sketch_internal *)NULL;
	else if( (dp=db_lookup( dbip, sketch_name, LOOKUP_NOISY)) == DIR_NULL )
	{
		bu_log( "rt_extrude_import: ERROR: Cannot find sketch (%s) for extrusion\n",
			sketch_name );
		extrude_ip->skt = (struct rt_sketch_internal *)NULL;
	}
	else
	{
		if( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity, resp ) != ID_SKETCH )
		{
			bu_log( "rt_extrude_import: ERROR: Cannot import sketch (%s) for extrusion\n",
				sketch_name );
			bu_free( ip->idb_ptr, "extrusion" );
			return( -1 );
		}
		else
			extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}

	ntohd( (unsigned char *)tmp_vec, ptr, ELEMENTS_PER_VECT*4 );
	MAT4X3PNT( extrude_ip->V, mat, tmp_vec[0] );
	MAT4X3VEC( extrude_ip->h, mat, tmp_vec[1] );
	MAT4X3VEC( extrude_ip->u_vec, mat, tmp_vec[2] );
	MAT4X3VEC( extrude_ip->v_vec, mat, tmp_vec[3] );
	ptr += ELEMENTS_PER_VECT * 4 * SIZEOF_NETWORK_DOUBLE;
	extrude_ip->keypoint = bu_glong( ptr );
	ptr += SIZEOF_NETWORK_LONG;
	extrude_ip->sketch_name = bu_strdup( (const char *)ptr );

	return(0);			/* OK */
}

/*8
 *			R T _ E X T R U D E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_extrude_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_extrude_internal	*extrude_ip =
		(struct rt_extrude_internal *)ip->idb_ptr;
	char	buf[256];
	point_t V;
	vect_t h, u, v;

	RT_EXTRUDE_CK_MAGIC(extrude_ip);
	bu_vls_strcat( str, "2D extrude (EXTRUDE)\n");
	VSCALE( V, extrude_ip->V, mm2local );
	VSCALE( h, extrude_ip->h, mm2local );
	VSCALE( u, extrude_ip->u_vec, mm2local );
	VSCALE( v, extrude_ip->v_vec, mm2local );
	sprintf( buf, "\tV = (%g %g %g)\n\tH = (%g %g %g)\n\tu_dir = (%g %g %g)\n\tv_dir = (%g %g %g)\n",
		V3INTCLAMPARGS( V ),
		V3INTCLAMPARGS( h ),
		V3INTCLAMPARGS( u ),
		V3INTCLAMPARGS( v ) );
	bu_vls_strcat( str, buf );
	sprintf( buf, "\tsketch name: %s\n",
		extrude_ip->sketch_name );
	bu_vls_strcat( str, buf );


	return(0);
}

/**
 *			R T _ E X T R U D E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_extrude_ifree(struct rt_db_internal *ip)
{
	register struct rt_extrude_internal	*extrude_ip;
	struct rt_db_internal			tmp_ip;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);
	if( extrude_ip->skt )
	{
		RT_INIT_DB_INTERNAL( &tmp_ip );
		tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		tmp_ip.idb_type = ID_SKETCH;
		tmp_ip.idb_ptr = (genptr_t)extrude_ip->skt;
		tmp_ip.idb_meth = &rt_functab[ID_SKETCH];
		rt_sketch_ifree( &tmp_ip );
	}
	extrude_ip->magic = 0;			/* sanity */

	bu_free( extrude_ip->sketch_name, "Extrude sketch_name" );
	bu_free( (char *)extrude_ip, "extrude ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_extrude_xform(
	struct rt_db_internal *op,
	const mat_t mat,
	struct rt_db_internal *ip,
	int free,
	struct db_i *dbip,
	struct resource *resp)
{
	struct rt_extrude_internal	*eip, *eop;
	point_t tmp_vec;

	RT_CK_DB_INTERNAL( ip );
	RT_CK_RESOURCE(resp)
	eip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC( eip );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of extrude_xform():\n" );
		bu_mem_barriercheck();
	}

	if( op != ip )
	{
		RT_INIT_DB_INTERNAL( op );
		eop = (struct rt_extrude_internal *)bu_malloc( sizeof( struct rt_extrude_internal ), "eop" );
		eop->magic = RT_EXTRUDE_INTERNAL_MAGIC;
		eop->sketch_name = bu_strdup( eip->sketch_name );
		op->idb_ptr = (genptr_t)eop;
		op->idb_meth = &rt_functab[ID_EXTRUDE];
		op->idb_major_type = DB5_MAJORTYPE_BRLCAD;
		op->idb_type = ID_EXTRUDE;
		if( ip->idb_avs.magic == BU_AVS_MAGIC ) {
			bu_avs_init( &op->idb_avs, ip->idb_avs.count, "avs" );
			bu_avs_merge( &op->idb_avs, &ip->idb_avs );
		}
	}
	else
		eop = (struct rt_extrude_internal *)ip->idb_ptr;

	MAT4X3PNT( tmp_vec, mat, eip->V );
	VMOVE( eop->V, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->h );
	VMOVE( eop->h, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->u_vec );
	VMOVE( eop->u_vec, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->v_vec );
	VMOVE( eop->v_vec, tmp_vec );
	eop->keypoint = eip->keypoint;

	if( free && ip != op )
	{
		eop->skt = eip->skt;
		eip->skt = (struct rt_sketch_internal *)NULL;
		rt_db_free_internal( ip, resp );
	}
	else if( eip->skt )
		eop->skt = rt_copy_sketch( eip->skt );
	else
		eop->skt = (struct rt_sketch_internal *)NULL;

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of extrude_xform():\n" );
		bu_mem_barriercheck();
	}

	return( 0 );
}

int
rt_extrude_tclform( const struct rt_functab *ftp, Tcl_Interp *interp )
{
        RT_CK_FUNCTAB(ftp);

        Tcl_AppendResult( interp,
			  "V {%f %f %f} H {%f %f %f} A {%f %f %f} B {%f %f %f} S %s K %d", (char *)NULL );

        return TCL_OK;

}

int
rt_extrude_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	register struct rt_extrude_internal *extr=(struct rt_extrude_internal *) intern->idb_ptr;
        Tcl_DString     ds;
        struct bu_vls   vls;
	int ret=TCL_OK;

	RT_EXTRUDE_CK_MAGIC( extr );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );


	if( attr == (char *)NULL )
	{
		bu_vls_strcpy( &vls, "extrude" );
		bu_vls_printf( &vls, " V {%.25g %.25g %.25g}", V3ARGS( extr->V ) );
		bu_vls_printf( &vls, " H {%.25g %.25g %.25g}", V3ARGS( extr->h ) );
		bu_vls_printf( &vls, " A {%.25g %.25g %.25g}", V3ARGS( extr->u_vec ) );
		bu_vls_printf( &vls, " B {%.25g %.25g %.25g}", V3ARGS( extr->v_vec ) );
		bu_vls_printf( &vls, " S %s", extr->sketch_name );
		bu_vls_printf( &vls, " K %d", extr->keypoint );
	}
	else if( *attr == 'V' )
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( extr->V ) );
	else if( *attr == 'H' )
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( extr->h ) );
	else if( *attr == 'A' )
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( extr->u_vec ) );
	else if( *attr == 'B' )
		bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( extr->v_vec ) );
	else if( *attr == 'S' )
		bu_vls_printf( &vls, "%s", extr->sketch_name );
	else if( *attr == 'K' )
		bu_vls_printf( &vls, "%d", extr->keypoint );
	else
	{
		bu_vls_strcat( &vls, "ERROR: unrecognized attribute, must be V, H, A, B, S, or K!!!" );
		ret = TCL_ERROR;
	}

        Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
        Tcl_DStringResult( interp, &ds );
        Tcl_DStringFree( &ds );
        bu_vls_free( &vls );
        return( ret );
}

int
rt_extrude_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
        struct rt_extrude_internal *extr;
        fastf_t *new;
	fastf_t len;

        RT_CK_DB_INTERNAL( intern );
        extr = (struct rt_extrude_internal *)intern->idb_ptr;
        RT_EXTRUDE_CK_MAGIC( extr );

	while( argc >= 2 )
	{
		int array_len=3;

		if( *argv[0] == 'V' )
		{
			new = extr->V;
			if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) !=
			    array_len ) {
				Tcl_SetResult( interp,
				      "ERROR: incorrect number of coordinates for vertex\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}
		}
		else if( *argv[0] == 'H' )
		{
			new = extr->h;
			if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) !=
			    array_len ) {
				Tcl_SetResult( interp,
				      "ERROR: incorrect number of coordinates for vector\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}
		}
		else if( *argv[0] == 'A' )
		{
			new = extr->u_vec;
			if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) !=
			    array_len ) {
				Tcl_SetResult( interp,
				      "ERROR: incorrect number of coordinates for vector\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}

			/* insure that u_vec and v_vec are the same length */
			len = MAGNITUDE( extr->u_vec );
			VUNITIZE( extr->v_vec );
			VSCALE( extr->v_vec, extr->v_vec, len );
		}
		else if( *argv[0] == 'B' )
		{
			new = extr->v_vec;
			if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) !=
			    array_len ) {
				Tcl_SetResult( interp,
				      "ERROR: incorrect number of coordinates for vector\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}
			/* insure that u_vec and v_vec are the same length */
			len = MAGNITUDE( extr->v_vec );
			VUNITIZE( extr->u_vec );
			VSCALE( extr->u_vec, extr->u_vec, len );
		}
		else if( *argv[0] =='K' )
			extr->keypoint = atoi( argv[1] );
		else if( *argv[0] == 'S' ) {
			if( extr->sketch_name )
				bu_free( (char *)extr->sketch_name, "rt_extrude_tcladjust: sketch_name" );
			extr->sketch_name = bu_strdup( argv[1] );
		}

		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
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
