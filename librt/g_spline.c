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
	struct b_tree * next;   /* Next tree over only used at level 0 */
	point_t	min,  max;  /* Current surface minimum and maximum */
	struct b_spline * root;   /* Leaf b_spline surface. Null if interior */
	struct b_spline * u_diff;
	struct b_spline * v_diff;
	struct b_tree * left;   /* Left sub tree */
	struct b_tree * right;  /* Right sub tree */
	short dir;		    /* Subdivision direction */
	short level;		    /* Tree Depth Level  */
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
	vect_t  hit_vpriv;  /* Store parametric u and v information */
	struct local_hit * next;
	struct local_hit * prev;
};

#define NULLHIT	    (struct local_hit *) 0
#define NULLTREE    (struct b_tree *) 0

struct local_hit * spl_hit_head = NULLHIT;
int hit_count;

#define MINMAX(a,b,c)   { FAST fastf_t ftemp;\
        if( (ftemp = (c)) < (a) )  a = ftemp;\
        if( ftemp > (b) )  b = ftemp; }

#define MM(v)   MINMAX( stp->st_min[0], stp->st_max[0], v[0] ); \
                MINMAX( stp->st_min[1], stp->st_max[1], v[1] ); \
                MINMAX( stp->st_min[2], stp->st_max[2], v[2] )

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
 * we need to decalre some of the read variables as float and not
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
	struct b_tree  *nlist = NULLTREE;
	static union record rec;
	register int    i;
	int n_srfs;

	bp = (struct B_solid *) sp;

	n_srfs = bp->B_nsurf;

	while (n_srfs-- > 0) {
		struct b_tree * s_tree;
		struct b_spline * new_srf;
		int nbytes, nby;
		float * vp;
		float * fp;
		fastf_t * mesh_ptr;

		i = fread( (char *) &rec, sizeof(rec), 1, rtip->fp );

		if( i != 1 )
			rt_bomb("spl_prep: read error");

		if ( rec.u_id != ID_BSURF )
		{
			break;
		}

		if (rec.d.d_geom_type != 3 && rec.d.d_geom_type != 4 )
		{
			fprintf(stderr,"BSURF geom_type = %d\n", 
			    rec.d.d_geom_type );
			return(1);	/* BAD */
		}

		/* Read in the knot vectors and convert them to the 
		 * internal representation.
		 */
		nbytes = rec.d.d_nknots * sizeof( union record );

		if ( (vp = ( float *) rt_malloc( nbytes, "spl knots" ))
		    == (float *) 0 )
		{
			rt_bomb("spl_prep: malloc error\n");
		}

		if ( fread( (char *) vp, nbytes, 1, rtip->fp ) != 1 )
		{
			break;
		}

		/* If everything up to here is ok then allocate memory
		 * for a surface.
	 	 */
		new_srf = (struct b_spline *)
		    rt_malloc( sizeof( struct b_spline),  "b_spline" );
		new_srf->u_kv = (struct knot_vec *)
		    rt_malloc( sizeof( struct knot_vec ),  "u knots" );
		new_srf->v_kv = (struct knot_vec *)
		    rt_malloc( sizeof( struct knot_vec ),  "v knots" );

		new_srf->u_kv->k_size = ( rec.d.d_kv_size[0] );
		new_srf->v_kv->k_size = ( rec.d.d_kv_size[1] );

		new_srf->u_kv->knots = (fastf_t *)
		    rt_malloc( sizeof( fastf_t ) * rec.d.d_kv_size[0], 
		    "u knot values" );
		new_srf->v_kv->knots = ( fastf_t *)
		    rt_malloc( sizeof( fastf_t ) * rec.d.d_kv_size[1], 
		    "v knot values" );

		fp = vp;
		for( i = 0; i < rec.d.d_kv_size[0]; i++){	/* U knots */
			new_srf->u_kv->knots[i] = (fastf_t) *vp++;
		}
		for( i = 0; i < rec.d.d_kv_size[1]; i++)	/* V knots */
			new_srf->v_kv->knots[i] =  (fastf_t) *vp++;

		norm_kv( new_srf->u_kv);
		norm_kv( new_srf->v_kv);
		
		rt_free( (char *) fp, "spl_prep: fp" );

		/* Read in the mesh control points and convert them to
		 * the b-spline data structure.
		 */
		nby = rec.d.d_nctls * sizeof( union record);

		if (( vp = (float *) rt_malloc(nby,  "control mesh"))
		    == (float *)0)
		{
			rt_bomb("spl_prep: malloc error mesh control points");
		}

		if( fread((char *) vp,  nby,  1, rtip->fp) != 1)
			break;

		fp = vp;

		new_srf->ctl_mesh = (struct b_mesh *) 
			rt_malloc( sizeof (struct b_mesh), "B_MESH");;

		new_srf->ctl_mesh->mesh_size[0] = rec.d.d_ctl_size[0];
		new_srf->ctl_mesh->mesh_size[1] = rec.d.d_ctl_size[1];

		new_srf->order[0] = rec.d.d_order[0];
		new_srf->order[1] = rec.d.d_order[1];

		new_srf->ctl_mesh->mesh = (fastf_t *)
		    rt_malloc( sizeof ( fastf_t ) * rec.d.d_ctl_size[0] *
		    rec.d.d_ctl_size[1] * ELEMENTS_PER_VECT,  
		    "new control mesh");


		mesh_ptr = new_srf->ctl_mesh->mesh;

		for( i = 0; 
		     i < ( rec.d.d_ctl_size[0] * rec.d.d_ctl_size[1]);
		     i++)
		{
			float tmp[4];

			if ( rec.d.d_geom_type == 4) {
				MAT4X4PNT( tmp, mat, vp);
				HDIVIDE( mesh_ptr,  tmp);
				mesh_ptr += ELEMENTS_PER_VECT;
				vp += HPT_LEN;
			} else {
				MAT4X3PNT( mesh_ptr, mat, vp);
				mesh_ptr += ELEMENTS_PER_VECT;
				vp += ELEMENTS_PER_VECT;
			}
		}
		rt_free( (char *) fp,  "free up mesh points");

		GETSTRUCT( s_tree, b_tree );
		s_tree->next = nlist;
		nlist = s_tree;

		s_tree->left = NULLTREE;
		s_tree->right = NULLTREE;
		s_tree->root = new_srf;
		s_tree->u_diff = (struct b_spline *) u_diff_spline( new_srf );
		s_tree->v_diff = (struct b_spline *) v_diff_spline( new_srf );
		s_tree->dir = COL;
		s_tree->level = 0;

		bound_spl( s_tree->root, s_tree->min, s_tree->max);
		MM(s_tree->min);
		MM(s_tree->max);
		if ( rt_g.debug & DEBUG_SPLINE ) {
			pr_spl( "initial surface",s_tree->root );
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
	register struct b_tree * ncnt = (struct b_tree *) stp->st_specific;

	if( ncnt  == (struct b_tree *)0 )  {
		rt_log("spline(%s):  no surfaces\n", stp->st_name);
		return;
	}

	for( ; ncnt != NULLTREE; ncnt = ncnt->next )
		pr_spl( "B_Spline", ncnt->root );
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

spl_curve()
{
}

spl_free( stp )
register struct soltab * stp;
{
	struct b_tree * nlist = ( struct b_tree *) stp->st_specific;
	struct b_tree * c_tree;
	
	for( c_tree = nlist; c_tree != (struct b_tree *)0; )
	{
		c_tree = nlist->next;
		n_free( nlist );
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
		free_spl( rootp->root );

	if ( rootp->u_diff != (struct b_spline *) 0 )
		free_spl( rootp->u_diff );

	if ( rootp->v_diff != (struct b_spline *) 0 )
		free_spl( rootp->v_diff );
	
	rt_free( rootp, "n_free: tree structure ");

}
/* 
 *	S P L _ N O R M
 */
spl_norm()
{
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

struct b_tree * curr_tree;

struct seg *
spl_shot( stp,  rp, ap)
struct soltab *stp;
register struct xray *rp;
struct application * ap;
{
	struct b_tree * nlist = ( struct b_tree *) stp->st_specific;
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

	for(; nlist != (struct b_tree *) 0; nlist = nlist->next )
	{
		curr_tree = nlist;
		n_shoot( rp, invdir, nlist, ap );
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

		if (hit2 != NULLHIT)
		{
			segp->seg_out.hit_dist = hit2->hit_dist;
			VMOVE(segp->seg_out.hit_point, hit2->hit_point );
			VMOVE(segp->seg_out.hit_normal, hit2->hit_normal);
			VMOVE(segp->seg_out.hit_vpriv,hit2->hit_vpriv);
			rt_free( hit2, "spl_shot: hit point");
		} else	/* Fake it */
		{
			segp->seg_out.hit_dist = hit1->hit_dist + .01;
			VJOIN1( segp->seg_out.hit_point,
			    rp->r_pt, segp->seg_out.hit_dist, rp->r_dir);
			VMOVE( segp->seg_out.
			    hit_normal, segp->seg_in.hit_normal );
			VMOVE(segp->seg_out.hit_vpriv,hit1->hit_vpriv);
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


n_shoot( rp,  invdir,  tree, ap)
register struct xray *rp;
fastf_t * invdir;
struct b_tree * tree;
struct application *ap;
{
	int flat;
	struct b_spline * sub;
	fastf_t pix_size;

	if ( tree == NULLTREE)	/* Passed a null pointer  */
		return;

	if ( rt_in_rpp ( rp,  invdir,  tree->min,  tree->max))
	{
		pix_size = (ap->a_rbeam + ap->a_diverge * rp->r_max);

		if ( tree->root != (struct b_spline *) 0 )
		{

			if ( (tree->level == 0))
			if ( (tree->left != NULLTREE))
				goto shoot;
		
			flat =	spl_flat( tree->root, pix_size );

			if (flat)
			{
				shot_poly( rp,  tree);
				return;
			}

			sub = (struct b_spline *) 
				split_spl( tree->root, tree->dir);

			if( tree->level >= 1) {
				free_spl( tree->root );
				tree->root = (struct b_spline *) 0;
			}

			tree->left = (struct b_tree *) 
				rt_malloc( sizeof (struct b_tree), 
					"nshoot: left tree");
			tree->right = (struct b_tree *) 
				rt_malloc( sizeof (struct b_tree), 
					"nshoot: right tree");

			tree->left->root = sub;
			tree->left->next = NULLTREE;
			bound_spl( tree->left->root, tree->left->min, tree->left->max);
			tree->left->dir = (tree-> dir == 0) ? 1:0;
			tree->left->level = tree->level + 1;
			tree->left->left = tree->left->right = NULLTREE;
			tree->left->u_diff = (struct b_spline *)0;
			tree->left->v_diff =  (struct b_spline *)0;

			tree->right->root = sub->next; 		
			tree->right->next = NULLTREE;
			bound_spl( tree->right->root, tree->right->min, tree->right->max);
			tree->right->dir = (tree-> dir == 0) ? 1:0;
			tree->right->level = tree->level + 1; 
			tree->right->left =
			tree->right->right = NULLTREE; 		
			tree->right->u_diff = (struct b_spline *)0;
			tree->right->v_diff = (struct b_spline *)0;
		}
shoot:
		if ( rt_g.debug & DEBUG_SPLINE ) 
		    (void)rt_log("spline: Left tree level %d\n", tree->level);

		n_shoot( rp,  invdir,  tree->left, ap);

		if ( rt_g.debug & DEBUG_SPLINE ) 
		    (void)rt_log("spline: Right tree level %d\n", tree->level);

		n_shoot( rp,  invdir,  tree->right, ap);
	}
}

shot_poly( rp, tree )
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
			    (void)rt_log("spline: Hit found at level %d\n", tree->level);
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
		(void) rt_log("Bounding Box hit but no surface hit");
		pr_spl("B_Spline surface", tree->root);
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
	fastf_t uv[2];
	struct plane plane_form(), pln;
	fastf_t * u_eval, * v_eval, * tmp_hit;
	point_t norm, diff;

	float denom, t;
	int curr_sign;

	pln = plane_form( p1->ply[0], p1->ply[1], p1->ply[2] );

	denom = VDOT( pln.nrm, rp->r_dir);

	if (APX_EQ( denom, 0.0))
		return (struct local_hit *) 0;

	t = - (VDOT( pln.nrm, rp->r_pt) + pln.offset)/ denom;

	if ( t < 0.005  )
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
		
	interp_uv( p1, itr_point, uv );

	h0->hit_dist = t;
	VMOVE(h0->hit_point, itr_point);
	h0->hit_vpriv[0] = uv[0];
	h0->hit_vpriv[1] = uv[1];

	tmp_hit = (fastf_t *) srf_eval( curr_tree->root, uv[0], uv[1]);

	VSUB2( diff, tmp_hit, h0->hit_point);
	
	u_eval = (fastf_t *) srf_eval( curr_tree->u_diff, uv[0], uv[1]);

	v_eval = (fastf_t *) srf_eval( curr_tree->v_diff, uv[0], uv[1]);

	VCROSS( norm, u_eval, v_eval);
	VUNITIZE( norm);
	VMOVE( h0->hit_normal, norm);

	if( rt_g.debug & DEBUG_SPLINE)
	{ 
		VPRINT("hit point", h0->hit_point);
		VPRINT("Normal", h0->hit_normal);
	} 
	
	rt_free( u_eval, "ray_poly: u_eval" );
	rt_free( v_eval, "ray_poly: v_eval" );

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
