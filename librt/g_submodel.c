/*
 *			G _ S U B M O D E L . C
 *
 *  Purpose -
 *	Intersect a ray with an entire subspace full of geometry,
 *	included from another .g file, as a subordinate
 *	instance of LIBRT.
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_submodel_internal --- parameters for solid
 *	define submodel_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_submodel_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_SUBMODEL, increment ID_MAXIMUM
 *	edit Cakefile to add g_submodel.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_submodel() routine
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
 *	This software is Copyright (C) 1998 by the United States Army.
 *	All rights reserved.
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
#include "./debug.h"

/* parameters for solid, internal representation
 * This goes in rtgeom.h
 */
/* parameters for solid, internal representation */
struct rt_submodel_internal {
	long	magic;
	char	file[128];	/* .g filename, 0-len --> this database. */
	char	treetop[128];	/* one treetop only */
	int	meth;		/* space partitioning method */
	/* other option flags (lazy prep, etc.)?? */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	mat_t	root2leaf;
};
#define RT_SUBMODEL_INTERNAL_MAGIC	0x7375626d	/* subm */
#define RT_SUBMODEL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_SUBMODEL_INTERNAL_MAGIC,"rt_submodel_internal")

#define RT_SUBMODEL_O(m)	offsetof(struct rt_submodel_internal, m)

struct bu_structparse rt_submodel_parse[] = {
	{"%s",	128, "file",	offsetofarray(struct rt_submodel_internal, file), FUNC_NULL },
	{"%s",	128, "treetop", offsetofarray(struct rt_submodel_internal, treetop), FUNC_NULL },
	{"%d",	1, "meth",	RT_SUBMODEL_O(meth),		FUNC_NULL },
	{"",	0, (char *)0, 0,			FUNC_NULL }
};



/* ray tracing form of solid, including precomputed terms */
struct submodel_specific {
	long		magic;
	mat_t		subm2m;		/* To transform normals back out to model coords */
	mat_t		m2subm;		/* To transform rays into local coord sys */
	struct rt_i	*rtip;		/* sub model */
};
#define RT_SUBMODEL_SPECIFIC_MAGIC	0x73756253	/* subS */
#define RT_CK_SUBMODEL_SPECIFIC(_p)	RT_CKMAG(_p,RT_SUBMODEL_SPECIFIC_MAGIC,"submodel_specific")

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
	vect_t	radvec;
	vect_t	diam;
	char	*argv[2];

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);

	if( sip->file[0] == '\0' )  {
		/* No .g file name given, tree is in current .g file */
		sub_rtip = rt_new_rti( rtip->rti_dbip );
	} else {
		/* XXX Might want to cache & reuse dbip's, to save dirbuilds */
		sub_rtip = rt_dirbuild( sip->file, NULL, 0 );
	}
	if( sub_rtip == RTI_NULL )  return -1;

	/* Propagate some important settings downward */
	sub_rtip->useair = rtip->useair;
	sub_rtip->rti_dont_instance = rtip->rti_dont_instance;
	sub_rtip->rti_hasty_prep = rtip->rti_hasty_prep;
	sub_rtip->rti_tol = rtip->rti_tol;	/* struct copy */
	sub_rtip->rti_ttol = rtip->rti_ttol;	/* struct copy */

	if( sip->meth )  {
		sub_rtip->rti_space_partition = sip->meth;
	} else {
		sub_rtip->rti_space_partition = rtip->rti_space_partition;
	}

	rt_bomb("XXX Gak, rt_gettrees can't be used recursively\n");
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

	/* OK, it's going to work. */

	/* Register ourselves with containing rtip, for linked
	 * preps, frees, etc.
	 */
#if 0
	bu_ptbl_insert_unique( &rtip->rti_sub_rtip, (long *)sub_rtip );
	sub_rtip->rti_up = (genptr_t)stp;
#endif
	
	BU_GETSTRUCT( submodel, submodel_specific );
	stp->st_specific = (genptr_t)submodel;

	mat_copy( submodel->subm2m, sip->root2leaf );
	mat_inv( submodel->m2subm, sip->root2leaf );
	submodel->rtip = sub_rtip;

	/* Propagage submodel bounding box back upwards, rotated&scaled. */
	bn_rotate_bbox( stp->st_min, stp->st_max,
		submodel->subm2m,
		sub_rtip->mdl_min, sub_rtip->mdl_max );

	VSUB2( diam, stp->st_max, stp->st_min );
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSCALE( radvec, diam, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

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
}

/*
 */
rt_submodel_normal_hook( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	struct rt_i	*sub_rtip;
	struct submodel_specific	*submodel;
	struct soltab	*up_stp;
	vect_t		norm;

#if 0
	sub_rtip = stp->st_rtip;
	up_stp = (struct soltab *)sub_rtip->rti_up;
	RT_CK_SOLTAB(up_stp);
	submodel = (struct submodel_specific *)up_stp->st_specific;
	RT_CK_SUBMODEL_SPECIFIC(submodel);

	rt_functab[stp->st_id].ft_norm(hitp, stp, rayp);
	/* XXX This can't be done more than once per hit,
	 * as it corrupts the hit_normal field.
	 * We have no control over whether application will want more. :-(
	 * The TCL and NIRT interfaces certainly will...
	 */
	VMOVE( norm, hitp->hit_normal );
	MAT4X3VEC( hitp->hit_normal, submodel->subm2m, norm );
#endif

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
	register struct application	*oap =
		(struct application *)ap->a_uptr;
	struct seg		*segp;

	RT_CK_PT_HD(PartHeadp);
	RT_AP_CK(oap);
	RT_CK_RTI(oap->a_rt_i);

	/* Steal & xform the segment list */
	while( BU_LIST_NON_EMPTY( &(segHeadp->l) ) ) {
		segp = BU_LIST_FIRST( seg, &(segHeadp->l) );
		RT_CHECK_SEG(segp);
		BU_LIST_DEQUEUE( &(segp->l) );
#if 0
		BU_LIST_INSERT( oap->a_finished_segs_p, &(segp->l) );
#endif

		/* Adjust for new start point & matrix scaling */
		segp->seg_in.hit_dist = oap->a_ray.r_min +
			segp->seg_in.hit_dist * ap->a_dist;
		segp->seg_out.hit_dist = oap->a_ray.r_min +
			segp->seg_out.hit_dist * ap->a_dist;

		/* Need to set hook to catch norm and curve ops,
		 * and to revector them here, so that xforms can be done.
		 */

	}

	/* Steal the partition list */
	/* ??? Do these need to be sort/merged into the Final list? */
	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
		RT_CK_PT(pp);
		DEQUEUE_PT(pp);
#if 0
		INSERT_PT( pp, oap->a_Final_Part_p );
#endif
	}
	return 1;
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
	CONST struct rt_tol	*tol = &ap->a_rt_i->rti_tol;
	struct application	nap;
	point_t			startpt;

	RT_CK_SUBMODEL_SPECIFIC(submodel);

	nap = *ap;		/* struct copy */
	nap.a_rt_i = submodel->rtip;
	nap.a_hit = rt_submodel_a_hit;
	nap.a_miss = rt_submodel_a_miss;
	nap.a_uptr = (genptr_t)ap;
	nap.a_dist = submodel->m2subm[15];	/* scale, local to world */

	/* shootray already computed a_ray.r_min & r_max for us */
	/* Make starting point reasonable once in local coord system */
	VJOIN1( startpt, ap->a_ray.r_pt, ap->a_ray.r_min, ap->a_ray.r_dir );
	MAT4X3PNT( nap.a_ray.r_pt, submodel->m2subm, startpt );
	MAT4X3VEC( nap.a_ray.r_dir, submodel->m2subm, ap->a_ray.r_dir );

	if( rt_shootray( &nap ) <= 0 )  return 0;	/* MISS */

	/* All the real (sneaky) work is done in the hit routine */

	/* Trick:  We added nothing to seghead, but we signal hit */
	/* (If a_onehit is set, we may need to cons up a dummy hit) */
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
	register struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	/* XXX This will never be called */
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
	register struct submodel_specific *submodel =
		(struct submodel_specific *)stp->st_specific;

	/* XXX This will never be called */
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
	struct db_i	*dbip;
	struct bu_list	*vheadp;
};

/*
 *			R T _ S U B M O D E L _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 *  This routine should be generally exported for other uses.
 */
HIDDEN union tree *rt_submodel_wireframe_leaf( tsp, pathp, ep, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct bu_external	*ep;
int			id;
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

		if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
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

	rt_functab[id].ft_ifree( &intern );

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

	state.ts_m = (struct model **)&good;	/* hack */
	good.vheadp = vhead;

	/* Any good way to cache good.dbip's ? */
	if( (good.dbip = db_open( sip->file, "r" )) == DBI_NULL )
		return -1;
	if( db_scan( good.dbip, (int (*)())db_diradd, 1 ) < 0 )  {
		db_close(good.dbip);
		return -1;
	}

	argv[0] = sip->treetop;
	argv[1] = NULL;
	ret = db_walk_tree( good.dbip, 1, (CONST char **)argv,
		1,
		&state,
		0,			/* take all regions */
		NULL,			/* rt_submodel_wireframe_region_end */
		rt_submodel_wireframe_leaf );

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
CONST struct rt_tol	*tol;
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
CONST struct rt_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_submodel_internal	*sip;
	union record			*rp;
	struct bu_vls		str;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_STRSOL )  {
		rt_log("rt_submodel_import: defective strsol record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_SUBMODEL;
	ip->idb_ptr = bu_malloc( sizeof(struct rt_submodel_internal), "rt_submodel_internal");
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if( bu_struct_parse( &str, rt_submodel_parse, (char *)sip ) < 0 )  {
		bu_vls_free( &str );
fail:
		rt_free( (char *)sip , "rt_submodel_import: sip" );
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

	return(0);			/* OK */
}

/*
 *			R T _ S U B M O D E L _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_submodel_export( ep, ip, local2mm, dbip )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_submodel_internal	*sip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SUBMODEL )  return(-1);
	sip = (struct rt_submodel_internal *)ip->idb_ptr;
	RT_SUBMODEL_CK_MAGIC(sip);

bu_bomb("rt_submodel_export()\n");

#if 0
	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "submodel external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = SUBMODEL;	/* GED primitive type from db.h */

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */


	/* convert from local editing units to mm and export
	 * to database record format
	 *
	 * Warning: type conversion: double to float
	 */
	VSCALE( &rec->s.s_values[0], sip->submodel_V, local2mm );
	rec->s.s_values[3] = sip->submodel_radius * local2mm;
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
	rt_vls_strcat( str, "instanced submodel (SUBMODEL)\n");

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
