/*                           S P L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "./b_spline.h"

struct b_spline *
spl_new(int u_order, int v_order, int n_u, int n_v, int n_rows, int n_cols, int evp)
{
    struct b_spline *srf;

    srf = (struct b_spline *) bu_malloc(sizeof(struct b_spline), "spl_new: srf");

    srf->next = (struct b_spline *)0;
    srf->order[0] = u_order;
    srf->order[1] = v_order;

    srf->u_kv = (struct knot_vec *) bu_malloc(sizeof(struct knot_vec), "spl_new: srf->u_kv");
    srf->v_kv = (struct knot_vec *) bu_malloc(sizeof(struct knot_vec), "spl_new: srf->v_kv");

    srf->u_kv->k_size = n_u;
    srf->v_kv->k_size = n_v;

    srf->u_kv->knots = (fastf_t *) bu_calloc(n_u, sizeof(fastf_t), "spl_new: srf->u_kv->knots");
    srf->v_kv->knots = (fastf_t *) bu_calloc(n_v, sizeof(fastf_t), "spl_new: srf->v_kv->knots");

    srf->ctl_mesh = (struct b_mesh *) bu_malloc(sizeof(struct b_mesh), "spl_new: srf->ctl_mesh");

    srf->ctl_mesh->mesh = (fastf_t *) bu_calloc(n_rows * n_cols * evp,
						sizeof (fastf_t), "spl_new: srf->ctl_mesh->mesh");

    srf->ctl_mesh->pt_type = evp;
    srf->ctl_mesh->mesh_size[0] = n_rows;
    srf->ctl_mesh->mesh_size[1] = n_cols;

    return srf;
}


void
spl_sfree(struct b_spline * srf)
{
    bu_free((char *)srf->u_kv->knots, "spl_sfree: srf->u_kv->knots");
    bu_free((char *)srf->v_kv->knots, "spl_sfree: srf->v_kv->knots");
    bu_free((char *)srf->u_kv, "spl_sfree: srf->u_kv");
    bu_free((char *)srf->v_kv, "spl_sfree: srf->v_kv");

    bu_free((char *)srf->ctl_mesh->mesh, "spl_sfree: srf->ctl_mesh->mesh");
    bu_free((char *)srf->ctl_mesh, "spl_sfree: srf->ctl_mesh");

    bu_free((char *)srf, "spl_sfree: srf");
}


struct knot_vec *
spl_kvknot(int order, fastf_t lower, fastf_t upper, int num)
{
    int i;
    int total;
    fastf_t knot_step;
    struct knot_vec *new_knots;

    total = order * 2 + num;

    knot_step = (upper - lower) / (num + 1);

    new_knots = (struct knot_vec *) bu_malloc(sizeof(struct knot_vec), "spl_kvknot: new_knots");
    new_knots->k_size = total;

    new_knots->knots = (fastf_t *) bu_calloc(total, sizeof(fastf_t), "spl_kvknot: new_knots->knots");

    for (i = 0; i < order; i++)
	new_knots->knots[i] = lower;

    for (i = order; i <= (num + order -1); i++)
	new_knots->knots[i] = new_knots->knots[i-1] + knot_step;

    for (i = num + order; i < total; i++)
	new_knots->knots[i] = upper;

    return new_knots;
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
