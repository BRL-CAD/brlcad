/*                  O B J _ A D J U S T . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2020 United States Government as represented by
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

#include "rt/func.h"

int
rt_obj_adjust(struct bu_vls *logstr, struct rt_db_internal *ip, int argc, const char **argv)
{
    int id;
    const struct rt_functab *ft;

    if (!logstr || !ip)
	return -1;

    BU_CK_VLS(logstr);
    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_adjust)
	return -4;

    return ft->ft_adjust(logstr, ip, argc, argv);
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
