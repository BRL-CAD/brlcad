/*                    O B J _ P L O T . C
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


#include "bn.h"
#include "raytrace.h"

#include "../librt_private.h"


static int
obj_plot_init(const struct rt_functab **ft, mat_t nonuniform_mat, struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol)
{
    int id;
    int have_nonuniform;

    if (!ft || !vhead || !ip)
	return -1;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0 || id > ID_MAXIMUM)
	return -2;

    *ft = &OBJ[id];

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -5;

    return have_nonuniform;
}


int
rt_obj_plot_view(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *v)
{
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    int have_nonuniform;
    int ret;

    have_nonuniform = obj_plot_init(&ft, nonuniform_mat, vhead, ip, tol);
    if (have_nonuniform < 0)
	return have_nonuniform;
    if (ttol) BG_CK_TESS_TOL(ttol);
    if (!ft->ft_plot)
	return -4;

    ret = ft->ft_plot(vhead, ip, ttol, tol, v);
    if (ret == 0 && have_nonuniform > 0)
	_rt_nonuniform_transform_vlist(vhead, nonuniform_mat);

    return ret;
}


int
rt_obj_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    return rt_obj_plot_view(vhead, ip, ttol, tol, NULL);
}


int
rt_obj_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bview *v, fastf_t scale)
{
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    int have_nonuniform;
    int ret;

    have_nonuniform = obj_plot_init(&ft, nonuniform_mat, vhead, ip, tol);
    if (have_nonuniform < 0)
	return have_nonuniform;
    if (!ft->ft_adaptive_plot)
	return -4;

    ret = ft->ft_adaptive_plot(vhead, ip, tol, v, scale);
    if (ret == 0 && have_nonuniform > 0)
	_rt_nonuniform_transform_vlist(vhead, nonuniform_mat);

    return ret;
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
