/*
 *			N U R B _ S P L I T . C
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

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/* Algorithm
 *
 * 	Given a parametric direction (u or v) look at the direction 
 * knot vector and insert a multiple knot of parametric direction surface
 * order. This is somewhat different than rt_nurb_split in that the surface is 
 * give a parametric value at which to split the surface. 
 * rt_nurb_kvmult does the right thing in inserting a multiple knot with the 
 * correct amount. Separate the surface and return the two resulting surface.
 */

struct face_g_snurb *
rt_nurb_s_xsplit( srf, param, dir)
struct face_g_snurb * srf;
fastf_t param;
int dir;
{
	struct knot_vector new_kv;
	struct oslo_mat * oslo;
	int i;
	int k_index;
	struct face_g_snurb * srf1, * srf2;

	NMG_CK_SNURB(srf);

	if ( dir == RT_NURB_SPLIT_ROW )
	{
		rt_nurb_kvmult(&new_kv, &srf->u, srf->order[0], param, (struct resource *)NULL);

		k_index = srf->order[0];

		oslo = ( struct oslo_mat *) 
			rt_nurb_calc_oslo( srf->order[RT_NURB_SPLIT_ROW], &srf->u, &new_kv, (struct resource *)NULL);

		GET_SNURB(srf1);
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = RT_NURB_SPLIT_ROW;
		rt_nurb_kvextract(&srf1->u, &new_kv, 0, k_index + srf1->order[0], (struct resource *)NULL);
		rt_nurb_kvcopy(&srf1->v, &srf->v, (struct resource *)NULL);
		
		srf1->pt_type = srf->pt_type;
		srf1->s_size[0] = srf1->v.k_size - 
			srf1->order[1];
		srf1->s_size[1] = srf1->u.k_size - 
			srf1->order[0];

		srf1->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf1->s_size[0] *
				srf1->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf1->pt_type),
				"rt_nurb_s_xsplit: srf1 row mesh control points");

		GET_SNURB(srf2);
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = RT_NURB_SPLIT_ROW;
		rt_nurb_kvextract(&srf2->u, &new_kv, k_index, new_kv.k_size, (struct resource *)NULL);
		rt_nurb_kvcopy(&srf2->v, &srf->v, (struct resource *)NULL);
		
		srf2->pt_type = srf->pt_type;
		srf2->s_size[0] = srf2->v.k_size - 
			srf2->order[1];
		srf2->s_size[1] = srf2->u.k_size - 
			srf2->order[0];

		srf2->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf2->s_size[0] *
				srf2->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf2->pt_type),
				"rt_nurb_s_xsplit: srf2 row mesh control points");

		for( i = 0; i < srf->s_size[0]; i++)
		{
			fastf_t * old_mesh_ptr;
			fastf_t * new_mesh_ptr;

			old_mesh_ptr = &srf->ctl_points[
				i * srf->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf->pt_type)];
			new_mesh_ptr = &srf1->ctl_points[
				i * srf1->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf1->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				RT_NURB_EXTRACT_COORDS( srf->pt_type ),
				RT_NURB_EXTRACT_COORDS( srf1->pt_type ),
				0, k_index, srf1->pt_type);
			new_mesh_ptr = &srf2->ctl_points[
				i * srf2->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf2->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				RT_NURB_EXTRACT_COORDS( srf->pt_type ),
				RT_NURB_EXTRACT_COORDS( srf2->pt_type ),
				k_index, new_kv.k_size - srf2->order[0], 
				srf2->pt_type);
		}
	}
	else 
	{
		rt_nurb_kvmult(&new_kv, &srf->v, srf->order[RT_NURB_SPLIT_COL], param, (struct resource *)NULL);

		k_index = srf->order[1];

		oslo = ( struct oslo_mat *) 
			rt_nurb_calc_oslo( srf->order[RT_NURB_SPLIT_COL], &srf->v, &new_kv, (struct resource *)NULL);

		GET_SNURB(srf1);
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = RT_NURB_SPLIT_COL;
		rt_nurb_kvextract(&srf1->v, &new_kv, 0, k_index + srf1->order[RT_NURB_SPLIT_COL], (struct resource *)NULL);
		rt_nurb_kvcopy(&srf1->u, &srf->u, (struct resource *)NULL);
		
		srf1->pt_type = srf->pt_type;
		srf1->s_size[0] = srf1->v.k_size - 
			srf1->order[1];
		srf1->s_size[1] = srf1->u.k_size - 
			srf1->order[0];

		srf1->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf1->s_size[0] *
				srf1->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf1->pt_type),
				"rt_nurb_split: srf1 row mesh control points");

		GET_SNURB(srf2);
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = RT_NURB_SPLIT_COL;
		rt_nurb_kvextract(&srf2->v, &new_kv, k_index, new_kv.k_size, (struct resource *)NULL);
		rt_nurb_kvcopy(&srf2->u, &srf->u, (struct resource *)NULL);
		
		srf2->pt_type = srf->pt_type;
		srf2->s_size[0] = srf2->v.k_size - 
			srf2->order[1];
		srf2->s_size[1] = srf2->u.k_size - 
			srf2->order[0];

		srf2->ctl_points = (fastf_t *)
			rt_malloc( sizeof(fastf_t) * srf2->s_size[0] *
				srf2->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf2->pt_type),
				"rt_nurb_s_xsplit: srf2 row mesh control points");

		for( i = 0; i < srf->s_size[1]; i++)
		{
			fastf_t * old_mesh_ptr;
			fastf_t * new_mesh_ptr;

			old_mesh_ptr = &srf->ctl_points[
				i * RT_NURB_EXTRACT_COORDS( srf->pt_type)];
			new_mesh_ptr = &srf1->ctl_points[
				i * RT_NURB_EXTRACT_COORDS( srf1->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				srf->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf->pt_type ),
				srf1->s_size[1] * 
				RT_NURB_EXTRACT_COORDS( srf1->pt_type ),
				0, k_index, srf1->pt_type);
			new_mesh_ptr = &srf2->ctl_points[
				i * RT_NURB_EXTRACT_COORDS( srf2->pt_type)];
			rt_nurb_map_oslo( oslo, old_mesh_ptr, new_mesh_ptr,
				srf->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf->pt_type ),
				srf2->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf2->pt_type ),
				k_index, new_kv.k_size - srf2->order[1], 
				srf2->pt_type);
		}
	}
	
	BU_LIST_APPEND( &srf1->l, &srf2->l );

	rt_free( (char *) new_kv.knots, "rt_nurb_s_xsplit: new_kv.knots");

	rt_nurb_free_oslo(oslo, (struct resource *)NULL);

	return srf1;
}

/* rt_nurb_c_xsplit()
 * Split a NURB curve by inserting a multiple knot and return
 * the result of the two curves.
 *
 * Algorithm
 *
 * Insert a multiple knot of the curve order. A parameter is give for the
 * knot value for which the curve will be split.
 */

struct edge_g_cnurb *
rt_nurb_c_xsplit( crv, param)
struct edge_g_cnurb * crv;
fastf_t param;
{
	struct knot_vector new_kv;
	struct oslo_mat * oslo;
	int k_index;
	struct edge_g_cnurb * crv1, * crv2;
	int coords;

	NMG_CK_CNURB(crv);

	coords = RT_NURB_EXTRACT_COORDS( crv->pt_type ),

	k_index = crv->order;
	rt_nurb_kvmult(&new_kv, &crv->k, crv->order, param, (struct resource *)NULL);

	oslo = ( struct oslo_mat *) 
		rt_nurb_calc_oslo( crv->order, &crv->k, &new_kv, (struct resource *)NULL);

	GET_CNURB(crv1);
	crv1->order  = crv->order;
	rt_nurb_kvextract(&crv1->k, &new_kv, 0, k_index + crv->order, (struct resource *)NULL);
	crv1->pt_type = crv->pt_type;
	crv1->c_size = crv1->k.k_size - crv1->order;
	crv1->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * crv1->c_size *
			RT_NURB_EXTRACT_COORDS( crv1->pt_type),
			"rt_nurb_c_xsplit: crv1 control points");

	GET_CNURB(crv2);
	crv2->order  = crv->order;
	rt_nurb_kvextract(&crv2->k, &new_kv, k_index, new_kv.k_size, (struct resource *)NULL);
	crv2->pt_type = crv->pt_type;
	crv2->c_size = crv2->k.k_size - crv2->order;
	crv2->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * crv2->c_size *
			RT_NURB_EXTRACT_COORDS( crv2->pt_type),
			"rt_nurb_c_xsplit: crv2 row mesh control points");

	rt_nurb_map_oslo( oslo, crv->ctl_points, crv1->ctl_points,
		coords, coords, 0, k_index, crv->pt_type );

	rt_nurb_map_oslo( oslo, crv->ctl_points, crv2->ctl_points,
		coords, coords, k_index, new_kv.k_size - crv2->order, 
		crv2->pt_type );

	rt_nurb_free_oslo( oslo, (struct resource *)NULL );

	rt_free( (char *) new_kv.knots, "rt_nurb_c_xsplit: new_kv.knots");

	BU_LIST_APPEND( &crv1->l, &crv2->l );
	return crv1;
}
