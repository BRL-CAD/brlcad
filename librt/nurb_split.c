/*	 	N U R B _ S P L I T . C
 *
 * Function -
 * 	Subdivide a nurb surface by inserting a multiple knot of
 * 	of the surface order in a given direction and return the 
 *	resulting surfaces.
 *
 * Author-
 *	Paul Randal Stay
 *
 * Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The US Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland 21005
 *
 *
 * Copyright Notice -
 * 	This software if Copyright (C) 1990 by the United States Arym.
 *	All Rights Reserved.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./nurb.h"

/* Algorithm
 *
 * 	Given a parametric direction (u or v) look at the direction 
 * knot vector and insert a multiple knot of parametric direction surface
 * order.  If internal knot values exist than pick the one closest to the
 * middle and add additional knots to split at that value, otherwise
 * add multiple knots at the mid point of the knot vector. Use the new
 * knot vector to pass to the oslo refinement process and split the surface.
 * Separate the surface and return the two resulting surface.
 */

struct snurb *
rt_nurb_s_split( srf, dir)
struct snurb * srf;
int dir;
{
	struct knot_vector *new_kv;
	fastf_t value;
	struct oslo_mat * oslo;
	int i;
	int k_index;
	struct snurb * srf1, * srf2;

	if ( dir == ROW )
	{
		value = srf->u_knots->knots[(srf->u_knots->k_size -1)/2];
		
		for( i = 0; i < srf->u_knots->k_size; i++)
			if( value == srf->u_knots->knots[i] )
			{
				k_index = i;
				break;
			}
		if ( k_index == 0)
		{
			value = ( value + 
				srf->u_knots->knots[ srf->u_knots->k_size -1])
				/2.0;
			k_index = srf->order[0];
		}

		new_kv = (struct knot_vector *) 
			rt_nurb_kvmult(srf->u_knots, srf->order[0], value);

		oslo = ( struct oslo_mat *) 
			rt_nurb_calc_oslo( srf->order[ROW], srf->u_knots, new_kv);

		srf1 = (struct snurb *) rt_malloc( sizeof( struct snurb),
			"rt_nurb_s_split: row snurb struct");
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = ROW;
		srf1->u_knots = (struct knot_vector *) 
			rt_nurb_kvextract(new_kv, 0, k_index + srf1->order[0]);
		srf1->v_knots = (struct knot_vector *) 
			rt_nurb_kvcopy(srf->v_knots);
		
		srf1->mesh = (struct s_mesh *) rt_malloc( sizeof( struct s_mesh),
			"rt_nurb_s_split: srf1 s_mesh");

		srf1->mesh->pt_type = srf->mesh->pt_type;
		srf1->mesh->s_size[0] = srf1->v_knots->k_size - 
			srf1->order[1];
		srf1->mesh->s_size[1] = srf1->u_knots->k_size - 
			srf1->order[0];

		srf1->mesh->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf1->mesh->s_size[0] *
				srf1->mesh->s_size[1] * 
				EXTRACT_COORDS( srf1->mesh->pt_type),
				"rt_nurb_s_split: srf1 row mesh control points");

		srf2 = (struct snurb *) rt_malloc( sizeof( struct snurb),
			"rt_nurb_s_split: row snurb struct");
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = ROW;
		srf2->u_knots = (struct knot_vector *) 
			rt_nurb_kvextract(new_kv, k_index, new_kv->k_size);
		srf2->v_knots = (struct knot_vector *) 
			rt_nurb_kvcopy(srf->v_knots);
		
		srf2->mesh = (struct s_mesh *) rt_malloc( sizeof( struct s_mesh),
			"rt_nurb_s_split: srf2 s_mesh");

		srf2->mesh->pt_type = srf->mesh->pt_type;
		srf2->mesh->s_size[0] = srf2->v_knots->k_size - 
			srf2->order[1];
		srf2->mesh->s_size[1] = srf2->u_knots->k_size - 
			srf2->order[0];

		srf2->mesh->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf2->mesh->s_size[0] *
				srf2->mesh->s_size[1] * 
				EXTRACT_COORDS( srf2->mesh->pt_type),
				"rt_nurb_s_split: srf2 row mesh control points");

		for( i = 0; i < srf->mesh->s_size[0]; i++)
		{
			fastf_t * old_mesh_ptr;
			fastf_t * new_mesh_ptr;

			old_mesh_ptr = &srf->mesh->ctl_points[
				i * srf->mesh->s_size[1] * 
				EXTRACT_COORDS( srf->mesh->pt_type)];
			new_mesh_ptr = &srf1->mesh->ctl_points[
				i * srf1->mesh->s_size[1] * 
				EXTRACT_COORDS( srf1->mesh->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				EXTRACT_COORDS( srf->mesh->pt_type ),
				EXTRACT_COORDS( srf1->mesh->pt_type ),
				0, k_index, srf1->mesh->pt_type);
			new_mesh_ptr = &srf2->mesh->ctl_points[
				i * srf2->mesh->s_size[1] * 
				EXTRACT_COORDS( srf2->mesh->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				EXTRACT_COORDS( srf->mesh->pt_type ),
				EXTRACT_COORDS( srf2->mesh->pt_type ),
				k_index, new_kv->k_size - srf2->order[0], 
				srf2->mesh->pt_type);
		}
	}
	else 
	{
		value = srf->v_knots->knots[(srf->v_knots->k_size -1)/2];
		
		for( i = 0; i < srf->v_knots->k_size; i++)
			if( value == srf->v_knots->knots[i] )
			{
				k_index = i;
				break;
			}
		if ( k_index == 0)
		{
			value = ( value + 
				srf->v_knots->knots[ srf->v_knots->k_size -1])
				/2.0;
			k_index = srf->order[1];
		}

		new_kv = (struct knot_vector *) 
			rt_nurb_kvmult(srf->v_knots, srf->order[COL], value);

		oslo = ( struct oslo_mat *) 
			rt_nurb_calc_oslo( srf->order[COL], srf->v_knots, new_kv);

		srf1 = (struct snurb *) rt_malloc( sizeof( struct snurb),
			"rt_nurb_s_split: col snurb struct");
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = COL;
		srf1->v_knots = (struct knot_vector *) 
			rt_nurb_kvextract(new_kv, 0, k_index + srf1->order[COL]);
		srf1->u_knots = (struct knot_vector *) rt_nurb_kvcopy(srf->u_knots);
		
		srf1->mesh = (struct s_mesh *) 
			rt_malloc( sizeof( struct s_mesh),
			"rt_nurb_s_split: srf1 col s_mesh");

		srf1->mesh->pt_type = srf->mesh->pt_type;
		srf1->mesh->s_size[0] = srf1->v_knots->k_size - 
			srf1->order[1];
		srf1->mesh->s_size[1] = srf1->u_knots->k_size - 
			srf1->order[0];

		srf1->mesh->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf1->mesh->s_size[0] *
				srf1->mesh->s_size[1] * 
				EXTRACT_COORDS( srf1->mesh->pt_type),
				"rt_nurb_s_split: srf1 col mesh control points");

		srf2 = (struct snurb *) rt_malloc( sizeof( struct snurb),
			"rt_nurb_s_split: col snurb struct");
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = COL;
		srf2->v_knots = (struct knot_vector *) 
			rt_nurb_kvextract(new_kv, k_index, new_kv->k_size);
		srf2->u_knots = (struct knot_vector *) rt_nurb_kvcopy(srf->u_knots);
		
		srf2->mesh = (struct s_mesh *) 
			rt_malloc( sizeof( struct s_mesh),
			"rt_nurb_s_split: col srf2 s_mesh");

		srf2->mesh->pt_type = srf->mesh->pt_type;
		srf2->mesh->s_size[0] = srf2->v_knots->k_size - 
			srf2->order[1];
		srf2->mesh->s_size[1] = srf2->u_knots->k_size - 
			srf2->order[0];

		srf2->mesh->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf2->mesh->s_size[0] *
				srf2->mesh->s_size[1] * 
				EXTRACT_COORDS( srf2->mesh->pt_type),
				"rt_nurb_s_split: srf2 col mesh control points");

		for( i = 0; i < srf->mesh->s_size[1]; i++)
		{
			fastf_t * old_mesh_ptr;
			fastf_t * new_mesh_ptr;

			old_mesh_ptr = &srf->mesh->ctl_points[
				i * EXTRACT_COORDS( srf->mesh->pt_type)];
			new_mesh_ptr = &srf1->mesh->ctl_points[
				i * EXTRACT_COORDS( srf1->mesh->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				srf->mesh->s_size[1] *
				EXTRACT_COORDS( srf->mesh->pt_type ),
				srf1->mesh->s_size[1] * 
				EXTRACT_COORDS( srf1->mesh->pt_type ),
				0, k_index, srf1->mesh->pt_type);
			new_mesh_ptr = &srf2->mesh->ctl_points[
				i * EXTRACT_COORDS( srf2->mesh->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				srf->mesh->s_size[1] *
				EXTRACT_COORDS( srf->mesh->pt_type ),
				srf2->mesh->s_size[1] *
				EXTRACT_COORDS( srf2->mesh->pt_type ),
				k_index, new_kv->k_size - srf2->order[1], 
				srf2->mesh->pt_type);
		}
	}
	
	srf1->next = srf2;
	srf2->next = (struct snurb *)0;

	rt_nurb_free_oslo(oslo);
	rt_free( (char *)new_kv->knots, "rt_nurb_s_split: new kv knots");
	rt_free( (char *)new_kv, "rt_nurb_s_split: new kv knot struct");

	return (struct snurb *) srf1;
}

/* rt_nurb_c_split()
 * Split a NURB curve by inserting a multiple knot and return
 * the result of the two curves.
 *
 * Algorithm
 *
 * Insert a multiple knot of the curve 
 * order.  If internal knot values exist than pick the one closest to the
 * middle and add additional knots to split at that value, otherwise
 * add multiple knots at the mid point of the knot vector. Use the new
 * knot vector to pass to the oslo refinement process and split the curve.
 * Separate the curve and return the two resulting curves.
 */

struct cnurb *
rt_nurb_c_split( crv)
struct cnurb * crv;
{
	struct knot_vector *new_kv;
	fastf_t value;
	struct oslo_mat * oslo;
	int i;
	int k_index;
	struct cnurb * crv1, * crv2;
	int coords;

	coords = EXTRACT_COORDS( crv->mesh->pt_type ),

	value = crv->knot->knots[(crv->knot->k_size -1)/2];
		
	for( i = 0; i < crv->knot->k_size; i++)
		if( value == crv->knot->knots[i] )
		{
			k_index = i;
			break;
		}
	if ( k_index == 0)
	{
		value = ( value + 
			crv->knot->knots[ crv->knot->k_size -1])
			/2.0;
		k_index = crv->order;
	}

	new_kv = (struct knot_vector *) 
		rt_nurb_kvmult(crv->knot, crv->order, value);

	oslo = ( struct oslo_mat *) 
		rt_nurb_calc_oslo( crv->order, crv->knot, new_kv);

	crv1 = (struct cnurb *) rt_malloc( sizeof( struct cnurb),
		"rt_nurb_c_split: cnurb struct");

	crv1->order  = crv->order;
	crv1->knot = (struct knot_vector *) 
		rt_nurb_kvextract(new_kv, 0, k_index + crv->order);
	crv1->mesh = (struct c_mesh *) rt_malloc( sizeof( struct c_mesh),
		"rt_nurb_c_split: crv1 c_mesh");
	crv1->mesh->pt_type = crv->mesh->pt_type;
	crv1->mesh->c_size = crv1->knot->k_size - crv1->order;
	crv1->mesh->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * crv1->mesh->c_size *
			EXTRACT_COORDS( crv1->mesh->pt_type),
			"rt_nurb_c_split: crv1 control points");

	crv2 = (struct cnurb *) rt_malloc( sizeof( struct cnurb),
		"rt_nurb_c_split: crv2 cnurb struct");

	crv2->order  = crv->order;
	crv2->knot = (struct knot_vector *) 
		rt_nurb_kvextract(new_kv, k_index, new_kv->k_size);
	crv2->mesh = (struct c_mesh *) rt_malloc( sizeof( struct c_mesh),
		"rt_nurb_split: crv2 s_mesh");
	crv2->mesh->pt_type = crv->mesh->pt_type;
	crv2->mesh->c_size = crv2->knot->k_size - crv2->order;
	crv2->mesh->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * crv2->mesh->c_size *
			EXTRACT_COORDS( crv2->mesh->pt_type),
			"rt_nurb_s_split: crv2 mesh control points");

	rt_nurb_map_oslo( oslo, crv->mesh->ctl_points, crv1->mesh->ctl_points,
		coords, coords, 0, k_index, crv->mesh->pt_type );

	rt_nurb_map_oslo( oslo, crv->mesh->ctl_points, crv2->mesh->ctl_points,
		coords, coords, k_index, new_kv->k_size - crv2->order, 
		crv2->mesh->pt_type );

	rt_nurb_free_oslo( oslo );

	crv1->next = crv2;
	return (struct cnurb *) crv1;
}
