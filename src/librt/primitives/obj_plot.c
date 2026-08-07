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


int
rt_obj_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    return rt_obj_plot_view(vhead, ip, ttol, tol, NULL);
}


int
rt_obj_plot_view(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *view)
{
    int id;
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    struct bg_tess_tol body_ttol;
    struct bn_tol body_tol;
    const struct bg_tess_tol *plot_ttol = ttol;
    const struct bn_tol *plot_tol = tol;
    struct bu_list plotted;
    int have_nonuniform;
    int ret;

    if (!vhead || !ip)
	return -1;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    if (ttol) BG_CK_TESS_TOL(ttol);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_plot)
	return -4;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -5;

    if (have_nonuniform > 0) {
	_rt_nonuniform_tolerances(ttol ? &body_ttol : NULL, tol ? &body_tol : NULL,
		ttol, tol, nonuniform_mat);
	if (ttol) plot_ttol = &body_ttol;
	if (tol) plot_tol = &body_tol;
    }

    BU_LIST_INIT(&plotted);
    ret = ft->ft_plot(&plotted, ip, plot_ttol, plot_tol, view);
    if (ret == 0) {
	if (have_nonuniform > 0)
	    _rt_nonuniform_transform_vlist(&plotted, nonuniform_mat);
	BU_LIST_APPEND_LIST(vhead, &plotted);
    } else if (BU_LIST_NON_EMPTY(&plotted)) {
	BV_FREE_VLIST(&rt_vlfree, &plotted);
    }

    return ret;
}


int
rt_obj_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bview *view, fastf_t s_size)
{
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    struct bn_tol body_tol;
    const struct bn_tol *plot_tol = tol;
    struct bu_list plotted;
    int have_nonuniform;
    int id;
    int ret;

    if (!vhead || !ip || !view)
	return -1;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;
    ft = &OBJ[id];
    if (!ft->ft_adaptive_plot)
	return -4;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -5;
    if (have_nonuniform > 0) {
	_rt_nonuniform_tolerances(NULL, tol ? &body_tol : NULL, NULL, tol, nonuniform_mat);
	if (tol) plot_tol = &body_tol;
	if (s_size > 0.0)
	    s_size /= _rt_nonuniform_max_stretch(nonuniform_mat);
    }

    BU_LIST_INIT(&plotted);
    ret = ft->ft_adaptive_plot(&plotted, ip, plot_tol, view, s_size);
    if (ret == 0) {
	if (have_nonuniform > 0)
	    _rt_nonuniform_transform_vlist(&plotted, nonuniform_mat);
	BU_LIST_APPEND_LIST(vhead, &plotted);
    } else if (BU_LIST_NON_EMPTY(&plotted)) {
	BV_FREE_VLIST(&rt_vlfree, &plotted);
    }

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
