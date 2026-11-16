/*                        O B J _ U V . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2026 United States Government as represented by
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


int
rt_obj_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    int id;
    const struct rt_functab *ft;

    if (!stp || !hitp || !uvp)
	return -1;

    RT_CK_SOLTAB(stp);
    RT_CK_HIT(hitp);
    if (ap) RT_CK_APPLICATION(ap);

    id = stp->st_id;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_uv)
	return -4;

    ft->ft_uv(ap, stp, hitp, uvp);
    return 0;
}


int
rt_obj_surface_derivatives(struct application *ap, struct soltab *stp,
    const struct hit *hitp, struct rt_surface_derivatives *derivatives)
{
    if (!ap || !stp || !hitp || !derivatives)
	return RT_DERIVATIVES_ERROR;
    RT_CK_APPLICATION(ap);
    RT_CK_SOLTAB(stp);
    RT_CK_HIT(hitp);
    RT_CK_FUNCTAB(stp->st_meth);
    if (!stp->st_meth->ft_surface_derivatives)
	return RT_DERIVATIVES_UNAVAILABLE;

    struct hit local = *hitp;
    const struct xray *ray = hitp->hit_rayp ? hitp->hit_rayp : &ap->a_ray;
    RT_CK_RAY(ray);
    VJOIN1(local.hit_point, ray->r_pt, local.hit_dist, ray->r_dir);
    struct rt_surface_derivatives result = RT_SURFACE_DERIVATIVES_INIT_ZERO;
    const int status = stp->st_meth->ft_surface_derivatives(ap, stp, &local, &result);
    if (status != RT_DERIVATIVES_OK)
	return status == RT_DERIVATIVES_UNAVAILABLE ? status : RT_DERIVATIVES_ERROR;
    if (VINVALID(result.dpdu) || VINVALID(result.dpdv) ||
	VINVALID(result.dndu) || VINVALID(result.dndv))
	return RT_DERIVATIVES_ERROR;
    *derivatives = result;
    return RT_DERIVATIVES_OK;
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
