/*		N U R B _ R A Y . C
 *
 *  Function-
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
 * Copyright Notice -
 *     This software is Copyright (C) 1991 by the United States Army.
 *     All rights reserved.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./nurb.h"

struct snurb *
rt_nurb_project_srf( srf, plane1, plane2)
struct snurb *srf;
plane_t plane1, plane2;
{

	register struct snurb *psrf;
	register fastf_t *mp1, *mp2;
	int	n_pt_type;
	int	rational;
	int	i;

	rational = EXTRACT_RAT( srf->mesh->pt_type);

	n_pt_type = MAKE_PT_TYPE( 2, PT_PROJ, 0);

	psrf = (struct snurb *) rt_nurb_new_snurb( srf->order[0], srf->order[1],
	    srf->u_knots->k_size, srf->v_knots->k_size,
	    srf->mesh->s_size[0], srf->mesh->s_size[1], n_pt_type);

	psrf->next = (struct snurb *)0;
	psrf->trim = (struct cnurb *)0;
	psrf->dir = COL;

	for ( i = 0; i < srf->u_knots->k_size; i++) {
		psrf->u_knots->knots[i] = srf->u_knots->knots[i];
	}

	for ( i = 0; i < srf->v_knots->k_size; i++) {
		psrf->v_knots->knots[i] = srf->v_knots->knots[i];
	}

	mp1 = srf->mesh->ctl_points;
	mp2 = psrf->mesh->ctl_points;

	for ( i = 0; i < srf->mesh->s_size[0] * srf->mesh->s_size[1]; i++) {

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

		mp1 += EXTRACT_COORDS(srf->mesh->pt_type);
		mp2 += EXTRACT_COORDS(psrf->mesh->pt_type);
	}

	return (struct snurb *) psrf;
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

struct s_mesh *
rt_nurb_clip_srf( srf, dir, min, max)
struct snurb *srf;
int	dir;
fastf_t *min, *max;
{
	register struct internal_convex_hull *ch;
	register fastf_t * mp1;
	fastf_t * p1, *p2, *p3, *p4;	/* corner points of the mesh */
	fastf_t v1[2], v2[2], v3[2];		/* vectors from corneres */
	struct internal_line l1;
	fastf_t norm;
	fastf_t value;
	int	i, j, k;
	int	coords;
	int	stride;
	int col_size, row_size;

	col_size = srf->mesh->s_size[1];
	row_size = srf->mesh->s_size[0];

	coords = EXTRACT_COORDS(srf->mesh->pt_type);

	p1 = srf->mesh->ctl_points;
	p2 = srf->mesh->ctl_points + coords * (col_size - 1);
	p3 = srf->mesh->ctl_points + (coords * col_size * 
	    (row_size - 1));
	p4 = srf->mesh->ctl_points + (coords * col_size * 
	    (row_size - 1)) + 
	    ((col_size - 1) * coords);

	if ( dir == ROW) {
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

	if( dir == ROW)
	{
		ch = (struct internal_convex_hull *) rt_malloc(
			sizeof( struct internal_convex_hull) * col_size,
			"rt_nurb_clip_srf:convex_hull");

		for( i = 0; i < col_size; i++)
		{
			ch[i].param = (fastf_t) i / (col_size - 1.0);
			ch[i].min = 1.0e8;
			ch[i].max = -1.0e8;
		}

		mp1 = srf->mesh->ctl_points;

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
		if ( SIGN(ch[col_size -1].min) != SIGN(ch[col_size -1].max))
			*max = 1.0;
	} else
	{
		ch = (struct internal_convex_hull *) rt_malloc(
			sizeof( struct internal_convex_hull) * row_size,
			"rt_nurb_clip_srf:convex_hull");

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

			mp1 = srf->mesh->ctl_points + i * coords;
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
		if ( SIGN(ch[row_size-1 ].min) != SIGN(ch[row_size -1].max))
			*max = 1.0;	}

	rt_free( ch, "rt_nurb_clip_srf:convex_hull");
}


struct snurb *
rt_nurb_region_from_srf( srf, dir, param1, param2)
struct snurb *srf;
int	dir;
fastf_t param1, param2;
{
	int	i;
	struct snurb *region;
	struct knot_vector *new_knots;

	if ( dir == ROW) {
		new_knots = (struct knot_vector *) rt_malloc( 
		    sizeof( struct knot_vector ), 
		    "region from srf knot vector");

		new_knots->k_size = srf->order[0] * 2;
		new_knots->knots = (fastf_t * ) rt_malloc( sizeof (fastf_t)
		    *srf->order[0] * 2, 
		    "rt_nurb__region_from_srf: newknot values");

		for ( i = 0; i < srf->order[0]; i++) {
			new_knots->knots[i] = param1;
			new_knots->knots[i+srf->order[0]] = param2;
		}
	} else
	 {
		new_knots = (struct knot_vector *) rt_malloc( 
		    sizeof( struct knot_vector ), 
		    "region from srf knot vector");

		new_knots->k_size = srf->order[1] * 2;
		new_knots->knots = (fastf_t * ) rt_malloc( sizeof (fastf_t)
		    *srf->order[1] * 2, 
		    "rt_nurb_region_from_srf: newknot values");

		for ( i = 0; i < srf->order[1]; i++) {
			new_knots->knots[i] = param1;
			new_knots->knots[i+srf->order[1]] = param2;
		}

	}

	region = (struct snurb *)
		rt_nurb_s_refine( srf, dir, new_knots);

	rt_free( new_knots->knots, "rt_nurb_region_from_srf:knotvalues");
	rt_free( new_knots, "rt_nurb_region_from_srf:knot vector");

	return region;
}
