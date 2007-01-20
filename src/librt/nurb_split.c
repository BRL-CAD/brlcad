/*                    N U R B _ S P L I T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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

/** @addtogroup nurb */
/*@{*/
/** @file nurb_split.c
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
 */
/*@}*/

#include "common.h"



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
 * order.  If internal knot values exist than pick the one closest to the
 * middle and add additional knots to split at that value, otherwise
 * add multiple knots at the mid point of the knot vector. Use the new
 * knot vector to pass to the oslo refinement process and split the surface.
 * Separate the surface and return the two resulting surface.
 *
 *  The original surface is undisturbed by this operation.
 */
void
rt_nurb_s_split(struct bu_list *split_hd, const struct face_g_snurb *srf, int dir, struct resource *res)
{
	struct knot_vector new_kv;
	fastf_t value;
	struct oslo_mat * oslo;
	int i;
	int k_index = 0;
	struct face_g_snurb * srf1, * srf2;

	NMG_CK_SNURB(srf);

	if ( dir == RT_NURB_SPLIT_ROW )
	{
		value = srf->u.knots[(srf->u.k_size -1)/2];

		for( i = 0; i < srf->u.k_size; i++)
			if( value == srf->u.knots[i] )
			{
				k_index = i;
				break;
			}
		if ( k_index == 0)
		{
			value = ( value +
				srf->u.knots[ srf->u.k_size -1])
				/2.0;
			k_index = srf->order[0];
		}

		rt_nurb_kvmult( &new_kv, &srf->u, srf->order[0], value, res);

		oslo = ( struct oslo_mat *)
			rt_nurb_calc_oslo( srf->order[RT_NURB_SPLIT_ROW], &srf->u, &new_kv, res);

		GET_SNURB( srf1 );
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = RT_NURB_SPLIT_ROW;
		rt_nurb_kvextract(&srf1->u, &new_kv, 0, k_index + srf1->order[0], res);
		rt_nurb_kvcopy(&srf1->v, &srf->v, res);

		srf1->pt_type = srf->pt_type;
		srf1->s_size[0] = srf1->v.k_size -
			srf1->order[1];
		srf1->s_size[1] = srf1->u.k_size -
			srf1->order[0];

		srf1->ctl_points = (fastf_t *)
			bu_malloc( sizeof(fastf_t) * srf1->s_size[0] *
				srf1->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf1->pt_type),
				"rt_nurb_s_split: srf1 row mesh control points");

		GET_SNURB( srf2 );
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = RT_NURB_SPLIT_ROW;
		rt_nurb_kvextract(&srf2->u, &new_kv, k_index, new_kv.k_size, res);
		rt_nurb_kvcopy(&srf2->v, &srf->v, res);

		srf2->pt_type = srf->pt_type;
		srf2->s_size[0] = srf2->v.k_size -
			srf2->order[1];
		srf2->s_size[1] = srf2->u.k_size -
			srf2->order[0];

		srf2->ctl_points = (fastf_t *)
			bu_malloc( sizeof(fastf_t) * srf2->s_size[0] *
				srf2->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf2->pt_type),
				"rt_nurb_s_split: srf2 row mesh control points");

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
		value = srf->v.knots[(srf->v.k_size -1)/2];

		for( i = 0; i < srf->v.k_size; i++)
			if( value == srf->v.knots[i] )
			{
				k_index = i;
				break;
			}
		if ( k_index == 0)
		{
			value = ( value +
				srf->v.knots[ srf->v.k_size -1])
				/2.0;
			k_index = srf->order[1];
		}

		rt_nurb_kvmult(&new_kv, &srf->v, srf->order[RT_NURB_SPLIT_COL], value, res);

		oslo = ( struct oslo_mat *)
			rt_nurb_calc_oslo( srf->order[RT_NURB_SPLIT_COL], &srf->v, &new_kv, res);

		GET_SNURB( srf1 );
		srf1->order[0]  = srf->order[0];
		srf1->order[1]  = srf->order[1];
		srf1->dir = RT_NURB_SPLIT_COL;
		rt_nurb_kvextract(&srf1->v, &new_kv, 0, k_index + srf1->order[RT_NURB_SPLIT_COL], res);
		rt_nurb_kvcopy(&srf1->u, &srf->u, res);

		srf1->pt_type = srf->pt_type;
		srf1->s_size[0] = srf1->v.k_size -
			srf1->order[1];
		srf1->s_size[1] = srf1->u.k_size -
			srf1->order[0];

		srf1->ctl_points = (fastf_t *)
			bu_malloc( sizeof(fastf_t) * srf1->s_size[0] *
				srf1->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf1->pt_type),
				"rt_nurb_s_split: srf1 col mesh control points");

		GET_SNURB( srf2 );
		srf2->order[0]  = srf->order[0];
		srf2->order[1]  = srf->order[1];
		srf2->dir = RT_NURB_SPLIT_COL;
		rt_nurb_kvextract(&srf2->v, &new_kv, k_index, new_kv.k_size, res);
		rt_nurb_kvcopy(&srf2->u, &srf->u, res);

		srf2->pt_type = srf->pt_type;
		srf2->s_size[0] = srf2->v.k_size -
			srf2->order[1];
		srf2->s_size[1] = srf2->u.k_size -
			srf2->order[0];

		srf2->ctl_points = (fastf_t *)
			bu_malloc( sizeof(fastf_t) * srf2->s_size[0] *
				srf2->s_size[1] *
				RT_NURB_EXTRACT_COORDS( srf2->pt_type),
				"rt_nurb_s_split: srf2 col mesh control points");

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

	/* Arrangement will be:  head, srf1, srf2 */
	BU_LIST_APPEND( split_hd, &srf2->l );
	BU_LIST_APPEND( split_hd, &srf1->l );

	rt_nurb_free_oslo(oslo, res);
	bu_free( (char *)new_kv.knots, "rt_nurb_s_split: new kv knots");

}

/*
 *			R T _ N U R B _ C _ S P L I T
 *
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
 *
 *  The original curve is undisturbed by this operation.
 */
void
rt_nurb_c_split(struct bu_list *split_hd, const struct edge_g_cnurb *crv)
{
	struct knot_vector new_kv;
	fastf_t value;
	struct oslo_mat * oslo;
	int i;
	int k_index = 0;
	struct edge_g_cnurb * crv1, * crv2;
	int coords;

	NMG_CK_CNURB(crv);

	coords = RT_NURB_EXTRACT_COORDS( crv->pt_type ),

	value = crv->k.knots[(crv->k.k_size -1)/2];

	for( i = 0; i < crv->k.k_size; i++)
		if( value == crv->k.knots[i] )
		{
			k_index = i;
			break;
		}
	if ( k_index == 0)
	{
		value = ( value +
			crv->k.knots[ crv->k.k_size -1])
			/2.0;
		k_index = crv->order;
	}

	rt_nurb_kvmult(&new_kv, &crv->k, crv->order, value, (struct resource *)NULL);

	oslo = ( struct oslo_mat *)
		rt_nurb_calc_oslo( crv->order, &crv->k, &new_kv, (struct resource *)NULL);

	GET_CNURB( crv1 );
	crv1->order  = crv->order;
	rt_nurb_kvextract(&crv1->k, &new_kv, 0, k_index + crv->order, (struct resource *)NULL);
	crv1->pt_type = crv->pt_type;
	crv1->c_size = crv1->k.k_size - crv1->order;
	crv1->ctl_points = (fastf_t *)
		bu_malloc( sizeof(fastf_t) * crv1->c_size *
			RT_NURB_EXTRACT_COORDS( crv1->pt_type),
			"rt_nurb_c_split: crv1 control points");

	GET_CNURB( crv2 );
	crv2->order  = crv->order;
	rt_nurb_kvextract(&crv2->k, &new_kv, k_index, new_kv.k_size, (struct resource *)NULL);
	crv2->pt_type = crv->pt_type;
	crv2->c_size = crv2->k.k_size - crv2->order;
	crv2->ctl_points = (fastf_t *)
		bu_malloc( sizeof(fastf_t) * crv2->c_size *
			RT_NURB_EXTRACT_COORDS( crv2->pt_type),
			"rt_nurb_s_split: crv2 mesh control points");

	rt_nurb_map_oslo( oslo, crv->ctl_points, crv1->ctl_points,
		coords, coords, 0, k_index, crv->pt_type );

	rt_nurb_map_oslo( oslo, crv->ctl_points, crv2->ctl_points,
		coords, coords, k_index, new_kv.k_size - crv2->order,
		crv2->pt_type );

	rt_nurb_free_oslo( oslo, (struct resource *)NULL );

	bu_free( (char *) new_kv.knots, "rt_nurb_c_split; new_kv.knots" );

	/* Arrangement will be:  head, crv1, crv2 */
	BU_LIST_APPEND( split_hd, &crv2->l );
	BU_LIST_APPEND( split_hd, &crv1->l );
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
