#define DEBUG	0
/*
 *			G _ N M G . C
 *
 *  Purpose -
 *	Intersect a ray with an NMG solid
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "./debug.h"

/* rt_nmg_internal is just "model", from nmg.h */

#define	NMG_SPEC_START_MAGIC	6014061
#define	NMG_SPEC_END_MAGIC		7013061

/* This is the solid information specific to an nmg solid */
struct nmg_specific {
	int			nmg_smagic;	/* STRUCT START magic number */
	struct model		*nmg_model;
	char			*manifolds; /*  structure 1-3manifold table */
	vect_t			nmg_invdir;
	int			nmg_emagic;	/* STRUCT END magic number */
};


/*
 *  			R T _ N M G _ P R E P
 *  
 *  Given a pointer to a ged database record, and a transformation matrix,
 *  determine if this is a valid nmg, and if so, precompute various
 *  terms of the formula.
 *  
 *  returns -
 *  	0	nmg is ok
 *  	!0	error in description
 *  
 *  implicit return -
 *  	a struct nmg_specific is created, and it's address is stored in
 *  	stp->st_specific for use by nmg_shot().
 */
int
rt_nmg_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct model		*m;
	register struct nmg_specific	*nmg_s;
	struct nmgregion *rp;
	vect_t work;	

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	GETSTRUCT( nmg_s, nmg_specific );
	stp->st_specific = (genptr_t)nmg_s;
	nmg_s->nmg_model = m;
	ip->idb_ptr = (genptr_t)NULL;
	nmg_s->nmg_smagic = NMG_SPEC_START_MAGIC;
	nmg_s->nmg_emagic = NMG_SPEC_END_MAGIC;

	/* get bounding box of NMG solid */
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

		nmg_ck_vs_in_region( rp , &rtip->rti_tol );

	}

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE(work);

	/* build table indicating the manifold level
	 * of each sub-element of NMG solid
	 */
	nmg_s->manifolds = nmg_manifolds(m);





	return(0);
}

/*
 *			R T _ N M G _ P R I N T
 */
void
rt_nmg_print( stp )
register CONST struct soltab *stp;
{
	register struct model *m =
		(struct model *)stp->st_specific;

	NMG_CK_MODEL(m);
	nmg_pr_m(m);
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
rt_nmg_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;	/* info about the ray */
struct application	*ap;	
struct seg		*seghead;	/* intersection w/ ray */
{
	struct ray_data rd;
	int status;
	struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;

	if(rt_g.NMG_debug & DEBUG_NMGRT) {
		rt_log("rt_nmg_shot()\n\t");
		rt_pr_tol(&ap->a_rt_i->rti_tol);
	}

	/* check validity of nmg specific structure */
 	if (nmg->nmg_smagic != NMG_SPEC_START_MAGIC)
		rt_bomb("start of NMG st_specific structure corrupted\n");

 	if (nmg->nmg_emagic != NMG_SPEC_END_MAGIC)
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

	/* build the NMG per-ray data structure */
	rd.rd_m = nmg->nmg_model;
	rd.manifolds = nmg->manifolds;
	VMOVE(rd.rd_invdir, nmg->nmg_invdir);
	rd.rp = rp;
	rd.tol = &ap->a_rt_i->rti_tol;
	rd.ap = ap;
	rd.stp = stp;
	rd.seghead = seghead;
	rd.classifying_ray = 0;
	
	/* create a table to keep track of which elements have been
	 * processed before and which haven't.  Elements in this table
	 * will either be:
	 *		(NULL)		item not previously processed
	 *		hitmiss ptr	item previously processed
	 */
	rd.hitmiss = (struct hitmiss **)rt_calloc( rd.rd_m->maxindex,
		sizeof(struct hitmiss *), "nmg geom hit list");

	/* initialize the lists of things that have been hit/missed */
	RT_LIST_INIT(&rd.rd_hit);
	RT_LIST_INIT(&rd.rd_miss);

	/* intersect the ray with the geometry */
	nmg_isect_ray_model(&rd);

	/* build the segment lists */
	status = nmg_ray_segs(&rd);

	/* free the hitmiss table */
	rt_free( (char *)rd.hitmiss, "free nmg geom hit list");

	return(status);
}


#define RT_NMG_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ N M G _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nmg_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
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
	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
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
/*	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific; */

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
/*	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific; */
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
 *			R T _ N M G _ P L O T
 */
int
rt_nmg_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct rt_tol	*tol;
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
 * XXX This routine "destroys" the internal nmg solid.
 * This means that once you tesselate an NMG solid, your in-memory
 * copy becomes invalid, and you can't do anything else with it
 * until you get a new copy from disk.
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
CONST struct rt_tol	*tol;
{
	LOCAL struct model	*lm;

	NMG_CK_MODEL(m);

	RT_CK_DB_INTERNAL(ip);
	lm = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(lm);

	if( RT_LIST_IS_EMPTY( &(lm->r_hd) ) )  {
		/* No regions in imported geometry, can't give valid 'r' */
		*r = (struct nmgregion *)NULL;
		return -1;
	}

	/* XXX A big hack, just for testing ***/

	*r = RT_LIST_FIRST(nmgregion, &(lm->r_hd) );
	NMG_CK_REGION(*r);
	if( RT_LIST_NEXT_NOT_HEAD( *r, &(lm->r_hd) ) )  {
		struct nmgregion *r2;

		r2 = RT_LIST_PNEXT( nmgregion, &((*r)->l) );
		while( RT_LIST_NOT_HEAD( &r2->l, &(lm->r_hd) ) )
		{
			struct nmgregion *next_r;

			next_r = RT_LIST_PNEXT( nmgregion, &r2->l );
			nmg_merge_regions( *r, r2, tol );

			r2 = next_r;
		}
	}


	/* XXX The next two lines "destroy" the internal nmg solid.
	 * This means that once you tesselate an NMG solid, your in-memory
	 * copy becomes invalid, and you can't do anything else with it
	 * until you get a new copy from disk.
	 */
	nmg_merge_models(m, lm);
	ip->idb_ptr = GENPTR_NULL;

	return(0);
}

#define RT_CK_DISKMAGIC(_cp,_magic)	\
	if( rt_glong(_cp) != _magic )  { \
		rt_log("RT_CK_DISKMAGIC: magic mis-match, got x%x, s/b x%x, file %s, line %d\n", \
			rt_glong(_cp), _magic, __FILE__, __LINE__); \
		rt_bomb("bad magic\n"); \
	}

/*
 * ----------------------------------------------------------------------
 *
 *  Definitions for the binary, machine-independent format of
 *  the NMG data structures.
 *
 *  There are two special values that may be assigned to an disk_index_t
 *  to signal special processing when the structure is re-imported.
 */
#define DISK_INDEX_NULL		0
#define DISK_INDEX_LISTHEAD	-1

#define DISK_MODEL_VERSION	1	/* V0 was Release 4.0 */

typedef	unsigned char	disk_index_t[4];
struct disk_rt_list  {
	disk_index_t		forw;
	disk_index_t		back;
};

#define DISK_MODEL_MAGIC	0x4e6d6f64	/* Nmod */
struct disk_model {
	unsigned char		magic[4];
	unsigned char		version[4];	/* unused */
	struct disk_rt_list	r_hd;
};

#define DISK_REGION_MAGIC	0x4e726567	/* Nreg */
struct disk_nmgregion {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t   		m_p;
	disk_index_t		ra_p;
	struct disk_rt_list	s_hd;
};

#define DISK_REGION_A_MAGIC	0x4e725f61	/* Nr_a */
struct disk_nmgregion_a {
	unsigned char		magic[4];
	unsigned char		min_pt[3*8];
	unsigned char		max_pt[3*8];
};

#define DISK_SHELL_MAGIC	0x4e73686c	/* Nshl */
struct disk_shell {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t		r_p;
	disk_index_t		sa_p;
	struct disk_rt_list	fu_hd;
	struct disk_rt_list	lu_hd;
	struct disk_rt_list	eu_hd;
	disk_index_t			vu_p;
};

#define DISK_SHELL_A_MAGIC	0x4e735f61	/* Ns_a */
struct disk_shell_a {
	unsigned char		magic[4];
	unsigned char		min_pt[3*8];
	unsigned char		max_pt[3*8];
};

#define DISK_FACE_MAGIC		0x4e666163	/* Nfac */
struct disk_face {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t		fu_p;
	disk_index_t		g;
	unsigned char		flip[4];
};

#define DISK_FACE_G_PLANE_MAGIC	0x4e666770	/* Nfgp */
struct disk_face_g_plane {
	unsigned char		magic[4];
	struct disk_rt_list	f_hd;
	unsigned char		N[4*8];
};

#define DISK_FACE_G_SNURB_MAGIC	0x4e666773	/* Nfgs */
struct disk_face_g_snurb {
	unsigned char		magic[4];
	struct disk_rt_list	f_hd;
	unsigned char		u_order[4];
	unsigned char		v_order[4];
	unsigned char		u_size[4];	/* u.k_size */
	unsigned char		v_size[4];	/* v.k_size */
	disk_index_t		u_knots;	/* u.knots subscript */
	disk_index_t		v_knots;	/* v.knots subscript */
	unsigned char		us_size[4];
	unsigned char		vs_size[4];
	unsigned char		pt_type[4];
	disk_index_t		ctl_points;	/* subscript */
};

#define DISK_FACEUSE_MAGIC	0x4e667520	/* Nfu */
struct disk_faceuse {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t		s_p;
	disk_index_t		fumate_p;
	unsigned char		orientation[4];
	disk_index_t		f_p;
	disk_index_t		fua_p;
	struct disk_rt_list	lu_hd;
};

#define DISK_LOOP_MAGIC		0x4e6c6f70	/* Nlop */
struct disk_loop {
	unsigned char		magic[4];
	disk_index_t		lu_p;
	disk_index_t		lg_p;
};

#define DISK_LOOP_G_MAGIC	0x4e6c5f67	/* Nl_g */
struct disk_loop_g {
	unsigned char		magic[4];
	unsigned char		min_pt[3*8];
	unsigned char		max_pt[3*8];
};

#define DISK_LOOPUSE_MAGIC	0x4e6c7520	/* Nlu */
struct disk_loopuse {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t		up;
	disk_index_t		lumate_p;
	unsigned char		orientation[4];
	disk_index_t		l_p;
	disk_index_t		lua_p;
	struct disk_rt_list	down_hd;
};

#define DISK_EDGE_MAGIC		0x4e656467	/* Nedg */
struct disk_edge {
	unsigned char		magic[4];
	disk_index_t		eu_p;
	unsigned char		is_real[4];
};

#define DISK_EDGE_G_LSEG_MAGIC	0x4e65676c	/* Negl */
struct disk_edge_g_lseg {
	unsigned char		magic[4];
	struct disk_rt_list	eu_hd2;
	unsigned char		e_pt[3*8];
	unsigned char		e_dir[3*8];
};

#define DISK_EDGE_G_CNURB_MAGIC	0x4e656763	/* Negc */
struct disk_edge_g_cnurb {
	unsigned char		magic[4];
	struct disk_rt_list	eu_hd2;
	unsigned char		order[4];
	unsigned char		k_size[4];	/* k.k_size */
	disk_index_t		knots;		/* knot.knots subscript */
	unsigned char		c_size[4];
	unsigned char		pt_type[4];
	disk_index_t		ctl_points;	/* subscript */
};

#define DISK_EDGEUSE_MAGIC	0x4e657520	/* Neu */
struct disk_edgeuse {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	struct disk_rt_list	l2;
	disk_index_t		up;
	disk_index_t		eumate_p;
	disk_index_t		radial_p;
	disk_index_t		e_p;
	disk_index_t		eua_p;
	unsigned char  		orientation[4];
	disk_index_t		vu_p;
	disk_index_t		g;
};

#define DISK_VERTEX_MAGIC	0x4e767274	/* Nvrt */
struct disk_vertex {
	unsigned char		magic[4];
	struct disk_rt_list	vu_hd;
	disk_index_t			vg_p;
};

#define DISK_VERTEX_G_MAGIC	0x4e765f67	/* Nv_g */
struct disk_vertex_g {
	unsigned char		magic[4];
	unsigned char		coord[3*8];
};

#define DISK_VERTEXUSE_MAGIC	0x4e767520	/* Nvu */
struct disk_vertexuse {
	unsigned char		magic[4];
	struct disk_rt_list	l;
	disk_index_t		up;
	disk_index_t		v_p;
	disk_index_t		a;
};

#define DISK_VERTEXUSE_A_PLANE_MAGIC	0x4e767561	/* Nvua */
struct disk_vertexuse_a_plane {
	unsigned char		magic[4];
	unsigned char		N[3*8];
};

#define DISK_VERTEXUSE_A_CNURB_MAGIC	0x4e766163	/* Nvac */
struct disk_vertexuse_a_cnurb {
	unsigned char		magic[4];
	unsigned char		param[3*8];
};

#define DISK_DOUBLE_ARRAY_MAGIC	0x4e666172		/* Narr */
struct disk_double_array  {
	unsigned char		magic[4];
	unsigned char		ndouble[4];	/* # of doubles to follow */
	unsigned char		vals[1*8];	/* actually [ndouble*8] */
};

/* ---------------------------------------------------------------------- */
/* All these arrays and defines have to use the same implicit index values */
#define NMG_KIND_MODEL		0
#define NMG_KIND_NMGREGION	1
#define NMG_KIND_NMGREGION_A	2
#define NMG_KIND_SHELL		3
#define NMG_KIND_SHELL_A	4
#define NMG_KIND_FACEUSE	5
#define NMG_KIND_FACE		6
#define NMG_KIND_FACE_G_PLANE	7
#define NMG_KIND_FACE_G_SNURB	8
#define NMG_KIND_LOOPUSE	9
#define NMG_KIND_LOOP		10
#define NMG_KIND_LOOP_G		11
#define NMG_KIND_EDGEUSE	12
#define NMG_KIND_EDGE		13
#define NMG_KIND_EDGE_G_LSEG	14
#define NMG_KIND_EDGE_G_CNURB	15
#define NMG_KIND_VERTEXUSE	16
#define NMG_KIND_VERTEXUSE_A_PLANE	17
#define NMG_KIND_VERTEXUSE_A_CNURB	18
#define NMG_KIND_VERTEX		19
#define NMG_KIND_VERTEX_G	20
/* 21 through 24 are unassigned, and reserved for future use */

#define NMG_KIND_DOUBLE_ARRAY	25		/* special, variable sized */

/* This number must have some extra space, for upwards compatability */
/* 26 is the limit, in the current incarnation of db.h */
#define NMG_N_KINDS		26		/* number of kinds */

CONST int	rt_nmg_disk_sizes[NMG_N_KINDS] = {
	sizeof(struct disk_model),		/* 0 */
	sizeof(struct disk_nmgregion),
	sizeof(struct disk_nmgregion_a),
	sizeof(struct disk_shell),
	sizeof(struct disk_shell_a),
	sizeof(struct disk_faceuse),
	sizeof(struct disk_face),
	sizeof(struct disk_face_g_plane),
	sizeof(struct disk_face_g_snurb),
	sizeof(struct disk_loopuse),
	sizeof(struct disk_loop),		/* 10 */
	sizeof(struct disk_loop_g),
	sizeof(struct disk_edgeuse),
	sizeof(struct disk_edge),
	sizeof(struct disk_edge_g_lseg),
	sizeof(struct disk_edge_g_cnurb),
	sizeof(struct disk_vertexuse),
	sizeof(struct disk_vertexuse_a_plane),
	sizeof(struct disk_vertexuse_a_cnurb),
	sizeof(struct disk_vertex),
	sizeof(struct disk_vertex_g),		/* 20 */
	0,
	0,
	0,
	0,
	0  /* disk_double_array, MUST BE ZERO */	/* 25: MUST BE ZERO */
};
CONST char	rt_nmg_kind_names[NMG_N_KINDS+2][18] = {
	"model",				/* 0 */
	"nmgregion",
	"nmgregion_a",
	"shell",
	"shell_a",
	"faceuse",
	"face",
	"face_g_plane",
	"face_g_snurb",
	"loopuse",
	"loop",					/* 10 */
	"loop_g",
	"edgeuse",
	"edge",
	"edge_g_lseg",
	"edge_g_cnurb",
	"vertexuse",
	"vertexuse_a_plane",
	"vertexuse_a_cnurb",
	"vertex",
	"vertex_g",				/* 20 */
	"k21",
	"k22",
	"k23",
	"k24",
	"double_array",				/* 25 */
	"k26-OFF_END",
	"k27-OFF_END"
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
	case NMG_FACE_MAGIC:
		return NMG_KIND_FACE;
	case NMG_FACE_G_PLANE_MAGIC:
		return NMG_KIND_FACE_G_PLANE;
	case NMG_FACE_G_SNURB_MAGIC:
		return NMG_KIND_FACE_G_SNURB;
	case NMG_LOOPUSE_MAGIC:
		return NMG_KIND_LOOPUSE;
	case NMG_LOOP_G_MAGIC:
		return NMG_KIND_LOOP_G;
	case NMG_LOOP_MAGIC:
		return NMG_KIND_LOOP;
	case NMG_EDGEUSE_MAGIC:
		return NMG_KIND_EDGEUSE;
	case NMG_EDGE_MAGIC:
		return NMG_KIND_EDGE;
	case NMG_EDGE_G_LSEG_MAGIC:
		return NMG_KIND_EDGE_G_LSEG;
	case NMG_EDGE_G_CNURB_MAGIC:
		return NMG_KIND_EDGE_G_CNURB;
	case NMG_VERTEXUSE_MAGIC:
		return NMG_KIND_VERTEXUSE;
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
		return NMG_KIND_VERTEXUSE_A_PLANE;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
		return NMG_KIND_VERTEXUSE_A_CNURB;
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
	long	first_fastf_relpos;	/* for snurb and cnurb. */
	long	byte_offset;		/* for snurb and cnurb. */
};

/* XXX These are horribly non-PARALLEL, and they *must* be PARALLEL ! */
static unsigned char	*rt_nmg_fastf_p;
static unsigned int	rt_nmg_cur_fastf_subscript;

/*
 *			R T _ N M G _ E X P O R T _ F A S T F
 *
 *  Format a variable sized array of fastf_t's into external format
 *  (IEEE big endian double precision) with a 2 element header.
 *
 *		+-----------+
 *		|  magic    |
 *		+-----------+
 *		|  count    |
 *		+-----------+
 *		|           |
 *		~  doubles  ~
 *		~    :      ~
 *		|           |
 *		+-----------+
 *
 *  Increments the pointer to the next free byte in the external array,
 *  and increments the subscript number of the next free array.
 *
 *  Note that this subscript number is consistent with the rest of the
 *  NMG external subscript numbering, so that the first disk_double_array
 *  subscript will be one larger than the largest disk_vertex_g subscript,
 *  and in the external record the array of fastf_t arrays will follow
 *  the array of disk_vertex_g structures.
 *
 *  Returns -
 *	subscript number of this array, in the external form.
 */
int
rt_nmg_export_fastf( fp, count, pt_type, scale )
CONST fastf_t	*fp;
int		count;
int		pt_type;	/* If zero, means literal array of values */
double		scale;
{
	register unsigned char	*cp;

	if( pt_type )
		count *= RT_NURB_EXTRACT_COORDS(pt_type);

	cp = rt_nmg_fastf_p;
	(void)rt_plong( cp + 0, DISK_DOUBLE_ARRAY_MAGIC );
	(void)rt_plong( cp + 4, count );
	if( pt_type == 0 || scale == 1.0 )  {
		htond( cp + (4+4), (unsigned char *)fp, count );
	} else {
		fastf_t		*new;

		/* Need to scale data by 'scale' ! */
		new = (fastf_t *)rt_malloc( count*sizeof(fastf_t), "rt_nmg_export_fastf" );
		if( RT_NURB_IS_PT_RATIONAL(pt_type) )  {
			/* Don't scale the homogeneous (rational) coord */
			register int	i;
			int		nelem;	/* # elements per tuple */

			nelem = RT_NURB_EXTRACT_COORDS(pt_type);
			for( i = 0; i < count; i += nelem )  {
				VSCALEN( &new[i], &fp[i], scale, nelem-1 );
				new[i+nelem-1] = fp[i+nelem-1];
			}
		} else {
			/* Scale everything as one long array */
			VSCALEN( new, fp, scale, count );
		}
		htond( cp + (4+4), (unsigned char *)new, count );
		rt_free( (char *)new, "rt_nmg_export_fastf" );
	}
	cp += (4+4) + count * 8;
	rt_nmg_fastf_p = cp;
	return rt_nmg_cur_fastf_subscript++;
}

/*
 *			R T _ N M G _ I M P O R T _ F A S T F
 */
fastf_t *
rt_nmg_import_fastf( base, ecnt, subscript, mat, len, pt_type )
CONST unsigned char	*base;
struct nmg_exp_counts	*ecnt;
long			subscript;
CONST matp_t		mat;
int			len;		/* expected size */
int			pt_type;
{
	CONST unsigned char	*cp;
	register int		count;
	fastf_t			*ret;
	fastf_t			*tmp;

	if( ecnt[subscript].byte_offset <= 0 || ecnt[subscript].kind != NMG_KIND_DOUBLE_ARRAY )  {
		rt_log("subscript=%d, byte_offset=%d, kind=%d (expected %d)\n",
			subscript, ecnt[subscript].byte_offset,
			ecnt[subscript].kind, NMG_KIND_DOUBLE_ARRAY );
		rt_bomb("rt_nmg_import_fastf() bad ecnt table\n");
	}


	cp = base + ecnt[subscript].byte_offset;
	if( rt_glong( cp ) != DISK_DOUBLE_ARRAY_MAGIC )  {
		rt_log("magic mis-match, got x%x, s/b x%x, file %s, line %d\n",
			rt_glong(cp), DISK_DOUBLE_ARRAY_MAGIC, __FILE__, __LINE__);
		rt_log("subscript=%d, byte_offset=%d\n",
			 subscript, ecnt[subscript].byte_offset);
		rt_bomb("rt_nmg_import_fastf() bad magic\n");
	}

	if( pt_type )
		len *= RT_NURB_EXTRACT_COORDS(pt_type);

	count = rt_glong( cp + 4 );
	if( count != len )  {
		rt_log("rt_nmg_import_fastf() subscript=%d, expected len=%d, got=%d\n",
			subscript, len, count );
		rt_bomb("rt_nmg_import_fastf()\n");
	}
	ret = (fastf_t *)rt_malloc( count * sizeof(fastf_t), "rt_nmg_import_fastf[]" );
	if( !mat )  {
		ntohd( (unsigned char *)ret, cp + (4+4), count );
		return ret;
	}

	/*
	 *  An amazing amount of work: transform all points by 4x4 mat.
	 *  Need to know width of data points, may be 3, or 4-tuples.
	 *  The vector times matrix calculation can't be done in place.
	 */
	tmp = (fastf_t *)rt_malloc( count * sizeof(fastf_t), "rt_nmg_import_fastf tmp[]" );
	ntohd( (unsigned char *)tmp, cp + (4+4), count );
	switch( RT_NURB_EXTRACT_COORDS(pt_type) )  {
	case 3:
		if( RT_NURB_IS_PT_RATIONAL(pt_type) )  rt_bomb("rt_nmg_import_fastf() Rational 3-tuple?\n");
		for( count -= 3 ; count >= 0; count -= 3 )  {
			MAT4X3PNT( &ret[count], mat, &tmp[count] );
		}
		break;
	case 4:
		if( !RT_NURB_IS_PT_RATIONAL(pt_type) )  rt_bomb("rt_nmg_import_fastf() non-rational 4-tuple?\n");
		for( count -= 4 ; count >= 0; count -= 4 )  {
			MAT4X4PNT( &ret[count], mat, &tmp[count] );
		}
		break;
	default:
		rt_bomb("rt_nmg_import_fastf() unsupported # of coords in ctl_point\n");
	}
	rt_free( (char *)tmp, "rt_nmg_import_fastf tmp[]" );
	return ret;
}

/*
 *			R T _ N M G _ R E I N D E X
 *
 *  Depends on ecnt[0].byte_offset having been set to maxindex.
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
	int	ret=0;	/* zero is NOT the default value, this is just to satisfy CRAY compilers */

	/* If null pointer, return new subscript of zero */
	if( p == 0 )  {
		ret = 0;
		index = 0;	/* sanity */
	} else {
		index = nmg_index_of_struct((long *)(p));
		if( index == -1 )  {
			ret = DISK_INDEX_LISTHEAD; /* FLAG:  special list head */
		} else if( index < -1 ) {
			rt_bomb("rt_nmg_reindex(): unable to obtain struct index\n");
		} else {
			ret = ecnt[index].new_subscript;
			if( ecnt[index].kind < 0 )  {
				rt_log("rt_nmg_reindex(p=x%x), p->index=%d, ret=%d, kind=%d\n", p, index, ret, ecnt[index].kind);
				rt_bomb("rt_nmg_reindex() This index not found in ecnt[]\n");
			}
			/* ret == 0 on supressed loop_g ptrs, etc */
			if( ret < 0 || ret > ecnt[0].byte_offset )  {
				rt_log("rt_nmg_reindex(p=x%x) %s, p->index=%d, ret=%d, maxindex=%d\n",
					p,
					rt_identify_magic(*(long *)p),
					index, ret, ecnt[0].byte_offset);
				rt_bomb("rt_nmg_reindex() subscript out of range\n");
			}
		}
	}
/*rt_log("rt_nmg_reindex(p=x%x), p->index=%d, ret=%d\n", p, index, ret);*/
	return( ret );
}

/* forw may never be null;  back may be null for loopuse (sigh) */
#define INDEX(o,i,elem)	\
	(void)rt_plong(&(o)->elem[0], rt_nmg_reindex((genptr_t)((i)->elem), ecnt))
#define INDEXL(oo,ii,elem)	{ \
	register long _f = rt_nmg_reindex((genptr_t)((ii)->elem.forw), ecnt); \
	if( _f == DISK_INDEX_NULL )  rt_log("Warning rt_nmg_edisk: reindex forw to null?\n"); \
	(void)rt_plong( (oo)->elem.forw, _f ); \
	(void)rt_plong( (oo)->elem.back, rt_nmg_reindex((genptr_t)((ii)->elem.back), ecnt) ); }
#define PUTMAGIC(_magic)	(void)rt_plong( &d->magic[0], _magic )

/*
 *			R T _ N M G _ E D I S K
 *
 *  Export a given structure from memory to disk format
 *
 *  Scale geometry by 'local2mm'
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
			PUTMAGIC( DISK_MODEL_MAGIC );
			rt_plong( d->version, 0 );
			INDEXL( d, m, r_hd );
		}
		return;
	case NMG_KIND_NMGREGION:
		{
			struct nmgregion	*r = (struct nmgregion *)ip;
			struct disk_nmgregion	*d;
			d = &((struct disk_nmgregion *)op)[oindex];
			NMG_CK_REGION(r);
			PUTMAGIC( DISK_REGION_MAGIC );
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
			PUTMAGIC( DISK_REGION_A_MAGIC );
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
			PUTMAGIC( DISK_SHELL_MAGIC );
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
			PUTMAGIC( DISK_SHELL_A_MAGIC );
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
			PUTMAGIC( DISK_FACEUSE_MAGIC );
			INDEXL( d, fu, l );
			INDEX( d, fu, s_p );
			INDEX( d, fu, fumate_p );
			rt_plong( d->orientation, fu->orientation );
			INDEX( d, fu, f_p );
			INDEXL( d, fu, lu_hd );
		}
		return;
	case NMG_KIND_FACE:
		{
			struct face	*f = (struct face *)ip;
			struct disk_face	*d;
			d = &((struct disk_face *)op)[oindex];
			NMG_CK_FACE(f);
			PUTMAGIC( DISK_FACE_MAGIC );
			INDEXL( d, f, l );	/* face is member of fg list */
			INDEX( d, f, fu_p );
			rt_plong( d->g, rt_nmg_reindex((genptr_t)(f->g.magic_p), ecnt) );
			rt_plong( d->flip, f->flip );
		}
		return;
	case NMG_KIND_FACE_G_PLANE:
		{
			struct face_g_plane	*fg = (struct face_g_plane *)ip;
			struct disk_face_g_plane	*d;
			plane_t			plane;
			d = &((struct disk_face_g_plane *)op)[oindex];
			NMG_CK_FACE_G_PLANE(fg);
			PUTMAGIC( DISK_FACE_G_PLANE_MAGIC );
			INDEXL( d, fg, f_hd );
			VMOVE( plane, fg->N );
			plane[3] = fg->N[3] * local2mm;
			htond( d->N, plane, 4 );
		}
		return;
	case NMG_KIND_FACE_G_SNURB:
		{
			struct face_g_snurb	*fg = (struct face_g_snurb *)ip;
			struct disk_face_g_snurb	*d;

			d = &((struct disk_face_g_snurb *)op)[oindex];
			NMG_CK_FACE_G_SNURB(fg);
			PUTMAGIC( DISK_FACE_G_SNURB_MAGIC );
			INDEXL( d, fg, f_hd );
			rt_plong( d->u_order, fg->order[0] );
			rt_plong( d->v_order, fg->order[1] );
			rt_plong( d->u_size, fg->u.k_size );
			rt_plong( d->v_size, fg->v.k_size );
			rt_plong( d->u_knots,
				rt_nmg_export_fastf( fg->u.knots,
					fg->u.k_size, 0, 1.0 ) );
			rt_plong( d->v_knots,
				rt_nmg_export_fastf( fg->v.knots,
					fg->v.k_size, 0, 1.0 ) );
			rt_plong( d->us_size, fg->s_size[0] );
			rt_plong( d->vs_size, fg->s_size[1] );
			rt_plong( d->pt_type, fg->pt_type );
			/* scale XYZ ctl_points by local2mm */
			rt_plong( d->ctl_points,
				rt_nmg_export_fastf( fg->ctl_points,
					fg->s_size[0] * fg->s_size[1],
					fg->pt_type,
					local2mm ) );
		}
		return;
	case NMG_KIND_LOOPUSE:
		{
			struct loopuse	*lu = (struct loopuse *)ip;
			struct disk_loopuse	*d;
			d = &((struct disk_loopuse *)op)[oindex];
			NMG_CK_LOOPUSE(lu);
			PUTMAGIC( DISK_LOOPUSE_MAGIC );
			INDEXL( d, lu, l );
			rt_plong( d->up, rt_nmg_reindex((genptr_t)(lu->up.magic_p), ecnt) );
			INDEX( d, lu, lumate_p );
			rt_plong( d->orientation, lu->orientation );
			INDEX( d, lu, l_p );
			INDEXL( d, lu, down_hd );
		}
		return;
	case NMG_KIND_LOOP:
		{
			struct loop	*loop = (struct loop *)ip;
			struct disk_loop	*d;
			d = &((struct disk_loop *)op)[oindex];
			NMG_CK_LOOP(loop);
			PUTMAGIC( DISK_LOOP_MAGIC );
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
			PUTMAGIC( DISK_LOOP_G_MAGIC );
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
			PUTMAGIC( DISK_EDGEUSE_MAGIC );
			INDEXL( d, eu, l );
			/* NOTE The pointers in l2 point at other l2's.
			 * nmg_index_of_struct() will point 'em back
			 * at the top of the edgeuse.  Beware on import.
			 */
			INDEXL( d, eu, l2 );
			rt_plong( d->up, rt_nmg_reindex((genptr_t)(eu->up.magic_p), ecnt) );
			INDEX( d, eu, eumate_p );
			INDEX( d, eu, radial_p );
			INDEX( d, eu, e_p );
			rt_plong( d->orientation, eu->orientation);
			INDEX( d, eu, vu_p );
			rt_plong( d->g, rt_nmg_reindex((genptr_t)(eu->g.magic_p), ecnt) );
		}
		return;
	case NMG_KIND_EDGE:
		{
			struct edge	*e = (struct edge *)ip;
			struct disk_edge	*d;
			d = &((struct disk_edge *)op)[oindex];
			NMG_CK_EDGE(e);
			PUTMAGIC( DISK_EDGE_MAGIC );
			rt_plong( d->is_real, e->is_real );
			INDEX( d, e, eu_p );
		}
		return;
	case NMG_KIND_EDGE_G_LSEG:
		{
			struct edge_g_lseg	*eg = (struct edge_g_lseg *)ip;
			struct disk_edge_g_lseg	*d;
			point_t			pt;
			d = &((struct disk_edge_g_lseg *)op)[oindex];
			NMG_CK_EDGE_G_LSEG(eg);
			PUTMAGIC( DISK_EDGE_G_LSEG_MAGIC );
			INDEXL( d, eg, eu_hd2 );
			VSCALE( pt, eg->e_pt, local2mm );
			htond( d->e_pt, pt, 3);
			htond( d->e_dir, eg->e_dir, 3);
		}
		return;
	case NMG_KIND_EDGE_G_CNURB:
		{
			struct edge_g_cnurb	*eg = (struct edge_g_cnurb *)ip;
			struct disk_edge_g_cnurb	*d;
			d = &((struct disk_edge_g_cnurb *)op)[oindex];
			NMG_CK_EDGE_G_CNURB(eg);
			PUTMAGIC( DISK_EDGE_G_CNURB_MAGIC );
			INDEXL( d, eg, eu_hd2 );
			rt_plong( d->order, eg->order );

			/* If order is zero, everything else is NULL */
			if( eg->order == 0 )  return;

			rt_plong( d->k_size, eg->k.k_size );
			rt_plong( d->knots,
				rt_nmg_export_fastf( eg->k.knots,
					eg->k.k_size, 0, 1.0 ) );
			rt_plong( d->c_size, eg->c_size );
			rt_plong( d->pt_type, eg->pt_type );
			/*
			 * The curve's control points are in parameter space
			 * for cnurbs on snurbs, and in XYZ for cnurbs on planar faces.
			 * UV values do NOT get transformed, XYZ values do!
			 */
			rt_plong( d->ctl_points,
				rt_nmg_export_fastf( eg->ctl_points,
					eg->c_size,
					eg->pt_type,
					RT_NURB_EXTRACT_PT_TYPE( eg->pt_type ) == RT_NURB_PT_UV ?
						1.0 : local2mm ) );
		}
		return;
	case NMG_KIND_VERTEXUSE:
		{
			struct vertexuse	*vu = (struct vertexuse *)ip;
			struct disk_vertexuse	*d;
			d = &((struct disk_vertexuse *)op)[oindex];
			NMG_CK_VERTEXUSE(vu);
			PUTMAGIC( DISK_VERTEXUSE_MAGIC );
			INDEXL( d, vu, l );
			rt_plong( d->up,
				rt_nmg_reindex((genptr_t)(vu->up.magic_p), ecnt) );
			INDEX( d, vu, v_p );
			if(vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
			rt_plong( d->a,
				rt_nmg_reindex((genptr_t)(vu->a.magic_p), ecnt) );
		}
		return;
	case NMG_KIND_VERTEXUSE_A_PLANE:
		{
			struct vertexuse_a_plane	*vua = (struct vertexuse_a_plane *)ip;
			struct disk_vertexuse_a_plane	*d;
			d = &((struct disk_vertexuse_a_plane *)op)[oindex];
			NMG_CK_VERTEXUSE_A_PLANE(vua);
			PUTMAGIC( DISK_VERTEXUSE_A_PLANE_MAGIC );
			/* Normal vectors don't scale */
			/* This is not a plane equation here */
			htond( d->N, vua->N, 3 );
		}
		return;
	case NMG_KIND_VERTEXUSE_A_CNURB:
		{
			struct vertexuse_a_cnurb	*vua = (struct vertexuse_a_cnurb *)ip;
			struct disk_vertexuse_a_cnurb	*d;

			d = &((struct disk_vertexuse_a_cnurb *)op)[oindex];
			NMG_CK_VERTEXUSE_A_CNURB(vua);
			PUTMAGIC( DISK_VERTEXUSE_A_CNURB_MAGIC );
			/* (u,v) parameters on curves don't scale */
			htond( d->param, vua->param, 3 );
		}
		return;
	case NMG_KIND_VERTEX:
		{
			struct vertex	*v = (struct vertex *)ip;
			struct disk_vertex	*d;
			d = &((struct disk_vertex *)op)[oindex];
			NMG_CK_VERTEX(v);
			PUTMAGIC( DISK_VERTEX_MAGIC );
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
			PUTMAGIC( DISK_VERTEX_G_MAGIC );
			VSCALE( pt, vg->coord, local2mm );
			htond( d->coord, pt, 3 );
		}
		return;
	}
	rt_log("rt_nmg_edisk kind=%d unknown\n", ecnt[index].kind);
}
#undef INDEX
#undef INDEXL

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
	else	(ii)->elem.back = (struct rt_list *)ptrs[sub]; }

/* For use with the edgeuse l2 / edge_g eu2_hd secondary list */
/* The subscripts will point to the edgeuse, not the edgeuse's l2 rt_list */
#define INDEXL_HD2(oo,ii,elem,hd)	{ \
	register int	sub; \
	register struct edgeuse	*eu2; \
	if( (sub = rt_glong((oo)->elem.forw)) < 0 ) { \
		(ii)->elem.forw = &(hd); \
	} else { \
		eu2 = (struct edgeuse *)ptrs[sub]; \
		NMG_CK_EDGEUSE(eu2); \
		(ii)->elem.forw = &eu2->l2; \
	} \
	if( (sub = rt_glong((oo)->elem.back)) < 0 ) { \
		(ii)->elem.back = &(hd); \
	} else { \
		eu2 = (struct edgeuse *)ptrs[sub]; \
		NMG_CK_EDGEUSE(eu2); \
		(ii)->elem.back = &eu2->l2; \
	} }


/*
 *			R T _ N M G _ I D I S K
 *
 *  Import a given structure from disk to memory format.
 *
 *  Transform geometry by given matrix.
 */
int
rt_nmg_idisk( op, ip, ecnt, index, ptrs, mat, basep )
genptr_t	op;		/* ptr to in-memory structure */
genptr_t	ip;		/* base of disk array */
struct nmg_exp_counts	*ecnt;
int		index;
long		**ptrs;
CONST mat_t	mat;
CONST unsigned char	*basep;	/* base of whole import record */
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
			INDEXL_HD( d, m, r_hd, m->r_hd );
		}
		return 0;
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
		return 0;
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
		return 0;
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
			NMG_CK_REGION(s->r_p);
			INDEXL_HD( d, s, l, s->r_p->s_hd ); /* after s->r_p */
		}
		return 0;
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
		return 0;
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
			INDEXL_HD( d, fu, lu_hd, fu->lu_hd );
			INDEXL_HD( d, fu, l, fu->s_p->fu_hd ); /* after fu->s_p */
			NMG_CK_FACE(fu->f_p);
			NMG_CK_FACEUSE(fu->fumate_p);
		}
		return 0;
	case NMG_KIND_FACE:
		{
			struct face	*f = (struct face *)op;
			struct disk_face	*d;
			int			g_index;

			d = &((struct disk_face *)ip)[iindex];
			NMG_CK_FACE(f);
			RT_CK_DISKMAGIC( d->magic, DISK_FACE_MAGIC );
			INDEX( d, f, faceuse, fu_p );
			g_index = rt_glong(d->g);
			f->g.magic_p = (long *)ptrs[g_index];
			f->flip = rt_glong( d->flip );
			/* Enrole this face on fg's list of users */
			NMG_CK_FACE_G_EITHER(f->g.magic_p);
			INDEXL_HD( d, f, l, f->g.plane_p->f_hd ); /* after fu->fg_p set */
			NMG_CK_FACEUSE(f->fu_p);
		}
		return 0;
	case NMG_KIND_FACE_G_PLANE:
		{
			struct face_g_plane	*fg = (struct face_g_plane *)op;
			struct disk_face_g_plane	*d;
			plane_t			plane;
			d = &((struct disk_face_g_plane *)ip)[iindex];
			NMG_CK_FACE_G_PLANE(fg);
			RT_CK_DISKMAGIC( d->magic, DISK_FACE_G_PLANE_MAGIC );
			INDEXL_HD( d, fg, f_hd, fg->f_hd );
			ntohd( plane, d->N, 4 );
			rt_rotate_plane( fg->N, mat, plane );
		}
		return 0;
	case NMG_KIND_FACE_G_SNURB:
		{
			struct face_g_snurb	*fg = (struct face_g_snurb *)op;
			struct disk_face_g_snurb	*d;
			d = &((struct disk_face_g_snurb *)ip)[iindex];
			NMG_CK_FACE_G_SNURB(fg);
			RT_CK_DISKMAGIC( d->magic, DISK_FACE_G_SNURB_MAGIC );
			INDEXL_HD( d, fg, f_hd, fg->f_hd );
			fg->order[0] = rt_glong( d->u_order );
			fg->order[1] = rt_glong( d->v_order );
			fg->u.k_size = rt_glong( d->u_size );
			fg->u.knots = rt_nmg_import_fastf( basep, ecnt,
				rt_glong( d->u_knots ), (matp_t)NULL,
				fg->u.k_size, 0 );
			fg->v.k_size = rt_glong( d->v_size );
			fg->v.knots = rt_nmg_import_fastf( basep, ecnt,
				rt_glong( d->v_knots ), (matp_t)NULL,
				fg->v.k_size, 0 );
			fg->s_size[0] = rt_glong( d->us_size );
			fg->s_size[1] = rt_glong( d->vs_size );
			fg->pt_type = rt_glong( d->pt_type );
			/* Transform ctl_points by 'mat' */
			fg->ctl_points = rt_nmg_import_fastf( basep, ecnt,
				rt_glong( d->ctl_points ), (matp_t)mat,
				fg->s_size[0] * fg->s_size[1],
				fg->pt_type );
		}
		return 0;
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
		return 0;
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
		return 0;
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
		return 0;
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
			eu->orientation = rt_glong( d->orientation );
			INDEX( d, eu, vertexuse, vu_p );
			up_kind = ecnt[up_index].kind;
			if( up_kind == NMG_KIND_LOOPUSE )  {
				INDEXL_HD( d, eu, l, eu->up.lu_p->down_hd );
			} else if( up_kind == NMG_KIND_SHELL )  {
				INDEXL_HD( d, eu, l, eu->up.s_p->eu_hd );
			} else rt_log("bad edgeuse up, index=%d, kind=%d\n", up_index, up_kind);
			eu->g.magic_p = (long *)ptrs[rt_glong(d->g)];
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_EDGEUSE(eu->eumate_p);
			NMG_CK_EDGEUSE(eu->radial_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			if( eu->g.magic_p != NULL )
			{
				NMG_CK_EDGE_G_EITHER(eu->g.magic_p);

				/* Note that l2 subscripts will be for edgeuse, not l2 */
				/* g.lseg_p->eu_hd2 is a pun for g.cnurb_p->eu_hd2 also */
				INDEXL_HD2( d, eu, l2, eu->g.lseg_p->eu_hd2 );
			}
			else
			{
				eu->l2.forw = &eu->l2;
				eu->l2.back = &eu->l2;
			}
		}
		return 0;
	case NMG_KIND_EDGE:
		{
			struct edge	*e = (struct edge *)op;
			struct disk_edge	*d;
			d = &((struct disk_edge *)ip)[iindex];
			NMG_CK_EDGE(e);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGE_MAGIC );
			e->is_real = rt_glong( d->is_real );
			INDEX( d, e, edgeuse, eu_p );
			NMG_CK_EDGEUSE(e->eu_p);
		}
		return 0;
	case NMG_KIND_EDGE_G_LSEG:
		{
			struct edge_g_lseg	*eg = (struct edge_g_lseg *)op;
			struct disk_edge_g_lseg	*d;
			point_t			pt;
			vect_t			dir;

			d = &((struct disk_edge_g_lseg *)ip)[iindex];
			NMG_CK_EDGE_G_LSEG(eg);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGE_G_LSEG_MAGIC );
			/* Forw subscript points to edgeuse, not edgeuse2 */
			INDEXL_HD2( d, eg, eu_hd2, eg->eu_hd2 );
			ntohd(pt, d->e_pt, 3);
			ntohd(dir, d->e_dir, 3);
			MAT4X3PNT( eg->e_pt, mat, pt );
			MAT4X3VEC( eg->e_dir, mat, dir );
		}
		return 0;
	case NMG_KIND_EDGE_G_CNURB:
		{
			struct edge_g_cnurb	*eg = (struct edge_g_cnurb *)op;
			struct disk_edge_g_cnurb	*d;
			d = &((struct disk_edge_g_cnurb *)ip)[iindex];
			NMG_CK_EDGE_G_CNURB(eg);
			RT_CK_DISKMAGIC( d->magic, DISK_EDGE_G_CNURB_MAGIC );
			INDEXL_HD2( d, eg, eu_hd2, eg->eu_hd2 );
			eg->order = rt_glong( d->order );

			/* If order is zero, so is everything else */
			if( eg->order == 0 )  return 0;

			eg->k.k_size = rt_glong( d->k_size );
			eg->k.knots = rt_nmg_import_fastf( basep, ecnt,
				rt_glong( d->knots ), (matp_t)NULL,
				eg->k.k_size, 0 );
			eg->c_size = rt_glong( d->c_size );
			eg->pt_type = rt_glong( d->pt_type );
			/*
			 * The curve's control points are in parameter space.
			 * They do NOT get transformed!
			 */
			if( RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV )  {
				/* UV coords on snurb surface don't get xformed */
				eg->ctl_points = rt_nmg_import_fastf( basep,
					ecnt,
					rt_glong( d->ctl_points ), (matp_t)NULL,
					eg->c_size, eg->pt_type );
			} else {
				/* XYZ coords on planar face DO get xformed */
				eg->ctl_points = rt_nmg_import_fastf( basep,
					ecnt,
					rt_glong( d->ctl_points ), (matp_t)mat,
					eg->c_size, eg->pt_type );
			}
		}
		return 0;
	case NMG_KIND_VERTEXUSE:
		{
			struct vertexuse	*vu = (struct vertexuse *)op;
			struct disk_vertexuse	*d;
			d = &((struct disk_vertexuse *)ip)[iindex];
			NMG_CK_VERTEXUSE(vu);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEXUSE_MAGIC );
			vu->up.magic_p = (long *)ptrs[rt_glong(d->up)];
			INDEX( d, vu, vertex, v_p );
			vu->a.magic_p = (long *)ptrs[rt_glong(d->a)];
			NMG_CK_VERTEX(vu->v_p);
			if(vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
			INDEXL_HD( d, vu, l, vu->v_p->vu_hd );
		}
		return 0;
	case NMG_KIND_VERTEXUSE_A_PLANE:
		{
			struct vertexuse_a_plane	*vua = (struct vertexuse_a_plane *)op;
			struct disk_vertexuse_a_plane	*d;
			vect_t			norm;
			d = &((struct disk_vertexuse_a_plane *)ip)[iindex];
			NMG_CK_VERTEXUSE_A_PLANE(vua);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEXUSE_A_PLANE_MAGIC );
			ntohd( norm, d->N, 3 );
			MAT4X3VEC( vua->N, mat, norm );
		}
		return 0;
	case NMG_KIND_VERTEXUSE_A_CNURB:
		{
			struct vertexuse_a_cnurb	*vua = (struct vertexuse_a_cnurb *)op;
			struct disk_vertexuse_a_cnurb	*d;
			d = &((struct disk_vertexuse_a_cnurb *)ip)[iindex];
			NMG_CK_VERTEXUSE_A_CNURB(vua);
			RT_CK_DISKMAGIC( d->magic, DISK_VERTEXUSE_A_CNURB_MAGIC );
			/* These parameters are invarient w.r.t. 'mat' */
			ntohd( vua->param, d->param, 3 );
		}
		return 0;
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
		return 0;
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
		return 0;
	}
	rt_log("rt_nmg_idisk kind=%d unknown\n", ecnt[index].kind);
	return -1;
}

/*
 *			R T _ N M G _ I A L L O C
 *
 *  Allocate storage for all the in-memory NMG structures,
 *  in preparation for the importation operation,
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
		if( kind == NMG_KIND_DOUBLE_ARRAY )  continue;
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
			case NMG_KIND_FACE:
				{
					struct face	*f;
					GET_FACE( f, m );
					f->l.magic = NMG_FACE_MAGIC;
					ptrs[subscript] = (long *)f;
				}
				break;
			case NMG_KIND_FACE_G_PLANE:
				{
					struct face_g_plane	*fg;
					GET_FACE_G_PLANE( fg, m );
					fg->magic = NMG_FACE_G_PLANE_MAGIC;
					RT_LIST_INIT( &fg->f_hd );
					ptrs[subscript] = (long *)fg;
				}
				break;
			case NMG_KIND_FACE_G_SNURB:
				{
					struct face_g_snurb	*fg;
					GET_FACE_G_SNURB( fg, m );
					fg->l.magic = NMG_FACE_G_SNURB_MAGIC;
					RT_LIST_INIT( &fg->f_hd );
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
					eu->l2.magic = NMG_EDGEUSE2_MAGIC;
					ptrs[subscript] = (long *)eu;
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
			case NMG_KIND_EDGE_G_LSEG:
				{
					struct edge_g_lseg	*eg;
					GET_EDGE_G_LSEG( eg, m );
					eg->l.magic = NMG_EDGE_G_LSEG_MAGIC;
					RT_LIST_INIT( &eg->eu_hd2 );
					ptrs[subscript] = (long *)eg;
				}
				break;
			case NMG_KIND_EDGE_G_CNURB:
				{
					struct edge_g_cnurb	*eg;
					GET_EDGE_G_CNURB( eg, m );
					eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;
					RT_LIST_INIT( &eg->eu_hd2 );
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
			case NMG_KIND_VERTEXUSE_A_PLANE:
				{
					struct vertexuse_a_plane	*vua;
					GET_VERTEXUSE_A_PLANE( vua, m );
					vua->magic = NMG_VERTEXUSE_A_PLANE_MAGIC;
					ptrs[subscript] = (long *)vua;
				}
				break;
			case NMG_KIND_VERTEXUSE_A_CNURB:
				{
					struct vertexuse_a_cnurb	*vua;
					GET_VERTEXUSE_A_CNURB( vua, m );
					vua->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;
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
 *			R T _ N M G _ I 2 A L L O C
 *
 *  Find the locations of all the variable-sized fastf_t arrays in
 *  the input record.  Record that position as a byte offset from the
 *  very front of the input record in ecnt[], indexed by subscript number.
 *
 *  No storage is allocated here, that will be done by rt_nmg_import_fastf()
 *  on the fly.
 *  A separate call to rt_malloc() will be used, so that nmg_keg(), etc.,
 *  can kill each array as appropriate.
 */
void
rt_nmg_i2alloc( ecnt, cp, kind_counts, maxindex )
struct nmg_exp_counts	*ecnt;
unsigned char		*cp;
int			kind_counts[NMG_N_KINDS];
int			maxindex;
{
	register int	kind;
	int		nkind;
	int		subscript;
	int		offset;
	int		i;

	nkind = kind_counts[NMG_KIND_DOUBLE_ARRAY];
	if( nkind <= 0 )  return;

	/* First, find the beginning of the fastf_t arrays */
	subscript = 1;
	offset = 0;
	for( kind = 0; kind < NMG_N_KINDS; kind++ )  {
		if( kind == NMG_KIND_DOUBLE_ARRAY )  continue;
		offset += rt_nmg_disk_sizes[kind] * kind_counts[kind];
		subscript += kind_counts[kind];
	}

	/* Should have found the first one now */
	RT_CK_DISKMAGIC( cp + offset, DISK_DOUBLE_ARRAY_MAGIC );
#if DEBUG
rt_log("rt_nmg_i2alloc() first one at cp=x%x, offset=%d, subscript=%d\n", cp, offset, subscript );
#endif
	for( i=0; i < nkind; i++ )  {
		int	ndouble;
		RT_CK_DISKMAGIC( cp + offset, DISK_DOUBLE_ARRAY_MAGIC );
		ndouble = rt_glong( cp + offset + 4 );
		ecnt[subscript].kind = NMG_KIND_DOUBLE_ARRAY;
		/* Stored byte offset is from beginning of disk record */
		ecnt[subscript].byte_offset = offset;
		offset += (4+4) + 8*ndouble;
		subscript++;
	}
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
rt_nmg_import_internal( ip, ep, mat, rebound, tol )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
int				rebound;
CONST struct rt_tol		*tol;
{
	struct model			*m;
	union record			*rp;
	int				kind_counts[NMG_N_KINDS];
	unsigned char			*cp;
	long				**real_ptrs;
	long				**ptrs;
	struct nmg_exp_counts		*ecnt;
	int				i;
	int				maxindex;
	int				kind;
	static long			bad_magic = 0x999;

	RT_CK_EXTERNAL( ep );
	RT_CK_TOL( tol );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_NMG )  {
		rt_log("rt_nmg_import: defective record\n");
		return(-1);
	}

	/*
	 *  Check for proper version.
	 *  In the future, this will be the backwards-compatability hook.
	 */
	if( rp->nmg.N_version != DISK_MODEL_VERSION )  {
		rt_log("rt_nmg_import:  expected NMG '.g' format version %d, got version %d, aborting.\n",
			DISK_MODEL_VERSION,
			rp->nmg.N_version );
		return -1;
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
	ptrs[maxindex+1] = &bad_magic;	/* [maxindex+1] gives bad magic */

	/* Allocate storage for all the NMG structs, in ptrs[] */
	m = rt_nmg_ialloc( ptrs, ecnt, kind_counts );

	/* Locate the variably sized fastf_t arrays.  ecnt[] has room. */
	cp = (unsigned char *)(rp+1);	/* start at first granule in */
	rt_nmg_i2alloc( ecnt, cp, kind_counts, maxindex );

	/* Import each structure, in turn */
	for( i=1; i < maxindex; i++ )  {
		/* If we made it to the last kind, stop.  Nothing follows */
		if( ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY )  break;
		if( rt_nmg_idisk( (genptr_t)(ptrs[i]), (genptr_t)cp,
			ecnt, i, ptrs, mat, (unsigned char *)(rp+1) ) < 0 )
				return -1;	/* FAIL */
		cp += rt_nmg_disk_sizes[ecnt[i].kind];
	}

	if( rebound )  {
		/* Recompute all bounding boxes in model */
		nmg_rebound(m, tol);
	} else {
		/*
		 *  Need to recompute bounding boxes for the faces here.
		 *  Other bounding boxes will exist and be intact if NMG
		 *  exporter wrote the _a structures.
		 */
		for( i=1; i < maxindex; i++ )  {
			if( ecnt[i].kind != NMG_KIND_FACE )  continue;
			nmg_face_bb( (struct face *)ptrs[i], tol );
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
 *
 *  If the "compact" flag is not set, then the NMG model is saved, verbatim.
 *
 *  The overall layout of the on-disk NMG is like this:
 *
 *	+---------------------------+
 *	|  NMG header granule       |
 *	|    solid name             |
 *	|    # additional granules  |
 *	|    format version         |
 *	|    kind_count[] array     |
 *	+---------------------------+
 *	|                           |
 *	|                           |
 *	~     N_count granules      ~
 *	~              :            ~
 *	|              :            |
 *	|                           |
 *	+---------------------------+
 *
 *  In the additional granules, all structures of "kind" 0 (model) go first,
 *  followed by all structures of kind 1 (nmgregion), etc.
 *  As each structure is output, it is assigned a subscript number,
 *  starting with #1 for the model structure.
 *  All pointers are converted to the matching subscript numbers.
 *  An on-disk subscript of zero indicates a corresponding NULL pointer in memory.
 *  All integers are converted to network (Big-Endian) byte order.
 *  All floating point values are stored in network (Big-Endian IEEE) format.
 */
int
rt_nmg_export_internal( ep, ip, local2mm, compact )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
int				compact;
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
	int				double_count;
	int				fastf_byte_count;

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
	ecnt = (struct nmg_exp_counts *)rt_calloc( m->maxindex+1,
		sizeof(struct nmg_exp_counts), "ecnt[]" );
	for( i = 0; i < NMG_N_KINDS; i++ )
		kind_counts[i] = 0;
	subscript = 1;		/* must be larger than DISK_INDEX_NULL */
	double_count = 0;
	fastf_byte_count = 0;
	for( i=0; i < m->maxindex; i++ )  {
		if( ptrs[i] == (long *)0 )  {
			ecnt[i].kind = -1;
			continue;
		}
		kind = rt_nmg_magic_to_kind( *(ptrs[i]) );
		ecnt[i].per_struct_index = kind_counts[kind]++;
		ecnt[i].kind = kind;
		/* Handle the variable sized kinds */
		switch(kind)  {
		case NMG_KIND_FACE_G_SNURB:
			{
				struct face_g_snurb	*fg;
				int			ndouble;
				fg = (struct face_g_snurb *)ptrs[i];
				ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
				kind_counts[NMG_KIND_DOUBLE_ARRAY] += 3;
				ndouble =  fg->u.k_size +
					   fg->v.k_size +
					   fg->s_size[0] * fg->s_size[1] *
					   RT_NURB_EXTRACT_COORDS(fg->pt_type);
				double_count += ndouble;
				ecnt[i].byte_offset = fastf_byte_count;
				fastf_byte_count += 3*(4+4) + 8*ndouble;
			}
			break;
		case NMG_KIND_EDGE_G_CNURB:
			{
				struct edge_g_cnurb	*eg;
				int			ndouble;
				eg = (struct edge_g_cnurb *)ptrs[i];
				ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
				/* If order is zero, no knots or ctl_points */
				if( eg->order == 0 )  break;
				kind_counts[NMG_KIND_DOUBLE_ARRAY] += 2;
				ndouble = eg->k.k_size + eg->c_size *
					RT_NURB_EXTRACT_COORDS(eg->pt_type);
				double_count += ndouble;
				ecnt[i].byte_offset = fastf_byte_count;
				fastf_byte_count += 2*(4+4) + 8*ndouble;
			}
			break;
		}
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
	/* Tack on the variable sized fastf_t arrays at the end */
	rt_nmg_cur_fastf_subscript = subscript;
	subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

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
	/* Account for variable sized double arrays, at the end */
	tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
			double_count * 8;

	ecnt[0].byte_offset = subscript; /* implicit arg to rt_nmg_reindex() */

	additional_grans = (tot_size + sizeof(union record)-1) / sizeof(union record);
	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (1 + additional_grans) * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "nmg external");
	rp = (union record *)ep->ext_buf;
	rp->nmg.N_id = DBID_NMG;
	rp->nmg.N_version = DISK_MODEL_VERSION;
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
	/* disk_arrays[NMG_KIND_DOUBLE_ARRAY] is set properly because it is last */
	rt_nmg_fastf_p = (unsigned char *)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

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
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	struct model			*m;
	union record			*rp;
	struct rt_tol			tol;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_NMG )  {
		rt_log("rt_nmg_import: defective record\n");
		return(-1);
	}

	/* XXX The bounding box routines need a tolerance.
	 * XXX This is sheer guesswork here.
	 * As long as this NMG is going to be turned into vlist, or
	 * handed off to the boolean evaluator, any non-zero numbers are fine.
	 */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	if( rt_nmg_import_internal( ip, ep, mat, 1, &tol ) < 0 )
		return(-1);

	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	if( rt_g.debug || rt_g.NMG_debug )
		nmg_vmodel(m);

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
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct model			*m;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_NMG )  return(-1);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	/* To ensure that a decent database is written, verify source first */
	nmg_vmodel(m);

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

	NMG_CK_MODEL(m);
	rt_vls_printf( str, "n-Manifold Geometry solid (NMG) maxindex=%d\n",
		m->maxindex);

	if( !verbose )  return(0);

#if 0
	{
	struct nmg_struct_counts	count;
	long			**ptrs;
	/* This really is not useful for the MGED overlay! */
	ptrs = nmg_m_struct_count( &count, m );

	/* If verbose, should print out structure counts */
	nmg_vls_struct_counts( str, &count );

	rt_free( (char *)ptrs, "struct_count *ptrs[]" );
	}
#endif

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
	if (ip->idb_ptr) {
		m = (struct model *)ip->idb_ptr;
		NMG_CK_MODEL(m);
		nmg_km( m );
	}

	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
