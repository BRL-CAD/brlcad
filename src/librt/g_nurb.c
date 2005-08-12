/*                        G _ N U R B . C
 * BRL-CAD
 *
 * Copyright (C) 1991-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** \addtogroup g */

/*@{*/
/** @file g_nurb.c
 *	Intersect a ray with a Non Uniform Rational B-Spline.
 *
 *  Authors -
 *	Paul R. Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
/*@}*/

#ifndef lint
static const char RCSnurb[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "nurb.h"		/* before nmg.h */
#include "nmg.h"
#include "rtgeom.h"
#include "./debug.h"

#ifndef M_SQRT1_2
#	define M_SQRT1_2       0.70710678118654752440
#endif /* M_SQRT1_2 */

struct nurb_specific {
	struct nurb_specific *  next;	/* next surface in the the solid */
	struct face_g_snurb *	srf;	/* Original surface description */
	struct bu_list		bez_hd;	/* List of Bezier face_g_snurbs */
};

struct nurb_hit {
	struct nurb_hit * next;
	struct nurb_hit * prev;
	fastf_t		hit_dist;	/* Distance from the r_pt to surface */
	point_t		hit_point;	/* intersection point */
	vect_t		hit_normal;	/* Surface normal */
	fastf_t		hit_uv[2];	/* Surface parametric u,v */
	char *		hit_private;	/* Store current nurb root */
};

#define NULL_HIT  (struct nurb_hit *)0

RT_EXTERN(int rt_nurb_grans, (struct face_g_snurb * srf));
RT_EXTERN(struct nurb_hit *rt_conv_uv, (struct nurb_specific *n,
	struct xray *r, struct rt_nurb_uv_hit *h));
RT_EXTERN(struct nurb_hit *rt_return_nurb_hit, (struct nurb_hit * head));
RT_EXTERN(void		rt_nurb_add_hit, (struct nurb_hit *head,
			struct nurb_hit * hit, const struct bn_tol *tol));


/**
 *  			R T _ N U R B _ P R E P
 *  
 *  Given a pointer of a GED database record, and a transformation matrix,
 *  determine if this is a valid NURB, and if so, prepare the surface
 *  so the intersections will work.
 */

int
rt_nurb_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_nurb_internal	*sip;
	struct nurb_specific 	*nurbs;
	int 			i;

	nurbs = (struct nurb_specific *) 0;

	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	for( i = 0; i < sip->nsrf; i++)
	{
		struct face_g_snurb * s;
		struct nurb_specific * n;

		BU_GETSTRUCT( n, nurb_specific);

		/* Store off the original face_g_snurb */
		s = rt_nurb_scopy (sip->srfs[i], (struct resource *)NULL);
		NMG_CK_SNURB(s);
		rt_nurb_s_bound(s, s->min_pt, s->max_pt);

		n->srf = s;
		BU_LIST_INIT( &n->bez_hd );

		/* Grind up the original surf into a list of Bezier face_g_snurbs */
		(void)rt_nurb_bezier( &n->bez_hd, sip->srfs[i], (struct resource *)NULL );
		
		/* Compute bounds of each Bezier face_g_snurb */
		for( BU_LIST_FOR( s, face_g_snurb, &n->bez_hd ) )  {
			NMG_CK_SNURB(s);
			rt_nurb_s_bound( s, s->min_pt, s->max_pt );
			VMINMAX( stp->st_min, stp->st_max, s->min_pt);
			VMINMAX( stp->st_min, stp->st_max, s->max_pt);
		}

		n->next = nurbs;
		nurbs = n;
	}

	stp->st_specific = (genptr_t)nurbs;

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

	return 0;
}

/**
 *			R T _ N U R B _ P R I N T
 */
void
rt_nurb_print(register const struct soltab *stp)
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;

	if( nurb == (struct nurb_specific *)0)
	{
		bu_log("rt_nurb_print: no surfaces\n");
		return;
	}

	for( ; nurb != (struct nurb_specific *)0; nurb = nurb->next)
	{
		/* XXX There is a linked list of Bezier surfaces to print here too */
		rt_nurb_s_print("NURB", nurb->srf);
	}
}

/**
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
rt_nurb_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct nurb_specific * nurb =
		(struct nurb_specific *)stp->st_specific;
	register struct seg *segp;
	const struct bn_tol	*tol = &ap->a_rt_i->rti_tol;
	point_t p1, p2, p3, p4;
	vect_t dir1, dir2;
	plane_t	plane1, plane2;
	struct nurb_hit * hit;
	struct nurb_hit hit_list;
	vect_t invdir;
	int hit_num;

	invdir[0] = invdir[1] = invdir[2] = INFINITY;
	if(!NEAR_ZERO(rp->r_dir[0], SQRT_SMALL_FASTF)) 
		invdir[0] = 1.0 / rp->r_dir[0];
	if(!NEAR_ZERO(rp->r_dir[1], SQRT_SMALL_FASTF)) 
		invdir[1] = 1.0 / rp->r_dir[1];
	if(!NEAR_ZERO(rp->r_dir[2], SQRT_SMALL_FASTF)) 
		invdir[2] = 1.0 / rp->r_dir[2];

	/* Create two orthogonal Planes their intersection contains the ray
	 * so we can project the surface into a 2 dimensional problem
	 */

	bn_vec_ortho(dir1, rp->r_dir);
	VCROSS( dir2, rp->r_dir, dir1);

	VMOVE(p1, rp->r_pt);
	VADD2(p2, rp->r_pt, rp->r_dir);
	VADD2(p3, rp->r_pt, dir1);
	VADD2(p4, rp->r_pt, dir2);

	/* Note: the equation of the plane in BRLCAD is
	 * Ax + By + Cz = D represented by [A B C D]
	 */
	bn_mk_plane_3pts( plane1, p1, p3, p2, tol );
	bn_mk_plane_3pts( plane2, p1, p2, p4, tol );
	
	/* make sure that the hit_list is zero */

	hit_list.next = (struct nurb_hit *)0;
	hit_list.prev = (struct nurb_hit *)0;
	hit_list.hit_dist = 0;
	VSET(hit_list.hit_point, 0.0, 0.0, 0.0);
	VSET(hit_list.hit_normal, 0.0, 0.0, 0.0);
	hit_list.hit_uv[0] = 	hit_list.hit_uv[1] = 0.0;
	hit_list.hit_private = (char *)0;

	while( nurb != (struct nurb_specific *) 0 )
	{
		struct face_g_snurb * s;
		struct rt_nurb_uv_hit *hp;

		for( BU_LIST_FOR( s, face_g_snurb, &nurb->bez_hd ) )  {
			if( !rt_in_rpp( rp, invdir, s->min_pt, s->max_pt))
				continue;

#define UV_TOL	1.0e-6	/* Paper says 1.0e-4 is reasonable for 1k images, not close up */
			hp = rt_nurb_intersect(
				s, plane1, plane2, UV_TOL, (struct resource *)ap->a_resource );
			while( hp != (struct rt_nurb_uv_hit *)0)
			{
				struct rt_nurb_uv_hit * o;

				if( RT_G_DEBUG & DEBUG_SPLINE )
					bu_log("hit at %d %d sub = %d u = %f v = %f\n",
						ap->a_x, ap->a_y, hp->sub, hp->u, hp->v);

				hit = (struct nurb_hit *) 
					rt_conv_uv(nurb, rp, hp);

				o = hp;
				hp = hp->next;
				bu_free( (char *)o,
					"rt_nurb_shot:rt_nurb_uv_hit structure");

				rt_nurb_add_hit( &hit_list, hit, tol );
			}
		}
		nurb = nurb->next;
		/* Insert Trimming routines here */
	}

	/* Convert hits to segments for rt */

	hit_num = 0;

	while( hit_list.next != NULL_HIT )
	{
		struct nurb_hit * h1, * h2;

		RT_GET_SEG( segp, ap->a_resource);

		h1 = (struct nurb_hit *) rt_return_nurb_hit( &hit_list );
		h2 = (struct nurb_hit *) rt_return_nurb_hit( &hit_list );

		segp->seg_stp = stp;
		segp->seg_in.hit_dist = h1->hit_dist;
		VMOVE(segp->seg_in.hit_point, h1->hit_point);
		segp->seg_in.hit_vpriv[0] = h1->hit_uv[0];
		segp->seg_in.hit_vpriv[1] = h1->hit_uv[1];
		segp->seg_in.hit_private = h1->hit_private;
		segp->seg_in.hit_vpriv[2] = 0;
		hit_num++;


		if( h2 != NULL_HIT)
		{
			segp->seg_out.hit_dist = h2->hit_dist;
			VMOVE(segp->seg_out.hit_point, h2->hit_point);
			segp->seg_out.hit_vpriv[0] = h2->hit_uv[0];
			segp->seg_out.hit_vpriv[1] = h2->hit_uv[1];
			segp->seg_out.hit_private = h2->hit_private;
			bu_free( (char *)h2,"rt_nurb_shot: nurb hit");
			hit_num++;
		} 
		else
		{
			segp->seg_out.hit_dist = h1->hit_dist + .01;
			VJOIN1(segp->seg_out.hit_point,
				rp->r_pt, segp->seg_out.hit_dist, rp->r_dir);
			segp->seg_out.hit_vpriv[0] = h1->hit_uv[0];
			segp->seg_out.hit_vpriv[1] = h1->hit_uv[1];
			segp->seg_out.hit_vpriv[2] = 1;
			segp->seg_out.hit_private = h1->hit_private;
		}

		bu_free( (char *)h1, "rt_nurb_shot:nurb hit");
		
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}

	return(hit_num);	/* not hit */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/**
 *			R T _ N U R B _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nurb_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */
                  	    
{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ N U R B _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nurb_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */

	struct face_g_snurb * n  = (struct face_g_snurb *) hitp->hit_private;
	fastf_t u = hitp->hit_vpriv[0];
	fastf_t v = hitp->hit_vpriv[1];
	fastf_t norm[4];

	rt_nurb_s_norm( n, u, v, norm);
	
	VMOVE( hitp->hit_normal, norm);
	
	if ( hitp->hit_vpriv[2] == 1)
	{
		VREVERSE( hitp->hit_normal, norm );
	}
	
	return;
}

/**
 *			R T _ N U R B _ C U R V E
 *
 *  Return the curvature of the nurb.
 */
void
rt_nurb_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */
	struct face_g_snurb * srf = (struct face_g_snurb *) hitp->hit_private;
        fastf_t         u, v;

	if( srf->order[0] <= 2 && srf->order[1] <= 2)
	{
	 	cvp->crv_c1 = cvp->crv_c2 = 0;

		/* any tangent direction */
	 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
		return;
	}
	
	u = hitp->hit_vpriv[0];
	v = hitp->hit_vpriv[1];
	
	rt_nurb_curvature( cvp, srf, u, v );
}

/**
 *  			R T _ N U R B _ U V
 *  
 *  For a hit on the surface of an nurb, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_nurb_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */
	uvp->uv_u = hitp->hit_vpriv[0];
	uvp->uv_v = hitp->hit_vpriv[1];
	return;
}

/**
 *		R T _ N U R B _ F R E E
 */
void
rt_nurb_free(register struct soltab *stp)
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;
	register struct nurb_specific *next;

	if( nurb == (struct nurb_specific *)0)
		rt_bomb("rt_nurb_free: no surfaces\n");

	for( ; nurb != (struct nurb_specific *)0; nurb = next)  {
		register struct face_g_snurb	*s;

		next = nurb->next;

		/* There is a linked list of surfaces to free for each nurb */
		while( BU_LIST_WHILE( s, face_g_snurb, &nurb->bez_hd ) )  {
			NMG_CK_SNURB( s );
			BU_LIST_DEQUEUE( &(s->l) );
			rt_nurb_free_snurb( s, (struct resource *)NULL );
		}
		rt_nurb_free_snurb( nurb->srf, (struct resource *)NULL );	/* original surf */
		bu_free( (char *)nurb, "nurb_specific" );
	}
}

/**
 *			R T _ N U R B _ C L A S S
 */
int
rt_nurb_class(void)
{
	return(0);
}

/**
 *			R T _ N U R B _ P L O T
 */
int
rt_nurb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_nurb_internal *sip;
	register int		i;
	register int		j;
	register fastf_t	* vp;
	int			s;

	
	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);
	
	for( s=0; s < sip->nsrf; s++)
	{
		struct face_g_snurb 	* n, *r, *c;
		int 		coords;
		fastf_t 	bound;
		point_t		tmp_pt;
		fastf_t 	rel;
		struct knot_vector 	tkv1,
					tkv2;
		fastf_t		tess;
		int		num_knots;
		fastf_t		rt_nurb_par_edge(const struct face_g_snurb *srf, fastf_t epsilon);

		n = (struct face_g_snurb *) sip->srfs[s];

                VSUB2(tmp_pt, n->min_pt, n->max_pt);
                bound =         MAGNITUDE( tmp_pt)/ 2.0;
                /*
                 *  Establish tolerances
                 */
                if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )  {
                        rel = 0.0;              /* none */
                } else {
                        /* Convert rel to absolute by scaling by diameter */
                        rel = ttol->rel * 2 * bound;
                }
                if( ttol->abs <= 0.0 )  {
                        if( rel <= 0.0 )  {
                                /* No tolerance given, use a default */
                                rel = 2 * 0.10 * bound;        /* 10% */
                        } else {
                                /* Use absolute-ized relative tolerance */
                        }
                } else {
                        /* Absolute tolerance was given, pick smaller */
                        if( ttol->rel <= 0.0 || rel > ttol->abs )
                                rel = ttol->abs;
                }

		if( n->order[0] < 3 || n->order[1] < 3 ) {
			/* cannot use rt_nurb_par_edge() in this case */
			tess = 0.25; /* hack for now */
		} else
	                tess = (fastf_t) rt_nurb_par_edge(n, rel);

                num_knots = (int)floor(1.0/((M_SQRT1_2 / 2.0) * tess));

                if( num_knots < 2) num_knots = 2;

                rt_nurb_kvknot( &tkv1, n->order[0],
                        n->u.knots[0],
                        n->u.knots[n->u.k_size-1], num_knots, (struct resource *)NULL);

                rt_nurb_kvknot( &tkv2, n->order[1],
                        n->v.knots[0],
                        n->v.knots[n->v.k_size-1], num_knots, (struct resource *)NULL);


		r = (struct face_g_snurb *) rt_nurb_s_refine( n, RT_NURB_SPLIT_COL, &tkv2, (struct resource *)NULL);
		c = (struct face_g_snurb *) rt_nurb_s_refine( r, RT_NURB_SPLIT_ROW, &tkv1, (struct resource *)NULL);

		coords = RT_NURB_EXTRACT_COORDS(n->pt_type);
	
		if( RT_NURB_IS_PT_RATIONAL(n->pt_type))
		{
			vp = c->ctl_points;
			for(i= 0; 
				i < c->s_size[0] * c->s_size[1]; 
				i++)
			{
				vp[0] /= vp[3];
				vp[1] /= vp[3];
				vp[2] /= vp[3];
				vp[3] /= vp[3];
				vp += coords;
			}
		}

		
		vp = c->ctl_points;
		for( i = 0; i < c->s_size[0]; i++)
		{
			RT_ADD_VLIST( vhead, vp, BN_VLIST_LINE_MOVE );
			vp += coords;
			for( j = 1; j < c->s_size[1]; j++)
			{
				RT_ADD_VLIST( vhead, vp, BN_VLIST_LINE_DRAW );
				vp += coords;
			}
		}
		
		for( j = 0; j < c->s_size[1]; j++)
		{
			int stride;
			
			stride = c->s_size[1] * coords;
			vp = &c->ctl_points[j * coords];
			RT_ADD_VLIST( vhead, vp, BN_VLIST_LINE_MOVE );
			for( i = 0; i < c->s_size[0]; i++)
			{
				RT_ADD_VLIST( vhead, vp, BN_VLIST_LINE_DRAW );
				vp += stride;
			}
		}
		rt_nurb_free_snurb(c, (struct resource *)NULL);
		rt_nurb_free_snurb(r, (struct resource *)NULL);

		bu_free( (char *) tkv1.knots, "rt_nurb_plot:tkv1>knots");
		bu_free( (char *) tkv2.knots, "rt_nurb_plot:tkv2.knots");
	}
	return(0);
}

/**
 *			R T _ N U R B _ T E S S
 */
int
rt_nurb_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	return(-1);
}

/**
 *			R T _ N U R B _ I M P O R T
 */
int
rt_nurb_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{

	struct rt_nurb_internal * sip;
	union record 		*rp;
	register int		i;
	int			s;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_BSOLID ) 
	{
		bu_log("rt_nurb_import: defective header record");
		return (-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_BSPLINE;
	ip->idb_meth = &rt_functab[ID_BSPLINE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_nurb_internal), "rt_nurb_internal");
	sip = (struct rt_nurb_internal *)ip->idb_ptr;
	sip->magic = RT_NURB_INTERNAL_MAGIC;


	sip->nsrf = rp->B.B_nsurf;
	sip->srfs = (struct face_g_snurb **) bu_malloc(
		sip->nsrf * sizeof( struct face_g_snurb), "nurb srfs[]");
	rp++;

	for( s = 0; s < sip->nsrf; s++)
	{
		register fastf_t 	* m;
		int			coords;
		register dbfloat_t	*vp;
		int			pt_type;
		
		if( rp->d.d_id != ID_BSURF )  {
			bu_log("rt_nurb_import() surf %d bad ID\n", s);
			return -1;
		}

		if( rp->d.d_geom_type == 3)
			pt_type = RT_NURB_MAKE_PT_TYPE(3,RT_NURB_PT_XYZ,RT_NURB_PT_NONRAT);
		else
			pt_type = RT_NURB_MAKE_PT_TYPE(4,RT_NURB_PT_XYZ,RT_NURB_PT_RATIONAL);

		sip->srfs[s] = (struct face_g_snurb *) rt_nurb_new_snurb(
			rp->d.d_order[0],rp->d.d_order[1],
			rp->d.d_kv_size[0],rp->d.d_kv_size[1],
			rp->d.d_ctl_size[0],rp->d.d_ctl_size[1],
			pt_type, (struct resource *)NULL);

		vp = (dbfloat_t *) &rp[1];
		
		for( i = 0; i < rp->d.d_kv_size[0]; i++)
			sip->srfs[s]->u.knots[i] = (fastf_t) *vp++;

		for( i = 0; i < rp->d.d_kv_size[1]; i++)
			sip->srfs[s]->v.knots[i] = (fastf_t) *vp++;

		rt_nurb_kvnorm( &sip->srfs[s]->u);
		rt_nurb_kvnorm( &sip->srfs[s]->v);

		vp = (dbfloat_t *) &rp[rp->d.d_nknots+1];
		m = sip->srfs[s]->ctl_points;
		coords = rp->d.d_geom_type;
		i = (rp->d.d_ctl_size[0] *rp->d.d_ctl_size[1]);
		if( coords == 3)
		{
			for( ; i> 0; i--)
			{
				MAT4X3PNT( m, mat, vp);
				m += 3;
				vp += 3;
			}
		} else if( coords == 4)
		{
			for( ; i> 0; i--)
			{
				MAT4X4PNT( m, mat, vp);
				m += 4;
				vp += 4;
			}
		} else {
			bu_log("rt_nurb_internal: %d invalid elements per vect\n", rp->d.d_geom_type);
			return (-1);
		}
		
		/* bound the surface for tolerancing and other bounding box tests */
                rt_nurb_s_bound( sip->srfs[s], sip->srfs[s]->min_pt,
                        sip->srfs[s]->max_pt);

		rp += 1 + rp->d.d_nknots + rp->d.d_nctls;
	}
	return (0);
}

struct nurb_hit *
rt_conv_uv(struct nurb_specific *n, struct xray *r, struct rt_nurb_uv_hit *h)
{
	struct nurb_hit * hit;
	fastf_t pt[4];
	point_t vecsub;

	hit = (struct nurb_hit *) bu_malloc( sizeof (struct nurb_hit),
		"rt_conv_uv:nurb hit");
	
	hit->prev = hit->next = (struct nurb_hit *)0;
	VSET(hit->hit_normal, 0.0, 0.0, 0.0);
	
	rt_nurb_s_eval(n->srf, h->u, h->v, pt);

	if( RT_NURB_IS_PT_RATIONAL(n->srf->pt_type) )
	{
		hit->hit_point[0] = pt[0] / pt[3];
		hit->hit_point[1] = pt[1] / pt[3];
		hit->hit_point[2] = pt[2] / pt[3];
	} else
	{
		hit->hit_point[0] = pt[0];
		hit->hit_point[1] = pt[1];
		hit->hit_point[2] = pt[2];
	}

	VSUB2( vecsub, hit->hit_point, r->r_pt);
	hit->hit_dist = VDOT( vecsub, r->r_dir);
	hit->hit_uv[0] = h->u;
	hit->hit_uv[1] = h->v;
	hit->hit_private = (char *) n->srf;
	
	return (struct nurb_hit *) hit;
}

void
rt_nurb_add_hit(struct nurb_hit *head, struct nurb_hit *hit, const struct bn_tol *tol)
{
	register struct nurb_hit * h_ptr;

	BN_CK_TOL(tol);
#if 0
	/* Shouldn't be discarded, because shootray moves start pt around */
	if( hit->hit_dist < .001)
	{

		bu_free( (char *) hit, "internal_add_hit: hit");
		return;
	}
#endif
	
	/* If this is the only one, nothing to check against */
	if( head->next == (struct nurb_hit *) 0)
	{
		head->next = hit;
		hit->prev = head;
		return;
	}

	/* Check for duplicates */
	for( h_ptr = head->next; h_ptr != (struct nurb_hit *)0; h_ptr = h_ptr->next)
	{
		register fastf_t	f;

		/* This test a distance in model units (mm) */
		f = hit->hit_dist - h_ptr->hit_dist;
		if( NEAR_ZERO( f, tol->dist ) )  goto duplicate;

		/* These tests are in parameter space, 0..1 */
		f = hit->hit_uv[0] - h_ptr->hit_uv[0];
		if( NEAR_ZERO( f, 0.0001 ) )  goto duplicate;
		f = hit->hit_uv[1] - h_ptr->hit_uv[1];
		if( NEAR_ZERO( f, 0.0001 ) )  goto duplicate;
	}

	hit->prev = head;
	hit->next = head->next;
	hit->next->prev = hit;
	head->next = hit;
	return;
duplicate:
	bu_free( (char *) hit, "add hit: hit");
	return;
}

struct nurb_hit *
rt_return_nurb_hit(struct nurb_hit *head)
{

	register struct nurb_hit * h, * ret;
	fastf_t dist;

	if( head->next == NULL_HIT)
		return NULL_HIT;

	dist = INFINITY;
	ret = NULL_HIT;

	for( h = head->next; h != NULL_HIT; h = h->next)
	{
		if( h->hit_dist < dist )
		{
			ret = h;
			dist = ret->hit_dist;
		}
	}
	
	if( ret != NULL_HIT)
	{
		if( ret->prev != NULL_HIT) ret->prev->next = ret->next;
		if( ret->next != NULL_HIT) ret->next->prev = ret->prev;
		ret->next = ret->prev = NULL_HIT;
	}
	return (struct nurb_hit *) ret;
}

/**
 *			R T _ N U R B _ E X P O R T
 */
int
rt_nurb_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	register int		rec_ptr;
	struct rt_nurb_internal	* sip;
	union record		* rec;
	int			s;
	int			grans;
	int			total_grans;
	dbfloat_t		* vp;
	int			n;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_BSPLINE) return(-1);
	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	/* Figure out how many recs to buffer by
	 * walking through the surfaces and
	 * calculating the number of granuels
	 * needed for storage and add it to the total
	 */
	total_grans = 1;	/* First gran for BSOLID record */
	for( s = 0; s < sip->nsrf; s++)
	{
		total_grans += rt_nurb_grans(sip->srfs[s]);
	}

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = total_grans * sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc(1,ep->ext_nbytes,"nurb external");
	rec = (union record *)ep->ext_buf;

	rec[0].B.B_id = ID_BSOLID;
	rec[0].B.B_nsurf = sip->nsrf;
	
	rec_ptr = 1;

	for( s = 0; s < sip->nsrf; s++)
	{
		register struct face_g_snurb	*srf = sip->srfs[s];
		NMG_CK_SNURB(srf);

		grans = rt_nurb_grans( srf);

		rec[rec_ptr].d.d_id = ID_BSURF;
		rec[rec_ptr].d.d_nknots = (((srf->u.k_size + srf->v.k_size) 
			* sizeof(dbfloat_t)) + sizeof(union record)-1)/ sizeof(union record);
		rec[rec_ptr].d.d_nctls = ((
			RT_NURB_EXTRACT_COORDS(srf->pt_type)
			* (srf->s_size[0] * srf->s_size[1])
			* sizeof(dbfloat_t)) + sizeof(union record)-1 )
			/ sizeof(union record);

		rec[rec_ptr].d.d_order[0] = srf->order[0];
		rec[rec_ptr].d.d_order[1] = srf->order[1];
		rec[rec_ptr].d.d_kv_size[0] = srf->u.k_size;
		rec[rec_ptr].d.d_kv_size[1] = srf->v.k_size;
		rec[rec_ptr].d.d_ctl_size[0] = 	srf->s_size[0];
		rec[rec_ptr].d.d_ctl_size[1] = 	srf->s_size[1];
		rec[rec_ptr].d.d_geom_type = 
			RT_NURB_EXTRACT_COORDS(srf->pt_type);

		vp = (dbfloat_t *) &rec[rec_ptr +1];
		for(n = 0; n < rec[rec_ptr].d.d_kv_size[0]; n++)
		{
			*vp++ = srf->u.knots[n];
		}

		for(n = 0; n < rec[rec_ptr].d.d_kv_size[1]; n++)
		{
			*vp++ = srf->v.knots[n];
		}
		
		vp = (dbfloat_t *) &rec[rec_ptr + 1 +
			rec[rec_ptr].d.d_nknots];

		for( n = 0; n < (srf->s_size[0] * srf->s_size[1]) * 
			rec[rec_ptr].d.d_geom_type; n++)
			*vp++ = srf->ctl_points[n];

		rec_ptr += grans;
		total_grans -= grans;
	}
	return(0);
}

int
rt_nurb_bytes(struct face_g_snurb *srf)
{
	int	total_bytes=0;

	total_bytes = 3 * SIZEOF_NETWORK_LONG		/* num_coords and order */
		+ 2 * SIZEOF_NETWORK_LONG		/* k_size in both knot vectors */
		+ srf->u.k_size * SIZEOF_NETWORK_DOUBLE	/* u knot vector knots */
		+ srf->v.k_size * SIZEOF_NETWORK_DOUBLE	/* v knot vector knots */
		+ 2 * SIZEOF_NETWORK_LONG		/* mesh size */
		+ RT_NURB_EXTRACT_COORDS(srf->pt_type) *
			(srf->s_size[0] * srf->s_size[1]) * SIZEOF_NETWORK_DOUBLE;	/* control point mesh */

	return total_bytes;
}

/**
 *			R T _ N U R B _ E X P O R T 5
 */
int
rt_nurb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_nurb_internal	* sip;
	int			s;
	unsigned char		*cp;
	int			coords;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_BSPLINE) return(-1);
	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	/* Figure out how many bytes are needed by
	 * walking through the surfaces and
	 * calculating the number of bytes
	 * needed for storage and add it to the total
	 */
	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_LONG;	/* number of surfaces */
	for( s = 0; s < sip->nsrf; s++)
	{
		ep->ext_nbytes += rt_nurb_bytes(sip->srfs[s]);
	}

	ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes,"nurb external");
	cp = (unsigned char *)ep->ext_buf;

	(void)bu_plong( cp, sip->nsrf );
	cp += SIZEOF_NETWORK_LONG;
	
	for( s = 0; s < sip->nsrf; s++)
	{
		register struct face_g_snurb	*srf = sip->srfs[s];

		NMG_CK_SNURB(srf);

		coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
		(void)bu_plong( cp, coords );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->order[0] );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->order[1] );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->u.k_size );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->v.k_size );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->s_size[0] );
		cp += SIZEOF_NETWORK_LONG;
		(void)bu_plong( cp, srf->s_size[1] );
		cp += SIZEOF_NETWORK_LONG;
		htond( cp, (unsigned char *)srf->u.knots, srf->u.k_size );
		cp += srf->u.k_size * SIZEOF_NETWORK_DOUBLE;
		htond( cp, (unsigned char *)srf->v.knots, srf->v.k_size );
		cp += srf->v.k_size * SIZEOF_NETWORK_DOUBLE;

		htond( cp, (unsigned char *)srf->ctl_points,
			coords * srf->s_size[0] * srf->s_size[1] );
		cp += coords * srf->s_size[0] * srf->s_size[1] * SIZEOF_NETWORK_DOUBLE;
	}

	return(0);
}


/**
 *			R T _ N U R B _ I M P O R T 5
 */
int
rt_nurb_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{

	struct rt_nurb_internal * sip;
	register int		i;
	int			s;
	unsigned char		*cp;
	fastf_t			tmp_vec[4];

	BU_CK_EXTERNAL( ep );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_BSPLINE;
	ip->idb_meth = &rt_functab[ID_BSPLINE];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_nurb_internal), "rt_nurb_internal");
	sip = (struct rt_nurb_internal *)ip->idb_ptr;
	sip->magic = RT_NURB_INTERNAL_MAGIC;

	cp = ep->ext_buf;
	
	sip->nsrf = bu_glong( cp );
	cp += SIZEOF_NETWORK_LONG;
	sip->srfs = (struct face_g_snurb **) bu_calloc(
		sip->nsrf, sizeof( struct face_g_snurb *), "nurb srfs[]");

	for( s = 0; s < sip->nsrf; s++)
	{
		register struct face_g_snurb	*srf;
		int			coords;
		int			pt_type;
		int			order[2], u_size, v_size;
		int			s_size[2];

		pt_type = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		order[0] = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		order[1] = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		u_size = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		v_size = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		s_size[0] = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		s_size[1] = bu_glong( cp );
		cp += SIZEOF_NETWORK_LONG;
		if( pt_type == 3)
			pt_type = RT_NURB_MAKE_PT_TYPE(3,RT_NURB_PT_XYZ,RT_NURB_PT_NONRAT);
		else
			pt_type = RT_NURB_MAKE_PT_TYPE(4,RT_NURB_PT_XYZ,RT_NURB_PT_RATIONAL);

		sip->srfs[s] = (struct face_g_snurb *) rt_nurb_new_snurb(
			order[0],order[1],
			u_size,v_size,
			s_size[0],s_size[1],
			pt_type, (struct resource *)NULL);

		srf = sip->srfs[s];
		coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

		ntohd( (unsigned char *)srf->u.knots, cp, srf->u.k_size );
		cp += srf->u.k_size * SIZEOF_NETWORK_DOUBLE;
		ntohd( (unsigned char *)srf->v.knots, cp, srf->v.k_size );
		cp += srf->v.k_size * SIZEOF_NETWORK_DOUBLE;

		rt_nurb_kvnorm( &srf->u);
		rt_nurb_kvnorm( &srf->v);

		ntohd( (unsigned char *)srf->ctl_points, cp,
			coords * srf->s_size[0] * srf->s_size[1] );

		cp += coords * srf->s_size[0] * srf->s_size[1] * SIZEOF_NETWORK_DOUBLE;

		for( i=0 ; i<srf->s_size[0] * srf->s_size[1] ; i++ )
		{
			if( coords == 3 )
			{
				VMOVE( tmp_vec, &srf->ctl_points[i*coords] );
				MAT4X3PNT( &srf->ctl_points[i*coords], mat, tmp_vec );
			}
			else if( coords == 4 )
			{
				HMOVE( tmp_vec, &srf->ctl_points[i*coords] );
				MAT4X4PNT( &srf->ctl_points[i*coords], mat, tmp_vec );
			}
			else
			{
				bu_log("rt_nurb_internal: %d invalid elements per vect\n", coords);
				return (-1);
			}
		}

		/* bound the surface for tolerancing and other bounding box tests */
                rt_nurb_s_bound( sip->srfs[s], sip->srfs[s]->min_pt,
                        sip->srfs[s]->max_pt);
	}
	return (0);
}


int 
rt_nurb_grans(struct face_g_snurb *srf)
{
	int total_knots, total_points;
	int	k_gran;
	int	p_gran;

	total_knots = srf->u.k_size + srf->v.k_size;
	k_gran = ((total_knots * sizeof(dbfloat_t)) + sizeof(union record)-1)
		/ sizeof(union record);

	total_points = RT_NURB_EXTRACT_COORDS(srf->pt_type) *
		(srf->s_size[0] * srf->s_size[1]);
	p_gran = ((total_points * sizeof(dbfloat_t)) + sizeof(union record)-1)
		/ sizeof(union record);

	return 1 + k_gran + p_gran;
}

/**
 *			R T _ N U R B _ I F R E E
 */
void
rt_nurb_ifree(struct rt_db_internal *ip)
{
	register struct rt_nurb_internal * sip;
	register int			 i;

	RT_CK_DB_INTERNAL(ip);
	sip = ( struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	/* Free up storage for the nurb surfaces */
	for( i = 0; i < sip->nsrf; i++)
	{
		rt_nurb_free_snurb( sip->srfs[i], (struct resource *)NULL );
	}
	bu_free( (char *)sip->srfs, "nurb surfs[]" );
	sip->magic = 0;
	sip->nsrf = 0;
	bu_free( (char *)sip, "sip ifree");
	ip->idb_ptr = GENPTR_NULL;
}

/**
 *			R T _ N U R B _ D E S C R I B E
 */
int
rt_nurb_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register int		j;
	register struct rt_nurb_internal * sip =
		(struct rt_nurb_internal *) ip->idb_ptr;
	int			i;
	int			surf;

	RT_NURB_CK_MAGIC(sip);
	bu_vls_strcat( str, "Non Uniform Rational B-Spline solid (NURB)\n");
	
	bu_vls_printf( str, "\t%d surfaces\n", sip->nsrf);
	if( verbose < 2 )  return 0;

	for( surf = 0; surf < sip->nsrf; surf++)
	{
		register struct face_g_snurb 	* np;
		register fastf_t 	* mp;
		int			ncoord;

		np = sip->srfs[surf];
		NMG_CK_SNURB(np);
		mp = np->ctl_points;
		ncoord = RT_NURB_EXTRACT_COORDS(np->pt_type);

		bu_vls_printf( str,
			"\tSurface %d: order %d x %d, mesh %d x %d\n",
			surf, np->order[0], np->order[1],
			np->s_size[0], np->s_size[1]);

		bu_vls_printf( str, "\t\tVert (%g, %g, %g)\n",
			mp[X] * mm2local, 
			mp[Y] * mm2local, 
			mp[Z] * mm2local);

		if( verbose < 3 ) continue;

		/* Print out the knot vectors */
		bu_vls_printf( str, "\tU: ");
		for( i=0; i < np->u.k_size; i++ )
			bu_vls_printf( str, "%g, ", np->u.knots[i] );
		bu_vls_printf( str, "\n\tV: ");
		for( i=0; i < np->v.k_size; i++ )
			bu_vls_printf( str, "%g, ", np->v.knots[i] );
		bu_vls_printf( str, "\n");
		
		/* print out all the points */
		for(i=0; i < np->s_size[0]; i++)
		{
			bu_vls_printf( str, "\tRow %d:\n", i);
			for( j = 0; j < np->s_size[1]; j++)
			{
				if( ncoord == 3 ) {
					bu_vls_printf( str, "\t\t(%g, %g, %g)\n",
						mp[X] * mm2local, 
						mp[Y] * mm2local, 
						mp[Z] * mm2local);
				} else {
					bu_vls_printf( str, "\t\t(%g, %g, %g, %g)\n",
						mp[X] * mm2local, 
						mp[Y] * mm2local, 
						mp[Z] * mm2local,
						mp[W] );
				}
				mp += ncoord;
			}
		}
	}
	return 0;
}


int
rt_nurb_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	struct rt_nurb_internal *nurb=(struct rt_nurb_internal *)intern->idb_ptr;
	Tcl_DString		ds;
	struct bu_vls		vls;
	struct face_g_snurb	*srf;
	int			i, j, k;
	int			coords;
	int			status;

	RT_NURB_CK_MAGIC( nurb );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL ) {
		bu_vls_strcpy( &vls, "bspline" );
		bu_vls_printf( &vls, " N %d S {", nurb->nsrf );
		for( i=0 ; i<nurb->nsrf ; i++ ) {
			srf = nurb->srfs[i];
			bu_vls_printf( &vls, " { O {%d %d} s {%d %d} T %d u {",
				       srf->order[0], srf->order[1],
				       srf->s_size[0], srf->s_size[1],
				       srf->pt_type, srf->u.k_size );
			for( j=0 ; j<srf->u.k_size ; j++ ) {
				bu_vls_printf( &vls, " %.25G", srf->u.knots[j] );
			}
			bu_vls_printf( &vls, "} v {", srf->v.k_size );
			for( j=0 ; j<srf->v.k_size ; j++ ) {
				bu_vls_printf( &vls, " %.25G", srf->v.knots[j] );
			}
			bu_vls_strcat( &vls, "} P {" );
			
			coords = RT_NURB_EXTRACT_COORDS( srf->pt_type );
			for( j=0 ; j<srf->s_size[0]*srf->s_size[1] ; j++ ) {
				for( k=0 ; k<coords ; k++ ) {
					bu_vls_printf( &vls, " %.25G",
						       srf->ctl_points[j*coords + k] );
				}
			}
			bu_vls_strcat( &vls, " } }" );
		}
		bu_vls_printf( &vls, " }" );
		status = TCL_OK;
	} else {
		bu_vls_printf( &vls, "Nurb has no attribute '%s'", attr );
		status = TCL_ERROR;
	}

	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );

	return( status );
}

int
rt_nurb_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
	struct rt_nurb_internal *nurb;
	int srf_no;
	Tcl_Obj *obj, *list, **srf_array, **srf_param_array;
	struct face_g_snurb *srf;
	int i;
	char *key;
	int len;

	RT_CK_DB_INTERNAL( intern );
	nurb = (struct rt_nurb_internal *)intern->idb_ptr;
	RT_NURB_CK_MAGIC( nurb );

	while( argc >= 2 ) {
		obj = Tcl_NewStringObj( argv[1], -1 );
		list = Tcl_NewListObj( 0, NULL );
		if( Tcl_ListObjAppendList( interp, list, obj ) != TCL_OK ) {
			return( TCL_ERROR );
		}

		if( !strcmp( argv[0], "N" ) ) {
			if( nurb->srfs ) {
				for( i=0 ; i<nurb->nsrf ; i++ )
					rt_nurb_free_snurb( nurb->srfs[i], NULL );
				bu_free( (char *)nurb->srfs, "nurb surfaces" );
			}
			nurb->nsrf = atoi( argv[1] );
			nurb->srfs = (struct face_g_snurb **) bu_calloc(
				nurb->nsrf, sizeof( struct face_g_snurb *), "nurb srfs[]");
		} else if( !strcmp( argv[0], "S" ) ) {
			(void)Tcl_ListObjGetElements( interp, list, &len, &srf_array );
			for( srf_no=0 ; srf_no < nurb->nsrf ; srf_no++ ) {
				int n_params=0;
				int *order=NULL, *s_size=NULL, u_size=0, v_size=0, pt_type=0;
				fastf_t *u_pts=NULL, *v_pts=NULL;

				(void)Tcl_ListObjGetElements( interp, srf_array[srf_no], &n_params,
							      &srf_param_array );
				
				for( i=0 ; i<n_params ; i+= 2 ) {
					int tmp_len;

					key = Tcl_GetStringFromObj( srf_param_array[i], NULL );
					if( !strcmp( key, "O" ) ) {
						tmp_len = 0;
						if( tcl_obj_to_int_array( interp, srf_param_array[i+1],
							      &order, &tmp_len ) != 2 ) {
							Tcl_SetResult( interp,
							      "ERROR: unable to parse surface\n",
							       TCL_STATIC );
							return( TCL_ERROR );
						}
					} else if( !strcmp( key, "s" ) ) {
						tmp_len = 0;
						if( tcl_obj_to_int_array( interp, srf_param_array[i+1],
							      &s_size, &tmp_len ) != 2 ) {
							Tcl_SetResult( interp,
							      "ERROR: unable to parse surface\n",
							       TCL_STATIC );
							return( TCL_ERROR );
						}
					} else if( !strcmp( key, "T" ) ) {
						pt_type = atoi( Tcl_GetStringFromObj(
								     srf_param_array[i+1], NULL ) );
					} else if( !strcmp( key, "u" ) ) {
						tcl_obj_to_fastf_array( interp, srf_param_array[i+1], &u_pts,
									&u_size );
					} else if( !strcmp( key, "v" ) ) {
						tcl_obj_to_fastf_array( interp, srf_param_array[i+1], &v_pts,
									&v_size );
					} else if( !strcmp( key, "P" ) ) {
						int tmp2;

						if( !order || !s_size || !u_pts || !v_pts ||
						    u_size == 0 || v_size == 0 || pt_type == 0 ) {
							Tcl_SetResult( interp,
							    "ERROR: Need all other details set before ctl points\n",
							    TCL_STATIC );
							return( TCL_ERROR );
						}
						nurb->srfs[srf_no] = (struct face_g_snurb *) rt_nurb_new_snurb(
						      order[0],order[1],
						      u_size,v_size,
						      s_size[0],s_size[1],
						      pt_type, (struct resource *)NULL);
						srf = nurb->srfs[srf_no];
						bu_free( (char *)order, "order" );
						bu_free( (char *)s_size, "s_size" );
						(void)memcpy( srf->u.knots, u_pts,
							      srf->u.k_size * sizeof( fastf_t ) );
						(void)memcpy( srf->v.knots, v_pts,
							      srf->v.k_size * sizeof( fastf_t ) );
						bu_free( (char *)u_pts, "u_pts" );
						bu_free( (char *)v_pts, "v_pts" );
						tmp_len = srf->s_size[0] * srf->s_size[1] * 
							RT_NURB_EXTRACT_COORDS(srf->pt_type );
						tmp2 = tmp_len;
						if( tcl_obj_to_fastf_array( interp, srf_param_array[i+1],
							   &srf->ctl_points, &tmp_len ) != tmp2 ) {
							Tcl_SetResult( interp,
							      "ERROR: unable to parse surface\n",
							       TCL_STATIC );
							return( TCL_ERROR );
						}
					}
				}
			}
		}

		Tcl_DecrRefCount( list );

		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
