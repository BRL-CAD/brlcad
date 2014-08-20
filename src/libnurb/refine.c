/*                   N U R B _ R E F I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file primitives/bspline/nurb_refine.c
 *
 * Refines a surface by inserting knots with respect to the row or
 * columns of the control points resulting in the same surface with new
 * control points.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "nurb.h"


/**
 * Returns a refined surface.  The original surface is unmodified.
 */
struct face_g_snurb *
nurb_s_refine(const struct face_g_snurb *srf, int dir, struct knot_vector *kv, struct resource *res)
    /* Old surface to be refined */
    /* Direction to refine */
    /* Row = 0, Col = 1 */
    /* New knot vector */

{
    register struct face_g_snurb * nurb_srf;
    struct oslo_mat *oslo;	/* oslo refinement matrix */
    int i;

    NMG_CK_SNURB(srf);

    if (dir == NURB_SPLIT_ROW) {
	/* Row (u) direction */
	GET_SNURB(nurb_srf);
	nurb_srf->order[0] = srf->order[0];
	nurb_srf->order[1] = srf->order[1];

	nurb_kvcopy(&nurb_srf->u, kv, res);
	nurb_kvcopy(&nurb_srf->v, &srf->v, res);

	nurb_srf->s_size[0] = srf->s_size[0];
	nurb_srf->s_size[1] = kv->k_size - srf->order[0];
	nurb_srf->pt_type = srf->pt_type;
	nurb_srf->ctl_points = (fastf_t *)
	    bu_malloc(sizeof (fastf_t) *
		      nurb_srf->s_size[0] *
		      nurb_srf->s_size[1] *
		      NURB_EXTRACT_COORDS(nurb_srf->pt_type),
		      "nurb_s_refine: row mesh control points");

	oslo = (struct oslo_mat *)
	    nurb_calc_oslo (srf -> order[NURB_SPLIT_ROW], &srf->u, kv, res);

	for (i = 0; i < nurb_srf->s_size[0]; i++) {
	    fastf_t * old_mesh_ptr;
	    fastf_t * new_mesh_ptr;

	    old_mesh_ptr = &srf->ctl_points[
		i * srf->s_size[1] *
		NURB_EXTRACT_COORDS(srf->pt_type)];
	    new_mesh_ptr = &nurb_srf->ctl_points[
		i * nurb_srf->s_size[1] *
		NURB_EXTRACT_COORDS(nurb_srf->pt_type)];
	    nurb_map_oslo(oslo, old_mesh_ptr, new_mesh_ptr,
			  NURB_EXTRACT_COORDS(srf->pt_type),
			  NURB_EXTRACT_COORDS(nurb_srf->pt_type),
			  0, kv->k_size - nurb_srf->order[0],
			  nurb_srf->pt_type);
	}

	nurb_free_oslo(oslo, res);

    } else {
	/* Col (v) direction */
	GET_SNURB(nurb_srf);
	nurb_srf->order[0] = srf->order[0];
	nurb_srf->order[1] = srf->order[1];

	nurb_kvcopy(&nurb_srf->u, &srf->u, res);
	nurb_kvcopy(&nurb_srf->v, kv, res);

	nurb_srf->s_size[0] = kv->k_size - srf->order[1];
	nurb_srf->s_size[1] = srf->s_size[1];

	nurb_srf->pt_type = srf->pt_type;
	nurb_srf->ctl_points = (fastf_t *)
	    bu_malloc(sizeof (fastf_t) *
		      nurb_srf->s_size[0] *
		      nurb_srf->s_size[1] *
		      NURB_EXTRACT_COORDS(nurb_srf->pt_type),
		      "nurb_s_refine: row mesh control points");

	oslo = (struct oslo_mat *)
	    nurb_calc_oslo (srf->order[NURB_SPLIT_COL], &srf->v, kv, res);

	for (i = 0; i < nurb_srf->s_size[1]; i++) {
	    fastf_t * old_mesh_ptr;
	    fastf_t * new_mesh_ptr;

	    old_mesh_ptr = &srf->ctl_points[
		i * NURB_EXTRACT_COORDS(srf->pt_type)];
	    new_mesh_ptr = &nurb_srf->ctl_points[
		i * NURB_EXTRACT_COORDS(nurb_srf->pt_type)];
	    nurb_map_oslo(oslo, old_mesh_ptr, new_mesh_ptr,
			  srf->s_size[1] *
			  NURB_EXTRACT_COORDS(srf->pt_type),
			  nurb_srf->s_size[1] *
			  NURB_EXTRACT_COORDS(nurb_srf->pt_type),
			  0, kv->k_size - nurb_srf->order[1],
			  nurb_srf->pt_type);
	}
	nurb_free_oslo(oslo, res);
    }
    return nurb_srf;
}


struct edge_g_cnurb *
nurb_c_refine(const struct edge_g_cnurb *crv, struct knot_vector *kv)
{
    struct oslo_mat * oslo;
    struct edge_g_cnurb * new_crv;
    int i, coords;

    NMG_CK_CNURB(crv);

    coords = NURB_EXTRACT_COORDS(crv->pt_type);

    new_crv = (struct edge_g_cnurb *) nurb_new_cnurb(
	crv->order, kv->k_size, kv->k_size - crv->order,
	crv->pt_type);

    oslo = (struct oslo_mat *) nurb_calc_oslo(
	crv->order, &crv->k, kv, (struct resource *)NULL);

    nurb_map_oslo(oslo, crv->ctl_points,
		  new_crv->ctl_points,
		  coords, coords, 0,
		  kv->k_size - new_crv->order,
		  new_crv->pt_type);

    new_crv->k.k_size = kv->k_size;

    for (i = 0; i < kv->k_size; i++)
	new_crv->k.knots[i] = kv->knots[i];

    nurb_free_oslo(oslo, (struct resource *)NULL);

    return new_crv;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
