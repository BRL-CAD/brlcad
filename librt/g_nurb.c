/*
 *			G _ N U R B . C
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
static char RCSnurb[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "nurb.h"
#include "./debug.h"

struct nurb_tree {
	point_t min, max;		/* Min and Max RPP of sub surface */
	struct snurb	 * root;	/* Null if Non leaf node */
	strcut nurb_tree * left,
			 * right;
};

struct nurb_internal {
	struct nurb_internal *  next;	/* next surface in the the solid */
	point_t		        min,	/* Bounding box of surface
			        max;
	struct snurb *		root;	/* Null if non leaf node */
	struct nurb_tree 	* left, /* pointer to left sub tree */
				* right;/* Pointer to right sub tree */
};

struct nurb_hit {
	fastf_t		hit_dist;	/* Distance from the r_pt to surface */
	point_t		hit_point;	/* intersection point */
	vect_t		hit_normal;	/* Surface normal */
	fastf_t		hit_uv[2];	/* Surface parametric u,v */
	char *		hit_private;	/* Store current nurb root */
	struct nurb_hit * next;
	struct nurb_hit * prev;
};

#define NULL_TREE (struct nurb_tree *)0
#define NULL_HIT  (struct nurb_hit *)0

/*
 *  			R T _ N U R B _ P R E P
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
 *  	A struct nurb_specific is created, and it's address is stored in
 *  	stp->st_specific for use by nurb_shot().
 */
int
rt_nurb_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	register struct nurb_specific *nurb;
	struct nurb_specific * n;

	rt_nurb_import( nurb, rec, stp->st_pathmat );
	
	for( n = nurb; n != (struct nurb_internal *)0; n = n->next)
	{
		VMINMAX( stp->st_min, stp->st_max, n->root->min);
		VMINMAX( stp->st_max, stp->st_max, n->root->min);
	}
	
	stp->st_specific = (genptr) nurb;
	VSET(stp->st_center, 
		(stp->st_max[0] + stp->st_min[0])/2.0,
		(stp->st_max[1] + stp->st_min[1])/2.0,
		(stp->st_max[2] + stp->st_min[2])/2.0);
	{
		fastf_t f, dx, dy, dz;
		
		dx = (stp->st_max[0] - stp->st_min[0])/2.;
		f = dx;
		dy = (stp->st_max[1] - stp->st_min[1])/2.;
		if( dy > f) f = dy;
		dx = (stp->st_max[2] - stp->st_min[2])/2.;
		if( dz > f ) f = dz;

		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy* dz*dz);
	}
	return 0;
}

/*
 *			R T _ N U R B _ P R I N T
 */
void
rt_nurb_print( stp )
register struct soltab *stp;
{
	register struct nurb_internal *nurb =
		(struct nurb_internal *)stp->st_specific;

	if( nurb == (struct snurb *)0)
	{
		rt_log("rt_nurb_print: no surfaces\n");
		return;
	}

	for( ; nurb != (struct nurb_internal *)0; nurb = nurb->next)
	{
		nurb_s_print("NURB", nurb->root);
	}
}

/*
 *  			R T _ N U R B _ S H O T
 *  
 *  Intersect a ray with a nurb.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_nurb_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;
	register struct seg *segp;

	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T _ N U R B _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nurb_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
}

/*
 *  			R T _ N U R B _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nurb_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ N U R B _ C U R V E
 *
 *  Return the curvature of the nurb.
 */
void
rt_nurb_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ N U R B _ U V
 *  
 *  For a hit on the surface of an nurb, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_nurb_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;
}

/*
 *		R T _ N U R B _ F R E E
 */
void
rt_nurb_free( stp )
register struct soltab *stp;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;

	rt_free( (char *)nurb, "nurb_specific" );
}

/*
 *			R T _ N U R B _ C L A S S
 */
int
rt_nurb_class()
{
	return(0);
}

/*
 *			R T _ N U R B _ P L O T
 */
int
rt_nurb_plot( rp, vhead, dp, abs_tol, rel_tol, norm_tol )
union record	*rp;
struct rt_list	*vhead;
struct directory *dp;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
	return(-1);
}

/*
 *			R T _ N U R B _ T E S S
 */
int
rt_nurb_tess( r, m, rp, dp, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
union record		*rp;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	return(-1);
}

/*
 *			R T _ N U R B _ I M P O R T
 */
int
rt_nurb_import( nurb, rp, mat )
struct nurb_internal	*nurb;
union record		*rp;
register mat_t		mat;
{
	int surf_num;
	int currec;

	surf_num = rp[0].B.B_nsurf;
	currec = 1;

	while ( surf_num--)
	{
		struct nurb_internal * n_tree;
		struct snurb		* srf;
		struct snurb		* srf_split;
		
		if( (srf = (struct snurb *) rt_nurb_readin( &rp[currec], mat)) == (struct snurb *)0)
		{
			rt_log("rt_nurb_import srf: database read error\n");
			return (-1);
		}
		
		currec += 1 + rp[currec].d.d_nknots +
			rp[currec].d.d_nctls;

		GETSTRUCT( n_tree, nurb_internal);
		GETSTRUCT( nurb_internal->left, nurb_tree);
		GETSTRUCT( nurb_internal->right, nurb_tree);
		
		n_tree->next = nurb;
		nurb = n_tree;

		srf_split =(struct snurb *) nurb_s_split( srf, ROW);
		n_tree->left->root = srf_split;
		n_tree->right->root = srf_split->next;
		srf_split->next = (struct snurb *)0;

		n_tree->left->left = NULL_TREE;
		n_tree->left->right = NULL_TREE;
		n_tree->right->left = NULL_TREE;
		n_tree->right->right = NULL_TREE;

		n_tree->root = srf;
		
		nurb_s_bound(n_tree->root, n_tree->min, n_tree->max);
		nurb_s_bound(n_tree->left->root, n_tree->left->min,
			n_tree->left->max);
		nurb_s_bound(n_tree->right->root, n_tree->right->min,
			n_tree->right->max);
	}

	return(0);			/* OK */
}

struct snurb *
rt_nurb_readin(drec, mat)
union record *drec;
matp_t mat;
{

	register int i;
	register fastf_t * mesh_ptr;
	register dbfloat_t	*vp;
	struct snurb * srf;
	int coords;
	int rational;
	int pt_type;

	if( drec[0].u_id != ID_BSURF ) 
	{
		rt_log("rt_nurb_readin: bad record 0 %o\n", drec[0].u_id);
		return (struct snurb * )0;
	}

	coords = drec[0].d.d_geom_type;

	if( coords == 4) rational = TRUE;

	pt_type = MAKE_PT_TYPE(coords, PT_XYZ, rational);


	/* Allocate memory for a new nurb surface */
	srf = nurb_new_snurb( 
		drec[0].d.d_order[0], drec[0].d.d_order[1],  
		drec[0].d.d_kv_size[0],drec[0].d.d_kv_size[1],
		drec[0].d.d_ctl_size[0],drec[0].d.d_ctl_size[1],
		pt_type);

	/* Read in the knot vectors */
	vp = ( dbfloat_t *) &drec[1];
	
	for( i = 0; i < drec[0].d.d_kv_size[0]; i++)
		srf->u_knots->knots[i] = (fastf_t) * vp++;

	for( i = 0; i < drec[0].d.d_kv_size[1]; i++)
		srf->v_knots->knots[i] = (fastf_t) * vp++;

	nurb_kvnorm( srf->u_knots);
	nurb_kvnorm( srf->v_knots);

	vp = (dbfloat_t *) &drec[drec[0].d.d_nknots+1];
	mesh_ptr = srf->mesh->ctl_points;
	i = ( drec[0].d.d_ctl_size[0] * drec[0].d.d_ctl_size[1]);
	

	if( coords > 4)
	{
		rt_log("rt_nurb_readin: %d invalid # of coords\n",coords);
		return (struct snurb *)0;
	}

	if( rational )
	{
		for( i > 0 ; i--)
		{
			MAT4X3PNT( mesh_ptr, mat, vp);
			mesh_ptr += coords;
			vp += coords;
		}
	} else
	{
		for( ; i> 0; i--)
		{
			MAT4X4PNT( mesh_ptr, mat, vp);
			mesh_ptr += coords;
			vp += coords;
		}
	}
	return srf;
}
