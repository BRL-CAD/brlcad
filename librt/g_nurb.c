/*
 *			G _ N U R B . C
 *
 *  Purpose -
 *	Intersect a ray with a Non Uniform Rational B-Spline
 *
 *  Authors -
	Paul R. Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1991 by the United States Army.
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
#include "nmg.h"
#include "nurb.h"
#include "./debug.h"

struct rt_nurb_internal {
	long	magic;
	int 	nsrf;	/* number of surfaces */
	struct snurb ** srfs;
};

#define RT_NURB_INTERNAL_MAGIC	0x002b2bdd
#define RT_NURB_CK_MAGIC( _p) RT_CKMAG(_p,RT_NURB_INTERNAL_MAGIC,"rt_nurb_internal");

struct nurb_b_list {
	struct nurb_b_list *	next;
	point_t 		min,	/* bounding box */
				max;
	struct snurb * 		srf;	/* bezier form of surface */
};

struct nurb_specific {
	struct nurb_specific *  next;	/* next surface in the the solid */
	point_t		        min,	/* Bounding box of surface */
			        max;
	struct snurb *		srf;	/* Surface description */
	struct nurb_b_list *	b_list; /* Converted into bezier form */
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

RT_EXTERN(int rt_nurb_grans, (struct snurb * srf));
RT_EXTERN(struct nurb_hit *rt_conv_uv, (struct nurb_specific *n,
	struct xray *r, struct rt_nurb_uv_hit *h));
RT_EXTERN(struct nurb_hit *rt_return_nurb_hit, (struct nurb_hit * head));


/*
 *  			R T _ N U R B _ P R E P
 *  
 *  Given a pointer of a GED database record, and a transformation matrix,
 *  determine if this is a valid NURB, and if so, prepare the surface
 *  so the intersections will work.
 */

int
rt_nurb_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal 	*ip;
struct rt_i		*rtip;
{
	struct rt_nurb_internal	*sip;
	struct nurb_specific 	*nurbs;
	int 			i;

	nurbs = (struct nurb_specific *) 0;

	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	for( i = 0; i < sip->nsrf; i++)
	{
		struct snurb * s;
		struct nurb_specific * n;

		GETSTRUCT( n, nurb_specific);

		n->srf = (struct snurb *) rt_nurb_scopy (sip->srfs[i]);

		n->b_list = (struct nurb_b_list *)0;

		rt_nurb_s_bound(n->srf, n->min, n->max);
		
		s = (struct snurb *) rt_nurb_bezier(n->srf);
		
		while( s != (struct snurb *)0 )
		{
			struct nurb_b_list *b;

			b = (struct nurb_b_list *) rt_malloc( 
				sizeof(struct nurb_b_list ),
				"rt_nurb_prep:nurb_b_list");
			b->srf = s;
			rt_nurb_s_bound( s, b->min, b->max);
			
			VMINMAX( stp->st_min, stp->st_max, b->min);
			VMINMAX( stp->st_min, stp->st_max, b->max);
			s = s->next;
			b->srf->next = (struct snurb *)0;
			b->next = n->b_list;
			n->b_list = b;
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

/*
 *			R T _ N U R B _ P R I N T
 */
void
rt_nurb_print( stp )
register CONST struct soltab *stp;
{
	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific;

	if( nurb == (struct nurb_specific *)0)
	{
		rt_log("rt_nurb_print: no surfaces\n");
		return;
	}

	for( ; nurb != (struct nurb_specific *)0; nurb = nurb->next)
	{
		rt_nurb_s_print("NURB", nurb->srf);
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
	register struct nurb_specific * nurb =
		(struct nurb_specific *)stp->st_specific;
	register struct seg *segp;
	CONST struct rt_tol	*tol = &ap->a_rt_i->rti_tol;
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

	vec_ortho(dir1, rp->r_dir);
	VCROSS( dir2, rp->r_dir, dir1);

	VMOVE(p1, rp->r_pt);
	VADD2(p2, rp->r_pt, rp->r_dir);
	VADD2(p3, rp->r_pt, dir1);
	VADD2(p4, rp->r_pt, dir2);

	/* Note: the equation of the plane in BRLCAD is
	 * Ax + By + Cz = D represented by [A B C D]
	 */
	rt_mk_plane_3pts( plane1, p1, p3, p2, tol );
	rt_mk_plane_3pts( plane2, p1, p2, p4, tol );
	
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
		struct nurb_b_list * n;
		struct rt_nurb_uv_hit *hp;

		n = nurb->b_list;
		while( n != (struct nurb_b_list *)0)
		{
			if( !rt_in_rpp( rp, invdir, n->min, n->max))
			{
				n = n -> next;
				continue;
			}

#define UV_TOL	1.0e-6	/* Paper says 1.0e-4 is reasonable for 1k images, not close up */
			hp = rt_nurb_intersect(
				n->srf, plane1, plane2, UV_TOL );
			while( hp != (struct rt_nurb_uv_hit *)0)
			{
				struct rt_nurb_uv_hit * o;

				if( rt_g.debug & DEBUG_SPLINE )
					rt_log("hit at %d %d sub = %d u = %f v = %f\n",
						ap->a_x, ap->a_y, hp->sub, hp->u, hp->v);

				hit = (struct nurb_hit *) 
					rt_conv_uv(nurb, rp, hp);

				o = hp;
				hp = hp->next;
				rt_free( (char *)o,
					"rt_nurb_shot:rt_nurb_uv_hit structure");

				rt_nurb_add_hit( &hit_list, hit);
			}
			n = n->next;  
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
			rt_free( (char *)h2,"rt_nurb_shot: nurb hit");
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

		rt_free( (char *)h1, "rt_nurb_shot:nurb hit");
		
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}

	return(hit_num);	/* not hit */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T _ N U R B _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_nurb_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
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
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */

	struct snurb * n  = (struct snurb *) hitp->hit_private;
	fastf_t u = hitp->hit_vpriv[0];
	fastf_t v = hitp->hit_vpriv[1];
	fastf_t * norm;

	norm = (fastf_t *) rt_nurb_s_norm( n, u, v);
	
	VMOVE( hitp->hit_normal, norm);
	
	if ( hitp->hit_vpriv[2] == 1)
	{
		VREVERSE( hitp->hit_normal, norm );
	}
	
	rt_free((char *) norm, "rt_nurb_norm: norm");
	
	return;
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
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */
	struct snurb * srf = (struct snurb *) hitp->hit_private;
	struct snurb * us, *vs, * uus, * vvs, *uvs;
	fastf_t *ue, *ve, *uue, *vve, *uve, *se;
        fastf_t         u, v;
        fastf_t         E, F, G;                /* First Fundamental Form */
        fastf_t         L, M, N;                /* Second Fundamental form */
        fastf_t         denom;
        fastf_t         wein[4];                /*Weingarten matrix */
        fastf_t         evec[3];
        fastf_t         mean, gauss, discrim;
        vect_t          norm;
	int 		i;

	if( srf->order[0] <= 2 && srf->order[1] <= 2)
	{

	 	cvp->crv_c1 = cvp->crv_c2 = 0;

		/* any tangent direction */
	 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
		return;
	}
	
	u = hitp->hit_vpriv[0];
	v = hitp->hit_vpriv[1];
	
	us = (struct snurb *) rt_nurb_s_diff(srf, RT_NURB_SPLIT_ROW);
	vs = (struct snurb *) rt_nurb_s_diff(srf, RT_NURB_SPLIT_COL);
	uus = (struct snurb *) rt_nurb_s_diff(us, RT_NURB_SPLIT_ROW);
	vvs = (struct snurb *) rt_nurb_s_diff(vs, RT_NURB_SPLIT_COL);
	uvs = (struct snurb *) rt_nurb_s_diff(vs, RT_NURB_SPLIT_ROW);
	
	se = (fastf_t *) rt_nurb_s_eval(srf, u, v);
	ue = (fastf_t *) rt_nurb_s_eval(us, u,v);
	ve = (fastf_t *) rt_nurb_s_eval(vs, u,v);
	uue = (fastf_t *) rt_nurb_s_eval(uus, u,v);
	vve = (fastf_t *) rt_nurb_s_eval(vvs, u,v);
	uve = (fastf_t *) rt_nurb_s_eval(uvs, u,v);

	rt_nurb_free_snurb( us);
	rt_nurb_free_snurb( vs);
	rt_nurb_free_snurb( uus);
	rt_nurb_free_snurb( vvs);
	rt_nurb_free_snurb( uvs);

	if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ))
	{
		for( i = 0; i < 3; i++)
		{
			ue[i] = (1.0 / se[3] * ue[i]) -
				(ue[3]/se[3]) * se[0]/se[3];
			ve[i] = (1.0 / se[3] * ve[i]) -
				(ve[3]/se[3]) * se[0]/se[3];
		}
		VCROSS(norm, ue, ve);
		VUNITIZE(norm);
		E = VDOT( ue, ue);
		F = VDOT( ue, ve);
		G = VDOT( ve, ve);
		
		for( i = 0; i < 3; i++)
		{
			uue[i] = (1.0 / se[3] * uue[i]) -
				2 * (uue[3]/se[3]) * uue[i] -
				uue[3]/se[3] * (se[i]/se[3]);

			vve[i] = (1.0 / se[3] * vve[i]) -
				2 * (vve[3]/se[3]) * vve[i] -
				vve[3]/se[3] * (se[i]/se[3]);

			 uve[i] = 1.0 / se[3] * uve[i] +
	                        (-1.0 / (se[3] * se[3])) *
        	                (ve[3] * ue[i] + ue[3] * ve[i] +
                	         uve[3] * se[i]) + 
				(-2.0 / (se[3] * se[3] * se[3])) *
	                        (ve[3] * ue[3] * se[i]);
		}

		L = VDOT( norm, uue);
		M = VDOT( norm, uve);
		N = VDOT( norm, vve);
		
	} else
	{
		VCROSS( norm, ue, ve);
		VUNITIZE( norm );
		E = VDOT( ue, ue);
		F = VDOT( ue, ve);
		G = VDOT( ve, ve);
		
		L = VDOT( norm, uue);
		M = VDOT( norm, uve);
		N = VDOT( norm, vve);
	}

	denom = ( (E*G) - (F*F) );
	gauss = (L * N - M *M)/denom;
	mean = (G * L + E * N - 2 * F * M) / (2 * denom);
	discrim = sqrt( mean * mean - gauss);
	
	cvp->crv_c1 = mean - discrim;
	cvp->crv_c2 = mean + discrim;

	if( APX_EQ( ( E*G - F*F), 0.0 ))
	{
		rt_log("first fundamental form is singular E = %g F= %g G = %g\n",
			E,F,G);
		goto cleanup;
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
	cvp->crv_pdir[0] = evec[0] * ue[0] + evec[1] * ve[0];
        cvp->crv_pdir[1] = evec[0] * ue[1] + evec[1] * ve[1];
        cvp->crv_pdir[2] = evec[0] * ue[2] + evec[1] * ve[2];
	VUNITIZE( cvp->crv_pdir);

cleanup:
	rt_free( (char *) se, "rt_nurb_curv:se");
	rt_free( (char *) ue, "rt_nurb_curv:ue");
	rt_free( (char *) ve, "rt_nurb_curv:ve");
	rt_free( (char *) uue, "rt_nurb_curv:uue");
	rt_free( (char *) vve, "rt_nurb_curv:vve");
	rt_free( (char *) uve, "rt_nurb_curv:uve");
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
/*	register struct nurb_specific *nurb =
		(struct nurb_specific *)stp->st_specific; */
	uvp->uv_u = hitp->hit_vpriv[0];
	uvp->uv_v = hitp->hit_vpriv[1];
	return;
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
rt_nurb_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_nurb_internal *sip;
	register int		i;
	register int		j;
	register fastf_t	* vp;
	int			s;
	struct knot_vector 	tkv1,
				tkv2,
				tau1,
				tau2;
	
	RT_CK_DB_INTERNAL(ip);
	sip = (struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);
	
	for( s=0; s < sip->nsrf; s++)
	{
		struct snurb 	* n, *r, *c;
		int 		coords;

		n = (struct snurb *) sip->srfs[s];

		rt_nurb_kvgen( &tkv1,
			n->u_knots.knots[0],
			n->u_knots.knots[n->u_knots.k_size-1], 10);

		rt_nurb_kvgen( &tkv2,
			n->v_knots.knots[0],
			n->v_knots.knots[n->v_knots.k_size-1], 10);
		
		rt_nurb_kvmerge(&tau1, &tkv1, &n->u_knots);
		rt_nurb_kvmerge(&tau2, &tkv2, &n->v_knots);

		r = (struct snurb *) rt_nurb_s_refine( n, RT_NURB_SPLIT_COL, &tau2);
		c = (struct snurb *) rt_nurb_s_refine( r, RT_NURB_SPLIT_ROW, &tau1);

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
			RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_MOVE );
			vp += coords;
			for( j = 1; j < c->s_size[1]; j++)
			{
				RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_DRAW );
				vp += coords;
			}
		}
		
		for( j = 0; j < c->s_size[1]; j++)
		{
			int stride;
			
			stride = c->s_size[1] * coords;
			vp = &c->ctl_points[j * coords];
			RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_MOVE );
			for( i = 0; i < c->s_size[0]; i++)
			{
				RT_ADD_VLIST( vhead, vp, RT_VLIST_LINE_DRAW );
				vp += stride;
			}
		}
		rt_nurb_free_snurb(c);
		rt_nurb_free_snurb(r);

		rt_free( (char *) tau1.knots, "rt_nurb_plot:tau1.knots");
		rt_free( (char *) tau2.knots, "rt_nurb_plot:tau2.knots");
		rt_free( (char *) tkv1.knots, "rt_nurb_plot:tkv1>knots");
		rt_free( (char *) tkv2.knots, "rt_nurb_plot:tkv2.knots");
	}
	return(0);
}

/*
 *			R T _ N U R B _ T E S S
 */
int
rt_nurb_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ N U R B _ I M P O R T
 */
int
rt_nurb_import( ip, ep, mat )
struct rt_db_internal	*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{

	struct rt_nurb_internal * sip;
	union record 		*rp;
	register int		i;
	int			s;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_BSOLID ) 
	{
		rt_log("rt_spl_import: defective header record");
		return (-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_BSPLINE;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_nurb_internal), "rt_nurb_internal");
	sip = (struct rt_nurb_internal *)ip->idb_ptr;
	sip->magic = RT_NURB_INTERNAL_MAGIC;


	sip->nsrf = rp->B.B_nsurf;
	sip->srfs = (struct snurb **) rt_malloc(
		sip->nsrf * sizeof( struct snurb), "nurb srfs[]");
	rp++;

	for( s = 0; s < sip->nsrf; s++)
	{
		register fastf_t 	* m;
		int			coords;
		register dbfloat_t	*vp;
		int			pt_type;
		
		if( rp->d.d_geom_type == 3)
			pt_type = RT_NURB_MAKE_PT_TYPE(3,RT_NURB_PT_XYZ,RT_NURB_PT_NONRAT);
		else
			pt_type = RT_NURB_MAKE_PT_TYPE(4,RT_NURB_PT_XYZ,RT_NURB_PT_RATIONAL);

		sip->srfs[s] = (struct snurb *) rt_nurb_new_snurb(
			rp->d.d_order[0],rp->d.d_order[1],
			rp->d.d_kv_size[0],rp->d.d_kv_size[1],
			rp->d.d_ctl_size[0],rp->d.d_ctl_size[1],
			pt_type);

		vp = (dbfloat_t *) &rp[1];
		
		for( i = 0; i < rp->d.d_kv_size[0]; i++)
			sip->srfs[s]->u_knots.knots[i] = (fastf_t) *vp++;

		for( i = 0; i < rp->d.d_kv_size[1]; i++)
			sip->srfs[s]->v_knots.knots[i] = (fastf_t) *vp++;

		rt_nurb_kvnorm( &sip->srfs[s]->u_knots);
		rt_nurb_kvnorm( &sip->srfs[s]->v_knots);

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
			rt_log("rt_nurb_internal: %d invalid elements per vect\n", rp->d.d_geom_type);
			return (-1);
		}
		
		rp += 1 + rp->d.d_nknots + rp->d.d_nctls;
	}
	return (0);
}

struct nurb_hit *
rt_conv_uv( n, r, h)
struct nurb_specific * n;
struct xray * r;
struct rt_nurb_uv_hit * h;
{
	struct nurb_hit * hit;
	fastf_t * pt;
	point_t vecsub;

	hit = (struct nurb_hit *) rt_malloc( sizeof (struct nurb_hit),
		"rt_conv_uv:nurb hit");
	
	hit->prev = hit->next = (struct nurb_hit *)0;
	VSET(hit->hit_normal, 0.0, 0.0, 0.0);
	
	pt = (fastf_t *) rt_nurb_s_eval(n->srf, h->u, h->v);

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

	rt_free( (char *) pt, "point");
	
	VSUB2( vecsub, hit->hit_point, r->r_pt);
	hit->hit_dist = VDOT( vecsub, r->r_dir);
	hit->hit_uv[0] = h->u;
	hit->hit_uv[1] = h->v;
	hit->hit_private = (char *) n->srf;
	
	return (struct nurb_hit *) hit;
}

#define EQ_HIT(a,b) ( fabs((a) - (b)) < EPSILON)

rt_nurb_add_hit( head, hit )
struct nurb_hit * head;
struct nurb_hit * hit;
{
	register struct nurb_hit * h_ptr;

#if 0
	/* Shouldn't be discarded, because shootray moves start pt around */
	if( hit->hit_dist < .001)
	{
		rt_free( (char *) hit, "internal_add_hit: hit");
		return;
	}
#endif
	
	if( head->next == (struct nurb_hit *) 0)
	{
		head->next = hit;
		hit->prev = head;
		return;
	}

	/* Check for duplicates */
	for( h_ptr = head->next; h_ptr != (struct nurb_hit *)0; h_ptr = h_ptr->next)
	{
		if( EQ_HIT(hit->hit_dist,h_ptr->hit_dist) &&
			EQ_HIT(hit->hit_uv[0], h_ptr->hit_uv[0]) &&
			EQ_HIT(hit->hit_uv[1], h_ptr->hit_uv[1]) )
		{
			rt_free( (char *) hit, "add hit: hit");
			return;
		}
	}

	hit->prev = head;
	hit->next = head->next;
	hit->next->prev = hit;
	head->next = hit;
}

struct nurb_hit *
rt_return_nurb_hit( head )
struct nurb_hit * head;
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

int
rt_nurb_export( ep, ip, local2mm)
struct rt_external	 	* ep;
CONST struct rt_db_internal	* ip;
double				local2mm;
{
	register int		rec_ptr;
	struct rt_nurb_internal	* sip;
	union record		* rec;
	int			s;
	int			grans;
	int			total_grans;
	double			* vp;
	int			n;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_BSOLID) return(-1);
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

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = total_grans * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc(1,ep->ext_nbytes,"nurb external");
	rec = (union record *)ep->ext_buf;

	rec[0].B.B_id = ID_BSOLID;
	rec[0].B.B_nsurf = sip->nsrf;
	
	rec_ptr = 1;

	for( s = 0; s < sip->nsrf; s++)
	{
		grans = rt_nurb_grans( sip->srfs[s]);

		rec[rec_ptr].d.d_id = ID_BSURF;
		rec[rec_ptr].d.d_order[0] = sip->srfs[s]->order[0];
		rec[rec_ptr].d.d_order[1] = sip->srfs[s]->order[1];
		rec[rec_ptr].d.d_kv_size[0] = sip->srfs[s]->u_knots.k_size;
		rec[rec_ptr].d.d_kv_size[1] = sip->srfs[s]->v_knots.k_size;
		rec[rec_ptr].d.d_ctl_size[0] = 	sip->srfs[s]->s_size[0];
		rec[rec_ptr].d.d_ctl_size[1] = 	sip->srfs[s]->s_size[1];
		rec[rec_ptr].d.d_geom_type = 
			RT_NURB_EXTRACT_COORDS(sip->srfs[s]->pt_type);

		vp = (double *) &rec[rec_ptr +1];
		for(n = 0; n < rec[rec_ptr].d.d_kv_size[0]; n++)
		{
			*vp++ = sip->srfs[s]->u_knots.knots[n];
		}

		for(n = 0; n < rec[rec_ptr].d.d_kv_size[1]; n++)
		{
			*vp++ = sip->srfs[s]->v_knots.knots[n];
		}
		
		vp = (double *) &rec[(rec[rec_ptr].d.d_kv_size[0] + 
			rec[rec_ptr].d.d_kv_size[0] + 7) /8];

		for( n = 0; n < (rec[rec_ptr].d.d_ctl_size[0] + 
			rec[rec_ptr].d.d_ctl_size[1]) * 
			rec[rec_ptr].d.d_geom_type; n++)
			*vp++ = sip->srfs[s]->ctl_points[n];

		rec_ptr += grans;
		total_grans -= grans;
	}
	return(0);
}

int 
rt_nurb_grans( srf )
struct snurb * srf;
{
	int grans = 0;
	int total_knots, total_points;

	grans++;
	total_knots = srf->u_knots.k_size +
		srf->v_knots.k_size;
	grans += (total_knots+7)/8;

	total_points = RT_NURB_EXTRACT_COORDS(srf->pt_type) *
		(srf->s_size[0] +
		srf->s_size[1]);

	grans += (total_points + 7)/8;

	return grans;
}

void
rt_nurb_ifree( ip )
struct rt_db_internal 	*ip;
{
	register struct rt_nurb_internal * sip;
	register int			 i;

	RT_CK_DB_INTERNAL(ip);
	sip = ( struct rt_nurb_internal *) ip->idb_ptr;
	RT_NURB_CK_MAGIC(sip);

	/* Free up storage for the nurb surfaces */
	for( i = 0; i < sip->nsrf; i++)
	{
		rt_nurb_free_snurb( sip->srfs[i] );
	}
	sip->magic = 0;
	sip->nsrf = 0;
	rt_free( (char *)sip, "sip ifree");
	ip->idb_ptr = GENPTR_NULL;
}

rt_nurb_describe(str, ip, verbose, mm2local )
struct rt_vls		* str;
struct rt_db_internal	* ip;
int			verbose;
double			mm2local;
{
	register int		j;
	register struct rt_nurb_internal * sip =
		(struct rt_nurb_internal *) ip->idb_ptr;
	char 			buf[256];
	int			i;
	int			surf;

	RT_NURB_CK_MAGIC(sip);
	rt_vls_strcat( str, "Non Uniform Rational B-SPline solid (nurb)\n");
	
	sprintf( buf, "\t%d surfaces\n", sip->nsrf);
	rt_vls_strcat( str, buf );

	for( surf = 0; surf < sip->nsrf; surf++)
	{
		register struct snurb 	* np;
		register fastf_t 	* mp;

		np = sip->srfs[surf];
		mp = np->ctl_points;

		sprintf( buf, "\tSurface %d: order %d x %d, mesh %d x %d\n",
			surf, np->order[0], np->order[1],
			np->s_size[0], np->s_size[1]);
		rt_vls_strcat( str, buf);

		sprintf( buf, "\t\tV (%g, %g, %g)\n",
			mp[X] * mm2local, 
			mp[Y] * mm2local, 
			mp[Z] * mm2local);

		rt_vls_strcat( str, buf);
		
		if( !verbose ) continue;
		
		/* print out all the points */
		for(i=0; i < np->s_size[0]; i++)
		{
			sprintf( buf,"\tRT_NURB_SPLIT_ROW %d:\n", i);
			rt_vls_strcat( str, buf );
			for( j = 0; j < np->s_size[1]; j++)
			{
				sprintf( buf, "\t\t(%g, %g, %g)\n",
					mp[X] * mm2local, 
					mp[Y] * mm2local, 
					mp[Z] * mm2local);
				rt_vls_strcat( str, buf);
				mp += RT_NURB_EXTRACT_COORDS(np->pt_type);
			}
		}
	}
	return 0;
}

rt_nurb_pbound( srf, vmin, vmax)
struct snurb * srf;
point_t vmin, vmax;
{
	register fastf_t * ptr;
	register int coords;
	int i;
	
 	vmin[0] = vmin[1] = vmin[2] = INFINITY;
	vmax[0] = vmax[1] = vmax[2] = -INFINITY;

	ptr = srf->ctl_points;

	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	for( i = (srf->s_size[RT_NURB_SPLIT_ROW] * 
	    srf->s_size[RT_NURB_SPLIT_COL] ); i > 0; i--)
	{
		V_MIN( (vmin[0]), (ptr[0]));
		V_MAX( (vmax[0]), (ptr[0]));

		V_MIN( (vmin[1]), (ptr[1]));
		V_MAX( (vmax[1]), (ptr[1]));
		
		ptr += coords;
	}
}
