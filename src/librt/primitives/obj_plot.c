/*                    O B J _ P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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

#include "bu.h"
#include "bn.h"
#include "raytrace.h"


int
rt_obj_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    int id;
    const struct rt_functab *ft;

    if (!vhead || !ip)
	return -1;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    if (ttol) RT_CK_TESS_TOL(ttol);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;
    if (!ft->ft_plot)
	return -4;

    return ft->ft_plot(vhead, ip, ttol, tol, NULL);
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
