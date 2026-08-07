/*                    O B J _ B B O X . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "raytrace.h"

#include "../librt_private.h"


int
rt_obj_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *tol)
{
    mat_t nonuniform_mat;
    struct bn_tol body_tol;
    const struct bn_tol *bbox_tol = tol;
    int have_nonuniform;
    int id;
    int ret;

    if (!ip || !min || !max)
	return -1;
    RT_CK_DB_INTERNAL(ip);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0 || !OBJ[id].ft_bbox)
	return -2;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -3;
    if (have_nonuniform > 0 && tol) {
	_rt_nonuniform_tolerances(NULL, &body_tol, NULL, tol, nonuniform_mat);
	bbox_tol = &body_tol;
    }

    ret = OBJ[id].ft_bbox(ip, min, max, bbox_tol);
    if (ret)
	return ret;

    if (have_nonuniform > 0) {
	point_t transformed_min, transformed_max;
	_rt_nonuniform_transform_bbox(&transformed_min, &transformed_max,
		nonuniform_mat, *min, *max);
	VMOVE(*min, transformed_min);
	VMOVE(*max, transformed_max);
    }

    return 0;
}
