/*                    O B J _ P R E P . C
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
#include "rt/func.h"

#include "../librt_private.h"


int
rt_obj_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    int id;
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    int have_nonuniform = 0;
    int ret;

    if (!stp)
	return -1;

    RT_CK_SOLTAB(stp);
    if (ip)
	RT_CK_DB_INTERNAL(ip);
    if (rtip)
	RT_CK_RTI(rtip);

    id = stp->st_id;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_prep)
	return -4;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -5;

    ret = ft->ft_prep(stp, ip, rtip);
    if (ret)
	return ret;

    if (have_nonuniform > 0) {
	if (_rt_nonuniform_soltab_setup(stp, nonuniform_mat, rtip ? &rtip->rti_tol : NULL) < 0)
	    return -6;
    }

    return 0;
}


int
rt_obj_prep_serialize(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version)
{
    const struct rt_functab *ft;

    if (!stp)
	return -1;

    RT_CK_SOLTAB(stp);
    if (ip)
	RT_CK_DB_INTERNAL(ip);
    if (external)
	BU_CK_EXTERNAL(external);

    if (stp->st_id < 0)
	return -2;

    ft = &OBJ[stp->st_id];

    if (!ft)
	return -3;

    if (!ft->ft_prep_serialize)
	return -4;

    return ft->ft_prep_serialize(stp, ip, external, version);
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
