#define DEBUG	0
/*
 *			G _ N M G . C
 *
 *  Purpose -
 *	Intersect a ray with a 
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
static char RCSnmg[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

/* rt_nmg_internal is just "model", from nmg.h */

struct hitlist {
	struct rt_list	l;
	struct hit	hit;
};

struct nmg_specific {
	int		nmg_smagic;
	vect_t		nmg_V;	/* */
	struct model	*nmg_model;
	vect_t		nmg_invdir;
	int		nmg_emagic;
};
#define	G_NMG_START_MAGIC	6014061
#define	G_NMG_END_MAGIC		7013061


/*
 *  			R T _ N M G _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid NMG, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	NMG is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct nmg_specific is created, and it's address is stored in
 *  	stp->st_specific for use by nmg_shot().
 */
int
rt_nmg_prep( stp, ip, rtip, tol )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
CONST struct rt_tol	*tol;
{
	struct model		*m;
	register struct nmg_specific	*nmg;
	struct nmgregion *rp;
	vect_t work;	

	rt_g.NMG_debug |= DEBUG_NMGRT;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	GETSTRUCT( nmg, nmg_specific );
	stp->st_specific = (genptr_t)nmg;
	nmg->nmg_model = m;
	ip->idb_ptr = (genptr_t)NULL;
	nmg->nmg_smagic = G_NMG_START_MAGIC;
	nmg->nmg_emagic = G_NMG_END_MAGIC;

	/* Get Bounding box of solid */
	VSETALL(stp->st_min, MAX_FASTF);
	VSETALL(stp->st_max, -MAX_FASTF);

	/* the model bounding box is an amalgam of the 
	 * nmgregion bounding boxes.
	 */
	for (RT_LIST_FOR(rp, nmgregion, &m->r_hd )) {
		NMG_CK_REGION(rp);
		NMG_CK_REGION_A(rp->ra_p);

		VMINMAX(stp->st_min, stp->st_max, rp->ra_p->min_pt);
		VMINMAX(stp->st_min, stp->st_max, rp->ra_p->max_pt);
	}

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE(work);

	nmg_manifolds(m);

	return(0);
}

/*
 *			R T _ N M G _ P R I N T
 */
void
rt_nmg_print( stp )
register struct soltab *stp;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;
}

/*
 *			P L O T _ R A Y _ F A C E
 */
static void
plot_ray_face(pt, dir, fu)
point_t pt;
vect_t dir;
struct faceuse *fu;
{
	FILE *fd;
	long *b;
	point_t pp;
	static int i=0;
	char name[32];

	if ( ! (rt_g.NMG_debug & DEBUG_NMGRT) )
		return;

	sprintf(name, "lee%0d.pl", i++);
	if ((fd = fopen(name, "w")) == (FILE *)NULL) {
		rt_log("plot_ray_face cannot open %s", name);
		rt_bomb("aborting");
	}


	b = (long *)rt_calloc( fu->s_p->r_p->m_p->maxindex, sizeof(long), "bit vec");

	nmg_pl_fu(fd, fu, b, 200, 200, 200);

	rt_free((char *)b, "bit vec");

	VSCALE(pp, dir, 1000.0);
	VADD2(pp, pt, pp);
	pdv_3line( fd, pt, pp );
}


/*
 *
 *
 *	if we hit a 3 manifold for seg_in we enter the solid
 *	if we hit a 3 manifold for seg_out we leave the solid
 *	if we hit a 0,1or2 manifold for seg_in, we enter/leave the solid.
 *	if we hit a 0,1or2 manifold for seg_out, we ignore it.
 */
static int
vertex_hit(v_p, seg_p, rp, tbl, a_hit, filled)
struct vertex	*v_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitlist	*a_hit;
int		filled;
{
	char manifolds = NMG_MANIFOLDS(tbl, v_p);

	if (manifolds & NMG_3MANIFOLD) {
	} else if (manifolds & NMG_2MANIFOLD && filled == 0) {
	} else if (manifolds & NMG_1MANIFOLD && filled == 0) {
	} else if (manifolds & NMG_0MANIFOLD ( {
	    if (filled == 0) {
		/* we've hit a lone vertex */
		bcopy(&a_hit->hit, seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, seg_p->seg_out, sizeof(struct hit));
		VREVERSE(seg_p->seg_in.hit_normal, rp->r_dir);
		VMOVE(seg_p->seg_out.hit_normal, rp->r_dir);

		filled = 2;
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");
	}
	return(filled);
}

/*
 *
 *	if we hit a 3 manifold for seg_in we enter the solid
 *	if we hit a 3 manifold for seg_out we leave the solid
 *	if we hit a 1or2 manifold for seg_in, we enter/leave the solid.
 *	if we hit a 1or2 manifold for seg_out, we ignore it.
 */
static int
edge_hit(e_p, seg_p, rp, tbl, a_hit, filled)
struct edge	*e_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitlist	*a_hit;
int		filled;
{
	struct edgeuse *eu_p;
	char manifolds = NMG_MANIFOLDS(tbl, e_p);
	

	if (manifolds & NMG_3MANIFOLD) {
	} else if (manifolds & NMG_2MANIFOLD) {
	    if (filled == 0) {
	    	/* hit a 2-manifold in space ( dangling face ) */


		bcopy(&a_hit->hit, seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, seg_p->seg_out, sizeof(struct hit));

	    	eu_p = e_p->eu_p;

	    	/* go looking for a use of this edge in a 2-Manifold */

	    	while ( ! (NMG_MANIFOLDS(tbl, eu_p) & NMG_2MANIFOLD) &&
		    eu_p->radial_p->eumate_p != e_p->eu_p)
			eu_p = eu_p->radial_p->eumate_p;

		VMOVE(seg_p->seg_in.hit_normal, eu_p->up.lu_p->up.fu_p->f_p->fg_p->N);
	    	if (VDOT(seg_p->seg_in.hit_normal, rp->r_dir) > 0.0) {
	    		VMOVE(seg_p->seg_out.hit_normal,
	    			seg_p->seg_in.hit_normal);
	    		VREVERSE(seg_p->seg_in.hit_normal,
	    			seg_p->seg_in.hit_normal);
	    	} else {
	    		VREVERSE(seg_p->seg_out.hit_normal,
	    			seg_p->seg_in.hit_normal);
	    	}
	    	
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");
	} else if (manifolds & NMG_1MANIFOLD) {
	    if (filled == 0) {
		vect_t eray;

		/* we've got a wire edge.
		 *
		 * Generate a normal for the edge which is perpendicular to
		 * the edge in the plane formed by the ray and the edge.
		 *
		 */

		bcopy(&a_hit->hit, seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, seg_p->seg_out, sizeof(struct hit));

		/* make the normal for the ray
		 *
		 * make ray of edge
		 */
		VSUB2(eray, e_p->eu_p->vu_p->v_p->vg_p->coord,
			e_p->eu_p->eumate_p->vu_p->v_p->vg_p->coord);
		
		/* make N perpendicular to ray and edge */
		VCROSS(seg_p->seg_in.hit_normal, rp->r_dir, eray);

		/* make N point toward ray origin in plane of ray and edge */
		VCROSS(seg_p->seg_in.hit_normal, eray,
			seg_p->seg_in.hit_normal);

		/* reverse normal for out-point */
		VREVERSE(seg_p->seg_out.hit_normal, seg_p->seg_in.hit_normal);

		filled = 2;
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");
	}
	return(filled);
}

/*
 *	if we hit a 3 manifold face for seg_in, we're entering a solid
 *	if we hit a 3 manifold face for seg_out, we're leaving a solid
 *	if we hit a 2 manifold face for seg_in, we're entering/leaving a solid
 *	if we hit a 2 manifold face for seg_out, it's ignored
 */
static int
face_hit(f_p, seg_p, rp, tbl, a_hit, filled)
struct face	*f_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitlist	*a_hit;
int		filled;
{
	char manifolds = NMG_MANIFOLDS(tbl, f_p);

	if (manifolds & NMG_3MANIFOLD) {
		if (filled = 0) {
			/* entering solid */
			bcopy(&a_hit->hit, seg_p->seg_in, sizeof(struct hit));
			VMOVE(seg_p->seg_in.hit_normal, f_p->fg_p->N);
			filled = 1;
		} else {
			/* leaving solid */
			bcopy(&a_hit->hit, seg_p->seg_out, sizeof(struct hit));
			VMOVE(seg_p->seg_out.hit_normal, f_p->fg_p->N);
			filled = 2;
		}

	} else if (filled == 0) {
		/* just hit an exterior dangling face */

		bcopy(&a_hit->hit, seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, seg_p->seg_out, sizeof(struct hit));
		if (VDOT(f_p->fg_p->N, rp->r_dir) <= 0.0) {
			/* face normal points back along ray */
			VMOVE(seg_p->seg_in.hit_normal, f_p->fg_p->N);
			VMOVE(seg_p->seg_out.hit_normal, f_p->fg_p->N);
		} else {
			VREVERSE(seg_p->seg_in.hit_normal, f_p->fg_p->N);
			VREVERSE(seg_p->seg_out.hit_normal, f_p->fg_p->N);
		}

		filled = 2;
	}

	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

	return(filled);
}


static int
build_segs(hl, ap, nmg_spec, seghead, rp)
struct hitlist *hl;
struct application *ap;
struct nmg_specific *nmg_spec;
struct seg		*seghead;	/* intersection w/ ray */
register struct xray	*rp;	/* info about the ray */
{
	struct seg *seg_p;
	int seg_count=0;
	struct hitlist *a_hit;
	char *tbl;
	int hits_filled;

	tbl = nmg_spec->nmg_model->manifolds;

	/* build up the list of segments based upon the hit points.
	 */

	while (RT_LIST_NON_EMPTY(&hl->l) ) {
	    RT_GET_SEG(seg_p, ap->a_resource);

	    hits_filled = 0;

	    while (hits_filled < 2) {
		a_hit = RT_LIST_FIRST(hitlist, &hl->l);

		switch (*(int *)a_hit->hit.hit_private) {
		case NMG_VERTEX_MAGIC:
			hits_filled = vertex_hit(
			(struct vertex *)a_hit->hit.hit_private,
			seg_p, rp, tbl, a_hit, hits_filled);
			break;
		case NMG_EDGE_MAGIC:
			hits_filled = edge_hit(
			(struct edge *)a_hit->hit.hit_private,
			seg_p, rp, tbl, a_hit, hits_filled);
			break;
		case NMG_FACE_MAGIC:
			hits_filled = face_hit(
			(struct face *)a_hit->hit.hit_private,
			seg_p, rp, tbl, a_hit, hits_filled);
			break;
		default: rt_log("bogus topology hit?\n"); abort();
			break;
		}
	    }


	    RT_LIST_APPEND(&seghead->l, &seg_p->l);
	    ++seg_count;
	}
	return(seg_count);
}


/*
 *  			R T _ N M G _ S H O T
 *  
 *  Intersect a ray with a nmg.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_nmg_shot( stp, rp, ap, seghead, tol )
struct soltab		*stp;
register struct xray	*rp;	/* info about the ray */
struct application	*ap;	
struct seg		*seghead;	/* intersection w/ ray */
CONST struct rt_tol	*tol;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;
	struct hitlist *hl, *a_hit, *isect_ray_nmg();
	struct seg *seg_p;
	int seg_count=0;

	if(rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("rt_nmg_shot\n");

	/* check validity of nmg specific structure */
 	if (nmg->nmg_smagic != G_NMG_START_MAGIC)
		rt_bomb("start of NMG st_specific structure corrupted\n");

 	if (nmg->nmg_emagic != G_NMG_END_MAGIC)
		rt_bomb("end of NMG st_specific structure corrupted\n");

	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( rp->r_dir[X], SQRT_SMALL_FASTF ) )  {
		nmg->nmg_invdir[X]=1.0/rp->r_dir[X];
	} else {
		nmg->nmg_invdir[X] = INFINITY;
		rp->r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Y], SQRT_SMALL_FASTF ) )  {
		nmg->nmg_invdir[Y]=1.0/rp->r_dir[Y];
	} else {
		nmg->nmg_invdir[Y] = INFINITY;
		rp->r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Z], SQRT_SMALL_FASTF ) )  {
		nmg->nmg_invdir[Z]=1.0/rp->r_dir[Z];
	} else {
		nmg->nmg_invdir[Z] = INFINITY;
		rp->r_dir[Z] = 0.0;
	}

	hl = isect_ray_nmg(rp, nmg->nmg_invdir, nmg->nmg_model, &tol);

	if (! hl || RT_LIST_IS_EMPTY(&hl->l)) {
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("ray missed NMG\n");
		return(0);			/* MISS */
	}

	if (rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log("\nsorted nmg/ray hit list\n");
		for (RT_LIST_FOR(a_hit, hitlist, &hl->l)) {
			rt_log("ray_hit_distance %g (%g %g %g)",
				a_hit->hit.hit_dist,
				a_hit->hit.hit_point[0],
				a_hit->hit.hit_point[1],
				a_hit->hit.hit_point[2]);

			switch ( *(int*)a_hit->hit.hit_private) {
			case NMG_FACE_MAGIC: rt_log("\tface\n"); break;
			case NMG_EDGE_MAGIC: rt_log("\tedge\n"); break;
			case NMG_VERTEX_MAGIC: rt_log("\tvertex\n"); break;
			default : rt_log(" hit unknown magic (%d)\n", 
				*(int*)a_hit->hit.hit_private); break;
			}
		}
	}

	seg_count = build_segs(hl, ap, nmg, seghead, rp);
	
	if (!(rt_g.NMG_debug & DEBUG_NMGRT))
		return(seg_count);

	/* print debugging data before returning */
	rt_log("segment list\n");
	for (RT_LIST_FOR(seg_p, seg, &seghead->l) ) {
		rt_log("dist %g  pt(%g,%g,%g)  Norm(%g,%g,%g)\n",
		seg_p->seg_in.hit_dist,
		seg_p->seg_in.hit_point[0],
		seg_p->seg_in.hit_point[1],
		seg_p->seg_in.hit_point[2],
		seg_p->seg_in.hit_normal[0],
		seg_p->seg_in.hit_normal[1],
		seg_p->seg_in.hit_normal[2]);
	}

	rt_log("returning\n");

	return(seg_count);
}

#define RT_NMG_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ N M G _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nmg_vshot( stp, rp, segp, n, resp, tol )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
CONST struct rt_tol	*tol;
{
	rt_vstub( stp, rp, segp, n, resp, tol );
}

/*
 *  			R T _ N M G _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nmg_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
/*	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir ); */
}

/*
 *			R T _ N M G _ C U R V E
 *
 *  Return the curvature of the nmg.
 */
void
rt_nmg_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ N M G _ U V
 *  
 *  For a hit on the surface of an nmg, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_nmg_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;
}

/*
 *		R T _ N M G _ F R E E
 */
void
rt_nmg_free( stp )
register struct soltab *stp;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;

	nmg_km( nmg->nmg_model );
	rt_free( (char *)nmg, "nmg_specific" );
}

/*
 *			R T _ N M G _ C L A S S
 */
int
rt_nmg_class()
{
	return(0);
}

/*
 *			N M G _ M _ T O _ V L I S T
 * XXX move to nmg_plot.c
 */
void
nmg_m_to_vlist( vhead, m, poly_markers )
struct rt_list	*vhead;
struct model	*m;
int		poly_markers;
{
	register struct region	*r;

	NMG_CK_MODEL( m );
	for( RT_LIST_FOR( r, region, &m->r_hd ) )  {
		NMG_CK_REGION( r );
		nmg_r_to_vlist( vhead, r, poly_markers );
	}
}

/*
 *			R T _ N M G _ P L O T
 */
int
rt_nmg_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct model	*m;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	nmg_m_to_vlist( vhead, m, 0 );

	return(0);
}

/*
 *			R T _ N M G _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_nmg_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct model	*lm;
	struct nmgregion	*lr;

	NMG_CK_MODEL(m);

	RT_CK_DB_INTERNAL(ip);
	lm = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(lm);

	if( RT_LIST_IS_EMPTY( &(lm->r_hd) ) )  {
		/* No regions in imported geometry, that is OK */
		return(0);
	}

	/* XXX A big hack, just for testing ***/

	*r = RT_LIST_FIRST(nmgregion, &(lm->r_hd) );
	NMG_CK_REGION(*r);
	if( RT_LIST_NEXT_NOT_HEAD( *r, &(lm->r_hd) ) )  {
		rt_log("rt_nmg_tess: WARNING, disk record contains more than 1 region, you probably won't get what you expect!\n");
		/* Let it proceed, on the off chance it's useful */
	}

	/* What follows should probably be made a generic nmg operation */

	/* Dest must be empty to avoid index confusion */
	if( RT_LIST_NON_EMPTY( &(m->r_hd) ) )  {
		rt_log("rt_nmg_tess: destination model non-empty\n");
		/* XXX Here, should use some kind of nmg_copy() */
		return(-1);
	}

	/* Swipe newly imported model's stuff */
	RT_LIST_APPEND_LIST( &(m->r_hd), &(lm->r_hd) );
	m->maxindex += lm->maxindex;	/* just barely OK for a hack */

	/* Re-home regions to new model (which was empty until now) */
	for( RT_LIST_FOR( lr, nmgregion, &(m->r_hd) ) )  {
		lr->m_p = m;
	}

	return(0);
}

/*
 * ----------------------------------------------------------------------
 *
 *  Definitions for the binary, machine-independent format of
 *  the NMG data structures.
 *
 *  There are two special values that may be assigned to an index_t
 *  to signal special processing when the structure is re-imported.
 */
#define DISK_INDEX_NULL		0
#define DISK_INDEX_LISTHEAD	-1

typedef	char		index_t[4];
struct disk_rt_list  {
	index_t		forw;
	index_t		back;
};

#define DISK_MODEL_MAGIC	0x4e6d6f64	/* Nmod */
struct disk_model {
	char			magic[4];
	index_t			ma_p;
	struct disk_rt_list	r_hd;
};

#define DISK_MODEL_A_MAGIC	0x4e6d5f61	/* Nm_a */
struct disk_model_a {
	char			magic[4];
};

#define DISK_REGION_MAGIC	0x4e726567	/* Nreg */
struct disk_nmgregion {
	char			magic[4];
	struct disk_rt_list	l;
	index_t   		m_p;
	index_t			ra_p;
	struct disk_rt_list	s_hd;
};

#define DISK_REGION_A_MAGIC	0x4e725f61	/* Nr_a */
struct disk_nmgregion_a {
	char			magic[4];
	char			min_pt[3*8];
	char			max_pt[3*8];
};

#define DISK_SHELL_MAGIC	0x4e73686c	/* Nshl */
struct disk_shell {
	char			magic[4];
	struct disk_rt_list	l;
	index_t			r_p;
	index_t			sa_p;
	struct disk_rt_list	fu_hd;
	struct disk_rt_list	lu_hd;
	struct disk_rt_list	eu_hd;
	index_t			vu_p;
};

#define DISK_SHELL_A_MAGIC	0x4e735f61	/* Ns_a */
struct disk_shell_a {
	char			magic[4];
	char			min_pt[3*8];
	char			max_pt[3*8];
};

#define DISK_FACE_MAGIC		0x4e666163	/* Nfac */
struct disk_face {
	char			magic[4];
	index_t			fu_p;
	index_t			fg_p;
};

#define DISK_FACE_G_MAGIC	0x4e665f61	/* Nf_a */
struct disk_face_g {
	char			magic[4];
	char			N[4*8];
	/*
	 * Note that min_pt and max_pt are not stored on disk,
	 * regardless of "compact" flag setting.
	 */
};

#define DISK_FACEUSE_MAGIC	0x4e667520	/* Nfu */
struct disk_faceuse {
	char			magic[4];
	struct disk_rt_list	l;
	index_t			s_p;
	index_t			fumate_p;
	char			orientation[4];
	index_t			f_p;
	index_t			fua_p;
	struct disk_rt_list	lu_hd;
};

#define DISK_FACEUSE_A_MAGIC	0x4e667561	/* Nfua */
struct disk_faceuse_a {
	char			magic[4];
};

#define DISK_LOOP_MAGIC		0x4e6c6f70	/* Nlop */
struct disk_loop {
	char			magic[4];
	index_t			lu_p;
	index_t			lg_p;
};

#define DISK_LOOP_G_MAGIC	0x4e6c5f67	/* Nl_g */
struct disk_loop_g {
	char			magic[4];
	char			min_pt[3*8];
	char			max_pt[3*8];
};

#define DISK_LOOPUSE_MAGIC	0x4e6c7520	/* Nlu */
struct disk_loopuse {
	char			magic[4];
	struct disk_rt_list	l;
	index_t			up;
	index_t			lumate_p;
	char			orientation[4];
	index_t			l_p;
	index_t			lua_p;
	struct disk_rt_list	down_hd;
};

#define DISK_LOOPUSE_A_MAGIC	0x4e6c7561	/* Nlua */
struct disk_loopuse_a {
	char			magic[4];
};

#define DISK_EDGE_MAGIC		0x4e656467	/* Nedg */
struct disk_edge {
	char			magic[4];
	index_t			eu_p;
	index_t			eg_p;
};

#define DISK_EDGE_G_MAGIC	0x4e655f67	/* Ne_g */
struct disk_edge_g {
	char			magic[4];
};

#define DISK_EDGEUSE_MAGIC	0x4e657520	/* Neu */
struct disk_edgeuse {
	char			magic[4];
	struct disk_rt_list	l;
	index_t			up;
	index_t			eumate_p;
	index_t			radial_p;
	index_t			e_p;
	index_t			eua_p;
	char	  		orientation[4];
	index_t			vu_p;
};

#define DISK_EDGEUSE_A_MAGIC	0x4e657561	/* Neua */
struct disk_edgeuse_a {
	char			magic[4];
};

#define DISK_VERTEX_MAGIC	0x4e767274	/* Nvrt */
struct disk_vertex {
	char			magic[4];
	struct disk_rt_list	vu_hd;
	index_t			vg_p;
};

#define DISK_VERTEX_G_MAGIC	0x4e765f67	/* Nv_g */
struct disk_vertex_g {
	char			magic[4];
	char			coord[3*8];
};

#define DISK_VERTEXUSE_MAGIC	0x4e767520	/* Nvu */
struct disk_vertexuse {
	char			magic[4];
	struct disk_rt_list	l;
	index_t			up;
	index_t			v_p;
	index_t			vua_p;
};

#define DISK_VERTEXUSE_A_MAGIC	0x4e767561	/* Nvua */
struct disk_vertexuse_a {
	char			magic[4];
	char			N[3*8];
};

/* ---------------------------------------------------------------------- */
/* All these arrays and defines have to use the same implicit index values */
#define NMG_KIND_MODEL		0
#define NMG_KIND_MODEL_A	1
#define NMG_KIND_NMGREGION	2
#define NMG_KIND_NMGREGION_A	3
#define NMG_KIND_SHELL		4
#define NMG_KIND_SHELL_A	5
#define NMG_KIND_FACEUSE	6
#define NMG_KIND_FACEUSE_A	7
#define NMG_KIND_FACE		8
#define NMG_KIND_FACE_G		9
#define NMG_KIND_LOOPUSE	10
#define NMG_KIND_LOOPUSE_A	11
#define NMG_KIND_LOOP		12
#define NMG_KIND_LOOP_G		13
#define NMG_KIND_EDGEUSE	14
#define NMG_KIND_EDGEUSE_A	15
#define NMG_KIND_EDGE		16
#define NMG_KIND_EDGE_G		17
#define NMG_KIND_VERTEXUSE	18
#define NMG_KIND_VERTEXUSE_A	19
#define NMG_KIND_VERTEX		20
#define NMG_KIND_VERTEX_G	21

#define NMG_N_KINDS		22		/* number of kinds */

int	rt_nmg_disk_sizes[NMG_N_KINDS] = {
	sizeof(struct disk_model),
	sizeof(struct disk_model_a),
	sizeof(struct disk_nmgregion),
	sizeof(struct disk_nmgregion_a),
	sizeof(struct disk_shell),
	sizeof(struct disk_shell_a),
	sizeof(struct disk_faceuse),
	sizeof(struct disk_faceuse_a),
	sizeof(struct disk_face),
	sizeof(struct disk_face_g),
	sizeof(struct disk_loopuse),
	sizeof(struct disk_loopuse_a),
	sizeof(struct disk_loop),
	sizeof(struct disk_loop_g),
	sizeof(struct disk_edgeuse),
	sizeof(struct disk_edgeuse_a),
	sizeof(struct disk_edge),
	sizeof(struct disk_edge_g),
	sizeof(struct disk_vertexuse),
	sizeof(struct disk_vertexuse_a),
	sizeof(struct disk_vertex),
	sizeof(struct disk_vertex_g)
};
char	rt_nmg_kind_names[NMG_N_KINDS][12] = {
	"model",
	"model_a",
	"nmgregion",
	"nmgregion_a",
	"shell",
	"shell_a",
	"faceuse",
	"faceuse_a",
	"face",
	"face_g",
	"loopuse",
	"loopuse_a",
	"loop",
	"loop_g",
	"edgeuse",
	"edgeuse_a",
	"edge",
	"edge_g",
	"vertexuse",
	"vertexuse_a",
	"vertex",
	"vertex_g"
};

/*
 *			R T _ N M G _ M A G I C _ T O _ K I N D
 *
 *  Given the magic number for an NMG structure, return the
 *  manifest constant which identifies that structure kind.
 */
int
rt_nmg_magic_to_kind( magic )
register long	magic;
{
	switch(magic)  {
	case NMG_MODEL_MAGIC:
		return NMG_KIND_MODEL;
	case NMG_MODEL_A_MAGIC:
		return NMG_KIND_MODEL_A;
	case NMG_REGION_MAGIC:
		return NMG_KIND_NMGREGION;
	case NMG_REGION_A_MAGIC:
		return NMG_KIND_NMGREGION_A;
	case NMG_SHELL_MAGIC:
		return NMG_KIND_SHELL;
	case NMG_SHELL_A_MAGIC:
		return NMG_KIND_SHELL_A;
	case NMG_FACEUSE_MAGIC:
		return NMG_KIND_FACEUSE;
	case NMG_FACEUSE_A_MAGIC:
		return NMG_KIND_FACEUSE_A;
	case NMG_FACE_MAGIC:
		return NMG_KIND_FACE;
	case NMG_FACE_G_MAGIC:
		return NMG_KIND_FACE_G;
	case NMG_LOOPUSE_MAGIC:
		return NMG_KIND_LOOPUSE;
	case NMG_LOOPUSE_A_MAGIC:
		return NMG_KIND_LOOPUSE_A;
	case NMG_LOOP_MAGIC:
		return NMG_KIND_LOOP;
	case NMG_LOOP_G_MAGIC:
		return NMG_KIND_LOOP_G;
	case NMG_EDGEUSE_MAGIC:
		return NMG_KIND_EDGEUSE;
	case NMG_EDGEUSE_A_MAGIC:
		return NMG_KIND_EDGEUSE_A;
	case NMG_EDGE_MAGIC:
		return NMG_KIND_EDGE;
	case NMG_EDGE_G_MAGIC:
		return NMG_KIND_EDGE_G;
	case NMG_VERTEXUSE_MAGIC:
		return NMG_KIND_VERTEXUSE;
	case NMG_VERTEXUSE_A_MAGIC:
		return NMG_KIND_VERTEXUSE_A;
	case NMG_VERTEX_MAGIC:
		return NMG_KIND_VERTEX;
	case NMG_VERTEX_G_MAGIC:
		return NMG_KIND_VERTEX_G;
	}
	/* default */
	rt_log("magic = x%x\n", magic);
	rt_bomb("rt_nmg_magic_to_kind: bad magic");
	return -1;
}

/* ---------------------------------------------------------------------- */

struct nmg_exp_counts {
	long	new_subscript;
	long	per_struct_index;
	int	kind;
};

/*
 *			R T _ N M G _ R E I N D E X
 *
 *  There are some special values for the disk index returned here:
 *	>0	normal structure index.
 *	 0	substitute a null pointer when imported.
 *	-1	substitute pointer to within-struct list head when imported.
 */
int
rt_nmg_reindex(p, ecnt)
genptr_t		p;
struct nmg_exp_counts	*ecnt;
{
	int	index;
	int	ret;

	/* If null pointer, return new subscript of zero */
	if( p == 0 )  {
		ret = 0;
		index = 0;	/* sanity */
	} else {
		index = nmg_index_of_struct((long *)(p));
		if( index == -1 )  {
			ret = DISK_INDEX_LISTHEAD; /* FLAG:  special list head */
		} else if( index < -1 ) {
			ret = DISK_INDEX_NULL;	/* ERROR. Use null ptr on import */
		} else {
			ret = ecnt[index].new_subscript;
		}
	}
/*rt_log("rt_nmg_reindex(p=x%x), index=%d, newindex=%d\n", p, index, ret);*/
	return( ret );
}

/* forw may never be null;  back may be null for loopuse (sigh) */
#define INDEX(o,i,elem)	\
	(void)rt_plong((o)->elem, rt_nmg_reindex((genptr_t)((i)->elem), ecnt))
#define INDEXL(oo,ii,elem)	{ \
	register long _f = rt_nmg_reindex((genptr_t)((ii)->elem.forw), ecnt); \
	if( _f == DISK_INDEX_NULL )  rt_bomb("rt_nmg_edisk: reindex forw to null?\n"); \
	(void)rt_plong( (oo)->elem.forw, _f ); \
	(void)rt_plong( (oo)->elem.back, rt_nmg_reindex((genptr_t)((ii)->elem.back), ecnt) ); }

/*
 *			R T _ N M G _ E D I S K
 *
 *  Export a given structure from memory to disk format
 */
void
rt_nmg_edisk( op, ip, ecnt, index, local2mm )
genptr_t	op;		/* base of disk array */
genptr_t	ip;		/* ptr to in-memory structure */
struct nmg_exp_counts	*ecnt;
int		index;
double		local2mm;
{
	int	oindex;		/* index in op */

	oindex = ecnt[index].per_struct_index;
	switch(ecnt[index].kind)  {
	case NMG_KIND_MODEL:
		{
			struct model	*m = (struct model *)ip;
			struct disk_model	*d;
			d = &((struct disk_model *)op)[oindex];
			NMG_CK_MODEL(m);
			rt_plong( d->magic, DISK_MODEL_MAGIC );
			INDEX( d, m, ma_p );
			INDEXL( d, m, r_hd );
		}
		return;
	case NMG_KIND_MODEL_A:
		{
			struct model_a	*ma = (struct model_a *)ip;
			struct disk_model_a	*d;
			d = &((struct disk_model_a *)op)[oindex];
			NMG_CK_MODEL_A(ma);
			rt_plong( d->magic, DISK_MODEL_A_MAGIC );
		}
		return;
	case NMG_KIND_NMGREGION:
		{
			struct nmgregion	*r = (struct nmgregion *)ip;
			struct disk_nmgregion	*d;
			d = &((struct disk_nmgregion *)op)[oindex];
			NMG_CK_REGION(r);
			rt_plong( d->magic, DISK_REGION_MAGIC );
			INDEXL( d, r, l );
			INDEX( d, r, m_p );
			INDEX( d, r, ra_p );
			INDEXL( d, r, s_hd );
		}
		return;
	case NMG_KIND_NMGREGION_A:
		{
			struct nmgregion_a	*r = (struct nmgregion_a *)ip;
			struct disk_nmgregion_a	*d;
			point_t			min, max;
			d = &((struct disk_nmgregion_a *)op)[oindex];
			NMG_CK_REGION_A(r);
			rt_plong( d->magic, DISK_REGION_A_MAGIC );
			VSCALE( min, r->min_pt, local2mm );
			VSCALE( max, r->max_pt, local2mm );
			htond( d->min_pt, min, 3 );
			htond( d->max_pt, max, 3 );
		}
		return;
	case NMG_KIND_SHELL:
		{
			struct shell	*s = (struct shell *)ip;
			struct disk_shell	*d;
			d = &((struct disk_shell *)op)[oindex];
			NMG_CK_SHELL(s);
			rt_plong( d->magic, DISK_SHELL_MAGIC );
			INDEXL( d, s, l );
			INDEX( d, s, r_p );
			INDEX( d, s, sa_p );
			INDEXL( d, s, fu_hd );
			INDEXL( d, s, lu_hd );
			INDEXL( d, s, eu_hd );
			INDEX( d, s, vu_p );
		}
		return;
	case NMG_KIND_SHELL_A:
		{
			struct shell_a	*sa = (struct shell_a *)ip;
			struct disk_shell_a	*d;
			point_t			min, max;
			d = &((struct disk_shell_a *)op)[oindex];
			NMG_CK_SHELL_A(sa);
			rt_plong( d->magic, DISK_SHELL_A_MAGIC );
			VSCALE( min, sa->min_pt, local2mm );
			VSCALE( max, sa->max_pt, local2mm );
			htond( d->min_pt, min, 3 );
			htond( d->max_pt, max, 3 );
		}
		return;
	case NMG_KIND_FACEUSE:
		{
			struct faceuse	*fu = (struct faceuse *)ip;
			struct disk_faceuse	*d;
			d = &((struct disk_faceuse *)op)[oindex];
			NMG_CK_FACEUSE(fu);
			NMG_CK_FACEUSE(fu->fumate_p);
			NMG_CK_FACE(fu->f_p);
			if( fu->f_p != fu->fumate_p->f_p )  rt_log("faceuse export, differing faces\n");
			rt_plong( d->magic, DISK_FACEUSE_MAGIC );
			INDEXL( d, fu, l );
			INDEX( d, fu, s_p );
			INDEX( d, fu, fumate_p );
			rt_plong( d->orientation, fu->orientation );
			INDEX( d, fu, f_p );
			INDEX( d, fu, fua_p );
			INDEXL( d, fu, lu_hd );
		}
		return;
	case NMG_KIND_FACEUSE_A:
		{
			struct faceuse_a	*fua = (struct faceuse_a *)ip;
			struct disk_faceuse_a	*d;
			d = &((struct disk_faceuse_a *)op)[oindex];
			NMG_CK_FACEUSE_A(fua);
			rt_plong( d->magic, DISK_FACEUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_FACE:
		{
			struct face	*f = (struct face *)ip;
			struct disk_face	*d;
			d = &((struct disk_face *)op)[oindex];
			NMG_CK_FACE(f);
			rt_plong( d->magic, DISK_FACE_MAGIC );
			INDEX( d, f, fu_p );
			INDEX( d, f, fg_p );
		}
		return;
	case NMG_KIND_FACE_G:
		{
			struct face_g	*fg = (struct face_g *)ip;
			struct disk_face_g	*d;
			point_t			min, max;
			plane_t			plane;
			d = &((struct disk_face_g *)op)[oindex];
			NMG_CK_FACE_G(fg);
			rt_plong( d->magic, DISK_FACE_G_MAGIC );
			VMOVE( plane, fg->N );
			plane[3] = fg->N[3] * local2mm;
			htond( d->N, plane, 4 );
		}
		return;
	case NMG_KIND_LOOPUSE:
		{
			struct loopuse	*lu = (struct loopuse *)ip;
			struct disk_loopuse	*d;
			d = &((struct disk_loopuse *)op)[oindex];
			NMG_CK_LOOPUSE(lu);
			rt_plong( d->magic, DISK_LOOPUSE_MAGIC );
			INDEXL( d, lu, l );
			rt_plong( d->up, rt_nmg_reindex((genptr_t)(lu->up.magic_p), ecnt) );
			INDEX( d, lu, lumate_p );
			rt_plong( d->orientation, lu->orientation );
			INDEX( d, lu, l_p );
			INDEX( d, lu, lua_p );
			INDEXL( d, lu, down_hd );
		}
		return;
	case NMG_KIND_LOOPUSE_A:
		{
			struct loopuse_a	*lua = (struct loopuse_a *)ip;
			struct disk_loopuse_a	*d;
			d = &((struct disk_loopuse_a *)op)[oindex];
			NMG_CK_LOOPUSE_A(lua);
			rt_plong( d->magic, DISK_LOOPUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_LOOP:
		{
			struct loop	*loop = (struct loop *)ip;
			struct disk_loop	*d;
			d = &((struct disk_loop *)op)[oindex];
			NMG_CK_LOOP(loop);
			rt_plong( d->magic, DISK_LOOP_MAGIC );
			INDEX( d, loop, lu_p );
			INDEX( d, loop, lg_p );
		}
		return;
	case NMG_KIND_LOOP_G:
		{
			struct loop_g	*lg = (struct loop_g *)ip;
			struct disk_loop_g	*d;
			point_t			min, max;
			d = &((struct disk_loop_g *)op)[oindex];
			NMG_CK_LOOP_G(lg);
			rt_plong( d->magic, DISK_LOOP_G_MAGIC );
			VSCALE( min, lg->min_pt, local2mm );
			VSCALE( max, lg->max_pt, local2mm );
			htond( d->min_pt, min, 3 );
			htond( d->max_pt, max, 3 );
		}
		return;
	case NMG_KIND_EDGEUSE:
		{
			struct edgeuse	*eu = (struct edgeuse *)ip;
			struct disk_edgeuse	*d;
			d = &((struct disk_edgeuse *)op)[oindex];
			NMG_CK_EDGEUSE(eu);
			rt_plong( d->magic, DISK_EDGEUSE_MAGIC );
			INDEXL( d, eu, l );
			rt_plong( d->up, rt_nmg_reindex((genptr_t)(eu->up.magic_p), ecnt) );
			INDEX( d, eu, eumate_p );
			INDEX( d, eu, radial_p );
			INDEX( d, eu, e_p );
			INDEX( d, eu, eua_p );
			rt_plong( d->orientation, eu->orientation);
			INDEX( d, eu, vu_p );
		}
		return;
	case NMG_KIND_EDGEUSE_A:
		{
			struct edgeuse_a	*eua = (struct edgeuse_a *)ip;
			struct disk_edgeuse_a	*d;
			d = &((struct disk_edgeuse_a *)op)[oindex];
			NMG_CK_EDGEUSE_A(eua);
			rt_plong( d->magic, DISK_EDGEUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_EDGE:
		{
			struct edge	*e = (struct edge *)ip;
			struct disk_edge	*d;
			d = &((struct disk_edge *)op)[oindex];
			NMG_CK_EDGE(e);
			rt_plong( d->magic, DISK_EDGE_MAGIC );
			INDEX( d, e, eu_p );
			INDEX( d, e, eg_p );
		}
		return;
	case NMG_KIND_EDGE_G:
		{
			struct edge_g	*eg = (struct edge_g *)ip;
			struct disk_edge_g	*d;
			d = &((struct disk_edge_g *)op)[oindex];
			NMG_CK_EDGE_G(eg);
			rt_plong( d->magic, DISK_EDGE_G_MAGIC );
		}
		return;
	case NMG_KIND_VERTEXUSE:
		{
			struct vertexuse	*vu = (struct vertexuse *)ip;
			struct disk_vertexuse	*d;
			d = &((struct disk_vertexuse *)op)[oindex];
			NMG_CK_VERTEXUSE(vu);
			rt_plong( d->magic, DISK_VERTEXUSE_MAGIC );
			INDEXL( d, vu, l );
			rt_plong( d->up, rt_nmg_reindex((genptr_t)(vu->up.magic_p), ecnt) );
			INDEX( d, vu, v_p );
			INDEX( d, vu, vua_p );
		}
		return;
	case NMG_KIND_VERTEXUSE_A:
		{
			struct vertexuse_a	*vua = (struct vertexuse_a *)ip;
			struct disk_vertexuse_a	*d;
			plane_t			plane;
			d = &((struct disk_vertexuse_a *)op)[oindex];
			NMG_CK_VERTEXUSE_A(vua);
			rt_plong( d->magic, DISK_VERTEXUSE_A_MAGIC );
			VMOVE( plane, vua->N );
			plane[3] = vua->N[3] * local2mm;
			htond( d->N, plane, 4 );
		}
		return;
	case NMG_KIND_VERTEX:
		{
			struct vertex	*v = (struct vertex *)ip;
			struct disk_vertex	*d;
			d = &((struct disk_vertex *)op)[oindex];
			NMG_CK_VERTEX(v);
			rt_plong( d->magic, DISK_VERTEX_MAGIC );
			INDEXL( d, v, vu_hd );
			INDEX( d, v, vg_p );
		}
		return;
	case NMG_KIND_VERTEX_G:
		{
			struct vertex_g	*vg = (struct vertex_g *)ip;
			struct disk_vertex_g	*d;
			point_t			pt;
			d = &((struct disk_vertex_g *)op)[oindex];
			NMG_CK_VERTEX_G(vg);
			rt_plong( d->magic, DISK_VERTEX_G_MAGIC );
			VSCALE( pt, vg->coord, local2mm );
			htond( d->coord, pt, 3 );
		}
		return;
	}
	rt_log("rt_nmg_edisk kind=%d unknown\n", ecnt[index].kind);
}
#undef INDEX
#undef INDEXL

#define RT_CK_DISKMAGIC(_cp,_magic)	\
	if( rt_glong(_cp) != _magic )  { \
		rt_log("RT_CK_DISKMAGIC: magic mis-match, got x%x, s/b x%x\n", rt_glong(_cp), _magic); \
		rt_bomb("bad magic\n"); \
	}


/*
 *  For symmetry with export, use same macro names and arg ordering,
 *  but here take from "o" (outboard) variable and put in "i" (internal).
 *
 *  NOTE that the "< 0" test here is a comparison with DISK_INDEX_LISTHEAD.
 */
#define INDEX(o,i,ty,elem)	(i)->elem = (struct ty *)ptrs[rt_glong((o)->elem)]
#define INDEXL_HD(oo,ii,elem,hd)	{ \
	register int	sub; \
	if( (sub = rt_glong((oo)->elem.forw)) < 0 ) \
		(ii)->elem.forw = &(hd); \
	else	(ii)->elem.forw = (struct rt_list *)ptrs[sub]; \
	if( (sub = rt_glong((oo)->elem.back)) < 0 ) \
		(ii)->elem.back = &(hd); \
	else (ii)->elem.back = (struct rt_list *)ptrs[sub]; }

/*
 *			R T _ N M G _ I D I S K
 *
 *  Import a given structure from disk to memory format.
 */
void
rt_nmg_idisk( op, ip, ecnt, index, ptrs, mat )
genptr_t	op;		/* ptr to in-memory structure */
genptr_t	ip;		/* base of disk array */
struct nmg_exp_counts	*ecnt;
int		index;
long		**ptrs;
mat_t		mat;
{
	int	iindex;		/* index in ip */

	iindex = 0;
	switch(ecnt[index].kind)  {
	case NMG_KIND_MODEL:
		{
			struct model	*m = (struct model *)op;
			struct disk_model	*d;
			d = &((struct disk_model *)ip)[iindex];
			NMG_CK_MODEL(m);
			RT_CK_DISKMAGIC( d->magic, DISK_MODEL_MAGIC );
			INDEX( d, m, model_a, ma_p );
			INDEXL_HD( d, m, r_hd, m->r_hd );
		}
		return;
	case NMG_KIND_MODEL_A:
		{
			struct model_a	*ma = (struct model_a *)op;
			struct disk_model_a	*d;
			d = &((struct disk_model_a *)ip)[iindex];
			NMG_CK_MODEL_A(ma);
			RT_CK_DISKMAGIC( d->magic, DISK_MODEL_A_MAGIC );
		}
		return;
	case NMG_KIND_NMGREGION:
		{
			struct nmgregion	*r = (struct nmgregion *)op;
			struct disk_nmgregion	*d;
			d = &((struct disk_nmgregion *)ip)[iindex];
			NMG_CK_REGION(r);
			RT_CK_DISKMAGIC( d->magic, DISK_REGION_MAGIC );
			INDEX( d, r, model, m_p );
			INDEX( d, r, nmgregion_a, ra_p );
			INDEXL_HD( d, r, s_hd, r->s_hd );
			INDEXL_HD( d, r, l, r->m_p->r_hd ); /* do after m_p */
			NMG_CK_MODEL(r->m_p);
		}
		return;
	case NMG_KIND_NMGREGION_A:
		{
			struct nmgregion_a	*r = (struct nmgregion_a *)op;
			struct disk_nmgregion_a	*d;
			point_t			min, max;
			d = &((struct disk_nmgregion_a *)ip)[iindex];
			NMG_CK_REGION_A(r);
			RT_CK_DISKMAGIC( d->magic, DISK_REGION_A_MAGIC );
			ntohd( min, d->min_pt, 3 );
			ntohd( max, d->max_pt, 3 );
			rt_rotate_bbox( r->min_pt, r->max_pt, mat, min, max );
		}
		return;
	case NMG_KIND_SHELL:
		{
			struct shell	*s = (struct shell *)op;
			struct disk_shell	*d;
			d = &((struct disk_shell *)ip)[iindex];
			NMG_CK_SHELL(s);
			RT_CK_DISKMAGIC( d->magic, DISK_SHELL_MAGIC );
			INDEX( d, s, nmgregion, r_p );
			INDEX( d, s, shell_a, sa_p );
			INDEXL_HD( d, s, fu_hd, s->fu_hd );
			INDEXL_HD( d, s, lu_hd, s->lu_hd );
			INDEXL_HD( d, s, eu_hd, s->eu_hd );
			INDEX( d, s, vertexuse, vu_p );
			INDEXL_HD( d, s, l, s->r_p->s_hd ); /* after s->r_p */
			NMG_CK_REGION(s->r_p);
		}
		return;
	case NMG_KIND_SHELL_A:
		{
			struct shell_a	*sa = (struct shell_a *)op;
			struct disk_shell_a	*d;
			point_t			min, max;
			d = &((struct disk_shell_a *)ip)[iindex];
			NMG_CK_SHELL_A(sa);
			RT_CK_DISKMAGIC( d->magic, DISK_SHELL_A_MAGIC );
			ntohd( min, d->min_pt, 3 );
			ntohd( max, d->max_pt, 3 );
			rt_rotate_bbox( sa->min_pt, sa->max_pt, mat, min, max );
		}
		return;
	case NMG_KIND_FACEUSE:
		{
			struct faceuse	*fu = (struct faceuse *)op;
			struct disk_faceuse	*d;
			d = &((struct disk_faceuse *)ip)[iindex];
			NMG_CK_FACEUSE(fu);
			RT_CK_DISKMAGIC( d->magic, DISK_FACEUSE_MAGIC );
			INDEX( d, fu, shell, s_p );
			INDEX( d, fu, faceuse, fumate_p );
			fu->orientation = rt_glong( d->orientation );
			INDEX( d, fu, face, f_p );
			INDEX( d, fu, faceuse_a, fua_p );
			INDEXL_HD( d, fu, lu_hd, fu->lu_hd );
			INDEXL_HD( d, fu, l, fu->s_p->fu_hd ); /* after fu->s_p */
			NMG_CK_FACE(fu->f_p);
			NMG_CK_FACEUSE(fu->fumate_p);
		}
		return;
	case NMG_KIND_FACEUSE_A:
		{
			struct faceuse_a	*fua = (struct faceuse_a *)op;
			struct disk_faceuse_a	*d;
			d = &((struct disk_faceuse_a *)ip)[iindex];
			NMG_CK_FACEUSE_A(fua);
			RT_CK_DISKMAGIC( d->magic, DISK_FACEUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_FACE:
		{
			struct face	*f = (struct face *)op;
			struct disk_face	*d;
			d = &((struct disk_face *)ip)[iindex];
			NMG_CK_FACE(f);
			RT_CK_DISKMAGIC( d->magic, DISK_FACE_MAGIC );
			INDEX( d, f, faceuse, fu_p );
			INDEX( d, f, face_g, fg_p );
			NMG_CK_FACEUSE(f->fu_p);
		}
		return;
	case NMG_KIND_FACE_G:
		{
			struct face_g	*fg = (struct face_g *)op;
			struct disk_face_g	*d;
			plane_t			plane;
			point_t			min, max;
			d = &((struct disk_face_g *)ip)[iindex];
			NMG_CK_FACE_G(fg);
			RT_CK_DISKMAGIC( d->magic, DISK_FACE_G_MAGIC );
			ntohd( plane, d->N, 4 );
			rt_rotate_plane( fg->N, mat, plane );
		}
		return;
	case NMG_KIND_LOOPUSE:
		{
			struct loopuse	*lu = (struct loopuse *)op;
			struct disk_loopuse	*d;
			int			up_index;
			int			up_kind;

			d = &((struct disk_loopuse *)ip)[iindex];
			NMG_CK_LOOPUSE(lu);
			RT_CK_DISKMAGIC( d->magic, DISK_LOOPUSE_MAGIC );
			up_index = rt_glong(d->up);
			lu->up.magic_p = (long *)ptrs[up_index];
			INDEX( d, lu, loopuse, lumate_p );
			lu->orientation = rt_glong( d->orientation );
			INDEX( d, lu, loop, l_p );
			INDEX( d, lu, loopuse_a, lua_p );
			up_kind = ecnt[up_index].kind;
			if( up_kind == NMG_KIND_FACEUSE )  {
				INDEXL_HD( d, lu, l, lu->up.fu_p->lu_hd );
			} else if( up_kind == NMG_KIND_SHELL )  {
				INDEXL_HD( d, lu, l, lu->up.s_p->lu_hd );
			} else rt_log("bad loopuse up, index=%d, kind=%d\n", up_index, up_kind);
			INDEXL_HD( d, lu, down_hd, lu->down_hd );
			if( lu->down_hd.forw == RT_LIST_NULL )
				rt_bomb("rt_nmg_idisk: null loopuse down_hd.forw\n");
			NMG_CK_LOOP(lu->l_p);
		}
		return;
	case NMG_KIND_LOOPUSE_A:
		{
			struct loopuse_a	*lua = (struct loopuse_a *)op;
			struct disk_loopuse_a	*d;
			d = &((struct disk_loopuse_a *)ip)[iindex];
			NMG_CK_LOOPUSE_A(lua);
			RT_CK_DISKMAGIC( d->magic, DISK_LOOPUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_LOOP:
		{
			struct loop	*loop = (struct loop *)op;
			struct disk_loop	*d;
			d = &((struct disk_loop *)ip)[iindex];
			NMG_CK_LOOP(loop);
			RT_CK_DISKMAGIC( d->magic, DISK_LOOP_MAGIC );
			INDEX( d, loop, loopuse, lu_p );
			INDEX( d, loop, loop_g, lg_p );
			NMG_CK_LOOPUSE(loop->lu_p);
		}
		return;
	case NMG_KIND_LOOP_G:
		{
			struct loop_g	*lg = (struct loop_g *)op;
			struct disk_loop_g	*d;
			point_t			min, max;
			d = &((struct disk_loop_g *)ip)[iindex];
			NMG_CK_LOOP_G(lg);
			RT_CK_DISKMAGIC( d->magic, DISK_LOOP_G_MAGIC );
			ntohd( min, d->min_pt, 3 );
			ntohd( max, d->max_pt, 3 );
			rt_rotate_bbox( lg->min_pt, lg->max_pt, mat, min, max );
		}
		return;
	case NMG_KIND_EDGEUSE:
		{
			struct edgeuse	*eu = (struct edgeuse *)op;
			struct disk_edgeuse	*d;
			int			up_index;
			int			up_kind;

			d = &((struct disk_edgeuse *)ip)[iindex];
			NMG_CK_EDGEUSE(eu);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGEUSE_MAGIC );
			up_index = rt_glong(d->up);
			eu->up.magic_p = (long *)ptrs[up_index];
			INDEX( d, eu, edgeuse, eumate_p );
			INDEX( d, eu, edgeuse, radial_p );
			INDEX( d, eu, edge, e_p );
			INDEX( d, eu, edgeuse_a, eua_p );
			eu->orientation = rt_glong( d->orientation );
			INDEX( d, eu, vertexuse, vu_p );
			up_kind = ecnt[up_index].kind;
			if( up_kind == NMG_KIND_LOOPUSE )  {
				INDEXL_HD( d, eu, l, eu->up.lu_p->down_hd );
			} else if( up_kind == NMG_KIND_SHELL )  {
				INDEXL_HD( d, eu, l, eu->up.s_p->eu_hd );
			} else rt_log("bad edgeuse up, index=%d, kind=%d\n", up_index, up_kind);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_EDGEUSE(eu->eumate_p);
			NMG_CK_EDGEUSE(eu->radial_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
		}
		return;
	case NMG_KIND_EDGEUSE_A:
		{
			struct edgeuse_a	*eua = (struct edgeuse_a *)op;
			struct disk_edgeuse_a	*d;
			d = &((struct disk_edgeuse_a *)ip)[iindex];
			NMG_CK_EDGEUSE_A(eua);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGEUSE_A_MAGIC );
		}
		return;
	case NMG_KIND_EDGE:
		{
			struct edge	*e = (struct edge *)op;
			struct disk_edge	*d;
			d = &((struct disk_edge *)ip)[iindex];
			NMG_CK_EDGE(e);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGE_MAGIC );
			INDEX( d, e, edgeuse, eu_p );
			INDEX( d, e, edge_g, eg_p );
			NMG_CK_EDGEUSE(e->eu_p);
		}
		return;
	case NMG_KIND_EDGE_G:
		{
			struct edge_g	*eg = (struct edge_g *)op;
			struct disk_edge_g	*d;
			d = &((struct disk_edge_g *)ip)[iindex];
			NMG_CK_EDGE_G(eg);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGE_G_MAGIC );
		}
		return;
	case NMG_KIND_VERTEXUSE:
		{
			struct vertexuse	*vu = (struct vertexuse *)op;
			struct disk_vertexuse	*d;
			d = &((struct disk_vertexuse *)ip)[iindex];
			NMG_CK_VERTEXUSE(vu);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEXUSE_MAGIC );
			INDEXL_HD( d, vu, l, vu->l );
			vu->up.magic_p = (long *)ptrs[rt_glong(d->up)];
			INDEX( d, vu, vertex, v_p );
			INDEX( d, vu, vertexuse_a, vua_p );
			NMG_CK_VERTEX(vu->v_p);
		}
		return;
	case NMG_KIND_VERTEXUSE_A:
		{
			struct vertexuse_a	*vua = (struct vertexuse_a *)op;
			struct disk_vertexuse_a	*d;
			plane_t			plane;
			d = &((struct disk_vertexuse_a *)ip)[iindex];
			NMG_CK_VERTEXUSE_A(vua);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEXUSE_A_MAGIC );
			ntohd( plane, d->N, 4 );
			rt_rotate_plane( vua->N, mat, plane );
		}
		return;
	case NMG_KIND_VERTEX:
		{
			struct vertex	*v = (struct vertex *)op;
			struct disk_vertex	*d;
			d = &((struct disk_vertex *)ip)[iindex];
			NMG_CK_VERTEX(v);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEX_MAGIC );
			INDEXL_HD( d, v, vu_hd, v->vu_hd );
			INDEX( d, v, vertex_g, vg_p );
		}
		return;
	case NMG_KIND_VERTEX_G:
		{
			struct vertex_g	*vg = (struct vertex_g *)op;
			struct disk_vertex_g	*d;
			point_t			pt;
			d = &((struct disk_vertex_g *)ip)[iindex];
			NMG_CK_VERTEX_G(vg);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEX_G_MAGIC );
			ntohd( pt, d->coord, 3 );
			MAT4X3PNT( vg->coord, mat, pt );
		}
		return;
	}
	rt_log("rt_nmg_idisk kind=%d unknown\n", ecnt[index].kind);
}

/*
 *			R T _ N M G _ I A L L O C
 *
 *  Allocate storage for all the in-memory NMG structures,
 *  using the GET_xxx() macros, so that m->maxindex, etc,
 *  are all appropriately handled.
 */
struct model *
rt_nmg_ialloc( ptrs, ecnt, kind_counts )
long				**ptrs;
struct nmg_exp_counts		*ecnt;
int				kind_counts[NMG_N_KINDS];
{
	struct model		*m = (struct model *)0;
	int			subscript;
	int			kind;
	int			j;

	subscript = 1;
	for( kind = 0; kind < NMG_N_KINDS; kind++ )  {
#if DEBUG
rt_log("%d  %s\n", kind_counts[kind], rt_nmg_kind_names[kind] );
#endif
		for( j = 0; j < kind_counts[kind]; j++ )  {
			ecnt[subscript].kind = kind;
			ecnt[subscript].per_struct_index = 0; /* unused on import */
			switch( kind )  {
			case NMG_KIND_MODEL:
				if( m )  rt_bomb("multiple models?");
				m = nmg_mm();
				/* Keep disk indices & new indices equal... */
				m->maxindex++;
				ptrs[subscript] = (long *)m;
				break;
			case NMG_KIND_MODEL_A:
				{
					struct model_a	*ma;
					GET_MODEL_A( ma, m );
					ma->magic = NMG_MODEL_A_MAGIC;
					ptrs[subscript] = (long *)ma;
				}
				break;
			case NMG_KIND_NMGREGION:
				{
					struct nmgregion	*r;
					GET_REGION( r, m );
					r->l.magic = NMG_REGION_MAGIC;
					RT_LIST_INIT( &r->s_hd );
					ptrs[subscript] = (long *)r;
				}
				break;
			case NMG_KIND_NMGREGION_A:
				{
					struct nmgregion_a		*ra;
					GET_REGION_A( ra, m );
					ra->magic = NMG_REGION_A_MAGIC;
					ptrs[subscript] = (long *)ra;
				}
				break;
			case NMG_KIND_SHELL:
				{
					struct shell	*s;
					GET_SHELL( s, m );
					s->l.magic = NMG_SHELL_MAGIC;
					RT_LIST_INIT( &s->fu_hd );
					RT_LIST_INIT( &s->lu_hd );
					RT_LIST_INIT( &s->eu_hd );
					ptrs[subscript] = (long *)s;
				}
				break;
			case NMG_KIND_SHELL_A:
				{
					struct shell_a	*sa;
					GET_SHELL_A( sa, m );
					sa->magic = NMG_SHELL_A_MAGIC;
					ptrs[subscript] = (long *)sa;
				}
				break;
			case NMG_KIND_FACEUSE:
				{
					struct faceuse	*fu;
					GET_FACEUSE( fu, m );
					fu->l.magic = NMG_FACEUSE_MAGIC;
					RT_LIST_INIT( &fu->lu_hd );
					ptrs[subscript] = (long *)fu;
				}
				break;
			case NMG_KIND_FACEUSE_A:
				{
					struct faceuse_a	*fua;
					GET_FACEUSE_A( fua, m );
					fua->magic = NMG_FACEUSE_A_MAGIC;
					ptrs[subscript] = (long *)fua;
				}
				break;
			case NMG_KIND_FACE:
				{
					struct face	*f;
					GET_FACE( f, m );
					f->magic = NMG_FACE_MAGIC;
					ptrs[subscript] = (long *)f;
				}
				break;
			case NMG_KIND_FACE_G:
				{
					struct face_g	*fg;
					GET_FACE_G( fg, m );
					fg->magic = NMG_FACE_G_MAGIC;
					ptrs[subscript] = (long *)fg;
				}
				break;
			case NMG_KIND_LOOPUSE:
				{
					struct loopuse	*lu;
					GET_LOOPUSE( lu, m );
					lu->l.magic = NMG_LOOPUSE_MAGIC;
					RT_LIST_INIT( &lu->down_hd );
					ptrs[subscript] = (long *)lu;
				}
				break;
			case NMG_KIND_LOOPUSE_A:
				{
					struct loopuse_a	*lua;
					GET_LOOPUSE_A( lua, m );
					lua->magic = NMG_LOOPUSE_A_MAGIC;
					ptrs[subscript] = (long *)lua;
				}
				break;
			case NMG_KIND_LOOP:
				{
					struct loop	*l;
					GET_LOOP( l, m );
					l->magic = NMG_LOOP_MAGIC;
					ptrs[subscript] = (long *)l;
				}
				break;
			case NMG_KIND_LOOP_G:
				{
					struct loop_g	*lg;
					GET_LOOP_G( lg, m );
					lg->magic = NMG_LOOP_G_MAGIC;
					ptrs[subscript] = (long *)lg;
				}
				break;
			case NMG_KIND_EDGEUSE:
				{
					struct edgeuse	*eu;
					GET_EDGEUSE( eu, m );
					eu->l.magic = NMG_EDGEUSE_MAGIC;
					ptrs[subscript] = (long *)eu;
				}
				break;
			case NMG_KIND_EDGEUSE_A:
				{
					struct edgeuse_a	*eua;
					GET_EDGEUSE_A( eua, m );
					eua->magic = NMG_EDGEUSE_A_MAGIC;
					ptrs[subscript] = (long *)eua;
				}
				break;
			case NMG_KIND_EDGE:
				{
					struct edge	*e;
					GET_EDGE( e, m );
					e->magic = NMG_EDGE_MAGIC;
					ptrs[subscript] = (long *)e;
				}
				break;
			case NMG_KIND_EDGE_G:
				{
					struct edge_g	*eg;
					GET_EDGE_G( eg, m );
					eg->magic = NMG_EDGE_G_MAGIC;
					ptrs[subscript] = (long *)eg;
				}
				break;
			case NMG_KIND_VERTEXUSE:
				{
					struct vertexuse	*vu;
					GET_VERTEXUSE( vu, m );
					vu->l.magic = NMG_VERTEXUSE_MAGIC;
					ptrs[subscript] = (long *)vu;
				}
				break;
			case NMG_KIND_VERTEXUSE_A:
				{
					struct vertexuse_a	*vua;
					GET_VERTEXUSE_A( vua, m );
					vua->magic = NMG_VERTEXUSE_A_MAGIC;
					ptrs[subscript] = (long *)vua;
				}
				break;
			case NMG_KIND_VERTEX:
				{
					struct vertex	*v;
					GET_VERTEX( v, m );
					v->magic = NMG_VERTEX_MAGIC;
					RT_LIST_INIT( &v->vu_hd );
					ptrs[subscript] = (long *)v;
				}
				break;
			case NMG_KIND_VERTEX_G:
				{
					struct vertex_g	*vg;
					GET_VERTEX_G( vg, m );
					vg->magic = NMG_VERTEX_G_MAGIC;
					ptrs[subscript] = (long *)vg;
				}
				break;
			default:
				rt_log("bad kind = %d\n", kind);
				ptrs[subscript] = (long *)0;
				break;
			}
#if DEBUG
rt_log("   disk_index=%d, kind=%s, ptr=x%x, final_index=%d\n",
subscript, rt_nmg_kind_names[kind],
ptrs[subscript], nmg_index_of_struct(ptrs[subscript]) );
#endif
			/* new_subscript unused on import except for printf()s */
			ecnt[subscript].new_subscript = nmg_index_of_struct(ptrs[subscript]);
			subscript++;
		}
	}
	return(m);
}

/*
 *			R T _ N M G _ I M P O R T _ I N T E R N A L
 *
 *  Import an NMG from the database format to the internal format.
 *  Apply modeling transformations as well.
 *
 *  Special subscripts are used in the disk file:
 *	-1	indicates a pointer to the rt_list structure which
 *		heads a linked list, and is not the first struct element.
 *	 0	indicates that a null pointer should be used.
 */
int
rt_nmg_import_internal( ip, ep, mat, rebound )
struct rt_db_internal	*ip;
struct rt_external	*ep;
register mat_t		mat;
int			rebound;
{
	struct model			*m;
	union record			*rp;
	int				kind_counts[NMG_N_KINDS];
	char				*cp;
	long				**real_ptrs;
	long				**ptrs;
	struct nmg_exp_counts		*ecnt;
	int				i;
	int				maxindex;
	int				kind;
	static long			bad_magic = 0x999;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_NMG )  {
		rt_log("rt_nmg_import: defective record\n");
		return(-1);
	}

	/* Obtain counts of each kind of structure */
	maxindex = 1;
	for( kind = 0; kind < NMG_N_KINDS; kind++ )  {
		kind_counts[kind] = rt_glong( rp->nmg.N_structs+4*kind );
		maxindex += kind_counts[kind];
	}
#if DEBUG
	rt_log("import maxindex=%d\n", maxindex);
#endif

	/* Collect overall new subscripts, and structure-specific indices */
	ecnt = (struct nmg_exp_counts *)rt_calloc( maxindex+3,
		sizeof(struct nmg_exp_counts), "ecnt[]" );
	real_ptrs = (long **)rt_calloc( maxindex+3,
		sizeof(long *), "ptrs[]" );
	/* So that indexing [-1] gives an appropriately bogus magic # */
	ptrs = real_ptrs+1;
	ptrs[-1] = &bad_magic;		/* [-1] gives bad magic */
	ptrs[0] = (long *)0;		/* [0] gives NULL */
	ptrs[maxindex] = &bad_magic;	/* [maxindex] gives bad magic */

	/* Allocate storage for all the NMG structs, in ptrs[] */
	m = rt_nmg_ialloc( ptrs, ecnt, kind_counts );

	/* Import each structure, in turn */
	cp = (char *)(rp+1);	/* start at first granule in */
	for( i=1; i < maxindex; i++ )  {
		rt_nmg_idisk( (genptr_t)(ptrs[i]), (genptr_t)cp,
			ecnt, i, ptrs, mat );
		cp += rt_nmg_disk_sizes[ecnt[i].kind];
	}

	if( rebound )  {
		/* Recompute all bounding boxes in model */
		nmg_rebound(m);
	} else {
		/*
		 *  Need to recompute bounding boxes for the faces here.
		 *  Other bounding boxes will exist and be intact if NMG
		 *  exporter wrote the _a structures.
		 */
		for( i=1; i < maxindex; i++ )  {
			if( ecnt[i].kind != NMG_KIND_FACE )  continue;
			nmg_face_bb( (struct face *)ptrs[i] );
		}
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_NMG;
	ip->idb_ptr = (genptr_t)m;

	rt_free( (char *)ecnt, "ecnt[]" );
	rt_free( (char *)real_ptrs, "ptrs[]" );

	return(0);			/* OK */
}

/*
 *			R T _ N M G _ E X P O R T _ I N T E R N A L
 *
 *  The name is added by the caller, in the usual place.
 *
 *  When the "compact" flag is set, bounding boxes from (at present)
 *	nmgregion_a
 *	shell_a
 *	loop_g
 *  are not converted for storage in the database.
 *  They should be re-generated at import time.
 *  (Saving space in face_g is problematic).
 *
 *  If the "compact" flag is not set, then the NMG model is saved, verbatim.
 */
int
rt_nmg_export_internal( ep, ip, local2mm, compact )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
int			compact;
{
	struct model			*m;
	union record			*rp;
	struct nmg_struct_counts	cntbuf;
	long				**ptrs;
	struct nmg_exp_counts		*ecnt;
	int				i;
	int				subscript;
	int				kind_counts[NMG_N_KINDS];
	genptr_t			disk_arrays[NMG_N_KINDS];
	int				additional_grans;
	int				tot_size;
	int				kind;
	char				*cp;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_NMG )  return(-1);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	/* As a by-product, this fills in the ptrs[] array! */
	bzero( (char *)&cntbuf, sizeof(cntbuf) );
	ptrs = nmg_m_struct_count( &cntbuf, m );
#if DEBUG
	nmg_pr_struct_counts( &cntbuf, "Counts in rt_nmg_export" );
#endif

	/* Collect overall new subscripts, and structure-specific indices */
	ecnt = (struct nmg_exp_counts *)rt_calloc( m->maxindex,
		sizeof(struct nmg_exp_counts), "ecnt[]" );
	for( i = 0; i < NMG_N_KINDS; i++ )
		kind_counts[i] = 0;
	subscript = 1;		/* must be larger than DISK_INDEX_NULL */
	for( i=0; i < m->maxindex; i++ )  {
		if( ptrs[i] == (long *)0 )  continue;
		kind = rt_nmg_magic_to_kind( *(ptrs[i]) );
		ecnt[i].per_struct_index = kind_counts[kind]++;
		ecnt[i].kind = kind;
	}
	if( compact )  {
		kind_counts[NMG_KIND_NMGREGION_A] = 0;
		kind_counts[NMG_KIND_SHELL_A] = 0;
		kind_counts[NMG_KIND_LOOP_G] = 0;
	}

	/* Assign new subscripts to ascending guys of same kind */
	for( kind = 0; kind < NMG_N_KINDS; kind++ )  {
		if( compact && ( kind == NMG_KIND_NMGREGION_A ||
		    kind == NMG_KIND_SHELL_A ||
		    kind == NMG_KIND_LOOP_G ) )  {
			/*
			 * Don't assign any new subscripts for them.
		    	 * Instead, use DISK_INDEX_NULL, yielding null ptrs.
		    	 */
			for( i=0; i < m->maxindex; i++ )  {
				if( ptrs[i] == (long *)0 )  continue;
				if( ecnt[i].kind != kind )  continue;
				ecnt[i].new_subscript = DISK_INDEX_NULL;
			}
			continue;
		}
		for( i=0; i < m->maxindex; i++ )  {
			if( ptrs[i] == (long *)0 )  continue;
			if( ecnt[i].kind != kind )  continue;
			ecnt[i].new_subscript = subscript++;
		}
	}

	/* Sanity checking */
#if DEBUG
rt_log("Mapping of old index to new index, and kind\n");
#endif
	for( i=0; i < m->maxindex; i++ )  {
		if( ptrs[i] == (long *)0 )  continue;
#if DEBUG
		rt_log(" %4d %4d %s (%d)\n",
			i, ecnt[i].new_subscript,
			rt_nmg_kind_names[ecnt[i].kind], ecnt[i].kind);
#endif
		if( nmg_index_of_struct(ptrs[i]) != i )  {
			rt_log("***ERROR, ptrs[%d]->index = %d\n",
				i, nmg_index_of_struct(ptrs[i]) );
		}
		if( rt_nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind )  {
			rt_log("@@@ERROR, ptrs[%d] kind(%d) != %d\n",
				i, rt_nmg_magic_to_kind(*ptrs[i]),
				ecnt[i].kind);
		}
	}

	tot_size = 0;
	for( i = 0; i < NMG_N_KINDS; i++ )  {
#if DEBUG
		rt_log("%d of kind %s (%d)\n",
			kind_counts[i], rt_nmg_kind_names[i], i);
#endif
		if( kind_counts[i] <= 0 )  {
			disk_arrays[i] = GENPTR_NULL;
			continue;
		}
		tot_size += kind_counts[i] * rt_nmg_disk_sizes[i];
	}
	additional_grans = (tot_size + sizeof(union record)-1) / sizeof(union record);
	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (1 + additional_grans) * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "nmg external");
	rp = (union record *)ep->ext_buf;
	rp->nmg.N_id = DBID_NMG;
	(void)rt_plong( rp->nmg.N_count, additional_grans );

	/* Record counts of each kind of structure */
	for( kind = 0; kind < NMG_N_KINDS; kind++ )  {
		(void)rt_plong( rp->nmg.N_structs+4*kind, kind_counts[kind] );
	}

	cp = (char *)(rp+1);	/* advance one granule */
	for( i=0; i < NMG_N_KINDS; i++ )  {
		disk_arrays[i] = (genptr_t)cp;
		cp += kind_counts[i] * rt_nmg_disk_sizes[i];
	}

	/* Convert all the structures to their disk versions */
	for( i = m->maxindex-1; i >= 0; i-- )  {
		if( ptrs[i] == (long *)0 )  continue;
		kind = ecnt[i].kind;
		if( kind_counts[kind] <= 0 )  continue;
		rt_nmg_edisk( (genptr_t)(disk_arrays[kind]),
			(genptr_t)(ptrs[i]), ecnt, i, local2mm );
	}

	rt_free( (char *)ptrs, "ptrs[]" );
	rt_free( (char *)ecnt, "ecnt[]" );

	return(0);
}

/*
 *			R T _ N M G _ I M P O R T
 *
 *  Import an NMG from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_nmg_import( ip, ep, mat )
struct rt_db_internal	*ip;
struct rt_external	*ep;
register mat_t		mat;
{
	struct model			*m;
	struct nmgregion		*r;
	union record			*rp;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_NMG )  {
		rt_log("rt_nmg_import: defective record\n");
		return(-1);
	}

	if( rt_nmg_import_internal( ip, ep, mat, 1 ) < 0 )
		return(-1);

	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	return(0);			/* OK */
}

/*
 *			R T _ N M G _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 *
 */
int
rt_nmg_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
	struct model			*m;
	union record			*rp;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_NMG )  return(-1);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	/* The "compact" flag is used to save space in the database */
	return  rt_nmg_export_internal( ep, ip, local2mm, 1 );
}

/*
 *			R T _ N M G _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_nmg_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct model	*m =
		(struct model *)ip->idb_ptr;
	char	buf[256];

	NMG_CK_MODEL(m);
	rt_vls_strcat( str, "n-Manifold Geometry solid (NMG)\n");

	if( !verbose )  return(0);

	/* Should print out # of database granules used */
	/* If verbose, should print out structure counts */

	return(0);
}

/*
 *			R T _ N M G _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_nmg_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct model	*m;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);
	nmg_km( m );

	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
