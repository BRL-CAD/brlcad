/*                      N U R B _ R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nurb */
/** @{ */
/** @file nurb_ray.c
 *
 *  	Functions which support the ray intersection
 *	for surfaces.
 *
 *  Author -
 *	Paul R. Stay
 *
 *  Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 */
/** @} */

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "../librt/debug.h"


void rt_nurb_pbound(struct face_g_snurb *srf, fastf_t *vmin, fastf_t *vmax);

struct face_g_snurb *
rt_nurb_project_srf(const struct face_g_snurb *srf, fastf_t *plane1, fastf_t *plane2, struct resource *res)
{

	register struct face_g_snurb *psrf;
	register fastf_t *mp1, *mp2;
	int	n_pt_type;
	int	rational;
	int	i;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log( "rt_nurb_project_srf: projecting surface, planes = (%g %g %g %g) (%g %g %g %g)\n",
			V4ARGS( plane1 ), V4ARGS( plane2 ) );

	rational = RT_NURB_IS_PT_RATIONAL( srf->pt_type);

	n_pt_type = RT_NURB_MAKE_PT_TYPE( 2, RT_NURB_PT_PROJ, 0);

	psrf = (struct face_g_snurb *) rt_nurb_new_snurb( srf->order[0], srf->order[1],
	    srf->u.k_size, srf->v.k_size,
	    srf->s_size[0], srf->s_size[1], n_pt_type, res);

	psrf->dir = RT_NURB_SPLIT_COL;

	for ( i = 0; i < srf->u.k_size; i++) {
		psrf->u.knots[i] = srf->u.knots[i];
	}

	for ( i = 0; i < srf->v.k_size; i++) {
		psrf->v.knots[i] = srf->v.knots[i];
	}

	mp1 = srf->ctl_points;
	mp2 = psrf->ctl_points;

	for ( i = 0; i < srf->s_size[0] * srf->s_size[1]; i++) {

		if ( rational ) {
			mp2[0] = (mp1[0] / mp1[3] * plane1[0] +
			    mp1[1] / mp1[3] * plane1[1] +
			    mp1[2] / mp1[3] * plane1[2] - plane1[3]) *
			    mp1[3];
			mp2[1] = (mp1[0] / mp1[3] * plane2[0] +
			    mp1[1] / mp1[3] * plane2[1] +
			    mp1[2] / mp1[3] * plane2[2] - plane2[3]) *
			    mp1[3];
		} else
		 {
			mp2[0] = mp1[0] * plane1[0] + mp1[1] * plane1[1] +
			    mp1[2] * plane1[2] - plane1[3];
			mp2[1] = mp1[0] * plane2[0] + mp1[1] * plane2[1] +
			    mp1[2] * plane2[2] - plane2[3];
		}

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		{
			if( rational )
				bu_log( "\tmesh pt (%g %g %g %g), becomes (%g %g)\n", V4ARGS( mp1 ), mp2[0], mp2[1] );
			else
				bu_log( "\tmesh pt (%g %g %g), becomes (%g %g)\n", V3ARGS( mp1 ), mp2[0], mp2[1] );
		}

		mp1 += RT_NURB_EXTRACT_COORDS(srf->pt_type);
		mp2 += RT_NURB_EXTRACT_COORDS(psrf->pt_type);
	}

	return (struct face_g_snurb *) psrf;
}


/* This routine should go away and be changed into a macro
 * but for now I want to be able to useit with debugging.
 * 						- Paul
 */
#define FINDZERO(x0,x1,y0,y1) (x0 - y0 * ( x1 - x0) / (y1-y0))

struct internal_line {
	fastf_t a, b;
};

struct internal_convex_hull {
	fastf_t param;
	fastf_t min, max;
};


#define SIGN(a)	((a < 0.0)? -1 : 1)

void
rt_nurb_clip_srf(const struct face_g_snurb *srf, int dir, fastf_t *min, fastf_t *max)
{
	struct internal_convex_hull ch[20]; /* max order is 10 */
	register fastf_t * mp1;
	fastf_t * p1, *p2, *p3, *p4;	/* corner points of the mesh */
	fastf_t v1[2], v2[2], v3[2];		/* vectors from corneres */
	struct internal_line l1;
	fastf_t norm;
	fastf_t value;
	int	i;
	register int	j;
	int	k;
	int	coords;
	int col_size, row_size;

	col_size = srf->s_size[1];
	row_size = srf->s_size[0];

	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	p1 = srf->ctl_points;
	p2 = srf->ctl_points + coords * (col_size - 1);
	p3 = srf->ctl_points + (coords * col_size *
	    (row_size - 1));
	p4 = srf->ctl_points + (coords * col_size *
	    (row_size - 1)) +
	    ((col_size - 1) * coords);

	if ( dir == RT_NURB_SPLIT_ROW) {
		v1[0] = p1[0] - p3[0];
		v1[1] = p1[1] - p3[1];

		v2[0] = p2[0] - p4[0];
		v2[1] = p2[1] - p4[1];
	} else
	 {
		v1[0] = p1[0] - p2[0];
		v1[1] = p1[1] - p2[1];

		v2[0] = p3[0] - p4[0];
		v2[1] = p3[1] - p4[1];
	}

	v3[0] = v1[0] + v2[0];
	v3[1] = v1[1] + v1[1];

	norm = sqrt( v3[1] * v3[1] + v3[0] * v3[0]);
	l1.a = v3[1] / norm;
	l1.b = -v3[0] / norm;

	*min = 1.0e8;
	*max = -1.0e8;

	if( dir == RT_NURB_SPLIT_ROW)
	{
		for( i = 0; i < col_size; i++)
		{
			ch[i].param = (fastf_t) i / (col_size - 1.0);
			ch[i].min = 1.0e8;
			ch[i].max = -1.0e8;
		}

		mp1 = srf->ctl_points;

		for( i = 0; i < row_size; i++)
		{
			for( j = 0; j < col_size; j++)
			{
				value = - (mp1[0] * l1.a + mp1[1] * l1.b);
				if( value <= ch[j].min)
					ch[j].min = value;
				if( value >= ch[j].max)
					ch[j].max = value;
				mp1 += coords;
			}
		}

		for( k = 0; k < col_size - 1; k++)
		for( j = k+1; j < col_size; j++)
		{
			fastf_t d;
			fastf_t param1, param2;

			param1 = ch[k].param;
			param2 = ch[j].param;

			d = FINDZERO( param1, param2, ch[k].max, ch[j].max);
			if( d <= *min ) *min = d * .99;
			if( d >= *max ) *max = d * .99 + .01;

			d = FINDZERO( param1, param2, ch[k].min, ch[j].min);
			if( d <= *min ) *min = d * .99;
			if( d >= *max ) *max = d * .99 + .01;
		}

		if (*min <= 0.0)
			*min = 0.0;
		if (*max >= 1.0)
			*max = 1.0;
		if ( SIGN(ch[0].min) != SIGN(ch[0].max))
			*min = 0.0;
		i = SIGN(ch[col_size -1].min);
		j = SIGN(ch[col_size -1].max);
		if ( i != j)
			*max = 1.0;
	} else
	{
		for( i = 0; i < row_size; i++)
		{
			ch[i].param = (fastf_t) i / (row_size - 1.0);
			ch[i].min = 1.0e8;
			ch[i].max = -1.0e8;
		}


		for( i = 0; i < col_size; i++)
		{
			int stride;

			stride = coords * col_size;

			mp1 = srf->ctl_points + i * coords;
			for( j = 0; j < row_size; j++)
			{
				value = - (mp1[0] * l1.a + mp1[1] * l1.b);
				if( value <= ch[j].min)
					ch[j].min = value;
				if( value >= ch[j].max)
					ch[j].max = value;
				mp1 += stride;
			}
		}

		for( k = 0; k < row_size - 1; k++)
		for( j = k+1; j < row_size; j++)
		{
			fastf_t d;
			fastf_t param1, param2;

			param1 = ch[k].param;
			param2 = ch[j].param;

			d = FINDZERO( param1, param2, ch[k].max, ch[j].max);
			if( d <= *min ) *min = d * .99;
			if( d >= *max ) *max = d * .99 + .01;

			d = FINDZERO( param1, param2, ch[k].min, ch[j].min);
			if( d <= *min ) *min = d * .99;
			if( d >= *max ) *max = d * .99 + .01;
		}
		if (*min <= 0.0)
			*min = 0.0;
		if (*max >= 1.0)
			*max = 1.0;
		if ( SIGN(ch[0].min) != SIGN(ch[0].max))
			*min = 0.0;
		i = SIGN(ch[row_size-1 ].min);
		j = SIGN(ch[row_size -1].max);
		if ( i != j )
			*max = 1.0;	}
}

/*
 *			R T _ N U R B _ R E G I O N _ F R O M _ S R F
 */
struct face_g_snurb *
rt_nurb_region_from_srf(const struct face_g_snurb *srf, int dir, fastf_t param1, fastf_t param2, struct resource *res)
{
	register int	i;
	struct face_g_snurb *region;
	struct knot_vector new_knots;
	fastf_t knot_vec[40];

	/* Build the new knot vector in the local array */
	/* XXX fill in magic number here? */
	new_knots.knots = & knot_vec[0];

	if ( dir == RT_NURB_SPLIT_ROW) {
		new_knots.k_size = srf->order[0] * 2;

		for ( i = 0; i < srf->order[0]; i++) {
			knot_vec[i] = param1;
			knot_vec[i+srf->order[0]] = param2;
		}
	} else
	 {
		new_knots.k_size = srf->order[1] * 2;

		for ( i = 0; i < srf->order[1]; i++) {
			knot_vec[i] = param1;
			knot_vec[i+srf->order[1]] = param2;
		}

	}
	if( new_knots.k_size >= 40 )  rt_bomb("rt_nurb_region_from_srf() local kv overflow\n");

	region = rt_nurb_s_refine( srf, dir, &new_knots, res);

	return region;
}

/*
 *			R T _ N U R B _ I N T E R S E C T
 */
struct rt_nurb_uv_hit *
rt_nurb_intersect(const struct face_g_snurb *srf, fastf_t *plane1, fastf_t *plane2, double uv_tol, struct resource *res)
{
	struct rt_nurb_uv_hit * h;
	struct face_g_snurb 	* psrf,
			* osrf;
	int 		dir,
			sub;

	point_t 	vmin,
			vmax;
	fastf_t 	u[2],
			v[2];
	struct bu_list	plist;

	NMG_CK_SNURB(srf);

	h = (struct rt_nurb_uv_hit *) 0;
	BU_LIST_INIT( &plist );

	/* project the surface to a 2 dimensional problem */
	/* NOTE that this gives a single snurb back, NOT a list */
	psrf = rt_nurb_project_srf( srf, plane2, plane1, res );
	psrf->dir = 1;
	BU_LIST_APPEND( &plist, &psrf->l );

	if( RT_G_DEBUG & DEBUG_SPLINE )
		rt_nurb_s_print("srf", psrf);

	/* This list starts out with only a single snurb,
	 * but more may be added on as work progresses.
	 */
top:
	while( BU_LIST_WHILE( psrf, face_g_snurb, &plist ) )  {
		int flat;

		BU_LIST_DEQUEUE( &psrf->l );
		NMG_CK_SNURB(psrf);
		sub = 0;
		flat = 0;
		dir = psrf->dir;

		while(!flat)
		{
			fastf_t smin, smax;

			sub++;
			dir = (dir == 0)?1:0;	/* change direction */

			if( RT_G_DEBUG & DEBUG_SPLINE )
				rt_nurb_s_print("psrf", psrf);

			rt_nurb_pbound( psrf, vmin, vmax);

			/* Check for origin to be included in the bounding box */
			if( !(vmin[0] <= 0.0 && vmin[1] <= 0.0 &&
				vmax[0] >= 0.0 && vmax[1] >= 0.0 ))
			{
				if( RT_G_DEBUG & DEBUG_SPLINE )
					bu_log( "this srf doesn't include the origin\n" );
				flat = 1;
				rt_nurb_free_snurb( psrf, res );
				continue;
			}

			rt_nurb_clip_srf( psrf, dir, &smin, &smax);

			if( (smax - smin) > .8)  {
				/* Split surf, requeue both sub-surfs at head */
				/* New surfs will have same dir as arg, here */
				if( RT_G_DEBUG & DEBUG_SPLINE )
					bu_log( "splitting this surface\n" );
				rt_nurb_s_split( &plist, psrf, dir, res );
				rt_nurb_free_snurb( psrf, res );
				goto top;
			}
			if( smin > 1.0 || smax < 0.0 )
			{
				if( RT_G_DEBUG & DEBUG_SPLINE )
					bu_log( "eliminating this surface (smin=%g, smax=%g)\n", smin, smax );
				flat = 1;
				rt_nurb_free_snurb( psrf, res );
				continue;
			}
			if ( dir == RT_NURB_SPLIT_ROW)
			{
		                smin = (1.0 - smin) * psrf->u.knots[0] +
                		        smin * psrf->u.knots[
		                        psrf->u.k_size -1];
		                smax = (1.0 - smax) * psrf->u.knots[0] +
		                        smax * psrf->u.knots[
                		        psrf->u.k_size -1];
			} else
			{
	                        smin = (1.0 - smin) * psrf->v.knots[0] +
        	                        smin * psrf->v.knots[
                	                psrf->v.k_size -1];
                        	smax = (1.0 - smax) * psrf->v.knots[0] +
                                	smax * psrf->v.knots[
	                                psrf->v.k_size -1];
			}

			osrf = psrf;
			psrf = (struct face_g_snurb *)	rt_nurb_region_from_srf(
				osrf, dir, smin, smax, res);

			psrf->dir = dir;
			rt_nurb_free_snurb(osrf, res);

			if( RT_G_DEBUG & DEBUG_SPLINE )
			{
				bu_log( "After call to rt_nurb_region_from_srf() (smin=%g, smax=%g)\n", smin, smax );
				rt_nurb_s_print("psrf", psrf);
			}

			u[0] = psrf->u.knots[0];
			u[1] = psrf->u.knots[psrf->u.k_size -1];

			v[0] = psrf->v.knots[0];
			v[1] = psrf->v.knots[psrf->v.k_size -1];

                        if( (u[1] - u[0]) < uv_tol && (v[1] - v[0]) < uv_tol)
                        {
				struct rt_nurb_uv_hit * hit;

				if( RT_G_DEBUG & DEBUG_SPLINE )
				{
					fastf_t p1[4], p2[4];
					int coords;
					vect_t diff;

					coords = RT_NURB_EXTRACT_COORDS( srf->pt_type );
					rt_nurb_s_eval( srf, u[0], v[0], p1 );
					rt_nurb_s_eval( srf, u[1], v[1], p2 );

					if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
					{
						fastf_t inv_w;

						inv_w = 1.0 / p1[coords-1];
						VSCALE( p1, p1, inv_w )

						inv_w = 1.0 / p2[coords-1];
						VSCALE( p2, p2, inv_w )
					}

					VSUB2( diff, p1, p2 )
					bu_log( "Precision of hit point = %g (%f %f %f) <-> (%f %f %f)\n",
						MAGNITUDE( diff ), V3ARGS( p1 ), V3ARGS( p2 ) );
				}

                       		hit = (struct rt_nurb_uv_hit *) bu_malloc(
						sizeof( struct rt_nurb_uv_hit),  "hit" );

                        	hit->next = (struct rt_nurb_uv_hit *)0;
                        	hit->sub = sub;
                        	hit->u = (u[0] + u[1])/2.0;
                        	hit->v = (v[0] + v[1])/2.0;

                        	if( h == (struct rt_nurb_uv_hit *)0)
                        		h = hit;
                        	else
                        	{
                        		hit->next = h;
                        		h = hit;
                        	}
                        	flat = 1;
                        	rt_nurb_free_snurb( psrf, res );
                        }
			if( (u[1] - u[0]) > (v[1] - v[0]) )
				dir = 1;
			else dir = 0;
		}
	}

	return (struct rt_nurb_uv_hit *)h;
}

void
rt_nurb_pbound(struct face_g_snurb *srf, fastf_t *vmin, fastf_t *vmax)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
