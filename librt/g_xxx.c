/*
 *			G _ X X X . C
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
static char RCSxxx[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

struct rt_xxx_internal {
	long	magic;
	vect_t	xxx_V;
};
#define RT_XXX_INTERNAL_MAGIC	0xxx
#define RT_XXX_CK_MAGIC(_p)	RT_CKMAG(_p,RT_XXX_INTERNAL_MAGIC,"rt_xxx_internal")

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
rt_xxx_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	register struct xxx_specific *xxx;
}

/*
 *			R T _ X X X _ P R I N T
 */
void
rt_xxx_print( stp )
register struct soltab *stp;
{
	register struct xxx_specific *xxx =
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

	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T _ X X X _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_xxx_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
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
rt_xxx_plot( rp, mat, vhead, dp, abs_tol, rel_tol, norm_tol )
union record	*rp;
mat_t		mat;
struct vlhead	*vhead;
struct directory *dp;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
	return(-1);
}

/*
 *			R T _ X X X _ T E S S
 */
int
rt_xxx_tess( r, m, rp, mat, dp, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
union record		*rp;
mat_t			mat;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	return(-1);
}

/*
 *			R T _ X X X _ I M P O R T
 */
int
rt_xxx_import( xxx, rp, mat )
struct xxx_internal	*xxx;
union record		*rp;
register mat_t		mat;
{
	return(0);			/* OK */
}
