#define HONEST_WAY	1

/*
 *			G _ S U B M O D E L . C
 *
 *  Purpose -
 *	Intersect a ray with an entire subspace full of geometry,
 *	possibly included from another .g file, with a subordinate
 *	instance of LIBRT.
 *
 *  This solid is particularly useful when instancing millions of copies
 *  of a given complex object, such as a detailed tree.
 *
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSsubmodel[] = "@(#)$Header$ (BRL)";
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

#define RT_SUBMODEL_O(m)	offsetof(struct rt_submodel_internal, m)

CONST struct bu_structparse rt_submodel_parse[] = {
	{"%s",	128, "file",	bu_offsetofarray(struct rt_submodel_internal, file), BU_STRUCTPARSE_FUNC_NULL },
	{"%s",	128, "treetop", bu_offsetofarray(struct rt_submodel_internal, treetop), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "meth",	RT_SUBMODEL_O(meth),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0, 0,			BU_STRUCTPARSE_FUNC_NULL }
};



/* ray tracing form of solid, including precomputed terms */
struct submodel_specific {
	long		magic;
	mat_t		subm2m;		/* To transform normals back out to model coords */
	mat_t		m2subm;		/* To transform rays into local coord sys */
	struct rt_i	*rtip;		/* sub model */
};
#define RT_SUBMODEL_SPECIFIC_MAGIC	0x73756253	/* subS */
#define RT_CK_SUBMODEL_SPECIFIC(_p)	BU_CKMAG(_p,RT_SUBMODEL_SPECIFIC_MAGIC,"submodel_specific")

#if !HONEST_WAY
CONST extern struct rt_functab rt_submodelhook_functab;
#endif

/*
 *  			R T _ S U B M O D E L _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid SUBMODEL, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	SUBMODEL is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct submodel_specific is created, and it's address is stored in
 *  	stp->st_specific for use by submodel_shot().
 */
int
rt_submodel_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_submodel_internal	*sip;
	struct submodel_specific	*submodel;
	struct rt_i			*sub_rtip;
	struct db_i			*sub_dbip;
	struct soltab			*sub_stp;
	struct region			*rp;
	vect_t	radvec;
	vect_t	diam;
	char	*argv[2];
	struct rt_i	**rtipp;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);

	bu_semaphore_acquire(RT_SEM_MODEL);
	/* This code must be prepared to run in parallel
	 * without tripping over itself.
	 */
	if( sip->file[0] == '\0' )  {
		/* No .g file name given, tree is in current .g file */
		sub_dbip = rtip->rti_dbip;
	} else {
		/* db_open will cache dbip's via bu_open_mapped_file() */
		if( (sub_dbip = db_open( sip->file, "r" )) == DBI_NULL )
		    	return -1;
		if( !db_is_directory_non_empty(sub_dbip) )  {
			/* This is first open of db, build directory */
			if( db_scan( sub_dbip, (int (*)())db_diradd, 1, NULL ) < 0 )  {
				db_close( sub_dbip );
				bu_semaphore_release(RT_SEM_MODEL);
				return -1;
			}
		}
	}

	/*
	 *  Search for a previous exact use of this file and treetop,
	 *  so as to obtain storage efficiency from re-using it.
	 *  Search dbi_client list for a re-use of an rt_i.
	 *  rtip's are registered there by db_clone_dbi().
	 */
	for( BU_PTBL_FOR( rtipp, (struct rt_i **), &sub_dbip->dbi_clients ) )  {
		register char	*ttp;
		RT_CK_RTI(*rtipp);
		ttp = (*rtipp)->rti_treetop;
		if( ttp && strcmp( ttp, sip->treetop ) == 0 )  {
			/* Re-cycle an already prepped rti */
			sub_rtip = *rtipp;
			sub_rtip->rti_uses++;

			bu_semaphore_release(RT_SEM_MODEL);

			if( rt_g.debug & (DEBUG_DB|DEBUG_SOLIDS) )  {
				bu_log("rt_submodel_prep(%s): Re-used already prepped database %s, rtip=x%lx\n",
					stp->st_dp->d_namep,
					sub_dbip->dbi_filename,
					(long)sub_rtip );
			}
			goto done;
		}
	}

	sub_rtip = rt_new_rti( sub_dbip );	/* does db_clone_dbi() */
	RT_CK_RTI(sub_rtip);

	bu_semaphore_release(RT_SEM_MODEL);

	if( rt_g.debug & (DEBUG_DB|DEBUG_SOLIDS) )  {
		bu_log("rt_submodel_prep(%s): Opened database %s\n",
			stp->st_dp->d_namep, sub_dbip->dbi_filename );
	}

	/* Propagate some important settings downward */
	sub_rtip->useair = rtip->useair;
	sub_rtip->rti_dont_instance = rtip->rti_dont_instance;
	sub_rtip->rti_hasty_prep = rtip->rti_hasty_prep;
	sub_rtip->rti_tol = rtip->rti_tol;	/* struct copy */
	sub_rtip->rti_ttol = rtip->rti_ttol;	/* struct copy */
	sub_rtip->rti_treetop = bu_strdup(sip->treetop);

	if( sip->meth )  {
		sub_rtip->rti_space_partition = sip->meth;
	} else {
		sub_rtip->rti_space_partition = rtip->rti_space_partition;
	}

	argv[0] = sip->treetop;
	argv[1] = NULL;
	if( rt_gettrees( sub_rtip, 1, (CONST char **)argv, 1 ) < 0 )  {
		rt_free_rti( rtip );
		return -2;
	}

	if( sub_rtip->nsolids <= 0 )  {
		bu_log("rt_submodel_prep(%s): %s No solids found\n",
			stp->st_dp->d_namep, sip->file );
		return -3;
	}

	/* OK, it's going to work.  Prep the submodel. */
	/* Stay on 1 CPU because we're already multi-threaded at this point. */
	rt_prep_parallel(sub_rtip, 1);

	/* Ensure bu_ptbl rti_resources is full size.  Ptrs will be null */
	if( BU_PTBL_LEN(&sub_rtip->rti_resources) < sub_rtip->rti_resources.blen )  {
		BU_PTBL_LEN(&sub_rtip->rti_resources) = sub_rtip->rti_resources.blen;
	}

#if !HONEST_WAY
	/*
	 *  Record the "up" pointer from the sub model.
	 */
	/* XXX This is evil and must be eliminated */
	sub_rtip->rti_up = stp;

	/*
	 *  XXX Idea? Here, just rip off the regions and solids from the
	 *  submodel, and make them part of the main model.
	 *  Then just stealing segs will be OK.
	 */
	while( BU_LIST_NON_EMPTY( &sub_rtip->HeadRegion ) )  {
		rp = BU_LIST_FIRST( region, &sub_rtip->HeadRegion );
		BU_LIST_DEQUEUE( &(rp->l) );

		bu_semaphore_acquire( RT_SEM_RESULTS );	/* enter critical section */
		/* Add to up_rtip's list */
		BU_LIST_INSERT( &rtip->HeadRegion, &(rp->l) );
		rp->reg_bit = rtip->nregions++;	/* Assign bit vector pos. */
		bu_semaphore_release( RT_SEM_RESULTS );	/* leave critical section */

	}

	/*
	 *  Visit all solids in sub_rtip and change their st_meth
	 *  pointers to our hook routines,
	 *  for coordinate system transformations.
	 */
	RT_VISIT_ALL_SOLTABS_START( sub_stp, sub_rtip )  {
		sub_stp->st_meth = &rt_submodelhook_functab;
	} RT_VISIT_ALL_SOLTABS_END
#endif

done:	
	BU_GETSTRUCT( submodel, submodel_specific );
	submodel->magic = RT_SUBMODEL_SPECIFIC_MAGIC;
	stp->st_specific = (genptr_t)submodel;

	bn_mat_copy( submodel->subm2m, sip->root2leaf );
	bn_mat_inv( submodel->m2subm, sip->root2leaf );
	submodel->rtip = sub_rtip;

	/* Propagage submodel bounding box back upwards, rotated&scaled. */
	bn_rotate_bbox( stp->st_min, stp->st_max,
		submodel->subm2m,
		sub_rtip->mdl_min, sub_rtip->mdl_max );

	VSUB2( diam, stp->st_max, stp->st_min );
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSCALE( radvec, diam, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

	if( rt_g.debug & (DEBUG_DB|DEBUG_SOLIDS) )  {
		bu_log("rt_submodel_prep(%s): finished loading database %s\n",
			stp->st_dp->d_namep, sub_dbip->dbi_filename );
	}

	return(0);		/* OK */
}

/*
 *			R T _ S U B M O D E L _ P R I N T
 */
void
rt_submodel_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;

	RT_CK_SUBMODEL_SPECIFIC(submodel);

	bn_mat_print("subm2m", submodel->subm2m);
	bn_mat_print("m2subm", submodel->m2subm);

	bu_log_indent_delta(4);
	bu_log("submodel->rtip=x%x\n", submodel->rtip);

	/* Loop through submodel's solid table printing them too. */
	RT_VISIT_ALL_SOLTABS_START( stp, submodel->rtip )  {
		rt_pr_soltab(stp);
	} RT_VISIT_ALL_SOLTABS_END
	bu_log_indent_delta(-4);
}


/*
 *			R T _ S U B M O D E L _ A _ M I S S
 */
int
rt_submodel_a_miss( ap )
struct application	*ap;
{
	return 0;
}

struct submodel_gobetween {
	struct application	*up_ap;
	struct seg		*up_seghead;
	struct soltab		*up_stp;
	fastf_t			delta;		/* distance offset */
};

/*
 *			R T _ S U B M O D E L _ A _ H I T
 */
int
rt_submodel_a_hit( ap, PartHeadp, segHeadp )
struct application	*ap;
struct partition	*PartHeadp;
struct seg		*segHeadp;
{
	register struct partition *pp;
	struct application	*up_ap;
	struct seg		*segp;
	struct soltab		*up_stp;
	struct region		*up_reg;
	struct submodel_gobetween *gp;
	struct submodel_specific *submodel;
	fastf_t			delta;
	int			count = 0;

	RT_AP_CHECK(ap);
	RT_CK_PT_HD(PartHeadp);
	gp = (struct submodel_gobetween *)ap->a_uptr;
	up_ap = gp->up_ap;
	RT_AP_CHECK(up_ap);		/* original ap, in containing  */
	RT_CK_RTI(up_ap->a_rt_i);
	up_stp = gp->up_stp;
	RT_CK_SOLTAB(up_stp);
	submodel = (struct submodel_specific *)up_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	/* Take the first containing region */
	up_reg = (struct region *)BU_PTBL_GET(&(up_stp->st_regions), 0);
	RT_CK_REGION(up_reg);

	/* Need to tackle this honestly --
	 * build a totally new segment list,
	 * with normals, uv, and curvature already computed,
	 * and converted back into up_model coordinate system.
	 * So the lazy-evaluation routines don't have to do anything.
	 * This is probably almost as cheap as the coordinate
	 * re-mapping in the special hook routines.
	 * Then the submodel can be stacked arbitrarily deep.
	 * Alternatively, submodels can't include submodels.
	 */
#if HONEST_WAY
	for( BU_LIST_FOR( segp, seg, &(segHeadp->l) ) )  {
		struct seg	*up_segp;
		RT_CHECK_SEG(segp);

		/* Construct a completely new segment */
		RT_GET_SEG(up_segp, up_ap->a_resource );

		*up_segp = *segp;	/* struct copy */
		up_segp->seg_stp = up_stp;

		/* Adjust for scale difference */
		MAT4XSCALOR( up_segp->seg_in.hit_dist, submodel->subm2m, segp->seg_in.hit_dist);
		up_segp->seg_in.hit_dist -= gp->delta;
		MAT4XSCALOR( up_segp->seg_out.hit_dist, submodel->subm2m, segp->seg_out.hit_dist);
		up_segp->seg_out.hit_dist -= gp->delta;

		/* Link to ray in upper model, not submodel */
		up_segp->seg_in.hit_rayp = &up_ap->a_ray;
		up_segp->seg_out.hit_rayp = &up_ap->a_ray;
		
		/* Pre-calculate what would have been "lazy evaluation" */
		VJOIN1( up_segp->seg_in.hit_point,  up_ap->a_ray.r_pt,
			up_segp->seg_in.hit_dist, up_ap->a_ray.r_dir );
		VJOIN1( up_segp->seg_out.hit_point,  up_ap->a_ray.r_pt,
			up_segp->seg_out.hit_dist, up_ap->a_ray.r_dir );

		/* RT_HIT_NORMAL */
		segp->seg_stp->st_meth->ft_norm( &segp->seg_in,
			segp->seg_stp, segp->seg_in.hit_rayp );
		segp->seg_stp->st_meth->ft_norm( &segp->seg_out,
			segp->seg_stp, segp->seg_out.hit_rayp );
		MAT4X3VEC( up_segp->seg_in.hit_normal, submodel->subm2m,
			segp->seg_in.hit_normal );
		MAT4X3VEC( up_segp->seg_out.hit_normal, submodel->subm2m,
			segp->seg_out.hit_normal );

		/* RT_HIT_UV */
		{
			struct uvcoord	uv;
			RT_HIT_UVCOORD( ap, segp->seg_stp, &segp->seg_in, &uv );
			up_segp->seg_in.hit_vpriv[X] = uv.uv_u;
			up_segp->seg_in.hit_vpriv[Y] = uv.uv_v;
			if( uv.uv_du >= uv.uv_dv )
				up_segp->seg_in.hit_vpriv[Z] = uv.uv_du;
			else
				up_segp->seg_in.hit_vpriv[Z] = uv.uv_dv;

			RT_HIT_UVCOORD( ap, segp->seg_stp, &segp->seg_out, &uv );
			up_segp->seg_out.hit_vpriv[X] = uv.uv_u;
			up_segp->seg_out.hit_vpriv[Y] = uv.uv_v;
			if( uv.uv_du >= uv.uv_dv )
				up_segp->seg_out.hit_vpriv[Z] = uv.uv_du;
			else
				up_segp->seg_out.hit_vpriv[Z] = uv.uv_dv;
		}

		/* RT_HIT_CURVATURE */
		/* no place to stash curvature data! */

		/* Leave behind a distinctive marker to make sure we're
		 * getting the right stuff back in the hooked functions.
		 */
		up_segp->seg_in.hit_surfno = 17;
		up_segp->seg_out.hit_surfno = 17;

		/* Put this segment on caller's shot routine seglist */
		BU_LIST_INSERT( &(gp->up_seghead->l), &(up_segp->l) );
		count++;
	}
#else
	/* Steal & xform the segment list */
	while( BU_LIST_NON_EMPTY( &(segHeadp->l) ) ) {
		segp = BU_LIST_FIRST( seg, &(segHeadp->l) );
		RT_CHECK_SEG(segp);
		BU_LIST_DEQUEUE( &(segp->l) );

		/* Adjust for scale difference */
		MAT4XSCALOR( segp->seg_in.hit_dist, submodel->subm2m, segp->seg_in.hit_dist);
		segp->seg_in.hit_dist -= gp->delta;
		MAT4XSCALOR( segp->seg_out.hit_dist, submodel->subm2m, segp->seg_out.hit_dist);
		segp->seg_out.hit_dist -= gp->delta;

		/* Link to ray in upper model, not submodel */
		segp->seg_in.hit_rayp = &up_ap->a_ray;
		segp->seg_out.hit_rayp = &up_ap->a_ray;

		/* Put this segment on caller's shot routine seglist */
		BU_LIST_INSERT( &(gp->up_seghead->l), &(segp->l) );
		count++;
	}
#endif
	return count;
}

/*
 *  			R T _ S U B M O D E L _ S H O T
 *  
 *  Intersect a ray with a submodel.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_submodel_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;
	register struct seg *segp;
	CONST struct bn_tol	*tol = &ap->a_rt_i->rti_tol;
	struct application	sub_ap;
	struct submodel_gobetween	gb;
	vect_t			vdiff;
	int			code;
	struct bu_ptbl		*restbl;
	struct resource		*resp;
	int			cpu;

	RT_CK_SOLTAB(stp);
	RT_CK_RTI(ap->a_rt_i);
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	gb.up_ap = ap;
	gb.up_seghead = seghead;
	gb.up_stp = stp;

	sub_ap = *ap;		/* struct copy */
	sub_ap.a_rt_i = submodel->rtip;
	sub_ap.a_hit = rt_submodel_a_hit;
	sub_ap.a_miss = rt_submodel_a_miss;
	sub_ap.a_uptr = (genptr_t)&gb;
	sub_ap.a_purpose = "rt_submodel_shot";

	/*
	 * Obtain the resource structure for this CPU.
	 * No need to semaphore because there is one pointer per cpu already.
	 */
	restbl = &submodel->rtip->rti_resources;	/* a ptbl */
	cpu = ap->a_resource->re_cpu;
	if( !( cpu < BU_PTBL_END(restbl) ) )  {
		bu_log("ptbl_end=%d, ptbl_blen=%d, cpu=%d\n",
			BU_PTBL_END(restbl), restbl->blen, cpu );
	}
	BU_ASSERT( cpu < BU_PTBL_END(restbl) );
	if( (resp = (struct resource *)BU_PTBL_GET(restbl, cpu)) == NULL )  {
		/* First ray for this cpu for this submodel, alloc up */
		BU_GETSTRUCT( resp, resource );
		/* XXX Should be a BU_PTBL_SET() */
		BU_PTBL_GET(restbl, cpu) = (long *)resp;
		rt_init_resource( resp, cpu );
	}
	RT_CK_RESOURCE(resp);
	sub_ap.a_resource = resp;

	/* shootray already computed a_ray.r_min & r_max for us */
	/* Construct the ray in submodel coords. */
	/* Do this in a repeatable way */
	/* Distances differ only by a scale factor of m[15] */
	MAT4X3PNT( sub_ap.a_ray.r_pt, submodel->m2subm, ap->a_ray.r_pt );
	MAT4X3VEC( sub_ap.a_ray.r_dir, submodel->m2subm, ap->a_ray.r_dir );

	/* NOTE: ap->a_ray.r_pt is not the same as rp->r_pt! */
	/* This changes the distances */
	VSUB2( vdiff, rp->r_pt, ap->a_ray.r_pt );
	gb.delta = VDOT( vdiff, ap->a_ray.r_dir );

	/* set flag indicating not to bother with boolean computation. */
/* XXX rt_shootray() doesn't pay attention yet. */
	submodel->rtip->rti_nobool = 1;

	code = rt_shootray( &sub_ap );

	if( code <= 0 )  return 0;	/* MISS */

	/* All the real (sneaky) work is done in the hit routine */
	/* a_hit routine will have added the segs to seghead */

	return 1;		/* HIT */
}

#define RT_SUBMODEL_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ S U B M O D E L _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_submodel_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ S U B M O D E L _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_submodel_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	RT_CK_HIT(hitp);

	if( hitp->hit_surfno != 17 )  bu_bomb("rt_submodel surfno mis-match\n");

	/* hitp->hit_point is already valid */
	/* hitp->hit_normal is already valid */
}

/*
 *			R T _ S U B M O D E L _ C U R V E
 *
 *  Return the curvature of the submodel.
 */
void
rt_submodel_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );

	/* XXX This will never be called */
	bu_log("rt_submodel_curve() not implemented, need extra fields in 'struct hit'\n");
}

/*
 *  			R T _ S U B M O D E L _ U V
 *  
 *  For a hit on the surface of an submodel, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_submodel_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	RT_CK_HIT(hitp);

	if( hitp->hit_surfno != 17 )  bu_bomb("rt_submodel surfno mis-match\n");

	uvp->uv_u = hitp->hit_vpriv[X];
	uvp->uv_v = hitp->hit_vpriv[Y];
	uvp->uv_du = uvp->uv_dv = hitp->hit_vpriv[Z];
}

/*
 *		R T _ S U B M O D E L _ F R E E
 */
void
rt_submodel_free( stp )
register struct soltab *stp;
{
	register struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;

	rt_free_rti( submodel->rtip );

	bu_free( (genptr_t)submodel, "submodel_specific" );
}

/*
 *			R T _ S U B M O D E L _ C L A S S
 */
int
rt_submodel_class( stp, min, max, tol )
CONST struct soltab    *stp;
CONST vect_t		min, max;
CONST struct bn_tol    *tol;
{
	return RT_CLASSIFY_UNIMPLEMENTED;
}

struct goodies {
	struct db_i		*dbip;
	struct bu_list		*vheadp;
};

/*
 *			R T _ S U B M O D E L _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 *  This routine should be generally exported for other uses.
 */
HIDDEN union tree *rt_submodel_wireframe_leaf( tsp, pathp, ep, id, client_data )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct bu_external	*ep;
int			id;
genptr_t		client_data;
{
	struct rt_db_internal	intern;
	union tree	*curtree;
	struct goodies	*gp;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);

	gp = (struct goodies *)tsp->ts_m;	/* hack */
	RT_CK_DBI(gp->dbip);

	/* NON-PARALLEL access to vlist pointed to by vheadp is not semaphored */
	if(bu_is_parallel()) bu_bomb("rt_submodel_wireframe_leaf() non-parallel code\n");

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);

		bu_log("rt_submodel_wireframe_leaf(%s) path=%s\n",
			rt_functab[id].ft_name, sofar );
		bu_free((genptr_t)sofar, "path string");
	}

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, tsp->ts_mat, gp->dbip ) < 0 )  {
		bu_log("rt_submodel_wireframe_leaf(%s): %s solid import failure\n",
			rt_functab[id].ft_name,
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );

		if( intern.idb_ptr )  intern.idb_meth->ft_ifree( &intern );
		return(TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[id].ft_plot(
	    gp->vheadp,
	    &intern,
	    tsp->ts_ttol, tsp->ts_tol ) < 0 )  {
		bu_log("rt_submodel_wireframe_leaf(%s): %s plot failure\n",
			rt_functab[id].ft_name,
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );

		rt_functab[id].ft_ifree( &intern );
		return(TREE_NULL);		/* ERROR */
	}

	intern.idb_meth->ft_ifree( &intern );

	/* Indicate success by returning something other than TREE_NULL */
	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;

	return( curtree );
}

/*
 *			R T _ S U B M O D E L _ P L O T
 *
 *  Not unlike mged/dodraw.c drawtrees()
 *
 *  Note:  The submodel will be drawn entirely in one color
 *  (for mged, typically this is red),
 *  because we can't return a vlblock, only one vlist,
 *  which by definition, is all one color.
 */
int
rt_submodel_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_submodel_internal	*sip;
	struct db_tree_state	state;
	struct db_i		*dbip;
	int			ret;
	char			*argv[2];
	struct goodies		good;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);

	BU_LIST_INIT( vhead );

	state = rt_initial_tree_state;	/* struct copy */
	state.ts_ttol = ttol;
	state.ts_tol = tol;
	bn_mat_copy( state.ts_mat, sip->root2leaf );

	state.ts_m = (struct model **)&good;	/* hack -- passthrough to rt_submodel_wireframe_leaf() */
	good.vheadp = vhead;

	if( sip->file[0] != '\0' )  {
		/* db_open will cache dbip's via bu_open_mapped_file() */
		if( (good.dbip = db_open( sip->file, "r" )) == DBI_NULL )  {
			bu_log("rt_submodel_plot() db_open(%s) failure\n", sip->file);
			return -1;
		}
		if( !db_is_directory_non_empty(good.dbip) )  {
			/* This is first open of this database, build directory */
			if( db_scan( good.dbip, (int (*)())db_diradd, 1, NULL ) < 0 )  {
				bu_log("rt_submodel_plot() db_scan() failure\n");
				db_close(good.dbip);
				return -1;
			}
		}
	} else {
		/* Make another use of the current database */
		RT_CK_DBI(sip->dbip);
		good.dbip = (struct db_i *)sip->dbip;	/* un-CONST-cast */
		/* good.dbip->dbi_uses++; */	/* don't close it either */
	}

	argv[0] = sip->treetop;
	argv[1] = NULL;
	ret = db_walk_tree( good.dbip, 1, (CONST char **)argv,
		1,
		&state,
		0,			/* take all regions */
		NULL,			/* rt_submodel_wireframe_region_end */
		rt_submodel_wireframe_leaf,
		(genptr_t)NULL );

	if( ret < 0 )  bu_log("rt_submodel_plot() db_walk_tree(%s) failure\n", sip->treetop);
	if( sip->file[0] != '\0' )
		db_close(good.dbip);
	return ret;
}

/*
 *			R T _ S U B M O D E L _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_submodel_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_submodel_internal	*sip;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);

	return(-1);
}

/*
 *			R T _ S U B M O D E L _ I M P O R T
 *
 *  Import an SUBMODEL from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_submodel_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_submodel_internal	*sip;
	union record			*rp;
	struct bu_vls		str;

	BU_CK_EXTERNAL( ep );
	RT_CK_DBI(dbip);

	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_STRSOL )  {
		bu_log("rt_submodel_import: defective strsol record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_SUBMODEL;
	ip->idb_meth = &rt_functab[ID_SUBMODEL];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_submodel_internal), "rt_submodel_internal");
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;
	sip->dbip = dbip;

	bn_mat_copy( sip->root2leaf, mat );

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
#if 0
bu_log("rt_submodel_import: '%s'\n", rp->ss.ss_args);
#endif
	if( bu_struct_parse( &str, rt_submodel_parse, (char *)sip ) < 0 )  {
		bu_vls_free( &str );
fail:
		bu_free( (char *)sip , "rt_submodel_import: sip" );
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -2;
	}
	bu_vls_free( &str );

	/* Check for reasonable values */
	if( sip->treetop[0] == '\0' )  {
		bu_log("rt_submodel_import() treetop= must be specified\n");
		goto fail;
	}
#if 0
bu_log("import: file='%s', treetop='%s', meth=%d\n", sip->file, sip->treetop, sip->meth);
bn_mat_print("root2leaf", sip->root2leaf );
#endif

	return(0);			/* OK */
}

/*
 *			R T _ S U B M O D E L _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_submodel_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_submodel_internal	*sip;
	union record		*rec;
	struct bu_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SUBMODEL )  return(-1);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);
#if 0
bu_log("export: file='%s', treetop='%s', meth=%d\n", sip->file, sip->treetop, sip->meth);
#endif

	/* Ignores scale factor */
	BU_ASSERT( local2mm == 1.0 );

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
	ep->ext_buf = bu_calloc( 1, ep->ext_nbytes, "submodel external");
	rec = (union record *)ep->ext_buf;

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_submodel_parse, (char *)sip );

	rec->ss.ss_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "submodel", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN-1 );
	bu_vls_free( &str );
#if 0
bu_log("rt_submodel_export: '%s'\n", rec->ss.ss_args);
#endif

	return(0);
}

/*
 *			R T _ S U B M O D E L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_submodel_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_submodel_internal	*sip =
		(struct rt_submodel_internal *)ip->idb_ptr;

	RT_SUBMODEL_CK_MAGIC(sip);
	bu_vls_strcat( str, "instanced submodel (SUBMODEL)\n");

	bu_vls_printf(str, "\tfile='%s', treetop='%s', meth=%d\n",
		sip->file,
		sip->treetop,
		sip->meth );

	return(0);
}

/*
 *			R T _ S U B M O D E L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_submodel_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_submodel_internal	*sip;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);
	sip->magic = 0;			/* sanity */

	bu_free( (genptr_t)sip, "submodel ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


#if !HONEST_WAY

/* ============================================================ */
/*								*/
/*	Hooks applied to all solids in the submodel,		*/
/*	to intermediate between model and submodel coords.	*/
/*								*/
/* ============================================================ */

/*
 *  			R T _ S U B M O D E L H O O K _ P R E P
 */
int
rt_submodelhook_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	bu_bomb("rt_submodelhook_prep()\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ P R I N T
 */
void
rt_submodelhook_print( stp )
register CONST struct soltab *stp;
{
	rt_functab[stp->st_id].ft_print(stp);
}

/*
 *  			R T _ S U B M O D E L H O O K _ S H O T
 *  
 *  Intersect a ray with an object in a submodel.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_submodelhook_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	return rt_functab[stp->st_id].ft_shot( stp, rp, ap, seghead );
}

/*
 *			R T _ S U B M O D E L H O O K _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_submodelhook_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	bu_bomb("rt_submodelhook_vshot\n");
}

/*
 *  			R T _ S U B M O D E L H O O K _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 *
 *  Pass the original ray through the world-to-submodel transform,
 *  then send the normal back through the inverse transform.
 */
void
rt_submodelhook_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	struct rt_i	*sub_rtip;
	struct soltab	*upper_stp;	/* The submodel "solid" */
	struct xray	sub_xray;
	vect_t		sub_norm;
	register struct submodel_specific *submodel;
	struct hit	sub_hit;

	sub_rtip = stp->st_rtip;
	RT_CK_RTI(sub_rtip);

	/* XXX There should be more than one of these */
	upper_stp = sub_rtip->rti_up;
	RT_CK_SOLTAB(upper_stp);
/* XXX Really need some other way to get upper_stp */

	submodel = (struct submodel_specific *)upper_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	/* Reconstruct the ray in submodel coords */
	MAT4X3PNT( sub_xray.r_pt, submodel->m2subm, rp->r_pt );
	MAT4X3VEC( sub_xray.r_dir, submodel->m2subm, rp->r_dir );

	/* Reconstruct the hit in submodel coords */
	sub_hit = *hitp;		/* struct copy */
	MAT4XSCALOR( sub_hit.hit_dist, submodel->m2subm, hitp->hit_dist );
	VJOIN1( hitp->hit_point, sub_xray.r_pt, sub_hit.hit_dist, sub_xray.r_dir );
	hitp->hit_rayp = &sub_xray;

	rt_functab[stp->st_id].ft_norm( &sub_hit, stp, &sub_xray );

	/* Convert the normal back to model coords */
	MAT4X3VEC( hitp->hit_normal, submodel->subm2m, sub_hit.hit_normal );
}

/*
 *			R T _ S U B M O D E L H O O K _ C U R V E
 *
 *  Return the curvature of the submodel solid.
 */
void
rt_submodelhook_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	struct rt_i	*sub_rtip;
	struct soltab	*upper_stp;	/* The submodel "solid" */
	struct xray	sub_xray;
	struct hit	sub_hit;
	vect_t		sub_pdir;
	register struct submodel_specific *submodel;
/* XXX How do we get original ray??? */
struct xray *rp = (struct xray *)NULL;
bu_bomb("rt_submodelhook_curve() -- no rp?\n");

	sub_rtip = stp->st_rtip;
	RT_CK_RTI(sub_rtip);

	/* XXX There should be more than one of these */
	upper_stp = sub_rtip->rti_up;
	RT_CK_SOLTAB(upper_stp);
	submodel = (struct submodel_specific *)upper_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	/* Reconstruct the ray in submodel coords */
	sub_xray.magic = RT_RAY_MAGIC;
	sub_xray.index = -42;
	MAT4X3PNT( sub_xray.r_pt, submodel->m2subm, rp->r_pt );
	MAT4X3VEC( sub_xray.r_dir, submodel->m2subm, rp->r_dir );

	/*
	 * Convert the dist, point, & normal back to submodel coords.
	 */
	sub_hit = *hitp;		/* struct copy */
	/* Rescale the distance */
	MAT4XSCALOR( sub_hit.hit_dist, submodel->m2subm, hitp->hit_dist );
	/* Recompute the hit point */
	VJOIN1( sub_hit.hit_point, sub_xray.r_pt, sub_hit.hit_dist, sub_xray.r_dir );
	/* Convert the surface normal */
	MAT4X3VEC( sub_hit.hit_normal, submodel->m2subm, hitp->hit_normal );
	sub_hit.hit_rayp = &sub_xray;

	rt_functab[stp->st_id].ft_curve( cvp, &sub_hit, stp );

	/* Convert the pdir back to model coords */
	VMOVE( sub_pdir, cvp->crv_pdir );
	MAT4X3VEC( cvp->crv_pdir, submodel->subm2m, sub_pdir );

	/* The curvature numbers don't change */
}

/*
 *  			R T _ S U B M O D E L H O O K _ U V
 *  
 *  For a hit on the surface of an submodelhook, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_submodelhook_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	struct rt_i	*sub_rtip;
	struct soltab	*upper_stp;	/* The submodel "solid" */
	struct hit	sub_hit;
	register struct submodel_specific *submodel;
	struct application	sub_ap;

	sub_rtip = stp->st_rtip;
	RT_CK_RTI(sub_rtip);
	upper_stp = sub_rtip->rti_up;
	RT_CK_SOLTAB(upper_stp);
	submodel = (struct submodel_specific *)upper_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	/* Have to convert application struct, for "ray footprint" stuff */
	sub_ap = *ap;			/* struct copy */

	/* Reconstruct the ray in submodel coords */
	sub_ap.a_ray.magic = RT_RAY_MAGIC;
	sub_ap.a_ray.index = -42;
	MAT4X3PNT( sub_ap.a_ray.r_pt, submodel->m2subm, ap->a_ray.r_pt );
	MAT4X3VEC( sub_ap.a_ray.r_dir, submodel->m2subm, ap->a_ray.r_dir );

	/*
	 * Convert the dist, point, & normal back to submodel coords.
	 */
	sub_hit = *hitp;		/* struct copy */
	/* Rescale the distance */
	MAT4XSCALOR( sub_hit.hit_dist, submodel->m2subm, hitp->hit_dist );
	/* Recompute the hit point */
	VJOIN1( sub_hit.hit_point, sub_ap.a_ray.r_pt, sub_hit.hit_dist, sub_ap.a_ray.r_dir );
	/* Convert the surface normal */
	MAT4X3VEC( sub_hit.hit_normal, submodel->m2subm, hitp->hit_normal );
	sub_hit.hit_rayp = &sub_ap.a_ray;

	rt_functab[stp->st_id].ft_uv( ap, stp, &sub_hit, uvp );

	/* No transformation of u and v values are necessary */
}

/*
 *		R T _ S U B M O D E L H O O K _ F R E E
 */
void
rt_submodelhook_free( stp )
register struct soltab *stp;
{
	rt_functab[stp->st_id].ft_free( stp );
}

/*
 *			R T _ S U B M O D E L H O O K _ C L A S S
 */
int
rt_submodelhook_class( stp, min, max, tol )
CONST struct soltab    *stp;
CONST vect_t		min, max;
CONST struct bn_tol    *tol;
{
	struct rt_i	*sub_rtip;
	struct soltab	*upper_stp;	/* The submodel "solid" */
	register struct submodel_specific *submodel;
	point_t		sub_min, sub_max;
	struct bn_tol	sub_tol;

	sub_rtip = stp->st_rtip;
	RT_CK_RTI(sub_rtip);
	upper_stp = sub_rtip->rti_up;
	RT_CK_SOLTAB(upper_stp);
	submodel = (struct submodel_specific *)upper_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);
	
	/* Transform min and max points */
	MAT4X3PNT( sub_min, submodel->m2subm, min );
	MAT4X3PNT( sub_max, submodel->m2subm, max );

	/* Scale tol */
	sub_tol.magic = tol->magic;
	MAT4XSCALOR(sub_tol.dist, submodel->m2subm, tol->dist );
	sub_tol.dist_sq = sub_tol.dist * sub_tol.dist;
	sub_tol.perp = sub_tol.perp;
	sub_tol.para = sub_tol.para;

	return rt_functab[stp->st_id].ft_classify( stp, sub_min, sub_max, &sub_tol );
}

/*
 *			R T _ S U B M O D E L H O O K _ P L O T
 */
int
rt_submodelhook_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	bu_bomb("rt_submodelhook_plot\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_submodelhook_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
{
	bu_bomb("rt_submodelhook_tess\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ I M P O R T
 *
 */
int
rt_submodelhook_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	bu_bomb("rt_submodelhook_import\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ E X P O R T
 */
int
rt_submodelhook_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	bu_bomb("rt_submodelhook_export\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_submodelhook_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	bu_bomb("rt_submodelhook_describe\n");
	/* NOTREACHED */
}

/*
 *			R T _ S U B M O D E L H O O K _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_submodelhook_ifree( ip )
struct rt_db_internal	*ip;
{
	rt_functab[ip->idb_type].ft_ifree( ip );
}

/*
 *			R T _ S U B M O D E L H O O K _ X F O R M
 *
 *  Create transformed version of internal form.  Free *ip if requested.
 *  Implement this if it's faster than doing an export/import cycle.
 */
int
rt_submodelhook_xform( op, mat, ip, free )
struct rt_db_internal	*op;
CONST mat_t		mat;
struct rt_db_internal	*ip;
int			free;
{
	bu_bomb("rt_submodelhook_xform\n");
	/* NOTREACHED */
}

CONST struct rt_functab rt_submodelhook_functab = {
	RT_FUNCTAB_MAGIC, "ID_SUBMODELHOOK", "submdhk",
		0,		/* hooks only */
		rt_submodelhook_prep,	rt_submodelhook_shot,	rt_submodelhook_print,	rt_submodelhook_norm,
		rt_submodelhook_uv,		rt_submodelhook_curve,	rt_submodelhook_class,	rt_submodelhook_free,
		rt_submodelhook_plot,	NULL,		rt_submodelhook_tess,	NULL,
		rt_submodelhook_import,	rt_submodelhook_export,	rt_submodelhook_ifree,
		rt_submodelhook_describe,	rt_submodelhook_xform,	NULL,
		0, 0,
/* 		rt_parsetab_tclget, rt_parsetab_tcladjust, rt_parsetab_tclform, */
		NULL, NULL, NULL,
		NULL,
};

#endif /* !HONEST_WAY */
