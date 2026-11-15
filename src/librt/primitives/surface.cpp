/*                        S U R F A C E . C P P
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

#include <cmath>

#include "raytrace.h"
#include "rt/surface.h"

int
rt_obj_surface_attributes(struct rt_surface_attributes *out,
    struct application *ap, struct soltab *solid, const struct hit *hit, int flip)
{
    if (!out || !ap || !solid || !hit)
	return -1;
    RT_CK_APPLICATION(ap);
    RT_CK_SOLTAB(solid);
    RT_CK_HIT(hit);
    struct rt_surface_attributes result = {};
    struct xray *ray = hit->hit_rayp ? hit->hit_rayp : &ap->a_ray;
    if (rt_obj_normals(hit, solid, ray, result.geometric_normal, result.shading_normal) != 0)
	return -1;
    VJOIN1(result.point, ray->r_pt, hit->hit_dist, ray->r_dir);
    if (VINVALID(result.point))
	return -1;
    if (solid->st_meth->ft_uv) {
	struct hit local = *hit;
	VMOVE(local.hit_point, result.point);
	VMOVE(local.hit_normal, result.shading_normal);
	struct uvcoord uv = {};
	if (rt_obj_uv(ap, solid, &local, &uv) != 0 ||
	    !std::isfinite(uv.uv_u) || !std::isfinite(uv.uv_v))
	    return -1;
	result.uv[0] = uv.uv_u;
	result.uv[1] = uv.uv_v;
	result.uv_available = 1;
    }
    const int status = rt_obj_surface_derivatives(ap, solid, hit, &result.derivatives);
    if (status == RT_DERIVATIVES_ERROR)
	return -1;
    result.derivatives_available = status == RT_DERIVATIVES_OK;
    if (flip) {
	VREVERSE(result.geometric_normal, result.geometric_normal);
	VREVERSE(result.shading_normal, result.shading_normal);
	VREVERSE(result.derivatives.dndu, result.derivatives.dndu);
	VREVERSE(result.derivatives.dndv, result.derivatives.dndv);
    }
    *out = result;
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
