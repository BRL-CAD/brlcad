/*                    O B J _ T E S S . C
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
rt_obj_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    int id;
    const struct rt_functab *ft;
    mat_t nonuniform_mat;
    struct bg_tess_tol body_ttol;
    struct bn_tol body_tol;
    const struct bg_tess_tol *tess_ttol = ttol;
    const struct bn_tol *tess_tol = tol;
    int have_nonuniform;
    int ret;

    if (!r || !ip)
	return -1;

    if (*r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);
    RT_CK_DB_INTERNAL(ip);
    if (ttol) BG_CK_TESS_TOL(ttol);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_tessellate)
	return -4;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -5;

    if (have_nonuniform > 0) {
	_rt_nonuniform_tolerances(ttol ? &body_ttol : NULL, tol ? &body_tol : NULL,
		ttol, tol, nonuniform_mat);
	if (ttol) tess_ttol = &body_ttol;
	if (tol) tess_tol = &body_tol;
    }

    ret = ft->ft_tessellate(r, m, ip, tess_ttol, tess_tol);
    if (ret == 0 && have_nonuniform > 0 && r && *r)
	_rt_nonuniform_transform_nmgregion(*r, nonuniform_mat, tol);

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
