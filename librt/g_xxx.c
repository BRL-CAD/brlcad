/*
 *			X X X . C
 *
 *  Purpose -
 *	Intersect a ray with a 
 *
 *  Authors -
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSsph[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

struct xxx_internal {
	vect_t	xxx_V;
};

struct xxx_specific {
	vect_t	xxx_V;
};

/*
 *  			X X X _ P R E P
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
xxx_prep( stp, rec, rtip )
struct soltab		*stp;
struct rt_i		*rtip;
union record		*rec;
{
	register struct xxx_specific *xxx;
}

void
xxx_print( stp )
register struct soltab *stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/*
 *  			X X X _ S H O T
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
xxx_shot( stp, rp, ap, seghead )
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
 *			S P H _ V S H O T
 *
 *  Vectorized version.
 */
void
xxx_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	register struct xxx_specific *xxx;
}

/*
 *  			X X X _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
xxx_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			X X X _ C U R V E
 *
 *  Return the curvature of the xxx.
 */
void
xxx_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			X X X _ U V
 *  
 *  For a hit on the surface of an xxx, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
xxx_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/*
 *		X X X _ F R E E
 */
void
xxx_free( stp )
register struct soltab *stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	rt_free( (char *)xxx, "xxx_specific" );
}

int
xxx_class()
{
	return(0);
}

int
xxx_plot()
{
}

int
xxx_tess()
{
}

int
xxx_import( xxx, rp, matp )
struct xxx_internal	*xxx;
union record		*rp;
register matp_t		matp;
{
	return(0);			/* OK */
}
