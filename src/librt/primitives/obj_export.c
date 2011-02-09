/*                  O B J _ E X P O R T . C
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

#include "bn.h"
#include "raytrace.h"


int
rt_obj_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp)
{
    int id;
    const struct rt_functab *ft;
    int (*export)(struct bu_external *, const struct rt_db_internal *, double, const struct db_i *, struct resource *);

    if (!ep || !ip || !dbip || local2mm < 0.0)
	return -1;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_DBI(dbip);
    if (resp) RT_CK_RESOURCE(resp);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;

    if (dbip->dbi_version < 5) {
	export = ft->ft_export4;
    } else {
	export = ft->ft_export5;
    }

    if (!export)
	return -4;

    return export(ep, ip, local2mm, dbip, resp);
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
