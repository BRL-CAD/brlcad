/* 
 *			G _ S P L I N E . C
 *
 * Function -
 *     Ray trace Spline code to work with librt and libspl.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1986 by the United States Army.
 *     All rights reserved.
 */

#include <stdio.h>	/* GED specific include files */
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

#include "../libspl/b_spline.h"

struct rt_spl_internal {
	long		magic;
	int		nsurf;
	fastf_t		resolution;
	struct b_spline	**surfs;
};
#define RT_SPL_INTERNAL_MAGIC	0x00911911
#define RT_SPL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_SPL_INTERNAL_MAGIC,"rt_spl_internal")

struct b_tree {
	point_t min, max;		/* bounding box info */
	struct b_spline * root;	        /* B_spline surface */
	struct b_tree  * left;
	struct b_tree * right;
};

struct b_head {
	struct b_head * next;   /* Next tree over only used at level 0 */
	point_t	min,  max;  /* Current surface minimum and maximum */
	struct b_spline * root;   /* Leaf b_spline surface. Null if interior */
	struct b_spline * u_diff;
	struct b_spline * v_diff;
	struct b_tree * left;   /* Left sub tree */
	struct b_tree * right;  /* Right sub tree */
};

/* 
 * Local hit structure. We need this since the number of hits
 * may be large. An extra forward link will allow hits to be 
 * added from the surface.
 */

struct local_hit {
	fastf_t hit_dist;	/* Distance from the r_pt to the surface */
	point_t hit_point;	/* Intersection point */
	vect_t  hit_normal;	/* Surface Normal at the hit_point */
	vect_t  hit_vpriv;      /* Store parametric u and v information */
	char * hit_private; 	/* store cuurent b-spline surface pointer */
	struct local_hit * next;
	struct local_hit * prev;
};

#define NULLHIT	    (struct local_hit *) 0
#define NULLTREE    (struct b_tree *) 0

struct local_hit * rt_spl_hit_head = NULLHIT;

/* Algorithm - Fire a ray at the bounding box of a b_spline surface.  If an
 * intersection is found then subdivide the surface creating two new
 * surfaces and freeing the original surface.  Recursively shoot at each of
 * the new surfaces until it misses or hits.  A hit is found if the
 * bounding box is intersects within the ray cone and if there are no
 * ineterior knots.
 *
 *  
 *	   -- (cone)
 *       --    |---| bounding box
 *  ray -------------------------->
 *       --    |---|
 *         --
 */
void			rt_spl_n_shoot();
void			rt_spl_add_hit();
void			rt_spl_n_free();
void			rt_spl_shot_poly();
struct local_hit	*rt_spl_get_next_hit();
struct local_hit	*rt_spl_ray_poly();

#define SPL_NULL	((struct b_spline *)0)


/* 
 *			R T _ S P L _ P R E P
 *
 * Given a pointer of a GED database record, and a transformation matrix,
 * determine if this is avalid B_spline solid, and if so prepare the
 * surface so that the subdivision works.
 */
int
rt_spl_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_spl_internal	*sip;
	struct b_head		*nlist = (struct b_head *) 0;
	int			i;

	sip = (struct rt_spl_internal *)ip->idb_ptr;
	RT_SPL_CK_MAGIC(sip);

	for( i=0; i < sip->nsurf; i++ )  {
		struct b_head * s_tree;
		struct b_spline * new_srf;
		struct b_spline * s_split;

		new_srf = sip->surfs[i];

		GETSTRUCT( s_tree, b_head );
		GETSTRUCT( s_tree->left, b_tree );
		GETSTRUCT( s_tree->right, b_tree );

		/* Add to linked list */
		s_tree->next = nlist;
		nlist = s_tree;

		s_split = (struct b_spline * ) spl_split( new_srf, ROW);
		s_tree->left->root = s_split;
		s_tree->right->root = s_split->next;
		s_split->next = (struct b_spline *) 0;

		s_tree->left->left = s_tree->left->right = NULLTREE;
		s_tree->right->left = s_tree->right->right = NULLTREE;

		s_tree->root = new_srf;
		s_tree->u_diff = (struct b_spline *) spl_u_diff( new_srf );
		s_tree->v_diff = (struct b_spline *) spl_v_diff( new_srf );

		spl_bound( s_tree->root, s_tree->min, s_tree->max);
		spl_bound( s_tree->left->root, 
		    s_tree->left->min, s_tree->left->max);
		spl_bound( s_tree->right->root, 
		    s_tree->right->min, s_tree->right->max);
		VMINMAX( stp->st_min, stp->st_max, s_tree->min );
		VMINMAX( stp->st_min, stp->st_max, s_tree->max );
		if ( rt_g.debug & DEBUG_SPLINE ) {
			rt_pr_spl( "initial surface",s_tree->root );
			fprintf(stderr, "bounding box\n");
			VPRINT("min", s_tree->min);
			VPRINT("max", s_tree->max);
		}
	}
	stp->st_specific = (genptr_t)nlist;

	VADD2SCALE( stp->st_center, stp->st_max, stp->st_min, 0.5 );
	{
		fastf_t f, dx, dy, dz;
		dx = (stp->st_max[0] - stp->st_min[0])/2;
		f = dx;
		dy = (stp->st_max[1] - stp->st_min[1])/2;
		if( dy > f )  f = dy;
		dz = (stp->st_max[2] - stp->st_min[2])/2;
		if( dz > f )  f = dz;
		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	}

	return(0);		/* OK */
}

/*
 *			R T _ S P L _ P R I N T
 */
void
rt_spl_print( stp )
register struct soltab * stp;
{
	register struct b_head * ncnt = (struct b_head *) stp->st_specific;

	if( ncnt  == (struct b_head *)0 )  {
		rt_log("spline(%s):  no surfaces\n", stp->st_name);
		return;
	}

	for( ; ncnt != (struct b_head *)0; ncnt = ncnt->next )
		rt_pr_spl( "B_Spline", ncnt->root );
}

/* 
 *			R T _ S P L _ U V
 */
void
rt_spl_uv(ap, stp, hitp, uvp)
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{

	uvp->uv_u = hitp->hit_vpriv[0];
	uvp->uv_v = hitp->hit_vpriv[1];

	return;
}

int
rt_spl_class()
{
	return(0);
}

/*
 *			R T _ S P L _ P L O T
 */
int
rt_spl_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_spl_internal	*sip;
	register int	i;
	register int	j;
	register fastf_t *vp;
	int		s;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_spl_internal *)ip->idb_ptr;
	RT_SPL_CK_MAGIC(sip);

	for( s=0; s < sip->nsurf; s++ )  {
		register struct b_spline	*new;

		new = sip->surfs[s];
		/* Perhaps some spline refinement here? */

		/* Eliminate any homogenous coordinates */
		if( new->ctl_mesh->pt_type == P4 )  {
			vp = new->ctl_mesh->mesh;
			i = new->ctl_mesh->mesh_size[0] * new->ctl_mesh->mesh_size[1];
			for( ; i>0; i--, vp += new->ctl_mesh->pt_type )  {
				HDIVIDE( vp, vp );
				/* Leaves us with [x,y,z,1] */
			}
		}

		/* 
		 * Draw the control mesh, by tracing each curve.
		 */
#define CTL_POS(a,b)	((((a)*new->ctl_mesh->mesh_size[1])+(b))*new->ctl_mesh->pt_type)
		vp = new->ctl_mesh->mesh;

		for( i = 0; i < new->ctl_mesh->mesh_size[0]; i++) {
			RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_MOVE );
			vp += new->ctl_mesh->pt_type;
			for( j = 1; j < new->ctl_mesh->mesh_size[1]; j++ )  {
				/** CTL_POS( i, j ); **/
				RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_DRAW );
				vp += new->ctl_mesh->pt_type;
			}
		}

		/*
		 *  Connect the Ith points on each curve, to make a mesh.
		 */
		for( i = 0; i < new->ctl_mesh->mesh_size[1]; i++ )  {
			vp = new->ctl_mesh->mesh+CTL_POS( 0, i );
			RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_MOVE );
			for( j = 1; j < new->ctl_mesh->mesh_size[0]; j++ )  {
				vp = new->ctl_mesh->mesh+CTL_POS( j, i );
				RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_DRAW );
			}
		}
	}
	return(0);
}

/*
 *			R T _ S P L _ C U R V E
 */
void
rt_spl_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct b_head *s_ptr = (struct b_head *) hitp->hit_private;

	fastf_t        *u_eval, *v_eval, *s_eval, 
	 	       *u2_eval, *v2_eval, *uv_eval;
	fastf_t         u, v;
	fastf_t         E, F, G;		/* First Fundamental Form */
	fastf_t		L, M, N;		/* Second Fundamental form */
	struct b_spline *u2_srf, *v2_srf, *uv_srf;
	fastf_t         denom;
	fastf_t		wein[4];		/*Weingarten matrix */
	fastf_t		evec[3];
	fastf_t		mean, gauss, discrim;
	vect_t 		norm;

	u = hitp->hit_vpriv[0];
	v = hitp->hit_vpriv[1];

	u2_srf = (struct b_spline *) spl_u_diff(s_ptr->u_diff);
	v2_srf = (struct b_spline *) spl_v_diff(s_ptr->v_diff);
	uv_srf = (struct b_spline *) spl_u_diff(s_ptr->v_diff);

	s_eval = (fastf_t *) spl_srf_eval(s_ptr->root, u, v);
	u_eval = (fastf_t *) spl_srf_eval(s_ptr->u_diff, u, v);
	v_eval = (fastf_t *) spl_srf_eval(s_ptr->v_diff, u, v);

	uv_eval = (fastf_t *) spl_srf_eval(uv_srf, u, v);
	u2_eval = (fastf_t *) spl_srf_eval(u2_srf, u, v);
	v2_eval = (fastf_t *) spl_srf_eval(v2_srf, u, v);

	spl_sfree(u2_srf);
	spl_sfree(uv_srf);
	spl_sfree(v2_srf);

	if (s_ptr->root->ctl_mesh->pt_type == P3) {
		VCROSS( norm, u_eval, v_eval);
		VUNITIZE( norm );
		VMOVE( hitp->hit_normal, norm );
		E = VDOT(u_eval, u_eval);
		F = VDOT(u_eval, v_eval);
		G = VDOT(v_eval, v_eval);

		L = VDOT(norm, u2_eval);
		M = VDOT(norm, uv_eval);
		N = VDOT(norm, v2_eval);

	} else if (s_ptr->root->ctl_mesh->pt_type == P4) {
		vect_t          ue, ve;
		vect_t          u2e, v2e, uve;


		ue[0] =
		    (1.0 / s_eval[3] * u_eval[0]) - (u_eval[3] / s_eval[3]) *
			s_eval[0] / s_eval[3];
		ve[0] =
		    (1.0 / s_eval[3] * v_eval[0]) - (v_eval[3] / s_eval[3]) *
			s_eval[0] / s_eval[3];

		ue[1] =
		    (1.0 / s_eval[3] * u_eval[1]) - (u_eval[3] / s_eval[3]) *
			s_eval[1] / s_eval[3];
		ve[1] =
		    (1.0 / s_eval[3] * v_eval[1]) - (v_eval[3] / s_eval[3]) *
			s_eval[1] / s_eval[3];

		ue[2] =
		    (1.0 / s_eval[3] * u_eval[2]) - (u_eval[3] / s_eval[3]) *
			s_eval[2] / s_eval[3];
		ve[2] =
		    (1.0 / s_eval[3] * v_eval[2]) - (v_eval[3] / s_eval[3]) *
			s_eval[2] / s_eval[3];


		VCROSS( norm, ue, ve );
		VUNITIZE( norm );
		VMOVE( hitp->hit_normal, norm );

		E = VDOT(ue, ue);
		F = VDOT(ue, ve);
		G = VDOT(ve, ve);

		u2e[0] = 1.0 / s_eval[3] * (u2_eval[0]) -
			2 * (u2_eval[3] / s_eval[3]) * u2_eval[0] -
			u2_eval[3] / s_eval[3] * (s_eval[0] / s_eval[3]);

		v2e[0] = 1.0 / s_eval[3] * (v2_eval[0]) -
			2 * (v2_eval[3] / s_eval[3]) * v2_eval[0] -
			v2_eval[3] / s_eval[3] * (s_eval[0] / s_eval[3]);

		u2e[1] = 1.0 / s_eval[3] * (u2_eval[1]) -
			2 * (u2_eval[3] / s_eval[3]) * u2_eval[1] -
			u2_eval[3] / s_eval[3] * (s_eval[1] / s_eval[3]);

		v2e[1] = 1.0 / s_eval[3] * (v2_eval[1]) -
			2 * (v2_eval[3] / s_eval[3]) * v2_eval[1] -
			v2_eval[3] / s_eval[3] * (s_eval[1] / s_eval[3]);

		u2e[2] = 1.0 / s_eval[3] * (u2_eval[2]) -
			2 * (u2_eval[3] / s_eval[3]) * u2_eval[2] -
			u2_eval[3] / s_eval[3] * (s_eval[2] / s_eval[3]);

		v2e[2] = 1.0 / s_eval[3] * (v2_eval[2]) -
			2 * (v2_eval[3] / s_eval[3]) * v2_eval[2] -
			v2_eval[3] / s_eval[3] * (s_eval[2] / s_eval[3]);

		uve[0] = 1.0 / s_eval[3] * uv_eval[0] +
			(-1.0 / (s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[0] + u_eval[3] * v_eval[0] +
			 uv_eval[3] * s_eval[0]) +
			(-2.0 / (s_eval[3] * s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[3] * s_eval[0]);

		uve[1] = 1.0 / s_eval[3] * uv_eval[1] +
			(-1.0 / (s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[1] + u_eval[3] * v_eval[1] +
			 uv_eval[3] * s_eval[1]) +
			(-2.0 / (s_eval[3] * s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[3] * s_eval[1]);

		uve[2] = 1.0 / s_eval[3] * uv_eval[2] +
			(-1.0 / (s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[2] + u_eval[3] * v_eval[2] +
			 uv_eval[3] * s_eval[2]) +
			(-2.0 / (s_eval[3] * s_eval[3] * s_eval[3])) *
			(v_eval[3] * u_eval[3] * s_eval[2]);

		L = VDOT(norm, u2e);
		M = VDOT(norm, uve);
		N = VDOT(norm, v2e);
	} else {
		rt_log("rt_spl_curve: bad mesh point type %d\n",
			s_ptr->root->ctl_mesh->pt_type);
		goto	cleanup;
	}

	denom = ( (E*G) - (F*F) ); 

	gauss = ( L * N - M *M)/ denom;
	mean  = ( G * L + E * N - 2 * F * M) / (2 * denom );

	discrim = sqrt( mean * mean - gauss );

	cvp->crv_c1 = mean - discrim;
	cvp->crv_c2 = mean + discrim;

	if ( APX_EQ( (E * G) - ( F* F ) , 0.0 ) )
	{
		rt_log("first fundamental form is singular E = %g F = %g G = %g\n",
			E,F,G);
		return;
	}

	wein[0] = ( (G * L) - (F * M))/ (denom);
	wein[1] = ( (G * M) - (F * N))/ (denom);
	wein[2] = ( (E * M) - (F * L))/ (denom);
	wein[3] = ( (E * N) - (F * M))/ (denom);


	if ( APX_EQ( wein[1] , 0.0 ) && APX_EQ( wein[3] - cvp->crv_c1, 0.0) )
	{
		evec[0] = 0.0; evec[1] = 1.0;
	} else
	{
		evec[0] = 1.0;
		if( fabs( wein[1] ) > fabs( wein[3] - cvp->crv_c1) )
		{
			evec[1] = (cvp->crv_c1 - wein[0]) / wein[1];
		} else
		{
			evec[1] = wein[2] / ( cvp->crv_c1 - wein[3] );
		}
	}

	VSET( cvp->crv_pdir, 0.0, 0.0, 0.0 );

	cvp->crv_pdir[0] = evec[0] * u_eval[0] + evec[1] * v_eval[0];
	cvp->crv_pdir[1] = evec[0] * u_eval[1] + evec[1] * v_eval[1];
	cvp->crv_pdir[2] = evec[0] * u_eval[2] + evec[1] * v_eval[2];

	VUNITIZE( cvp->crv_pdir );

cleanup:
	rt_free( (char *)s_eval, "spl_curve:s_eval");
	rt_free( (char *)u_eval, "spl_curve:u_eval");
	rt_free( (char *)v_eval, "spl_curve:v_eval");
	rt_free( (char *)u2_eval, "spl_curve:u2_eval");
	rt_free( (char *)v2_eval, "spl_curve:v2_eval");
	rt_free( (char *)uv_eval, "spl_curve:uv_eval");
}

/*
 *			R T _ S P L _ F R E E
 */
void
rt_spl_free( stp )
register struct soltab * stp;
{
	struct b_head * nlist = ( struct b_head *) stp->st_specific;
	struct b_head * c_tree;
	
	for( c_tree = nlist; c_tree != (struct b_head *)0; )
	{
		c_tree = nlist->next;
		rt_spl_n_free( nlist->left );
		rt_spl_n_free( nlist->right );
		spl_sfree( nlist->root);
		spl_sfree( nlist->u_diff);
		spl_sfree( nlist->v_diff);
		rt_free( (char *)nlist, "rt_spl_free: b_head structure");
	}

	return;
}

/*
 *			R T _ S P L _ N _ F R E E
 */
void
rt_spl_n_free( tree)
struct b_tree * tree;
{
	struct b_tree * leftp, * rightp, * rootp;
	
	rootp = tree;

	if ( tree->left != (struct b_tree *) 0 )
	{
		leftp = tree->left;
		rt_spl_n_free( leftp );
	}

	if ( tree->right != (struct b_tree *) 0 )
	{
		rightp = tree->right;
		rt_spl_n_free( rightp );
	}

	if ( rootp->root != (struct b_spline *) 0 )
		spl_sfree( rootp->root );

	rt_free( (char *)rootp, "rt_spl_n_free: tree structure ");

}

/* 
 *			R T _ S P L _ N O R M
 */
void
rt_spl_norm(hitp, stp, rp)
register struct hit * hitp;
struct soltab * stp;
register struct xray *rp;
{
	struct b_head * h_ptr = (struct b_head *) hitp->hit_private;
	fastf_t u = hitp->hit_vpriv[0];
	fastf_t v = hitp->hit_vpriv[1];
	fastf_t *u_eval, * v_eval;
	vect_t norm;
	
	/* if the object is linear in one of the directions
	 * for now just use the existing normal from the polygon
         * else calculate it from the derivatives
         */

	if ( h_ptr->root->order[0] == 2 || h_ptr->root->order[1] == 2 )
		return;

	if( h_ptr->root->ctl_mesh->pt_type == P3)
	{
		u_eval = (fastf_t *) 
			spl_srf_eval( h_ptr->u_diff, u, v);

		v_eval = (fastf_t *) 
			spl_srf_eval( h_ptr->v_diff, u, v);

		VCROSS( norm, u_eval, v_eval);
		VUNITIZE( norm);
		VMOVE( hitp->hit_normal, norm);
	} else 
	if( h_ptr->root->ctl_mesh->pt_type == P4)
	{
		fastf_t * spl_eval;
		fastf_t  w, one_w;
		point_t u_norm, v_norm;

		u_eval = (fastf_t *) 
			spl_srf_eval( h_ptr->u_diff, u, v);

		v_eval = (fastf_t *) 
			spl_srf_eval( h_ptr->v_diff, u, v);
		
		spl_eval = (fastf_t *)
			spl_srf_eval( h_ptr->root, u, v);

		w = spl_eval[3];
		one_w = 1.0 / w;

		u_norm[0] = (one_w * u_eval[0]) - 
			(u_eval[3] / w * ( spl_eval[0] / w ));

		u_norm[1] = (one_w * u_eval[1]) - 
			(u_eval[3] / w * ( spl_eval[1] / w ));

		u_norm[2] = (one_w * u_eval[2]) - 
			(u_eval[3] / w * ( spl_eval[2] / w));


		v_norm[0] = (one_w * v_eval[0]) - 
			(v_eval[3] / w * ( spl_eval[0] / w ));

		v_norm[1] = ( one_w * v_eval[1]) - 
			(v_eval[3] / w * ( spl_eval[1] / w ));

		v_norm[2] = ( one_w * v_eval[2]) - 
			(v_eval[3] / w * ( spl_eval[2] / w ));

		VCROSS( norm, u_norm, v_norm );
		VUNITIZE( norm);
		VMOVE(hitp->hit_normal, norm);
		rt_free( (char *)spl_eval, "rt_spl_curve: spl_eval" );
	}
	else
	{
		rt_log("rt_spl_curve: bad mesh point type %d\n",
			h_ptr->root->ctl_mesh->pt_type);
		return;
	}

	if ( hitp->hit_vpriv[2] )
	{	
		VREVERSE( hitp->hit_normal, norm );
	}

	rt_free( (char *)u_eval, "rt_spl_curve: u_eval" );
	rt_free( (char *)v_eval, "rt_spl_curve: v_eval" );

	return;
}


/* 
 *			R T _ S P L _ S H O T 
 *
 * Intersect a ray with a set of b_spline surfaces. If an intersection
 * occurs a struct seg will be acquired and filled in.
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */

struct b_head * curr_tree;

int
rt_spl_shot( stp,  rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	struct b_head * nlist = ( struct b_head *) stp->st_specific;
	auto vect_t invdir;
	struct seg * segp;

	invdir[0] = invdir[1] = invdir[2] = INFINITY;
	if(!NEAR_ZERO(rp->r_dir[0], SQRT_SMALL_FASTF)) 
		invdir[0] = 1.0 / rp->r_dir[0];
	if(!NEAR_ZERO(rp->r_dir[1], SQRT_SMALL_FASTF)) 
		invdir[1] = 1.0 / rp->r_dir[1];
	if(!NEAR_ZERO(rp->r_dir[2], SQRT_SMALL_FASTF)) 
		invdir[2] = 1.0 / rp->r_dir[2];

	RES_ACQUIRE( &rt_g.res_model );	

	for(; nlist != (struct b_head *) 0; nlist = nlist->next )
	{
		curr_tree = nlist;
		rt_spl_n_shoot( rp, invdir, nlist->left,  ap, 0 );
		rt_spl_n_shoot( rp, invdir, nlist->right, ap, 0 );
	}

	/* Sort the hit points and create the segments if only one hit
	 * than add a distance and fake one.
	 */

	if (rt_spl_hit_head == NULLHIT )
	{
		RES_RELEASE( &rt_g.res_model );	
		return (0);
	}

	while ( rt_spl_hit_head != NULLHIT)
	{
		register struct local_hit * hit1, * hit2;
		register struct seg * seg2p;

		RT_GET_SEG( segp, ap->a_resource );

		hit1 = rt_spl_get_next_hit( );
		hit2 = rt_spl_get_next_hit( );

		segp->seg_stp = stp;
		segp->seg_in.hit_dist = hit1->hit_dist;
		VMOVE(segp->seg_in.hit_point, hit1->hit_point );
		VMOVE(segp->seg_in.hit_normal, hit1->hit_normal);
		VMOVE(segp->seg_in.hit_vpriv,hit1->hit_vpriv);
		segp->seg_in.hit_private = hit1->hit_private;
		
		if (hit2 != NULLHIT)
		{
			segp->seg_out.hit_dist = hit2->hit_dist;
			VMOVE(segp->seg_out.hit_point, hit2->hit_point );
			VMOVE(segp->seg_out.hit_vpriv,hit2->hit_vpriv);
			segp->seg_out.hit_private = hit2->hit_private;
			rt_free( (char *)hit2, "rt_spl_shot: hit point");
		} else	/* Fake it */
		{
			segp->seg_out.hit_dist = hit1->hit_dist + .01;
			VJOIN1( segp->seg_out.hit_point,
			    rp->r_pt, segp->seg_out.hit_dist, rp->r_dir);
			VMOVE(segp->seg_out.hit_vpriv,hit1->hit_vpriv);
			segp->seg_out.hit_vpriv[2] = 1; /* flip normal */
			segp->seg_out.hit_private = hit1->hit_private;
		}

		rt_free( (char *)hit1, "rt_spl_shot: hit point");

		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	RES_RELEASE( &rt_g.res_model );	
	return(2);
}

/*
 *			R T _ S P L _ N _ S H O O T
 */
void
rt_spl_n_shoot( rp,  invdir,  tree, ap, level)
register struct xray *rp;
fastf_t * invdir;
struct b_tree * tree;
struct application *ap;
int level;
{
	int flat;
	struct b_spline * sub;
	fastf_t pix_size;

	if ( tree == NULLTREE)	/* Passed a null pointer  */
		return;

	if ( !rt_in_rpp ( rp,  invdir,  tree->min,  tree->max))
		return;

	pix_size = (ap->a_rbeam + ap->a_diverge * rp->r_max);

	if ( tree->root != (struct b_spline *) 0 )  {

		if( spl_check( tree->root ) < 0)  {
			rt_pr_spl("rt_spl_n_shoot: bad tree root", tree->root);
			return;
		}

		flat =	spl_flat( tree->root, pix_size );
		if (flat == FLAT)
		{
			rt_spl_shot_poly( rp,  tree, level);
			return;
		}

		sub = (struct b_spline *) 
		spl_split( tree->root, flat);
		if( spl_check( sub ) < 0 || 
		    spl_check( sub->next ) < 0 )  {
		    	rt_pr_spl("error in spl_split() input:", tree->root);
			rt_pr_spl("Left output:", sub);
			rt_pr_spl("Right output:", sub->next);
		    	return;
		}

		/* Record new right and left descendants */
		GETSTRUCT( tree->left, b_tree );
		GETSTRUCT( tree->right, b_tree );
		tree->left->root = sub;
		spl_bound( tree->left->root,
		    tree->left->min, tree->left->max);
		tree->left->left = tree->left->right = NULLTREE;

		tree->right->root = sub->next; 		
		spl_bound( tree->right->root,
		    tree->right->min, tree->right->max);
		tree->right->left = tree->right->right = NULLTREE;

		/* Now, release old "root" (leaf) */
		spl_sfree( tree->root );
		tree->root = (struct b_spline *) 0;
	}

	if ( rt_g.debug & DEBUG_SPLINE ) 
	    rt_log("spline: Left tree level %d\n", level);

	rt_spl_n_shoot( rp,  invdir,  tree->left, ap, level+1 );

	if ( rt_g.debug & DEBUG_SPLINE ) 
	    rt_log("spline: Right tree level %d\n", level);

	rt_spl_n_shoot( rp,  invdir,  tree->right, ap, level+1);
}

/*
 *			R T _ S P L _ S H O T _ P O L Y
 */
void
rt_spl_shot_poly( rp, tree, level )
struct xray *rp;
struct b_tree * tree;
int level;
{
	struct  spl_poly * _poly, *p, *tmp;
	struct  local_hit * h0;
	int hit_count;

	hit_count = 0;
	
	_poly = (struct spl_poly *) spl_to_poly( tree->root );

	for( p = _poly; p!= ( struct spl_poly *)0; p = p->next)
	{
		h0 = rt_spl_ray_poly( rp, p);
		if ( h0 != NULLHIT )
		{

			if ( rt_g.debug & DEBUG_SPLINE ) 
			    rt_log("spline: Hit found at level %d\n",
				level);
			hit_count++;
			rt_spl_add_hit( h0 );
			break;
		}
	}

	for ( p = _poly; p != (struct spl_poly *) 0;  )
	{
		tmp = p;
		p = p->next;
		rt_free( (char *)tmp, "rt_spl_shot_poly: polygon" );
	}

	if ( !hit_count && rt_g.debug & DEBUG_SPLINE )
	{
		rt_log("Bounding Box hit but no surface hit");
		VPRINT("min", tree->min);
		VPRINT("max", tree->max);
		rt_pr_spl("B_Spline surface", tree->root);
	}

}

#define EQ_HIT(a,b)	( ((a) - (b) ) < EPSILON)

/* If this is a duplicate of an existing hit point than
 * it means that the ray hit exactly on an edge of a patch 
 * subdivision and you need to thow out the extra hit.
 */

/*
 *			R T _ S P L _ A D D _ H I T
 */
void
rt_spl_add_hit( hit1 )
struct local_hit * hit1;
{

	register struct local_hit * h_ptr;

	if ( rt_spl_hit_head == NULLHIT) {
	        hit1 ->next = hit1-> prev = NULLHIT;
		rt_spl_hit_head = hit1;
		return;
	}

	/* check for duplicates */
	for( h_ptr = rt_spl_hit_head; h_ptr != NULLHIT; h_ptr = h_ptr->next)
	{
		if( EQ_HIT(hit1->hit_dist, h_ptr->hit_dist ) &&
		    EQ_HIT(hit1->hit_vpriv[0], h_ptr->hit_vpriv[0]) &&
		    EQ_HIT(hit1->hit_vpriv[1], h_ptr->hit_vpriv[1]) )
		{
			rt_free( (char *) hit1, "rt_spl_add_hit: duplicate");
			return;
		}
	}
	
	hit1->prev = NULLHIT;
	hit1->next = rt_spl_hit_head;
	rt_spl_hit_head->prev = hit1;
	rt_spl_hit_head = hit1;
}

/*
 *			R T _ S P L _ G E T _ N E X T _ H I T
 */
struct local_hit *
rt_spl_get_next_hit(  )
{
	register struct local_hit * list_ptr;
	struct local_hit *rt_hit = NULLHIT;
	fastf_t dist;

	dist = INFINITY;

	if (rt_spl_hit_head == NULLHIT)
		return NULLHIT;

	for( list_ptr = rt_spl_hit_head;
		list_ptr != NULLHIT; list_ptr = list_ptr->next )
	{
		if (list_ptr->hit_dist < dist )
		{
			rt_hit = list_ptr;
			dist = list_ptr->hit_dist;
		}
	}

					/* remove rtn_hit from list */
	if ( rt_hit != NULLHIT )
	{
	    if ( rt_spl_hit_head == rt_hit)
		rt_spl_hit_head = rt_hit->next;
	    if ( rt_hit->prev != NULLHIT)
		rt_hit->prev->next = rt_hit->next;
	    if ( rt_hit->next != NULLHIT)
		rt_hit->next->prev = rt_hit->prev;
  	    rt_hit->next = rt_hit->prev = NULLHIT;

	    return rt_hit;
	} else
		return NULLHIT;
}

/*
 *			R T _ S P L _ R A Y _ P O L Y
 */
struct local_hit *
rt_spl_ray_poly( rp, p1 )
struct xray * rp;
struct spl_poly * p1;
{
	register struct local_hit * h0;
	point_t pt1, pt2, pt3;
	point_t Q, B, norm;
	fastf_t d, offset, t;
	unsigned int i0, i1, i2;
	fastf_t NX, NY, NZ;

	norm[0] = (p1->ply[0])[1] * ((p1->ply[1])[2] - (p1->ply[2])[2]) + 
		  (p1->ply[1])[1] * ((p1->ply[2])[2] - (p1->ply[0])[2]) + 
		  (p1->ply[2])[1] * ((p1->ply[0])[2] - (p1->ply[1])[2]);

	norm[1] = -((p1->ply[0])[0] * ((p1->ply[1])[2] - (p1->ply[2])[2]) + 
		    (p1->ply[1])[0] * ((p1->ply[2])[2] - (p1->ply[0])[2]) + 
		    (p1->ply[2])[0] * ((p1->ply[0])[2] - (p1->ply[1])[2]));

	norm[2] = (p1->ply[0])[0] * ((p1->ply[1])[1] - (p1->ply[2])[1]) + 
		  (p1->ply[1])[0] * ((p1->ply[2])[1] - (p1->ply[0])[1]) + 
		  (p1->ply[2])[0] * ((p1->ply[0])[1] - (p1->ply[1])[1]);

	offset = -(
		(p1->ply[0])[0] * ((p1->ply[1])[1] * (p1->ply[2])[2] - 
	        (p1->ply[2])[1] * (p1->ply[1])[2]) + 

		(p1->ply[1])[0] * ((p1->ply[2])[1] * (p1->ply[0])[2] - 
	        (p1->ply[0])[1] * (p1->ply[2])[2]) +

    		(p1->ply[2])[0] * ((p1->ply[0])[1] * (p1->ply[1])[2] - 
		(p1->ply[1])[1] * (p1->ply[0])[2]));

	d = ((norm)[0] * (rp->r_dir)[0] + 
	    (norm)[1] * (rp->r_dir)[1] + 
	    (norm)[2] * (rp->r_dir)[2]);

	if ( fabs(d) < 0.0001 )
		return 0;

	t = -((
	    norm[0] * rp->r_pt[0] + 
	    norm[1] * rp->r_pt[1] + 
	    norm[2] * rp->r_pt[2]) + offset) / d;

	NX  = fabs( norm[X] );
	NY  = fabs( norm[Y] );
	NZ  = fabs( norm[Z] );

	i0 = 0; i1 = 1; i2 = 2;

	if ( NY > NX) { 
		i0 = 1; 
		i1 = 0; 
	}

	if ( NZ > NY) { 
		i0 = 2; 
		i1 = 1; 
		i2 = 0;
	}

	Q[i0] = rp->r_pt[i0] + rp->r_dir[i0] * t;
	Q[i1] = rp->r_pt[i1] + rp->r_dir[i1] * t;
	Q[i2] = rp->r_pt[i2] + rp->r_dir[i2] * t;

	VSUB2( pt1, (p1->ply[2]), (p1->ply[1]));
	VSUB2( pt2, Q, (p1->ply[1]));
	VCROSS( pt3, pt1, pt2 );
	B[X] = pt3[i0] / norm[i0];
	if( B[X] < 0.0 || B[X] > 1.0 )
		return 0;

	VSUB2( pt1, (p1->ply[0]), (p1->ply[2]));
	VSUB2( pt2, Q, (p1->ply[2]));
	VCROSS( pt3, pt1, pt2 );
	B[Y] = pt3[i0] / norm[i0];
	if( B[Y] < 0.0 || B[Y] > 1.0 )
		return 0;

	VSUB2( pt1, (p1->ply[1]), (p1->ply[0]));
	VSUB2( pt2, Q, (p1->ply[0]));
	VCROSS( pt3, pt1, pt2 );
	B[Z] = pt3[i0] / norm[i0];
	if( B[Z] < 0.0 || B[Z] > 1.0 )
		return 0;
		
	/* if we reach this point we have a hit */
	h0 = (struct local_hit *) rt_malloc ( sizeof ( struct local_hit ), 
		"rt_spl_ray_poly: hit point");

	h0->next = (struct local_hit *)0;
	h0->prev = (struct local_hit *)0;

	h0->hit_dist = t;
	VMOVE(h0->hit_normal, norm );
	VUNITIZE(h0->hit_normal);
	if( VDOT( rp->r_dir, h0->hit_normal ) > 0 )
		VREVERSE( h0->hit_normal, h0->hit_normal );
	
	VMOVE(h0->hit_point, Q);
	h0->hit_vpriv[0] = 
		((p1->uv[0][0] * B[0]) + (p1->uv[1][0] * B[1]) + 
		(p1->uv[2][0] * B[2]));
	h0->hit_vpriv[1] = 
		((p1->uv[0][1] * B[0]) + (p1->uv[1][1] * B[1]) + 
		(p1->uv[2][1] * B[2]));

	h0->hit_vpriv[2] = 0;			/* if set flip normal */

	h0->hit_private = (char *) curr_tree;
	
	if( rt_g.debug & DEBUG_SPLINE)
	{ 
		VPRINT("hit point", h0->hit_point);
		fprintf(stderr,"u = %f  v = %f\n ", h0->hit_vpriv[0], h0->hit_vpriv[1]);
	} 
	
	return h0;
}

/*
 *			R T _ S P L _ T E S S
 */
int
rt_spl_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_spl_internal	*sip;
	int	i;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_spl_internal *)ip->idb_ptr;
	RT_SPL_CK_MAGIC(sip);

	return(-1);
}

/*
 *			R T _ S P L _ I M P O R T
 *
 *  Read all the curves in as a two dimensional array.
 *  The caller is responsible for freeing the dynamic memory.
 *
 *  Note that in each curve array, the first point is replicated
 *  as the last point, to make processing the data easier.
 */
int
rt_spl_import( ip, ep, mat )
struct rt_db_internal	*ip;
struct rt_external	*ep;
mat_t			mat;
{
	struct rt_spl_internal *sip;
	union record	*rp;
	register int	i, j;
	LOCAL vect_t	base_vect;
	int		currec;
	int		s;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_BSOLID )  {
		rt_log("rt_spl_import: defective header record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_BSPLINE;
	ip->idb_ptr = rt_malloc(sizeof(struct rt_spl_internal), "rt_spl_internal");
	sip = (struct rt_spl_internal *)ip->idb_ptr;
	sip->magic = RT_SPL_INTERNAL_MAGIC;

	sip->nsurf = rp->B.B_nsurf;
	sip->resolution = rp->B.B_resolution;

	sip->surfs = (struct b_spline **)rt_malloc(
		sip->nsurf * sizeof(struct b_spline *), "spl surfs[]" );

	rp++;
	for( s = 0; s < sip->nsurf; s++ )  {
		register fastf_t	*mesh_ptr;
		int			epv;
		register dbfloat_t	*vp;

		if( rp->u_id != ID_BSURF )  {
			rt_log("rt_spl_import: defective surface record\n");
			return(-1);
		}
		/*
		 * Allocate memory for this surface.
	 	 */
		sip->surfs[s] = (struct b_spline *) spl_new(
			rp->d.d_order[0], rp->d.d_order[1],
			rp->d.d_kv_size[0], rp->d.d_kv_size[1],
			rp->d.d_ctl_size[0], rp->d.d_ctl_size[1],
			rp->d.d_geom_type );

		/* Read in the knot vectors and convert them to the 
		 * internal representation.
		 */
		vp = (dbfloat_t *) &rp[1];

		/* U knots */
		for( i = 0; i < rp->d.d_kv_size[0]; i++)
			sip->surfs[s]->u_kv->knots[i] = (fastf_t) *vp++;
		/* V knots */
		for( i = 0; i < rp->d.d_kv_size[1]; i++)
			sip->surfs[s]->v_kv->knots[i] =  (fastf_t) *vp++;

		spl_kvnorm( sip->surfs[s]->u_kv);
		spl_kvnorm( sip->surfs[s]->v_kv);

		/*
		 *  Transform the mesh control points in place,
		 *  in the b-spline data structure.
		 */
		vp = (dbfloat_t *) &rp[rp->d.d_nknots+1];
		mesh_ptr = sip->surfs[s]->ctl_mesh->mesh;
		epv = rp->d.d_geom_type;
		i = ( rp->d.d_ctl_size[0] * rp->d.d_ctl_size[1]);
		if( epv == P3 )  {
			for( ; i > 0; i--)  {
				MAT4X3PNT( mesh_ptr, mat, vp);
				mesh_ptr += P3;
				vp += P3;
			}
		} else if( epv == P4 )  {
			for( ; i > 0; i--)  {
				MAT4X4PNT( mesh_ptr, mat, vp);
				mesh_ptr += P4;
				vp += P4;
			}
		} else {
			rt_log("rt_spl_import:  %d invalid elements-per-vect\n", epv );
			return(-1);
		}
		rp += 1 + rp->d.d_nknots + rp->d.d_nctls;
	}
	return( 0 );
}

/*
 *			R T _ S P L _ E X P O R T
 *
 *  The name will be added by the caller.
 *  Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_spl_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
#if 0
	struct rt_spl_internal	*sip;
	union record		*rec;
	point_t		base_pt;
	int		per_curve_grans;
	int		cur;		/* current curve number */
	int		gno;		/* current granule number */

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SPL )  return(-1);
	sip = (struct rt_spl_internal *)ip->idb_ptr;
	RT_SPL_CK_MAGIC(sip);

	per_curve_grans = (sip->pts_per_curve+7)/8;

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (1 + per_curve_grans * sip->ncurves) *
		sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "spl external");
	rec = (union record *)ep->ext_buf;

	rec[0].a.a_id = ID_SPL_A;
	rec[0].a.a_type = SPL;			/* obsolete? */
	rec[0].a.a_m = sip->ncurves;
	rec[0].a.a_n = sip->pts_per_curve;
	rec[0].a.a_curlen = per_curve_grans;
	rec[0].a.a_totlen = per_curve_grans * sip->ncurves;

	VMOVE( base_pt, &sip->curves[0][0] );
	/* The later subtraction will "undo" this, leaving just base_pt */
	VADD2( &sip->curves[0][0], &sip->curves[0][0], base_pt);

	gno = 1;
	for( cur=0; cur<sip->ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;
		int			left;

		fp = sip->curves[cur];
		left = sip->pts_per_curve;
		for( npts=0; npts < sip->pts_per_curve; npts+=8, left -= 8 )  {
			register int	el;
			register int	lim;
			register struct spl_ext	*bp = &rec[gno].b;

			bp->b_id = ID_SPL_B;
			bp->b_type = SPLCONT;	/* obsolete? */
			bp->b_n = cur+1;		/* obsolete? */
			bp->b_ngranule = (npts/8)+1; /* obsolete? */

			lim = (left > 8 ) ? 8 : left;
			for( el=0; el < lim; el++ )  {
				vect_t	diff;
				VSUB2SCALE( diff, fp, base_pt, local2mm );
				/* NOTE: also type converts to dbfloat_t */
				VMOVE( &(bp->b_values[el*3]), diff );
				fp += ELEMENTS_PER_VECT;
			}
			gno++;
		}
	}
#endif
	return(0);
}

/*
 *			R T _ S P L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_spl_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register int			j;
	register struct rt_spl_internal	*sip =
		(struct rt_spl_internal *)ip->idb_ptr;
	char				buf[256];
	int				i;
	int				surf;

	RT_SPL_CK_MAGIC(sip);
	rt_vls_strcat( str, "arbitrary rectangular solid (SPL)\n");

	sprintf(buf, "\t%d surfaces, resolution=%g\n",
		sip->nsurf, sip->resolution );
	rt_vls_strcat( str, buf );

	for( surf=0; surf < sip->nsurf; surf++ )  {
		register struct b_spline	*sp;
		register struct b_mesh		*mp;

		sp = sip->surfs[surf];
		mp = sp->ctl_mesh;
		sprintf(buf, "\tSurface %d:  order %d * %d, mesh %d * %d\n",
			surf,
			sp->order[0],
			sp->order[1],
			mp->mesh_size[0],
			mp->mesh_size[1] );
		rt_vls_strcat( str, buf );

		sprintf(buf, "\t\tKnot vector %d + %d\n",
			sp->u_kv->k_size,
			sp->u_kv->k_size );
		rt_vls_strcat( str, buf );

		sprintf(buf, "\t\tV (%g, %g, %g)\n",
			mp->mesh[X] * mm2local,
			mp->mesh[Y] * mm2local,
			mp->mesh[Z] * mm2local );
		rt_vls_strcat( str, buf );

		if( !verbose )  continue;

		/* Print out all the points */
		for( i=0; i < mp->mesh_size[0]; i++ )  {
			register fastf_t *v = mp->mesh;

			sprintf( buf, "\Row %d:\n", i );
			rt_vls_strcat( str, buf );
			for( j=0; j < mp->mesh_size[1]; j++ )  {
				sprintf(buf, "\t\t(%g, %g, %g)\n",
					v[X] * mm2local,
					v[Y] * mm2local,
					v[Z] * mm2local );
				rt_vls_strcat( str, buf );
				v += ELEMENTS_PER_VECT;
			}
		}
	}

	return(0);
}

/*
 *			R T _ S P L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_spl_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_spl_internal	*sip;
	register int			i;

	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_spl_internal *)ip->idb_ptr;
	RT_SPL_CK_MAGIC(sip);

	/*
	 *  Free storage for faces
	 */
	for( i = 0; i < sip->nsurf; i++ )  {
		spl_sfree( sip->surfs[i] );
	}
	sip->magic = 0;		/* sanity */
	sip->nsurf = 0;
	rt_free( (char *)sip, "spl ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
