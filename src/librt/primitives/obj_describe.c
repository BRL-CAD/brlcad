/*                O B J _ D E S C R I B E . C
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

#include "raytrace.h"


int
rt_obj_describe(struct bu_vls *logstr, const struct rt_db_internal *ip, int verbose, double mm2local, struct resource *resp, struct db_i *dbip)
{
    int id;
    const struct rt_functab *ft;

    if (!logstr || !ip)
	return -1;

    BU_CK_VLS(logstr);
    RT_CK_DB_INTERNAL(ip);
    if (resp) RT_CK_RESOURCE(resp);
    if (dbip) RT_CK_DBI(dbip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;
    if (!ft->ft_describe)
	return -4;

    return ft->ft_describe(logstr, ip, verbose, mm2local, resp, dbip);
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
