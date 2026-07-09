/*                 O B J _ M E T R I C S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include "raytrace.h"

#include "../librt_private.h"

extern void rt_crofton_volume(fastf_t *vol, const struct rt_db_internal *ip);
extern void rt_crofton_volume_implicit(fastf_t *vol, const struct rt_db_internal *ip);


int
rt_obj_volume(fastf_t *volume, const struct rt_db_internal *ip)
{
    int id;
    mat_t nonuniform_mat;
    int have_nonuniform;

    if (!volume || !ip)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;
    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -4;

    if (OBJ[id].ft_volume) {
	OBJ[id].ft_volume(volume, ip);
    } else {
	struct rt_crofton_params params = {50000u, 0.0, 0.0};
	rt_crofton_sample(NULL, volume, ip, &params);
	if (*volume <= 0.0)
	    return -3;
    }

    if (have_nonuniform > 0 && OBJ[id].ft_volume &&
	OBJ[id].ft_volume != rt_crofton_volume &&
	OBJ[id].ft_volume != rt_crofton_volume_implicit)
	*volume *= _rt_nonuniform_volume_scale(nonuniform_mat);

    return 0;
}


int
rt_obj_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    int id;
    mat_t nonuniform_mat;
    int have_nonuniform;

    if (!area || !ip)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;
    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -4;
    if (have_nonuniform > 0) {
	struct rt_crofton_params params = {50000u, 0.0, 0.0};
	rt_crofton_sample(area, NULL, ip, &params);
	if (*area <= 0.0)
	    return -5;
	return 0;
    }

    if (!OBJ[id].ft_surf_area) {
	struct rt_crofton_params params = {50000u, 0.0, 0.0};
	rt_crofton_sample(area, NULL, ip, &params);
	if (*area <= 0.0)
	    return -3;
	return 0;
    }

    OBJ[id].ft_surf_area(area, ip);
    return 0;
}


int
rt_obj_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    int id;
    mat_t nonuniform_mat;
    int have_nonuniform;

    if (!cent || !ip)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;
    if (!OBJ[id].ft_centroid)
	return -3;

    OBJ[id].ft_centroid(cent, ip);

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -4;
    if (have_nonuniform > 0) {
	point_t transformed;
	MAT4X3PNT(transformed, nonuniform_mat, *cent);
	VMOVE(*cent, transformed);
    }

    return 0;
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
