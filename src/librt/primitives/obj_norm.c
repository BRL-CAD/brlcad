/*                    O B J _ N O R M . C
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
rt_obj_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    int id;
    const struct rt_functab *ft;

    if (!hitp || !stp)
	return -1;

    RT_CK_SOLTAB(stp);
    RT_CK_HIT(hitp);
    if (rp) RT_CK_RAY(rp);

    id = stp->st_id;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_norm)
	return -4;

    ft->ft_norm(hitp, stp, rp);
    return 0;
}


int
rt_obj_normals(const struct hit *hitp, struct soltab *stp, struct xray *rp,
    vect_t geometric, vect_t shading)
{
    if (!hitp || !stp || !rp || !geometric || !shading || geometric == shading)
	return -1;
    RT_CK_SOLTAB(stp);
    RT_CK_HIT(hitp);
    RT_CK_RAY(rp);
    RT_CK_FUNCTAB(stp->st_meth);

    const struct rt_functab *ft = stp->st_meth;
    vect_t geometric_result, shading_result;
    if (ft->ft_normals) {
	if (ft->ft_normals(hitp, stp, rp, geometric_result, shading_result) < 0)
	    return -2;
    } else if (ft->ft_norm) {
	struct hit local = *hitp;
	ft->ft_norm(&local, stp, rp);
	VMOVE(geometric_result, local.hit_normal);
	VMOVE(shading_result, local.hit_normal);
    } else {
	return -3;
    }
    const fastf_t geometric_length = MAGNITUDE(geometric_result);
    const fastf_t shading_length = MAGNITUDE(shading_result);
    if (!isfinite(geometric_length) || !isfinite(shading_length) ||
	!(geometric_length > SMALL_FASTF) || !(shading_length > SMALL_FASTF))
	return -4;
    VSCALE(geometric, geometric_result, 1.0 / geometric_length);
    VSCALE(shading, shading_result, 1.0 / shading_length);
    if (VDOT(shading, geometric) < 0.0)
	VREVERSE(shading, shading);
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
