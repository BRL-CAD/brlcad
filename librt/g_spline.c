/* 
 *       S P L I N E . C
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

struct local_hit * spl_hit_head = NULLHIT;
int hit_count;

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

/* 
 * S P L _ P R E P
 *
 * Given a pointer of a GED database record, and a transformation matrix,
 * determine if this is avalid B_spline solid, and if so prepare the
 * surface so that the subdivision works.
 */

/* Since the record granuals consist of floating point values
 * we need to decalre some of the read variables as dbfloat_t and not
 * fastf_t.
 */

spl_prep( vec,  stp,  mat, sp, rtip)
register fastf_t * vec;
struct soltab *stp;
matp_t mat;
struct solidrec *sp;
struct rt_i * rtip;
{
	struct B_solid * bp;
	struct b_head  *nlist = (struct b_head *) 0;
	static union record rec;
	register int    i;
	int n_srfs;

	bp = (struct B_solid *) sp;

	n_srfs = bp->B_nsurf;

	while (n_srfs-- > 0) {
		struct b_head * s_tree;
		struct b_spline * new_srf;
		struct b_spline * s_split;
		int nbytes, nby;
		dbfloat_t * vp;
		dbfloat_t * fp;
		fastf_t * mesh_ptr;
		int epv;

		i = fread( (char *) &rec, sizeof(rec), 1, rtip->fp );

		if( i != 1 )
			rt_bomb("spl_prep: read error");

		if ( rec.u_id != ID_BSURF )
		{
			break;
		}

		/* Read in the knot vectors and convert them to the 
		 * internal representation.
		 */
		nbytes = rec.d.d_nknots * sizeof( union record );
		fp = vp = ( dbfloat_t *) rt_malloc( nbytes, "spl knots" );
		if ( fread( (char *) vp, nbytes, 1, rtip->fp ) != 1 )
		{
			break;
		}

		epv = rec.d.d_geom_type;

		/* If everything up to here is ok then allocate memory
		 * for a surface.
	 	 */
		new_srf = (struct b_spline *) spl_new(
			rec.d.d_order[0], rec.d.d_order[1],
			rec.d.d_kv_size[0], rec.d.d_kv_size[1],
			rec.d.d_ctl_size[0], rec.d.d_ctl_size[1],
			rec.d.d_geom_type );

		for( i = 0; i < rec.d.d_kv_size[0]; i++){	/* U knots */
			new_srf->u_kv->knots[i] = (fastf_t) *vp++;
		}
		for( i = 0; i < rec.d.d_kv_size[1]; i++)	/* V knots */
			new_srf->v_kv->knots[i] =  (fastf_t) *vp++;

		rt_free( (char *) fp, "spl_prep: fp" );

		/* Read in the mesh control points and convert them to
		 * the b-spline data structure.
		 */
		nby = rec.d.d_nctls * sizeof( union record);
		fp = vp = (dbfloat_t *) rt_malloc(nby,  "control mesh");
		if( fread((char *) vp,  nby,  1, rtip->fp) != 1)
			break;

		spl_kvnorm( new_srf->u_kv);
		spl_kvnorm( new_srf->v_kv);

		mesh_ptr = new_srf->ctl_mesh->mesh;

		i = ( rec.d.d_ctl_size[0] * rec.d.d_ctl_size[1]);
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
			rt_log("%s:  %d not valid elements-per-vect\n",
				stp->st_name, epv );
			return(-1);	/* BAD */
		}

		rt_free( (char *) fp,  "free up mesh points");

		GETSTRUCT( s_tree, b_head );
		GETSTRUCT( s_tree->left, b_tree );
		GETSTRUCT( s_tree->right, b_tree );
		s_tree->next = nlist;
		nlist = s_tree;

		s_split = (struct b_spline * ) spl_split( new_srf, 0);
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
			rt_pr_spl( "left  surface", s_tree->left->root );
			rt_pr_spl( "right  surface",s_tree->right->root );
			fprintf(stderr, "bounding box\n");
			VPRINT("min", s_tree->min);
			VPRINT("max", s_tree->max);
		}
	}

	stp->st_specific = (int *)nlist;
	VSET( stp->st_center,
	    (stp->st_max[0] + stp->st_min[0])/2,
	    (stp->st_max[1] + stp->st_min[1])/2,
	    (stp->st_max[2] + stp->st_min[2])/2 );
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
 * S P L _ P R I N T
 */

spl_print( stp )
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
 *	S P L _ U V
 */
spl_uv(ap, stp, hitp, uvp)
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	uvp->uv_u = hitp->hit_vpriv[0];
	uvp->uv_v = hitp->hit_vpriv[1];
	return;
}

spl_class()
{
}

spl_plot()
{
}

spl_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct b_head *s_ptr = (struct b_head *) hitp->hit_private;

	fastf_t        *u_eval, *v_eval, *s_eval, 
	 	       *u2_eval, *v2_eval, *uv_eval;
	fastf_t         u, v;
	fastf_t         E, F, G, e, f, g;
	struct b_spline *u2_srf, *v2_srf, *uv_srf;
	fastf_t         denom, a11, a12, a21, a22, a, b, c;
	vect_t          vec1, vec2;
	vect_t          uvec, vvec;
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
		E = VDOT(v_eval, v_eval);

		e = VDOT(norm, u2_eval);
		f = VDOT(norm, uv_eval);
		g = VDOT(norm, v2_eval);

	} else if (s_ptr->root->ctl_mesh->pt_type == P4) {
		vect_t          ue, ve;
		vect_t          u2e, v2e, uve;
		vect_t		u_norm;


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

		e = VDOT(norm, u2e);
		f = VDOT(norm, uve);
		g = VDOT(norm, v2e);
	}

	denom = ( (E*G) - (F*F) ); 

	a11 = (( f * F ) -  (e * G) ) / denom;

	a21 = ((e*F) - (f * E))/ denom;

	a12 = ((g * F) - (f * G))/ denom;

	a22 = ((f * F) - (g * E))/ denom;

	a = 1.0;
	b = - ( a11 +  a22);
	c = (a11 * a22) - (a12 * a21);

	rt_orthovec( uvec, hitp->hit_normal );
	VCROSS( vvec, hitp->hit_normal, uvec );

	eigen2x2( &cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c );
	VCOMB2( cvp->crv_pdir, vec1[X], uvec, vec1[Y], vvec );
	VUNITIZE( cvp->crv_pdir );

	rt_free(s_eval, "spl_curve:s_eval");
	rt_free(u_eval, "spl_curve:u_eval");
	rt_free(v_eval, "spl_curve:v_eval");
	rt_free(u2_eval, "spl_curve:u2_eval");
	rt_free(v2_eval, "spl_curve:v2_eval");
	rt_free(uv_eval, "spl_curve:uv_eval");
}

spl_free( stp )
register struct soltab * stp;
{
	struct b_head * nlist = ( struct b_head *) stp->st_specific;
	struct b_head * c_tree;
	
	for( c_tree = nlist; c_tree != (struct b_head *)0; )
	{
		c_tree = nlist->next;
		n_free( nlist->left );
		n_free( nlist->right );
		spl_sfree( nlist->root);
		spl_sfree( nlist->u_diff);
		spl_sfree( nlist->v_diff);
		rt_free( nlist, "spl_free: b_head structure");
	}

	return;
}

n_free( tree)
struct b_tree * tree;
{
	struct b_tree * leftp, * rightp, * rootp;
	
	rootp = tree;

	if ( tree->left != (struct b_tree *) 0 )
	{
		leftp = tree->left;
		n_free( leftp );
	}

	if ( tree->right != (struct b_tree *) 0 )
	{
		rightp = tree->right;
		n_free( rightp );
	}

	if ( rootp->root != (struct b_spline *) 0 )
		spl_sfree( rootp->root );

	rt_free( rootp, "n_free: tree structure ");

}
/* 
 *	S P L _ N O R M
 */
spl_norm(hitp, stp, rp)
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
		rt_free( spl_eval, "ray_poly: spl_eval" );
	}

	if ( hitp->hit_vpriv[2] )
	{	
		VREVERSE( hitp->hit_normal, norm );
	}

	rt_free( u_eval, "ray_poly: u_eval" );
	rt_free( v_eval, "ray_poly: v_eval" );

	return;
}


/* 
 * S P L _ S H O T 
 * Intersect a ray with a set of b_spline surfaces. If an intersection
 * occurs a struct seg will be acquired and filled in.
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */

struct b_head * curr_tree;

struct seg *
spl_shot( stp,  rp, ap)
struct soltab *stp;
register struct xray *rp;
struct application * ap;
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

	hit_count = 0;

	RES_ACQUIRE( &rt_g.res_model );	

	for(; nlist != (struct b_head *) 0; nlist = nlist->next )
	{
		curr_tree = nlist;
		n_shoot( rp, invdir, nlist->left,  ap, 1, 0 );
		n_shoot( rp, invdir, nlist->right, ap, 1, 0 );
	}

	/* Sort the hit points and create the segments if only one hit
	 * than add a distance and fake one.
	 */

	if (spl_hit_head == NULLHIT )
	{
		RES_RELEASE( &rt_g.res_model );	
		return (SEG_NULL);
	}

	GET_SEG( segp, ap->a_resource );

	while ( spl_hit_head != NULLHIT)
	{
		register struct local_hit * hit1, * hit2;
		register struct seg * seg2p;
		struct local_hit * get_next_hit();

		hit1 = get_next_hit( );
		hit2 = get_next_hit( );

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
			rt_free( hit2, "spl_shot: hit point");
		} else	/* Fake it */
		{
			segp->seg_out.hit_dist = hit1->hit_dist + .01;
			VJOIN1( segp->seg_out.hit_point,
			    rp->r_pt, segp->seg_out.hit_dist, rp->r_dir);
			VMOVE(segp->seg_out.hit_vpriv,hit1->hit_vpriv);
			segp->seg_out.hit_vpriv[2] = 1; /* flip normal */
			segp->seg_out.hit_private = hit1->hit_private;
		}

		rt_free( hit1, "spl_shot: hit point");

		if ( spl_hit_head != NULLHIT)
		{
			GET_SEG( seg2p, ap->a_resource);
			seg2p->seg_next = segp;
			segp =  seg2p;
		}
	}
	RES_RELEASE( &rt_g.res_model );	
	return segp;
}

#define OTHERDIR(dir)	( (dir == 0)? 1:0)


n_shoot( rp,  invdir,  tree, ap, dir, level)
register struct xray *rp;
fastf_t * invdir;
struct b_tree * tree;
struct application *ap;
int dir;
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
			rt_pr_spl("n_shoot: bad tree root", tree->root);
			return;
		}

		flat =	spl_flat( tree->root, pix_size );
		if (flat)
		{
			shot_poly( rp,  tree, level);
			return;
		}

		sub = (struct b_spline *) 
		spl_split( tree->root, dir);
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

	n_shoot( rp,  invdir,  tree->left, ap,
	    OTHERDIR(dir), level+1 );

	if ( rt_g.debug & DEBUG_SPLINE ) 
	    rt_log("spline: Right tree level %d\n", level);

	n_shoot( rp,  invdir,  tree->right, ap,
	    OTHERDIR(dir), level+1);
}

shot_poly( rp, tree, level )
struct xray *rp;
struct b_tree * tree;
{
	struct  spl_poly * poly, *p, *tmp;
	struct  local_hit * h0, * ray_poly();
	
	poly = (struct spl_poly *) spl_to_poly( tree->root );

	for( p = poly; p!= ( struct spl_poly *)0; p = p->next)
	{
		h0 = ray_poly( rp, p);
		if ( h0 != NULLHIT )
		{

			if ( rt_g.debug & DEBUG_SPLINE ) 
			    rt_log("spline: Hit found at level %d\n",
				level);
			hit_count++;
			add_hit( h0 );
			break;
		}
	}

	for ( p = poly; p != (struct spl_poly *) 0;  )
	{
		tmp = p;
		p = p->next;
		rt_free( tmp, "shot_poly: polygon" );
	}

	if ( !hit_count && rt_g.debug & DEBUG_SPLINE )
	{
		rt_log("Bounding Box hit but no surface hit");
		rt_pr_spl("B_Spline surface", tree->root);
	}

}

add_hit( hit1 )
struct local_hit * hit1;
{
	if ( spl_hit_head == NULLHIT) {
	        hit1 ->next = hit1-> prev = NULLHIT;
		spl_hit_head = hit1;
		return;
	} else
	{
		hit1->prev = NULLHIT;
		hit1->next = spl_hit_head;
		spl_hit_head->prev = hit1;
		spl_hit_head = hit1;
	}
}

struct local_hit *
get_next_hit(  )
{
	register struct local_hit * list_ptr;
	struct local_hit * rt_hit;
	float dist;

	dist = INFINITY;

	if (spl_hit_head == NULLHIT)
		return NULLHIT;

	for( list_ptr = spl_hit_head;
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
	    if ( spl_hit_head == rt_hit)
		spl_hit_head = rt_hit->next;
	    if ( rt_hit->prev != NULLHIT)
		rt_hit->prev->next = rt_hit->next;
	    if ( rt_hit->next != NULLHIT)
		rt_hit->next->prev = rt_hit->prev;
  	    rt_hit->next = rt_hit->prev = NULLHIT;

	    return rt_hit;
	} else
		return NULLHIT;
}



#define V_CROSS_SIGN( a, b )  	(( ((a[0] * b[1] - a[1] * b[0]) + (a[1] * b[2] - a[2] * b[1]) +	(a[0] * b[2] - a[2] * b[0])) >= 0.0)? 1 : 0)

struct plane {				/* Plane definition */
        point_t  nrm;			/* Plane Normal */
	fastf_t  offset;			/* Plane Offset */
};

struct local_hit *
ray_poly( rp, p1 )
struct xray * rp;
struct spl_poly * p1;
{
	point_t itr_point;
	vect_t b_minus_a, c_minus_b, a_minus_c, itr_cross;
	struct local_hit * h0;
	fastf_t uv[2], tmp;
	struct plane plane_form(), pln;

	float denom, t;
	int curr_sign;

	pln = plane_form( p1->ply[0], p1->ply[1], p1->ply[2] );

	denom = VDOT( pln.nrm, rp->r_dir);

	if (APX_EQ( denom, 0.0))
		return (struct local_hit *) 0;

	t = - (VDOT( pln.nrm, rp->r_pt) + pln.offset)/ denom;

	if ( t < 0.0005  )
		return (struct local_hit *) 0;

	VJOIN1( itr_point, rp->r_pt, t, rp->r_dir);

	VSUB2( b_minus_a, p1->ply[1], p1->ply[0]);
	VSUB2( c_minus_b, p1->ply[2], p1->ply[1]);

	VSUB2( itr_cross, itr_point, p1->ply[0]);
	curr_sign = V_CROSS_SIGN( b_minus_a, itr_cross);

	VSUB2( itr_cross, itr_point, p1->ply[1]);
	if ( V_CROSS_SIGN( c_minus_b, itr_cross) != curr_sign )
		return (struct local_hit *) 0;

	VSUB2( a_minus_c, p1->ply[0], p1->ply[2]);
	VSUB2( itr_cross, itr_point, p1->ply[2]);
	if ( V_CROSS_SIGN( a_minus_c, itr_cross) != curr_sign )
		return (struct local_hit *) 0;
		
	/* if we reach this point we have a hit */

	h0 = (struct local_hit *) rt_malloc ( sizeof ( struct local_hit ), 
		"ray_poly: hit point");

	h0->next = (struct local_hit *)0;
	h0->prev = (struct local_hit *)0;
		
	interp_uv( p1, itr_point, uv );

	h0->hit_dist = t;

	/* This is a hack */
	/* If a surface is linear in either direction than
	 * the normal must be approximated since the dirivatives
	 * can't be used to calculate the normal.
         */

	VMOVE(h0->hit_normal, pln.nrm );
	VUNITIZE(h0->hit_normal);
	if( VDOT( rp->r_dir, h0->hit_normal ) > 0 )
		VREVERSE( h0->hit_normal, h0->hit_normal );
	
	VMOVE(h0->hit_point, itr_point);
	h0->hit_vpriv[0] = uv[0];

	h0->hit_vpriv[1] = uv[1];
	h0->hit_vpriv[2] = 0;			/* if set flip normal */
	h0->hit_private = (char *) curr_tree;
	
	if( rt_g.debug & DEBUG_SPLINE)
	{ 
		VPRINT("hit point", h0->hit_point);
	} 
	
	return h0;
}

/*****************************************************************
 * TAG( plane_form )
 * 
 * Form the plane equation from three points.
 * Inputs:
 * 	Three homogeneous 4 points a, b, and c.
 * Outputs:
 * 	A plane equation.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Cross product expansion from Foley and Van Dam
 */

struct plane
plane_form( a, b, c )
point_t a, b, c;
{
    struct plane plane_p;

    fastf_t x1, y1, z1, x2, y2, z2, x3, y3, z3;

    x1 = a[0];
    y1 = a[1];
    z1 = a[2];
    x2 = b[0];
    y2 = b[1];
    z2 = b[2];
    x3 = c[0];
    y3 = c[1];
    z3 = c[2];

    plane_p.nrm[0] = y1 * (z2 - z3) + y2 * (z3 - z1) + y3 * (z1 - z2);
    plane_p.nrm[1] = -(x1 * (z2 - z3) + x2 * (z3 - z1) + x3 * (z1 - z2));
    plane_p.nrm[2] = x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2);

    plane_p.offset = -(x1 * (y2 * z3 - y3 * z2) + x2 * (y3 * z1 - y1 * z3)
			+ x3 * (y1 * z2 - y2 * z1));

    return plane_p;
}

interp_uv( p1, itr,uv )
fastf_t uv[2];
point_t itr;
struct spl_poly * p1;
{
	point_t tmp, b_minus_a, c_minus_a,
		d_minus_a;

	fastf_t r, s, t, area_s, area_t, area;

	VSUB2( b_minus_a, p1->ply[1], p1->ply[0]);
	VSUB2( c_minus_a, p1->ply[2], p1->ply[0]);

	VCROSS( tmp, b_minus_a, c_minus_a );
	area = MAGNITUDE( tmp );

	if (area <= 0.0)
		fprintf( stderr, "interp_norm: polygon has zero area\n");

	VSUB2( d_minus_a, itr, p1->ply[0] );
	
	VCROSS( tmp, b_minus_a, d_minus_a );
	area_t = MAGNITUDE( tmp );

	VCROSS( tmp, d_minus_a, c_minus_a );
	area_s = MAGNITUDE( tmp );

	t = area_t / area;
	s = area_s / area;
	r = 1.0 - s - t;

	uv[0] = (fastf_t)
		((p1->uv[0][0] * r) + (p1->uv[1][0] * s) + (p1->uv[2][0] * t));
	uv[1] = (fastf_t)
		((p1->uv[0][1] * r) + (p1->uv[1][1] * s) + (p1->uv[2][1] * t));
}
