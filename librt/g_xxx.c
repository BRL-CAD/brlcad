/*
 *			G _ X X X . C
 *
 *  Purpose -
 *	Intersect a ray with a 
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_xxx_internal --- parameters for solid
 *	define xxx_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_xxx_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_XXX, increment ID_MAXIMUM
 *	edit Cakefile to add g_xxx.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_xxx() routine
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

#if 0
/* parameters for solid, internal representation
 * This goes in rtgeom.h
 */
/* parameters for solid, internal representation */
struct rt_xxx_internal {
	long	magic;
	vect_t	v;
};
#define RT_XXX_INTERNAL_MAGIC	0xxx
#define RT_XXX_CK_MAGIC(_p)	RT_CKMAG(_p,RT_XXX_INTERNAL_MAGIC,"rt_xxx_internal")
#endif

/* ray tracing form of solid, including precomputed terms */
struct xxx_specific {
	vect_t	xxx_V;
};

/*
 *  			R T _ X X X _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid XXX, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	XXX is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct xxx_specific is created, and it's address is stored in
 *  	stp->st_specific for use by xxx_shot().
 */
int
rt_xxx_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_xxx_internal		*xxx_ip;
	register struct xxx_specific	*xxx;
	CONST struct rt_tol		*tol = &rtip->rti_tol;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);
}

/*
 *			R T _ X X X _ P R I N T
 */
void
rt_xxx_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/*
 *  			R T _ X X X _ S H O T
 *  
 *  Intersect a ray with a xxx.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_xxx_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
	register struct seg *segp;
	CONST struct rt_tol	*tol = &ap->a_rt_i->rti_tol;

	return(0);			/* MISS */
}

#define RT_XXX_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ X X X _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_xxx_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ X X X _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_xxx_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ X X X _ C U R V E
 *
 *  Return the curvature of the xxx.
 */
void
rt_xxx_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ X X X _ U V
 *  
 *  For a hit on the surface of an xxx, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_xxx_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/*
 *		R T _ X X X _ F R E E
 */
void
rt_xxx_free( stp )
register struct soltab *stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	rt_free( (char *)xxx, "xxx_specific" );
}

/*
 *			R T _ X X X _ C L A S S
 */
int
rt_xxx_class()
{
	return(0);
}

/*
 *			R T _ X X X _ P L O T
 */
int
rt_xxx_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	return(-1);
}

/*
 *			R T _ X X X _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_xxx_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	return(-1);
}

/*
 *			R T _ X X X _ I M P O R T
 *
 *  Import an XXX from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_xxx_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;
	union record			*rp;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		rt_log("rt_xxx_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_XXX;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_xxx_internal), "rt_xxx_internal");
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	xxx_ip->magic = RT_XXX_INTERNAL_MAGIC;

	MAT4X3PNT( xxx_ip->xxx_V, mat, &rp->s.s_values[0] );

	return(0);			/* OK */
}

/*
 *			R T _ X X X _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_xxx_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_xxx_internal	*xxx_ip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_XXX )  return(-1);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "xxx external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = XXX;	/* GED primitive type from db.h */

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */


	/* convert from local editing units to mm and export
	 * to database record format
	 *
	 * Warning: type conversion: double to float
	 */
	VSCALE( &rec->s.s_values[0], xxx_ip->xxx_V, local2mm );
	rec->s.s_values[3] = xxx_ip->xxx_radius * local2mm;

	return(0);
}

/*
 *			R T _ X X X _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_xxx_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_xxx_internal	*xxx_ip =
		(struct rt_xxx_internal *)ip->idb_ptr;
	char	buf[256];

	RT_XXX_CK_MAGIC(xxx_ip);
	rt_vls_strcat( str, "truncated general xxx (XXX)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		xxx_ip->v[X] * mm2local,
		xxx_ip->v[Y] * mm2local,
		xxx_ip->v[Z] * mm2local );
	rt_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ X X X _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_xxx_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);
	xxx_ip->magic = 0;			/* sanity */

	rt_free( (char *)xxx_ip, "xxx ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
