/*
 *			G _ E X T R U D E . C
 *
 *  Purpose -
 *	Provide support for solids of extrusion
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSextrude[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

/* From g_sketch.c */
BU_EXTERN( struct rt_sketch_internal *rt_copy_sketch, (CONST struct rt_sketch_internal *sketch_ip ) );

struct extrude_specific {
	mat_t rot, irot;	/* rotation and translation to get extrsuion vector in +z direction with V at origin */
	vect_t unit_h;		/* unit vector in direction of extrusion vector */
	vect_t u_vec;		/* u vector rotated and projected */
	vect_t v_vec;		/* v vector rotated and projected */
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

/*
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
rt_extrude_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_extrude_internal *eip;
	register struct extrude_specific *extr;
	struct rt_sketch_internal *skt;
	LOCAL vect_t tmp, tmp2;
	fastf_t tmp_f;
	int i, curve_no;
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

	/* build a transformation matrix to rotate extrusion vector to z-axis */
	VSET( tmp, 0, 0, 1 )
	bn_mat_fromto( extr->rot, eip->h, tmp );

	/* and translate to origin */
	extr->rot[MDX] = -VDOT( eip->V, &extr->rot[0] );
	extr->rot[MDY] = -VDOT( eip->V, &extr->rot[4] );
	extr->rot[MDZ] = -VDOT( eip->V, &extr->rot[8] );

	/* and save the inverse */
	bn_mat_inv( extr->irot, extr->rot );

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
		VCROSS( tmp2, tmp, extr->rot_axis );
		MAT4X3VEC( extr->perp, extr->rot, tmp2 );
		VUNITIZE( extr->perp );
	}

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

	/* apply the rotation matrix  to all the vertices */
	if( vert_count )
		extr->verts = (point_t *)bu_calloc( vert_count, sizeof( point_t ), "extr->verts" );
	VSETALL( stp->st_min, MAX_FASTF );
	VSETALL( stp->st_max, -MAX_FASTF );
	for( i=0 ; i<skt->vert_count ; i++ )
	{
		VJOIN2( tmp, eip->V, skt->verts[i][0], eip->u_vec, skt->verts[i][1], eip->v_vec );
		VMINMAX( stp->st_min, stp->st_max, tmp );
		MAT4X3PNT( extr->verts[i], extr->rot, tmp );
	}
	curr_vert = skt->vert_count;

	/* and the u,v vectors */
	MAT4X3VEC( extr->u_vec, extr->rot, eip->u_vec );
	MAT4X3VEC( extr->v_vec, extr->rot, eip->v_vec );

	/* calculate the rotated pl1 */
	VCROSS( extr->pl1_rot, extr->u_vec, extr->v_vec );
	VUNITIZE( extr->pl1_rot );
	extr->pl1_rot[3] = VDOT( extr->pl1_rot, extr->verts[0] );

	/* copy the curve */
	rt_copy_curve( &extr->crv, &skt->skt_curve );

	/* if any part of the curve is a circular arc, the arc may extend beyond the listed vertices */
	for( i=0 ; i<skt->skt_curve.seg_count ; i++ )
	{
		struct carc_seg *csg=(struct carc_seg *)skt->skt_curve.segments[i];
		struct carc_seg *csg_extr=(struct carc_seg *)extr->crv.segments[i];
		point_t center;

		if( csg->magic != CURVE_CARC_MAGIC )
			continue;

		if( csg->radius <= 0.0 )
		{
			point_t start;
			fastf_t radius;

			csg_extr->center = csg->end;
			VJOIN2( start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec );
			VJOIN2( center, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec );
			VSUB2( tmp, start, center );
			radius = MAGNITUDE( tmp );
			csg_extr->radius = -radius;	/* need the correct magnitude for normal calculation */
			VJOIN1( tmp, center, radius, eip->u_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, -radius, eip->u_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, radius, eip->v_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, -radius, eip->v_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
		}
		else
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
			if( magsq_s2m >= csg->radius*csg->radius )
			{
				bu_log( "Impossible radius for circular arc in extrusion (%s)!!!\n", 
						stp->st_dp->d_namep );
				return( -1 );
			}
			dist = sqrt( csg->radius*csg->radius - magsq_s2m );

			/* save arc center */
			if( csg->center_is_left )
				VJOIN1( center, mid, dist, bisector )
			else
				VJOIN1( center, mid, -dist, bisector )
			MAT4X3PNT( extr->verts[curr_vert], extr->rot, center );
			csg_extr->center = curr_vert;
			curr_vert++;

			VJOIN1( tmp, center, csg->radius, eip->u_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, -csg->radius, eip->u_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, csg->radius, eip->v_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
			VJOIN1( tmp, center, -csg->radius, eip->v_vec );
			VMINMAX( stp->st_min, stp->st_max, tmp );
		}
	}

	VADD2( tmp, stp->st_min, eip->h );
	VADD2( tmp2, stp->st_max, eip->h );
	VMINMAX( stp->st_min, stp->st_max, tmp );
	VMINMAX( stp->st_min, stp->st_max, tmp2 );
	VBLEND2( stp->st_center, 0.5, stp->st_min, 0.5, stp->st_max );
	VSUB2( tmp, stp->st_max, stp->st_min );
	stp->st_aradius = 0.5 * MAGNITUDE( tmp );
	stp->st_bradius = stp->st_aradius;

	return(0);              /* OK */
}

/*
 *			R T _ E X T R U D E _ P R I N T
 */
void
rt_extrude_print( stp )
register CONST struct soltab *stp;
{
}

int
get_quadrant( v, local_x, local_y, vx, vy )
vect_t local_x, local_y;
point2d_t v;
fastf_t *vx, *vy;
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
isect_line_earc( dist, ray_start, ray_dir, center, ra, rb, norm, start, end, orientation )
fastf_t dist[2];
point_t ray_start;
vect_t ray_dir;
point_t center, start, end;
vect_t ra, rb, norm;
int orientation;	/* 0 -> ccw, !0 -> cw */
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

int
isect_line2_ellipse( dist, ray_start, ray_dir, center, ra, rb )
fastf_t dist[2];
point_t ray_start, center;
vect_t ray_dir, ra, rb;
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
		bu_bomb( "ERROR: isect_line2_ellipse: semi-axis length is too small!!!\n" );

	dda = V2DOT( ray_dir, ra );
	ddb = V2DOT( ray_dir, rb );

	a = dda*dda/ra_4 + ddb*ddb/rb_4;
	b = 2.0 * (pmcda*dda/ra_4 + pmcdb*ddb/rb_4);
	c = pmcda*pmcda/ra_4 + pmcdb*pmcdb/rb_4 - 1.0;

	disc = b*b - 4.0*a*c;

	if( disc < 0.0 )
		return( 0 );

	if( disc == 0.0 )
	{
		dist[0] = -b/(2.0*a);
		return( 1 );
	}

	dist[0] = (-b - sqrt( disc )) / (2.0*a);
	dist[1] = (-b + sqrt( disc )) / (2.0*a);
	return( 2 );
}

/*
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
rt_extrude_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
	register int i, j, k;
	fastf_t dist_top, dist_bottom, to_bottom;
	fastf_t dist[2];
	fastf_t dot_pl1, dir_dot_z;
	point_t tmp, tmp2;
	point_t ray_start, ray_dir;	/* 2D */
	struct curve *crv;
	struct hit hits[MAX_HITS];
	fastf_t dists_before[MAX_HITS];
	fastf_t dists_after[MAX_HITS];
	fastf_t *dists;
	int dist_count;
	int hit_count=0;
	int hits_before_bottom=0, hits_after_top=0;
	int code;
	int check_inout=0;
	int top_face=TOP_FACE, bot_face=BOTTOM_FACE;
	int surfno;
	int free_dists=0;

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
		struct carc_seg *csg;
		fastf_t diff;

		if( free_dists )
			bu_free( (char *)dists, "dists" );

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
				free_dists = 0;
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

						/* build the ellipse */
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
				free_dists = 0;
				surfno = CARC_SEG;
				break;
			case CURVE_NURB_MAGIC:
				break;
			default:
				bu_log( "Unrecognized segment type in sketch (%s) referenced by extrusion (%s)\n",
					stp->st_dp->d_namep );
				bu_bomb( "Unrecognized segment type in sketch\n" );
				break;
		}

		for( j=0 ; j<hit_count ; j++ )
		{
			k = 0;
			while( k < dist_count )
			{
				diff = dists[k] - hits[j].hit_dist;
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
		for( j=0 ; j<hits_before_bottom ; j++ )
		{
			k = 0;
			while( k < dist_count )
			{
				diff = dists[k] - dists_before[j];
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


		if( check_inout )
		{
			for( j=0 ; j<dist_count ; j++ )
			{
				if( dists[j] < 0.0 )
					hit_count++;
			}
			continue;
		}

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
				default:
					bu_log( "ERROR: rt_extrude_shot: unrecognized segment type in solid %s\n",
						stp->st_dp->d_namep );
					bu_bomb( "ERROR: rt_extrude_shot: unrecognized segment type in solid\n" );
					break;
			}
			hit_count++;
		}
	}

	if( free_dists )
		bu_free( (char *)dists, "dists" );

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
		bu_log( "ERROR: rt_extrude_shot(): odd number of hits (%d) (ignoring last hit)\n", hit_count );
		bu_log( "ray start = (%20.10f %20.10f %20.10f)\n", V3ARGS( rp->r_pt ) );
		bu_log( "ray dir = (%20.10f %20.10f %20.10f)", V3ARGS( rp->r_dir ) );
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

/*
 *			R T _ E X T R U D E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_extrude_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ E X T R U D E _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_extrude_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
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

/*
 *			R T _ E X T R U D E _ C U R V E
 *
 *  Return the curvature of the extrude.
 */
void
rt_extrude_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
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

/*
 *  			R T _ E X T R U D E _ U V
 *  
 *  For a hit on the surface of an extrude, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_extrude_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
}

/*
 *		R T _ E X T R U D E _ F R E E
 */
void
rt_extrude_free( stp )
register struct soltab *stp;
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;

	if( extrude->verts )
		bu_free( (char *)extrude->verts, "extrude->verts" );
	rt_curve_free( extrude->crv );
	bu_free( (char *)extrude, "extrude_specific" );
}

/*
 *			R T _ E X T R U D E _ C L A S S
 */
int
rt_extrude_class()
{
	return(0);
}

/*
 *			R T _ E X T R U D E _ P L O T
 */
int
rt_extrude_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct curve			*crv=(struct curve *)NULL;
	int				curve_no;
	struct rt_sketch_internal	*sketch_ip;
	point_t				end_of_h;
	int				i1, i2, nused1, nused2;
	struct bn_vlist			*vp1, *vp2, *vp2_start;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	if( !extrude_ip->skt )
	{
		bu_log( "rt_extrude_plot: ERROR: no sketch for extrusion!!!!\n" );

		RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, extrude_ip->V, BN_VLIST_LINE_DRAW );
		return( 0 );
	}

	sketch_ip = extrude_ip->skt;
	RT_SKETCH_CK_MAGIC( sketch_ip );

	crv = &sketch_ip->skt_curve;

	/* plot bottom curve */
	vp1 = BU_LIST_LAST( bn_vlist, vhead );
	nused1 = vp1->nused;
	if( curve_to_vlist( vhead, ttol, extrude_ip->V, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, crv ) )
	{
		bu_log( "Error: sketch (%s) references non-existent vertices!!!\n",
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
	if( i1 >= vp1->nused )
	{
		i1 = 0;
		vp1 = BU_LIST_NEXT( bn_vlist, &vp1->l );
	}
	i2 = nused2;
	if( i2 >= vp2->nused )
	{
		i2 = 0;
		vp2 = BU_LIST_NEXT( bn_vlist, &vp2->l );
		nused2--;
	}
	while( vp1 != vp2_start || i1 != nused2 )
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

/*
 *			R T _ E X T R U D E _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_extrude_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	return(-1);
}

/*
 *			R T _ E X T R U D E _ I M P O R T
 *
 *  Import an EXTRUDE from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_extrude_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
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

	RT_INIT_DB_INTERNAL( ip );
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
		if( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity ) != ID_SKETCH )
		{
			bu_log( "rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion (%.16s)\n",
				sketch_name, rp->extr.ex_name );
			bu_free( ip->idb_ptr, "extrusion" );
			return( -1 );
		}
		else
			extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}

	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_V, 3 );
	MAT4X3PNT( extrude_ip->V, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_h, 3 );
	MAT4X3VEC( extrude_ip->h, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_uvec, 3 );
	MAT4X3VEC( extrude_ip->u_vec, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_vvec, 3 );
	MAT4X3VEC( extrude_ip->v_vec, mat, tmp_vec );
	extrude_ip->keypoint = bu_glong( rp->extr.ex_key );

	ptr = (char *)rp;
	ptr += sizeof( struct extr_rec );
	strncpy( extrude_ip->sketch_name, ptr, 16 );

	return(0);			/* OK */
}

/*
 *			R T _ E X T R U D E _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_extrude_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_extrude_internal	*extrude_ip;
	vect_t				tmp_vec;
	union record			*rec;
	unsigned char			*ptr;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EXTRUDE )  return(-1);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = 2*sizeof( union record );
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "extrusion external");
	rec = (union record *)ep->ext_buf;

	rec->extr.ex_id = DBID_EXTR;

	VSCALE( tmp_vec, extrude_ip->V, local2mm );
	htond( rec->extr.ex_V, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->h, local2mm );
	htond( rec->extr.ex_h, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->u_vec, local2mm );
	htond( rec->extr.ex_uvec, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->v_vec, local2mm );
	htond( rec->extr.ex_vvec, (unsigned char *)tmp_vec, 3 );
	bu_plong( rec->extr.ex_key, extrude_ip->keypoint );
	bu_plong( rec->extr.ex_count, 1 );

	ptr = (unsigned char *)rec;
	ptr += sizeof( struct extr_rec );

	strncpy( (char *)ptr, extrude_ip->sketch_name, 16 );

	return(0);
}

/*
 *			R T _ E X T R U D E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_extrude_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
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
		V3ARGS( V ),
		V3ARGS( h ),
		V3ARGS( u ),
		V3ARGS( v ) );
	bu_vls_strcat( str, buf );
	sprintf( buf, "\tsketch name: %.16s\n",
		extrude_ip->sketch_name );
	bu_vls_strcat( str, buf );
	

	return(0);
}

/*
 *			R T _ E X T R U D E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_extrude_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_extrude_internal	*extrude_ip;
	struct rt_db_internal			tmp_ip;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);
	if( extrude_ip->skt )
	{
		RT_INIT_DB_INTERNAL( &tmp_ip );
		tmp_ip.idb_type = ID_SKETCH;
		tmp_ip.idb_ptr = (genptr_t)extrude_ip->skt;
		tmp_ip.idb_meth = &rt_functab[ID_SKETCH];
		rt_sketch_ifree( &tmp_ip );
	}
	extrude_ip->magic = 0;			/* sanity */

	bu_free( (char *)extrude_ip, "extrude ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_extrude_xform( op, mat, ip, free, dbip )
struct rt_db_internal *op;
CONST mat_t mat;
struct rt_db_internal *ip;
int free;
struct db_i *dbip;
{
	struct rt_extrude_internal	*eip, *eop;
	point_t tmp_vec;

	RT_CK_DB_INTERNAL( ip );
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
		op->idb_ptr = (genptr_t)eop;
		op->idb_meth = &rt_functab[ID_EXTRUDE];
		op->idb_type = ID_EXTRUDE;
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
	strncpy( eop->sketch_name, eip->sketch_name, 16 );

	if( free && ip != op )
	{
		eop->skt = eip->skt;
		eip->skt = (struct rt_sketch_internal *)NULL;
		rt_functab[ip->idb_type].ft_ifree( ip );
		ip->idb_ptr = (genptr_t) 0;
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
rt_extrude_tclget( interp, intern, attr )
Tcl_Interp                      *interp;
CONST struct rt_db_internal     *intern;
CONST char                      *attr;
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
rt_extrude_tcladjust( interp, intern, argc, argv )
Tcl_Interp              *interp;
struct rt_db_internal   *intern;
int                     argc;
char                    **argv;
{
        struct rt_extrude_internal *extr;
        int ret;
        fastf_t *new;

        RT_CK_DB_INTERNAL( intern );
        extr = (struct rt_extrude_internal *)intern->idb_ptr;
        RT_EXTRUDE_CK_MAGIC( extr );

	while( argc >= 2 )
	{
		int array_len=3;

		if( *argv[0] == 'V' )
		{
			new = extr->V;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] == 'H' )
		{
			new = extr->h;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] == 'A' )
		{
			new = extr->u_vec;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] == 'B' )
		{
			new = extr->v_vec;
			if( (ret=tcl_list_to_fastf_array( interp, argv[1], &new, &array_len ) ) )
				return( ret );
		}
		else if( *argv[0] =='K' )
			extr->keypoint = atoi( argv[1] );
		else if( *argv[0] == 'S' )
			NAMEMOVE( argv[1], extr->sketch_name );

		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
}
