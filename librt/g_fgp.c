/*
 *			G _ P L A T E . C
 *
 *  Purpose -
 *	Intersect a ray with a fgp mode solid
 *
 *  Authors -
 *  	John Anderson
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
static char RCSxxx[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

/* ray tracing form of solid, including precomputed terms */
struct fgp_specific {
	fastf_t thickness;
	int mode;
	struct soltab *ref_stp;
};

/* defines for hit_surfno field of a struct hit */
#define	ZERO_SEG	1	/* created from a zero length segment */
#define	IN_SEG		2	/* created from an "in_hit" */
#define	OUT_SEG		3	/* created from an "out_hit" */

#define	RT_FGP_O(m)	offsetof( struct rt_fgp_internal, m )

CONST struct bu_structparse rt_fgp_parse[] = {
	{ "%s", NAMELEN, "solid", bu_offsetofarray(struct rt_fgp_internal, referenced_solid), BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 1, "t", RT_FGP_O( thickness ), BU_STRUCTPARSE_FUNC_NULL },
	{ "%d", 1, "m", RT_FGP_O( mode ), BU_STRUCTPARSE_FUNC_NULL },
	{0} };

static fastf_t cos_min=0.0008726;

/*
 *  			R T _ P L A T E _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid fgp mode solid, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	fgp is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct fgp_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rt_fgp_shot().
 */
int
rt_fgp_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_fgp_internal			*fgp_ip;
	register struct fgp_specific		*fgp;
	CONST struct bn_tol			*tol = &rtip->rti_tol;
	struct directory			*ref_dp;
	struct rt_db_internal			ref_int;
	struct soltab				*ref_stp;
	struct db_full_path			*ref_path;
	int					ref_id;
	int					ref_ret;

	RT_CK_DB_INTERNAL(ip);
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC(fgp_ip);

	if( (ref_dp = db_lookup( rtip->rti_dbip, fgp_ip->referenced_solid, 1 )) == DIR_NULL )
	{
		bu_log( "Cannot find solid %s (referenced by fgp mode solid %s)\n",
			fgp_ip->referenced_solid, stp->st_dp->d_namep );
		return( 1 );
	}

	if( (ref_id = rt_db_get_internal( &ref_int, ref_dp, rtip->rti_dbip, fgp_ip->xform ) ) < 0 )
	{
		bu_log( "Error getting internal form of solid %s (referenced by fgp mode solid %s)\n",
			fgp_ip->referenced_solid, stp->st_dp->d_namep );
		return( 2 );
	}

	BU_GETSTRUCT( fgp, fgp_specific );
	fgp->mode = fgp_ip->mode;
	fgp->thickness = fgp_ip->thickness;
	stp->st_specific = (genptr_t)fgp;

	BU_GETSTRUCT( ref_stp, soltab );
	fgp->ref_stp = ref_stp;
	BU_LIST_INIT( &ref_stp->l );
	BU_LIST_INIT( &ref_stp->l2 );
	ref_stp->l.magic = RT_SOLTAB_MAGIC;
	ref_stp->l2.magic = RT_SOLTAB2_MAGIC;
	ref_stp->st_rtip = rtip;
	ref_stp->st_id = ref_id;
	ref_stp->st_meth = &rt_functab[ref_id];
	ref_stp->st_dp = ref_dp;
	if( stp->st_matp )
	{
		ref_stp->st_matp = (matp_t)bu_malloc( sizeof( mat_t ), "ref_stp->st_matp" );
		bn_mat_copy( ref_stp->st_matp, stp->st_matp );
	}
	else
		ref_stp->st_matp = NULL;

	db_full_path_init( &ref_stp->st_path );
	if( stp->st_path.magic == DB_FULL_PATH_MAGIC )
		db_dup_full_path( &ref_stp->st_path, &stp->st_path );
	db_add_node_to_full_path( &ref_stp->st_path, ref_dp );

	ref_ret =  ref_stp->st_meth->ft_prep( ref_stp, &ref_int, rtip );

	rt_db_free_internal( &ref_int );

	stp->st_aradius = ref_stp->st_aradius;
	stp->st_bradius = ref_stp->st_bradius;
	VMOVE( stp->st_max, ref_stp->st_max );
	VMOVE( stp->st_min, ref_stp->st_min );

	return( ref_ret );
}

/*
 *			R T _ P L A T E _ P R I N T
 */
void
rt_fgp_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct fgp_specific *fgp =
		(struct fgp_specific *)stp->st_specific;

	if( fgp->mode == RT_FGP_CENTER_MODE )
	{
		bu_log( "Referenced solid: %s-16.16s\n", fgp->ref_stp->st_dp->d_namep );
		bu_log( "Thickness %g (Centered about hit point)\n", fgp->thickness );
	}
	else if( fgp->mode == RT_FGP_FRONT_MODE )
	{
		bu_log( "Referenced solid: %s-16.16s\n", fgp->ref_stp->st_dp->d_namep );
		bu_log( "Thickness %g (Extended from hit point in ray direction)\n", fgp->thickness );
	}
	else
	{
		bu_log( "Referenced solid: %s-16.16s\n", fgp->ref_stp->st_dp->d_namep );
		bu_log( "Thickness %g (Unreconized mode %d)\n",  fgp->thickness, fgp->mode );
	}
}

/*
 *  			R T _ P L A T E _ S H O T
 *  
 *  Intersect a ray with a fgp mode solid.
 *  If an intersection occurs, at least one struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_fgp_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct fgp_specific *fgp =
		(struct fgp_specific *)stp->st_specific;
	struct seg		ref_seghead;
	register struct seg	*segp, *new_seg;
	int			ref_ret, hit_count=0;
	fastf_t			cos_in, cos_out;
	fastf_t			dist1, dist2;
	vect_t			norm_in, norm_out;
	CONST struct bn_tol	*tol = &ap->a_rt_i->rti_tol;

	BU_LIST_INIT( &ref_seghead.l );

	ref_ret = fgp->ref_stp->st_meth->ft_shot( fgp->ref_stp, rp, ap, &ref_seghead );

	if( ref_ret )
	{
		while( BU_LIST_WHILE( segp, seg, &ref_seghead.l ) )
		{
			BU_LIST_DEQUEUE( &segp->l );

			if( segp->seg_in.hit_dist == segp->seg_out.hit_dist )
			{
				/* zero length segment */

				dist1 = segp->seg_in.hit_dist;

				/* get the normal */
				RT_HIT_NORMAL( norm_in, &segp->seg_in, fgp->ref_stp, rp, 0 )
				RT_HIT_NORMAL( norm_out, &segp->seg_out, fgp->ref_stp, rp, 0 )

				cos_in = VDOT( rp->r_dir, norm_in );
				if( cos_in < 0.0 )
					cos_in = -cos_in;

				if( cos_in < cos_min )
				{
					/* FASTGEN4 calls this a miss */
					RT_FREE_SEG( segp, ap->a_resource );
					continue;
				}

				segp->seg_stp = stp;
				
				/* set the surface number */
				segp->seg_in.hit_surfno = ZERO_SEG;
				segp->seg_out.hit_surfno = ZERO_SEG;

				/* set the normals */
				VMOVE( segp->seg_in.hit_normal, norm_in )
				VMOVE( segp->seg_out.hit_normal, norm_out )

				if( fgp->mode == RT_FGP_FRONT_MODE )
				{
					segp->seg_in.hit_dist = dist1;
					segp->seg_out.hit_dist = dist1 + fgp->thickness / cos_in;

					/* correct the hit point */
					VJOIN1( segp->seg_in.hit_point, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir )
					VJOIN1( segp->seg_out.hit_point, rp->r_pt, segp->seg_out.hit_dist, rp->r_dir )

					BU_LIST_INSERT( &(seghead->l), &(segp->l) );
					hit_count++;
				}
				else if( fgp->mode == RT_FGP_CENTER_MODE )
				{
					fastf_t half_los=0.5 * fgp->thickness / cos_in;

					segp->seg_in.hit_dist = dist1 - half_los;
					segp->seg_out.hit_dist = dist1 +  half_los;

					/* correct the hit point */
					VJOIN1( segp->seg_in.hit_point, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir )
					VJOIN1( segp->seg_out.hit_point, rp->r_pt, segp->seg_out.hit_dist, rp->r_dir )
					BU_LIST_INSERT( &(seghead->l), &(segp->l) );
					hit_count++;
				}
				else
				{
					bu_log( "Unrecognized mode (%d) for fgp solid (%s)\n\tHit ignored\n" );
					RT_FREE_SEG( segp, ap->a_resource );
				}
				continue;
			}

			/* get the normal */
			RT_HIT_NORMAL( norm_in, &segp->seg_in, fgp->ref_stp, rp, 0 )
			RT_HIT_NORMAL( norm_out, &segp->seg_out, fgp->ref_stp, rp, 0 )

			cos_in = VDOT( rp->r_dir, norm_in );
			if( cos_in < 0.0 )
				cos_in = -cos_in;

			cos_out = VDOT( rp->r_dir, norm_out );
			if( cos_out < 0.0 )
				cos_out = -cos_out;

			if( fgp->mode == RT_FGP_FRONT_MODE )
			{
				if( cos_in > cos_min )
				{
					/* otherwise FASTGEN4 calls this a miss */

					RT_GET_SEG( new_seg, ap->a_resource );
					new_seg->seg_stp = stp;
					new_seg->seg_in.hit_dist = segp->seg_in.hit_dist;
					new_seg->seg_out.hit_dist = segp->seg_in.hit_dist + fgp->thickness / cos_in;

					/* correct the hit point */
					VJOIN1( new_seg->seg_in.hit_point, rp->r_pt, new_seg->seg_in.hit_dist, rp->r_dir )
					VJOIN1( new_seg->seg_out.hit_point, rp->r_pt, new_seg->seg_out.hit_dist, rp->r_dir )

					/* set the normals */
					VMOVE( new_seg->seg_in.hit_normal, norm_in )
					VREVERSE( new_seg->seg_out.hit_normal, norm_in )

					new_seg->seg_in.hit_surfno = IN_SEG;
					new_seg->seg_out.hit_surfno = IN_SEG;

					BU_LIST_INSERT( &(seghead->l), &(new_seg->l) );
					hit_count++;
				}

				if( cos_out > cos_min )
				{
					/* otherwise FASTGEN4 calls this a miss */

					RT_GET_SEG( new_seg, ap->a_resource );
					new_seg->seg_stp = stp;
					new_seg->seg_in.hit_dist = segp->seg_out.hit_dist;
					new_seg->seg_out.hit_dist = segp->seg_out.hit_dist + fgp->thickness / cos_out;

					/* correct the hit point */
					VJOIN1( new_seg->seg_in.hit_point, rp->r_pt, new_seg->seg_in.hit_dist, rp->r_dir )
					VJOIN1( new_seg->seg_out.hit_point, rp->r_pt, new_seg->seg_out.hit_dist, rp->r_dir )

					/* set the normals */
					VREVERSE( new_seg->seg_in.hit_normal, norm_out )
					VMOVE( new_seg->seg_out.hit_normal, norm_out )

					new_seg->seg_in.hit_surfno = OUT_SEG;
					new_seg->seg_out.hit_surfno = OUT_SEG;

					BU_LIST_INSERT( &(seghead->l), &(new_seg->l) );
					hit_count++;
				}
			}
			else if( fgp->mode == RT_FGP_CENTER_MODE )
			{
				if( cos_in > cos_min ) /* otherwise FASTGEN4 calls this a miss */
				{
					/* otherwise FASTGEN4 calls this a miss */

					fastf_t half_los=0.5 * fgp->thickness / cos_in;

					RT_GET_SEG( new_seg, ap->a_resource );
					new_seg->seg_stp = stp;
					new_seg->seg_in.hit_dist = segp->seg_in.hit_dist - half_los;
					new_seg->seg_out.hit_dist = segp->seg_in.hit_dist + half_los;

					/* correct the hit point */
					VJOIN1( new_seg->seg_in.hit_point, rp->r_pt, new_seg->seg_in.hit_dist, rp->r_dir )
					VJOIN1( new_seg->seg_out.hit_point, rp->r_pt, new_seg->seg_out.hit_dist, rp->r_dir )

					/* set the normals */
					VMOVE( new_seg->seg_in.hit_normal, norm_in )
					VREVERSE( new_seg->seg_out.hit_normal, norm_in )

					new_seg->seg_in.hit_surfno = IN_SEG;
					new_seg->seg_out.hit_surfno = IN_SEG;

					BU_LIST_INSERT( &(seghead->l), &(new_seg->l) );
					hit_count++;
				}

				if( cos_out > cos_min )
				{
					/* otherwise FASTGEN4 calls this a miss */

					fastf_t half_los=0.5 * fgp->thickness / cos_out;

					RT_GET_SEG( new_seg, ap->a_resource );
					new_seg->seg_stp = stp;
					new_seg->seg_in.hit_dist = segp->seg_out.hit_dist - half_los;
					new_seg->seg_out.hit_dist = segp->seg_out.hit_dist + half_los;

					/* correct the hit point */
					VJOIN1( new_seg->seg_in.hit_point, rp->r_pt, new_seg->seg_in.hit_dist, rp->r_dir )
					VJOIN1( new_seg->seg_out.hit_point, rp->r_pt, new_seg->seg_out.hit_dist, rp->r_dir )

					/* set the normals */
					VREVERSE( new_seg->seg_in.hit_normal, norm_out )
					VMOVE( new_seg->seg_out.hit_normal, norm_out )

					new_seg->seg_in.hit_surfno = OUT_SEG;
					new_seg->seg_out.hit_surfno = OUT_SEG;

					BU_LIST_INSERT( &(seghead->l), &(new_seg->l) );
					hit_count++;
				}
			}
			else
				bu_log( "Unrecognized mode (%d) for fgp solid (%s)\n\tHit ignored\n" );

			RT_FREE_SEG( segp, ap->a_resource );
		}
	}

	return( hit_count );
}

#define RT_FGP_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ P L A T E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_fgp_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ P L A T E _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 *	this is all calculated by the shot routine, nothing to do here.
 */
void
rt_fgp_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	return;
}

/*
 *			R T _ P L A T E _ C U R V E
 *
 *  Return the curvature of the fgp.
 */
void
rt_fgp_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{

	/* for now, don't do curvature */
 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ P L A T E_ U V
 *  
 *  For a hit on the surface of an fgp, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 */
void
rt_fgp_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	uvp->uv_u = 0.0;
	uvp->uv_v = 0.0;
	uvp->uv_du = 0.0;
	uvp->uv_dv = 0.0;
}

/*
 *		R T _ P L A T E _ F R E E
 */
void
rt_fgp_free( stp )
register struct soltab *stp;
{
	register struct fgp_specific *fgp =
		(struct fgp_specific *)stp->st_specific;

	fgp->ref_stp->st_meth->ft_free( fgp->ref_stp );

	bu_free( (char *)fgp->ref_stp, "ref_stp" );

	bu_free( (char *)fgp, "fgp_specific" );
}

/*
 *			R T _ P L A T E _ C L A S S
 */
int
rt_fgp_class( stp, min, max, tol )
CONST struct soltab    *stp;
CONST vect_t		min, max;
CONST struct bn_tol    *tol;
{
	register struct fgp_specific *fgp =
		(struct fgp_specific *)stp->st_specific;


	return( fgp->ref_stp->st_meth->ft_classify( fgp->ref_stp, min, max, tol ) );
}

/*
 *			R T _ P L A T E _ P L O T
 */
int
rt_fgp_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_fgp_internal	*fgp_ip;
	struct rt_db_internal		ref_int;
	int				ref_id;
	int				ref_ret;

	RT_CK_DB_INTERNAL(ip);
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC(fgp_ip);

	if( (ref_id = rt_db_get_internal( &ref_int, fgp_ip->ref_dp , fgp_ip->dbip, &fgp_ip->xform ) ) < 0 )
	{
		bu_log( "Error getting internal form of solid %s (referenced by fgp mode solid)\n",
			fgp_ip->referenced_solid );
		return( -2 );
	}
	
	ref_ret = rt_functab[ref_id].ft_plot( vhead, &ref_int, ttol, tol );

	rt_db_free_internal( &ref_int );

	return(ref_ret);
}

/*
 *			R T _ P L A T E _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_fgp_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_fgp_internal	*fgp_ip;

	RT_CK_DB_INTERNAL(ip);
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC(fgp_ip);

	return(-1);
}

/*
 *			R T _ P L A T E _ I M P O R T
 *
 *  Import an fgp from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_fgp_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_fgp_internal	*fgp_ip;
	union record			*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */

	if( rp->u_id != DBID_FGP )  {
		bu_log("rt_fgp_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_FGP;
	ip->idb_meth = &rt_functab[ID_FGP];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_fgp_internal), "rt_fgp_internal");
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	fgp_ip->magic = RT_FGP_INTERNAL_MAGIC;
	ntohd( (unsigned char *)(&fgp_ip->thickness), rp->fgp.fgp_thickness, 1 );
	fgp_ip->mode = bu_glong( rp->fgp.fgp_mode );
	NAMEMOVE( rp->fgp.fgp_ref_sol, fgp_ip->referenced_solid );
	bn_mat_copy( fgp_ip->xform, mat );

	if( dbip != DBI_NULL )
	{
		if( (fgp_ip->ref_dp = db_lookup( dbip, fgp_ip->referenced_solid, 0 )) == DIR_NULL )
		{
			bu_log( "Cannot find solid %s (referenced by fgp mode solid)\n",
				fgp_ip->referenced_solid );
			return( 1 );
		}

		fgp_ip->dbip = dbip;
	}
	else
	{
		fgp_ip->ref_dp = DIR_NULL;
		fgp_ip->dbip = DBI_NULL;
	}

	return(0);			/* OK */
}

/*
 *			R T _ P L A T E _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_fgp_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_fgp_internal	*fgp_ip;
	union record			*rec;
	fastf_t				tmp;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_FGP )  return(-1);
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC(fgp_ip);

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "fgp external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->fgp.fgp_id = DBID_FGP;	/* GED primitive type from db.h */

	tmp = fgp_ip->thickness * local2mm;
	htond( rec->fgp.fgp_thickness, (unsigned char *)(&tmp), 1 );
	NAMEMOVE( fgp_ip->referenced_solid, rec->fgp.fgp_ref_sol );
	(void)bu_plong( rec->fgp.fgp_mode, fgp_ip->mode );

	return(0);
}

/*
 *			R T _ P L A T E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_fgp_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_fgp_internal	*fgp_ip =
		(struct rt_fgp_internal *)ip->idb_ptr;
	char	buf[256];

	RT_FGP_CK_MAGIC(fgp_ip);
	bu_vls_strcat( str, "fgp mode solid (FGP)\n");

	if( fgp_ip->mode == RT_FGP_FRONT_MODE )
		bu_vls_strcat( str, "\tthickness extends in ray direction from hit point\n" );
	else if( fgp_ip->mode == RT_FGP_CENTER_MODE )
		bu_vls_strcat( str, "\tthickness is extended equally on both sides of hit point\n" );
	else
		bu_vls_strcat( str, "\tUnrecognized mode!!!\n" );

	sprintf(buf, "\tThickness = %g\n\treferenced solid = %s\n",
		fgp_ip->thickness * mm2local,
		fgp_ip->referenced_solid );
	bu_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ P L A T E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_fgp_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_fgp_internal	*fgp_ip;

	RT_CK_DB_INTERNAL(ip);
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC(fgp_ip);
	fgp_ip->magic = 0;			/* sanity */

	bu_free( (char *)fgp_ip, "fgp ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_fgp_tnurb( r, m, ip, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
struct bn_tol		*tol;
{
	return( 1 );
}

int
rt_fgp_xform(op, mat, ip, free)
struct rt_db_internal	*op;
CONST mat_t		mat;
struct rt_db_internal *ip;
CONST int	free;
{
	struct rt_fgp_internal *fgp_ip, *fgp_op;

	if( op == ip )
		return( 0 );

	RT_CK_DB_INTERNAL( ip );
	fgp_ip = (struct rt_fgp_internal *)ip->idb_ptr;
	RT_FGP_CK_MAGIC( fgp_ip );

	/*
	 * We are done with the input solid so free it if required.
	 */
	if (free) {
		RT_INIT_DB_INTERNAL( op );
		*op = *ip;
		ip->idb_ptr = (genptr_t) 0;
	}
	else
	{
		RT_INIT_DB_INTERNAL( op );
		BU_GETSTRUCT( fgp_op, rt_fgp_internal );
		*fgp_op = *fgp_ip;
		*op = *ip;
		op->idb_ptr = (genptr_t)fgp_op;
	}

	return( 0 );
}
