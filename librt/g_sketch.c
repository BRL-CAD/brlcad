/*
 *			G _ S K E T C H . C
 *
 *  Purpose -
 *	Provide support for 2D sketches
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_sketch_internal --- parameters for solid
 *	define sketch_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_sketch_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_SKETCH, increment ID_MAXIMUM
 *	edit db_scan.c to add support for new solid type
 *	edit Cakefile to add g_sketch.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_sketch() routine
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  
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
static char RCSsketch[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "./debug.h"

fastf_t rt_cnurb_par_edge();

/*
 *  			R T _ S K E T C H _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid SKETCH, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	SKETCH is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct sketch_specific is created, and it's address is stored in
 *  	stp->st_specific for use by sketch_shot().
 */
int
rt_sketch_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	stp->st_specific = (genptr_t)NULL;
	return( 0 );
}

/*
 *			R T _ S K E T C H _ P R I N T
 */
void
rt_sketch_print( stp )
register CONST struct soltab *stp;
{
}

/*
 *  			R T _ S K E T C H _ S H O T
 *  
 *  Intersect a ray with a sketch.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_sketch_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	return(0);			/* MISS */
}

#define RT_SKETCH_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ S K E T C H _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_sketch_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ S K E T C H _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_sketch_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ S K E T C H _ C U R V E
 *
 *  Return the curvature of the sketch.
 */
void
rt_sketch_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ S K E T C H _ U V
 *  
 *  For a hit on the surface of an sketch, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_sketch_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
}

/*
 *		R T _ S K E T C H _ F R E E
 */
void
rt_sketch_free( stp )
register struct soltab *stp;
{
	register struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;

	bu_free( (char *)sketch, "sketch_specific" );
}

/*
 *			R T _ S K E T C H _ C L A S S
 */
int
rt_sketch_class()
{
	return(0);
}

/*
 *			C U R V E _ T O _ V L I S T
 */
void
curve_to_vlist( vhead, ttol, V, u_vec, v_vec, sketch_ip, crv )
struct bu_list          *vhead;
CONST struct rt_tess_tol *ttol;
point_t			V;
vect_t			u_vec, v_vec;
struct rt_sketch_internal *sketch_ip;
struct curve                    *crv;
{
	long *lng;
	int seg_no, i;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	fastf_t delta;
	point_t center, start_pt, end_pt;
	fastf_t pt[4];
	vect_t semi_a, semi_b;
	fastf_t radius;

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of curve_to_vlist():\n" );
		bu_mem_barriercheck();
	}

	for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
	{
		lng = (long *)crv->segments[seg_no];
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				VJOIN2( pt, V, sketch_ip->verts[lsg->start][0], u_vec, sketch_ip->verts[lsg->start][1], v_vec);
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
				VJOIN2( pt, V, sketch_ip->verts[lsg->end][0], u_vec, sketch_ip->verts[lsg->end][1], v_vec);
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				break;
			case CURVE_CARC_MAGIC:
			{
				point2d_t mid_pt, start2d, end2d, center2d, s2m, dir;
				fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;
				fastf_t start_ang, end_ang, tot_ang, cosdel, sindel;
				fastf_t oldu, oldv, newu, newv;
				int nsegs;

				delta = M_PI/4.0;
				csg = (struct carc_seg *)lng;
				if( csg->radius < 0.0 )
				{
					VJOIN2( center, V, sketch_ip->verts[csg->end][0], u_vec, sketch_ip->verts[csg->end][1], v_vec);
					VJOIN2( pt, V, sketch_ip->verts[csg->start][0], u_vec, sketch_ip->verts[csg->start][1], v_vec);
					VSUB2( semi_a, pt, center );
					VJOIN2( pt, V, -sketch_ip->verts[csg->start][1], u_vec, sketch_ip->verts[csg->start][0], v_vec);
					VSUB2( semi_b, pt, center );
					radius = MAGNITUDE( semi_a );
				}
				else if( csg->radius < SMALL_FASTF )
				{
					bu_log( "Radius too small in curve (%s)!!\n", crv->crv_name );
					break;
				}
				else
					radius = csg->radius;

				if( ttol->abs > 0.0 )
				{
					fastf_t tmp_delta, ratio;

					ratio = ttol->abs / radius;
					if( ratio < 1.0 )
					{
						tmp_delta = 2.0 * acos( 1.0 - ratio);
						if( tmp_delta < delta )
							delta = tmp_delta;
					}
				}
				if( ttol->rel > 0.0 && ttol->rel < 1.0 )
				{
					fastf_t tmp_delta;

					tmp_delta = 2.0 * acos( 1.0 - ttol->rel );
					if( tmp_delta < delta )
						delta = tmp_delta;
				}
				if( ttol->norm > 0.0 )
				{
					fastf_t norm;

					norm = ttol->norm * M_PI / 180.0;
					if( norm < delta )
						delta = norm;
				}
				if( csg->radius < 0.0 )
				{
					/* this is a full circle */
					nsegs = ceil( 2.0 * M_PI / delta );
					delta = 2.0 * M_PI / (double)nsegs;
					cosdel = cos( delta );
					sindel = sin( delta );
					oldu = 1.0;
					oldv = 0.0;
					VJOIN2( start_pt, center, oldu, semi_a, oldv, semi_b );
					RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
					for( i=1 ; i<nsegs ; i++ )
					{
						newu = oldu * cosdel - oldv * sindel;
						newv = oldu * sindel + oldv * cosdel;
						VJOIN2( pt, center, newu, semi_a, newv, semi_b );
						RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
						oldu = newu;
						oldv = newv;
					}
					RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_DRAW );
					break;
				}

				/* this is an arc (not a full circle) */
				V2MOVE( start2d, sketch_ip->verts[csg->start] );
				V2MOVE( end2d, sketch_ip->verts[csg->end] );
				mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
				mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
				V2SUB2( s2m, mid_pt, start2d )
				dir[0] = -s2m[1];
				dir[1] = s2m[0];
				s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
				if( s2m_len_sq < SMALL_FASTF )
				{
					bu_log( "start and end points are too close together in circular arc (segment #%d)\n\
						of curve %s\n", seg_no, crv->crv_name );
					break;
				}
				len_sq = radius*radius - s2m_len_sq;
				if( len_sq < 0.0 )
				{
					bu_log( "Impossible radius for specified start and end points in circular arc\n\
						(segment #%d) of curve %s\n", seg_no, crv->crv_name );
					break;
				}
				tmp_len = sqrt( dir[0]*dir[0] + dir[1]*dir[1] );
				dir[0] = dir[0] / tmp_len;
				dir[1] = dir[1] / tmp_len;
				tmp_len = sqrt( len_sq );
				V2JOIN1( center2d, mid_pt, tmp_len, dir )

				/* check center location */
				cross_z = ( end2d[X] - start2d[X] )*( center2d[Y] - start2d[Y] ) -
					( end2d[Y] - start2d[Y] )*( center2d[X] - start2d[X] );
				if( !(cross_z > 0.0 && csg->center_is_left) )
					V2JOIN1( center2d, mid_pt, -tmp_len, dir );
				start_ang = atan2( start2d[Y]-center2d[Y], start2d[X]-center2d[X] );
				end_ang = atan2( end2d[Y]-center2d[Y], end2d[X]-center2d[X] );
				if( csg->orientation ) /* clock-wise */
				{
					if( end_ang > start_ang )
						end_ang -= 2.0 * M_PI;
					tot_ang = start_ang - end_ang;
				}
				else /* counter-clock-wise */
				{
					if( end_ang < start_ang )
						end_ang += 2.0 * M_PI;
					tot_ang = end_ang - start_ang;
				}
				nsegs = ceil( tot_ang / delta );
				if( nsegs < 3 )
					nsegs = 3;
				delta = tot_ang / nsegs;
				cosdel = cos( delta );
				sindel = sin( delta );
				VJOIN2( center, V, center2d[0], u_vec, center2d[1], v_vec );
				VJOIN2( start_pt, V, start2d[0], u_vec, start2d[1], v_vec );
				VJOIN2( end_pt, V, end2d[0], u_vec, end2d[1], v_vec );
				oldu = (start2d[0] - center2d[0]);
				oldv = (start2d[1] - center2d[1]);
				RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
				for( i=0 ; i<nsegs ; i++ )
				{
					newu = oldu * cosdel - oldv * sindel;
					newv = oldu * sindel + oldv * cosdel;
					VJOIN2( pt, center, newu, u_vec, newv, v_vec );
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
					oldu = newu;
					oldv = newv;
				}
				RT_ADD_VLIST( vhead, end_pt, BN_VLIST_LINE_DRAW );
				break;
			}
			case CURVE_NURB_MAGIC:
			{
				struct edge_g_cnurb eg;
				int coords;
				fastf_t inv_weight;
				int num_intervals;
				fastf_t param_delta, epsilon;

				nsg = (struct nurb_seg *)lng;
				if( nsg->order < 3 )
				{
					/* just straight lines */
					VJOIN2( start_pt, V, sketch_ip->verts[nsg->ctl_points[0]][0], u_vec, sketch_ip->verts[nsg->ctl_points[0]][1], v_vec );
					if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
					{
						inv_weight = 1.0/nsg->weights[0];
						VSCALE( start_pt, start_pt, inv_weight );
					}
					RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
					for( i=1 ; i<nsg->c_size ; i++ )
					{
						VJOIN2( pt, V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
						if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
						{
							inv_weight = 1.0/nsg->weights[i];
							VSCALE( pt, pt, inv_weight );
						}
						RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
					}
					break;
				}
				eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
				eg.order = nsg->order;
				eg.k.k_size = nsg->k.k_size;
				eg.k.knots = nsg->k.knots;
				eg.c_size = nsg->c_size;
				coords = 3 + RT_NURB_IS_PT_RATIONAL( nsg->pt_type );
				eg.pt_type = RT_NURB_MAKE_PT_TYPE( coords, 2, RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) );
				eg.ctl_points = (fastf_t *)bu_malloc( nsg->c_size * coords * sizeof( fastf_t ), "eg.ctl_points" );
				if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
				{
					for( i=0 ; i<nsg->c_size ; i++ )
					{
						VJOIN2( &eg.ctl_points[i*coords], V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
						eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
					}
				}
				else
				{
					for( i=0 ; i<nsg->c_size ; i++ )
						VJOIN2( &eg.ctl_points[i*coords], V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
				}
				epsilon = MAX_FASTF;
				if( ttol->abs > 0.0 && ttol->abs < epsilon )
					epsilon = ttol->abs;
				if( ttol->rel > 0.0 )
				{
					point2d_t min_pt, max_pt, tmp_pt;
					point2d_t diff;
					fastf_t tmp_epsilon;

					min_pt[0] = MAX_FASTF;
					min_pt[1] = MAX_FASTF;
					max_pt[0] = -MAX_FASTF;
					max_pt[1] = -MAX_FASTF;

					for( i=0 ; i<nsg->c_size ; i++ )
					{
						V2MOVE( tmp_pt, sketch_ip->verts[nsg->ctl_points[i]] );
						if( tmp_pt[0] > max_pt[0] )
							max_pt[0] = tmp_pt[0];
						if( tmp_pt[1] > max_pt[1] )
							max_pt[1] = tmp_pt[1];
						if( tmp_pt[0] < min_pt[0] )
							min_pt[0] = tmp_pt[0];
						if( tmp_pt[1] < min_pt[1] )
							min_pt[1] = tmp_pt[1];
					}

					V2SUB2( diff, max_pt, min_pt )
					tmp_epsilon = ttol->rel * sqrt( MAG2SQ( diff ) );
					if( tmp_epsilon < epsilon )
						epsilon = tmp_epsilon;
					
				}
				param_delta = rt_cnurb_par_edge( &eg, epsilon );
				num_intervals = ceil( (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/param_delta );
				if( num_intervals < 3 )
					num_intervals = 3;
				if( num_intervals > 500 )
				{
					bu_log( "num_intervals was %d\n", num_intervals );
					num_intervals = 500;
				}
				param_delta = (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/(double)num_intervals;
				for( i=0 ; i<=num_intervals ; i++ )
				{
					fastf_t t;
					int j;

					t = nsg->k.knots[0] + i*param_delta;
					rt_nurb_c_eval( &eg, t, pt );
					if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
					{
						for( j=0 ; j<coords-1 ; j++ )
							pt[j] /= pt[coords-1];
					}
					if( i == 0 )
						RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
					else
						RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				}
				bu_free( (char *)eg.ctl_points, "eg.ctl_points" );
				break;
			}
			default:
				bu_log( "curve_to_vlist: ERROR: unrecognized segment type!!!!\n" );
				break;
		}
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of curve_to_vlist():\n" );
		bu_mem_barriercheck();
	}
}

/*
 *			R T _ S K E T C H _ P L O T
 */
int
rt_sketch_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol		*tol;
{
	LOCAL struct rt_sketch_internal	*sketch_ip;
	int				curve_no;

	RT_CK_DB_INTERNAL(ip);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		curve_to_vlist( vhead, ttol, sketch_ip->V, sketch_ip->u_vec, sketch_ip->v_vec, sketch_ip, &sketch_ip->curves[curve_no] );
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_sketch_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ S K E T C H _ I M P O R T
 *
 *  Import an SKETCH from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_sketch_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_sketch_internal	*sketch_ip;
	union record			*rp;
	vect_t				v;
	int				seg_count;
	int				seg_no;
	int				curve_no;
	unsigned char			*ptr;
	genptr_t			*segs;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_SKETCH )  {
		bu_log("rt_sketch_import: defective record\n");
		return(-1);
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_import():\n" );
		bu_mem_barriercheck();
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_SKETCH;
	ip->idb_meth = &rt_functab[ID_SKETCH];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_sketch_internal), "rt_sketch_internal");
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;

	ntohd( (unsigned char *)v, rp->skt.skt_V, 3 );
	MAT4X3PNT( sketch_ip->V, mat, v );
	ntohd( (unsigned char *)v, rp->skt.skt_uvec, 3 );
	MAT4X3VEC( sketch_ip->u_vec, mat, v );
	ntohd( (unsigned char *)v, rp->skt.skt_vvec, 3 );
	MAT4X3VEC( sketch_ip->v_vec, mat, v );
	sketch_ip->vert_count = bu_glong( rp->skt.skt_vert_count );
	sketch_ip->curve_count = bu_glong( rp->skt.skt_curve_count );
	seg_count = bu_glong( rp->skt.skt_seg_count );

	ptr = (unsigned char *)rp;
	ptr += sizeof( struct sketch_rec );
	sketch_ip->verts = (point2d_t *)bu_calloc( sketch_ip->vert_count, sizeof( point2d_t ), "sketch_ip->vert" );
	ntohd( (unsigned char *)sketch_ip->verts, ptr, sketch_ip->vert_count*2 );
	ptr += 16 * sketch_ip->vert_count;

	segs = (genptr_t *)bu_calloc( seg_count, sizeof( genptr_t ), "segs" );
	for( seg_no=0 ; seg_no < seg_count ; seg_no++ )
	{
		long magic;
		struct line_seg *lsg;
		struct carc_seg *csg;
		struct nurb_seg *nsg;
		int i;

		magic = bu_glong( ptr );
		ptr += 4;
		switch( magic )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "lsg" );
				lsg->magic = magic;
				lsg->start = bu_glong( ptr );
				ptr += 4;
				lsg->end = bu_glong( ptr );
				ptr += 4;
				segs[seg_no] = (genptr_t)lsg;
				break;
			case CURVE_CARC_MAGIC:
				csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "csg" );
				csg->magic = magic;
				csg->start = bu_glong( ptr );
				ptr += 4;
				csg->end = bu_glong( ptr );
				ptr += 4;
				csg->orientation = bu_glong( ptr );
				ptr += 4;
				csg->center_is_left = bu_glong( ptr );
				ptr += 4;
				ntohd( (unsigned char *)&csg->radius, ptr, 1 );
				ptr += 8;
				segs[seg_no] = (genptr_t)csg;
				break;
			case CURVE_NURB_MAGIC:
				nsg = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nsg" );
				nsg->magic = magic;
				nsg->order = bu_glong( ptr );
				ptr += 4;
				nsg->pt_type = bu_glong( ptr );
				ptr += 4;
				nsg->k.k_size = bu_glong( ptr );
				ptr += 4;
				nsg->k.knots = (fastf_t *)bu_malloc( nsg->k.k_size * sizeof( fastf_t ), "nsg->k.knots" );
				ntohd( (unsigned char *)nsg->k.knots, ptr, nsg->k.k_size );
				ptr += 8 * nsg->k.k_size;
				nsg->c_size = bu_glong( ptr );
				ptr += 4;
				nsg->ctl_points = (int *)bu_malloc( nsg->c_size * sizeof( int ), "nsg->ctl_points" );
				for( i=0 ; i<nsg->c_size ; i++ )
				{
					nsg->ctl_points[i] = bu_glong( ptr );
					ptr += 4;
				}
				if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
				{
					nsg->weights = (fastf_t *)bu_malloc( nsg->c_size * sizeof( fastf_t ), "nsg->weights" );
					ntohd( (unsigned char *)nsg->weights, ptr, nsg->c_size );
					ptr += 8 * nsg->c_size;
				}
				else
					nsg->weights = (fastf_t *)NULL;
				segs[seg_no] = (genptr_t)nsg;
				break;
			default:
				bu_bomb( "rt_sketch_import: ERROR: unrecognized segment type!!!\n" );
				break;
		}
	}

	sketch_ip->curves = (struct curve *)bu_calloc( sketch_ip->curve_count, sizeof( struct curve ), "sketch_ip->curves" );
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;
		int i;

		crv = &sketch_ip->curves[curve_no];
		NAMEMOVE( (char *)ptr, crv->crv_name );
		ptr += NAMESIZE;
		crv->seg_count = bu_glong( ptr );
		ptr += 4;

		crv->reverse = (int *)bu_calloc( crv->seg_count, sizeof(int), "crv->reverse" );
		crv->segments = (genptr_t *)bu_calloc( crv->seg_count, sizeof(genptr_t), "crv->segemnts" );
		for( i=0 ; i<crv->seg_count ; i++ )
		{
			int seg_index;

			crv->reverse[i] = bu_glong( ptr );
			ptr += 4;
			seg_index = bu_glong( ptr );
			ptr += 4;
			crv->segments[i] = segs[seg_index];
		}
	}

	bu_free( (char *)segs, "segs" );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_import():\n" );
		bu_mem_barriercheck();
	}

	return(0);			/* OK */
}

/*
 *			R T _ S K E T C H _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_sketch_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_sketch_internal	*sketch_ip;
	union record		*rec;
	int	i, curve_no, seg_no, nbytes=0, ngran;
	struct bu_ptbl segs;
	vect_t tmp_vec;
	unsigned char *ptr;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SKETCH )  return(-1);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_export():\n" );
		bu_mem_barriercheck();
	}

	BU_INIT_EXTERNAL(ep);

	bu_ptbl_init( &segs, 64, "rt_sketch_export: segs" );
	nbytes = sizeof(union record);		/* base record */
	nbytes += sketch_ip->vert_count*(8*2);	/* vertex list */
	nbytes += sketch_ip->curve_count*(NAMESIZE+4);	/* curve name and seg_count */
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;

		crv = &sketch_ip->curves[curve_no];
		nbytes += 4 + crv->seg_count*8;	/* segment count + reverse flags and indices into segments array */

		for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
			bu_ptbl_ins_unique( &segs, (long *)crv->segments[seg_no] );
	}

	for( seg_no=0 ; seg_no < BU_PTBL_END( &segs ) ; seg_no++ )
	{
		long *lng;
		struct nurb_seg *nseg;

		lng = BU_PTBL_GET( &segs, seg_no );
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				nbytes += 12;
				break;
			case CURVE_CARC_MAGIC:
				nbytes += 28;
				break;
			case CURVE_NURB_MAGIC:
				nseg = (struct nurb_seg *)lng;
				nbytes += 16 + sizeof( struct knot_vector) + nseg->k.k_size * 8 + nseg->c_size * 4;
				if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
					nbytes += nseg->c_size * 8;	/* weights */
				break;
			default:
				bu_log( "rt_sketch_export: unsupported segement type (x%x)\n", *lng );
				bu_bomb( "rt_sketch_export: unsupported segement type\n" );
		}
	}

	ngran = ceil((double)(nbytes + sizeof(union record)) / sizeof(union record));
	ep->ext_nbytes = ngran * sizeof(union record);	
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "sketch external");

	rec = (union record *)ep->ext_buf;

	rec->skt.skt_id = DBID_SKETCH;

	/* convert from local editing units to mm and export
	 * to database record format
	 */
	VSCALE( tmp_vec, sketch_ip->V, local2mm );
	htond( rec->skt.skt_V, (unsigned char *)tmp_vec, 3 );

	/* uvec and uvec are unit vectors, do not convert to/from mm */
	htond( rec->skt.skt_uvec, (unsigned char *)sketch_ip->u_vec, 3 );
	htond( rec->skt.skt_vvec, (unsigned char *)sketch_ip->v_vec, 3 );

	(void)bu_plong( rec->skt.skt_vert_count, sketch_ip->vert_count );
	(void)bu_plong( rec->skt.skt_curve_count, sketch_ip->curve_count );
	(void)bu_plong( rec->skt.skt_seg_count, BU_PTBL_END( &segs) );
	(void)bu_plong( rec->skt.skt_count, ngran-1 );

	ptr = (unsigned char *)rec;
	ptr += sizeof( struct sketch_rec );
	/* convert 2D points to mm */
	for( i=0 ; i<sketch_ip->vert_count ; i++ )
	{
		point2d_t pt2d;

		V2SCALE( pt2d, sketch_ip->verts[i], local2mm )
		htond( ptr, (CONST unsigned char *)pt2d, 2 );
		ptr += 16;
	}

	for( seg_no=0 ; seg_no < BU_PTBL_END( &segs) ; seg_no++ )
	{
		struct line_seg *lseg;
		struct carc_seg *cseg;
		struct nurb_seg *nseg;
		long *lng;
		fastf_t tmp_fastf;

		/* write segment type ID, and segement parameters */
		lng = BU_PTBL_GET( &segs, seg_no );
		switch (*lng )
		{
			case CURVE_LSEG_MAGIC:
				lseg = (struct line_seg *)lng;
				(void)bu_plong( ptr, CURVE_LSEG_MAGIC );
				ptr += 4;
				(void)bu_plong( ptr, lseg->start );
				ptr += 4;
				(void)bu_plong( ptr, lseg->end );
				ptr += 4;
				break;
			case CURVE_CARC_MAGIC:
				cseg = (struct carc_seg *)lng;
				(void)bu_plong( ptr, CURVE_CARC_MAGIC );
				ptr += 4;
				(void)bu_plong( ptr, cseg->start );
				ptr += 4;
				(void)bu_plong( ptr, cseg->end );
				ptr += 4;
				(void) bu_plong( ptr, cseg->orientation );
				ptr += 4;
				(void) bu_plong( ptr, cseg->center_is_left );
				ptr += 4;
				tmp_fastf = cseg->radius * local2mm;
				htond( ptr, (unsigned char *)&tmp_fastf, 1 );
				ptr += 8;
				break;
			case CURVE_NURB_MAGIC:
				nseg = (struct nurb_seg *)lng;
				(void)bu_plong( ptr, CURVE_NURB_MAGIC );
				ptr += 4;
				(void)bu_plong( ptr, nseg->order );
				ptr += 4;
				(void)bu_plong( ptr, nseg->pt_type );
				ptr += 4;
				(void)bu_plong( ptr, nseg->k.k_size );
				ptr += 4;
				htond( ptr, (CONST unsigned char *)nseg->k.knots, nseg->k.k_size );
				ptr += nseg->k.k_size * 8;
				(void)bu_plong( ptr, nseg->c_size );
				ptr += 4;
				for( i=0 ; i<nseg->c_size ; i++ )
				{
					(void)bu_plong( ptr, nseg->ctl_points[i] );
					ptr +=  4;
				}
				if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
				{
					htond( ptr, (CONST unsigned char *)nseg->weights, nseg->c_size );
					ptr += 8 * nseg->c_size;
				}
				break;
			default:
				bu_bomb( "rt_sketch_export: ERROR: unrecognized curve type!!!!\n" );
				break;
				
		}
	}

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;
		int i;

		crv = &sketch_ip->curves[curve_no];

		/* write curve parameters */
		NAMEMOVE( crv->crv_name, (char *)ptr );
		ptr += NAMESIZE;
		(void)bu_plong( ptr,  crv->seg_count );
		ptr += 4;
		for( i=0 ; i<crv->seg_count ; i++ )
		{
			int seg_index;

			(void)bu_plong( ptr, crv->reverse[i] );
			ptr += 4;
			seg_index = bu_ptbl_locate( &segs, (long *)crv->segments[i] );
			if( seg_index < 0 )
				bu_bomb( "rt_sketch_export: ERROR: unlisted segement encountered!!!\n" );

			(void)bu_plong( ptr, seg_index );
			ptr += 4;
		}
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_export():\n" );
		bu_mem_barriercheck();
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_sketch_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_sketch_internal	*sketch_ip =
		(struct rt_sketch_internal *)ip->idb_ptr;
	int curve_no;
	int seg_no;
	char	buf[256];

	RT_SKETCH_CK_MAGIC(sketch_ip);
	bu_vls_strcat( str, "2D sketch (SKETCH)\n");

	sprintf(buf, "\tV = (%g %g %g)\n\t%d vertices, %d curves\n",
		V3ARGS( sketch_ip->V ),
		sketch_ip->vert_count,
		sketch_ip->curve_count );
	bu_vls_strcat( str, buf );

	if( !verbose )
		return( 0 );

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		sprintf(buf, "\tCurve: %-16.16s\n", sketch_ip->curves[curve_no].crv_name );
		bu_vls_strcat( str, buf );
		for( seg_no=0 ; seg_no < sketch_ip->curves[curve_no].seg_count ; seg_no++ )
		{
			struct line_seg *lsg;
			struct carc_seg *csg;
			struct nurb_seg *nsg;

			lsg = (struct line_seg *)sketch_ip->curves[curve_no].segments[seg_no];
			switch( lsg->magic )
			{
				case CURVE_LSEG_MAGIC:
					lsg = (struct line_seg *)sketch_ip->curves[curve_no].segments[seg_no];
					if( sketch_ip->curves[curve_no].reverse[seg_no] )
						sprintf( buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
							V2ARGS( sketch_ip->verts[lsg->end] ),
							V2ARGS( sketch_ip->verts[lsg->start] ) );
					else
						sprintf( buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
							V2ARGS( sketch_ip->verts[lsg->start] ),
							V2ARGS( sketch_ip->verts[lsg->end] ) );
					bu_vls_strcat( str, buf );
					break;
				case CURVE_CARC_MAGIC:
					csg = (struct carc_seg *)sketch_ip->curves[curve_no].segments[seg_no];
					if( csg->radius < 0.0 )
					{
						bu_vls_strcat( str, "\t\tFull Circle:\n" );
						sprintf( buf, "\t\t\tcenter: (%g %g)\n",
							V2ARGS( sketch_ip->verts[csg->end] ) );
						bu_vls_strcat( str, buf );
						sprintf( buf, "\t\t\tpoint on circle: (%g %g)\n",
							V2ARGS( sketch_ip->verts[csg->start] ) );
						bu_vls_strcat( str, buf );
					}
					else
					{
						bu_vls_strcat( str, "\t\tCircular Arc:\n" );
						if( sketch_ip->curves[curve_no].reverse[seg_no] )
							bu_vls_strcat( str, "\t\t\tarc is reversed\n" );
						sprintf( buf, "\t\t\tstart: (%g, %g)\n",
							V2ARGS( sketch_ip->verts[csg->start] ) );
						bu_vls_strcat( str, buf );
						sprintf( buf, "\t\t\tend: (%g, %g)\n",
							V2ARGS( sketch_ip->verts[csg->end] ) );
						bu_vls_strcat( str, buf );
						sprintf( buf, "\t\t\tradius: %g\n", csg->radius );
						bu_vls_strcat( str, buf );
						if( csg->orientation )
							bu_vls_strcat( str, "\t\t\tcurve is clock-wise\n" );
						else
							bu_vls_strcat( str, "\t\t\tcurve is counter-clock-wise\n" );
						if( csg->center_is_left )
							bu_vls_strcat( str, "\t\t\tcenter of curvature is left of the line from start point to end point\n" );
						else
							bu_vls_strcat( str, "\t\t\tcenter of curvature is right of the line from start point to end point\n" );
					}
					break;
				case CURVE_NURB_MAGIC:
					nsg = (struct nurb_seg *)sketch_ip->curves[curve_no].segments[seg_no];
					bu_vls_strcat( str, "\t\tNURB Curve:\n" );
					if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
					{
						sprintf( buf, "\t\t\tCurve is rational\n" );
						bu_vls_strcat( str, buf );
					}
					sprintf( buf, "\t\t\torder = %d, number of control points = %d\n",
						nsg->order, nsg->c_size );
					bu_vls_strcat( str, buf );
					if( sketch_ip->curves[curve_no].reverse[seg_no] )
						sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
							V2ARGS( sketch_ip->verts[nsg->ctl_points[nsg->c_size-1]] ),
							V2ARGS( sketch_ip->verts[nsg->ctl_points[0]] ) );
					else
						sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
							V2ARGS( sketch_ip->verts[nsg->ctl_points[0]] ),
							V2ARGS( sketch_ip->verts[nsg->ctl_points[nsg->c_size-1]] ) );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tknot values are %g to %g\n",
						nsg->k.knots[0], nsg->k.knots[nsg->k.k_size-1] );
					bu_vls_strcat( str, buf );
					break;
				default:
					bu_bomb( "rt_sketch_describe: ERROR: unrecognized segment type\n" );
			}
		}
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_sketch_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_sketch_internal	*sketch_ip;
	struct curve				*crv;
	int					curve_no;
	int					seg_no;

	RT_CK_DB_INTERNAL(ip);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);
	sketch_ip->magic = 0;			/* sanity */

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_ifree():\n" );
		bu_mem_barriercheck();
	}

	bu_free( (char *)sketch_ip->verts, "sketch_ip->verts" );
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		long *lng;

		crv = &sketch_ip->curves[curve_no];

		for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
		{
			lng = (long *)crv->segments[seg_no];
			if( *lng > 0 )
			{
				*lng = 0;
				bu_free( (char *)lng, "lng" );
			}
		}

		bu_free( (char *)crv->reverse, "crv->reverse" );
		bu_free( (char *)crv->segments, "crv->segments" );
	}
	bu_free( (char *)sketch_ip->curves, "sketch_ip->curves" );
	bu_free( (char *)sketch_ip, "sketch ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_ifree():\n" );
		bu_mem_barriercheck();
	}

}

struct rt_sketch_internal *
rt_copy_sketch( sketch_ip )
CONST struct rt_sketch_internal *sketch_ip;
{
	struct rt_sketch_internal *out;
	int i;
	struct bu_ptbl curves;

	RT_SKETCH_CK_MAGIC( sketch_ip );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of rt_copy_sketch():\n" );
		bu_mem_barriercheck();
	}

	out = (struct rt_sketch_internal *) bu_malloc( sizeof( struct rt_sketch_internal ), "rt_sketch_internal" );
	*out = *sketch_ip;	/* struct copy */

	out->verts = (point2d_t *)bu_calloc( out->vert_count, sizeof( point2d_t ), "out->verts" );
	for( i=0 ; i<out->vert_count ; i++ )
		V2MOVE( out->verts[i], sketch_ip->verts[i] );

	out->curves = (struct curve *)bu_calloc( out->curve_count, sizeof( struct curve ), "out->curves" );
	for( i=0 ; i<out->curve_count ; i++ )
	{
		struct curve *crv_out, *crv_in;
		int j;

		crv_out = &out->curves[i];
		crv_in = &sketch_ip->curves[i];
		strncpy( crv_out->crv_name, crv_in->crv_name, SKETCH_NAME_LEN );
		crv_out->seg_count = crv_in->seg_count;
		crv_out->reverse = (int *)bu_calloc( crv_out->seg_count, sizeof( int ), "crv->reverse" );
		crv_out->segments = (genptr_t *)bu_calloc( crv_out->seg_count, sizeof( genptr_t ), "crv->segments" );
		for( j=0 ; j<crv_out->seg_count ; j++ )
		{
			long *lng;
			struct line_seg *lsg_out, *lsg_in;
			struct carc_seg *csg_out, *csg_in;
			struct nurb_seg *nsg_out, *nsg_in;

			crv_out->reverse[j] = crv_in->reverse[j];
			lng = (long *)crv_in->segments[j];
			switch( *lng )
			{
				case CURVE_LSEG_MAGIC:
					lsg_in = (struct line_seg *)lng;
					lsg_out = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "line_seg" );
					crv_out->segments[j] = (genptr_t)lsg_out;
					*lsg_out = *lsg_in;
					break;
				case CURVE_CARC_MAGIC:
					csg_in = (struct carc_seg *)lng;
					csg_out = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "carc_seg" );
					crv_out->segments[j] = (genptr_t)csg_out;
					*csg_out = *csg_in;
					break;
				case CURVE_NURB_MAGIC:
					nsg_in = (struct nurb_seg *)lng;
					nsg_out = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nurb_seg" );
					crv_out->segments[j] = (genptr_t)nsg_out;
					*nsg_out = *nsg_in;
					nsg_out->ctl_points = (int *)bu_calloc( nsg_in->c_size, sizeof( int ), "nsg_out->ctl_points" );
					for( i=0 ; i<nsg_out->c_size ; i++ )
						nsg_out->ctl_points[i] = nsg_in->ctl_points[i];
					if( RT_NURB_IS_PT_RATIONAL( nsg_in->pt_type ) )
					{
						nsg_out->weights = (fastf_t *)bu_malloc( nsg_out->c_size * sizeof( fastf_t ), "nsg_out->weights" );
						for( i=0 ; i<nsg_out->c_size ; i++ )
							nsg_out->weights[i] = nsg_in->weights[i];
					}
					else
						nsg_out->weights = (fastf_t *)NULL;
					nsg_out->k.knots = bu_malloc( nsg_in->k.k_size * sizeof( fastf_t ), "nsg_out->k.knots" );
					for( i=0 ; i<nsg_in->k.k_size ; i++ )
						nsg_out->k.knots[i] = nsg_in->k.knots[i];
					break;
				default:
					bu_log( "rt_copy_sketch: ERROR: unrecognized segment type!!!!\n" );
					return( (struct rt_sketch_internal *)NULL );
			}
		}
	}

	/* fix back pointers */
	bu_ptbl_init( &curves, 10, "bu_ptbl curves" );
	for( i=0 ; i<out->curve_count ; i++ )
	{
		struct curve *crv_out;
		int j;

		crv_out = &out->curves[i];
		for( j=0 ; j<crv_out->seg_count ; j++ )
		{
			long *lng;
			int k;

			/* count how many curves use this segment */
			lng = (long *)crv_out->segments[j];
			for( k=0 ; k<out->curve_count ; k++ )
			{
				int l;
				struct curve *crv;

				crv = &out->curves[k];
				for( l=0 ; l< crv->seg_count ; l++ )
				{
					if( (long *)crv->segments[l] == lng )
						bu_ptbl_ins_unique( &curves, lng );
				}
			}

			crv_out->segments = bu_calloc( BU_PTBL_END( &curves ), sizeof( genptr_t ), "crv_out->segments" );
			for( k=0 ; k<BU_PTBL_END( &curves ) ; k++ )
				crv_out->segments = (genptr_t)BU_PTBL_GET( &curves, k );
			bu_ptbl_trunc( &curves, 0 );
		}
	}
	bu_ptbl_free( &curves );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of rt_copy_sketch():\n" );
		bu_mem_barriercheck();
	}

	return( out );
}
