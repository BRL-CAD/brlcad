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
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

/* rt_nmg_internal is just "model", from nmg.h */

struct nmg_specific {
	vect_t	nmg_V;
};

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
rt_nmg_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct model		*m;
	register struct nmg_specific	*nmg;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);
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
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;
	register struct seg *segp;

	return(2);			/* HIT */
}

#define RT_NMG_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ N M G _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nmg_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
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
	register struct nmg_specific *nmg =
		(struct nmg_specific *)stp->st_specific;

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
rt_nmg_plot( vhead, ip, abs_tol, rel_tol, norm_tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	LOCAL struct model	*m;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	return(-1);
}

/*
 *			R T _ N M G _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_nmg_tess( r, m, ip, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	LOCAL struct model	*m;

	RT_CK_DB_INTERNAL(ip);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	return(-1);
}

/* Format of db.h nmg.N_nstructs info, sucked straight from nmg_struct_counts */
#define NMG_O(m)	offsetof(struct nmg_struct_counts, m)

struct rt_imexport rt_nmg_structs_fmt[] = {
	"%d",	NMG_O(model),		1,
	"%d",	NMG_O(model_a),		1,
	"%d",	NMG_O(region),		1,
	"%d",	NMG_O(region_a),	1,
	"%d",	NMG_O(shell),		1,
	"%d",	NMG_O(shell_a),		1,
	"%d",	NMG_O(faceuse),		1,
	"%d",	NMG_O(faceuse_a),	1,
	"%d",	NMG_O(face),		1,
	"%d",	NMG_O(face_g),		1,
	"%d",	NMG_O(loopuse),		1,
	"%d",	NMG_O(loopuse_a),	1,
	"%d",	NMG_O(loop),		1,
	"%d",	NMG_O(loop_g),		1,
	"%d",	NMG_O(edgeuse),		1,
	"%d",	NMG_O(edgeuse_a),	1,
	"%d",	NMG_O(edge),		1,
	"%d",	NMG_O(edge_g),		1,
	"%d",	NMG_O(vertexuse),	1,
	"%d",	NMG_O(vertexuse_a),	1,
	"%d",	NMG_O(vertex),		1,
	"%d",	NMG_O(vertex_g),	1,
	"",	0,			0
};

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
	LOCAL struct model		*m;
	union record			*rp;
	struct nmg_struct_counts	cntbuf;
	struct rt_external		count_ext;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_NMG )  {
		rt_log("rt_nmg_import: defective record\n");
		return(-1);
	}

	bzero( (char *)&cntbuf, sizeof(cntbuf) );
	RT_INIT_EXTERNAL(&count_ext);
	count_ext.ext_nbytes = sizeof(rp->nmg.N_structs);
	count_ext.ext_buf = rp->nmg.N_structs;
	if( rt_struct_import( (genptr_t)&cntbuf, rt_nmg_structs_fmt, &count_ext ) <= 0 )  {
		rt_log("rt_struct_import failure\n");
		return(-1);
	}
	nmg_pr_struct_counts( &cntbuf, "After import" );

return(-1);
#if 0
	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_NMG;
	ip->idb_ptr = rt_malloc( sizeof(struct model), "model");
	m = (struct model *)ip->idb_ptr;
	m->magic = RT_NMG_INTERNAL_MAGIC;

	return(0);			/* OK */
#endif
}

/*
 *			R T _ N M G _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_nmg_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
	struct model			*m;
	union record			*rp;
	struct nmg_struct_counts	cntbuf;
	struct rt_external		count_ext;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_NMG )  return(-1);
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);

	bzero( (char *)&cntbuf, sizeof(cntbuf) );
	nmg_m_struct_count( &cntbuf, m );
	nmg_pr_struct_counts( &cntbuf, "Counts in rt_nmg_export" );

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "nmg external");
	rp = (union record *)ep->ext_buf;
	rp->nmg.N_id = DBID_NMG;
	rp->nmg.N_count = 0;		/* XXX for now */

	RT_INIT_EXTERNAL(&count_ext);
	if( rt_struct_export( &count_ext, (genptr_t)&cntbuf, rt_nmg_structs_fmt ) < 0 )  {
		rt_log("rt_struct_export() failure\n");
		return(-1);
	}
	if( count_ext.ext_nbytes > sizeof(rp->nmg.N_structs) )
		rt_bomb("nmg.N_structs overflow");
	bcopy( count_ext.ext_buf, rp->nmg.N_structs, count_ext.ext_nbytes );
	db_free_external( &count_ext );

	return(0);
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
	rt_vls_strcat( str, "truncated general nmg (NMG)\n");

#if 0
	sprintf(buf, "\tV (%g, %g, %g)\n",
		m->v[X] * mm2local,
		m->v[Y] * mm2local,
		m->v[Z] * mm2local );
	rt_vls_strcat( str, buf );
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
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);
	nmg_km( m );

	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
